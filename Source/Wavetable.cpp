#include "Wavetable.h"

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
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

//==============================================================================
bool WaveSpec::operator== (const WaveSpec& o) const noexcept
{
    if (name != o.name || std::abs (tail - o.tail) > 1.0e-3f || frames.size() != o.frames.size())
        return false;
    for (size_t f = 0; f < frames.size(); ++f)
    {
        if (frames[f].size() != o.frames[f].size())
            return false;
        for (size_t k = 0; k < frames[f].size(); ++k)
            if (std::abs (frames[f][k] - o.frames[f][k]) > 1.0e-3f)
                return false;
    }
    return true;
}

float WaveSpec::digitToAmplitude (int digit) noexcept
{
    digit = juce::jlimit (0, 9, digit);
    return digit == 0 ? 0.0f : std::pow (10.0f, -(float) (9 - digit) * 4.0f / 20.0f); // 4 dB per step
}

int WaveSpec::amplitudeToDigit (float amplitude) noexcept
{
    if (amplitude < 0.02f)
        return 0;
    return juce::jlimit (1, 9, 9 + juce::roundToInt (20.0f * std::log10 (juce::jmin (1.0f, amplitude)) / 4.0f));
}

juce::var WaveSpec::toVar (bool asDigits) const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("name", name);
    obj->setProperty ("tail", asDigits ? juce::var (juce::roundToInt (tail * 9.0f)) : juce::var (std::round (tail * 1000.0f) / 1000.0f));
    juce::Array<juce::var> frameArray;
    for (const auto& frame : frames)
    {
        juce::Array<juce::var> amps;
        for (float a : frame)
            amps.add (asDigits ? juce::var (amplitudeToDigit (a)) : juce::var (std::round (a * 1000.0f) / 1000.0f));
        frameArray.add (juce::var (amps));
    }
    obj->setProperty (asDigits ? "spectra" : "frames", juce::var (frameArray));
    return juce::var (obj);
}

std::optional<WaveSpec> WaveSpec::fromVar (const juce::var& v)
{
    auto* obj = v.getDynamicObject();
    if (obj == nullptr)
        return std::nullopt;

    const bool digits = obj->hasProperty ("spectra");
    const auto* frameArray = obj->getProperty (digits ? "spectra" : "frames").getArray();
    if (frameArray == nullptr)
        return std::nullopt;

    WaveSpec s;
    s.name = obj->getProperty ("name").toString().trim().substring (0, 24);
    const auto tailVar = obj->getProperty ("tail");
    s.tail = juce::jlimit (0.0f, 1.0f, digits ? (float) (int) tailVar / 9.0f : (float) (double) tailVar);

    float peak = 0.0f;
    for (const auto& frameVar : *frameArray)
    {
        auto* amps = frameVar.getArray();
        if (amps == nullptr)
            continue;
        std::vector<float> frame ((size_t) kHarmonics, 0.0f);
        for (int k = 0; k < kHarmonics && k < amps->size(); ++k)
        {
            const auto& a = (*amps)[k];
            frame[(size_t) k] = digits ? digitToAmplitude ((int) a) : juce::jlimit (0.0f, 1.0f, (float) (double) a);
            peak = std::max (peak, frame[(size_t) k]);
        }
        s.frames.push_back (std::move (frame));
        if ((int) s.frames.size() >= kMaxFrames)
            break;
    }
    if (s.frames.empty() || peak < 0.002f)  // nothing there, or silence
        return std::nullopt;
    return s;
}

juce::String WaveSpec::toJson (bool asDigits) const
{
    return juce::JSON::toString (toVar (asDigits), true);
}

std::optional<WaveSpec> WaveSpec::fromJson (const juce::String& text)
{
    if (text.trim().isEmpty())
        return std::nullopt;
    return fromVar (juce::JSON::parse (text));
}

namespace
{
    // The level the tail continues from: the loudest of the last four listed harmonics.
    float tailAnchor (const std::vector<float>& frame)
    {
        float top = 0.0f;
        for (size_t k = frame.size() >= 4 ? frame.size() - 4 : 0; k < frame.size(); ++k)
            top = std::max (top, frame[k]);
        return top;
    }
}

std::shared_ptr<UserTable> buildSpectralTable (const WaveSpec& spec)
{
    if (spec.isEmpty())
        return nullptr;

    constexpr int kOutFrames = 16;
    auto table = std::make_shared<UserTable>();
    table->name = spec.name.isNotEmpty() ? spec.name : juce::String ("Designed");
    table->frames = kOutFrames;
    table->data.assign ((size_t) kOutFrames * Tables::kLevels * Tables::kStride, 0.0f);

    juce::dsp::FFT fft (11);
    std::vector<float> spectrum ((size_t) Tables::kSize * 2), cycle ((size_t) Tables::kSize);
    const int last = (int) spec.frames.size() - 1;

    for (int f = 0; f < kOutFrames; ++f)
    {
        const float pos = last > 0 ? (float) f / (float) (kOutFrames - 1) * (float) last : 0.0f;
        const int i0 = juce::jlimit (0, last, (int) pos), i1 = juce::jmin (last, i0 + 1);
        const float t = pos - (float) i0;
        const auto& fa = spec.frames[(size_t) i0];
        const auto& fb = spec.frames[(size_t) i1];

        // Harmonic k is a sine with alternating sign, like a saw's series, so
        // 1/k levels really make a saw and odd-only levels really make a square.
        std::fill (spectrum.begin(), spectrum.end(), 0.0f);
        auto setHarmonic = [&] (int k, float a) { spectrum[(size_t) k * 2 + 1] = (k % 2 == 1 ? -1.0f : 1.0f) * a; };
        for (int k = 1; k <= WaveSpec::kHarmonics; ++k)
            setHarmonic (k, fa[(size_t) (k - 1)] + (fb[(size_t) (k - 1)] - fa[(size_t) (k - 1)]) * t);

        const float anchor = tailAnchor (fa) + (tailAnchor (fb) - tailAnchor (fa)) * t;
        if (spec.tail > 0.001f && anchor > 0.0f)
            for (int k = WaveSpec::kHarmonics + 1; k < Tables::kSize / 2; ++k)
                setHarmonic (k, anchor * spec.tail * (float) WaveSpec::kHarmonics / (float) k);

        fft.performRealOnlyInverseTransform (spectrum.data());
        std::copy (spectrum.begin(), spectrum.begin() + Tables::kSize, cycle.begin());
        Tables::buildMips (cycle.data(), table->data.data() + (size_t) f * Tables::kLevels * Tables::kStride);
    }
    return table;
}

WaveSpec analyseWave (const std::function<float (float, float)>& sample, const juce::String& name)
{
    WaveSpec spec;
    spec.name = name;

    juce::dsp::FFT fft (11);
    std::vector<float> buf ((size_t) Tables::kSize * 2);
    float tailSum = 0.0f;
    int tailCount = 0;

    for (float morph : { 0.0f, 0.5f, 1.0f })
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < Tables::kSize; ++i)
            buf[(size_t) i] = sample (morph, (float) i / (float) Tables::kSize);
        fft.performRealOnlyForwardTransform (buf.data(), true);
        auto magnitude = [&] (int k) { return std::hypot (buf[(size_t) k * 2], buf[(size_t) k * 2 + 1]); };

        std::vector<float> frame ((size_t) WaveSpec::kHarmonics);
        float peak = 1.0e-9f;
        for (int k = 1; k <= WaveSpec::kHarmonics; ++k)
        {
            frame[(size_t) (k - 1)] = magnitude (k);
            peak = std::max (peak, frame[(size_t) (k - 1)]);
        }
        for (auto& a : frame)
            a = std::round (a / peak * 100.0f) / 100.0f;

        const float anchor = tailAnchor (frame);
        if (anchor > 0.02f)
            for (int k = WaveSpec::kHarmonics + 1; k <= 64; ++k)
            {
                tailSum += (magnitude (k) / peak) / (anchor * (float) WaveSpec::kHarmonics / (float) k);
                ++tailCount;
            }
        spec.frames.push_back (std::move (frame));
    }
    spec.tail = tailCount > 0 ? juce::jlimit (0.0f, 1.0f, std::round (tailSum / (float) tailCount * 10.0f) / 10.0f) : 0.0f;
    return spec;
}

void UserWavetables::retireOld (double olderThanSeconds)
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [&] (const auto& r) { return now - r.second > olderThanSeconds * 1000.0; }),
                   retired.end());
}

} // namespace stacks
