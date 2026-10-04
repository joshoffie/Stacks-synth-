#include "Parameters.h"

namespace stacks
{

const juce::StringArray& waveNames()
{
    static const juce::StringArray names { "Sine", "Triangle", "Saw", "Pulse", "Sync",
                                           "Organ", "Formant", "Glass", "Fold", "Grit" };
    return names;
}

const juce::StringArray& filterTypeNames()
{
    static const juce::StringArray names { "LP12", "LP24", "HP12", "HP24", "BP12", "BP24" };
    return names;
}

const juce::StringArray& lfoShapeNames()
{
    static const juce::StringArray names { "Sine", "Triangle", "Saw", "Square", "Random" };
    return names;
}

const juce::StringArray& lfoDestNames()
{
    static const juce::StringArray names { "Off", "Pitch", "Filter", "Morph A", "Morph B", "FM", "Amp", "Pan" };
    return names;
}

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
                    pid, s.name, *s.choices, (int) s.def));
                break;
        }
    }

    return layout;
}

} // namespace stacks
