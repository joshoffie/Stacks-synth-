#include "PatchExplainer.h"
#include "LlmPatchGenerator.h"

namespace stacks
{

juce::String PatchExplainer::userPrompt (const Patch& p)
{
    juce::String s;
    s << "Explain this patch to someone who is new to synthesizers, in plain words, no JSON.\n"
      << "Patch \"" << p.name << "\"" << (p.category.isNotEmpty() ? " (" + p.category + ")" : juce::String()) << ": " << p.description << "\n"
      << "settings: " << juce::JSON::toString (p.paramsToVar(), true) << "\n";
    if (! p.waves[0].isEmpty())
        s << "oscillator A plays a designed wavetable called \"" << p.waves[0].name << "\".\n";
    s << "Write two short paragraphs. First: WHY IT SOUNDS LIKE THIS - the three or four settings that give it its character and what each contributes "
      << "(name the knobs exactly as they appear: Cutoff, A Morph, Attack, Reverb Mix...). Second, starting with 'Try:' - three concrete knob moves, "
      << "each on its own line as 'Knob: direction - what you'll hear'. Under 140 words in total. /no_think";
    return s;
}

bool PatchExplainer::explain (LlmBackend& backend, const Patch& p,
                              const std::function<void (const juce::String&)>& onText,
                              const std::function<bool()>& shouldCancel, juce::String& error)
{
    // Printable text only, bounded, so a small model cannot ramble.
    const juce::String grammar = "root ::= [\\x20-\\x7E\\n]{80,1100}\n";
    return backend.chat (LlmPatchGenerator::systemPrompt (false), userPrompt (p), grammar, onText,
                         [] (const juce::String&) {}, shouldCancel, error);
}

} // namespace stacks
