#pragma once

#include <JuceHeader.h>

#include "Parameters.h"

namespace stacks
{

namespace colours
{
    const juce::Colour background { 0xff1b1d22 };
    const juce::Colour panel      { 0xff24272e };
    const juce::Colour card       { 0xff2b2f37 };
    const juce::Colour accent     { 0xfff2a541 };
    const juce::Colour accentDim  { 0xff4d3a1f };
    const juce::Colour text       { 0xffe8e8e8 };
    const juce::Colour muted      { 0xff8a8f99 };
}

// One colour per modulation source, used for rings, editors and tabs.
juce::Colour modSourceColour (int source);

// A labelled rotary knob bound to one parameter. Shows modulation rings and
// can be clicked as a target while a modulator is being assigned.
class ParamKnob : public juce::Component
{
public:
    // compact = no value read-out underneath (used in the header)
    ParamKnob (juce::AudioProcessorValueTreeState&, const ParamSpec&, bool compact = false);
    void setAccent (juce::Colour);
    void setModulations (std::vector<std::pair<int, float>> sourceAndAmount); // repaints the rings
    void setAssignMode (bool on, juce::Colour sourceColour);
    int parameterIndex() const { return paramIndex; }

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    std::function<void (int paramIndex)> onAssignClick;

private:
    juce::Label label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::vector<std::pair<int, float>> modulations;
    int paramIndex = -1;
    bool compact = false, assignMode = false;
    juce::Colour assignColour;
};

// A labelled drop-down bound to one Choice parameter.
class ParamChoice : public juce::Component
{
public:
    ParamChoice (juce::AudioProcessorValueTreeState&, const ParamSpec&);
    void resized() override;

private:
    juce::Label label;
    juce::ComboBox combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

} // namespace stacks
