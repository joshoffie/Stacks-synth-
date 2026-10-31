#pragma once

#include <juce_core/juce_core.h>
#include <vector>

#include "Patch.h"

namespace stacks
{

struct GenerationRequest
{
    std::vector<Patch> parents;   // empty = start fresh; otherwise evolve from these
    juce::String hint;            // free-text direction from the user, may be empty
    juce::String displayName;     // what the player sees as the seed of this batch ("Pad"); empty = the hint
    int count = 10;
    float variation = 0.5f;       // 0 = subtle changes, 1 = wild
    bool designWaves = true;      // invent new wavetables ("Custom") rather than only picking built-ins
    juce::String brief;           // the AI's own description of what the hint should sound like (filled in by the generator)
    int generation = 1;
};

// Callbacks a generator uses to report progress while it works. All may be
// empty; all may be called from the generator's (background) thread.
struct GenerationProgress
{
    std::function<void (const Patch&)> onPatch;          // one more candidate is ready
    std::function<void (const juce::String&)> onStatus;  // short progress text for the UI
    std::function<void (float, const juce::String&)> onProgress; // 0..1 toward the next patch (-1 = unknown), with detail
    std::function<bool()> shouldCancel;                  // polled; true = stop as soon as possible
    std::function<void (const juce::StringArray&)> onWrites;   // settings just written for the patch in progress, as "id\tvalue" (and "wave\tname")

    void patch (const Patch& p) const        { if (onPatch) onPatch (p); }
    void status (const juce::String& s) const { if (onStatus) onStatus (s); }
    void progress (float f, const juce::String& detail) const { if (onProgress) onProgress (f, detail); }
    bool cancelled() const                   { return shouldCancel && shouldCancel(); }
    void writes (const juce::StringArray& w) const { if (onWrites && ! w.isEmpty()) onWrites (w); }
};

// Anything that can turn a request into candidate patches. generate() runs on
// a background thread and may take a while (the AI generators do).
class PatchGenerator
{
public:
    virtual ~PatchGenerator() = default;
    virtual juce::String name() const = 0;
    virtual std::vector<Patch> generate (const GenerationRequest&, const GenerationProgress&) = 0;
};

// The breeder's mutation step on its own: nudges every parameter (amount 0..1,
// 0.5 feels like a sibling). The AI generator uses it when a model hands back
// a copy of a parent or of another patch in the batch.
void mutatePatch (Patch&, float amount, juce::int64 seed);
// How many settings differ noticeably between two patches: a choice that
// changed, a knob moved more than 12% of its travel, or a connection that
// differs (source/target). The Lab uses it to keep descendants apart.
int countAudibleDifferences (const Patch&, const Patch&);

// Structural differences: a different wavetable family or designed table, warp,
// oscillator presence or interval, FM / sub / noise presence, unison class,
// filter type / routing / model, a cutoff an octave away, the filter envelope's
// direction, the amp envelope's class, modulation connections (as a set),
// LFO shapes in use, which effects are on and their modes, the chain order,
// the arpeggiator and the sampler. Knob nudges do not count; new ideas do.
int countStructuralDifferences (const Patch&, const Patch&);

// How far a grow request strays, from its Variation amount.
enum class GrowMode { Close, Medium, Far };
GrowMode growModeFor (float variation) noexcept;

// "swelling pad", "short pluck", "percussive", "sustained, long tail", "sustained".
juce::String envelopeClass (const Patch&);

// What makes a sound itself, in a few short phrases a designer could keep while
// changing everything else: source, envelope class, filter, movement, space, character.
juce::StringArray identityTraits (const Patch&);

// Directed changes with intent, used instead of random noise when a child is too
// close to its parent or a sibling: each one is a real idea (another wavetable
// family, a rethought filter, new movement, a different space...). `move` wraps
// around kNumIdeaMoves; `seed` makes the choice inside the move repeatable.
constexpr int kNumIdeaMoves = 8;
void applyIdeaMove (Patch&, int move, juce::int64 seed);

// Far mode keeps the parent's identity: envelope class and category survive.
void keepIdentity (Patch& child, const Patch& parent);

// Honours what a prompt asks for by name: "lush chorus", "with delay", "tape",
// "shimmer", "distorted", "wide", "sub bass", "portamento", "punchy", "riser",
// "arp". Raises the settings those words call for and never lowers what the
// model chose (except the release for "punchy"/"stab"/"short").
void applyPromptCues (const juce::String& hint, Patch&);
WaveSpec mutateWave (const WaveSpec&, float amount, juce::int64 seed);

// No AI: archetype-based random patches, plus crossover + mutation of the
// parents. Always available, also the fallback when a model is missing.
class RandomPatchGenerator : public PatchGenerator
{
public:
    juce::String name() const override { return "Random"; }
    std::vector<Patch> generate (const GenerationRequest&, const GenerationProgress&) override;

private:
    std::atomic<int> counter { 0 };
    juce::SharedResourcePointer<WavetableBank> bank;   // to analyse a parent's built-in wave
};

} // namespace stacks
