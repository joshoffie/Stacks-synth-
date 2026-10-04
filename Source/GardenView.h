#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

namespace stacks
{

// The breeding loop as a plant: the sound you're playing is the seed in the
// middle, the candidates are leaves around it (AI above, random below).
// Click a leaf to hear it, drag it outward for wilder children, right-click
// to plant it (it becomes the seed and a new generation grows from it).
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

    std::function<void()> onEvolve;             // plant the seed: evolve from current / favourites
    std::function<void()> onSave;               // save what's playing (the Lab's folder + name dialog)
    std::function<void()> onFresh;              // fresh ideas
    std::function<void (int)> onEvolveFrom;     // evolve from candidate index
    std::function<float()> getVariation;
    std::function<void (float)> setVariation;

    struct Branch { juce::Point<float> unit { 1.0f, 0.0f }; float baseLength = 1.0f, restT = 1.0f; };

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
    std::vector<float> stretch;                 // per leaf: where a drag left it (fraction of its branch), 0 = not dragged
    std::vector<Branch> branches;               // per leaf, from the last layout: the line a drag moves along
    int shownGeneration = -1;
    double lastRefreshMs = 0.0;
};

} // namespace stacks
