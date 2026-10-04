#pragma once

#include <atomic>
#include <mutex>

#include "LlmBackend.h"

struct llama_model;
struct llama_context;
struct llama_vocab;

namespace stacks
{

// Runs a GGUF model inside the plug-in with llama.cpp (Metal on Apple
// silicon). The model is loaded on first use and kept until it has been idle
// for a while, because loading costs seconds and the weights cost gigabytes.
class LlamaBackend : public LlmBackend
{
public:
    LlamaBackend (juce::File modelFile, juce::String displayName);
    ~LlamaBackend() override;

    juce::String name() const override       { return "Built-in"; }
    juce::String modelName() const override  { return displayName; }
    bool isAvailable (juce::String& reason) override;
    bool chat (const juce::String& systemPrompt, const juce::String& userPrompt, const juce::String& grammar,
               const std::function<void (const juce::String&)>& onText,
               const std::function<void (const juce::String&)>& onPhase,
               const std::function<bool()>& shouldCancel, juce::String& error) override;

    const juce::File& file() const           { return modelFile; }
    bool isLoaded() const                    { return loaded.load(); }
    void unloadIfIdle (double idleSeconds);  // safe to call from the message thread

private:
    bool ensureLoaded (juce::String& error); // caller holds `lock`
    void unload();                           // caller holds `lock`

    juce::File modelFile;
    juce::String displayName;
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
    const llama_vocab* vocab = nullptr;
    std::mutex lock;
    std::atomic<bool> loaded { false };
    std::atomic<double> lastUsedMs { 0.0 };
};

} // namespace stacks
