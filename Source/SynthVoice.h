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
    void controllerMoved (int, int) override {}
    void setCurrentPlaybackSampleRate (double newRate) override;
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    static constexpr int kSub = 32;       // modulation is updated every kSub samples
    static constexpr int kMaxUnison = 4;

    float lfoValue (int shape, float phase, float held) const noexcept;
    void updateEnvelopes (const SynthParams&);

    VoiceContext& ctx;
    juce::ADSR ampEnv, filterEnv;
    juce::dsp::LadderFilter<float> filter;
    juce::dsp::LadderFilterMode filterMode = juce::dsp::LadderFilterMode::LPF24;
    juce::AudioBuffer<float> scratch { 2, kSub };
    juce::Random rng;

    float velocityGain = 1.0f;
    float pitchBendSemis = 0.0f;
    float currentNote = 60.0f, targetNote = 60.0f, glideInc = 0.0f;

    float phaseA[kMaxUnison] {}, phaseB[kMaxUnison] {};
    float subPhase = 0.0f;
    float lfoPhase[2] {}, lfoHeld[2] {};
};

} // namespace stacks
