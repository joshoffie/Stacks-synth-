#pragma once

#include <JuceHeader.h>

#include "Controls.h"
#include "PluginProcessor.h"

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

    // One of the five results.
    class Card : public juce::Component
    {
    public:
        void set (const Patch&, bool selected);
        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
        void mouseDown (const juce::MouseEvent&) override  { if (onClick) onClick(); }
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
        std::function<void()> onClick;
        juce::String name, category, description;
        bool selected = false, hover = false;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refresh();
    void pick (const juce::String& category);
    void grow();
    void setVariation (float);
    void paintGrowth (juce::Graphics&, juce::Rectangle<float>);

    StacksAudioProcessor& processor;
    Stage stage = Stage::pick;
    juce::String category, note;
    std::vector<std::unique_ptr<Tile>> tiles;
    std::vector<std::unique_ptr<Card>> cards;
    juce::TextButton growButton { "Grow from this one" }, backButton { "Start over" }, saveButton { "Save it" }, openSynthButton { "Open the synth" };
    juce::TextButton closeButton { "Close" }, mediumButton { "Medium" }, farButton { "Far" };
    float variation = 0.55f;
    float anim = 0.0f;
    int growthSeed = 1;
    juce::String stageText;
};

} // namespace stacks
