#include "Parameters.h"

namespace stacks
{

const juce::StringArray& waveNames()
{
    static const juce::StringArray names { "Sine", "Triangle", "Saw", "Pulse", "Sync",
                                           "Organ", "Formant", "Glass", "Fold", "Grit",
                                           "User 1", "User 2", "User 3", "User 4", "Custom" };
    return names;
}

const juce::StringArray& filterTypeNames()
{
    static const juce::StringArray names { "LP12", "LP24", "HP12", "HP24", "BP12", "BP24", "Notch", "Comb", "Formant" };
    return names;
}

const juce::StringArray& distModeNames()
{
    static const juce::StringArray names { "Soft", "Hard", "Tube", "Fold", "Crush" };
    return names;
}

const juce::StringArray& warpNames()
{
    static const juce::StringArray names { "Off", "Sync", "Bend", "PWM", "Mirror", "Fold", "Quantize" };
    return names;
}

const juce::StringArray& arpModeNames()
{
    static const juce::StringArray names { "Off", "Up", "Down", "Up-Down", "Random", "As Played" };
    return names;
}

const juce::StringArray& arpRateNames()
{
    static const juce::StringArray names { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    return names;
}

const juce::StringArray& macroNames()
{
    static const juce::StringArray names { "Brightness", "Movement", "Grit", "Space", "Width", "Length" };
    return names;
}

const juce::StringArray& lfoShapeNames()
{
    static const juce::StringArray names { "Sine", "Triangle", "Saw", "Ramp", "Square", "Random", "Custom" };
    return names;
}

const juce::StringArray& lfoSyncNames()
{
    static const juce::StringArray names { "Free", "1/16", "1/8T", "1/8", "1/8D", "1/4", "1/2", "1 bar", "2 bars" };
    return names;
}

float lfoSyncBeats (int sync)
{
    static const float beats[] = { 0.0f, 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 1.0f, 2.0f, 4.0f, 8.0f };
    return sync > 0 && sync < 9 ? beats[sync] : 0.0f;
}

const juce::StringArray& lfoModeNames()
{
    static const juce::StringArray names { "Free", "Note" };
    return names;
}

const juce::StringArray& modSourceNames()
{
    static const juce::StringArray names { "Off", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Filter Env", "Mod Env", "Velocity", "Key", "Mod Wheel", "Aftertouch", "Random",
                                           "Brightness", "Movement", "Grit", "Space", "Width", "Length", "Slide" };
    return names;
}

namespace
{
    // Built once: the virtual targets, then one entry per modulatable parameter.
    struct TargetTable
    {
        juce::StringArray names;
        std::vector<int> paramForTarget;   // -1 for virtual targets
        std::vector<int> targetForParam;   // -1 when not modulatable

        TargetTable()
        {
            names.addArray ({ "Off", "Pitch", "Pitch B", "Amp", "Pan" });
            paramForTarget.assign ((size_t) kNumVirtualTargets, -1);
            targetForParam.assign ((size_t) kNumParams, -1);

            const auto& specs = paramSpecs();
            for (int i = 0; i < kNumParams; ++i)
            {
                const auto& sp = specs[(size_t) i];
                const juce::String id (sp.id);
                const bool modSlot = id.startsWith ("mod") && (id.endsWith ("_amount") || id.endsWith ("_source") || id.endsWith ("_dest"));
                const juce::String group (sp.group);
                if (sp.kind != ParamKind::Float || modSlot || id == "master_gain" || group == "MACROS" || group == "ARP")
                    continue;   // macros and the arp are sources and a player, not targets
                targetForParam[(size_t) i] = names.size();
                names.add (sp.name);
                paramForTarget.push_back (i);
            }
        }
    };

    const TargetTable& targets()
    {
        static const TargetTable table;
        return table;
    }
}

const juce::StringArray& modTargetNames()          { return targets().names; }
int modTargetParamIndex (int target)               { const auto& t = targets().paramForTarget; return target >= 0 && target < (int) t.size() ? t[(size_t) target] : -1; }
int modTargetForParam (int paramIndex)             { const auto& t = targets().targetForParam; return paramIndex >= 0 && paramIndex < (int) t.size() ? t[(size_t) paramIndex] : -1; }
bool isModulatableParam (int paramIndex)           { return modTargetForParam (paramIndex) > 0; }

const juce::NormalisableRange<float>& paramRange (int paramIndex)
{
    static const std::vector<juce::NormalisableRange<float>> ranges = []
    {
        std::vector<juce::NormalisableRange<float>> out;
        for (const auto& sp : paramSpecs())
        {
            juce::NormalisableRange<float> r (sp.min, sp.kind == ParamKind::Choice ? (float) (sp.choices().size() - 1) : sp.max);
            if (sp.skewCentre > 0.0f)
                r.setSkewForCentre (sp.skewCentre);
            out.push_back (r);
        }
        return out;
    }();
    return ranges[(size_t) juce::jlimit (0, kNumParams - 1, paramIndex)];
}

const juce::StringArray& chorusModeNames()
{
    static const juce::StringArray names { "Chorus", "Ensemble", "Flanger", "Dimension" };
    return names;
}

const juce::StringArray& delayModeNames()
{
    static const juce::StringArray names { "Stereo", "Ping-Pong", "Tape" };
    return names;
}

const juce::StringArray& delaySyncNames()
{
    static const juce::StringArray names { "Free", "1/16", "1/8T", "1/8", "1/8D", "1/4", "1/4D", "1/2" };
    return names;
}

const juce::StringArray& reverbTypeNames()
{
    static const juce::StringArray names { "Room", "Plate", "Hall", "Shimmer" };
    return names;
}

bool isAdvancedParam (const char* id)
{
    static const juce::StringArray advanced { "chorus_voices", "chorus_feedback", "chorus_spread", "chorus_tone",
                                              "delay_tone", "delay_hpf", "delay_wow", "delay_width",
                                              "reverb_predelay", "reverb_lowcut", "reverb_highcut", "reverb_mod", "reverb_shimmer", "reverb_width",
                                              "dist_tone", "eq_low_freq", "eq_mid_freq", "eq_mid_q", "eq_high_freq", "comp_attack", "comp_release", "arp_swing", "bend_range" };
    return advanced.contains (id);
}

namespace
{
    // The slot and LFO parameters are laid out contiguously in the table, so
    // the k-th one is a fixed stride from the first.
    P offsetFrom (P first, int k, int stride) { return (P) ((int) first + k * stride); }
}

P modSourceParam (int slot) { return offsetFrom (P::mod1_source, juce::jlimit (0, kNumModSlots - 1, slot), 3); }
P modDestParam (int slot)   { return offsetFrom (P::mod1_dest,   juce::jlimit (0, kNumModSlots - 1, slot), 3); }
P modAmountParam (int slot) { return offsetFrom (P::mod1_amount, juce::jlimit (0, kNumModSlots - 1, slot), 3); }

P macroParam (int k)    { return offsetFrom (P::macro1, juce::jlimit (0, kNumMacros - 1, k), 1); }
P lfoShapeParam (int k) { return offsetFrom (P::lfo1_shape, juce::jlimit (0, kNumLfos - 1, k), 5); }
P lfoRateParam (int k)  { return offsetFrom (P::lfo1_rate,  juce::jlimit (0, kNumLfos - 1, k), 5); }
P lfoSyncParam (int k)  { return offsetFrom (P::lfo1_sync,  juce::jlimit (0, kNumLfos - 1, k), 5); }
P lfoPhaseParam (int k) { return offsetFrom (P::lfo1_phase, juce::jlimit (0, kNumLfos - 1, k), 5); }
P lfoModeParam (int k)  { return offsetFrom (P::lfo1_mode,  juce::jlimit (0, kNumLfos - 1, k), 5); }

const std::vector<ParamSpec>& paramSpecs()
{
    static const std::vector<ParamSpec> specs = {
#define STACKS_SPEC_ENTRY(id, name, group, kind, mn, mx, def, centre, unit, choices, hint) \
        ParamSpec { #id, name, group, ParamKind::kind, (float) (mn), (float) (mx), (float) (def), \
                    (float) (centre), unit, choices, hint },
        STACKS_PARAMS(STACKS_SPEC_ENTRY)
#undef STACKS_SPEC_ENTRY
    };
    return specs;
}

int paramIndexForId(const juce::String& id)
{
    const auto& specs = paramSpecs();
    for (int i = 0; i < (int) specs.size(); ++i)
        if (id == specs[(size_t) i].id)
            return i;
    return -1;
}

namespace
{
    // Compact readouts for the knobs and the host: "1.2k", "440", "12ms", "0.30s", "-6.0dB".
    juce::String formatValue (const ParamSpec& s, float v)
    {
        const juce::String unit (s.unit);
        if (unit == "Hz")
        {
            if (v >= 1000.0f) return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + "k";
            if (v >= 100.0f)  return juce::String (juce::roundToInt (v));
            return juce::String (v, 1);
        }
        if (unit == "s")   return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + "ms" : juce::String (v, 2) + "s";
        if (unit == "ms")  return juce::String (juce::roundToInt (v)) + "ms";
        if (unit == "dB")  return juce::String (v, 1) + "dB";
        if (unit == "ct")  return juce::String (v, 1);
        return juce::String (v, 2);
    }

    float parseValue (const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        float scale = 1.0f;
        if (t.endsWith ("khz") || t.endsWith ("k")) scale = 1000.0f;
        else if (t.endsWith ("ms"))                  scale = 0.001f;
        return t.retainCharacters ("0123456789.-+").getFloatValue() * scale;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& s : paramSpecs())
    {
        const juce::ParameterID pid { s.id, 1 };

        switch (s.kind)
        {
            case ParamKind::Float:
            {
                juce::NormalisableRange<float> range (s.min, s.max);
                if (s.skewCentre > 0.0f)
                    range.setSkewForCentre (s.skewCentre);

                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, s.name, range, s.def,
                    juce::AudioParameterFloatAttributes()
                        .withLabel (s.unit)
                        .withStringFromValueFunction ([&s] (float v, int) { return formatValue (s, v); })
                        .withValueFromStringFunction ([] (const juce::String& t) { return parseValue (t); })));
                break;
            }
            case ParamKind::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (
                    pid, s.name, (int) s.min, (int) s.max, (int) s.def,
                    juce::AudioParameterIntAttributes().withLabel (s.unit)));
                break;

            case ParamKind::Choice:
                jassert (s.choices != nullptr);
                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    pid, s.name, s.choices(), (int) s.def));
                break;
        }
    }

    return layout;
}

} // namespace stacks
