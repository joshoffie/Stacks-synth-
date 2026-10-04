#include "LabPanel.h"
#include "SynthPanel.h" // colours

namespace stacks
{

namespace
{
    constexpr int kRandomItemId = 1;
    constexpr int kOllamaItemBase = 100;
    constexpr int kOllamaUnavailableId = 99;
}

//==============================================================================
PatchCard::PatchCard()
{
    favButton.setButtonText (juce::String::fromUTF8 ("\xe2\x99\xa5")); // ♥
    favButton.setTitle ("Favourite");
    favButton.setColour (juce::TextButton::textColourOffId, colours::text);
    favButton.setColour (juce::TextButton::textColourOnId, colours::text);
    favButton.onClick = [this] { if (onFavourite) onFavourite(); };
    addAndMakeVisible (favButton);
}

void PatchCard::set (const Patch& p, bool isAuditioned, bool isFavourite)
{
    name = p.name;
    description = p.description;
    category = p.category;
    origin = p.origin;
    auditioned = isAuditioned;
    favourite = isFavourite;
    favButton.setColour (juce::TextButton::buttonColourId, favourite ? colours::accent : colours::panel);
    favButton.setTooltip (favourite ? "Remove from favourites" : "Keep this one: the next evolution breeds from it");
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

void PatchCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (2).toFloat();
    g.setColour (auditioned ? colours::accentDim : colours::card);
    g.fillRoundedRectangle (r, 6.0f);
    if (auditioned)
    {
        g.setColour (colours::accent);
        g.drawRoundedRectangle (r.reduced (0.75f), 6.0f, 1.5f);
    }

    auto area = getLocalBounds().reduced (10, 6).withTrimmedRight (34);
    auto titleRow = area.removeFromTop (18);

    g.setColour (colours::text);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    const int nameWidth = juce::jmin (titleRow.getWidth() - 70, juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 4);
    g.drawText (name, titleRow.removeFromLeft (nameWidth), juce::Justification::centredLeft, true);

    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    juce::String tags = category.toUpperCase();
    if (origin.isNotEmpty())
        tags << (tags.isEmpty() ? "" : "  ") << juce::String::fromUTF8 ("\xe2\x9c\xa6 ") << origin.toUpperCase(); // ✦ AI
    if (tags.isNotEmpty())
    {
        g.setColour (colours::accent);
        g.drawText (tags, titleRow.withTrimmedLeft (6), juce::Justification::centredLeft, true);
    }

    g.setColour (colours::muted);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawFittedText (description, area, juce::Justification::topLeft, 2, 0.9f);
}

//==============================================================================
LabPanel::LabPanel (StacksAudioProcessor& p) : processor (p)
{
    header.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    header.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (header);

    engineLabel.setText ("Engine", juce::dontSendNotification);
    engineLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    engineLabel.setColour (juce::Label::textColourId, colours::muted);
    addAndMakeVisible (engineLabel);

    engineBox.setTooltip ("Who designs the patches: the built-in random breeder, or a local AI model served by Ollama");
    engineBox.onChange = [this] { engineChosen(); };
    addAndMakeVisible (engineBox);

    refreshEnginesButton.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xbb")); // ↻
    refreshEnginesButton.setTooltip ("Look for Ollama models again");
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

    newBatchButton.setTooltip ("Ten fresh patches from scratch");
    newBatchButton.onClick = [this]
    {
        if (processor.lab().generating)
            processor.cancelGeneration();
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

    viewport.setViewedComponent (&cardList, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    favCurrentButton.setTooltip ("Add the sound you're hearing right now (with your knob tweaks) to the favourites");
    favCurrentButton.onClick = [this] { processor.favouriteCurrent(); };
    addAndMakeVisible (favCurrentButton);

    saveButton.onClick = [this] { saveCurrent(); };
    addAndMakeVisible (saveButton);
    loadButton.onClick = [this] { loadPatch(); };
    addAndMakeVisible (loadButton);

    status.setFont (juce::Font (juce::FontOptions (11.0f)));
    status.setColour (juce::Label::textColourId, colours::muted);
    addAndMakeVisible (status);

    processor.labBroadcaster.addChangeListener (this);
    rebuildEngineMenu();
    refresh();
}

LabPanel::~LabPanel()
{
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
    newBatchButton.setBounds (buttons.removeFromLeft (100));
    buttons.removeFromLeft (6);
    evolveButton.setBounds (buttons);
    r.removeFromTop (10);

    favouritesLabel.setBounds (r.removeFromTop (16));
    favouriteStrip.setBounds (r.removeFromTop (26));
    r.removeFromTop (8);

    status.setBounds (r.removeFromBottom (18));
    r.removeFromBottom (4);
    auto bottom = r.removeFromBottom (26);
    favCurrentButton.setBounds (bottom.removeFromLeft (90));
    bottom.removeFromLeft (6);
    saveButton.setBounds (bottom.removeFromLeft (80));
    bottom.removeFromLeft (6);
    loadButton.setBounds (bottom.removeFromLeft (80));
    r.removeFromBottom (8);

    viewport.setBounds (r);

    // Favourite chips flow left to right.
    int x = 0;
    for (auto& chip : favouriteChips)
    {
        const int w = juce::jmin (160, chip->getBestWidthForHeight (24));
        chip->setBounds (x, 1, w, 24);
        x += w + 4;
    }

    // Cards
    const int cardWidth = viewport.getWidth() - (viewport.isVerticalScrollBarShown() ? viewport.getScrollBarThickness() : 0);
    cardList.setSize (juce::jmax (1, cardWidth), (int) cards.size() * PatchCard::kHeight);
    for (int i = 0; i < (int) cards.size(); ++i)
        cards[(size_t) i]->setBounds (0, i * PatchCard::kHeight, cardList.getWidth(), PatchCard::kHeight);
}

void LabPanel::rebuildEngineMenu()
{
    const auto choice = processor.engine();
    const auto& models = processor.ollamaModels();

    engineBox.clear (juce::dontSendNotification);
    engineMenuModels.clear();
    engineBox.addItem ("Random (no AI)", kRandomItemId);

    if (models.isEmpty())
    {
        engineBox.addItem ("Ollama: not running / no models", kOllamaUnavailableId);
        engineBox.setItemEnabled (kOllamaUnavailableId, false);
        // Keep a configured-but-missing model selectable so the setting survives.
        if (choice.kind == EngineKind::Ollama && choice.model.isNotEmpty())
        {
            engineMenuModels.add (choice.model);
            engineBox.addItem ("Ollama: " + choice.model, kOllamaItemBase);
        }
    }
    else
    {
        for (const auto& m : models)
        {
            engineMenuModels.add (m);
            engineBox.addItem ("Ollama: " + m, kOllamaItemBase + engineMenuModels.size() - 1);
        }
        if (choice.kind == EngineKind::Ollama && ! models.contains (choice.model) && choice.model.isNotEmpty())
        {
            engineMenuModels.add (choice.model);
            engineBox.addItem ("Ollama: " + choice.model + " (missing)", kOllamaItemBase + engineMenuModels.size() - 1);
        }
    }

    int selectedId = kRandomItemId;
    if (choice.kind == EngineKind::Ollama)
    {
        const int idx = engineMenuModels.indexOf (choice.model);
        if (idx >= 0)
            selectedId = kOllamaItemBase + idx;
    }
    engineBox.setSelectedId (selectedId, juce::dontSendNotification);
}

void LabPanel::engineChosen()
{
    const int id = engineBox.getSelectedId();
    EngineChoice choice;
    if (id >= kOllamaItemBase && id - kOllamaItemBase < engineMenuModels.size())
    {
        choice.kind = EngineKind::Ollama;
        choice.model = engineMenuModels[id - kOllamaItemBase];
    }
    if (! (choice == processor.engine()))
        processor.setEngine (choice);
}

void LabPanel::refresh()
{
    const auto& lab = processor.lab();

    header.setText (lab.generation > 0 ? juce::String (juce::CharPointer_UTF8 ("AI Lab  \xc2\xb7  Generation ")) + juce::String (lab.generation)
                                       : juce::String ("AI Lab"),
                    juce::dontSendNotification);

    rebuildEngineMenu();

    const int favCount = (int) lab.favourites.size();
    evolveButton.setButtonText (favCount > 0 ? "Evolve from " + juce::String (favCount) + (favCount == 1 ? " favourite" : " favourites")
                                             : "Evolve current sound");
    evolveButton.setEnabled (! lab.generating);
    newBatchButton.setButtonText (lab.generating ? "Stop" : "New batch");
    newBatchButton.setTooltip (lab.generating ? "Stop generating; keep what has arrived" : "Ten fresh patches from scratch");
    backButton.setEnabled (! lab.generating && ! lab.history.empty());
    engineBox.setEnabled (! lab.generating);

    favouritesLabel.setText (favCount > 0 ? "FAVOURITES" : "FAVOURITES  (click a card's heart to keep it)", juce::dontSendNotification);

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
        cards[(size_t) i]->set (c, i == lab.auditioned, processor.indexOfFavourite (c) >= 0);
    }

    // A new batch starts at the top of the list.
    if (lab.generation != shownGeneration)
    {
        shownGeneration = lab.generation;
        viewport.setViewPosition (0, 0);
    }

    status.setText (lab.status.isNotEmpty() ? lab.status
                                            : "Engine: " + processor.engineName() + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  press New batch to start")),
                    juce::dontSendNotification);

    resized();
}

void LabPanel::saveCurrent()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Stacks Patches");
    dir.createDirectory();

    const auto patch = processor.currentPatch();
    const auto safeName = juce::File::createLegalFileName (patch.name);

    chooser = std::make_unique<juce::FileChooser> ("Save patch", dir.getChildFile (safeName + ".json"), "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, patch] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File())
                                  return;
                              auto named = patch;
                              named.name = file.getFileNameWithoutExtension();
                              file.replaceWithText (named.toJson());
                              processor.setCurrentPatchName (named.name);
                              processor.labBroadcaster.sendChangeMessage();
                          });
}

void LabPanel::loadPatch()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Stacks Patches");

    chooser = std::make_unique<juce::FileChooser> ("Load patch", dir, "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (! file.existsAsFile())
                                  return;
                              if (auto patch = Patch::fromJson (file.loadFileAsString()))
                              {
                                  processor.applyPatch (*patch);
                                  processor.favouriteCurrent(); // so it can seed the next evolution
                              }
                          });
}

} // namespace stacks
