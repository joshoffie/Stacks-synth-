#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>

namespace stacks
{

// A whole track in numbers: tempo, key, where the mix has room, brightness,
// density, dynamics and width. Signal analysis only; the model designs from
// the brief it produces.
struct SongAnalysis
{
    float duration = 0.0f;                 // seconds analysed
    float bpm = 0.0f, tempoConfidence = 0.0f;
    int keyRoot = -1;                      // 0 = C .. 11 = B, -1 unknown
    bool minor = false;
    float keyConfidence = 0.0f;
    static constexpr int kBands = 6;       // sub, bass, low mids, mids, upper mids, air
    float bandDb[kBands] {};               // energy per octave, dB relative to the median band
    int openBand = 3;                      // the emptiest band: where a new sound has room
    int fullBand = 1;                      // the busiest band: stay out of it
    float centroidHz = 0.0f;
    float onsetsPerSecond = 0.0f;          // rhythmic density
    float dynamicsDb = 0.0f;               // spread of short-term loudness
    float width = 0.0f;                    // 0 mono .. 1 uncorrelated
    juce::String feel;                     // "house", "drum and bass"... a guess from tempo and tone
    juce::String brief;                    // the analysis in words, for the model's prompt

    juce::String keyName() const;          // "A minor"
    static const char* bandName (int band);
};

// Decodes a file to mono at about 22 kHz (and measures its width), up to maxSeconds.
bool readSongMono (const juce::File&, std::vector<float>& mono, double& sampleRate, float& width, juce::String& error, double maxSeconds = 240.0);

SongAnalysis analyseSong (const std::vector<float>& mono, double sampleRate, float width);

} // namespace stacks
