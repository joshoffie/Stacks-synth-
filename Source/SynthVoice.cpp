#include "SynthVoice.h"

#include <cmath>

namespace stacks
{

namespace
{
    constexpr float kVoiceGain = 0.35f;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
    inline float wrap01 (float p) noexcept     { return p - std::floor (p); }

    juce::dsp::LadderFilterMode filterModeFor (int type) noexcept
    {
        using M = juce::dsp::LadderFilterMode;
        switch (type)
        {
            case 0:  return M::LPF12;
            case 1:  return M::LPF24;
            case 2:  return M::HPF12;
            case 3:  return M::HPF24;
            case 4:  return M::BPF12;
            default: return M::BPF24;
        }
    }
}

SynthVoice::SynthVoice (VoiceContext& context) : ctx (context)
{
    rng.setSeedRandomly();
    filter.setMode (filterMode);
}

bool SynthVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*> (sound) != nullptr;
}

void SynthVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    if (newRate <= 0.0)
        return;

    juce::dsp::ProcessSpec spec { newRate, (juce::uint32) kSub, 2 };
    filter.prepare (spec);
    filter.reset();
    ampEnv.setSampleRate (newRate);
    filterEnv.setSampleRate (newRate);
    modEnv.setSampleRate (newRate);
}

void SynthVoice::updateEnvelopes (const SynthParams& p)
{
    ampEnv.setParameters ({ p.get (P::aenv_attack), p.get (P::aenv_decay),
                            p.get (P::aenv_sustain), p.get (P::aenv_release) });
    filterEnv.setParameters ({ p.get (P::fenv_attack), p.get (P::fenv_decay),
                               p.get (P::fenv_sustain), p.get (P::fenv_release) });
    modEnv.setParameters ({ p.get (P::menv_attack), p.get (P::menv_decay),
                            p.get (P::menv_sustain), p.get (P::menv_release) });
}

void SynthVoice::startNote (int midiNoteNumber, float vel, juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    const auto& p = *ctx.params;

    velocity = vel;
    velocityGain = 0.25f + 0.75f * vel;
    noteRandom = rng.nextFloat() * 2.0f - 1.0f;
    serial = ++ctx.voiceCounter;
    ctx.displayVoice.store (serial);
    pitchWheelMoved (currentPitchWheelPosition);

    // Glide starts from whatever note was played last, on any voice.
    targetNote = (float) midiNoteNumber;
    const float glideTime = p.get (P::glide);
    const float last = ctx.lastNote.load();
    if (glideTime > 0.001f && last >= 0.0f && std::abs (last - targetNote) > 0.01f)
    {
        currentNote = last;
        glideInc = (targetNote - currentNote) / (glideTime * (float) getSampleRate());
    }
    else
    {
        currentNote = targetNote;
        glideInc = 0.0f;
    }
    ctx.lastNote.store (targetNote);

    // Spread unison start phases so stacked copies don't all hit the same peak.
    const int unison = juce::jlimit (1, kMaxUnison, p.geti (P::unison_voices));
    for (int u = 0; u < kMaxUnison; ++u)
    {
        const float ph = unison > 1 ? (float) u / (float) unison : 0.0f;
        phaseA[u] = ph;
        phaseB[u] = ph;
    }
    subPhase = 0.0f;
    for (int k = 0; k < kNumLfos; ++k)
    {
        lfoNotePhase[k] = 0.0f;
        lfoHeld[k] = rng.nextFloat() * 2.0f - 1.0f;
    }

    updateEnvelopes (p);
    filter.reset();
    ampEnv.noteOn();
    filterEnv.noteOn();
    modEnv.noteOn();
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnv.noteOff();
        filterEnv.noteOff();
        modEnv.noteOff();
    }
    else
    {
        clearCurrentNote();
        ampEnv.reset();
        filterEnv.reset();
        modEnv.reset();
    }
}

void SynthVoice::pitchWheelMoved (int newPitchWheelValue)
{
    pitchBendSemis = (float) (newPitchWheelValue - 8192) / 8192.0f * 2.0f;
}

void SynthVoice::controllerMoved (int controllerNumber, int newValue)
{
    if (controllerNumber == 1)
        ctx.modWheel.store ((float) newValue / 127.0f);
}

void SynthVoice::aftertouchChanged (int newValue)
{
    aftertouch = (float) newValue / 127.0f;
    ctx.aftertouch.store (aftertouch);
}

void SynthVoice::channelPressureChanged (int newValue)
{
    aftertouch = (float) newValue / 127.0f;
    ctx.aftertouch.store (aftertouch);
}

// Value of LFO k for this sub-block. Free-running LFOs follow the processor's
// global phase so every voice (and the effects) see the same waveform; Note
// mode LFOs restart with each key and live in the voice.
float SynthVoice::lfoValueFor (int k, const SynthParams& p, int sampleInBlock, int blockLen, float& held) noexcept
{
    const int shape = p.geti (lfoShapeParam (k));
    const bool noteMode = p.geti (lfoModeParam (k)) == 1;
    const float phaseOffset = p.get (lfoPhaseParam (k));
    const float sr = (float) getSampleRate();

    float rate;
    const float beats = lfoSyncBeats (p.geti (lfoSyncParam (k)));
    if (beats > 0.0f)
        rate = (float) (ctx.bpm / 60.0) / beats;
    else
        rate = p.get (lfoRateParam (k));

    float phase;
    if (noteMode)
    {
        phase = lfoNotePhase[k];
        lfoNotePhase[k] += rate * (float) blockLen / sr;
        if (lfoNotePhase[k] >= 1.0f)
        {
            lfoNotePhase[k] = wrap01 (lfoNotePhase[k]);
            held = rng.nextFloat() * 2.0f - 1.0f;
        }
        if (shape == ShapeRandom)
            return held;
    }
    else
    {
        if (shape == ShapeRandom)
            return ctx.lfoGlobalValue[k];
        phase = ctx.lfoBlockPhase[k] + ctx.lfoRateHz[k] * (float) sampleInBlock / sr;
    }

    return ctx.lfoTables->get (k).at (phase + phaseOffset);
}

// Built-in tables come from the shared bank, "User n" from the imported slots,
// "Custom" from the patch's designed table for this oscillator (a sine stands
// in for an empty slot).
float SynthVoice::readWave (int osc, int wave, int mip, float morph, float phase) const noexcept
{
    if (wave < WavetableBank::kNumBuiltIn)
        return ctx.bank->read (wave, mip, morph, phase);
    const int slot = wave >= kCustomWave ? UserWavetables::customSlot (osc) : wave - WavetableBank::kNumBuiltIn;
    if (auto* table = ctx.user != nullptr ? ctx.user->active (slot) : nullptr)
        return table->read (mip, morph, phase);
    return std::sin (twoPi * phase);
}

// LFOs, Key and Random are bipolar (-1..1); envelopes and controllers are 0..1.
float SynthVoice::sourceValue (int source, const float* lfo, float filterEnvValue, float modEnvValue) const noexcept
{
    switch (source)
    {
        case SrcLfo1:       return lfo[0];
        case SrcLfo2:       return lfo[1];
        case SrcLfo3:       return lfo[2];
        case SrcLfo4:       return lfo[3];
        case SrcFilterEnv:  return filterEnvValue;
        case SrcModEnv:     return modEnvValue;
        case SrcVelocity:   return velocity;
        case SrcKey:        return juce::jlimit (-1.0f, 1.0f, (currentNote - 60.0f) / 36.0f);
        case SrcModWheel:   return ctx.modWheel.load();
        case SrcAftertouch: return aftertouch;
        case SrcRandom:     return noteRandom;
        default:            return 0.0f;
    }
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const auto& base = *ctx.params;
    const double sr  = getSampleRate();
    const float fsr  = (float) sr;
    const int blockEnd = startSample + numSamples;

    auto* scratchL = scratch.getWritePointer (0);
    auto* scratchR = scratch.getWritePointer (1);
    const int numOut = out.getNumChannels();

    int pos = startSample;
    int remaining = numSamples;

    while (remaining > 0)
    {
        const int n = juce::jmin (remaining, kSub);

        // ---- envelopes & LFOs for this sub-block ------------------------------
        float envF = 0.0f, envM = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            envF = filterEnv.getNextSample();
            envM = modEnv.getNextSample();
        }
        float lfo[kNumLfos];
        for (int k = 0; k < kNumLfos; ++k)
            lfo[k] = lfoValueFor (k, base, pos, n, lfoHeld[k]);

        // ---- the connections: a modulated copy of every parameter -------------
        local = base;
        float pitchMod = 0.0f, pitchBMod = 0.0f, pan = 0.0f, ampGain = 1.0f;
        for (int i = 0; i < kNumModSlots; ++i)
        {
            const int src = base.geti (modSourceParam (i));
            const int target = base.geti (modDestParam (i));
            const float amount = base.get (modAmountParam (i));
            if (src == SrcOff || target == TargetOff || std::abs (amount) < 1.0e-4f)
                continue;

            const float s = sourceValue (src, lfo, envF, envM);
            switch (target)
            {
                case TargetPitch:  pitchMod  += amount * s * 12.0f; break;
                case TargetPitchB: pitchBMod += amount * s * 12.0f; break;
                case TargetPan:    pan       += amount * s; break;
                case TargetAmp:
                {
                    // Attenuation style: velocity/envelope at full amount scale from
                    // silence to full; an LFO becomes tremolo.
                    float u = isBipolarSource (src) ? 0.5f * (s + 1.0f) : s;
                    if (amount < 0.0f) u = 1.0f - u;
                    ampGain *= 1.0f - std::abs (amount) * (1.0f - juce::jlimit (0.0f, 1.0f, u));
                    break;
                }
                default:
                {
                    const int paramIndex = modTargetParamIndex (target);
                    if (paramIndex >= 0)
                        nudgeParam (local, paramIndex, amount * s);
                    break;
                }
            }
        }
        const auto& p = local;
        if (ctx.liveValues != nullptr && ctx.displayVoice.load (std::memory_order_relaxed) == serial)
            for (int i = 0; i < kNumParams; ++i)
                ctx.liveValues[i].store (local.v[i], std::memory_order_relaxed);
        pitchMod  = juce::jlimit (-24.0f, 24.0f, pitchMod);
        pitchBMod = juce::jlimit (-24.0f, 24.0f, pitchBMod);
        pan = juce::jlimit (-1.0f, 1.0f, pan);

        updateEnvelopes (p);

        // ---- per-sub-block constants, from the modulated copy -----------------
        const int unison        = juce::jlimit (1, kMaxUnison, p.geti (P::unison_voices));
        const float detuneCents = p.get (P::unison_detune);
        const float spread      = p.get (P::unison_spread);
        constexpr int kLastWave = kCustomWave;
        const int waveA         = juce::jlimit (0, kLastWave, p.geti (P::oscA_wave));
        const int waveB         = juce::jlimit (0, kLastWave, p.geti (P::oscB_wave));
        const float levelA      = p.get (P::oscA_level);
        const float levelB      = p.get (P::oscB_level);
        const float subLevel    = p.get (P::sub_level);
        const float noiseLevel  = p.get (P::noise_level) * 0.5f;
        const float semisA      = (float) p.geti (P::oscA_coarse) + p.get (P::oscA_fine) / 100.0f;
        const float semisB      = (float) p.geti (P::oscB_coarse) + p.get (P::oscB_fine) / 100.0f + pitchBMod;
        const float morphA      = juce::jlimit (0.0f, 1.0f, p.get (P::oscA_morph));
        const float morphB      = juce::jlimit (0.0f, 1.0f, p.get (P::oscB_morph));
        const float fm          = juce::jlimit (0.0f, 1.0f, p.get (P::fm_amount));
        const float fmDepth     = fm * fm * 2.0f;        // cycles of phase excursion
        const float unisonComp  = 1.0f / std::sqrt ((float) unison);

        float gainL[kMaxUnison], gainR[kMaxUnison], detuneRatio[kMaxUnison];
        for (int u = 0; u < unison; ++u)
        {
            const float offset = unison > 1 ? -1.0f + 2.0f * (float) u / (float) (unison - 1) : 0.0f;
            const float upan   = offset * spread;
            gainL[u] = unison > 1 ? std::sqrt (0.5f * (1.0f - upan)) * juce::MathConstants<float>::sqrt2 : 1.0f;
            gainR[u] = unison > 1 ? std::sqrt (0.5f * (1.0f + upan)) * juce::MathConstants<float>::sqrt2 : 1.0f;
            detuneRatio[u] = std::exp2 (offset * detuneCents / 1200.0f);
        }

        // ---- glide --------------------------------------------------------
        if (glideInc != 0.0f)
        {
            currentNote += glideInc * (float) n;
            if ((glideInc > 0.0f && currentNote >= targetNote) || (glideInc < 0.0f && currentNote <= targetNote))
            {
                currentNote = targetNote;
                glideInc = 0.0f;
            }
        }

        const float note   = currentNote + pitchBendSemis + pitchMod;
        const float baseHz = midiToHz (note);
        const float hzA    = baseHz * std::exp2 (semisA / 12.0f);
        const float hzB    = baseHz * std::exp2 (semisB / 12.0f);
        const int mipA     = WavetableBank::levelFor (hzA * (1.0f + 3.0f * fm), sr); // FM widens A's spectrum
        const int mipB     = WavetableBank::levelFor (hzB, sr);

        float incA[kMaxUnison], incB[kMaxUnison];
        for (int u = 0; u < unison; ++u)
        {
            incA[u] = hzA * detuneRatio[u] / fsr;
            incB[u] = hzB * detuneRatio[u] / fsr;
        }
        const float subInc = baseHz * 0.5f / fsr;

        // ---- oscillators -> scratch (pre-filter) --------------------------
        for (int i = 0; i < n; ++i)
        {
            float l = 0.0f, r = 0.0f;
            for (int u = 0; u < unison; ++u)
            {
                const float sB = readWave (1, waveB, mipB, morphB, phaseB[u]);
                const float pa = wrap01 (phaseA[u] + fmDepth * sB);
                const float sA = readWave (0, waveA, mipA, morphA, pa);
                const float s  = sA * levelA + sB * levelB;
                l += s * gainL[u];
                r += s * gainR[u];

                phaseA[u] = wrap01 (phaseA[u] + incA[u]);
                phaseB[u] = wrap01 (phaseB[u] + incB[u]);
            }

            const float mono = std::sin (twoPi * subPhase) * subLevel
                             + (rng.nextFloat() * 2.0f - 1.0f) * noiseLevel;
            subPhase += subInc; if (subPhase >= 1.0f) subPhase -= 1.0f;

            scratchL[i] = l * unisonComp + mono;
            scratchR[i] = r * unisonComp + mono;
        }

        // ---- filter --------------------------------------------------------
        const auto wantedMode = filterModeFor (p.geti (P::filter_type));
        if (wantedMode != filterMode)
        {
            filterMode = wantedMode;
            filter.setMode (filterMode); // resets state, so only on a real change
        }
        const float keyTrack = p.get (P::filter_keytrack) * (currentNote - 60.0f) / 12.0f;
        float cutoff = p.get (P::filter_cutoff) * std::exp2 (envF * p.get (P::filter_env) + keyTrack);
        cutoff = juce::jlimit (20.0f, juce::jmin (20000.0f, fsr * 0.45f), cutoff);
        filter.setCutoffFrequencyHz (cutoff);
        filter.setResonance (juce::jlimit (0.0f, 1.0f, p.get (P::filter_res)));
        filter.setDrive (juce::jmax (1.0f, p.get (P::filter_drive)));

        juce::dsp::AudioBlock<float> block (scratch);
        auto subBlock = block.getSubBlock (0, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> context (subBlock);
        filter.process (context);

        // ---- amplitude, pan, output ----------------------------------------
        const float gl = std::sqrt (0.5f * (1.0f - pan)) * juce::MathConstants<float>::sqrt2;
        const float gr = std::sqrt (0.5f * (1.0f + pan)) * juce::MathConstants<float>::sqrt2;
        auto* outL = out.getWritePointer (0, pos);
        auto* outR = numOut > 1 ? out.getWritePointer (1, pos) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            const float g = ampEnv.getNextSample() * velocityGain * ampGain * kVoiceGain;
            if (outR != nullptr)
            {
                outL[i] += scratchL[i] * g * gl;
                outR[i] += scratchR[i] * g * gr;
            }
            else
            {
                outL[i] += 0.5f * (scratchL[i] + scratchR[i]) * g;
            }
        }

        pos += n;
        remaining -= n;
        juce::ignoreUnused (blockEnd);

        if (! ampEnv.isActive())
        {
            clearCurrentNote();
            filter.reset();
            break;
        }
    }
}

} // namespace stacks
