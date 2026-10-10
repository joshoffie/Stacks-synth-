#include "QuickTweak.h"

namespace stacks
{

namespace
{
    juce::String shortValue (const ParamSpec& s, float v)
    {
        const juce::String unit (s.unit);
        if (s.kind == ParamKind::Choice) return s.choices()[juce::jlimit (0, s.choices().size() - 1, (int) std::round (v))];
        if (unit == "Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (v));
        if (unit == "s")  return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + "ms" : juce::String (v, 2) + "s";
        if (unit == "dB") return juce::String (v, 1) + "dB";
        if (s.kind == ParamKind::Int) return juce::String ((int) std::round (v));
        return juce::String (v, 2);
    }

    // The edits a request can ask for, built up against the current sound.
    struct Edits
    {
        const Patch& current;
        std::vector<TweakChange> changes;
        float strength = 1.0f;

        float value (P p) const
        {
            for (const auto& c : changes) if (c.paramIndex == (int) p) return c.value;
            return current.get (p);
        }
        void put (P p, float v)
        {
            Patch scratch = current;
            scratch.set (p, v);               // clamps and rounds for us
            v = scratch.get (p);
            for (auto& c : changes) if (c.paramIndex == (int) p) { c.value = v; return; }
            changes.push_back ({ (int) p, v });
        }
        // Moves a knob by a fraction of its travel (normalised, so a skewed
        // cutoff moves musically).
        void nudge (P p, float fraction)
        {
            const auto& range = paramRange ((int) p);
            const float norm = range.convertTo0to1 (juce::jlimit (range.start, range.end, value (p)));
            put (p, range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, norm + fraction * strength)));
        }
        void scale (P p, float factor)   { put (p, value (p) * std::pow (factor, strength)); }
        void atLeast (P p, float v)      { if (value (p) < v) put (p, v); }
        void atMost (P p, float v)       { if (value (p) > v) put (p, v); }
        void set (P p, float v)          { put (p, v); }
        void setAmpEnv (float a, float d, float su, float r)    { set (P::aenv_attack, a); set (P::aenv_decay, d); set (P::aenv_sustain, su); set (P::aenv_release, r); }
        void setFilterEnv (float a, float d, float su, float r) { set (P::fenv_attack, a); set (P::fenv_decay, d); set (P::fenv_sustain, su); set (P::fenv_release, r); }

        bool hasConnection (int src, int dst) const
        {
            for (int i = 0; i < kNumModSlots; ++i)
                if ((int) current.get (modSourceParam (i)) == src && (int) current.get (modDestParam (i)) == dst)
                    return true;
            return false;
        }
        // A connection in the first free slot; false when the matrix is full.
        bool addConnection (int src, int dst, float amount)
        {
            for (int i = 0; i < kNumModSlots; ++i)
                if ((int) current.get (modSourceParam (i)) == SrcOff || (int) current.get (modDestParam (i)) == TargetOff)
                {
                    set (modSourceParam (i), (float) src);
                    set (modDestParam (i), (float) dst);
                    set (modAmountParam (i), amount);
                    return true;
                }
            return false;
        }
        // Slow motion on A Morph from the first LFO nobody uses.
        void addMovement (float amount, float rateHz)
        {
            int lfo = -1;
            for (int k = 0; k < kNumLfos && lfo < 0; ++k) if (! lfoInUse (k)) lfo = k;
            if (lfo < 0) return;
            set (lfoShapeParam (lfo), (float) ShapeSine);
            set (lfoSyncParam (lfo), 0.0f);
            set (lfoRateParam (lfo), rateHz);
            addConnection (SrcLfo1 + lfo, modTargetForParam ((int) P::oscA_morph), amount);
        }

        // Every connection from an LFO, scaled; `pitchOnly` limits it to vibrato.
        void scaleLfoAmounts (float factor, bool pitchOnly)
        {
            for (int i = 0; i < kNumModSlots; ++i)
            {
                const int src = (int) current.get (modSourceParam (i));
                const int dst = (int) current.get (modDestParam (i));
                if (! (src >= SrcLfo1 && src <= SrcLfo4) || dst == TargetOff) continue;
                if (pitchOnly != (dst == TargetPitch || dst == TargetPitchB)) continue;
                put (modAmountParam (i), current.get (modAmountParam (i)) * std::pow (factor, strength));
            }
        }
        void scaleFreeLfoRates (float factor)
        {
            for (int k = 0; k < kNumLfos; ++k)
                if ((int) current.get (lfoSyncParam (k)) == 0 && lfoInUse (k))
                    scale (lfoRateParam (k), factor);
        }
        bool lfoInUse (int k) const
        {
            for (int i = 0; i < kNumModSlots; ++i)
                if ((int) current.get (modSourceParam (i)) == SrcLfo1 + k && (int) current.get (modDestParam (i)) != TargetOff)
                    return true;
            return false;
        }
        bool hasVibrato() const
        {
            for (int i = 0; i < kNumModSlots; ++i)
            {
                const int src = (int) current.get (modSourceParam (i));
                const int dst = (int) current.get (modDestParam (i));
                if (src >= SrcLfo1 && src <= SrcLfo4 && (dst == TargetPitch || dst == TargetPitchB)) return true;
            }
            return false;
        }
        // Vibrato from scratch: a free LFO at 5.5 Hz on Pitch, in a free slot.
        void addVibrato (float amount)
        {
            int lfo = -1;
            for (int k = 0; k < kNumLfos && lfo < 0; ++k) if (! lfoInUse (k)) lfo = k;
            if (lfo < 0) lfo = 3;
            for (int i = 0; i < kNumModSlots; ++i)
                if ((int) current.get (modSourceParam (i)) == SrcOff || (int) current.get (modDestParam (i)) == TargetOff)
                {
                    set (lfoShapeParam (lfo), (float) ShapeSine);
                    set (lfoSyncParam (lfo), 0.0f);
                    set (lfoRateParam (lfo), 5.5f);
                    set (modSourceParam (i), (float) (SrcLfo1 + lfo));
                    set (modDestParam (i), (float) TargetPitch);
                    set (modAmountParam (i), amount * strength);
                    return;
                }
        }
    };

    bool hasWord (const juce::StringArray& words, std::initializer_list<const char*> options)
    {
        for (auto* o : options)
            if (words.contains (o))
                return true;
        return false;
    }

    // "more reverb", "less delay", "no chorus", "a lot more space"
    int moreOrLess (const juce::StringArray& words, std::initializer_list<const char*> nouns)
    {
        for (int i = 0; i < words.size(); ++i)
        {
            bool noun = false;
            for (auto* n : nouns) if (words[i] == n || words[i] == juce::String (n) + "s") noun = true;
            if (! noun) continue;
            for (int j = juce::jmax (0, i - 3); j < i; ++j)
            {
                if (words[j] == "more" || words[j] == "extra" || words[j] == "add" || words[j] == "some" || words[j] == "with") return 1;
                if (words[j] == "less" || words[j] == "fewer" || words[j] == "reduce" || words[j] == "lower" || words[j] == "drop") return -1;
                if (words[j] == "no" || words[j] == "without" || words[j] == "remove" || words[j] == "kill") return -2;
            }
            if (i + 1 < words.size() && (words[i + 1] == "up" || words[i + 1] == "down"))
                return words[i + 1] == "up" ? 1 : -1;
            return 1;   // "reverb" on its own: a bit more of it
        }
        return 0;
    }
}

namespace
{
    // Like moreOrLess, but a noun on its own counts for nothing: "bass" is a
    // sound type, "more bass" is the low end.
    int qualified (const juce::StringArray& words, std::initializer_list<const char*> nouns)
    {
        for (int i = 0; i < words.size(); ++i)
        {
            bool noun = false;
            for (auto* n : nouns) if (words[i] == n || words[i] == juce::String (n) + "s") noun = true;
            if (! noun) continue;
            for (int j = juce::jmax (0, i - 3); j < i; ++j)
            {
                if (words[j] == "more" || words[j] == "extra" || words[j] == "add" || words[j] == "some") return 1;
                if (words[j] == "less" || words[j] == "fewer" || words[j] == "reduce" || words[j] == "lower" || words[j] == "drop") return -1;
                if (words[j] == "no" || words[j] == "without" || words[j] == "remove" || words[j] == "kill") return -2;
            }
            if (i + 1 < words.size() && (words[i + 1] == "up" || words[i + 1] == "down"))
                return words[i + 1] == "up" ? 1 : -1;
        }
        return 0;
    }
}

bool ruleTweak (const juce::String& request, const Patch& current, std::vector<TweakChange>& changes, juce::String& summary)
{
    juce::StringArray words;
    words.addTokens (request.toLowerCase().replaceCharacters (",.;:!?/-", "        "), " ", "");
    words.removeEmptyStrings();
    words.trim();
    if (words.isEmpty())
        return false;

    Edits e { current, {}, 1.0f };
    if (hasWord (words, { "slightly", "slight", "bit", "little", "touch", "tad", "subtle", "subtly", "hint" })) e.strength = 0.5f;
    if (hasWord (words, { "much", "way", "lot", "lots", "very", "really", "massively", "far", "heavily", "double" })) e.strength = 2.0f;
    juce::StringArray understood;

    // A sound type by name ("short pluck", "make it a pad", "like a bass") reshapes
    // the envelopes and the filter to that type and keeps the oscillators; "short"
    // and "long" pick its tighter or longer version. Adjectives below then move
    // things relative to that.
    const bool wantsShort = hasWord (words, { "short", "shorter", "tight", "tighter", "snappy", "snappier", "quick", "staccato" });
    const bool wantsLong  = hasWord (words, { "long", "longer", "sustained", "slow", "slower", "lush", "huge", "big", "epic" });
    juce::String archetype;
    if (hasWord (words, { "pluck", "plucks", "plucky", "plucked" }))                                            archetype = "pluck";
    else if (hasWord (words, { "stab", "stabs", "stabby" }))                                                    archetype = "stab";
    else if (hasWord (words, { "pad", "pads" }))                                                                archetype = "pad";
    else if (hasWord (words, { "bass", "sub", "subbass", "bassline", "808" }) && qualified (words, { "bass", "sub" }) == 0
             && ! hasWord (words, { "bassier", "deeper", "heavier" }))                                           archetype = "bass";
    else if (hasWord (words, { "lead", "leads", "solo" }))                                                      archetype = "lead";
    else if (hasWord (words, { "keys", "piano", "rhodes", "epiano", "wurli", "clav", "clavinet", "organ" }))    archetype = "keys";
    else if (hasWord (words, { "bell", "bells", "chime", "chimes", "mallet", "marimba", "vibes", "glock" }))     archetype = "bell";
    else if (hasWord (words, { "drone", "drones", "texture", "textures", "ambient", "atmosphere", "atmospheric", "soundscape" })) archetype = "drone";
    else if (hasWord (words, { "perc", "percussion", "drum", "drums", "hit", "hits", "kick", "tom", "snare" })) archetype = "perc";
    else if (hasWord (words, { "arp", "arpeggio", "arpeggiated", "arpeggiate", "sequence", "sequenced" }))      archetype = "arp";

    if (archetype == "pluck")
    {
        const float decay = wantsShort ? 0.12f : wantsLong ? 0.5f : 0.25f;
        e.setAmpEnv (0.002f, decay, 0.0f, wantsShort ? 0.12f : 0.25f);
        e.setFilterEnv (0.001f, decay * 0.8f, 0.1f, 0.2f);
        if (current.get (P::filter_env) < 2.0f) e.set (P::filter_env, 3.0f);
        if (current.get (P::filter_cutoff) > 3000.0f) e.set (P::filter_cutoff, 1500.0f);
        else if (current.get (P::filter_cutoff) < 300.0f) e.set (P::filter_cutoff, 600.0f);
        e.atMost (P::reverb_mix, 0.25f);
        e.set (P::glide, 0.0f);
        if ((int) current.get (P::unison_voices) > 4) e.set (P::unison_voices, 3.0f);
        understood.add (wantsShort ? "short pluck" : wantsLong ? "long pluck" : "pluck");
    }
    else if (archetype == "stab")
    {
        e.setAmpEnv (0.002f, wantsLong ? 0.4f : 0.2f, 0.0f, 0.15f);
        e.setFilterEnv (0.001f, 0.15f, 0.0f, 0.15f);
        if (current.get (P::filter_env) < 1.5f) e.set (P::filter_env, 2.0f);
        if (current.get (P::filter_cutoff) < 1500.0f) e.set (P::filter_cutoff, 3000.0f);
        if ((int) current.get (P::unison_voices) < 3) { e.set (P::unison_voices, 4.0f); e.atLeast (P::unison_detune, 15.0f); e.atLeast (P::unison_spread, 0.6f); }
        e.atLeast (P::chorus_mix, 0.2f);
        e.set (P::glide, 0.0f);
        understood.add ("stab");
    }
    else if (archetype == "pad")
    {
        e.setAmpEnv (wantsShort ? 0.15f : wantsLong ? 1.2f : 0.5f, 1.0f, 0.85f, wantsLong ? 3.0f : wantsShort ? 0.8f : 1.5f);
        e.setFilterEnv (0.3f, 1.0f, 0.6f, 1.0f);
        if (current.get (P::filter_env) > 2.0f) e.set (P::filter_env, 1.0f);
        if ((int) current.get (P::unison_voices) < 3) { e.set (P::unison_voices, 4.0f); e.atLeast (P::unison_detune, 12.0f); e.atLeast (P::unison_spread, 0.6f); }
        e.atLeast (P::chorus_mix, 0.25f);
        e.atLeast (P::reverb_mix, 0.35f);
        e.atLeast (P::reverb_size, 0.6f);
        e.set (P::glide, 0.0f);
        if (! e.lfoInUse (0) && ! e.lfoInUse (1)) e.addMovement (0.3f, 0.2f);
        understood.add (wantsLong ? "long pad" : "pad");
    }
    else if (archetype == "bass")
    {
        const bool sub = hasWord (words, { "sub", "subbass", "808" });
        if (current.get (P::oscA_coarse) >= 0.0f) e.set (P::oscA_coarse, -12.0f);
        if (sub) { e.set (P::oscA_wave, 0.0f); e.set (P::sub_level, 0.8f); e.set (P::filter_cutoff, 300.0f); }   // Sine, a lot of sub, closed
        else     { e.atLeast (P::sub_level, 0.5f); if (current.get (P::filter_cutoff) > 1000.0f) e.set (P::filter_cutoff, 600.0f); }
        e.setAmpEnv (0.003f, 0.3f, wantsShort ? 0.3f : 0.6f, wantsShort ? 0.08f : 0.15f);
        e.setFilterEnv (0.001f, 0.25f, 0.2f, 0.15f);
        if (current.get (P::filter_env) < 1.0f && ! sub) e.set (P::filter_env, 1.5f);
        if ((int) current.get (P::unison_voices) > 2) e.set (P::unison_voices, 1.0f);
        e.atMost (P::reverb_mix, 0.1f);
        e.atMost (P::delay_mix, 0.1f);
        e.atMost (P::chorus_mix, 0.1f);
        understood.add (sub ? "sub bass" : "bass");
    }
    else if (archetype == "lead")
    {
        e.setAmpEnv (0.01f, 0.3f, 0.75f, wantsLong ? 0.6f : 0.25f);
        e.setFilterEnv (0.005f, 0.3f, 0.5f, 0.3f);
        e.atLeast (P::glide, 0.05f);
        if (! e.hasVibrato()) e.addVibrato (0.02f);
        if ((int) current.get (P::unison_voices) > 3) e.set (P::unison_voices, 2.0f);
        if (current.get (P::filter_cutoff) < 1500.0f) e.set (P::filter_cutoff, 2500.0f);
        if (current.get (P::filter_env) > 3.0f) e.set (P::filter_env, 1.5f);
        e.atMost (P::reverb_mix, 0.3f);
        if (current.get (P::delay_mix) < 0.1f) e.set (P::delay_mix, 0.25f);
        understood.add ("lead");
    }
    else if (archetype == "keys")
    {
        e.setAmpEnv (0.003f, wantsLong ? 1.8f : 0.9f, 0.25f, 0.45f);
        e.setFilterEnv (0.002f, 0.5f, 0.2f, 0.4f);
        e.set (P::filter_env, 1.5f);
        if (current.get (P::filter_cutoff) < 1500.0f) e.set (P::filter_cutoff, 3000.0f);
        if (! e.hasConnection (SrcVelocity, modTargetForParam ((int) P::filter_cutoff))) e.addConnection (SrcVelocity, modTargetForParam ((int) P::filter_cutoff), 0.4f);
        if ((int) current.get (P::unison_voices) > 2) e.set (P::unison_voices, 1.0f);
        e.atLeast (P::reverb_mix, 0.2f);
        e.atMost (P::chorus_mix, 0.2f);
        e.set (P::glide, 0.0f);
        understood.add ("keys");
    }
    else if (archetype == "bell")
    {
        if (current.get (P::fm_amount) < 0.3f) e.set (P::fm_amount, 0.5f);
        e.set (P::oscB_coarse, current.get (P::oscB_level) <= 0.05f ? 19.0f : 12.0f);
        e.setAmpEnv (0.002f, wantsLong ? 2.5f : 1.4f, 0.0f, 1.2f);
        e.setFilterEnv (0.001f, 1.0f, 0.0f, 1.0f);
        e.set (P::filter_env, 0.0f);
        e.atLeast (P::filter_cutoff, 6000.0f);
        e.atLeast (P::reverb_mix, 0.3f);
        e.set (P::unison_voices, 1.0f);
        e.set (P::glide, 0.0f);
        understood.add ("bell");
    }
    else if (archetype == "drone")
    {
        e.setAmpEnv (1.5f, 2.0f, 1.0f, 3.0f);
        e.setFilterEnv (1.0f, 2.0f, 0.8f, 2.0f);
        e.atLeast (P::reverb_mix, 0.45f);
        e.atLeast (P::reverb_size, 0.75f);
        e.atLeast (P::chorus_mix, 0.2f);
        if (! e.lfoInUse (0) && ! e.lfoInUse (1)) e.addMovement (0.35f, 0.12f);
        understood.add ("drone");
    }
    else if (archetype == "perc")
    {
        e.setAmpEnv (0.001f, wantsLong ? 0.25f : 0.1f, 0.0f, 0.08f);
        e.setFilterEnv (0.001f, 0.08f, 0.0f, 0.05f);
        e.set (P::filter_env, 4.0f);
        if (! e.hasConnection (SrcModEnv, TargetPitch))
        {
            e.set (P::menv_attack, 0.001f); e.set (P::menv_decay, 0.08f); e.set (P::menv_sustain, 0.0f); e.set (P::menv_release, 0.05f);
            e.addConnection (SrcModEnv, TargetPitch, 0.35f);   // the pitch drop that makes a hit
        }
        e.atMost (P::reverb_mix, 0.2f);
        e.set (P::unison_voices, 1.0f);
        e.set (P::glide, 0.0f);
        understood.add ("percussive");
    }
    else if (archetype == "arp")
    {
        e.set (P::arp_mode, 1.0f);                       // Up
        e.set (P::arp_rate, wantsShort ? 3.0f : 1.0f);    // 1/16 or 1/8
        e.set (P::arp_gate, 0.6f);
        if (current.get (P::aenv_sustain) > 0.6f || current.get (P::aenv_attack) > 0.05f)
            e.setAmpEnv (0.003f, 0.3f, 0.3f, 0.2f);
        understood.add ("arpeggiated");
    }

    // Tone
    if (hasWord (words, { "brighter", "bright", "brighten", "open", "opener", "airy", "airier", "sparkle", "sparklier", "crisper", "crisp", "sharper" }))
    {
        e.nudge (P::filter_cutoff, 0.12f);
        e.nudge (P::eq_high_gain, 0.08f);
        if ((int) current.get (P::filter_routing) != 0) e.nudge (P::filter2_cutoff, 0.1f);
        understood.add ("brighter");
    }
    if (hasWord (words, { "darker", "dark", "darken", "duller", "dull", "muffled", "muddier", "mellow", "mellower", "softer-top", "rounder" }))
    {
        e.nudge (P::filter_cutoff, -0.12f);
        e.nudge (P::eq_high_gain, -0.08f);
        understood.add ("darker");
    }
    if (hasWord (words, { "warmer", "warm", "warmth" }))
    {
        e.nudge (P::filter_cutoff, -0.06f);
        e.nudge (P::eq_low_gain, 0.08f);
        e.nudge (P::eq_high_gain, -0.04f);
        if (current.get (P::dist_mix) < 0.05f) { e.set (P::dist_mode, 0.0f); e.set (P::dist_drive, 6.0f); e.set (P::dist_mix, 0.15f * e.strength); }
        understood.add ("warmer");
    }
    if (hasWord (words, { "colder", "cold", "icy", "icier", "glassy", "glassier", "thinner", "thin" }))
    {
        e.nudge (P::eq_low_gain, -0.1f);
        e.nudge (P::filter_cutoff, 0.06f);
        e.nudge (P::sub_level, -0.3f);
        understood.add ("thinner");
    }

    // Idioms
    if (hasWord (words, { "underwater", "submerged", "drowned", "muffled" }))
    {
        e.nudge (P::filter_cutoff, -0.3f);
        e.nudge (P::filter_res, 0.08f);
        e.atLeast (P::reverb_mix, 0.4f * e.strength);
        e.atLeast (P::reverb_size, 0.7f);
        e.atLeast (P::chorus_mix, 0.25f);
        understood.add ("underwater");
    }
    if (hasWord (words, { "dreamy", "dreamier", "dreamlike", "ethereal", "floaty", "hazy", "hazier", "washy", "washed" }))
    {
        e.put (P::aenv_attack, juce::jmax (0.4f * e.strength, current.get (P::aenv_attack)));
        e.scale (P::aenv_release, 2.0f);
        e.atLeast (P::reverb_mix, 0.4f * e.strength);
        e.atLeast (P::reverb_size, 0.7f);
        e.atLeast (P::chorus_mix, 0.25f);
        understood.add ("dreamier");
    }
    if (hasWord (words, { "lofi", "lo-fi", "vintage", "retro", "old", "dusty", "worn", "tape", "cassette", "vinyl", "record" }))
    {
        e.nudge (P::filter_cutoff, -0.1f);
        e.nudge (P::eq_high_gain, -0.15f);
        e.atLeast (P::noise_level, 0.06f * e.strength);
        if (current.get (P::dist_mix) < 0.15f) { e.set (P::dist_mode, 2.0f); e.set (P::dist_drive, 8.0f); e.set (P::dist_mix, 0.2f * e.strength); }
        understood.add ("lo-fi");
    }
    if (hasWord (words, { "8-bit", "8bit", "chiptune", "bitcrushed", "crushed", "digital", "glitchy" }))
    {
        e.set (P::dist_mode, 4.0f);   // Crush
        e.atLeast (P::dist_drive, 14.0f);
        e.atLeast (P::dist_mix, 0.5f * e.strength);
        understood.add ("crushed");
    }
    if (hasWord (words, { "hollow", "hollower", "nasal", "boxy" }))
    {
        e.nudge (P::eq_low_gain, -0.1f);
        e.nudge (P::eq_mid_gain, 0.2f);
        understood.add ("hollower");
    }

    // Width
    if (hasWord (words, { "wider", "wide", "widen", "stereo", "bigger", "huge", "massive" }))
    {
        e.nudge (P::unison_spread, 0.25f);
        if ((int) current.get (P::unison_voices) < 3) e.set (P::unison_voices, 3.0f);
        if (current.get (P::chorus_mix) < 0.1f) e.set (P::chorus_mix, 0.25f * e.strength); else e.nudge (P::chorus_mix, 0.15f);
        understood.add ("wider");
    }
    if (hasWord (words, { "narrower", "narrow", "mono", "centred", "centered", "tighter-stereo", "smaller" }))
    {
        e.nudge (P::unison_spread, -0.3f);
        e.nudge (P::chorus_mix, -0.2f);
        understood.add ("narrower");
    }
    if (hasWord (words, { "fatter", "fat", "thicker", "thick", "beefier", "fuller", "chunkier" }))
    {
        e.set (P::unison_voices, (float) juce::jmin (16, (int) current.get (P::unison_voices) + (e.strength >= 2.0f ? 4 : 2)));
        e.nudge (P::unison_detune, 0.1f);
        e.nudge (P::sub_level, 0.2f);
        understood.add ("fatter");
    }

    // Envelope. A named stage ("shorter release", "less decay", "longer attack")
    // moves only that stage; "shorter" / "longer" on their own shape the whole note.
    const bool shorterWord = hasWord (words, { "shorter", "short", "snappier", "snappy", "tighter", "staccato", "clipped", "quicker", "faster", "less", "lower", "reduce", "drop" });
    const bool longerWord  = hasWord (words, { "longer", "long", "sustained", "held", "drawn", "lingering", "slower", "more", "extra", "bigger", "raise", "higher" });
    const bool stageNamed  = hasWord (words, { "release", "tail", "decay", "attack", "sustain" });
    if (stageNamed && (shorterWord || longerWord))
    {
        const float factor = shorterWord ? 0.5f : 2.0f;
        if (hasWord (words, { "release", "tail" })) { e.scale (P::aenv_release, factor); understood.add (shorterWord ? "shorter release" : "longer release"); }
        if (hasWord (words, { "decay" }))           { e.scale (P::aenv_decay, factor);   understood.add (shorterWord ? "shorter decay" : "longer decay"); }
        if (hasWord (words, { "attack" }))          { e.scale (P::aenv_attack, factor);  understood.add (shorterWord ? "faster attack" : "slower attack"); }
        if (hasWord (words, { "sustain" }))         { e.nudge (P::aenv_sustain, shorterWord ? -0.25f : 0.25f); understood.add (shorterWord ? "less sustain" : "more sustain"); }
    }
    else if (archetype.isEmpty() && hasWord (words, { "shorter", "short", "snappier", "snappy", "tighter", "staccato", "clipped" }))
    {
        e.scale (P::aenv_release, 0.5f);
        e.scale (P::aenv_decay, 0.7f);
        if (current.get (P::aenv_sustain) > 0.6f) e.nudge (P::aenv_sustain, -0.25f);
        understood.add ("shorter");
    }
    else if (archetype.isEmpty() && hasWord (words, { "longer", "long", "sustained", "held", "drawn", "lingering" }))
    {
        e.scale (P::aenv_release, 2.0f);
        if (current.get (P::aenv_sustain) < 0.3f) e.nudge (P::aenv_sustain, 0.25f);
        understood.add ("longer");
    }
    // The filter by name: "open the filter", "lower the cutoff", "less filter".
    if (hasWord (words, { "cutoff", "filter" }) && ! stageNamed)
    {
        const bool down = hasWord (words, { "lower", "less", "close", "closed", "down", "darker", "reduce" });
        const bool up   = hasWord (words, { "higher", "more", "open", "up", "raise", "brighter" });
        if (down != up)
        {
            e.nudge (P::filter_cutoff, down ? -0.12f : 0.12f);
            understood.add (down ? "cutoff down" : "cutoff up");
        }
    }
    if (hasWord (words, { "volume", "level", "gain" }) && ! hasWord (words, { "louder", "quieter" }))
    {
        const bool down = hasWord (words, { "lower", "less", "down", "reduce", "quieter" });
        e.nudge (P::master_gain, down ? -0.1f : 0.1f);
        understood.add (down ? "quieter" : "louder");
    }
    if (hasWord (words, { "punchier", "punchy", "punch", "harder-hitting", "attacky", "percussive", "plucky", "pluckier" }))
    {
        e.scale (P::aenv_attack, 0.3f);
        if (current.get (P::filter_env) < 1.5f) e.set (P::filter_env, 1.5f + e.strength);
        e.scale (P::fenv_decay, 0.6f);
        e.scale (P::aenv_decay, 0.8f);
        understood.add ("punchier");
    }
    if (hasWord (words, { "softer", "soft", "gentler", "gentle", "smoother", "smooth", "slower-attack", "swell", "swelling", "pad-like", "padlike" }))
    {
        e.put (P::aenv_attack, juce::jmax (0.08f * e.strength, current.get (P::aenv_attack) * 2.0f));
        e.nudge (P::dist_mix, -0.15f);
        e.nudge (P::filter_res, -0.08f);
        understood.add ("softer");
    }

    // Character
    if (hasWord (words, { "dirtier", "dirty", "grittier", "gritty", "harsher", "harsh", "aggressive", "angrier", "angry", "distorted", "crunchier", "crunchy", "saturated", "driven", "overdriven", "nastier", "nasty", "rougher", "rough" }))
    {
        if (current.get (P::dist_mix) < 0.2f) e.set (P::dist_mix, 0.35f * e.strength); else e.nudge (P::dist_mix, 0.2f);
        e.nudge (P::dist_drive, 0.15f);
        e.nudge (P::filter_drive, 0.1f);
        understood.add ("dirtier");
    }
    if (hasWord (words, { "cleaner", "clean", "purer", "pure", "undistorted", "tidier" }))
    {
        e.set (P::dist_mix, 0.0f);
        e.set (P::filter_drive, 1.0f);
        understood.add ("cleaner");
    }
    {
        const int res = moreOrLess (words, { "resonance", "resonant", "squelch", "squelchy", "acid", "peak" });
        if (res != 0)
        {
            e.nudge (P::filter_res, res > 0 ? 0.15f : res == -2 ? -1.0f : -0.15f);
            understood.add (res > 0 ? "more resonance" : "less resonance");
        }
    }
    {
        const int sub = qualified (words, { "bass", "sub", "low", "lows", "bottom", "weight", "deep" });
        if (sub != 0 || hasWord (words, { "deeper", "bassier", "heavier" }))
        {
            const bool more = sub > 0 || sub == 0;
            if (more) { e.nudge (P::sub_level, 0.3f); e.nudge (P::eq_low_gain, 0.12f); }
            else      { e.nudge (P::sub_level, sub == -2 ? -1.0f : -0.3f); e.nudge (P::eq_low_gain, -0.12f); }
            understood.add (more ? "more bass" : "less bass");
        }
    }
    {
        const int noise = moreOrLess (words, { "noise", "noisy", "noisier", "hiss", "breath", "breathy", "breathier", "air" });
        if (noise != 0)
        {
            e.nudge (P::noise_level, noise > 0 ? 0.15f : noise == -2 ? -1.0f : -0.15f);
            understood.add (noise > 0 ? "more noise" : "less noise");
        }
    }
    {
        const int fm = moreOrLess (words, { "fm", "metallic", "bell-like", "clangy", "clangier", "inharmonic" });
        if (fm != 0)
        {
            e.nudge (P::fm_amount, fm > 0 ? 0.15f : fm == -2 ? -1.0f : -0.15f);
            understood.add (fm > 0 ? "more FM" : "less FM");
        }
    }

    // Effects
    auto effect = [&] (std::initializer_list<const char*> nouns, P mix, const char* label)
    {
        const int dir = moreOrLess (words, nouns);
        if (dir == 0) return;
        if (dir == -2)      e.set (mix, 0.0f);
        else if (dir < 0)   e.nudge (mix, -0.2f);
        else if (current.get (mix) < 0.08f) e.set (mix, 0.3f * e.strength);
        else                e.nudge (mix, 0.2f);
        understood.add (juce::String (dir > 0 ? "more " : dir == -2 ? "no " : "less ") + label);
    };
    effect ({ "reverb", "verb", "room", "hall", "space", "spacious", "ambience", "ambient", "atmosphere" }, P::reverb_mix, "reverb");
    effect ({ "delay", "echo", "echoes", "repeats" }, P::delay_mix, "delay");
    effect ({ "chorus", "shimmer", "ensemble", "lush", "lusher" }, P::chorus_mix, "chorus");
    effect ({ "compression", "compressor", "glue", "squash", "squashed" }, P::comp_mix, "compression");
    effect ({ "distortion", "drive", "overdrive", "fuzz", "crunch", "grit" }, P::dist_mix, "distortion");
    if (hasWord (words, { "wetter", "wet" }))  { e.nudge (P::reverb_mix, 0.2f); e.nudge (P::delay_mix, 0.1f); understood.add ("wetter"); }
    if (hasWord (words, { "drier", "dry" }))   { e.nudge (P::reverb_mix, -0.25f); e.nudge (P::delay_mix, -0.2f); understood.add ("drier"); }

    // Movement
    {
        const int mov = moreOrLess (words, { "movement", "motion", "moving", "wobble", "wobbly", "modulation", "animated", "animation", "alive", "evolving" });
        if (mov != 0)
        {
            if (mov > 0 && ! e.lfoInUse (0) && ! e.lfoInUse (1))
            {
                // Nothing moves yet: a slow sine on A Morph.
                for (int i = 0; i < kNumModSlots; ++i)
                    if ((int) current.get (modSourceParam (i)) == SrcOff || (int) current.get (modDestParam (i)) == TargetOff)
                    {
                        e.set (lfoShapeParam (0), (float) ShapeSine); e.set (lfoSyncParam (0), 0.0f); e.set (lfoRateParam (0), 0.25f);
                        e.set (modSourceParam (i), (float) SrcLfo1); e.set (modDestParam (i), (float) modTargetForParam ((int) P::oscA_morph)); e.set (modAmountParam (i), 0.3f * e.strength);
                        break;
                    }
            }
            else
                e.scaleLfoAmounts (mov > 0 ? 1.6f : mov == -2 ? 0.0f : 0.6f, false);
            understood.add (mov > 0 ? "more movement" : "less movement");
        }
    }
    {
        const int vib = moreOrLess (words, { "vibrato", "wobble-pitch" });
        if (vib != 0)
        {
            if (vib > 0 && ! e.hasVibrato()) e.addVibrato (0.02f);
            else e.scaleLfoAmounts (vib > 0 ? 1.5f : vib == -2 ? 0.0f : 0.6f, true);
            understood.add (vib > 0 ? "more vibrato" : "less vibrato");
        }
    }
    if (! stageNamed && archetype.isEmpty() && hasWord (words, { "faster", "fast", "speedier", "busier" }))  { e.scaleFreeLfoRates (1.6f); e.scale (P::chorus_rate, 1.3f); understood.add ("faster"); }
    if (! stageNamed && archetype.isEmpty() && hasWord (words, { "slower", "slow", "lazier", "calmer", "calm" })) { e.scaleFreeLfoRates (0.6f); e.scale (P::chorus_rate, 0.8f); understood.add ("slower"); }

    // Pitch and voice
    if (hasWord (words, { "octave" }) || hasWord (words, { "higher", "lower" }))
    {
        const bool down = hasWord (words, { "down", "lower", "below" });
        for (int osc = 0; osc < kNumOscs; ++osc)
            if (osc == 0 || current.get (oscLevelParam (osc)) > 0.05f)
                e.put (oscCoarseParam (osc), current.get (oscCoarseParam (osc)) + (down ? -12.0f : 12.0f));
        e.put (P::smp_coarse, current.get (P::smp_coarse) + (down ? -12.0f : 12.0f));
        understood.add (down ? "octave down" : "octave up");
    }
    if (hasWord (words, { "glide", "portamento", "slide", "legato" }))
    {
        const int dir = moreOrLess (words, { "glide", "portamento", "slide", "legato" });
        if (dir == -2 || dir < 0) e.set (P::glide, dir == -2 ? 0.0f : current.get (P::glide) * 0.5f);
        else e.put (P::glide, juce::jmax (0.08f * e.strength, current.get (P::glide) * 1.5f));
        understood.add (dir < 0 ? "less glide" : "glide");
    }
    {
        const int det = moreOrLess (words, { "detune", "detuned", "unison", "voices" });
        if (det != 0)
        {
            e.nudge (P::unison_detune, det > 0 ? 0.15f : -0.15f);
            if (det > 0 && (int) current.get (P::unison_voices) < 3) e.set (P::unison_voices, 3.0f);
            understood.add (det > 0 ? "more detune" : "less detune");
        }
    }
    if (hasWord (words, { "louder", "loud" }))   { e.nudge (P::master_gain, 0.1f); understood.add ("louder"); }
    if (hasWord (words, { "quieter", "quiet" })) { e.nudge (P::master_gain, -0.1f); understood.add ("quieter"); }

    if (understood.isEmpty() || e.changes.empty())
        return false;
    changes = e.changes;
    summary = understood.joinIntoString (", ");
    summary = summary.substring (0, 1).toUpperCase() + summary.substring (1) + ": " + describeChanges (current, changes);
    return true;
}

juce::String describeChanges (const Patch& current, const std::vector<TweakChange>& changes)
{
    juce::StringArray parts;
    for (const auto& c : changes)
    {
        if (c.paramIndex < 0 || c.paramIndex >= kNumParams) continue;
        const auto& s = paramSpecs()[(size_t) c.paramIndex];
        const juce::String id (s.id);
        if (id.startsWith ("mod") && (id.endsWith ("_source") || id.endsWith ("_dest"))) continue;   // the amount line says it
        parts.add (juce::String (s.name) + " " + shortValue (s, current.get ((P) c.paramIndex)) + juce::String::fromUTF8 (" \xe2\x86\x92 ") + shortValue (s, c.value));
        if (parts.size() >= 6) { parts.add (juce::String::fromUTF8 ("\xe2\x80\xa6")); break; }
    }
    return parts.joinIntoString (", ");
}

//==============================================================================
namespace
{
    // The settings the model may touch: every knob that is not a connection,
    // a macro, the master or fine print behind a "more" button.
    bool tweakable (const ParamSpec& s)
    {
        const juce::String id (s.id);
        if (id == "master_gain" || id.startsWith ("macro") || id.startsWith ("mod")) return false;
        if (s.kind == ParamKind::Choice) return false;
        return ! isAdvancedParam (s.id);
    }
}

juce::String tweakSystemPrompt()
{
    juce::String s;
    s << "You adjust ONE existing patch on Stacks, a wavetable/FM synthesizer, by a small amount. "
      << "The user asks for a change in plain words; reply with the fewest settings that achieve it (1 to 6), keeping the sound's character. "
      << "\"slightly\" means a small move, \"much\" a big one. Values are plain numbers in the unit given, within the range.\n"
      << "Settings (id: min to max [unit] - meaning):\n";
    for (const auto& spec : paramSpecs())
    {
        if (! tweakable (spec)) continue;
        juce::String hint (spec.aiHint);
        hint = hint.upToFirstOccurrenceOf (";", false, false).upToFirstOccurrenceOf (" (", false, false);
        s << spec.id << ": " << juce::String (spec.min) << " to " << juce::String (spec.max)
          << (spec.unit[0] != 0 ? juce::String (" ") + spec.unit : juce::String()) << " - " << hint << "\n";
    }
    s << "How to read a request (move 2 to 6 settings, each by a clear but tasteful amount, relative to the current values; never touch what the request does not mention):\n"
      << "- Tone: brighter = filter_cutoff up (x1.5 to x2) and eq_high_gain +2 to +4; darker = the opposite; warmer = cutoff a little down, eq_low_gain +2, dist_mix 0.1 to 0.2 (Soft); thinner/colder = sub_level and eq_low_gain down.\n"
      << "- Size: wider = unison_spread up, unison_voices 3 to 4, chorus_mix 0.2 to 0.4; fatter = unison_voices +2, unison_detune up, sub_level up; narrower/mono = unison_spread and chorus_mix down.\n"
      << "- Length: shorter = aenv_release and aenv_decay down (sustain down if it is high); longer = aenv_release up, sustain up if it is low; a named stage (release, decay, attack, sustain) moves only that stage.\n"
      << "- Shape: punchier = aenv_attack 0.002, filter_env 2 to 4 with fenv_decay 0.05 to 0.3; softer/smoother = aenv_attack 0.1 to 0.5, dist_mix and filter_res down.\n"
      << "- Character: dirtier/aggressive = dist_mix 0.3 to 0.6, dist_drive up, filter_drive up, filter_res up a little; cleaner = dist_mix 0, filter_drive 1; more resonance = filter_res up 0.15; metallic = fm_amount up.\n"
      << "- Movement: faster/slower = lfoN_rate, chorus_rate and delay_time; more movement = lfoN_rate 0.1 to 0.5 with mod amounts up (you cannot add connections, only move the settings listed).\n"
      << "- Space: reverb, delay, chorus or compression up/down = that effect's _mix (0 = off); wetter/drier = reverb_mix and delay_mix together; bigger room = reverb_size up.\n"
      << "- A sound type reshapes the envelopes and filter to that type and keeps the oscillators:\n"
      << "  pluck: aenv 0.002 / 0.25 / 0 / 0.25, fenv_decay 0.2, fenv_sustain 0.1, filter_env 3, filter_cutoff 600 to 2000, reverb_mix under 0.25\n"
      << "  stab: aenv 0.002 / 0.2 / 0 / 0.15, unison_voices 4, filter_env 2, filter_cutoff 3000\n"
      << "  pad: aenv 0.5 / 1 / 0.85 / 1.5 to 3, unison_voices 4 with unison_detune 12+, chorus_mix 0.3, reverb_mix 0.4\n"
      << "  bass: oscA_coarse -12, sub_level 0.6, filter_cutoff 300 to 800, aenv 0.003 / 0.3 / 0.6 / 0.15, unison_voices 1, reverb_mix and chorus_mix 0\n"
      << "  lead: aenv 0.01 / 0.3 / 0.75 / 0.25, glide 0.05, filter_cutoff 2000 to 6000, delay_mix 0.25\n"
      << "  keys: aenv 0.003 / 0.9 / 0.25 / 0.45, filter_env 1.5, fenv_decay 0.5, filter_cutoff 3000\n"
      << "  bell: fm_amount 0.5 with oscB_coarse 12 or 19, aenv 0.002 / 1.4 / 0 / 1.2, filter_cutoff 6000+\n"
      << "  drone: aenv 1.5 / 2 / 1 / 3, reverb_mix 0.5, reverb_size 0.8, chorus_mix 0.2\n"
      << "  perc: aenv 0.001 / 0.1 / 0 / 0.08, filter_env 4, fenv_decay 0.08\n"
      << "  \"short\" or \"long\" with a type picks its tighter or longer version (decay and release).\n"
      << "- Amount words: slightly / a bit = small moves (10 to 20% of the range); much / way / a lot = big moves; plain requests sit between. Several requests in one line each get their changes.\n"
      << "Examples (current cutoff 2000, reverb_mix 0.1, aenv_attack 0.01):\n"
      << "\"underwater\" -> {\"changes\":[{\"id\":\"filter_cutoff\",\"value\":350},{\"id\":\"reverb_mix\",\"value\":0.5},{\"id\":\"reverb_size\",\"value\":0.8},{\"id\":\"chorus_mix\",\"value\":0.3}]}\n"
      << "\"dreamier\" -> {\"changes\":[{\"id\":\"aenv_attack\",\"value\":0.6},{\"id\":\"aenv_release\",\"value\":2},{\"id\":\"reverb_mix\",\"value\":0.45},{\"id\":\"chorus_mix\",\"value\":0.3}]}\n"
      << "\"more aggressive\" -> {\"changes\":[{\"id\":\"dist_mix\",\"value\":0.5},{\"id\":\"dist_drive\",\"value\":22},{\"id\":\"filter_res\",\"value\":0.45},{\"id\":\"aenv_attack\",\"value\":0.002}]}\n"
      << "\"like an old record\" -> {\"changes\":[{\"id\":\"filter_cutoff\",\"value\":3500},{\"id\":\"noise_level\",\"value\":0.08},{\"id\":\"dist_mix\",\"value\":0.2},{\"id\":\"eq_high_gain\",\"value\":-4}]}\n"
      << "Reply with JSON only: {\"changes\":[{\"id\":\"<setting id>\",\"value\":<number>}]}";
    return s;
}

juce::String tweakUserPrompt (const Patch& current, const juce::String& request)
{
    juce::String s;
    s << "Current settings: ";
    juce::StringArray parts;
    for (const auto& spec : paramSpecs())
        if (tweakable (spec))
            parts.add (juce::String (spec.id) + "=" + juce::String (current.get ((P) paramIndexForId (spec.id)), 3).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."));
    s << parts.joinIntoString (" ") << "\n"
      << "Request: \"" << request.trim() << "\". Change only what that needs.\n/no_think";
    return s;
}

juce::String tweakGrammar()
{
    auto lit = [] (const juce::String& text) { return "\"" + text.replace ("\\", "\\\\").replace ("\"", "\\\"") + "\""; };
    juce::StringArray ids;
    for (const auto& spec : paramSpecs())
        if (tweakable (spec))
            ids.add (lit (spec.id));
    juce::String g;
    g << "root ::= " << lit ("{\"changes\":[") << " change (" << lit (",") << " change){0,7} " << lit ("]}") << "\n"
      << "change ::= " << lit ("{\"id\":\"") << " pid " << lit ("\",\"value\":") << " num " << lit ("}") << "\n"
      << "pid ::= " << ids.joinIntoString (" | ") << "\n"
      << "num ::= \"-\"? [0-9]{1,5} (\".\" [0-9]{1,4})?\n";
    return g;
}

bool parseTweakReply (const juce::String& json, const Patch& current, std::vector<TweakChange>& changes, juce::String& summary)
{
    const auto start = json.indexOfChar ('{');
    const auto end = json.lastIndexOfChar ('}');
    if (start < 0 || end <= start)
        return false;
    const auto parsed = juce::JSON::parse (json.substring (start, end + 1));
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return false;
    const auto list = obj->getProperty ("changes");
    if (! list.isArray())
        return false;
    Patch scratch = current;
    for (const auto& item : *list.getArray())
    {
        auto* c = item.getDynamicObject();
        if (c == nullptr) continue;
        const int index = paramIndexForId (c->getProperty ("id").toString());
        if (index < 0 || ! tweakable (paramSpecs()[(size_t) index])) continue;
        scratch.set (index, (float) c->getProperty ("value"));
        if (std::abs (scratch.get ((P) index) - current.get ((P) index)) < 1.0e-5f) continue;
        bool known = false;
        for (auto& ch : changes) if (ch.paramIndex == index) { ch.value = scratch.get ((P) index); known = true; }
        if (! known) changes.push_back ({ index, scratch.get ((P) index) });
    }
    if (changes.empty())
        return false;
    summary = describeChanges (current, changes);
    return true;
}

} // namespace stacks
