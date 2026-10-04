#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

namespace stacks
{

// One candidate patch in the list. Click to audition, ♥ to keep.
class PatchCard : public juce::Component
{
public:
    static constexpr int kHeight = 60;

    PatchCard();
    void set (const Patch&, bool isAuditioned, bool isFavourite);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void()> onAudition, onFavourite;

private:
    juce::TextButton favButton;
    juce::String name, description, category, origin;
    bool auditioned = false, favourite = false;
};

// The generate -> audition -> pick -> evolve workflow.
class LabPanel : public juce::Component,
                 private juce::ChangeListener
{
public:
    explicit LabPanel (StacksAudioProcessor&);
    ~LabPanel() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();
    void rebuildEngineMenu();
    void engineChosen();
    void saveCurrent();
    void loadPatch();

    StacksAudioProcessor& processor;

    juce::Label header, engineLabel, variationLabel, favouritesLabel, status;
    juce::ComboBox engineBox;
    juce::TextButton refreshEnginesButton;
    juce::TextEditor hint;
    juce::Slider variation;
    juce::TextButton newBatchButton { "New batch" }, evolveButton { "Evolve" }, backButton { "<" };
    juce::TextButton favCurrentButton { "+ current" }, saveButton { "Save..." }, loadButton { "Load..." };

    juce::Viewport viewport;
    juce::Component cardList;
    std::vector<std::unique_ptr<PatchCard>> cards;

    juce::Component favouriteStrip;
    std::vector<std::unique_ptr<juce::TextButton>> favouriteChips;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::StringArray engineMenuModels;   // Ollama model name per menu entry
    juce::StringArray engineMenuBuiltins; // built-in model id per menu entry
    int shownGeneration = -1;
};

} // namespace stacks
