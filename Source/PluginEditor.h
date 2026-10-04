#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "Controls.h"
#include "StacksLookAndFeel.h"
#include "Displays.h"
#include "SettingsPanel.h"
#include "TunerView.h"
#include "SynthPanel.h"
#include "LabPanel.h"

namespace stacks
{

class StacksAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::DragAndDropContainer,
                                    private juce::ChangeListener
{
public:
    explicit StacksAudioProcessorEditor (StacksAudioProcessor&);
    ~StacksAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    StacksAudioProcessor& synthProcessor;
    StacksLookAndFeel lookAndFeel;

    juce::Label title, patchName;
    ParamKnob masterKnob;
    ScopeView scope;
    TunerView tuner;
    juce::TextButton settingsButton, undoButton, redoButton;
    SynthPanel synthPanel;
    LabPanel labPanel;
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips { this, 600 };
};

} // namespace stacks
