# AI benchmarks

Run `StacksTests --bench <label>` after changing the prompt, grammar or guard. Score is 0-100 over delivery, movement, designed tables, hint adherence, in-key output before the guard, evolve diversity and speed (see Tests/Tests.cpp).

| date | label | model | score | notes |
|---|---|---|---|---|
| 2026-10-04 15.47 | baseline: units + directions + 12k ctx | Qwen3 4B | 99.6 | fresh 3/3 3/3 3/3 3/3 3/3 |
| 2026-10-04 18.24 | tags + explain era: 3 tags per patch, URL models | Qwen3 4B | 98.5 | fresh 3/3 3/3 3/3 3/3 3/3 |
| 2026-10-04 21.13 | warps, cues, factory | Qwen3 4B | 98.8 | fresh 3/3 3/3 3/3 3/3 3/3 |
| 2026-10-04 23.41 | osc C, filter 2, sampler, MTS | Qwen3 4B | 95.5 | fresh 3/3 3/3 3/3 3/3 3/3 |
| 2026-10-04 23.53 | rerun: osc C, filter 2, sampler, MTS | Qwen3 4B | 96.5 | fresh 3/3 3/3 3/3 3/3 3/3 |
| 2026-10-19 00.00 | 2026-10-18 after routing | Qwen3 4B | 98.4 | fresh 3/3 3/3 3/3 3/3 3/3 |
| 2026-10-31 16.55 | 2026-10-31 grow modes | Qwen3 4B | 97.4 | fresh 3/3 3/3 3/3 3/3 3/3 |
| 2026-11-14 22.48 | hp guard on struck sounds | Qwen3 4B | 97.2 | fresh 3/3 3/3 3/3 3/3 3/3 |
| 2026-11-15 04.24 | exp envelopes, ramped ladder 2x, mip blend, scatter, filter guards | Qwen3 4B | 97.6 | fresh 3/3 3/3 3/3 3/3 3/3 |
| 2026-11-15 14.45 | round two: zdf ladder, steal fade, unison stack, built-in tables, two recipes, reverb by kind, band-limited warps | Qwen3 4B | 96.9 | fresh 3/3 3/3 3/3 3/3 3/3 |
| 2026-11-15 14.57 | round two rerun, idle machine | Qwen3 4B | 97.6 | fresh 3/3 3/3 3/3 3/3 3/3 |
