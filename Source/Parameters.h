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
const juce::StringArray& modSourceNames();
const juce::StringArray& modDestNames();
const juce::StringArray& chorusModeNames();
const juce::StringArray& delayModeNames();
const juce::StringArray& delaySyncNames();
const juce::StringArray& reverbTypeNames();

// Advanced parameters live behind a section's "..." button rather than on the panel.
bool isAdvancedParam (const char* id);

// Modulation matrix: kNumModSlots slots of source -> destination x amount.
enum ModSource { SrcOff = 0, SrcLfo1, SrcLfo2, SrcFilterEnv, SrcModEnv, SrcVelocity, SrcKey, SrcModWheel, SrcAftertouch, SrcRandom, kNumModSources };
enum ModDest   { DestOff = 0, DestPitch, DestPitchB, DestFilter, DestResonance, DestMorphA, DestMorphB, DestFM, DestAmp, DestPan,
                 DestLfo1Rate, DestLfo2Rate, DestBLevel, DestNoise, kNumModDests };
constexpr int kNumModSlots = 6;

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
 X(sub_level,      "Sub",          "MIX",        Float,   0,     1,     0,     0,     "",   nullptr,            "sine sub-oscillator one octave below") \
 X(noise_level,    "Noise",        "MIX",        Float,   0,     1,     0,     0,     "",   nullptr,            "white noise level") \
 X(fm_amount,      "FM B>A",       "MIX",        Float,   0,     1,     0,     0,     "",   nullptr,            "how much B frequency-modulates A: 0 none, 0.1 warm, 0.3+ metallic") \
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
 X(menv_attack,    "ME Attack",    "MOD ENV",    Float,   0.001, 10,    0.005, 0.2,   "s",  nullptr,            "mod envelope attack in seconds (a free ADSR, only used as a matrix source)") \
 X(menv_decay,     "ME Decay",     "MOD ENV",    Float,   0.001, 10,    0.3,   0.3,   "s",  nullptr,            "mod envelope decay in seconds") \
 X(menv_sustain,   "ME Sustain",   "MOD ENV",    Float,   0,     1,     0,     0,     "",   nullptr,            "mod envelope sustain level") \
 X(menv_release,   "ME Release",   "MOD ENV",    Float,   0.001, 10,    0.3,   0.3,   "s",  nullptr,            "mod envelope release in seconds") \
 X(lfo1_shape,     "LFO1 Shape",   "LFO 1",      Choice,  0,     4,     0,     0,     "",   &lfoShapeNames(),   "LFO 1 waveform (route it with a mod slot)") \
 X(lfo1_rate,      "LFO1 Rate",    "LFO 1",      Float,   0.01,  30,    2,     0.55,  "Hz", nullptr,            "LFO 1 speed in Hz") \
 X(lfo2_shape,     "LFO2 Shape",   "LFO 2",      Choice,  0,     4,     1,     0,     "",   &lfoShapeNames(),   "LFO 2 waveform (route it with a mod slot)") \
 X(lfo2_rate,      "LFO2 Rate",    "LFO 2",      Float,   0.01,  30,    0.3,   0.55,  "Hz", nullptr,            "LFO 2 speed in Hz") \
 X(mod1_source,     "Mod1 Src",     "MOD MATRIX", Choice,  0,     9,     0,     0,     "",   &modSourceNames(),  "modulation slot 1 source") \
 X(mod1_dest,       "Mod1 Dest",    "MOD MATRIX", Choice,  0,     13,    0,     0,     "",   &modDestNames(),    "modulation slot 1 destination") \
 X(mod1_amount,     "Mod1 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "modulation slot 1 depth -1..1 (Pitch: x12 semitones, Filter: x5 octaves, others: x1)") \
 X(mod2_source,     "Mod2 Src",     "MOD MATRIX", Choice,  0,     9,     0,     0,     "",   &modSourceNames(),  "modulation slot 2 source") \
 X(mod2_dest,       "Mod2 Dest",    "MOD MATRIX", Choice,  0,     13,    0,     0,     "",   &modDestNames(),    "modulation slot 2 destination") \
 X(mod2_amount,     "Mod2 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "modulation slot 2 depth -1..1 (Pitch: x12 semitones, Filter: x5 octaves, others: x1)") \
 X(mod3_source,     "Mod3 Src",     "MOD MATRIX", Choice,  0,     9,     0,     0,     "",   &modSourceNames(),  "modulation slot 3 source") \
 X(mod3_dest,       "Mod3 Dest",    "MOD MATRIX", Choice,  0,     13,    0,     0,     "",   &modDestNames(),    "modulation slot 3 destination") \
 X(mod3_amount,     "Mod3 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "modulation slot 3 depth -1..1 (Pitch: x12 semitones, Filter: x5 octaves, others: x1)") \
 X(mod4_source,     "Mod4 Src",     "MOD MATRIX", Choice,  0,     9,     0,     0,     "",   &modSourceNames(),  "modulation slot 4 source") \
 X(mod4_dest,       "Mod4 Dest",    "MOD MATRIX", Choice,  0,     13,    0,     0,     "",   &modDestNames(),    "modulation slot 4 destination") \
 X(mod4_amount,     "Mod4 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "modulation slot 4 depth -1..1 (Pitch: x12 semitones, Filter: x5 octaves, others: x1)") \
 X(mod5_source,     "Mod5 Src",     "MOD MATRIX", Choice,  0,     9,     0,     0,     "",   &modSourceNames(),  "modulation slot 5 source") \
 X(mod5_dest,       "Mod5 Dest",    "MOD MATRIX", Choice,  0,     13,    0,     0,     "",   &modDestNames(),    "modulation slot 5 destination") \
 X(mod5_amount,     "Mod5 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "modulation slot 5 depth -1..1 (Pitch: x12 semitones, Filter: x5 octaves, others: x1)") \
 X(mod6_source,     "Mod6 Src",     "MOD MATRIX", Choice,  0,     9,     0,     0,     "",   &modSourceNames(),  "modulation slot 6 source") \
 X(mod6_dest,       "Mod6 Dest",    "MOD MATRIX", Choice,  0,     13,    0,     0,     "",   &modDestNames(),    "modulation slot 6 destination") \
 X(mod6_amount,     "Mod6 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "modulation slot 6 depth -1..1 (Pitch: x12 semitones, Filter: x5 octaves, others: x1)") \
 X(unison_voices,  "Unison",       "VOICE",      Int,     1,     4,     1,     0,     "",   nullptr,            "stacked detuned copies per note, 1-4") \
 X(unison_detune,  "Detune",       "VOICE",      Float,   0,     50,    10,    0,     "ct", nullptr,            "unison detune in cents") \
 X(unison_spread,  "Spread",       "VOICE",      Float,   0,     1,     0.5,   0,     "",   nullptr,            "unison stereo width") \
 X(glide,          "Glide",        "VOICE",      Float,   0,     2,     0,     0,     "s",  nullptr,            "portamento time in seconds") \
 X(chorus_mode,    "Chorus Mode",  "CHORUS",     Choice,  0,     3,     0,     0,     "",   &chorusModeNames(), "Chorus = classic, Ensemble = lush string-machine, Flanger = short metallic sweep, Dimension = wide and subtle") \
 X(chorus_rate,    "Chorus Rate",  "CHORUS",     Float,   0.05,  5,     0.8,   0.5,   "Hz", nullptr,            "chorus speed in Hz") \
 X(chorus_depth,   "Chorus Depth", "CHORUS",     Float,   0,     1,     0.3,   0,     "",   nullptr,            "chorus depth") \
 X(chorus_mix,     "Chorus Mix",   "CHORUS",     Float,   0,     1,     0,     0,     "",   nullptr,            "chorus amount") \
 X(chorus_voices,  "Voices",       "CHORUS",     Int,     1,     4,     2,     0,     "",   nullptr,            "number of chorus voices (advanced)") \
 X(chorus_feedback,"Chorus FB",    "CHORUS",     Float,  -0.9,   0.9,   0,     0,     "",   nullptr,            "chorus/flanger feedback, negative inverts (advanced)") \
 X(chorus_spread,  "Spread",       "CHORUS",     Float,   0,     1,     0.7,   0,     "",   nullptr,            "stereo width of the chorus (advanced)") \
 X(chorus_tone,    "Tone",         "CHORUS",     Float,   1000,  20000, 12000, 4000,  "Hz", nullptr,            "low-pass on the chorus signal (advanced)") \
 X(delay_mode,     "Delay Mode",   "DELAY",      Choice,  0,     2,     0,     0,     "",   &delayModeNames(),  "Stereo = two taps, Ping-Pong = bounces left/right, Tape = dark with wow and saturation") \
 X(delay_sync,     "Sync",         "DELAY",      Choice,  0,     7,     0,     0,     "",   &delaySyncNames(),  "Free uses Delay Time; otherwise a note value locked to the host tempo") \
 X(delay_time,     "Delay Time",   "DELAY",      Float,   0.02,  1.5,   0.375, 0.25,  "s",  nullptr,            "delay time in seconds when Sync is Free") \
 X(delay_feedback, "Delay FB",     "DELAY",      Float,   0,     0.95,  0.4,   0,     "",   nullptr,            "delay feedback, number of repeats") \
 X(delay_mix,      "Delay Mix",    "DELAY",      Float,   0,     1,     0,     0,     "",   nullptr,            "delay amount") \
 X(delay_tone,     "Delay Tone",   "DELAY",      Float,   500,   20000, 6000,  3000,  "Hz", nullptr,            "low-pass in the feedback path, repeats get darker (advanced)") \
 X(delay_hpf,      "Delay HPF",    "DELAY",      Float,   20,    2000,  120,   200,   "Hz", nullptr,            "high-pass in the feedback path, keeps repeats out of the bass (advanced)") \
 X(delay_wow,      "Wow",          "DELAY",      Float,   0,     1,     0.1,   0,     "",   nullptr,            "tape-style pitch wobble of the repeats (advanced)") \
 X(delay_width,    "Delay Width",  "DELAY",      Float,   0,     1,     0.3,   0,     "",   nullptr,            "offsets the right channel's time for stereo width (advanced)") \
 X(reverb_type,    "Reverb Type",  "REVERB",     Choice,  0,     3,     1,     0,     "",   &reverbTypeNames(), "Room = small, Plate = bright and dense, Hall = long, Shimmer = hall with an octave-up halo") \
 X(reverb_size,    "Reverb Size",  "REVERB",     Float,   0,     1,     0.5,   0,     "",   nullptr,            "decay length") \
 X(reverb_damp,    "Reverb Damp",  "REVERB",     Float,   0,     1,     0.5,   0,     "",   nullptr,            "high-frequency damping of the tail") \
 X(reverb_mix,     "Reverb Mix",   "REVERB",     Float,   0,     1,     0.15,  0,     "",   nullptr,            "reverb amount") \
 X(reverb_predelay,"Pre-delay",    "REVERB",     Float,   0,     200,   10,    0,     "ms", nullptr,            "gap before the reverb starts, in ms (advanced)") \
 X(reverb_lowcut,  "Low Cut",      "REVERB",     Float,   20,    1000,  100,   150,   "Hz", nullptr,            "high-pass on the reverb input, keeps the tail out of the bass (advanced)") \
 X(reverb_highcut, "High Cut",     "REVERB",     Float,   1000,  20000, 10000, 4500,  "Hz", nullptr,            "low-pass on the reverb output (advanced)") \
 X(reverb_mod,     "Reverb Mod",   "REVERB",     Float,   0,     1,     0.3,   0,     "",   nullptr,            "modulation inside the tail, smoother and more chorus-like (advanced)") \
 X(reverb_shimmer, "Shimmer",      "REVERB",     Float,   0,     1,     0,     0,     "",   nullptr,            "octave-up pitch-shifted feedback for a halo; the Shimmer type turns it up (advanced)") \
 X(reverb_width,   "Reverb Width", "REVERB",     Float,   0,     1,     1,     0,     "",   nullptr,            "stereo width of the tail (advanced)") \
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

// Parameter of modulation slot `slot` (0-based).
P modSourceParam (int slot);
P modDestParam (int slot);
P modAmountParam (int slot);
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
