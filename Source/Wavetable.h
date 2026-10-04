#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace stacks
{

// A bank of morphing, band-limited wavetables.
//
// Every wave has kFrames frames (the "morph" axis) and every frame is stored at
// kLevels mip levels, each with fewer harmonics than the last. At playback the
// voice picks the level whose highest harmonic stays below Nyquist for the
// note being played, which is what keeps high notes from aliasing.
class WavetableBank
{
public:
    static constexpr int kNumWaves = 10;
    static constexpr int kFrames   = 8;
    static constexpr int kLevels   = 10;   // level k keeps harmonics up to 1024 >> k
    static constexpr int kSize     = 2048; // samples per cycle
    static constexpr int kStride   = kSize + 1; // +1 guard sample for interpolation

    WavetableBank();

    // phase in [0,1), morph in [0,1]
    float read (int wave, int level, float morph, float phase) const noexcept
    {
        const float fpos = morph * (float) (kFrames - 1);
        int f0 = (int) fpos;
        if (f0 >= kFrames - 1) f0 = kFrames - 2;
        const float ft = fpos - (float) f0;

        const float x = phase * (float) kSize;
        int i0 = (int) x;
        if (i0 >= kSize) i0 = kSize - 1;
        if (i0 < 0) i0 = 0;
        const float t = x - (float) i0;

        const float* a = table (wave, f0, level);
        const float* b = table (wave, f0 + 1, level);
        const float sa = a[i0] + t * (a[i0 + 1] - a[i0]);
        const float sb = b[i0] + t * (b[i0 + 1] - b[i0]);
        return sa + ft * (sb - sa);
    }

    // Mip level whose harmonics stay below Nyquist for this fundamental.
    static int levelFor (float freqHz, double sampleRate) noexcept
    {
        const float x = freqHz * (float) kSize / (float) sampleRate; // = freq * 1024 / nyquist
        if (x <= 1.0f) return 0;
        const int level = (int) std::ceil (std::log2 (x));
        return level < kLevels ? level : kLevels - 1;
    }

private:
    const float* table (int wave, int frame, int level) const noexcept
    {
        return data.data() + (((size_t) wave * kFrames + (size_t) frame) * kLevels + (size_t) level) * kStride;
    }
    float* table (int wave, int frame, int level) noexcept
    {
        return data.data() + (((size_t) wave * kFrames + (size_t) frame) * kLevels + (size_t) level) * kStride;
    }

    static double sampleFn (int wave, double morph, double phase);

    std::vector<float> data;
};

} // namespace stacks
