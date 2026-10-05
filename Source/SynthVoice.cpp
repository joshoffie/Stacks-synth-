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
    for (auto& f : filters) f.ladder.setMode (f.mode);
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

    for (auto& f : filters) f.prepare (newRate, kSub);
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

    // The Synthesiser set our channel before calling us: start with that
    // channel's current slide and pressure (an MPE controller sends them first).
    channel = 1;
    for (int ch = 1; ch <= 16; ++ch)
        if (isPlayingChannel (ch)) { channel = ch; break; }
    slide = ctx.channelSlide[channel - 1];
    aftertouch = ctx.channelPressure[channel - 1];

    // A microtuning master may exclude this key from its scale.
    if (ctx.mts != nullptr && MTS_ShouldFilterNote (ctx.mts, (char) midiNoteNumber, (signed char) (channel - 1)))
    {
        clearCurrentNote();
        return;
    }

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
        phaseC[u] = ph;
    }
    subPhase = 0.0f;
    for (int k = 0; k < kNumLfos; ++k)
    {
        lfoNotePhase[k] = 0.0f;
        lfoHeld[k] = rng.nextFloat() * 2.0f - 1.0f;
    }

    updateEnvelopes (p);
    for (auto& f : filters) f.reset();
    sampler.start (ctx.samples != nullptr ? ctx.samples->current() : nullptr, getSampleRate(), midiNoteNumber);
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
        sampler.stop();
        ampEnv.reset();
        filterEnv.reset();
        modEnv.reset();
    }
}

void SynthVoice::pitchWheelMoved (int newPitchWheelValue)
{
    bendNorm = (float) (newPitchWheelValue - 8192) / 8192.0f;
}

void SynthVoice::controllerMoved (int controllerNumber, int newValue)
{
    if (controllerNumber == 1)
        ctx.modWheel.store ((float) newValue / 127.0f);
    else if (controllerNumber == 74)   // MPE slide, per note
    {
        slide = (float) newValue / 127.0f;
        ctx.slide.store (slide);
    }
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

void SynthVoice::FilterUnit::prepare (double sampleRate, int blockSize)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2 };
    ladder.prepare (spec);
    for (auto& c : comb) c.prepare ((int) (sampleRate / 20.0) + 8);
    reset();
}

void SynthVoice::FilterUnit::reset() noexcept
{
    ladder.reset();
    for (auto& s : svfA) s.reset();
    for (auto& s : svfB) s.reset();
    for (auto& c : comb) c.clear();
}

void SynthVoice::FilterUnit::process (int type, float cutoff, float res, float drive, juce::AudioBuffer<float>& buffer, int n, float fsr) noexcept
{
    if (type <= 5)
    {
        const auto wanted = filterModeFor (type);
        if (wanted != mode)
        {
            mode = wanted;
            ladder.setMode (mode);   // resets state, so only on a real change
        }
        ladder.setCutoffFrequencyHz (cutoff);
        ladder.setResonance (res);
        ladder.setDrive (drive);
        juce::dsp::AudioBlock<float> block (buffer);
        auto subBlock = block.getSubBlock (0, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> context (subBlock);
        ladder.process (context);
    }
    else
        processExtra (type, cutoff, res, buffer, n, fsr);
}

// Notch (a hole at the cutoff), Comb (a resonance at the cutoff's pitch) and
// Formant (two vowel peaks; the cutoff sweeps A-E-I-O-U).
void SynthVoice::FilterUnit::processExtra (int type, float cutoff, float res, juce::AudioBuffer<float>& buffer, int n, float fsr) noexcept
{
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    float* ch[2] = { L, R };

    if (type == 6)   // Notch
    {
        const float g = std::tan (juce::MathConstants<float>::pi * cutoff / fsr);
        const float k = 1.0f / (0.7f + res * 8.0f);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                svfA[c].process (ch[c][i], g, k);
                ch[c][i] = svfA[c].lp + svfA[c].hp;
            }
        return;
    }
    if (type == 7)   // Comb
    {
        const float delaySamples = juce::jlimit (2.0f, fsr / 20.0f, fsr / cutoff);
        const float fb = res * 0.93f;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float z = comb[c].read (delaySamples);
                const float y = ch[c][i] + fb * z;
                comb[c].write (y);
                ch[c][i] = 0.5f * (ch[c][i] + y);
            }
        return;
    }
    // Formant: F1/F2 of A, E, I, O, U, swept by the cutoff on a log scale.
    static const float f1[] = { 730.0f, 530.0f, 390.0f, 570.0f, 440.0f };
    static const float f2[] = { 1090.0f, 1840.0f, 1990.0f, 840.0f, 1020.0f };
    const float pos = juce::jlimit (0.0f, 3.999f, std::log2 (juce::jmax (1.0f, cutoff / 150.0f)) / std::log2 (20000.0f / 150.0f) * 4.0f);
    const int v0 = (int) pos; const float t = pos - (float) v0; const int v1 = juce::jmin (4, v0 + 1);
    const float fa = f1[v0] + (f1[v1] - f1[v0]) * t, fb2 = f2[v0] + (f2[v1] - f2[v0]) * t;
    const float ga = std::tan (juce::MathConstants<float>::pi * juce::jmin (fa, fsr * 0.45f) / fsr);
    const float gb = std::tan (juce::MathConstants<float>::pi * juce::jmin (fb2, fsr * 0.45f) / fsr);
    const float k = 1.0f / (2.0f + res * 16.0f);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < n; ++i)
        {
            svfA[c].process (ch[c][i], ga, k);
            svfB[c].process (ch[c][i], gb, k);
            ch[c][i] = (svfA[c].bp + 0.7f * svfB[c].bp) * 2.2f;
        }
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
        case SrcAftertouch: return juce::jmax (aftertouch, ctx.mpe.load (std::memory_order_relaxed) ? ctx.masterPressure : 0.0f);
        case SrcSlide:      return slide;
        case SrcRandom:     return noteRandom;
        case SrcMacro1: case SrcMacro2: case SrcMacro3: case SrcMacro4: case SrcMacro5: case SrcMacro6:
                            return ctx.params->get (macroParam (source - SrcMacro1));   // the big knobs, shared by every voice
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
        const int warpA         = juce::jlimit (0, kNumWarpModes - 1, p.geti (P::oscA_warp));
        const int warpB         = juce::jlimit (0, kNumWarpModes - 1, p.geti (P::oscB_warp));
        const float warpAmtA    = juce::jlimit (0.0f, 1.0f, p.get (P::oscA_warp_amt));
        const float warpAmtB    = juce::jlimit (0.0f, 1.0f, p.get (P::oscB_warp_amt));
        const float uniMorph    = p.get (P::unison_morph);
        const int waveC         = juce::jlimit (0, kLastWave, p.geti (P::oscC_wave));
        const float levelC      = p.get (P::oscC_level);
        const float semisC      = (float) p.geti (P::oscC_coarse) + p.get (P::oscC_fine) / 100.0f;
        const float morphC      = juce::jlimit (0.0f, 1.0f, p.get (P::oscC_morph));
        const int warpC         = juce::jlimit (0, kNumWarpModes - 1, p.geti (P::oscC_warp));
        const float warpAmtC    = juce::jlimit (0.0f, 1.0f, p.get (P::oscC_warp_amt));
        const int routing       = juce::jlimit (0, 3, p.geti (P::filter_routing));   // 0 Off, 1 Series, 2 Parallel, 3 Split
        const bool split        = routing == 3;
        const bool useC         = levelC > 0.0001f;
        const float fmDepth     = fm * fm * 2.0f;        // cycles of phase excursion
        const float unisonComp  = 1.0f / std::sqrt ((float) unison);

        float gainL[kMaxUnison], gainR[kMaxUnison], detuneRatio[kMaxUnison], morphAu[kMaxUnison], morphBu[kMaxUnison], morphCu[kMaxUnison];
        for (int u = 0; u < unison; ++u)
        {
            const float offset = unison > 1 ? -1.0f + 2.0f * (float) u / (float) (unison - 1) : 0.0f;
            morphAu[u] = juce::jlimit (0.0f, 1.0f, morphA + 0.5f * uniMorph * offset);   // Uni Morph: each copy at its own table position
            morphBu[u] = juce::jlimit (0.0f, 1.0f, morphB + 0.5f * uniMorph * offset);
            morphCu[u] = juce::jlimit (0.0f, 1.0f, morphC + 0.5f * uniMorph * offset);
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

        // Pitch wheel: Bend Range on ordinary channels; in MPE mode a member
        // channel (2-16) bends 48 semitones per note and channel 1 bends everything.
        const bool mpe     = ctx.mpe.load (std::memory_order_relaxed);
        const bool member  = mpe && channel != 1;
        const float pitchBendSemis = bendNorm * (member ? 48.0f : (float) p.geti (P::bend_range)) + (member ? ctx.masterBend : 0.0f);
        // MTS-ESP: the master's retuning for the key being played (0 without a master).
        const float retune = ctx.mts != nullptr && MTS_HasMaster (ctx.mts)
                           ? (float) MTS_RetuningInSemitones (ctx.mts, (char) juce::jlimit (0, 127, (int) std::lround (currentNote)), (signed char) (channel - 1))
                           : 0.0f;
        const float note   = currentNote + retune + pitchBendSemis + pitchMod;
        const float baseHz = midiToHz (note);
        const float hzA    = baseHz * std::exp2 (semisA / 12.0f);
        const float hzB    = baseHz * std::exp2 (semisB / 12.0f);
        const float hzC    = baseHz * std::exp2 (semisC / 12.0f);
        const int mipA     = WavetableBank::levelFor (hzA * (1.0f + 3.0f * fm) * warpRateFactor (warpA, warpAmtA), sr); // FM and Sync widen A's spectrum
        const int mipB     = WavetableBank::levelFor (hzB * warpRateFactor (warpB, warpAmtB), sr);
        const int mipC     = WavetableBank::levelFor (hzC * warpRateFactor (warpC, warpAmtC), sr);

        float incA[kMaxUnison], incB[kMaxUnison], incC[kMaxUnison];
        for (int u = 0; u < unison; ++u)
        {
            incA[u] = hzA * detuneRatio[u] / fsr;
            incB[u] = hzB * detuneRatio[u] / fsr;
            incC[u] = hzC * detuneRatio[u] / fsr;
        }
        const float subInc = baseHz * 0.5f / fsr;

        // ---- oscillators -> the filter buses (pre-filter) -------------------
        // Bus 1 (scratch) feeds filter 1, bus 2 (scratch2) feeds filter 2. Only
        // Split routing separates them: A, sub and noise on bus 1, B and C on bus 2.
        auto* scratch2L = scratch2.getWritePointer (0);
        auto* scratch2R = scratch2.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            float l1 = 0.0f, r1 = 0.0f, l2 = 0.0f, r2 = 0.0f;
            for (int u = 0; u < unison; ++u)
            {
                const float sB = warpSample (warpB, readWave (1, waveB, mipB, morphBu[u], warpPhase (warpB, phaseB[u], warpAmtB)), warpAmtB);
                const float pa = warpPhase (warpA, wrap01 (phaseA[u] + fmDepth * sB), warpAmtA);
                const float sA = warpSample (warpA, readWave (0, waveA, mipA, morphAu[u], pa), warpAmtA);
                const float sC = useC ? warpSample (warpC, readWave (2, waveC, mipC, morphCu[u], warpPhase (warpC, phaseC[u], warpAmtC)), warpAmtC) : 0.0f;
                const float a  = sA * levelA;
                const float bc = sB * levelB + sC * levelC;
                if (split)
                {
                    l1 += a * gainL[u];  r1 += a * gainR[u];
                    l2 += bc * gainL[u]; r2 += bc * gainR[u];
                }
                else
                {
                    const float s = a + bc;
                    l1 += s * gainL[u];
                    r1 += s * gainR[u];
                }

                phaseA[u] = wrap01 (phaseA[u] + incA[u]);
                phaseB[u] = wrap01 (phaseB[u] + incB[u]);
                phaseC[u] = wrap01 (phaseC[u] + incC[u]);
            }

            const float mono = std::sin (twoPi * subPhase) * subLevel
                             + (rng.nextFloat() * 2.0f - 1.0f) * noiseLevel;
            subPhase += subInc; if (subPhase >= 1.0f) subPhase -= 1.0f;

            scratchL[i] = l1 * unisonComp + mono;
            scratchR[i] = r1 * unisonComp + mono;
            scratch2L[i] = l2 * unisonComp;
            scratch2R[i] = r2 * unisonComp;
        }

        // ---- the sample oscillator joins bus 1 ---------------------------------
        if (sampler.isActive() && p.geti (P::smp_mode) > 0)
        {
            SamplerParams sp;
            sp.mode = p.geti (P::smp_mode);
            sp.level = p.get (P::smp_level);
            sp.start = juce::jlimit (0.0f, 1.0f, p.get (P::smp_start));
            sp.loop = p.geti (P::smp_loop) == 1;
            sp.semitones = (float) p.geti (P::smp_coarse) + p.get (P::smp_fine) / 100.0f;
            sp.grainMs = p.get (P::grain_size);
            sp.grainsPerSecond = p.get (P::grain_rate);
            sp.spray = p.get (P::grain_spray);
            sp.pitchRand = p.get (P::grain_pitch);
            sp.spread = p.get (P::grain_spread);
            sampler.render (scratchL, scratchR, n, sp, note - (float) sampler.startedNote(), rng);
        }

        // ---- filters -------------------------------------------------------
        const float keyTrack = p.get (P::filter_keytrack) * (currentNote - 60.0f) / 12.0f;
        const float maxHz    = juce::jmin (20000.0f, fsr * 0.45f);
        const float cutoff   = juce::jlimit (20.0f, maxHz, p.get (P::filter_cutoff)  * std::exp2 (envF * p.get (P::filter_env)  + keyTrack));
        const float cutoff2  = juce::jlimit (20.0f, maxHz, p.get (P::filter2_cutoff) * std::exp2 (envF * p.get (P::filter2_env) + keyTrack));
        const int type1 = p.geti (P::filter_type), type2 = p.geti (P::filter2_type);
        const float res1 = juce::jlimit (0.0f, 1.0f, p.get (P::filter_res)),  drive1 = juce::jmax (1.0f, p.get (P::filter_drive));
        const float res2 = juce::jlimit (0.0f, 1.0f, p.get (P::filter2_res)), drive2 = juce::jmax (1.0f, p.get (P::filter2_drive));

        if (routing == 2)   // Parallel: filter 2 gets its own copy of the whole mix
            for (int c = 0; c < 2; ++c)
                scratch2.copyFrom (c, 0, scratch, c, 0, n);

        filters[0].process (type1, cutoff, res1, drive1, scratch, n, fsr);
        if (routing == 1)
            filters[1].process (type2, cutoff2, res2, drive2, scratch, n, fsr);        // Series
        else if (routing >= 2)
        {
            filters[1].process (type2, cutoff2, res2, drive2, scratch2, n, fsr);       // Parallel / Split
            const float g = routing == 2 ? 0.7f : 1.0f;
            for (int c = 0; c < 2; ++c)
            {
                if (routing == 2) scratch.applyGain (c, 0, n, g);
                scratch.addFrom (c, 0, scratch2, c, 0, n, g);
            }
        }

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
            for (auto& f : filters) f.reset();
            sampler.stop();
            break;
        }
    }
}

} // namespace stacks
