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

// One connection shown on a knob.
struct KnobModulation { int slot, source; float amount; };

// A labelled rotary knob bound to one parameter. Shows modulation rings (drag
// a ring to change its depth), accepts a modulator dropped on it, and can be
// clicked as a target while a modulator is being assigned.
class ParamKnob : public juce::Component,
                  public juce::DragAndDropTarget
{
public:
    // compact = no value read-out underneath (used in the header)
    ParamKnob (juce::AudioProcessorValueTreeState&, const ParamSpec&, bool compact = false);
    ~ParamKnob() override;
    void setAccent (juce::Colour);
    void setModulations (std::vector<KnobModulation>); // repaints the rings
    void setAssignMode (bool on, juce::Colour sourceColour);
    int parameterIndex() const { return paramIndex; }

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    std::function<void (int paramIndex)> onAssignClick;
    std::function<void (int source, int paramIndex)> onModulatorDropped;
    std::function<void (int slot, float amount)> onRingDrag;

private:
    class RingOverlay;
    juce::Rectangle<float> knobBounds() const;      // the rotary's own square
    float ringRadiusFor (int which) const;

    juce::Label label;
    juce::Slider slider;
    std::unique_ptr<RingOverlay> overlay;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::vector<KnobModulation> modulations;
    int paramIndex = -1;
    bool compact = false, assignMode = false, dragOver = false;
    juce::Colour assignColour, dragColour;
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
