#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "../Patch.h"
#include "../Sampler.h"

namespace stacks
{

// What a recording sounds like, in numbers: pitch, envelope, spectrum over
// time, noise, width, vibrato. No model involved; this is signal analysis.
struct SampleAnalysis
{
    bool pitched = false;
    float f0 = 0.0f;                                  // Hz, median over the pitched frames
    float attack = 0.01f, decay = 0.5f, sustain = 0.5f, release = 0.3f;   // seconds, except sustain (0..1)
    bool decaying = false;                            // dies away by itself (pluck, bell) rather than holding
    float centroidAttack = 2000.0f, centroidSustain = 2000.0f;   // Hz
    float brightnessDrop = 0.0f;                      // octaves the centroid falls from the attack into the body
    float noiseRatio = 0.0f;                          // energy away from the harmonics, 0..1
    float width = 0.0f;                               // 0 mono .. 1 uncorrelated channels
    float vibratoHz = 0.0f, vibratoSemis = 0.0f;
    float duration = 0.0f;                            // seconds of sound above the floor
    WaveSpec wave;                                    // the harmonic spectrum at the attack, the body and late on
    juce::String category;                            // Pluck, Pad, Bass, Lead, Keys, Bell or Texture
    juce::String brief;                               // the analysis in words, for the model's prompt
};

SampleAnalysis analyseSample (const SampleData&);

// A patch that imitates the analysis directly: the designed table on A, the
// measured envelope, a filter at the brightness with a sweep if it darkens,
// noise, width and vibrato as found. Instant; the model then evolves it.
Patch patchFromAnalysis (const SampleAnalysis&, const juce::String& name);

} // namespace stacks
