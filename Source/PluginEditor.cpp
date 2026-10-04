#include "PluginEditor.h"

namespace stacks
{

namespace
{
    constexpr int kHeaderHeight = 60;
    constexpr int kKeyboardHeight = 64;
    constexpr int kLabWidth = 400;
    constexpr int kMargin = 8;
}

StacksAudioProcessorEditor::StacksAudioProcessorEditor (StacksAudioProcessor& p)
    : AudioProcessorEditor (&p),
      synthProcessor (p),
      masterKnob (p.apvts, spec (P::master_gain), true),
      scope (p),
      synthPanel (p),
      labPanel (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);

    title.setText ("STACKS", juce::dontSendNotification);
    title.setFont (StacksLookAndFeel::font (22.0f, true));
    title.setColour (juce::Label::textColourId, colours::accent);
    addAndMakeVisible (title);

    patchName.setFont (StacksLookAndFeel::font (16.0f));
    patchName.setColour (juce::Label::textColourId, colours::text);
    patchName.setEditable (false, true, false);
    patchName.setTooltip ("Double-click to rename the current patch");
    patchName.onTextChange = [this] { synthProcessor.setCurrentPatchName (patchName.getText()); };
    addAndMakeVisible (patchName);

    addAndMakeVisible (masterKnob);
    addAndMakeVisible (scope);

    settingsButton.setButtonText (juce::String::fromUTF8 ("\xe2\x9a\x99"));   // gear
    settingsButton.setTooltip ("Settings: which AI model designs patches, your own models, calm mode, and the model guide");
    settingsButton.onClick = [this] { SettingsPanel::show (synthProcessor, this); };
    addAndMakeVisible (settingsButton);
    addAndMakeVisible (synthPanel);
    addAndMakeVisible (labPanel);

    keyboard.setAvailableRange (36, 96);
    keyboard.setKeyWidth (22.0f);
    addAndMakeVisible (keyboard);

    synthProcessor.labBroadcaster.addChangeListener (this);
    changeListenerCallback (nullptr);

    setResizable (true, true);
    setResizeLimits (1340, 680, 2200, 1400);
    setSize (1460, 736);
}

StacksAudioProcessorEditor::~StacksAudioProcessorEditor()
{
    synthProcessor.labBroadcaster.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void StacksAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    patchName.setText (synthProcessor.currentPatchName(), juce::dontSendNotification);
}

void StacksAudioProcessorEditor::paint (juce::Graphics& g)
{
    // A faint vertical gradient, so the panels read as sitting on a surface.
    juce::ColourGradient grad (colours::background.brighter (0.06f), 0.0f, 0.0f, colours::background.darker (0.12f), 0.0f, (float) getHeight(), false);
    g.setGradientFill (grad);
    g.fillAll();
}

void StacksAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (kMargin);

    auto header = r.removeFromTop (kHeaderHeight);
    masterKnob.setBounds (header.removeFromRight (64));
    settingsButton.setBounds (header.removeFromRight (30).withSizeKeepingCentre (26, 26));
    header.removeFromRight (6);
    title.setBounds (header.removeFromLeft (130).withTrimmedBottom (14));
    patchName.setBounds (header.removeFromLeft (juce::jmin (420, header.getWidth() / 2)).reduced (6, 0).withTrimmedBottom (14));
    header.removeFromRight (10);
    scope.setBounds (header.reduced (0, 4));
    r.removeFromTop (kMargin);

    labPanel.setBounds (r.removeFromRight (kLabWidth));
    r.removeFromRight (kMargin);

    keyboard.setBounds (r.removeFromBottom (kKeyboardHeight));
    r.removeFromBottom (kMargin);
    synthPanel.setBounds (r);
}

} // namespace stacks
