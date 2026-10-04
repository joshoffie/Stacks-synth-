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

// A labelled rotary knob bound to one parameter.
class ParamKnob : public juce::Component
{
public:
    // compact = no value read-out underneath (used in the header)
    ParamKnob (juce::AudioProcessorValueTreeState&, const ParamSpec&, bool compact = false);
    void resized() override;

private:
    juce::Label label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
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

// All the synth controls, grouped into sections and flowed into rows.
class SynthPanel : public juce::Component
{
public:
    static constexpr int kCell = 60, kChoiceCell = 82, kCellH = 86, kTitleH = 16, kPad = 6, kGap = 6;

    explicit SynthPanel (juce::AudioProcessorValueTreeState&);
    void resized() override;
    void paint (juce::Graphics&) override;
    int heightForWidth (int width);

private:
    struct Section
    {
        juce::String title;
        std::vector<std::pair<juce::Component*, int>> controls; // component, width
        juce::Rectangle<int> bounds;
    };

    int layout (int width, bool apply);

    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<Section> sections;
};

} // namespace stacks
