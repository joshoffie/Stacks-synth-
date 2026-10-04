#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "Wavetable.h"

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
    const SynthParams* params = nullptr;
    std::atomic<float> lastNote { -1.0f }; // most recent note, for glide
    std::atomic<float> modWheel { 0.0f };  // CC1, 0..1, shared by all voices
};

// One playing note: 2 wavetable oscillators (B can FM A) x up to 4 unison
// copies, sub + noise, ladder filter, two envelopes, two LFOs.
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
    static constexpr int kMaxUnison = 4;

    float lfoValue (int shape, float phase, float held) const noexcept;
    void updateEnvelopes (const SynthParams&);

    float sourceValue (int source, float lfo1, float lfo2, float filterEnvValue, float modEnvValue) const noexcept;

    VoiceContext& ctx;
    juce::ADSR ampEnv, filterEnv, modEnv;
    juce::dsp::LadderFilter<float> filter;
    juce::dsp::LadderFilterMode filterMode = juce::dsp::LadderFilterMode::LPF24;
    juce::AudioBuffer<float> scratch { 2, kSub };
    juce::Random rng;

    float velocity = 1.0f, velocityGain = 1.0f;
    float pitchBendSemis = 0.0f;
    float aftertouch = 0.0f;       // 0..1
    float noteRandom = 0.0f;       // -1..1, drawn per note
    float lfoRateMod[2] {};        // octaves, from the previous sub-block
    float currentNote = 60.0f, targetNote = 60.0f, glideInc = 0.0f;

    float phaseA[kMaxUnison] {}, phaseB[kMaxUnison] {};
    float subPhase = 0.0f;
    float lfoPhase[2] {}, lfoHeld[2] {};
};

} // namespace stacks
