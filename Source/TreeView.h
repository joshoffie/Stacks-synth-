#pragma once

#include <JuceHeader.h>

namespace stacks
{

class StacksAudioProcessor;

// The family tree: every generation this session as a column, its seed on
// top and its candidates below, with a line from the candidate that became
// the next seed. Click any node to hear it again.
class FamilyTree : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    explicit FamilyTree (StacksAudioProcessor&);
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    int preferredWidth() const;

private:
    struct Node { juce::Rectangle<float> bounds; int generation, candidate; juce::String name, tip; bool ai = false, seed = false; };
    StacksAudioProcessor& processor;
    std::vector<Node> nodes;
    int hovered = -1;
};

} // namespace stacks
