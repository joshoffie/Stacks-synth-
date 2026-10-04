#include "LibraryPanel.h"
#include "Controls.h"

namespace stacks
{

namespace
{
    constexpr int kFolderRowH = 28, kPatchRowH = 44;
    juce::String heart() { return juce::String::fromUTF8 ("\xe2\x99\xa5"); }
}

//==============================================================================
class LibraryPanel::Row : public juce::Component
{
public:
    Row (juce::File f, bool folder) : file (std::move (f)), isFolder (folder)
    {
        name = file.getFileNameWithoutExtension();
        if (! isFolder)
        {
            if (auto p = Patch::fromJson (file.loadFileAsString()))
            {
                if (p->name.isNotEmpty()) name = p->name;
                description = p->description;
                category = p->category;
                favourite = p->favourite;
            }
            heartButton.setButtonText (heart());
            heartButton.setTitle ("Favourite " + name);
            heartButton.setTooltip ("Favourite");
            heartButton.onClick = [this] { if (onHeart) onHeart(); };
            addAndMakeVisible (heartButton);
            menuButton.setButtonText ("...");
            menuButton.setTooltip ("Move or delete");
            menuButton.onClick = [this] { if (onMenu) onMenu(); };
            addAndMakeVisible (menuButton);
        }
    }

    void setLoaded (bool isLoaded)
    {
        loaded = isLoaded;
        heartButton.setColour (juce::TextButton::buttonColourId, favourite ? colours::accent : colours::panel);
        repaint();
    }

    void resized() override
    {
        if (isFolder) return;
        auto r = getLocalBounds().reduced (6, 4);
        heartButton.setBounds (r.removeFromRight (28).withHeight (24));
        r.removeFromRight (4);
        menuButton.setBounds (r.removeFromRight (30).withHeight (24));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && ! isFolder) { if (onMenu) onMenu(); return; }
        if (onOpen) onOpen();
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        setTitle ((isFolder ? "Open folder " : "Load patch ") + name);
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::button,
            juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this] { if (onOpen) onOpen(); }));
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (2).toFloat();
        if (isFolder)
        {
            g.setColour (colours::card.brighter (0.05f));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (juce::Colour (0xff7fa7d8));
            g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
            g.drawText (juce::String::fromUTF8 ("\xe2\x96\xb8  ") + name, getLocalBounds().reduced (10, 0), juce::Justification::centredLeft, true);
            return;
        }

        g.setColour (loaded ? colours::accentDim : colours::card);
        g.fillRoundedRectangle (r, 5.0f);
        if (loaded)
        {
            g.setColour (colours::accent);
            g.drawRoundedRectangle (r.reduced (0.75f), 5.0f, 1.2f);
        }
        auto area = getLocalBounds().reduced (10, 5).withTrimmedRight (70);
        auto title = area.removeFromTop (17);
        g.setColour (colours::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        const int w = juce::jmin (title.getWidth() - 60, juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 4);
        g.drawText (name, title.removeFromLeft (w), juce::Justification::centredLeft, true);
        if (category.isNotEmpty())
        {
            g.setColour (colours::accent);
            g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
            g.drawText (category.toUpperCase(), title.withTrimmedLeft (6), juce::Justification::centredLeft, true);
        }
        g.setColour (colours::muted);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (description, area, juce::Justification::topLeft, true);
    }

    juce::File file;
    bool isFolder = false, favourite = false, loaded = false;
    juce::String name, description, category;
    juce::TextButton heartButton, menuButton;
    std::function<void()> onOpen, onHeart, onMenu;
};

//==============================================================================
LibraryPanel::~LibraryPanel() = default;

LibraryPanel::LibraryPanel (StacksAudioProcessor& p) : processor (p)
{
    pathLabel.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    pathLabel.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (pathLabel);

    upButton.setTooltip ("Up one folder");
    upButton.onClick = [this] { openFolder (processor.libraryFolder().getParentDirectory()); };
    addAndMakeVisible (upButton);

    newFolderButton.onClick = [this] { newFolder(); };
    addAndMakeVisible (newFolderButton);

    revealButton.setTooltip ("Show this folder in the Finder");
    revealButton.onClick = [this] { processor.libraryFolder().revealToUser(); };
    addAndMakeVisible (revealButton);

    saveHereButton.setTooltip ("Save the sound you're hearing into this folder");
    saveHereButton.setColour (juce::TextButton::buttonColourId, colours::accent);
    saveHereButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    saveHereButton.onClick = [this] { saveHere(); };
    addAndMakeVisible (saveHereButton);

    favouritesOnly.setButtonText (heart());
    favouritesOnly.setTooltip ("Show favourites only");
    favouritesOnly.setClickingTogglesState (true);
    favouritesOnly.setColour (juce::TextButton::buttonOnColourId, colours::accent);
    favouritesOnly.onClick = [this] { showFavouritesOnly = favouritesOnly.getToggleState(); refresh(); };
    addAndMakeVisible (favouritesOnly);

    emptyLabel.setText (juce::String::fromUTF8 ("Nothing here yet. Press the \xe2\x99\xa5 on a card or on Now Playing to save into this folder."), juce::dontSendNotification);
    emptyLabel.setFont (juce::Font (juce::FontOptions (11.5f)));
    emptyLabel.setColour (juce::Label::textColourId, colours::muted);
    emptyLabel.setJustificationType (juce::Justification::centredTop);
    addChildComponent (emptyLabel);

    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    refresh();
}

void LibraryPanel::openFolder (const juce::File& folder)
{
    const auto root = StacksAudioProcessor::libraryRoot();
    if (folder == root || folder.isAChildOf (root))
        processor.setLibraryFolder (folder);
}

void LibraryPanel::newFolder()
{
    auto* w = new juce::AlertWindow ("New folder", "Name for the new folder inside " + processor.libraryFolder().getFileName() + ":", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", "", "Name");
    w->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result == 1)
        {
            const auto name = juce::File::createLegalFileName (w->getTextEditorContents ("name").trim());
            if (name.isNotEmpty())
            {
                const auto folder = processor.libraryFolder().getChildFile (name);
                folder.createDirectory();
                openFolder (folder);
            }
        }
    }), true);
}

void LibraryPanel::saveHere()
{
    const auto current = processor.currentPatch();
    const juce::File original (current.filePath);
    const bool fromPreset = original.existsAsFile();
    const auto originalName = original.getFileNameWithoutExtension();

    auto* w = new juce::AlertWindow ("Save preset here", "Into " + (processor.libraryFolder() == StacksAudioProcessor::libraryRoot() ? juce::String ("the library") : processor.libraryFolder().getFileName()), juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", fromPreset ? originalName + " 2" : processor.currentPatchName(), "Name");
    w->addButton (fromPreset ? "Save as new" : "Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    if (fromPreset)
        w->addButton ("Overwrite \"" + originalName + "\"", 2);
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, original, originalName] (int result)
    {
        if (result == 2)
            processor.savePreset (originalName, original.getParentDirectory(), false);
        else if (result == 1)
            processor.savePreset (w->getTextEditorContents ("name"), processor.libraryFolder(), true);
        refresh();
    }), true);
}

void LibraryPanel::confirmDelete (const juce::File& file)
{
    juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                           .withIconType (juce::MessageBoxIconType::WarningIcon)
                                           .withTitle ("Move to Trash?")
                                           .withMessage ("Move \"" + file.getFileNameWithoutExtension() + "\" to the Trash?")
                                           .withButton ("Move to Trash")
                                           .withButton ("Cancel"),
                                       [this, file] (int result)
                                       {
                                           if (result == 1)
                                           {
                                               file.moveToTrash();
                                               refresh();
                                           }
                                       });
}

void LibraryPanel::showMenuFor (const juce::File& file)
{
    juce::PopupMenu menu;
    juce::PopupMenu moveTo;
    const auto root = StacksAudioProcessor::libraryRoot();
    std::vector<juce::File> folders { root };
    for (const auto& f : root.findChildFiles (juce::File::findDirectories, true))
        folders.push_back (f);
    for (const auto& folder : folders)
    {
        if (folder == file.getParentDirectory()) continue;
        const auto label = folder == root ? juce::String ("(top level)") : folder.getRelativePathFrom (root);
        moveTo.addItem (label, [this, file, folder]
        {
            const auto target = folder.getNonexistentChildFile (file.getFileNameWithoutExtension(), ".json", false);
            if (file.moveFileTo (target))
                refresh();
        });
    }
    menu.addSubMenu ("Move to", moveTo);
    menu.addSeparator();
    menu.addItem ("Move to Trash", [this, file] { confirmDelete (file); });
    menu.showMenuAsync (juce::PopupMenu::Options());
}

void LibraryPanel::refresh()
{
    const auto folder = processor.libraryFolder();
    const auto root = StacksAudioProcessor::libraryRoot();
    pathLabel.setText (folder == root ? juce::String ("Library") : folder.getRelativePathFrom (root).replaceCharacter ('/', '>'), juce::dontSendNotification);
    upButton.setEnabled (folder != root);

    rows.clear();
    auto folders = folder.findChildFiles (juce::File::findDirectories, false);
    folders.sort();
    for (const auto& f : folders)
    {
        auto row = std::make_unique<Row> (f, true);
        row->onOpen = [this, f] { openFolder (f); };
        list.addAndMakeVisible (*row);
        rows.push_back (std::move (row));
    }
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.json");
    files.sort();
    const auto loadedPath = processor.currentPatch().filePath;
    for (const auto& f : files)
    {
        auto row = std::make_unique<Row> (f, false);
        if (showFavouritesOnly && ! row->favourite)
            continue;
        row->setLoaded (loadedPath == f.getFullPathName());
        row->onOpen  = [this, f] { processor.loadLibraryPatch (f); refresh(); };
        row->onHeart = [this, f] { processor.toggleFavouriteFile (f); refresh(); };
        row->onMenu  = [this, f] { showMenuFor (f); };
        list.addAndMakeVisible (*row);
        rows.push_back (std::move (row));
    }
    emptyLabel.setVisible (rows.empty());
    resized();
}

void LibraryPanel::paint (juce::Graphics&) {}

void LibraryPanel::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (24);
    upButton.setBounds (top.removeFromLeft (28));
    top.removeFromLeft (4);
    revealButton.setBounds (top.removeFromRight (56));
    top.removeFromRight (4);
    newFolderButton.setBounds (top.removeFromRight (86));
    top.removeFromRight (4);
    saveHereButton.setBounds (top.removeFromRight (78));
    top.removeFromRight (4);
    favouritesOnly.setBounds (top.removeFromRight (28));
    top.removeFromRight (4);
    pathLabel.setBounds (top);
    r.removeFromTop (6);
    viewport.setBounds (r);
    emptyLabel.setBounds (r.reduced (10, 20));

    const int width = juce::jmax (1, viewport.getWidth() - (viewport.isVerticalScrollBarShown() ? viewport.getScrollBarThickness() : 0));
    int y = 0;
    for (auto& row : rows)
    {
        const int h = row->isFolder ? kFolderRowH : kPatchRowH;
        row->setBounds (0, y, width, h);
        y += h;
    }
    list.setSize (width, juce::jmax (1, y));
}

} // namespace stacks
