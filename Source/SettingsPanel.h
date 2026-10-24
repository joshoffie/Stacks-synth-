#pragma once

#include <JuceHeader.h>

#include "StacksLookAndFeel.h"

namespace stacks
{

class StacksAudioProcessor;

// The gear menu: which model designs patches (built-in downloads, Ollama's
// copies, or your own .gguf), calm mode, and a guide to all of it.
class SettingsPanel : public juce::Component,
                      private juce::ChangeListener
{
public:
    explicit SettingsPanel (StacksAudioProcessor&);
    ~SettingsPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static juce::DialogWindow* show (StacksAudioProcessor&, juce::Component* centreAround);   // the caller owns the window's lifetime
    static juce::String guideText();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();
    void rebuildMenu();
    void engineChosen();
    void addCustomModel();
    void addModelFromUrl();

    StacksAudioProcessor& processor;
    StacksLookAndFeel lookAndFeel;
    juce::Label title, engineLabel, statusLabel, guideTitle;
    juce::ComboBox engineBox;
    juce::TextButton refreshButton, downloadButton, addModelButton, urlModelButton, removeModelButton, saveGuideButton;
    juce::ToggleButton calmToggle { "Calm mode: no live knob markers or LFO playhead (the scope stays)" };
    juce::Label mtsLabel;   // MTS-ESP status
    juce::TextButton tipsButton { "Show the tips again" };
    juce::ToggleButton mpeToggle  { "MPE: per-note pitch bend (48 st on channels 2-16), pressure and slide (CC74); channel 1 is the master" };
    juce::TextEditor guide;
    juce::StringArray menuBuiltins, menuOllama;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace stacks
