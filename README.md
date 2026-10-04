# Stacks

A hybrid wavetable/FM synthesizer for Logic Pro (AU), VST3 hosts and
standalone, with an on-board AI lab: it proposes patches, you pick what you
like, and it breeds the next generation from your picks.

## Requirements (macOS, Apple silicon)

- Xcode 26 (installed; the command line tools alone also work)
- JUCE 8 at `/Applications/JUCE` (installed)
- CMake ≥ 3.22 and Ninja: `brew install cmake ninja`

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

## The AI Lab

1. **New batch** — ten candidate patches. **Evolve** — ten descendants of your
   favourites (or of whatever you are hearing, if you have no favourites yet).
2. Click a card to load it and play; click its ♥ to keep it as a parent.
3. Type a direction ("darker", "more movement", "plucky") to steer the next round.

**Engine** picks who designs the patches:

- *Random (no AI)* — archetype-based random patches, crossover and mutation.
  Instant, always available.
- *Ollama: &lt;model&gt;* — a local language model served by the
  [Ollama](https://ollama.com) app. Per batch the model designs 5 patches that
  stream in at the top of the list (tagged ✦ AI) while the random breeder fills
  the other 5 instantly. Needs Ollama running with a model pulled, e.g.
  `ollama pull qwen3:4b`. On an M4 with 16 GB, `qwen3:8b` produces one patch
  every ~40 s; `qwen3:4b` is roughly twice as fast.

The engine choice is stored in `~/Library/Application Support/Stacks/`.
Patches are plain JSON (`Save…` / `Load…`, default folder `~/Documents/Stacks Patches`).

## Layout

```
Source/
  Parameters.*     the parameter table: ids, ranges, defaults, groups, AI hints
  Wavetable.*      10 band-limited morphing wavetables, built at startup
  SynthVoice.*     one voice: 2 wavetable oscs (B can FM A), sub, noise, ladder filter, 2 env, 2 LFO, unison
  Patch.*          a named set of parameter values; JSON in/out; apply/capture
  PatchGenerator.* generators: Random (archetypes + crossover/mutation)
  ai/LlmBackend.h  interface for "a model that streams a chat reply"; OllamaBackend.cpp speaks HTTP to Ollama
  ai/LlmPatchGenerator.* prompt built from the parameter table, streaming JSON parser, fallback to Random
  PluginProcessor.* audio engine, master FX, lab state, engine selection, background generation
  PluginEditor.*   window: header, knob panel, AI lab, keyboard
  SynthPanel.*     knob/drop-down panel generated from the parameter table
  LabPanel.*       generate → audition → ♥ → evolve UI
```
