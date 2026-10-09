#pragma once

#include <JuceHeader.h>

#include "Controls.h"
#include "PluginProcessor.h"

namespace stacks
{

// Shows one LFO's shape, animates its position, and lets the user draw a
// custom shape: click to add a point, drag to move, double-click to remove.
class LfoDisplay : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::Timer
{
public:
    LfoDisplay (StacksAudioProcessor&, int lfoIndex, juce::Colour);
    ~LfoDisplay() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    juce::Point<float> toNorm (juce::Point<float> screen) const;
    juce::Point<float> toScreen (juce::Point<float> norm) const;
    int hitPoint (juce::Point<float> screen) const;
    void beginEditing();

    StacksAudioProcessor& processor;
    int lfo;
    juce::Colour colour;
    LfoPoints points;
    int dragIndex = -1;
};

// The modulators area: tabs for LFO 1-4, Mod Env and the other sources, each
// with its controls, an Assign button and the list of its connections.
class ModulatorsPanel : public juce::Component,
                        private juce::ChangeListener
{
public:
    ModulatorsPanel (StacksAudioProcessor&);
    ~ModulatorsPanel() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    void refreshConnections();
    void setAssigning (int source);              // -1 = not assigning
    int currentSource() const;

    std::function<void (int source)> onAssignRequest;
    std::function<void()> onAssignCancel;

private:
    enum Tab { TabLfo1, TabLfo2, TabLfo3, TabLfo4, TabModEnv, TabSources, kNumTabs };

    void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshConnections(); }
    void showTab (int tab);
    void addVirtualTargetMenu();

    // A tab you can also drag onto a knob to connect its modulator.
    class DragTab : public juce::TextButton
    {
    public:
        int source = 0;
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! dragging && e.getDistanceFromDragStart() > 6)
                if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
                {
                    dragging = true;
                    container->startDragging (juce::var (source), this, juce::ScaledImage(), true);
                }
            juce::TextButton::mouseDrag (e);
        }
        void mouseUp (const juce::MouseEvent& e) override { dragging = false; juce::TextButton::mouseUp (e); }
    private:
        bool dragging = false;
    };

    StacksAudioProcessor& processor;
    DragTab tabButtons[kNumTabs];
    int currentTab = TabLfo1;
    int assigningSource = -1;

    // per-tab content, rebuilt on tab change
    std::vector<std::unique_ptr<juce::Component>> content;
    std::unique_ptr<LfoDisplay> display;
    juce::ComboBox sourcePicker;                 // Sources tab

    juce::TextButton assignButton, virtualButton;
    struct Row
    {
        juce::Label name;
        juce::Slider depth;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::TextButton remove;
    };
    std::vector<std::unique_ptr<Row>> rows;
    juce::Viewport viewport;
    juce::Component list;
    juce::Label emptyLabel;                      // "no connections" in the list
};

} // namespace stacks
