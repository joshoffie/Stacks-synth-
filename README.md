# Stacks

A hybrid wavetable/FM synthesizer for Logic Pro (AU), VST3 hosts and
standalone, with an on-board AI lab: it proposes patches, you pick what you
like, and it breeds the next generation from your picks.

## Requirements (macOS, Apple silicon)

- Xcode 26 (installed; the command line tools alone also work)
- JUCE 8 at `/Applications/JUCE` (installed)
- CMake ≥ 3.22 and Ninja: `brew install cmake ninja`
- Internet on first configure: CMake fetches llama.cpp (pinned release) automatically

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
colour-coded (knob colour = row). The tabs above them switch between **ALL**
(every row at once) and one row at a time, enlarged to fill the panel:

1. **SOUND** — Osc A, Osc B (morphing wavetables, importable "User 1-4"
   tables via each oscillator's Shape cell, and **Custom**: a table the AI or
   the random breeder designed for this patch), Mix (sub, noise, FM B→A)
2. **FILTER** — ladder filter, its envelope, the amp envelope
3. **MODULATORS** — LFO 1-4 with drawable shapes, the Mod Env, Assign, plus
   Voice (unison, glide). See *Modulators* below.
4. **SPACE** — effects. Core knobs on the panel, the rest behind each
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

## The AI Lab

The **GARDEN** tab shows the loop as a plant: the sound you're playing is the
seed in the middle; AI ideas grow above it, random variations below. Click a
leaf to hear it, drag it outward for wilder children, right-click it to plant
it (it becomes the seed and a new generation grows) or to favourite it.
Right-click the seed for fresh ideas or to favourite what you're hearing.
The **LIST** tab is the same batch with full descriptions. Every finished
generation is also saved under `~/Library/Application Support/Stacks/history`.

While a batch is being written, a progress bar above the tabs shows which patch
the model is on, its name as soon as it is known, and how many settings it has
written so far; the Garden draws the same progress as a ring around the seed.

1. **Fresh ideas** — ten new patches from the direction text alone. **Evolve** —
   ten descendants of the sound you're playing. The sound you're playing is
   never replaced; it stays pinned at the top of the LIST as *Now Playing*,
   and the Garden's seed is whatever this generation grew from, so loading
   other sounds (from the library, say) leaves the Garden alone. Click the
   seed to hear the parent again.
2. Click a leaf or card to load it and play. **♥ Favourite** saves what you're
   hearing into a library folder of your choice and gives it a heart.
3. Type a direction ("darker", "more movement", "in the style of MGMT") to
   steer the next round. With an AI engine, the model first writes itself a
   short *sound brief* about what that direction should sound like, then
   designs from it.
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

**Library**: the LIBRARY tab browses `~/Music/Stacks Patches`: only your own
saved presets and the folders you made. **Save** on the Now Playing card asks
for a folder and a name and stores the sound exactly as it is; if you edited a
preset you loaded, it offers *Save as new* or *Overwrite*. The ♥ on a card or
in the library is just a favourite flag (the ♥ filter in the library shows only
those); to grow from a favourite, select it and press Evolve. A patch's "..."
moves it to another folder or trashes it.

The engine choice is stored in `~/Library/Application Support/Stacks/`, next to
`llama.log` (runtime + speed stats), `last-ai-prompt.txt`, `last-ai-reply.txt`
and `grammar.gbnf` for prompt tuning. Patches are plain JSON (`Save…` / `Load…`,
default folder `~/Music/Stacks Patches`).

## Tests

`StacksTests` is a console app over the engine-side code (no plug-in wrapper):
spectral table rendering and analysis, the patch JSON format, the tuning
guard, the AI grammar (every referenced rule defined, no underscores) and
prompt, the random breeder, and the AI generator fed by a fake model that
streams canned JSON in awkward chunks.

```bash
cmake --build build.nosync --target StacksTests && ./build.nosync/StacksTests_artefacts/Release/StacksTests
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
  SynthVoice.*     one voice: 2 wavetable oscs (B can FM A), sub, noise, ladder filter, 3 env, 4 LFO, 12 connections, unison
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
