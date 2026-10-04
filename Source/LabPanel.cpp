#include "LabPanel.h"
#include "Controls.h" // colours

namespace stacks
{

namespace
{
    constexpr int kRandomItemId = 1;
    constexpr int kOllamaItemBase = 100;
    constexpr int kOllamaUnavailableId = 99;
    constexpr int kBuiltinItemBase = 200;

    const juce::Colour kRandomCard { 0xff23262c };
    const juce::Colour kRandomTag  { 0xff7c828c };

    juce::String sizeText (juce::int64 bytes)
    {
        return juce::String ((double) bytes / 1.0e9, 1) + " GB";
    }

    juce::String heart()  { return juce::String::fromUTF8 ("\xe2\x99\xa5"); }   // ♥
    juce::String spark()  { return juce::String::fromUTF8 ("\xe2\x9c\xa6"); }   // ✦
}

//==============================================================================
PatchCard::PatchCard()
{
    favButton.setButtonText (heart());
    favButton.setTitle ("Favourite");
    favButton.setColour (juce::TextButton::textColourOffId, colours::text);
    favButton.setColour (juce::TextButton::textColourOnId, colours::text);
    favButton.onClick = [this] { if (onFavourite) onFavourite(); };
    addAndMakeVisible (favButton);
}

void PatchCard::set (const Patch& p, Style s, bool isAuditioned, bool isFavourite)
{
    name = p.name;
    description = p.description;
    category = p.category;
    style = s;
    auditioned = isAuditioned;
    favourite = isFavourite;
    favButton.setColour (juce::TextButton::buttonColourId, favourite ? colours::accent : colours::panel);
    favButton.setTooltip (favourite ? "Stop breeding from this (it stays saved in the library)" : "Save into the open library folder and breed from it");
    setTitle ((style == Style::nowPlaying ? "Now playing " : "Audition ") + name);
    repaint();
}

void PatchCard::resized()
{
    favButton.setBounds (getWidth() - 36, 6, 28, 24);
}

void PatchCard::mouseDown (const juce::MouseEvent&)
{
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
    g.fillRoundedRectangle (r, 6.0f);
    if (auditioned || isCurrent)
    {
        g.setColour (isCurrent ? colours::accent.withAlpha (0.7f) : colours::accent);
        g.drawRoundedRectangle (r.reduced (0.75f), 6.0f, 1.5f);
    }

    auto area = getLocalBounds().reduced (10, 6).withTrimmedRight (34);
    auto titleRow = area.removeFromTop (18);

    g.setColour (isRandom ? colours::text.withAlpha (0.8f) : colours::text);
    g.setFont (juce::Font (juce::FontOptions (isRandom ? 13.0f : 14.0f, juce::Font::bold)));
    const int nameWidth = juce::jmin (titleRow.getWidth() - 90, juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 4);
    g.drawText (name, titleRow.removeFromLeft (nameWidth), juce::Justification::centredLeft, true);

    juce::String tags = category.toUpperCase();
    juce::String badge = isCurrent ? "NOW PLAYING" : isRandom ? "RANDOM" : spark() + " AI";
    if (tags.isNotEmpty()) tags << "  ";
    tags << badge;

    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.setColour (isRandom ? kRandomTag : colours::accent);
    g.drawText (tags, titleRow.withTrimmedLeft (6), juce::Justification::centredLeft, true);

    g.setColour (colours::muted);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
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
    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    if (collapsible)
    {
        auto tri = r.removeFromLeft (14).toFloat().reduced (3.0f, 7.0f);
        juce::Path p;
        if (expanded)
            p.addTriangle (tri.getX(), tri.getY(), tri.getRight(), tri.getY(), tri.getCentreX(), tri.getBottom());
        else
            p.addTriangle (tri.getX(), tri.getY(), tri.getRight(), tri.getCentreY(), tri.getX(), tri.getBottom());
        g.fillPath (p);
    }
    g.drawText (title, r, juce::Justification::centredLeft);
    g.setColour (colour.withAlpha (0.25f));
    g.fillRect (r.removeFromBottom (1));
}

//==============================================================================
LabPanel::LabPanel (StacksAudioProcessor& p) : processor (p), library (p), garden (p)
{
    header.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    header.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (header);

    engineLabel.setText ("Engine", juce::dontSendNotification);
    engineLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    engineLabel.setColour (juce::Label::textColourId, colours::muted);
    addAndMakeVisible (engineLabel);

    engineBox.setTooltip ("Who designs the patches: the built-in random breeder, a model running inside Stacks, or the Ollama app");
    engineBox.onChange = [this] { engineChosen(); };
    addAndMakeVisible (engineBox);

    refreshEnginesButton.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xbb")); // ↻
    refreshEnginesButton.setTooltip ("Look for models again");
    refreshEnginesButton.onClick = [this] { processor.refreshOllamaModels(); };
    addAndMakeVisible (refreshEnginesButton);

    hint.setTextToShowWhenEmpty ("Direction (optional): darker, more movement, plucky...", colours::muted);
    hint.setMultiLine (false);
    hint.setReturnKeyStartsNewLine (false);
    hint.onReturnKey = [this] { evolveButton.triggerClick(); };
    addAndMakeVisible (hint);

    variationLabel.setText ("Variation", juce::dontSendNotification);
    variationLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    variationLabel.setColour (juce::Label::textColourId, colours::muted);
    addAndMakeVisible (variationLabel);

    variation.setSliderStyle (juce::Slider::LinearHorizontal);
    variation.setRange (0.0, 1.0, 0.01);
    variation.setValue (0.5);
    variation.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    variation.setTooltip ("How far the children may stray from their parents");
    addAndMakeVisible (variation);

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

    backButton.setTooltip ("Back to the previous batch");
    backButton.onClick = [this] { processor.goBackGeneration(); };
    addAndMakeVisible (backButton);

    favouritesLabel.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    favouritesLabel.setColour (juce::Label::textColourId, colours::accent);
    addAndMakeVisible (favouritesLabel);
    addAndMakeVisible (favouriteStrip);

    nowPlaying.onFavourite = [this]
    {
        const int idx = processor.indexOfFavourite (processor.currentPatch());
        if (idx >= 0) processor.removeFavourite (idx);
        else          processor.favouriteCurrent();
    };
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
    addAndMakeVisible (viewport);

    for (auto* tab : { &gardenTab, &ideasTab, &libraryTab })
    {
        tab->setClickingTogglesState (false);
        tab->setColour (juce::TextButton::buttonOnColourId, colours::accentDim);
        tab->setColour (juce::TextButton::textColourOnId, colours::text);
        addAndMakeVisible (*tab);
    }
    gardenTab.setTooltip ("The breeding view: your sound is the seed, candidates are leaves");
    gardenTab.onClick = [this] { showView (View::garden); };
    ideasTab.setTooltip ("The same candidates as a list with descriptions");
    ideasTab.onClick = [this] { showView (View::ideas); };
    libraryTab.onClick = [this] { showView (View::library); };

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
    libraryTab.setTooltip (juce::String::fromUTF8 ("Your saved patches, in folders. The \xe2\x99\xa5 on any card saves it into the open folder."));
    addChildComponent (library);

    nowPlaying.setTooltip (juce::String::fromUTF8 ("What you're hearing. Its \xe2\x99\xa5 saves it into the open library folder and breeds from it."));

    status.setFont (juce::Font (juce::FontOptions (11.0f)));
    status.setColour (juce::Label::textColourId, colours::muted);
    addAndMakeVisible (status);

    processor.labBroadcaster.addChangeListener (this);
    rebuildEngineMenu();
    showView (View::garden);
    refresh();
    startTimer (1000);
}

LabPanel::~LabPanel()
{
    stopTimer();
    processor.labBroadcaster.removeChangeListener (this);
}

void LabPanel::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
}

void LabPanel::resized()
{
    auto r = getLocalBounds().reduced (10);

    header.setBounds (r.removeFromTop (22));
    r.removeFromTop (4);

    auto engineRow = r.removeFromTop (24);
    engineLabel.setBounds (engineRow.removeFromLeft (60));
    refreshEnginesButton.setBounds (engineRow.removeFromRight (28));
    engineRow.removeFromRight (4);
    engineBox.setBounds (engineRow);
    r.removeFromTop (6);

    hint.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);

    auto varRow = r.removeFromTop (22);
    variationLabel.setBounds (varRow.removeFromLeft (60));
    variation.setBounds (varRow);
    r.removeFromTop (6);

    auto buttons = r.removeFromTop (28);
    backButton.setBounds (buttons.removeFromLeft (30));
    buttons.removeFromLeft (6);
    newBatchButton.setBounds (buttons.removeFromLeft (104));
    buttons.removeFromLeft (6);
    evolveButton.setBounds (buttons);
    r.removeFromTop (10);

    favouritesLabel.setBounds (r.removeFromTop (16));
    favouriteStrip.setBounds (r.removeFromTop (26));
    r.removeFromTop (8);

    auto tabs = r.removeFromTop (24);
    const int tabW = tabs.getWidth() / 3;
    gardenTab.setBounds (tabs.removeFromLeft (tabW).reduced (1, 0));
    ideasTab.setBounds (tabs.removeFromLeft (tabW).reduced (1, 0));
    libraryTab.setBounds (tabs.reduced (1, 0));
    r.removeFromTop (6);

    status.setBounds (r.removeFromBottom (18));
    r.removeFromBottom (4);

    viewport.setBounds (r);
    library.setBounds (r);
    garden.setBounds (r);

    // Favourite chips flow left to right.
    int x = 0;
    for (auto& chip : favouriteChips)
    {
        const int w = juce::jmin (160, chip->getBestWidthForHeight (24));
        chip->setBounds (x, 1, w, 24);
        x += w + 4;
    }

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
        for (int i = 0; i < (int) cards.size(); ++i)
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
    for (int i = 0; i < (int) cards.size(); ++i)
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

void LabPanel::rebuildEngineMenu()
{
    const auto choice = processor.engine();
    const auto& ollama = processor.ollamaModels();
    const auto& builtins = processor.builtInModels();

    engineBox.clear (juce::dontSendNotification);
    engineMenuModels.clear();
    engineMenuBuiltins.clear();

    engineBox.addItem ("Random (no AI)", kRandomItemId);

    engineBox.addSectionHeading ("Built-in AI (runs inside Stacks)");
    for (const auto& m : builtins)
    {
        juce::String label = m.label;
        if (m.fromOllama)       label << " (Ollama's copy)";
        if (! m.installed)      label << "  -  download " << sizeText (m.bytes);
        engineMenuBuiltins.add (m.id);
        engineBox.addItem (label, kBuiltinItemBase + engineMenuBuiltins.size() - 1);
    }

    engineBox.addSectionHeading ("Ollama app");
    if (ollama.isEmpty())
    {
        engineBox.addItem ("Ollama isn't running", kOllamaUnavailableId);
        engineBox.setItemEnabled (kOllamaUnavailableId, false);
        if (choice.kind == EngineKind::Ollama && choice.model.isNotEmpty())
        {
            engineMenuModels.add (choice.model);
            engineBox.addItem ("Ollama: " + choice.model, kOllamaItemBase);
        }
    }
    else
    {
        for (const auto& m : ollama)
        {
            engineMenuModels.add (m);
            engineBox.addItem ("Ollama: " + m, kOllamaItemBase + engineMenuModels.size() - 1);
        }
        if (choice.kind == EngineKind::Ollama && choice.model.isNotEmpty() && ! ollama.contains (choice.model))
        {
            engineMenuModels.add (choice.model);
            engineBox.addItem ("Ollama: " + choice.model + " (missing)", kOllamaItemBase + engineMenuModels.size() - 1);
        }
    }

    int selectedId = kRandomItemId;
    if (choice.kind == EngineKind::Ollama)
    {
        const int idx = engineMenuModels.indexOf (choice.model);
        if (idx >= 0) selectedId = kOllamaItemBase + idx;
    }
    else if (choice.kind == EngineKind::Builtin)
    {
        const int idx = engineMenuBuiltins.indexOf (choice.model);
        if (idx >= 0) selectedId = kBuiltinItemBase + idx;
    }
    engineBox.setSelectedId (selectedId, juce::dontSendNotification);
}

void LabPanel::engineChosen()
{
    const int id = engineBox.getSelectedId();
    EngineChoice choice;

    if (id >= kBuiltinItemBase && id - kBuiltinItemBase < engineMenuBuiltins.size())
    {
        const auto modelId = engineMenuBuiltins[id - kBuiltinItemBase];
        for (const auto& m : processor.builtInModels())
        {
            if (m.id != modelId) continue;
            if (! m.installed)
            {
                // Not here yet: fetch it, the selection moves over once it has arrived.
                processor.startModelDownload (modelId);
                rebuildEngineMenu();
                return;
            }
        }
        choice.kind = EngineKind::Builtin;
        choice.model = modelId;
    }
    else if (id >= kOllamaItemBase && id - kOllamaItemBase < engineMenuModels.size())
    {
        choice.kind = EngineKind::Ollama;
        choice.model = engineMenuModels[id - kOllamaItemBase];
    }

    if (! (choice == processor.engine()))
        processor.setEngine (choice);
}

void LabPanel::refreshNowPlaying()
{
    const auto current = processor.currentPatch();
    const auto key = current.name + "|" + current.description + "|" + juce::String (processor.indexOfFavourite (current));
    if (key == shownNowPlaying)
        return;
    shownNowPlaying = key;
    nowPlaying.set (current, PatchCard::Style::nowPlaying, false, processor.indexOfFavourite (current) >= 0);
}

void LabPanel::timerCallback()
{
    refreshNowPlaying(); // knob tweaks change the description without any broadcast
}

void LabPanel::refresh()
{
    const auto& lab = processor.lab();

    header.setText (lab.generation > 0 ? juce::String (juce::CharPointer_UTF8 ("AI Lab  \xc2\xb7  Generation ")) + juce::String (lab.generation)
                                       : juce::String ("AI Lab"),
                    juce::dontSendNotification);

    rebuildEngineMenu();

    const bool busy = processor.isBusy();
    const int favCount = (int) lab.favourites.size();
    juce::String currentName = processor.currentPatchName();
    if (currentName.length() > 20) currentName = currentName.substring (0, 19) + juce::String::fromUTF8 ("\xe2\x80\xa6");
    evolveButton.setButtonText (favCount > 0 ? "Evolve " + juce::String (favCount) + " " + heart()
                                             : "Evolve: " + currentName);
    evolveButton.setTooltip (favCount > 0 ? "Ten descendants of your favourites, steered by the direction text"
                                          : "Ten descendants of the sound you're playing now, steered by the direction text");
    evolveButton.setEnabled (! busy);
    newBatchButton.setButtonText (busy ? "Stop" : "Fresh ideas");
    newBatchButton.setTooltip (lab.generating ? "Stop generating; keep what has arrived"
                             : processor.isDownloading() ? "Cancel the model download"
                             : "Ten new patches from scratch. Uses the direction text, not the current sound.");
    backButton.setEnabled (! busy && ! lab.history.empty());
    engineBox.setEnabled (! busy);

    favouritesLabel.setText (favCount > 0 ? "BREEDING FROM" : juce::String::fromUTF8 ("BREEDING FROM  (press a \xe2\x99\xa5 to save a patch and breed from it)"), juce::dontSendNotification);

    // Rebuild favourite chips
    favouriteChips.clear();
    for (int i = 0; i < favCount; ++i)
    {
        auto chip = std::make_unique<juce::TextButton> (lab.favourites[(size_t) i].name + "  x");
        chip->setTooltip ("Remove from favourites");
        chip->onClick = [this, i] { processor.removeFavourite (i); };
        favouriteStrip.addAndMakeVisible (*chip);
        favouriteChips.push_back (std::move (chip));
    }

    // Candidate cards
    while (cards.size() < lab.candidates.size())
    {
        auto card = std::make_unique<PatchCard>();
        const int index = (int) cards.size();
        card->onAudition  = [this, index] { processor.audition (index); };
        card->onFavourite = [this, index] { processor.toggleFavourite (index); };
        cardList.addAndMakeVisible (*card);
        cards.push_back (std::move (card));
    }
    while (cards.size() > lab.candidates.size())
        cards.pop_back();

    for (int i = 0; i < (int) cards.size(); ++i)
    {
        const auto& c = lab.candidates[(size_t) i];
        cards[(size_t) i]->set (c, c.origin == "AI" ? PatchCard::Style::ai : PatchCard::Style::random,
                                i == lab.auditioned, processor.indexOfFavourite (c) >= 0);
    }

    shownNowPlaying.clear();
    refreshNowPlaying();
    gardenTab.setToggleState (view == View::garden, juce::dontSendNotification);
    ideasTab.setToggleState (view == View::ideas, juce::dontSendNotification);
    libraryTab.setToggleState (view == View::library, juce::dontSendNotification);
    if (view == View::library) library.refresh();
    garden.refresh();

    // A new batch starts at the top of the list.
    if (lab.generation != shownGeneration)
    {
        shownGeneration = lab.generation;
        viewport.setViewPosition (0, 0);
    }

    status.setText (lab.status.isNotEmpty() ? lab.status
                                            : "Engine: " + processor.engineName() + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  press Fresh ideas to start")),
                    juce::dontSendNotification);

    resized();
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
    if (v == View::library) library.refresh();
    if (v == View::garden)  garden.refresh();
}

} // namespace stacks
