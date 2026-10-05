#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>
#include <vector>

namespace stacks
{

// An audio file loaded for the sample oscillator. Immutable once built, so the
// audio thread reads it through a plain pointer.
struct SampleData
{
    juce::String name;                 // file name without the extension
    juce::AudioBuffer<float> audio;    // 1 or 2 channels
    double sampleRate = 44100.0;
    int rootNote = 60;                 // the key that plays the file at its own pitch
    int length() const noexcept { return audio.getNumSamples(); }
};

// Owns the loaded samples. The active one is handed to voices by pointer; old
// ones stay alive for the processor's lifetime (a handful of files at most), so
// no voice ever reads freed memory.
class SampleBank
{
public:
    static constexpr double kMaxSeconds = 60.0;   // longer files are cut here

    // Decodes a file (message thread). Null and an error message on failure.
    std::unique_ptr<SampleData> read (const juce::File&, juce::String& error) const;
    void setActive (std::unique_ptr<SampleData>);                                   // message thread
    void clear() noexcept                      { active.store (nullptr, std::memory_order_release); }
    const SampleData* current() const noexcept { return active.load (std::memory_order_acquire); }
    juce::String name() const                  { auto* s = current(); return s != nullptr ? s->name : juce::String(); }

private:
    std::atomic<const SampleData*> active { nullptr };
    std::vector<std::unique_ptr<SampleData>> owned;
};

// What the player reads per block, from the modulated parameters.
struct SamplerParams
{
    int mode = 0;                 // 0 off, 1 pitched, 2 granular
    float level = 0.8f;
    float start = 0.0f;           // 0..1 position in the file
    bool loop = true;             // pitched mode
    float semitones = 0.0f;       // transpose
    float grainMs = 80.0f;
    float grainsPerSecond = 20.0f;
    float spray = 0.1f;           // 0..1 scatter around start
    float pitchRand = 0.0f;       // semitones of random detune per grain
    float spread = 0.5f;          // stereo scatter of grains
};

// The per-voice player: Pitched (one playhead resampled to the note) or
// Granular (short Hann-windowed grains sown around Start). Adds into the buffers.
class SampleVoice
{
public:
    static constexpr int kMaxGrains = 16;

    void start (const SampleData*, double hostSampleRate, int midiNote) noexcept;
    void stop() noexcept                    { sample = nullptr; }
    bool isActive() const noexcept          { return sample != nullptr; }
    int startedNote() const noexcept        { return note; }

    // noteOffsetSemitones: glide, bend and pitch modulation relative to the started note.
    void render (float* left, float* right, int numSamples, const SamplerParams&, float noteOffsetSemitones, juce::Random&) noexcept;

private:
    struct Grain { double pos = 0.0, inc = 1.0; int length = 0, age = 0; float gainL = 0.0f, gainR = 0.0f; bool active = false; };

    float readInterp (int channel, double pos) const noexcept;
    void spawnGrain (const SamplerParams&, double ratio, juce::Random&) noexcept;

    const SampleData* sample = nullptr;
    double hostRate = 48000.0;
    int note = 60;
    double playhead = -1.0;       // < 0: not placed yet (Start is read on the first block)
    bool finished = false;
    double grainClock = 0.0;      // samples until the next grain
    Grain grains[kMaxGrains];
};

} // namespace stacks
