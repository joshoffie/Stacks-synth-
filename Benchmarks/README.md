# AI benchmarks

Run `StacksTests --bench <label>` after changing the prompt, grammar or guard. Score is 0-100 over delivery, movement, designed tables, hint adherence, in-key output before the guard, evolve diversity and speed (see Tests/Tests.cpp).

| date | label | model | score | notes |
|---|---|---|---|---|
| 2026-10-04 15.47 | baseline: units + directions + 12k ctx | Qwen3 4B | 99.6 | fresh 3/3 3/3 3/3 3/3 3/3 |
| 2026-10-04 18.24 | tags + explain era: 3 tags per patch, URL models | Qwen3 4B | 98.5 | fresh 3/3 3/3 3/3 3/3 3/3 |
| 2026-10-04 21.13 | warps, cues, factory | Qwen3 4B | 98.8 | fresh 3/3 3/3 3/3 3/3 3/3 |
| 2026-10-04 23.41 | osc C, filter 2, sampler, MTS | Qwen3 4B | 95.5 | fresh 3/3 3/3 3/3 3/3 3/3 |
