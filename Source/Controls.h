#pragma once

#include <JuceHeader.h>

#include "Parameters.h"

namespace stacks
{

// The palette. Mutable so a theme file can recolour the whole UI at startup
// (see loadThemeFile); every component reads these when it paints.
namespace colours
{
    inline juce::Colour background { 0xff1b1d22 };
    inline juce::Colour panel      { 0xff24272e };
    inline juce::Colour card       { 0xff2b2f37 };
    inline juce::Colour accent     { 0xfff2a541 };
    inline juce::Colour accentDim  { 0xff4d3a1f };
    inline juce::Colour text       { 0xffe8e8e8 };
    inline juce::Colour muted      { 0xff8a8f99 };

    // One hue per row of the panel (knob colour = row).
    inline juce::Colour rowSound    { 0xfff2a541 }; // amber
    inline juce::Colour rowFilter   { 0xffe8775a }; // coral
    inline juce::Colour rowMovement { 0xff5ec8c0 }; // teal
    inline juce::Colour rowSpace    { 0xff7fa7d8 }; // blue
    inline juce::Colour rowShape    { 0xffd08ab8 }; // rose
    inline juce::Colour rowMacro    { 0xffe0c070 }; // gold
    inline juce::Colour rowSample   { 0xff9ad17b }; // green
}

// Reads a theme file: a JSON object of hex colours ("rrggbb" or "aarrggbb")
// under any of the keys background, panel, card, accent, accentDim, text,
// muted, rowSound, rowFilter, rowMovement, rowSpace, rowShape, rowMacro, rowSample.
// Missing keys keep the default. Call before any window is created.
void loadThemeFile (const juce::File&);

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
    void setLarge (bool);                              // bigger label and read-out for the macro page
    void setModulations (std::vector<KnobModulation>); // repaints the rings
    void setLiveValue (float realValue);               // where the modulation has the knob right now
    void clearLiveValue();
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
    float liveNorm = -1.0f;                          // -1 = none
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
