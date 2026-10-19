#pragma once

#include <JuceHeader.h>

namespace stacks
{

// The plug-in's look: graphite surfaces, one display typeface, flat knobs
// with a coloured value arc and pointer, flat buttons, underlined tabs.
class StacksLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StacksLookAndFeel();

    static juce::String displayFontName();                    // the typeface the whole UI uses
    static juce::Font font (float height, bool bold = false); // a Font in that typeface

    // Space kept around a rotary knob inside its slider bounds (room for the
    // modulation rings). ParamKnob mirrors this to place the rings.
    static constexpr float kKnobMargin = 7.0f;

    // Section cards and small framed displays share this treatment.
    static void drawCard (juce::Graphics&, juce::Rectangle<float>, float corner = 6.0f);
    static void drawInset (juce::Graphics&, juce::Rectangle<float>, float corner = 4.0f);

    // The knob in two layers, so a skin can replace the body (cap + pointer)
    // and keep the coloured arc, or the other way round. `bounds` is the whole
    // slider area; the margin is taken inside. Angles as JUCE's rotary slider.
    static void drawKnobBody (juce::Graphics&, juce::Rectangle<float> bounds, float angle, bool enabled, juce::Colour fill);
    static void drawKnobArc (juce::Graphics&, juce::Rectangle<float> bounds, float startAngle, float endAngle, float angle, float zeroAngle, bool enabled, juce::Colour fill);
    // Background pieces drawn by code when the skin has no image for them.
    static void drawButtonFace (juce::Graphics&, juce::Rectangle<float>, juce::Colour base, bool on, bool highlighted, bool down);
    static void drawToggleBox (juce::Graphics&, juce::Rectangle<float> box, bool on, bool highlighted, juce::Colour tick);
    static void drawComboFace (juce::Graphics&, juce::Rectangle<float>, juce::Colour background, bool enabled, juce::Colour arrow);

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowTitleFont() override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool isScrollbarVertical,
                        int thumbStartPosition, int thumbSize, bool isMouseOver, bool isMouseDown) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

private:
    juce::Typeface::Ptr regular, bold;
};

} // namespace stacks
