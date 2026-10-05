# Serum 2 vs Stacks (October 2026)

From a delegated research pass over Xfer's site and manual, MusicTech, MusicRadar, KVR and the Xfer forum. "NC" = not confirmed by sources.

## Serum 2 in brief

- Version 2.0 (March 2025), point releases to 2.1.x in 2026 (K35 filter, theme.json skinning, pop-out sample editor), Linux VST3 beta (August 2026). $249, lifetime updates, free for Serum 1 owners, rent-to-own via Splice. VST3/AU/AAX, no standalone, no CLAP.
- Oscillators: three, each Wavetable / Sample / Multisample (SFZ) / Granular (256 grains) / Spectral (resynthesis, PNG import), plus Sub and Noise. Dual warp per oscillator (sync, bend, PWM, FM, PD, AM, RM, per-osc filter and distortion). Unison up to 16 voices. Wavetable editor (draw, FFT, formula, audio import). 626 factory presets, 288 wavetables.
- Filters: two, series or parallel, per-oscillator routing and sends; ~90 modes carried over plus PZSVF (draw the response), DJ filter, MG ladder, comb 2, diffuser, vintage models, K35.
- Modulation: 4 envelopes, 6–10 LFOs (drawable, chaos, S&H, 2D path), 8 macros, drag-and-drop matrix with curve remapping, oscillators and filters as audio-rate sources, MPE sources, live modulation arcs on knobs.
- Effects: 13 in a modular rack with multiple instances (hyper/dimension, distortion, flanger, phaser, chorus, delay, compressor, reverb with new algorithms, EQ, filter, Bode shifter, convolution, utility) plus three splitters, two aux busses.
- Performance: arpeggiator with 12 patterns, a clip sequencer with MIDI out and audio drag-out, MPE, MTS-ESP tuning.
- UI: resizable, themes, mixer page with signal flow and meters, undo/redo, tooltips. Browser with tags, ratings, audition previews. No cloud.

## Gap table

| Serum 2 feature | Stacks | Solo-dev call |
|---|---|---|
| 3 oscillators | partial (2) | cheap, moderate value |
| Wavetable warps (sync, bend, PWM, mirror, fold), dual warp | yes: Sync, Bend, PWM, Mirror, Fold, Quantize per oscillator (one warp each), shown in the Shape cell | done 2026-10-04 |
| 16-voice unison with WT-pos spread | yes: up to 16 voices, Uni Morph spreads the table position | done 2026-10-04 |
| Sample / multisample / granular / spectral oscillators | no | expensive; the AI spectral-recipe tables cover part of the "design by spectrum" case |
| Wavetable editor | no | a harmonic-bar editor is cheap and pairs with the recipe tables |
| Two filters, series/parallel, per-osc routing | no (one ladder) | medium, high value |
| Big filter set (comb, formant, notch, vintage) | LP/HP/BP 12/24 plus Notch, Comb, Formant | done 2026-10-04 (vintage models open) |
| 8 macros | 6 named macros, wired per patch by the model or defaults, MACROS page first | done 2026-10-04 |
| Drag-drop matrix, bypass, curve remap, audio-rate sources | partial (12 fixed slots, drag-to-knob) | bypass + curves cheap; audio-rate skip |
| Live modulation arcs on knobs | rings + live marker, calm mode toggle | partial |
| 13-effect modular rack, aux busses, splitters | distortion, EQ, chorus, delay, reverb, compressor in a fixed order | effects done 2026-10-04; rack order and busses open |
| Bode shifter, convolution | no | low priority |
| Arpeggiator | yes: up/down/up-down/random/as-played, synced rates, octaves, gate, swing | done 2026-10-04 |
| Clip sequencer with MIDI out | no | expensive; skip |
| MPE | yes: Settings > MPE, 48 st member bend, pressure, Slide (CC74) source | done 2026-10-04 |
| MTS-ESP microtuning | no | needs the ODDSound client vendored (a download to approve) |
| Resizable vector UI, theme file | yes: resizable vector UI, theme.json in the library folder | done 2026-10-04 |
| Undo / redo | yes: every load, drag and knob gesture | done 2026-10-04 |
| Tagged browser with ratings and audition previews | folders, hearts, tags on every preset, audition phrase on click | done 2026-10-04 (ratings beyond hearts open) |
| Factory library (626 presets, 288 tables) | 85 AI-generated presets in 8 categories, each with its own designed table, tags and a prompt-cue pass | done 2026-10-04; grows with every run |
| SFZ / IR / PNG / MIDI import | partial (WAV tables) | skip for now |
| Windows / AAX / Linux | Windows VST3/Standalone builds in CI (untested on hardware); AAX needs the Avid SDK | partial |

## What matters most day to day

1. A factory library with tags and one-click audition. It is what most people touch first.
2. Macros and visible modulation feedback. That is how patches get performed.
3. Effects breadth: distortion, EQ, compressor and a second filter are expected on every bass and lead.
4. An arpeggiator: cheap relative to its perceived value. Skip the clip sequencer.
5. Undo/redo.

## What Stacks has that Serum 2 does not (none found in any source)

- The AI Lab: prompt to patch, evolve, explain. "Sound design by conversation" instead of a preset browser.
- Fully offline local model: no cloud, no account, no subscription, no latency.
- Explain: the only synth that says why a patch sounds the way it does and which knob to touch. Serum's steep curve is a recurring review complaint.
- The family tree: version control for sound. Serum has undo, not lineage.
- The tuner with auto-tune: instant pitch correction, where Serum's wavetable editor makes you do it by ear.
- AI-designed spectral-recipe tables: designed harmonic content without the CPU cost of a spectral oscillator, which is reviewers' main complaint about Serum 2.
- The live garden: drag a leaf to blend a sound toward or past its parent in real time.
