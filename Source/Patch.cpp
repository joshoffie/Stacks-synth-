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
    const float maxValue = s.kind == ParamKind::Choice ? (float) (s.choices().size() - 1) : s.max;
    v = juce::jlimit (s.min, maxValue, v);
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
    if (favourite)
        obj->setProperty ("favourite", true);
    if (! tags.isEmpty())
    {
        juce::Array<juce::var> t;
        for (const auto& tag : tags) t.add (tag);
        obj->setProperty ("tags", juce::var (t));
    }
    if (parentName.isNotEmpty()) obj->setProperty ("parentName", parentName);
    if (prompt.isNotEmpty())     obj->setProperty ("prompt", prompt);
    obj->setProperty ("params", paramsToVar());

    bool anyWave = false;
    for (const auto& w : userWaves) anyWave = anyWave || w.isNotEmpty();
    if (anyWave)
    {
        auto* waves = new juce::DynamicObject();
        for (int k = 0; k < 4; ++k)
            if (userWaves[(size_t) k].isNotEmpty())
                waves->setProperty ("user" + juce::String (k + 1), userWaves[(size_t) k]);
        obj->setProperty ("userWaves", juce::var (waves));
    }

    for (int osc = 0; osc < 2; ++osc)
        if (! this->waves[(size_t) osc].isEmpty())
            obj->setProperty (osc == 0 ? "waveA" : "waveB", this->waves[(size_t) osc].toVar());

    bool anyShape = false;
    for (const auto& sh : lfoShapes) anyShape = anyShape || sh.isNotEmpty();
    if (anyShape)
    {
        auto* shapes = new juce::DynamicObject();
        for (int k = 0; k < kNumLfos; ++k)
            if (lfoShapes[(size_t) k].isNotEmpty())
                shapes->setProperty ("lfo" + juce::String (k + 1), juce::JSON::parse (lfoShapes[(size_t) k]));
        obj->setProperty ("lfoShapes", juce::var (shapes));
    }
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
                const auto& choices = s.choices();
                const int idx = juce::jlimit (0, choices.size() - 1, (int) std::round (v));
                params->setProperty (s.id, choices[idx]);
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
    p.favourite = (bool) obj->getProperty ("favourite");
    p.parentName = obj->getProperty ("parentName").toString();
    p.prompt = obj->getProperty ("prompt").toString();
    if (base != nullptr) p.tags = base->tags;
    if (auto* t = obj->getProperty ("tags").getArray())
    {
        p.tags.clear();
        for (const auto& v : *t)
        {
            const auto tag = v.toString().trim().toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789 -").trim();
            if (tag.isNotEmpty() && ! p.tags.contains (tag) && p.tags.size() < 6) p.tags.add (tag);
        }
    }
    if (base != nullptr)
    {
        p.lfoShapes = base->lfoShapes;
        p.userWaves = base->userWaves;
        p.waves = base->waves;
    }
    if (auto* waves = obj->getProperty ("userWaves").getDynamicObject())
        for (int k = 0; k < 4; ++k)
        {
            const auto v = waves->getProperty ("user" + juce::String (k + 1));
            if (v.isString()) p.userWaves[(size_t) k] = v.toString();
        }
    if (auto* shapes = obj->getProperty ("lfoShapes").getDynamicObject())
        for (int k = 0; k < kNumLfos; ++k)
        {
            const auto v = shapes->getProperty ("lfo" + juce::String (k + 1));
            if (v.isArray()) p.lfoShapes[(size_t) k] = juce::JSON::toString (v, true);
        }
    for (int osc = 0; osc < 2; ++osc)
        if (auto w = WaveSpec::fromVar (obj->getProperty (osc == 0 ? "waveA" : "waveB")))
            p.waves[(size_t) osc] = *w;

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
            const auto& choices = s.choices();
            int idx = -1;
            for (int i = 0; i < choices.size() && idx < 0; ++i)
                if (choices[i].equalsIgnoreCase (text))
                    idx = i;
            for (int i = 0; i < choices.size() && idx < 0; ++i)
                if (choices[i].containsIgnoreCase (text) || text.containsIgnoreCase (choices[i]))
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

const juce::Identifier& Patch::lfoShapesTreeType()
{
    static const juce::Identifier type ("LfoShapes");
    return type;
}

juce::Identifier Patch::lfoShapeProperty (int k)
{
    return juce::Identifier ("lfo" + juce::String (k + 1));
}

const juce::Identifier& Patch::userWavesTreeType()
{
    static const juce::Identifier type ("UserWaves");
    return type;
}

juce::Identifier Patch::userWaveProperty (int slot)
{
    return juce::Identifier ("user" + juce::String (slot + 1));
}

const juce::Identifier& Patch::customWavesTreeType()
{
    static const juce::Identifier type ("CustomWaves");
    return type;
}

juce::Identifier Patch::customWaveProperty (int osc)
{
    return juce::Identifier (osc == 0 ? "waveA" : "waveB");
}

Patch Patch::capture (const juce::AudioProcessorValueTreeState& apvts)
{
    Patch p;
    const auto& specs = paramSpecs();
    for (int i = 0; i < kNumParams; ++i)
        if (auto* raw = apvts.getRawParameterValue (specs[(size_t) i].id))
            p.values[(size_t) i] = raw->load();

    auto shapes = apvts.state.getChildWithName (lfoShapesTreeType());
    if (shapes.isValid())
        for (int k = 0; k < kNumLfos; ++k)
            p.lfoShapes[(size_t) k] = shapes.getProperty (lfoShapeProperty (k)).toString();

    auto waves = apvts.state.getChildWithName (userWavesTreeType());
    if (waves.isValid())
        for (int k = 0; k < 4; ++k)
            p.userWaves[(size_t) k] = waves.getProperty (userWaveProperty (k)).toString();

    auto custom = apvts.state.getChildWithName (customWavesTreeType());
    if (custom.isValid())
        for (int osc = 0; osc < 2; ++osc)
            if (auto w = WaveSpec::fromJson (custom.getProperty (customWaveProperty (osc)).toString()))
                p.waves[(size_t) osc] = *w;
    return p;
}

void Patch::applyTo (juce::AudioProcessorValueTreeState& apvts) const
{
    const auto& specs = paramSpecs();
    for (int i = 0; i < kNumParams; ++i)
        if (auto* param = apvts.getParameter (specs[(size_t) i].id))
            param->setValueNotifyingHost (param->convertTo0to1 (values[(size_t) i]));

    auto shapes = apvts.state.getOrCreateChildWithName (lfoShapesTreeType(), nullptr);
    for (int k = 0; k < kNumLfos; ++k)
    {
        const auto& json = lfoShapes[(size_t) k];
        if (json.isNotEmpty()) shapes.setProperty (lfoShapeProperty (k), json, nullptr);
        else                   shapes.removeProperty (lfoShapeProperty (k), nullptr);
    }

    // Only overwrite the user slots a patch actually names, so loading a patch
    // that doesn't use them leaves the player's imports alone.
    auto waves = apvts.state.getOrCreateChildWithName (userWavesTreeType(), nullptr);
    for (int k = 0; k < 4; ++k)
        if (userWaves[(size_t) k].isNotEmpty())
            waves.setProperty (userWaveProperty (k), userWaves[(size_t) k], nullptr);

    // The designed tables travel with the patch: set them, or clear them.
    auto custom = apvts.state.getOrCreateChildWithName (customWavesTreeType(), nullptr);
    for (int osc = 0; osc < 2; ++osc)
    {
        const auto& w = this->waves[(size_t) osc];
        if (! w.isEmpty()) custom.setProperty (customWaveProperty (osc), w.toJson(), nullptr);
        else               custom.removeProperty (customWaveProperty (osc), nullptr);
    }
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

    // Pitch modulation: a gentle vibrato, a short attack drop or a performance
    // bend - never a per-note detune (Velocity/Key/Random would put chords out
    // of tune) and never a stepped or random LFO, which is a trill or a drift.
    const int fineA = modTargetForParam ((int) P::oscA_fine), fineB = modTargetForParam ((int) P::oscB_fine);
    const int cutoffTarget = modTargetForParam ((int) P::filter_cutoff);
    auto lfoIsSmooth = [&] (int src)
    {
        const int shape = (int) p.get (lfoShapeParam (src - SrcLfo1));
        return shape == ShapeSine || shape == ShapeTriangle;
    };
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int src = (int) p.get (modSourceParam (i));
        const int dst = (int) p.get (modDestParam (i));
        if (src == SrcOff || dst == TargetOff)
            continue;
        const bool isLfo   = src >= SrcLfo1 && src <= SrcLfo4;
        const bool toPitch = dst == TargetPitch || (dst == TargetPitchB && audibleB);
        const bool toFine  = dst == fineA || (dst == fineB && audibleB);
        if (! toPitch && ! toFine)
            continue;
        const float a = p.get (modAmountParam (i));

        // A square, saw or random LFO on pitch: send that movement to the filter instead.
        if (isLfo && ! lfoIsSmooth (src))
        {
            p.set (modDestParam (i), (float) cutoffTarget);
            p.set (modAmountParam (i), juce::jlimit (-0.3f, 0.3f, a));
            continue;
        }

        float cap = 0.0f;
        if (toFine)
            cap = isLfo ? 0.08f : (src == SrcModWheel || src == SrcAftertouch) ? 0.1f : 0.03f; // +-16 cents vibrato, +-6 cents drift
        else switch (src)
        {
            case SrcLfo1: case SrcLfo2: case SrcLfo3: case SrcLfo4: cap = 0.03f; break; // ~+-36 cents vibrato
            case SrcFilterEnv: case SrcModEnv:     cap = 0.35f; break; // pluck / drum pitch drop
            case SrcModWheel:                      cap = 0.17f; break; // whole-tone bend
            case SrcAftertouch:                    cap = 0.08f; break;
            default:                               cap = 0.0f;  break; // Velocity, Key, Random: no
        }

        // An envelope that sustains above zero would hold the note off-key.
        if (toPitch && src == SrcModEnv && p.get (P::menv_sustain) > 0.05f)
            p.set (P::menv_sustain, 0.0f);
        if (toPitch && src == SrcFilterEnv && p.get (P::fenv_sustain) > 0.05f)
            cap = 0.0f;

        if (cap <= 0.0f)
        {
            p.set (modSourceParam (i), (float) SrcOff);
            p.set (modAmountParam (i), 0.0f);
            continue;
        }
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

    juce::String oscText (const Patch& p, P wave, P coarse, const WaveSpec& designed)
    {
        const int idx = juce::jlimit (0, waveNames().size() - 1, (int) p.get (wave));
        auto text = idx == kCustomWave && ! designed.isEmpty() ? designed.name : waveNames()[idx];
        const int st = (int) p.get (coarse);
        if (st != 0)
            text << "(" << (st > 0 ? "+" : "") << st << ")";
        return text;
    }
}

juce::StringArray autoTags (const Patch& p)
{
    juce::StringArray t;
    if (p.category.isNotEmpty()) t.add (p.category.toLowerCase());
    const float cutoff = p.get (P::filter_cutoff);
    if (cutoff < 600.0f) t.add ("dark"); else if (cutoff > 5000.0f) t.add ("bright");
    if (p.get (P::aenv_attack) > 0.4f) t.add ("slow");
    else if (p.get (P::aenv_decay) < 0.4f && p.get (P::aenv_sustain) < 0.2f) t.add ("plucky");
    if (p.get (P::unison_voices) >= 3) t.add ("wide");
    if (p.get (P::fm_amount) > 0.3f) t.add ("metallic");
    if (p.get (P::noise_level) > 0.2f) t.add ("noisy");
    if (p.get (P::sub_level) > 0.4f) t.add ("deep");
    if (p.get (P::reverb_mix) > 0.4f) t.add ("spacious");
    if (p.get (P::chorus_mix) > 0.3f) t.add ("lush");
    for (int i = 0; i < kNumModSlots && ! t.contains ("moving"); ++i)
    {
        const int src = (int) p.get (modSourceParam (i));
        if (src >= SrcLfo1 && src <= SrcLfo4 && (int) p.get (modDestParam (i)) != TargetOff) t.add ("moving");
    }
    while (t.size() > 4) t.remove (t.size() - 1);
    return t;
}

juce::String patchTips (const Patch& p)
{
    juce::StringArray lines;
    const float cutoff = p.get (P::filter_cutoff);
    lines.add ("Cutoff (" + hzText (cutoff) + "): the brightness. Lower it for darker and softer, raise it to open up. "
               + (p.get (P::filter_res) > 0.4f ? juce::String ("Resonance is up, so the cutoff point rings - back it off if it whistles.")
                                               : juce::String ("Add Resonance to make the cutoff point sing.")));
    const float att = p.get (P::aenv_attack), dec = p.get (P::aenv_decay), sus = p.get (P::aenv_sustain), rel = p.get (P::aenv_release);
    if (att > 0.3f)      lines.add ("Attack (" + juce::String (att, 2) + " s) fades each note in. Shorten it under 0.05 s for an instant, keyboard-like start.");
    else if (sus < 0.2f) lines.add ("Decay (" + juce::String (dec, 2) + " s) sets how fast each note dies away; Sustain is near zero so this is a plucky sound. Raise Sustain to hold notes.");
    else                 lines.add ("Attack is quick and Sustain holds (" + juce::String (sus, 2) + "). Raise Attack toward 0.5 s for a pad-like swell, or lengthen Release (" + juce::String (rel, 2) + " s) for a longer tail.");
    if (p.get (P::filter_env) > 0.5f)       lines.add ("Filt Env (+" + juce::String (p.get (P::filter_env), 1) + " oct) opens the filter at the start of each note; F Decay decides how fast it closes. More = a stronger 'wah' on every hit.");
    else if (p.get (P::filter_env) < -0.5f) lines.add ("Filt Env is negative: the filter starts closed and opens into the note. Shorten F Decay for a snappier swell.");
    else                                    lines.add ("Filt Env is near zero, so the filter stays put. Try +2 with a short F Decay for a plucky bite.");
    bool anyLfo = false;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int src = (int) p.get (modSourceParam (i)), dst = (int) p.get (modDestParam (i));
        if (src == SrcOff || dst == TargetOff) continue;
        const auto target = modTargetNames()[juce::jlimit (0, modTargetNames().size() - 1, dst)];
        if (src >= SrcLfo1 && src <= SrcLfo4)
        {
            anyLfo = true;
            const int k = src - SrcLfo1;
            lines.add ("LFO " + juce::String (k + 1) + " moves " + target + " at " + juce::String (p.get (lfoRateParam (k)), 2) + " Hz. Its Rate knob speeds the motion up; the depth slider in the LFO tab makes it bolder or subtler.");
        }
        else if (src == SrcVelocity) lines.add ("Velocity is wired to " + target + ": playing harder changes it. That is why the sound responds to touch.");
        else if (src == SrcModEnv)   lines.add ("The Mod Env drives " + target + " for the first moments of each note - the attack character. ME Decay sets how long.");
    }
    if (! anyLfo) lines.add ("Nothing is moving on its own yet. In MODULATORS press Assign, click a knob (Cutoff, A Morph or Pan are good first picks) and the LFO will swing it.");
    if (p.get (P::unison_voices) <= 1) lines.add ("Unison is off. 3 voices with Detune around 15 makes it wide and thick; Spread puts the copies left and right.");
    else lines.add ("Unison (" + juce::String ((int) p.get (P::unison_voices)) + " voices, detune " + juce::String ((int) p.get (P::unison_detune)) + ") is what makes it wide. Pull Detune down for a cleaner, more in-tune centre.");
    const float rev = p.get (P::reverb_mix), del = p.get (P::delay_mix);
    if (rev < 0.15f && del < 0.1f) lines.add ("It is dry. Reverb Mix around 0.3 puts it in a room; Delay Mix with a synced 1/8 adds rhythm.");
    else lines.add ("Space: Reverb Mix " + juce::String (rev, 2) + (del > 0.1f ? ", Delay Mix " + juce::String (del, 2) : juce::String()) + ". Reverb Size lengthens the tail, Delay FB adds more repeats.");
    return lines.joinIntoString ("\n\n");
}

juce::String describePatch (const Patch& p)
{
    juce::StringArray parts;

    // Sources
    juce::String src = oscText (p, P::oscA_wave, P::oscA_coarse, p.waves[0]);
    if (p.get (P::oscB_level) > 0.05f)
        src << " + " << oscText (p, P::oscB_wave, P::oscB_coarse, p.waves[1]);
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
    static const char* shortSource[] = { "", "LFO1", "LFO2", "LFO3", "LFO4", "FEnv", "MEnv", "Vel", "Key", "Wheel", "AT", "Rnd" };
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int src = juce::jlimit (0, kNumModSources - 1, (int) p.get (modSourceParam (i)));
        const int dst = juce::jlimit (0, modTargetNames().size() - 1, (int) p.get (modDestParam (i)));
        if (src == SrcOff || dst == TargetOff || std::abs (p.get (modAmountParam (i))) < 0.02f)
            continue;
        parts.add (juce::String (shortSource[src]) + " > " + modTargetNames()[dst]);
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
