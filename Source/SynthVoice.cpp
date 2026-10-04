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
}

void SynthVoice::updateEnvelopes (const SynthParams& p)
{
    ampEnv.setParameters ({ p.get (P::aenv_attack), p.get (P::aenv_decay),
                            p.get (P::aenv_sustain), p.get (P::aenv_release) });
    filterEnv.setParameters ({ p.get (P::fenv_attack), p.get (P::fenv_decay),
                               p.get (P::fenv_sustain), p.get (P::fenv_release) });
}

void SynthVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    const auto& p = *ctx.params;

    velocityGain = 0.25f + 0.75f * velocity;
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

    updateEnvelopes (p);
    filter.reset();
    ampEnv.noteOn();
    filterEnv.noteOn();
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnv.noteOff();
        filterEnv.noteOff();
    }
    else
    {
        clearCurrentNote();
        ampEnv.reset();
        filterEnv.reset();
    }
}

void SynthVoice::pitchWheelMoved (int newPitchWheelValue)
{
    pitchBendSemis = (float) (newPitchWheelValue - 8192) / 8192.0f * 2.0f;
}

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
    const float levelB        = p.get (P::oscB_level);
    const float subLevel      = p.get (P::sub_level);
    const float noiseLevel    = p.get (P::noise_level) * 0.5f;
    const float semisA        = (float) p.geti (P::oscA_coarse) + p.get (P::oscA_fine) / 100.0f;
    const float semisB        = (float) p.geti (P::oscB_coarse) + p.get (P::oscB_fine) / 100.0f;
    const int   lfoShape[2]   = { p.geti (P::lfo1_shape),  p.geti (P::lfo2_shape) };
    const float lfoRate[2]    = { p.get (P::lfo1_rate),    p.get (P::lfo2_rate) };
    const float lfoAmt[2]     = { p.get (P::lfo1_amount),  p.get (P::lfo2_amount) };
    const int   lfoDest[2]    = { p.geti (P::lfo1_dest),   p.geti (P::lfo2_dest) };
    const float unisonComp    = 1.0f / std::sqrt ((float) unison);

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
    filter.setResonance (juce::jlimit (0.0f, 1.0f, p.get (P::filter_res)));
    filter.setDrive (juce::jmax (1.0f, p.get (P::filter_drive)));

    auto* scratchL = scratch.getWritePointer (0);
    auto* scratchR = scratch.getWritePointer (1);
    const int numOut = out.getNumChannels();

    int pos = startSample;
    int remaining = numSamples;

    while (remaining > 0)
    {
        const int n = juce::jmin (remaining, kSub);

        // ---- modulation, held constant for this sub-block ----------------
        float mod[8] {};     // summed LFO output per destination, scaled by amount
        float amtSum[8] {};  // summed amounts per destination (for tremolo depth)
        for (int l = 0; l < 2; ++l)
        {
            const int dest = juce::jlimit (0, 7, lfoDest[l]);
            if (dest != DestOff)
            {
                mod[dest]    += lfoValue (lfoShape[l], lfoPhase[l], lfoHeld[l]) * lfoAmt[l];
                amtSum[dest] += lfoAmt[l];
            }
            lfoPhase[l] += lfoRate[l] * (float) n / fsr;
            if (lfoPhase[l] >= 1.0f)
            {
                lfoPhase[l] = wrap01 (lfoPhase[l]);
                lfoHeld[l]  = rng.nextFloat() * 2.0f - 1.0f;
            }
        }

        const float pitchMod  = mod[DestPitch] * std::abs (mod[DestPitch]) * 12.0f; // square law: small = vibrato
        const float filterMod = mod[DestFilter] * 4.0f;                              // octaves
        const float morphA    = juce::jlimit (0.0f, 1.0f, p.get (P::oscA_morph) + 0.5f * mod[DestMorphA]);
        const float morphB    = juce::jlimit (0.0f, 1.0f, p.get (P::oscB_morph) + 0.5f * mod[DestMorphB]);
        const float fm        = juce::jlimit (0.0f, 1.0f, p.get (P::fm_amount) + 0.5f * mod[DestFM]);
        const float fmDepth   = fm * fm * 2.0f;                                      // cycles of phase excursion
        const float ampGain   = juce::jlimit (0.0f, 1.0f, 1.0f - 0.5f * amtSum[DestAmp] + 0.5f * mod[DestAmp]);
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
        float fenvValue = 0.0f;
        for (int i = 0; i < n; ++i)
            fenvValue = filterEnv.getNextSample();

        const float keyTrack = p.get (P::filter_keytrack) * (currentNote - 60.0f) / 12.0f;
        float cutoff = p.get (P::filter_cutoff) * std::exp2 (fenvValue * p.get (P::filter_env) + filterMod + keyTrack);
        cutoff = juce::jlimit (20.0f, juce::jmin (20000.0f, fsr * 0.45f), cutoff);
        filter.setCutoffFrequencyHz (cutoff);

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
