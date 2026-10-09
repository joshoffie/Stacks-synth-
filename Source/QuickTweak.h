#pragma once

#include <juce_core/juce_core.h>
#include <vector>

#include "Patch.h"

namespace stacks
{

// "Quick tweaks": a few words that change the playing sound a little
// ("slightly brighter", "more reverb", "shorter and wider") without a new
// batch. Plain words are understood here, instantly; anything else is handed
// to the model with a tiny grammar that only allows a short list of changes.

struct TweakChange { int paramIndex; float value; };

// Reads the request for words it knows (brighter, darker, warmer, wider,
// shorter, longer, punchier, softer, dirtier, cleaner, more/less reverb /
// delay / chorus / movement / bass / noise / vibrato, faster, slower, an
// octave up/down...). "slightly" halves and "much" doubles every move.
// Returns the changes relative to `current` and a one-line summary;
// false when no word was understood.
bool ruleTweak (const juce::String& request, const Patch& current, std::vector<TweakChange>& changes, juce::String& summary);

// The model's version: prompts and a grammar for {"changes":[{"id":..,"value":..}...]}.
juce::String tweakSystemPrompt();
juce::String tweakUserPrompt (const Patch& current, const juce::String& request);
juce::String tweakGrammar();
bool parseTweakReply (const juce::String& json, const Patch& current, std::vector<TweakChange>& changes, juce::String& summary);

// "Cutoff 800 -> 1.4k, Reverb Mix 0.15 -> 0.35"
juce::String describeChanges (const Patch& current, const std::vector<TweakChange>& changes);

} // namespace stacks
