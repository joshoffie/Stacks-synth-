#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GardenView.h"

namespace stacks
{

// The family tree as an endless canvas. Each generation is a column of blocks
// joined to the sound it grew from by bracket connectors; the newest batch
// drops into place at the right. Drag empty space to pan, pinch or scroll to
// zoom, click a block to hear it, add blocks to the mix with the corner plus
// (or a Cmd-click) and grow from all of them at once.
class TreeCanvas : public juce::Component,
                   private juce::Timer
{
public:
    explicit TreeCanvas (StacksAudioProcessor&, float defaultZoom = 1.0f);
    ~TreeCanvas() override;

    void refresh();                                            // nodes follow the lab state
    void centreOnNewest (bool animate);                        // the newest column in view
    std::vector<std::pair<int, int>> selection() const         { return { mix.begin(), mix.end() }; }   // (generation, candidate)
    void clearSelection();
    std::function<void()> onSelectionChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;
    void visibilityChanged() override;

    std::vector<std::pair<juce::String, juce::Rectangle<int>>> skinPieces() const;   // slab and first block, own coordinates
    juce::Rectangle<int> blocksArea() const;                                          // where the newest column sits, for the tour

    static constexpr float kBlockW = 150.0f, kBlockH = 54.0f, kDepth = 8.0f, kPitch = 70.0f, kColW = 230.0f;

private:
    struct Node
    {
        int gen = 0, cand = -1;                 // cand -1 = the seed slab of that generation
        juce::String name, description;
        GardenView::Style style;
        juce::Point<float> target;              // world position of the face's top-left
        float drop = 0.0f, vy = 0.0f, scale = 1.0f, age = 10.0f;   // the drop-in: negative drop = above its place
        bool landed = true;
    };
    struct Column { int gen = 0; std::vector<int> nodes; std::vector<int> parents; int seedNode = -1; float x = 0.0f; bool current = false; };

    class NodeComp;
    friend class NodeComp;

    void rebuild (bool settledOnly);
    void layout();
    void placeNodes();
    void timerCallback() override;
    juce::Point<float> toScreen (juce::Point<float> world) const;
    juce::Rectangle<float> faceOf (const Node&) const;        // screen rectangle of the front face
    bool isSelected (const Node& n) const                      { return mix.count ({ n.gen, n.cand }) > 0; }
    void toggleSelected (const Node&);
    void hear (const Node&);
    int nodeAt (int gen, int cand) const;

    StacksAudioProcessor& processor;
    std::vector<Node> nodes;
    std::vector<Column> columns;
    std::vector<std::unique_ptr<NodeComp>> comps;
    std::set<std::pair<int, int>> mix;
    juce::Point<float> pan, panTarget;                         // world point at the canvas's top-left
    float zoom, defaultZoom;
    bool dragging = false, animatingPan = false;
    juce::Point<float> dragStartPan;
    float anim = 0.0f;
    int knownGenerations = -1, knownCandidates = -1;
    bool centredWithSize = false;   // the first centring happens once the canvas has a size
    juce::TextButton centreButton { "Centre" };
};

} // namespace stacks
