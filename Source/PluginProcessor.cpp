#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ai/PatchExplainer.h"
#include "ai/LlmPatchGenerator.h"

namespace stacks
{

namespace
{
    constexpr int kNumVoices = 12;

    // Keeps wild AI patches from clipping hard: linear below 0.8, then a
    // gentle tanh knee that never exceeds 1.0.
    inline float softClip (float x) noexcept
    {
        constexpr float t = 0.8f;
        const float a = std::abs (x);
        if (a <= t)
            return x;
        const float y = t + (1.0f - t) * std::tanh ((a - t) / (1.0f - t));
        return x < 0.0f ? -y : y;
    }

    // ~/Library/Application Support/Stacks/Stacks.settings — shared by every instance.
    struct Settings
    {
        Settings()
        {
            juce::PropertiesFile::Options o;
            o.applicationName = "Stacks";
            o.filenameSuffix = "settings";
            o.folderName = "Stacks";
            o.osxLibrarySubFolder = "Application Support";
            o.storageFormat = juce::PropertiesFile::storeAsXML;
            file = std::make_unique<juce::PropertiesFile> (o);
        }
        std::unique_ptr<juce::PropertiesFile> file;
    };

    juce::PropertiesFile& settings()
    {
        static juce::SharedResourcePointer<Settings> shared;
        return *shared->file;
    }

    const juce::String kDot = juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
}

StacksAudioProcessor::StacksAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout()),
      randomGenerator (std::make_shared<RandomPatchGenerator>()),
      generator (randomGenerator)
{
    const auto& specs = paramSpecs();
    for (int i = 0; i < kNumParams; ++i)
    {
        rawParams[(size_t) i] = apvts.getRawParameterValue (specs[(size_t) i].id);
        params.v[i] = rawParams[(size_t) i]->load();
    }

    voiceContext.bank = &*bank;
    voiceContext.user = &userWaves;
    paramRange (0);   // builds the range table now, not on the audio thread
    voiceContext.params = &params;
    voiceContext.lfoTables = &lfoTables;
    voiceContext.liveValues = liveValues.data();

    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new SynthVoice (voiceContext));
    synth.addSound (new SynthSound());
    synth.setNoteStealingEnabled (true);

    for (int k = 0; k < kNumLfos; ++k)
        apvts.addParameterListener (paramId (lfoShapeParam (k)), this);
    attachStateListeners();
    rebuildLfoTables();
    reloadUserWaves();
    reloadCustomWaves();

    refreshModels();
    loadEngineFromSettings();
    refreshOllamaModels();

    currentFolder = juce::File (settings().getValue ("libraryFolder", libraryRoot().getFullPathName()));
    if (! currentFolder.isDirectory() || ! currentFolder.isAChildOf (libraryRoot().getParentDirectory()))
        currentFolder = libraryRoot();
    libraryRoot().createDirectory();

    startTimer (30000);
}

StacksAudioProcessor::~StacksAudioProcessor()
{
    stopTimer();
    cancelPendingUpdate();
    apvts.state.removeListener (this);
    for (int k = 0; k < kNumLfos; ++k)
        apvts.removeParameterListener (paramId (lfoShapeParam (k)), this);
    downloader.reset();
    cancelRequested = true;
    pool.removeAllJobs (true, 30000);   // a model load can't be interrupted; give it time
}

//==============================================================================
void StacksAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);

    chorus.prepare (sampleRate, samplesPerBlock);
    delay.prepare (sampleRate, samplesPerBlock);
    reverb.prepare (sampleRate, samplesPerBlock);
    fxBuffer.setSize (2, juce::jmax (1, samplesPerBlock));

    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (rawParams[(size_t) P::master_gain]->load()));
}

bool StacksAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void StacksAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    for (int i = 0; i < kNumParams; ++i)
        params.v[i] = rawParams[(size_t) i]->load();

    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto bpm = position->getBpm())
                currentBpm = *bpm;
    voiceContext.bpm = currentBpm;

    // Free-running LFOs: phase and value at the start of this block, shared by
    // every voice and by the effects.
    for (int k = 0; k < kNumLfos; ++k)
    {
        const float beats = lfoSyncBeats (params.geti (lfoSyncParam (k)));
        const float rate = beats > 0.0f ? (float) (currentBpm / 60.0) / beats : params.get (lfoRateParam (k));
        const int shape = params.geti (lfoShapeParam (k));

        voiceContext.lfoRateHz[k] = rate;
        voiceContext.lfoBlockPhase[k] = lfoPhase[k];
        voiceContext.lfoGlobalValue[k] = shape == ShapeRandom ? lfoHeld[k]
                                                              : lfoTables.get (k).at (lfoPhase[k] + params.get (lfoPhaseParam (k)));
        lfoPhaseForDisplay[k].store (lfoPhase[k]);
    }

    // Global modulation first so the markers fall back to it when no note is
    // sounding; the newest voice then overwrites the live values while it plays.
    applyGlobalModulation();

    // Advance the phases by the *modulated* rate (fxParams), so a connection to
    // an LFO's Rate knob actually speeds it up or slows it down.
    for (int k = 0; k < kNumLfos; ++k)
    {
        const float beats = lfoSyncBeats (params.geti (lfoSyncParam (k)));
        const float rate = beats > 0.0f ? (float) (currentBpm / 60.0) / beats : fxParams.get (lfoRateParam (k));
        voiceContext.lfoRateHz[k] = rate;
        lfoPhase[k] += rate * (float) numSamples / (float) currentSampleRate;
        if (lfoPhase[k] >= 1.0f)
        {
            lfoPhase[k] -= std::floor (lfoPhase[k]);
            lfoHeld[k] = lfoRng.nextFloat() * 2.0f - 1.0f;
        }
    }

    synth.renderNextBlock (buffer, midi, 0, numSamples);
    processEffects (buffer);

    // Feed the scope: a mono mix, written without locks (the display tolerates a torn float).
    {
        const int n = buffer.getNumSamples();
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : l;
        int w = scopeWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            scopeRing[(size_t) w] = 0.5f * (l[i] + r[i]);
            w = (w + 1) % kScopeSize;
        }
        scopeWrite.store (w, std::memory_order_release);
    }
}

void StacksAudioProcessor::copyRecentOutput (float* dest, int count) const
{
    count = juce::jlimit (0, kScopeSize, count);
    int start = scopeWrite.load (std::memory_order_acquire) - count;
    if (start < 0) start += kScopeSize;
    for (int i = 0; i < count; ++i)
        dest[i] = scopeRing[(size_t) ((start + i) % kScopeSize)];
}

// Connections whose target is a knob outside the voices (effects, master) are
// applied here, from the sources that exist globally.
void StacksAudioProcessor::applyGlobalModulation()
{
    fxParams = params;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int src = params.geti (modSourceParam (i));
        const int target = params.geti (modDestParam (i));
        const float amount = params.get (modAmountParam (i));
        const int paramIndex = modTargetParamIndex (target);
        if (src == SrcOff || paramIndex < 0 || std::abs (amount) < 1.0e-4f)
            continue;

        float s = 0.0f;
        switch (src)
        {
            case SrcLfo1: case SrcLfo2: case SrcLfo3: case SrcLfo4: s = voiceContext.lfoGlobalValue[src - SrcLfo1]; break;
            case SrcModWheel:   s = voiceContext.modWheel.load(); break;
            case SrcAftertouch: s = voiceContext.aftertouch.load(); break;
            default: continue; // per-note sources have no meaning for a global knob
        }
        nudgeParam (fxParams, paramIndex, amount * s);
    }
    for (int i = 0; i < kNumParams; ++i)
        liveValues[(size_t) i].store (fxParams.v[i], std::memory_order_relaxed);
}

void StacksAudioProcessor::processEffects (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() == 0 || n == 0)
        return;

    // The effects are stereo; a mono host gets a mono fold-down of the result.
    const bool mono = buffer.getNumChannels() < 2;
    juce::AudioBuffer<float>* target = &buffer;
    if (mono)
    {
        fxBuffer.setSize (2, n, false, false, true);
        fxBuffer.copyFrom (0, 0, buffer, 0, 0, n);
        fxBuffer.copyFrom (1, 0, buffer, 0, 0, n);
        target = &fxBuffer;
    }

    ChorusFx::Params cp;
    cp.mode = fxParams.geti (P::chorus_mode);
    cp.rate = fxParams.get (P::chorus_rate);
    cp.depth = fxParams.get (P::chorus_depth);
    cp.mix = fxParams.get (P::chorus_mix);
    cp.voices = fxParams.geti (P::chorus_voices);
    cp.feedback = fxParams.get (P::chorus_feedback);
    cp.spread = fxParams.get (P::chorus_spread);
    cp.toneHz = fxParams.get (P::chorus_tone);
    if (cp.mix > 0.0005f)
        chorus.process (*target, cp);

    DelayFx::Params dp;
    dp.mode = fxParams.geti (P::delay_mode);
    dp.sync = fxParams.geti (P::delay_sync);
    dp.timeSec = fxParams.get (P::delay_time);
    dp.feedback = fxParams.get (P::delay_feedback);
    dp.mix = fxParams.get (P::delay_mix);
    dp.toneHz = fxParams.get (P::delay_tone);
    dp.hpfHz = fxParams.get (P::delay_hpf);
    dp.wow = fxParams.get (P::delay_wow);
    dp.width = fxParams.get (P::delay_width);
    delay.process (*target, dp, currentBpm); // always runs so repeats ring out when the mix is pulled down

    ReverbFx::Params rp;
    rp.type = fxParams.geti (P::reverb_type);
    rp.size = fxParams.get (P::reverb_size);
    rp.damp = fxParams.get (P::reverb_damp);
    rp.mix = fxParams.get (P::reverb_mix);
    rp.predelayMs = fxParams.get (P::reverb_predelay);
    rp.lowCutHz = fxParams.get (P::reverb_lowcut);
    rp.highCutHz = fxParams.get (P::reverb_highcut);
    rp.mod = fxParams.get (P::reverb_mod);
    rp.shimmer = fxParams.get (P::reverb_shimmer);
    rp.width = fxParams.get (P::reverb_width);
    if (rp.mix > 0.0005f)
        reverb.process (*target, rp);

    if (mono)
    {
        buffer.copyFrom (0, 0, fxBuffer.getReadPointer (0), n, 0.5f);
        buffer.addFrom (0, 0, fxBuffer, 1, 0, n, 0.5f);
    }

    // Master gain + safety clip
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (fxParams.get (P::master_gain)));
    for (int i = 0; i < n; ++i)
    {
        const float g = masterGain.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* x = buffer.getWritePointer (ch);
            x[i] = softClip (x[i] * g);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* StacksAudioProcessor::createEditor()
{
    return new StacksAudioProcessorEditor (*this);
}

void StacksAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("StacksState");
    root.appendChild (apvts.copyState(), nullptr);

    juce::ValueTree lab ("Lab");
    lab.setProperty ("json", labToJson(), nullptr);
    root.appendChild (lab, nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void StacksAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.isValid())
        return;

    if (root.hasType ("StacksState"))
    {
        auto state = root.getChildWithName (apvts.state.getType());
        if (state.isValid())
        {
            apvts.replaceState (state);
            attachStateListeners();
            rebuildLfoTables();
            reloadUserWaves();
            reloadCustomWaves();
        }

        auto lab = root.getChildWithName ("Lab");
        if (lab.isValid())
            labFromJson (lab["json"].toString());
        loadedSnapshot = currentPatch();   // what was restored counts as unedited
    }
    else if (root.hasType (apvts.state.getType()))
    {
        apvts.replaceState (root);
        attachStateListeners();
        rebuildLfoTables();
        reloadUserWaves();
        reloadCustomWaves();
    }

    labBroadcaster.sendChangeMessage();
}

//==============================================================================
void StacksAudioProcessor::attachStateListeners()
{
    apvts.state.removeListener (this);
    apvts.state.getOrCreateChildWithName (Patch::lfoShapesTreeType(), nullptr);
    apvts.state.getOrCreateChildWithName (Patch::userWavesTreeType(), nullptr);
    apvts.state.getOrCreateChildWithName (Patch::customWavesTreeType(), nullptr);
    apvts.state.addListener (this);
}

void StacksAudioProcessor::parameterChanged (const juce::String&, float)     { triggerAsyncUpdate(); }
void StacksAudioProcessor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&)
{
    if (tree.hasType (Patch::lfoShapesTreeType()) || tree.hasType (Patch::userWavesTreeType()) || tree.hasType (Patch::customWavesTreeType()))
        triggerAsyncUpdate();
}
void StacksAudioProcessor::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child)
{
    if (child.hasType (Patch::lfoShapesTreeType()) || child.hasType (Patch::userWavesTreeType()) || child.hasType (Patch::customWavesTreeType()))
        triggerAsyncUpdate();
}
void StacksAudioProcessor::handleAsyncUpdate()
{
    rebuildLfoTables();
    reloadUserWaves();
    reloadCustomWaves();
}

void StacksAudioProcessor::rebuildLfoTables()
{
    for (int k = 0; k < kNumLfos; ++k)
        lfoTables.set (k, (int) rawParams[(size_t) lfoShapeParam (k)]->load(), lfoPoints (k));
    labBroadcaster.sendChangeMessage();
}

LfoPoints StacksAudioProcessor::lfoPoints (int k) const
{
    auto shapes = apvts.state.getChildWithName (Patch::lfoShapesTreeType());
    if (! shapes.isValid())
        return {};
    return LfoPoints::fromJson (shapes.getProperty (Patch::lfoShapeProperty (k)).toString());
}

void StacksAudioProcessor::setLfoPoints (int k, const LfoPoints& points)
{
    auto shapes = apvts.state.getOrCreateChildWithName (Patch::lfoShapesTreeType(), nullptr);
    if (points.points.empty())
        shapes.removeProperty (Patch::lfoShapeProperty (k), nullptr);
    else
        shapes.setProperty (Patch::lfoShapeProperty (k), points.toJson(), nullptr);

    // Drawing implies the Custom shape.
    if (auto* shape = apvts.getParameter (paramId (lfoShapeParam (k))))
        if ((int) rawParams[(size_t) lfoShapeParam (k)]->load() != ShapeCustom && ! points.points.empty())
            shape->setValueNotifyingHost (shape->convertTo0to1 ((float) ShapeCustom));
    rebuildLfoTables();
}

//==============================================================================
int StacksAudioProcessor::findModulation (int source, int target) const
{
    for (int i = 0; i < kNumModSlots; ++i)
        if ((int) rawParams[(size_t) modSourceParam (i)]->load() == source
            && (int) rawParams[(size_t) modDestParam (i)]->load() == target)
            return i;
    return -1;
}

int StacksAudioProcessor::addModulation (int source, int target, float amount)
{
    if (source == SrcOff || target == TargetOff)
        return -1;

    int slot = findModulation (source, target);
    if (slot < 0)
        for (int i = 0; i < kNumModSlots && slot < 0; ++i)
            if ((int) rawParams[(size_t) modSourceParam (i)]->load() == SrcOff
                || (int) rawParams[(size_t) modDestParam (i)]->load() == TargetOff)
                slot = i;
    if (slot < 0)
        return -1;

    auto set = [this] (P param, float value)
    {
        if (auto* p = apvts.getParameter (paramId (param)))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    set (modSourceParam (slot), (float) source);
    set (modDestParam (slot), (float) target);
    set (modAmountParam (slot), amount);
    return slot;
}

void StacksAudioProcessor::clearModulation (int slot)
{
    if (slot < 0 || slot >= kNumModSlots)
        return;
    auto set = [this] (P param, float value)
    {
        if (auto* p = apvts.getParameter (paramId (param)))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    set (modSourceParam (slot), (float) SrcOff);
    set (modDestParam (slot), (float) TargetOff);
    set (modAmountParam (slot), 0.0f);
}

std::vector<int> StacksAudioProcessor::modulationsFor (int source) const
{
    std::vector<int> slots;
    for (int i = 0; i < kNumModSlots; ++i)
        if ((int) rawParams[(size_t) modSourceParam (i)]->load() == source
            && (int) rawParams[(size_t) modDestParam (i)]->load() != TargetOff)
            slots.push_back (i);
    return slots;
}

std::vector<StacksAudioProcessor::Modulation> StacksAudioProcessor::modulationsOnParam (int paramIndex) const
{
    std::vector<Modulation> out;
    const int target = modTargetForParam (paramIndex);
    if (target <= 0)
        return out;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int src = (int) rawParams[(size_t) modSourceParam (i)]->load();
        if (src != SrcOff && (int) rawParams[(size_t) modDestParam (i)]->load() == target)
            out.push_back ({ i, src, rawParams[(size_t) modAmountParam (i)]->load() });
    }
    return out;
}

void StacksAudioProcessor::setModulationAmount (int slot, float amount)
{
    if (slot < 0 || slot >= kNumModSlots)
        return;
    if (auto* p = apvts.getParameter (paramId (modAmountParam (slot))))
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (-1.0f, 1.0f, amount)));
}

// Nothing a generation produced is ever lost: Library/Generations/Gen N - HH.MM
void StacksAudioProcessor::autoSaveGeneration()
{
    if (labState.candidates.empty())
        return;
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H.%M");
    auto folder = historyRoot().getChildFile ("Gen " + juce::String (labState.generation) + " - " + stamp);
    folder.createDirectory();
    for (const auto& c : labState.candidates)
    {
        auto file = folder.getNonexistentChildFile (juce::File::createLegalFileName (c.name.isEmpty() ? "Patch" : c.name), ".json", false);
        file.replaceWithText (c.toJson());
    }
}

//==============================================================================
Patch StacksAudioProcessor::currentPatch() const
{
    auto p = Patch::capture (apvts);
    p.name = patchName;
    p.category = patchCategory;
    p.origin = patchOrigin;
    p.filePath = patchFile;
    p.favourite = patchFavourite;
    p.tags = patchTags;
    p.description = describePatch (p);
    return p;
}

void StacksAudioProcessor::applyPatch (const Patch& p)
{
    p.applyTo (apvts);
    patchName = p.name;
    patchCategory = p.category;
    patchOrigin = p.origin;
    patchFile = p.filePath;
    patchFavourite = p.favourite;
    patchTags = p.tags;
    loadedSnapshot = p;
}

bool StacksAudioProcessor::currentIsEdited() const
{
    return ! Patch::capture (apvts).sameValuesAs (loadedSnapshot);
}

void StacksAudioProcessor::setCurrentPatchName (const juce::String& name)
{
    patchName = name.trim().isEmpty() ? juce::String ("Untitled") : name.trim();
}

//==============================================================================
void StacksAudioProcessor::loadEngineFromSettings()
{
    auto& s = settings();
    designWaves = s.getBoolValue ("designWaves", true);
    calm = s.getBoolValue ("calm", false);
    EngineChoice choice;
    const auto kind = s.getValue ("engine", "builtin");   // Qwen3 4B out of the box; it downloads itself on first use
    choice.kind  = kind == "ollama" ? EngineKind::Ollama : kind == "builtin" ? EngineKind::Builtin : EngineKind::Random;
    choice.model = s.getValue ("model", "qwen3-4b");
    if (choice.kind != EngineKind::Random && choice.model.isEmpty())
        choice.kind = EngineKind::Random;
    engineChoice = choice;
    rebuildGenerator();
}

void StacksAudioProcessor::setEngine (const EngineChoice& choice)
{
    engineChoice = choice;
    rebuildGenerator();

    auto& s = settings();
    s.setValue ("engine", choice.kind == EngineKind::Ollama ? "ollama" : choice.kind == EngineKind::Builtin ? "builtin" : "random");
    s.setValue ("model", choice.model);
    s.saveIfNeeded();

    labState.status = "Engine: " + engineName();
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::rebuildGenerator()
{
    generator = randomGenerator;
    activeBackend.reset();

    if (engineChoice.kind == EngineKind::Ollama)
    {
        activeBackend = std::make_shared<OllamaBackend> (engineChoice.model);
        generator = std::make_shared<LlmPatchGenerator> (activeBackend, randomGenerator);
    }
    else if (engineChoice.kind == EngineKind::Builtin)
    {
        for (const auto& m : knownModels)
        {
            if (m.id != engineChoice.model || ! m.installed)
                continue;
            if (builtInBackend == nullptr || builtInBackend->file() != m.file)
                builtInBackend = std::make_shared<LlamaBackend> (m.file, m.label);
            activeBackend = builtInBackend;
            generator = std::make_shared<LlmPatchGenerator> (builtInBackend, randomGenerator);
            break;
        }
    }
}

void StacksAudioProcessor::timerCallback()
{
    if (builtInBackend != nullptr && ! labState.generating)
        builtInBackend->unloadIfIdle (600.0); // ten minutes idle -> give the RAM back
    userWaves.retireOld (5.0);                // replaced tables no voice can still be reading
}

//==============================================================================
juce::File StacksAudioProcessor::wavetablesDirectory()
{
    return ModelManager::appDataDirectory().getChildFile ("wavetables");
}

int StacksAudioProcessor::firstFreeUserSlot() const
{
    for (int i = 0; i < UserWavetables::kUserSlots; ++i)
        if (userWaves.active (i) == nullptr)
            return i;
    return 0;
}

bool StacksAudioProcessor::importWavetable (const juce::File& source, int slot, juce::String& error)
{
    if (! source.existsAsFile())
    {
        error = "file not found";
        return false;
    }
    const auto dir = wavetablesDirectory();
    dir.createDirectory();
    auto target = dir.getChildFile (source.getFileName());
    if (! source.isAChildOf (dir) && (! target.existsAsFile() || target.getSize() != source.getSize()))
        if (! source.copyFileTo (target))
        {
            error = "could not copy the file into the Stacks wavetables folder";
            return false;
        }

    auto table = loadUserTable (target, error);
    if (table == nullptr)
        return false;

    slot = juce::jlimit (0, UserWavetables::kUserSlots - 1, slot);
    userWaves.set (slot, std::move (table));
    auto waves = apvts.state.getOrCreateChildWithName (Patch::userWavesTreeType(), nullptr);
    waves.setProperty (Patch::userWaveProperty (slot), target.getFileName(), nullptr);
    labBroadcaster.sendChangeMessage();
    return true;
}

void StacksAudioProcessor::reloadUserWaves()
{
    auto waves = apvts.state.getChildWithName (Patch::userWavesTreeType());
    if (! waves.isValid())
        return;
    for (int slot = 0; slot < UserWavetables::kUserSlots; ++slot)
    {
        const auto name = waves.getProperty (Patch::userWaveProperty (slot)).toString();
        if (name.isEmpty() || name == userWaves.name (slot))
            continue;
        juce::String error;
        if (auto table = loadUserTable (wavetablesDirectory().getChildFile (name), error))
            userWaves.set (slot, std::move (table));
    }
}

// The "Custom" wave of each oscillator is the designed table stored with the
// patch (as JSON in the state tree). Rebuilt only when the spectrum changes.
void StacksAudioProcessor::reloadCustomWaves()
{
    auto tree = apvts.state.getChildWithName (Patch::customWavesTreeType());
    for (int osc = 0; osc < 2; ++osc)
    {
        WaveSpec spec;
        if (tree.isValid())
            if (auto parsed = WaveSpec::fromJson (tree.getProperty (Patch::customWaveProperty (osc)).toString()))
                spec = *parsed;
        const int slot = UserWavetables::customSlot (osc);
        if (spec == customSpecs[osc] && (spec.isEmpty() == (userWaves.active (slot) == nullptr)))
            continue;
        customSpecs[osc] = spec;
        userWaves.set (slot, spec.isEmpty() ? nullptr : buildSpectralTable (spec));
    }
}

void StacksAudioProcessor::setCalmMode (bool shouldBeCalm)
{
    calm = shouldBeCalm;
    auto& s = settings();
    s.setValue ("calm", shouldBeCalm);
    s.saveIfNeeded();
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::setDesignWavetables (bool shouldDesign)
{
    designWaves = shouldDesign;
    auto& s = settings();
    s.setValue ("designWaves", shouldDesign);
    s.saveIfNeeded();
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::refreshModels()
{
    knownModels = ModelManager::catalogue();
}

bool StacksAudioProcessor::startModelDownload (const juce::String& modelId)
{
    if (isDownloading())
        return false;

    auto info = ModelManager::find (modelId);
    if (! info || info->installed)
        return false;

    downloader = std::make_unique<ModelDownloader>();
    lastDownloadPercent = -1;
    const auto label = info->label;

    downloader->onProgress = [this, label] (juce::int64 done, juce::int64 total)
    {
        const int percent = total > 0 ? (int) (done * 100 / total) : 0;
        if (percent == lastDownloadPercent)
            return;
        lastDownloadPercent = percent;
        labState.status = "Downloading " + label + "... " + juce::String (percent) + "%  ("
                        + juce::String (done / 1000000) + " of " + juce::String (total / 1000000) + " MB)";
        labBroadcaster.sendChangeMessage();
    };
    downloader->onFinished = [this, label, modelId] (bool ok, const juce::String& error)
    {
        refreshModels();
        if (ok)
        {
            setEngine ({ EngineKind::Builtin, modelId });
            labState.status = label + " installed and selected";
            if (pendingRequest)
            {
                auto request = *pendingRequest;
                pendingRequest.reset();
                labBroadcaster.sendChangeMessage();
                startGeneration (std::move (request));
                return;
            }
        }
        else
        {
            labState.status = "Download of " + label + " failed: " + error;
        }
        labBroadcaster.sendChangeMessage();
    };

    juce::String error;
    if (! downloader->start (*info, error))
    {
        labState.status = "Could not download " + label + ": " + error;
        downloader.reset();
        labBroadcaster.sendChangeMessage();
        return false;
    }

    labState.status = "Downloading " + label + "...";
    labBroadcaster.sendChangeMessage();
    return true;
}

void StacksAudioProcessor::cancelDownload()
{
    if (! isDownloading())
        return;
    const auto label = downloader->model().label;
    downloader.reset();
    labState.status = "Download of " + label + " cancelled";
    labBroadcaster.sendChangeMessage();
}

juce::String StacksAudioProcessor::engineName() const
{
    return generator->name();
}

void StacksAudioProcessor::refreshOllamaModels()
{
    refreshModels();
    juce::WeakReference<StacksAudioProcessor> weak (this);
    pool.addJob ([weak]
    {
        auto models = OllamaBackend::listModels();
        juce::MessageManager::callAsync ([weak, models]
        {
            if (auto* self = weak.get())
            {
                self->knownOllamaModels = models;
                self->labBroadcaster.sendChangeMessage();
            }
        });
    });
}

//==============================================================================
void StacksAudioProcessor::requestNewBatch (const juce::String& hint, float variation)
{
    GenerationRequest req;
    req.hint = hint;
    req.variation = variation;
    req.count = kBatchSize;
    req.generation = labState.generation + 1;
    startGeneration (std::move (req));
}

void StacksAudioProcessor::requestEvolve (const juce::String& hint, float variation)
{
    GenerationRequest req;
    req.hint = hint;
    req.variation = variation;
    req.count = kBatchSize;
    req.generation = labState.generation + 1;
    req.parents = { currentPatch() }; // Evolve grows from the sound you're hearing
    startGeneration (std::move (req));
}

void StacksAudioProcessor::requestEvolveFrom (const Patch& parent, const juce::String& hint, float variation)
{
    GenerationRequest req;
    req.hint = hint;
    req.variation = variation;
    req.count = kBatchSize;
    req.generation = labState.generation + 1;
    req.parents = { parent };
    if (labState.generating)
        return;
    applyPatch (parent); // the planted leaf becomes the seed you hear
    startGeneration (std::move (req));
}

void StacksAudioProcessor::cancelGeneration()
{
    if (labState.generating)
        cancelRequested = true;
}

void StacksAudioProcessor::startGeneration (GenerationRequest req)
{
    if (labState.generating)
        return;

    // The chosen built-in model isn't on this Mac yet: fetch it, then run this request.
    if (engineChoice.kind == EngineKind::Builtin && generator == randomGenerator)
    {
        if (auto info = ModelManager::find (engineChoice.model); info && ! info->installed && info->url.isNotEmpty())
        {
            pendingRequest = req;
            if (isDownloading())
                labState.status = "Still downloading " + info->label + " - your request runs when it lands";
            else if (startModelDownload (engineChoice.model))
                labState.status = "Downloading " + info->label + " first (" + juce::String (info->bytes / 1000000) + " MB) - your request runs when it lands";
            labBroadcaster.sendChangeMessage();
            return;
        }
    }

    const int token = ++generationToken;
    cancelRequested = false;

    // The new batch replaces the old list right away so streamed results have
    // somewhere to land. The sound that is loaded stays exactly as it is.
    if (! labState.candidates.empty())
    {
        labState.history.push_back ({ std::move (labState.candidates), labState.seed, labState.seedIsPatch, labState.generation });
        if (labState.history.size() > 20)
            labState.history.erase (labState.history.begin());
    }
    labState.candidates.clear();
    labState.auditioned = -1;
    labState.generation = req.generation;
    req.designWaves = designWaves;

    // The Garden's seed is what this batch grows from. Loading other sounds
    // later (from the library, say) leaves the garden exactly as it is.
    if (! req.parents.empty())
    {
        labState.seed = req.parents.front();
        labState.seedIsPatch = true;
    }
    else
    {
        labState.seed = Patch();
        labState.seed.name = req.hint.trim().isNotEmpty() ? "\"" + req.hint.trim() + "\"" : juce::String ("Fresh ideas");
        labState.seedIsPatch = false;
    }
    labState.generating = true;
    labState.status = "Generating with " + engineName() + "...";

    const bool usingAi = generator != randomGenerator;
    labState.progress = -1.0f;
    labState.progressDetail = usingAi ? "Starting " + engineName() + "..." : juce::String();
    juce::WeakReference<StacksAudioProcessor> weak (this);

    auto makeProgress = [weak, token, this]
    {
        GenerationProgress progress;
        progress.onPatch = [weak, token] (const Patch& p)
        {
            juce::MessageManager::callAsync ([weak, token, p]
            {
                if (auto* self = weak.get())
                    self->addCandidate (token, p);
            });
        };
        progress.onStatus = [weak, token] (const juce::String& s)
        {
            juce::MessageManager::callAsync ([weak, token, s]
            {
                if (auto* self = weak.get())
                    self->setStatus (token, s);
            });
        };
        progress.onProgress = [weak, token] (float fraction, const juce::String& detail)
        {
            juce::MessageManager::callAsync ([weak, token, fraction, detail]
            {
                if (auto* self = weak.get())
                    self->setProgress (token, fraction, detail);
            });
        };
        progress.shouldCancel = [this, token] { return cancelRequested.load() || generationToken != token; };
        return progress;
    };

    if (usingAi)
    {
        // Instant half: variations from the breeder, so there is something to play immediately.
        GenerationRequest quick = req;
        quick.count = kBatchSize - kAiPatchesPerBatch;
        for (auto& p : randomGenerator->generate (quick, {}))
            labState.candidates.push_back (std::move (p));

        GenerationRequest aiReq = req;
        aiReq.count = kAiPatchesPerBatch;
        auto gen = generator;
        auto progress = makeProgress();
        pool.addJob ([gen, aiReq, progress, weak, token]
        {
            gen->generate (aiReq, progress);
            juce::MessageManager::callAsync ([weak, token]
            {
                if (auto* self = weak.get())
                    self->finishGeneration (token);
            });
        });
    }
    else
    {
        auto gen = generator;
        auto progress = makeProgress();
        pool.addJob ([gen, req, progress, weak, token]
        {
            gen->generate (req, progress);
            juce::MessageManager::callAsync ([weak, token]
            {
                if (auto* self = weak.get())
                    self->finishGeneration (token);
            });
        });
    }

    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::addCandidate (int token, Patch p)
{
    if (token != generationToken)
        return;

    // Appended, never auditioned automatically: the panel groups AI ideas and
    // random variations, and the player decides what to hear next.
    labState.candidates.push_back (std::move (p));
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::setStatus (int token, const juce::String& s)
{
    if (token != generationToken)
        return;
    labState.status = s;
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::setProgress (int token, float fraction, const juce::String& detail)
{
    if (token != generationToken)
        return;
    labState.progress = fraction;
    labState.progressDetail = detail;
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::finishGeneration (int token)
{
    if (token != generationToken)
        return;

    labState.generating = false;
    labState.progress = -1.0f;
    labState.progressDetail.clear();
    const bool wasCancelled = cancelRequested.exchange (false);

    if (labState.candidates.empty())
        labState.status = wasCancelled ? "Stopped" : "Generation failed";
    else
    {
        labState.status = "Generation " + juce::String (labState.generation) + kDot + engineName()
                        + (wasCancelled ? " (stopped early)" : "");
        autoSaveGeneration();
    }

    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::goBackGeneration()
{
    if (labState.history.empty() || labState.generating)
        return;

    auto& previous = labState.history.back();
    labState.candidates = std::move (previous.candidates);
    labState.seed = std::move (previous.seed);
    labState.seedIsPatch = previous.seedIsPatch;
    labState.generation = juce::jmax (1, previous.generation > 0 ? previous.generation : labState.generation - 1);
    labState.history.pop_back();
    labState.status = "Generation " + juce::String (labState.generation) + "  (restored)";
    labState.auditioned = -1;
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::audition (int index)
{
    if (index < 0 || index >= (int) labState.candidates.size())
        return;

    applyPatch (labState.candidates[(size_t) index]);
    labState.auditioned = index;
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::auditionFromTree (int generation, int candidate)
{
    const bool current = generation >= (int) labState.history.size();
    if (generation < 0) return;
    const auto& candidates = current ? labState.candidates : labState.history[(size_t) generation].candidates;
    const auto& seed = current ? labState.seed : labState.history[(size_t) generation].seed;
    const bool seedIsPatch = current ? labState.seedIsPatch : labState.history[(size_t) generation].seedIsPatch;
    if (candidate < 0)
    {
        if (! seedIsPatch) return;
        applyPatch (seed);
        labState.auditioned = -1;
    }
    else
    {
        if (candidate >= (int) candidates.size()) return;
        applyPatch (candidates[(size_t) candidate]);
        labState.auditioned = current ? candidate : -1;
    }
    labBroadcaster.sendChangeMessage();
}

juce::String StacksAudioProcessor::currentExplanationKey() const
{
    const auto p = currentPatch();
    return p.name + "|" + describePatch (p);
}

void StacksAudioProcessor::explainCurrentPatch()
{
    const auto patch = currentPatch();
    const auto key = currentExplanationKey();
    const int token = ++explainToken;
    labState.explanationKey = key;
    labState.explanation = patchTips (patch);
    labState.modelExplanation.clear();
    auto backend = activeBackend;
    labState.explaining = backend != nullptr;
    labBroadcaster.sendChangeMessage();
    if (backend == nullptr)
        return;

    juce::WeakReference<StacksAudioProcessor> weak (this);
    pool.addJob ([weak, token, backend, patch]
    {
        juce::String text, error;
        double lastPush = 0.0;
        auto push = [&] (bool force)
        {
            const double now = juce::Time::getMillisecondCounterHiRes();
            if (! force && now - lastPush < 150.0) return;
            lastPush = now;
            juce::MessageManager::callAsync ([weak, token, text]
            {
                if (auto* self = weak.get())
                    if (self->explainToken.load() == token)
                    {
                        self->labState.modelExplanation = text.trim();
                        self->labBroadcaster.sendChangeMessage();
                    }
            });
        };
        PatchExplainer::explain (*backend, patch, [&] (const juce::String& t) { text += t; push (false); },
                                 [weak, token] { auto* self = weak.get(); return self == nullptr || self->explainToken.load() != token; }, error);
        push (true);
        juce::MessageManager::callAsync ([weak, token]
        {
            if (auto* self = weak.get())
                if (self->explainToken.load() == token)
                {
                    self->labState.explaining = false;
                    self->labBroadcaster.sendChangeMessage();
                }
        });
    });
}

void StacksAudioProcessor::setCurrentTags (const juce::StringArray& tags)
{
    patchTags = tags;
    if (patchFile.isNotEmpty() && juce::File (patchFile).existsAsFile())
        setFileTags (juce::File (patchFile), tags);
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::setFileTags (const juce::File& file, const juce::StringArray& tags)
{
    if (auto p = Patch::fromJson (file.loadFileAsString()))
    {
        p->tags = tags;
        file.replaceWithText (p->toJson());
        if (patchFile == file.getFullPathName()) patchTags = tags;
        for (auto& c : labState.candidates)
            if (c.filePath == file.getFullPathName()) c.tags = tags;
    }
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::auditionSeed()
{
    if (! labState.seedIsPatch)
        return;
    applyPatch (labState.seed);
    labState.auditioned = -1;
    labBroadcaster.sendChangeMessage();
}

//==============================================================================
// ~/Music rather than ~/Documents: Documents is behind a macOS privacy prompt
// that blocks the plug-in on first launch inside a host.
juce::File StacksAudioProcessor::libraryRoot()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Stacks Patches");
}

juce::File StacksAudioProcessor::historyRoot()
{
    return ModelManager::appDataDirectory().getChildFile ("history");
}

void StacksAudioProcessor::setLibraryFolder (const juce::File& folder)
{
    if (! folder.isDirectory())
        return;
    currentFolder = folder;
    settings().setValue ("libraryFolder", folder.getFullPathName());
    settings().saveIfNeeded();
    labBroadcaster.sendChangeMessage();
}

juce::File StacksAudioProcessor::savePatchToLibrary (Patch& p, const juce::File& folder)
{
    folder.createDirectory();
    juce::File file (p.filePath);
    if (! file.existsAsFile() || ! file.isAChildOf (libraryRoot()))
        file = folder.getNonexistentChildFile (juce::File::createLegalFileName (p.name.isEmpty() ? "Patch" : p.name), ".json", false);
    file.replaceWithText (p.toJson());
    p.filePath = file.getFullPathName();
    return file;
}

void StacksAudioProcessor::writeFavouriteFlag (const juce::File& file, bool favourite)
{
    if (auto p = Patch::fromJson (file.loadFileAsString()))
    {
        p->favourite = favourite;
        file.replaceWithText (p->toJson());
    }
}

juce::File StacksAudioProcessor::savePreset (const juce::String& name, const juce::File& folder, bool asNewFile)
{
    auto p = currentPatch();
    p.name = name.trim().isEmpty() ? patchName : name.trim();
    if (p.tags.isEmpty()) p.tags = autoTags (p);   // every saved preset is searchable
    patchTags = p.tags;
    // Overwrite only the loaded file itself (same folder, same name) unless a new file was asked for.
    juce::File existing (p.filePath);
    if (asNewFile || ! (existing.existsAsFile() && existing.getParentDirectory() == folder
                        && existing.getFileNameWithoutExtension() == juce::File::createLegalFileName (p.name)))
        p.filePath.clear();
    const auto file = savePatchToLibrary (p, folder);
    patchName = p.name;
    patchFile = p.filePath;
    loadedSnapshot = p;
    if (labState.auditioned >= 0 && labState.auditioned < (int) labState.candidates.size())
        labState.candidates[(size_t) labState.auditioned].filePath = p.filePath;
    labState.status = "Saved \"" + p.name + "\" to " + (folder == libraryRoot() ? juce::String ("the library") : folder.getFileName());
    labBroadcaster.sendChangeMessage();
    return file;
}

std::vector<juce::File> StacksAudioProcessor::libraryFolders() const
{
    std::vector<juce::File> folders { libraryRoot() };
    auto subs = libraryRoot().findChildFiles (juce::File::findDirectories, true);
    subs.sort();
    for (const auto& f : subs)
        folders.push_back (f);
    return folders;
}

void StacksAudioProcessor::toggleFavourite (int index)
{
    if (index < 0 || index >= (int) labState.candidates.size())
        return;

    auto& candidate = labState.candidates[(size_t) index];
    candidate.favourite = ! candidate.favourite;
    if (candidate.filePath.isNotEmpty() && juce::File (candidate.filePath).existsAsFile())
        writeFavouriteFlag (juce::File (candidate.filePath), candidate.favourite);
    if (labState.auditioned == index)
        patchFavourite = candidate.favourite;
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::favouriteCurrent()
{
    patchFavourite = ! patchFavourite;
    if (patchFile.isNotEmpty() && juce::File (patchFile).existsAsFile())
        writeFavouriteFlag (juce::File (patchFile), patchFavourite);
    if (labState.auditioned >= 0 && labState.auditioned < (int) labState.candidates.size())
        labState.candidates[(size_t) labState.auditioned].favourite = patchFavourite;
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::toggleFavouriteFile (const juce::File& file)
{
    if (auto p = Patch::fromJson (file.loadFileAsString()))
    {
        p->favourite = ! p->favourite;
        file.replaceWithText (p->toJson());
        if (patchFile == file.getFullPathName())
            patchFavourite = p->favourite;
        for (auto& c : labState.candidates)
            if (c.filePath == file.getFullPathName())
                c.favourite = p->favourite;
    }
    labBroadcaster.sendChangeMessage();
}

bool StacksAudioProcessor::loadLibraryPatch (const juce::File& file)
{
    auto p = Patch::fromJson (file.loadFileAsString());
    if (! p)
        return false;
    p->filePath = file.getFullPathName();
    if (p->name.isEmpty()) p->name = file.getFileNameWithoutExtension();
    applyPatch (*p);
    labState.auditioned = -1;
    labBroadcaster.sendChangeMessage();
    return true;
}

//==============================================================================
juce::String StacksAudioProcessor::labToJson() const
{
    auto toArray = [] (const std::vector<Patch>& patches)
    {
        juce::Array<juce::var> arr;
        for (const auto& p : patches)
        {
            auto v = p.toVar();
            if (p.filePath.isNotEmpty())
                if (auto* obj = v.getDynamicObject())
                    obj->setProperty ("file", p.filePath);
            arr.add (v);
        }
        return juce::var (arr);
    };

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("generation", labState.generation);
    obj->setProperty ("auditioned", labState.auditioned);
    obj->setProperty ("patchName", patchName);
    obj->setProperty ("patchCategory", patchCategory);
    obj->setProperty ("patchOrigin", patchOrigin);
    obj->setProperty ("candidates", toArray (labState.candidates));
    obj->setProperty ("seed", labState.seed.toVar());
    obj->setProperty ("seedIsPatch", labState.seedIsPatch);
    juce::Array<juce::var> tagArr;
    for (const auto& t : patchTags) tagArr.add (t);
    obj->setProperty ("patchTags", juce::var (tagArr));
    juce::Array<juce::var> hist;   // the family tree, most recent 12 generations
    for (size_t i = labState.history.size() > 12 ? labState.history.size() - 12 : 0; i < labState.history.size(); ++i)
    {
        const auto& gen = labState.history[i];
        auto* h = new juce::DynamicObject();
        h->setProperty ("candidates", toArray (gen.candidates));
        h->setProperty ("seed", gen.seed.toVar());
        h->setProperty ("seedIsPatch", gen.seedIsPatch);
        h->setProperty ("generation", gen.generation);
        hist.add (juce::var (h));
    }
    obj->setProperty ("history", juce::var (hist));
    obj->setProperty ("patchFile", patchFile);
    obj->setProperty ("patchFavourite", patchFavourite);
    return juce::JSON::toString (juce::var (obj), true);
}

void StacksAudioProcessor::labFromJson (const juce::String& json)
{
    auto parsed = juce::JSON::parse (json);
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return;

    auto readArray = [] (const juce::var& v)
    {
        std::vector<Patch> out;
        if (auto* arr = v.getArray())
            for (const auto& item : *arr)
                if (auto p = Patch::fromVar (item))
                {
                    if (auto* obj = item.getDynamicObject())
                        p->filePath = obj->getProperty ("file").toString();
                    out.push_back (*p);
                }
        return out;
    };

    labState.generation = (int) obj->getProperty ("generation");
    labState.auditioned = (int) obj->getProperty ("auditioned");
    labState.candidates = readArray (obj->getProperty ("candidates"));
    // Sessions saved before the seed existed show the loaded sound as the seed.
    labState.seed = Patch();
    labState.seed.name.clear();
    if (auto seed = Patch::fromVar (obj->getProperty ("seed"))) labState.seed = *seed;
    labState.seedIsPatch = (bool) obj->getProperty ("seedIsPatch");
    patchTags.clear();
    if (auto* t = obj->getProperty ("patchTags").getArray()) for (const auto& v : *t) patchTags.add (v.toString());
    patchFile = obj->getProperty ("patchFile").toString();
    patchFavourite = (bool) obj->getProperty ("patchFavourite");
    labState.history.clear();
    if (auto* hist = obj->getProperty ("history").getArray())
        for (const auto& hv : *hist)
            if (auto* h = hv.getDynamicObject())
            {
                LabState::Generation gen;
                gen.candidates = readArray (h->getProperty ("candidates"));
                if (auto seed = Patch::fromVar (h->getProperty ("seed"))) gen.seed = *seed;
                gen.seedIsPatch = (bool) h->getProperty ("seedIsPatch");
                gen.generation = (int) h->getProperty ("generation");
                labState.history.push_back (std::move (gen));
            }
    labState.generating = false;
    labState.status = labState.generation > 0 ? "Generation " + juce::String (labState.generation) : juce::String();

    const auto savedName = obj->getProperty ("patchName").toString();
    if (savedName.isNotEmpty())
        patchName = savedName;
    patchCategory = obj->getProperty ("patchCategory").toString();
    patchOrigin = obj->getProperty ("patchOrigin").toString();

    if (labState.auditioned >= (int) labState.candidates.size())
        labState.auditioned = -1;
}

} // namespace stacks

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new stacks::StacksAudioProcessor();
}
