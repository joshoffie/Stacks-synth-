#include "SynthPanel.h"

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
    const auto name = wave >= WavetableBank::kNumBuiltIn ? processor.userWaveName (wave - WavetableBank::kNumBuiltIn) : juce::String();
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
    const int user = wave - WavetableBank::kNumBuiltIn;
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
    const auto caption = user >= 0 ? (table != nullptr ? shownName.upToLastOccurrenceOf (".", false, false) : juce::String ("click to import"))
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
    const int slot = wave >= WavetableBank::kNumBuiltIn ? wave - WavetableBank::kNumBuiltIn : processor.firstFreeUserSlot();

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
SynthPanel::SynthPanel (StacksAudioProcessor& p) : processor (p), apvts (p.apvts)
{
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

    for (int i = 0; i < kNumModSlots; ++i)
    {
        apvts.addParameterListener (paramId (modSourceParam (i)), this);
        apvts.addParameterListener (paramId (modDestParam (i)), this);
        apvts.addParameterListener (paramId (modAmountParam (i)), this);
    }
    refreshModulationDisplay();
}

SynthPanel::~SynthPanel()
{
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
    const auto area = top->getLocalArea (this, section.moreButton->getBounds());
    juce::CallOutBox::launchAsynchronously (std::move (content), area, top);
}

SynthPanel::Section* SynthPanel::findSection (const juce::String& title)
{
    for (auto& s : sections)
        if (s->title == title)
            return s.get();
    return nullptr;
}

int SynthPanel::preferredHeight() const
{
    int h = 2 * kPad;
    for (const auto& row : rows)
        h += row.height + kGap;
    return h - kGap;
}

void SynthPanel::resized()
{
    const int normalH = kTitleH + kCellH + kPad;
    int y = kPad;

    for (auto& row : rows)
    {
        row.bounds = { 0, y, getWidth(), row.height };
        int x = kBand + kPad;
        for (auto* section : row.sections)
        {
            int w = 2 * kPad;
            for (const auto& [component, cellWidth] : section->controls)
                w += cellWidth;

            const int h = section->tall ? row.height : normalH;
            section->bounds = { x, y, w, h };
            if (section->moreButton != nullptr)
                section->moreButton->setBounds (section->bounds.withHeight (kTitleH).removeFromRight (40).reduced (3, 1));

            int cx = x + kPad;
            for (const auto& [component, cellWidth] : section->controls)
            {
                component->setBounds (cx, y + kTitleH, cellWidth, section->tall ? h - kTitleH - kPad : kCellH);
                cx += cellWidth;
            }
            x += w + kGap;
        }
        y += row.height + kGap;
    }
}

void SynthPanel::paint (juce::Graphics& g)
{
    for (const auto& row : rows)
    {
        // Caption band on the left, rotated to save width.
        auto band = row.bounds.withWidth (kBand).toFloat();
        g.setColour (row.colour.withAlpha (0.18f));
        g.fillRoundedRectangle (band.reduced (2.0f, 0.0f), 4.0f);
        g.setColour (row.colour);
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, band.getCentreX(), band.getCentreY()));
        g.drawText (row.caption, juce::Rectangle<float> (band.getCentreX() - band.getHeight() * 0.5f, band.getCentreY() - band.getWidth() * 0.5f,
                                                         band.getHeight(), band.getWidth()),
                    juce::Justification::centred, false);
        g.restoreState();

        for (const auto* section : row.sections)
        {
            g.setColour (colours::panel);
            g.fillRoundedRectangle (section->bounds.toFloat(), 6.0f);
            g.setColour (section->colour);
            g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
            const auto title = section->title == kModulatorsGroup ? juce::String ("MODULATORS   -   pick one, press Assign, click a knob") : section->title;
            g.drawText (title, section->bounds.withHeight (kTitleH).reduced (kPad, 0), juce::Justification::centredLeft);
        }
    }

    if (assigningSource >= 0)
    {
        g.setColour (modSourceColour (assigningSource));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText ("Click a knob to modulate it with " + modSourceNames()[assigningSource] + "   (Esc cancels)",
                    getLocalBounds().removeFromTop (16).withTrimmedLeft (kBand + 8), juce::Justification::centredLeft);
    }
}

} // namespace stacks
