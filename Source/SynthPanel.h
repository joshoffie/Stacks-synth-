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

// All the synth controls in fixed, captioned rows that follow the signal path,
// plus the modulators area. Owns the "assign a modulator to a knob" flow.
class SynthPanel : public juce::Component,
                   private juce::AudioProcessorValueTreeState::Listener,
                   private juce::AsyncUpdater,
                   private juce::Timer
{
public:
    static constexpr int kCell = 60, kChoiceCell = 82, kCellH = 86, kTitleH = 16, kPad = 6, kGap = 6, kBand = 20;
    static constexpr int kModulatorsHeight = 176;

    explicit SynthPanel (StacksAudioProcessor&);
    ~SynthPanel() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;   // clicking the background cancels assigning
    bool keyPressed (const juce::KeyPress&) override;
    int preferredHeight() const;

    void beginAssign (int source);
    void endAssign();
    void setView (int rowIndex);                              // -1 = all rows, -2 = the macros page

private:
    class RowContainer;
    class HelpStrip;
    class MacroPage;
    struct Section
    {
        juce::String title;
        juce::Colour colour;
        std::vector<std::pair<juce::Component*, int>> controls; // component, width
        std::vector<const ParamSpec*> advanced;                  // shown in a pop-out
        std::unique_ptr<juce::TextButton> moreButton;
        juce::Rectangle<int> bounds;
        bool tall = false;                                       // fills the row (the modulators area)
    };
    struct Row
    {
        juce::String caption;
        juce::Colour colour;
        int height = 0;
        std::vector<Section*> sections;
        juce::Rectangle<int> bounds;          // within its container
        int naturalWidth = 0;
        std::unique_ptr<RowContainer> container;
    };

    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;                       // refresh rings + connection lists
    void timerCallback() override;                           // live markers on modulated knobs
    void refreshModulationDisplay();
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
    std::unique_ptr<HelpStrip> help;
    std::unique_ptr<MacroPage> macroPage;
    std::vector<juce::Component*> optionalDisplays;          // shown when the row has room (always in a single-row view)
    int viewMode = -1;                                        // -1 = all
    int assigningSource = -1;
};

} // namespace stacks
