#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "Controls.h"
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

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    StacksAudioProcessor& synthProcessor;
    juce::LookAndFeel_V4 lookAndFeel;

    juce::Label title, patchName;
    ParamKnob masterKnob;
    SynthPanel synthPanel;
    LabPanel labPanel;
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips { this, 600 };
};

} // namespace stacks
