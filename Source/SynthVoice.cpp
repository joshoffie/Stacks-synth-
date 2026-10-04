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
    lfoPhase[0] = lfoPhase[1] = 0.0f;
    lfoHeld[0] = rng.nextFloat() * 2.0f - 1.0f;
    lfoHeld[1] = rng.nextFloat() * 2.0f - 1.0f;
    lfoRateMod[0] = lfoRateMod[1] = 0.0f;

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

void SynthVoice::aftertouchChanged (int newValue)       { aftertouch = (float) newValue / 127.0f; }
void SynthVoice::channelPressureChanged (int newValue)  { aftertouch = (float) newValue / 127.0f; }

float SynthVoice::lfoValue (int shape, float ph, float held) const noexcept
{
    switch (shape)
    {
        case 0:  return std::sin (twoPi * ph);
        case 1:  return 1.0f - 4.0f * std::abs (ph - 0.5f);
        case 2:  return 1.0f - 2.0f * ph;
        case 3:  return ph < 0.5f ? 1.0f : -1.0f;
        default: return held; // sample & hold
    }
}

// LFOs, Key and Random are bipolar (-1..1); envelopes and controllers are 0..1.
float SynthVoice::sourceValue (int source, float lfo1, float lfo2, float filterEnvValue, float modEnvValue) const noexcept
{
    switch (source)
    {
        case SrcLfo1:       return lfo1;
        case SrcLfo2:       return lfo2;
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

    const auto& p    = *ctx.params;
    const auto& bank = *ctx.bank;
    const double sr  = getSampleRate();
    const float fsr  = (float) sr;

    updateEnvelopes (p);

    // ---- per-block constants -------------------------------------------
    const int unison          = juce::jlimit (1, kMaxUnison, p.geti (P::unison_voices));
    const float detuneCents   = p.get (P::unison_detune);
    const float spread        = p.get (P::unison_spread);
    const int waveA           = juce::jlimit (0, WavetableBank::kNumWaves - 1, p.geti (P::oscA_wave));
    const int waveB           = juce::jlimit (0, WavetableBank::kNumWaves - 1, p.geti (P::oscB_wave));
    const float levelA        = p.get (P::oscA_level);
    const float baseLevelB    = p.get (P::oscB_level);
    const float subLevel      = p.get (P::sub_level);
    const float baseNoise     = p.get (P::noise_level);
    const float semisA        = (float) p.geti (P::oscA_coarse) + p.get (P::oscA_fine) / 100.0f;
    const float baseSemisB    = (float) p.geti (P::oscB_coarse) + p.get (P::oscB_fine) / 100.0f;
    const int   lfoShape[2]   = { p.geti (P::lfo1_shape), p.geti (P::lfo2_shape) };
    const float lfoRate[2]    = { p.get (P::lfo1_rate),   p.get (P::lfo2_rate) };
    const float unisonComp    = 1.0f / std::sqrt ((float) unison);

    int   slotSource[kNumModSlots], slotDest[kNumModSlots];
    float slotAmount[kNumModSlots];
    for (int i = 0; i < kNumModSlots; ++i)
    {
        slotSource[i] = p.geti (modSourceParam (i));
        slotDest[i]   = p.geti (modDestParam (i));
        slotAmount[i] = p.get (modAmountParam (i));
    }

    float gainL[kMaxUnison], gainR[kMaxUnison], detuneRatio[kMaxUnison];
    for (int u = 0; u < unison; ++u)
    {
        const float offset = unison > 1 ? -1.0f + 2.0f * (float) u / (float) (unison - 1) : 0.0f;
        const float pan    = offset * spread;
        gainL[u] = unison > 1 ? std::sqrt (0.5f * (1.0f - pan)) * juce::MathConstants<float>::sqrt2 : 1.0f;
        gainR[u] = unison > 1 ? std::sqrt (0.5f * (1.0f + pan)) * juce::MathConstants<float>::sqrt2 : 1.0f;
        detuneRatio[u] = std::exp2 (offset * detuneCents / 1200.0f);
    }

    // setMode() resets the filter state, so only call it on a real change.
    const auto wantedMode = filterModeFor (p.geti (P::filter_type));
    if (wantedMode != filterMode)
    {
        filterMode = wantedMode;
        filter.setMode (filterMode);
    }
    filter.setDrive (juce::jmax (1.0f, p.get (P::filter_drive)));

    auto* scratchL = scratch.getWritePointer (0);
    auto* scratchR = scratch.getWritePointer (1);
    const int numOut = out.getNumChannels();

    int pos = startSample;
    int remaining = numSamples;

    while (remaining > 0)
    {
        const int n = juce::jmin (remaining, kSub);

        // ---- modulation sources, held constant for this sub-block ----------
        float envF = 0.0f, envM = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            envF = filterEnv.getNextSample();
            envM = modEnv.getNextSample();
        }

        float lfo[2];
        for (int l = 0; l < 2; ++l)
        {
            lfo[l] = lfoValue (lfoShape[l], lfoPhase[l], lfoHeld[l]);
            const float rate = lfoRate[l] * std::exp2 (juce::jlimit (-4.0f, 4.0f, lfoRateMod[l]));
            lfoPhase[l] += rate * (float) n / fsr;
            if (lfoPhase[l] >= 1.0f)
            {
                lfoPhase[l] = wrap01 (lfoPhase[l]);
                lfoHeld[l]  = rng.nextFloat() * 2.0f - 1.0f;
            }
        }

        // ---- the matrix ------------------------------------------------------
        float mod[kNumModDests] {};
        float ampGain = 1.0f;
        for (int i = 0; i < kNumModSlots; ++i)
        {
            const int src = slotSource[i], dst = slotDest[i];
            const float amount = slotAmount[i];
            if (src == SrcOff || dst == DestOff || std::abs (amount) < 1.0e-4f)
                continue;

            const float s = sourceValue (src, lfo[0], lfo[1], envF, envM);
            if (dst == DestAmp)
            {
                // Attenuation style: velocity/envelope at full amount scale the
                // level from silence to full; an LFO becomes tremolo.
                const bool bipolar = src == SrcLfo1 || src == SrcLfo2 || src == SrcKey || src == SrcRandom;
                float u = bipolar ? 0.5f * (s + 1.0f) : s;
                if (amount < 0.0f) u = 1.0f - u;
                ampGain *= 1.0f - std::abs (amount) * (1.0f - juce::jlimit (0.0f, 1.0f, u));
            }
            else if (dst > 0 && dst < kNumModDests)
            {
                mod[dst] += amount * s;
            }
        }
        lfoRateMod[0] = mod[DestLfo1Rate] * 4.0f;
        lfoRateMod[1] = mod[DestLfo2Rate] * 4.0f;

        const float pitchMod  = juce::jlimit (-24.0f, 24.0f, mod[DestPitch] * 12.0f);
        const float semisB    = baseSemisB + juce::jlimit (-24.0f, 24.0f, mod[DestPitchB] * 12.0f);
        const float filterMod = mod[DestFilter] * 5.0f;  // octaves
        const float resonance = juce::jlimit (0.0f, 1.0f, p.get (P::filter_res) + mod[DestResonance]);
        const float morphA    = juce::jlimit (0.0f, 1.0f, p.get (P::oscA_morph) + mod[DestMorphA]);
        const float morphB    = juce::jlimit (0.0f, 1.0f, p.get (P::oscB_morph) + mod[DestMorphB]);
        const float fm        = juce::jlimit (0.0f, 1.0f, p.get (P::fm_amount) + mod[DestFM]);
        const float fmDepth   = fm * fm * 2.0f;          // cycles of phase excursion
        const float levelB    = juce::jlimit (0.0f, 1.0f, baseLevelB + mod[DestBLevel]);
        const float noiseLevel = juce::jlimit (0.0f, 1.0f, baseNoise + mod[DestNoise]) * 0.5f;
        const float pan       = juce::jlimit (-1.0f, 1.0f, mod[DestPan]);

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
                const float sB = bank.read (waveB, mipB, morphB, phaseB[u]);
                const float pa = wrap01 (phaseA[u] + fmDepth * sB);
                const float sA = bank.read (waveA, mipA, morphA, pa);
                const float s  = sA * levelA + sB * levelB;
                l += s * gainL[u];
                r += s * gainR[u];

                phaseA[u] += incA[u]; if (phaseA[u] >= 1.0f) phaseA[u] -= 1.0f;
                phaseB[u] += incB[u]; if (phaseB[u] >= 1.0f) phaseB[u] -= 1.0f;
            }

            const float mono = std::sin (twoPi * subPhase) * subLevel
                             + (rng.nextFloat() * 2.0f - 1.0f) * noiseLevel;
            subPhase += subInc; if (subPhase >= 1.0f) subPhase -= 1.0f;

            scratchL[i] = l * unisonComp + mono;
            scratchR[i] = r * unisonComp + mono;
        }

        // ---- filter --------------------------------------------------------
        const float keyTrack = p.get (P::filter_keytrack) * (currentNote - 60.0f) / 12.0f;
        float cutoff = p.get (P::filter_cutoff) * std::exp2 (envF * p.get (P::filter_env) + filterMod + keyTrack);
        cutoff = juce::jlimit (20.0f, juce::jmin (20000.0f, fsr * 0.45f), cutoff);
        filter.setCutoffFrequencyHz (cutoff);
        filter.setResonance (resonance);

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

        if (! ampEnv.isActive())
        {
            clearCurrentNote();
            filter.reset();
            break;
        }
    }
}

} // namespace stacks
