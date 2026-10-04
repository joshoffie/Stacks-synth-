#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <array>
#include <vector>

namespace stacks
{

// Fractional delay line, linear interpolation. Convention: read(N) *before*
// write(x[n]) returns x[n-N].
class FracDelay
{
public:
    void prepare (int maxSamples)
    {
        buf.assign ((size_t) juce::jmax (8, maxSamples + 4), 0.0f);
        w = 0;
    }
    void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); }

    void write (float x) noexcept
    {
        buf[(size_t) w] = x;
        if (++w >= (int) buf.size()) w = 0;
    }

    float read (float delay) const noexcept
    {
        const int size = (int) buf.size();
        delay = juce::jlimit (1.0f, (float) (size - 3), delay);
        const int di = (int) delay;
        const float frac = delay - (float) di;
        int i0 = w - di;     if (i0 < 0) i0 += size;
        int i1 = i0 - 1;     if (i1 < 0) i1 += size;
        return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

private:
    std::vector<float> buf;
    int w = 0;
};

struct OnePole
{
    float z = 0.0f;
    float lp (float x, float c) noexcept { z += c * (x - z); return z; }
    float hp (float x, float c) noexcept { return x - lp (x, c); }
    void reset() noexcept { z = 0.0f; }
};

inline float onePoleCoef (float hz, double sampleRate) noexcept
{
    return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jlimit (1.0f, (float) sampleRate * 0.45f, hz) / (float) sampleRate);
}

//==============================================================================
// Multi-voice chorus with Chorus / Ensemble / Flanger / Dimension characters.
class ChorusFx
{
public:
    struct Params
    {
        int mode = 0;            // 0 Chorus, 1 Ensemble, 2 Flanger, 3 Dimension
        float rate = 0.8f, depth = 0.3f, mix = 0.0f;
        int voices = 2;
        float feedback = 0.0f, spread = 0.7f, toneHz = 12000.0f;
    };

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (juce::AudioBuffer<float>&, const Params&);

private:
    double sr = 48000.0;
    FracDelay line[2];
    OnePole tone[2];
    float lastWet[2] {};
    float phase = 0.0f, fastPhase = 0.0f;
};

//==============================================================================
// Stereo / Ping-Pong / Tape delay with tempo sync and filtered feedback.
class DelayFx
{
public:
    struct Params
    {
        int mode = 0;            // 0 Stereo, 1 Ping-Pong, 2 Tape
        int sync = 0;            // 0 Free, then note values
        float timeSec = 0.375f, feedback = 0.4f, mix = 0.0f;
        float toneHz = 6000.0f, hpfHz = 120.0f, wow = 0.1f, width = 0.3f;
    };

    static float timeForSync (int sync, double bpm, float freeTimeSec) noexcept;

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (juce::AudioBuffer<float>&, const Params&, double bpm);

private:
    double sr = 48000.0;
    FracDelay line[2];
    OnePole lp[2], hp[2];
    juce::SmoothedValue<float> timeSmoothed;
    float fb[2] {};
    float wowPhase = 0.0f, flutterPhase = 0.0f;
    int lastMode = -1;
};

//==============================================================================
// Dattorro-style plate tank with Room / Plate / Hall / Shimmer flavours.
class ReverbFx
{
public:
    struct Params
    {
        int type = 1;            // 0 Room, 1 Plate, 2 Hall, 3 Shimmer
        float size = 0.5f, damp = 0.5f, mix = 0.15f;
        float predelayMs = 10.0f, lowCutHz = 100.0f, highCutHz = 10000.0f;
        float mod = 0.3f, shimmer = 0.0f, width = 1.0f;
    };

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (juce::AudioBuffer<float>&, const Params&);

private:
    struct Allpass
    {
        FracDelay d;
        float len = 1.0f, g = 0.5f;
        float process (float x, float modSamples) noexcept
        {
            const float z = d.read (len + modSamples);
            const float v = x + g * z;
            d.write (v);
            return z - g * v;
        }
    };

    // Two crossfaded taps sliding through a short window: a simple octave shifter.
    struct PitchShifter
    {
        FracDelay d;
        float phase = 0.0f, window = 2048.0f;
        float process (float x, float ratio) noexcept;
    };

    double sr = 48000.0;
    float scale = 1.0f;          // sample-rate scaling of Dattorro's 29.8 kHz lengths
    FracDelay predelay;
    OnePole inLp, inHp, outLp[2];
    Allpass inDiff[4];
    Allpass apMod[2], apTail[2];
    FracDelay del1[2], del2[2];
    OnePole dampLp[2];
    float tankOut[2] {};
    float lfoPhase[2] {};
    PitchShifter shifter;
    float shimmerFeedback = 0.0f;
};

} // namespace stacks

namespace stacks
{

//==============================================================================
// Oversampled waveshaper: Soft / Hard / Tube / Fold / Crush, with a tone
// control after it and a dry/wet mix.
class DistortionFx
{
public:
    struct Params { int mode = 0; float driveDb = 12.0f, toneHz = 8000.0f, mix = 0.0f; };

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (juce::AudioBuffer<float>&, const Params&);

private:
    double sr = 48000.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    OnePole tone[2], dc[2];
    float crushHeld[2] {};
    int crushCount[2] {};
    juce::AudioBuffer<float> dry;
};

// Three bands: low shelf, mid peak, high shelf.
class EqFx
{
public:
    struct Params { float lowGainDb = 0.0f, lowHz = 150.0f, midGainDb = 0.0f, midHz = 1200.0f, midQ = 1.0f, highGainDb = 0.0f, highHz = 6000.0f; };

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (juce::AudioBuffer<float>&, const Params&);

private:
    double sr = 48000.0;
    juce::dsp::IIR::Filter<float> low[2], mid[2], high[2];
    Params last;
    bool primed = false;
};

// Glue at the end of the chain, with parallel mix.
class CompressorFx
{
public:
    struct Params { float thresholdDb = -18.0f, ratio = 4.0f, attackMs = 10.0f, releaseMs = 150.0f, makeupDb = 0.0f, mix = 0.0f; };

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (juce::AudioBuffer<float>&, const Params&);

private:
    juce::dsp::Compressor<float> comp;
    juce::AudioBuffer<float> dry;
};

} // namespace stacks
