#include "Patch.h"

namespace stacks
{

Patch::Patch()
{
    const auto& specs = paramSpecs();
    for (int i = 0; i < kNumParams; ++i)
        values[(size_t) i] = specs[(size_t) i].def;
}

void Patch::set (int index, float v) noexcept
{
    if (index < 0 || index >= kNumParams)
        return;

    const auto& s = paramSpecs()[(size_t) index];
    v = juce::jlimit (s.min, s.max, v);
    if (s.kind != ParamKind::Float)
        v = std::round (v);
    values[(size_t) index] = v;
}

bool Patch::sameValuesAs (const Patch& other) const noexcept
{
    for (int i = 0; i < kNumParams; ++i)
        if (std::abs (values[(size_t) i] - other.values[(size_t) i]) > 1.0e-4f)
            return false;
    return true;
}

juce::var Patch::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("name", name);
    obj->setProperty ("description", description);
    obj->setProperty ("category", category);
    if (origin.isNotEmpty())
        obj->setProperty ("origin", origin);
    obj->setProperty ("params", paramsToVar());
    return juce::var (obj);
}

juce::var Patch::paramsToVar() const
{
    auto* params = new juce::DynamicObject();
    const auto& specs = paramSpecs();

    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& s = specs[(size_t) i];
        const float v = values[(size_t) i];

        switch (s.kind)
        {
            case ParamKind::Choice:
            {
                const int idx = juce::jlimit (0, s.choices->size() - 1, (int) std::round (v));
                params->setProperty (s.id, (*s.choices)[idx]);
                break;
            }
            case ParamKind::Int:
                params->setProperty (s.id, (int) std::round (v));
                break;
            case ParamKind::Float:
                params->setProperty (s.id, std::round ((double) v * 10000.0) / 10000.0);
                break;
        }
    }

    return juce::var (params);
}

juce::String Patch::toJson() const
{
    return juce::JSON::toString (toVar());
}

std::optional<Patch> Patch::fromVar (const juce::var& v, const Patch* base)
{
    auto* obj = v.getDynamicObject();
    if (obj == nullptr)
        return std::nullopt;

    Patch p = base != nullptr ? *base : Patch();
    p.name        = obj->getProperty ("name").toString().trim();
    p.description = obj->getProperty ("description").toString().trim();
    p.category    = obj->getProperty ("category").toString().trim();
    if (p.name.isEmpty())
        p.name = base != nullptr ? base->name : juce::String ("Untitled");
    if (p.category.isEmpty() && base != nullptr)
        p.category = base->category;
    p.origin = obj->getProperty ("origin").toString().trim();

    auto* params = obj->getProperty ("params").getDynamicObject();
    if (params == nullptr)
        params = obj; // tolerate a flat object with the ids at top level

    const auto& specs = paramSpecs();

    for (const auto& prop : params->getProperties())
    {
        const int index = paramIndexForId (prop.name.toString());
        if (index < 0)
            continue;

        const auto& s = specs[(size_t) index];
        const auto& value = prop.value;

        if (s.kind == ParamKind::Choice && value.isString())
        {
            const auto text = value.toString().trim();
            int idx = -1;
            for (int i = 0; i < s.choices->size() && idx < 0; ++i)
                if ((*s.choices)[i].equalsIgnoreCase (text))
                    idx = i;
            for (int i = 0; i < s.choices->size() && idx < 0; ++i)
                if ((*s.choices)[i].containsIgnoreCase (text) || text.containsIgnoreCase ((*s.choices)[i]))
                    idx = i;
            if (idx >= 0)
                p.set (index, (float) idx);
            continue;
        }

        if (value.isDouble() || value.isInt() || value.isInt64() || value.isBool())
        {
            p.set (index, (float) (double) value);
        }
        else if (value.isString())
        {
            // Be forgiving about things like "1200 Hz" or "0.5s".
            const auto numeric = value.toString().retainCharacters ("0123456789.-+eE");
            if (numeric.isNotEmpty())
                p.set (index, numeric.getFloatValue());
        }
    }

    return p;
}

std::optional<Patch> Patch::fromJson (const juce::String& text, const Patch* base)
{
    auto parsed = juce::JSON::parse (text);
    if (parsed.isVoid())
        return std::nullopt;
    return fromVar (parsed, base);
}

Patch Patch::capture (const juce::AudioProcessorValueTreeState& apvts)
{
    Patch p;
    const auto& specs = paramSpecs();
    for (int i = 0; i < kNumParams; ++i)
        if (auto* raw = apvts.getRawParameterValue (specs[(size_t) i].id))
            p.values[(size_t) i] = raw->load();
    return p;
}

void Patch::applyTo (juce::AudioProcessorValueTreeState& apvts) const
{
    const auto& specs = paramSpecs();
    for (int i = 0; i < kNumParams; ++i)
        if (auto* param = apvts.getParameter (specs[(size_t) i].id))
            param->setValueNotifyingHost (param->convertTo0to1 (values[(size_t) i]));
}

//==============================================================================
namespace
{
    float snapTo (float value, std::initializer_list<float> allowed)
    {
        float best = *allowed.begin();
        for (float a : allowed)
            if (std::abs (a - value) < std::abs (best - value))
                best = a;
        return best;
    }
}

void keepPatchInTune (Patch& p)
{
    // Oscillator A carries the note: octaves only.
    p.set (P::oscA_coarse, snapTo (p.get (P::oscA_coarse), { -24.0f, -12.0f, 0.0f, 12.0f, 24.0f }));
    p.set (P::oscA_fine, juce::jlimit (-12.0f, 12.0f, p.get (P::oscA_fine)));

    // Oscillator B: octaves when you can hear it; as a pure FM modulator the
    // harmonic ratios 1.5x (+7), 3x (+19) and 4x (+24) are fine too.
    const bool audibleB = p.get (P::oscB_level) > 0.05f;
    if (audibleB)
    {
        p.set (P::oscB_coarse, snapTo (p.get (P::oscB_coarse), { -24.0f, -12.0f, 0.0f, 12.0f, 24.0f }));
        p.set (P::oscB_fine, juce::jlimit (-20.0f, 20.0f, p.get (P::oscB_fine)));
    }
    else
    {
        p.set (P::oscB_coarse, snapTo (p.get (P::oscB_coarse), { -24.0f, -12.0f, 0.0f, 7.0f, 12.0f, 19.0f, 24.0f }));
        p.set (P::oscB_fine, juce::jlimit (-8.0f, 8.0f, p.get (P::oscB_fine)));
    }

    // Pitch modulation: vibrato, a short attack drop or a performance bend,
    // never a per-note detune (Velocity/Key/Random would put chords out of tune).
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int src = (int) p.get (modSourceParam (i));
        const int dst = (int) p.get (modDestParam (i));
        if (dst != DestPitch && ! (dst == DestPitchB && audibleB))
            continue;
        float cap = 0.0f;
        switch (src)
        {
            case SrcLfo1: case SrcLfo2:            cap = 0.08f; break; // ~1 semitone vibrato
            case SrcFilterEnv: case SrcModEnv:     cap = 0.35f; break; // pluck / drum pitch drop
            case SrcModWheel: case SrcAftertouch:  cap = 0.17f; break; // whole-tone bend
            default:                               cap = 0.0f;  break; // Velocity, Key, Random: no
        }
        const float a = p.get (modAmountParam (i));
        p.set (modAmountParam (i), juce::jlimit (-cap, cap, a));
    }

    p.set (P::unison_detune, juce::jmin (35.0f, p.get (P::unison_detune)));
}

namespace
{
    juce::String hzText (float hz)
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, 1) + " kHz";
        return juce::String ((int) std::round (hz)) + " Hz";
    }

    juce::String oscText (const Patch& p, P wave, P coarse)
    {
        auto text = waveNames()[juce::jlimit (0, waveNames().size() - 1, (int) p.get (wave))];
        const int st = (int) p.get (coarse);
        if (st != 0)
            text << "(" << (st > 0 ? "+" : "") << st << ")";
        return text;
    }
}

juce::String describePatch (const Patch& p)
{
    juce::StringArray parts;

    // Sources
    juce::String src = oscText (p, P::oscA_wave, P::oscA_coarse);
    if (p.get (P::oscB_level) > 0.05f)
        src << " + " << oscText (p, P::oscB_wave, P::oscB_coarse);
    const float fm = p.get (P::fm_amount);
    if (fm > 0.05f)
        src << (fm > 0.3f ? ", heavy FM" : ", light FM");
    parts.add (src);

    const int unison = (int) p.get (P::unison_voices);
    if (unison > 1)
        parts.add (juce::String (unison) + "-voice unison");
    if (p.get (P::sub_level) > 0.2f)  parts.add ("sub");
    if (p.get (P::noise_level) > 0.1f) parts.add ("noise");

    // Filter
    juce::String filt = filterTypeNames()[juce::jlimit (0, filterTypeNames().size() - 1, (int) p.get (P::filter_type))];
    filt << " @ " << hzText (p.get (P::filter_cutoff));
    if (p.get (P::filter_res) > 0.5f)  filt << ", resonant";
    if (p.get (P::filter_drive) > 3.0f) filt << ", driven";
    if (std::abs (p.get (P::filter_env)) > 1.5f) filt << (p.get (P::filter_env) > 0 ? ", sweeping" : ", inverted sweep");
    parts.add (filt);

    // Envelope character
    const float att = p.get (P::aenv_attack), dec = p.get (P::aenv_decay), sus = p.get (P::aenv_sustain), rel = p.get (P::aenv_release);
    if (att > 0.5f)                parts.add ("slow attack");
    else if (dec < 0.4f && sus < 0.2f) parts.add ("plucky");
    if (rel > 1.5f)                parts.add ("long tail");
    if (p.get (P::glide) > 0.02f)  parts.add ("glide");

    // Motion
    static const char* shortSource[] = { "", "LFO1", "LFO2", "FEnv", "MEnv", "Vel", "Key", "Wheel", "AT", "Rnd" };
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int src = juce::jlimit (0, kNumModSources - 1, (int) p.get (modSourceParam (i)));
        const int dst = juce::jlimit (0, kNumModDests - 1, (int) p.get (modDestParam (i)));
        if (src == SrcOff || dst == DestOff || std::abs (p.get (modAmountParam (i))) < 0.02f)
            continue;
        parts.add (juce::String (shortSource[src]) + " > " + modDestNames()[dst]);
    }

    // Space
    if (p.get (P::chorus_mix) > 0.2f) parts.add ("chorus");
    if (p.get (P::delay_mix) > 0.15f) parts.add ("delay");
    const float rev = p.get (P::reverb_mix);
    if (rev > 0.45f)       parts.add ("big reverb");
    else if (rev > 0.25f)  parts.add ("reverb");

    return parts.joinIntoString (", ");
}

} // namespace stacks
