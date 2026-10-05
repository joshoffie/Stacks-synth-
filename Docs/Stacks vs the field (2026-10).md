# Stacks vs the field (October 2026)

What Stacks offers next to the synths producers actually buy, and next to the
AI sound tools that charge by the month. Desk research on 4 October 2026 plus
what is in this repository today. Prices are list prices found that day and
move often; "unverified" marks claims taken from one source.

## The field in one table

| Product | What it is | AI | Runs | Price model |
|---|---|---|---|---|
| **Stacks** | hybrid wavetable + FM + sample/granular synth, AU/VST3/Standalone (Windows build in CI) | on-board language model (Qwen3 4B/8B or any GGUF) writes whole patches from a prompt, evolves them, explains them, designs wavetables | on the Mac, offline, nothing leaves the machine | one-off, no account, no subscription (price to be set) |
| Xfer Serum 2 | the reference wavetable synth: 3 oscs with sample/granular/spectral modes, 2 filters, 13-effect rack, clip sequencer | none | local | $189 intro, $249 after; free for Serum 1 owners |
| Vital / Vital Pro | spectral-warping wavetable synth, text-to-wavetable | text-to-wavetable only (speech synthesis to table) | local | free; Plus $25; Pro $80; or $5/month |
| Arturia Pigments 6 | multi-engine synth (wavetable, virtual analogue, sample/granular, harmonic, utility) | none | local | $199 |
| Kilohearts Phase Plant | modular semi-patchable synth with the Kilohearts effect ecosystem | none | local | $199 |
| Sonic Charge Synplant 2 | genetic sound-design synth; Genopatch clones a loaded sample into a patch | sample-to-patch (Genopatch); PhenoType: a free local text-to-patch script with ~210 tags, no language model | local | one-off (around $149, unverified) |
| guk.ai Sistema2 | synth whose patches come from a cloud AI prompt | cloud text-to-patch, AI effect chains | needs the server | $149 or $10/month rent-to-own; 3-day trial |
| FADR SynthGPT 2 | plug-in that turns a text prompt into a playable instrument | cloud generation of original samples from text | needs FADR's engine | FADR Plus $10/month |
| DeepSynthAi (iOS) | AI that writes step-by-step patch instructions for any synth | cloud text to instructions | phone app | app store |
| micromusic Replicate | converts a sample into Vital/Serum presets with machine learning | sample-to-patch for other synths | local (unverified) | one-off |
| Segment Palora | synth with readable JSON patches and a brief for external AI assistants to write them | none inside the plug-in; you paste patches from a chat assistant | local synth, external AI | one-off |
| Neutone Morpho | real-time neural timbre transfer effect | neural resynthesis, trainable models | local | $99 plus paid models |
| Output Co-Producer | AI sample finder on the master bus matching your mix to a marketplace | matching, not synthesis | cloud | $9.99/month |
| Splice Craft / Magic Fit | AI that turns samples into instruments and fits them to the session | sample transformation | cloud | Splice subscription |
| Unison Plugin Pass | eight AI generators for melodies, chords, drums, bass, sounds | generative MIDI/sound tools | mixed | $19.99/month |

## What only Stacks does (checked against the code)

1. **A language model inside the plug-in, offline.** Every other text-to-patch
   product either calls a server (Sistema2, SynthGPT, DeepSynthAi) or has no
   model at all (PhenoType's tag matcher, Palora's paste-from-chat). Stacks runs
   llama.cpp with grammar-constrained decoding, so the model can only write a
   valid patch, and nothing is uploaded. No account, no monthly fee, works on a
   plane.
2. **The model designs wavetables, not just knob values.** Each patch can carry
   a 16-harmonic, multi-frame spectral recipe that becomes the Custom table.
   Vital's text-to-wavetable speaks a word; Stacks' tables are composed to fit
   the brief, and the breeder mutates them.
3. **Evolution with a family tree.** Generate, listen, Evolve from the one you
   like, see the lineage, drag a leaf to morph between parent and child in real
   time. Synplant has genetic breeding of its own patches; nobody combines it
   with a prompt-driven model and a history you can walk back through.
4. **Explain.** A "why it sounds like this and what to touch" readout for any
   patch, specific to its settings. No competitor explains patches in words.
5. **Prompt cues and a tuning guard.** What the prompt names ("lush chorus",
   "with delay", "wide") is honoured even when the model forgets, and every
   patch stays in key (octave snaps, pitch-LFO caps).
6. **Open model choice.** Qwen3 4B out of the box, 1.7B or 8B on request, or any
   GGUF chat model or Ollama copy. A cloud product cannot offer that.
7. **Measured.** `StacksTests --bench` scores the model's output (validity,
   connections, diversity, evolve distance) so features never silently cost
   quality: 99.6, 98.5, 98.8 across the last three builds.
8. **Beginner to expert on one screen.** Six named macros wired per patch,
   plain-language help for every control, audition on click, then the full
   synth underneath (3 oscillators with warps, sample/granular, 2 filters, 20
   modulation slots, MPE, MTS-ESP microtuning, theme file).

## Where the others are ahead (honest)

- **Engine depth and polish.** Serum 2's effect rack, clip sequencer and
  spectral oscillator, Phase Plant's modularity and Pigments' five engines are
  deeper than Stacks' fixed signal path. Their UIs have had years of refinement.
- **Preset count.** Serum 2 ships 626 presets and 288 tables; Stacks ships 85
  AI-generated presets. Stacks can grow its library in an afternoon with
  `StacksTests --factory`, but today the number is small.
- **Sample-to-patch.** Synplant's Genopatch and Replicate turn a recording into
  a patch. Stacks goes text-to-patch and sample-into-the-synth (the sample
  oscillator), but does not yet analyse a recording into a synth patch.
- **Model size.** A 4B model on a laptop writes good patches but is not a
  frontier model; cloud products can throw far bigger models at the problem.
  Stacks mitigates with the grammar, the brief pre-pass, the cues and the guard.
- **Platforms.** Windows builds in CI but is untested on hardware; AAX needs
  Avid's SDK; no Linux. Serum, Pigments and Phase Plant ship everywhere.
- **Ecosystem.** Tutorials, preset packs, community. Stacks has none yet.

## Pricing position

The subscription AI tools cluster at $10-20 a month ($120-240 a year) and stop
working when the server or the payment stops. Sistema2 is the direct comparable
and costs $149 or $10 a month for a cloud synth of modest sound. The serious
synths are $189-249 one-off with no AI. A one-off price between $59 and $99 for
Stacks ("a real synth with the AI built in, nothing to subscribe to, nothing
uploaded") undercuts the synths and beats the subscriptions inside a year while
making the offline story the headline. A free tier limited to the random
breeder (no model) would still be a usable synth and a wide funnel.

## Five moves that would widen the lead

1. **Sample-to-patch.** Analyse a dropped recording (spectral envelope, attack,
   decay, pitch stability) into a brief plus a WaveSpec, then let the model
   finish the patch. Closes the one AI feature Synplant and Replicate have.
2. **Nightly factory growth.** Run `--factory` on a schedule with new briefs
   and ship 300+ presets within a month; add star ratings and sort by rating.
3. **Harder benchmark.** Add a perceptual score (a CLAP-style text-audio
   similarity or timbral descriptors) so "does it sound like the prompt" is
   measured, not just validity and diversity.
4. **Windows on hardware.** One session on a Windows machine to run the CI
   artefacts and fix what breaks; then Stacks is the only offline AI synth on
   both platforms.
5. **Community packs.** Patches are small JSON files with a lineage: a shared
   folder or a one-click export of a family tree would spread quickly.

## Sources

- [AudioCipher: text-to-instrument tools](https://www.audiocipher.com/post/text-to-instrument), [AudioCipher: FADR SynthGPT vs Synplant](https://www.audiocipher.com/post/fadr-synthgpt-synplant)
- [FADR SynthGPT 2](https://fadr.com/blog/synthgpt2), [SynthGPT plug-in docs](https://fadr.com/docs/synthgpt-plugin)
- [Sistema2 at Plugin Boutique](https://www.pluginboutique.com/products/10986-Sistema2), [Gearnews on Sistema's model](https://www.gearnews.com/sistema-the-ai-driven-software-synth-that-charges-for-patches), [Rekkerd on Sistema pricing](https://rekkerd.org/?p=225631)
- [MusicTech: Synplant 2 review](https://musictech.com/reviews/plug-ins/sonic-charges-synplant-2-review/), [MusicRadar: Synplant 2 review](https://musicradar.com/reviews/sonic-charge-synplant-2-review)
- [Sonicstate: PhenoType text-to-patch for Synplant 2](https://www.sonicstate.com/news/2026/06/11/generate-synplant-patches-from-text/), [KVR: PhenoType release](https://www.kvraudio.com/news/sonic-charge-releases-phenotype---free-text-to-patch-generator-for-synplant-2-67348)
- [Dubspot: Palora](https://blog.dubspot.com/plugins/palora)
- [DeepSynthAi on the App Store](https://apps.apple.com/us/app/deepsynthai/id6745809349), [Replicate by micromusic at KVR](https://www.kvraudio.com/product/replicate-by-micromusic)
- [MusicTech: Serum 2 pricing](https://musictech.com/news/gear/serum-2-everything-you-need-to-know/), [Dubspot: Vital](https://blog.dubspot.com/plugins/vital), [Dubspot: Pigments 6](https://blog.dubspot.com/plugins/pigments-6), [Dubspot: Phase Plant](https://blog.dubspot.com/plugins/phase-plant)
- [Neutone Morpho pricing](https://thedailyworkflow.com/service/Neutone%20Morpho), [Qosmo: timbre transfer](https://qosmo.jp/en/products/timbre-transfer/)
- [Output Co-Producer](https://getcoai.com/tool-detail/output/164483/), [Music Business Worldwide: Splice AI tools](https://www.musicbusinessworldwide.com/splice-launches-ai-tools-that-compensate-sample-creators/)
- [Rekkerd: Unison Plugin Pass](https://rekkerd.org/unison-audio-launches-plugin-pass-subscription/), [MusicTech: Beatport Studio](https://musictech.com/news/beatport-studio/)
