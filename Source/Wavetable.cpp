#include "Wavetable.h"

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace stacks
{

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    // Deterministic pseudo-random value in [0,1) for the "Grit" table.
    float hash01 (uint32_t x) noexcept
    {
        x ^= x >> 16; x *= 0x7feb352dU;
        x ^= x >> 15; x *= 0x846ca68bU;
        x ^= x >> 16;
        return (float) (x & 0xffffffU) / 16777216.0f;
    }
}

// Each wave is defined as a plain time-domain cycle; the constructor then
// band-limits it with an FFT, so these can use discontinuities freely.
double WavetableBank::sampleFn (int wave, double m, double p)
{
    switch (wave)
    {
        case 0: // Sine -> soft saturated sine
        {
            const double x = std::sin (twoPi * p);
            return (1.0 - m) * x + m * std::tanh (5.0 * x) / std::tanh (5.0);
        }
        case 1: // Triangle -> skewed towards a saw
        {
            const double s = 0.5 - 0.45 * m;
            return p < s ? -1.0 + 2.0 * p / s : 1.0 - 2.0 * (p - s) / (1.0 - s);
        }
        case 2: // Saw via phase distortion: sine at m=0, sharp saw at m=1
        {
            const double k  = 0.5 - 0.49 * m;
            const double pd = p < k ? 0.5 * p / k : 0.5 + 0.5 * (p - k) / (1.0 - k);
            return std::cos (twoPi * pd);
        }
        case 3: // Pulse, width 50% -> 5%
        {
            const double w = 0.5 - 0.45 * m;
            return p < w ? 1.0 : -1.0;
        }
        case 4: // Sync: higher-pitched sine reset every cycle
        {
            const double r = 1.0 + 7.0 * m;
            return std::sin (twoPi * p * r) * (1.0 - p);
        }
        case 5: // Organ drawbars, morph between two registrations
        {
            static const double h[6]  = { 1, 2, 3, 4, 6, 8 };
            static const double r0[6] = { 1.0, 0.0, 0.5, 0.0, 0.2, 0.0 };
            static const double r1[6] = { 0.6, 0.9, 0.3, 0.7, 0.5, 0.5 };
            double y = 0.0;
            for (int i = 0; i < 6; ++i)
                y += (r0[i] + (r1[i] - r0[i]) * m) * std::sin (twoPi * h[i] * p);
            return y;
        }
        case 6: // Formant: windowed sine burst, formant moves up with morph
        {
            const double f   = 2.0 + 14.0 * m;
            const double win = 0.5 - 0.5 * std::cos (twoPi * p);
            return std::sin (twoPi * p * f) * win;
        }
        case 7: // Glass: sparse upper partials, morph tilts towards them
        {
            static const double h[7]    = { 1, 3, 5, 7, 11, 14, 19 };
            static const double base[7] = { 1.0, 0.5, 0.35, 0.3, 0.2, 0.15, 0.1 };
            double y = 0.0;
            for (int i = 0; i < 7; ++i)
            {
                const double amp = base[i] * (i == 0 ? (1.2 - 0.6 * m) : (0.3 + 1.4 * m));
                y += amp * std::sin (twoPi * h[i] * p);
            }
            return y;
        }
        case 8: // Fold: overdriven sine folded back on itself
        {
            double x = std::sin (twoPi * p) * (1.0 + 5.0 * m);
            while (x > 1.0 || x < -1.0)
                x = x > 1.0 ? 2.0 - x : -2.0 - x;
            return x;
        }
        case 9: // Grit: random harmonic series, morph between two seeds
        default:
        {
            double y = 0.0;
            for (uint32_t h = 1; h <= 48; ++h)
            {
                const double aA = hash01 (h * 7919u + 1u),   aB = hash01 (h * 7919u + 2u);
                const double pA = hash01 (h * 104729u + 3u), pB = hash01 (h * 104729u + 4u);
                const double amp = (aA + (aB - aA) * m) / std::pow ((double) h, 0.7);
                const double ph  = pA + (pB - pA) * m;
                y += amp * std::sin (twoPi * ((double) h * p + ph));
            }
            return y;
        }
    }
}

WavetableBank::WavetableBank()
{
    data.assign ((size_t) kNumWaves * kFrames * kLevels * kStride, 0.0f);

    juce::dsp::FFT fft (11); // 2^11 == kSize
    std::vector<float> time ((size_t) kSize);
    std::vector<float> spectrum ((size_t) kSize * 2);
    std::vector<float> work ((size_t) kSize * 2);

    for (int wave = 0; wave < kNumWaves; ++wave)
    {
        for (int frame = 0; frame < kFrames; ++frame)
        {
            const double morph = (double) frame / (double) (kFrames - 1);

            double mean = 0.0;
            for (int i = 0; i < kSize; ++i)
            {
                time[(size_t) i] = (float) sampleFn (wave, morph, (double) i / kSize);
                mean += time[(size_t) i];
            }
            mean /= kSize;

            // Remove DC, normalise the full-bandwidth cycle to peak 1.
            float peak = 1.0e-9f;
            for (auto& s : time) { s -= (float) mean; peak = std::max (peak, std::abs (s)); }
            for (auto& s : time) s /= peak;

            std::fill (spectrum.begin(), spectrum.end(), 0.0f);
            std::copy (time.begin(), time.end(), spectrum.begin());
            fft.performRealOnlyForwardTransform (spectrum.data(), true);

            for (int level = 0; level < kLevels; ++level)
            {
                const int maxHarmonic = (kSize / 2) >> level;

                work = spectrum;
                work[0] = work[1] = 0.0f; // DC
                for (int h = maxHarmonic + 1; h <= kSize / 2; ++h)
                    work[(size_t) h * 2] = work[(size_t) h * 2 + 1] = 0.0f;

                fft.performRealOnlyInverseTransform (work.data());

                float* dst = table (wave, frame, level);
                std::copy (work.begin(), work.begin() + kSize, dst);
                dst[kSize] = dst[0];
            }
        }
    }
}

} // namespace stacks
