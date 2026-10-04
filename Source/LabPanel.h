#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "LibraryPanel.h"
#include "GardenView.h"
#include "TreeView.h"
#include "ExplainView.h"

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
    void set (const Patch&, Style, bool isAuditioned, bool isFavourite, int changesFromSeed = -1);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    std::function<void()> onAudition, onSave;        // click: hear; right-click or the Save button: save
    void showSaveButton (bool);

private:
    juce::TextButton saveButton { "Save" };
    juce::String name, description, category;
    Style style = Style::ai;
    int changesFromSeed = -1;
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

// A slim bar that shows how far the model is toward the next patch, with a
// sweeping segment while it is loading or reading.
class ProgressStrip : public juce::Component,
                      private juce::Timer
{
public:
    ProgressStrip();
    void set (bool active, float fraction, const juce::String& detail, int done, int total);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    bool active = false;
    float fraction = -1.0f;
    juce::String detail;
    int done = 0, total = 0;
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
    enum class View { garden, tree, ideas, library, explain };
    void showView (View);
    void savePresetDialog();                               // folder + name, saves exactly what's playing

    StacksAudioProcessor& processor;

    juce::Label header, variationLabel, status;
    ProgressStrip progressStrip;
    juce::TextEditor hint;
    juce::Slider variation;
    juce::ToggleButton designWavesToggle { "Design wavetables" };
    juce::TextButton savePresetButton;
    juce::TextButton newBatchButton { "Generate" }, evolveButton { "Evolve" }, backButton { "<" };
    juce::TextButton gardenTab { "GARDEN" }, treeTab { "TREE" }, ideasTab { "LIST" }, libraryTab { "LIBRARY" }, explainTab { "EXPLAIN" };
    View view = View::garden;
    LibraryPanel library;
    GardenView garden;
    FamilyTree tree;
    juce::Viewport treeViewport;
    ExplainView explain;

    juce::Viewport viewport;
    juce::Component cardList;
    PatchCard nowPlaying;
    SectionHeader aiHeader, randomHeader;
    std::vector<std::unique_ptr<PatchCard>> cards;
    bool randomExpanded = false, randomExpandedByUser = false;
    juce::String shownNowPlaying;


    int shownGeneration = -1;
};

} // namespace stacks
