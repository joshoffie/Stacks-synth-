#pragma once

#include <set>
#include <string>

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
    // Finished "key": value pairs in a half-written patch object, each once; see the .cpp.
    static juce::StringArray extractWrites (const std::string& partialObject, std::set<std::string>& alreadyReported);
    static juce::String grammar (int patchCount, bool designWaves = true); // GBNF for {"patches":[exactly patchCount]} built from the parameter table

private:
    std::shared_ptr<LlmBackend> backend;
    std::shared_ptr<PatchGenerator> fallback;
};

} // namespace stacks
