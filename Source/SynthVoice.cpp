#include "SynthVoice.h"

#include <cmath>

namespace stacks
{

namespace
{
    constexpr float kVoiceGain = 0.45f;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
    inline float wrap01 (float p) noexcept     { return p - std::floor (p); }
}

//==============================================================================
void Ladder::setMode (int type) noexcept
{
    type = juce::jlimit (0, 5, type);
    if (type == mode)
        return;
    mode = type;
    // The six responses as taps on the four stages (juce::dsp::LadderFilter's table), with its 1.2 output gain.
    static const float taps[6][5] = { { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f },      // LP12
                                      { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f },      // LP24
                                      { 1.0f, -2.0f, 1.0f, 0.0f, 0.0f },     // HP12
                                      { 1.0f, -4.0f, 6.0f, -4.0f, 1.0f },    // HP24
                                      { 0.0f, 1.0f, -1.0f, 0.0f, 0.0f },     // BP12
                                      { 0.0f, 0.0f, 1.0f, -2.0f, 1.0f } };   // BP24
    for (int i = 0; i < 5; ++i) A[i] = 1.2f * taps[type][i];
    comp = type == 2 || type == 3 ? 0.0f : 0.5f;
    reset();
}

void Ladder::prepare (double sr, int maxBlock)
{
    sampleRate = 2.0 * (sr > 0.0 ? sr : 48000.0);
    os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    os->initProcessing ((size_t) juce::jmax (1, maxBlock));
    reset();
}

Ladder::Coefs Ladder::coefsFor (float cutoffHz, float resonance, float drive) const noexcept
{
    Coefs c;
    c.a1 = std::exp (-twoPi * juce::jlimit (10.0f, 0.45f * (float) sampleRate, cutoffHz) / (float) sampleRate);
    c.res = 0.1f + 0.9f * juce::jlimit (0.0f, 1.0f, resonance);
    c.drive = juce::jmax (1.0f, drive);
    c.gain = std::pow (c.drive, -2.642f) * 0.6103f + 0.3903f;
    c.drive2 = c.drive * 0.04f + 0.96f;
    c.gain2 = std::pow (c.drive2, -2.642f) * 0.6103f + 0.3903f;
    return c;
}

void Ladder::process (float* L, float* R, int n, float cutoffHz, float resonance, float drive) noexcept
{
    if (os == nullptr || n <= 0 || n > kMaxBlock)
        return;
    const Coefs target = coefsFor (cutoffHz, resonance, drive);
    if (! primed) { last = target; primed = true; }
    float* ptrs[2] = { L, R };
    juce::dsp::AudioBlock<float> block (ptrs, 2, (size_t) n);
    auto up = os->processSamplesUp (block);
    const int m = (int) up.getNumSamples();
    float* ch[2] = { up.getChannelPointer (0), up.getChannelPointer (1) };
    const float inv = 1.0f / (float) juce::jmax (1, m);
    for (int i = 0; i < m; ++i)
    {
        const float t = (float) (i + 1) * inv;
        const float a1 = last.a1 + (target.a1 - last.a1) * t;
        const float res = last.res + (target.res - last.res) * t;
        const float gain = last.gain + (target.gain - last.gain) * t;
        const float gain2 = last.gain2 + (target.gain2 - last.gain2) * t;
        const float dr = last.drive + (target.drive - last.drive) * t;
        const float dr2 = last.drive2 + (target.drive2 - last.drive2) * t;
        const float g = 1.0f - a1, b0 = g * 0.76923076923f, b1 = g * 0.23076923076f;
        for (int c = 0; c < 2; ++c)
        {
            float* st = s[c];
            const float dx = gain * saturate (dr * ch[c][i]);
            const float a = dx + res * -4.0f * (gain2 * saturate (dr2 * st[4]) - dx * comp);
            const float b = b1 * st[0] + a1 * st[1] + b0 * a;
            const float cc = b1 * st[1] + a1 * st[2] + b0 * b;
            const float d = b1 * st[2] + a1 * st[3] + b0 * cc;
            const float e = b1 * st[3] + a1 * st[4] + b0 * d;
            st[0] = a; st[1] = b; st[2] = cc; st[3] = d; st[4] = e;
            ch[c][i] = a * A[0] + b * A[1] + cc * A[2] + d * A[3] + e * A[4];
        }
    }
    os->processSamplesDown (block);
    last = target;
}

//==============================================================================
SynthVoice::SynthVoice (VoiceContext& context) : ctx (context)
{
    if (ctx.randomSeed != 0) rng.setSeed ((juce::int64) ctx.randomSeed * 7919 + ++ctx.voiceCounter);
    else                     rng.setSeedRandomly();
    for (auto& f : filters) f.ladder.setMode (1);
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
    const float e = envScale;
    ampEnv.setParameters (p.get (P::aenv_attack) * e, p.get (P::aenv_decay) * e, p.get (P::aenv_sustain), p.get (P::aenv_release) * e);
    filterEnv.setParameters (p.get (P::fenv_attack) * e, p.get (P::fenv_decay) * e, p.get (P::fenv_sustain), p.get (P::fenv_release) * e);
    modEnv.setParameters (p.get (P::menv_attack) * e, p.get (P::menv_decay) * e, p.get (P::menv_sustain), p.get (P::menv_release) * e);
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

    // Every note starts the oscillators somewhere new, like free-running analog
    // oscillators do, so two notes never have the same attack and A against B
    // never sits in the same comb twice. Unison copies keep their even spread
    // around that start so the stack doesn't all hit one peak.
    const int unison = juce::jlimit (1, kMaxUnison, p.geti (P::unison_voices));
    const float startA = rng.nextFloat(), startB = rng.nextFloat(), startC = rng.nextFloat();
    for (int u = 0; u < kMaxUnison; ++u)
    {
        const float ph = unison > 1 ? (float) u / (float) unison : 0.0f;
        phaseA[u] = wrap01 (ph + startA);
        phaseB[u] = wrap01 (ph + startB);
        phaseC[u] = wrap01 (ph + startC);
    }
    subPhase = 0.0f;
    for (int k = 0; k < kNumLfos; ++k)
    {
        lfoNotePhase[k] = 0.0f;
        lfoHeld[k] = rng.nextFloat() * 2.0f - 1.0f;
    }

    envScale = 1.0f + 0.2f * p.get (P::char_env_var) * (rng.nextFloat() * 2.0f - 1.0f);   // circuits never time twice alike
    driftPitch = rng.nextFloat() * 2.0f - 1.0f;
    driftFilter = rng.nextFloat() * 2.0f - 1.0f;
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
// in for an empty slot). `mip` is fractional: two neighbouring band-limited
// levels are blended so a pitch sweep never steps in brightness.
float SynthVoice::readWave (int osc, int wave, float mip, float morph, float phase) const noexcept
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
    ladder.prepare (sampleRate, blockSize);
    for (auto& c : comb) c.prepare ((int) (sampleRate / 20.0) + 8);
    reset();
}

void SynthVoice::FilterUnit::reset() noexcept
{
    ladder.reset();
    for (auto& s : svfA) s.reset();
    for (auto& s : svfB) s.reset();
    for (auto& c : comb) c.clear();
    lastCutoff = -1.0f;
}

void SynthVoice::FilterUnit::process (int type, int model, float cutoff, float res, float drive, juce::AudioBuffer<float>& buffer, int n, float fsr) noexcept
{
    const float from = lastCutoff > 0.0f ? lastCutoff : cutoff;
    lastCutoff = cutoff;

    // Character: Warm and Diode saturate the input and push the ladder; Smooth
    // swaps the ladder for a soft state-variable filter.
    if (model == 1 || model == 2)
    {
        const float k = model == 1 ? 1.5f : 2.6f, comp = model == 1 ? 1.25f : 1.7f;
        for (int c = 0; c < 2; ++c)
        {
            float* x = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i) x[i] = Ladder::saturate (x[i] * k) / comp;
        }
        if (model == 2) { drive = juce::jmax (drive, 2.5f); res = juce::jmin (1.0f, res * 1.15f); }
        else            { drive = juce::jmax (drive, 1.2f); }
    }
    if (model == 3 && type <= 5)
    {
        const float g0 = std::tan (juce::MathConstants<float>::pi * juce::jmin (from, fsr * 0.45f) / fsr);
        const float g1 = std::tan (juce::MathConstants<float>::pi * juce::jmin (cutoff, fsr * 0.45f) / fsr);
        const float kd = 2.0f - 1.85f * juce::jlimit (0.0f, 1.0f, res);
        const bool twoPole24 = (type % 2) == 1;
        const float inv = 1.0f / (float) juce::jmax (1, n);
        for (int c = 0; c < 2; ++c)
        {
            float* x = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float g = g0 + (g1 - g0) * (float) (i + 1) * inv;
                svfA[c].process (x[i], g, kd);
                float y = type < 2 ? svfA[c].lp : type < 4 ? svfA[c].hp : svfA[c].bp;
                if (twoPole24)
                {
                    svfB[c].process (y, g, kd);
                    y = type < 2 ? svfB[c].lp : type < 4 ? svfB[c].hp : svfB[c].bp;
                }
                x[i] = y;
            }
        }
        return;
    }
    if (type <= 5)
    {
        ladder.setMode (type);
        ladder.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, cutoff, res, drive);
    }
    else
        processExtra (type, from, cutoff, res, buffer, n, fsr);
}

// Notch (a hole at the cutoff), Comb (a resonance at the cutoff's pitch) and
// Formant (two vowel peaks; the cutoff sweeps A-E-I-O-U). The cutoff ramps
// from `cutoffFrom` to `cutoffTo` over the block.
void SynthVoice::FilterUnit::processExtra (int type, float cutoffFrom, float cutoffTo, float res, juce::AudioBuffer<float>& buffer, int n, float fsr) noexcept
{
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    float* ch[2] = { L, R };
    const float inv = 1.0f / (float) juce::jmax (1, n);

    if (type == 6)   // Notch
    {
        const float g0 = std::tan (juce::MathConstants<float>::pi * juce::jmin (cutoffFrom, fsr * 0.45f) / fsr);
        const float g1 = std::tan (juce::MathConstants<float>::pi * juce::jmin (cutoffTo, fsr * 0.45f) / fsr);
        const float k = 1.0f / (0.7f + res * 8.0f);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                svfA[c].process (ch[c][i], g0 + (g1 - g0) * (float) (i + 1) * inv, k);
                ch[c][i] = svfA[c].lp + svfA[c].hp;
            }
        return;
    }
    if (type == 7)   // Comb
    {
        const float d0 = juce::jlimit (2.0f, fsr / 20.0f, fsr / juce::jmax (20.0f, cutoffFrom));
        const float d1 = juce::jlimit (2.0f, fsr / 20.0f, fsr / juce::jmax (20.0f, cutoffTo));
        const float fb = res * 0.93f;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float z = comb[c].read (d0 + (d1 - d0) * (float) (i + 1) * inv);
                const float y = ch[c][i] + fb * z;
                comb[c].write (y);
                ch[c][i] = 0.5f * (ch[c][i] + y);
            }
        return;
    }
    // Formant: F1/F2 of A, E, I, O, U, swept by the cutoff on a log scale.
    static const float f1[] = { 730.0f, 530.0f, 390.0f, 570.0f, 440.0f };
    static const float f2[] = { 1090.0f, 1840.0f, 1990.0f, 840.0f, 1020.0f };
    auto formants = [&] (float cutoff, float& ga, float& gb)
    {
        const float pos = juce::jlimit (0.0f, 3.999f, std::log2 (juce::jmax (1.0f, cutoff / 150.0f)) / std::log2 (20000.0f / 150.0f) * 4.0f);
        const int v0 = (int) pos; const float t = pos - (float) v0; const int v1 = juce::jmin (4, v0 + 1);
        const float fa = f1[v0] + (f1[v1] - f1[v0]) * t, fb2 = f2[v0] + (f2[v1] - f2[v0]) * t;
        ga = std::tan (juce::MathConstants<float>::pi * juce::jmin (fa, fsr * 0.45f) / fsr);
        gb = std::tan (juce::MathConstants<float>::pi * juce::jmin (fb2, fsr * 0.45f) / fsr);
    };
    float ga0, gb0, ga1, gb1;
    formants (cutoffFrom, ga0, gb0);
    formants (cutoffTo, ga1, gb1);
    const float k = 1.0f / (2.0f + res * 16.0f);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) (i + 1) * inv;
            svfA[c].process (ch[c][i], ga0 + (ga1 - ga0) * t, k);
            svfB[c].process (ch[c][i], gb0 + (gb1 - gb0) * t, k);
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
        case SrcSeq:        return ctx.seqValue;
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
        // Analog drift: a slow random walk per voice, a few cents and a breath of filter.
        {
            driftPitch += 0.03f * ((rng.nextFloat() * 2.0f - 1.0f) - driftPitch);
            driftFilter += 0.02f * ((rng.nextFloat() * 2.0f - 1.0f) - driftFilter);
        }
        const float note   = currentNote + retune + pitchBendSemis + pitchMod + driftPitch * 0.08f * p.get (P::char_drift);
        const float baseHz = midiToHz (note);
        const float hzA    = baseHz * std::exp2 (semisA / 12.0f);
        const float hzB    = baseHz * std::exp2 (semisB / 12.0f);
        const float hzC    = baseHz * std::exp2 (semisC / 12.0f);
        const float mipA   = Tables::levelForF (hzA * (1.0f + 3.0f * fm) * warpRateFactor (warpA, warpAmtA), sr); // FM and Sync widen A's spectrum
        const float mipB   = Tables::levelForF (hzB * warpRateFactor (warpB, warpAmtB), sr);
        const float mipC   = Tables::levelForF (hzC * warpRateFactor (warpC, warpAmtC), sr);

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

        // ---- character: warmth (saturation before the filter) ----------------
        const float warmth = p.get (P::char_sat);
        if (warmth > 0.001f)
        {
            const float k = 1.0f + 3.0f * warmth, comp = 1.0f / (1.0f + 1.2f * warmth);
            for (auto* buf : { &scratch, &scratch2 })
                for (int c = 0; c < 2; ++c)
                {
                    float* x = buf->getWritePointer (c);
                    for (int i = 0; i < n; ++i) x[i] = Ladder::saturate (x[i] * k) * comp;
                }
        }

        // ---- filters -------------------------------------------------------
        const float keyTrack = p.get (P::filter_keytrack) * (currentNote - 60.0f) / 12.0f + driftFilter * 0.3f * p.get (P::char_drift);
        const float maxHz    = juce::jmin (20000.0f, fsr * 0.45f);
        const float cutoff   = juce::jlimit (20.0f, maxHz, p.get (P::filter_cutoff)  * std::exp2 (envF * p.get (P::filter_env)  + keyTrack));
        const float cutoff2  = juce::jlimit (20.0f, maxHz, p.get (P::filter2_cutoff) * std::exp2 (envF * p.get (P::filter2_env) + keyTrack));
        const int type1 = p.geti (P::filter_type), type2 = p.geti (P::filter2_type);
        const float res1 = juce::jlimit (0.0f, 1.0f, p.get (P::filter_res)),  drive1 = juce::jmax (1.0f, p.get (P::filter_drive));
        const float res2 = juce::jlimit (0.0f, 1.0f, p.get (P::filter2_res)), drive2 = juce::jmax (1.0f, p.get (P::filter2_drive));

        if (routing == 2)   // Parallel: filter 2 gets its own copy of the whole mix
            for (int c = 0; c < 2; ++c)
                scratch2.copyFrom (c, 0, scratch, c, 0, n);

        const int model = p.geti (P::filter_model);
        filters[0].process (type1, model, cutoff, res1, drive1, scratch, n, fsr);
        if (routing == 1)
            filters[1].process (type2, model, cutoff2, res2, drive2, scratch, n, fsr);        // Series
        else if (routing >= 2)
        {
            filters[1].process (type2, model, cutoff2, res2, drive2, scratch2, n, fsr);       // Parallel / Split
            const float g = routing == 2 ? 0.7f : 1.0f;
            for (int c = 0; c < 2; ++c)
            {
                if (routing == 2) scratch.applyGain (c, 0, n, g);
                scratch.addFrom (c, 0, scratch2, c, 0, n, g);
            }
        }

        // ---- amplitude, pan, output ----------------------------------------
        // The gains this sub-block asks for are reached over its samples, from
        // where the last one left them, so a tremolo or a pan LFO never steps.
        const float gl = std::sqrt (0.5f * (1.0f - pan)) * juce::MathConstants<float>::sqrt2;
        const float gr = std::sqrt (0.5f * (1.0f + pan)) * juce::MathConstants<float>::sqrt2;
        const float targetL = velocityGain * ampGain * kVoiceGain * gl;
        const float targetR = velocityGain * ampGain * kVoiceGain * gr;
        if (! gainsPrimed) { lastGainL = targetL; lastGainR = targetR; gainsPrimed = true; }
        const float stepL = (targetL - lastGainL) / (float) n, stepR = (targetR - lastGainR) / (float) n;
        float gL = lastGainL, gR = lastGainR;
        auto* outL = out.getWritePointer (0, pos);
        auto* outR = numOut > 1 ? out.getWritePointer (1, pos) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            gL += stepL; gR += stepR;
            const float e = ampEnv.getNextSample();
            if (outR != nullptr)
            {
                outL[i] += scratchL[i] * e * gL;
                outR[i] += scratchR[i] * e * gR;
            }
            else
            {
                outL[i] += 0.5f * (scratchL[i] * gL + scratchR[i] * gR) * e;
            }
        }
        lastGainL = targetL;
        lastGainR = targetR;

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
