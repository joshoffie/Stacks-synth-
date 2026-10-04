#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <optional>

#include "Parameters.h"

namespace stacks
{

// A complete sound: a name, a few words about it, and a value for every
// parameter. This is what the generators produce and what gets saved as JSON.
struct Patch
{
    Patch(); // every parameter at its default

    juce::String name { "Init" };
    juce::String description;
    juce::String category;                 // e.g. "Pad", "Bass" — free text
    juce::String origin;                   // "AI" when a language model designed it, otherwise empty
    std::array<float, kNumParams> values;  // real-world values; Choice params hold the index

    float get (P p) const noexcept { return values[(size_t) p]; }
    void set (P p, float v) noexcept { set ((int) p, v); }
    void set (int index, float v) noexcept; // clamps to range, rounds Int/Choice

    bool sameValuesAs (const Patch& other) const noexcept;

    // JSON: {"name": ..., "description": ..., "category": ..., "params": {"oscA_wave": "Saw", "filter_cutoff": 1200, ...}}
    // Parameters missing from the JSON take their value from `base` (defaults when null).
    juce::var toVar() const;
    juce::var paramsToVar() const;
    juce::String toJson() const;
    static std::optional<Patch> fromVar (const juce::var&, const Patch* base = nullptr);
    static std::optional<Patch> fromJson (const juce::String&, const Patch* base = nullptr);

    // Read the live parameter values / push this patch into the processor.
    // Both are message-thread operations.
    static Patch capture (const juce::AudioProcessorValueTreeState&);
    void applyTo (juce::AudioProcessorValueTreeState&) const;
};

// Keeps a patch in tune with the note that is played: oscillator A only at
// octaves, an audible oscillator B only at octaves (classic FM ratios are
// allowed when B is a silent modulator), fine detune and pitch-LFO depth capped.
void keepPatchInTune (Patch&);

// A short, human-readable account of how a patch is built
// ("Saw + Pulse(-12), 4-voice unison, LP24 @ 1.2 kHz, slow attack, big reverb").
juce::String describePatch (const Patch&);

} // namespace stacks
