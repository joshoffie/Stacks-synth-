#include "Effects.h"

#include <cmath>

namespace stacks
{

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
    inline float softSat (float x) noexcept { return std::tanh (x); }
}

//==============================================================================
void ChorusFx::prepare (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& l : line)
        l.prepare ((int) (0.06 * sr) + 8);
    reset();
}

void ChorusFx::reset()
{
    for (auto& l : line) l.clear();
    for (auto& t : tone) t.reset();
    lastWet[0] = lastWet[1] = 0.0f;
    phase = fastPhase = 0.0f;
}

void ChorusFx::process (juce::AudioBuffer<float>& buffer, const Params& prm)
{
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    // Character presets per mode
    float centreMs = 7.0f, depthMs = 3.0f * prm.depth, rate = prm.rate, fastDepthMs = 0.0f, channelOffset = 0.25f * prm.spread;
    int voices = juce::jlimit (1, 4, prm.voices);
    float feedback = juce::jlimit (-0.9f, 0.9f, prm.feedback);
    bool invertRight = false;
    float dryGain = 1.0f - 0.3f * prm.mix;

    switch (prm.mode)
    {
        case 1: // Ensemble: three slow voices plus a fast shimmer, like a string machine
            centreMs = 12.0f; depthMs = 4.0f * prm.depth; rate = prm.rate * 0.6f; fastDepthMs = 0.25f * prm.depth;
            voices = juce::jmax (3, voices); feedback *= 0.5f;
            break;
        case 2: // Flanger: very short delay, feedback does the talking
            centreMs = 1.5f; depthMs = 1.2f * prm.depth; dryGain = 1.0f;
            break;
        case 3: // Dimension: two voices moving in opposite directions, wide and subtle
            centreMs = 5.0f; depthMs = 1.5f * prm.depth; voices = 2; invertRight = true; channelOffset = 0.0f; dryGain = 1.0f;
            break;
        default: break;
    }

    const float cTone = onePoleCoef (prm.toneHz, sr);
    const float msToSamples = (float) sr / 1000.0f;
    const float voiceGain = 1.0f / std::sqrt ((float) voices);
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        phase += rate / (float) sr;        if (phase >= 1.0f) phase -= 1.0f;
        fastPhase += 6.1f / (float) sr;    if (fastPhase >= 1.0f) fastPhase -= 1.0f;
        const float fast = fastDepthMs * std::sin (twoPi * fastPhase);

        for (int ch = 0; ch < 2; ++ch)
        {
            const float x = ch == 0 ? L[i] : R[i];
            float wet = 0.0f;
            for (int v = 0; v < voices; ++v)
            {
                float ph = phase + (float) v / (float) voices + (ch == 1 ? channelOffset : 0.0f);
                ph -= std::floor (ph);
                float lfo = std::sin (twoPi * ph);
                if (invertRight && ch == 1) lfo = -lfo;
                const float delayMs = juce::jmax (0.3f, centreMs + depthMs * lfo + fast);
                wet += line[ch].read (delayMs * msToSamples);
            }
            wet = tone[ch].lp (wet * voiceGain, cTone);
            lastWet[ch] = wet;
            line[ch].write (softSat (x + feedback * wet));

            const float out = x * dryGain + wet * prm.mix;
            if (ch == 0) L[i] = out; else R[i] = out;
        }
    }
}

//==============================================================================
float DelayFx::timeForSync (int sync, double bpm, float freeTimeSec) noexcept
{
    static const float beats[] = { 0.0f, 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f }; // Free, 1/16, 1/8T, 1/8, 1/8D, 1/4, 1/4D, 1/2
    if (sync <= 0 || sync >= (int) (sizeof (beats) / sizeof (beats[0])))
        return freeTimeSec;
    const float quarter = (float) (60.0 / juce::jlimit (20.0, 300.0, bpm));
    return beats[sync] * quarter;
}

void DelayFx::prepare (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& l : line)
        l.prepare ((int) (2.2 * sr) + 64);
    timeSmoothed.reset (sr, 0.05);
    timeSmoothed.setCurrentAndTargetValue (0.375f);
    lastMode = -1;
    reset();
}

void DelayFx::reset()
{
    for (auto& l : line) l.clear();
    for (auto& f : lp) f.reset();
    for (auto& f : hp) f.reset();
    fb[0] = fb[1] = 0.0f;
    wowPhase = flutterPhase = 0.0f;
}

void DelayFx::process (juce::AudioBuffer<float>& buffer, const Params& prm, double bpm)
{
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    if (prm.mode != lastMode)
    {
        // Tape slews slowly between times (audible pitch bend); the others snap quickly.
        const float current = timeSmoothed.getCurrentValue();
        timeSmoothed.reset (sr, prm.mode == 2 ? 0.4 : 0.05);
        timeSmoothed.setCurrentAndTargetValue (current);
        lastMode = prm.mode;
    }

    const float time = juce::jlimit (0.02f, 2.0f, timeForSync (prm.sync, bpm, prm.timeSec));
    timeSmoothed.setTargetValue (time);

    const bool tape = prm.mode == 2, pingPong = prm.mode == 1;
    const float cLp = onePoleCoef (tape ? prm.toneHz * 0.6f : prm.toneHz, sr);
    const float cHp = onePoleCoef (prm.hpfHz, sr);
    const float wowMs = prm.wow * (tape ? 2.5f : 1.2f);
    const float widthRatio = pingPong ? 1.0f : lerp (1.0f, tape ? 0.85f : 0.667f, prm.width);
    const float msToSamples = (float) sr / 1000.0f;
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        wowPhase += 0.45f / (float) sr;      if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flutterPhase += 6.3f / (float) sr;   if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
        const float wobble = wowMs * (std::sin (twoPi * wowPhase) + 0.12f * std::sin (twoPi * flutterPhase)) * msToSamples;

        const float tSamples = timeSmoothed.getNextValue() * (float) sr;
        const float dL = tSamples + wobble;
        const float dR = tSamples * widthRatio - wobble * 0.7f;

        const float outL = line[0].read (dL);
        const float outR = line[1].read (dR);

        float fL = lp[0].lp (hp[0].hp (outL, cHp), cLp);
        float fR = lp[1].lp (hp[1].hp (outR, cHp), cLp);
        if (tape)
        {
            fL = softSat (fL * 1.4f) / 1.4f;
            fR = softSat (fR * 1.4f) / 1.4f;
        }

        const float xL = L[i], xR = R[i];
        if (pingPong)
        {
            const float mono = 0.5f * (xL + xR);
            line[0].write (mono + prm.feedback * fR);
            line[1].write (fL);
        }
        else
        {
            line[0].write (xL + prm.feedback * fL);
            line[1].write (xR + prm.feedback * fR);
        }

        L[i] = xL + prm.mix * outL;
        R[i] = xR + prm.mix * outR;
    }
}

//==============================================================================
float ReverbFx::PitchShifter::process (float x, float ratio) noexcept
{
    phase += (ratio - 1.0f) / window;
    phase -= std::floor (phase);
    float phase2 = phase + 0.5f;
    phase2 -= std::floor (phase2);

    const float d1 = 1.0f + (1.0f - phase)  * (window - 2.0f);
    const float d2 = 1.0f + (1.0f - phase2) * (window - 2.0f);
    const float g1 = std::sin (juce::MathConstants<float>::pi * phase);
    const float g2 = std::sin (juce::MathConstants<float>::pi * phase2);

    const float out = g1 * d.read (d1) + g2 * d.read (d2);
    d.write (x);
    return out;
}

void ReverbFx::prepare (double sampleRate, int)
{
    sr = sampleRate;
    scale = (float) (sr / 29761.0);
    const float maxSize = 1.8f;

    predelay.prepare ((int) (0.25 * sr) + 8);

    // Dattorro's lengths (in 29.8 kHz samples), scaled to our rate.
    const float inLens[4] = { 142, 107, 379, 277 };
    const float inG[4]    = { 0.75f, 0.75f, 0.625f, 0.625f };
    for (int k = 0; k < 4; ++k)
    {
        inDiff[k].len = inLens[k] * scale;
        inDiff[k].g = inG[k];
        inDiff[k].d.prepare ((int) (inLens[k] * scale) + 8);
    }

    const float modLens[2]  = { 672, 908 };
    const float tailLens[2] = { 1800, 2656 };
    const float d1Lens[2]   = { 4453, 4217 };
    const float d2Lens[2]   = { 3720, 3163 };
    for (int k = 0; k < 2; ++k)
    {
        apMod[k].g = -0.7f;
        apMod[k].d.prepare ((int) (modLens[k] * scale * maxSize) + 64);
        apTail[k].g = 0.5f;
        apTail[k].d.prepare ((int) (tailLens[k] * scale * maxSize) + 8);
        del1[k].prepare ((int) (d1Lens[k] * scale * maxSize) + 8);
        del2[k].prepare ((int) (d2Lens[k] * scale * maxSize) + 8);
    }

    shifter.window = (float) (int) (0.05 * sr);
    shifter.d.prepare ((int) shifter.window + 8);
    reset();
}

void ReverbFx::reset()
{
    predelay.clear();
    inLp.reset(); inHp.reset();
    for (auto& o : outLp) o.reset();
    for (auto& a : inDiff) a.d.clear();
    for (int k = 0; k < 2; ++k)
    {
        apMod[k].d.clear(); apTail[k].d.clear(); del1[k].clear(); del2[k].clear(); dampLp[k].reset();
        tankOut[k] = 0.0f; lfoPhase[k] = 0.0f;
    }
    lfoPhase[1] = 0.37f;
    shifter.d.clear();
    shifter.phase = 0.0f;
    shimmerFeedback = 0.0f;
}

void ReverbFx::process (juce::AudioBuffer<float>& buffer, const Params& prm)
{
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    // Flavour per type: tank size, decay range, extra damping, shimmer floor.
    float sizeF = 1.0f, decayMin = 0.3f, decayMax = 0.9f, dampBoost = 0.0f, shimmer = prm.shimmer;
    switch (prm.type)
    {
        case 0: sizeF = 0.55f; decayMin = 0.2f; decayMax = 0.72f; break;                        // Room
        case 2: sizeF = 1.6f;  decayMin = 0.45f; decayMax = 0.96f; dampBoost = 0.15f; break;    // Hall
        case 3: sizeF = 1.6f;  decayMin = 0.5f;  decayMax = 0.95f; shimmer = juce::jmax (0.5f, shimmer); break; // Shimmer
        default: break;                                                                          // Plate
    }
    sizeF = juce::jlimit (0.4f, 1.8f, sizeF);

    const float decay = lerp (decayMin, decayMax, juce::jlimit (0.0f, 1.0f, prm.size));
    const float dampHz = lerp (18000.0f, 1500.0f, juce::jlimit (0.0f, 1.0f, prm.damp + dampBoost));
    const float cDamp = onePoleCoef (dampHz, sr);
    const float cLow  = onePoleCoef (prm.lowCutHz, sr);
    const float cHigh = onePoleCoef (prm.highCutHz, sr);
    const float cBand = 0.9995f;
    const float preSamples = juce::jmax (1.0f, prm.predelayMs * (float) sr / 1000.0f);
    const float excursion = prm.mod * 16.0f * scale;
    const float width = juce::jlimit (0.0f, 1.0f, prm.width);
    const float shimmerGain = juce::jlimit (0.0f, 1.0f, shimmer) * 0.45f;

    const float s = scale * sizeF;
    const float modLen[2]  = { 672 * s, 908 * s };
    const float tailLen[2] = { 1800 * s, 2656 * s };
    const float d1Len[2]   = { 4453 * s, 4217 * s };
    const float d2Len[2]   = { 3720 * s, 3163 * s };
    for (int k = 0; k < 2; ++k)
    {
        apMod[k].len = modLen[k];
        apTail[k].len = tailLen[k];
    }

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        // Input conditioning
        float in = 0.5f * (L[i] + R[i]);
        in = inHp.hp (in, cLow);
        in = inLp.lp (in, cBand);

        const float pre = predelay.read (preSamples);
        predelay.write (in + shimmerFeedback);

        float x = pre;
        for (auto& a : inDiff)
            x = a.process (x, 0.0f);

        // Tank: two cross-coupled halves
        for (int k = 0; k < 2; ++k)
        {
            lfoPhase[k] += (k == 0 ? 1.0f : 1.17f) / (float) sr;
            if (lfoPhase[k] >= 1.0f) lfoPhase[k] -= 1.0f;
        }
        float next[2];
        for (int k = 0; k < 2; ++k)
        {
            const float feed = softSat (x + decay * tankOut[1 - k]);
            const float a = apMod[k].process (feed, excursion * std::sin (twoPi * lfoPhase[k]));
            const float d1 = del1[k].read (d1Len[k]);
            del1[k].write (a);
            const float damped = dampLp[k].lp (d1, cDamp) * decay;
            const float t = apTail[k].process (damped, 0.0f);
            next[k] = del2[k].read (d2Len[k]);
            del2[k].write (t);
        }
        tankOut[0] = next[0];
        tankOut[1] = next[1];

        // Output taps (Dattorro's table), scaled like the tank
        float yL = 0.6f * (  del1[1].read (266 * s) + del1[1].read (2974 * s) - apTail[1].d.read (1913 * s) + del2[1].read (1996 * s)
                           - del1[0].read (1990 * s) - apTail[0].d.read (187 * s) - del2[0].read (1066 * s));
        float yR = 0.6f * (  del1[0].read (353 * s) + del1[0].read (3627 * s) - apTail[0].d.read (1228 * s) + del2[0].read (2673 * s)
                           - del1[1].read (2111 * s) - apTail[1].d.read (335 * s) - del2[1].read (121 * s));

        yL = outLp[0].lp (yL, cHigh);
        yR = outLp[1].lp (yR, cHigh);

        // Shimmer: octave-up copy of the tail goes back in (and sparkles on top)
        if (shimmerGain > 0.0f)
        {
            const float shifted = shifter.process (0.5f * (yL + yR), 2.0f);
            shimmerFeedback = softSat (shimmerGain * shifted);
            yL += 0.3f * shimmerGain * shifted;
            yR += 0.3f * shimmerGain * shifted;
        }
        else
        {
            shimmerFeedback = 0.0f;
        }

        const float mid = 0.5f * (yL + yR), side = 0.5f * (yL - yR) * width;
        L[i] += prm.mix * (mid + side);
        R[i] += prm.mix * (mid - side);
    }
}

} // namespace stacks
