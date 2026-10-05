#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

namespace stacks
{

// A folder browser over ~/Documents/Stacks Patches: folders you can enter and
// create, patches you click to load and heart to breed from.
class LibraryPanel : public juce::Component
{
public:
    explicit LibraryPanel (StacksAudioProcessor&);
    ~LibraryPanel() override;
    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    class Row;
    void openFolder (const juce::File&);
    void newFolder();
    void showMenuFor (const juce::File& file);
    void confirmDelete (const juce::File& file);
    void saveHere();

    StacksAudioProcessor& processor;
    juce::Label pathLabel, emptyLabel;
    juce::TextButton upButton { "<" }, newFolderButton { "New folder" }, revealButton { "Finder" }, favouritesOnly, saveHereButton { "Save here" };
    juce::TextButton playOnClick { "Play" };   // audition previews on/off
    bool showFavouritesOnly = false;
    juce::String activeTag;
    std::vector<std::unique_ptr<juce::TextButton>> tagButtons;
    void editTags (const juce::File& file);
    juce::Viewport viewport;
    juce::Component list;
    std::vector<std::unique_ptr<Row>> rows;
};

} // namespace stacks
