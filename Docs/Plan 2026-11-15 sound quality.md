# Plan: sound quality (2026-11-15)

An overnight pass over the question "why don't the sounds stand next to the
synths people buy, and what would change that". Three sources: the engine code
read end to end, the ratings from the first two tuning sessions, and a new
offline **audio audit** that plays every preset through the real voices and
effects and measures what comes out (`StacksTests --audit`, see the end).
Half the findings were fixed tonight; the rest are ranked below with effort
and a way to judge each one done.

## 1. What was wrong, in order of how much it costs the sound

### Engine

1. **Linear envelopes.** `juce::ADSR` moves in straight lines. A linear decay
   hangs near the top, then stops dead; a linear release does the same. Every
   classic synth's envelope falls exponentially (a straight line in dB), which
   is what "tight" plucks and natural tails are made of. *Fixed.*
2. **The filter could not snap.** `juce::dsp::LadderFilter` glides its cutoff
   and resonance over a hard-coded 50 ms. A filter envelope with a 10 ms attack
   or a 30 ms decay was smeared to 50 ms: the snap of every pluck, key and bass
   was gone before it started. *Fixed.*
3. **Stepped modulation.** Modulation was computed once per 32 samples and held:
   amp, pan and cutoff moved in 0.7 ms steps (zipper on fast LFOs, clicks on
   deep tremolo), and the wavetable's band-limited level switched abruptly as
   pitch moved, so vibrato, glide and FM sweeps flickered in brightness.
   *Fixed.*
4. **Aliasing from the filter.** The ladder's saturators create harmonics
   above Nyquist that fold back as inharmonic hash, about -50 dB under a
   bright designed table at C7. Serum and Vital oversample here. *Fixed.*
5. **Every note identical.** Oscillators always started at phase 0, so every
   note had exactly the same attack and A against B sat in the same comb each
   time; free-running analog oscillators never do that. *Fixed.*
6. **Quiet.** Voice gain 0.35 and a -6 dB master put a full oscillator at
   about -16 dBFS. Side by side with any other synth Stacks sounded smaller
   before anyone judged the tone. *Fixed (voice gain and the factory level
   pass below).*
7. **Twelve voices.** With long releases, chords of four stole voices (and the
   steal is a hard cut). *Raised to 16; the cut remains, see next steps.*

### Generation (the model and the guards)

8. **High-pass and band-pass main filters.** Of the 32 sounds rated so far, 9
   of 13 trash sounds had a HP or BP main filter; 6 of the 7 with a 24 dB
   HP/BP were trash. In the factory, 4 presets rendered at -45 to -55 dBFS
   peak (effectively silent) because a HP12/HP24 at 500-2000 Hz sat above
   their whole spectrum. *Fixed: guards 4b/4c (see §2).*
9. **Negative filter envelopes and low cutoffs.** 70% of factory presets have a
   negative `filter_env` (basses: 22 of 25), and `filter_cutoff` is 800 Hz at
   the median. A pad at LP12 164 Hz with a -2.6 octave dive ("Resonant Pulse",
   rated 1) has nothing left to hear; 47 presets had a spectral centroid under
   350 Hz at C4, i.e. the fundamental and little else. *Partly fixed: guard 4d
   plus prompt lines; the taste question (how bright should pads be) is open.*
10. **One timbral signature.** Every one of the 165 factory presets puts a
    designed table on oscillator A: 16 harmonics, phases all aligned, a 1/k
    tail. Phase-aligned additive tables have a sharp peak and a buzzy, cutting
    character; the built-in tables (Saw, Pulse, Sync, Organ, Formant, Glass,
    Fold, Grit) go unused on A. *Partly fixed: a `scatter` digit on designed
    tables; the built-in share is a next step.*
11. **Habits of the model.** Reverb type Plate in 77% of saved generations,
    `unison_detune` left at 10 almost always, filter 2 used in 2 of 263,
    warps in 5 of 263, `Key > Cutoff` the most common first connection
    although key tracking is already a knob. *Open: prompt and recipe work.*

### Factory library

12. **Loudness all over the place.** 41 dB between the 10th and 90th
    percentile of RMS; median peak -18 dBFS; 109 of 165 under -30 dB RMS.
    *Fixed: level pass.*
13. The four silent presets, the dull ones and the HP/BP ones above.
    *Fixed by the guard pass; 52 presets still read as dark (see §3).*

## 2. Done tonight

Engine (`Source/SynthVoice.h/.cpp`, `Source/Wavetable.h/.cpp`):

- `Envelope`: analog-curve ADSR. Attack rises fast then eases (aims past 1.0
  and lands at the attack time); decay and release are exponential, times are
  where the curve is a tenth of the way left (-20 dB). A sustain-0 sound that
  has died frees its voice instead of holding it silently until the key lifts.
- `Ladder`: the JUCE ladder's structure (four stages, saturated feedback, the
  six responses as taps) with the cutoff, resonance and drive ramped across
  each 32-sample sub-block instead of smoothed over 50 ms, running at **2x
  oversampling** (polyphase IIR half-band, full quality), with a rational tanh
  instead of a 128-point lookup table.
- Per-sample ramps for the output gains (amp modulation, pan, velocity) and
  for the state-variable cutoffs (Smooth model, Notch, Comb, Formant).
- `Tables::levelForF` and fractional-level reads: two neighbouring mip levels
  blend (leaning on the duller one late, so nothing passes Nyquist); pitch
  sweeps no longer step in brightness.
- Random start phase per note for A, B and C (unison copies keep their even
  spread around it).
- Voice gain 0.35 to 0.45; voices 12 to 16.
- `WaveSpec::scatter` (0-1, a digit for the model): each harmonic gets a fixed
  pseudo-random phase offset scaled by the amount. Same spectrum, rounder
  wave, lower peak. Random designer and mutation write it; the grammar and
  prompt ask the model for it; the Shape display draws it.

Generation (`Source/PatchGenerator.cpp`, `Source/ai/LlmPatchGenerator.cpp`):

- `guardFilters` (called by `enrichPatch`, and runnable over the factory):
  - 4b: a struck sound or lead through a HP/BP main filter: the high-pass goes
    low and still (120-300 Hz), a band-pass on a lead becomes the low-pass it
    meant, a band-pass on a struck sound turns gentle and low, and the struck
    sweep moves to a LP24 filter 2 in series; velocity opens that instead.
  - 4c: HP24/BP24 as the main filter becomes HP12/BP12, any kind.
  - 4d: a low-pass cutoff is moved so its envelope has somewhere to go: the
    dive of a negative envelope stays above 220 Hz (90 for basses), the point
    where the envelope settles stays above 220 Hz (400 for keys and leads, 90
    for basses), a sweep that starts above 5 kHz starts at 1.2-3 kHz instead.
- Prompt: HP24/BP24 warning; negative envelopes are rare and need a cutoff
  above 1500; cutoff ranges by kind (keys and leads 1500-5000, pads 600-2500,
  plucks 400-1200 opening 2-4 octaves, basses 80-800); the scatter digit.

Factory (`Factory/**/*.json`): `--retouch guards` changed the filters of 73
presets (a second pass with the keys/leads floor moved 12 of them again), then
`--level` set each preset's Master so one note at the kind's usual octave
peaks at -10 dBFS (11 presets hit Master's +6 dB limit). Only `master_gain`,
`filter_*`, `filter2_*` and the retargeted velocity connection changed; the
files were rewritten by the same serialiser as before, so the diff also shows
every line because the line endings went from CRLF to LF.

### Numbers (the audit, one note per preset at 48 kHz, see `Tuning/audit/`)

| measure | before | engine only | engine + factory passes |
|---|---|---|---|
| median peak | -18.1 dBFS | -15.6 | **-10.0** |
| RMS spread, 10th to 90th percentile | 41.3 dB | 31.0 | **17.4** |
| median attack to 90% | 82 ms | 46 | 46 |
| presets effectively silent (peak under -40 dBFS) | 4 | 4 | **0** |
| designed table at C7, energy between harmonics | -50 dB | -56 | **-75** |
| built-in Sine at C7, between harmonics | -79 dB | | **-91** |
| "dull" (centroid under 350 Hz at C4) | 47 | 54 | 52 |

(The "before" alias column in the saved before.md reads -45 dB for everything:
that run's measure counted the analysis window's own skirt; the -50 figure is
the corrected measure on the old engine.)

Some presets, before and after everything:

| preset | peak dBFS | attack ms | centroid Hz |
|---|---|---|---|
| Glassy Pluck | -52.5 to -11.5 | 10 to 6 | 7854 to 1227 |
| Punchy Bass (a pluck through HP24 1.5 kHz) | -42.2 to -10.4 | 42 to 8 | |
| Wobbly Tape Pluck (lead through HP12 1 kHz) | -54.5 to -9.5 | | 7349 to 330 |
| Punchy House Stab | -13.2 to -10.0 | 344 to 14 | |
| Dreamy Piano | -11.2 to -9.5 | 300 to 124 | 265 to 406 |
| Crystal Rings | -16.5 to -10.8 | 48 to 36 | 755 to 1315 |

Checks: 1779 engine tests pass (new: mip band limiting at every level,
scatter round trip and magnitude survival, the guards on eleven cases). The
model benchmark with the new prompt and grammar scored 97.6 (97.2 before;
`Benchmarks/README.md`, the row labelled "exp envelopes, ramped ladder 2x,
mip blend, scatter, filter guards").

What tonight could not do is listen. The numbers say the fixes land; the ear
decides whether the new envelopes and the filter guards feel right. The first
session should be an A/B of the factory in the new build, with the tuning rig
on (1 to 5).

## 2b. Round two, the same night (the "do this" list)

Engine (`Source/SynthVoice.*`):

- **Click-free voice steal.** A stolen voice (or a hard all-notes-off) no
  longer stops in one sample: `stopNote` arms a 128-sample fade of what was
  sounding, `startNote` parks the new note, and the render loop begins it when
  the fade is done. Measured by a new engine test: twenty-four quick notes of
  a sine pad on sixteen voices, largest sample-to-sample jump 0.09 of the
  peak (a cut reads near 1, the sine's own slope 0.03).
- **Zero-delay-feedback ladder.** The four TPT stages and the feedback are
  solved within the sample, so the resonance peak sits on the cutoff (977 Hz
  for 1 kHz, 5.9 kHz for 6 kHz, 11.5 kHz for 12 kHz; the one-sample-delayed
  loop drifted well below). Unity drive is close to linear: a 0.5 sine comes
  back at 0.498 with the third harmonic at -52 dB (the old ladder compressed
  peaks by a sixth). Resonance reaches 4.4 at the top so it sings (late peak
  0.68 on an impulse, bounded); half the passband loss is made up. With the
  oversampling kept, the designed table's alias reading went from -75 to
  -90 dB, the factory's worst from -59 to -88.
- **Unison like a stack.** Inner copies bunch toward the centre in pitch
  (|x|^1.6, the JP-8000 shape), outer copies a fifth quieter, every copy at
  its own start phase, pan spread even.

Generation (`Source/ai/LlmPatchGenerator.cpp`, `Source/PatchGenerator.*`):

- **Built-in tables on A.** `waveA` is optional in the grammar; each fresh or
  Far slot is told either to design its table or to take the built-in that
  fits its recipe (two saws: Saw, FM: Sine, PWM: Pulse, formant and vocal:
  Formant, wavefold: Fold, hard sync: Sync, wide glassy: Glass, organ: Organ),
  half and half per batch. A table the model designs anyway for a built-in
  slot is dropped. The benchmark now rewards a share near one half instead
  of one.
- **Two new recipes**: "two filters" (Parallel routing, a resonant LP24
  with its envelope beside a BP12 with a slow LFO on Cutoff 2) and "warped"
  (Bend or Mirror swept by the mod envelope and a slow LFO).
- **Reverb by kind**: the prompt names Hall or Lush for pads and drones,
  Plate or Room for plucks, keys and basses, Shimmer for textures and glowing
  bells; a big pad on a Plate moves to Hall or Lush half the time.

- **The warps were the last aliasing source.** The regrown candidates' audit
  caught it: a Fold warp at 0.6 read -3 dB of alias (as loud as the harmonics),
  Sync -25, Mirror -21, Bend -26, PWM -21; everything with the warp off sat at
  -86. The oscillators now run at twice the rate while a warp other than
  Quantize is on (the buses are decimated after), Mirror is redefined so the
  phase has no jump at the wrap (the peak slides from the end to the middle),
  Sync crossfades into its reset over the last tenth of the master cycle, and
  Fold is anti-aliased by its antiderivative (first-order ADAA). Measured in
  the engine test: Fold 0.6 -34 dB, Sync -53, Bend -65, PWM -40, Mirror -61.

Factory: re-levelled for the new filter (96 presets moved, none now silent;
9 remain at Master's limit). `--regrow` grew three candidates each for the
fifty presets the audit read as weakest, into `Tuning/regrown/` for a
listening pass (nothing in `Factory/` replaced; see that folder's README).

| measure | before everything | after round one | after round two |
|---|---|---|---|
| median peak | -18.1 dBFS | -10.0 | -10.1 |
| RMS spread, 10th to 90th percentile | 41.3 dB | 17.4 | 17.4 |
| median attack to 90% | 82 ms | 46 | 42 |
| designed table at C7, energy between harmonics | -50 dB | -75 | -90 |
| worst alias in the factory | -33 dB (measure skirt) / about -50 | -59 | -88 |
| voice steal, largest jump / peak | about 1 (a cut) | about 1 | 0.09 |
| resonance peak for a 6 kHz cutoff | below the cutoff | below the cutoff | 5.9 kHz |
| Fold warp at 0.6, energy between harmonics | -3 dB | -3 dB | -34 dB |

Still open from the list below: items 3's remaining half (a drive law and
compensation are in; the filter character beyond that is taste), 4, 5, 10,
12, 13, 14, and 6 and 11 wait for the ear.

## 2c. The first listening pass, and a decision (15 Nov, afternoon)

Josh rated 41 sounds in the new build: pads and keys fine, nothing broken.
Of 11 factory leads, 8 were rated 1 and 2 were rated 5; five of the 8 were
already on the audit's regrown list, so the ear and the measurements agree
on what is weak. The regrown candidates were put in the library (Regrown
folder) and rated: 7 earned a 4 or 5, 11 a 1 or 2, the rest were fine.

Swaps made from that: candidates rated 4 or 5 replaced their originals
(Glassy Hiss, Piano Chime, Psychedelic Vibrato, Stereo Future Bass, Vibrant
Bass, Whispering Wind); Glowing Formant Pad was itself rated 4, so its
candidate joined beside it, as did Whispering Wind's second 4; the ten
originals rated 1 whose candidates were all fine took the candidate the audit
liked best (two renamed with "II" for a name clash). Sixteen files left the
factory, eighteen came in: 167 presets. The review folder keeps the fine,
unplaced candidates (134); the 1-2 rated ones are gone.

**Decision (Josh):** the rating system stays in the shipped synth. Every
player rates with 1 to 5 and their Stacks nudges what it generates toward
their taste, so each copy becomes personally tuned. That turns the rig from
an internal tool into a feature, with design work ahead:

- the nudge has to be automatic and bounded per user: recipe weights with the
  40% floor and the wildcard slot, avoid-lines from a user's own ACT traits,
  never examples of liked patches (the policy in `Docs/Tuning rig.md` holds);
- a reset ("forget my ratings") and a view of what the synth has learned, in
  plain words;
- the ratings and the derived weights live with the user's library, not in
  app support, so they travel with a backup;
- the hidden-rig switch goes; the pill and the keys become a normal part of
  the Home and Lab screens, with the tour explaining them;
- the pooled ratings from testers keep feeding the baked-in guards exactly as
  now, so the shipped defaults keep improving too.

Two answers from Josh (15 Nov, later): a player's ratings shape **every** new
sound the synth generates, the five a Home tile grows as much as a Lab batch or
a grow; and a tuned copy is **strictly personal**, there is no sharing or
export of a taste profile.

**Built the same evening** (`Source/Taste.*`, `Docs/Taste.md`): the profile
(avoided traits with the five-and-five rule, recipe weights floored at 0.4 and
capped at 1.7, one even draw in five), applied to every generation request;
the pill for everyone with the summary and the reset behind a click; a Taste
section in Settings with "Start over: forget my ratings"; ratings stored in
the library's Taste folder (the rig's logs moved over once); a tour step. The
taste tests measure the bounds: a loved recipe takes 10% of slots against 7%
even, a hated one still 4%, an avoided trait falls from 47% of pads to 0%
with the pads as varied as before, nothing moves under the thresholds.

## 3. Next, ranked

### Engine

1. *(done in round two)* **Voice stealing without the click** (half a day). The Synthesiser cuts a
   stolen voice to zero in one sample. Keep a short "dying" buffer per voice
   or a shared steal-tail bus: when a voice is stolen, render its next 5 ms
   with a fade and mix that under the new note. Done when a four-note chord
   with a 4 s release re-struck quickly has no clicks in the audit (add a
   chord mode to `--audit`).
2. *(done in round two)* **Unison that sounds like a stack** (half a day). Detune is spread
   linearly and every copy has the same level; Serum-class unison spreads the
   inner voices tighter than the outer ones, drops the outer levels a little
   and randomises each copy's start phase. Measure: the "supersaw" presets'
   width and crest in the audit; the ear for the rest.
3. *(the ZDF ladder, the drive law and half the compensation are done in round two)* **Filter character** (one to two days). The ladder's one-sample feedback
   delay detunes the resonance above about 5 kHz; drive at 1.0 already
   compresses peaks by 17% (tanh at unity), so "Clean" is not clean. Options:
   a zero-delay-feedback ladder (Zavalishin), a drive law whose unity setting
   is linear, and resonance compensation of the passband loss. Measure: the
   audit's alias and centroid columns, plus a sweep test (add `--sweep`).
4. **Noise and sub with colour** (two hours). White noise and a pure sine sub
   are the only options. Pink/brown noise and a sub with a touch of second
   harmonic make basses and textures fuller. Cheap, and the generator can ask
   for them.
5. **Reverb**: the Dattorro plate is good; Hall and Lush are the same tank
   resized. A second algorithm (a feedback delay network with 8 lines) would
   give pads a real hall. One day.

### Generation

6. **Taste: how bright should a pad be?** 52 presets still have a centroid
   under 350 Hz at C4. The guard floors (220 Hz rest for pads) are the
   broken-sound floor, not a taste. Decide with the tuning rig: rate the dark
   pads; if they come back 1-2, raise the pad floor to 400 Hz in `guardFilters`
   and re-run `--retouch guards` and `--level`. Zero code otherwise.
7. *(done in round two)* **Built-in tables on A** (half a day). The grammar makes `waveA` mandatory
   when designing is on, so every AI patch is a designed table. Make it
   optional with a target share ("design a table for about half the patches;
   the others pick a built-in that fits") and check the benchmark's
   `customShare` lands near 0.5 rather than 1.0. The ten built-ins have phase
   structure the designed ones never get.
8. *(done in round two, without weights)* **Recipes that touch the unused half of the synth** (two hours). Filter 2
   is used in 2 of 263 generations, warps in 5. Two new fresh recipes ("two
   filters in parallel, one resonant", "warped: Bend or Mirror on A swept by
   the mod env") and a recipe weight floor so each is tried. Tuning rig
   decides their weights after a session.
9. *(done in round two)* **Reverb type by kind** (one hour): Plate in 77% of patches. A line in the
   prompt (Hall or Lush for pads and drones, Room or Plate for plucks and keys,
   Shimmer for textures) plus a guard that moves a pad with size > 0.6 off
   Plate half the time.
10. **Unison detune written by the model** (one hour): `unison_detune` stays
    at 10 almost always; the prompt names 12-30 but the model does not write
    it. Put `unison_detune` in the core settings list after `unison_voices`
    so the grammar forces a value.

### Factory

11. *(candidates grown in round two from the audit's weakest fifty; the ear chooses)* **Re-grow the weakest third** (an evening of machine time). After one
    rating session on the retouched factory, the presets rated 1-2 get
    regenerated with the current prompt and guards (`--factory` takes a prompt
    index), then audited and levelled. The library should then also carry
    `scatter` on its tables.
12. **A second note in the level pass** (one hour): the pass levels one note
    at one velocity. Add a chord render and keep the louder of the two, so
    pads played as chords do not lean on the soft clip.

### Rigs

13. **Chord and velocity renders in the audit** (two hours): a 4-note chord
    and a soft note per preset, with a click detector on the voice steal and
    a crest-factor column. This is what items 1 and 12 need.
14. **An ear-truth loop** (ongoing): the tuning rig's ratings against the
    audit's columns. Once 60-80 sounds are rated, fit which measures predict
    1-2 ratings; those become the audit's flags, replacing tonight's
    thresholds.

## 4. The rigs

```
StacksTests --audit Factory --out build.nosync/audit/now.md        # the table and summary
StacksTests --audit Factory/Pad --wav build.nosync/audit/wavs      # plus the renders
StacksTests --retouch guards --dry-run                             # what the filter guards would move
StacksTests --retouch guards                                        # move them
StacksTests --level Factory --dry-run                               # what the level pass would set
StacksTests --level Factory --target -10                            # set it
StacksTests --regrow <presets> --out Tuning/regrown --count 3       # new candidates from each preset's prompt, levelled
```

The audit renders one note (MIDI 60, velocity 0.8) held 1.5 s then released,
4 s in all, deterministic (the offline voices are seeded). Columns: RMS and
peak in dBFS; onset (the first millisecond against the peak; 0 is a hard
edge); attack to 90%; release to -40 dB and its shape (t(-6)/t(-30): 0.2 is
exponential, 0.5 linear); centroid; energy above 5 kHz and below 150 Hz;
width (1 minus the L/R correlation); alias (oscillator A alone at C7, energy
between the harmonics against the harmonics). Onset, attack, release and
shape come from a dry render; the rest from the full chain. Flags are
thresholds, not verdicts: "quiet" is RMS under -30 dB over the held part,
which every short pluck earns.

The reports from tonight are in `Tuning/audit/` (before, after the engine,
after the factory passes).
