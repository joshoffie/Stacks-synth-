#include "PluginEditor.h"

namespace stacks
{

namespace
{
    constexpr int kHeaderHeight = 44;
    constexpr int kKeyboardHeight = 58;
    constexpr int kLabWidth = 392;
    constexpr int kMargin = 6;
}

StacksAudioProcessorEditor::StacksAudioProcessorEditor (StacksAudioProcessor& p)
    : AudioProcessorEditor (&p),
      synthProcessor (p),
      masterKnob (p.apvts, spec (P::master_gain), true),
      scope (p),
      tuner (p),
      synthPanel (p),
      labPanel (p),
      keyboard (p.keyboardState)
{
    setLookAndFeel (&lookAndFeel);

    patchName.setFont (StacksLookAndFeel::font (15.0f, true));
    patchName.setColour (juce::Label::textColourId, colours::text);
    patchName.setEditable (false, true, false);
    patchName.setTooltip ("The sound you're hearing. Double-click to rename it.");
    patchName.onTextChange = [this] { synthProcessor.setCurrentPatchName (patchName.getText()); };
    addAndMakeVisible (patchName);

    masterKnob.setAccent (colours::accent);
    addAndMakeVisible (masterKnob);
    addAndMakeVisible (scope);
    addAndMakeVisible (tuner);

    settingsButton.setButtonText (juce::String::fromUTF8 ("\xe2\x9a\x99"));   // gear
    settingsButton.setTooltip ("Settings: which AI model designs patches, your own models, calm mode, and the model guide");
    settingsButton.onClick = [this] { SettingsPanel::show (synthProcessor, this); };
    addAndMakeVisible (settingsButton);

    undoButton.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xb6"));   // undo arrow
    undoButton.setTooltip ("Undo (Cmd+Z): back to the sound before the last load, drag, tweak or knob move");
    undoButton.onClick = [this] { synthProcessor.undo(); };
    addAndMakeVisible (undoButton);
    redoButton.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xb7"));
    redoButton.setTooltip ("Redo (Shift+Cmd+Z)");
    redoButton.onClick = [this] { synthProcessor.redo(); };
    addAndMakeVisible (redoButton);
    for (auto* b : { &settingsButton, &undoButton, &redoButton })
    {
        styleAsTab (*b);
        b->getProperties().set ("bright", true);
    }
    // Keys reach the editor itself (undo/redo shortcuts) and otherwise pass on to
    // the host, so Logic's musical typing keeps working; no child is focused
    // until it is clicked.
    setWantsKeyboardFocus (true);
    setFocusContainerType (juce::Component::FocusContainerType::keyboardFocusContainer);
    addAndMakeVisible (synthPanel);
    addAndMakeVisible (labPanel);
    addAndMakeVisible (keyboard);

    synthProcessor.labBroadcaster.addChangeListener (this);
    changeListenerCallback (nullptr);

    setResizable (true, true);
    setResizeLimits (1280, 720, 2600, 1600);
    setSize (1500, 860);
}

StacksAudioProcessorEditor::~StacksAudioProcessorEditor()
{
    synthProcessor.labBroadcaster.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void StacksAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    patchName.setText (synthProcessor.currentPatchName(), juce::dontSendNotification);
    undoButton.setEnabled (synthProcessor.canUndo());
    redoButton.setEnabled (synthProcessor.canRedo());
}

namespace
{
    // Tab still walks the controls, but when the window comes to the front no
    // child is chosen automatically.
    struct NoAutoFocusTraverser : public juce::KeyboardFocusTraverser
    {
        juce::Component* getDefaultComponent (juce::Component*) override { return nullptr; }
    };
}

std::unique_ptr<juce::ComponentTraverser> StacksAudioProcessorEditor::createKeyboardFocusTraverser()
{
    return std::make_unique<NoAutoFocusTraverser>();
}

bool StacksAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const bool cmd = key.getModifiers().isCommandDown();
    if (cmd && key.getKeyCode() == 'Z' && key.getModifiers().isShiftDown()) { synthProcessor.redo(); return true; }
    if (cmd && key.getKeyCode() == 'Z')  { synthProcessor.undo(); return true; }
    if (cmd && key.getKeyCode() == 'Y')  { synthProcessor.redo(); return true; }
    return false;
}

void StacksAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // Header: the wordmark, and a hairline under the whole strip.
    auto h = headerBounds;
    g.setColour (colours::text);
    g.setFont (StacksLookAndFeel::font (17.0f, true).withExtraKerningFactor (0.22f));
    auto logo = h.removeFromLeft (112).withTrimmedLeft (10);
    g.drawText ("STACKS", logo, juce::Justification::centredLeft);
    g.setColour (colours::accent);
    g.fillEllipse ((float) logo.getX() + 88.0f, (float) logo.getCentreY() - 1.5f + 4.0f, 4.0f, 4.0f);   // the dot after the name
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (headerBounds.getX(), headerBounds.getBottom(), headerBounds.getWidth(), 1);
}

void StacksAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();

    auto header = r.removeFromTop (kHeaderHeight);
    headerBounds = header;
    header = header.reduced (kMargin, 0);
    masterKnob.setBounds (header.removeFromRight (44).reduced (0, 2));
    header.removeFromRight (2);
    settingsButton.setBounds (header.removeFromRight (28).withSizeKeepingCentre (26, 26));
    redoButton.setBounds (header.removeFromRight (26).withSizeKeepingCentre (24, 26));
    undoButton.setBounds (header.removeFromRight (26).withSizeKeepingCentre (24, 26));
    header.removeFromRight (10);
    tuner.setBounds (header.removeFromRight (230).reduced (0, 8));
    header.removeFromRight (10);
    header.removeFromLeft (112);   // the wordmark, painted
    patchName.setBounds (header.removeFromLeft (juce::jmin (360, header.getWidth() / 3)).reduced (0, 8));
    header.removeFromLeft (10);
    scope.setBounds (header.reduced (0, 7));

    r.removeFromTop (kMargin);
    r = r.reduced (kMargin, 0);
    keyboard.setBounds (r.removeFromBottom (kKeyboardHeight));
    r.removeFromBottom (kMargin);

    labPanel.setBounds (r.removeFromRight (kLabWidth));
    r.removeFromRight (kMargin);
    synthPanel.setBounds (r);
}

} // namespace stacks
