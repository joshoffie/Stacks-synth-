#include "SynthPanel.h"
#include <algorithm>
#include "Displays.h"
#include "StacksLookAndFeel.h"

namespace stacks
{

namespace
{
    // One card of the panel: which parameter groups it holds, how many
    // columns its grid has, and the display drawn across its top.
    struct SectionSpec { const char* title; std::vector<const char*> groups; int columns; const char* display; };
    struct RowSpec { const char* caption; const juce::Colour* colour; std::vector<SectionSpec> sections; };

    const char* kModulatorsSection = "MODULATORS";

    const std::vector<RowSpec>& rowSpecs()
    {
        static const std::vector<RowSpec> specs = {
            { "SOUND",      &colours::rowSound,    { { "OSC A", { "OSC A" }, 4, "waveA" }, { "OSC B", { "OSC B" }, 4, "waveB" }, { "OSC C", { "OSC C" }, 4, "waveC" },
                                                     { "MIX", { "MIX" }, 1, nullptr }, { "SAMPLE", { "SAMPLE", "GRAIN" }, 4, "sample" } } },
            { "FILTER",     &colours::rowFilter,   { { "FILTER", { "FILTER", "FILTER 2" }, 6, "filter" }, { "CHARACTER", { "CHARACTER" }, 2, nullptr }, { "FILTER ENV", { "FILTER ENV" }, 2, "fenv" }, { "AMP ENV", { "AMP ENV" }, 2, "aenv" } } },
            { "EFFECTS",    &colours::rowSpace,    { { "DISTORTION", { "DISTORTION" }, 2, nullptr }, { "EQ", { "EQ" }, 2, nullptr }, { "COMPRESSOR", { "COMPRESSOR" }, 2, nullptr },
                                                     { "CHORUS", { "CHORUS" }, 2, nullptr }, { "HYPER", { "HYPER" }, 1, nullptr }, { "TAPE", { "TAPE" }, 2, nullptr },
                                                     { "DELAY", { "DELAY" }, 3, nullptr }, { "REVERB", { "REVERB" }, 2, nullptr } } },
            { "MODULATORS", &colours::rowMovement, { { kModulatorsSection, {}, 0, nullptr }, { "VOICE", { "VOICE" }, 3, nullptr }, { "ARP", { "ARP" }, 3, nullptr } } },
        };
        return specs;
    }

    // Knob colour = the row's hue, except the SHAPE effects keep their own.
    juce::Colour colourForGroup (const juce::String& group)
    {
        if (group == "DISTORTION" || group == "EQ" || group == "COMPRESSOR") return colours::rowShape;
        if (group == "SAMPLE" || group == "GRAIN") return colours::rowSample;
        for (const auto& row : rowSpecs())
            for (const auto& section : row.sections)
                for (auto* g : section.groups)
                    if (group == g)
                        return *row.colour;
        return colours::accent;
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
        heading.setText (title, juce::dontSendNotification);
        heading.setFont (StacksLookAndFeel::font (10.5f, true).withExtraKerningFactor (0.08f));
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
        setSize (12 + (int) controls.size() * SynthPanel::kCell + 12, 22 + SynthPanel::kCellH + 10);
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
WaveDisplay::WaveDisplay (StacksAudioProcessor& p, int o, juce::Colour c) : processor (p), osc (o), colour (c)
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
    const int wave = (int) processor.apvts.getRawParameterValue (paramId (oscWaveParam (osc)))->load();
    const float morph = processor.apvts.getRawParameterValue (paramId (oscMorphParam (osc)))->load();
    const int warp = (int) processor.apvts.getRawParameterValue (paramId (oscWarpParam (osc)))->load();
    const float warpAmt = processor.apvts.getRawParameterValue (paramId (oscWarpAmtParam (osc)))->load();
    const int slot = wave >= kCustomWave ? UserWavetables::customSlot (osc) : wave - WavetableBank::kNumBuiltIn;
    const auto name = wave >= WavetableBank::kNumBuiltIn ? processor.userWaveName (slot) : juce::String();
    if (wave != shownWave || std::abs (morph - shownMorph) > 0.002f || name != shownName || warp != shownWarp || std::abs (warpAmt - shownWarpAmt) > 0.002f)
    {
        shownWave = wave;
        shownMorph = morph;
        shownName = name;
        shownWarp = warp;
        shownWarpAmt = warpAmt;
        repaint();
    }
}

void WaveDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    StacksLookAndFeel::drawInset (g, r);

    const int wave = juce::jmax (0, shownWave);
    const float morph = juce::jlimit (0.0f, 1.0f, shownMorph);
    const bool custom = wave >= kCustomWave;
    const int user = custom ? UserWavetables::customSlot (osc) : wave - WavetableBank::kNumBuiltIn;
    const UserTable* table = user >= 0 ? processor.userWavetables().active (user) : nullptr;

    // faint centre line
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 3.0f, r.getRight() - 3.0f);

    // One cycle of the table at a given morph position (warp included).
    auto sampleAt = [&] (float m, float phase)
    {
        const float wp = warpPhase (shownWarp, phase >= 1.0f ? 0.999f : phase, shownWarpAmt);
        float y = 0.0f;
        if (user < 0)              y = processor.builtInWavetables().read (wave, 0, m, wp);
        else if (table != nullptr) y = table->read (0, m, wp);
        else                       y = std::sin (juce::MathConstants<float>::twoPi * wp);
        return warpSample (shownWarp, y, shownWarpAmt);
    };
    const bool missing = user >= 0 && table == nullptr;
    const auto lineColour = missing ? colours::muted : colour;
    const int steps = juce::jlimit (48, 160, (int) r.getWidth() / 2);

    if (r.getWidth() >= 96.0f && r.getHeight() >= 44.0f && ! missing)
    {
        // The whole table as frames receding into the panel; the frame the morph
        // sits on is lit, the others fade with depth.
        constexpr int frames = 12;
        const float dx = r.getWidth() * 0.2f / (float) (frames - 1), dy = r.getHeight() * 0.4f / (float) (frames - 1);
        const float waveW = r.getWidth() - 10.0f - dx * (float) (frames - 1), waveH = r.getHeight() - 10.0f - dy * (float) (frames - 1);
        const float x0 = r.getX() + 5.0f, yBase = r.getBottom() - 5.0f - waveH * 0.5f;
        const float morphFrame = morph * (float) (frames - 1);
        for (int f = frames - 1; f >= 0; --f)
        {
            const float m = (float) f / (float) (frames - 1);
            const float ox = x0 + dx * (float) f, oy = yBase - dy * (float) f;
            const float closeness = 1.0f - juce::jmin (1.0f, std::abs ((float) f - morphFrame));
            juce::Path frame;
            for (int i = 0; i <= steps; ++i)
            {
                const float phase = (float) i / (float) steps;
                const float px = ox + phase * waveW, py = oy - sampleAt (m, phase) * waveH * 0.46f;
                if (i == 0) frame.startNewSubPath (px, py); else frame.lineTo (px, py);
            }
            if (closeness > 0.0f)
            {
                juce::Path fill (frame);
                fill.lineTo (ox + waveW, oy);
                fill.lineTo (ox, oy);
                fill.closeSubPath();
                g.setColour (lineColour.withAlpha (0.1f * closeness));
                g.fillPath (fill);
            }
            const float depthFade = 0.14f + 0.26f * (1.0f - (float) f / (float) (frames - 1));
            g.setColour (lineColour.withAlpha (closeness > 0.0f ? 0.45f + 0.55f * closeness : depthFade));
            g.strokePath (frame, juce::PathStrokeType (closeness > 0.5f ? 1.8f : 1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
    else
    {
        juce::Path path;
        for (int i = 0; i <= steps; ++i)
        {
            const float phase = (float) i / (float) steps;
            const float px = r.getX() + 4.0f + phase * (r.getWidth() - 8.0f);
            const float py = r.getCentreY() - sampleAt (morph, phase) * (r.getHeight() * 0.5f - 5.0f);
            if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
        }
        juce::Path fill (path);
        fill.lineTo (r.getRight() - 4.0f, r.getCentreY());
        fill.lineTo (r.getX() + 4.0f, r.getCentreY());
        fill.closeSubPath();
        g.setColour (lineColour.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (lineColour);
        g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // The table's name only when it is a designed or imported one.
    if (user >= 0)
    {
        g.setColour (colours::muted);
        g.setFont (StacksLookAndFeel::font (9.0f));
        const auto caption = table != nullptr ? shownName.upToLastOccurrenceOf (".", false, false) : juce::String (custom ? "no table designed" : "click to import");
        g.drawText (caption, r.reduced (5.0f, 2.0f).toNearestInt(), juce::Justification::bottomRight, true);
    }
}

void WaveDisplay::mouseDown (const juce::MouseEvent&)
{
    importWavetable();
}

void WaveDisplay::importWavetable()
{
    const int wave = (int) processor.apvts.getRawParameterValue (paramId (oscWaveParam (osc)))->load();
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
                                  if (auto* param = processor.apvts.getParameter (paramId (oscWaveParam (osc))))
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
SampleDisplay::SampleDisplay (StacksAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts, juce::Colour c) : processor (p), colour (c)
{
    start = apvts.getRawParameterValue (paramId (P::smp_start));
    mode = apvts.getRawParameterValue (paramId (P::smp_mode));
    setTooltip ("The sample oscillator's file. Click to load one (wav, aiff, flac, mp3; up to a minute is kept); right-click to remove it. The marker is Start.");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (12);
}

SampleDisplay::~SampleDisplay()
{
    stopTimer();
}

void SampleDisplay::timerCallback()
{
    const auto* current = processor.sampleBank().current();
    const float st = start->load();
    const int md = (int) mode->load();
    if (current != shown || std::abs (st - shownStart) > 0.002f || md != shownMode)
    {
        shown = current;
        shownStart = st;
        shownMode = md;
        repaint();
    }
}

void SampleDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    StacksLookAndFeel::drawInset (g, r);
    g.setColour (colours::muted);
    g.setFont (StacksLookAndFeel::font (10.0f));

    const auto* s = shown;
    if (s == nullptr || s->length() < 2)
    {
        g.drawFittedText ("click to load a sample", r.toNearestInt().reduced (4), juce::Justification::centred, 2);
        return;
    }
    // Overview: the peak of each pixel column.
    const int columns = juce::jmax (1, (int) r.getWidth() - 8);
    const int len = s->length();
    const float* a = s->audio.getReadPointer (0);
    const float midY = r.getCentreY(), half = r.getHeight() * 0.5f - 4.0f;
    g.setColour ((shownMode == 0 ? colours::muted : colour).withAlpha (0.85f));
    for (int c = 0; c < columns; ++c)
    {
        const int i0 = (int) ((juce::int64) c * len / columns), i1 = juce::jmax (i0 + 1, (int) ((juce::int64) (c + 1) * len / columns));
        float peak = 0.0f;
        for (int i = i0; i < i1 && i < len; i += juce::jmax (1, (i1 - i0) / 64))
            peak = juce::jmax (peak, std::abs (a[i]));
        const float x = r.getX() + 4.0f + (float) c;
        g.drawVerticalLine ((int) x, midY - peak * half, midY + peak * half + 1.0f);
    }
    const float sx = r.getX() + 4.0f + juce::jlimit (0.0f, 1.0f, shownStart) * (float) (columns - 1);
    g.setColour (colours::text.withAlpha (0.85f));
    g.drawVerticalLine ((int) sx, r.getY() + 2.0f, r.getBottom() - 2.0f);
    g.setColour (colours::muted);
    g.setFont (StacksLookAndFeel::font (9.0f));
    g.drawText (s->name.upToLastOccurrenceOf (".", false, false), r.reduced (5.0f, 2.0f).toNearestInt(), juce::Justification::bottomRight, true);
}

void SampleDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (processor.sampleBank().current() != nullptr)
            processor.clearSample();
        return;
    }
    chooser = std::make_unique<juce::FileChooser> ("Load a sample for the sample oscillator",
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (! file.existsAsFile())
                                  return;
                              juce::String error;
                              if (! processor.importSample (file, error))
                                  juce::NativeMessageBox::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                                                         .withTitle ("Couldn't load that sample").withMessage (error).withButton ("OK"), nullptr);
                          });
}

//==============================================================================
int SynthPanel::Section::naturalWidth() const
{
    if (custom != nullptr) return 2 * kPad + 640;
    return 2 * kPad + columns * kCell;
}

int SynthPanel::Section::naturalHeight() const
{
    if (custom != nullptr) return kTitleH + kModulatorsHeight + kPad;
    return kTitleH + (display != nullptr ? kDisplayH + kPad : 0) + rows() * kCellH + kPad;
}

//==============================================================================
// One row of cards, drawn with its caption band. The whole container is
// scaled to fit the panel.
class SynthPanel::RowContainer : public juce::Component
{
public:
    RowContainer (SynthPanel& p, Row& r) : panel (p), row (r) {}

    bool isEffects() const { return row.caption == "EFFECTS"; }

    void paint (juce::Graphics& g) override
    {
        // The caption, rotated, in a slim band on the left.
        auto band = juce::Rectangle<float> (0.0f, 0.0f, (float) kBand, (float) getHeight());
        g.setColour (row.colour.withAlpha (0.14f));
        g.fillRoundedRectangle (band.reduced (1.0f, 0.0f), 3.0f);
        g.setColour (row.colour);
        g.setFont (StacksLookAndFeel::font (9.0f, true).withExtraKerningFactor (0.14f));
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, band.getCentreX(), band.getCentreY()));
        g.drawText (row.caption, juce::Rectangle<float> (band.getCentreX() - band.getHeight() * 0.5f, band.getCentreY() - band.getWidth() * 0.5f,
                                                         band.getHeight(), band.getWidth()),
                    juce::Justification::centred, false);
        g.restoreState();

        for (int i = 0; i < (int) row.sections.size(); ++i)
        {
            const auto* section = row.sections[(size_t) i];
            const auto b = section->bounds.toFloat();
            const int e = isEffects() ? SynthPanel::effectIndexFor (section->title) : -1;
            const bool off = e >= 0 && panel.apvts.getRawParameterValue (paramId (fxOnParam (e)))->load() < 0.5f;
            if (off) g.setOpacity (0.45f);
            StacksLookAndFeel::drawCard (g, b);
            g.setColour (section->colour);
            g.setFont (StacksLookAndFeel::font (9.5f, true).withExtraKerningFactor (0.1f));
            g.drawText (section->title, section->bounds.withHeight (kTitleH).reduced (kPad + 2, 0).withTrimmedTop (2), juce::Justification::centredLeft);
            if (isEffects() && e >= 0)
            {
                // a grip in the title strip: this card can be dragged
                g.setColour (section->colour.withAlpha (0.5f));
                const float gx = b.getRight() - 40.0f, gy = b.getY() + 6.0f;
                for (int k = 0; k < 3; ++k) g.fillEllipse (gx + 4.0f * (float) k, gy, 2.0f, 2.0f);
                for (int k = 0; k < 3; ++k) g.fillEllipse (gx + 4.0f * (float) k, gy + 4.0f, 2.0f, 2.0f);
            }
            g.setOpacity (1.0f);
            if (dragging && i == dragIndex)
            {
                g.setColour (section->colour.withAlpha (0.25f));
                g.fillRoundedRectangle (b, 6.0f);
            }
        }
        if (isEffects())
            paintLinks (g);
    }

    // The signal flows left to right: a link between each pair of cards, an
    // arrow into the next one. Dimmed and broken when the next effect is off.
    void paintLinks (juce::Graphics& g)
    {
        for (int i = 0; i + 1 < (int) row.sections.size(); ++i)
        {
            const auto gapRect = linkRect (i);
            const int e = SynthPanel::effectIndexFor (row.sections[(size_t) i + 1]->title);
            const bool on = e < 0 || panel.apvts.getRawParameterValue (paramId (fxOnParam (e)))->load() > 0.5f;
            const float y = gapRect.getCentreY();
            const float x0 = gapRect.getX() + 1.0f, x1 = gapRect.getRight() - 1.0f;
            g.setColour (on ? row.colour.withAlpha (0.9f) : colours::muted.withAlpha (0.6f));
            if (on)
                g.drawLine (x0, y, x1 - 5.0f, y, 1.6f);
            else
            {
                const float dashes[] = { 3.0f, 3.0f };
                juce::Path dash; dash.startNewSubPath (x0, y); dash.lineTo (x1 - 5.0f, y);
                juce::Path dashed; juce::PathStrokeType (1.4f).createDashedStroke (dashed, dash, dashes, 2);
                g.fillPath (dashed);
            }
            juce::Path arrow;
            arrow.addTriangle (x1 - 6.0f, y - 4.5f, x1 - 6.0f, y + 4.5f, x1 + 1.0f, y);
            g.fillPath (arrow);
            // the switch in the middle of the link
            const float cx = gapRect.getCentreX() - 2.0f;
            g.setColour (colours::background);
            g.fillEllipse (cx - 5.0f, y - 5.0f, 10.0f, 10.0f);
            g.setColour (on ? row.colour : colours::muted);
            g.drawEllipse (cx - 5.0f, y - 5.0f, 10.0f, 10.0f, 1.2f);
            if (on) g.fillEllipse (cx - 2.5f, y - 2.5f, 5.0f, 5.0f);
        }
        if (dragging && dragIndex >= 0)
        {
            const int target = dropIndex (dragX);
            // a bar where the card would land
            float x = (float) (kBand + kPad) - 4.0f;
            if (target > 0)
            {
                const int before = juce::jmin (target, (int) row.sections.size()) - 1;
                x = (float) row.sections[(size_t) before]->bounds.getRight() + (float) panel.rowGap (row) * 0.5f;
            }
            g.setColour (colours::text.withAlpha (0.8f));
            g.fillRoundedRectangle (x - 1.5f, 2.0f, 3.0f, (float) getHeight() - 4.0f, 1.5f);
        }
    }

    // The gap between card i and card i+1, over the whole row height.
    juce::Rectangle<float> linkRect (int i) const
    {
        const auto& a = row.sections[(size_t) i]->bounds;
        const auto& b = row.sections[(size_t) i + 1]->bounds;
        return juce::Rectangle<float> ((float) a.getRight(), (float) a.getY(), (float) (b.getX() - a.getRight()), (float) a.getHeight());
    }

    int dropIndex (int x) const
    {
        int target = 0;
        for (const auto* s : row.sections)
            if (x > s->bounds.getCentreX()) ++target;
        return target;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragIndex = -1; dragging = false;
        if (! isEffects()) return;
        // a link's switch: in or out of the chain
        for (int i = 0; i + 1 < (int) row.sections.size(); ++i)
            if (linkRect (i).expanded (2.0f, 0.0f).contains (e.position))
            {
                panel.toggleEffect (SynthPanel::effectIndexFor (row.sections[(size_t) i + 1]->title));
                return;
            }
        // a card's title strip: pick it up
        for (int i = 0; i < (int) row.sections.size(); ++i)
        {
            const auto strip = row.sections[(size_t) i]->bounds.withHeight (kTitleH);
            if (strip.contains (e.getPosition()) && SynthPanel::effectIndexFor (row.sections[(size_t) i]->title) >= 0)
            {
                dragIndex = i;
                dragX = e.x;
                setMouseCursor (juce::MouseCursor::DraggingHandCursor);
                return;
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragIndex < 0) return;
        if (! dragging && std::abs (e.getDistanceFromDragStartX()) > 6) dragging = true;
        dragX = e.x;
        if (dragging) repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        if (dragging && dragIndex >= 0)
        {
            int target = dropIndex (e.x);
            if (target > dragIndex) --target;             // the card leaves its own slot first
            panel.moveEffect (dragIndex, target);
        }
        dragIndex = -1;
        dragging = false;
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (! isEffects()) return;
        bool overStrip = false, overLink = false;
        for (int i = 0; i < (int) row.sections.size(); ++i)
            if (row.sections[(size_t) i]->bounds.withHeight (kTitleH).contains (e.getPosition()) && SynthPanel::effectIndexFor (row.sections[(size_t) i]->title) >= 0) overStrip = true;
        for (int i = 0; i + 1 < (int) row.sections.size(); ++i)
            if (linkRect (i).contains (e.position)) overLink = true;
        setMouseCursor (overStrip ? juce::MouseCursor::DraggingHandCursor : overLink ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

private:
    SynthPanel& panel;
    Row& row;
    int dragIndex = -1, dragX = 0;
    bool dragging = false;
};

//==============================================================================
// The first screen: six big knobs every sound answers. Under each one, what it
// is wired to right now (the model or the defaults decide per patch).
class SynthPanel::MacroPage : public juce::Component
{
public:
    MacroPage (StacksAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts) : processor (p)
    {
        for (int k = 0; k < kNumMacros; ++k)
        {
            auto knob = std::make_unique<ParamKnob> (apvts, spec (macroParam (k)));
            knob->setAccent (colours::rowMacro);
            knob->setLarge (true);
            addAndMakeVisible (*knob);
            knobs.push_back (std::move (knob));
        }
        refreshTargets();
    }

    void refreshTargets()
    {
        targets.clear();
        for (int k = 0; k < kNumMacros; ++k)
        {
            juce::StringArray parts;
            for (int slot : processor.modulationsFor (SrcMacro1 + k))
            {
                const int target = (int) processor.apvts.getRawParameterValue (paramId (modDestParam (slot)))->load();
                const float amount = processor.apvts.getRawParameterValue (paramId (modAmountParam (slot)))->load();
                if (target <= 0) continue;
                parts.add (modTargetNames()[juce::jlimit (0, modTargetNames().size() - 1, target)] + " " + (amount >= 0 ? "+" : "") + juce::String (juce::roundToInt (amount * 100)) + "%");
            }
            targets.add (parts.isEmpty() ? juce::String ("not wired") : parts.joinIntoString ("   "));
            std::vector<KnobModulation> mods;
            for (const auto& m : processor.modulationsOnParam ((int) macroParam (k))) mods.push_back ({ m.slot, m.source, m.amount });
            knobs[(size_t) k]->setModulations (mods);
        }
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (24, 12);
        const int cols = 3, rows = 2;
        const int w = r.getWidth() / cols, h = r.getHeight() / rows;
        for (int k = 0; k < kNumMacros; ++k)
        {
            auto cell = juce::Rectangle<int> (r.getX() + (k % cols) * w, r.getY() + (k / cols) * h, w, h);
            cell.removeFromBottom (24);   // the target line
            const int side = juce::jmin (cell.getWidth(), cell.getHeight(), 180);
            knobs[(size_t) k]->setBounds (cell.withSizeKeepingCentre (side, side));
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (24, 12);
        const int cols = 3, rows = 2;
        const int w = r.getWidth() / cols, h = r.getHeight() / rows;
        g.setFont (StacksLookAndFeel::font (11.0f));
        for (int k = 0; k < kNumMacros; ++k)
        {
            auto cell = juce::Rectangle<int> (r.getX() + (k % cols) * w, r.getY() + (k / cols) * h, w, h);
            auto line = cell.removeFromBottom (24).reduced (6, 0);
            g.setColour (targets[k] == "not wired" ? colours::muted.withAlpha (0.6f) : colours::rowMacro.withAlpha (0.85f));
            g.drawFittedText (targets[k], line, juce::Justification::centredTop, 1);
        }
    }

private:
    StacksAudioProcessor& processor;
    std::vector<std::unique_ptr<ParamKnob>> knobs;
    juce::StringArray targets;
};

//==============================================================================
SynthPanel::SynthPanel (StacksAudioProcessor& p) : processor (p), apvts (p.apvts)
{
    macroPage = std::make_unique<MacroPage> (p, apvts);
    addChildComponent (*macroPage);

    setWantsKeyboardFocus (true);

    modulators = std::make_unique<ModulatorsPanel> (processor);
    modulators->onAssignRequest = [this] (int source) { beginAssign (source); };
    modulators->onAssignCancel  = [this] { endAssign(); };

    // Every control, made once from the parameter table and filed under its group.
    std::map<juce::String, std::vector<juce::Component*>> byGroup;
    std::map<juce::String, std::vector<const ParamSpec*>> advancedByGroup;
    for (const auto& spec : paramSpecs())
    {
        const juce::String group (spec.group);
        if (group == "MASTER" || group == "MACROS" || group == "MOD MATRIX" || group == "MOD ENV" || group == "FXROUTE" || group.startsWith ("LFO "))
            continue;
        if (isAdvancedParam (spec.id))
        {
            advancedByGroup[group].push_back (&spec);
            continue;
        }
        std::unique_ptr<juce::Component> control;
        if (spec.kind == ParamKind::Choice)
            control = std::make_unique<ParamChoice> (apvts, spec);
        else
        {
            auto knob = std::make_unique<ParamKnob> (apvts, spec);
            knob->setAccent (colourForGroup (group));
            knob->onAssignClick = [this] (int paramIndex) { knobClicked (paramIndex); };
            knob->onModulatorDropped = [this] (int source, int paramIndex) { processor.addModulation (source, modTargetForParam (paramIndex)); };
            knob->onRingDrag = [this] (int slot, float amount) { processor.setModulationAmount (slot, amount); };
            knob->onRemoveModulation = [this] (int slot) { processor.clearModulation (slot); };
            knob->onShowSource = [this] (int source)
            {
                if (viewMode >= 0 && viewMode != (int) rows.size() - 1) setView (-1);   // the modulators row must be on screen
                modulators->showSource (source);
            };
            if (juce::String (spec.id) == "delay_time") delayTimeKnob = knob.get();
            knobs.push_back (knob.get());
            control = std::move (knob);
        }
        byGroup[group].push_back (control.get());
        controls.push_back (std::move (control));
    }

    auto makeDisplay = [this] (const char* which, juce::Colour colour) -> juce::Component*
    {
        const juce::String w (which);
        std::unique_ptr<juce::Component> d;
        if (w == "waveA")        d = std::make_unique<WaveDisplay> (processor, 0, colour);
        else if (w == "waveB")   d = std::make_unique<WaveDisplay> (processor, 1, colour);
        else if (w == "waveC")   d = std::make_unique<WaveDisplay> (processor, 2, colour);
        else if (w == "sample")  d = std::make_unique<SampleDisplay> (processor, apvts, colour);
        else if (w == "filter")  d = std::make_unique<FilterCurve> (apvts, colour);
        else if (w == "fenv")    d = std::make_unique<EnvelopeDisplay> (apvts, "fenv", colour);
        else if (w == "aenv")    d = std::make_unique<EnvelopeDisplay> (apvts, "aenv", colour);
        if (d == nullptr) return nullptr;
        auto* raw = d.get();
        controls.push_back (std::move (d));
        return raw;
    };

    // The cards, in rows that follow the signal path.
    for (const auto& rowSpec : rowSpecs())
    {
        Row row;
        row.caption = rowSpec.caption;
        row.colour = *rowSpec.colour;
        for (const auto& ss : rowSpec.sections)
        {
            sections.push_back (std::make_unique<Section>());
            auto* section = sections.back().get();
            section->title = ss.title;
            section->columns = juce::jmax (1, ss.columns);
            if (juce::String (ss.title) == kModulatorsSection)
            {
                section->colour = colours::rowMovement;
                section->custom = modulators.get();
            }
            else
            {
                section->colour = colourForGroup (ss.groups.front());
                for (auto* g : ss.groups)
                {
                    for (auto* c : byGroup[g]) section->cells.push_back (c);
                    for (auto* a : advancedByGroup[g]) section->advanced.push_back (a);
                }
                if (ss.display != nullptr)
                    section->display = makeDisplay (ss.display, section->colour);
            }
            if (! section->advanced.empty())
            {
                section->moreButton = std::make_unique<juce::TextButton> (juce::String::fromUTF8 ("\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2"));
                section->moreButton->setTooltip ("More " + juce::String (ss.title).toLowerCase() + " settings");
                styleAsTab (*section->moreButton);
                auto* sectionPtr = section;
                section->moreButton->onClick = [this, sectionPtr] { showAdvanced (*sectionPtr); };
            }
            row.sections.push_back (section);
        }
        int w = kBand + kPad, h = 0;
        for (auto* s : row.sections) { w += s->naturalWidth() + rowGap (row); h = juce::jmax (h, s->naturalHeight()); }
        row.naturalWidth = w - rowGap (row) + kPad;
        row.naturalHeight = h;
        rows.push_back (std::move (row));
    }

    // The containers come last, once the rows have their final home in the
    // vector (each holds a reference to its row); the controls move into them.
    for (auto& row : rows)
    {
        row.container = std::make_unique<RowContainer> (*this, row);
        addAndMakeVisible (*row.container);
        for (auto* section : row.sections)
        {
            if (section->custom != nullptr)  row.container->addAndMakeVisible (*section->custom);
            for (auto* c : section->cells)   row.container->addAndMakeVisible (*c);
            if (section->display != nullptr) row.container->addAndMakeVisible (*section->display);
            if (section->moreButton != nullptr) row.container->addAndMakeVisible (*section->moreButton);
        }
    }

    sortEffectsRow();
    lastEffectOrder = effectOrderSignature();

    // View tabs: everything, or one row filling the panel.
    auto addViewButton = [this] (const juce::String& text, int mode, juce::Colour colour)
    {
        auto b = std::make_unique<juce::TextButton> (text);
        styleAsTab (*b);
        b->setColour (juce::TextButton::buttonOnColourId, colour);
        b->onClick = [this, mode] { setView (mode); };
        addAndMakeVisible (*b);
        viewButtons.push_back (std::move (b));
    };
    addViewButton ("MACROS", -2, colours::rowMacro);
    addViewButton ("ALL", -1, colours::accent);
    for (int i = 0; i < (int) rows.size(); ++i)
        addViewButton (rows[(size_t) i].caption, i, rows[(size_t) i].colour);
    setView (-1);

    for (int e = 0; e < kNumFx; ++e)
    {
        apvts.addParameterListener (paramId (fxPosParam (e)), this);
        apvts.addParameterListener (paramId (fxOnParam (e)), this);
    }
    for (int i = 0; i < kNumModSlots; ++i)
    {
        apvts.addParameterListener (paramId (modSourceParam (i)), this);
        apvts.addParameterListener (paramId (modDestParam (i)), this);
        apvts.addParameterListener (paramId (modAmountParam (i)), this);
    }
    refreshModulationDisplay();
    processor.labBroadcaster.addChangeListener (this);
    seenTweakSerial = processor.lab().tweakSerial;
    startTimerHz (30);
}

void SynthPanel::timerCallback()
{
    // Knobs a sync setting overrides read as switched off.
    if (delayTimeKnob != nullptr)
    {
        const bool synced = (int) apvts.getRawParameterValue (paramId (P::delay_sync))->load() != 0;
        if (delayTimeKnob->isEnabled() == synced) delayTimeKnob->setEnabled (! synced);
    }
    modulators->refreshEnabled();

    if (processor.calmMode())
    {
        for (auto* knob : knobs) knob->clearLiveValue();
        return;
    }
    for (auto* knob : knobs)
        knob->setLiveValue (processor.liveValue (knob->parameterIndex()));
}

void SynthPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const auto& lab = processor.lab();
    if (lab.tweakSerial == seenTweakSerial)
        return;
    seenTweakSerial = lab.tweakSerial;
    for (auto* knob : knobs)
        if (std::find (lab.tweakedParams.begin(), lab.tweakedParams.end(), knob->parameterIndex()) != lab.tweakedParams.end())
            knob->flash();
}

SynthPanel::~SynthPanel()
{
    stopTimer();
    processor.labBroadcaster.removeChangeListener (this);
    cancelPendingUpdate();
    for (int e = 0; e < kNumFx; ++e)
    {
        apvts.removeParameterListener (paramId (fxPosParam (e)), this);
        apvts.removeParameterListener (paramId (fxOnParam (e)), this);
    }
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
    const auto sig = effectOrderSignature();
    if (sig != lastEffectOrder)            // a loaded patch or a drag changed the chain
    {
        lastEffectOrder = sig;
        sortEffectsRow();
        resized();
        repaint();
    }
}

int SynthPanel::effectIndexFor (const juce::String& title)
{
    static const char* titles[kNumFx] = { "DISTORTION", "EQ", "CHORUS", "HYPER", "TAPE", "DELAY", "REVERB", "COMPRESSOR" };
    for (int e = 0; e < kNumFx; ++e) if (title == titles[e]) return e;
    return -1;
}

int SynthPanel::rowGap (const Row& row) const
{
    return row.caption == "EFFECTS" ? 22 : kGap;
}

juce::String SynthPanel::effectOrderSignature() const
{
    juce::String s;
    for (int e = 0; e < kNumFx; ++e)
        s << (int) apvts.getRawParameterValue (paramId (fxPosParam (e)))->load() << ":" << (int) apvts.getRawParameterValue (paramId (fxOnParam (e)))->load() << ",";
    return s;
}

void SynthPanel::sortEffectsRow()
{
    for (auto& row : rows)
    {
        if (row.caption != "EFFECTS") continue;
        auto posOf = [this] (const Section* s)
        {
            const int e = effectIndexFor (s->title);
            return e < 0 ? 100 : (int) apvts.getRawParameterValue (paramId (fxPosParam (e)))->load() * 10 + e;   // ties break by effect index
        };
        std::stable_sort (row.sections.begin(), row.sections.end(), [&] (const Section* a, const Section* b) { return posOf (a) < posOf (b); });
    }
}

// The card at `fromIndex` (in the row's current order) lands at `toIndex`; every
// effect gets a fresh position 0..7 so the order is always a valid permutation.
void SynthPanel::moveEffect (int fromIndex, int toIndex)
{
    for (auto& row : rows)
    {
        if (row.caption != "EFFECTS") continue;
        std::vector<Section*> order = row.sections;
        const int n = (int) order.size();
        if (fromIndex < 0 || fromIndex >= n) return;
        toIndex = juce::jlimit (0, n - 1, toIndex);
        if (fromIndex == toIndex) return;
        auto* moved = order[(size_t) fromIndex];
        order.erase (order.begin() + fromIndex);
        order.insert (order.begin() + toIndex, moved);
        processor.pushUndoSnapshot();
        for (int i = 0; i < n; ++i)
        {
            const int e = effectIndexFor (order[(size_t) i]->title);
            if (e < 0) continue;
            if (auto* param = apvts.getParameter (paramId (fxPosParam (e))))
                param->setValueNotifyingHost (param->convertTo0to1 ((float) i));
        }
        row.sections = order;
        lastEffectOrder = effectOrderSignature();
        resized();
        repaint();
    }
}

void SynthPanel::toggleEffect (int e)
{
    if (e < 0 || e >= kNumFx) return;
    if (auto* param = apvts.getParameter (paramId (fxOnParam (e))))
    {
        const bool on = param->getValue() > 0.5f;
        processor.pushUndoSnapshot();
        param->setValueNotifyingHost (on ? 0.0f : 1.0f);
    }
    lastEffectOrder = effectOrderSignature();
    repaint();
}

void SynthPanel::refreshModulationDisplay()
{
    if (macroPage) macroPage->refreshTargets();
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
    repaint();
}

void SynthPanel::endAssign()
{
    assigningSource = -1;
    for (auto* knob : knobs)
        knob->setAssignMode (false, {});
    modulators->setAssigning (-1);
    repaint();
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
        viewButtons[(size_t) i]->setToggleState (i - 2 == rowIndex, juce::dontSendNotification);
    for (int i = 0; i < (int) rows.size(); ++i)
        rows[(size_t) i].container->setVisible (viewMode == -1 || viewMode == i);
    macroPage->setVisible (viewMode == -2);
    if (viewMode == -2) macroPage->refreshTargets();
    resized();
    repaint();
}

// Places a row's cards across `width`: each card gets its natural width plus
// a share of the leftover (displays and the modulators area take more), cells
// never wider than kMaxCell; what is still left spreads the cards apart. Every
// card is `height` tall: the display takes the slack when there is one.
void SynthPanel::layoutRow (Row& row, int width, int height)
{
    const int n = (int) row.sections.size();
    std::vector<int> widths (row.sections.size());
    const int gap = rowGap (row);
    int natural = kBand + kPad + kPad + (n - 1) * gap, totalWeight = 0;
    for (int i = 0; i < n; ++i) { widths[(size_t) i] = row.sections[(size_t) i]->naturalWidth(); natural += widths[(size_t) i]; totalWeight += row.sections[(size_t) i]->weight(); }

    // Two passes: share the leftover by weight, cap the cells, share what the caps freed.
    int leftover = juce::jmax (0, width - natural);
    for (int pass = 0; pass < 2 && leftover > 0 && totalWeight > 0; ++pass)
    {
        int given = 0, weightNext = 0;
        for (int i = 0; i < n; ++i)
        {
            auto* s = row.sections[(size_t) i];
            if (s->weight() == 0) continue;
            int extra = leftover * s->weight() / totalWeight;
            const int cap = s->custom != nullptr ? 2 * kPad + 1100 : 2 * kPad + s->columns * kMaxCell;
            const bool capped = widths[(size_t) i] + extra >= cap;
            if (capped) extra = juce::jmax (0, cap - widths[(size_t) i]);
            widths[(size_t) i] += extra;
            given += extra;
            if (! capped) weightNext += s->weight();
        }
        leftover -= given;
        totalWeight = weightNext;
        if (given == 0) break;
    }
    const int spread = n > 1 ? leftover / (n - 1) : 0;   // whatever the caps left over spaces the cards out

    int x = kBand + kPad;
    for (int i = 0; i < n; ++i)
    {
        auto* section = row.sections[(size_t) i];
        const int w = widths[(size_t) i];
        section->bounds = { x, 0, w, height };
        if (section->moreButton != nullptr)
            section->moreButton->setBounds (section->bounds.withHeight (kTitleH).removeFromRight (26).reduced (2, 1));

        auto body = section->bounds.reduced (kPad).withTrimmedTop (kTitleH - kPad);
        if (section->custom != nullptr)
        {
            section->custom->setBounds (body);
        }
        else
        {
            const int gridRows = section->rows();
            const int gridH = gridRows * kCellH;
            if (section->display != nullptr)
            {
                auto displayArea = body.removeFromTop (juce::jmax (kDisplayH, body.getHeight() - gridH - kPad));
                section->display->setBounds (displayArea);
                body.removeFromTop (kPad);
            }
            else
                body = body.withSizeKeepingCentre (body.getWidth(), juce::jmin (body.getHeight(), gridH));
            const int cellW = juce::jmax (1, body.getWidth() / section->columns);
            const int gridW = cellW * section->columns;
            const int x0 = body.getX() + (body.getWidth() - gridW) / 2;
            for (int c = 0; c < (int) section->cells.size(); ++c)
                section->cells[(size_t) c]->setBounds (x0 + (c % section->columns) * cellW, body.getY() + (c / section->columns) * kCellH, cellW, kCellH);
        }
        x += w + gap + spread;
    }
}

void SynthPanel::resized()
{
    auto area = getLocalBounds();
    auto tabs = area.removeFromTop (22);
    tabs.removeFromLeft (kBand);
    const int tabW = juce::jmin (96, tabs.getWidth() / juce::jmax (1, (int) viewButtons.size()));
    for (auto& b : viewButtons)
        b->setBounds (tabs.removeFromLeft (tabW));
    area.removeFromTop (6);
    macroPage->setBounds (area);

    std::vector<Row*> shown;
    if (viewMode < 0)                           for (auto& row : rows) shown.push_back (&row);
    else if (viewMode < (int) rows.size())      shown.push_back (&rows[(size_t) viewMode]);
    if (shown.empty())
        return;

    // The largest scale at which every row on show fits, by width and by height.
    const int availW = area.getWidth() - 2, availH = area.getHeight() - 2;
    int widest = 0, totalH = (int) (shown.size() - 1) * kGap;
    for (auto* row : shown) { widest = juce::jmax (widest, row->naturalWidth); totalH += row->naturalHeight; }
    const float maxScale = viewMode >= 0 ? 2.0f : 1.35f;
    const float scale = juce::jlimit (0.5f, maxScale, juce::jmin ((float) availW / (float) widest, (float) availH / (float) totalH));

    // Leftover height goes to the rows that can use it: those with displays
    // first, the modulators area too.
    const int rowW = (int) ((float) availW / scale);
    int spareH = (int) ((float) availH / scale) - totalH;
    std::vector<int> heights;
    int stretchy = 0;
    for (auto* row : shown)
    {
        bool can = false;
        for (auto* s : row->sections) can = can || s->display != nullptr || s->custom != nullptr;
        heights.push_back (row->naturalHeight);
        if (can) ++stretchy;
    }
    if (spareH > 0 && stretchy > 0)
    {
        const int each = juce::jmin (spareH / stretchy, viewMode >= 0 ? 150 : 70);
        for (size_t i = 0; i < shown.size(); ++i)
        {
            bool can = false;
            for (auto* s : shown[i]->sections) can = can || s->display != nullptr || s->custom != nullptr;
            if (can) { heights[i] += each; spareH -= each; }
        }
    }

    // Lay every row out in its own container at natural size, then place the
    // containers at the shared scale, stacked and centred in any remaining slack.
    for (auto& row : rows)
        if (std::find (shown.begin(), shown.end(), &row) == shown.end())
            layoutRow (row, row.naturalWidth, row.naturalHeight);
    float y = (float) area.getY() + juce::jmax (0.0f, (float) spareH * scale * 0.5f);
    const int gapExtra = viewMode < 0 && shown.size() > 1 && spareH > 0 ? juce::jmin (spareH / (int) (shown.size() - 1), 10) : 0;
    for (size_t i = 0; i < shown.size(); ++i)
    {
        auto* row = shown[i];
        layoutRow (*row, rowW, heights[i]);
        row->container->setTopLeftPosition (0, 0);
        row->container->setSize (rowW, heights[i]);
        row->container->setTransform (juce::AffineTransform::scale (scale).translated ((float) area.getX() + 1.0f, y));
        y += ((float) heights[i] + (float) (kGap + gapExtra)) * scale;
    }
}

void SynthPanel::paint (juce::Graphics& g)
{
    if (assigningSource >= 0)
    {
        const auto colour = modSourceColour (assigningSource);
        const auto text = "Click a knob to modulate it with " + modSourceNames()[assigningSource] + "   (Esc cancels)";
        g.setFont (StacksLookAndFeel::font (11.5f, true));
        auto pill = getLocalBounds().removeFromTop (22).toFloat();
        const float w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 24.0f;
        pill = pill.removeFromRight (w + 4.0f).withHeight (20.0f);
        g.setColour (colour.withAlpha (0.2f));
        g.fillRoundedRectangle (pill, 10.0f);
        g.setColour (colour);
        g.drawRoundedRectangle (pill.reduced (0.5f), 10.0f, 1.0f);
        g.drawText (text, pill.toNearestInt(), juce::Justification::centred);
    }
}

} // namespace stacks
