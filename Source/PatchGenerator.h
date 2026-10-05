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

    void patch (const Patch& p) const        { if (onPatch) onPatch (p); }
    void status (const juce::String& s) const { if (onStatus) onStatus (s); }
    void progress (float f, const juce::String& detail) const { if (onProgress) onProgress (f, detail); }
    bool cancelled() const                   { return shouldCancel && shouldCancel(); }
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
