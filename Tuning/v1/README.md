# Tuning session v1 - 14 Nov 2026

Rater: Josh, on the standalone, build 0.1.3, the first rig (keys 1 / 2 / 3,
read by the report as 1 / 3 / 5). Files here:

- `report.md` - the report as `StacksTests --ratings` wrote it after the
  session (re-run any time with `StacksTests --ratings Tuning/v1`).
- `josh-2026-11-14.jsonl` - the raw log: 41 lines, 23 distinct sounds,
  18 ratings on 17 of them (one re-rated).

## What it says

Pads, almost entirely: 15 rated of 20 seen. 5 trash, 8 fine, 2 whoa
(33% trash, 13% whoa, mean 2.6 of 5). One bass (trash) and one lead (fine)
are too few to say anything.

Trash traits, all **watch** (no single trait is behind five trash yet, so
none reached ACT):

- synced LFO - 3 of 5 trash, 2 of 10 others
- release over 2 s - 3 of 5 trash, 2 of 10 others
- mid resonance - 4 of 5 trash, 4 of 10 others
- pairs: light FM + mid resonance (3 trash, 1 other); synced LFO + wet plate
  reverb (3 trash, 1 other)

Whoa traits: both whoa pads were `grow:far` children (2 of 2, against 6 of
13 others) - weak, and the kind of thing that must not become a rule.

By recipe: the grown sounds carried the session (`grow:far` 8 rated, mean
2.8, 38% trash, 25% whoa); the fresh engines had one rating each, so nothing
can be said about them yet.

## Decision

No change to the engine from v1. Next session: the 1-5 scale, twenty or so
pads again to see whether synced LFO / long release / mid resonance hold up,
and a fresh batch or two per other kind (pluck, bass, keys, lead) so the
recipes get rated.
