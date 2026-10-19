#include "PluginEditor.h"
#include "Skin.h"

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
      homeView (p),
      keyboard (p.keyboardState)
{
    skin::load();   // PNG pieces replace the drawn look wherever a file exists (see Design/Skin spec.md)
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
    settingsButton.setTooltip ("Settings: calm mode, MPE and microtuning");
    settingsButton.onClick = [this]
    {
        if (settingsWindow != nullptr) { settingsWindow->toFront (true); return; }
        settingsWindow = SettingsPanel::show (synthProcessor, this);
    };
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
    addAndMakeVisible (homeView);
    homeView.onOpenSynth = [this] { setHome (false); };
    homeView.onSave = [this] { labPanel.savePresetDialog(); };
    modeButton.setTooltip ("Home: pick a kind of sound and grow it. Synth: every knob.");
    modeButton.onClick = [this] { setHome (! showingHome); };
    addAndMakeVisible (modeButton);
    setHome (true);
    addAndMakeVisible (keyboard);

    synthProcessor.labBroadcaster.addChangeListener (this);
    changeListenerCallback (nullptr);

    setResizable (true, true);
    setResizeLimits (1280, 720, 2600, 1600);
    setSize (1500, 860);
}

StacksAudioProcessorEditor::~StacksAudioProcessorEditor()
{
    settingsWindow.deleteAndZero();   // it talks to the processor; it must not outlive the editor
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
    if (skin::has ("background")) skin::drawStretched (g, "background", getLocalBounds().toFloat());
    if (skin::has ("header"))     skin::drawStretched (g, "header", headerBounds.toFloat());

    // Header: the wordmark, and a hairline under the whole strip.
    auto h = headerBounds;
    g.setColour (colours::text);
    g.setFont (StacksLookAndFeel::font (17.0f, true).withExtraKerningFactor (0.22f));
    auto logo = h.removeFromLeft (112).withTrimmedLeft (10);
    g.drawText ("STACKS", logo, juce::Justification::centredLeft);
    g.setColour (colours::accent);
    g.fillEllipse ((float) logo.getX() + 88.0f, (float) logo.getCentreY() - 1.5f + 4.0f, 4.0f, 4.0f);   // the dot after the name
    if (! skin::has ("header"))
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (headerBounds.getX(), headerBounds.getBottom(), headerBounds.getWidth(), 1);
    }
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
    modeButton.setBounds (header.removeFromLeft (64).withSizeKeepingCentre (62, 24));
    header.removeFromLeft (8);
    patchName.setBounds (header.removeFromLeft (juce::jmin (360, header.getWidth() / 3)).reduced (0, 8));
    header.removeFromLeft (10);
    scope.setBounds (header.reduced (0, 7));

    r.removeFromTop (kMargin);
    r = r.reduced (kMargin, 0);
    keyboard.setBounds (r.removeFromBottom (kKeyboardHeight));
    r.removeFromBottom (kMargin);

    homeView.setBounds (r);
    labPanel.setBounds (r.removeFromRight (kLabWidth));
    r.removeFromRight (kMargin);
    synthPanel.setBounds (r);
}

void StacksAudioProcessorEditor::renderScreens (const juce::File& directory, float scale)
{
    directory.createDirectory();
    int shot = 0;
    auto shoot = [&] (const juce::String& name)
    {
        resized();
        const auto image = createComponentSnapshot (getLocalBounds(), true, scale);
        const auto file = directory.getChildFile (juce::String (++shot).paddedLeft ('0', 2) + " " + name + ".png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        if (out.openedOk())
            juce::PNGImageFormat().writeImageToStream (image, out);
    };

    setHome (true);
    shoot ("home");
    setHome (false);
    synthPanel.setView (-2); shoot ("synth - macros");
    synthPanel.setView (-1); shoot ("synth - all");
    static const char* rowNames[] = { "sound", "filter", "effects", "modulators" };
    for (int r = 0; r < synthPanel.numRows(); ++r)
    {
        synthPanel.setView (r);
        shoot ("synth - " + juce::String (r < 4 ? rowNames[r] : "row " + juce::String (r)));
    }
    if (auto* mods = synthPanel.modulatorsPanel())
    {
        static const char* tabNames[] = { "lfo 1", "lfo 2", "lfo 3", "lfo 4", "mod env", "seq", "sources" };
        synthPanel.setView (synthPanel.numRows() - 1);
        for (int t = 0; t < ModulatorsPanel::kNumTabs; ++t)
        {
            mods->showTab (t);
            shoot ("modulators - " + juce::String (tabNames[t]));
        }
        mods->showTab (0);
    }
    synthPanel.setView (-1);
    labPanel.showView (LabPanel::View::garden);  shoot ("lab - stacks");
    labPanel.showView (LabPanel::View::tree);    shoot ("lab - history");
    labPanel.showView (LabPanel::View::ideas);   shoot ("lab - list");
    labPanel.showView (LabPanel::View::library); shoot ("lab - library");
    labPanel.showView (LabPanel::View::garden);
    setHome (true);
}

void StacksAudioProcessorEditor::setHome (bool home)
{
    showingHome = home;
    homeView.setVisible (home);
    synthPanel.setVisible (! home);
    labPanel.setVisible (! home);
    modeButton.setButtonText (home ? "Synth" : "Home");
    resized();
}

} // namespace stacks
