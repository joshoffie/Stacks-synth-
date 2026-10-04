#pragma once

#include <JuceHeader.h>

#include "Parameters.h"
#include "Wavetable.h"
#include "SynthVoice.h"
#include "Patch.h"
#include "LfoTable.h"
#include "PatchGenerator.h"
#include "Effects.h"
#include "ai/ModelManager.h"
#include "ai/LlamaBackend.h"

namespace stacks
{

// Everything the AI Lab panel shows. Message thread only.
struct LabState
{
    int generation = 0;
    std::vector<Patch> candidates;              // the current batch
    std::vector<Patch> favourites;              // what the next evolution breeds from
    std::vector<std::vector<Patch>> history;    // earlier batches, for "back"
    int auditioned = -1;                        // index into candidates currently loaded
    bool generating = false;
    juce::String status;
};

enum class EngineKind { Random, Ollama, Builtin };

struct EngineChoice
{
    EngineKind kind = EngineKind::Random;
    juce::String model;                         // Ollama model name ("qwen3:8b") or built-in model id ("qwen3-4b")
    bool operator== (const EngineChoice& o) const { return kind == o.kind && model == o.model; }
};

class StacksAudioProcessor : public juce::AudioProcessor,
                             private juce::Timer,
                             private juce::AudioProcessorValueTreeState::Listener,
                             private juce::ValueTree::Listener,
                             private juce::AsyncUpdater
{
public:
    StacksAudioProcessor();
    ~StacksAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    const juce::String getName() const override            { return JucePlugin_Name; }
    bool acceptsMidi() const override                      { return true; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 6.0; }

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    //==========================================================================
    // Lab API — call from the message thread. Changes are announced through
    // labBroadcaster so the editor can repaint.
    const LabState& lab() const                            { return labState; }
    juce::ChangeBroadcaster labBroadcaster;

    void requestNewBatch (const juce::String& hint, float variation);
    void requestEvolve (const juce::String& hint, float variation);
    void cancelGeneration();
    void goBackGeneration();
    void audition (int candidateIndex);

    // Library: the heart saves a patch into the current folder and adds it to
    // the breeding set ("favourites"); the set is what Evolve breeds from.
    static juce::File libraryRoot();
    juce::File libraryFolder() const                       { return currentFolder; }
    void setLibraryFolder (const juce::File&);
    juce::File savePatchToLibrary (Patch&, const juce::File& folder); // sets patch.filePath, returns the file
    void toggleFavourite (int candidateIndex);             // save (if needed) + toggle breeding membership
    void favouriteCurrent();                               // the playing sound: save + toggle
    void toggleFavouriteFile (const juce::File&);          // from the library browser
    void removeFavourite (int favouriteIndex);
    int indexOfFavourite (const Patch&) const;
    int indexOfFavouriteFile (const juce::File&) const;
    bool loadLibraryPatch (const juce::File&);             // audition a saved patch

    Patch currentPatch() const;                            // what is loaded right now, including knob tweaks
    void applyPatch (const Patch&);
    const juce::String& currentPatchName() const           { return patchName; }
    void setCurrentPatchName (const juce::String& name);

    // Engine selection (persisted in the user's settings, shared by all instances)
    EngineChoice engine() const                            { return engineChoice; }
    void setEngine (const EngineChoice&);
    juce::String engineName() const;
    const juce::StringArray& ollamaModels() const          { return knownOllamaModels; }
    void refreshOllamaModels();                            // async; broadcasts when done

    // Built-in models (run inside the plug-in with llama.cpp)
    const std::vector<ModelInfo>& builtInModels() const    { return knownModels; }
    void refreshModels();
    bool startModelDownload (const juce::String& modelId); // false if one is already running
    void cancelDownload();
    bool isDownloading() const                             { return downloader != nullptr && downloader->isRunning(); }
    bool isBusy() const                                    { return labState.generating || isDownloading(); }

    static constexpr int kBatchSize = 10;
    static constexpr int kAiPatchesPerBatch = 5;           // the rest are instant Random variations

    // Modulation connections (message thread). Returns the slot used, or -1.
    int addModulation (int source, int target, float amount = 0.3f);
    void clearModulation (int slot);
    int findModulation (int source, int target) const;
    std::vector<int> modulationsFor (int source) const;      // slots using this source
    std::vector<std::pair<int, float>> modulationsOnParam (int paramIndex) const; // (source, amount)

    // Imported wavetables ("User 1-4")
    static juce::File wavetablesDirectory();
    const UserWavetables& userWavetables() const           { return userWaves; }
    const WavetableBank& builtInWavetables() const         { return *bank; }
    juce::String userWaveName (int slot) const             { return userWaves.name (slot); }
    bool importWavetable (const juce::File&, int slot, juce::String& error); // copies into the folder, loads, names the slot
    int firstFreeUserSlot() const;

    // Drawn LFO shapes and the tables the UI can display
    LfoPoints lfoPoints (int k) const;
    void setLfoPoints (int k, const LfoPoints&);
    const LfoTable& lfoTable (int k) const                  { return lfoTables.get (k); }
    float lfoDisplayPhase (int k) const                     { return lfoPhaseForDisplay[k].load(); }

private:
    void startGeneration (GenerationRequest);
    void addCandidate (int token, Patch);
    void setStatus (int token, const juce::String&);
    void finishGeneration (int token);
    void loadEngineFromSettings();
    void rebuildGenerator();
    void timerCallback() override;                         // frees an idle built-in model
    void parameterChanged (const juce::String&, float) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void handleAsyncUpdate() override;                     // rebuilds LFO tables on the message thread
    void rebuildLfoTables();
    void reloadUserWaves();                                // loads whatever the state tree names
    void attachStateListeners();
    void applyGlobalModulation();                          // modulated copy of the params for the effects
    juce::String labToJson() const;
    void labFromJson (const juce::String&);
    void processEffects (juce::AudioBuffer<float>&);

    // Synth engine
    juce::SharedResourcePointer<WavetableBank> bank;      // built once, shared by all instances
    UserWavetables userWaves;                             // per instance, named in the state tree
    SynthParams params;                                   // base values this block
    SynthParams fxParams;                                 // base + global modulation, read by the effects
    VoiceContext voiceContext;
    LfoTableBank lfoTables;
    float lfoPhase[kNumLfos] {}, lfoHeld[kNumLfos] {};
    std::atomic<float> lfoPhaseForDisplay[kNumLfos] {};
    juce::Random lfoRng;
    std::array<std::atomic<float>*, kNumParams> rawParams {};
    juce::Synthesiser synth;

    // Master effects
    ChorusFx chorus;
    DelayFx delay;
    ReverbFx reverb;
    juce::AudioBuffer<float> fxBuffer;                    // stereo scratch when the host gives us mono
    juce::SmoothedValue<float> masterGain;
    double currentSampleRate = 44100.0;
    double currentBpm = 120.0;

    // Lab
    std::shared_ptr<PatchGenerator> randomGenerator;
    std::shared_ptr<PatchGenerator> generator;             // what the buttons use (may be the random one)
    EngineChoice engineChoice;
    juce::StringArray knownOllamaModels;
    std::vector<ModelInfo> knownModels;
    std::shared_ptr<LlamaBackend> builtInBackend;          // kept across engine rebuilds so the model stays loaded
    std::unique_ptr<ModelDownloader> downloader;
    int lastDownloadPercent = -1;
    juce::ThreadPool pool { 2 };
    LabState labState;
    juce::File currentFolder;
    juce::String patchName { "Init" };
    juce::String patchCategory, patchOrigin;               // of the loaded patch, for the Now Playing card
    int generationToken = 0;                               // bumps per request; stale callbacks are ignored
    std::atomic<bool> cancelRequested { false };

    JUCE_DECLARE_WEAK_REFERENCEABLE (StacksAudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StacksAudioProcessor)
};

} // namespace stacks
