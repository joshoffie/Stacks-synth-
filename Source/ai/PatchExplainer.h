#pragma once

#include <juce_core/juce_core.h>
#include <functional>

#include "LlmBackend.h"
#include "../Patch.h"

namespace stacks
{

// Asks the model why a patch sounds the way it does and which knobs to try.
// Plain text, streamed; shares the patch system prompt so its cache is reused.
class PatchExplainer
{
public:
    static juce::String userPrompt (const Patch&);
    static bool explain (LlmBackend&, const Patch&,
                         const std::function<void (const juce::String&)>& onText,
                         const std::function<bool()>& shouldCancel, juce::String& error);
};

} // namespace stacks
