#include "PatchExplainer.h"
#include "LlmPatchGenerator.h"

namespace stacks
{

namespace
{
    juce::String hz (float v)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String ((int) std::round (v)) + " Hz";
    }

    juce::String seconds (float v)
    {
        return v < 1.0f ? juce::String ((int) std::round (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s";
    }

    // The patch as a sound designer would read it off the panel: knob names as
    // shown, with values and units, only what matters. Far easier for a small
    // model than a hundred raw parameters.
    juce::String digest (const Patch& p)
    {
        juce::StringArray lines;
        const int waveA = (int) p.get (P::oscA_wave), waveB = (int) p.get (P::oscB_wave);
        juce::String a = waveNames()[juce::jlimit (0, waveNames().size() - 1, waveA)];
        if (waveA == kCustomWave && ! p.waves[0].isEmpty()) a = "a designed wavetable called \"" + p.waves[0].name + "\"";
        lines.add ("Osc A: " + a + ", A Morph " + juce::String (p.get (P::oscA_morph), 2) + ", A Level " + juce::String (p.get (P::oscA_level), 2)
                   + (p.get (P::oscA_coarse) != 0.0f ? ", A Coarse " + juce::String ((int) p.get (P::oscA_coarse)) + " st" : juce::String()));
        if (p.get (P::oscB_level) > 0.02f || p.get (P::fm_amount) > 0.02f)
            lines.add ("Osc B: " + waveNames()[juce::jlimit (0, waveNames().size() - 1, waveB)] + ", B Coarse " + juce::String ((int) p.get (P::oscB_coarse)) + " st, B Level "
                       + juce::String (p.get (P::oscB_level), 2) + (p.get (P::fm_amount) > 0.02f ? ", FM B>A " + juce::String (p.get (P::fm_amount), 2) + " (B frequency-modulates A)" : juce::String()));
        if (p.get (P::sub_level) > 0.05f)   lines.add ("Sub " + juce::String (p.get (P::sub_level), 2) + " (sine an octave down)");
        if (p.get (P::noise_level) > 0.05f) lines.add ("Noise " + juce::String (p.get (P::noise_level), 2));
        lines.add ("Filter: " + filterTypeNames()[juce::jlimit (0, filterTypeNames().size() - 1, (int) p.get (P::filter_type))] + ", Cutoff " + hz (p.get (P::filter_cutoff))
                   + ", Resonance " + juce::String (p.get (P::filter_res), 2) + (p.get (P::filter_drive) > 1.5f ? ", Drive " + juce::String (p.get (P::filter_drive), 1) : juce::String())
                   + ", Filt Env " + juce::String (p.get (P::filter_env), 1) + " oct (F Decay " + seconds (p.get (P::fenv_decay)) + ", F Sustain " + juce::String (p.get (P::fenv_sustain), 2) + ")");
        lines.add ("Amp envelope: Attack " + seconds (p.get (P::aenv_attack)) + ", Decay " + seconds (p.get (P::aenv_decay)) + ", Sustain " + juce::String (p.get (P::aenv_sustain), 2) + ", Release " + seconds (p.get (P::aenv_release)));
        for (int i = 0; i < kNumModSlots; ++i)
        {
            const int src = (int) p.get (modSourceParam (i)), dst = (int) p.get (modDestParam (i));
            if (src == SrcOff || dst == TargetOff) continue;
            juce::String line = modSourceNames()[juce::jlimit (0, modSourceNames().size() - 1, src)] + " > " + modTargetNames()[juce::jlimit (0, modTargetNames().size() - 1, dst)]
                                + " depth " + juce::String (p.get (modAmountParam (i)), 2);
            if (src >= SrcLfo1 && src <= SrcLfo4)
            {
                const int k = src - SrcLfo1;
                line << " (LFO " << (k + 1) << ": " << lfoShapeNames()[juce::jlimit (0, lfoShapeNames().size() - 1, (int) p.get (lfoShapeParam (k)))] << " at " << juce::String (p.get (lfoRateParam (k)), 2) << " Hz)";
            }
            lines.add (line);
        }
        const int unison = (int) p.get (P::unison_voices);
        if (unison > 1) lines.add ("Unison " + juce::String (unison) + " voices, Detune " + juce::String ((int) p.get (P::unison_detune)) + " cents, Spread " + juce::String (p.get (P::unison_spread), 2));
        if (p.get (P::glide) > 0.01f) lines.add ("Glide " + seconds (p.get (P::glide)));
        juce::StringArray fx;
        if (p.get (P::chorus_mix) > 0.05f) fx.add ("Chorus Mix " + juce::String (p.get (P::chorus_mix), 2) + " (" + chorusModeNames()[juce::jlimit (0, chorusModeNames().size() - 1, (int) p.get (P::chorus_mode))] + ")");
        if (p.get (P::delay_mix) > 0.05f)  fx.add ("Delay Mix " + juce::String (p.get (P::delay_mix), 2) + ", Delay FB " + juce::String (p.get (P::delay_feedback), 2));
        if (p.get (P::reverb_mix) > 0.05f) fx.add ("Reverb Mix " + juce::String (p.get (P::reverb_mix), 2) + " (" + reverbTypeNames()[juce::jlimit (0, reverbTypeNames().size() - 1, (int) p.get (P::reverb_type))] + ", Size " + juce::String (p.get (P::reverb_size), 2) + ")");
        lines.add (fx.isEmpty() ? juce::String ("Effects: none (dry)") : "Effects: " + fx.joinIntoString (", "));
        return lines.joinIntoString ("\n");
    }
}

juce::String PatchExplainer::userPrompt (const Patch& p)
{
    juce::String s;
    s << "Patch \"" << p.name << "\"" << (p.category.isNotEmpty() ? " (" + p.category + ")" : juce::String()) << ", as the panel shows it:\n"
      << digest (p) << "\n\n"
      << "Talk directly to the player, who is new to synthesizers. Under WHY IT SOUNDS LIKE THIS, write three to five sentences on what gives this "
      << "particular sound its character: which of the settings above matter most and what each one does to what they hear. Use the knob names exactly as "
      << "written above. Be specific to these values - no generalities about synthesis, no restating the task.\n"
      << "Under TRY, give three knob moves, one per line, each as '- Knob: new setting - what you will hear', chosen so each one changes this sound in a "
      << "clearly different direction (darker, more movement, shorter, wider...). /no_think";
    return s;
}

bool PatchExplainer::explain (LlmBackend& backend, const Patch& p,
                              const std::function<void (const juce::String&)>& onText,
                              const std::function<bool()>& shouldCancel, juce::String& error)
{
    // The shape is pinned, so the model answers instead of narrating the task,
    // and every part has a length it can't overrun.
    const juce::String grammar =
        "root ::= \"WHY IT SOUNDS LIKE THIS\\n\" why \"\\n\\nTRY\\n\" tip \"\\n\" tip \"\\n\" tip\n"
        "why ::= wchar{160,560} (\"\\n\" wchar{40,320})?\n"
        "tip ::= \"- \" [A-Z] [A-Za-z0-9 />]{1,22} \": \" wchar{25,180}\n"
        "wchar ::= [^\\n\\x00-\\x1F]\n";
    return backend.chat (LlmPatchGenerator::systemPrompt (false), userPrompt (p), grammar, onText,
                         [] (const juce::String&) {}, shouldCancel, error);
}

} // namespace stacks
