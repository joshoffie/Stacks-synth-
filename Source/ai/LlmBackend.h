#pragma once

#include <juce_core/juce_core.h>
#include <functional>

namespace stacks
{

// Something that can answer a chat prompt with a stream of text. The patch
// generator only ever talks to this interface, so the model can live in a
// local Ollama server today and inside the plug-in (llama.cpp) tomorrow.
class LlmBackend
{
public:
    virtual ~LlmBackend() = default;

    virtual juce::String name() const = 0;       // "Ollama"
    virtual juce::String modelName() const = 0;  // "qwen3:8b"

    // Cheap reachability check. Fills `reason` when it returns false.
    virtual bool isAvailable (juce::String& reason) = 0;

    // Streams the reply through onText (deltas, in order). Blocks until done.
    // Returns false and fills `error` on failure or cancellation.
    virtual bool chat (const juce::String& systemPrompt,
                       const juce::String& userPrompt,
                       const std::function<void (const juce::String&)>& onText,
                       const std::function<bool()>& shouldCancel,
                       juce::String& error) = 0;
};

// A local Ollama server, spoken to over a plain socket (no system HTTP stack,
// so it behaves the same inside any host application).
class OllamaBackend : public LlmBackend
{
public:
    explicit OllamaBackend (juce::String model, juce::String host = "127.0.0.1", int port = 11434);

    juce::String name() const override       { return "Ollama"; }
    juce::String modelName() const override  { return model; }
    bool isAvailable (juce::String& reason) override;
    bool chat (const juce::String& systemPrompt, const juce::String& userPrompt,
               const std::function<void (const juce::String&)>& onText,
               const std::function<bool()>& shouldCancel, juce::String& error) override;

    // Names of the models the server has installed; empty when unreachable.
    static juce::StringArray listModels (const juce::String& host = "127.0.0.1", int port = 11434, int timeoutMs = 1500);

private:
    juce::String model, host;
    int port;
};

} // namespace stacks
