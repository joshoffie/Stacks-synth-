#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

namespace stacks
{

// The breeding loop as a plant: the sound you're playing is the seed in the
// middle, the candidates are leaves around it (AI above, random below).
// Click a leaf to hear it, double-click to plant it (it becomes the seed and
// a new generation grows from it), right-click for more.
class GardenView : public juce::Component,
                   private juce::Timer
{
public:
    explicit GardenView (StacksAudioProcessor&);
    ~GardenView() override;

    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    std::function<void()> onEvolve;             // plant the seed: evolve from current / favourites
    std::function<void()> onFresh;              // fresh ideas
    std::function<void (int)> onEvolveFrom;     // evolve from candidate index
    std::function<float()> getVariation;
    std::function<void (float)> setVariation;

private:
    class Leaf;
    void timerCallback() override;
    void layoutLeaves();
    juce::Point<float> seedCentre() const;
    float branchLength() const;

    StacksAudioProcessor& processor;
    std::vector<std::unique_ptr<Leaf>> leaves;
    std::vector<double> appearedAt;             // per leaf, for the sprout animation
    int hoveredLeaf = -1;
    int shownGeneration = -1;
    double lastRefreshMs = 0.0;
};

} // namespace stacks
