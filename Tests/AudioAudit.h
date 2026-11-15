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

} // namespace stacks
