#include "SynthPanel.h"

namespace stacks
{

namespace
{
    // Row colours: one hue per stage of the signal path, so a knob's colour
    // says what part of the synth it belongs to.
    const juce::Colour kSound    { 0xfff2a541 }; // amber
    const juce::Colour kFilter   { 0xffe8775a }; // coral
    const juce::Colour kMovement { 0xff5ec8c0 }; // teal
    const juce::Colour kMatrix   { 0xffa08cf0 }; // violet
    const juce::Colour kSpace    { 0xff7fa7d8 }; // blue

    struct RowSpec { const char* caption; juce::Colour colour; std::vector<const char*> groups; };

    const std::vector<RowSpec>& rowSpecs()
    {
        static const std::vector<RowSpec> specs = {
            { "SOUND",    kSound,    { "OSC A", "OSC B", "MIX" } },
            { "FILTER",   kFilter,   { "FILTER", "FILTER ENV", "AMP ENV" } },
            { "MOVEMENT", kMovement, { "MOD ENV", "LFO 1", "LFO 2", "VOICE" } },
            { "MATRIX",   kMatrix,   { "MOD MATRIX" } },
            { "SPACE",    kSpace,    { "CHORUS", "DELAY", "REVERB" } },
        };
        return specs;
    }

    juce::Colour colourForGroup (const juce::String& group)
    {
        for (const auto& row : rowSpecs())
            for (auto* g : row.groups)
                if (group == g)
                    return row.colour;
        return colours::accent;
    }
}

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

void ParamKnob::setAccent (juce::Colour c)
{
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
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
ModMatrixPanel::ModMatrixPanel (juce::AudioProcessorValueTreeState& apvts, juce::Colour accent)
{
    for (int i = 0; i < kNumModSlots; ++i)
    {
        auto& s = slots[(size_t) i];
        s.source.addItemList (modSourceNames(), 1);
        s.dest.addItemList (modDestNames(), 1);
        s.source.setTooltip ("Slot " + juce::String (i + 1) + " source");
        s.dest.setTooltip ("Slot " + juce::String (i + 1) + " destination");
        s.amount.setSliderStyle (juce::Slider::LinearHorizontal);
        s.amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.amount.setPopupDisplayEnabled (true, false, this);
        s.amount.setTooltip ("Slot " + juce::String (i + 1) + " amount, -1..1 (Pitch x12 semitones, Filter x5 octaves)");
        s.amount.setColour (juce::Slider::trackColourId, accent);
        s.amount.setColour (juce::Slider::thumbColourId, colours::text);
        addAndMakeVisible (s.source);
        addAndMakeVisible (s.dest);
        addAndMakeVisible (s.amount);
        s.sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramId (modSourceParam (i)), s.source);
        s.destAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramId (modDestParam (i)), s.dest);
        s.amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId (modAmountParam (i)), s.amount);
    }
}

void ModMatrixPanel::paint (juce::Graphics& g)
{
    // Thin arrows between source and destination make the rows read as "A -> B".
    g.setColour (colours::muted);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    for (const auto& s : slots)
        g.drawText (juce::String::fromUTF8 ("\xe2\x86\x92"), s.source.getRight(), s.source.getY(), s.dest.getX() - s.source.getRight(), s.source.getHeight(),
                    juce::Justification::centred);
}

void ModMatrixPanel::resized()
{
    const int rowH = 26, columnGap = 14;
    const int columnWidth = (getWidth() - columnGap) / 2;

    for (int i = 0; i < kNumModSlots; ++i)
    {
        auto& s = slots[(size_t) i];
        const int column = i / 3, rowIndex = i % 3;
        auto r = juce::Rectangle<int> (column * (columnWidth + columnGap), 4 + rowIndex * rowH, columnWidth, rowH).reduced (0, 2);
        s.source.setBounds (r.removeFromLeft (96));
        r.removeFromLeft (16); // arrow
        s.dest.setBounds (r.removeFromLeft (100));
        r.removeFromLeft (4);
        s.amount.setBounds (r);
    }
}

//==============================================================================
// The pop-out holding a section's advanced parameters.
class AdvancedBox : public juce::Component
{
public:
    AdvancedBox (juce::AudioProcessorValueTreeState& apvts, const juce::String& title, juce::Colour colour,
                 const std::vector<const ParamSpec*>& specs)
    {
        heading.setText (title + "  -  more", juce::dontSendNotification);
        heading.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        heading.setColour (juce::Label::textColourId, colour);
        addAndMakeVisible (heading);

        for (auto* spec : specs)
        {
            std::unique_ptr<juce::Component> c;
            if (spec->kind == ParamKind::Choice)
                c = std::make_unique<ParamChoice> (apvts, *spec);
            else
            {
                auto knob = std::make_unique<ParamKnob> (apvts, *spec);
                knob->setAccent (colour);
                c = std::move (knob);
            }
            addAndMakeVisible (*c);
            controls.push_back (std::move (c));
        }
        setSize (12 + (int) controls.size() * SynthPanel::kCell + 12, 20 + SynthPanel::kCellH + 10);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::panel);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 6);
        heading.setBounds (r.removeFromTop (18));
        for (auto& c : controls)
            c->setBounds (r.removeFromLeft (SynthPanel::kCell));
    }

private:
    juce::Label heading;
    std::vector<std::unique_ptr<juce::Component>> controls;
};

SynthPanel::SynthPanel (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    // One section per parameter group, controls generated from the table...
    for (const auto& spec : paramSpecs())
    {
        const juce::String group (spec.group);
        if (group == "MASTER")
            continue; // lives in the header

        auto* section = findSection (group);
        if (section == nullptr)
        {
            sections.push_back (std::make_unique<Section>());
            section = sections.back().get();
            section->title = group;
            section->colour = colourForGroup (group);

            if (group == "MOD MATRIX")
            {
                auto matrix = std::make_unique<ModMatrixPanel> (apvts, section->colour);
                addAndMakeVisible (*matrix);
                section->controls.emplace_back (matrix.get(), ModMatrixPanel::kWidth);
                controls.push_back (std::move (matrix));
            }
        }
        if (group == "MOD MATRIX")
            continue; // handled by the matrix component

        if (isAdvancedParam (spec.id))
        {
            section->advanced.push_back (&spec);
            if (section->moreButton == nullptr)
            {
                section->moreButton = std::make_unique<juce::TextButton> ("more");
                section->moreButton->setTooltip ("More " + group.toLowerCase() + " options");
                section->moreButton->setColour (juce::TextButton::buttonColourId, colours::card);
                auto* sectionPtr = section;
                section->moreButton->onClick = [this, sectionPtr] { showAdvanced (*sectionPtr); };
                addAndMakeVisible (*section->moreButton);
            }
            continue;
        }

        std::unique_ptr<juce::Component> control;
        int cellWidth = kCell;
        if (spec.kind == ParamKind::Choice)
        {
            control = std::make_unique<ParamChoice> (apvts, spec);
            cellWidth = kChoiceCell;
        }
        else
        {
            auto knob = std::make_unique<ParamKnob> (apvts, spec);
            knob->setAccent (section->colour);
            control = std::move (knob);
        }

        addAndMakeVisible (*control);
        section->controls.emplace_back (control.get(), cellWidth);
        controls.push_back (std::move (control));
    }

    // ...placed in fixed rows that follow the signal path.
    for (const auto& spec : rowSpecs())
    {
        Row row;
        row.caption = spec.caption;
        row.colour = spec.colour;
        for (auto* g : spec.groups)
            if (auto* section = findSection (g))
                row.sections.push_back (section);
        if (! row.sections.empty())
            rows.push_back (std::move (row));
    }
}

void SynthPanel::showAdvanced (Section& section)
{
    auto content = std::make_unique<AdvancedBox> (apvts, section.title, section.colour, section.advanced);
    auto* top = getTopLevelComponent();
    const auto area = top->getLocalArea (this, section.moreButton->getBounds());
    juce::CallOutBox::launchAsynchronously (std::move (content), area, top);
}

SynthPanel::Section* SynthPanel::findSection (const juce::String& title)
{
    for (auto& s : sections)
        if (s->title == title)
            return s.get();
    return nullptr;
}

int SynthPanel::preferredHeight() const
{
    const int rowH = kTitleH + kCellH + kPad;
    return (int) rows.size() * (rowH + kGap) - kGap + 2 * kPad;
}

void SynthPanel::resized()
{
    const int rowH = kTitleH + kCellH + kPad;
    int y = kPad;

    for (auto& row : rows)
    {
        row.bounds = { 0, y, getWidth(), rowH };
        int x = kBand + kPad;
        for (auto* section : row.sections)
        {
            int w = 2 * kPad;
            for (const auto& [component, cellWidth] : section->controls)
                w += cellWidth;

            section->bounds = { x, y, w, rowH };
            if (section->moreButton != nullptr)
                section->moreButton->setBounds (section->bounds.withHeight (kTitleH).removeFromRight (40).reduced (3, 1));
            int cx = x + kPad;
            for (const auto& [component, cellWidth] : section->controls)
            {
                component->setBounds (cx, y + kTitleH, cellWidth, kCellH);
                cx += cellWidth;
            }
            x += w + kGap;
        }
        y += rowH + kGap;
    }
}

void SynthPanel::paint (juce::Graphics& g)
{
    for (const auto& row : rows)
    {
        // Caption band on the left, rotated to save width.
        auto band = row.bounds.withWidth (kBand).toFloat();
        g.setColour (row.colour.withAlpha (0.18f));
        g.fillRoundedRectangle (band.reduced (2.0f, 0.0f), 4.0f);
        g.setColour (row.colour);
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, band.getCentreX(), band.getCentreY()));
        g.drawText (row.caption, juce::Rectangle<float> (band.getCentreX() - band.getHeight() * 0.5f, band.getCentreY() - band.getWidth() * 0.5f,
                                                         band.getHeight(), band.getWidth()),
                    juce::Justification::centred, false);
        g.restoreState();

        for (const auto* section : row.sections)
        {
            g.setColour (colours::panel);
            g.fillRoundedRectangle (section->bounds.toFloat(), 6.0f);
            g.setColour (section->colour);
            g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
            g.drawText (section->title, section->bounds.withHeight (kTitleH).reduced (kPad, 0), juce::Justification::centredLeft);
        }
    }
}

} // namespace stacks
