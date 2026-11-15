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
3. Press **1** (trash), **2** (fine) or **3** (whoa). The pill flashes the verdict.
4. Next block. Grow from a good one, pick another kind, keep going.

Rate the sound you hear, not the idea: a great concept that sounds thin is
trash, a plain sound that works is fine, and whoa is for the ones you'd
have never asked for and want to keep. Rating the same sound again
overrides the earlier verdict. Do several kinds per session; the report is
per kind and needs at least six rated of a kind before it says anything.

## What is written

`~/Library/Application Support/Stacks/ratings/<mac name>-<date>.jsonl`, one
line per rating and one line (verdict 0) per sound the engine showed you,
with the whole patch, the prompt and brief it came from, the recipe it was
written to, the grow mode and variation. Click the pill to open the folder.
To send it in, zip the folder.

## The report

    build.nosync/StacksTests_artefacts/Release/StacksTests --ratings [folder or file ...]

With no argument it reads this Mac's folder; give it several folders to
merge testers. Per kind: what the trash shares (each trait with how often it
appears in trash against the rest you rated, and the lift), what the whoa
ones share, trait pairs the trash shares, how each recipe fared, and the
median numbers of trash against the rest. It also writes `report.md` next to
the data.

## What happens with it

Nothing automatic. Findings with real evidence become, by hand and one
commit at a time: a guard in `enrichPatch` (a click is not a pluck), a line
in the prompt's per-kind guidance ("avoid: ...", "what tends to work: ..."),
or a weight on a recipe in `freshEngines()` - never below 40% of an even
draw, and one slot in five stays a wildcard, so the surprises keep coming.
Each change goes through `StacksTests --bench` before it ships.
