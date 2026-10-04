#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace stacks
{

enum class ParamKind { Float, Int, Choice };

// One row of the parameter table. This is the single source of truth: the host
// parameters, the knob panel, the patch JSON format and (later) the prompt we
// hand to the AI are all generated from it.
struct ParamSpec
{
    const char* id;                   // stable identifier, also the JSON key
    const char* name;                 // unique, host-facing name
    const char* group;                // panel section
    ParamKind kind;
    float min, max, def;
    float skewCentre;                 // > 0: value shown at the middle of the knob (log-style)
    const char* unit;
    const juce::StringArray* choices; // Choice params only
    const char* aiHint;               // what the parameter does, in one line
};

const juce::StringArray& waveNames();
const juce::StringArray& filterTypeNames();
const juce::StringArray& lfoShapeNames();
const juce::StringArray& lfoDestNames();

enum LfoDest { DestOff = 0, DestPitch, DestFilter, DestMorphA, DestMorphB, DestFM, DestAmp, DestPan };

// X(id, name, group, kind, min, max, default, skewCentre, unit, choices, aiHint)
#define STACKS_PARAMS(X) \
 X(oscA_wave,      "A Wave",       "OSC A",      Choice,  0,     9,     2,     0,     "",   &waveNames(),       "oscillator A wavetable") \
 X(oscA_morph,     "A Morph",      "OSC A",      Float,   0,     1,     0.5,   0,     "",   nullptr,            "position inside wavetable A; changes its timbre") \
 X(oscA_coarse,    "A Coarse",     "OSC A",      Int,    -24,    24,    0,     0,     "st", nullptr,            "oscillator A transpose in semitones") \
 X(oscA_fine,      "A Fine",       "OSC A",      Float,  -100,   100,   0,     0,     "ct", nullptr,            "oscillator A detune in cents") \
 X(oscA_level,     "A Level",      "OSC A",      Float,   0,     1,     0.8,   0,     "",   nullptr,            "oscillator A volume") \
 X(oscB_wave,      "B Wave",       "OSC B",      Choice,  0,     9,     0,     0,     "",   &waveNames(),       "oscillator B wavetable") \
 X(oscB_morph,     "B Morph",      "OSC B",      Float,   0,     1,     0.5,   0,     "",   nullptr,            "position inside wavetable B") \
 X(oscB_coarse,    "B Coarse",     "OSC B",      Int,    -24,    24,    0,     0,     "st", nullptr,            "oscillator B transpose in semitones (also the FM ratio)") \
 X(oscB_fine,      "B Fine",       "OSC B",      Float,  -100,   100,   5,     0,     "ct", nullptr,            "oscillator B detune in cents") \
 X(oscB_level,     "B Level",      "OSC B",      Float,   0,     1,     0,     0,     "",   nullptr,            "oscillator B volume (can be 0 when B is only an FM modulator)") \
 X(fm_amount,      "FM B>A",       "OSC B",      Float,   0,     1,     0,     0,     "",   nullptr,            "how much B frequency-modulates A: 0 none, 0.1 warm, 0.3+ metallic") \
 X(sub_level,      "Sub",          "MIX",        Float,   0,     1,     0,     0,     "",   nullptr,            "sine sub-oscillator one octave below") \
 X(noise_level,    "Noise",        "MIX",        Float,   0,     1,     0,     0,     "",   nullptr,            "white noise level") \
 X(filter_type,    "Filter Type",  "FILTER",     Choice,  0,     5,     1,     0,     "",   &filterTypeNames(), "LP = low-pass, HP = high-pass, BP = band-pass; 12/24 dB per octave") \
 X(filter_cutoff,  "Cutoff",       "FILTER",     Float,   20,    20000, 8000,  632,   "Hz", nullptr,            "filter cutoff frequency in Hz") \
 X(filter_res,     "Resonance",    "FILTER",     Float,   0,     1,     0.1,   0,     "",   nullptr,            "filter resonance, self-oscillates near 1") \
 X(filter_drive,   "Drive",        "FILTER",     Float,   1,     10,    1,     0,     "",   nullptr,            "filter input saturation, 1 = clean") \
 X(filter_env,     "Filt Env",     "FILTER",     Float,  -5,     5,     0,     0,     "oct",nullptr,            "filter envelope depth in octaves (negative = inverted)") \
 X(filter_keytrack,"Key Track",    "FILTER",     Float,   0,     1,     0.3,   0,     "",   nullptr,            "how much cutoff follows the played pitch") \
 X(fenv_attack,    "F Attack",     "FILTER ENV", Float,   0.001, 10,    0.01,  0.2,   "s",  nullptr,            "filter envelope attack in seconds") \
 X(fenv_decay,     "F Decay",      "FILTER ENV", Float,   0.001, 10,    0.3,   0.3,   "s",  nullptr,            "filter envelope decay in seconds") \
 X(fenv_sustain,   "F Sustain",    "FILTER ENV", Float,   0,     1,     0.5,   0,     "",   nullptr,            "filter envelope sustain level") \
 X(fenv_release,   "F Release",    "FILTER ENV", Float,   0.001, 10,    0.3,   0.3,   "s",  nullptr,            "filter envelope release in seconds") \
 X(aenv_attack,    "Attack",       "AMP ENV",    Float,   0.001, 10,    0.005, 0.2,   "s",  nullptr,            "amplitude attack in seconds") \
 X(aenv_decay,     "Decay",        "AMP ENV",    Float,   0.001, 10,    0.2,   0.3,   "s",  nullptr,            "amplitude decay in seconds") \
 X(aenv_sustain,   "Sustain",      "AMP ENV",    Float,   0,     1,     0.8,   0,     "",   nullptr,            "amplitude sustain level") \
 X(aenv_release,   "Release",      "AMP ENV",    Float,   0.001, 10,    0.3,   0.3,   "s",  nullptr,            "amplitude release in seconds") \
 X(lfo1_shape,     "LFO1 Shape",   "LFO 1",      Choice,  0,     4,     0,     0,     "",   &lfoShapeNames(),   "LFO 1 waveform") \
 X(lfo1_rate,      "LFO1 Rate",    "LFO 1",      Float,   0.01,  30,    2,     0.55,  "Hz", nullptr,            "LFO 1 speed in Hz") \
 X(lfo1_amount,    "LFO1 Amt",     "LFO 1",      Float,   0,     1,     0,     0,     "",   nullptr,            "LFO 1 depth") \
 X(lfo1_dest,      "LFO1 Dest",    "LFO 1",      Choice,  0,     7,     0,     0,     "",   &lfoDestNames(),    "what LFO 1 modulates") \
 X(lfo2_shape,     "LFO2 Shape",   "LFO 2",      Choice,  0,     4,     1,     0,     "",   &lfoShapeNames(),   "LFO 2 waveform") \
 X(lfo2_rate,      "LFO2 Rate",    "LFO 2",      Float,   0.01,  30,    0.3,   0.55,  "Hz", nullptr,            "LFO 2 speed in Hz") \
 X(lfo2_amount,    "LFO2 Amt",     "LFO 2",      Float,   0,     1,     0,     0,     "",   nullptr,            "LFO 2 depth") \
 X(lfo2_dest,      "LFO2 Dest",    "LFO 2",      Choice,  0,     7,     0,     0,     "",   &lfoDestNames(),    "what LFO 2 modulates") \
 X(unison_voices,  "Unison",       "VOICE",      Int,     1,     4,     1,     0,     "",   nullptr,            "stacked detuned copies per note, 1-4") \
 X(unison_detune,  "Detune",       "VOICE",      Float,   0,     50,    10,    0,     "ct", nullptr,            "unison detune in cents") \
 X(unison_spread,  "Spread",       "VOICE",      Float,   0,     1,     0.5,   0,     "",   nullptr,            "unison stereo width") \
 X(glide,          "Glide",        "VOICE",      Float,   0,     2,     0,     0,     "s",  nullptr,            "portamento time in seconds") \
 X(chorus_mix,     "Chorus Mix",   "CHORUS",     Float,   0,     1,     0,     0,     "",   nullptr,            "chorus amount") \
 X(chorus_rate,    "Chorus Rate",  "CHORUS",     Float,   0.05,  5,     0.8,   0.5,   "Hz", nullptr,            "chorus speed in Hz") \
 X(chorus_depth,   "Chorus Depth", "CHORUS",     Float,   0,     1,     0.3,   0,     "",   nullptr,            "chorus depth") \
 X(delay_mix,      "Delay Mix",    "DELAY",      Float,   0,     1,     0,     0,     "",   nullptr,            "delay amount") \
 X(delay_time,     "Delay Time",   "DELAY",      Float,   0.02,  1.5,   0.375, 0.25,  "s",  nullptr,            "delay time in seconds") \
 X(delay_feedback, "Delay FB",     "DELAY",      Float,   0,     0.95,  0.4,   0,     "",   nullptr,            "delay feedback, number of repeats") \
 X(reverb_mix,     "Reverb Mix",   "REVERB",     Float,   0,     1,     0.15,  0,     "",   nullptr,            "reverb amount") \
 X(reverb_size,    "Reverb Size",  "REVERB",     Float,   0,     1,     0.5,   0,     "",   nullptr,            "reverb room size") \
 X(reverb_damp,    "Reverb Damp",  "REVERB",     Float,   0,     1,     0.5,   0,     "",   nullptr,            "reverb high-frequency damping") \
 X(master_gain,    "Master",       "MASTER",     Float,  -24,    6,    -6,     0,     "dB", nullptr,            "output level in dB")

enum class P : int
{
#define STACKS_ENUM_ENTRY(id, ...) id,
    STACKS_PARAMS(STACKS_ENUM_ENTRY)
#undef STACKS_ENUM_ENTRY
    COUNT
};

constexpr int kNumParams = static_cast<int>(P::COUNT);

const std::vector<ParamSpec>& paramSpecs();
inline const ParamSpec& spec(P p) { return paramSpecs()[static_cast<size_t>(p)]; }
inline const char* paramId(P p) { return spec(p).id; }

// -1 when no parameter has that id.
int paramIndexForId(const juce::String& id);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Plain copy of every parameter's current value, indexed by P. Refreshed once
// per audio block and read by the voices.
struct SynthParams
{
    float v[kNumParams] {};
    float get(P p) const noexcept { return v[static_cast<int>(p)]; }
    int geti(P p) const noexcept { return static_cast<int>(v[static_cast<int>(p)] + 0.5f); }
};

} // namespace stacks
