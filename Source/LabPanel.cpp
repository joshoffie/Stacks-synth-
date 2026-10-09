#include "LabPanel.h"
#include "PatchGenerator.h"
#include "Controls.h" // colours
#include "StacksLookAndFeel.h"

namespace stacks
{

namespace
{
    const juce::Colour kRandomCard { 0xff1e2126 };
    const juce::Colour kRandomTag  { 0xff7c828c };

    juce::String spark()  { return juce::String::fromUTF8 ("\xe2\x9c\xa6"); }   // ✦
}

//==============================================================================
PatchCard::PatchCard()
{
    saveButton.setTooltip ("Save this sound as a preset: pick a folder and a name");
    saveButton.setColour (juce::TextButton::buttonColourId, colours::accent);
    saveButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    saveButton.onClick = [this] { if (onSave) onSave(); };
    addChildComponent (saveButton);
}

void PatchCard::showSaveButton (bool show)
{
    saveButton.setVisible (show);
    resized();
}

void PatchCard::set (const Patch& p, Style s, bool isAuditioned, bool isFavourite, int changes)
{
    name = p.name;
    changesFromSeed = changes;
    description = p.description;
    category = p.category;
    style = s;
    auditioned = isAuditioned;
    favourite = isFavourite;
    setTitle ((style == Style::nowPlaying ? "Now playing " : "Audition ") + name);
    repaint();
}

void PatchCard::resized()
{
    saveButton.setBounds (getWidth() - 58, 6, 50, 22);
}

void PatchCard::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onSave) onSave(); return; }   // right-click: save it
    if (onAudition)
        onAudition();
}

std::unique_ptr<juce::AccessibilityHandler> PatchCard::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::button,
        juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this] { if (onAudition) onAudition(); }));
}

void PatchCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (2).toFloat();
    const bool isRandom = style == Style::random;
    const bool isCurrent = style == Style::nowPlaying;

    g.setColour (auditioned ? colours::accentDim : isRandom ? kRandomCard : colours::card);
    g.fillRoundedRectangle (r, 5.0f);
    if (auditioned || isCurrent)
    {
        g.setColour (isCurrent ? colours::accent.withAlpha (0.6f) : colours::accent);
        g.drawRoundedRectangle (r.reduced (0.75f), 5.0f, 1.2f);
    }

    auto area = getLocalBounds().reduced (10, 6).withTrimmedRight (saveButton.isVisible() ? 60 : 4);
    auto titleRow = area.removeFromTop (18);

    g.setColour (isRandom ? colours::text.withAlpha (0.8f) : colours::text);
    g.setFont (StacksLookAndFeel::font (isRandom ? 12.5f : 13.0f, true));
    const int nameWidth = juce::jmin (titleRow.getWidth() - 70, juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 4);
    g.drawText (name, titleRow.removeFromLeft (nameWidth), juce::Justification::centredLeft, true);

    juce::String tags = category.toUpperCase();
    juce::String badge = isCurrent ? "NOW PLAYING" : isRandom ? "RANDOM" : spark() + " AI";
    if (tags.isNotEmpty()) tags << "  ";
    tags << badge;
    if (changesFromSeed >= 0 && style != Style::nowPlaying)
        tags << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) << changesFromSeed << " changes";

    g.setFont (StacksLookAndFeel::font (9.5f, true).withExtraKerningFactor (0.06f));
    g.setColour (isRandom ? kRandomTag : colours::accent);
    g.drawText (tags, titleRow.withTrimmedLeft (6), juce::Justification::centredLeft, true);

    g.setColour (colours::muted);
    g.setFont (StacksLookAndFeel::font (10.5f));
    g.drawFittedText (description, area, juce::Justification::topLeft, isRandom ? 1 : 2, 0.9f);
}

//==============================================================================
void SectionHeader::set (const juce::String& t, bool c, bool e, juce::Colour col)
{
    title = t; collapsible = c; expanded = e; colour = col;
    setMouseCursor (collapsible ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void SectionHeader::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (6, 0);
    g.setColour (colour);
    g.setFont (StacksLookAndFeel::font (9.5f, true).withExtraKerningFactor (0.08f));
    if (collapsible)
    {
        auto tri = r.removeFromLeft (12).toFloat().reduced (3.0f, 7.0f);
        juce::Path p;
        if (expanded)
            p.addTriangle (tri.getX(), tri.getY(), tri.getRight(), tri.getY(), tri.getCentreX(), tri.getBottom());
        else
            p.addTriangle (tri.getX(), tri.getY(), tri.getRight(), tri.getCentreY(), tri.getX(), tri.getBottom());
        g.fillPath (p);
    }
    g.drawText (title, r, juce::Justification::centredLeft);
    g.setColour (colour.withAlpha (0.2f));
    g.fillRect (r.removeFromBottom (1));
}

//==============================================================================
ProgressStrip::ProgressStrip() {}

void ProgressStrip::set (bool isActive, float f, const juce::String& text, int d, int t)
{
    active = isActive; fraction = f; detail = text; done = d; total = t;
    if (active && ! isTimerRunning()) startTimerHz (30);
    if (! active && isTimerRunning()) stopTimer();
    repaint();
}

void ProgressStrip::paint (juce::Graphics& g)
{
    if (! active)
        return;
    auto r = getLocalBounds().toFloat();
    auto bar = r.removeFromTop (6.0f).reduced (0.0f, 1.0f);
    g.setColour (colours::card);
    g.fillRoundedRectangle (bar, 2.0f);

    // Completed patches as segments, the current one filling up.
    const int segments = juce::jmax (1, total);
    const float segW = bar.getWidth() / (float) segments;
    for (int i = 0; i < segments; ++i)
    {
        auto seg = juce::Rectangle<float> (bar.getX() + segW * (float) i, bar.getY(), segW - 2.0f, bar.getHeight());
        if (i < done)
        {
            g.setColour (colours::accent);
            g.fillRoundedRectangle (seg, 2.0f);
        }
        else if (i == done)
        {
            if (fraction >= 0.0f)
            {
                g.setColour (colours::accent.withAlpha (0.9f));
                g.fillRoundedRectangle (seg.withWidth (seg.getWidth() * juce::jlimit (0.0f, 1.0f, fraction)), 2.0f);
            }
            else
            {
                // indeterminate sweep
                const float t = (float) std::fmod (juce::Time::getMillisecondCounterHiRes() / 900.0, 1.0);
                const float w = seg.getWidth() * 0.3f;
                const float x = seg.getX() + (seg.getWidth() - w) * (0.5f - 0.5f * std::cos (t * juce::MathConstants<float>::twoPi));
                g.setColour (colours::accent.withAlpha (0.7f));
                g.fillRoundedRectangle (x, seg.getY(), w, seg.getHeight(), 2.0f);
            }
        }
    }

    g.setColour (colours::text.withAlpha (0.85f));
    g.setFont (StacksLookAndFeel::font (10.0f));
    g.drawFittedText (detail, r.toNearestInt().withTrimmedTop (1), juce::Justification::centredLeft, 1);
}

//==============================================================================
LabPanel::LabPanel (StacksAudioProcessor& p) : processor (p), library (p), garden (p), tree (p), explain (p)
{
    header.setText ("AI LAB", juce::dontSendNotification);
    header.setFont (StacksLookAndFeel::font (10.0f, true).withExtraKerningFactor (0.18f));
    header.setColour (juce::Label::textColourId, colours::muted);
    addAndMakeVisible (header);

    generationLabel.setFont (StacksLookAndFeel::font (10.0f, true).withExtraKerningFactor (0.1f));
    generationLabel.setColour (juce::Label::textColourId, colours::accent);
    generationLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (generationLabel);

    hint.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Describe a sound and press Return\xe2\x80\xa6 or drop a recording here"), colours::muted);
    hint.setMultiLine (false);
    hint.setReturnKeyStartsNewLine (false);
    hint.setFont (StacksLookAndFeel::font (12.0f));
    hint.onReturnKey = [this] { newBatchButton.triggerClick(); };   // a prompt stands on its own: no preset needed
    hint.onEscapeKey = [this] { hint.giveAwayKeyboardFocus(); };    // back to the host's keyboard
    addAndMakeVisible (hint);

    tweak.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Quick tweak the playing sound: brighter, more reverb, shorter\xe2\x80\xa6"), colours::muted);
    tweak.setMultiLine (false);
    tweak.setReturnKeyStartsNewLine (false);
    tweak.setFont (StacksLookAndFeel::font (12.0f));
    tweak.setTooltip ("A few words that change the sound you're hearing, in place: brighter, darker, warmer, wider, shorter, punchier, softer, dirtier, "
                      "more/less reverb, delay, chorus, bass, noise, movement, vibrato, faster, slower, an octave up... Plain words act at once; "
                      "anything else goes to the AI model. Cmd+Z undoes a tweak.");
    tweak.onReturnKey = [this]
    {
        const auto text = tweak.getText().trim();
        if (text.isEmpty()) return;
        processor.quickTweak (text);
        tweak.setText ({}, juce::dontSendNotification);
    };
    tweak.onEscapeKey = [this] { tweak.giveAwayKeyboardFocus(); };
    addAndMakeVisible (tweak);

    variationLabel.setText ("Variation", juce::dontSendNotification);
    variationLabel.setFont (StacksLookAndFeel::font (10.5f));
    variationLabel.setColour (juce::Label::textColourId, colours::muted);
    addAndMakeVisible (variationLabel);

    variation.setSliderStyle (juce::Slider::LinearHorizontal);
    variation.setRange (0.0, 1.0, 0.01);
    variation.setValue (0.5);
    variation.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    variation.setTooltip ("How far the children may stray from their parents");
    addAndMakeVisible (variation);

    designWavesToggle.setTooltip ("Let the AI and the random breeder invent new wavetables for oscillator A (shown as \"Custom\"), "
                                  "instead of only picking built-in ones. AI batches take about a third longer.");
    designWavesToggle.setColour (juce::ToggleButton::textColourId, colours::muted);
    designWavesToggle.setColour (juce::ToggleButton::tickColourId, colours::accent);
    designWavesToggle.onClick = [this] { processor.setDesignWavetables (designWavesToggle.getToggleState()); };
    addAndMakeVisible (designWavesToggle);

    savePresetButton.setButtonText ("Save");
    savePresetButton.setTooltip ("Save the sound you're hearing as a preset: pick a folder and a name. Hearts live in the library.");
    savePresetButton.onClick = [this] { savePresetDialog(); };
    addAndMakeVisible (savePresetButton);

    newBatchButton.onClick = [this]
    {
        if (processor.lab().generating)
            processor.cancelGeneration();
        else if (processor.isDownloading())
            processor.cancelDownload();
        else
            processor.requestNewBatch (hint.getText(), (float) variation.getValue());
    };
    addAndMakeVisible (newBatchButton);

    evolveButton.setColour (juce::TextButton::buttonColourId, colours::accent);
    evolveButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    evolveButton.onClick = [this] { processor.requestEvolve (hint.getText(), (float) variation.getValue()); };
    addAndMakeVisible (evolveButton);

    fromAudioButton.setTooltip ("Pick an audio file (or drop one anywhere on this panel). A short recording is recreated: Stacks measures its pitch, envelope, spectrum, noise, width and vibrato, loads an imitation at once and evolves it. A whole track (over 20 s) is analysed for tempo, key and where the mix has room, and a fresh batch is designed to fit it.");
    fromAudioButton.onClick = [this]
    {
        audioChooser = std::make_unique<juce::FileChooser> ("Recreate a recording", juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                            "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        audioChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                   [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) recreateFromAudio (fc.getResult()); });
    };
    addAndMakeVisible (fromAudioButton);

    backButton.setTooltip ("Back to the previous batch");
    backButton.onClick = [this] { processor.goBackGeneration(); };
    addAndMakeVisible (backButton);

    nowPlaying.onSave = [this] { savePresetDialog(); };
    garden.onSave = [this] { savePresetDialog(); };
    nowPlaying.showSaveButton (true);
    cardList.addAndMakeVisible (nowPlaying);

    aiHeader.set (spark() + " AI IDEAS", false, true, colours::accent);
    cardList.addAndMakeVisible (aiHeader);
    randomHeader.onToggle = [this]
    {
        randomExpanded = ! randomExpanded;
        randomExpandedByUser = true;
        layoutCards();
    };
    cardList.addAndMakeVisible (randomHeader);

    viewport.setViewedComponent (&cardList, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);

    for (auto* tab : { &gardenTab, &treeTab, &ideasTab, &libraryTab, &explainTab })
    {
        styleAsTab (*tab);
        tab->setColour (juce::TextButton::buttonOnColourId, colours::accent);
        addAndMakeVisible (*tab);
    }
    gardenTab.setTooltip ("Stacks: your sound is the seed in the middle, the new ideas grow around it. Click a leaf to hear it, drag it in or out to blend it with the seed, right-click to plant or save it.");
    gardenTab.onClick = [this] { showView (View::garden); };
    ideasTab.setTooltip ("The same candidates as a list with descriptions");
    ideasTab.onClick = [this] { showView (View::ideas); };
    libraryTab.onClick = [this] { showView (View::library); };
    treeTab.setTooltip ("History: every generation this session as a family tree, click any node to hear it again");
    treeTab.onClick = [this] { showView (View::tree); };
    explainTab.setTooltip ("Why the playing sound sounds like this, and which knobs to try");
    explainTab.onClick = [this] { showView (View::explain); };
    treeViewport.setViewedComponent (&tree, false);
    treeViewport.setScrollBarsShown (true, true);
    treeViewport.setScrollBarThickness (6);
    addChildComponent (treeViewport);
    addChildComponent (explain);

    garden.onEvolve = [this] { processor.requestEvolve (hint.getText(), (float) variation.getValue()); };
    garden.onFresh = [this] { processor.requestNewBatch (hint.getText(), (float) variation.getValue()); };
    garden.onEvolveFrom = [this] (int index)
    {
        const auto& lab = processor.lab();
        if (index >= 0 && index < (int) lab.candidates.size())
            processor.requestEvolveFrom (lab.candidates[(size_t) index], hint.getText(), (float) variation.getValue());
    };
    garden.getVariation = [this] { return (float) variation.getValue(); };
    garden.setVariation = [this] (float v) { variation.setValue (v, juce::sendNotificationSync); };
    addChildComponent (garden);
    libraryTab.setTooltip ("Your saved patches, in your folders.");
    addChildComponent (library);

    nowPlaying.setTooltip ("What you're hearing. Evolve grows from it. Save stores it as a preset in a folder you choose.");

    status.setFont (StacksLookAndFeel::font (10.5f));
    status.setColour (juce::Label::textColourId, colours::muted);
    status.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (status);
    addAndMakeVisible (progressStrip);

    processor.labBroadcaster.addChangeListener (this);
    showView (View::garden);
    refresh();
    startTimer (1000);
}

LabPanel::~LabPanel()
{
    stopTimer();
    processor.labBroadcaster.removeChangeListener (this);
}

bool LabPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg"))
            return true;
    return false;
}

void LabPanel::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg"))
        {
            recreateFromAudio (juce::File (f));
            return;
        }
}

// Analyse -> imitation as the current sound -> the brief in the prompt box -> Evolve.
void LabPanel::recreateFromAudio (const juce::File& file)
{
    juce::String brief, error;
    if (StacksAudioProcessor::looksLikeASong (file))
    {
        // A whole track: tempo, key and the room in the mix become the brief for a fresh batch.
        if (! processor.designForSong (file, brief, error))
        {
            juce::NativeMessageBox::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                                   .withTitle ("Couldn't analyse that track").withMessage (error).withButton ("OK"), nullptr);
            return;
        }
        hint.setText (brief, juce::dontSendNotification);
        processor.requestNewBatch (brief, (float) variation.getValue());
        return;
    }
    if (! processor.recreateFromAudio (file, brief, error))
    {
        juce::NativeMessageBox::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                               .withTitle ("Couldn't recreate that file").withMessage (error).withButton ("OK"), nullptr);
        return;
    }
    hint.setText (brief, juce::dontSendNotification);
    processor.requestEvolve (brief, (float) variation.getValue());
}

void LabPanel::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 7.0f);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 7.0f, 1.0f);
}

void LabPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);

    auto top = r.removeFromTop (16);
    generationLabel.setBounds (top.removeFromRight (80));
    header.setBounds (top);
    r.removeFromTop (6);

    hint.setBounds (r.removeFromTop (26));
    r.removeFromTop (5);
    tweak.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);

    auto varRow = r.removeFromTop (18);
    variationLabel.setBounds (varRow.removeFromLeft (54));
    designWavesToggle.setBounds (varRow.removeFromRight (136));
    varRow.removeFromRight (8);
    variation.setBounds (varRow);
    r.removeFromTop (6);

    auto buttons = r.removeFromTop (26);
    backButton.setBounds (buttons.removeFromLeft (26));
    buttons.removeFromLeft (5);
    const int bw = (buttons.getWidth() - 3 * 5) / 4;
    newBatchButton.setBounds (buttons.removeFromLeft (bw));
    buttons.removeFromLeft (5);
    evolveButton.setBounds (buttons.removeFromLeft (bw));
    buttons.removeFromLeft (5);
    fromAudioButton.setBounds (buttons.removeFromLeft (bw));
    buttons.removeFromLeft (5);
    savePresetButton.setBounds (buttons);
    r.removeFromTop (4);
    progressStrip.setBounds (r.removeFromTop (18));
    r.removeFromTop (2);

    auto tabs = r.removeFromTop (22);
    const int tabW = tabs.getWidth() / 5;
    gardenTab.setBounds (tabs.removeFromLeft (tabW));
    treeTab.setBounds (tabs.removeFromLeft (tabW));
    ideasTab.setBounds (tabs.removeFromLeft (tabW));
    libraryTab.setBounds (tabs.removeFromLeft (tabW));
    explainTab.setBounds (tabs);
    r.removeFromTop (6);

    status.setBounds (r.removeFromBottom (16));
    r.removeFromBottom (4);

    viewport.setBounds (r);
    library.setBounds (r);
    garden.setBounds (r);
    treeViewport.setBounds (r);
    tree.setSize (juce::jmax (tree.preferredWidth(), r.getWidth()), juce::jmax (tree.getHeight(), r.getHeight()));
    explain.setBounds (r);

    layoutCards();
}

// Now Playing, then the AI ideas, then the (foldable) random variations.
void LabPanel::layoutCards()
{
    const auto& lab = processor.lab();
    const int width = juce::jmax (1, viewport.getWidth() - (viewport.isVerticalScrollBarShown() ? viewport.getScrollBarThickness() : 0));
    const bool aiEngine = processor.engine().kind != EngineKind::Random;

    int aiCount = 0, randomCount = 0;
    for (const auto& c : lab.candidates)
        (c.origin == "AI" ? aiCount : randomCount)++;

    const bool aiBusy = lab.generating && aiEngine;
    const bool showAi = aiCount > 0 || aiBusy;
    const bool expanded = randomExpandedByUser ? randomExpanded : ! showAi;

    int y = 0;
    nowPlaying.setBounds (0, y, width, PatchCard::kHeight);
    y += PatchCard::kHeight + 4;

    aiHeader.setVisible (showAi);
    if (showAi)
    {
        juce::String title = spark() + " AI IDEAS";
        if (aiBusy) title << "   -   designing " << (aiCount + 1) << " of " << StacksAudioProcessor::kAiPatchesPerBatch << "...";
        aiHeader.set (title, false, true, colours::accent);
        aiHeader.setBounds (0, y, width, SectionHeader::kHeight);
        y += SectionHeader::kHeight;
        const int shown = (int) juce::jmin (cards.size(), lab.candidates.size());
        for (int i = 0; i < shown; ++i)
        {
            if (lab.candidates[(size_t) i].origin != "AI") continue;
            cards[(size_t) i]->setVisible (true);
            cards[(size_t) i]->setBounds (0, y, width, PatchCard::kHeight);
            y += PatchCard::kHeight;
        }
        y += 6;
    }

    randomHeader.setVisible (randomCount > 0);
    if (randomCount > 0)
    {
        randomHeader.set ((showAi ? "RANDOM VARIATIONS (" : "RANDOM PATCHES (") + juce::String (randomCount) + ")", true, expanded, kRandomTag);
        randomHeader.setBounds (0, y, width, SectionHeader::kHeight);
        y += SectionHeader::kHeight;
    }
    for (int i = 0; i < (int) juce::jmin (cards.size(), lab.candidates.size()); ++i)
    {
        if (lab.candidates[(size_t) i].origin == "AI") continue;
        cards[(size_t) i]->setVisible (expanded);
        if (expanded)
        {
            cards[(size_t) i]->setBounds (0, y, width, PatchCard::kCompactHeight);
            y += PatchCard::kCompactHeight;
        }
    }

    cardList.setSize (width, juce::jmax (y, 1));
}

void LabPanel::refreshNowPlaying()
{
    auto current = processor.currentPatch();
    const bool edited = juce::File (current.filePath).existsAsFile() && processor.currentIsEdited();
    if (edited)
        current.category = (current.category.isNotEmpty() ? current.category + "  " : juce::String()) + "edited";
    const auto key = current.name + "|" + current.description + "|" + (current.favourite ? "1" : "0") + (edited ? "e" : "");
    if (key == shownNowPlaying)
        return;
    shownNowPlaying = key;
    nowPlaying.set (current, PatchCard::Style::nowPlaying, false, current.favourite);
}

void LabPanel::timerCallback()
{
    refreshNowPlaying(); // knob tweaks change the description without any broadcast
}

// The newest generation sits at the right end of the tree: bring it into view.
void LabPanel::scrollTreeToNewest()
{
    tree.refresh();
    tree.setSize (juce::jmax (tree.preferredWidth(), treeViewport.getWidth()), juce::jmax (tree.getHeight(), treeViewport.getHeight()));
    treeViewport.setViewPosition (juce::jmax (0, tree.getWidth() - treeViewport.getWidth()), 0);
}

void LabPanel::refresh()
{
    const auto& lab = processor.lab();

    generationLabel.setText (lab.generation > 0 ? "GEN " + juce::String (lab.generation) : juce::String(), juce::dontSendNotification);

    const bool busy = processor.isBusy();
    evolveButton.setTooltip ("Ten descendants of \"" + processor.currentPatchName() + "\", the sound you're playing now, steered by the direction text");
    evolveButton.setEnabled (! busy);
    newBatchButton.setButtonText (busy ? "Stop" : "Generate");
    newBatchButton.setTooltip (lab.generating ? "Stop generating; keep what has arrived"
                             : processor.isDownloading() ? "Cancel the model download"
                             : "New patches from your description alone - no preset needed. (Evolve grows from the sound you're hearing instead.)");
    backButton.setEnabled (! busy && ! lab.history.empty());
    designWavesToggle.setToggleState (processor.designWavetables(), juce::dontSendNotification);
    tweak.setEnabled (! lab.tweaking);

    // Candidate cards
    while (cards.size() < lab.candidates.size())
    {
        auto card = std::make_unique<PatchCard>();
        const int index = (int) cards.size();
        card->onAudition  = [this, index] { processor.audition (index); };
        card->onSave = [this, index] { processor.audition (index); savePresetDialog(); };   // right-click a card: save it
        cardList.addAndMakeVisible (*card);
        cards.push_back (std::move (card));
    }
    while (cards.size() > lab.candidates.size())
        cards.pop_back();

    for (int i = 0; i < (int) cards.size(); ++i)
    {
        const auto& c = lab.candidates[(size_t) i];
        cards[(size_t) i]->set (c, c.origin == "AI" ? PatchCard::Style::ai : PatchCard::Style::random,
                                i == lab.auditioned, c.favourite, lab.seedIsPatch ? countAudibleDifferences (c, lab.seed) : -1);
    }

    shownNowPlaying.clear();
    refreshNowPlaying();
    gardenTab.setToggleState (view == View::garden, juce::dontSendNotification);
    ideasTab.setToggleState (view == View::ideas, juce::dontSendNotification);
    libraryTab.setToggleState (view == View::library, juce::dontSendNotification);
    treeTab.setToggleState (view == View::tree, juce::dontSendNotification);
    explainTab.setToggleState (view == View::explain, juce::dontSendNotification);
    if (view == View::tree)
    {
        if (lab.generation != treeGeneration) { treeGeneration = lab.generation; scrollTreeToNewest(); }
        else tree.refresh();
    }
    if (view == View::library) library.refresh();
    garden.refresh();

    // A new batch starts at the top of the list.
    if (lab.generation != shownGeneration)
    {
        shownGeneration = lab.generation;
        viewport.setViewPosition (0, 0);
    }

    {
        int aiDone = 0;
        for (const auto& c : lab.candidates) if (c.origin == "AI") ++aiDone;
        const bool aiEngine = processor.engine().kind != EngineKind::Random;
        progressStrip.set (lab.generating && aiEngine, lab.progress, lab.progressDetail, aiDone, StacksAudioProcessor::kAiPatchesPerBatch);
    }

    status.setText (lab.status.isNotEmpty() ? lab.status : processor.engineName(), juce::dontSendNotification);
    status.setColour (juce::Label::textColourId, lab.tweaking ? colours::accent : colours::muted);

    resized();
}

void LabPanel::savePresetDialog()
{
    const auto folders = processor.libraryFolders();
    const auto root = StacksAudioProcessor::libraryRoot();
    juce::StringArray names;
    int selected = 0;
    for (int i = 0; i < (int) folders.size(); ++i)
    {
        names.add (folders[(size_t) i] == root ? juce::String ("Library") : folders[(size_t) i].getRelativePathFrom (root));
        if (folders[(size_t) i] == processor.libraryFolder()) selected = i;
    }

    // Loaded from a preset? Then the choice is explicit: a new preset, or overwrite the original.
    const auto current = processor.currentPatch();
    const juce::File original (current.filePath);
    const bool fromPreset = original.existsAsFile();
    const auto originalName = original.getFileNameWithoutExtension();

    auto* w = new juce::AlertWindow ("Save preset",
                                     fromPreset ? "This sound came from \"" + originalName + "\"" + (processor.currentIsEdited() ? " and you've changed it." : ".")
                                                : juce::String ("Saves exactly what you're hearing, knob tweaks included."),
                                     juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", fromPreset ? originalName + " 2" : processor.currentPatchName(), "Name for the new preset");
    w->addComboBox ("folder", names, "Folder");
    w->getComboBoxComponent ("folder")->setSelectedItemIndex (selected, juce::dontSendNotification);
    w->addButton (fromPreset ? "Save as new" : "Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    if (fromPreset)
        w->addButton ("Overwrite \"" + originalName + "\"", 2);
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, folders, original, originalName] (int result)
    {
        if (result == 2)
        {
            processor.savePreset (originalName, original.getParentDirectory(), false);
        }
        else if (result == 1)
        {
            const int idx = w->getComboBoxComponent ("folder")->getSelectedItemIndex();
            const auto folder = idx >= 0 && idx < (int) folders.size() ? folders[(size_t) idx] : StacksAudioProcessor::libraryRoot();
            processor.savePreset (w->getTextEditorContents ("name"), folder, true);
            processor.setLibraryFolder (folder);
        }
    }), true);
}

void LabPanel::showView (View v)
{
    view = v;
    gardenTab.setToggleState (v == View::garden, juce::dontSendNotification);
    ideasTab.setToggleState (v == View::ideas, juce::dontSendNotification);
    libraryTab.setToggleState (v == View::library, juce::dontSendNotification);
    garden.setVisible (v == View::garden);
    viewport.setVisible (v == View::ideas);
    library.setVisible (v == View::library);
    treeTab.setToggleState (v == View::tree, juce::dontSendNotification);
    explainTab.setToggleState (v == View::explain, juce::dontSendNotification);
    treeViewport.setVisible (v == View::tree);
    explain.setVisible (v == View::explain);
    if (v == View::tree) { treeGeneration = processor.lab().generation; scrollTreeToNewest(); }
    if (v == View::explain && processor.lab().explanationKey != processor.currentExplanationKey()) processor.explainCurrentPatch();
    if (v == View::library) library.refresh();
    if (v == View::garden)  garden.refresh();
}

} // namespace stacks
