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
    const juce::StringArray& (*choices)(); // Choice params only
    const char* aiHint;                    // what the parameter does, in one line
};

const juce::StringArray& waveNames();
const juce::StringArray& filterTypeNames();
const juce::StringArray& lfoShapeNames();
const juce::StringArray& lfoSyncNames();
const juce::StringArray& lfoModeNames();
const juce::StringArray& modSourceNames();
const juce::StringArray& modTargetNames();
const juce::StringArray& chorusModeNames();
const juce::StringArray& delayModeNames();
const juce::StringArray& delaySyncNames();
const juce::StringArray& reverbTypeNames();

// Advanced parameters live behind a section's "..." button rather than on the panel.
bool isAdvancedParam (const char* id);

// Modulation: kNumModSlots connections of source -> target x amount. Targets are
// a few virtual ones (pitch, amp, pan) followed by every float parameter.
enum ModSource { SrcOff = 0, SrcLfo1, SrcLfo2, SrcLfo3, SrcLfo4, SrcFilterEnv, SrcModEnv, SrcVelocity, SrcKey, SrcModWheel, SrcAftertouch, SrcRandom, kNumModSources };
enum ModTarget { TargetOff = 0, TargetPitch, TargetPitchB, TargetAmp, TargetPan, kNumVirtualTargets };
constexpr int kNumModSlots = 12;
constexpr int kNumLfos = 4;
enum LfoShape  { ShapeSine = 0, ShapeTriangle, ShapeSaw, ShapeRamp, ShapeSquare, ShapeRandom, ShapeCustom };

inline bool isBipolarSource (int src) noexcept
{
    return src == SrcLfo1 || src == SrcLfo2 || src == SrcLfo3 || src == SrcLfo4 || src == SrcKey || src == SrcRandom;
}

// X(id, name, group, kind, min, max, default, skewCentre, unit, choices, aiHint)
#define STACKS_PARAMS(X) \
 X(oscA_wave,      "A Wave",       "OSC A",      Choice,  0,     9,     2,     0,     "",   waveNames,       "oscillator A wavetable (User 1-4 are the player's imported tables; prefer the built-ins)") \
 X(oscA_morph,     "A Morph",      "OSC A",      Float,   0,     1,     0.5,   0,     "",   nullptr,            "position inside wavetable A; changes its timbre") \
 X(oscA_coarse,    "A Coarse",     "OSC A",      Int,    -24,    24,    0,     0,     "st", nullptr,            "oscillator A transpose in semitones") \
 X(oscA_fine,      "A Fine",       "OSC A",      Float,  -100,   100,   0,     0,     "ct", nullptr,            "oscillator A detune in cents") \
 X(oscA_level,     "A Level",      "OSC A",      Float,   0,     1,     0.8,   0,     "",   nullptr,            "oscillator A volume") \
 X(oscB_wave,      "B Wave",       "OSC B",      Choice,  0,     9,     0,     0,     "",   waveNames,       "oscillator B wavetable (User 1-4 are the player's imported tables; prefer the built-ins)") \
 X(oscB_morph,     "B Morph",      "OSC B",      Float,   0,     1,     0.5,   0,     "",   nullptr,            "position inside wavetable B") \
 X(oscB_coarse,    "B Coarse",     "OSC B",      Int,    -24,    24,    0,     0,     "st", nullptr,            "oscillator B transpose in semitones (also the FM ratio)") \
 X(oscB_fine,      "B Fine",       "OSC B",      Float,  -100,   100,   5,     0,     "ct", nullptr,            "oscillator B detune in cents") \
 X(oscB_level,     "B Level",      "OSC B",      Float,   0,     1,     0,     0,     "",   nullptr,            "oscillator B volume (can be 0 when B is only an FM modulator)") \
 X(sub_level,      "Sub",          "MIX",        Float,   0,     1,     0,     0,     "",   nullptr,            "sine sub-oscillator one octave below") \
 X(noise_level,    "Noise",        "MIX",        Float,   0,     1,     0,     0,     "",   nullptr,            "white noise level") \
 X(fm_amount,      "FM B>A",       "MIX",        Float,   0,     1,     0,     0,     "",   nullptr,            "how much B frequency-modulates A: 0 none, 0.1 warm, 0.3+ metallic") \
 X(filter_type,    "Filter Type",  "FILTER",     Choice,  0,     5,     1,     0,     "",   filterTypeNames, "LP = low-pass, HP = high-pass, BP = band-pass; 12/24 dB per octave") \
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
 X(lfo1_shape,     "LFO1 Shape",   "LFO 1",      Choice,  0,     6,     0,     0,     "",   lfoShapeNames,      "LFO 1 waveform; Custom uses the shape drawn in its editor") \
 X(lfo1_rate,      "LFO1 Rate",    "LFO 1",      Float,   0.01,  30,    2,     0.55,  "Hz", nullptr,            "LFO 1 speed in Hz when its Sync is Free") \
 X(lfo1_sync,      "LFO1 Sync",    "LFO 1",      Choice,  0,     8,     0,     0,     "",   lfoSyncNames,       "Free uses Rate; otherwise LFO 1 cycles over a note length locked to the tempo") \
 X(lfo1_phase,     "LFO1 Phase",   "LFO 1",      Float,   0,     1,     0,     0,     "",   nullptr,            "start phase of LFO 1, 0-1") \
 X(lfo1_mode,      "LFO1 Mode",    "LFO 1",      Choice,  0,     1,     0,     0,     "",   lfoModeNames,       "Free: LFO 1 runs continuously and is shared by all notes; Note: restarts on every key") \
 X(lfo2_shape,     "LFO2 Shape",   "LFO 2",      Choice,  0,     6,     1,     0,     "",   lfoShapeNames,      "LFO 2 waveform; Custom uses the shape drawn in its editor") \
 X(lfo2_rate,      "LFO2 Rate",    "LFO 2",      Float,   0.01,  30,    0.3,     0.55,  "Hz", nullptr,            "LFO 2 speed in Hz when its Sync is Free") \
 X(lfo2_sync,      "LFO2 Sync",    "LFO 2",      Choice,  0,     8,     0,     0,     "",   lfoSyncNames,       "Free uses Rate; otherwise LFO 2 cycles over a note length locked to the tempo") \
 X(lfo2_phase,     "LFO2 Phase",   "LFO 2",      Float,   0,     1,     0,     0,     "",   nullptr,            "start phase of LFO 2, 0-1") \
 X(lfo2_mode,      "LFO2 Mode",    "LFO 2",      Choice,  0,     1,     0,     0,     "",   lfoModeNames,       "Free: LFO 2 runs continuously and is shared by all notes; Note: restarts on every key") \
 X(lfo3_shape,     "LFO3 Shape",   "LFO 3",      Choice,  0,     6,     0,     0,     "",   lfoShapeNames,      "LFO 3 waveform; Custom uses the shape drawn in its editor") \
 X(lfo3_rate,      "LFO3 Rate",    "LFO 3",      Float,   0.01,  30,    0.1,     0.55,  "Hz", nullptr,            "LFO 3 speed in Hz when its Sync is Free") \
 X(lfo3_sync,      "LFO3 Sync",    "LFO 3",      Choice,  0,     8,     0,     0,     "",   lfoSyncNames,       "Free uses Rate; otherwise LFO 3 cycles over a note length locked to the tempo") \
 X(lfo3_phase,     "LFO3 Phase",   "LFO 3",      Float,   0,     1,     0,     0,     "",   nullptr,            "start phase of LFO 3, 0-1") \
 X(lfo3_mode,      "LFO3 Mode",    "LFO 3",      Choice,  0,     1,     0,     0,     "",   lfoModeNames,       "Free: LFO 3 runs continuously and is shared by all notes; Note: restarts on every key") \
 X(lfo4_shape,     "LFO4 Shape",   "LFO 4",      Choice,  0,     6,     0,     0,     "",   lfoShapeNames,      "LFO 4 waveform; Custom uses the shape drawn in its editor") \
 X(lfo4_rate,      "LFO4 Rate",    "LFO 4",      Float,   0.01,  30,    5,     0.55,  "Hz", nullptr,            "LFO 4 speed in Hz when its Sync is Free") \
 X(lfo4_sync,      "LFO4 Sync",    "LFO 4",      Choice,  0,     8,     0,     0,     "",   lfoSyncNames,       "Free uses Rate; otherwise LFO 4 cycles over a note length locked to the tempo") \
 X(lfo4_phase,     "LFO4 Phase",   "LFO 4",      Float,   0,     1,     0,     0,     "",   nullptr,            "start phase of LFO 4, 0-1") \
 X(lfo4_mode,      "LFO4 Mode",    "LFO 4",      Choice,  0,     1,     0,     0,     "",   lfoModeNames,       "Free: LFO 4 runs continuously and is shared by all notes; Note: restarts on every key") \
 X(mod1_source,     "Mod1 Src",     "MOD MATRIX", Choice,  0,     11,    0,     0,     "",   modSourceNames,     "connection 1 source") \
 X(mod1_dest,       "Mod1 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 1 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod1_amount,     "Mod1 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 1 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod2_source,     "Mod2 Src",     "MOD MATRIX", Choice,  0,     11,    0,     0,     "",   modSourceNames,     "connection 2 source") \
 X(mod2_dest,       "Mod2 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 2 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod2_amount,     "Mod2 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 2 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod3_source,     "Mod3 Src",     "MOD MATRIX", Choice,  0,     11,    0,     0,     "",   modSourceNames,     "connection 3 source") \
 X(mod3_dest,       "Mod3 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 3 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod3_amount,     "Mod3 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 3 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod4_source,     "Mod4 Src",     "MOD MATRIX", Choice,  0,     11,    0,     0,     "",   modSourceNames,     "connection 4 source") \
 X(mod4_dest,       "Mod4 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 4 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod4_amount,     "Mod4 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 4 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod5_source,     "Mod5 Src",     "MOD MATRIX", Choice,  0,     11,    0,     0,     "",   modSourceNames,     "connection 5 source") \
 X(mod5_dest,       "Mod5 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 5 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod5_amount,     "Mod5 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 5 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod6_source,     "Mod6 Src",     "MOD MATRIX", Choice,  0,     11,    0,     0,     "",   modSourceNames,     "connection 6 source") \
 X(mod6_dest,       "Mod6 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 6 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod6_amount,     "Mod6 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 6 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod7_source,     "Mod7 Src",     "MOD MATRIX", Choice,  0,     11,    0,     0,     "",   modSourceNames,     "connection 7 source") \
 X(mod7_dest,       "Mod7 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 7 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod7_amount,     "Mod7 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 7 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod8_source,     "Mod8 Src",     "MOD MATRIX", Choice,  0,     11,    0,     0,     "",   modSourceNames,     "connection 8 source") \
 X(mod8_dest,       "Mod8 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 8 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod8_amount,     "Mod8 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 8 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod9_source,     "Mod9 Src",     "MOD MATRIX", Choice,  0,     11,    0,     0,     "",   modSourceNames,     "connection 9 source") \
 X(mod9_dest,       "Mod9 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 9 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod9_amount,     "Mod9 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 9 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod10_source,     "Mod10 Src",     "MOD MATRIX", Choice,  0,     11,    0,     0,     "",   modSourceNames,     "connection 10 source") \
 X(mod10_dest,       "Mod10 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 10 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod10_amount,     "Mod10 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 10 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod11_source,     "Mod11 Src",     "MOD MATRIX", Choice,  0,     11,    0,     0,     "",   modSourceNames,     "connection 11 source") \
 X(mod11_dest,       "Mod11 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 11 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod11_amount,     "Mod11 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 11 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod12_source,     "Mod12 Src",     "MOD MATRIX", Choice,  0,     11,    0,     0,     "",   modSourceNames,     "connection 12 source") \
 X(mod12_dest,       "Mod12 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 12 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod12_amount,     "Mod12 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 12 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(unison_voices,  "Unison",       "VOICE",      Int,     1,     4,     1,     0,     "",   nullptr,            "stacked detuned copies per note, 1-4") \
 X(unison_detune,  "Detune",       "VOICE",      Float,   0,     50,    10,    0,     "ct", nullptr,            "unison detune in cents") \
 X(unison_spread,  "Spread",       "VOICE",      Float,   0,     1,     0.5,   0,     "",   nullptr,            "unison stereo width") \
 X(glide,          "Glide",        "VOICE",      Float,   0,     2,     0,     0,     "s",  nullptr,            "portamento time in seconds") \
 X(chorus_mode,    "Chorus Mode",  "CHORUS",     Choice,  0,     3,     0,     0,     "",   chorusModeNames, "Chorus = classic, Ensemble = lush string-machine, Flanger = short metallic sweep, Dimension = wide and subtle") \
 X(chorus_rate,    "Chorus Rate",  "CHORUS",     Float,   0.05,  5,     0.8,   0.5,   "Hz", nullptr,            "chorus speed in Hz") \
 X(chorus_depth,   "Chorus Depth", "CHORUS",     Float,   0,     1,     0.3,   0,     "",   nullptr,            "chorus depth") \
 X(chorus_mix,     "Chorus Mix",   "CHORUS",     Float,   0,     1,     0,     0,     "",   nullptr,            "chorus amount") \
 X(chorus_voices,  "Voices",       "CHORUS",     Int,     1,     4,     2,     0,     "",   nullptr,            "number of chorus voices (advanced)") \
 X(chorus_feedback,"Chorus FB",    "CHORUS",     Float,  -0.9,   0.9,   0,     0,     "",   nullptr,            "chorus/flanger feedback, negative inverts (advanced)") \
 X(chorus_spread,  "Spread",       "CHORUS",     Float,   0,     1,     0.7,   0,     "",   nullptr,            "stereo width of the chorus (advanced)") \
 X(chorus_tone,    "Tone",         "CHORUS",     Float,   1000,  20000, 12000, 4000,  "Hz", nullptr,            "low-pass on the chorus signal (advanced)") \
 X(delay_mode,     "Delay Mode",   "DELAY",      Choice,  0,     2,     0,     0,     "",   delayModeNames,  "Stereo = two taps, Ping-Pong = bounces left/right, Tape = dark with wow and saturation") \
 X(delay_sync,     "Sync",         "DELAY",      Choice,  0,     7,     0,     0,     "",   delaySyncNames,  "Free uses Delay Time; otherwise a note value locked to the host tempo") \
 X(delay_time,     "Delay Time",   "DELAY",      Float,   0.02,  1.5,   0.375, 0.25,  "s",  nullptr,            "delay time in seconds when Sync is Free") \
 X(delay_feedback, "Delay FB",     "DELAY",      Float,   0,     0.95,  0.4,   0,     "",   nullptr,            "delay feedback, number of repeats") \
 X(delay_mix,      "Delay Mix",    "DELAY",      Float,   0,     1,     0,     0,     "",   nullptr,            "delay amount") \
 X(delay_tone,     "Delay Tone",   "DELAY",      Float,   500,   20000, 6000,  3000,  "Hz", nullptr,            "low-pass in the feedback path, repeats get darker (advanced)") \
 X(delay_hpf,      "Delay HPF",    "DELAY",      Float,   20,    2000,  120,   200,   "Hz", nullptr,            "high-pass in the feedback path, keeps repeats out of the bass (advanced)") \
 X(delay_wow,      "Wow",          "DELAY",      Float,   0,     1,     0.1,   0,     "",   nullptr,            "tape-style pitch wobble of the repeats (advanced)") \
 X(delay_width,    "Delay Width",  "DELAY",      Float,   0,     1,     0.3,   0,     "",   nullptr,            "offsets the right channel's time for stereo width (advanced)") \
 X(reverb_type,    "Reverb Type",  "REVERB",     Choice,  0,     3,     1,     0,     "",   reverbTypeNames, "Room = small, Plate = bright and dense, Hall = long, Shimmer = hall with an octave-up halo") \
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

// LFO k (0-based) parameters.
P lfoShapeParam (int k);
P lfoRateParam (int k);
P lfoSyncParam (int k);
P lfoPhaseParam (int k);
P lfoModeParam (int k);

// Target <-> parameter mapping. Virtual targets return -1 / have no parameter.
int modTargetParamIndex (int target);
int modTargetForParam (int paramIndex);     // -1 when the parameter can't be modulated
bool isModulatableParam (int paramIndex);

// The knob range of every parameter (skewed where the table says so).
const juce::NormalisableRange<float>& paramRange (int paramIndex);

// LFO cycle length in beats for a sync setting (0 = free).
float lfoSyncBeats (int sync);
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
