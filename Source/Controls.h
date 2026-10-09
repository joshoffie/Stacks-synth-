#pragma once

#include <JuceHeader.h>

#include "Parameters.h"

namespace stacks
{

// The palette: cool graphite surfaces, one ice-blue accent for the chrome,
// and one hue per stage of the signal path for the knobs. Mutable so a theme
// file can recolour the whole UI at startup (see loadThemeFile); every
// component reads these when it paints.
namespace colours
{
    inline juce::Colour background { 0xff111316 };
    inline juce::Colour panel      { 0xff191c20 };
    inline juce::Colour card       { 0xff22262b };
    inline juce::Colour accent     { 0xff5cb8ff };
    inline juce::Colour accentDim  { 0xff1d3447 };
    inline juce::Colour text       { 0xffe8ebee };
    inline juce::Colour muted      { 0xff7c8591 };
    inline juce::Colour outline    { 0xff2c3137 };

    // One hue per row of the panel (knob colour = row).
    inline juce::Colour rowSound    { 0xfff5a524 }; // amber
    inline juce::Colour rowFilter   { 0xffff6f61 }; // coral
    inline juce::Colour rowMovement { 0xff3fd1c4 }; // teal
    inline juce::Colour rowSpace    { 0xff5c9dff }; // blue
    inline juce::Colour rowShape    { 0xffc07cf0 }; // violet
    inline juce::Colour rowMacro    { 0xffffd166 }; // gold
    inline juce::Colour rowSample   { 0xff9ad17b }; // green
}

// Reads a theme file: a JSON object of hex colours ("rrggbb" or "aarrggbb")
// under any of the keys background, panel, card, accent, accentDim, text,
// muted, outline, rowSound, rowFilter, rowMovement, rowSpace, rowShape,
// rowMacro, rowSample. Missing keys keep the default. Call before any window
// is created.
void loadThemeFile (const juce::File&);

// One colour per modulation source, used for rings, editors and tabs.
juce::Colour modSourceColour (int source);

// Marks a TextButton as a tab: no box, just the caption, underlined in its
// colour when it is the selected one.
void styleAsTab (juce::Button&);

// One connection shown on a knob.
struct KnobModulation { int slot, source; float amount; };

// A labelled rotary knob bound to one parameter. The caption shows the name,
// and the value while the mouse is over it. Shows modulation rings (drag a
// ring to change its depth), accepts a modulator dropped on it, and can be
// clicked as a target while a modulator is being assigned.
class ParamKnob : public juce::Component,
                  public juce::DragAndDropTarget,
                  private juce::Timer
{
public:
    // compact = no caption at all (used in the header)
    ParamKnob (juce::AudioProcessorValueTreeState&, const ParamSpec&, bool compact = false);
    ~ParamKnob() override;
    void setAccent (juce::Colour);
    void setLarge (bool);                              // bigger caption for the macro page
    void setModulations (std::vector<KnobModulation>); // repaints the rings
    void setLiveValue (float realValue);               // where the modulation has the knob right now
    void clearLiveValue();
    void setAssignMode (bool on, juce::Colour sourceColour);
    void flash();                                      // show the value in the accent colour for a moment (a tweak moved it)
    int parameterIndex() const { return paramIndex; }

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void enablementChanged() override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    std::function<void (int paramIndex)> onAssignClick;
    std::function<void (int source, int paramIndex)> onModulatorDropped;
    std::function<void (int slot, float amount)> onRingDrag;

    static constexpr int kCaptionH = 13;

private:
    class RingOverlay;
    juce::Rectangle<float> knobBounds() const;      // the rotary's own square
    float ringRadiusFor (int which) const;
    void updateCaption();
    void timerCallback() override;                  // ends a flash

    juce::Label label;
    juce::Slider slider;
    std::unique_ptr<RingOverlay> overlay;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::vector<KnobModulation> modulations;
    int paramIndex = -1;
    bool compact = false, large = false, assignMode = false, dragOver = false, showingValue = false, flashing = false;
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

// The on-screen keyboard: all 88 keys, drawn flat, filling whatever width it
// is given.
class StacksKeyboard : public juce::MidiKeyboardComponent
{
public:
    explicit StacksKeyboard (juce::MidiKeyboardState&);
    void resized() override;

protected:
    void drawWhiteNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;
    juce::String getWhiteNoteText (int midiNoteNumber) override;
};

} // namespace stacks
