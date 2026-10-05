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

    const std::vector<const char*>& coreParamIds();
    bool isCoreParam (const char* id);

    // The parent as the model sees it: the core settings, whatever differs from
    // the defaults, and the connections that are switched on. Everything else is
    // a default the child inherits anyway - and every value shown here is one
    // the model tends to echo back, which is what used to overflow the context.
    juce::String compactParams (const Patch& p)
    {
        const Patch defaults;
        auto v = p.paramsToVar();
        if (auto* obj = v.getDynamicObject())
        {
            for (int i = 0; i < kNumModSlots; ++i)
                if ((int) p.get (modSourceParam (i)) == SrcOff || isMacroSource ((int) p.get (modSourceParam (i))))
                {
                    obj->removeProperty (paramId (modSourceParam (i)));
                    obj->removeProperty (paramId (modDestParam (i)));
                    obj->removeProperty (paramId (modAmountParam (i)));
                }
            const auto& specs = paramSpecs();
            for (int i = 0; i < kNumParams; ++i)
            {
                const juce::String id (specs[(size_t) i].id);
                if (id.startsWith ("mod") || isCoreParam (specs[(size_t) i].id) || id == "master_gain")
                    continue;
                if (std::abs (p.values[(size_t) i] - defaults.values[(size_t) i]) < 1.0e-4f)
                    obj->removeProperty (specs[(size_t) i].id);
            }
            obj->removeProperty ("master_gain");
        }
        return juce::JSON::toString (v, true);
    }

    // Settings every patch must state, in this order (the grammar enforces it),
    // followed by one real connection. Without this, small models write 4-5
    // values and no modulation; with too many forced values they fill in
    // numbers they don't understand instead of leaning on the defaults.
    const std::vector<const char*>& coreParamIds()
    {
        static const std::vector<const char*> ids {
            "oscA_wave", "oscA_morph", "oscA_level",
            "oscB_wave", "oscB_coarse", "oscB_level", "fm_amount", "sub_level",
            "filter_type", "filter_cutoff", "filter_res", "filter_env", "fenv_decay", "fenv_sustain",
            "aenv_attack", "aenv_decay", "aenv_sustain", "aenv_release",
            "unison_voices", "reverb_mix"
        };
        return ids;
    }

    bool isCoreParam (const char* id)
    {
        const std::string s (id);
        for (auto* c : coreParamIds())
            if (s == c)
                return true;
        return false;
    }

    // Connections the model may write: slots 1-6, each as one unit whose target
    // list fits its source. LFO 3/4 and slots 7-12 are left to the player.
    constexpr int kAiSlots = 6;
    const std::vector<const char*> kLfoTargets    { "A Morph", "B Morph", "Cutoff", "Resonance", "FM B>A", "Amp", "Pan", "Pitch", "Sub", "Noise",
                                                    "Chorus Mix", "Delay Mix", "Reverb Mix", "Shimmer", "Detune", "Drive", "A Warp Amt", "Cutoff 2" };
    const std::vector<const char*> kEnvTargets    { "Pitch", "Pitch B", "Cutoff", "A Morph", "B Morph", "FM B>A", "B Level", "Noise", "Resonance", "Drive", "A Warp Amt", "Cutoff 2" };
    const std::vector<const char*> kVelTargets    { "Cutoff", "Amp", "FM B>A", "A Morph", "Drive", "Resonance", "Decay", "Noise", "B Level" };
    const std::vector<const char*> kKeyTargets    { "Cutoff", "Pan", "A Morph", "Decay", "Release", "Detune" };
    const std::vector<const char*> kPerfTargets   { "Cutoff", "FM B>A", "A Morph", "B Morph", "Resonance", "Drive", "Chorus Mix", "Reverb Mix", "Delay Mix", "Amp", "A Warp Amt" };
    const std::vector<const char*> kRandomTargets { "A Morph", "B Morph", "Cutoff", "Pan", "Detune", "Decay", "FM B>A" };

    juce::String joinNames (const std::vector<const char*>& names)
    {
        juce::StringArray a;
        for (auto* n : names)
            if (modTargetNames().contains (n))
                a.add (n);
        return a.joinIntoString (", ");
    }

    // A setting the model may add after the core: not a connection key (those
    // come as units), not LFO 1/2's shape/rate/sync (they come with their
    // connection), not LFO 3/4 at all.
    bool isExtraParam (const char* id)
    {
        const juce::String s (id);
        if (s == "master_gain" || s == "bend_range" || s.startsWith ("smp_") || s.startsWith ("grain_") || isCoreParam (id) || s.startsWith ("mod") || s.startsWith ("lfo3_") || s.startsWith ("lfo4_") || s.startsWith ("macro") || s.startsWith ("arp_"))
            return false;
        if ((s.startsWith ("lfo1_") || s.startsWith ("lfo2_")) && (s.endsWith ("_shape") || s.endsWith ("_rate") || s.endsWith ("_sync")))
            return false;
        return true;
    }

    // One concrete direction of change per descendant. Small models asked to
    // "vary" a parent hand back near-copies; told exactly what to change they
    // don't. The Variation knob decides how far the list is allowed to go.
    juce::String descendantDirections (const GenerationRequest& req)
    {
        static const char* const gentle[] = {
            "keep its character but redesign the wavetable spectrum and move Morph with a slower LFO",
            "darker and softer: lower filter_cutoff by at least an octave, slower aenv_attack, more reverb",
            "brighter and snappier: open the filter, shorter aenv_decay and aenv_release, add a synced Ping-Pong delay",
            "wider: unison_voices 3-4 with unison_detune 15-30 and chorus_mix 0.3-0.5",
            "replace the LFO connection with a different target (Pan, Cutoff, Amp, Chorus Mix or Reverb Mix), a different shape and rate",
            "half speed: slower LFO rate, longer aenv_release, a longer, darker reverb",
        };
        static const char* const bolder[] = {
            "change oscillator B: another wave, oscB_coarse an octave or a fifth away as a silent FM modulator, fm_amount 0.3-0.6",
            "make it rhythmic: a Square or Random LFO synced to 1/8 or 1/16 on Cutoff or Pan at 0.4-0.6",
            "thin and resonant: less sub_level and fm_amount, filter_res 0.5-0.7, a touch of noise_level",
            "an inverted filter sweep: negative filter_env with a long fenv_decay, over a Plate reverb",
            "Mod Env > FM B>A 0.4-0.6 with menv_decay 0.3 for a metallic attack, then a soft body",
            "a tape-delay texture: delay_mode Tape, delay_feedback 0.5-0.7, delay_mix 0.4, chorus_mode Ensemble",
        };
        static const char* const wild[] = {
            "turn it into a Pluck with the same tone: aenv_sustain 0, aenv_decay 0.2-0.5, filter_env 2-4",
            "turn it into a Bass: oscA_coarse -12, sub_level 0.6-0.9, filter_cutoff 150-600, no reverb",
            "turn it into a Lead: glide 0.05-0.12, a Sine LFO vibrato on Pitch at 0.02-0.03 and 5-6 Hz, unison 2-3",
            "turn it into a Texture: Grit or Formant on oscillator A, noise, a Random LFO on A Morph, long delay feedback",
            "turn it into a Bell: fm_amount 0.5-0.8 with oscB_coarse 19 or 24 as a silent modulator, aenv_sustain 0, long release",
        };
        std::vector<const char*> pool (std::begin (gentle), std::end (gentle));
        if (req.variation > 0.33f) pool.insert (pool.end(), std::begin (bolder), std::end (bolder));
        if (req.variation > 0.66f) pool.insert (pool.end(), std::begin (wild), std::end (wild));

        juce::Random rng ((juce::int64) juce::Time::getHighResolutionTicks());
        std::vector<const char*> order (pool);
        for (int i = (int) order.size() - 1; i > 0; --i)
            std::swap (order[(size_t) i], order[(size_t) rng.nextInt (i + 1)]);

        juce::String s;
        for (int i = 0; i < req.count; ++i)
            s << "  Descendant " << (i + 1) << ": " << order[(size_t) i % order.size()] << ".\n";
        return s;
    }

    // The parent's oscillator spectra, so a descendant's table can be a relative of them.
    juce::String parentSpectra (const Patch& parent)
    {
        static juce::SharedResourcePointer<WavetableBank> bank;
        juce::String s;
        for (int osc = 0; osc < kNumOscs; ++osc)
        {
            if (osc > 0 && parent.get (oscLevelParam (osc)) <= 0.05f)
                continue;
            const int wave = (int) parent.get (oscWaveParam (osc));
            const juce::String key = Patch::waveKey (osc);
            if (wave == kCustomWave && ! parent.waves[(size_t) osc].isEmpty())
                s << key << " (its designed table): " << parent.waves[(size_t) osc].toJson (true) << "\n";
            else if (wave < WavetableBank::kNumBuiltIn)
                s << key << " (spectrum of its " << waveNames()[wave] << " table at morph 0, 0.5 and 1): "
                  << analyseWave ([&] (float m, float ph) { return bank->read (wave, 0, m, ph >= 1.0f ? 0.999f : ph); }, waveNames()[wave]).toJson (true) << "\n";
        }
        return s;
    }

    // "EchoingPad" -> "Echoing Pad"; models often drop the space.
    // "cold formant" -> "Cold Formant". Words that already start with a capital
    // (or digits, or "II") are left alone.
    juce::String titleCase (const juce::String& name)
    {
        juce::StringArray words;
        words.addTokens (name, " ", "");
        words.removeEmptyStrings();
        for (auto& w : words)
            if (juce::CharacterFunctions::isLowerCase (w[0]))
                w = w.substring (0, 1).toUpperCase() + w.substring (1);
        return words.joinIntoString (" ");
    }

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

juce::String LlmPatchGenerator::systemPrompt (bool designWaves)
{
    juce::String s;
    s << "You are an expert sound designer programming Stacks, a polyphonic hybrid wavetable/FM synthesizer.\n"
      << "Signal path: oscillators A and B (morphing wavetables; B can frequency-modulate A), plus a sub oscillator and noise, "
      << "into a ladder filter with its own envelope, then the amplitude envelope, then chorus, delay and reverb. "
      << "LFOs, a mod envelope, velocity, key, mod wheel, aftertouch and per-note random are wired to targets through connections. Unison stacks detuned copies of a note for width.\n\n"
      << "Parameters (id: range [unit] - meaning):\n";

    // The four LFOs and twelve connections are described once each as families.
    for (const auto& spec : paramSpecs())
    {
        juce::String id (spec.id);
        if (id == "master_gain" || id.startsWith ("macro") || id.startsWith ("arp_"))
            continue;   // the player's big knobs and the arp are not the model's business

        juce::String suffix;
        if (id.startsWith ("lfo"))
        {
            if (! id.startsWith ("lfo1_")) continue;
            id = "lfoN_" + id.fromFirstOccurrenceOf ("_", false, false);
            suffix = " (N = 1 or 2)";
        }
        else if (id.startsWith ("mod") && (id.endsWith ("_source") || id.endsWith ("_dest") || id.endsWith ("_amount")))
        {
            continue; // described as connections below
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
      << "Connections route modulation: modN_source, modN_dest, modN_amount (N = 1 to " << kAiSlots << "), written together. Each source only goes to targets that suit it:\n"
      << "  LFO 1 or LFO 2 > " << joinNames (kLfoTargets) << "  (write that LFO's lfoN_shape, lfoN_rate and lfoN_sync right before the connection)\n"
      << "  Filter Env or Mod Env > " << joinNames (kEnvTargets) << "  (Mod Env: write menv_decay and menv_sustain right before it)\n"
      << "  Velocity > " << joinNames (kVelTargets) << "\n"
      << "  Key > " << joinNames (kKeyTargets) << "\n"
      << "  Mod Wheel or Aftertouch > " << joinNames (kPerfTargets) << "\n"
      << "  Random (a new value on every note) > " << joinNames (kRandomTargets) << "\n"
      << "amount is -1 to 1: a fraction of the target knob's travel (on Pitch, 1 = 12 semitones). "
      << "Using LFOs well: a slow Sine or Triangle at 0.05-0.5 Hz on A Morph, Cutoff or Pan (0.2-0.4) gives pads and textures life; "
      << "a Sine at 4-7 Hz on Pitch at 0.02-0.03 is vibrato, on Amp at 0.2-0.4 tremolo; a Square or Random with lfoN_sync 1/8 or 1/16 on Cutoff, Pan or A Morph (0.3-0.5) chops rhythmically; "
      << "an LFO on Chorus Mix, Reverb Mix or Shimmer (0.2-0.4) makes the effects breathe. Never put a Square, Saw, Ramp or Random LFO on Pitch. "
      << "Velocity > Cutoff 0.2-0.5 makes playing dynamics matter; Mod Env (menv_decay 0.2-1, menv_sustain 0) > FM B>A or A Morph 0.3-0.6 gives an evolving attack; Mod Wheel > Cutoff 0.3-0.6 is live control. "
      << "Every patch has connection 1 from a performance source or envelope and connection 2 from an LFO - pick the LFO's target, shape and rate to suit the sound (slow and smooth for pads, synced and stepped for rhythm, 4-7 Hz Sine on Pitch or Amp for vibrato or tremolo); add a third or fourth connection if the sound needs it. Every connection should be clearly audible and musical.\n"
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
      << "Pitch is only modulated for vibrato (Sine LFO, amount up to 0.03) or an attack pitch drop (Mod Env with menv_sustain 0, amount up to 0.35); never Velocity, Key or Random to Pitch.\n"
      << "Avoid filter_res above 0.8 together with filter_drive above 4, and aenv_attack above 3.\n\n"
      << "Reply with JSON only, no prose. Shape (angle brackets are placeholders for your own choices, never copy them):\n"
      << "{\"patches\":[{\"name\":\"<two words>\",\"category\":\"<Pad|Pluck|Bass|Keys|Lead|Bell|Texture|Drone>\",\"tags\":[\"<word>\",\"<word>\",\"<word>\"],\"description\":\"<one vivid sentence about how it sounds>\",\"parent\":<1>,"
      << "\"params\":{\"oscA_wave\":\"<wave>\",\"oscA_morph\":<0-1>,\"oscA_level\":<0-1>,"
      << "\"oscB_wave\":\"<wave>\",\"oscB_coarse\":<semitones>,\"oscB_level\":<0-1>,\"fm_amount\":<0-1>,\"sub_level\":<0-1>,"
      << "\"filter_type\":\"<type>\",\"filter_cutoff\":<Hz>,\"filter_res\":<0-1>,\"filter_env\":<-5..5>,\"fenv_decay\":<seconds>,\"fenv_sustain\":<0-1>,"
      << "\"aenv_attack\":<seconds>,\"aenv_decay\":<seconds>,\"aenv_sustain\":<0-1>,\"aenv_release\":<seconds>,"
      << "\"unison_voices\":<1-4>,\"reverb_mix\":<0-1>,"
      << "\"mod1_source\":\"<Velocity|Key|Mod Wheel|Aftertouch|Filter Env|Mod Env|Random>\",\"mod1_dest\":\"<a target that suits it>\",\"mod1_amount\":<-1..1>,"
      << "\"lfo1_shape\":\"<shape>\",\"lfo1_rate\":<Hz>,\"lfo1_sync\":\"<Free or a note value>\",\"mod2_source\":\"LFO 1\",\"mod2_dest\":\"<a target from the LFO list>\",\"mod2_amount\":<-1..1>,"
      << "<any extra settings and connections>}";
    if (designWaves)
        s << ",\"waveA\":{\"name\":\"<two words>\",\"tail\":<0-9>,\"spectra\":[[<16 digits>],[<16 digits>],[<16 digits>]]}";
    s << "}]}\n"
      << "category is one of Pad, Pluck, Bass, Keys, Lead, Bell, Texture, Drone. tags are three lowercase words a producer would search for (mood, texture, use: warm, analog, intro). "
      << "params always starts with these core settings in this exact order: " << juce::StringArray (coreParamIds().data(), (int) coreParamIds().size()).joinIntoString (", ")
      << ", then connection 1 (Velocity, Key, Mod Wheel, Aftertouch, Filter Env, Mod Env or Random), then connection 2 (LFO 1 or LFO 2, with its shape, rate and sync), then any extra settings that define the sound "
      << "(more connections, oscA_coarse, noise, chorus and delay, effect details, glide, filter drive...). Every parameter you omit keeps its base value. ";
    if (designWaves)
        s << "\nWavetable design: every patch ends with \"waveA\", a brand-new wavetable for oscillator A (oscA_wave then becomes Custom). "
          << "Each spectrum lists the levels of harmonics 1-16 as digits 0-9 (9 = full, every step down is 4 dB quieter, 0 = silent); "
          << "give 2 to 4 spectra that differ - the Morph knob sweeps from the first to the last. "
          << "tail = how much the harmonics above 16 keep going (0 = mellow, 9 = as bright as a saw). "
          << "Recipes: saw 9,8,7,6,5,5,5,4,4,4,4,3,3,3,3,3 - square 9,0,7,0,6,0,5,0,5,0,4,0,4,0,4,0 - hollow 9,2,8,2,5,1,3,1,2,0,1,0,1,0,0,0 - "
          << "glassy bell 9,0,0,6,0,0,5,0,0,0,4,0,0,0,0,3 (sparse partials) - vocal 5,8,9,9,6,3,2,1,1,0,0,0,0,0,0,0 (a bump is a formant) - "
          << "organ 9,8,0,7,0,0,0,6,0,0,0,0,0,0,0,0. Invent your own that fits the sound and give it a two-word name; never copy a recipe exactly. "
          << "\"waveB\" and \"waveC\" (same format) are optional and design oscillator B or C instead of a built-in.\n";
    s
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
        if (req.brief.isNotEmpty())
            s << "Sound brief (your own notes on that direction - turn it into settings): " << req.brief << "\n";
        s << variationPhrase (req.variation, false) << "\n";
        if (req.designWaves)
            s << "Design a new wavetable (waveA) for each one.\n";
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
            if (req.designWaves)
                s << parentSpectra (parent);
        }
        s << "Create " << req.count << " descendants. Keep what makes the parents appealing and vary them "
          << variationPhrase (req.variation, true) << ". Each descendant has its own direction - follow it:\n"
          << descendantDirections (req);
        if (req.parents.size() > 1)
            s << "Let some descendants combine traits from two parents. ";
        s << "Set \"parent\" to the number of the parent whose values fill in anything you omit. "
          << "Give each descendant a fresh two-word name that shares one word with its parent.\n";
        if (req.designWaves)
            s << "Design each descendant's waveA as a relative of its parent's spectrum: keep the character, change the shape.\n";
        s << "Never copy a parent: a descendant must differ in at least " << juce::roundToInt (2.0f + 8.0f * req.variation)
          << " settings that you can hear (a knob moved by a quarter of its range or more, a different choice, a different connection), and its wavetable spectrum must change too.\n";
        if (req.hint.trim().isNotEmpty())
            s << "Direction from the user: \"" << req.hint.trim() << "\". Follow it closely.\n";
        if (req.brief.isNotEmpty())
            s << "Sound brief (your own notes on that direction - turn it into settings): " << req.brief << "\n";
    }

    s << "The \"patches\" array must contain exactly " << req.count << " entries - do not stop early.\n"
      << "/no_think";
    return s;
}

juce::String LlmPatchGenerator::grammar (int patchCount, bool designWaves)
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
      << lit (",\"tags\":[") << " tag " << lit (",") << " tag " << lit (",") << " tag " << lit ("]")
      << lit (",\"description\":") << " desc " << lit (",\"parent\":") << " int " << lit (",\"params\":{") << " params " << lit ("}")
      << (designWaves ? " " + lit (",\"waveA\":") + " wave (" + lit (",\"waveB\":") + " wave)? (" + lit (",\"waveC\":") + " wave)?" : juce::String())
      << " " << lit ("}") << "\n"
      << "params ::= core (" << lit (",") << " param)*\n";

    // GBNF rule names may contain '-' but not '_', so "oscA_wave" becomes rule "p-oscA-wave".
    auto ruleName = [] (const char* id) { return "p-" + juce::String (id).replaceCharacter ('_', '-'); };

    // The mandatory prefix: every core setting in order, then connection 1 with a live source and target.
    juce::StringArray coreRules;
    for (auto* id : coreParamIds())
        coreRules.add (ruleName (id));
    coreRules.add ("base-conn1");
    coreRules.add ("lfo-conn2");
    g << "core ::= " << coreRules.joinIntoString (" " + lit (",") + " ") << "\n";

    // Connections are units: source, target and amount together, with a target
    // list that fits the source, so an LFO can't land on a fine-tune and
    // velocity can't land on the reverb width. LFO units also carry that LFO's
    // shape, rate and sync; the Mod Env unit carries its decay and sustain.
    auto targetRule = [&] (const juce::String& rule, const std::vector<const char*>& names)
    {
        juce::StringArray options;
        for (auto* n : names)
            if (modTargetNames().contains (n))
                options.add (lit ("\"" + juce::String (n) + "\""));
        g << rule << " ::= " << options.joinIntoString (" | ") << "\n";
    };
    targetRule ("lfo-target", kLfoTargets);
    targetRule ("env-target", kEnvTargets);
    targetRule ("vel-target", kVelTargets);
    targetRule ("key-target", kKeyTargets);
    targetRule ("perf-target", kPerfTargets);
    targetRule ("rnd-target", kRandomTargets);

    for (int n = 1; n <= kAiSlots; ++n)
    {
        const juce::String N (n);
        auto unit = [&] (const juce::String& rule, const juce::String& prefix, const juce::String& sourceAlternatives, const juce::String& target)
        {
            g << rule << N << " ::= " << prefix << lit ("\"mod" + N + "_source\":") << " " << sourceAlternatives << " "
              << lit (",\"mod" + N + "_dest\":") << " " << target << " " << lit (",\"mod" + N + "_amount\":") << " num\n";
        };
        const juce::String comma = " " + lit (",") + " ";
        unit ("lfo1-conn", "p-lfo1-shape" + comma + "p-lfo1-rate" + comma + "p-lfo1-sync" + comma, lit ("\"LFO 1\""), "lfo-target");
        unit ("lfo2-conn", "p-lfo2-shape" + comma + "p-lfo2-rate" + comma + "p-lfo2-sync" + comma, lit ("\"LFO 2\""), "lfo-target");
        unit ("fenv-conn", "", lit ("\"Filter Env\""), "env-target");
        unit ("menv-conn", "p-menv-decay" + comma + "p-menv-sustain" + comma, lit ("\"Mod Env\""), "env-target");
        unit ("vel-conn",  "", lit ("\"Velocity\""), "vel-target");
        unit ("key-conn",  "", lit ("\"Key\""), "key-target");
        unit ("perf-conn", "", "(" + lit ("\"Mod Wheel\"") + " | " + lit ("\"Aftertouch\"") + ")", "perf-target");
        unit ("rnd-conn",  "", lit ("\"Random\""), "rnd-target");
        g << "lfo-conn" << N << " ::= lfo1-conn" << N << " | lfo2-conn" << N << "\n"
          << "base-conn" << N << " ::= fenv-conn" << N << " | menv-conn" << N << " | vel-conn" << N << " | key-conn" << N << " | perf-conn" << N << " | rnd-conn" << N << "\n"
          << "conn" << N << " ::= base-conn" << N << " | lfo-conn" << N << "\n";
    }

    // Extras: anything that is not already part of the core, plus more connections.
    juce::StringArray alternatives;
    for (const auto& spec : paramSpecs())
        if (isExtraParam (spec.id))
            alternatives.add (ruleName (spec.id));
    for (int n = 3; n <= kAiSlots; ++n)
        alternatives.add ("conn" + juce::String (n));
    g << "param ::= " << alternatives.joinIntoString (" | ") << "\n";

    // Only rules the grammar references.
    auto isUsed = [&] (const char* id)
    {
        const juce::String s (id);
        return isCoreParam (id) || isExtraParam (id) || s == "menv_decay" || s == "menv_sustain"
            || ((s.startsWith ("lfo1_") || s.startsWith ("lfo2_")) && (s.endsWith ("_shape") || s.endsWith ("_rate") || s.endsWith ("_sync")));
    };

    for (const auto& spec : paramSpecs())
    {
        if (! isUsed (spec.id))
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
      << "tag ::= \"\\\"\" [a-z] [a-z0-9 -]{1,13} \"\\\"\"\n"
      << "desc ::= \"\\\"\" dchar{10,220} \"\\\"\"\n"
      << "nchar ::= [A-Za-z0-9 '&-]\n"
      << "dchar ::= [^\"\\\\\\x00-\\x1F]\n"
      << "num ::= \"-\"? [0-9]{1,5} (\".\" [0-9]{1,4})?\n"
      << "int ::= [0-9]{1,2}\n"
      << "sint ::= \"-\"? [0-9]{1,2}\n";
    if (designWaves)
        g << "wave ::= " << lit ("{\"name\":") << " name " << lit (",\"tail\":") << " digit " << lit (",\"spectra\":[")
          << " spectrum (" << lit (",") << " spectrum){1,3} " << lit ("]}") << "\n"
          << "spectrum ::= " << lit ("[") << " digit (" << lit (",") << " digit){15} " << lit ("]") << "\n"
          << "digit ::= [0-9]\n";
    return g;
}

std::vector<Patch> LlmPatchGenerator::generate (const GenerationRequest& request, const GenerationProgress& progress)
{
    juce::String reason;
    if (! backend->isAvailable (reason))
    {
        progress.status ("AI unavailable (" + reason + ") - using Random");
        return fallback->generate (request, progress);
    }

    // A style or artist reference ("like MGMT", "80s Italo bass") is a lot to
    // ask of a small model mid-JSON. So first it writes a few sentences about
    // what that should sound like in synth terms, and designs from those.
    GenerationRequest req = request;
    if (req.hint.trim().isNotEmpty() && ! progress.cancelled())
    {
        const auto hint = req.hint.trim();
        progress.status ("Thinking about \"" + hint + "\"...");
        progress.progress (0.0f, "Thinking about what \"" + hint + "\" should sound like...");
        juce::String brief, briefError;
        const juce::String briefGrammar = "root ::= [\\x20-\\x7E\\n]{80,700}\n";
        // Same system prompt as the patch call, so the model's cache of it is reused.
        backend->chat (systemPrompt (req.designWaves),
                       "Before designing anything: in three or four short sentences, describe the synthesizer sound(s) for this request: \"" + hint + "\". "
                       "Be concrete: waveforms and layering, filter and envelope shape, movement (what the LFOs modulate and how fast), effects, and the vibe. "
                       "If it names an artist, song or genre, describe the synth sounds they are known for. No preamble. /no_think",
                       briefGrammar,
                       [&] (const juce::String& t) { brief += t; },
                       [&] (const juce::String& phase) { progress.status (phase); },
                       [&] { return progress.cancelled() || brief.length() > 900; },
                       briefError);
        req.brief = brief.replace ("\n", " ").trim().substring (0, 900);
        if (progress.cancelled())
            return {};
    }

    progress.status ("Asking " + backend->modelName() + "...");

    // Written up front so a failed run can still be inspected.
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("Application Support").getChildFile ("Stacks");
        dir.createDirectory();
        dir.getChildFile ("last-ai-prompt.txt").replaceWithText (systemPrompt (req.designWaves) + "\n\n----- USER -----\n" + userPrompt (req));
        dir.getChildFile ("grammar.gbnf").replaceWithText (grammar (req.count, req.designWaves));
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

        // A designed table switches its oscillator to Custom; Custom without a table falls back to a saw.
        if (auto* obj = v.getDynamicObject())
            for (int osc = 0; osc < kNumOscs; ++osc)
            {
                const P waveParam = oscWaveParam (osc);
                if (obj->hasProperty (Patch::waveKey (osc)) && ! patch->waves[(size_t) osc].isEmpty())
                    patch->set (waveParam, (float) kCustomWave);
                if ((int) patch->get (waveParam) == kCustomWave && patch->waves[(size_t) osc].isEmpty())
                    patch->set (waveParam, 2.0f);
            }

        patch->name = titleCase (spaceOutCamelCase (patch->name)).substring (0, 28);
        for (auto& w : patch->waves)
            if (! w.isEmpty()) w.name = titleCase (spaceOutCamelCase (w.name)).substring (0, 24);   // "glassy bell" -> "Glassy Bell"
        // Small models sometimes copy the example patch's name straight from the prompt.
        if (patch->name.equalsIgnoreCase ("Velvet Horizon") || patch->name.equalsIgnoreCase ("Two Words") || patch->name.isEmpty())
            patch->name = patch->category + " " + juce::String ((int) out.size() + 1);

        // A child wearing its parent's exact name gets a suffix.
        for (const auto& parent : req.parents)
            if (parent.name.equalsIgnoreCase (patch->name))
                patch->name << " II";

        keepPatchInTune (*patch);
        applyPromptCues (req.hint, *patch);   // the chorus a "lush chorus" prompt asked for, etc.

        // Small models repeat themselves - a parent verbatim, or a sibling. Rather
        // than dropping the copy (and shorting the batch), nudge it into a relative.
        auto isCopy = [&] (const Patch& other) { return other.sameValuesAs (*patch) || other.name.equalsIgnoreCase (patch->name); };
        bool copy = false;
        for (const auto& parent : req.parents) copy = copy || isCopy (parent);
        for (const auto& sibling : out)        copy = copy || isCopy (sibling);
        if (copy)
        {
            const auto seed = (juce::int64) juce::Time::getHighResolutionTicks() + (juce::int64) out.size();
            mutatePatch (*patch, 0.25f + 0.5f * req.variation, seed);
            if (! patch->waves[0].isEmpty())
            {
                patch->waves[0] = mutateWave (patch->waves[0], 0.25f + 0.5f * req.variation, seed);
                patch->set (P::oscA_wave, (float) kCustomWave);
            }
            keepPatchInTune (*patch);
            for (const auto& sibling : out)
                if (sibling.name.equalsIgnoreCase (patch->name))
                    patch->name << " II";
        }

        // Descendants must be audibly different from the parent and from each
        // other; the model drifts toward near-copies, so push them apart.
        if (! req.parents.empty())
        {
            const int wanted = juce::roundToInt (2.0f + 8.0f * req.variation);
            const auto seed = (juce::int64) juce::Time::getHighResolutionTicks() + (juce::int64) out.size() * 7919;
            auto tooClose = [&]
            {
                if (countAudibleDifferences (*patch, *base) < wanted) return true;
                for (const auto& sibling : out)
                    if (countAudibleDifferences (*patch, sibling) < juce::jmax (2, wanted / 2)) return true;
                return false;
            };
            for (int attempt = 0; attempt < 4 && tooClose(); ++attempt)
            {
                mutatePatch (*patch, 0.3f + 0.4f * req.variation, seed + attempt);
                if (! patch->waves[0].isEmpty() && patch->waves[0] == base->waves[0])
                    patch->waves[0] = mutateWave (patch->waves[0], 0.4f + 0.4f * req.variation, seed + attempt);
                if (! patch->waves[0].isEmpty())
                    patch->set (P::oscA_wave, (float) kCustomWave);
                keepPatchInTune (*patch);
            }
        }

        ensureMacroRoutings (*patch);
        patch->set (P::master_gain, -6.0f);
        patch->origin = "AI";
        patch->prompt = req.hint.trim();
        patch->parentName = req.parents.empty() ? juce::String() : base->name;
        if (patch->tags.isEmpty()) patch->tags = autoTags (*patch);
        for (const auto& parent : req.parents)   // the model copied the parent's blurb: describe the child instead
            if (patch->description.length() > 20 && parent.description.startsWithIgnoreCase (patch->description))
                patch->description.clear();
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
                name = titleCase (spaceOutCamelCase (juce::String::fromUTF8 (obj.data() + start, (int) (end - start))));
        }
        int keys = 0;
        for (size_t i = 0; (i = obj.find ("\":", i)) != std::string::npos; ++i) ++keys;
        const int params = juce::jmax (0, keys - 6);
        const float fraction = juce::jlimit (0.03f, 0.96f, 0.08f + (float) params / 20.0f);
        juce::String detail = "Writing patch " + juce::String (done + 1) + " of " + juce::String (req.count);
        if (name.isNotEmpty()) detail << ":  \"" << name << "\"";
        if (params > 0) detail << "  -  " << params << " settings so far";
        progress.progress (fraction, detail);
    };

    juce::String error, rawReply;
    const bool ok = backend->chat (systemPrompt (req.designWaves), userPrompt (req), grammar (req.count, req.designWaves),
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
