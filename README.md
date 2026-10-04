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
colour-coded (knob colour = row):

1. **SOUND** — Osc A, Osc B (morphing wavetables), Mix (sub, noise, FM B→A)
2. **FILTER** — ladder filter, its envelope, the amp envelope
3. **MOVEMENT** — Mod Env (a free third ADSR), LFO 1, LFO 2, Voice (unison, glide)
4. **MATRIX** — six modulation slots: source → destination × amount. Sources:
   LFO 1/2, Filter Env, Mod Env, Velocity, Key, Mod Wheel, Aftertouch, per-note
   Random. Destinations: Pitch, Pitch B, Filter, Resonance, Morph A/B, FM, Amp,
   Pan, LFO rates, B Level, Noise.
5. **SPACE** — effects. Core knobs on the panel, the rest behind each
   section's **more** button:
   - *Chorus*: Chorus / Ensemble (string machine) / Flanger / Dimension modes;
     more: voices, feedback, stereo spread, tone.
   - *Delay*: Stereo / Ping-Pong / Tape modes, tempo sync (1/16 … 1/2, dotted,
     triplet) or free time; more: tone and high-pass in the feedback path, tape
     wow, stereo width.
   - *Reverb*: Dattorro-style plate tank with Room / Plate / Hall / Shimmer
     types; more: pre-delay, low cut, high cut, tail modulation, octave-up
     shimmer amount, width.

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
leaf to hear it, drag it outward for wilder children, double-click to plant it
(it becomes the seed and a new generation grows), right-click for save/breed.
Double-click the seed to evolve what you're hearing; right-click it for fresh
ideas. The **LIST** tab is the same batch with full descriptions. Every finished
generation is also saved under `Library/Generations/Gen N - HH.MM`.

1. **Fresh ideas** — ten new patches from the direction text alone. **Evolve** —
   ten descendants of your ♥ favourites (or of the sound you're playing, if you
   have none). The sound you're playing is never replaced; it stays pinned at
   the top as *Now Playing*.
2. Click a card to load it and play; click its ♥ to keep it as a parent.
3. Type a direction ("darker", "more movement", "plucky") to steer the next round.

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
  On an M4 with 16 GB the 4B model runs at ~30 tokens/s: a fresh patch every
  ~13 s, an evolved one every ~5 s. The model is unloaded after 10 idle minutes.
- *Ollama app* — the same models served by a running [Ollama](https://ollama.com).
  Handy for experiments; friends don't need it.

Per batch the AI designs 5 patches that stream into the **✦ AI IDEAS** section
as they arrive, while the random breeder's 5 land in a folded **RANDOM
VARIATIONS** section underneath.

**Library**: the LIBRARY tab browses `~/Music/Stacks Patches`. Make folders,
open them, click a patch to load it. The ♥ on any card (or on Now Playing)
saves that patch into the open folder *and* adds it to "Breeding from", the
set Evolve works from. A patch's "..." moves it to another folder or trashes it.

The engine choice is stored in `~/Library/Application Support/Stacks/`, next to
`llama.log` (runtime + speed stats), `last-ai-prompt.txt`, `last-ai-reply.txt`
and `grammar.gbnf` for prompt tuning. Patches are plain JSON (`Save…` / `Load…`,
default folder `~/Music/Stacks Patches`).

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
  Wavetable.*      10 band-limited morphing wavetables, built at startup
  SynthVoice.*     one voice: 2 wavetable oscs (B can FM A), sub, noise, ladder filter, 2 env, 2 LFO, unison
  Patch.*          a named set of parameter values; JSON in/out; apply/capture
  PatchGenerator.* generators: Random (archetypes + crossover/mutation)
  ai/LlmBackend.h  interface for "a model that streams a chat reply"; OllamaBackend.cpp speaks HTTP to Ollama
  ai/LlamaBackend.* the same interface on top of llama.cpp (Metal), loads a GGUF inside the plug-in
  ai/ModelManager.* model catalogue, Ollama-copy detection, background downloads
  ai/LlmPatchGenerator.* prompt + GBNF grammar built from the parameter table, streaming JSON parser, fallback to Random
  PluginProcessor.* audio engine, master FX, lab state, engine selection, background generation
  PluginEditor.*   window: header, knob panel, AI lab, keyboard
  SynthPanel.*     knob/drop-down panel generated from the parameter table
  LabPanel.*       generate → audition → ♥ → evolve UI
```
