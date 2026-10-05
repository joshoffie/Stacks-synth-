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
const juce::StringArray& filterRoutingNames();   // second filter: Off, Series, Parallel, Split
const juce::StringArray& distModeNames();
const juce::StringArray& warpNames();     // oscillator warps: Off, Sync, Bend, PWM, Mirror, Fold, Quantize
const juce::StringArray& arpModeNames();
const juce::StringArray& arpRateNames();
const juce::StringArray& macroNames();   // the six macro knobs, in order
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
enum ModSource { SrcOff = 0, SrcLfo1, SrcLfo2, SrcLfo3, SrcLfo4, SrcFilterEnv, SrcModEnv, SrcVelocity, SrcKey, SrcModWheel, SrcAftertouch, SrcRandom,
                 SrcMacro1, SrcMacro2, SrcMacro3, SrcMacro4, SrcMacro5, SrcMacro6,
                 SrcSlide,   // MPE slide (CC74), per note; appended last so saved sources keep their numbers
                 kNumModSources };
constexpr int kNumMacros = 6;   // Brightness, Movement, Grit, Space, Width, Length: big knobs any patch can be played with
inline bool isMacroSource (int src) noexcept { return src >= SrcMacro1 && src < SrcMacro1 + kNumMacros; }
enum ModTarget { TargetOff = 0, TargetPitch, TargetPitchB, TargetAmp, TargetPan, kNumVirtualTargets };
constexpr int kNumModSlots = 20;
constexpr int kNumLfos = 4;
constexpr int kNumOscs = 3;   // wavetable oscillators A, B, C
enum LfoShape  { ShapeSine = 0, ShapeTriangle, ShapeSaw, ShapeRamp, ShapeSquare, ShapeRandom, ShapeCustom };

inline bool isBipolarSource (int src) noexcept
{
    return src == SrcLfo1 || src == SrcLfo2 || src == SrcLfo3 || src == SrcLfo4 || src == SrcKey || src == SrcRandom;
}

// X(id, name, group, kind, min, max, default, skewCentre, unit, choices, aiHint)
#define STACKS_PARAMS(X) \
 X(oscA_wave,      "A Wave",       "OSC A",      Choice,  0,     9,     2,     0,     "",   waveNames,       "oscillator A wavetable; Custom = the table designed in waveA (User 1-4 are the player's imported files: don't pick them)") \
 X(oscA_morph,     "A Morph",      "OSC A",      Float,   0,     1,     0.5,   0,     "",   nullptr,            "position inside wavetable A; changes its timbre") \
 X(oscA_coarse,    "A Coarse",     "OSC A",      Int,    -24,    24,    0,     0,     "st", nullptr,            "oscillator A transpose in semitones") \
 X(oscA_fine,      "A Fine",       "OSC A",      Float,  -100,   100,   0,     0,     "ct", nullptr,            "oscillator A detune in cents") \
 X(oscA_level,     "A Level",      "OSC A",      Float,   0,     1,     0.8,   0,     "",   nullptr,            "oscillator A volume") \
 X(oscA_warp,      "A Warp",       "OSC A",      Choice,  0,     6,     0,     0,     "",   warpNames,          "warp for table A: Off; Sync = hard-sync buzz; Bend = phase distortion; PWM = pulse width (sweep A Warp Amt with an LFO); Mirror = forward then backward; Fold = wavefolder grit; Quantize = digital crunch") \
 X(oscA_warp_amt,  "A Warp Amt",   "OSC A",      Float,   0,     1,     0,     0,     "",   nullptr,            "how far the A warp goes, 0 = untouched") \
 X(oscB_wave,      "B Wave",       "OSC B",      Choice,  0,     9,     0,     0,     "",   waveNames,       "oscillator B wavetable; Custom = the table designed in waveB (User 1-4 are imported files: don't pick them)") \
 X(oscB_morph,     "B Morph",      "OSC B",      Float,   0,     1,     0.5,   0,     "",   nullptr,            "position inside wavetable B") \
 X(oscB_coarse,    "B Coarse",     "OSC B",      Int,    -24,    24,    0,     0,     "st", nullptr,            "oscillator B transpose in semitones (also the FM ratio)") \
 X(oscB_fine,      "B Fine",       "OSC B",      Float,  -100,   100,   5,     0,     "ct", nullptr,            "oscillator B detune in cents") \
 X(oscB_level,     "B Level",      "OSC B",      Float,   0,     1,     0,     0,     "",   nullptr,            "oscillator B volume (can be 0 when B is only an FM modulator)") \
 X(oscB_warp,      "B Warp",       "OSC B",      Choice,  0,     6,     0,     0,     "",   warpNames,          "warp for table B, same modes as A Warp") \
 X(oscB_warp_amt,  "B Warp Amt",   "OSC B",      Float,   0,     1,     0,     0,     "",   nullptr,            "how far the B warp goes, 0 = untouched") \
 X(oscC_wave,      "C Wave",       "OSC C",      Choice,  0,     9,     0,     0,     "",   waveNames,       "oscillator C wavetable, a third layer beside A and B (Custom = the table designed in waveC)") \
 X(oscC_morph,     "C Morph",      "OSC C",      Float,   0,     1,     0.5,   0,     "",   nullptr,            "position inside wavetable C") \
 X(oscC_coarse,    "C Coarse",     "OSC C",      Int,    -24,    24,    0,     0,     "st", nullptr,            "oscillator C transpose in semitones (octaves and fifths stay in key)") \
 X(oscC_fine,      "C Fine",       "OSC C",      Float,  -100,   100,   0,     0,     "ct", nullptr,            "oscillator C detune in cents") \
 X(oscC_level,     "C Level",      "OSC C",      Float,   0,     1,     0,     0,     "",   nullptr,            "oscillator C volume, 0 = off") \
 X(oscC_warp,      "C Warp",       "OSC C",      Choice,  0,     6,     0,     0,     "",   warpNames,          "warp for table C, same modes as A Warp") \
 X(oscC_warp_amt,  "C Warp Amt",   "OSC C",      Float,   0,     1,     0,     0,     "",   nullptr,            "how far the C warp goes") \
 X(sub_level,      "Sub",          "MIX",        Float,   0,     1,     0,     0,     "",   nullptr,            "sine sub-oscillator one octave below") \
 X(noise_level,    "Noise",        "MIX",        Float,   0,     1,     0,     0,     "",   nullptr,            "white noise level") \
 X(fm_amount,      "FM B>A",       "MIX",        Float,   0,     1,     0,     0,     "",   nullptr,            "how much B frequency-modulates A: 0 none, 0.1 warm, 0.3+ metallic") \
 X(filter_type,    "Filter Type",  "FILTER",     Choice,  0,     8,     1,     0,     "",   filterTypeNames, "LP = low-pass, HP = high-pass, BP = band-pass (12/24 dB per octave); Notch = a hole at the cutoff; Comb = metallic resonance at the cutoff pitch; Formant = vowel (cutoff sweeps A-E-I-O-U)") \
 X(filter_cutoff,  "Cutoff",       "FILTER",     Float,   20,    20000, 8000,  632,   "Hz", nullptr,            "filter cutoff frequency in Hz") \
 X(filter_res,     "Resonance",    "FILTER",     Float,   0,     1,     0.1,   0,     "",   nullptr,            "filter resonance, self-oscillates near 1") \
 X(filter_drive,   "Drive",        "FILTER",     Float,   1,     10,    1,     0,     "",   nullptr,            "filter input saturation, 1 = clean") \
 X(filter_env,     "Filt Env",     "FILTER",     Float,  -5,     5,     0,     0,     "oct",nullptr,            "filter envelope depth in octaves (negative = inverted)") \
 X(filter_keytrack,"Key Track",    "FILTER",     Float,   0,     1,     0.3,   0,     "",   nullptr,            "how much cutoff follows the played pitch") \
 X(filter_routing, "Routing",      "FILTER 2",   Choice,  0,     3,     0,     0,     "",   filterRoutingNames, "second filter: Off; Series = filter 1 then filter 2; Parallel = both on the whole sound, summed; Split = A, sub and noise through filter 1, B and C through filter 2") \
 X(filter2_type,   "Type 2",       "FILTER 2",   Choice,  0,     8,     1,     0,     "",   filterTypeNames, "second filter's type") \
 X(filter2_cutoff, "Cutoff 2",     "FILTER 2",   Float,   20,    20000, 2000,  632,   "Hz", nullptr,            "second filter's cutoff in Hz") \
 X(filter2_res,    "Res 2",        "FILTER 2",   Float,   0,     1,     0.1,   0,     "",   nullptr,            "second filter's resonance") \
 X(filter2_env,    "Env 2",        "FILTER 2",   Float,  -5,     5,     0,     0,     "oct",nullptr,            "filter envelope depth on cutoff 2, in octaves") \
 X(filter2_drive,  "Drive 2",      "FILTER 2",   Float,   1,     10,    1,     0,     "",   nullptr,            "second filter's input saturation (advanced)") \
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
 X(mod1_source,     "Mod1 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 1 source") \
 X(mod1_dest,       "Mod1 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 1 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod1_amount,     "Mod1 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 1 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod2_source,     "Mod2 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 2 source") \
 X(mod2_dest,       "Mod2 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 2 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod2_amount,     "Mod2 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 2 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod3_source,     "Mod3 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 3 source") \
 X(mod3_dest,       "Mod3 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 3 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod3_amount,     "Mod3 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 3 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod4_source,     "Mod4 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 4 source") \
 X(mod4_dest,       "Mod4 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 4 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod4_amount,     "Mod4 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 4 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod5_source,     "Mod5 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 5 source") \
 X(mod5_dest,       "Mod5 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 5 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod5_amount,     "Mod5 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 5 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod6_source,     "Mod6 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 6 source") \
 X(mod6_dest,       "Mod6 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 6 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod6_amount,     "Mod6 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 6 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod7_source,     "Mod7 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 7 source") \
 X(mod7_dest,       "Mod7 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 7 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod7_amount,     "Mod7 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 7 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod8_source,     "Mod8 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 8 source") \
 X(mod8_dest,       "Mod8 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 8 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod8_amount,     "Mod8 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 8 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod9_source,     "Mod9 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 9 source") \
 X(mod9_dest,       "Mod9 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 9 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod9_amount,     "Mod9 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 9 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod10_source,     "Mod10 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 10 source") \
 X(mod10_dest,       "Mod10 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 10 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod10_amount,     "Mod10 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 10 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod11_source,     "Mod11 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 11 source") \
 X(mod11_dest,       "Mod11 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 11 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod11_amount,     "Mod11 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 11 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod12_source,     "Mod12 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 12 source") \
 X(mod12_dest,       "Mod12 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 12 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod12_amount,     "Mod12 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 12 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod13_source,     "Mod13 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 13 source") \
 X(mod13_dest,       "Mod13 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 13 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod13_amount,     "Mod13 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 13 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod14_source,     "Mod14 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 14 source") \
 X(mod14_dest,       "Mod14 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 14 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod14_amount,     "Mod14 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 14 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod15_source,     "Mod15 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 15 source") \
 X(mod15_dest,       "Mod15 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 15 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod15_amount,     "Mod15 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 15 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod16_source,     "Mod16 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 16 source") \
 X(mod16_dest,       "Mod16 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 16 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod16_amount,     "Mod16 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 16 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod17_source,     "Mod17 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 17 source") \
 X(mod17_dest,       "Mod17 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 17 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod17_amount,     "Mod17 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 17 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod18_source,     "Mod18 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 18 source") \
 X(mod18_dest,       "Mod18 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 18 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod18_amount,     "Mod18 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 18 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod19_source,     "Mod19 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 19 source") \
 X(mod19_dest,       "Mod19 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 19 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod19_amount,     "Mod19 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 19 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(mod20_source,     "Mod20 Src",     "MOD MATRIX", Choice,  0,     18,    0,     0,     "",   modSourceNames,     "connection 20 source") \
 X(mod20_dest,       "Mod20 Target",  "MOD MATRIX", Choice,  0,     999,   0,     0,     "",   modTargetNames,     "connection 20 target: Pitch, Pitch B, Amp, Pan or the name of any knob") \
 X(mod20_amount,     "Mod20 Amt",     "MOD MATRIX", Float,  -1,     1,     0,     0,     "",   nullptr,            "connection 20 depth -1..1 (Pitch: x12 semitones; knobs: fraction of the knob's travel)") \
 X(unison_voices,  "Unison",       "VOICE",      Int,     1,     16,    1,     0,     "",   nullptr,            "stacked detuned copies per note, 1-16 (3 = wide, 7+ = supersaw)") \
 X(unison_detune,  "Detune",       "VOICE",      Float,   0,     50,    10,    0,     "ct", nullptr,            "unison detune in cents") \
 X(unison_spread,  "Spread",       "VOICE",      Float,   0,     1,     0.5,   0,     "",   nullptr,            "unison stereo width") \
 X(unison_morph,   "Uni Morph",    "VOICE",      Float,   0,     1,     0,     0,     "",   nullptr,            "spreads the unison copies across the wavetable position, so each copy has a slightly different timbre") \
 X(glide,          "Glide",        "VOICE",      Float,   0,     2,     0,     0,     "s",  nullptr,            "portamento time in seconds") \
 X(bend_range,     "Bend Range",   "VOICE",      Int,     1,     48,    2,     0,     "st", nullptr,            "pitch-wheel range in semitones (advanced)") \
 X(macro1,         "Brightness",   "MACROS",     Float,   0,     1,     0,     0,     "",   nullptr,            "macro: opens the sound up (wired to Cutoff by default)") \
 X(macro2,         "Movement",     "MACROS",     Float,   0,     1,     0,     0,     "",   nullptr,            "macro: more motion (A Morph and Chorus Mix by default)") \
 X(macro3,         "Grit",         "MACROS",     Float,   0,     1,     0,     0,     "",   nullptr,            "macro: dirt and edge (filter Drive and FM by default)") \
 X(macro4,         "Space",        "MACROS",     Float,   0,     1,     0,     0,     "",   nullptr,            "macro: room and echo (Reverb Mix and Delay Mix by default)") \
 X(macro5,         "Width",        "MACROS",     Float,   0,     1,     0,     0,     "",   nullptr,            "macro: stereo size (Detune and Spread by default)") \
 X(macro6,         "Length",       "MACROS",     Float,   0,     1,     0,     0,     "",   nullptr,            "macro: longer notes (Release and Decay by default)") \
 X(arp_mode,       "Arp",          "ARP",        Choice,  0,     5,     0,     0,     "",   arpModeNames,       "arpeggiator: Off, or the order held notes are played in") \
 X(arp_rate,       "Rate",         "ARP",        Choice,  0,     5,     1,     0,     "",   arpRateNames,       "arpeggiator step length, locked to the host tempo") \
 X(arp_octaves,    "Octaves",      "ARP",        Int,     1,     4,     1,     0,     "",   nullptr,            "how many octaves the pattern climbs through") \
 X(arp_gate,       "Gate",         "ARP",        Float,   0.1,   1,     0.6,   0,     "",   nullptr,            "how much of each step the note sounds for") \
 X(arp_swing,      "Swing",        "ARP",        Float,   0,     0.5,   0,     0,     "",   nullptr,            "delays every other step for a shuffle feel (advanced)") \
 X(dist_mode,      "Dist Mode",    "DISTORTION", Choice,  0,     4,     0,     0,     "",   distModeNames,      "Soft = warm saturation, Hard = clipping, Tube = asymmetric valve-like, Fold = wavefolder, Crush = bit and rate reduction") \
 X(dist_drive,     "Dist Drive",   "DISTORTION", Float,   0,     36,    12,    0,     "dB", nullptr,            "distortion input gain in dB") \
 X(dist_tone,      "Dist Tone",    "DISTORTION", Float,   500,   20000, 8000,  3000,  "Hz", nullptr,            "low-pass after the distortion (advanced)") \
 X(dist_mix,       "Dist Mix",     "DISTORTION", Float,   0,     1,     0,     0,     "",   nullptr,            "distortion amount, 0 = off") \
 X(eq_low_gain,    "Low",          "EQ",         Float,  -12,    12,    0,     0,     "dB", nullptr,            "low shelf gain in dB") \
 X(eq_mid_gain,    "Mid",          "EQ",         Float,  -12,    12,    0,     0,     "dB", nullptr,            "mid peak gain in dB") \
 X(eq_high_gain,   "High",         "EQ",         Float,  -12,    12,    0,     0,     "dB", nullptr,            "high shelf gain in dB") \
 X(eq_low_freq,    "Low Freq",     "EQ",         Float,   40,    600,   150,   150,   "Hz", nullptr,            "low shelf corner (advanced)") \
 X(eq_mid_freq,    "Mid Freq",     "EQ",         Float,   200,   8000,  1200,  1000,  "Hz", nullptr,            "mid peak frequency (advanced)") \
 X(eq_mid_q,       "Mid Q",        "EQ",         Float,   0.3,   4,     1,     0,     "",   nullptr,            "mid peak width, higher = narrower (advanced)") \
 X(eq_high_freq,   "High Freq",    "EQ",         Float,   2000,  16000, 6000,  5000,  "Hz", nullptr,            "high shelf corner (advanced)") \
 X(chorus_mode,    "Chorus Mode",  "CHORUS",     Choice,  0,     3,     0,     0,     "",   chorusModeNames, "Chorus = classic, Ensemble = lush string-machine, Flanger = short metallic sweep, Dimension = wide and subtle") \
 X(chorus_rate,    "Chorus Rate",  "CHORUS",     Float,   0.05,  5,     0.8,   0.5,   "Hz", nullptr,            "chorus speed in Hz") \
 X(chorus_depth,   "Chorus Depth", "CHORUS",     Float,   0,     1,     0.3,   0,     "",   nullptr,            "chorus depth") \
 X(chorus_mix,     "Chorus Mix",   "CHORUS",     Float,   0,     1,     0,     0,     "",   nullptr,            "chorus amount") \
 X(chorus_voices,  "Voices",       "CHORUS",     Int,     1,     4,     2,     0,     "",   nullptr,            "number of chorus voices (advanced)") \
 X(chorus_feedback,"Chorus FB",    "CHORUS",     Float,  -0.9,   0.9,   0,     0,     "",   nullptr,            "chorus/flanger feedback, negative inverts (advanced)") \
 X(chorus_spread,  "Chorus Spread","CHORUS",     Float,   0,     1,     0.7,   0,     "",   nullptr,            "stereo width of the chorus (advanced)") \
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
 X(comp_threshold, "Threshold",    "COMPRESSOR", Float,  -48,    0,    -18,    0,     "dB", nullptr,            "level above which the compressor pulls down") \
 X(comp_ratio,     "Ratio",        "COMPRESSOR", Float,   1,     20,    4,     0,     "",   nullptr,            "how hard it pulls down, 4 = gentle glue, 20 = limiting") \
 X(comp_attack,    "Comp Attack",  "COMPRESSOR", Float,   1,     100,   10,    0,     "ms", nullptr,            "how fast it reacts (advanced)") \
 X(comp_release,   "Comp Release", "COMPRESSOR", Float,   20,    1000,  150,   0,     "ms", nullptr,            "how fast it lets go (advanced)") \
 X(comp_makeup,    "Makeup",       "COMPRESSOR", Float,   0,     24,    0,     0,     "dB", nullptr,            "gain added back after compression") \
 X(comp_mix,       "Comp Mix",     "COMPRESSOR", Float,   0,     1,     0,     0,     "",   nullptr,            "compressor amount, 0 = off, under 1 = parallel compression") \
 X(master_gain,    "Master",       "MASTER",     Float,  -24,    6,    -6,     0,     "dB", nullptr,            "output level in dB")

enum class P : int
{
#define STACKS_ENUM_ENTRY(id, ...) id,
    STACKS_PARAMS(STACKS_ENUM_ENTRY)
#undef STACKS_ENUM_ENTRY
    COUNT
};

constexpr int kNumParams = static_cast<int>(P::COUNT);

// Oscillator k (0 = A, 1 = B, 2 = C) parameters.
inline P oscWaveParam    (int osc) noexcept { return osc == 0 ? P::oscA_wave     : osc == 1 ? P::oscB_wave     : P::oscC_wave; }
inline P oscMorphParam   (int osc) noexcept { return osc == 0 ? P::oscA_morph    : osc == 1 ? P::oscB_morph    : P::oscC_morph; }
inline P oscCoarseParam  (int osc) noexcept { return osc == 0 ? P::oscA_coarse   : osc == 1 ? P::oscB_coarse   : P::oscC_coarse; }
inline P oscLevelParam   (int osc) noexcept { return osc == 0 ? P::oscA_level    : osc == 1 ? P::oscB_level    : P::oscC_level; }
inline P oscWarpParam    (int osc) noexcept { return osc == 0 ? P::oscA_warp     : osc == 1 ? P::oscB_warp     : P::oscC_warp; }
inline P oscWarpAmtParam (int osc) noexcept { return osc == 0 ? P::oscA_warp_amt : osc == 1 ? P::oscB_warp_amt : P::oscC_warp_amt; }

const std::vector<ParamSpec>& paramSpecs();

// Parameter of modulation slot `slot` (0-based).
P modSourceParam (int slot);
P modDestParam (int slot);
P modAmountParam (int slot);

// Macro k (0-based) knob.
P macroParam (int k);

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
