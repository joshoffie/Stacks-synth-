#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <vector>

#include "Patch.h"

namespace stacks
{

// One rating as the player made it, reduced to what taste needs.
struct TasteRating
{
    juce::String key, category, recipe;
    int verdict = 0;             // 1 trash .. 5 whoa
    juce::StringArray features;  // tasteFeatures of the patch
};

// Every rating in the log folder (`*.jsonl`, one JSON object per line): lines
// with a verdict, the latest verdict per sound, the first rig's 1 / 2 / 3 read
// as 1 / 3 / 5. Unrated "seen" lines are skipped.
std::vector<TasteRating> readRatingLogs (const juce::File& folder);

// What a player's ratings say about their taste, and the only two ways it is
// allowed to steer what the engine writes next:
//
//  - Avoid: a trait their trash shares (at least kMinTrash trash and kMinOthers
//    other rated sounds of that kind, with the trait at least kMinLift times as
//    common in the trash). New sounds that carry it are nudged away from it,
//    and the model is told to avoid it. This is the stronger pull.
//  - Favour: a trait their whoa sounds (4 and 5) share, by the same rule
//    against the rest. One in three new sounds of that kind that lacks such a
//    trait is given one, and the model is told what the player leans toward,
//    "when it fits, not every time". This is the gentler pull: it never adds
//    more than one trait to a sound, never touches two sounds in three, and
//    never shows the model a liked sound.
//  - Recipe weights: a recipe the player keeps rating high is drawn a little
//    more often, one they keep rating low a lot less often; never past kCap
//    or under kFloor, and one slot in five is drawn evenly whatever the
//    weights.
//
// Every trait of a sound counts (tasteFeatures: sources, table shape, filter,
// character, envelopes, every connection, every effect and its settings).
// Nothing happens until the ratings support it, so a fresh copy generates
// exactly like the factory build, and "forget my ratings" takes it back there.
struct TasteProfile
{
    struct Avoid
    {
        juce::String kind;       // "pad", "keys", ... or "" for every kind
        juce::String feature;    // a tasteFeatures token, e.g. "filter: HP24"
        int trash = 0, ofTrash = 0, others = 0, ofOthers = 0;
        float lift = 1.0f;
    };
    std::vector<Avoid> avoids;
    std::vector<Avoid> favours;   // the same shape, read from the whoa sounds: trash/ofTrash hold the whoa counts
    std::map<juce::String, std::map<juce::String, float>> recipeWeights;   // kind -> recipe tag -> weight, 1 = an even draw; a kind learns only from its own ratings
    int rated = 0, trash = 0, whoa = 0;

    static constexpr float kFloor = 0.4f, kCap = 1.7f;        // a disliked recipe falls to 0.4 of even; a loved one rises to 1.7 at most
    static constexpr int kMinTrash = 5, kMinOthers = 5;       // before a trait can be avoided
    static constexpr float kMinLift = 2.0f;
    static constexpr int kMaxAvoidsPerKind = 3;
    static constexpr float kLeanChance = 0.33f;               // the share of new sounds that are given one favoured trait they lack

    static TasteProfile fromRatings (const std::vector<TasteRating>&);
    static TasteProfile fromFolder (const juce::File& folder) { return fromRatings (readRatingLogs (folder)); }

    bool isEmpty() const noexcept                            { return avoids.empty() && favours.empty() && recipeWeights.empty(); }
    float weightFor (const juce::String& kind, const juce::String& recipeTag) const;   // 1 for a recipe never rated on that kind
    juce::StringArray avoidedTraitsIn (const Patch&) const;  // the avoided traits this patch carries, for its kind and for every kind
    juce::StringArray favouredTraitsMissingIn (const Patch&) const;   // the favoured traits of its kind this patch lacks
    juce::String promptLines() const;                        // "Avoid on pads: ..." lines for the model, at most one per kind; never what to imitate
    juce::String summary() const;                            // plain words for Settings: what it has learned
};

// The kind a category belongs to for taste: "pad", "pluck", "bass", "keys",
// "lead", "bell", "texture", "drone" or "other".
juce::String tasteKind (const juce::String& category);

// Moves a patch away from one avoided trait by the smallest change that
// removes it (another filter type, another interval for B, another unison
// size, a neighbouring band of a knob, an effect switched off...). Traits with
// no single knob behind them take a directed idea move instead.
void nudgeAway (Patch&, const juce::String& feature, juce::int64 seed);

// Nudges until the patch carries none of the profile's avoided traits, up to
// `attempts` times. Returns how many it still carries.
int steerAwayFromAvoided (Patch&, const TasteProfile&, juce::int64 seed, int attempts = 6);

// Gives a patch one trait, by the smallest change that adds it (the inverse of
// nudgeAway). Traits with no single knob behind them are left alone.
void nudgeToward (Patch&, const juce::String& feature, juce::int64 seed);

// With probability kLeanChance, gives the patch one favoured trait of its kind
// that it lacks. Returns true when it did.
bool leanTowardFavoured (Patch&, const TasteProfile&, juce::int64 seed);

// `count` recipe indices for a batch from `tags`, drawn without replacement by
// the profile's weights for each slot's kind (`slotKinds`, as tasteKind gives
// them; fewer entries than slots means the last one repeats, none means an
// even draw); in every run of five, one slot is drawn evenly whatever the
// weights, so a disliked recipe still turns up now and then.
std::vector<int> drawRecipes (int count, const std::vector<juce::String>& tags, const TasteProfile&, juce::Random&, const std::vector<juce::String>& slotKinds = {});

} // namespace stacks
