#include "LlmPatchGenerator.h"

#include <string>

namespace stacks
{

namespace
{
    juce::String numberText (float v)
    {
        if (std::abs (v - std::round (v)) < 1.0e-4f)
            return juce::String ((int) std::round (v));
        return juce::String (v, 3).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    }

    // Full values, minus connections that are switched off (saves a lot of tokens).
    juce::String compactParams (const Patch& p)
    {
        auto v = p.paramsToVar();
        if (auto* obj = v.getDynamicObject())
            for (int i = 0; i < kNumModSlots; ++i)
                if ((int) p.get (modSourceParam (i)) == SrcOff)
                {
                    obj->removeProperty (paramId (modSourceParam (i)));
                    obj->removeProperty (paramId (modDestParam (i)));
                    obj->removeProperty (paramId (modAmountParam (i)));
                }
        return juce::JSON::toString (v, true);
    }

    // "EchoingPad" -> "Echoing Pad"; models often drop the space.
    juce::String spaceOutCamelCase (const juce::String& name)
    {
        juce::String out;
        juce::juce_wchar previous = 0;
        for (auto c : name)
        {
            if (previous != 0 && juce::CharacterFunctions::isUpperCase (c)
                && juce::CharacterFunctions::isLowerCase (previous))
                out << ' ';
            out << juce::String::charToString (c);
            previous = c;
        }
        return out.trim();
    }

    juce::String variationPhrase (float v, bool evolving)
    {
        if (evolving)
        {
            if (v < 0.33f) return "subtly: adjust a handful of parameters, keep the character intact";
            if (v < 0.66f) return "moderately: change the timbre or the movement while keeping what made the parent appealing";
            return "boldly: take real risks with waveforms, FM and modulation, but keep every result musical";
        }
        if (v < 0.33f) return "Stay conventional and immediately playable.";
        if (v < 0.66f) return "Balance familiar sounds with a few surprises.";
        return "Be adventurous and experimental, but musical.";
    }

    // Pulls complete {...} objects out of the "patches" array while the JSON
    // is still streaming in, so each candidate can be shown immediately.
    class StreamingPatchParser
    {
    public:
        explicit StreamingPatchParser (std::function<void (const juce::var&)> onObject) : emit (std::move (onObject)) {}

        void feed (const juce::String& text)
        {
            buffer += text.toStdString();
            scan();
        }

        // Called once the stream has ended. Falls back to parsing the whole
        // reply if the incremental scan found nothing (e.g. a bare array).
        void finish()
        {
            scan();
            if (emitted > 0)
                return;

            auto text = juce::String::fromUTF8 (buffer.c_str(), (int) buffer.size()).trim();
            if (text.startsWith ("```"))
                text = text.fromFirstOccurrenceOf ("\n", false, false).upToLastOccurrenceOf ("```", false, false);
            auto whole = juce::JSON::parse (text);
            if (auto* obj = whole.getDynamicObject())
            {
                if (auto* arr = obj->getProperty ("patches").getArray())
                    for (const auto& item : *arr) emitOne (item);
                else if (obj->hasProperty ("params") || obj->hasProperty ("name"))
                    emitOne (whole);
            }
            else if (auto* arr = whole.getArray())
            {
                for (const auto& item : *arr) emitOne (item);
            }
        }

        int count() const { return emitted; }

        // The object being written right now (empty between patches).
        std::string currentObject() const
        {
            if (objectStart == std::string::npos || objectStart >= buffer.size()) return {};
            return buffer.substr (objectStart);
        }

    private:
        void emitOne (const juce::var& v)
        {
            if (v.getDynamicObject() != nullptr)
            {
                ++emitted;
                emit (v);
            }
        }

        void scan()
        {
            while (pos < buffer.size())
            {
                if (! inArray)
                {
                    const auto key = buffer.find ("\"patches\"", pos);
                    if (key == std::string::npos) { pos = buffer.size() > 10 ? buffer.size() - 10 : 0; return; }
                    const auto bracket = buffer.find ('[', key);
                    if (bracket == std::string::npos) { pos = key; return; }
                    inArray = true;
                    pos = bracket + 1;
                    continue;
                }

                const char c = buffer[pos];
                if (inString)
                {
                    if (escape)          escape = false;
                    else if (c == '\\')  escape = true;
                    else if (c == '"')   inString = false;
                }
                else if (c == '"')
                {
                    inString = true;
                }
                else if (c == '{')
                {
                    if (depth == 0) objectStart = pos;
                    ++depth;
                }
                else if (c == '}')
                {
                    if (depth > 0 && --depth == 0 && objectStart != std::string::npos)
                    {
                        const auto text = buffer.substr (objectStart, pos - objectStart + 1);
                        emitOne (juce::JSON::parse (juce::String::fromUTF8 (text.c_str(), (int) text.size())));
                        objectStart = std::string::npos;
                    }
                }
                else if (c == ']' && depth == 0)
                {
                    pos = buffer.size();
                    return;
                }
                ++pos;
            }
        }

        std::function<void (const juce::var&)> emit;
        std::string buffer;
        size_t pos = 0, objectStart = std::string::npos;
        int depth = 0, emitted = 0;
        bool inArray = false, inString = false, escape = false;
    };
}

//==============================================================================
LlmPatchGenerator::LlmPatchGenerator (std::shared_ptr<LlmBackend> b, std::shared_ptr<PatchGenerator> f)
    : backend (std::move (b)), fallback (std::move (f)) {}

juce::String LlmPatchGenerator::name() const
{
    return backend->name() + " " + backend->modelName();
}

juce::String LlmPatchGenerator::systemPrompt()
{
    juce::String s;
    s << "You are an expert sound designer programming Stacks, a polyphonic hybrid wavetable/FM synthesizer.\n"
      << "Signal path: oscillators A and B (morphing wavetables; B can frequency-modulate A), plus a sub oscillator and noise, "
      << "into a ladder filter with its own envelope, then the amplitude envelope, then chorus, delay and reverb. "
      << "Two LFOs each modulate one destination. Unison stacks detuned copies of a note for width.\n\n"
      << "Parameters (id: range [unit] - meaning):\n";

    // The four LFOs and twelve connections are described once each as families.
    for (const auto& spec : paramSpecs())
    {
        juce::String id (spec.id);
        if (id == "master_gain")
            continue;

        juce::String suffix;
        if (id.startsWith ("lfo"))
        {
            if (! id.startsWith ("lfo1_")) continue;
            id = "lfoN_" + id.fromFirstOccurrenceOf ("_", false, false);
            suffix = " (N = 1 to 4)";
        }
        else if (id.startsWith ("mod") && (id.endsWith ("_source") || id.endsWith ("_dest") || id.endsWith ("_amount")))
        {
            if (! id.startsWith ("mod1_")) continue;
            id = "modN_" + id.fromFirstOccurrenceOf ("_", false, false);
            suffix = " (N = 1 to 12)";
        }

        s << id << suffix << ": ";
        if (spec.kind == ParamKind::Choice)
            s << "one of " << spec.choices().joinIntoString (", ");
        else
            s << numberText (spec.min) << " to " << numberText (spec.max) << (spec.unit[0] != 0 ? juce::String (" ") + spec.unit : juce::String());
        s << " - " << spec.aiHint << "\n";
    }

    s << "\nWavetable characters: Sine = pure (morph adds warmth); Triangle = soft (morph skews it toward a saw); "
      << "Saw = classic (morph 0 is nearly a sine, 1 is razor sharp); Pulse = hollow (morph narrows the pulse); "
      << "Sync = aggressive hard-sync (morph raises the sync pitch); Organ = drawbars (morph changes the registration); "
      << "Formant = vocal (morph moves the formant up); Glass = sparse bell-like partials; "
      << "Fold = wavefolded sine (morph adds folds, saturated); Grit = noisy random harmonics (digital, lo-fi).\n"
      << "Modulation is routed with connections (modN_source, modN_dest, modN_amount); modN_dest is Pitch, Pitch B, Amp, Pan or the exact name of a knob, "
      << "and the amount is a fraction of that knob's travel (bipolar sources swing both ways). LFOs do nothing until a connection uses them. "
      << "Typical: LFO 1 > A Morph 0.2-0.4 for slow movement (lfo1_rate 0.05-0.5); LFO 1 > Pitch 0.02-0.05 with lfo1_rate 4-7 for vibrato; "
      << "Velocity > Cutoff 0.2-0.5 so dynamics matter; Mod Env > FM B>A or A Morph 0.3-0.6 for an evolving attack (menv_decay 0.2-1, menv_sustain 0); "
      << "Mod Wheel > Cutoff 0.3-0.6 for live control; LFO 2 > Chorus Mix or Reverb Mix 0.2-0.4 for effects that breathe; Random > A Morph 0.1-0.3 for per-note variation. "
      << "Every patch uses at least one connection; Velocity > Cutoff is the usual minimum.\n"
      << "Effects: reverb_type Shimmer with reverb_shimmer 0.3-0.7 gives a glowing octave-up halo (pads, textures); Hall for long tails, Room for short; "
      << "chorus_mode Ensemble is a lush string-machine, Dimension is wide and subtle, Flanger needs chorus_feedback 0.4-0.8; "
      << "delay_mode Ping-Pong with delay_sync 1/8 or 1/8D suits plucks and leads, Tape is dark and wobbly.\n"
      << "Musical guidance: pads want aenv_attack 0.3-2, long release, unison 3-4, chorus and reverb; "
      << "plucks want aenv_decay 0.1-0.6, aenv_sustain near 0, filter_env 2-4 with fenv_decay 0.05-0.4; "
      << "basses want sub_level, filter_cutoff 80-800, oscA_coarse -12; "
      << "bells want fm_amount 0.3-0.8 with oscB_coarse 7, 12, 19 or 24 (B as a silent modulator) and aenv_sustain 0; "
      << "leads want glide 0.03-0.15 and vibrato; "
      << "textures want Grit, Formant or Fold, some noise, LFOs routed to Morph A or FM, and long delay feedback.\n"
      << "Stay in tune with the played note: oscA_coarse only -24, -12, 0, 12 or 24; oscB_coarse at octaves when oscB_level is above 0.05, "
      << "or 7, 19 or 24 only when B is a silent FM modulator; keep oscA_fine within -12..12 and oscB_fine within -20..20 cents; "
      << "a slot to Pitch is only for vibrato (LFO, amount up to 0.08) or an attack pitch drop (Mod Env, amount up to 0.35); never Velocity, Key or Random to Pitch.\n"
      << "Avoid filter_res above 0.8 together with filter_drive above 4, and aenv_attack above 3.\n\n"
      << "Reply with JSON only, no prose, in exactly this shape:\n"
      << "{\"patches\": [{\"name\": \"Two Words\", \"category\": \"Pad\", \"description\": \"one vivid sentence about how it sounds\", "
      << "\"parent\": 1, \"params\": {\"oscA_wave\": \"Saw\", \"filter_cutoff\": 1200}}]}\n"
      << "category is one of Pad, Pluck, Bass, Keys, Lead, Bell, Texture, Drone. "
      << "In params list only the parameters that define the sound (usually 12 to 25); every parameter you omit keeps its base value. "
      << "Use plain numbers without units and the exact option names for choice parameters. "
      << "Make every patch in a batch clearly different from the others. "
      << "Write compact JSON on a single line with no indentation, no newlines, no markdown fences and nothing before or after it.";
    return s;
}

juce::String LlmPatchGenerator::userPrompt (const GenerationRequest& req)
{
    juce::String s;

    if (req.parents.empty())
    {
        s << "Create " << req.count << " patches. Anything you omit keeps its default (a plain saw into an open low-pass with a quick envelope, no effects, no modulation).\n";
        if (req.hint.trim().isNotEmpty())
            s << "Direction from the user: \"" << req.hint.trim() << "\". Follow it closely.\n";
        else
            s << "Cover a range of categories: pads, plucks, basses, keys, leads, bells, textures.\n";
        s << variationPhrase (req.variation, false) << "\n";
    }
    else
    {
        s << "The user picked these favourites:\n";
        for (int i = 0; i < (int) req.parents.size(); ++i)
        {
            const auto& parent = req.parents[(size_t) i];
            s << "Parent " << (i + 1) << " - \"" << parent.name << "\"";
            if (parent.category.isNotEmpty()) s << " (" << parent.category << ")";
            if (parent.description.isNotEmpty()) s << ": " << parent.description;
            s << "\nparams: " << compactParams (parent) << "\n";
        }
        s << "Create " << req.count << " descendants. Keep what makes the parents appealing and vary them "
          << variationPhrase (req.variation, true) << ". ";
        if (req.parents.size() > 1)
            s << "Let some descendants combine traits from two parents. ";
        s << "Set \"parent\" to the number of the parent whose values fill in anything you omit. "
          << "Give each descendant a fresh two-word name that shares one word with its parent.\n";
        if (req.hint.trim().isNotEmpty())
            s << "Direction from the user: \"" << req.hint.trim() << "\". Follow it closely.\n";
    }

    s << "The \"patches\" array must contain exactly " << req.count << " entries - do not stop early.\n"
      << "/no_think";
    return s;
}

juce::String LlmPatchGenerator::grammar (int patchCount)
{
    patchCount = juce::jlimit (1, 20, patchCount);
    auto lit = [] (const juce::String& text)
    {
        return "\"" + text.replace ("\\", "\\\\").replace ("\"", "\\\"") + "\"";
    };
    auto key = [&] (const juce::String& id) { return lit ("\"" + id + "\":"); };

    juce::String g;
    // Exactly patchCount entries: small models otherwise close the array early.
    g << "root ::= " << lit ("{\"patches\":[") << " patch";
    if (patchCount > 1)
        g << " (" << lit (",") << " patch){" << (patchCount - 1) << "}";
    g << " " << lit ("]}") << "\n"
      << "patch ::= " << lit ("{\"name\":") << " name " << lit (",\"category\":") << " category "
      << lit (",\"description\":") << " desc " << lit (",\"parent\":") << " int " << lit (",\"params\":{") << " params " << lit ("}}") << "\n"
      << "params ::= param (" << lit (",") << " param)*\n";

    // GBNF rule names may contain '-' but not '_', so "oscA_wave" becomes rule "p-oscA-wave".
    auto ruleName = [] (const char* id) { return "p-" + juce::String (id).replaceCharacter ('_', '-'); };

    juce::StringArray alternatives;
    for (const auto& spec : paramSpecs())
        if (std::string (spec.id) != "master_gain")
            alternatives.add (ruleName (spec.id));
    g << "param ::= " << alternatives.joinIntoString (" | ") << "\n";

    for (const auto& spec : paramSpecs())
    {
        if (std::string (spec.id) == "master_gain")
            continue;
        g << ruleName (spec.id) << " ::= " << key (spec.id) << " ";
        if (spec.kind == ParamKind::Choice)
        {
            juce::StringArray options;
            for (const auto& c : spec.choices())
                options.add (lit ("\"" + c + "\""));
            g << "(" << options.joinIntoString (" | ") << ")";
        }
        else if (std::string (spec.id) == "oscA_coarse")
        {
            g << "(\"-24\" | \"-12\" | \"0\" | \"12\" | \"24\")";
        }
        else if (std::string (spec.id) == "oscB_coarse")
        {
            g << "(\"-24\" | \"-12\" | \"0\" | \"7\" | \"12\" | \"19\" | \"24\")";
        }
        else if (spec.kind == ParamKind::Int)
        {
            g << "sint";
        }
        else
        {
            g << "num";
        }
        g << "\n";
    }

    juce::StringArray categories;
    for (auto* c : { "Pad", "Pluck", "Bass", "Keys", "Lead", "Bell", "Texture", "Drone" })
        categories.add (lit (juce::String ("\"") + c + "\""));
    g << "category ::= " << categories.joinIntoString (" | ") << "\n"
      << "name ::= \"\\\"\" nchar{2,30} \"\\\"\"\n"
      << "desc ::= \"\\\"\" dchar{10,220} \"\\\"\"\n"
      << "nchar ::= [A-Za-z0-9 '&-]\n"
      << "dchar ::= [^\"\\\\\\x00-\\x1F]\n"
      << "num ::= \"-\"? [0-9]{1,5} (\".\" [0-9]{1,4})?\n"
      << "int ::= [0-9]{1,2}\n"
      << "sint ::= \"-\"? [0-9]{1,2}\n";
    return g;
}

std::vector<Patch> LlmPatchGenerator::generate (const GenerationRequest& req, const GenerationProgress& progress)
{
    juce::String reason;
    if (! backend->isAvailable (reason))
    {
        progress.status ("AI unavailable (" + reason + ") - using Random");
        return fallback->generate (req, progress);
    }

    progress.status ("Asking " + backend->modelName() + "...");

    // Written up front so a failed run can still be inspected.
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("Application Support").getChildFile ("Stacks");
        dir.createDirectory();
        dir.getChildFile ("last-ai-prompt.txt").replaceWithText (systemPrompt() + "\n\n----- USER -----\n" + userPrompt (req));
        dir.getChildFile ("grammar.gbnf").replaceWithText (grammar (req.count));
    }

    std::vector<Patch> out;
    const Patch defaults;

    StreamingPatchParser parser ([&] (const juce::var& v)
    {
        // Ignore stray objects (a model quoting the format in prose, for instance).
        if (auto* obj = v.getDynamicObject(); obj == nullptr || ! (obj->hasProperty ("params") || obj->hasProperty ("name")))
            return;

        // Pick the base this patch builds on: the named parent, else parent 1, else defaults.
        const Patch* base = &defaults;
        if (! req.parents.empty())
        {
            base = &req.parents.front();
            if (auto* obj = v.getDynamicObject())
            {
                const int parentIndex = (int) obj->getProperty ("parent") - 1;
                if (parentIndex >= 0 && parentIndex < (int) req.parents.size())
                    base = &req.parents[(size_t) parentIndex];
            }
        }

        auto patch = Patch::fromVar (v, base);
        if (! patch)
            return;

        // Small models sometimes repeat themselves; a duplicate helps nobody.
        for (const auto& existing : out)
            if (existing.sameValuesAs (*patch) || existing.name.equalsIgnoreCase (patch->name))
                return;

        // A child wearing its parent's exact name gets a suffix.
        for (const auto& parent : req.parents)
            if (parent.name.equalsIgnoreCase (patch->name))
                patch->name << " II";

        keepPatchInTune (*patch);
        patch->set (P::master_gain, -6.0f);
        patch->origin = "AI";
        patch->name = spaceOutCamelCase (patch->name).substring (0, 28);
        if (patch->description.isEmpty())
            patch->description = describePatch (*patch);
        else if (patch->description.length() < 60)
            patch->description << " (" << describePatch (*patch) << ")";

        out.push_back (*patch);
        progress.patch (*patch);
        progress.status (juce::String (out.size()) + " of " + juce::String (req.count) + " from " + backend->modelName() + "...");
    });

    // Progress toward the next patch, read off the half-written JSON: the name
    // appears early, then each parameter is one more step toward ~18.
    double lastProgressMs = 0.0;
    auto reportProgress = [&] (bool force)
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (! force && now - lastProgressMs < 120.0)
            return;
        lastProgressMs = now;
        const auto obj = parser.currentObject();
        const int done = (int) out.size();
        if (obj.empty())
        {
            progress.progress (0.0f, done == 0 ? "Thinking about the first patch..." : "Thinking about patch " + juce::String (done + 1) + "...");
            return;
        }
        juce::String name;
        const auto nameAt = obj.find ("\"name\":\"");
        if (nameAt != std::string::npos)
        {
            const auto start = nameAt + 8;
            const auto end = obj.find ('"', start);
            if (end != std::string::npos)
                name = spaceOutCamelCase (juce::String::fromUTF8 (obj.data() + start, (int) (end - start)));
        }
        int keys = 0;
        for (size_t i = 0; (i = obj.find ("\":", i)) != std::string::npos; ++i) ++keys;
        const int params = juce::jmax (0, keys - 5);
        const float fraction = juce::jlimit (0.03f, 0.96f, 0.08f + (float) params / 20.0f);
        juce::String detail = "Writing patch " + juce::String (done + 1) + " of " + juce::String (req.count);
        if (name.isNotEmpty()) detail << ":  \"" << name << "\"";
        if (params > 0) detail << "  -  " << params << " settings so far";
        progress.progress (fraction, detail);
    };

    juce::String error, rawReply;
    const bool ok = backend->chat (systemPrompt(), userPrompt (req), grammar (req.count),
                                   [&] (const juce::String& delta) { rawReply << delta; parser.feed (delta); reportProgress (false); },
                                   [&] (const juce::String& phaseText) { progress.progress (-1.0f, phaseText); },
                                   progress.shouldCancel, error);
    parser.finish();
    progress.progress (-1.0f, {});

    // Keep the last exchange on disk: invaluable when tuning the prompt.
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("Application Support").getChildFile ("Stacks");
        dir.createDirectory();
        dir.getChildFile ("last-ai-reply.txt").replaceWithText (rawReply + (error.isNotEmpty() ? "\n\n----- ERROR -----\n" + error : juce::String()));
    }

    if (progress.cancelled())
        return out;

    if (out.empty())
    {
        progress.status ("AI gave nothing usable (" + (ok ? juce::String ("empty reply") : error) + ") - using Random");
        return fallback->generate (req, progress);
    }

    if ((int) out.size() > req.count)
        out.resize ((size_t) req.count);
    return out;
}

} // namespace stacks
