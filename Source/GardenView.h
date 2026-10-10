#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

namespace stacks
{

// The breeding loop as a pile of blocks: the sound you're playing is the slab
// at the bottom, the candidates pile up on it as they arrive (rows of 4, 3, 2
// and 1, so the AI ideas land on top). Every block looks like its patch:
// hue from the category, lightness from the cutoff, width from the unison
// spread, depth from the release, corners from the attack, its oscillator's
// wave on the face, a serrated edge for distortion, speckle for noise, a glow
// for reverb, echoes for delay, stripes for unison, a slow bob when an LFO
// moves it. Click a block to hear it, drag it up to exaggerate or down to
// blend it with the seed, right-click to plant it or save it.
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
    void visibilityChanged() override;

    std::function<void()> onEvolve;             // plant the seed: evolve from current / favourites
    std::function<void()> onSave;               // save what's playing (the Lab's folder + name dialog)
    std::function<void()> onFresh;              // fresh ideas
    std::function<void (int)> onEvolveFrom;     // evolve from candidate index
    std::function<float()> getVariation;
    std::function<void (float)> setVariation;

    // How a patch looks as a block.
    struct Style
    {
        juce::Colour colour;
        float width = 1.0f;                     // of the base block width
        float depth = 10.0f;                    // the top face, in px
        float corner = 3.0f;                    // front face corner radius
        bool serrated = false;                  // distortion
        float speckle = 0.0f;                   // noise, 0..1
        float glow = 0.0f;                      // reverb mix, 0..1
        float echo = 0.0f;                      // delay mix, 0..1
        int stripes = 0;                        // unison voices beyond one
        float bobHz = 0.0f;                     // an LFO moves the sound: the block bobs
        std::vector<float> wave;                // one cycle of oscillator A, -1..1
    };
    static Style styleFor (const Patch&, const WavetableBank&, bool ai);

private:
    class Block;
    void timerCallback() override;
    void layoutBlocks();
    void updateTimer();
    juce::Rectangle<float> slotFor (int index) const;   // front face of slot `index`
    juce::Rectangle<float> slabFace() const;
    float baseBlockWidth() const;
    float blockHeight() const;                          // taller when the panel has room, so blocks read as cubes
    int rowOf (int index, int& column, int& count) const;

    StacksAudioProcessor& processor;
    std::vector<std::unique_ptr<Block>> blocks;
    std::vector<double> appearedAt;             // per block, for the drop-in animation
    int hoveredBlock = -1;
    int shownGeneration = -1;
    Style seedStyle;
    bool seedStyled = false;
};

} // namespace stacks
