#include "Sampler.h"

#include <cmath>

namespace stacks
{

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
}

std::unique_ptr<SampleData> SampleBank::read (const juce::File& file, juce::String& error) const
{
    if (! file.existsAsFile())
    {
        error = "file not found";
        return nullptr;
    }
    static juce::AudioFormatManager formats;
    static std::once_flag once;
    std::call_once (once, [] { formats.registerBasicFormats(); });

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "not an audio file this build can read (wav, aiff, flac, mp3, ogg)";
        return nullptr;
    }
    const int channels = juce::jlimit (1, 2, (int) reader->numChannels);
    const auto maxSamples = (juce::int64) (kMaxSeconds * reader->sampleRate);
    const int length = (int) juce::jmin (reader->lengthInSamples, maxSamples);
    if (length < 2)
    {
        error = "the file is empty";
        return nullptr;
    }
    auto data = std::make_unique<SampleData>();
    data->name = file.getFileNameWithoutExtension();
    data->sampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
    data->audio.setSize (channels, length);
    reader->read (&data->audio, 0, length, 0, true, channels > 1);
    return data;
}

void SampleBank::setActive (std::unique_ptr<SampleData> data)
{
    const SampleData* raw = data.get();
    if (data != nullptr)
        owned.push_back (std::move (data));
    active.store (raw, std::memory_order_release);
}

//==============================================================================
void SampleVoice::start (const SampleData* s, double hostSampleRate, int midiNote) noexcept
{
    sample = s;
    hostRate = hostSampleRate > 0.0 ? hostSampleRate : 48000.0;
    note = midiNote;
    playhead = -1.0;
    finished = false;
    grainClock = 0.0;
    for (auto& g : grains) g.active = false;
}

float SampleVoice::readInterp (int channel, double pos) const noexcept
{
    const int len = sample->length();
    const int i0 = juce::jlimit (0, len - 1, (int) pos);
    const int i1 = juce::jmin (len - 1, i0 + 1);
    const float frac = (float) (pos - (double) i0);
    const float* a = sample->audio.getReadPointer (juce::jmin (channel, sample->audio.getNumChannels() - 1));
    return a[i0] + (a[i1] - a[i0]) * frac;
}

void SampleVoice::spawnGrain (const SamplerParams& sp, double ratio, juce::Random& rng) noexcept
{
    Grain* g = nullptr;
    for (auto& candidate : grains)
        if (! candidate.active) { g = &candidate; break; }
    if (g == nullptr)   // all busy: take the one furthest along
    {
        float oldest = -1.0f;
        for (auto& candidate : grains)
        {
            const float progress = (float) candidate.age / (float) juce::jmax (1, candidate.length);
            if (progress > oldest) { oldest = progress; g = &candidate; }
        }
    }
    const int len = sample->length();
    const double centre = (double) sp.start + (double) sp.spray * (rng.nextDouble() * 2.0 - 1.0);
    g->pos = juce::jlimit (0.0, (double) (len - 2), centre * (double) (len - 1));
    g->inc = ratio * std::exp2 ((double) sp.pitchRand * (rng.nextDouble() * 2.0 - 1.0) / 12.0);
    g->length = juce::jmax (32, (int) ((double) sp.grainMs * 0.001 * hostRate));
    g->age = 0;
    const float pan = juce::jlimit (-1.0f, 1.0f, sp.spread * (rng.nextFloat() * 2.0f - 1.0f));
    g->gainL = std::sqrt (0.5f * (1.0f - pan));
    g->gainR = std::sqrt (0.5f * (1.0f + pan));
    g->active = true;
}

void SampleVoice::render (float* L, float* R, int n, const SamplerParams& sp, float noteOffset, juce::Random& rng) noexcept
{
    if (sample == nullptr || sample->length() < 2 || sp.mode == 0 || sp.level <= 0.0f)
        return;
    const int len = sample->length();
    const double ratio = std::exp2 (((double) (note - sample->rootNote) + (double) sp.semitones + (double) noteOffset) / 12.0)
                       * sample->sampleRate / hostRate;
    const int chR = sample->audio.getNumChannels() > 1 ? 1 : 0;

    if (sp.mode == 1)   // Pitched: one playhead from Start, looped or once
    {
        const double startPos = juce::jlimit (0.0, (double) (len - 2), (double) sp.start * (double) (len - 1));
        if (playhead < 0.0)
            playhead = startPos;
        for (int i = 0; i < n && ! finished; ++i)
        {
            L[i] += readInterp (0, playhead) * sp.level;
            R[i] += readInterp (chR, playhead) * sp.level;
            playhead += ratio;
            if (playhead >= (double) (len - 1))
            {
                if (sp.loop) playhead = startPos + std::fmod (playhead - (double) (len - 1), juce::jmax (1.0, (double) (len - 1) - startPos));
                else         finished = true;
            }
        }
        return;
    }

    // Granular: grains sown at Grain Rate around Start, each a Hann window.
    const double interval = hostRate / (double) juce::jmax (0.5f, sp.grainsPerSecond);
    const float overlap = juce::jmax (1.0f, sp.grainsPerSecond * sp.grainMs * 0.001f);
    const float comp = sp.level / std::sqrt (overlap);
    for (int i = 0; i < n; ++i)
    {
        grainClock -= 1.0;
        if (grainClock <= 0.0)
        {
            spawnGrain (sp, ratio, rng);
            grainClock += interval * (0.85 + 0.3 * rng.nextDouble());
        }
        float l = 0.0f, r = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const float w = 0.5f - 0.5f * std::cos (twoPi * (float) g.age / (float) g.length);
            l += readInterp (0, g.pos) * w * g.gainL;
            r += readInterp (chR, g.pos) * w * g.gainR;
            g.pos += g.inc;
            if (++g.age >= g.length || g.pos >= (double) (len - 1))
                g.active = false;
        }
        L[i] += l * comp;
        R[i] += r * comp;
    }
}

} // namespace stacks
