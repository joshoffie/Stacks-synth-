#include "LlamaBackend.h"

#include <llama.h>
#include <string>
#include <thread>
#include <vector>

namespace stacks
{

namespace
{
    constexpr int kContextTokens = 8192;
    constexpr int kBatchTokens   = 512;
    constexpr int kMaxNewTokens  = 6000;

    // ~/Library/Application Support/Stacks/llama.log: what the runtime says
    // about the GPU, the model and any fallbacks. Capped so it can't grow forever.
    juce::File logFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Application Support").getChildFile ("Stacks").getChildFile ("llama.log");
    }

    void appendLog (const juce::String& text)
    {
        auto f = logFile();
        f.getParentDirectory().createDirectory();
        if (f.getSize() > 512 * 1024)
            f.deleteFile();
        f.appendText (text, false, false, "\n");
    }

    void fileLog (ggml_log_level level, const char* text, void*)
    {
        if (level >= GGML_LOG_LEVEL_INFO && text != nullptr)
            appendLog (juce::String::fromUTF8 (text));
    }

    struct Runtime
    {
        Runtime()  { llama_log_set (fileLog, nullptr); llama_backend_init(); appendLog ("\n==== runtime started ====\n"); }
        ~Runtime() { llama_backend_free(); }
    };

    void ensureRuntime()
    {
        static Runtime runtime;
    }

    // Tokens can split a multi-byte character; only hand on whole ones.
    size_t completeUtf8Prefix (const std::string& s)
    {
        const size_t n = s.size();
        for (size_t back = 1; back <= 4 && back <= n; ++back)
        {
            const auto c = (unsigned char) s[n - back];
            if ((c & 0xC0) == 0x80)
                continue; // continuation byte, keep looking for the lead
            const size_t need = c < 0x80 ? 1 : (c >> 5) == 0x06 ? 2 : (c >> 4) == 0x0E ? 3 : (c >> 3) == 0x1E ? 4 : 1;
            return back >= need ? n : n - back;
        }
        return n;
    }
}

LlamaBackend::LlamaBackend (juce::File f, juce::String n)
    : modelFile (std::move (f)), displayName (std::move (n)) {}

LlamaBackend::~LlamaBackend()
{
    std::lock_guard<std::mutex> guard (lock);
    unload();
}

bool LlamaBackend::isAvailable (juce::String& reason)
{
    if (! modelFile.existsAsFile())
    {
        reason = "model file is missing (" + modelFile.getFileName() + ")";
        return false;
    }
    return true;
}

bool LlamaBackend::ensureLoaded (juce::String& error)
{
    if (model != nullptr)
        return true;

    ensureRuntime();

    auto modelParams = llama_model_default_params();
    modelParams.n_gpu_layers = 999; // everything on the GPU

    model = llama_model_load_from_file (modelFile.getFullPathName().toRawUTF8(), modelParams);
    if (model == nullptr)
    {
        error = "could not load " + modelFile.getFileName();
        return false;
    }

    auto ctxParams = llama_context_default_params();
    ctxParams.n_ctx = kContextTokens;
    ctxParams.n_batch = kBatchTokens;
    ctxParams.n_ubatch = kBatchTokens;
    const int hw = (int) std::thread::hardware_concurrency();
    ctxParams.n_threads = juce::jmax (2, hw / 2);
    ctxParams.n_threads_batch = ctxParams.n_threads;
    ctxParams.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_AUTO;
    ctxParams.no_perf = true;

    ctx = llama_init_from_model (model, ctxParams);
    if (ctx == nullptr)
    {
        llama_model_free (model);
        model = nullptr;
        error = "could not create a context for " + modelFile.getFileName();
        return false;
    }

    vocab = llama_model_get_vocab (model);
    loaded = true;
    return true;
}

void LlamaBackend::unload()
{
    if (ctx != nullptr)   { llama_free (ctx); ctx = nullptr; }
    if (model != nullptr) { llama_model_free (model); model = nullptr; }
    vocab = nullptr;
    kvTokens.clear();
    loaded = false;
}

void LlamaBackend::unloadIfIdle (double idleSeconds)
{
    std::unique_lock<std::mutex> guard (lock, std::try_to_lock);
    if (! guard.owns_lock() || model == nullptr)
        return;
    if (juce::Time::getMillisecondCounterHiRes() - lastUsedMs.load() > idleSeconds * 1000.0)
        unload();
}

bool LlamaBackend::chat (const juce::String& systemPrompt, const juce::String& userPrompt, const juce::String& grammar,
                         const std::function<void (const juce::String&)>& onText,
                         const std::function<void (const juce::String&)>& onPhase,
                         const std::function<bool()>& shouldCancel, juce::String& error)
{
    std::lock_guard<std::mutex> guard (lock);
    lastUsedMs = juce::Time::getMillisecondCounterHiRes();

    auto phase = [&] (const juce::String& text) { if (onPhase) onPhase (text); };
    if (model == nullptr)
        phase ("Loading " + displayName + " into memory...");
    if (! ensureLoaded (error))
        return false;

    auto cancelled = [&] { return shouldCancel && shouldCancel(); };

    // ---- prompt, through the model's own chat template ----------------------
    const char* tmpl = llama_model_chat_template (model, nullptr);
    const std::string sys = systemPrompt.toStdString();
    const std::string usr = userPrompt.toStdString();
    llama_chat_message messages[2] = { { "system", sys.c_str() }, { "user", usr.c_str() } };

    std::vector<char> buf (sys.size() + usr.size() + 1024);
    int length = llama_chat_apply_template (tmpl, messages, 2, true, buf.data(), (int) buf.size());
    if (length < 0) // unknown template: fall back to ChatML
    {
        tmpl = nullptr;
        length = llama_chat_apply_template (nullptr, messages, 2, true, buf.data(), (int) buf.size());
    }
    if (length > (int) buf.size())
    {
        buf.resize ((size_t) length + 1);
        length = llama_chat_apply_template (tmpl, messages, 2, true, buf.data(), (int) buf.size());
    }
    if (length < 0)
    {
        error = "chat template failed";
        return false;
    }
    std::string prompt (buf.data(), (size_t) length);

    // Qwen3 and friends: pre-fill an empty reasoning block so they answer directly.
    if (tmpl != nullptr && std::string (tmpl).find ("<think>") != std::string::npos)
        prompt += "<think>\n\n</think>\n\n";

    // ---- tokenise -----------------------------------------------------------
    std::vector<llama_token> tokens (prompt.size() + 16);
    int count = llama_tokenize (vocab, prompt.c_str(), (int) prompt.size(), tokens.data(), (int) tokens.size(), true, true);
    if (count < 0)
    {
        tokens.resize ((size_t) -count);
        count = llama_tokenize (vocab, prompt.c_str(), (int) prompt.size(), tokens.data(), (int) tokens.size(), true, true);
    }
    if (count <= 0)
    {
        error = "tokenisation failed";
        return false;
    }
    tokens.resize ((size_t) count);

    const int contextSize = (int) llama_n_ctx (ctx);
    if (count >= contextSize - 64)
    {
        error = "prompt too long for the model's context";
        return false;
    }

    // ---- prefill, reusing the prefix already in the KV cache -----------------
    // The system prompt is long and identical from call to call; only the user
    // part and the reply change. Whatever prefix matches the previous call is
    // kept and only the rest is read, which turns ~12 s of reading into ~1 s.
    phase ("Reading your sound and the synth...");
    const double tStart = juce::Time::getMillisecondCounterHiRes();
    auto* memory = llama_get_memory (ctx);
    int common = 0;
    while (common < (int) kvTokens.size() && common < count - 1 && kvTokens[(size_t) common] == tokens[(size_t) common])
        ++common;
    if (common > 0 && llama_memory_seq_rm (memory, 0, common, -1))
    {
        kvTokens.resize ((size_t) common);
    }
    else
    {
        llama_memory_clear (memory, true);
        kvTokens.clear();
        common = 0;
    }
    auto forgetCache = [&] { llama_memory_clear (memory, true); kvTokens.clear(); };
    for (int i = common; i < count; i += kBatchTokens)
    {
        if (cancelled()) { error = "cancelled"; forgetCache(); return false; }
        const int len = juce::jmin (kBatchTokens, count - i);
        if (llama_decode (ctx, llama_batch_get_one (tokens.data() + i, len)) != 0)
        {
            error = "decode failed while reading the prompt";
            forgetCache();
            return false;
        }
        kvTokens.insert (kvTokens.end(), tokens.begin() + i, tokens.begin() + i + len);
    }

    // ---- sampling -----------------------------------------------------------
    auto chainParams = llama_sampler_chain_default_params();
    chainParams.no_perf = true;
    llama_sampler* sampler = llama_sampler_chain_init (chainParams);
    if (grammar.isNotEmpty())
    {
        // Constrains every token to the JSON shape we parse: no rambling, no invalid output.
        if (auto* g = llama_sampler_init_grammar (vocab, grammar.toRawUTF8(), "root"))
            llama_sampler_chain_add (sampler, g);
        else
            appendLog ("grammar was rejected by llama.cpp; sampling unconstrained");
    }
    llama_sampler_chain_add (sampler, llama_sampler_init_penalties (llama_vocab_n_tokens (vocab), 256, 1.08f, 0.0f, 0.0f));
    llama_sampler_chain_add (sampler, llama_sampler_init_top_k (40));
    llama_sampler_chain_add (sampler, llama_sampler_init_top_p (0.95f, 1));
    llama_sampler_chain_add (sampler, llama_sampler_init_min_p (0.05f, 1));
    llama_sampler_chain_add (sampler, llama_sampler_init_temp (0.8f));
    llama_sampler_chain_add (sampler, llama_sampler_init_dist ((uint32_t) juce::Time::getHighResolutionTicks()));

    const double tPrompt = juce::Time::getMillisecondCounterHiRes();
    phase ("Writing...");
    std::string pending;
    int position = count;
    int generated = 0;
    bool ok = true;

    for (int produced = 0; produced < kMaxNewTokens; ++produced)
    {
        if (cancelled()) { error = "cancelled"; ok = false; break; }

        const llama_token token = llama_sampler_sample (sampler, ctx, -1);
        if (llama_vocab_is_eog (vocab, token))
            break;
        ++generated;

        char piece[256];
        const int pieceLength = llama_token_to_piece (vocab, token, piece, (int) sizeof (piece), 0, true);
        if (pieceLength > 0)
        {
            pending.append (piece, (size_t) pieceLength);
            const size_t complete = completeUtf8Prefix (pending);
            if (complete > 0)
            {
                onText (juce::String::fromUTF8 (pending.data(), (int) complete));
                pending.erase (0, complete);
            }
        }

        if (++position >= contextSize)
            break;

        llama_token next = token;
        if (llama_decode (ctx, llama_batch_get_one (&next, 1)) != 0)
        {
            error = "decode failed";
            ok = false;
            forgetCache();
            break;
        }
        kvTokens.push_back (next);
    }

    if (! pending.empty())
        onText (juce::String::fromUTF8 (pending.data(), (int) pending.size()));

    llama_sampler_free (sampler);
    lastUsedMs = juce::Time::getMillisecondCounterHiRes();

    const double promptSec = (tPrompt - tStart) / 1000.0, genSec = (lastUsedMs.load() - tPrompt) / 1000.0;
    appendLog ("chat: prompt " + juce::String (count) + " tokens (" + juce::String (common) + " cached) in " + juce::String (promptSec, 1) + " s ("
               + juce::String (promptSec > 0 ? count / promptSec : 0.0, 0) + " tok/s), generated " + juce::String (generated)
               + " tokens in " + juce::String (genSec, 1) + " s (" + juce::String (genSec > 0 ? generated / genSec : 0.0, 1) + " tok/s)"
               + (ok ? "" : ", error: " + error));
    return ok;
}

} // namespace stacks
