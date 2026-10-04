#include "SettingsPanel.h"
#include "PluginProcessor.h"
#include "Controls.h"

namespace stacks
{

namespace
{
    constexpr int kRandomItemId = 1, kOllamaUnavailableId = 99, kOllamaItemBase = 100, kBuiltinItemBase = 200;

    juce::String sizeText (juce::int64 bytes)
    {
        if (bytes <= 0) return {};
        return bytes >= 1000000000 ? juce::String ((double) bytes / 1.0e9, 1) + " GB" : juce::String (bytes / 1000000) + " MB";
    }
}

SettingsPanel::SettingsPanel (StacksAudioProcessor& p) : processor (p)
{
    setLookAndFeel (&lookAndFeel);

    title.setText ("Settings", juce::dontSendNotification);
    title.setFont (StacksLookAndFeel::font (20.0f, true));
    title.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (title);

    engineLabel.setText ("AI engine", juce::dontSendNotification);
    engineLabel.setFont (StacksLookAndFeel::font (12.0f, true));
    engineLabel.setColour (juce::Label::textColourId, colours::muted);
    addAndMakeVisible (engineLabel);

    engineBox.setTooltip ("Who designs the patches: a model running inside Stacks, a model served by the Ollama app, or the random breeder");
    engineBox.onChange = [this] { engineChosen(); };
    addAndMakeVisible (engineBox);

    refreshButton.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xbb"));
    refreshButton.setTooltip ("Look for models again (Ollama, downloads, your own files)");
    refreshButton.onClick = [this] { processor.refreshOllamaModels(); processor.refreshModels(); rebuildMenu(); refresh(); };
    addAndMakeVisible (refreshButton);

    statusLabel.setFont (StacksLookAndFeel::font (12.0f));
    statusLabel.setColour (juce::Label::textColourId, colours::text);
    statusLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (statusLabel);

    downloadButton.setButtonText ("Download");
    downloadButton.onClick = [this]
    {
        if (processor.isDownloading()) processor.cancelDownload();
        else processor.startModelDownload (processor.engine().model);
        refresh();
    };
    addAndMakeVisible (downloadButton);

    addModelButton.setButtonText ("Add your own model (.gguf)...");
    addModelButton.setTooltip ("Any llama.cpp-compatible chat model in GGUF format. See the guide below.");
    addModelButton.onClick = [this] { addCustomModel(); };
    addAndMakeVisible (addModelButton);

    removeModelButton.setButtonText ("Forget this model");
    removeModelButton.setTooltip ("Removes your own model from the list (the file stays where it is)");
    removeModelButton.onClick = [this]
    {
        ModelManager::removeCustomModel (processor.engine().model);
        processor.refreshModels();
        processor.setEngine ({ EngineKind::Builtin, "qwen3-4b" });
        rebuildMenu();
        refresh();
    };
    addAndMakeVisible (removeModelButton);

    calmToggle.setColour (juce::ToggleButton::textColourId, colours::text);
    calmToggle.onClick = [this] { processor.setCalmMode (calmToggle.getToggleState()); };
    addAndMakeVisible (calmToggle);

    guideTitle.setText ("Model guide", juce::dontSendNotification);
    guideTitle.setFont (StacksLookAndFeel::font (12.0f, true));
    guideTitle.setColour (juce::Label::textColourId, colours::muted);
    addAndMakeVisible (guideTitle);

    guide.setMultiLine (true, true);
    guide.setReadOnly (true);
    guide.setScrollbarsShown (true);
    guide.setCaretVisible (false);
    guide.setFont (StacksLookAndFeel::font (12.5f));
    guide.setColour (juce::TextEditor::textColourId, colours::text);
    guide.setColour (juce::TextEditor::backgroundColourId, colours::panel);
    guide.setText (guideText(), false);
    addAndMakeVisible (guide);

    saveGuideButton.setButtonText ("Save the guide as a file...");
    saveGuideButton.onClick = [this]
    {
        auto file = ModelManager::appDataDirectory().getChildFile ("Stacks model guide.md");
        file.replaceWithText (guideText());
        file.revealToUser();
    };
    addAndMakeVisible (saveGuideButton);

    processor.labBroadcaster.addChangeListener (this);
    rebuildMenu();
    refresh();
}

SettingsPanel::~SettingsPanel()
{
    processor.labBroadcaster.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void SettingsPanel::show (StacksAudioProcessor& p, juce::Component* centreAround)
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new SettingsPanel (p));
    o.content->setSize (640, 660);
    o.dialogTitle = "Stacks settings";
    o.dialogBackgroundColour = colours::background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.componentToCentreAround = centreAround;
    o.launchAsync();
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void SettingsPanel::resized()
{
    auto r = getLocalBounds().reduced (18);
    title.setBounds (r.removeFromTop (28));
    r.removeFromTop (10);

    engineLabel.setBounds (r.removeFromTop (18));
    auto row = r.removeFromTop (28);
    refreshButton.setBounds (row.removeFromRight (30));
    row.removeFromRight (6);
    engineBox.setBounds (row);
    r.removeFromTop (6);
    statusLabel.setBounds (r.removeFromTop (34));
    auto buttons = r.removeFromTop (26);
    downloadButton.setBounds (buttons.removeFromLeft (110));
    buttons.removeFromLeft (8);
    addModelButton.setBounds (buttons.removeFromLeft (220));
    buttons.removeFromLeft (8);
    removeModelButton.setBounds (buttons.removeFromLeft (140));
    r.removeFromTop (14);

    calmToggle.setBounds (r.removeFromTop (22));
    r.removeFromTop (14);

    guideTitle.setBounds (r.removeFromTop (18));
    saveGuideButton.setBounds (r.removeFromBottom (26).removeFromLeft (200));
    r.removeFromBottom (8);
    guide.setBounds (r);
}

void SettingsPanel::rebuildMenu()
{
    const auto choice = processor.engine();
    engineBox.clear (juce::dontSendNotification);
    menuBuiltins.clear();
    menuOllama.clear();

    engineBox.addSectionHeading ("Built-in (runs inside Stacks, offline)");
    for (const auto& m : processor.builtInModels())
    {
        juce::String label = m.label;
        if (m.id.startsWith ("file:"))  label << "  -  your own model";
        else if (m.fromOllama)          label << "  -  Ollama's copy";
        if (! m.installed)              label << "  -  download " << sizeText (m.bytes);
        menuBuiltins.add (m.id);
        engineBox.addItem (label, kBuiltinItemBase + menuBuiltins.size() - 1);
    }

    engineBox.addSectionHeading ("Ollama app");
    const auto& ollama = processor.ollamaModels();
    if (ollama.isEmpty())
    {
        engineBox.addItem ("Ollama isn't running", kOllamaUnavailableId);
        engineBox.setItemEnabled (kOllamaUnavailableId, false);
    }
    for (const auto& name : ollama)
    {
        menuOllama.add (name);
        engineBox.addItem ("Ollama: " + name, kOllamaItemBase + menuOllama.size() - 1);
    }

    engineBox.addSectionHeading ("No AI");
    engineBox.addItem ("Random breeder only", kRandomItemId);

    int selected = kRandomItemId;
    if (choice.kind == EngineKind::Builtin && menuBuiltins.contains (choice.model)) selected = kBuiltinItemBase + menuBuiltins.indexOf (choice.model);
    if (choice.kind == EngineKind::Ollama && menuOllama.contains (choice.model))    selected = kOllamaItemBase + menuOllama.indexOf (choice.model);
    engineBox.setSelectedId (selected, juce::dontSendNotification);
}

void SettingsPanel::engineChosen()
{
    const int id = engineBox.getSelectedId();
    EngineChoice choice;
    if (id >= kBuiltinItemBase && id - kBuiltinItemBase < menuBuiltins.size())
    {
        choice.kind = EngineKind::Builtin;
        choice.model = menuBuiltins[id - kBuiltinItemBase];
    }
    else if (id >= kOllamaItemBase && id - kOllamaItemBase < menuOllama.size())
    {
        choice.kind = EngineKind::Ollama;
        choice.model = menuOllama[id - kOllamaItemBase];
    }
    if (! (choice == processor.engine()))
        processor.setEngine (choice);   // a missing built-in model downloads itself on the first Generate
    refresh();
}

void SettingsPanel::refresh()
{
    const auto choice = processor.engine();
    calmToggle.setToggleState (processor.calmMode(), juce::dontSendNotification);

    juce::String status;
    bool canDownload = false, custom = false;
    if (choice.kind == EngineKind::Builtin)
    {
        for (const auto& m : processor.builtInModels())
        {
            if (m.id != choice.model) continue;
            custom = m.id.startsWith ("file:");
            if (processor.isDownloading())      status = processor.lab().status;
            else if (m.installed)               status = m.label + "  -  installed" + (m.bytes > 0 ? "  -  " + sizeText (m.bytes) : juce::String()) + (m.note.isNotEmpty() ? "\n" + m.note : juce::String());
            else                               { status = m.label + "  -  not on this Mac yet (" + sizeText (m.bytes) + "). It downloads on the first Generate, or now:"; canDownload = true; }
        }
    }
    else if (choice.kind == EngineKind::Ollama)
        status = "Ollama serves " + choice.model + " over http://127.0.0.1:11434. Keep the Ollama app running.";
    else
        status = "No model: the random breeder designs patches from archetypes. Instant, offline, never surprising.";
    statusLabel.setText (status, juce::dontSendNotification);

    downloadButton.setVisible (canDownload || processor.isDownloading());
    downloadButton.setButtonText (processor.isDownloading() ? "Cancel" : "Download");
    removeModelButton.setVisible (custom);
}

void SettingsPanel::addCustomModel()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a GGUF model file", juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.gguf");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (! file.existsAsFile()) return;
        const auto id = ModelManager::addCustomModel (file);
        processor.refreshModels();
        processor.setEngine ({ EngineKind::Builtin, id });
        rebuildMenu();
        refresh();
    });
}

juce::String SettingsPanel::guideText()
{
    return juce::String::fromUTF8 (R"(STACKS MODEL GUIDE

Stacks designs patches with a language model that runs inside the plug-in.
Nothing leaves your Mac, there is no account and no subscription. The model
is just a file; this guide explains the choices.

THE BUILT-IN MODELS (recommended)

  Qwen3 4B   - the default. About 2.5 GB. On an Apple-silicon Mac with 16 GB
               it writes a patch every 20-30 seconds and understands style
               directions well. If nothing else, use this.
  Qwen3 1.7B - about 1.8 GB and roughly twice as fast, but it follows
               directions less closely and its sounds are plainer. Good on
               8 GB Macs or when you want quick sketches.
  Qwen3 8B   - about 5 GB, the most musical and the best with references
               ("in the style of..."). Needs 16 GB or more and is roughly
               half the speed of the 4B.

The first time you press Generate with a model that isn't on this Mac yet,
Stacks downloads it (from Hugging Face) into
~/Library/Application Support/Stacks/models and then runs your request.
You can also start the download from this window. Models are unloaded after
ten idle minutes to give the memory back; the first batch after a pause
takes a few seconds longer while the model loads.

OLLAMA'S COPIES

If you use the Ollama app, Stacks lists the Qwen3 models it has already
pulled as "Ollama's copy" and loads those files directly, so there is no
second download. Selecting "Ollama: <model>" instead sends the work to the
Ollama app over the local network port - handy for trying other models
Ollama offers, but the app has to be running.

YOUR OWN MODEL

"Add your own model (.gguf)..." accepts any chat model in GGUF format that
llama.cpp can load (Qwen, Llama 3.x, Mistral, Gemma, Phi and so on).
What matters:

  - Pick an *instruct* / *chat* variant, not a base model.
  - Quantised files work well: Q4_K_M is the usual sweet spot. Expect about
    0.6 GB of RAM per billion parameters at Q4, plus a little for context.
  - The model needs a chat template inside the file (almost all do).
    Stacks adds an empty "thinking" block for models that reason, so Qwen3
    and similar answer directly.
  - Output is grammar-constrained, so any model produces valid patches; the
    difference between models is musical judgement and how well they follow
    your direction.
  - The context window must hold about 6,000 tokens. Stacks asks for 12k.

Stacks remembers the file's location; "Forget this model" only removes it
from the list.

WHAT THE ENGINE ACTUALLY DOES

Each batch sends the model a description of the synth, your direction (and
the current sound when you Evolve), and asks for patches as JSON. A grammar
pins the exact shape: a core of 20 settings, two modulation connections
whose targets suit their sources, a designed wavetable, and any extras. A
tuning guard then keeps everything in key. With a style direction the model
first writes itself a short "sound brief" and designs from that.

TROUBLESHOOTING

  - "AI unavailable ... using Random": the chosen model file is missing or
    the Ollama app isn't running. Check this window.
  - Slow batches: close other heavy apps; try the 1.7B; turn off "Design
    wavetables" in the Lab for about a third less work per patch.
  - Logs and the last prompt/reply live in
    ~/Library/Application Support/Stacks (llama.log, last-ai-prompt.txt,
    last-ai-reply.txt) - useful when reporting a problem.
)");
}

} // namespace stacks
