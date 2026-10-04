#include "LlmBackend.h"

#include <string>

namespace stacks
{

namespace
{
    // Minimal HTTP/1.1 client: sends one request and hands body bytes to
    // onBody as they arrive (chunked transfer decoded). onBody also gets
    // (nullptr, 0) heartbeats while waiting, so callers can cancel. It returns
    // false to stop. Result is the HTTP status, or 0 when nothing connected.
    int httpRequest (const juce::String& host, int port, const juce::String& method, const juce::String& path,
                     const juce::String& body, int connectTimeoutMs, int idleTimeoutMs,
                     const std::function<bool (const char*, int)>& onBody, juce::String& error)
    {
        juce::StreamingSocket socket;
        if (! socket.connect (host, port, connectTimeoutMs))
        {
            error = "could not connect to " + host + ":" + juce::String (port);
            return 0;
        }

        juce::String request;
        request << method << " " << path << " HTTP/1.1\r\n"
                << "Host: " << host << ":" << port << "\r\n"
                << "Connection: close\r\nAccept: application/json\r\n";
        if (body.isNotEmpty())
            request << "Content-Type: application/json\r\nContent-Length: " << (int) body.getNumBytesAsUTF8() << "\r\n";
        request << "\r\n" << body;

        const int requestLength = (int) request.getNumBytesAsUTF8();
        if (socket.write (request.toRawUTF8(), requestLength) != requestLength)
        {
            error = "failed to send request";
            return 0;
        }

        std::string data;
        size_t cursor = 0;
        bool headersDone = false, chunked = false, needChunkSize = true;
        int status = 0;
        long long contentLength = -1, bodyReceived = 0, chunkRemaining = 0;
        int idleMs = 0;
        char buf[16384];

        for (;;)
        {
            const int ready = socket.waitUntilReady (true, 250);
            if (ready < 0)
                break;
            if (ready == 0)
            {
                idleMs += 250;
                if (idleMs > idleTimeoutMs) { error = "timed out waiting for the model"; return status; }
                if (! onBody (nullptr, 0)) return status;
                continue;
            }
            idleMs = 0;

            const int n = socket.read (buf, (int) sizeof (buf), false);
            if (n <= 0)
                break;
            data.append (buf, (size_t) n);

            if (! headersDone)
            {
                const auto end = data.find ("\r\n\r\n");
                if (end == std::string::npos)
                    continue;

                const juce::String headers (data.substr (0, end).c_str());
                status = headers.fromFirstOccurrenceOf (" ", false, false).getIntValue();
                for (const auto& line : juce::StringArray::fromLines (headers))
                {
                    const auto key = line.upToFirstOccurrenceOf (":", false, false).trim().toLowerCase();
                    const auto value = line.fromFirstOccurrenceOf (":", false, false).trim().toLowerCase();
                    if (key == "transfer-encoding" && value.contains ("chunked")) chunked = true;
                    if (key == "content-length") contentLength = value.getLargeIntValue();
                }
                headersDone = true;
                cursor = end + 4;
            }

            if (chunked)
            {
                for (;;)
                {
                    if (needChunkSize)
                    {
                        const auto eol = data.find ("\r\n", cursor);
                        if (eol == std::string::npos) break;
                        chunkRemaining = std::strtoll (data.substr (cursor, eol - cursor).c_str(), nullptr, 16);
                        cursor = eol + 2;
                        needChunkSize = false;
                        if (chunkRemaining == 0) return status; // final chunk
                    }

                    const auto available = (long long) (data.size() - cursor);
                    if (available <= 0) break;

                    const auto take = (size_t) std::min (available, chunkRemaining);
                    if (take > 0)
                    {
                        if (! onBody (data.data() + cursor, (int) take)) return status;
                        cursor += take;
                        chunkRemaining -= (long long) take;
                    }
                    if (chunkRemaining > 0) break;        // rest of this chunk hasn't arrived
                    if (data.size() - cursor < 2) break;  // trailing CRLF not here yet
                    cursor += 2;
                    needChunkSize = true;
                }

                if (cursor > 65536) { data.erase (0, cursor); cursor = 0; }
            }
            else
            {
                const auto available = (int) (data.size() - cursor);
                if (available > 0)
                {
                    if (! onBody (data.data() + cursor, available)) return status;
                    bodyReceived += available;
                    data.clear();
                    cursor = 0;
                }
                if (contentLength >= 0 && bodyReceived >= contentLength)
                    return status;
            }
        }

        return status;
    }

    // Collects a whole (small) response body.
    juce::var getJson (const juce::String& host, int port, const juce::String& path, int timeoutMs, juce::String& error)
    {
        std::string body;
        const int status = httpRequest (host, port, "GET", path, {}, timeoutMs, timeoutMs,
                                        [&] (const char* bytes, int n) { if (n > 0) body.append (bytes, (size_t) n); return true; },
                                        error);
        if (status == 0)
            return {};
        if (status >= 400)
        {
            error = "HTTP " + juce::String (status);
            return {};
        }
        return juce::JSON::parse (juce::String::fromUTF8 (body.c_str(), (int) body.size()));
    }
}

//==============================================================================
OllamaBackend::OllamaBackend (juce::String modelToUse, juce::String hostToUse, int portToUse)
    : model (std::move (modelToUse)), host (std::move (hostToUse)), port (portToUse) {}

juce::StringArray OllamaBackend::listModels (const juce::String& host, int port, int timeoutMs)
{
    juce::String error;
    juce::StringArray names;
    const auto tags = getJson (host, port, "/api/tags", timeoutMs, error);
    if (auto* obj = tags.getDynamicObject())
        if (auto* models = obj->getProperty ("models").getArray())
            for (const auto& m : *models)
                if (auto* mo = m.getDynamicObject())
                    names.add (mo->getProperty ("name").toString());
    names.sort (true);
    return names;
}

bool OllamaBackend::isAvailable (juce::String& reason)
{
    const auto models = listModels (host, port, 2000);
    if (models.isEmpty())
    {
        reason = "Ollama isn't running on " + host + ":" + juce::String (port);
        return false;
    }

    for (const auto& m : models)
        if (m == model || m.startsWith (model + ":") || (model.endsWith (":latest") && m == model.upToLastOccurrenceOf (":latest", false, false)))
            return true;

    reason = "model '" + model + "' isn't installed (run: ollama pull " + model + ")";
    return false;
}

bool OllamaBackend::chat (const juce::String& systemPrompt, const juce::String& userPrompt,
                          const std::function<void (const juce::String&)>& onText,
                          const std::function<bool()>& shouldCancel, juce::String& error)
{
    auto buildBody = [&] (bool includeThink)
    {
        auto* sys = new juce::DynamicObject();
        sys->setProperty ("role", "system");
        sys->setProperty ("content", systemPrompt);
        auto* usr = new juce::DynamicObject();
        usr->setProperty ("role", "user");
        usr->setProperty ("content", userPrompt);

        auto* options = new juce::DynamicObject();
        options->setProperty ("temperature", 0.9);
        options->setProperty ("top_p", 0.95);
        options->setProperty ("repeat_penalty", 1.05);
        options->setProperty ("num_ctx", 8192);
        options->setProperty ("num_predict", 6000);

        // No "format": "json" on purpose: Ollama's JSON mode pretty-prints,
        // which costs ~30% more tokens; the prompt asks for compact JSON and
        // the parser tolerates stray text around it.
        auto* body = new juce::DynamicObject();
        body->setProperty ("model", model);
        body->setProperty ("stream", true);
        if (includeThink)
            body->setProperty ("think", false); // Qwen3 & co: skip the reasoning phase
        body->setProperty ("options", juce::var (options));
        body->setProperty ("messages", juce::var (juce::Array<juce::var> { juce::var (sys), juce::var (usr) }));
        return juce::JSON::toString (juce::var (body), true);
    };

    auto run = [&] (const juce::String& body, juce::String& streamError) -> int
    {
        std::string pending;
        bool done = false;

        auto handleLine = [&] (const std::string& line) -> bool
        {
            if (line.empty())
                return true;
            auto v = juce::JSON::parse (juce::String::fromUTF8 (line.c_str(), (int) line.size()));
            auto* obj = v.getDynamicObject();
            if (obj == nullptr)
                return true;
            if (obj->hasProperty ("error"))
            {
                streamError = obj->getProperty ("error").toString();
                return false;
            }
            if (auto* msg = obj->getProperty ("message").getDynamicObject())
            {
                const auto content = msg->getProperty ("content").toString();
                if (content.isNotEmpty())
                    onText (content);
            }
            if ((bool) obj->getProperty ("done"))
            {
                done = true;
                return false;
            }
            return true;
        };

        auto onBody = [&] (const char* bytes, int n) -> bool
        {
            if (shouldCancel && shouldCancel())
            {
                streamError = "cancelled";
                return false;
            }
            if (n > 0)
                pending.append (bytes, (size_t) n);

            size_t nl;
            while ((nl = pending.find ('\n')) != std::string::npos)
            {
                const auto line = pending.substr (0, nl);
                pending.erase (0, nl + 1);
                if (! handleLine (line))
                    return false;
            }
            return true;
        };

        juce::String connectError;
        const int status = httpRequest (host, port, "POST", "/api/chat", body, 5000, 240000, onBody, connectError);
        if (! done && streamError.isEmpty() && ! pending.empty())
            handleLine (pending); // an error body without a trailing newline
        if (status == 0)
            streamError = connectError;
        else if (status >= 400 && streamError.isEmpty())
            streamError = "HTTP " + juce::String (status);
        return status;
    };

    juce::String streamError;
    run (buildBody (true), streamError);

    // Models without a "thinking" mode reject the flag: retry once without it.
    if (streamError.containsIgnoreCase ("think"))
    {
        streamError.clear();
        run (buildBody (false), streamError);
    }

    if (streamError.isNotEmpty())
    {
        error = streamError;
        return false;
    }
    return true;
}

} // namespace stacks
