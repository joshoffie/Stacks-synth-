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

namespace
{
    using Piece = std::pair<juce::String, juce::Rectangle<int>>;

    // Walks the tree for the standard controls; each file name is reported once.
    void collectPieces (juce::Component& root, juce::Component& c, std::vector<Piece>& out, juce::Rectangle<int> header)
    {
        if (! c.isVisible() || c.getWidth() <= 0 || c.getHeight() <= 0) return;   // isShowing() needs a window; the tool has none
        auto add = [&] (const juce::String& name)
        {
            for (const auto& p : out) if (p.first == name) return;
            out.emplace_back (name, root.getLocalArea (&c, c.getLocalBounds()));
        };
        if (auto* knob = dynamic_cast<stacks::ParamKnob*> (&c))
        {
            const auto r = root.getLocalArea (&c, c.getLocalBounds());
            add (header.contains (r.getCentre()) ? "knob-master.png" : "knob.png");
            (void) knob;
        }
        else if (auto* b = dynamic_cast<juce::TextButton*> (&c))
        {
            const auto r = root.getLocalArea (&c, c.getLocalBounds());
            if ((bool) b->getProperties()["tab"]) { if (! header.contains (r.getCentre()) && b->getToggleState()) add ("tab-on.png"); }   // a real, selected page tab, not a header icon
            else if (b->findColour (juce::TextButton::buttonColourId).getPerceivedBrightness() > 0.45f) add ("button-bright.png");
            else add ("button.png");
        }
        else if (dynamic_cast<juce::ToggleButton*> (&c) != nullptr) add ("toggle-on.png");
        else if (dynamic_cast<juce::ComboBox*> (&c) != nullptr) add ("combo.png");
        else if (auto* s = dynamic_cast<juce::Slider*> (&c))
        {
            if (! s->isRotary()) add ("slider-track.png");
        }
        else if (dynamic_cast<juce::TextEditor*> (&c) != nullptr) add ("texteditor.png");
        for (auto* child : c.getChildren())
            collectPieces (root, *child, out, header);
    }
}

void StacksAudioProcessorEditor::annotateScreens (const juce::File& directory, float scale)
{
    directory.createDirectory();
    int shot = 0;
    auto shoot = [&] (const juce::String& name)
    {
        resized();
        auto image = createComponentSnapshot (getLocalBounds(), true, scale);

        std::vector<Piece> pieces;
        pieces.emplace_back ("header.png", headerBounds);
        for (auto* child : getChildren())   // not from the editor itself: headless, it reports itself invisible
            collectPieces (*this, *child, pieces, headerBounds);
        auto addFrom = [&] (juce::Component& panel, const std::vector<Piece>& list)
        {
            if (! panel.isVisible()) return;
            for (const auto& p : list)
            {
                bool seen = false;
                for (const auto& q : pieces) seen = seen || q.first == p.first;
                if (! seen) pieces.emplace_back (p.first, getLocalArea (&panel, p.second));
            }
        };
        addFrom (synthPanel, synthPanel.skinPieces());
        addFrom (labPanel, labPanel.skinPieces());
        addFrom (homeView, homeView.skinPieces());
        pieces.emplace_back ("key-white.png", getLocalArea (&keyboard, keyboard.getRectangleForKey (48).toNearestInt()));
        pieces.emplace_back ("key-black.png", getLocalArea (&keyboard, keyboard.getRectangleForKey (49).toNearestInt()));
        pieces.emplace_back ("background.png", juce::Rectangle<int> (getWidth() - 150, getHeight() - keyboard.getHeight() - 30, 140, 20));

        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (scale));
        const juce::Colour ink (0xffffd166);
        g.setFont (StacksLookAndFeel::font (12.0f, true));
        int n = 0;
        for (const auto& p : pieces)
        {
            const auto r = p.second;
            if (r.isEmpty()) continue;
            g.setColour (ink);
            g.drawRect (r.expanded (2), 2);
            // The label sits above the piece, or below when there is no room; alternate sides so neighbours don't stack.
            const int w = (int) g.getCurrentFont().getStringWidthFloat (p.first) + 12;
            const bool below = r.getY() < 30 || (n % 2 == 1 && r.getBottom() + 26 < getHeight());
            int lx = juce::jlimit (2, getWidth() - w - 2, p.first.startsWith ("key-black") ? r.getRight() + 4 : p.first.startsWith ("key-white") ? r.getX() - w - 4 : r.getX());
            int ly = below ? r.getBottom() + 6 : r.getY() - 24;
            juce::Rectangle<int> label (lx, ly, w, 18);
            g.setColour (juce::Colours::black.withAlpha (0.85f));
            g.fillRoundedRectangle (label.toFloat(), 4.0f);
            g.setColour (ink);
            g.drawRoundedRectangle (label.toFloat(), 4.0f, 1.0f);
            g.drawLine ((float) label.getX() + 8.0f, below ? (float) label.getY() : (float) label.getBottom(), (float) r.getX() + 4.0f, below ? (float) r.getBottom() : (float) r.getY(), 1.5f);
            g.drawText (p.first, label, juce::Justification::centred);
            ++n;
        }

        const auto file = directory.getChildFile (juce::String (++shot).paddedLeft ('0', 2) + " " + name + " (annotated).png");
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
    synthPanel.setView (2);  shoot ("synth - effects");
    synthPanel.setView (-1);
    labPanel.showView (LabPanel::View::garden);  shoot ("lab - stacks");
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
