#include "PluginProcessor.h"
#include "PluginEditor.h"
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
    voiceContext.params = &params;

    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new SynthVoice (voiceContext));
    synth.addSound (new SynthSound());
    synth.setNoteStealingEnabled (true);

    refreshModels();
    loadEngineFromSettings();
    refreshOllamaModels();
    startTimer (30000);
}

StacksAudioProcessor::~StacksAudioProcessor()
{
    stopTimer();
    downloader.reset();
    cancelRequested = true;
    pool.removeAllJobs (true, 8000);
}

//==============================================================================
void StacksAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };

    chorus.prepare (spec);
    chorus.reset();
    chorus.setCentreDelay (7.0f);
    chorus.setFeedback (0.0f);

    delay.prepare (spec);
    delay.setMaximumDelayInSamples ((int) (2.0 * sampleRate) + 2);
    delay.reset();
    delayFeedbackState[0] = delayFeedbackState[1] = 0.0f;
    delaySamples.reset (sampleRate, 0.05);
    delaySamples.setCurrentAndTargetValue (rawParams[(size_t) P::delay_time]->load() * (float) sampleRate);

    reverb.prepare (spec);
    reverb.reset();
    wetBuffer.setSize (2, juce::jmax (1, samplesPerBlock));

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

    synth.renderNextBlock (buffer, midi, 0, numSamples);
    processEffects (buffer);
}

void StacksAudioProcessor::processEffects (juce::AudioBuffer<float>& buffer)
{
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0)
        return;

    juce::dsp::AudioBlock<float> block (buffer);
    auto mainBlock = block.getSubsetChannelBlock (0, (size_t) numCh);

    // Chorus
    const float chorusMix = params.get (P::chorus_mix);
    if (chorusMix > 0.0005f)
    {
        chorus.setRate (params.get (P::chorus_rate));
        chorus.setDepth (params.get (P::chorus_depth));
        chorus.setMix (chorusMix);
        juce::dsp::ProcessContextReplacing<float> ctx (mainBlock);
        chorus.process (ctx);
    }

    // Delay with a one-pole low-pass in the feedback path
    {
        const float mix = params.get (P::delay_mix);
        const float fb  = params.get (P::delay_feedback);
        delaySamples.setTargetValue (params.get (P::delay_time) * (float) currentSampleRate);

        for (int i = 0; i < n; ++i)
        {
            const float d = delaySamples.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* x = buffer.getWritePointer (ch);
                const float in = x[i];
                delay.pushSample (ch, in + fb * delayFeedbackState[ch]);
                const float wet = delay.popSample (ch, d);
                delayFeedbackState[ch] += 0.4f * (wet - delayFeedbackState[ch]);
                x[i] = in + mix * wet;
            }
        }
    }

    // Reverb, run in parallel and mixed in
    const float reverbMix = params.get (P::reverb_mix);
    if (reverbMix > 0.0005f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize   = params.get (P::reverb_size);
        rp.damping    = params.get (P::reverb_damp);
        rp.wetLevel   = 0.33f;
        rp.dryLevel   = 0.0f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);

        wetBuffer.setSize (numCh, n, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            wetBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

        juce::dsp::AudioBlock<float> wet (wetBuffer);
        juce::dsp::ProcessContextReplacing<float> ctx (wet);
        reverb.process (ctx);

        for (int ch = 0; ch < numCh; ++ch)
            buffer.addFrom (ch, 0, wetBuffer, ch, 0, n, reverbMix);
    }

    // Master gain + safety clip
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (params.get (P::master_gain)));
    for (int i = 0; i < n; ++i)
    {
        const float g = masterGain.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
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
            apvts.replaceState (state);

        auto lab = root.getChildWithName ("Lab");
        if (lab.isValid())
            labFromJson (lab["json"].toString());
    }
    else if (root.hasType (apvts.state.getType()))
    {
        apvts.replaceState (root);
    }

    labBroadcaster.sendChangeMessage();
}

//==============================================================================
Patch StacksAudioProcessor::currentPatch() const
{
    auto p = Patch::capture (apvts);
    p.name = patchName;
    p.description = describePatch (p);
    if (labState.auditioned >= 0 && labState.auditioned < (int) labState.candidates.size())
        p.category = labState.candidates[(size_t) labState.auditioned].category;
    return p;
}

void StacksAudioProcessor::applyPatch (const Patch& p)
{
    p.applyTo (apvts);
    patchName = p.name;
}

void StacksAudioProcessor::setCurrentPatchName (const juce::String& name)
{
    patchName = name.trim().isEmpty() ? juce::String ("Untitled") : name.trim();
}

//==============================================================================
void StacksAudioProcessor::loadEngineFromSettings()
{
    auto& s = settings();
    EngineChoice choice;
    const auto kind = s.getValue ("engine", "random");
    choice.kind  = kind == "ollama" ? EngineKind::Ollama : kind == "builtin" ? EngineKind::Builtin : EngineKind::Random;
    choice.model = s.getValue ("model", "");
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

    if (engineChoice.kind == EngineKind::Ollama)
    {
        generator = std::make_shared<LlmPatchGenerator> (std::make_shared<OllamaBackend> (engineChoice.model), randomGenerator);
    }
    else if (engineChoice.kind == EngineKind::Builtin)
    {
        for (const auto& m : knownModels)
        {
            if (m.id != engineChoice.model || ! m.installed)
                continue;
            if (builtInBackend == nullptr || builtInBackend->file() != m.file)
                builtInBackend = std::make_shared<LlamaBackend> (m.file, m.label);
            generator = std::make_shared<LlmPatchGenerator> (builtInBackend, randomGenerator);
            break;
        }
    }
}

void StacksAudioProcessor::timerCallback()
{
    if (builtInBackend != nullptr && ! labState.generating)
        builtInBackend->unloadIfIdle (600.0); // ten minutes idle -> give the RAM back
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
    req.parents = labState.favourites.empty() ? std::vector<Patch> { currentPatch() } : labState.favourites;
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

    const int token = ++generationToken;
    cancelRequested = false;
    aiInserted = 0;

    // The new batch replaces the old one right away so streamed results have somewhere to land.
    if (! labState.candidates.empty())
    {
        labState.history.push_back (std::move (labState.candidates));
        if (labState.history.size() > 20)
            labState.history.erase (labState.history.begin());
    }
    labState.candidates.clear();
    labState.auditioned = -1;
    labState.generation = req.generation;
    labState.generating = true;
    labState.status = "Generating with " + engineName() + "...";

    const bool usingAi = generator != randomGenerator;
    juce::WeakReference<StacksAudioProcessor> weak (this);

    auto makeProgress = [weak, token, this] (bool insertAtTop)
    {
        GenerationProgress progress;
        progress.onPatch = [weak, token, insertAtTop] (const Patch& p)
        {
            juce::MessageManager::callAsync ([weak, token, insertAtTop, p]
            {
                if (auto* self = weak.get())
                    self->addCandidate (token, p, insertAtTop);
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
        if (! labState.candidates.empty())
            audition (0);

        GenerationRequest aiReq = req;
        aiReq.count = kAiPatchesPerBatch;
        auto gen = generator;
        auto progress = makeProgress (true);
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
        auto progress = makeProgress (false);
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

void StacksAudioProcessor::addCandidate (int token, Patch p, bool insertAtTop)
{
    if (token != generationToken)
        return;

    if (insertAtTop)
    {
        const int index = juce::jmin (aiInserted, (int) labState.candidates.size());
        labState.candidates.insert (labState.candidates.begin() + index, std::move (p));
        if (labState.auditioned >= index)
            ++labState.auditioned;
        ++aiInserted;
    }
    else
    {
        labState.candidates.push_back (std::move (p));
    }

    if (labState.auditioned < 0)
        audition (0);
    else
        labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::setStatus (int token, const juce::String& s)
{
    if (token != generationToken)
        return;
    labState.status = s;
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::finishGeneration (int token)
{
    if (token != generationToken)
        return;

    labState.generating = false;
    const bool wasCancelled = cancelRequested.exchange (false);

    if (labState.candidates.empty())
        labState.status = wasCancelled ? "Stopped" : "Generation failed";
    else
        labState.status = "Generation " + juce::String (labState.generation) + kDot + engineName()
                        + (wasCancelled ? " (stopped early)" : "");

    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::goBackGeneration()
{
    if (labState.history.empty() || labState.generating)
        return;

    labState.candidates = std::move (labState.history.back());
    labState.history.pop_back();
    labState.generation = juce::jmax (1, labState.generation - 1);
    labState.status = "Generation " + juce::String (labState.generation) + "  (restored)";
    labState.auditioned = -1;
    audition (0);
}

void StacksAudioProcessor::audition (int index)
{
    if (index < 0 || index >= (int) labState.candidates.size())
        return;

    applyPatch (labState.candidates[(size_t) index]);
    labState.auditioned = index;
    labBroadcaster.sendChangeMessage();
}

int StacksAudioProcessor::indexOfFavourite (const Patch& p) const
{
    for (int i = 0; i < (int) labState.favourites.size(); ++i)
        if (labState.favourites[(size_t) i].sameValuesAs (p))
            return i;
    return -1;
}

void StacksAudioProcessor::toggleFavourite (int index)
{
    if (index < 0 || index >= (int) labState.candidates.size())
        return;

    const auto& candidate = labState.candidates[(size_t) index];
    const int existing = indexOfFavourite (candidate);
    if (existing >= 0)
        labState.favourites.erase (labState.favourites.begin() + existing);
    else
        labState.favourites.push_back (candidate);

    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::removeFavourite (int favouriteIndex)
{
    if (favouriteIndex < 0 || favouriteIndex >= (int) labState.favourites.size())
        return;
    labState.favourites.erase (labState.favourites.begin() + favouriteIndex);
    labBroadcaster.sendChangeMessage();
}

void StacksAudioProcessor::favouriteCurrent()
{
    auto p = currentPatch();
    if (indexOfFavourite (p) < 0)
        labState.favourites.push_back (std::move (p));
    labBroadcaster.sendChangeMessage();
}

//==============================================================================
juce::String StacksAudioProcessor::labToJson() const
{
    auto toArray = [] (const std::vector<Patch>& patches)
    {
        juce::Array<juce::var> arr;
        for (const auto& p : patches)
            arr.add (p.toVar());
        return juce::var (arr);
    };

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("generation", labState.generation);
    obj->setProperty ("auditioned", labState.auditioned);
    obj->setProperty ("patchName", patchName);
    obj->setProperty ("candidates", toArray (labState.candidates));
    obj->setProperty ("favourites", toArray (labState.favourites));
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
                    out.push_back (*p);
        return out;
    };

    labState.generation = (int) obj->getProperty ("generation");
    labState.auditioned = (int) obj->getProperty ("auditioned");
    labState.candidates = readArray (obj->getProperty ("candidates"));
    labState.favourites = readArray (obj->getProperty ("favourites"));
    labState.history.clear();
    labState.generating = false;
    labState.status = labState.generation > 0 ? "Generation " + juce::String (labState.generation) : juce::String();

    const auto savedName = obj->getProperty ("patchName").toString();
    if (savedName.isNotEmpty())
        patchName = savedName;

    if (labState.auditioned >= (int) labState.candidates.size())
        labState.auditioned = -1;
}

} // namespace stacks

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new stacks::StacksAudioProcessor();
}
