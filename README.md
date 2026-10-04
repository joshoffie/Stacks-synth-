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

## The AI Lab

1. **New batch** — ten candidate patches. **Evolve** — ten descendants of your
   favourites (or of whatever you are hearing, if you have no favourites yet).
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

Per batch the AI designs 5 patches that stream in at the top of the list
(tagged ✦ AI) while the random breeder fills the other 5 instantly.

The engine choice is stored in `~/Library/Application Support/Stacks/`, next to
`llama.log` (runtime + speed stats), `last-ai-prompt.txt`, `last-ai-reply.txt`
and `grammar.gbnf` for prompt tuning. Patches are plain JSON (`Save…` / `Load…`,
default folder `~/Documents/Stacks Patches`).

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
