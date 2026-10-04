#pragma once

#include <memory>

#include "../PatchGenerator.h"
#include "LlmBackend.h"

namespace stacks
{

// Asks a language model to design patches. The prompt is built from the
// parameter table, the reply is parsed incrementally so candidates show up
// one at a time, and anything the model leaves out is filled from a base
// patch (the first parent when evolving, the defaults otherwise).
class LlmPatchGenerator : public PatchGenerator
{
public:
    LlmPatchGenerator (std::shared_ptr<LlmBackend> backend, std::shared_ptr<PatchGenerator> fallback);

    juce::String name() const override;
    std::vector<Patch> generate (const GenerationRequest&, const GenerationProgress&) override;

    // Exposed so the prompt can be inspected / tuned.
    static juce::String systemPrompt (bool designWaves = true);
    static juce::String userPrompt (const GenerationRequest&);
    static juce::String grammar (int patchCount, bool designWaves = true); // GBNF for {"patches":[exactly patchCount]} built from the parameter table

private:
    std::shared_ptr<LlmBackend> backend;
    std::shared_ptr<PatchGenerator> fallback;
};

} // namespace stacks
