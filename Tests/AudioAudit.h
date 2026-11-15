#pragma once

#include <juce_core/juce_core.h>

namespace stacks
{

// `StacksTests --audit <patch.json | folder>... [--wav <dir>] [--out <file.md>] [--note <midi>]`
//
// Plays every patch through the real voice engine and effect chain, offline,
// and measures what comes out: level, attack time, release length and shape,
// spectral centroid, high and low energy, stereo width, and how much the
// oscillator aliases on a high note. One line per patch, a summary with the
// worst offenders, optionally the renders as WAV files. The engine's own
// benchmark: run it before and after a DSP change and compare the tables.
int audioAudit (const juce::StringArray& args);

// `StacksTests --level <folder>... [--target <dBFS>] [--dry-run]`
//
// Evens out a library's loudness: every patch is rendered (one note, the
// category's usual octave), and its Master is moved so the note peaks at the
// target (-10 dBFS by default) within Master's range. Only master_gain changes.
int levelPatches (const juce::StringArray& args);

// Twenty-four quick notes of a long-release sine pad on sixteen voices, so the
// last eight steal. Returns the largest sample-to-sample jump in the output as a
// fraction of the peak: a one-sample cut reads near 1, a fade near the sine's
// own slope (0.03). Printed and checked by the engine tests.
float stealJumpRatio();

// Oscillator A alone at C7 with the given warp and amount (the audit's alias
// measure): energy between the harmonics against the harmonics, in dB.
float warpAliasDb (int warp, float amount, const char* table);

} // namespace stacks
