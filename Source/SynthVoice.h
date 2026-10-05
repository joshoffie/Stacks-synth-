#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "Wavetable.h"
#include "Effects.h"
#include "LfoTable.h"
#include "libMTSClient.h"

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
    float masterBend = 0.0f;                  // semitones from channel 1's wheel (MPE master), audio thread only
    float masterPressure = 0.0f;              // channel 1's pressure
    float channelSlide[16] {};                // per channel, so a note that starts after the controller moved begins right
    float channelPressure[16] {};
    std::atomic<float>* liveValues = nullptr;  // kNumParams, what the UI's markers show
    std::atomic<int> voiceCounter { 0 };       // hands out serial numbers at note-on
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

    float readWave (int osc, int wave, int mip, float morph, float phase) const noexcept;
    float lfoValueFor (int k, const SynthParams& p, int sampleInBlock, int blockLen, float& heldOut) noexcept;
    float sourceValue (int source, const float* lfo, float filterEnvValue, float modEnvValue) const noexcept;

    VoiceContext& ctx;
    juce::ADSR ampEnv, filterEnv, modEnv;
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
    // line for Notch, Comb and Formant. Two of them: filter 1 and filter 2.
    struct FilterUnit
    {
        juce::dsp::LadderFilter<float> ladder;
        juce::dsp::LadderFilterMode mode = juce::dsp::LadderFilterMode::LPF24;
        Svf svfA[2], svfB[2];
        FracDelay comb[2];
        void prepare (double sampleRate, int blockSize);
        void reset() noexcept;
        void process (int type, float cutoff, float res, float drive, juce::AudioBuffer<float>& buffer, int n, float fsr) noexcept;
        void processExtra (int type, float cutoff, float res, juce::AudioBuffer<float>& buffer, int n, float fsr) noexcept;
    };
    FilterUnit filters[2];
    juce::AudioBuffer<float> scratch { 2, kSub }, scratch2 { 2, kSub };   // the filter 1 bus and the filter 2 bus (Parallel / Split)
    juce::Random rng;

    int serial = -1;               // this note's serial, see VoiceContext::displayVoice
    float velocity = 1.0f, velocityGain = 1.0f;
    float bendNorm = 0.0f;         // wheel position -1..1; the range is applied per block
    float aftertouch = 0.0f;       // 0..1
    float slide = 0.0f;            // CC74, 0..1
    int channel = 1;               // MIDI channel of this note
    float noteRandom = 0.0f;       // -1..1, drawn per note
    float currentNote = 60.0f, targetNote = 60.0f, glideInc = 0.0f;

    float phaseA[kMaxUnison] {}, phaseB[kMaxUnison] {}, phaseC[kMaxUnison] {};
    float subPhase = 0.0f;
    float lfoNotePhase[kNumLfos] {}, lfoHeld[kNumLfos] {};
    SynthParams local;              // this sub-block's modulated copy of the parameters
};

} // namespace stacks
