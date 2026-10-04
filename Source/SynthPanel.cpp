#include "SynthPanel.h"
#include "Displays.h"
#include "StacksLookAndFeel.h"

namespace stacks
{

namespace
{
    // Row colours: one hue per stage of the signal path, so a knob's colour
    // says what part of the synth it belongs to.
    const juce::Colour kSound    { 0xfff2a541 }; // amber
    const juce::Colour kFilter   { 0xffe8775a }; // coral
    const juce::Colour kMovement { 0xff5ec8c0 }; // teal
    const juce::Colour kSpace    { 0xff7fa7d8 }; // blue

    const char* kModulatorsGroup = "__MODULATORS__";

    struct RowSpec { const char* caption; juce::Colour colour; int height; std::vector<const char*> groups; };

    const std::vector<RowSpec>& rowSpecs()
    {
        static const std::vector<RowSpec> specs = {
            { "SOUND",      kSound,    0, { "OSC A", "OSC B", "MIX" } },
            { "FILTER",     kFilter,   0, { "FILTER", "FILTER ENV", "AMP ENV" } },
            { "MODULATORS", kMovement, SynthPanel::kModulatorsHeight, { kModulatorsGroup, "VOICE" } },
            { "SPACE",      kSpace,    0, { "CHORUS", "DELAY", "REVERB" } },
        };
        return specs;
    }

    juce::Colour colourForGroup (const juce::String& group)
    {
        for (const auto& row : rowSpecs())
            for (auto* g : row.groups)
                if (group == g)
                    return row.colour;
        return colours::accent;
    }

    // Groups the modulators area draws itself.
    bool isModulatorGroup (const juce::String& group)
    {
        return group.startsWith ("LFO ") || group == "MOD MATRIX" || group == "MOD ENV";
    }
}

//==============================================================================
// The pop-out holding a section's advanced parameters.
class AdvancedBox : public juce::Component
{
public:
    AdvancedBox (juce::AudioProcessorValueTreeState& apvts, const juce::String& title, juce::Colour colour,
                 const std::vector<const ParamSpec*>& specs)
    {
        heading.setText (title + "  -  more", juce::dontSendNotification);
        heading.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        heading.setColour (juce::Label::textColourId, colour);
        addAndMakeVisible (heading);

        for (auto* spec : specs)
        {
            std::unique_ptr<juce::Component> c;
            if (spec->kind == ParamKind::Choice)
                c = std::make_unique<ParamChoice> (apvts, *spec);
            else
            {
                auto knob = std::make_unique<ParamKnob> (apvts, *spec);
                knob->setAccent (colour);
                c = std::move (knob);
            }
            addAndMakeVisible (*c);
            controls.push_back (std::move (c));
        }
        setSize (12 + (int) controls.size() * SynthPanel::kCell + 12, 20 + SynthPanel::kCellH + 10);
    }

    void paint (juce::Graphics& g) override { g.fillAll (colours::panel); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 6);
        heading.setBounds (r.removeFromTop (18));
        for (auto& c : controls)
            c->setBounds (r.removeFromLeft (SynthPanel::kCell));
    }

private:
    juce::Label heading;
    std::vector<std::unique_ptr<juce::Component>> controls;
};

//==============================================================================
WaveDisplay::WaveDisplay (StacksAudioProcessor& p, bool b, juce::Colour c) : processor (p), oscB (b), colour (c)
{
    setTooltip ("The oscillator's wave at its morph position. Click to import a wavetable (.wav, 2048-sample frames) into a User slot.");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (12);
}

WaveDisplay::~WaveDisplay()
{
    stopTimer();
}

void WaveDisplay::timerCallback()
{
    const int wave = (int) processor.apvts.getRawParameterValue (paramId (oscB ? P::oscB_wave : P::oscA_wave))->load();
    const float morph = processor.apvts.getRawParameterValue (paramId (oscB ? P::oscB_morph : P::oscA_morph))->load();
    const int slot = wave >= kCustomWave ? UserWavetables::customSlot (oscB ? 1 : 0) : wave - WavetableBank::kNumBuiltIn;
    const auto name = wave >= WavetableBank::kNumBuiltIn ? processor.userWaveName (slot) : juce::String();
    if (wave != shownWave || std::abs (morph - shownMorph) > 0.002f || name != shownName)
    {
        shownWave = wave;
        shownMorph = morph;
        shownName = name;
        repaint();
    }
}

void WaveDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f, 15.0f).withTrimmedBottom (2.0f);
    g.setColour (colours::background);
    g.fillRoundedRectangle (r, 4.0f);

    const int wave = juce::jmax (0, shownWave);
    const float morph = juce::jlimit (0.0f, 1.0f, shownMorph);
    const bool custom = wave >= kCustomWave;
    const int user = custom ? UserWavetables::customSlot (oscB ? 1 : 0) : wave - WavetableBank::kNumBuiltIn;
    const UserTable* table = user >= 0 ? processor.userWavetables().active (user) : nullptr;

    juce::Path path;
    const int steps = 48;
    for (int i = 0; i <= steps; ++i)
    {
        const float phase = (float) i / (float) steps;
        float y = 0.0f;
        if (user < 0)           y = processor.builtInWavetables().read (wave, 0, morph, phase >= 1.0f ? 0.999f : phase);
        else if (table != nullptr) y = table->read (0, morph, phase >= 1.0f ? 0.999f : phase);
        else                    y = std::sin (juce::MathConstants<float>::twoPi * phase);
        const float px = r.getX() + 2.0f + phase * (r.getWidth() - 4.0f);
        const float py = r.getCentreY() - y * (r.getHeight() * 0.5f - 3.0f);
        if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
    }
    g.setColour (user >= 0 && table == nullptr ? colours::muted : colour);
    g.strokePath (path, juce::PathStrokeType (1.6f));

    g.setColour (colours::muted);
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    const auto caption = custom    ? (table != nullptr ? shownName : juce::String ("no table designed"))
                       : user >= 0 ? (table != nullptr ? shownName.upToLastOccurrenceOf (".", false, false) : juce::String ("click to import"))
                                   : juce::String ("import...");
    g.drawText (caption, getLocalBounds().removeFromBottom (14), juce::Justification::centred, true);
    g.drawText ("Shape", getLocalBounds().removeFromTop (14), juce::Justification::centred, true);
}

void WaveDisplay::mouseDown (const juce::MouseEvent&)
{
    importWavetable();
}

void WaveDisplay::importWavetable()
{
    const int wave = (int) processor.apvts.getRawParameterValue (paramId (oscB ? P::oscB_wave : P::oscA_wave))->load();
    const int slot = wave >= WavetableBank::kNumBuiltIn && wave < kCustomWave ? wave - WavetableBank::kNumBuiltIn : processor.firstFreeUserSlot();

    chooser = std::make_unique<juce::FileChooser> ("Import a wavetable (.wav with 2048-sample frames, Serum style)",
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.wav;*.aif;*.aiff");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, slot] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (! file.existsAsFile())
                                  return;
                              juce::String error;
                              if (processor.importWavetable (file, slot, error))
                              {
                                  if (auto* param = processor.apvts.getParameter (paramId (oscB ? P::oscB_wave : P::oscA_wave)))
                                      param->setValueNotifyingHost (param->convertTo0to1 ((float) (WavetableBank::kNumBuiltIn + slot)));
                              }
                              else
                              {
                                  juce::NativeMessageBox::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                                                         .withTitle ("Couldn't import that wavetable").withMessage (error).withButton ("OK"), nullptr);
                              }
                          });
}

//==============================================================================
// One row of sections, drawn with its caption band. In the single-row views
// the whole container is scaled up to fill the panel.
class SynthPanel::RowContainer : public juce::Component
{
public:
    explicit RowContainer (Row& r) : row (r) {}

    void paint (juce::Graphics& g) override
    {
        auto band = juce::Rectangle<float> (0.0f, 0.0f, (float) kBand, (float) getHeight());
        g.setColour (row.colour.withAlpha (0.18f));
        g.fillRoundedRectangle (band.reduced (2.0f, 0.0f), 4.0f);
        g.setColour (row.colour);
        g.setFont (StacksLookAndFeel::font (10.0f, true).withExtraKerningFactor (0.12f));
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, band.getCentreX(), band.getCentreY()));
        g.drawText (row.caption, juce::Rectangle<float> (band.getCentreX() - band.getHeight() * 0.5f, band.getCentreY() - band.getWidth() * 0.5f,
                                                         band.getHeight(), band.getWidth()),
                    juce::Justification::centred, false);
        g.restoreState();

        for (const auto* section : row.sections)
        {
            const auto b = section->bounds.toFloat();
            juce::ColourGradient grad (colours::panel.brighter (0.05f), b.getX(), b.getY(), colours::panel.darker (0.08f), b.getX(), b.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (b, 7.0f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawRoundedRectangle (b.reduced (0.5f), 7.0f, 1.0f);
            g.setColour (section->colour);
            g.setFont (StacksLookAndFeel::font (10.5f, true).withExtraKerningFactor (0.06f));
            const auto title = section->title == kModulatorsGroup ? juce::String ("MODULATORS   -   pick one, press Assign, click a knob") : section->title;
            g.drawText (title, section->bounds.withHeight (kTitleH).reduced (kPad, 0), juce::Justification::centredLeft);
        }
    }

private:
    Row& row;
};

//==============================================================================
// One line of help under the rows: the control under the mouse, named and
// explained in plain words; otherwise what the current screen is for.
class SynthPanel::HelpStrip : public juce::Component,
                              private juce::Timer
{
public:
    explicit HelpStrip (SynthPanel& owner) : panel (owner)
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (colours::panel.withAlpha (0.7f));
        g.fillRoundedRectangle (r, 6.0f);
        juce::AttributedString text;
        if (title.isNotEmpty())
        {
            text.append (title + "   ", StacksLookAndFeel::font (12.0f, true), accent);
            text.append (body, StacksLookAndFeel::font (12.0f), colours::text.withAlpha (0.9f));
        }
        else
            text.append (body, StacksLookAndFeel::font (12.0f), colours::muted);
        text.setJustification (juce::Justification::centredLeft);
        text.setWordWrap (juce::AttributedString::none);
        text.draw (g, r.reduced (12.0f, 0.0f));
    }

private:
    void timerCallback() override
    {
        juce::String newTitle, newBody;
        juce::Colour newAccent = colours::accent;
        auto* under = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
        for (auto* c = under; c != nullptr && c != &panel; c = c->getParentComponent())
        {
            if (auto* knob = dynamic_cast<ParamKnob*> (c))
            {
                const auto& s = spec ((P) knob->parameterIndex());
                newTitle = s.name;
                newBody = juce::String (s.aiHint).replace (" (advanced)", "");
                if (s.unit[0] != 0) newBody << "  (" << s.unit << ")";
                newBody << "   -   drag up/down; double-click to reset; with Assign on, click to modulate it";
                break;
            }
            if (auto* tip = dynamic_cast<juce::SettableTooltipClient*> (c))
            {
                if (tip->getTooltip().isNotEmpty()) { newBody = tip->getTooltip(); break; }
            }
            if (auto* combo = dynamic_cast<juce::ComboBox*> (c))
            {
                if (combo->getTooltip().isNotEmpty()) { newTitle = "Choice"; newBody = combo->getTooltip(); break; }
            }
        }
        if (newBody.isEmpty())
        {
            static const char* const screens[] = {
                "SOUND is where the tone starts: two wavetable oscillators (A and B, B can FM A), a sub for weight and noise for air. Morph slides through each table.",
                "FILTER shapes the tone: cutoff is brightness, resonance a peak at the cutoff. The filter envelope moves the cutoff per note; the amp envelope shapes loudness.",
                "MODULATORS make things move: draw an LFO, press Assign and click any knob - it swings around its value. The Mod Env is a spare envelope for anything.",
                "SPACE is the room: chorus for width and shimmer, delay for echoes (in time with the host), reverb for the tail. The 'more' buttons hold the fine print.",
            };
            if (panel.viewMode >= 0 && panel.viewMode < 4) newBody = screens[panel.viewMode];
            else newBody = "Hover any control to see what it does. The tabs above open one section at a time, larger. Signal flows top to bottom: SOUND > FILTER > MODULATORS > SPACE.";
        }
        if (newTitle != title || newBody != body)
        {
            title = newTitle; body = newBody; accent = newAccent;
            repaint();
        }
    }

    SynthPanel& panel;
    juce::String title, body;
    juce::Colour accent { colours::accent };
};

//==============================================================================
SynthPanel::SynthPanel (StacksAudioProcessor& p) : processor (p), apvts (p.apvts)
{
    help = std::make_unique<HelpStrip> (*this);
    addAndMakeVisible (*help);

    setWantsKeyboardFocus (true);

    // The modulators area is one wide "section" of its own.
    {
        sections.push_back (std::make_unique<Section>());
        auto* section = sections.back().get();
        section->title = kModulatorsGroup;
        section->colour = kMovement;
        section->tall = true;
        modulators = std::make_unique<ModulatorsPanel> (processor);
        modulators->onAssignRequest = [this] (int source) { beginAssign (source); };
        modulators->onAssignCancel  = [this] { endAssign(); };
        addAndMakeVisible (*modulators);
        section->controls.emplace_back (modulators.get(), 674);
    }

    // One section per parameter group, controls generated from the table...
    for (const auto& spec : paramSpecs())
    {
        const juce::String group (spec.group);
        if (group == "MASTER" || isModulatorGroup (group))
            continue;

        auto* section = findSection (group);
        if (section == nullptr)
        {
            sections.push_back (std::make_unique<Section>());
            section = sections.back().get();
            section->title = group;
            section->colour = colourForGroup (group);
        }

        if (isAdvancedParam (spec.id))
        {
            section->advanced.push_back (&spec);
            if (section->moreButton == nullptr)
            {
                section->moreButton = std::make_unique<juce::TextButton> ("more");
                section->moreButton->setTooltip ("More " + group.toLowerCase() + " options");
                section->moreButton->setColour (juce::TextButton::buttonColourId, colours::card);
                auto* sectionPtr = section;
                section->moreButton->onClick = [this, sectionPtr] { showAdvanced (*sectionPtr); };
                addAndMakeVisible (*section->moreButton);
            }
            continue;
        }

        std::unique_ptr<juce::Component> control;
        int cellWidth = kCell;
        if (spec.kind == ParamKind::Choice)
        {
            control = std::make_unique<ParamChoice> (apvts, spec);
            cellWidth = kChoiceCell;
        }
        else
        {
            auto knob = std::make_unique<ParamKnob> (apvts, spec);
            knob->setAccent (section->colour);
            knob->onAssignClick = [this] (int paramIndex) { knobClicked (paramIndex); };
            knob->onModulatorDropped = [this] (int source, int paramIndex) { processor.addModulation (source, modTargetForParam (paramIndex)); };
            knob->onRingDrag = [this] (int slot, float amount) { processor.setModulationAmount (slot, amount); };
            knobs.push_back (knob.get());
            control = std::move (knob);
        }

        addAndMakeVisible (*control);
        section->controls.emplace_back (control.get(), cellWidth);
        controls.push_back (std::move (control));

        const juce::String id (spec.id);
        if (id == "oscA_wave" || id == "oscB_wave")
        {
            auto display = std::make_unique<WaveDisplay> (processor, id == "oscB_wave", section->colour);
            addAndMakeVisible (*display);
            section->controls.emplace_back (display.get(), kCell);
            controls.push_back (std::move (display));
        }
    }

    // Displays at the front of the filter and envelope sections.
    auto prepend = [this] (const char* group, std::unique_ptr<juce::Component> display, int width)
    {
        if (auto* section = findSection (group))
        {
            addAndMakeVisible (*display);
            optionalDisplays.push_back (display.get());
            section->controls.insert (section->controls.begin(), { display.get(), width });
            controls.push_back (std::move (display));
        }
    };
    if (auto* f = findSection ("FILTER"))     prepend ("FILTER",     std::make_unique<FilterCurve> (apvts, f->colour), 92);
    if (auto* f = findSection ("FILTER ENV")) prepend ("FILTER ENV", std::make_unique<EnvelopeDisplay> (apvts, "fenv", f->colour), 84);
    if (auto* f = findSection ("AMP ENV"))    prepend ("AMP ENV",    std::make_unique<EnvelopeDisplay> (apvts, "aenv", f->colour), 84);

    // ...placed in fixed rows that follow the signal path.
    for (const auto& spec : rowSpecs())
    {
        Row row;
        row.caption = spec.caption;
        row.colour = spec.colour;
        row.height = spec.height > 0 ? spec.height : kTitleH + kCellH + kPad;
        for (auto* g : spec.groups)
            if (auto* section = findSection (g))
                row.sections.push_back (section);
        if (! row.sections.empty())
            rows.push_back (std::move (row));
    }

    for (auto& row : rows)
    {
        row.container = std::make_unique<RowContainer> (row);
        addAndMakeVisible (*row.container);
        int w = kBand + kPad;
        for (auto* section : row.sections)
        {
            int sw = 2 * kPad;
            for (const auto& [component, cellWidth] : section->controls)
            {
                row.container->addAndMakeVisible (*component);   // re-parent into the row
                sw += cellWidth;
            }
            if (section->moreButton != nullptr)
                row.container->addAndMakeVisible (*section->moreButton);
            w += sw + kGap;
        }
        row.naturalWidth = w;
    }

    // View tabs: everything, or one row filling the panel.
    auto addViewButton = [this] (const juce::String& text, int mode, juce::Colour colour)
    {
        auto b = std::make_unique<juce::TextButton> (text);
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, colour.withAlpha (0.35f));
        b->setColour (juce::TextButton::textColourOnId, colours::text);
        b->onClick = [this, mode] { setView (mode); };
        addAndMakeVisible (*b);
        viewButtons.push_back (std::move (b));
    };
    addViewButton ("ALL", -1, colours::accent);
    for (int i = 0; i < (int) rows.size(); ++i)
        addViewButton (rows[(size_t) i].caption, i, rows[(size_t) i].colour);
    setView (-1);

    for (int i = 0; i < kNumModSlots; ++i)
    {
        apvts.addParameterListener (paramId (modSourceParam (i)), this);
        apvts.addParameterListener (paramId (modDestParam (i)), this);
        apvts.addParameterListener (paramId (modAmountParam (i)), this);
    }
    refreshModulationDisplay();
    startTimerHz (30);
}

void SynthPanel::timerCallback()
{
    if (processor.calmMode())
    {
        for (auto* knob : knobs) knob->clearLiveValue();
        return;
    }
    for (auto* knob : knobs)
        knob->setLiveValue (processor.liveValue (knob->parameterIndex()));
}

SynthPanel::~SynthPanel()
{
    stopTimer();
    cancelPendingUpdate();
    for (int i = 0; i < kNumModSlots; ++i)
    {
        apvts.removeParameterListener (paramId (modSourceParam (i)), this);
        apvts.removeParameterListener (paramId (modDestParam (i)), this);
        apvts.removeParameterListener (paramId (modAmountParam (i)), this);
    }
}

void SynthPanel::handleAsyncUpdate()
{
    refreshModulationDisplay();
}

void SynthPanel::refreshModulationDisplay()
{
    for (auto* knob : knobs)
    {
        std::vector<KnobModulation> mods;
        for (const auto& m : processor.modulationsOnParam (knob->parameterIndex()))
            mods.push_back ({ m.slot, m.source, m.amount });
        knob->setModulations (std::move (mods));
    }
    if (modulators)
        modulators->refreshConnections();
}

void SynthPanel::beginAssign (int source)
{
    assigningSource = source;
    for (auto* knob : knobs)
        knob->setAssignMode (true, modSourceColour (source));
    modulators->setAssigning (source);
    grabKeyboardFocus();
}

void SynthPanel::endAssign()
{
    assigningSource = -1;
    for (auto* knob : knobs)
        knob->setAssignMode (false, {});
    modulators->setAssigning (-1);
}

void SynthPanel::knobClicked (int paramIndex)
{
    if (assigningSource < 0)
        return;
    processor.addModulation (assigningSource, modTargetForParam (paramIndex));
    endAssign();
}

void SynthPanel::mouseDown (const juce::MouseEvent&)
{
    if (assigningSource >= 0)
        endAssign();
}

bool SynthPanel::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && assigningSource >= 0)
    {
        endAssign();
        return true;
    }
    return false;
}

void SynthPanel::showAdvanced (Section& section)
{
    auto content = std::make_unique<AdvancedBox> (apvts, section.title, section.colour, section.advanced);
    auto* top = getTopLevelComponent();
    const auto area = top->getLocalArea (section.moreButton.get(), section.moreButton->getLocalBounds());
    juce::CallOutBox::launchAsynchronously (std::move (content), area, top);
}

SynthPanel::Section* SynthPanel::findSection (const juce::String& title)
{
    for (auto& s : sections)
        if (s->title == title)
            return s.get();
    return nullptr;
}

void SynthPanel::setView (int rowIndex)
{
    viewMode = rowIndex;
    for (int i = 0; i < (int) viewButtons.size(); ++i)
        viewButtons[(size_t) i]->setToggleState (i - 1 == rowIndex, juce::dontSendNotification);
    for (int i = 0; i < (int) rows.size(); ++i)
        rows[(size_t) i].container->setVisible (viewMode < 0 || viewMode == i);
    resized();
    repaint();
}

int SynthPanel::preferredHeight() const
{
    int h = 24 + 4 + 2 * kPad + 28;
    for (const auto& row : rows)
        h += row.height + kGap;
    return h - kGap;
}

void SynthPanel::resized()
{
    auto area = getLocalBounds();
    auto tabs = area.removeFromTop (22);
    const int tabW = juce::jmin (120, (tabs.getWidth() - kBand) / juce::jmax (1, (int) viewButtons.size()));
    tabs.removeFromLeft (kBand);
    for (auto& b : viewButtons)
        b->setBounds (tabs.removeFromLeft (tabW).reduced (1, 0));
    area.removeFromTop (6);
    help->setBounds (area.removeFromBottom (24));
    area.removeFromBottom (4);

    const int normalH = kTitleH + kCellH + kPad;

    // The dense ALL view drops the response/envelope displays when a row would
    // not fit the panel otherwise; a single-row view always has room for them.
    auto isOptional = [this] (juce::Component* c) { return std::find (optionalDisplays.begin(), optionalDisplays.end(), c) != optionalDisplays.end(); };
    bool compact = false;
    if (viewMode < 0)
        for (const auto& row : rows)
        {
            int full = kBand + kPad;
            for (auto* section : row.sections)
            {
                full += 2 * kPad + kGap;
                for (const auto& [component, cellWidth] : section->controls) full += cellWidth;
            }
            if (full > area.getWidth() - 8) compact = true;
        }

    // Lay every row out in its own container at natural size...
    for (auto& row : rows)
    {
        int x = kBand + kPad;
        for (auto* section : row.sections)
        {
            auto widthOf = [&] (juce::Component* c, int cellWidth) { return compact && isOptional (c) ? 0 : cellWidth; };
            int w = 2 * kPad;
            for (const auto& [component, cellWidth] : section->controls)
                w += widthOf (component, cellWidth);
            const int h = section->tall ? row.height : normalH;
            section->bounds = { x, 0, w, h };
            if (section->moreButton != nullptr)
                section->moreButton->setBounds (section->bounds.withHeight (kTitleH).removeFromRight (40).reduced (3, 1));
            int cx = x + kPad;
            for (const auto& [component, cellWidth] : section->controls)
            {
                const int cw = widthOf (component, cellWidth);
                component->setVisible (cw > 0);
                if (cw > 0)
                    component->setBounds (cx, kTitleH, cw, section->tall ? h - kTitleH - kPad : kCellH);
                cx += cw;
            }
            x += w + kGap;
        }
        row.naturalWidth = x;
        row.container->setSize (row.naturalWidth, row.height);
    }

    // ...then place them: stacked for ALL, or one row scaled to fill.
    if (viewMode < 0)
    {
        int y = area.getY() + kPad;
        for (auto& row : rows)
        {
            row.container->setTransform ({});
            row.container->setTopLeftPosition (0, y);
            y += row.height + kGap;
        }
    }
    else if (viewMode < (int) rows.size())
    {
        auto& row = rows[(size_t) viewMode];
        const float scale = juce::jlimit (1.0f, 2.2f, juce::jmin ((float) (area.getWidth() - 8) / (float) row.naturalWidth,
                                                                   (float) (area.getHeight() - 8) / (float) row.height));
        const float px = (float) area.getX() + ((float) area.getWidth() - row.naturalWidth * scale) * 0.5f;
        const float py = (float) area.getY() + juce::jmax (4.0f, ((float) area.getHeight() - row.height * scale) * 0.35f);
        row.container->setTopLeftPosition (0, 0);
        row.container->setTransform (juce::AffineTransform::scale (scale).translated (px, py));
    }
}

void SynthPanel::paint (juce::Graphics& g)
{
    if (assigningSource >= 0)
    {
        g.setColour (modSourceColour (assigningSource));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText ("Click a knob to modulate it with " + modSourceNames()[assigningSource] + "   (Esc cancels)",
                    getLocalBounds().removeFromTop (22).withTrimmedLeft (kBand + (int) viewButtons.size() * 120 + 12), juce::Justification::centredLeft);
    }
}

} // namespace stacks
