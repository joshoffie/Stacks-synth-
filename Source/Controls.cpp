#include "Controls.h"

namespace stacks
{

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
        case SrcAftertouch: return juce::Colour (0xffc08cf0);
        case SrcRandom:     return juce::Colour (0xffa0a0a0);
        default:            return colours::muted;
    }
}

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
}

void ParamKnob::setAccent (juce::Colour c)
{
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
}

void ParamKnob::setModulations (std::vector<std::pair<int, float>> sourceAndAmount)
{
    modulations = std::move (sourceAndAmount);
    repaint();
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

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

void ParamKnob::paint (juce::Graphics& g)
{
    if (assignMode)
    {
        g.setColour (assignColour.withAlpha (0.18f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f);
        g.setColour (assignColour);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 1.5f);
    }

    if (modulations.empty())
        return;

    // Mirror LookAndFeel_V4's rotary geometry so the rings sit just outside the knob arc.
    auto area = slider.getBounds();
    if (! compact)
        area.removeFromBottom (15);
    const auto bounds = area.toFloat().reduced (10.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto rotary = slider.getRotaryParameters();
    const float span = rotary.endAngleRadians - rotary.startAngleRadians;
    const float v0 = (float) slider.valueToProportionOfLength (slider.getValue());

    float ringRadius = radius + 2.5f;
    for (const auto& [source, amount] : modulations)
    {
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
        ringRadius += 3.2f;
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
