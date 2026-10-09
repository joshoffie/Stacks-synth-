#include "Controls.h"
#include "StacksLookAndFeel.h"

namespace stacks
{

void loadThemeFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return;
    const auto parsed = juce::JSON::parse (file);   // keep the var alive: the object pointer below borrows from it
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return;
    auto pick = [obj] (const char* key, juce::Colour& target)
    {
        if (! obj->hasProperty (key))
            return;
        const auto hex = obj->getProperty (key).toString().trim().removeCharacters ("#");
        if (hex.length() == 6)      target = juce::Colour::fromString ("ff" + hex);
        else if (hex.length() == 8) target = juce::Colour::fromString (hex);
    };
    pick ("background",  colours::background);
    pick ("panel",       colours::panel);
    pick ("card",        colours::card);
    pick ("accent",      colours::accent);
    pick ("accentDim",   colours::accentDim);
    pick ("text",        colours::text);
    pick ("muted",       colours::muted);
    pick ("outline",     colours::outline);
    pick ("rowSound",    colours::rowSound);
    pick ("rowFilter",   colours::rowFilter);
    pick ("rowMovement", colours::rowMovement);
    pick ("rowSpace",    colours::rowSpace);
    pick ("rowShape",    colours::rowShape);
    pick ("rowMacro",    colours::rowMacro);
    pick ("rowSample",   colours::rowSample);
}

juce::Colour modSourceColour (int source)
{
    switch (source)
    {
        case SrcLfo1:       return juce::Colour (0xff3fd1c4);
        case SrcLfo2:       return juce::Colour (0xff5c9dff);
        case SrcLfo3:       return juce::Colour (0xffa08cf0);
        case SrcLfo4:       return juce::Colour (0xfff08cc0);
        case SrcFilterEnv:  return juce::Colour (0xffff6f61);
        case SrcModEnv:     return juce::Colour (0xfff5a524);
        case SrcVelocity:   return juce::Colour (0xff8fd18f);
        case SrcKey:        return juce::Colour (0xffc8c86a);
        case SrcModWheel:   return juce::Colour (0xffd9a06a);
        case SrcMacro1: case SrcMacro2: case SrcMacro3: case SrcMacro4: case SrcMacro5: case SrcMacro6:
                            return colours::rowMacro;
        case SrcAftertouch: return juce::Colour (0xffc08cf0);
        case SrcRandom:     return juce::Colour (0xffa0a0a0);
        default:            return colours::muted;
    }
}

void styleAsTab (juce::Button& b)
{
    b.getProperties().set ("tab", true);
    b.setClickingTogglesState (false);
}

//==============================================================================
// Sits on top of the slider and only answers to the mouse within a ring band;
// everywhere else it is transparent, so the knob still turns normally.
class ParamKnob::RingOverlay : public juce::Component
{
public:
    explicit RingOverlay (ParamKnob& k) : knob (k) { setInterceptsMouseClicks (true, false); }

    bool hitTest (int x, int y) override
    {
        return ringAt ({ (float) x, (float) y }) >= 0;
    }

    int ringAt (juce::Point<float> p) const
    {
        if (knob.modulations.empty())
            return -1;
        const auto b = knob.knobBounds();
        const auto local = p + getPosition().toFloat();          // overlay space -> knob space
        const float d = local.getDistanceFrom (b.getCentre());
        for (int i = 0; i < (int) knob.modulations.size(); ++i)
            if (std::abs (d - knob.ringRadiusFor (i)) <= 3.0f)
                return i;
        return -1;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        active = ringAt (e.position);
        if (active >= 0)
        {
            startAmount = knob.modulations[(size_t) active].amount;
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (active < 0 || active >= (int) knob.modulations.size() || ! knob.onRingDrag)
            return;
        const float amount = juce::jlimit (-1.0f, 1.0f, startAmount - (float) e.getDistanceFromDragStartY() / 150.0f);
        knob.onRingDrag (knob.modulations[(size_t) active].slot, amount);
    }

    void mouseUp (const juce::MouseEvent&) override { active = -1; setMouseCursor (juce::MouseCursor::NormalCursor); }
    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (ringAt (e.position) >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    }

private:
    ParamKnob& knob;
    int active = -1;
    float startAmount = 0.0f;
};

//==============================================================================
ParamKnob::ParamKnob (juce::AudioProcessorValueTreeState& apvts, const ParamSpec& spec, bool isCompact)
    : paramIndex (paramIndexForId (spec.id)), compact (isCompact)
{
    label.setText (spec.name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (StacksLookAndFeel::font (11.0f));
    label.setColour (juce::Label::textColourId, colours::muted);
    label.setInterceptsMouseClicks (false, false);
    label.setVisible (! compact);
    addChildComponent (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setTooltip (juce::String (spec.aiHint));
    slider.onValueChange = [this] { if (showingValue) updateCaption(); if (! modulations.empty()) repaint(); };
    slider.addMouseListener (this, false);   // enter / exit reach the cell, so the caption can switch to the value
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, spec.id, slider);

    overlay = std::make_unique<RingOverlay> (*this);
    addAndMakeVisible (*overlay);
    overlay->toFront (false);
}

ParamKnob::~ParamKnob()
{
    stopTimer();
    slider.removeMouseListener (this);
}

juce::Rectangle<float> ParamKnob::knobBounds() const
{
    const auto bounds = slider.getBounds().toFloat().reduced (StacksLookAndFeel::kKnobMargin);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    return juce::Rectangle<float> (2.0f * radius, 2.0f * radius).withCentre (bounds.getCentre());
}

float ParamKnob::ringRadiusFor (int which) const
{
    return knobBounds().getWidth() * 0.5f + 2.0f + 3.0f * (float) which;
}

bool ParamKnob::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.isInt() && isModulatableParam (paramIndex);
}

void ParamKnob::itemDragEnter (const SourceDetails& details)
{
    dragOver = true;
    dragColour = modSourceColour ((int) details.description);
    repaint();
}

void ParamKnob::itemDragExit (const SourceDetails&)
{
    dragOver = false;
    repaint();
}

void ParamKnob::itemDropped (const SourceDetails& details)
{
    dragOver = false;
    repaint();
    if (onModulatorDropped)
        onModulatorDropped ((int) details.description, paramIndex);
}

void ParamKnob::setAccent (juce::Colour c)
{
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
}

void ParamKnob::setModulations (std::vector<KnobModulation> mods)
{
    modulations = std::move (mods);
    if (modulations.empty()) liveNorm = -1.0f;
    repaint();
}

void ParamKnob::setLiveValue (float realValue)
{
    if (modulations.empty() || paramIndex < 0)
        return;
    const float norm = (float) slider.valueToProportionOfLength (juce::jlimit (slider.getMinimum(), slider.getMaximum(), (double) realValue));
    if (std::abs (norm - liveNorm) > 0.002f)
    {
        liveNorm = norm;
        repaint();
    }
}

void ParamKnob::setAssignMode (bool on, juce::Colour sourceColour)
{
    assignMode = on && isModulatableParam (paramIndex);
    assignColour = sourceColour;
    // JUCE hit-testing always descends into children that accept clicks, so in
    // assign mode the slider must step aside for the cell to get the click.
    slider.setInterceptsMouseClicks (! assignMode, ! assignMode);
    setMouseCursor (assignMode ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);

    // While assigning, the whole cell is one "Assign to <name>" button for
    // accessibility too; otherwise the slider speaks for itself.
    slider.setAccessible (! assignMode);
    setAccessible (assignMode);
    setTitle (assignMode ? "Assign to " + juce::String (spec ((P) paramIndex).name) : juce::String());
    invalidateAccessibilityHandler();
    repaint();
}

void ParamKnob::mouseDown (const juce::MouseEvent&)
{
    if (assignMode && onAssignClick)
        onAssignClick (paramIndex);
}

// The caption reads the value while the mouse is on the knob (and while it is
// being dragged), the name otherwise.
void ParamKnob::mouseEnter (const juce::MouseEvent&)
{
    showingValue = true;
    updateCaption();
}

void ParamKnob::mouseExit (const juce::MouseEvent&)
{
    if (slider.isMouseButtonDown())
        return;
    showingValue = false;
    updateCaption();
}

void ParamKnob::updateCaption()
{
    if (compact || paramIndex < 0)
        return;
    const auto& s = spec ((P) paramIndex);
    juce::Colour colour;
    if (showingValue || flashing)
    {
        auto text = slider.getTextFromValue (slider.getValue());
        const juce::String unit (s.unit);
        if (unit.isNotEmpty() && ! text.endsWith (unit) && ! text.endsWithChar ('k') && ! text.endsWith ("ms"))
            text << " " << unit;
        label.setText (text, juce::dontSendNotification);
        colour = flashing ? colours::accent : colours::text;
    }
    else
    {
        label.setText (s.name, juce::dontSendNotification);
        colour = large ? colours::text : colours::muted;
    }
    label.setColour (juce::Label::textColourId, isEnabled() ? colour : colour.withAlpha (0.35f));
}

// A tweak moved this knob: its value lights up for a couple of seconds.
void ParamKnob::flash()
{
    flashing = true;
    updateCaption();
    startTimer (2200);
}

void ParamKnob::timerCallback()
{
    stopTimer();
    flashing = false;
    updateCaption();
}

void ParamKnob::enablementChanged()
{
    updateCaption();
    repaint();
}

std::unique_ptr<juce::AccessibilityHandler> ParamKnob::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::button,
        juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this]
        {
            if (assignMode && onAssignClick)
                onAssignClick (paramIndex);
        }));
}

void ParamKnob::setLarge (bool isLarge)
{
    large = isLarge;
    label.setFont (StacksLookAndFeel::font (large ? 14.0f : 11.0f, large));
    label.setColour (juce::Label::textColourId, large ? colours::text : colours::muted);
    resized();
}

void ParamKnob::clearLiveValue()
{
    if (liveNorm >= 0.0f) { liveNorm = -1.0f; repaint(); }
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    if (! compact)
        label.setBounds (r.removeFromBottom (large ? 22 : kCaptionH));
    slider.setBounds (r);
    if (overlay) overlay->setBounds (r);
}

void ParamKnob::paint (juce::Graphics& g)
{
    if (assignMode || dragOver)
    {
        const auto c = dragOver ? dragColour : assignColour;
        g.setColour (c.withAlpha (dragOver ? 0.3f : 0.16f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f);
        g.setColour (c);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 1.2f);
    }

    if (modulations.empty())
        return;

    // Mirror the look-and-feel's rotary geometry so the rings sit just outside the knob arc.
    const auto kb = knobBounds();
    const auto centre = kb.getCentre();
    const auto rotary = slider.getRotaryParameters();
    const float span = rotary.endAngleRadians - rotary.startAngleRadians;
    const float v0 = (float) slider.valueToProportionOfLength (slider.getValue());

    int which = 0;
    for (const auto& m : modulations)
    {
        const int source = m.source;
        const float amount = m.amount;
        const float ringRadius = ringRadiusFor (which++);
        float from = v0, to = v0;
        if (isBipolarSource (source)) { from = v0 - std::abs (amount); to = v0 + std::abs (amount); }
        else                          { to = v0 + amount; }
        from = juce::jlimit (0.0f, 1.0f, from);
        to = juce::jlimit (0.0f, 1.0f, to);
        if (from > to) std::swap (from, to);

        juce::Path ring;
        ring.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                            rotary.startAngleRadians + from * span, rotary.startAngleRadians + to * span, true);
        g.setColour (modSourceColour (source).withAlpha (isEnabled() ? 0.95f : 0.3f));   // dim with the knob when a sync setting overrides it
        g.strokePath (ring, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // a dot at the knob's own value, so the ring reads as "around here"
        const float a = rotary.startAngleRadians + v0 * span;
        g.fillEllipse (centre.x + ringRadius * std::sin (a) - 1.6f, centre.y - ringRadius * std::cos (a) - 1.6f, 3.2f, 3.2f);
    }

    // Live marker: where the modulation has the value right now.
    if (liveNorm >= 0.0f && isEnabled())
    {
        const float outer = ringRadiusFor ((int) modulations.size() - 1);
        const float a0 = rotary.startAngleRadians + v0 * span;
        const float a1 = rotary.startAngleRadians + juce::jlimit (0.0f, 1.0f, liveNorm) * span;
        juce::Path sweep;
        sweep.addCentredArc (centre.x, centre.y, outer, outer, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
        g.setColour (colours::text.withAlpha (0.85f));
        g.strokePath (sweep, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::text);
        g.fillEllipse (centre.x + outer * std::sin (a1) - 2.6f, centre.y - outer * std::cos (a1) - 2.6f, 5.2f, 5.2f);
    }
}

//==============================================================================
ParamChoice::ParamChoice (juce::AudioProcessorValueTreeState& apvts, const ParamSpec& spec)
{
    label.setText (spec.name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (StacksLookAndFeel::font (11.0f));
    label.setColour (juce::Label::textColourId, colours::muted);
    addAndMakeVisible (label);

    combo.addItemList (spec.choices(), 1);
    combo.setTooltip (juce::String (spec.aiHint));
    addAndMakeVisible (combo);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, spec.id, combo);
}

void ParamChoice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromBottom (ParamKnob::kCaptionH));
    combo.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 4, 96), 22));
}

//==============================================================================
StacksKeyboard::StacksKeyboard (juce::MidiKeyboardState& state)
    : juce::MidiKeyboardComponent (state, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setAvailableRange (21, 108);   // A0 to C8
    setScrollButtonsVisible (false);
    setBlackNoteLengthProportion (0.62f);
    setBlackNoteWidthProportion (0.6f);
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff0b0c0e));
    setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
}

void StacksKeyboard::resized()
{
    setKeyWidth (juce::jmax (4.0f, (float) getWidth() / 52.0f));   // 52 white keys across the full width
    juce::MidiKeyboardComponent::resized();
}

void StacksKeyboard::drawWhiteNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                                    juce::Colour, juce::Colour)
{
    auto key = area.reduced (0.5f, 0.0f).withTrimmedBottom (1.0f);
    juce::Colour top (0xffe4e6ea), bottom (0xffc9ccd2);
    if (isDown)      { top = colours::accent.brighter (0.2f); bottom = colours::accent; }
    else if (isOver) { top = juce::Colour (0xfff2f3f5); bottom = juce::Colour (0xffd9dce1); }
    juce::ColourGradient grad (top, 0.0f, key.getY(), bottom, 0.0f, key.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (key, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawLine (key.getRight(), key.getY(), key.getRight(), key.getBottom(), 1.0f);

    const auto text = getWhiteNoteText (midiNoteNumber);
    if (text.isNotEmpty())
    {
        g.setColour (isDown ? juce::Colours::white : juce::Colour (0xff6b7079));
        g.setFont (StacksLookAndFeel::font (juce::jmin (9.5f, key.getWidth() * 0.45f)));
        g.drawText (text, key.withTrimmedBottom (2.0f).toNearestInt(), juce::Justification::centredBottom, false);
    }
}

void StacksKeyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    auto key = area.reduced (0.5f, 0.0f);
    juce::Colour top (0xff33373d), bottom (0xff15171a);
    if (isDown)      { top = colours::accent; bottom = colours::accent.darker (0.4f); }
    else if (isOver) { top = juce::Colour (0xff454a52); bottom = juce::Colour (0xff22252a); }
    g.setColour (juce::Colour (0xff0b0c0e));
    g.fillRoundedRectangle (key, 2.0f);
    juce::ColourGradient grad (top, 0.0f, key.getY(), bottom, 0.0f, key.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (key.reduced (1.0f, 0.0f).withTrimmedBottom (2.0f), 1.5f);
}

juce::String StacksKeyboard::getWhiteNoteText (int midiNoteNumber)
{
    return midiNoteNumber % 12 == 0 ? "C" + juce::String (midiNoteNumber / 12 - 1) : juce::String();
}

} // namespace stacks
