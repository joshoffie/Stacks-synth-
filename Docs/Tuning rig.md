# The tuning rig

A way for us (not players) to rate what the engine makes, so the next build
generates closer to what we like. It ships inside the plug-in but stays
invisible until a file switches it on. Players never see it; what they get
is the result: guards, prompt lines and recipe weights baked into the build.

## Switch it on

    touch ~/Library/Application\ Support/Stacks/tuning.on

Relaunch Stacks (or Logic). A small **TUNING - 0 rated** pill appears in the
header, left of the Life switch. Delete the file to switch it off.

## A rating session

1. Pick a kind of sound on Home (or grow from a preset). Five sounds arrive.
2. Click a block to hear it, play a few notes.
3. Press a number. The pill flashes the verdict.

   | key | means | in words |
   |---|---|---|
   | 1 | trash | broken or unusable: thin, clicks, out of tune, nothing there |
   | 2 | meh | works but weak; you would not keep it |
   | 3 | fine | a plain sound that does its job |
   | 4 | good | you would keep it |
   | 5 | whoa | the one you never asked for and must keep |

4. Next block. Grow from a good one, pick another kind, keep going.

Rate the sound you hear, not the idea: a great concept that sounds thin is a
1 or 2, and a 5 is rare. Rate before you touch the knobs if you can; a rating
after edits is logged as edited. Rating the same sound again overrides the
earlier verdict. Do several kinds per session; the report is per kind and
needs at least six rated of a kind before it says anything, and about twenty
before it says anything worth acting on.

The first rig (14 Nov 2026, session v1) had 1 / 2 / 3; the report reads those
lines as 1 / 3 / 5.

## What is written

`~/Library/Application Support/Stacks/ratings/<mac name>-<date>.jsonl`, one
line per rating and one line (verdict 0) per sound the engine showed you,
with the whole patch, the recipe it was written to, the grow mode and
variation, the scale, and whether it was edited before rating. Click the pill
to open the folder. To send it in, zip the folder.

## The report

    build.nosync/StacksTests_artefacts/Release/StacksTests --ratings [folder or file ...]

With no argument it reads this Mac's folder; give it several folders to
merge testers. Per kind: the spread of verdicts, what the trash (1-2) shares
against the rest you rated, what the whoa ones (4-5) share, trait pairs the
trash shares, how each recipe fared (mean, trash rate, whoa rate), and the
median numbers of trash against the rest. Each trait is tagged **ACT** (at
least five trash, or four whoa, behind it and twice the rate of the rest) or
**watch** (not yet). It also writes `report.md` next to the data.

## What happens with it: nudging, not pulling

Nothing is automatic, and the model is never shown the liked patches. The
report points; a person changes one thing at a time:

- **Only ACT lines count.** A watch line waits for another session to agree.
- **Avoid beats imitate.** Trash findings become a guard in `enrichPatch`
  (a click is not a pluck) or an "avoid" line in the prompt's per-kind
  guidance. Whoa findings never become "do this" lines or examples; at most
  they raise a recipe's weight in `freshEngines()`.
- **A budget per round.** At most three changes per report. A recipe weight
  never drops below 40% of an even draw, and one slot in five stays a
  wildcard whatever the weights say, so the surprises keep coming.
- **One change, one commit,** named after the report line it came from, so
  it can come out on its own.
- **The next session is the test.** Same kinds, same scale. The trash rate
  has to fall and the whoa rate must not fall; `StacksTests --bench` must
  not get worse. A change that fails either comes out.

## The audio audit: the engine's own rig

The ratings say what the ear thinks; the audit says what the speakers get.
It plays every patch through the real voices and effects offline and measures
it, so an engine change can be judged in numbers before anyone listens:

    build.nosync/StacksTests_artefacts/Release/StacksTests --audit Factory --out build.nosync/audit/now.md
    build.nosync/StacksTests_artefacts/Release/StacksTests --audit Factory/Pad --wav build.nosync/audit/wavs

One note per patch (MIDI 60, velocity 0.8, held 1.5 s, 4 s in all, 48 kHz,
repeatable). Columns: RMS and peak in dBFS, onset (the first millisecond
against the peak), attack to 90%, release to -40 dB and its shape (0.2 is an
exponential tail, 0.5 a straight line), spectral centroid, energy above 5 kHz
and below 150 Hz, stereo width, and alias (oscillator A alone at C7, what lies
between the harmonics). Flags are thresholds, not verdicts. The reports of
15 Nov 2026 live in `Tuning/audit/` (before, after the engine, after the
factory passes); `Docs/Plan 2026-11-15 sound quality.md` reads them.

Two more commands act on a library rather than measure it:

    StacksTests --retouch guards [--dry-run]     # the filter guards (guardFilters) over the factory
    StacksTests --level Factory [--target -10] [--dry-run]   # Master per preset so one note peaks at the target
    StacksTests --regrow <presets> --out Tuning/regrown      # three new candidates per preset from its own prompt, levelled; the ear picks

## Where this is going

Decided 15 Nov 2026: the rig ships. Every player rates 1 to 5 and their own
Stacks nudges its generation toward their taste, within the same bounds as
above (recipe weights with a floor and a wildcard, avoid-lines from their own
ACT traits, never examples of liked patches), with a reset and a plain-words
view of what it has learned. Until that is built, this document describes the
internal rig; the policy section is what the shipped version will automate.

## Sessions

- **v1, 14 Nov 2026** (build 0.1.3, three-point scale): exported to
  `Tuning/v1/`, with the report and the raw log. Mostly pads; nothing reached
  ACT, so no change was made.
- **v2, 14 Nov 2026** (five-point scale), in progress. One change, as a
  broken-sound guard rather than a taste call: a "clavinet keys" batch put
  three of five sounds through a high-pass or band-pass main filter with a
  positive struck envelope, which lifts the cutoff at note-on and leaves only
  fizz; all three were rated 1, the two low-pass ones 3. Struck sounds now
  keep a high-pass low and still (or a band-pass gentle and low) and take the
  sweep on a low-pass filter 2 in series, and the prompt says the sweep
  belongs on a low-pass. The report tags the pattern as `fenv: lifts a HP/BP`.
  Later that night, 32 sounds rated: 9 of the 13 trash had a HP or BP main
  filter, and 6 of the 7 with a 24 dB HP/BP were trash against 7 of 25 for
  the rest (lift 3, ACT). Acted on as broken-sound guards, 15 Nov 2026:
  HP24/BP24 main filters become 12 dB (4c); leads through a band-pass get the
  low-pass they meant (4b); a low-pass whose envelope dives or settles below
  220 Hz (400 for keys and leads, 90 for basses), the "Resonant Pulse" case,
  moves up so the note stays (4d). The same guards were run over the factory
  library, which the audio audit had shown carrying the same faults. Next
  session: pads, keys and leads in the new build, same 1 to 5 scale; the
  trash rate for HP/BP sounds has to fall and the pads' whoa rate must not.
