#include "PatchGenerator.h"
#include <set>

#include <cmath>
#include <cstring>

namespace stacks
{

namespace
{
    enum Archetype { Pad, Pluck, Bass, Keys, Lead, Bell, Texture, Drone, NumArchetypes };

    const char* archetypeName (int a)
    {
        static const char* names[] = { "Pad", "Pluck", "Bass", "Keys", "Lead", "Bell", "Texture", "Drone" };
        return names[juce::jlimit (0, NumArchetypes - 1, a)];
    }

    struct Rng
    {
        explicit Rng (juce::int64 seed) : r (seed) {}

        float uni (float a, float b)          { return a + (b - a) * r.nextFloat(); }
        float logUni (float a, float b)       { return std::exp (uni (std::log (a), std::log (b))); }
        bool  chance (float p)                { return r.nextFloat() < p; }
        int   pick (std::initializer_list<int> xs)
        {
            auto it = xs.begin();
            std::advance (it, r.nextInt ((int) xs.size()));
            return *it;
        }
        const char* pick (std::initializer_list<const char*> xs)
        {
            auto it = xs.begin();
            std::advance (it, r.nextInt ((int) xs.size()));
            return *it;
        }
        float gauss()
        {
            const float u1 = juce::jmax (1.0e-6f, r.nextFloat()), u2 = r.nextFloat();
            return std::sqrt (-2.0f * std::log (u1)) * std::cos (juce::MathConstants<float>::twoPi * u2);
        }

        juce::Random r;
    };

    int wave (const char* n) { return juce::jmax (0, waveNames().indexOf (n)); }
    int pickWave (Rng& rng, std::initializer_list<const char*> names) { return wave (rng.pick (names)); }

    //--------------------------------------------------------------------------
    const juce::StringArray& adjectives()
    {
        static const juce::StringArray a { "Velvet", "Amber", "Glacial", "Neon", "Dusty", "Hollow", "Liquid", "Rusted",
                                           "Silver", "Midnight", "Paper", "Electric", "Frozen", "Golden", "Shadow",
                                           "Crystal", "Smoky", "Violet", "Static", "Lunar", "Copper", "Feral", "Gentle",
                                           "Burnt", "Prism", "Opal", "Marble", "Ember", "Saline", "Cobalt" };
        return a;
    }

    const juce::StringArray& nouns (int archetype)
    {
        static const juce::StringArray n[NumArchetypes] = {
            { "Pad", "Drift", "Haze", "Bloom", "Choir", "Veil", "Strings", "Field" },
            { "Pluck", "Spark", "Drop", "Pizz", "Pin", "Tick", "Kalimba" },
            { "Bass", "Sub", "Growl", "Rumble", "Thump", "Floor" },
            { "Keys", "Piano", "Clav", "Chime", "Toy", "Celeste" },
            { "Lead", "Cry", "Horn", "Line", "Siren", "Voice" },
            { "Bell", "Glass", "Chime", "Tine", "Gong", "Bowl" },
            { "Texture", "Grain", "Smear", "Cloud", "Swarm", "Rust", "Static" },
            { "Drone", "Tide", "Abyss", "Hum", "Monolith", "Horizon" } };
        return n[juce::jlimit (0, NumArchetypes - 1, archetype)];
    }

    juce::String randomName (int archetype, Rng& rng)
    {
        return adjectives()[rng.r.nextInt (adjectives().size())] + " "
             + nouns (archetype)[rng.r.nextInt (nouns (archetype).size())];
    }

    // "Velvet Drift" -> "Amber Drift": new adjective, keep the family name.
    juce::String childName (const juce::String& parent, Rng& rng)
    {
        auto words = juce::StringArray::fromTokens (parent.trim(), " ", "");
        words.removeEmptyStrings();
        const auto noun = words.isEmpty() ? juce::String ("Patch") : words[words.size() - 1];
        juce::String adj;
        do
            adj = adjectives()[rng.r.nextInt (adjectives().size())];
        while (words.size() > 1 && adj == words[0]);
        return adj + " " + noun;
    }

    int archetypeFromHint (const juce::String& hintLower)
    {
        struct Key { const char* word; int archetype; };
        static const Key keys[] = { { "pad", Pad }, { "string", Pad }, { "choir", Pad },
                                    { "pluck", Pluck }, { "pizz", Pluck }, { "stab", Pluck },
                                    { "bass", Bass }, { "sub", Bass }, { "808", Bass },
                                    { "key", Keys }, { "piano", Keys }, { "clav", Keys }, { "organ", Keys },
                                    { "lead", Lead }, { "solo", Lead },
                                    { "bell", Bell }, { "chime", Bell }, { "mallet", Bell },
                                    { "texture", Texture }, { "noise", Texture }, { "glitch", Texture }, { "grain", Texture },
                                    { "drone", Drone }, { "ambient", Drone } };
        for (const auto& k : keys)
            if (hintLower.contains (k.word))
                return k.archetype;
        return -1;
    }

    //--------------------------------------------------------------------------
    void setEnvelope (Patch& p, P a, P d, P s, P r, float att, float dec, float sus, float rel)
    {
        p.set (a, att); p.set (d, dec); p.set (s, sus); p.set (r, rel);
    }

    int freeModSlot (const Patch& p)
    {
        for (int i = 0; i < kNumModSlots; ++i)
            if ((int) p.get (modSourceParam (i)) == SrcOff)
                return i;
        return -1;
    }

    void addMod (Patch& p, int source, int dest, float amount)
    {
        const int slot = freeModSlot (p);
        if (slot < 0) return;
        p.set (modSourceParam (slot), (float) source);
        p.set (modDestParam (slot), (float) dest);
        p.set (modAmountParam (slot), amount);
    }

    bool isLfoSource (int src) { return src == SrcLfo1 || src == SrcLfo2 || src == SrcLfo3 || src == SrcLfo4; }

    bool hasLfoMod (const Patch& p)
    {
        for (int i = 0; i < kNumModSlots; ++i)
            if (isLfoSource ((int) p.get (modSourceParam (i))) && (int) p.get (modDestParam (i)) != TargetOff)
                return true;
        return false;
    }

    int targetOf (P param) { return juce::jmax (0, modTargetForParam ((int) param)); }

    // Sets an LFO's shape and rate and routes it through a free matrix slot.
    void setLfo (Patch& p, int which, int dest, float rate, float amount, int shape)
    {
        if (which == 1) { p.set (P::lfo1_rate, rate); p.set (P::lfo1_shape, (float) shape); }
        else            { p.set (P::lfo2_rate, rate); p.set (P::lfo2_shape, (float) shape); }
        addMod (p, which == 1 ? SrcLfo1 : SrcLfo2, dest, amount);
    }

    void setModEnv (Patch& p, float att, float dec, float sus, float rel)
    {
        p.set (P::menv_attack, att); p.set (P::menv_decay, dec); p.set (P::menv_sustain, sus); p.set (P::menv_release, rel);
    }

    void setReverb (Patch& p, float mix, float size, float damp)
    {
        p.set (P::reverb_mix, mix); p.set (P::reverb_size, size); p.set (P::reverb_damp, damp);
    }

    void setReverbType (Patch& p, int type) { p.set (P::reverb_type, (float) type); }   // 0 Room, 1 Plate, 2 Hall, 3 Shimmer
    void setChorusMode (Patch& p, int mode) { p.set (P::chorus_mode, (float) mode); }   // 0 Chorus, 1 Ensemble, 2 Flanger, 3 Dimension
    void setDelayMode (Patch& p, int mode)  { p.set (P::delay_mode, (float) mode); }    // 0 Stereo, 1 Ping-Pong, 2 Tape

    void setDelay (Patch& p, Rng& rng, float mix, float feedback)
    {
        p.set (P::delay_mix, mix);
        p.set (P::delay_time, (float) rng.pick ({ 250, 375, 500, 750 }) / 1000.0f);
        p.set (P::delay_feedback, feedback);
    }

    Patch randomPatch (int archetype, Rng& rng)
    {
        Patch p;
        p.category = archetypeName (archetype);

        // Shared starting point
        p.set (P::oscA_morph, rng.uni (0.2f, 0.9f));
        p.set (P::oscB_morph, rng.uni (0.1f, 0.9f));
        p.set (P::oscA_level, rng.uni (0.6f, 0.9f));
        p.set (P::oscA_fine, rng.chance (0.3f) ? rng.uni (-8.0f, 8.0f) : 0.0f);
        p.set (P::oscB_fine, rng.uni (-12.0f, 12.0f));
        p.set (P::filter_keytrack, rng.uni (0.1f, 0.6f));
        p.set (P::reverb_mix, 0.0f);
        p.set (P::delay_mix, 0.0f);
        p.set (P::chorus_mix, 0.0f);

        switch (archetype)
        {
            case Pad:
                p.set (P::oscA_wave, pickWave (rng, { "Saw", "Triangle", "Organ", "Formant", "Glass" }));
                p.set (P::oscB_wave, pickWave (rng, { "Saw", "Triangle", "Sine", "Formant", "Organ" }));
                p.set (P::oscB_level, rng.uni (0.4f, 0.8f));
                p.set (P::oscB_coarse, (float) rng.pick ({ 0, 0, -12, 7, 12 }));
                p.set (P::fm_amount, rng.chance (0.3f) ? rng.uni (0.02f, 0.12f) : 0.0f);
                p.set (P::unison_voices, (float) rng.pick ({ 2, 3, 4, 4 }));
                p.set (P::unison_detune, rng.uni (8.0f, 25.0f));
                p.set (P::unison_spread, rng.uni (0.5f, 1.0f));
                p.set (P::filter_type, (float) rng.pick ({ 0, 1, 1 }));
                p.set (P::filter_cutoff, rng.logUni (400.0f, 3000.0f));
                p.set (P::filter_res, rng.uni (0.0f, 0.3f));
                p.set (P::filter_env, rng.uni (-1.0f, 1.5f));
                setEnvelope (p, P::fenv_attack, P::fenv_decay, P::fenv_sustain, P::fenv_release,
                             rng.logUni (0.3f, 2.0f), rng.logUni (1.0f, 4.0f), rng.uni (0.3f, 0.8f), rng.logUni (0.5f, 3.0f));
                setEnvelope (p, P::aenv_attack, P::aenv_decay, P::aenv_sustain, P::aenv_release,
                             rng.logUni (0.3f, 2.0f), rng.logUni (1.0f, 3.0f), rng.uni (0.7f, 1.0f), rng.logUni (1.0f, 4.0f));
                {
                    const int dest = rng.pick ({ targetOf (P::oscA_morph), targetOf (P::filter_cutoff), TargetPitch, targetOf (P::oscB_morph) });
                    setLfo (p, 1, dest, rng.logUni (0.05f, 0.5f), dest == TargetPitch ? rng.uni (0.01f, 0.03f) : rng.uni (0.1f, 0.5f), rng.pick ({ 0, 1 }));
                }
                if (rng.chance (0.5f))
                    setLfo (p, 2, TargetPan, rng.logUni (0.05f, 0.3f), rng.uni (0.2f, 0.6f), 0);
                addMod (p, SrcModWheel, targetOf (P::filter_cutoff), rng.uni (0.3f, 0.6f));
                p.set (P::chorus_mix, rng.uni (0.2f, 0.6f));
                p.set (P::chorus_rate, rng.uni (0.2f, 1.0f));
                p.set (P::chorus_depth, rng.uni (0.2f, 0.5f));
                setChorusMode (p, rng.pick ({ 0, 1, 1, 3 }));
                setReverb (p, rng.uni (0.3f, 0.6f), rng.uni (0.6f, 0.9f), rng.uni (0.3f, 0.7f));
                setReverbType (p, rng.pick ({ 1, 2, 2, 3 }));
                if ((int) p.get (P::reverb_type) == 3) p.set (P::reverb_shimmer, rng.uni (0.3f, 0.7f));
                if (rng.chance (0.4f))
                    setDelay (p, rng, rng.uni (0.1f, 0.3f), rng.uni (0.3f, 0.6f));
                break;

            case Pluck:
                p.set (P::oscA_wave, pickWave (rng, { "Saw", "Pulse", "Fold", "Glass", "Sync", "Triangle" }));
                p.set (P::oscB_wave, pickWave (rng, { "Sine", "Saw", "Glass", "Pulse" }));
                p.set (P::oscB_level, rng.chance (0.6f) ? rng.uni (0.2f, 0.7f) : 0.0f);
                p.set (P::oscB_coarse, (float) rng.pick ({ 0, 12, 7, 19, -12 }));
                p.set (P::fm_amount, rng.chance (0.5f) ? rng.uni (0.05f, 0.3f) : 0.0f);
                p.set (P::unison_voices, (float) rng.pick ({ 1, 1, 2 }));
                p.set (P::unison_detune, rng.uni (3.0f, 12.0f));
                p.set (P::filter_type, (float) rng.pick ({ 0, 1 }));
                p.set (P::filter_cutoff, rng.logUni (300.0f, 2000.0f));
                p.set (P::filter_res, rng.uni (0.1f, 0.5f));
                p.set (P::filter_env, rng.uni (1.5f, 4.0f));
                setEnvelope (p, P::fenv_attack, P::fenv_decay, P::fenv_sustain, P::fenv_release,
                             0.001f, rng.logUni (0.05f, 0.4f), rng.uni (0.0f, 0.2f), rng.logUni (0.05f, 0.4f));
                setEnvelope (p, P::aenv_attack, P::aenv_decay, P::aenv_sustain, P::aenv_release,
                             rng.logUni (0.001f, 0.005f), rng.logUni (0.1f, 0.6f), rng.uni (0.0f, 0.2f), rng.logUni (0.1f, 0.5f));
                addMod (p, SrcVelocity, targetOf (P::filter_cutoff), rng.uni (0.2f, 0.5f));
                setDelay (p, rng, rng.uni (0.2f, 0.4f), rng.uni (0.3f, 0.6f));
                if (rng.chance (0.5f)) { setDelayMode (p, 1); p.set (P::delay_sync, (float) rng.pick ({ 3, 4, 5 })); }
                setReverb (p, rng.uni (0.1f, 0.3f), rng.uni (0.3f, 0.7f), rng.uni (0.3f, 0.7f));
                setReverbType (p, rng.pick ({ 0, 1 }));
                break;

            case Bass:
                p.set (P::oscA_wave, pickWave (rng, { "Saw", "Pulse", "Fold", "Sine", "Triangle" }));
                p.set (P::oscA_coarse, (float) rng.pick ({ -12, -12, 0 }));
                p.set (P::oscB_wave, pickWave (rng, { "Saw", "Pulse", "Sine", "Sync" }));
                p.set (P::oscB_level, rng.chance (0.6f) ? rng.uni (0.3f, 0.7f) : 0.0f);
                p.set (P::oscB_coarse, (float) rng.pick ({ 0, -12, 12, 7 }));
                p.set (P::oscB_fine, rng.uni (-7.0f, 7.0f));
                p.set (P::fm_amount, rng.chance (0.4f) ? rng.uni (0.05f, 0.25f) : 0.0f);
                p.set (P::sub_level, rng.uni (0.5f, 1.0f));
                p.set (P::unison_voices, (float) rng.pick ({ 1, 1, 2 }));
                p.set (P::unison_detune, rng.uni (3.0f, 10.0f));
                p.set (P::unison_spread, rng.uni (0.0f, 0.4f));
                p.set (P::filter_type, 1.0f);
                p.set (P::filter_cutoff, rng.logUni (80.0f, 800.0f));
                p.set (P::filter_res, rng.uni (0.1f, 0.5f));
                p.set (P::filter_drive, rng.uni (1.0f, 4.0f));
                p.set (P::filter_env, rng.uni (1.0f, 3.0f));
                p.set (P::filter_keytrack, rng.uni (0.0f, 0.3f));
                setEnvelope (p, P::fenv_attack, P::fenv_decay, P::fenv_sustain, P::fenv_release,
                             0.001f, rng.logUni (0.05f, 0.3f), rng.uni (0.0f, 0.3f), rng.logUni (0.05f, 0.3f));
                setEnvelope (p, P::aenv_attack, P::aenv_decay, P::aenv_sustain, P::aenv_release,
                             rng.logUni (0.001f, 0.01f), rng.logUni (0.2f, 0.6f), rng.uni (0.4f, 0.9f), rng.logUni (0.05f, 0.3f));
                p.set (P::glide, rng.chance (0.4f) ? rng.uni (0.02f, 0.08f) : 0.0f);
                addMod (p, SrcVelocity, targetOf (P::filter_cutoff), rng.uni (0.2f, 0.5f));
                setReverb (p, rng.uni (0.0f, 0.1f), 0.3f, 0.7f);
                break;

            case Keys:
                p.set (P::oscA_wave, pickWave (rng, { "Organ", "Glass", "Sine", "Triangle", "Formant" }));
                p.set (P::oscB_wave, pickWave (rng, { "Sine", "Glass", "Triangle" }));
                p.set (P::oscB_level, rng.uni (0.0f, 0.5f));
                p.set (P::oscB_coarse, (float) rng.pick ({ 0, 12, 19, 24, -12 }));
                p.set (P::fm_amount, rng.uni (0.1f, 0.5f));
                p.set (P::unison_voices, (float) rng.pick ({ 1, 1, 2 }));
                p.set (P::unison_detune, rng.uni (3.0f, 8.0f));
                p.set (P::filter_type, (float) rng.pick ({ 0, 1 }));
                p.set (P::filter_cutoff, rng.logUni (2000.0f, 8000.0f));
                p.set (P::filter_res, rng.uni (0.0f, 0.3f));
                p.set (P::filter_env, rng.uni (0.0f, 1.5f));
                setEnvelope (p, P::fenv_attack, P::fenv_decay, P::fenv_sustain, P::fenv_release,
                             0.001f, rng.logUni (0.2f, 1.0f), rng.uni (0.2f, 0.6f), rng.logUni (0.2f, 0.8f));
                setEnvelope (p, P::aenv_attack, P::aenv_decay, P::aenv_sustain, P::aenv_release,
                             rng.logUni (0.001f, 0.01f), rng.logUni (0.5f, 2.0f), rng.uni (0.2f, 0.6f), rng.logUni (0.3f, 1.0f));
                if (rng.chance (0.4f))
                    setLfo (p, 1, TargetAmp, rng.uni (3.0f, 6.0f), rng.uni (0.1f, 0.3f), 0);
                addMod (p, SrcVelocity, targetOf (P::filter_cutoff), rng.uni (0.2f, 0.5f));
                p.set (P::chorus_mix, rng.uni (0.1f, 0.4f));
                setReverb (p, rng.uni (0.2f, 0.4f), rng.uni (0.4f, 0.7f), rng.uni (0.3f, 0.7f));
                break;

            case Lead:
                p.set (P::oscA_wave, pickWave (rng, { "Saw", "Pulse", "Sync", "Fold" }));
                p.set (P::oscB_wave, pickWave (rng, { "Saw", "Pulse", "Sine" }));
                p.set (P::oscB_level, rng.uni (0.3f, 0.8f));
                p.set (P::oscB_coarse, (float) rng.pick ({ 0, 0, 7, 12, -12 }));
                p.set (P::fm_amount, rng.chance (0.3f) ? rng.uni (0.05f, 0.2f) : 0.0f);
                p.set (P::unison_voices, (float) rng.pick ({ 1, 2, 2 }));
                p.set (P::unison_detune, rng.uni (5.0f, 15.0f));
                p.set (P::unison_spread, rng.uni (0.0f, 0.5f));
                p.set (P::glide, rng.uni (0.02f, 0.15f));
                p.set (P::filter_type, (float) rng.pick ({ 0, 1 }));
                p.set (P::filter_cutoff, rng.logUni (800.0f, 5000.0f));
                p.set (P::filter_res, rng.uni (0.2f, 0.6f));
                p.set (P::filter_drive, rng.uni (1.0f, 3.0f));
                p.set (P::filter_env, rng.uni (0.5f, 2.5f));
                setEnvelope (p, P::fenv_attack, P::fenv_decay, P::fenv_sustain, P::fenv_release,
                             rng.logUni (0.001f, 0.05f), rng.logUni (0.1f, 0.6f), rng.uni (0.3f, 0.7f), rng.logUni (0.1f, 0.4f));
                setEnvelope (p, P::aenv_attack, P::aenv_decay, P::aenv_sustain, P::aenv_release,
                             rng.logUni (0.005f, 0.05f), rng.logUni (0.1f, 0.5f), rng.uni (0.8f, 1.0f), rng.logUni (0.1f, 0.4f));
                setLfo (p, 1, TargetPitch, rng.uni (4.0f, 7.0f), rng.uni (0.015f, 0.04f), 0);
                addMod (p, SrcVelocity, targetOf (P::filter_cutoff), rng.uni (0.2f, 0.5f));
                setDelay (p, rng, rng.uni (0.2f, 0.4f), rng.uni (0.3f, 0.5f));
                setReverb (p, rng.uni (0.1f, 0.3f), rng.uni (0.4f, 0.7f), 0.5f);
                break;

            case Bell:
                p.set (P::oscA_wave, pickWave (rng, { "Sine", "Glass", "Triangle" }));
                p.set (P::oscB_wave, pickWave (rng, { "Sine", "Glass" }));
                p.set (P::oscB_level, rng.uni (0.0f, 0.3f));
                p.set (P::oscB_coarse, (float) rng.pick ({ 7, 12, 19, 24, 31 }));
                p.set (P::oscB_fine, rng.uni (0.0f, 20.0f));
                p.set (P::fm_amount, rng.uni (0.3f, 0.8f));
                p.set (P::unison_voices, (float) rng.pick ({ 1, 1, 2 }));
                p.set (P::unison_detune, rng.uni (2.0f, 6.0f));
                p.set (P::filter_type, (float) rng.pick ({ 0, 1, 2 }));
                p.set (P::filter_cutoff, rng.logUni (3000.0f, 12000.0f));
                p.set (P::filter_res, rng.uni (0.0f, 0.2f));
                p.set (P::filter_env, 0.0f);
                setEnvelope (p, P::aenv_attack, P::aenv_decay, P::aenv_sustain, P::aenv_release,
                             0.001f, rng.logUni (1.0f, 4.0f), 0.0f, rng.logUni (1.0f, 3.0f));
                setEnvelope (p, P::fenv_attack, P::fenv_decay, P::fenv_sustain, P::fenv_release,
                             0.001f, rng.logUni (0.5f, 2.0f), 0.0f, 1.0f);
                if (rng.chance (0.5f))
                    setLfo (p, 1, targetOf (P::fm_amount), rng.logUni (0.1f, 1.0f), rng.uni (0.05f, 0.2f), 0);
                setModEnv (p, 0.001f, rng.logUni (0.2f, 1.0f), 0.0f, 0.5f);
                addMod (p, SrcModEnv, targetOf (P::fm_amount), rng.uni (0.15f, 0.4f));
                setReverb (p, rng.uni (0.3f, 0.6f), rng.uni (0.6f, 0.9f), rng.uni (0.2f, 0.5f));
                break;

            case Texture:
                p.set (P::oscA_wave, pickWave (rng, { "Grit", "Formant", "Fold", "Sync", "Glass" }));
                p.set (P::oscB_wave, pickWave (rng, { "Grit", "Formant", "Saw", "Pulse" }));
                p.set (P::oscB_level, rng.uni (0.3f, 0.8f));
                p.set (P::oscB_coarse, (float) rng.pick ({ 0, 7, 12, -12, 5, -5 }));
                p.set (P::fm_amount, rng.uni (0.0f, 0.5f));
                p.set (P::noise_level, rng.uni (0.1f, 0.4f));
                p.set (P::unison_voices, (float) rng.pick ({ 1, 2, 4 }));
                p.set (P::unison_detune, rng.uni (10.0f, 40.0f));
                p.set (P::unison_spread, rng.uni (0.5f, 1.0f));
                p.set (P::filter_type, (float) rng.pick ({ 0, 1, 4, 5, 2 }));
                p.set (P::filter_cutoff, rng.logUni (300.0f, 4000.0f));
                p.set (P::filter_res, rng.uni (0.2f, 0.7f));
                p.set (P::filter_drive, rng.uni (1.0f, 5.0f));
                p.set (P::filter_env, rng.uni (-2.0f, 2.0f));
                setEnvelope (p, P::fenv_attack, P::fenv_decay, P::fenv_sustain, P::fenv_release,
                             rng.logUni (0.1f, 3.0f), rng.logUni (0.5f, 4.0f), rng.uni (0.2f, 0.8f), rng.logUni (0.5f, 3.0f));
                setEnvelope (p, P::aenv_attack, P::aenv_decay, P::aenv_sustain, P::aenv_release,
                             rng.logUni (0.05f, 2.0f), rng.logUni (0.5f, 3.0f), rng.uni (0.5f, 1.0f), rng.logUni (0.5f, 4.0f));
                setLfo (p, 1, rng.pick ({ targetOf (P::oscA_morph), targetOf (P::fm_amount), targetOf (P::oscB_morph) }), rng.logUni (0.1f, 3.0f), rng.uni (0.3f, 1.0f), rng.pick ({ 0, 1, 2, 4 }));
                setLfo (p, 2, rng.pick ({ TargetPan, targetOf (P::filter_cutoff), TargetAmp }), rng.logUni (0.1f, 2.0f), rng.uni (0.2f, 0.7f), rng.pick ({ 0, 1, 4 }));
                addMod (p, SrcRandom, targetOf (P::oscA_morph), rng.uni (0.1f, 0.3f));
                setDelay (p, rng, rng.uni (0.3f, 0.6f), rng.uni (0.5f, 0.85f));
                setDelayMode (p, rng.pick ({ 0, 1, 2, 2 }));
                setReverb (p, rng.uni (0.4f, 0.8f), rng.uni (0.6f, 1.0f), rng.uni (0.2f, 0.6f));
                setReverbType (p, rng.pick ({ 2, 3, 3 }));
                if ((int) p.get (P::reverb_type) == 3) p.set (P::reverb_shimmer, rng.uni (0.4f, 0.8f));
                break;

            case Drone:
            default:
                p.set (P::oscA_wave, pickWave (rng, { "Saw", "Organ", "Formant", "Grit", "Sine" }));
                p.set (P::oscA_coarse, (float) rng.pick ({ -12, -24, 0 }));
                p.set (P::oscB_wave, pickWave (rng, { "Saw", "Sine", "Formant", "Glass" }));
                p.set (P::oscB_level, rng.uni (0.4f, 0.8f));
                p.set (P::oscB_coarse, (float) rng.pick ({ 0, 7, 12, -12, 5 }));
                p.set (P::fm_amount, rng.uni (0.0f, 0.15f));
                p.set (P::sub_level, rng.uni (0.0f, 0.5f));
                p.set (P::unison_voices, 4.0f);
                p.set (P::unison_detune, rng.uni (5.0f, 15.0f));
                p.set (P::unison_spread, rng.uni (0.6f, 1.0f));
                p.set (P::filter_type, (float) rng.pick ({ 0, 1 }));
                p.set (P::filter_cutoff, rng.logUni (200.0f, 1500.0f));
                p.set (P::filter_res, rng.uni (0.1f, 0.5f));
                p.set (P::filter_env, rng.uni (0.0f, 1.0f));
                setEnvelope (p, P::fenv_attack, P::fenv_decay, P::fenv_sustain, P::fenv_release,
                             rng.logUni (2.0f, 8.0f), 4.0f, 1.0f, rng.logUni (2.0f, 6.0f));
                setEnvelope (p, P::aenv_attack, P::aenv_decay, P::aenv_sustain, P::aenv_release,
                             rng.logUni (2.0f, 8.0f), 2.0f, 1.0f, rng.logUni (3.0f, 8.0f));
                setLfo (p, 1, rng.pick ({ targetOf (P::oscA_morph), targetOf (P::filter_cutoff) }), rng.logUni (0.02f, 0.2f), rng.uni (0.2f, 0.6f), rng.pick ({ 0, 1 }));
                setLfo (p, 2, rng.pick ({ TargetPan, targetOf (P::oscB_morph) }), rng.logUni (0.02f, 0.15f), rng.uni (0.2f, 0.5f), 0);
                p.set (P::chorus_mix, rng.uni (0.1f, 0.4f));
                setChorusMode (p, rng.pick ({ 1, 3 }));
                setReverb (p, rng.uni (0.4f, 0.7f), rng.uni (0.8f, 1.0f), rng.uni (0.2f, 0.5f));
                setReverbType (p, rng.pick ({ 2, 3 }));
                if ((int) p.get (P::reverb_type) == 3) p.set (P::reverb_shimmer, rng.uni (0.3f, 0.6f));
                break;
        }

        p.name = randomName (archetype, rng);
        return p;
    }

    //--------------------------------------------------------------------------
    // Nudge every parameter a little (or a lot). Perturbation happens in the
    // knob's normalised space so a cutoff of 200 Hz moves by a musically
    // similar amount to one at 5 kHz.
    void mutate (Patch& p, float amount, Rng& rng)
    {
        const auto& specs = paramSpecs();
        for (int i = 0; i < kNumParams; ++i)
        {
            const auto& s = specs[(size_t) i];
            if (std::strcmp (s.id, "master_gain") == 0 || std::strcmp (s.id, "life") == 0 || std::strcmp (s.group, "MACROS") == 0 || std::strcmp (s.group, "ARP") == 0
                || std::strcmp (s.group, "SAMPLE") == 0 || std::strcmp (s.group, "GRAIN") == 0 || std::strcmp (s.group, "FXROUTE") == 0)   // the player picks the sample and the chain
                continue;

            if (s.kind == ParamKind::Choice)
            {
                const juce::String id (s.id);
                const float chance = id.endsWith ("_wave") ? 0.3f : id.endsWith ("_dest") ? 0.04f : id.endsWith ("_source") ? 0.06f : 0.12f;
                if (rng.chance (amount * chance))
                    p.set (i, (float) rng.r.nextInt (id.endsWith ("_wave") ? WavetableBank::kNumBuiltIn : id.endsWith ("_source") ? (int) SrcMacro1 : s.choices().size()));
                continue;
            }

            if (! rng.chance (0.3f + 0.5f * amount))
                continue;

            const auto& range = paramRange (i);
            float norm = range.convertTo0to1 (juce::jlimit (s.min, s.max, p.values[(size_t) i]));
            norm = juce::jlimit (0.0f, 1.0f, norm + rng.gauss() * 0.2f * amount);
            p.set (i, range.convertFrom0to1 (norm));
        }
    }

    // Child takes each whole section (OSC A, FILTER, ...) from one parent.
    Patch crossover (const std::vector<Patch>& parents, Rng& rng)
    {
        const auto& base = parents[(size_t) rng.r.nextInt ((int) parents.size())];
        Patch child = base;
        if (parents.size() < 2)
            return child;

        const auto& specs = paramSpecs();
        juce::String currentGroup;
        const Patch* donor = &base;
        for (int i = 0; i < kNumParams; ++i)
        {
            if (currentGroup != specs[(size_t) i].group)
            {
                currentGroup = specs[(size_t) i].group;
                donor = &parents[(size_t) rng.r.nextInt ((int) parents.size())];
                if (currentGroup == "OSC A") child.waves[0] = donor->waves[0];   // a designed table follows its oscillator
                if (currentGroup == "OSC B") child.waves[1] = donor->waves[1];
                if (currentGroup == "OSC C") child.waves[2] = donor->waves[2];
            }
            child.values[(size_t) i] = donor->values[(size_t) i];
        }
        return child;
    }

    //--------------------------------------------------------------------------
    // Designed wavetables: a spectrum recipe (a 1/k^slope series, maybe odd
    // harmonics only, maybe sparse partials, maybe a formant bump) in three
    // frames that drift, so the Morph knob has somewhere to go.
    WaveSpec randomWaveSpec (int archetype, Rng& rng)
    {
        WaveSpec s;
        const float slope  = archetype == Bass || archetype == Lead ? rng.uni (0.5f, 1.2f) : rng.uni (0.5f, 2.0f);
        const bool oddOnly = rng.chance (archetype == Bass || archetype == Lead ? 0.4f : 0.25f);
        const bool sparse  = archetype == Bell || archetype == Keys ? rng.chance (0.6f) : rng.chance (0.15f);
        const int bumpAt   = rng.chance (archetype == Texture || archetype == Pad ? 0.7f : 0.4f) ? 3 + rng.r.nextInt (10) : 0;
        const float bumpW  = rng.uni (1.0f, 3.0f), bumpGain = rng.uni (1.5f, 4.0f);
        const float drift  = rng.uni (-1.0f, 1.0f);   // how the frames change: brighter or darker, the bump moving

        bool keep[WaveSpec::kHarmonics];
        for (int k = 0; k < WaveSpec::kHarmonics; ++k)
            keep[k] = ! sparse || k == 0 || rng.chance (0.3f);

        for (int f = 0; f < 3; ++f)
        {
            const float m = (float) f / 2.0f;
            std::vector<float> frame ((size_t) WaveSpec::kHarmonics, 0.0f);
            float peak = 1.0e-6f;
            for (int k = 1; k <= WaveSpec::kHarmonics; ++k)
            {
                float a = std::pow ((float) k, -(slope + drift * (m - 0.5f)));
                if (oddOnly && k % 2 == 0) a *= 0.04f;
                if (! keep[k - 1])         a *= 0.03f;
                if (bumpAt > 0)
                {
                    const float centre = (float) bumpAt + drift * m * 4.0f;
                    a *= 1.0f + bumpGain * std::exp (-std::pow (((float) k - centre) / bumpW, 2.0f));
                }
                frame[(size_t) (k - 1)] = a;
                peak = std::max (peak, a);
            }
            for (auto& a : frame)
                a = std::round (a / peak * 100.0f) / 100.0f;
            s.frames.push_back (std::move (frame));
        }
        s.tail = rng.chance (0.5f) ? std::round (rng.uni (0.1f, 0.9f) * 10.0f) / 10.0f : 0.0f;
        s.name = randomName (archetype, rng);
        return s;
    }

    WaveSpec mutateWaveSpec (const WaveSpec& parent, float amount, Rng& rng)
    {
        WaveSpec s = parent;
        for (auto& frame : s.frames)
        {
            float peak = 1.0e-6f;
            for (auto& a : frame)
            {
                if (rng.chance (0.2f + 0.6f * amount))
                    a = juce::jlimit (0.0f, 1.0f, a * std::exp (rng.gauss() * 0.6f * amount)
                                                    + (rng.chance (0.1f * amount) ? rng.uni (0.0f, 0.3f) : 0.0f));
                peak = std::max (peak, a);
            }
            for (auto& a : frame)
                a = std::round (a / peak * 100.0f) / 100.0f;
        }
        if (rng.chance (0.5f))
            s.tail = juce::jlimit (0.0f, 1.0f, std::round ((s.tail + rng.gauss() * 0.3f * amount) * 10.0f) / 10.0f);
        s.name = childName (parent.name.isNotEmpty() ? parent.name : juce::String ("Wave"), rng);
        return s;
    }

    // Gives oscillator A a designed table: a fresh recipe for a new patch, or a
    // relative of the parent's table (designed or built-in) when evolving.
    void designWave (Patch& p, const GenerationRequest& req, const WavetableBank& bank, int archetype, Rng& rng)
    {
        if (req.parents.empty())
        {
            if (! rng.chance (0.6f))
                return;
            p.waves[0] = randomWaveSpec (archetype, rng);
            p.set (P::oscA_wave, (float) kCustomWave);
            return;
        }

        if (! rng.chance (0.7f))
            return;
        const int wave = (int) p.get (P::oscA_wave);
        WaveSpec base;
        if (wave == kCustomWave && ! p.waves[0].isEmpty())
            base = p.waves[0];
        else if (wave < WavetableBank::kNumBuiltIn || wave == kCustomWave)
        {
            const int source = wave == kCustomWave ? waveNames().indexOf ("Saw") : wave;   // Custom with no table: start from a saw
            base = analyseWave ([&] (float morph, float phase) { return bank.read (source, 0, morph, phase >= 1.0f ? 0.999f : phase); },
                                waveNames()[source]);
        }
        else
            return;   // an imported file: leave it alone
        p.waves[0] = mutateWaveSpec (base, 0.2f + 0.8f * req.variation, rng);
        p.set (P::oscA_wave, (float) kCustomWave);
    }

    // Cheap keyword steering until the language model takes over the hint.
    void applyHint (Patch& p, const juce::String& hintLower, Rng& rng)
    {
        if (hintLower.isEmpty())
            return;

        auto scaleCutoff = [&] (float f) { p.set (P::filter_cutoff, p.get (P::filter_cutoff) * f); };

        if (hintLower.contains ("dark") || hintLower.contains ("warm") || hintLower.contains ("muffled")) scaleCutoff (0.5f);
        if (hintLower.contains ("bright") || hintLower.contains ("sharp") || hintLower.contains ("crisp"))  scaleCutoff (2.0f);
        if (hintLower.contains ("slow") || hintLower.contains ("swell"))
        {
            p.set (P::aenv_attack, juce::jmax (p.get (P::aenv_attack), rng.uni (0.5f, 2.0f)));
            p.set (P::aenv_release, juce::jmax (p.get (P::aenv_release), rng.uni (1.0f, 3.0f)));
        }
        if (hintLower.contains ("fast") || hintLower.contains ("snappy") || hintLower.contains ("short") || hintLower.contains ("tight"))
        {
            p.set (P::aenv_attack, 0.002f);
            p.set (P::aenv_decay, juce::jmin (p.get (P::aenv_decay), rng.uni (0.1f, 0.4f)));
            p.set (P::aenv_release, juce::jmin (p.get (P::aenv_release), rng.uni (0.05f, 0.3f)));
        }
        if (hintLower.contains ("wet") || hintLower.contains ("space") || hintLower.contains ("reverb") || hintLower.contains ("ambient"))
        {
            p.set (P::reverb_mix, juce::jmax (p.get (P::reverb_mix), rng.uni (0.4f, 0.7f)));
            p.set (P::reverb_size, juce::jmax (p.get (P::reverb_size), 0.7f));
        }
        if (hintLower.contains ("dry"))
        {
            p.set (P::reverb_mix, 0.0f); p.set (P::delay_mix, 0.0f); p.set (P::chorus_mix, 0.0f);
        }
        if (hintLower.contains ("wide") || hintLower.contains ("thick") || hintLower.contains ("huge"))
        {
            p.set (P::unison_voices, 4.0f);
            p.set (P::unison_spread, 1.0f);
            p.set (P::unison_detune, juce::jmax (p.get (P::unison_detune), 12.0f));
        }
        if (hintLower.contains ("mono") || hintLower.contains ("thin") || hintLower.contains ("simple"))
            p.set (P::unison_voices, 1.0f);
        if (hintLower.contains ("movement") || hintLower.contains ("motion") || hintLower.contains ("evolving") || hintLower.contains ("wobble"))
        {
            if (! hasLfoMod (p))
                setLfo (p, 1, rng.pick ({ targetOf (P::oscA_morph), targetOf (P::filter_cutoff), targetOf (P::oscB_morph) }), rng.logUni (0.1f, 1.0f), rng.uni (0.3f, 0.7f), rng.pick ({ 0, 1 }));
            else
                for (int i = 0; i < kNumModSlots; ++i)
                {
                    const int src = (int) p.get (modSourceParam (i));
                    if (isLfoSource (src) && (int) p.get (modDestParam (i)) != TargetPitch)
                        p.set (modAmountParam (i), juce::jmax (p.get (modAmountParam (i)), rng.uni (0.3f, 0.7f)));
                }
        }
        if (hintLower.contains ("metal") || hintLower.contains ("fm") || hintLower.contains ("glassy"))
            p.set (P::fm_amount, juce::jmax (p.get (P::fm_amount), rng.uni (0.3f, 0.6f)));
        if (hintLower.contains ("dirty") || hintLower.contains ("grit") || hintLower.contains ("distort"))
            p.set (P::filter_drive, juce::jmax (p.get (P::filter_drive), rng.uni (3.0f, 7.0f)));
        if (hintLower.contains ("octave down") || hintLower.contains ("lower") || hintLower.contains ("deeper"))
            p.set (P::oscA_coarse, p.get (P::oscA_coarse) - 12.0f);
        if (hintLower.contains ("octave up") || hintLower.contains ("higher"))
            p.set (P::oscA_coarse, p.get (P::oscA_coarse) + 12.0f);
    }
} // namespace

int countAudibleDifferences (const Patch& a, const Patch& b)
{
    const auto& specs = paramSpecs();
    int differences = 0;
    for (int i = 0; i < kNumParams; ++i)
    {
        const juce::String id (specs[(size_t) i].id);
        const juce::String group (specs[(size_t) i].group);
        if (id == "master_gain" || id == "bend_range" || id == "life" || id.startsWith ("mod") || group == "MACROS" || group == "ARP" || group == "SAMPLE" || group == "GRAIN" || group == "FXROUTE")
            continue;
        if (specs[(size_t) i].kind == ParamKind::Choice)
        {
            if ((int) a.values[(size_t) i] != (int) b.values[(size_t) i])
            {
                // A warp mode is silent until its amount is up on at least one side.
                if (id == "oscA_warp" || id == "oscB_warp")
                {
                    const P amt = id == "oscA_warp" ? P::oscA_warp_amt : P::oscB_warp_amt;
                    if (a.get (amt) < 0.05f && b.get (amt) < 0.05f)
                        continue;
                }
                ++differences;
            }
            continue;
        }
        const auto& range = paramRange (i);
        const float na = range.convertTo0to1 (juce::jlimit (range.start, range.end, a.values[(size_t) i]));
        const float nb = range.convertTo0to1 (juce::jlimit (range.start, range.end, b.values[(size_t) i]));
        if (std::abs (na - nb) > 0.12f) ++differences;
    }
    // Connections as sets of (source, target): one difference per routing only one side has.
    auto routings = [] (const Patch& p)
    {
        std::vector<std::pair<int, int>> r;
        for (int i = 0; i < kNumModSlots; ++i)
            if ((int) p.get (modSourceParam (i)) != SrcOff && (int) p.get (modDestParam (i)) != TargetOff)
                r.emplace_back ((int) p.get (modSourceParam (i)), (int) p.get (modDestParam (i)));
        return r;
    };
    const auto ra = routings (a), rb = routings (b);
    for (const auto& r : ra) if (std::find (rb.begin(), rb.end(), r) == rb.end()) ++differences;
    for (const auto& r : rb) if (std::find (ra.begin(), ra.end(), r) == ra.end()) ++differences;
    for (int osc = 0; osc < kNumOscs; ++osc)
        if (a.waves[(size_t) osc] != b.waves[(size_t) osc]) ++differences;
    return differences;
}

void applyPromptCues (const juce::String& hint, Patch& p)
{
    const auto text = " " + hint.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789 -") + " ";
    auto has    = [&] (const char* w) { const juce::String word (w); return text.contains (" " + word + " ") || text.contains (" " + word + "s "); };
    auto phrase = [&] (const char* words) { return text.contains (words); };
    auto raise  = [&] (P param, float to) { p.set (param, juce::jmax (p.get (param), to)); };
    auto cap    = [&] (P param, float to) { p.set (param, juce::jmin (p.get (param), to)); };
    auto choose = [&] (P param, const juce::StringArray& names, const char* name) { const int i = names.indexOf (name); if (i >= 0) p.set (param, (float) i); };

    const bool ensemble = has ("ensemble") || phrase ("string machine") || has ("rotary");
    if (has ("chorus") || has ("lush") || ensemble)
    {
        raise (P::chorus_mix, 0.4f);
        raise (P::chorus_depth, 0.3f);
        if (ensemble) choose (P::chorus_mode, chorusModeNames(), "Ensemble");
    }
    if (has ("delay") || has ("echo"))
    {
        raise (P::delay_mix, 0.3f);
        raise (P::delay_feedback, 0.35f);
        if (p.get (P::delay_sync) < 0.5f) choose (P::delay_sync, delaySyncNames(), "1/8D");
    }
    if (has ("tape"))
    {
        choose (P::delay_mode, delayModeNames(), "Tape");
        raise (P::delay_wow, 0.3f);
        raise (P::delay_mix, 0.2f);
        raise (P::tape_amount, 0.45f);
    }
    if (has ("warm") || has ("analog") || has ("analogue") || has ("vintage") || has ("retro"))
    {
        raise (P::char_sat, 0.3f);
        raise (P::char_drift, 0.35f);
        if (p.get (P::filter_model) == 0.0f) choose (P::filter_model, filterModelNames(), "Warm");
    }
    if (has ("huge") || has ("massive") || has ("supersaw") || has ("wide"))
        raise (P::hyper_amount, 0.4f);
    if (has ("lush"))
        choose (P::reverb_type, reverbTypeNames(), "Lush");
    if (has ("reverb") || has ("ambient") || has ("cinematic") || has ("hall") || has ("wash") || phrase ("long tail")
        || has ("shimmer") || has ("spacious") || has ("ethereal"))
        raise (P::reverb_mix, 0.35f);
    if (phrase ("long tail") || phrase ("long decay") || has ("ambient") || has ("hall"))
        raise (P::reverb_size, 0.7f);
    if (has ("shimmer"))
    {
        choose (P::reverb_type, reverbTypeNames(), "Shimmer");
        raise (P::reverb_shimmer, 0.5f);
        raise (P::reverb_mix, 0.4f);
    }
    if (has ("distorted") || has ("distortion") || has ("screaming") || has ("dubstep") || has ("gritty") || has ("dirty") || has ("fuzz") || has ("overdriven"))
    {
        raise (P::dist_mix, 0.5f);
        raise (P::dist_drive, 18.0f);
    }
    if (phrase ("lo-fi") || has ("lofi") || has ("crushed") || has ("bitcrushed") || has ("bitcrush"))
    {
        choose (P::dist_mode, distModeNames(), "Crush");
        raise (P::dist_mix, 0.3f);
    }
    if (has ("wide") || has ("supersaw") || has ("detuned") || has ("huge") || has ("massive"))
    {
        raise (P::unison_voices, 5.0f);
        raise (P::unison_detune, 18.0f);
        raise (P::unison_spread, 0.7f);
    }
    if (phrase ("sub bass") || has ("sub"))
        raise (P::sub_level, 0.5f);
    if (has ("portamento") || has ("glide") || has ("legato"))
        raise (P::glide, 0.08f);
    if (has ("punchy") || has ("stab") || has ("short") || has ("staccato"))
    {
        cap (P::aenv_release, 0.3f);
        cap (P::aenv_attack, 0.01f);
    }
    if (phrase ("long release"))
        raise (P::aenv_release, 1.5f);
    if (has ("riser"))
        raise (P::aenv_attack, 2.0f);
    else if (has ("swell") || phrase ("slow attack") || has ("evolving"))
        raise (P::aenv_attack, 0.5f);
    if (has ("arp") || has ("arpeggio") || has ("arpeggiated") || has ("arpeggiator"))
        choose (P::arp_mode, arpModeNames(), "Up");
}

void mutatePatch (Patch& p, float amount, juce::int64 seed)
{
    Rng rng (seed);
    mutate (p, amount, rng);
}

WaveSpec mutateWave (const WaveSpec& w, float amount, juce::int64 seed)
{
    Rng rng (seed);
    return mutateWaveSpec (w, amount, rng);
}

std::vector<Patch> RandomPatchGenerator::generate (const GenerationRequest& req, const GenerationProgress& progress)
{
    const auto ticks = (juce::uint64) juce::Time::getHighResolutionTicks();
    const auto salt  = (juce::uint64) counter.fetch_add (1) * 0x9E3779B97F4A7C15ULL;
    Rng rng ((juce::int64) (ticks ^ salt));

    const auto hintLower = req.hint.toLowerCase();
    const int hintArchetype = archetypeFromHint (hintLower);

    std::vector<Patch> out;
    out.reserve ((size_t) req.count);

    for (int i = 0; i < req.count; ++i)
    {
        Patch p;
        int archetype = hintArchetype >= 0 ? hintArchetype : rng.r.nextInt (NumArchetypes);
        if (req.parents.empty())
        {
            // Fresh: cycle through the archetypes so a batch has range, unless
            // the hint asks for a specific kind of sound.
            archetype = hintArchetype >= 0 ? hintArchetype : (i + rng.r.nextInt (NumArchetypes)) % NumArchetypes;
            p = randomPatch (archetype, rng);
            if (req.variation > 0.6f)
                mutate (p, (req.variation - 0.6f) * 1.5f, rng);
        }
        else
        {
            p = crossover (req.parents, rng);
            // Scale 0..1 variation to a useful range: 0.5 should feel like a sibling.
            mutate (p, 0.15f + 0.85f * req.variation, rng);
            p.name = childName (req.parents[(size_t) rng.r.nextInt ((int) req.parents.size())].name, rng);
        }

        if (req.designWaves)
            designWave (p, req, *bank, archetype, rng);
        applyHint (p, hintLower, rng);
        keepPatchInTune (p);
        ensureMacroRoutings (p);
        p.set (P::master_gain, -6.0f);
        p.origin = "Random";
        p.prompt = req.hint.trim();
        p.parentName = req.parents.empty() ? juce::String() : req.parents.front().name;
        p.tags = autoTags (p);
        p.description = describePatch (p);
        progress.patch (p);
        out.push_back (std::move (p));

        if (progress.cancelled())
            break;
    }

    return out;
}

//==============================================================================
// Structure, identity and ideas: what "a new sound that is still related" means in code.

namespace
{
    bool fxActive (const Patch& p, int e)
    {
        if ((int) p.get (fxOnParam (e)) == 0) return false;
        switch (e)
        {
            case 0: return p.get (P::dist_mix) > 0.02f;
            case 1: return std::abs (p.get (P::eq_low_gain)) > 1.0f || std::abs (p.get (P::eq_mid_gain)) > 1.0f || std::abs (p.get (P::eq_high_gain)) > 1.0f;
            case 2: return p.get (P::chorus_mix) > 0.02f;
            case 3: return p.get (P::hyper_amount) > 0.02f;
            case 4: return p.get (P::tape_amount) > 0.02f;
            case 5: return p.get (P::delay_mix) > 0.02f;
            case 6: return p.get (P::reverb_mix) > 0.02f;
            default: return p.get (P::comp_mix) > 0.02f && p.get (P::comp_threshold) < -3.0f;
        }
    }

    std::vector<int> activeChain (const Patch& p)
    {
        std::vector<int> order;
        for (int e = 0; e < kNumFx; ++e) if (fxActive (p, e)) order.push_back (e);
        std::stable_sort (order.begin(), order.end(), [&] (int a, int b) { return p.get (fxPosParam (a)) < p.get (fxPosParam (b)); });
        return order;
    }

    // (source, target) pairs of the connections a player would hear.
    std::set<std::pair<int, int>> connections (const Patch& p)
    {
        std::set<std::pair<int, int>> out;
        for (int i = 0; i < kNumModSlots; ++i)
        {
            const int src = (int) p.get (modSourceParam (i));
            const int dest = (int) p.get (modDestParam (i));
            const float least = dest == TargetPitch || dest == TargetPitchB ? 0.01f : 0.05f;   // a vibrato is small by nature
            if (src == SrcOff || isMacroSource (src) || std::abs (p.get (modAmountParam (i))) < least) continue;
            out.insert ({ src, dest });
        }
        return out;
    }

    int bucket (float v, std::initializer_list<float> edges)
    {
        int b = 0;
        for (float e : edges) if (v >= e) ++b;
        return b;
    }

    float waveDistance (const WaveSpec& a, const WaveSpec& b)
    {
        if (a.isEmpty() || b.isEmpty()) return a.isEmpty() == b.isEmpty() ? 0.0f : 1.0f;
        float d = 0.0f;
        for (int k = 0; k < WaveSpec::kHarmonics; ++k)
            d += std::abs (a.frames[0][(size_t) k] - b.frames[0][(size_t) k]);
        return d / (float) WaveSpec::kHarmonics;
    }

    int filterEnvClass (const Patch& p) { const float e = p.get (P::filter_env); return e < -1.5f ? 0 : e > 1.5f ? 2 : 1; }
}

juce::String envelopeClass (const Patch& p)
{
    const float a = p.get (P::aenv_attack), d = p.get (P::aenv_decay), s = p.get (P::aenv_sustain), r = p.get (P::aenv_release);
    if (a > 0.25f)              return "swelling pad";
    if (s < 0.15f && d < 0.5f)  return "short pluck";
    if (s < 0.15f)              return "percussive";
    if (r > 1.0f)               return "sustained, long tail";
    return "sustained";
}

int countStructuralDifferences (const Patch& a, const Patch& b)
{
    int d = 0;
    auto choiceDiff = [&] (P param) { return (int) a.get (param) != (int) b.get (param); };

    // Sources
    for (int osc = 0; osc < kNumOscs; ++osc)
    {
        const bool aOn = osc == 0 || a.get (oscLevelParam (osc)) > 0.05f, bOn = osc == 0 || b.get (oscLevelParam (osc)) > 0.05f;
        if (aOn != bOn) { ++d; continue; }
        if (! aOn) continue;
        const int wa = (int) a.get (oscWaveParam (osc)), wb = (int) b.get (oscWaveParam (osc));
        if (wa != wb) ++d;
        else if (wa == kCustomWave && waveDistance (a.waves[(size_t) osc], b.waves[(size_t) osc]) > 0.15f) ++d;
        const bool warpA = a.get (oscWarpAmtParam (osc)) > 0.05f && (int) a.get (oscWarpParam (osc)) > 0;
        const bool warpB = b.get (oscWarpAmtParam (osc)) > 0.05f && (int) b.get (oscWarpParam (osc)) > 0;
        if (warpA != warpB || (warpA && choiceDiff (oscWarpParam (osc)))) ++d;
        if (osc > 0 && std::abs (a.get (oscCoarseParam (osc)) - b.get (oscCoarseParam (osc))) >= 5.0f) ++d;   // another interval
    }
    if (bucket (a.get (P::fm_amount), { 0.05f, 0.3f }) != bucket (b.get (P::fm_amount), { 0.05f, 0.3f })) ++d;
    if ((a.get (P::sub_level) > 0.2f) != (b.get (P::sub_level) > 0.2f)) ++d;
    if ((a.get (P::noise_level) > 0.1f) != (b.get (P::noise_level) > 0.1f)) ++d;
    if (bucket (a.get (P::unison_voices), { 1.5f, 4.5f }) != bucket (b.get (P::unison_voices), { 1.5f, 4.5f })) ++d;
    if (choiceDiff (P::smp_mode)) ++d;

    // Filter
    if (choiceDiff (P::filter_type)) ++d;
    if (choiceDiff (P::filter_routing)) ++d;
    else if ((int) a.get (P::filter_routing) > 0 && choiceDiff (P::filter2_type)) ++d;
    if (choiceDiff (P::filter_model)) ++d;
    if (std::abs (std::log2 (juce::jmax (20.0f, a.get (P::filter_cutoff)) / juce::jmax (20.0f, b.get (P::filter_cutoff)))) >= 1.0f) ++d;
    if (filterEnvClass (a) != filterEnvClass (b)) ++d;

    // Envelope class
    if (envelopeClass (a) != envelopeClass (b)) ++d;

    // Movement: connections as a set, plus the shapes of the LFOs in use
    const auto ca = connections (a), cb = connections (b);
    int conn = 0;
    for (const auto& c : ca) if (! cb.count (c)) ++conn;
    for (const auto& c : cb) if (! ca.count (c)) ++conn;
    d += juce::jmin (4, conn);
    for (int k = 0; k < kNumLfos; ++k)
    {
        bool usedA = false, usedB = false;
        for (const auto& c : ca) usedA = usedA || c.first == SrcLfo1 + k;
        for (const auto& c : cb) usedB = usedB || c.first == SrcLfo1 + k;
        if (usedA && usedB && (choiceDiff (lfoShapeParam (k)) || ((int) a.get (lfoSyncParam (k)) > 0) != ((int) b.get (lfoSyncParam (k)) > 0))) ++d;
    }

    // Space: which effects are on, their modes, the order
    for (int e = 0; e < kNumFx; ++e)
    {
        const bool onA = fxActive (a, e), onB = fxActive (b, e);
        if (onA != onB) { ++d; continue; }
        if (! onA) continue;
        switch (e)
        {
            case 0: if (choiceDiff (P::dist_mode)) ++d; break;
            case 2: if (choiceDiff (P::chorus_mode)) ++d; break;
            case 5: if (choiceDiff (P::delay_mode) || choiceDiff (P::delay_sync)) ++d; break;
            case 6: if (choiceDiff (P::reverb_type)) ++d; break;
            default: break;
        }
    }
    if (activeChain (a) != activeChain (b) && ! activeChain (a).empty() && ! activeChain (b).empty())
    {
        // the same set in a different order is one more idea; a different set was already counted
        std::vector<int> sa = activeChain (a), sb = activeChain (b);
        std::sort (sa.begin(), sa.end()); std::sort (sb.begin(), sb.end());
        if (sa == sb) ++d;
    }
    if (((int) a.get (P::arp_mode) > 0) != ((int) b.get (P::arp_mode) > 0)) ++d;
    return d;
}

GrowMode growModeFor (float v) noexcept
{
    return v < 0.33f ? GrowMode::Close : v < 0.66f ? GrowMode::Medium : GrowMode::Far;
}

juce::StringArray identityTraits (const Patch& p)
{
    juce::StringArray t;
    // Source
    {
        auto waveWords = [&] (int osc)
        {
            const int w = (int) p.get (oscWaveParam (osc));
            if (w == kCustomWave) return p.waves[(size_t) osc].isEmpty() ? juce::String ("a designed wavetable") : "a designed wavetable (" + p.waves[(size_t) osc].name.toLowerCase() + ")";
            if (w >= WavetableBank::kNumBuiltIn) return juce::String ("an imported wavetable");
            return "a " + waveNames()[w] + " wavetable";
        };
        juce::String src = waveWords (0);
        if (p.get (P::fm_amount) > 0.3f)        src << " with heavy FM";
        else if (p.get (P::fm_amount) > 0.05f)  src << " with light FM";
        if (p.get (P::oscB_level) > 0.05f && p.get (P::fm_amount) <= 0.05f)
        {
            const int c = juce::roundToInt (p.get (P::oscB_coarse));
            src << " layered with " << waveWords (1) << (c == 12 ? " an octave up" : c == -12 ? " an octave down" : c == 7 || c == -5 ? " a fifth away" : c != 0 ? " at another interval" : "");
        }
        if (p.get (P::sub_level) > 0.2f)   src << ", a sub underneath";
        if (p.get (P::noise_level) > 0.1f) src << ", a breath of noise";
        t.add (src);
    }
    t.add (envelopeClass (p) + " envelope");
    // Filter
    {
        const float c = p.get (P::filter_cutoff);
        juce::String f = c < 500.0f ? "a dark " : c < 2000.0f ? "a half-open " : "an open ";
        f << filterTypeNames()[juce::jlimit (0, filterTypeNames().size() - 1, (int) p.get (P::filter_type))] << " filter";
        if (p.get (P::filter_res) > 0.5f) f << ", resonant";
        if (p.get (P::filter_env) > 1.5f) f << ", sweeping down as the note plays";
        else if (p.get (P::filter_env) < -1.5f) f << ", opening as the note plays";
        t.add (f);
    }
    // Movement
    {
        juce::StringArray moves;
        for (const auto& c : connections (p))
        {
            juce::String who = c.first >= SrcLfo1 && c.first < SrcLfo1 + kNumLfos ? "an LFO" : c.first == SrcModEnv ? "the mod envelope" : c.first == SrcSeq ? "the step sequencer" : c.first == SrcFilterEnv ? "the filter envelope" : juce::String();
            if (who.isEmpty()) continue;
            if (c.first >= SrcLfo1 && c.first < SrcLfo1 + kNumLfos)
            {
                const int k = c.first - SrcLfo1;
                const float rate = p.get (lfoRateParam (k));
                who = ((int) p.get (lfoSyncParam (k)) > 0 ? "a tempo-synced LFO" : rate < 0.5f ? "a slow LFO" : rate < 3.0f ? "a steady LFO" : "a fast LFO");
            }
            const auto& names = modTargetNames();
            const juce::String target = c.second >= 0 && c.second < names.size() ? names[c.second] : "a knob";
            moves.add (who + " on " + target);
            if (moves.size() == 2) break;
        }
        t.add (moves.isEmpty() ? juce::String ("no modulation, a still sound") : "movement: " + moves.joinIntoString (" and "));
    }
    // Space
    {
        juce::StringArray fx;
        static const char* const names[kNumFx] = { "distortion", "EQ", "chorus", "hyper width", "tape", "delay", "reverb", "compression" };
        for (int e : activeChain (p)) fx.add (names[e]);
        t.add (fx.isEmpty() ? juce::String ("dry, no effects") : "space: " + fx.joinIntoString (", "));
    }
    // Character
    {
        juce::StringArray ch;
        const int uni = (int) p.get (P::unison_voices);
        if (uni > 1) ch.add (juce::String (uni) + "-voice unison, wide");
        if (p.get (P::glide) > 0.02f) ch.add ("glide between notes");
        if ((int) p.get (P::arp_mode) > 0) ch.add ("arpeggiated");
        if (p.get (P::char_drift) > 0.4f) ch.add ("drifting, analog");
        if (! ch.isEmpty()) t.add (ch.joinIntoString (", "));
    }
    return t;
}

void applyIdeaMove (Patch& p, int move, juce::int64 seed)
{
    juce::Random rng (seed * 31 + move);
    auto uni = [&] (float a, float b) { return a + (b - a) * rng.nextFloat(); };
    auto pick = [&] (std::initializer_list<int> xs) { auto it = xs.begin(); std::advance (it, rng.nextInt ((int) xs.size())); return *it; };
    switch (((move % kNumIdeaMoves) + kNumIdeaMoves) % kNumIdeaMoves)
    {
        case 0:   // another wavetable family on A, its own morph, no designed table
        {
            const int current = (int) p.get (P::oscA_wave);
            int w = current;
            for (int tries = 0; tries < 8 && (w == current); ++tries) w = rng.nextInt (WavetableBank::kNumBuiltIn);
            p.set (P::oscA_wave, (float) w);
            p.set (P::oscA_morph, uni (0.1f, 0.9f));
            p.waves[0] = WaveSpec();
            if (rng.nextFloat() < 0.4f) { p.set (P::oscA_warp, (float) pick ({ 1, 2, 3, 5 })); p.set (P::oscA_warp_amt, uni (0.2f, 0.6f)); }
            else p.set (P::oscA_warp_amt, 0.0f);
            break;
        }
        case 1:   // rethink the filter: type, an octave or two away, the envelope's direction
        {
            const int current = (int) p.get (P::filter_type);
            int t = current;
            for (int tries = 0; tries < 8 && t == current; ++tries) t = rng.nextInt (filterTypeNames().size());
            p.set (P::filter_type, (float) t);
            const float octaves = (rng.nextBool() ? 1.0f : -1.0f) * uni (1.0f, 2.0f);
            p.set (P::filter_cutoff, juce::jlimit (60.0f, 16000.0f, p.get (P::filter_cutoff) * std::exp2 (octaves)));
            const int cls = filterEnvClass (p);
            p.set (P::filter_env, cls == 1 ? (rng.nextBool() ? uni (2.0f, 4.0f) : uni (-4.0f, -2.0f)) : cls == 2 ? uni (-3.0f, -2.0f) : uni (2.0f, 3.5f));
            p.set (P::fenv_decay, uni (0.15f, 1.2f));
            p.set (P::filter_res, uni (0.1f, 0.6f));
            break;
        }
        case 2:   // new movement: a free LFO on a target the sound does not move yet
        {
            int slot = -1;
            for (int i = 0; i < kNumModSlots && slot < 0; ++i) if ((int) p.get (modSourceParam (i)) == SrcOff) slot = i;
            if (slot < 0) slot = kNumModSlots - 1;   // every slot taken: the last one is rewritten
            int lfo = 0;
            const auto used = connections (p);
            for (int k = 0; k < kNumLfos; ++k) { bool u = false; for (const auto& c : used) u = u || c.first == SrcLfo1 + k; if (! u) { lfo = k; break; } }
            p.set (lfoShapeParam (lfo), (float) pick ({ ShapeSine, ShapeTriangle, ShapeSaw, ShapeSquare, ShapeRandom }));
            const bool synced = rng.nextFloat() < 0.4f;
            p.set (lfoSyncParam (lfo), synced ? (float) pick ({ 2, 3, 4, 5 }) : 0.0f);
            p.set (lfoRateParam (lfo), std::exp (uni (std::log (0.08f), std::log (6.0f))));
            static const char* const knobTargets[] = { "oscA_morph", "filter_cutoff", "chorus_mix", "reverb_mix", "oscB_level", "unison_detune", "filter_res", "delay_mix" };
            int target = TargetOff; float amount = 0.5f;
            switch (rng.nextInt (4))
            {
                case 0: target = TargetPan; amount = uni (0.3f, 0.7f); break;
                case 1: target = TargetAmp; amount = uni (0.2f, 0.5f); break;
                case 2: target = TargetPitch; amount = uni (0.02f, 0.05f); break;
                default:
                {
                    const int idx = paramIndexForId (knobTargets[rng.nextInt ((int) std::size (knobTargets))]);
                    target = idx >= 0 ? modTargetForParam (idx) : TargetAmp;
                    amount = uni (0.3f, 0.7f) * (rng.nextBool() ? 1.0f : -1.0f);
                    break;
                }
            }
            if (target <= TargetOff) target = TargetAmp;
            p.set (modSourceParam (slot), (float) (SrcLfo1 + lfo));
            p.set (modDestParam (slot), (float) target);
            p.set (modAmountParam (slot), amount);
            break;
        }
        case 3:   // the filter envelope does the shaping: a sweep up or down with its own decay
        {
            const bool down = rng.nextBool();
            p.set (P::filter_env, down ? uni (2.5f, 4.0f) : uni (-4.0f, -2.5f));
            p.set (P::fenv_attack, down ? uni (0.0f, 0.02f) : uni (0.2f, 1.5f));
            p.set (P::fenv_decay, uni (0.2f, 1.5f));
            p.set (P::fenv_sustain, uni (0.0f, 0.4f));
            if (down) p.set (P::filter_cutoff, juce::jlimit (60.0f, 4000.0f, p.get (P::filter_cutoff) * 0.5f));
            break;
        }
        case 4:   // a different space: flip one effect, change a mode, sometimes reorder
        {
            int e = pick ({ 0, 2, 4, 5, 6, 3 });
            const int active = (int) activeChain (p).size();
            if (fxActive (p, e) && active <= 1)   // the only effect: bring another one in instead of switching it off
                for (int candidate : { 6, 5, 2, 4, 3, 0 }) if (! fxActive (p, candidate)) { e = candidate; break; }
            const bool on = fxActive (p, e);
            if (on && active > 1)
                p.set (fxOnParam (e), 0.0f);
            else
            {
                p.set (fxOnParam (e), 1.0f);
                switch (e)
                {
                    case 0: p.set (P::dist_mix, uni (0.3f, 0.7f)); p.set (P::dist_drive, uni (6.0f, 18.0f)); p.set (P::dist_mode, (float) rng.nextInt (5)); break;
                    case 2: p.set (P::chorus_mix, uni (0.25f, 0.5f)); p.set (P::chorus_mode, (float) rng.nextInt (4)); break;
                    case 3: p.set (P::hyper_amount, uni (0.3f, 0.7f)); break;
                    case 4: p.set (P::tape_amount, uni (0.3f, 0.8f)); p.set (P::tape_wow, uni (0.1f, 0.5f)); break;
                    case 5: p.set (P::delay_mix, uni (0.2f, 0.45f)); p.set (P::delay_feedback, uni (0.3f, 0.65f)); p.set (P::delay_mode, (float) rng.nextInt (3)); p.set (P::delay_sync, (float) pick ({ 0, 2, 3 })); break;
                    default: p.set (P::reverb_mix, uni (0.25f, 0.55f)); p.set (P::reverb_type, (float) rng.nextInt (reverbTypeNames().size())); p.set (P::reverb_size, uni (0.3f, 0.9f)); break;
                }
            }
            if (rng.nextFloat() < 0.5f)   // swap two positions in the chain
            {
                const int x = rng.nextInt (kNumFx), y = rng.nextInt (kNumFx);
                const float px = p.get (fxPosParam (x)), py = p.get (fxPosParam (y));
                p.set (fxPosParam (x), py); p.set (fxPosParam (y), px);
            }
            break;
        }
        case 5:   // FM in or out
        {
            if (p.get (P::fm_amount) < 0.05f)
            {
                p.set (P::fm_amount, uni (0.3f, 0.6f));
                p.set (P::oscB_level, juce::jmax (0.3f, p.get (P::oscB_level)));
                p.set (P::oscB_coarse, (float) pick ({ 7, 12, 19, 24, -12 }));
                p.set (P::oscB_wave, 0.0f);   // a sine modulator
            }
            else
            {
                p.set (P::fm_amount, 0.0f);
                p.set (P::oscB_coarse, (float) pick ({ 0, 12, -12 }));
                p.set (P::oscB_wave, (float) rng.nextInt (WavetableBank::kNumBuiltIn));
            }
            break;
        }
        case 6:   // unison in or out
        {
            if (p.get (P::unison_voices) < 1.5f)
            {
                p.set (P::unison_voices, (float) pick ({ 3, 4, 5, 6 }));
                p.set (P::unison_detune, uni (10.0f, 30.0f));
                p.set (P::unison_spread, uni (0.5f, 1.0f));
            }
            else
            {
                p.set (P::unison_voices, 1.0f);
                if (rng.nextBool()) { p.set (P::chorus_mix, uni (0.3f, 0.5f)); p.set (fxOnParam (2), 1.0f); }
            }
            break;
        }
        default:  // oscillator B rethought: present or not, another wave, another interval
        {
            if (p.get (P::oscB_level) > 0.05f && rng.nextFloat() < 0.35f)
                p.set (P::oscB_level, 0.0f);
            else
            {
                const int wave = (int) p.get (P::oscB_wave), coarse = juce::roundToInt (p.get (P::oscB_coarse));
                p.set (P::oscB_level, uni (0.4f, 0.8f));
                int w = wave;
                for (int tries = 0; tries < 8 && w == wave; ++tries) w = rng.nextInt (WavetableBank::kNumBuiltIn);
                p.set (P::oscB_wave, (float) w);
                int c = coarse;
                for (int tries = 0; tries < 8 && std::abs (c - coarse) < 5; ++tries) c = pick ({ -12, -5, 0, 7, 12, 19 });
                p.set (P::oscB_coarse, (float) c);
                p.set (P::oscB_fine, uni (-8.0f, 8.0f));
                p.waves[1] = WaveSpec();
            }
            break;
        }
    }
}

void keepIdentity (Patch& child, const Patch& parent)
{
    const auto cls = envelopeClass (parent);
    if (cls == "swelling pad")
        child.set (P::aenv_attack, juce::jmax (child.get (P::aenv_attack), juce::jmax (0.12f, 0.4f * parent.get (P::aenv_attack))));
    else if (cls == "short pluck" || cls == "percussive")
        child.set (P::aenv_sustain, juce::jmin (child.get (P::aenv_sustain), 0.3f));
    if (child.category.isEmpty()) child.category = parent.category;
}

} // namespace stacks
