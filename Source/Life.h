#pragma once

#include <juce_core/juce_core.h>
#include "Parameters.h"

namespace stacks
{

// Life: the sound slowly wanders as it plays. An offset layer on a handful of
// knobs, driven by slow noise plus a nudge on every note, added to the block's
// parameter snapshot after the preset's values are read and before anything
// else sees them. The saved values are never touched, so Off is the preset
// exactly, and every knob shows the movement through its live marker.
class Life
{
public:
    Life();
    void prepare (double sampleRate);
    void reset();                                 // back to the preset's values on the next block
    void noteOn() noexcept;                       // audio thread: a note just started
    void apply (SynthParams&, int level, int numSamples) noexcept;   // audio thread, level 0 (off) .. 5

    static constexpr int kLevels = 5;
    static float levelAmount (int level) noexcept;   // how far level 1..5 reaches, 0..1 of each target's excursion
    static bool isTarget (int paramIndex) noexcept;  // one of the knobs Life moves

private:
    struct Target { P param; float excursion; };    // excursion: fraction of the knob's range at level 5
    static constexpr int kTargets = 19;
    static const Target targets[kTargets];
    float phase[kTargets][3] {}, rate[kTargets][3] {};
    float kick[kTargets] {};
    double sampleRate = 48000.0;
    juce::Random rng;
    int pendingKicks = 0;
};

} // namespace stacks
