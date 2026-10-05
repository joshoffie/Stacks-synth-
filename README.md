# Stacks

A hybrid wavetable/FM synthesizer for Logic Pro (AU), VST3 hosts and
standalone, with an on-board AI lab: it proposes patches, you pick what you
like, and it breeds the next generation from your picks.

## Requirements (macOS, Apple silicon)

- Xcode 26 (installed; the command line tools alone also work)
- JUCE 8 at `/Applications/JUCE` (installed)
- CMake ≥ 3.22 and Ninja: `brew install cmake ninja`
- Internet on first configure: CMake fetches llama.cpp (pinned release) automatically

Windows (VST3 + Standalone, CPU-only AI) builds with Visual Studio 2022 through the same
CMake project; JUCE is fetched when `/Applications/JUCE` is absent. The GitHub Actions
workflow in `.github/workflows/build.yml` builds both platforms and runs the tests. The
Windows build is not yet tested on real hardware.

**Pro Tools (AAX).** Avid only hands the AAX SDK to registered developers
(developer.avid.com). Once you have it: configure with
`-DSTACKS_AAX_SDK_PATH=/path/to/aax-sdk` and the AAX format builds alongside the
others. Pro Tools release builds additionally need PACE/iLok signing through
Avid; unsigned AAX only loads in the Pro Tools Developer build.

## Build

```bash
cmake -S . -B build.nosync -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build.nosync
```

The build directory ends in `.nosync` so iCloud Drive leaves it alone.
Every build copies the plug-in into `~/Library/Audio/Plug-Ins/Components`
(AU) and `~/Library/Audio/Plug-Ins/VST3`.

## Trying it

- Standalone: `build.nosync/Stacks_artefacts/Release/Standalone/Stacks.app`
- Logic Pro: Software Instrument track → Instrument slot → AU Instruments →
  Hoffman Audio → Stacks. If it is missing, Logic Pro → Settings →
  Plug-in Manager → select Stacks → *Reset & Rescan Selection*, or restart Logic.

## The panel

Controls sit in fixed rows that follow the signal path, each row captioned and
colour-coded (knob colour = row). The tabs above them switch between
**MACROS** (six big knobs every sound answers: Brightness, Movement, Grit,
Space, Width, Length, each wired per patch by the model or by sensible
defaults), **ALL** (every row at once, scaled to fit) and one row at a time,
enlarged to fill the panel:

1. **SOUND** — Osc A, B and C (morphing wavetables, importable "User 1-4"
   tables via each oscillator's Shape cell, and **Custom**: a table the AI or
   the random breeder designed for this patch), each with a **Warp** (Sync,
   Bend, PWM, Mirror, Fold, Quantize; the Shape cell shows the warped wave),
   Mix (sub, noise, FM B→A). Wide rows wrap onto two lines.
2. **SAMPLE** — a sample oscillator: click the display to load a file (wav,
   aiff, flac, mp3; copied into `~/Music/Stacks Patches/Samples` and saved
   with the patch). **Pitched** plays it at the note (root C3, loop or once,
   from **Start**); **Granular** sows short Hann-windowed grains around Start
   with Grain Size, Grain Rate, Spray, random pitch and stereo spread. Start is
   a modulation target, so an LFO scans the file. The AI never touches it.
3. **FILTER** — ladder filter (LP/HP/BP, 12 or 24 dB) plus Notch, Comb and
   Formant modes, a **second filter** with Routing (Series, Parallel, or Split:
   A, sub and noise through 1, B and C through 2), its envelope, the amp
   envelope. The response display draws the combined curve.
4. **MODULATORS** — LFO 1-4 with drawable shapes, the Mod Env, Assign, Voice
   (unison up to 16 voices with **Uni Morph** spreading the copies across the
   table, glide, Bend Range under *more*) and the **Arp** (up/down/up-down/random/as-played, synced
   rates, octaves, gate, swing). See *Modulators* below.
5. **SHAPE** — oversampled distortion (soft, hard, tube, fold, crush), a
   three-band EQ, and a compressor at the end of the chain (parallel mix).
6. **SPACE** — effects. Core knobs on the panel, the rest behind each
   section's **more** button:
   - *Chorus*: Chorus / Ensemble (string machine) / Flanger / Dimension modes;
     more: voices, feedback, stereo spread, tone.
   - *Delay*: Stereo / Ping-Pong / Tape modes, tempo sync (1/16 … 1/2, dotted,
     triplet) or free time; more: tone and high-pass in the feedback path, tape
     wow, stereo width.
   - *Reverb*: Dattorro-style plate tank with Room / Plate / Hall / Shimmer
     types; more: pre-delay, low cut, high cut, tail modulation, octave-up
     shimmer amount, width.

Knobs that a modulator drives show a ring in the modulator's colour and a live
marker that follows the most recently played note, so you can see an LFO
moving a parameter while you hold a key.

The header shows the output as a waveform and a spectrum. The FILTER row draws
the filter's response curve and both envelopes, live. A help line under the
rows names the control under the mouse and says in plain words what it does;
with nothing under the mouse it describes the current screen. Every control
also has a tooltip.

Undo and redo (the arrows in the header, Cmd+Z and Shift+Cmd+Z) step through
whole sounds: every load, leaf drag and knob gesture leaves a snapshot.

Every generated patch (random or AI) passes a tuning guard: oscillator A only at
octaves, an audible oscillator B only at octaves, fine detune and pitch
modulation capped, so nothing comes out of key with what you play.

## Modulators

The MODULATORS row has tabs for LFO 1-4, the Mod Env and the performance
sources (velocity, key, mod wheel, aftertouch, per-note random). Each LFO has a
display you can draw in (click adds a point, drag moves, double-click removes;
right-click resets), shape presets, rate or tempo sync, phase, and Free /
Note (retrigger) mode. To route a modulator: press **Assign >**, then click any
knob - including effect knobs. The knob shows a ring in the modulator's colour;
the modulator's tab lists its targets with depth sliders and an × to remove.
"Pitch / Amp / Pan" adds the targets that aren't knobs. You can also drag a
modulator's tab straight onto a knob, and drag a knob's ring up or down to
change that connection's depth.

**Audition previews.** Clicking a patch anywhere (a library row, a leaf, a
card, the seed) plays a short phrase that suits it: a low note for a bass, a
chord for a pad, a triad for keys and bells. Browsing needs no keyboard. The
**Play** button in the Library header turns it off.

**MPE.** Settings (the gear) > MPE turns on per-note expression for Seaboard,
Osmose, Linnstrument and friends: channels 2-16 carry one note each with a
48-semitone bend, per-note pressure (the Aftertouch source) and slide (the
**Slide** source, CC74). Channel 1 stays the master; Bend Range (VOICE > more)
sets its wheel and ordinary MIDI. Logic: set the track's MIDI input to MPE.

**MTS-ESP.** Microtuning via the ODDSound client (`ThirdParty/MTS-ESP`): run
any MTS-ESP master (the free MTS-ESP Mini, Scala, Surge XT as master) and every
note in Stacks follows its scale, keys the scale leaves out stay silent. Nothing
to set up; Settings shows the connected scale.

## The AI Lab

The **STACKS** tab shows the loop as a plant: the sound you're playing is the
seed in the middle; AI ideas grow above it, random variations below. Click a
leaf to hear it, drag it outward for wilder children, right-click it to plant
it (it becomes the seed and a new generation grows) or to favourite it.
Right-click the seed for fresh ideas or to favourite what you're hearing.
The **LIST** tab is the same batch with full descriptions. Every finished
generation is also saved under `~/Library/Application Support/Stacks/history`.

While a batch is being written, a progress bar above the tabs shows which patch
the model is on, its name as soon as it is known, and how many settings it has
written so far; the Garden draws the same progress as a ring around the seed.

1. Type what you want ("mgmt style synth patch", "dark evolving pad") and press
   Return, or **Generate**: new patches from your description alone, no preset
   needed. **Evolve** — ten descendants of the sound you're playing, each
   given its own direction of change (the Variation knob decides how far they
   may stray, up to changing the category), and pushed apart if the model hands
   back near-copies. The sound you're playing is
   never replaced; it stays pinned at the top of the LIST as *Now Playing*,
   and the Garden's seed is whatever this generation grew from, so loading
   other sounds (from the library, say) leaves the Garden alone. Click the
   seed to hear the parent again.
2. Click a leaf or card to load it and play. **Save** (the button, or a
   right-click on a leaf, the seed or a card) stores what you're hearing into a
   library folder of your choice. Hearts live in the library: the ♥ on a saved
   preset marks it a favourite, and the library's ♥ filter shows only those.
3. The direction text also steers Evolve. With an AI engine, the model first
   writes itself a short *sound brief* about what the direction should sound
   like, then designs from it.
4. **Design wavetables** (on by default) lets both the AI and the random
   breeder invent a new wavetable for oscillator A in most patches - written
   as a spectrum (harmonic levels for 2-4 morph frames) and rendered into a
   band-limited table on the spot. When you Evolve, the model sees the
   parent's spectrum (even for a built-in wave) and designs a relative of it.
   The Shape cell shows the table and its name. Turn it off for faster
   batches that only pick built-in waves.

**Engine** picks who designs the patches:

- *Random (no AI)* — archetype-based random patches, crossover and mutation.
  Instant, always available.
- *Built-in AI* — a Qwen3 language model running **inside the plug-in**
  (llama.cpp on Metal, nothing else to install). Pick a model from the menu;
  if it isn't on the Mac yet it downloads from Hugging Face into
  `~/Library/Application Support/Stacks/models/` (1.7B ≈ 1.8 GB, 4B ≈ 2.5 GB,
  8B ≈ 5 GB). If the Ollama app has already pulled a Qwen3 model, Stacks lists
  it as "(Ollama's copy)" and uses that file directly, no second download.
  Output is grammar-constrained, so the model can only produce valid patches.
  The grammar also makes every patch state a core set of 20 settings and at
  least one modulation connection, and connections are written as units whose
  target list fits the source (an LFO can land on Morph, Cutoff, Pan, Amp,
  effect mixes or a gentle Pitch vibrato, never on a fine-tune; velocity on
  Cutoff, Amp, Decay...), with the LFO's shape, rate and sync alongside.
  A tuning guard then reroutes any stepped or random LFO off pitch and caps
  vibrato, so nothing comes out of key.
  On an M4 with 16 GB the 4B model runs at ~23-30 tokens/s: a batch of five
  patches takes about a minute. The model is unloaded after 10 idle minutes.
- *Ollama app* — the same models served by a running [Ollama](https://ollama.com).
  Handy for experiments; friends don't need it.

Per batch the AI designs 5 patches that stream into the **✦ AI IDEAS** section
as they arrive, while the random breeder's 5 land in a folded **RANDOM
VARIATIONS** section underneath.

**From audio.** The Lab's **From audio** button (or dropping an audio file on
the Lab) works two ways. A short recording (a one-shot, a note from another
synth, a voice) is *recreated*: Stacks measures its pitch, envelope, spectrum
over time, noise, width and vibrato, loads an imitation patch at once (the
measured spectrum becomes the Custom table) and evolves it with the model. A
whole track (over 20 seconds) is analysed for tempo, key, where the mix has
room, brightness, density and dynamics; the resulting brief goes into the
prompt box and a fresh batch is designed to fit the song, with auditions in
its key. The model never hears audio; the analysis is the ears, the model the
designer. `StacksTests --recreate file` and `--song file` print both paths.

**Prompt cues.** When a prompt names an effect or a voice setting ("lush chorus", "with
delay", "tape", "shimmer", "distorted", "wide", "sub bass", "portamento",
"punchy", "riser", "arp"), the generator raises that setting after the model
has written the patch, so the words are honoured even when the model forgets
them; it never lowers what the model chose.

**Library**: the LIBRARY tab browses `~/Music/Stacks Patches`: your saved
presets and folders, plus a **Factory** folder: 85 presets in eight
categories (Bass, Bell, Drone, Keys, Lead, Pad, Pluck, Texture), each with its
own AI-designed wavetable and tags, generated by the on-board model from 48
briefs and curated for sanity, unique names and audible difference. It is
installed from the bundle on first run and merged on later builds (only
missing files are added, so edits and deletions stick); `StacksTests
--factory` rebuilds it and `--retouch` re-applies the prompt cues. **Save** asks for a folder and a name
and stores the sound exactly as it is; if you edited a preset you loaded, it
offers *Save as new* or *Overwrite*. The ♥ on a library row is the only
favourite: a flag on that saved preset, which the ♥ filter shows. To grow from
a favourite, load it and press Evolve. A patch's "..." moves it to another
folder, edits its tags or trashes it.

The engine choice is stored in `~/Library/Application Support/Stacks/`, next to
`llama.log` (runtime + speed stats), `last-ai-prompt.txt`, `last-ai-reply.txt`
and `grammar.gbnf` for prompt tuning. Patches are plain JSON (`Save…` / `Load…`,
default folder `~/Music/Stacks Patches`).

## Benchmarks

`StacksTests --bench "<label>"` runs a fixed prompt set (no prompt, a style
reference, a pad, a pluck, a bass, plus two Evolve rounds) through the
installed built-in model and scores delivery, movement, designed tables, hint
adherence, in-key output before the tuning guard, Evolve diversity and speed
into a 0–100 figure. Each run lands in `Benchmarks/` as JSON plus a table row,
so a prompt, grammar or guard change is judged against the previous runs
rather than by ear alone. `Docs/` holds the competitive research that shaped
the roadmap.

## Tests

`StacksTests` is a console app over the engine-side code (no plug-in wrapper):
spectral table rendering and analysis, the patch JSON format, the tuning
guard, the AI grammar (every referenced rule defined, no underscores) and
prompt, the random breeder, and the AI generator fed by a fake model that
streams canned JSON in awkward chunks.

```bash
cmake --build build.nosync --target StacksTests && ./build.nosync/StacksTests_artefacts/Release/StacksTests
```

## Theme file

Drop a `theme.json` into `~/Music/Stacks Patches` and restart to recolour the
UI. Any subset of these keys, hex `rrggbb` or `aarrggbb`:

```json
{ "background": "15161a", "panel": "1f2128", "card": "272a33", "accent": "ffb24a",
  "text": "ececec", "muted": "8a8f99",
  "rowSound": "f2a541", "rowFilter": "e8775a", "rowMovement": "5ec8c0",
  "rowSpace": "7fa7d8", "rowShape": "d08ab8", "rowMacro": "e0c070" }
```

## Giving it to friends

Build in Release, then zip `~/Library/Audio/Plug-Ins/Components/Stacks.component`
(and the VST3 if wanted). They drop it into the same folder on their Mac and
rescan in Logic. First AI use downloads a model (ask them to pick 4B). The
plug-in is ad-hoc signed; Gatekeeper may need a right-click → Open on the
standalone app, and Logic may ask once to allow the component.

## Layout

```
Source/
  Parameters.*     the parameter table: ids, ranges, defaults, groups, AI hints
  Wavetable.*      10 band-limited morphing wavetables plus 4 user slots (WAV import), built at startup
  LfoTable.h       drawable LFO shapes (points -> 512-sample tables)
  SynthVoice.*     one voice: 3 wavetable oscs (B can FM A, 6 warps), sample osc, sub, noise, 2 filters, 3 env, 4 LFO, 20 connections, 16-voice unison
  Sampler.*        the sample oscillator: file bank (lock-free handoff) and the pitched / granular player
  ai/SampleAnalyser.* a recording in numbers (pitch, envelope, spectrum, noise, width, vibrato) and the imitation patch
  ai/SongAnalyser.*   a track in numbers (tempo, key, spectral room, density, dynamics) and the brief for the model
  Effects.*        chorus/ensemble/flanger/dimension, stereo/ping-pong/tape delay, Dattorro reverb with shimmer
  Patch.*          a named set of parameter values; JSON in/out; apply/capture
  PatchGenerator.* generators: Random (archetypes + crossover/mutation)
  ai/LlmBackend.h  interface for "a model that streams a chat reply"; OllamaBackend.cpp speaks HTTP to Ollama
  ai/LlamaBackend.* the same interface on top of llama.cpp (Metal), loads a GGUF inside the plug-in
  ai/ModelManager.* model catalogue, Ollama-copy detection, background downloads
  ai/LlmPatchGenerator.* prompt + GBNF grammar built from the parameter table, streaming JSON parser, fallback to Random
  PluginProcessor.* audio engine, master FX, lab state, engine selection, background generation
  PluginEditor.*   window: header, knob panel, AI lab, keyboard
  Controls.*       ParamKnob (rings, live marker, assign/drag target), ParamChoice
  SynthPanel.*     the fixed rows, ALL/row view tabs, advanced callouts, wave display
  ModulatorsPanel.* LFO editor tabs, Assign, connection list
  LabPanel.*       Fresh ideas / Evolve, progress strip, Now Playing, cards
  GardenView.*     seed-and-leaves view of a generation
  LibraryPanel.*   folder browser for saved presets
```
