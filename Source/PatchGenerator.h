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
    int generation = 1;
};

// Callbacks a generator uses to report progress while it works. All may be
// empty; all may be called from the generator's (background) thread.
struct GenerationProgress
{
    std::function<void (const Patch&)> onPatch;          // one more candidate is ready
    std::function<void (const juce::String&)> onStatus;  // short progress text for the UI
    std::function<bool()> shouldCancel;                  // polled; true = stop as soon as possible

    void patch (const Patch& p) const        { if (onPatch) onPatch (p); }
    void status (const juce::String& s) const { if (onStatus) onStatus (s); }
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

// No AI: archetype-based random patches, plus crossover + mutation of the
// parents. Always available, also the fallback when a model is missing.
class RandomPatchGenerator : public PatchGenerator
{
public:
    juce::String name() const override { return "Random"; }
    std::vector<Patch> generate (const GenerationRequest&, const GenerationProgress&) override;

private:
    std::atomic<int> counter { 0 };
};

} // namespace stacks
