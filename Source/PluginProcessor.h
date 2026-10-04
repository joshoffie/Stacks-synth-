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
    struct Generation { std::vector<Patch> candidates; Patch seed; bool seedIsPatch = false; int generation = 0; };
    std::vector<Generation> history;            // earlier batches, for "back"
    Patch seed;                                 // what this generation grew from (the Garden's centre)
    bool seedIsPatch = false;                   // a real sound you can hear again, not just a "fresh ideas" label
    int auditioned = -1;                        // index into candidates currently loaded
    bool generating = false;
    juce::String status;
    float progress = -1.0f;                     // 0..1 toward the next AI patch, -1 = unknown/idle
    juce::String explanation, modelExplanation; // quick tips, and the model's streamed explanation
    juce::String explanationKey;                // which sound they describe
    bool explaining = false;
    juce::String progressDetail;                // what the model is doing right now
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
    void requestEvolveFrom (const Patch& parent, const juce::String& hint, float variation); // "plant" one candidate
    void cancelGeneration();
    void goBackGeneration();
    void audition (int candidateIndex);
    void auditionSeed();                                   // hear the parent of this generation again
    void auditionFromTree (int generation, int candidate); // any node of the family tree (candidate -1 = that generation's seed)

    // Dragging a leaf: hear a blend between the seed (t = 0) and that candidate (t = 1), or
    // an exaggeration past it. commit keeps the blend as the candidate itself.
    void morphCandidate (int index, float t);
    void commitMorph (int index, float t);

    // Why it sounds like this: quick tips at once, then the model's explanation streams in.
    void explainCurrentPatch();
    juce::String currentExplanationKey() const;

    // Tags on the playing sound and on saved files
    const juce::StringArray& currentTags() const           { return patchTags; }
    void setCurrentTags (const juce::StringArray&);
    void setFileTags (const juce::File&, const juce::StringArray&);

    // Library: your saved patches, in your folders. The heart saves a sound
    // into the open folder (if it isn't saved yet) and marks it a favourite.
    static juce::File libraryRoot();
    static juce::File historyRoot();                       // auto-saved generations, outside the library
    juce::File libraryFolder() const                       { return currentFolder; }
    void setLibraryFolder (const juce::File&);
    juce::File savePatchToLibrary (Patch&, const juce::File& folder); // sets patch.filePath, returns the file
    juce::File savePreset (const juce::String& name, const juce::File& folder, bool asNewFile = false); // the playing sound, exactly as it is
    bool currentIsEdited() const;                          // knobs differ from the loaded preset file
    std::vector<juce::File> libraryFolders() const;        // root first, then every subfolder
    void toggleFavourite (int candidateIndex);             // flip the heart on a candidate
    void favouriteCurrent();                               // flip the heart on the playing sound
    bool currentIsFavourite() const                        { return patchFavourite; }
    void toggleFavouriteFile (const juce::File&);          // a saved patch: flip the heart in its file
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
    bool startModelDownload (const ModelInfo&);           // any GGUF, e.g. one you pasted a URL for
    void cancelDownload();
    bool isDownloading() const                             { return downloader != nullptr && downloader->isRunning(); }
    bool isBusy() const                                    { return labState.generating || isDownloading(); }
    std::shared_ptr<LlmBackend> aiBackend() const          { return activeBackend; }   // null with the random engine

    // Calm mode: no live knob markers or LFO playhead. Persisted.
    bool calmMode() const                                  { return calm; }
    void setCalmMode (bool);

    // Let the generators invent new wavetables ("Custom") instead of only picking built-ins. Persisted.
    bool designWavetables() const                          { return designWaves; }
    void setDesignWavetables (bool);
    const WaveSpec& customWave (int osc) const             { return customSpecs[osc & 1]; }

    static constexpr int kBatchSize = 10;
    static constexpr int kAiPatchesPerBatch = 5;           // the rest are instant Random variations

    // Modulation connections (message thread). Returns the slot used, or -1.
    int addModulation (int source, int target, float amount = 0.3f);
    void clearModulation (int slot);
    int findModulation (int source, int target) const;
    struct Modulation { int slot, source; float amount; };
    // The parameter's value as the effects see it this block: base plus the
    // free-running sources (LFOs, wheel, aftertouch). Drives the live markers.
    float liveValue (int paramIndex) const                  { return liveValues[(size_t) juce::jlimit (0, kNumParams - 1, paramIndex)].load (std::memory_order_relaxed); }
    std::vector<int> modulationsFor (int source) const;      // slots using this source
    std::vector<Modulation> modulationsOnParam (int paramIndex) const;
    void setModulationAmount (int slot, float amount);       // from a ring drag

    // Imported wavetables ("User 1-4")
    static juce::File wavetablesDirectory();
    const UserWavetables& userWavetables() const           { return userWaves; }
    const WavetableBank& builtInWavetables() const         { return *bank; }
    juce::String userWaveName (int slot) const             { return userWaves.name (slot); }
    bool importWavetable (const juce::File&, int slot, juce::String& error); // copies into the folder, loads, names the slot
    int firstFreeUserSlot() const;

    // The last few thousand output samples (mono), for the scope. Any thread may read.
    void copyRecentOutput (float* dest, int count) const;
    void copyRecentDry (float* dest, int count) const;    // the synth before the effects, for the tuner
    int lastPlayedNote() const;                          // MIDI note of the most recent key, -1 if none yet

    // Puts the playing patch in tune without changing its character; returns what changed.
    // `measuredCents` is what the tuner hears off right now (0 if unknown).
    juce::String autoTune (float measuredCents);

    // Drawn LFO shapes and the tables the UI can display
    LfoPoints lfoPoints (int k) const;
    void setLfoPoints (int k, const LfoPoints&);
    const LfoTable& lfoTable (int k) const                  { return lfoTables.get (k); }
    float lfoDisplayPhase (int k) const                     { return lfoPhaseForDisplay[k].load(); }

private:
    void startGeneration (GenerationRequest);
    void autoSaveGeneration();                             // every finished batch lands in Library/Generations
    void addCandidate (int token, Patch);
    void setStatus (int token, const juce::String&);
    void setProgress (int token, float fraction, const juce::String& detail);
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
    void reloadCustomWaves();                              // builds the patch's designed tables from the state tree
    void attachStateListeners();
    void applyGlobalModulation();                          // modulated copy of the params for the effects
    juce::String labToJson() const;
    void labFromJson (const juce::String&);
    void processEffects (juce::AudioBuffer<float>&);

    // Synth engine
    juce::SharedResourcePointer<WavetableBank> bank;      // built once, shared by all instances
    UserWavetables userWaves;                             // per instance, named in the state tree
    WaveSpec customSpecs[2];                              // what the Custom slots were built from
    bool designWaves = true;
    SynthParams params;                                   // base values this block
    SynthParams fxParams;                                 // base + global modulation, read by the effects
    std::array<std::atomic<float>, kNumParams> liveValues {};
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
    static constexpr int kScopeSize = 8192;
    std::array<float, kScopeSize> scopeRing {};
    std::atomic<int> scopeWrite { 0 };
    std::array<float, kScopeSize> dryRing {};
    std::atomic<int> dryWrite { 0 };
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
    std::shared_ptr<LlmBackend> activeBackend;             // whichever model the engine uses right now
    std::optional<GenerationRequest> pendingRequest;       // runs once a missing model has downloaded
    bool calm = false;
    std::unique_ptr<ModelDownloader> downloader;
    int lastDownloadPercent = -1;
    juce::ThreadPool pool { 2 };
    LabState labState;
    juce::File currentFolder;
    juce::String patchName { "Init" };
    juce::String patchCategory, patchOrigin, patchFile;    // of the loaded patch, for the Now Playing card
    juce::StringArray patchTags;
    std::atomic<int> explainToken { 0 };
    bool patchFavourite = false;
    Patch loadedSnapshot;                                  // values as loaded, to spot edits
    void writeFavouriteFlag (const juce::File&, bool);
    std::atomic<int> generationToken { 0 };                // bumps per request; stale callbacks are ignored (read from pool threads)
    std::atomic<bool> cancelRequested { false };

    JUCE_DECLARE_WEAK_REFERENCEABLE (StacksAudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StacksAudioProcessor)
};

} // namespace stacks
