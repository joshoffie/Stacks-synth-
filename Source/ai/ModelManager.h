#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace stacks
{

// A language model the built-in runtime can load: either one from our
// download catalogue or a copy that Ollama already has on this Mac.
struct ModelInfo
{
    juce::String id;          // stable key kept in settings, e.g. "qwen3-4b", "ollama:qwen3:4b"
    juce::String label;       // "Qwen3 4B"
    juce::String note;        // "recommended, 2.5 GB download"
    juce::String url;         // download source; empty for Ollama copies
    juce::int64 bytes = 0;    // expected size, for progress
    juce::File file;          // where the .gguf lives (or will live)
    bool installed = false;
    bool fromOllama = false;
};

class ModelManager
{
public:
    static juce::File appDataDirectory();   // ~/Library/Application Support/Stacks
    static juce::File modelsDirectory();    // .../Stacks/models

    // Download catalogue plus detected Ollama copies, `installed` refreshed.
    static std::vector<ModelInfo> catalogue();
    static std::optional<ModelInfo> find (const juce::String& id);

private:
    static std::vector<ModelInfo> builtInCatalogue();
    static std::vector<ModelInfo> ollamaCopies();
};

// Downloads one catalogue model in the background. Callbacks run on the
// message thread. Destroying the downloader cancels the download.
class ModelDownloader : private juce::URL::DownloadTask::Listener
{
public:
    ModelDownloader();
    ~ModelDownloader() override;

    bool start (const ModelInfo&, juce::String& error);
    void cancel();
    bool isRunning() const                 { return task != nullptr; }
    const ModelInfo& model() const         { return info; }

    std::function<void (juce::int64 done, juce::int64 total)> onProgress;
    std::function<void (bool ok, const juce::String& error)> onFinished;

private:
    void progress (juce::URL::DownloadTask*, juce::int64 bytesDownloaded, juce::int64 totalLength) override;
    void finished (juce::URL::DownloadTask*, bool success) override;

    std::unique_ptr<juce::URL::DownloadTask> task;
    ModelInfo info;
    juce::File partFile;
    std::shared_ptr<bool> alive;   // lets late callbacks notice we're gone
};

} // namespace stacks
