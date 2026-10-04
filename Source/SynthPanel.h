#pragma once

#include <JuceHeader.h>
#include <array>

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
    void setAccent (juce::Colour);
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

// The six modulation slots as a compact source -> destination x amount table.
class ModMatrixPanel : public juce::Component
{
public:
    static constexpr int kWidth = 616;
    ModMatrixPanel (juce::AudioProcessorValueTreeState&, juce::Colour accent);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    struct Slot
    {
        juce::ComboBox source, dest;
        juce::Slider amount;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment, destAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    };
    std::array<Slot, kNumModSlots> slots;
};

// All the synth controls in fixed, captioned rows that follow the signal path.
class SynthPanel : public juce::Component
{
public:
    static constexpr int kCell = 60, kChoiceCell = 82, kCellH = 86, kTitleH = 16, kPad = 6, kGap = 6, kBand = 20;

    explicit SynthPanel (juce::AudioProcessorValueTreeState&);
    void resized() override;
    void paint (juce::Graphics&) override;
    int preferredHeight() const;

private:
    struct Section
    {
        juce::String title;
        juce::Colour colour;
        std::vector<std::pair<juce::Component*, int>> controls; // component, width
        std::vector<const ParamSpec*> advanced;                  // shown in a pop-out
        std::unique_ptr<juce::TextButton> moreButton;
        juce::Rectangle<int> bounds;
    };
    struct Row
    {
        juce::String caption;
        juce::Colour colour;
        std::vector<Section*> sections;
        juce::Rectangle<int> bounds;
    };

    Section* findSection (const juce::String& title);
    void showAdvanced (Section&);

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<std::unique_ptr<Section>> sections;
    std::vector<Row> rows;
};

} // namespace stacks
