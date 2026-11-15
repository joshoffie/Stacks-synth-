#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "Wavetable.h"
#include "Effects.h"
#include "LfoTable.h"
#include "libMTSClient.h"
#include "Sampler.h"

namespace stacks
{

struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// Shared by every voice; owned by the processor.
struct VoiceContext
{
    const WavetableBank* bank = nullptr;
    const UserWavetables* user = nullptr;     // imported tables, "User 1-4"
    const SynthParams* params = nullptr;      // base (unmodulated) values for this block
    const LfoTableBank* lfoTables = nullptr;
    std::atomic<float> lastNote { -1.0f };    // most recent note, for glide
    std::atomic<float> modWheel { 0.0f };     // CC1, 0..1, shared by all voices
    std::atomic<float> aftertouch { 0.0f };   // last channel pressure, for global targets
    std::atomic<float> slide { 0.0f };        // last CC74 (MPE slide) on any channel, for global targets
    std::atomic<bool> mpe { false };          // MPE: channel 1 is the master, 2-16 carry one note each
    MTSClient* mts = nullptr;                 // MTS-ESP: retunes every note when a master is running
    const SampleBank* samples = nullptr;      // the sample oscillator's file
    float seqValue = 0.0f;                    // the step sequencer lane this block, -1..1
    float masterBend = 0.0f;                  // semitones from channel 1's wheel (MPE master), audio thread only
    float masterPressure = 0.0f;              // channel 1's pressure
    float channelSlide[16] {};                // per channel, so a note that starts after the controller moved begins right
    float channelPressure[16] {};
    std::atomic<float>* liveValues = nullptr;  // kNumParams, what the UI's markers show
    std::atomic<int> voiceCounter { 0 };       // hands out serial numbers at note-on
    int randomSeed = 0;                        // 0: every voice rolls its own dice (the plug-in); otherwise repeatable renders (the offline tools)
    std::atomic<int> displayVoice { -1 };      // the newest voice publishes its values
    double bpm = 120.0;
    float lfoRateHz[kNumLfos] {};             // effective rate (sync applied) for free-running LFOs
    float lfoBlockPhase[kNumLfos] {};         // free-running phase at the start of this block
    float lfoGlobalValue[kNumLfos] {};        // free-running value at block start (used for Random shape)
};

// Applies `delta` (fraction of knob travel) to a float parameter inside its own
// skewed range, so a cutoff moves musically and a 0..1 knob moves linearly.
inline void nudgeParam (SynthParams& p, int paramIndex, float delta) noexcept
{
    const auto& range = paramRange (paramIndex);
    float& v = p.v[paramIndex];
    const float norm = juce::jlimit (0.0f, 1.0f, range.convertTo0to1 (juce::jlimit (range.start, range.end, v)) + delta);
    v = range.convertFrom0to1 (norm);
}

// An ADSR with analog curves: the attack is a fast-then-easing rise, decay and
// release fall exponentially (a straight line in dB), the way a capacitor
// discharges and the way every classic synth's envelope sounds. A linear
// decay hangs at the top and then stops dead; this one is all tail. The decay
// time is where the curve has a tenth of the way left (-20 dB); the release
// time is where it is 26 dB down, and the voice is freed at -60 dB, about
// 2.3 times the release setting.
class Envelope
{
public:
    enum class Stage { idle, attack, decay, sustain, release };

    void setSampleRate (double sr) noexcept          { sampleRate = sr > 0.0 ? sr : 48000.0; }
    void setParameters (float attackSec, float decaySec, float sustainLevel, float releaseSec) noexcept
    {
        sustain = juce::jlimit (0.0f, 1.0f, sustainLevel);
        attackCoef  = coefFor (juce::jmax (0.0005f, attackSec) / 1.466f);   // reaches 1.0 at attackSec, aiming past it
        decayCoef   = coefFor (juce::jmax (0.001f, decaySec) / 2.3026f);    // -20 dB of the distance at decaySec
        releaseCoef = coefFor (juce::jmax (0.001f, releaseSec) / 3.0f);
    }
    void noteOn() noexcept                            { stage = Stage::attack; }   // from wherever the level is: no click on a retrigger
    void noteOff() noexcept                           { if (stage != Stage::idle) stage = Stage::release; }
    void reset() noexcept                             { stage = Stage::idle; level = 0.0f; }
    bool isActive() const noexcept                    { return stage != Stage::idle; }
    Stage currentStage() const noexcept               { return stage; }
    float value() const noexcept                      { return level; }

    float getNextSample() noexcept
    {
        switch (stage)
        {
            case Stage::attack:
                level += attackCoef * (1.3f - level);
                if (level >= 1.0f) { level = 1.0f; stage = Stage::decay; }
                break;
            case Stage::decay:
                level += decayCoef * (sustain - level);
                if (std::abs (level - sustain) < 1.0e-4f) { level = sustain; stage = Stage::sustain; }
                break;
            case Stage::sustain:
                level = sustain;
                if (level < 1.0e-4f) { level = 0.0f; stage = Stage::idle; }   // a pluck that has died frees its voice
                break;
            case Stage::release:
                level -= releaseCoef * level;
                if (level < 1.0e-3f) { level = 0.0f; stage = Stage::idle; }   // -60 dB: gone, the voice is free
                break;
            case Stage::idle:
            default:
                level = 0.0f;
                break;
        }
        return level;
    }

private:
    float coefFor (float tauSec) const noexcept { return 1.0f - std::exp (-1.0f / (tauSec * (float) sampleRate)); }

    double sampleRate = 48000.0;
    float attackCoef = 0.1f, decayCoef = 0.01f, releaseCoef = 0.01f, sustain = 1.0f;
    float level = 0.0f;
    Stage stage = Stage::idle;
};

// The four-pole transistor ladder, solved with zero-delay feedback: the four
// TPT one-pole stages and the feedback around them are resolved within the
// sample (Zavalishin), so the resonance peak sits on the cutoff all the way up
// instead of drifting below it as a one-sample-delayed loop does. The cutoff,
// resonance and drive ramp across each call rather than gliding over 50 ms, so
// a filter envelope with a 10 ms decay really snaps; it runs at twice the
// sample rate so what the saturators add above Nyquist folds back far quieter;
// drive 1 is close to linear (the old ladder compressed peaks by a sixth at
// unity) and the feedback is tamed by a rational tanh, which is what limits
// self-oscillation. Half the passband loss that resonance causes is made up.
class Ladder
{
public:
    static constexpr int kMaxBlock = 64;   // the longest run process() is asked for (a voice sub-block)
    void prepare (double sr, int maxBlock = kMaxBlock);
    void reset() noexcept
    {
        for (auto& ch : s) for (auto& v : ch) v = 0.0f;
        if (os != nullptr) os->reset();
        primed = false;
    }
    void setMode (int type) noexcept;   // 0 LP12, 1 LP24, 2 HP12, 3 HP24, 4 BP12, 5 BP24; resets the state on a real change
    // Both channels, n samples; cutoff, resonance (0..1) and drive (>= 1) move
    // from the previous call's values to these over the block.
    void process (float* L, float* R, int n, float cutoffHz, float resonance, float drive) noexcept;

    static float saturate (float x) noexcept   // tanh to within 2%, cheap
    {
        x = juce::jlimit (-3.0f, 3.0f, x);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

private:
    struct Coefs { float g = 0.1f, k = 0.0f, pre = 0.25f, post = 4.0f; };   // tan(pi fc/fs), feedback 0..4, the drive's input scale and make-up
    Coefs coefsFor (float cutoffHz, float resonance, float drive) const noexcept;

    double sampleRate = 48000.0;   // the rate the ladder runs at: twice the host's
    int mode = -1;
    float A[5] { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };   // taps on [input, y1, y2, y3, y4]
    float s[2][4] {};                               // the four stage states per channel
    Coefs last;
    bool primed = false;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;   // 2x: the saturators' products above Nyquist fold back far quieter
};

// One playing note: 3 wavetable oscillators (B can FM A, each with a warp) x
// up to 16 unison copies, sub + noise, two filters, three envelopes, four LFOs.
class SynthVoice : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice (VoiceContext& context);

    bool canPlaySound (juce::SynthesiserSound*) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int controllerNumber, int newValue) override;
    void aftertouchChanged (int newValue) override;
    void channelPressureChanged (int newValue) override;
    void setCurrentPlaybackSampleRate (double newRate) override;
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    static constexpr int kSub = 32;       // modulation is updated every kSub samples
    static constexpr int kMaxUnison = 16;

    float lfoValue (int shape, float phase, float held) const noexcept;
    void updateEnvelopes (const SynthParams&);
    void beginNote (int midiNoteNumber, float velocity, int pitchWheel, int midiChannel);   // what startNote does once the voice is free
    void hardReset() noexcept;   // everything silent and at rest, instantly (only ever after a fade)

    float readWave (int osc, int wave, float mip, float morph, float phase) const noexcept;
    // One warped sample: Sync crossfades into the reset over the last tenth of
    // the master cycle, Fold is anti-aliased by its antiderivative, the rest
    // reshape the phase.
    float readWarped (int osc, int wave, float mip, float morph, int warp, float phase, float amt, float& foldPrev) const noexcept;
    float lfoValueFor (int k, const SynthParams& p, int sampleInBlock, int blockLen, float& heldOut) noexcept;
    float sourceValue (int source, const float* lfo, float filterEnvValue, float modEnvValue) const noexcept;

    VoiceContext& ctx;
    Envelope ampEnv, filterEnv, modEnv;
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f, lp = 0.0f, bp = 0.0f, hp = 0.0f;
        void reset() noexcept { ic1 = ic2 = lp = bp = hp = 0.0f; }
        void process (float x, float g, float k) noexcept
        {
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
    };

    // One filter: the ladder for LP/HP/BP; a TPT state-variable pair and a comb
    // line for Notch, Comb and Formant. Two of them: filter 1 and filter 2. The
    // cutoff ramps across each sub-block from where it was, so modulation
    // never steps.
    struct FilterUnit
    {
        Ladder ladder;
        Svf svfA[2], svfB[2];
        FracDelay comb[2];
        float lastCutoff = -1.0f;   // < 0: nothing to ramp from
        void prepare (double sampleRate, int blockSize);
        void reset() noexcept;
        void process (int type, int model, float cutoff, float res, float drive, juce::AudioBuffer<float>& buffer, int n, float fsr) noexcept;
        void processExtra (int type, float cutoffFrom, float cutoffTo, float res, juce::AudioBuffer<float>& buffer, int n, float fsr) noexcept;
    };
    FilterUnit filters[2];
    SampleVoice sampler;
    juce::AudioBuffer<float> scratch { 2, kSub }, scratch2 { 2, kSub };   // the filter 1 bus and the filter 2 bus (Parallel / Split)
    // A warp (sync, bend, PWM, mirror, fold) is not band-limited: the oscillators
    // run at twice the rate while one is on and the two buses are decimated here.
    std::unique_ptr<juce::dsp::Oversampling<float>> oscOversampling;
    juce::Random rng;

    int serial = -1;               // this note's serial, see VoiceContext::displayVoice
    float velocity = 1.0f, velocityGain = 1.0f;
    float bendNorm = 0.0f;         // wheel position -1..1; the range is applied per block
    float aftertouch = 0.0f;       // 0..1
    float slide = 0.0f;            // CC74, 0..1
    float driftPitch = 0.0f, driftFilter = 0.0f;   // analog drift, slow random walks (-1..1)
    float envScale = 1.0f;         // per-note envelope time variation
    int channel = 1;               // MIDI channel of this note
    float noteRandom = 0.0f;       // -1..1, drawn per note
    float currentNote = 60.0f, targetNote = 60.0f, glideInc = 0.0f;
    float lastGainL = 0.0f, lastGainR = 0.0f;   // the output gains of the previous sub-block, ramped from
    bool gainsPrimed = false;

    // A stolen voice (or a hard all-notes-off) is not cut in one sample: the
    // sound it was making fades over kFadeSamples, and the note that took the
    // voice begins when the fade is done.
    static constexpr int kFadeSamples = 128;
    bool fadePending = false;
    int fadeRemaining = 0;
    struct PendingNote { int note = 60; float velocity = 1.0f; int wheel = 8192; int channel = 1; bool armed = false; } pending;

    float phaseA[kMaxUnison] {}, phaseB[kMaxUnison] {}, phaseC[kMaxUnison] {};
    float foldPrevA[kMaxUnison] {}, foldPrevB[kMaxUnison] {}, foldPrevC[kMaxUnison] {};   // the fold warp's previous inputs
    float subPhase = 0.0f;
    float lfoNotePhase[kNumLfos] {}, lfoHeld[kNumLfos] {};
    SynthParams local;              // this sub-block's modulated copy of the parameters
};

} // namespace stacks
