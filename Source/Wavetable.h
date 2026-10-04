#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace stacks
{

// Shared constants for every table: one cycle is kSize samples, each stored at
// kLevels mip levels with fewer harmonics than the last, so high notes don't alias.
struct Tables
{
    static constexpr int kLevels = 10;        // level k keeps harmonics up to 1024 >> k
    static constexpr int kSize   = 2048;      // samples per cycle
    static constexpr int kStride = kSize + 1; // +1 guard sample for interpolation

    // Band-limits one cycle into kLevels tables (dst holds kLevels * kStride floats).
    static void buildMips (const float* cycle, float* dst);

    // Mip level whose harmonics stay below Nyquist for this fundamental.
    static int levelFor (float freqHz, double sampleRate) noexcept
    {
        const float x = freqHz * (float) kSize / (float) sampleRate; // = freq * 1024 / nyquist
        if (x <= 1.0f) return 0;
        const int level = (int) std::ceil (std::log2 (x));
        return level < kLevels ? level : kLevels - 1;
    }

    // Interpolated read from a frame set laid out [frame][level][kStride].
    static float read (const float* data, int frames, int level, float morph, float phase) noexcept
    {
        const float fpos = morph * (float) (frames - 1);
        int f0 = (int) fpos;
        if (f0 >= frames - 1) f0 = juce::jmax (0, frames - 2);
        const float ft = frames > 1 ? fpos - (float) f0 : 0.0f;
        const int f1 = juce::jmin (frames - 1, f0 + 1);

        const float x = phase * (float) kSize;
        int i0 = (int) x;
        if (i0 >= kSize) i0 = kSize - 1;
        if (i0 < 0) i0 = 0;
        const float t = x - (float) i0;

        const float* a = data + ((size_t) f0 * kLevels + (size_t) level) * kStride;
        const float* b = data + ((size_t) f1 * kLevels + (size_t) level) * kStride;
        const float sa = a[i0] + t * (a[i0 + 1] - a[i0]);
        const float sb = b[i0] + t * (b[i0 + 1] - b[i0]);
        return sa + ft * (sb - sa);
    }
};

// The ten built-in morphing wavetables, generated once at startup.
class WavetableBank
{
public:
    static constexpr int kNumBuiltIn = 10;
    static constexpr int kFrames = 8;
    static constexpr int kLevels = Tables::kLevels;
    static constexpr int kSize = Tables::kSize;
    static constexpr int kStride = Tables::kStride;

    WavetableBank();

    float read (int wave, int level, float morph, float phase) const noexcept
    {
        return Tables::read (data.data() + (size_t) wave * kFrames * kLevels * kStride, kFrames, level, morph, phase);
    }

    static int levelFor (float freqHz, double sampleRate) noexcept { return Tables::levelFor (freqHz, sampleRate); }

private:
    static double sampleFn (int wave, double morph, double phase);
    std::vector<float> data;
};

// An imported wavetable: `frames` cycles, each band-limited.
struct UserTable
{
    juce::String name;
    int frames = 0;
    std::vector<float> data;   // [frame][level][kStride]

    float read (int level, float morph, float phase) const noexcept
    {
        return Tables::read (data.data(), frames, level, morph, phase);
    }
};

// A wavetable described by its spectrum: 2-4 frames of harmonic amplitudes
// (harmonic 1 = the fundamental); the Morph knob sweeps from the first frame
// to the last. `tail` continues the series above the listed harmonics with a
// 1/k slope, so a table can be as bright as a saw without listing 1000 numbers.
// This is what the AI (and the random breeder) design; it lives inside a patch.
struct WaveSpec
{
    static constexpr int kHarmonics = 16;
    static constexpr int kMaxFrames = 4;

    juce::String name;
    float tail = 0.0f;                        // 0..1
    std::vector<std::vector<float>> frames;   // each kHarmonics amplitudes, 0..1

    bool isEmpty() const noexcept { return frames.empty(); }
    bool operator== (const WaveSpec&) const noexcept;
    bool operator!= (const WaveSpec& o) const noexcept { return ! (*this == o); }

    // Stored as 0..1 amplitudes ("frames"). The language model writes digits
    // 0-9 instead ("spectra": 0 = silent, 9 = full, 4 dB per step); both are read.
    juce::var toVar (bool asDigits = false) const;
    static std::optional<WaveSpec> fromVar (const juce::var&);
    juce::String toJson (bool asDigits = false) const;
    static std::optional<WaveSpec> fromJson (const juce::String&);

    static float digitToAmplitude (int digit) noexcept;
    static int amplitudeToDigit (float amplitude) noexcept;
};

// Renders a spectrum into a playable, band-limited 16-frame table.
std::shared_ptr<UserTable> buildSpectralTable (const WaveSpec&);

// The spectrum of an existing wave at morph 0, 0.5 and 1, so a generator can
// design a relative of it. `sample (morph, phase)` returns the waveform.
WaveSpec analyseWave (const std::function<float (float, float)>& sample, const juce::String& name);

// Reads a Serum-style wavetable (.wav of 2048-sample frames; other frame sizes
// and single cycles are resampled). Returns null and fills `error` on failure.
std::shared_ptr<UserTable> loadUserTable (const juce::File&, juce::String& error);

// The imported user slots plus the two designed ("Custom") tables of the loaded
// patch. Published to the audio thread by atomic pointer; the
// previous table is kept alive until retireOld() says no voice can still read it.
class UserWavetables
{
public:
    static constexpr int kUserSlots = 4;                       // "User 1-4": imported files
    static constexpr int kCustomSlotA = 4, kCustomSlotB = 5;   // "Custom": the loaded patch's designed tables
    static constexpr int kSlots = 6;
    static int customSlot (int osc) noexcept                   { return osc == 0 ? kCustomSlotA : kCustomSlotB; }

    const UserTable* active (int slot) const noexcept
    {
        return slot >= 0 && slot < kSlots ? slots[slot].load (std::memory_order_acquire) : nullptr;
    }

    void set (int slot, std::shared_ptr<UserTable> table);
    void retireOld (double olderThanSeconds);   // message thread
    juce::String name (int slot) const          { return slot >= 0 && slot < kSlots && owned[slot] ? owned[slot]->name : juce::String(); }

private:
    std::atomic<const UserTable*> slots[kSlots] {};
    std::shared_ptr<UserTable> owned[kSlots];
    std::vector<std::pair<std::shared_ptr<UserTable>, double>> retired;
};

// Index of "Custom" in waveNames(): the built-ins, then User 1-4, then Custom.
constexpr int kCustomWave = WavetableBank::kNumBuiltIn + UserWavetables::kUserSlots;

} // namespace stacks
