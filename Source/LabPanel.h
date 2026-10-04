#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "LibraryPanel.h"
#include "GardenView.h"

namespace stacks
{

// One patch in the list. Click to audition, ♥ to keep.
class PatchCard : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    enum class Style { nowPlaying, ai, random };
    static constexpr int kHeight = 60, kCompactHeight = 50;

    PatchCard();
    void set (const Patch&, Style, bool isAuditioned, bool isFavourite);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    std::function<void()> onAudition, onFavourite;

private:
    juce::TextButton favButton;
    juce::String name, description, category;
    Style style = Style::ai;
    bool auditioned = false, favourite = false;
};

// Group heading in the list; the random one folds its cards away.
class SectionHeader : public juce::Component
{
public:
    static constexpr int kHeight = 24;
    void set (const juce::String& title, bool collapsible, bool expanded, juce::Colour colour);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (collapsible && onToggle) onToggle(); }
    std::function<void()> onToggle;

private:
    juce::String title;
    bool collapsible = false, expanded = true;
    juce::Colour colour;
};

// The generate -> audition -> pick -> evolve workflow.
class LabPanel : public juce::Component,
                 private juce::ChangeListener,
                 private juce::Timer
{
public:
    explicit LabPanel (StacksAudioProcessor&);
    ~LabPanel() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void timerCallback() override;
    void refresh();
    void refreshNowPlaying();
    void layoutCards();
    void rebuildEngineMenu();
    void engineChosen();
    enum class View { garden, ideas, library };
    void showView (View);

    StacksAudioProcessor& processor;

    juce::Label header, engineLabel, variationLabel, favouritesLabel, status;
    juce::ComboBox engineBox;
    juce::TextButton refreshEnginesButton;
    juce::TextEditor hint;
    juce::Slider variation;
    juce::TextButton newBatchButton { "Fresh ideas" }, evolveButton { "Evolve" }, backButton { "<" };
    juce::TextButton gardenTab { "GARDEN" }, ideasTab { "LIST" }, libraryTab { "LIBRARY" };
    View view = View::garden;
    LibraryPanel library;
    GardenView garden;

    juce::Viewport viewport;
    juce::Component cardList;
    PatchCard nowPlaying;
    SectionHeader aiHeader, randomHeader;
    std::vector<std::unique_ptr<PatchCard>> cards;
    bool randomExpanded = false, randomExpandedByUser = false;
    juce::String shownNowPlaying;

    juce::Component favouriteStrip;
    std::vector<std::unique_ptr<juce::TextButton>> favouriteChips;

    juce::StringArray engineMenuModels;   // Ollama model name per menu entry
    juce::StringArray engineMenuBuiltins; // built-in model id per menu entry
    int shownGeneration = -1;
};

} // namespace stacks
