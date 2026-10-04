#include "SynthPanel.h"

namespace stacks
{

//==============================================================================
ParamKnob::ParamKnob (juce::AudioProcessorValueTreeState& apvts, const ParamSpec& spec, bool compact)
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
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, spec.id, slider);
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

//==============================================================================
ParamChoice::ParamChoice (juce::AudioProcessorValueTreeState& apvts, const ParamSpec& spec)
{
    label.setText (spec.name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (11.0f)));
    label.setColour (juce::Label::textColourId, colours::muted);
    addAndMakeVisible (label);

    combo.addItemList (*spec.choices, 1);
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

//==============================================================================
SynthPanel::SynthPanel (juce::AudioProcessorValueTreeState& apvts)
{
    for (const auto& spec : paramSpecs())
    {
        if (juce::String (spec.group) == "MASTER")
            continue; // lives in the header

        if (sections.empty() || sections.back().title != spec.group)
            sections.push_back ({ spec.group, {}, {} });

        std::unique_ptr<juce::Component> control;
        int cellWidth = kCell;
        if (spec.kind == ParamKind::Choice)
        {
            control = std::make_unique<ParamChoice> (apvts, spec);
            cellWidth = kChoiceCell;
        }
        else
        {
            control = std::make_unique<ParamKnob> (apvts, spec);
        }

        addAndMakeVisible (*control);
        sections.back().controls.emplace_back (control.get(), cellWidth);
        controls.push_back (std::move (control));
    }
}

int SynthPanel::layout (int width, bool apply)
{
    const int rowH = kTitleH + kCellH + kPad;
    int x = kPad, y = kPad;

    for (auto& section : sections)
    {
        int w = 2 * kPad;
        for (const auto& [component, cellWidth] : section.controls)
            w += cellWidth;

        if (x > kPad && x + w > width - kPad)
        {
            x = kPad;
            y += rowH + kGap;
        }

        section.bounds = { x, y, w, rowH };
        if (apply)
        {
            int cx = x + kPad;
            for (const auto& [component, cellWidth] : section.controls)
            {
                component->setBounds (cx, y + kTitleH, cellWidth, kCellH);
                cx += cellWidth;
            }
        }

        x += w + kGap;
    }

    return y + rowH + kPad;
}

int SynthPanel::heightForWidth (int width)
{
    return layout (width, false);
}

void SynthPanel::resized()
{
    layout (getWidth(), true);
}

void SynthPanel::paint (juce::Graphics& g)
{
    for (const auto& section : sections)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (section.bounds.toFloat(), 6.0f);
        g.setColour (colours::accent);
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText (section.title, section.bounds.withHeight (kTitleH).reduced (kPad, 0),
                    juce::Justification::centredLeft);
    }
}

} // namespace stacks
