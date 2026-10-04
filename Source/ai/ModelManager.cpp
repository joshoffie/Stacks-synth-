#include "ModelManager.h"

namespace stacks
{

juce::File ModelManager::appDataDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Application Support").getChildFile ("Stacks");
}

juce::File ModelManager::modelsDirectory()
{
    return appDataDirectory().getChildFile ("models");
}

std::vector<ModelInfo> ModelManager::builtInCatalogue()
{
    const auto dir = modelsDirectory();
    std::vector<ModelInfo> list;

    auto add = [&] (const char* id, const char* label, const char* note, const char* url, juce::int64 bytes, const char* fileName)
    {
        ModelInfo m;
        m.id = id; m.label = label; m.note = note; m.url = url; m.bytes = bytes;
        m.file = dir.getChildFile (fileName);
        list.push_back (m);
    };

    // Official Qwen GGUF builds. Sizes are for the progress bar only.
    add ("qwen3-1.7b", "Qwen3 1.7B", "fastest, simpler ideas, 1.8 GB download",
         "https://huggingface.co/Qwen/Qwen3-1.7B-GGUF/resolve/main/Qwen3-1.7B-Q8_0.gguf", 1834426016LL, "Qwen3-1.7B-Q8_0.gguf");
    add ("qwen3-4b", "Qwen3 4B", "recommended, 2.5 GB download",
         "https://huggingface.co/Qwen/Qwen3-4B-GGUF/resolve/main/Qwen3-4B-Q4_K_M.gguf", 2497280256LL, "Qwen3-4B-Q4_K_M.gguf");
    add ("qwen3-8b", "Qwen3 8B", "richest, about half the speed of 4B, 5.0 GB download",
         "https://huggingface.co/Qwen/Qwen3-8B-GGUF/resolve/main/Qwen3-8B-Q4_K_M.gguf", 5027782944LL, "Qwen3-8B-Q4_K_M.gguf");

    return list;
}

// Ollama stores models as plain GGUF blobs; its manifests tell us which blob
// is which. Reusing them means no second download for Ollama users.
std::vector<ModelInfo> ModelManager::ollamaCopies()
{
    std::vector<ModelInfo> list;
    const auto root = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".ollama/models");
    const auto library = root.getChildFile ("manifests/registry.ollama.ai/library");
    if (! library.isDirectory())
        return list;

    for (const auto& modelDir : library.findChildFiles (juce::File::findDirectories, false))
    {
        for (const auto& manifest : modelDir.findChildFiles (juce::File::findFiles, false))
        {
            auto parsed = juce::JSON::parse (manifest.loadFileAsString());
            auto* obj = parsed.getDynamicObject();
            if (obj == nullptr) continue;
            auto* layers = obj->getProperty ("layers").getArray();
            if (layers == nullptr) continue;

            for (const auto& layer : *layers)
            {
                auto* lo = layer.getDynamicObject();
                if (lo == nullptr || ! lo->getProperty ("mediaType").toString().endsWith ("image.model"))
                    continue;

                const auto blob = root.getChildFile ("blobs").getChildFile (lo->getProperty ("digest").toString().replaceCharacter (':', '-'));
                if (! blob.existsAsFile())
                    continue;

                const auto name = modelDir.getFileName();
                const auto tag  = manifest.getFileName();
                ModelInfo m;
                m.id = "ollama:" + name + ":" + tag;
                m.label = name.substring (0, 1).toUpperCase() + name.substring (1) + " " + tag.toUpperCase();
                m.note = "already on this Mac (Ollama's copy), no download";
                m.file = blob;
                m.bytes = blob.getSize();
                m.installed = true;
                m.fromOllama = true;
                list.push_back (m);
            }
        }
    }

    std::sort (list.begin(), list.end(), [] (const ModelInfo& a, const ModelInfo& b) { return a.label < b.label; });
    return list;
}

std::vector<ModelInfo> ModelManager::catalogue()
{
    auto list = builtInCatalogue();
    for (auto& m : list)
        m.installed = m.file.existsAsFile() && m.file.getSize() > 1024 * 1024;

    for (auto& m : ollamaCopies())
        list.push_back (m);
    return list;
}

std::optional<ModelInfo> ModelManager::find (const juce::String& id)
{
    for (const auto& m : catalogue())
        if (m.id == id)
            return m;
    return std::nullopt;
}

//==============================================================================
ModelDownloader::ModelDownloader() : alive (std::make_shared<bool> (true)) {}

ModelDownloader::~ModelDownloader()
{
    *alive = false;
    cancel();
}

bool ModelDownloader::start (const ModelInfo& m, juce::String& error)
{
    if (task != nullptr)
    {
        error = "a download is already running";
        return false;
    }
    if (m.url.isEmpty())
    {
        error = "this model has no download source";
        return false;
    }

    info = m;
    info.file.getParentDirectory().createDirectory();
    partFile = info.file.getSiblingFile (info.file.getFileName() + ".part");
    partFile.deleteFile();

    task = juce::URL (info.url).downloadToFile (partFile, juce::URL::DownloadTaskOptions().withListener (this));
    if (task == nullptr)
    {
        error = "could not start the download";
        return false;
    }
    return true;
}

void ModelDownloader::cancel()
{
    if (task == nullptr)
        return;
    task.reset();          // stops the transfer
    partFile.deleteFile();
}

void ModelDownloader::progress (juce::URL::DownloadTask*, juce::int64 bytesDownloaded, juce::int64 totalLength)
{
    const auto total = totalLength > 0 ? totalLength : info.bytes;
    std::weak_ptr<bool> weak = alive;
    juce::MessageManager::callAsync ([this, weak, bytesDownloaded, total]
    {
        if (auto a = weak.lock(); a && *a && onProgress)
            onProgress (bytesDownloaded, total);
    });
}

void ModelDownloader::finished (juce::URL::DownloadTask*, bool success)
{
    std::weak_ptr<bool> weak = alive;
    juce::MessageManager::callAsync ([this, weak, success]
    {
        auto a = weak.lock();
        if (! a || ! *a)
            return;

        juce::String error;
        bool ok = success;
        if (ok && partFile.getSize() < 1024 * 1024)
        {
            ok = false;
            error = "the download was empty";
        }
        if (ok)
        {
            info.file.deleteFile();
            ok = partFile.moveFileTo (info.file);
            if (! ok) error = "could not move the file into place";
        }
        else
        {
            if (error.isEmpty()) error = "download failed (check your connection)";
            partFile.deleteFile();
        }

        task.reset();
        if (onFinished)
            onFinished (ok, error);
    });
}

} // namespace stacks
