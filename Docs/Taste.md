# Taste: how a copy of Stacks learns its player

Every player rates sounds with the number keys while a sound plays: 1 trash,
2 meh, 3 fine, 4 good, 5 whoa. The pill in the header counts the ratings and
flashes the verdict. From those ratings the copy builds a *taste profile*,
and every new sound it generates, on a Home tile, in the Lab, grown from a
favourite, answers to it. Profiles are personal: they live in the player's
library folder (`~/Music/Stacks Patches/Taste`), nothing is shared or
exported, and Settings has **Start over: forget my ratings**.

## What the profile is allowed to do

Two things, and only these two (`Source/Taste.h`):

1. **Steer away from what the player's trash shares.** For each kind of
   sound (pad, pluck, bass, keys, lead, bell, texture, drone) and across all
   kinds, a trait counts as *avoided* when at least five sounds rated 1 or 2
   carry it, at least five sounds rated higher of that kind exist, at least
   half the trash carries the trait, and it is at least twice as common in
   the trash as in the rest. At most three traits per kind. A new sound that
   carries an avoided trait is nudged off it by the smallest change that
   removes it (another filter type, another interval for B, a different
   unison size, a neighbouring band of a knob, an effect switched off; traits
   with no single knob behind them take a directed idea move), and the model
   is told in one sentence what to avoid. This is the stronger pull.

2. **Weight the recipes gently, per kind.** A fresh batch draws its engine
   recipes (two saws, FM, PWM, formant, ...) by weight instead of evenly, each
   slot with the weights of the kind it is going to be (the five sounds a Pad
   tile grows use the pad weights; a batch without a hint rotates through the
   kinds). A recipe's weight on a kind comes from the mean rating of the
   sounds of that kind written to it, shrunk toward even while there are few
   ratings, so "two saws" rated badly on basses says nothing about pads;
   dislikes pull harder than likes
   (0.75 per point below 3, 0.35 per point above). It never drops under 0.4
   of even or rises over 1.7, and one slot in every five is drawn evenly
   whatever the weights, so a disliked recipe still turns up now and then and
   a loved one never crowds the rest out.

What the profile never does: show the model the sounds the player liked, copy
their settings, or change anything until the ratings support it. A copy with
no ratings generates exactly as the factory build; forgetting the ratings
takes it back there.

## Measured (`StacksTests`, the taste section)

- Twelve trash pads through HP24 against eighteen good pads on LP12: the
  profile avoids `filter: HP24` on pads and nothing else; a pad through HP24
  leaves generation with a low-pass; a good pad is untouched; a *lead*
  through HP24 is untouched (the avoid was learned on pads).
- Four trash, or five trash with three others: nothing moves.
- A trait the trash and the good share alike: nothing moves.
- Recipe draw over 10,000 slots: a loved recipe (weight 1.7) takes 10% of the
  slots against 7% even, a hated one (0.4) still takes 4%, all fourteen
  recipes appear; with no taste, or for a kind the weights were not learned
  on, the draw is even.
- Over batches: random pads carry a Triangle LFO about half the time; for a
  player whose trash all had one, the share falls to 0% while the pads stay
  as varied (five distinct A tables either way).
- The log on disk: seen lines are skipped, the first rig's 1 / 2 / 3 reads as
  1 / 3 / 5, the latest verdict per sound wins; an empty folder is an empty
  profile.

## For whoever tunes the defaults

The rig described in `Docs/Tuning rig.md` is the same mechanism read by a
person: `StacksTests --ratings` builds the report from one or many players'
folders, and what reaches ACT there becomes a guard or a prompt line in the
build, under the same nudge policy. The shipped profile is that policy made
automatic, per player, with lower stakes: a bad avoid affects one copy and is
one reset away.
