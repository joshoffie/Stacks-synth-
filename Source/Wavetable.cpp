#include "Wavetable.h"

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <memory>

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

void Tables::buildMips (const float* cycle, float* dst)
{
    static thread_local std::unique_ptr<juce::dsp::FFT> fft;
    if (fft == nullptr)
        fft = std::make_unique<juce::dsp::FFT> (11); // 2^11 == kSize

    std::vector<float> time (cycle, cycle + kSize);

    // Remove DC, normalise the full-bandwidth cycle to peak 1.
    double mean = 0.0;
    for (float v : time) mean += v;
    mean /= kSize;
    float peak = 1.0e-9f;
    for (auto& v : time) { v -= (float) mean; peak = std::max (peak, std::abs (v)); }
    for (auto& v : time) v /= peak;

    std::vector<float> spectrum ((size_t) kSize * 2, 0.0f), work ((size_t) kSize * 2);
    std::copy (time.begin(), time.end(), spectrum.begin());
    fft->performRealOnlyForwardTransform (spectrum.data(), true);

    for (int level = 0; level < kLevels; ++level)
    {
        const int maxHarmonic = (kSize / 2) >> level;
        work = spectrum;
        work[0] = work[1] = 0.0f; // DC
        for (int h = maxHarmonic + 1; h <= kSize / 2; ++h)
            work[(size_t) h * 2] = work[(size_t) h * 2 + 1] = 0.0f;
        fft->performRealOnlyInverseTransform (work.data());

        float* out = dst + (size_t) level * kStride;
        std::copy (work.begin(), work.begin() + kSize, out);
        out[kSize] = out[0];
    }
}

WavetableBank::WavetableBank()
{
    data.assign ((size_t) kNumBuiltIn * kFrames * kLevels * kStride, 0.0f);
    std::vector<float> cycle ((size_t) kSize);

    for (int wave = 0; wave < kNumBuiltIn; ++wave)
    {
        for (int frame = 0; frame < kFrames; ++frame)
        {
            const double morph = (double) frame / (double) (kFrames - 1);
            for (int i = 0; i < kSize; ++i)
                cycle[(size_t) i] = (float) sampleFn (wave, morph, (double) i / kSize);
            Tables::buildMips (cycle.data(), data.data() + ((size_t) wave * kFrames + (size_t) frame) * kLevels * kStride);
        }
    }
}

//==============================================================================
std::shared_ptr<UserTable> loadUserTable (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "not an audio file I can read";
        return nullptr;
    }

    const auto length = (int) juce::jmin<juce::int64> (reader->lengthInSamples, 2048LL * 512LL);
    if (length < 64)
    {
        error = "file is too short";
        return nullptr;
    }

    juce::AudioBuffer<float> buffer ((int) reader->numChannels, length);
    reader->read (&buffer, 0, length, 0, true, true);

    // Mono sum
    std::vector<float> mono ((size_t) length, 0.0f);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < length; ++i)
            mono[(size_t) i] += buffer.getReadPointer (ch)[i] / (float) buffer.getNumChannels();

    // Frame size: Serum tables are 2048 per frame; anything else is treated as
    // frames of its own length (or a single cycle) and resampled to 2048.
    int frameSize = Tables::kSize;
    if (length < Tables::kSize)                 frameSize = length;
    else if (length % Tables::kSize != 0 && length % 1024 == 0) frameSize = 1024;
    else if (length % Tables::kSize != 0 && length % 512 == 0)  frameSize = 512;

    const int available = juce::jmax (1, length / frameSize);
    const int frames = juce::jmin (64, available);

    auto table = std::make_shared<UserTable>();
    table->name = file.getFileName();
    table->frames = frames;
    table->data.assign ((size_t) frames * Tables::kLevels * Tables::kStride, 0.0f);

    std::vector<float> cycle ((size_t) Tables::kSize);
    for (int f = 0; f < frames; ++f)
    {
        // evenly spaced pick when the file has more frames than we keep
        const int src = frames == available ? f : (int) ((juce::int64) f * (available - 1) / juce::jmax (1, frames - 1));
        const float* in = mono.data() + (size_t) src * frameSize;
        for (int i = 0; i < Tables::kSize; ++i)
        {
            const float pos = (float) i * (float) frameSize / (float) Tables::kSize;
            const int i0 = (int) pos;
            const int i1 = (i0 + 1) % frameSize;
            const float t = pos - (float) i0;
            cycle[(size_t) i] = in[i0] + t * (in[i1] - in[i0]);
        }
        Tables::buildMips (cycle.data(), table->data.data() + (size_t) f * Tables::kLevels * Tables::kStride);
    }
    return table;
}

void UserWavetables::set (int slot, std::shared_ptr<UserTable> table)
{
    if (slot < 0 || slot >= kSlots)
        return;
    if (owned[slot] != nullptr)
        retired.emplace_back (owned[slot], juce::Time::getMillisecondCounterHiRes());
    owned[slot] = std::move (table);
    slots[slot].store (owned[slot].get(), std::memory_order_release);
}

void UserWavetables::retireOld (double olderThanSeconds)
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [&] (const auto& r) { return now - r.second > olderThanSeconds * 1000.0; }),
                   retired.end());
}

} // namespace stacks
