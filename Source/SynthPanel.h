#pragma once

#include <JuceHeader.h>

#include "Controls.h"
#include "ModulatorsPanel.h"
#include "PluginProcessor.h"

namespace stacks
{

// The oscillator's current cycle at its morph position. Click to import a
// wavetable (.wav) into a User slot and switch the oscillator to it.
class WaveDisplay : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    WaveDisplay (StacksAudioProcessor&, int osc, juce::Colour);
    ~WaveDisplay() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void importWavetable();

    StacksAudioProcessor& processor;
    int osc;   // 0 = A, 1 = B, 2 = C
    juce::Colour colour;
    int shownWave = -1;
    float shownMorph = -1.0f;
    int shownWarp = 0;
    float shownWarpAmt = 0.0f;
    juce::String shownName;
    std::unique_ptr<juce::FileChooser> chooser;
};

// The sample oscillator's file as an overview with the Start marker. Click to
// load a file, right-click to remove it.
class SampleDisplay : public juce::Component,
                      public juce::SettableTooltipClient,
                      private juce::Timer
{
public:
    SampleDisplay (StacksAudioProcessor&, juce::AudioProcessorValueTreeState&, juce::Colour);
    ~SampleDisplay() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    StacksAudioProcessor& processor;
    juce::Colour colour;
    std::atomic<float>* start = nullptr;
    std::atomic<float>* mode = nullptr;
    const SampleData* shown = nullptr;
    float shownStart = -1.0f;
    int shownMode = -1;
    std::unique_ptr<juce::FileChooser> chooser;
};

// All the synth controls as cards in rows that follow the signal path: SOUND,
// FILTER, EFFECTS, then the MODULATORS. Each card is a grid of cells with an
// optional display across the top; rows stretch to fill the panel. Owns the
// "assign a modulator to a knob" flow.
class SynthPanel : public juce::Component,
                   private juce::AudioProcessorValueTreeState::Listener,
                   private juce::AsyncUpdater,
                   private juce::Timer,
                   private juce::ChangeListener
{
public:
    static constexpr int kCell = 62, kCellH = 70, kTitleH = 17, kPad = 5, kGap = 6, kBand = 14, kDisplayH = 60;
    static constexpr int kMaxCell = 100;                     // cells never stretch past this
    static constexpr int kModulatorsHeight = 168;            // the modulators area's natural inner height

    explicit SynthPanel (StacksAudioProcessor&);
    ~SynthPanel() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;   // clicking the background cancels assigning
    bool keyPressed (const juce::KeyPress&) override;

    void beginAssign (int source);
    void endAssign();
    void setView (int rowIndex);                              // -1 = all rows, -2 = the macros page

private:
    class RowContainer;
    class MacroPage;
    struct Section
    {
        juce::String title;
        juce::Colour colour;
        std::vector<juce::Component*> cells;                    // the grid, left to right then down
        int columns = 1;
        juce::Component* display = nullptr;                     // across the top of the grid
        juce::Component* custom = nullptr;                      // fills the body instead of a grid (the modulators area)
        std::vector<const ParamSpec*> advanced;                 // shown in a pop-out
        std::unique_ptr<juce::TextButton> moreButton;
        juce::Rectangle<int> bounds;                            // within its row container
        int rows() const        { return custom != nullptr ? 0 : ((int) cells.size() + columns - 1) / columns; }
        int naturalWidth() const;
        int naturalHeight() const;
        int weight() const      { return custom != nullptr ? 5 : display != nullptr ? columns * 2 : columns; }
    };
    struct Row
    {
        juce::String caption;
        juce::Colour colour;
        std::vector<Section*> sections;
        int naturalWidth = 0, naturalHeight = 0;
        std::unique_ptr<RowContainer> container;
    };

    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;                       // refresh rings + connection lists
    void timerCallback() override;                           // live markers on modulated knobs, sync dimming
    void changeListenerCallback (juce::ChangeBroadcaster*) override;   // a tweak landed: flash what it moved
    void refreshModulationDisplay();
    void layoutRow (Row&, int width, int height);            // places the sections and their cells
    Section* findSection (const juce::String& title);
    void showAdvanced (Section&);
    void knobClicked (int paramIndex);

    StacksAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<ParamKnob*> knobs;
    std::vector<std::unique_ptr<Section>> sections;
    std::vector<Row> rows;
    std::unique_ptr<ModulatorsPanel> modulators;
    std::vector<std::unique_ptr<juce::TextButton>> viewButtons;
    std::unique_ptr<MacroPage> macroPage;
    int viewMode = -1;                                        // -1 = all
    int assigningSource = -1;
    int seenTweakSerial = 0;
    ParamKnob* delayTimeKnob = nullptr;                      // dims while the delay is tempo-synced
};

} // namespace stacks
