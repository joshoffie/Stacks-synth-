#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "Controls.h"
#include "StacksLookAndFeel.h"
#include "HomeView.h"
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
    std::unique_ptr<juce::ComponentTraverser> createKeyboardFocusTraverser() override;   // no automatic focus target

    // Paints every screen (home, each synth page, each modulator tab, each lab
    // view) to numbered PNG files at `scale` times the window size. Used by the
    // StacksUiKit tool to hand the UI to a designer; the editor ends up back on Home.
    void renderScreens (const juce::File& directory, float scale);
    // The same screens with each skin file's name written next to the piece it paints.
    void annotateScreens (const juce::File& directory, float scale);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    StacksAudioProcessor& synthProcessor;
    StacksLookAndFeel lookAndFeel;

    juce::Label patchName;
    ParamKnob masterKnob;
    ScopeView scope;
    TunerView tuner;
    juce::TextButton settingsButton, undoButton, redoButton;
    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;   // one at a time; closed with the editor
    SynthPanel synthPanel;
    LabPanel labPanel;
    HomeView homeView;
    juce::TextButton modeButton;   // Home <-> Synth
    bool showingHome = true;
    void setHome (bool);
    StacksKeyboard keyboard;
    juce::Rectangle<int> headerBounds;
    juce::TooltipWindow tooltips { this, 600 };
};

} // namespace stacks
