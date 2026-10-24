#pragma once

#include <JuceHeader.h>

#include "Controls.h"
#include "PluginProcessor.h"
#include "GardenView.h"

namespace stacks
{

// The first screen. Pick a kind of sound, get five, play them, grow from the
// one you like. Nothing here mentions models or prompts: it reads as a very
// good algorithm that grows sounds.
class HomeView : public juce::Component,
                 private juce::ChangeListener,
                 private juce::Timer
{
public:
    explicit HomeView (StacksAudioProcessor&);
    ~HomeView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onOpenSynth;   // "Open the synth": the full editor
    std::function<void()> onSave;        // the library's save dialog (folder + name)

private:
    enum class Stage { pick, growing, results };

    // A big button for one kind of sound.
    class Tile : public juce::Component
    {
    public:
        Tile (const juce::String& name, const juce::String& blurb, juce::Colour);
        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
        void mouseDown (const juce::MouseEvent&) override  { if (onClick) onClick(); }
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
        std::function<void()> onClick;
        juce::String name, blurb;
        juce::Colour colour;
        bool hover = false;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refresh();
    void pick (const juce::String& category);
    void grow();
    void setVariation (float);
    void paintGrowth (juce::Graphics&, juce::Rectangle<float>);
    void paintConstruction (juce::Graphics&, juce::Rectangle<float> slot, juce::Colour, float progress);   // the block being written
    void showKindMenu (const juce::String& kind, Tile&);
    void growFromPreset (const juce::File&);
    juce::Colour kindColour (const juce::String& kind) const;

public:
    static void paintTileFace (juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool hover, const juce::String& kind = {});   // the tile's background, no words
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> skinPieces() const;   // a tile, the slab, a block; own coordinates
    bool showingResults() const noexcept { return stage == Stage::results; }
    juce::Rectangle<int> tourArea (const juce::String& what) const;   // "tiles", "blocks", "variation", "grow", "save", "open"; own coordinates
private:

    StacksAudioProcessor& processor;
    Stage stage = Stage::pick;
    juce::String category, note, pickNote;   // pickNote: why the last attempt came back empty
    std::vector<std::unique_ptr<Tile>> tiles;
    juce::TextButton growButton { "Grow from this one" }, backButton { "Start over" }, saveButton { "Save preset" }, openSynthButton { "Open the synth" };
    juce::TextButton closeButton { "Close" }, mediumButton { "Medium" }, farButton { "Far" };
    float variation = 0.55f;
    float anim = 0.0f;
    int growthSeed = 1;
    juce::String stageText;

    // The growing stage: blocks drop in as sounds land and settle on the slab.
    struct Falling
    {
        int index = 0;
        float x = 0.0f, y = 0.0f, vy = 0.0f;   // front-face top-left and vertical speed
        float scale = 0.2f, squash = 1.0f, age = 0.0f;
        bool landed = false;
        GardenView::Style style;
        juce::String name;
    };
    std::vector<Falling> falling;
    int hoveredBlock = -1;

    // One component per block, so a click (or an accessibility press) lands on exactly one sound.
    class BlockComp : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit BlockComp (HomeView& o) : owner (o) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { owner.processor.audition (index); }
        void mouseEnter (const juce::MouseEvent&) override { hovered = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { hovered = false; repaint(); }
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
        HomeView& owner;
        int index = 0;
        juce::Rectangle<float> face;   // in this component's space
        GardenView::Style style;
        juce::String name;
        float alpha = 1.0f;
        bool selected = false, hovered = false;
    };
    std::vector<std::unique_ptr<BlockComp>> blockComps;
    void placeBlocks();
    juce::Rectangle<float> stackArea() const;
    juce::Rectangle<float> faceOf (const Falling&, juce::Rectangle<float> area) const;
    void syncBlocks (bool settled);   // one block per sound; new ones fall in, or sit when `settled`
    void stepPhysics (float dt);
    GardenView::Style seedStyle;
    juce::String seedName;
    juce::Rectangle<float> slotFor (int index, juce::Rectangle<float> area) const;
    float blockWidth (juce::Rectangle<float> area) const  { return juce::jmin (230.0f, (area.getWidth() - 3.0f * 16.0f) / 4.0f); }
    float blockHeight (juce::Rectangle<float> area) const { return juce::jlimit (60.0f, 130.0f, juce::jmin (blockWidth (area) * 0.6f, (area.getHeight() - 60.0f) / 2.4f)); }
};

} // namespace stacks
