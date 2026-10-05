#include "Controls.h"

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
    pick ("rowSound",    colours::rowSound);
    pick ("rowFilter",   colours::rowFilter);
    pick ("rowMovement", colours::rowMovement);
    pick ("rowSpace",    colours::rowSpace);
    pick ("rowShape",    colours::rowShape);
    pick ("rowMacro",    colours::rowMacro);
}

juce::Colour modSourceColour (int source)
{
    switch (source)
    {
        case SrcLfo1:       return juce::Colour (0xff5ec8c0);
        case SrcLfo2:       return juce::Colour (0xff7fa7d8);
        case SrcLfo3:       return juce::Colour (0xffa08cf0);
        case SrcLfo4:       return juce::Colour (0xfff08cc0);
        case SrcFilterEnv:  return juce::Colour (0xffe8775a);
        case SrcModEnv:     return juce::Colour (0xfff2a541);
        case SrcVelocity:   return juce::Colour (0xff8fd18f);
        case SrcKey:        return juce::Colour (0xffc8c86a);
        case SrcModWheel:   return juce::Colour (0xffd9a06a);
        case SrcMacro1: case SrcMacro2: case SrcMacro3: case SrcMacro4: case SrcMacro5: case SrcMacro6:
                            return juce::Colour (0xffe0c070);   // the big knobs: gold
        case SrcAftertouch: return juce::Colour (0xffc08cf0);
        case SrcRandom:     return juce::Colour (0xffa0a0a0);
        default:            return colours::muted;
    }
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
            if (std::abs (d - knob.ringRadiusFor (i)) <= 3.5f)
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
    label.setFont (juce::Font (juce::FontOptions (11.0f)));
    label.setColour (juce::Label::textColourId, colours::muted);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    if (compact)
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    else
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 15);
    slider.setTooltip (juce::String (spec.aiHint));
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, colours::text);
    slider.onValueChange = [this] { if (! modulations.empty()) repaint(); };
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, spec.id, slider);

    overlay = std::make_unique<RingOverlay> (*this);
    addAndMakeVisible (*overlay);
    overlay->toFront (false);
}

ParamKnob::~ParamKnob() = default;

juce::Rectangle<float> ParamKnob::knobBounds() const
{
    auto area = slider.getBounds();
    if (! compact)
        area.removeFromBottom (15);
    const auto bounds = area.toFloat().reduced (10.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    return juce::Rectangle<float> (2.0f * radius, 2.0f * radius).withCentre (bounds.getCentre());
}

float ParamKnob::ringRadiusFor (int which) const
{
    return knobBounds().getWidth() * 0.5f + 2.5f + 3.2f * (float) which;
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
    // assign mode the slider and label must step aside for the cell to get the click.
    slider.setInterceptsMouseClicks (! assignMode, ! assignMode);
    label.setInterceptsMouseClicks (! assignMode, ! assignMode);
    setMouseCursor (assignMode ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);

    // While assigning, the whole cell is one "Assign to <name>" button for
    // accessibility too; otherwise the slider speaks for itself.
    slider.setAccessible (! assignMode);
    label.setAccessible (! assignMode);
    setAccessible (assignMode);
    setTitle (assignMode ? "Assign to " + label.getText() : juce::String());
    invalidateAccessibilityHandler();
    repaint();
}

void ParamKnob::mouseDown (const juce::MouseEvent&)
{
    if (assignMode && onAssignClick)
        onAssignClick (paramIndex);
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

void ParamKnob::setLarge (bool large)
{
    label.setFont (juce::Font (juce::FontOptions (large ? 15.0f : 11.0f, large ? juce::Font::bold : juce::Font::plain)));
    label.setColour (juce::Label::textColourId, large ? colours::text : colours::muted);
    if (! compact) slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, large ? 80 : 58, large ? 20 : 15);
    resized();
}

void ParamKnob::clearLiveValue()
{
    if (liveNorm >= 0.0f) { liveNorm = -1.0f; repaint(); }
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (label.getFont().getHeight() > 13.0f ? 22 : 14));
    slider.setBounds (r);
    if (overlay) overlay->setBounds (r);
}

void ParamKnob::paint (juce::Graphics& g)
{
    if (assignMode || dragOver)
    {
        const auto c = dragOver ? dragColour : assignColour;
        g.setColour (c.withAlpha (dragOver ? 0.3f : 0.18f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f);
        g.setColour (c);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 1.5f);
    }

    if (modulations.empty())
        return;

    // Mirror LookAndFeel_V4's rotary geometry so the rings sit just outside the knob arc.
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
        g.setColour (modSourceColour (source).withAlpha (0.95f));
        g.strokePath (ring, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // a dot at the knob's own value, so the ring reads as "around here"
        const float a = rotary.startAngleRadians + v0 * span;
        g.fillEllipse (centre.x + ringRadius * std::sin (a) - 1.8f, centre.y - ringRadius * std::cos (a) - 1.8f, 3.6f, 3.6f);
    }

    // Live marker: where the modulation has the value right now.
    if (liveNorm >= 0.0f)
    {
        const float outer = ringRadiusFor ((int) modulations.size() - 1);
        const float a0 = rotary.startAngleRadians + v0 * span;
        const float a1 = rotary.startAngleRadians + juce::jlimit (0.0f, 1.0f, liveNorm) * span;
        juce::Path sweep;
        sweep.addCentredArc (centre.x, centre.y, outer, outer, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
        g.setColour (colours::text.withAlpha (0.85f));
        g.strokePath (sweep, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::text);
        g.fillEllipse (centre.x + outer * std::sin (a1) - 3.0f, centre.y - outer * std::cos (a1) - 3.0f, 6.0f, 6.0f);
    }
}

//==============================================================================
ParamChoice::ParamChoice (juce::AudioProcessorValueTreeState& apvts, const ParamSpec& spec)
{
    label.setText (spec.name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (11.0f)));
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
    label.setBounds (r.removeFromTop (14));
    combo.setBounds (r.withSizeKeepingCentre (r.getWidth() - 2, 22));
}

} // namespace stacks
