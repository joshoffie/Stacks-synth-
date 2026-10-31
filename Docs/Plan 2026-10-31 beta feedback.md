# Plan: first beta feedback (2026-10-31)

Notes from the first tester, turned into work. Each item says what was found,
what we will build, how it will be judged done, and roughly how long it takes.
Order of work is at the end.

## 1. Patches don't differ enough (the big one)

**What was found.** Growing from a sound, even on Far, gives near-copies with a
few numbers nudged. Not new ideas that are still related.

**Why, from the code.**

- The model is handed the parent as a full parameter dump and told to write
  "descendants" whose omitted values are filled in from the parent. That is a
  nudge machine by construction: a child is the parent plus a few overrides.
- Close, Medium and Far only change the wording of one sentence and a minimum
  count of changed settings (2 to 10 out of about 200). Nothing forces a
  *structural* change: a different wavetable family, filter type, envelope
  shape, modulation routing or effect chain.
- When a child comes back too close, the repair is random noise on the
  numbers (mutatePatch). Noise is not an idea.
- All five siblings come from one model call, so they lean the same way.
- The benchmark scores evolve by counting changed knobs, so it rewards exactly
  the behaviour the tester dislikes.

**What we build.** Variation becomes three different mechanics, not three
adverbs:

| Mode | Mechanic | What the model sees | Guarantee |
|---|---|---|---|
| Close | Nudge (today's path) | full parent values | 3 to 5 audible moves, nothing structural |
| Medium | Directed change | parent values, plus two named areas per child it must rebuild (source, filter, movement, space, envelope) | each child rebuilds its two areas, inherits the rest |
| Far | Reimagine | the parent as a *description* only: category, envelope class, three identity traits in words. No parameter dump, no parent fill-in | a new design that keeps the three traits |

- **Identity traits** are extracted by code from the parent (for example
  "slow attack, formant wavetable, wide chorus") so "inspired by" has a
  concrete meaning the model is told to keep.
- **Each child gets a contrasting direction** written before any settings
  ("darker and slower", "brighter with rhythm", "stripped to the bone"), so
  siblings pull apart instead of clustering.
- **A new distance metric** counts structural changes (choices, routings,
  effect chain, envelope class) separately from knob moves. Each mode has a
  target on both. A child under target is repaired with a *directed idea
  move* from a library (swap wavetable family, change filter type and
  envelope together, add a movement routing, reorder the chain) rather than
  noise.
- **Multiple parents** (see item 8) are described trait by trait, and each
  child says which parent each trait came from.
- **Sampling temperature** follows the mode: Close cool, Far hot.

**Done when** the benchmark's evolve cases report, for Far, at least eight
structural changes from the parent and at least five between any two siblings,
while the parent's three traits survive in all five, and the overall score
stays at or above 98. New factory presets are grown again with the new path
so the library benefits.

**Effort.** Two days. The model runs at about two minutes per batch, so the
benchmark loop is the slow part.

## 2. Life

**What was asked.** A setting that lets the sound slowly change as it plays.
Off snaps back to the exact preset. Levels 1 (subtle) to 5 (a lot).
Algorithmic, not the engine.

**What we build.** `Life` is an offset layer, never a change to the preset.

- A smooth noise field (sums of slow sines at unrelated rates, plus a small
  kick on every note so playing moves it more than idling) drives about
  fourteen targets: oscillator morphs, cutoff, resonance, filter envelope
  amount, detune, unison spread, LFO rates, chorus, delay and reverb mix,
  drive, warmth, attack and release.
- Level sets how far: 1 about 2 percent of range, 2 about 5, 3 about 10,
  4 about 18, 5 about 30, and nudges the rates up a little.
- It is applied where modulation is already summed, so the knobs show the
  movement with the live markers that exist today, and the saved values are
  untouched. Off means zero offset: the preset exactly.
- Control: a segmented `Life` switch (Off 1 2 3 4 5) on the Home results
  screen and in the Synth header. It is a real parameter, so it automates
  and saves with a preset.

**Done when** Off is bit-identical to the preset, level 5 is clearly alive
but never out of tune or clipping, and a unit test checks the offsets stay
inside their ranges over ten minutes of simulated playing.

**Effort.** One day.

## 3. Too big across the screen

**What was found.** 1500 by 860 is wider than a 13 inch MacBook screen. The
window resizes today, but it reflows instead of shrinking, and the minimum is
still 1280 wide.

**What we build.** The whole interface scales as one picture. Drag the corner
or pick Small, Medium, Large or Fit screen from the gear menu. Range 60 to
150 percent, aspect locked. First open fits the screen it is on. The choice is
remembered per user and travels with the session.

**Done when** the plug-in opens fully visible on a 13 inch MacBook and at
every size the knobs, text and tour land where they should.

**Effort.** Half a day.

## 4. No way to end the helper early

**What we build.** Two things, since "helper" can mean either:

- The tour gets a close cross and answers Escape, on every step.
- While sounds are growing, Start over becomes Stop and cancels the batch
  (the engine already supports cancelling); in the Lab, Evolve turns into
  Stop for the same reason.

**Effort.** An hour.

## 5. Show the settings being written

**What was asked.** See the parameters being added in real time on the main
screen while a sound grows.

**What we build.** The engine streams its answer and we already read the
half-written text for the progress bar. We will read each finished
`setting: value` pair as it arrives and:

- show a live ticker under the block being built ("Cutoff 1.2 kHz", "Chorus
  Mix 35%", "Wavetable: Glassy Bell"),
- draw the block's wave preview from the settings so far, so it takes shape,
- on the Synth screen, flash each knob as it is written, with a ghost marker
  at the new value, using the flash that already exists for tweaks.

Nothing is applied to the sound until a block is clicked.

**Effort.** Half a day.

## 6. Sections as toggles instead of tabs

**What we build.** The Synth page tabs (Macros, All, Sound, Filter, Effects,
Modulators) become toggle chips. Any mix can be on. The rows that are on share
the space and scale up to fill it; the layout already scales rows to fit, so
this is a change of control, not of layout engine. The chips are remembered.

**Effort.** Half a day.

## 7. The tree replaces the falling stack

**What was asked.** Griffin's drawing: generations as columns, children in a
column joined to their parent by bracket connectors, a child that gets grown
from becomes the parent of the next column, and two selected parents join
into one bracket. Make it infinite: drag the canvas to see every generation,
click any node to play it. The History idea moves to the front.

**What we build.** One `TreeCanvas` used on Home and in the Lab (it replaces
both the Stacks garden and the History tab).

- Columns left to right, one per generation. Column 0 is the seed: the kind
  tile or the preset it started from.
- Nodes are the blocks, smaller. New ones still drop into place.
- Connectors are rounded elbows from each parent to a bracket that fans into
  its children. Two parents merge into one bracket.
- Drag empty space to pan, scroll or pinch to zoom, a Home button re-centres
  on the newest column. The canvas has no edge.
- Click a node to hear it. Hover for its description. The node being heard
  is lit; nodes already saved show a mark.
- Every generation record gains explicit parent links so connectors are
  exact. Old records fall back to matching the seed by name.

**Done when** a session of six generations with two merges reads clearly at
a glance, panning is smooth with fifty nodes, and clicking any node in any
column plays it.

**Effort.** Two days. The biggest change in this list.

## 8. Multi-select presets to grow from

**What we build.** On the tree and in the Library:

- Click plays and selects one, as now.
- Cmd-click, or the small `+` that appears in a node's corner on hover, adds
  it to the mix. Chosen nodes wear a ring.
- A tray above Grow lists the mix ("Warm Glass + Vocal Hum", each with a
  cross). The button reads "Grow from these 2".
- The engine gets all parents, described trait by trait, and each child says
  which parent gave it what. Item 1's Far mode handles the rest.

**Effort.** Half a day on top of item 7.

## 9. Rosetta (nice to have)

**What was found.** A Logic running under Rosetta cannot see Stacks because
the plug-in is Apple silicon only.

**Options.** A universal build adds an Intel slice. Under Rosetta the engine
would run on the CPU only, so growing sounds would take several times longer,
and the download grows. Recommendation: leave it out of the beta and keep the
tester note that says to turn Rosetta off; revisit if more than one tester
needs it.

**Effort.** One day plus testing on a Rosetta Logic.

## Order of work

1. Quick wins first, so the next build already feels better: items 4, 3, 5.
2. Item 1, the diversity deep dive. Benchmark before and after.
3. Item 2, Life.
4. Items 7 and 8 together, the tree and the mix tray.
5. Item 6, the section toggles.
6. Item 9 only if asked.

Each step ends with the test suite, the smoke test on an empty home folder,
and a 0.1.x update package the tester can install over the last one.
