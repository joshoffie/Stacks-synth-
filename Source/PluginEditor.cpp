#include "PluginEditor.h"

namespace stacks
{

namespace
{
    constexpr int kHeaderHeight = 58;
    constexpr int kKeyboardHeight = 64;
    constexpr int kLabWidth = 400;
    constexpr int kMargin = 8;
}

StacksAudioProcessorEditor::StacksAudioProcessorEditor (StacksAudioProcessor& p)
    : AudioProcessorEditor (&p),
      synthProcessor (p),
      masterKnob (p.apvts, spec (P::master_gain), true),
      synthPanel (p.apvts),
      labPanel (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    auto scheme = juce::LookAndFeel_V4::getMidnightColourScheme();
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::windowBackground, colours::background);
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::widgetBackground, colours::panel);
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::menuBackground, colours::panel);
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::defaultFill, colours::accent);
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::highlightedFill, colours::accent);
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::defaultText, colours::text);
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::highlightedText, juce::Colours::black);
    scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::outline, juce::Colour (0xff3a3f49));
    lookAndFeel.setColourScheme (scheme);
    lookAndFeel.setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    lookAndFeel.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3f49));
    lookAndFeel.setColour (juce::Slider::thumbColourId, colours::text);
    lookAndFeel.setColour (juce::Slider::trackColourId, colours::accent);
    lookAndFeel.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff343943));
    lookAndFeel.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff343943));
    lookAndFeel.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1b1d22));
    setLookAndFeel (&lookAndFeel);

    title.setText ("STACKS", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    title.setColour (juce::Label::textColourId, colours::accent);
    addAndMakeVisible (title);

    patchName.setFont (juce::Font (juce::FontOptions (16.0f)));
    patchName.setColour (juce::Label::textColourId, colours::text);
    patchName.setEditable (false, true, false);
    patchName.setTooltip ("Double-click to rename the current patch");
    patchName.onTextChange = [this] { synthProcessor.setCurrentPatchName (patchName.getText()); };
    addAndMakeVisible (patchName);

    addAndMakeVisible (masterKnob);
    addAndMakeVisible (synthPanel);
    addAndMakeVisible (labPanel);

    keyboard.setAvailableRange (36, 96);
    keyboard.setKeyWidth (22.0f);
    addAndMakeVisible (keyboard);

    synthProcessor.labBroadcaster.addChangeListener (this);
    changeListenerCallback (nullptr);

    setResizable (true, true);
    setResizeLimits (1200, 600, 2200, 1400);
    setSize (1360, 620);
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
    g.fillAll (colours::background);
}

void StacksAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (kMargin);

    auto header = r.removeFromTop (kHeaderHeight);
    masterKnob.setBounds (header.removeFromRight (64));
    title.setBounds (header.removeFromLeft (130).withTrimmedBottom (14));
    patchName.setBounds (header.reduced (6, 0).withTrimmedBottom (14));
    r.removeFromTop (kMargin);

    labPanel.setBounds (r.removeFromRight (kLabWidth));
    r.removeFromRight (kMargin);

    keyboard.setBounds (r.removeFromBottom (kKeyboardHeight));
    r.removeFromBottom (kMargin);
    synthPanel.setBounds (r);
}

} // namespace stacks
