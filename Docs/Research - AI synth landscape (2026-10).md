# AI-assisted synths: the landscape and what Stacks can own

Research pass, 4 October 2026 (web sources 2024–2026). Summarised from a
delegated research run; URLs inline for the key claims.

## Direct competitors and neighbours

- **Sonic Charge Synplant 2** (€149): seed-and-branches UI, Genopatch audio-to-patch. Praised as genuinely new; criticised for a tag-less browser, fiddly DNA editing, CPU, and fidelity on acoustic sources. https://musictech.com/reviews/plug-ins/sonic-charges-synplant-2-review/
- **Xfer Serum 2** ($189/$249): spectral/granular/sample oscillators, arp and clip sequencer, animated modulation on targets ("more visual feedback than any other synth"). No AI. https://musictech.com/reviews/software-instruments/xfer-records-serum-2-review
- **Arturia Pigments 7** ($199, often $99): audio-reactive Play page with quick-edit macros and in-app tutorials. No generative AI. https://www.soundonsound.com/news/arturia-announce-pigments-7
- **Vital**: text-to-wavetable in Pro; development considered abandoned, freemium did not fund it. https://www.kvraudio.com/forum/viewtopic.php?p=9116295
- **Kilohearts Phase Plant**: 8 macros, group-and-hide modules so deep patches present a small face. https://kilohearts.com/docs/phase_plant
- **Dawesome Myth** ($179): ML resynthesis from a sample, randomiser with frozen sections, timbre-as-colour UI. https://synthanatomy.com/2024/11/dawesome-myth-new-synthesizer-takes-you-on-an-ai-supported-resynthesis-journey.html
- **u-he Zebra 3** (2026): vector UI, colour-coded modulation graph; beta complaints about label contrast. https://synthanatomy.com/2026/04/u-he-zebra-3-modular-synthesizer-plugin.html
- **Text/audio-to-preset tools**: FADR SynthGPT ("a sample search with AI support", not in key), GUK.ai Sistema2 (prompt-to-patch, subscription disliked), MicroMusic Replicate (audio-to-preset for Vital/Serum, simple results), sonicLAB Fundamental 4.1 (Claude/Gemini write presets over MCP, cloud only), Sound Radix Radical1 (ChatGPT edits a known JSON template, which validates the grammar approach), DX7 AI Studio, SynthPilot, Tensynth (patch-aware MIDI auditioning), Preset Mutator, Natural Selection S (preset family trees).
- **Academic**: SynthScribe (CLAP search + genetic crossover), CTAG (ICML 2024), LLM4FM (ICML 2026, text and audio to DX7), InverSynth II, DiffMoog, Text2FX.

## What users say is worth paying for

Animated modulation on targets (with a way to turn it off), an audio-reactive overview, browsers that tag the *user's* presets, semantic macros (Brightness, Timbre, Time, Movement), constrained randomisers, an arp/sequencer, honest audio-to-patch, and an "expensive" UI: consistent knob spacing, vector/HiDPI, colour as meaning, restrained motion, high label contrast, designed for the screen rather than fake hardware.

Recurring gripes about AI rivals: cloud and subscription dependence, generic or off-key results, no transparency, sample tools posing as synths, fidelity marketing that under-delivers.

## Gaps an offline LLM lab can own (value x feasibility)

1. Explain the patch in words: why it sounds like that and which knob to touch. Nobody ships this.
2. Conversational refinement ("more like X, less Y") with memory. Only Fundamental 4.1 does it, cloud-only.
3. Design intent and genealogy: keep the prompt, parent and diff with every leaf; show a family tree.
4. Offline, no subscription, no credits: already Stacks' architecture; lead with it.
5. Teaching mode: tutorials generated from the user's own patch.
6. Patch-aware auditioning: a demo phrase chosen per patch.
7. Audio-to-patch framed as "inspired by" with a similarity meter, not cloning.
8. Reference-track style transfer: defer.
9. Seed cards: patch + intent + lineage as small JSON to share.

## Beginner vs advanced patterns worth borrowing

A semantic macro layer first with the full engine one click deeper; an audio-reactive Play view with 4–6 quick-edit parameters; group-and-hide for complex AI patches; use-case tutorials in-app; visual feedback that experts can switch off; colour as meaning; a "Garden only" mode for beginners.

## Evaluating AI presets

Published practice: parameter error, multi-scale spectral distance, MFCC, envelope similarity, FAD, CLAP text-audio alignment, listening tests (7-point Likert), LLM-judge checks. Practical plan for Stacks: a fixed prompt set in tiers (single descriptor, compound, referential, adversarial); validity (schema pass, renders, non-silent, level); semantic alignment (CLAP, timbral descriptors moving the right way); diversity among leaves and distance from the seed; edit fidelity ("darker" moves brightness monotonically); explanation accuracy; a quarterly human ABX. The current `StacksTests --bench` covers validity, movement, designed tables, hint adherence by category, in-key output before the guard, evolve diversity and speed; CLAP/timbral scoring is the next step.

## Top 10 recommendations

1. Make "why it sounds like this" a first-class panel.
2. Keep every leaf's prompt, parent and diff and render a family tree.
3. Lead with "offline, no subscription, no credits".
4. Add a Play view with 4–6 semantic macros the model pre-maps per patch.
5. Build a tagging browser that tags user presets, with LLM auto-tagging.
6. Animate modulation on targets with a global "calm" toggle.
7. Frame audio-to-patch as "inspired by" with a similarity meter.
8. Patch-aware auditioning: a demo phrase per patch.
9. Keep the template-plus-grammar strategy and make schema-valid rate a top-line KPI.
10. Automate CLAP + timbral + validity benchmarks on a 60-prompt set before every release.
