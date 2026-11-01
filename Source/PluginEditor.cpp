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
      keyboard (p.keyboardState),
      lifeButton (p.apvts, true)
{
    skin::load();   // PNG pieces replace the drawn look wherever a file exists (see Design/Skin spec.md)
    setLookAndFeel (&lookAndFeel);

    patchName.setFont (StacksLookAndFeel::font (15.0f, true));
    patchName.setColour (juce::Label::textColourId, colours::text);
    patchName.setEditable (false, true, false);
    patchName.setTooltip ("The sound you're hearing. Double-click to rename it.");
    patchName.onTextChange = [this] { synthProcessor.setCurrentPatchName (patchName.getText()); };
    content.addAndMakeVisible (patchName);

    masterKnob.setAccent (colours::accent);
    content.addAndMakeVisible (masterKnob);
    content.addAndMakeVisible (scope);
    content.addAndMakeVisible (tuner);

    settingsButton.setButtonText (juce::String::fromUTF8 ("\xe2\x9a\x99"));   // gear
    settingsButton.setTooltip ("Settings: calm mode, MPE and microtuning");
    settingsButton.onClick = [this]
    {
        if (settingsWindow != nullptr) { settingsWindow->toFront (true); return; }
        settingsWindow = SettingsPanel::show (synthProcessor, this);
    };
    content.addAndMakeVisible (settingsButton);

    undoButton.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xb6"));   // undo arrow
    undoButton.setTooltip ("Undo (Cmd+Z): back to the sound before the last load, drag, tweak or knob move");
    undoButton.onClick = [this] { synthProcessor.undo(); };
    content.addAndMakeVisible (undoButton);
    redoButton.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xb7"));
    redoButton.setTooltip ("Redo (Shift+Cmd+Z)");
    redoButton.onClick = [this] { synthProcessor.redo(); };
    content.addAndMakeVisible (redoButton);
    content.addAndMakeVisible (lifeButton);
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
    content.addAndMakeVisible (synthPanel);
    content.addAndMakeVisible (labPanel);
    content.addAndMakeVisible (homeView);
    homeView.onOpenSynth = [this] { setHome (false); };
    homeView.onSave = [this] { labPanel.savePresetDialog(); };
    modeButton.setTooltip ("Home: pick a kind of sound and grow it. Synth: every knob.");
    modeButton.onClick = [this] { setHome (! showingHome); };
    content.addAndMakeVisible (modeButton);
    setHome (true);
    content.addAndMakeVisible (keyboard);

    synthProcessor.labBroadcaster.addChangeListener (this);
    changeListenerCallback (nullptr);

    content.addChildComponent (tour);   // added last so it sits over everything
    juce::Timer::callAfterDelay (700, [safe = juce::Component::SafePointer<StacksAudioProcessorEditor> (this)]
    {
        if (safe != nullptr) safe->maybeStartTour();
    });

    addAndMakeVisible (content);
    layingOut = true;   // the limits below resize us to the minimum first; that must not be remembered as the user's choice
    setResizable (true, true);
    // No fixed aspect ratio: a host wraps the editor in its own chrome and would fight it.
    // The picture keeps its shape by letterboxing inside whatever size it is given.
    setResizeLimits (juce::roundToInt (kDesignW * 0.6f), juce::roundToInt (kDesignH * 0.6f), juce::roundToInt (kDesignW * 1.5f), juce::roundToInt (kDesignH * 1.5f));
    applyStoredScale();
    layingOut = false;
}

float StacksAudioProcessorEditor::uiScale() const
{
    if (getWidth() <= 0 || getHeight() <= 0) return 1.0f;
    return juce::jmin ((float) getWidth() / (float) kDesignW, (float) getHeight() / (float) kDesignH);
}

// The remembered scale, or one that fits the screen this window is on (a 13 inch
// MacBook gets about 85 percent), in steps of five percent.
void StacksAudioProcessorEditor::applyStoredScale()
{
    float k = synthProcessor.uiScale();
    if (k <= 0.0f)
    {
        auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr
                        ? juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea : juce::Rectangle<int> (1500, 900);
        if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
            area = display->userArea;
        k = juce::jmin (1.0f, (float) (area.getHeight() - 160) / (float) kDesignH, (float) (area.getWidth() - 80) / (float) kDesignW);
        k = std::floor (k * 20.0f) / 20.0f;
    }
    k = juce::jlimit (0.6f, 1.5f, k);
    setSize (juce::roundToInt (kDesignW * k), juce::roundToInt (kDesignH * k));
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
    if (! layingOut)
    {
        const float wanted = synthProcessor.uiScale();
        if (wanted <= 0.0f || std::abs (wanted - (float) getWidth() / (float) kDesignW) > 0.005f)
        {
            layingOut = true;
            applyStoredScale();
            if (wanted <= 0.0f) synthProcessor.setUiScale ((float) getWidth() / (float) kDesignW);   // "fit the screen" settles into a number
            layingOut = false;
        }
    }
    if (showingHome && homeView.showingResults() && ! synthProcessor.tourSeen ("results") && ! tour.isRunning())
        maybeStartTour();   // the first five sounds just landed
}

void StacksAudioProcessorEditor::maybeStartTour()
{
    if (tour.isRunning() || ! isShowing())
        return;
    auto in = [this] (juce::Component& c, juce::Rectangle<int> r) { return std::function<juce::Rectangle<int>()> ([this, &c, r] { return content.getLocalArea (&c, r); }); };
    auto whole = [this] (juce::Component& c) { return std::function<juce::Rectangle<int>()> ([this, &c] { return content.getLocalArea (c.getParentComponent(), c.getBounds()); }); };
    std::vector<TourOverlay::Step> steps;
    juce::String screen;
    if (showingHome && ! homeView.showingResults() && ! synthProcessor.tourSeen ("home"))
    {
        screen = "home";
        steps = { { in (homeView, homeView.tourArea ("tiles")), "Pick a kind of sound. Stacks makes five new ones just for you." },
                  { whole (keyboard), "Play notes here, or use your own MIDI keyboard." },
                  { whole (modeButton), "Synth opens every knob. Home always brings you back here." },
                  { [this] { return undoButton.getBounds().getUnion (redoButton.getBounds()); }, "Undo and redo any change." } };
    }
    else if (showingHome && homeView.showingResults() && ! synthProcessor.tourSeen ("results"))
    {
        screen = "results";
        steps = { { in (homeView, homeView.tourArea ("blocks")), "Click a block to hear it. Each one is a new sound." },
                  { in (homeView, homeView.tourArea ("blocks")), "Drag the empty space to look back at earlier sounds. The + on a block adds it to the mix." },
                  { in (homeView, homeView.tourArea ("variation")), "Choose how different the next five should be." },
                  { in (homeView, homeView.tourArea ("life")), "Life lets the sound slowly change as you play. Off is the sound exactly." },
                  { in (homeView, homeView.tourArea ("grow")), "Grow from this one makes five new sounds based on it." },
                  { in (homeView, homeView.tourArea ("save")), "Save preset keeps the sound in your library." },
                  { in (homeView, homeView.tourArea ("open")), "Open the synth to change every detail." } };
    }
    else if (! showingHome && ! synthProcessor.tourSeen ("synth"))
    {
        screen = "synth";
        steps = { { in (synthPanel, synthPanel.tourArea ("tabs")), "All shows every section. Sections hides the ones you don't need; the rest grow to fill the space." },
                  { in (synthPanel, synthPanel.tourArea ("knob")), "Drag a knob up or down. Hover over it to see its value." },
                  { in (labPanel, labPanel.tourArea ("evolve")), "Evolve makes five new sounds from the one you hear now." },
                  { in (labPanel, labPanel.tourArea ("library")), "Library holds every preset and the sounds you save." },
                  { whole (modeButton), "Home takes you back to the simple view." } };
    }
    if (screen.isEmpty())
        return;
    synthProcessor.markTourSeen (screen);   // once, even if another instance is open too
    tour.setBounds (getLocalBounds());
    tour.toFront (false);
    tour.start (std::move (steps), {});
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
    if (key.getKeyCode() == juce::KeyPress::escapeKey && tour.isRunning()) { tour.finish(); return true; }
    const bool cmd = key.getModifiers().isCommandDown();
    if (cmd && key.getKeyCode() == 'Z' && key.getModifiers().isShiftDown()) { synthProcessor.redo(); return true; }
    if (cmd && key.getKeyCode() == 'Z')  { synthProcessor.undo(); return true; }
    if (cmd && key.getKeyCode() == 'Y')  { synthProcessor.redo(); return true; }
    return false;
}

void StacksAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    g.addTransform (content.getTransform());
    if (skin::has ("background")) skin::drawStretched (g, "background", juce::Rectangle<float> ((float) kDesignW, (float) kDesignH));
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
    // The window is a scaled picture of the 1500 x 860 design, centred when the host's shape differs.
    const float k = uiScale();
    content.setBounds (0, 0, kDesignW, kDesignH);
    content.setTransform (juce::AffineTransform::scale (k).translated (std::floor (((float) getWidth() - kDesignW * k) * 0.5f),
                                                                       std::floor (((float) getHeight() - kDesignH * k) * 0.5f)));
    // The remembered scale follows the width only: a host that trims a few pixels
    // of height (the standalone's options bar) must not shrink the window a
    // little more on every launch.
    const float remembered = (float) getWidth() / (float) kDesignW;
    if (! layingOut && std::abs (synthProcessor.uiScale() - remembered) > 0.005f && synthProcessor.uiScale() > 0.0f)
    {
        // A dragged corner is remembered, once the size has settled; hosts resize us a few times on the way in.
        juce::Timer::callAfterDelay (900, [safe = juce::Component::SafePointer<StacksAudioProcessorEditor> (this), remembered]
        {
            if (safe == nullptr || safe->layingOut || std::abs ((float) safe->getWidth() / (float) kDesignW - remembered) > 0.005f) return;
            safe->layingOut = true;
            safe->synthProcessor.setUiScale (remembered);
            safe->layingOut = false;
        });
    }
    auto r = juce::Rectangle<int> (kDesignW, kDesignH);

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
    header.removeFromRight (8);
    lifeButton.setBounds (header.removeFromRight (60).withSizeKeepingCentre (58, 24));
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
    tour.setBounds (content.getLocalBounds());
}

namespace
{
    using Piece = std::pair<juce::String, juce::Rectangle<int>>;

    // Walks the tree for the standard controls; each file name is reported once.
    void collectPieces (juce::Component& root, juce::Component& c, std::vector<Piece>& out, juce::Rectangle<int> header)
    {
        if (! c.isVisible() || c.getWidth() <= 0 || c.getHeight() <= 0) return;   // isShowing() needs a window; the tool has none
        const auto r = root.getLocalArea (&c, c.getLocalBounds());
        auto add = [&] (const juce::String& name)
        {
            for (const auto& p : out) if (p.first == name) return;
            out.emplace_back (name, r);
        };
        if (dynamic_cast<stacks::ParamKnob*> (&c) != nullptr)
            add (header.contains (r.getCentre()) ? "knob-master.png" : "knob.png");
        else if (auto* b = dynamic_cast<juce::TextButton*> (&c))
        {
            if ((bool) b->getProperties()["tab"]) { if (! header.contains (r.getCentre()) && b->getToggleState()) add ("tab-on.png"); }   // a selected page tab, not a header icon
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

// Boxes one example of every skinned piece on screen and writes its file name
// beside it. Labels are placed where they hit neither another label nor a
// boxed piece, trying above, below, right and left in turn.
void StacksAudioProcessorEditor::drawAnnotations (juce::Image& image, float scale)
{
    std::vector<Piece> pieces;
    // The two whole-window surfaces, labelled at a corner: the header strip, and the window background
    // (pointed at the gap above the keyboard, which is plain background on every screen).
    pieces.emplace_back ("header.png", juce::Rectangle<int> (headerBounds.getRight() - 560, headerBounds.getBottom() - 2, 1, 1));
    pieces.emplace_back ("background.png", juce::Rectangle<int> (8, keyboard.getY() - 20, 1, 1));
    jassert (std::abs (uiScale() - 1.0f) < 0.01f);   // the labels are measured in design space; the tool renders at 100 percent
    for (auto* child : content.getChildren())   // not from the editor itself: headless, it reports itself invisible
        collectPieces (content, *child, pieces, headerBounds);
    auto addFrom = [&] (juce::Component& panel, const std::vector<Piece>& list)
    {
        if (! panel.isVisible()) return;
        for (const auto& p : list)
        {
            bool seen = false;
            for (const auto& q : pieces) seen = seen || q.first == p.first;
            if (! seen) pieces.emplace_back (p.first, content.getLocalArea (&panel, p.second));
        }
    };
    addFrom (synthPanel, synthPanel.skinPieces());
    addFrom (labPanel, labPanel.skinPieces());
    addFrom (homeView, homeView.skinPieces());
    pieces.emplace_back ("key-white.png", content.getLocalArea (&keyboard, keyboard.getRectangleForKey (48).toNearestInt()));
    pieces.emplace_back ("key-black.png", content.getLocalArea (&keyboard, keyboard.getRectangleForKey (49).toNearestInt()));

    if (juce::SystemStats::getEnvironmentVariable ("STACKS_DEBUG_PIECES", {}).isNotEmpty())
        for (const auto& p : pieces) std::fprintf (stderr, "piece %s %d,%d %dx%d\n", p.first.toRawUTF8(), p.second.getX(), p.second.getY(), p.second.getWidth(), p.second.getHeight());
    juce::Graphics g (image);
    g.addTransform (juce::AffineTransform::scale (scale));
    const juce::Colour ink (0xffffd166);
    const auto font = StacksLookAndFeel::font (12.0f, true);
    g.setFont (font);

    // Big surfaces get a label only, no box: a box round the whole window says nothing.
    auto labelOnly = [] (const juce::String& n) { return n == "header.png" || n == "lab.png" || n == "background.png"; };
    std::vector<juce::Rectangle<int>> taken;
    for (const auto& p : pieces)
        if (! labelOnly (p.first)) taken.push_back (p.second.expanded (3));

    auto place = [&] (const juce::Rectangle<int>& r, int w) -> juce::Rectangle<int>
    {
        const int h = 18, gap = 6;
        const juce::Rectangle<int> options[] = {
            { r.getX(), r.getY() - h - gap, w, h }, { r.getX(), r.getBottom() + gap, w, h },
            { r.getRight() + gap, r.getCentreY() - h / 2, w, h }, { r.getX() - w - gap, r.getCentreY() - h / 2, w, h },
            { r.getRight() - w, r.getY() - h - gap, w, h }, { r.getRight() - w, r.getBottom() + gap, w, h } };
        const auto screen = content.getLocalBounds().reduced (2);
        for (int push = 0; push < 12; ++push)
            for (auto o : options)
            {
                o = o.translated (0, (o.getY() < r.getY() ? -1 : 1) * push * (h + 2));
                if (! screen.contains (o)) continue;
                bool clear = true;
                for (const auto& t : taken) clear = clear && ! t.intersects (o);
                if (clear) return o;
            }
        return { juce::jlimit (2, kDesignW - w - 2, r.getX()), juce::jlimit (2, kDesignH - h - 2, r.getY() - h - gap), w, h };
    };

    for (const auto& p : pieces)
    {
        auto r = p.second;
        if (r.isEmpty()) continue;
        const bool boxed = ! labelOnly (p.first);
        if (! boxed && r.getWidth() > 1) r = juce::Rectangle<int> (r.getX() + 8, r.getY() + 4, 1, 1);   // point at the surface's top-left
        if (boxed)
        {
            g.setColour (ink);
            g.drawRect (r.expanded (2), 2);
        }
        const int w = (int) font.getStringWidthFloat (p.first) + 12;
        const auto label = boxed ? place (r, w) : juce::Rectangle<int> (juce::jlimit (2, kDesignW - w - 2, r.getX()), r.getY(), w, 18);
        taken.push_back (label.expanded (2));
        // a leader from the label to the nearest point of the piece
        const auto from = label.getConstrainedPoint (r.getCentre()).toFloat();
        const auto to = r.getConstrainedPoint (label.getCentre()).toFloat();
        g.setColour (ink);
        if (boxed && from.getDistanceFrom (to) > 2.0f) g.drawLine ({ from, to }, 1.5f);
        g.setColour (juce::Colours::black.withAlpha (0.88f));
        g.fillRoundedRectangle (label.toFloat(), 4.0f);
        g.setColour (ink);
        g.drawRoundedRectangle (label.toFloat(), 4.0f, 1.0f);
        g.drawText (p.first, label, juce::Justification::centred);
    }
}

void StacksAudioProcessorEditor::renderScreens (const juce::File& directory, float scale, bool annotate)
{
    layingOut = true;                 // tools render the design size, whatever scale the user remembered
    setSize (kDesignW, kDesignH);
    directory.createDirectory();
    int shot = 0;
    auto shoot = [&] (const juce::String& name)
    {
        resized();
        auto image = createComponentSnapshot (getLocalBounds(), true, scale);
        if (annotate) drawAnnotations (image, scale);
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
        shoot ("synth - only " + juce::String (r < 4 ? rowNames[r] : "row " + juce::String (r)));   // ALL with one section left on
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
    labPanel.showView (LabPanel::View::tree);    shoot ("lab - tree");
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
    if (tour.isRunning()) tour.finish();   // a tour belongs to the screen it was started on
    juce::Timer::callAfterDelay (350, [safe = juce::Component::SafePointer<StacksAudioProcessorEditor> (this)]
    {
        if (safe != nullptr) safe->maybeStartTour();
    });
}

} // namespace stacks
