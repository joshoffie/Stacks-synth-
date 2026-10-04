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

    static void show (StacksAudioProcessor&, juce::Component* centreAround);
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
    juce::TextEditor guide;
    juce::StringArray menuBuiltins, menuOllama;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace stacks
