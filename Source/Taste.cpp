#include "Taste.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "PatchGenerator.h"

namespace stacks
{

namespace
{
    // The report's reading of the first rig's 1 / 2 / 3.
    int verdictOnFiveScale (int verdict, bool legacy) noexcept { return legacy ? juce::jlimit (1, 5, verdict * 2 - 1) : juce::jlimit (0, 5, verdict); }

    const char* const kKinds[] = { "pad", "pluck", "bass", "keys", "lead", "bell", "texture", "drone" };

    // Which features may become an avoid: the recipe is weighted instead, and
    // the kind-wide housekeeping tokens say nothing a nudge could act on.
    bool avoidable (const juce::String& feature)
    {
        return ! feature.startsWith ("recipe: ") && feature != "mod: none" && feature != "fx: none" && feature != "no lfo";
    }

    struct Count { int withTrash = 0, withOthers = 0, withWhoa = 0, withLow = 0; };   // the side being read, the rest, and the whoa and trash counts for the cross-check

    // juce::Random's first draw is almost a linear function of the seed, so
    // neighbouring seeds (one per patch in a batch) would all fall the same way.
    // A few warm-up steps decorrelate them.
    juce::Random seededRandom (juce::int64 seed)
    {
        juce::Random rng (seed);
        for (int i = 0; i < 4; ++i) rng.nextInt();
        return rng;
    }

    // "pads", "basses", "keys": the kind in the plural, for the summary and the prompt.
    juce::String plural (const juce::String& kind)
    {
        if (kind == "keys") return kind;
        if (kind == "bass") return "basses";
        return kind + "s";
    }
}

juce::String tasteKind (const juce::String& category)
{
    const auto c = category.toLowerCase();
    if (c.contains ("pad") || c.contains ("string") || c.contains ("choir")) return "pad";
    if (c.contains ("pluck") || c.contains ("perc") || c.contains ("stab") || c.contains ("drum")) return "pluck";
    if (c.contains ("bass")) return "bass";
    if (c.contains ("key") || c.contains ("piano") || c.contains ("organ")) return "keys";
    if (c.contains ("lead")) return "lead";
    if (c.contains ("bell") || c.contains ("chime") || c.contains ("mallet")) return "bell";
    if (c.contains ("texture")) return "texture";
    if (c.contains ("drone")) return "drone";
    return "other";
}

std::vector<TasteRating> readRatingLogs (const juce::File& folder)
{
    std::map<juce::String, TasteRating> byKey;
    std::vector<juce::String> order;
    if (! folder.isDirectory())
        return {};
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.jsonl");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName() < b.getFileName(); });
    for (const auto& file : files)
    {
        juce::StringArray text;
        file.readLines (text);
        for (const auto& line : text)
        {
            if (line.trim().isEmpty()) continue;
            const auto v = juce::JSON::parse (line);
            auto* o = v.getDynamicObject();
            if (o == nullptr) continue;
            const bool legacy = ! o->hasProperty ("scale");
            const int verdict = verdictOnFiveScale ((int) o->getProperty ("verdict"), legacy);
            if (verdict <= 0) continue;   // a "seen" line
            auto patch = Patch::fromVar (o->getProperty ("patch"));
            if (! patch) continue;
            TasteRating r;
            r.key = o->getProperty ("key").toString();
            if (r.key.isEmpty()) r.key = juce::String::toHexString (juce::JSON::toString (patch->paramsToVar()).hashCode64());
            r.category = patch->category;
            r.recipe = patch->recipe;
            r.verdict = verdict;
            r.features = tasteFeatures (*patch);
            if (! byKey.count (r.key)) order.push_back (r.key);
            byKey[r.key] = std::move (r);   // the latest verdict for a sound wins
        }
    }
    std::vector<TasteRating> out;
    for (const auto& k : order) out.push_back (byKey[k]);
    return out;
}

TasteProfile TasteProfile::fromRatings (const std::vector<TasteRating>& ratings)
{
    TasteProfile t;
    t.rated = (int) ratings.size();
    for (const auto& r : ratings) { if (r.verdict <= 2) ++t.trash; if (r.verdict >= 4) ++t.whoa; }

    // Avoids: what the trash shares, per kind, then across every kind. Favours:
    // the same reading of the whoa sounds against the rest.
    auto findShared = [&] (const juce::String& kind, bool fromWhoa)
    {
        std::map<juce::String, Count> counts;
        int nSide = 0, nOthers = 0, nWhoa = 0, nLow = 0;
        for (const auto& r : ratings)
        {
            if (kind.isNotEmpty() && tasteKind (r.category) != kind) continue;
            const bool onSide = fromWhoa ? r.verdict >= 4 : r.verdict <= 2;   // the side being read
            const bool whoa = r.verdict >= 4, low = r.verdict <= 2;
            (onSide ? nSide : nOthers)++;
            if (whoa) ++nWhoa;
            if (low) ++nLow;
            std::set<juce::String> seen;
            for (const auto& f : r.features)
            {
                if (! avoidable (f) || seen.count (f)) continue;
                seen.insert (f);
                auto& c = counts[f];
                (onSide ? c.withTrash : c.withOthers)++;
                if (whoa) ++c.withWhoa;
                if (low) ++c.withLow;
            }
        }
        std::vector<Avoid> found;
        if (nSide < kMinTrash || nOthers < kMinOthers)
            return found;
        for (const auto& kv : counts)
        {
            const auto& c = kv.second;
            if (c.withTrash < kMinTrash) continue;
            const float pSide = (float) c.withTrash / (float) nSide;
            const float pOthers = ((float) c.withOthers + 0.5f) / ((float) nOthers + 1.0f);   // smoothed: a trait no good sound has is not infinitely bad
            const float lift = pSide / pOthers;
            if (pSide < 0.5f || lift < kMinLift) continue;
            // A favour must also be clearly rarer in the trash than in the whoa: what both share is no taste.
            if (fromWhoa && nLow > 0 && (float) c.withLow / (float) nLow > 0.5f * pSide) continue;
            found.push_back ({ kind, kv.first, c.withTrash, nSide, c.withOthers, nOthers, lift });
        }
        std::sort (found.begin(), found.end(), [] (const Avoid& a, const Avoid& b) { return a.lift * (float) a.trash > b.lift * (float) b.trash; });
        if ((int) found.size() > kMaxAvoidsPerKind) found.resize ((size_t) kMaxAvoidsPerKind);
        return found;
    };
    auto collect = [&] (bool fromWhoa, std::vector<Avoid>& into)
    {
        for (auto* kind : kKinds)
            for (auto& a : findShared (kind, fromWhoa)) into.push_back (a);
        for (auto& a : findShared ({}, fromWhoa))
        {
            bool dup = false;   // a kind already has it: the kind-wide line adds nothing
            for (const auto& k : into) dup = dup || (k.feature == a.feature && k.kind.isNotEmpty());
            if (! dup) into.push_back (a);
        }
    };
    collect (false, t.avoids);
    collect (true, t.favours);
    // A trait cannot be both: the trash reading wins (the stronger evidence of harm).
    t.favours.erase (std::remove_if (t.favours.begin(), t.favours.end(), [&] (const Avoid& f)
                     { for (const auto& a : t.avoids) if (a.feature == f.feature && (a.kind == f.kind || a.kind.isEmpty() || f.kind.isEmpty())) return true; return false; }), t.favours.end());

    // Recipe weights, per kind: shrunk toward even with few ratings; dislikes pull
    // harder than likes. A recipe rated badly on basses says nothing about pads.
    std::map<juce::String, std::map<juce::String, std::pair<int, double>>> byKind;   // kind -> tag -> (n, sum)
    for (const auto& r : ratings)
        if (r.recipe.isNotEmpty() && ! r.recipe.startsWith ("grow:") && r.recipe != "random" && r.recipe != "random:grow")
        {
            auto& e = byKind[tasteKind (r.category)][r.recipe];
            ++e.first; e.second += r.verdict;
        }
    for (const auto& kindEntry : byKind)
        for (const auto& kv : kindEntry.second)
        {
            const int n = kv.second.first;
            if (n < 2) continue;
            const double mean = kv.second.second / n;
            const double shrink = (double) n / (n + 3.0);
            const double delta = mean - 3.0;
            const double w = 1.0 + shrink * (delta < 0.0 ? 0.75 * delta : 0.35 * delta);
            t.recipeWeights[kindEntry.first][kv.first] = (float) juce::jlimit ((double) kFloor, (double) kCap, w);
        }
    return t;
}

float TasteProfile::weightFor (const juce::String& kind, const juce::String& recipeTag) const
{
    const auto k = recipeWeights.find (kind);
    if (k == recipeWeights.end()) return 1.0f;
    const auto it = k->second.find (recipeTag);
    return it == k->second.end() ? 1.0f : it->second;
}

juce::StringArray TasteProfile::avoidedTraitsIn (const Patch& p) const
{
    juce::StringArray out;
    if (avoids.empty()) return out;
    const auto kind = tasteKind (p.category);
    const auto features = tasteFeatures (p);
    for (const auto& a : avoids)
        if ((a.kind.isEmpty() || a.kind == kind) && features.contains (a.feature) && ! out.contains (a.feature))
            out.add (a.feature);
    return out;
}

juce::StringArray TasteProfile::favouredTraitsMissingIn (const Patch& p) const
{
    juce::StringArray out;
    if (favours.empty()) return out;
    const auto kind = tasteKind (p.category);
    const auto features = tasteFeatures (p);
    for (const auto& f : favours)
        if ((f.kind.isEmpty() || f.kind == kind) && ! features.contains (f.feature) && ! out.contains (f.feature))
            out.add (f.feature);
    return out;
}

juce::String TasteProfile::promptLines() const
{
    if (avoids.empty() && favours.empty()) return {};
    juce::String s = "This player has rated what you made before. ";
    std::map<juce::String, juce::StringArray> byKind;
    for (const auto& a : avoids) byKind[a.kind].addIfNotAlreadyThere (a.feature);
    for (const auto& kv : byKind)
        s << (kv.first.isEmpty() ? juce::String ("Avoid on every kind: ") : "Avoid on " + plural (kv.first) + ": ") << kv.second.joinIntoString ("; ") << ". ";
    std::map<juce::String, juce::StringArray> liked;
    for (const auto& f : favours) liked[f.kind].addIfNotAlreadyThere (f.feature);
    for (const auto& kv : liked)
        s << (kv.first.isEmpty() ? juce::String ("They tend to like, on every kind: ") : "They tend to like, on " + plural (kv.first) + ": ") << kv.second.joinIntoString ("; ") << " - lean that way when it fits, not every time. ";
    return s.trimEnd() + "\n";
}

juce::String TasteProfile::summary() const
{
    if (rated == 0) return "No ratings yet. Press 1 (trash) to 5 (whoa) while a sound plays; what you rate shapes the sounds Stacks makes for you.";
    juce::String s;
    s << "Rated " << rated << " sound" << (rated == 1 ? "" : "s") << " (" << trash << " trash, " << (rated - trash - whoa) << " fine, " << whoa << " whoa).";
    if (! avoids.empty())
    {
        juce::StringArray parts;
        for (const auto& a : avoids) parts.add ((a.kind.isEmpty() ? juce::String() : plural (a.kind) + ": ") + a.feature);
        s << " Steering away from " << parts.joinIntoString (", ") << ".";
    }
    if (! favours.empty())
    {
        juce::StringArray parts;
        for (const auto& f : favours) parts.add ((f.kind.isEmpty() ? juce::String() : plural (f.kind) + ": ") + f.feature);
        s << " Leaning toward " << parts.joinIntoString (", ") << ".";
    }
    juce::StringArray up, down;
    for (const auto& kindEntry : recipeWeights)
        for (const auto& kv : kindEntry.second)
        {
            if (kv.second >= 1.25f) up.add (kv.first + " on " + plural (kindEntry.first));
            if (kv.second <= 0.7f)  down.add (kv.first + " on " + plural (kindEntry.first));
        }
    if (! up.isEmpty())   s << " Drawing a little more often: " << up.joinIntoString (", ") << ".";
    if (! down.isEmpty()) s << " Drawing less often: " << down.joinIntoString (", ") << ".";
    if (avoids.empty() && favours.empty() && up.isEmpty() && down.isEmpty())
        s << " Nothing has changed yet: a pattern needs five sounds rated 1 or 2 (or five rated 4 or 5) and five others of one kind before it counts.";
    return s;
}

//==============================================================================
void nudgeAway (Patch& p, const juce::String& feature, juce::int64 seed)
{
    auto rng = seededRandom (seed);
    auto uni = [&] (float a, float b) { return a + (b - a) * rng.nextFloat(); };
    auto nameIndex = [] (const juce::StringArray& names, const char* name) { return (float) juce::jmax (0, names.indexOf (name)); };
    const auto prefix = feature.upToFirstOccurrenceOf (":", false, false).trim();
    const auto value  = feature.fromFirstOccurrenceOf (":", false, false).trim();

    // Three bands, moved to a neighbouring one: "low" goes up, "high" goes down, the middle goes either way.
    auto bandMove = [&] (P param, const juce::String& v, const char* low, const char* high, float toLow, float toMidLo, float toMidHi, float toHigh)
    {
        if (v == low)       p.set (param, uni (toMidLo, toMidHi));
        else if (v == high) p.set (param, uni (toMidLo, toMidHi));
        else                p.set (param, rng.nextBool() ? toLow : toHigh);
    };

    if (prefix == "filter")
    {
        p.set (P::filter_type, nameIndex (filterTypeNames(), value == "LP12" ? "LP24" : "LP12"));
        if (value.startsWith ("HP") || value.startsWith ("BP")) p.set (P::filter_cutoff, juce::jmax (p.get (P::filter_cutoff), 800.0f));
        return;
    }
    if (prefix == "filter 2 type") { p.set (P::filter2_type, nameIndex (filterTypeNames(), value == "LP24" ? "LP12" : "LP24")); return; }
    if (prefix == "cutoff 2")
    {
        const float c = p.get (P::filter2_cutoff);
        p.set (P::filter2_cutoff, value == "<300" ? uni (500.0f, 1500.0f) : value == ">4k" ? uni (1200.0f, 3500.0f) : (rng.nextBool() ? c * 2.4f : c / 2.4f));
        return;
    }
    if (prefix == "A morph")     { bandMove (P::oscA_morph, value, "low", "high", 0.15f, 0.4f, 0.6f, 0.85f); return; }
    if (prefix == "B morph")     { bandMove (P::oscB_morph, value, "low", "high", 0.15f, 0.4f, 0.6f, 0.85f); return; }
    if (prefix == "A interval")  { p.set (P::oscA_coarse, 0.0f); return; }
    if (prefix == "B level")     { bandMove (P::oscB_level, value, "quiet", "loud", 0.15f, 0.35f, 0.55f, 0.75f); return; }
    if (prefix == "B warp")      { p.set (P::oscB_warp, 0.0f); return; }
    if (prefix == "osc C" && value.isNotEmpty())   // "osc C: Wave interval": another table, back to the octave
    {
        static const char* const tables[] = { "Saw", "Triangle", "Sine", "Glass", "Organ" };
        p.set (P::oscC_wave, nameIndex (waveNames(), tables[rng.nextInt (5)]));
        p.set (P::oscC_coarse, rng.nextBool() ? 0.0f : 12.0f);
        return;
    }
    if (feature == "sample")     { p.set (P::smp_mode, 0.0f); return; }
    if (prefix == "detune")      { bandMove (P::unison_detune, value, "tight", "wide", 4.0f, 10.0f, 18.0f, 28.0f); return; }
    if (prefix == "spread")      { p.set (P::unison_spread, value == "narrow" ? uni (0.6f, 0.9f) : uni (0.1f, 0.3f)); return; }
    if (feature == "uni morph")  { p.set (P::unison_morph, 0.0f); return; }
    if (prefix == "table")
    {
        // "table: sparse/full", "table phases: aligned/round", "table tail: dull/bright": reshape the designed table, or take a built-in.
        if (p.waves[0].isEmpty()) return;
        if (value == "sparse" || value == "full")
        {
            // the opposite density: a built-in with that character stands in
            p.set (P::oscA_wave, nameIndex (waveNames(), value == "sparse" ? (rng.nextBool() ? "Saw" : "Pulse") : (rng.nextBool() ? "Glass" : "Sine")));
            p.waves[0] = WaveSpec();
        }
        return;
    }
    if (prefix == "table phases") { p.waves[0].scatter = value == "aligned" ? uni (0.5f, 1.0f) : 0.0f; return; }
    if (prefix == "table tail")   { p.waves[0].tail = value == "dull" ? uni (0.5f, 0.9f) : uni (0.0f, 0.3f); return; }
    if (prefix == "fenv decay")  { bandMove (P::fenv_decay, value, "<100ms", ">500ms", 0.05f, 0.15f, 0.4f, 0.8f); return; }
    if (prefix == "menv decay")  { bandMove (P::menv_decay, value, "<100ms", ">500ms", 0.05f, 0.15f, 0.4f, 0.8f); return; }
    if (prefix == "drive")       { bandMove (P::filter_drive, value, "clean", "hot", 1.0f, 1.8f, 3.0f, 5.0f); return; }
    if (prefix == "keytrack")    { bandMove (P::filter_keytrack, value, "none", "full", 0.0f, 0.2f, 0.5f, 0.8f); return; }
    if (prefix == "circuit")     { p.set (P::filter_model, (float) (((int) p.get (P::filter_model) + 1 + rng.nextInt (3)) % 4)); return; }
    if (prefix == "drift")       { bandMove (P::char_drift, value, "none", "much", 0.0f, 0.1f, 0.3f, 0.6f); return; }
    if (prefix == "warmth")      { bandMove (P::char_sat, value, "none", "much", 0.0f, 0.1f, 0.3f, 0.6f); return; }
    if (prefix == "lfo rate")
    {
        for (int k = 0; k < 2; ++k) if ((int) p.get (lfoSyncParam (k)) == 0) bandMove (lfoRateParam (k), value, "slow", "fast", 0.2f, 0.8f, 2.5f, 6.0f);
        return;
    }
    if (feature == "lfo: note mode") { for (int k = 0; k < 2; ++k) p.set (lfoModeParam (k), 0.0f); return; }
    if (prefix == "reverb size") { bandMove (P::reverb_size, value, "small", "big", 0.25f, 0.45f, 0.65f, 0.85f); return; }
    if (prefix == "reverb damp") { bandMove (P::reverb_damp, value, "bright", "dark", 0.2f, 0.4f, 0.6f, 0.8f); return; }
    if (feature == "shimmer")    { p.set (P::reverb_shimmer, 0.0f); if (reverbTypeNames()[(int) p.get (P::reverb_type)] == "Shimmer") p.set (P::reverb_type, nameIndex (reverbTypeNames(), "Hall")); return; }
    if (prefix == "delay feedback") { bandMove (P::delay_feedback, value, "few", "many", 0.2f, 0.4f, 0.55f, 0.75f); return; }
    if (prefix == "delay mix")   { bandMove (P::delay_mix, value, "light", "wet", 0.1f, 0.25f, 0.35f, 0.5f); return; }
    if (prefix == "chorus")      { p.set (P::chorus_mode, (float) (((int) p.get (P::chorus_mode) + 1 + rng.nextInt (3)) % 4)); return; }
    if (prefix == "distortion drive") { bandMove (P::dist_drive, value, "light", "heavy", 6.0f, 14.0f, 22.0f, 30.0f); return; }
    if (prefix == "hyper")       { bandMove (P::hyper_amount, value, "light", "wide", 0.0f, 0.35f, 0.55f, 0.8f); return; }
    if (prefix == "tape")        { bandMove (P::tape_amount, value, "light", "heavy", 0.0f, 0.35f, 0.55f, 0.8f); return; }
    if (prefix == "eq")
    {
        if (value.startsWith ("low"))  p.set (P::eq_low_gain, 0.0f);
        if (value.startsWith ("mid"))  p.set (P::eq_mid_gain, 0.0f);
        if (value.startsWith ("high")) p.set (P::eq_high_gain, 0.0f);
        return;
    }
    if (feature == "compressed") { p.set (P::comp_mix, 0.0f); return; }
    if (prefix == "fenv")        { p.set (P::filter_env, 0.0f); return; }
    if (prefix == "filter 2")    { p.set (P::filter_routing, 0.0f); return; }
    if (prefix == "cutoff")
    {
        const float c = p.get (P::filter_cutoff);
        p.set (P::filter_cutoff, value == "<300" ? uni (500.0f, 1200.0f) : value == ">4k" ? uni (1200.0f, 3500.0f) : (rng.nextBool() ? c * 2.4f : c / 2.4f));
        return;
    }
    if (prefix == "resonance")   { p.set (P::filter_res, value == "low" ? uni (0.35f, 0.55f) : value == "mid" ? uni (0.05f, 0.25f) : uni (0.3f, 0.5f)); return; }
    if (prefix == "filter env")  { p.set (P::filter_env, value == "flat" ? (rng.nextBool() ? uni (1.6f, 3.0f) : uni (-3.0f, -1.6f)) : value == "up" ? uni (-0.8f, 0.8f) : uni (1.6f, 3.0f)); return; }
    if (prefix == "wave A")
    {
        static const char* const tables[] = { "Saw", "Triangle", "Pulse", "Organ", "Glass", "Formant", "Fold", "Sine" };
        const char* pick = tables[rng.nextInt (8)];
        if (value == pick) pick = tables[(rng.nextInt (7) + 1 + 0) % 8];
        p.set (P::oscA_wave, nameIndex (waveNames(), pick));
        p.waves[0] = WaveSpec();
        return;
    }
    if (prefix == "osc B")
    {
        if (value == "off") { p.set (P::oscB_wave, nameIndex (waveNames(), rng.nextBool() ? "Saw" : "Triangle")); p.set (P::oscB_coarse, rng.nextBool() ? -12.0f : 0.0f); p.set (P::oscB_level, uni (0.3f, 0.5f)); }
        else
        {
            static const char* const tables[] = { "Saw", "Triangle", "Pulse", "Sine", "Organ", "Glass" };
            p.set (P::oscB_wave, nameIndex (waveNames(), tables[rng.nextInt (6)]));
            static const float intervals[] = { -12.0f, 0.0f, 12.0f };
            float c = intervals[rng.nextInt (3)];
            if (std::abs (c - p.get (P::oscB_coarse)) < 0.5f) c = intervals[(rng.nextInt (2) + 1) % 3];
            p.set (P::oscB_coarse, c);
        }
        return;
    }
    if (prefix == "fm")          { p.set (P::fm_amount, value == "none" ? uni (0.1f, 0.3f) : value == "light" ? (rng.nextBool() ? 0.0f : uni (0.35f, 0.6f)) : uni (0.0f, 0.15f)); return; }
    if (feature == "sub")        { p.set (P::sub_level, 0.0f); return; }
    if (feature == "noise")      { p.set (P::noise_level, 0.0f); return; }
    if (feature == "osc C")      { p.set (P::oscC_level, 0.0f); return; }
    if (feature == "glide")      { p.set (P::glide, 0.0f); return; }
    if (feature == "arp")        { p.set (P::arp_mode, 0.0f); return; }
    if (prefix == "unison")
    {
        p.set (P::unison_voices, value == "1" ? 3.0f : value == "2-4" ? (rng.nextBool() ? 1.0f : 5.0f) : 3.0f);
        if (p.get (P::unison_detune) < 8.0f && p.get (P::unison_voices) > 1.5f) p.set (P::unison_detune, uni (10.0f, 20.0f));
        return;
    }
    if (prefix == "warp")        { p.set (P::oscA_warp, 0.0f); return; }
    if (prefix == "attack")      { p.set (P::aenv_attack, value == "<10ms" ? uni (0.02f, 0.08f) : value == "10-100ms" ? (rng.nextBool() ? 0.003f : uni (0.15f, 0.6f)) : value == "100ms-1s" ? uni (0.01f, 0.06f) : uni (0.3f, 0.9f)); return; }
    if (prefix == "decay")       { p.set (P::aenv_decay, value == "<80ms" ? uni (0.15f, 0.4f) : value == "80-300ms" ? (rng.nextBool() ? 0.05f : uni (0.4f, 0.9f)) : value == "300ms-1s" ? uni (0.1f, 0.25f) : uni (0.4f, 0.9f)); return; }
    if (prefix == "sustain")     { p.set (P::aenv_sustain, value == "none" ? uni (0.3f, 0.6f) : value == "some" ? (rng.nextBool() ? 0.0f : uni (0.7f, 0.9f)) : uni (0.2f, 0.5f)); return; }
    if (prefix == "release")     { p.set (P::aenv_release, value == "<100ms" ? uni (0.2f, 0.45f) : value == "100-500ms" ? (rng.nextBool() ? 0.06f : uni (0.6f, 1.5f)) : value == "0.5-2s" ? uni (0.15f, 0.45f) : uni (0.6f, 1.8f)); return; }
    if (prefix == "mod")
    {
        // "Source > Target": that connection goes.
        const auto src = value.upToFirstOccurrenceOf (">", false, false).trim(), dst = value.fromFirstOccurrenceOf (">", false, false).trim();
        const int s = modSourceNames().indexOf (src), d = modTargetNames().indexOf (dst);
        bool removed = false;
        for (int i = 0; i < kNumModSlots && s >= 0 && d >= 0; ++i)
            if ((int) p.get (modSourceParam (i)) == s && (int) p.get (modDestParam (i)) == d) { p.set (modSourceParam (i), 0.0f); p.set (modDestParam (i), 0.0f); p.set (modAmountParam (i), 0.0f); removed = true; }
        if (removed) return;
    }
    if (prefix == "lfo shape")
    {
        const char* pick = value == "Sine" ? "Triangle" : "Sine";
        for (int k = 0; k < 2; ++k) p.set (lfoShapeParam (k), nameIndex (lfoShapeNames(), pick));
        return;
    }
    if (prefix == "lfo")         { for (int k = 0; k < 2; ++k) p.set (lfoSyncParam (k), value == "synced" ? 0.0f : 3.0f); return; }
    if (prefix == "fx")
    {
        static const char* const fxNames[] = { "distortion", "eq", "chorus", "hyper", "tape", "delay", "reverb", "compressor" };
        for (int e = 0; e < kNumFx; ++e)
            if (value == fxNames[e])
            {
                switch (e)
                {
                    case 0: p.set (P::dist_mix, 0.0f); break;
                    case 1: p.set (P::eq_low_gain, 0.0f); p.set (P::eq_mid_gain, 0.0f); p.set (P::eq_high_gain, 0.0f); break;
                    case 2: p.set (P::chorus_mix, 0.0f); break;
                    case 3: p.set (P::hyper_amount, 0.0f); break;
                    case 4: p.set (P::tape_amount, 0.0f); break;
                    case 5: p.set (P::delay_mix, 0.0f); break;
                    case 6: p.set (P::reverb_mix, 0.0f); break;
                    default: p.set (P::comp_mix, 0.0f); break;
                }
                return;
            }
    }
    if (prefix == "reverb")
    {
        // "Type band": another type, and the mix moved out of that band.
        const auto type = value.upToFirstOccurrenceOf (" ", false, false);
        static const char* const types[] = { "Room", "Plate", "Hall", "Lush", "Shimmer" };
        const char* pick = types[rng.nextInt (5)];
        if (type == pick) pick = types[(rng.nextInt (4) + 1) % 5];
        p.set (P::reverb_type, nameIndex (reverbTypeNames(), pick));
        const auto band = value.fromFirstOccurrenceOf (" ", false, false);
        p.set (P::reverb_mix, band == "light" ? uni (0.25f, 0.4f) : band == "wet" ? uni (0.1f, 0.25f) : (rng.nextBool() ? uni (0.05f, 0.15f) : uni (0.45f, 0.6f)));
        return;
    }
    if (prefix == "delay")
    {
        const bool synced = value.endsWith ("synced");
        p.set (P::delay_sync, synced ? 0.0f : 3.0f);
        p.set (P::delay_mode, (float) rng.nextInt (3));
        return;
    }
    if (prefix == "distortion")
    {
        const int current = (int) p.get (P::dist_mode);
        p.set (P::dist_mode, (float) ((current + 1 + rng.nextInt (4)) % 5));
        return;
    }
    // Envelope classes, "mod: none", "no lfo", "fx: none" and anything new: a directed idea move.
    applyIdeaMove (p, rng.nextInt (kNumIdeaMoves), seed);
}

int steerAwayFromAvoided (Patch& p, const TasteProfile& taste, juce::int64 seed, int attempts)
{
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        const auto carried = taste.avoidedTraitsIn (p);
        if (carried.isEmpty()) return 0;
        nudgeAway (p, carried[attempt % carried.size()], seed + attempt * 7919);
        keepPatchInTune (p);
    }
    return taste.avoidedTraitsIn (p).size();
}

void nudgeToward (Patch& p, const juce::String& feature, juce::int64 seed)
{
    auto rng = seededRandom (seed);
    auto uni = [&] (float a, float b) { return a + (b - a) * rng.nextFloat(); };
    auto nameIndex = [] (const juce::StringArray& names, const juce::String& name) { return (float) juce::jmax (0, names.indexOf (name)); };
    const auto prefix = feature.upToFirstOccurrenceOf (":", false, false).trim();
    const auto value  = feature.fromFirstOccurrenceOf (":", false, false).trim();
    // A knob into a named band: the band's centre.
    struct Band { const char* name; float lo, hi; };
    auto into = [&] (P param, std::initializer_list<Band> bands) { for (const auto& b : bands) if (value == b.name) { p.set (param, uni (b.lo, b.hi)); return true; } return false; };

    if (prefix == "wave A")       { if (value != "designed" && value != "imported" && waveNames().contains (value)) { p.set (P::oscA_wave, nameIndex (waveNames(), value)); p.waves[0] = WaveSpec(); } return; }
    if (prefix == "A morph")      { into (P::oscA_morph, { { "low", 0.05f, 0.3f }, { "mid", 0.38f, 0.62f }, { "high", 0.7f, 0.95f } }); return; }
    if (prefix == "A interval")   { p.set (P::oscA_coarse, value.getFloatValue()); return; }
    if (prefix == "osc B")
    {
        if (value == "off") { p.set (P::oscB_level, 0.0f); return; }
        // "Wave interval": the wave, then the interval word
        const auto wave = value.upToFirstOccurrenceOf (" ", false, false), rel = value.fromFirstOccurrenceOf (" ", false, false);
        if (waveNames().contains (wave)) p.set (P::oscB_wave, nameIndex (waveNames(), wave));
        if (p.get (P::oscB_level) < 0.05f) p.set (P::oscB_level, uni (0.3f, 0.5f));
        if (rel == "unison") p.set (P::oscB_coarse, 0.0f); else if (rel == "fifth") p.set (P::oscB_coarse, 7.0f); else if (rel.getIntValue() != 0) p.set (P::oscB_coarse, (float) rel.getIntValue());
        return;
    }
    if (prefix == "B level")      { if (p.get (P::oscB_level) > 0.05f) into (P::oscB_level, { { "quiet", 0.1f, 0.28f }, { "mid", 0.35f, 0.55f }, { "loud", 0.65f, 0.85f } }); return; }
    if (prefix == "B morph")      { into (P::oscB_morph, { { "low", 0.05f, 0.3f }, { "mid", 0.38f, 0.62f }, { "high", 0.7f, 0.95f } }); return; }
    if (prefix == "fm")           { into (P::fm_amount, { { "none", 0.0f, 0.0f }, { "light", 0.08f, 0.25f }, { "heavy", 0.35f, 0.6f } }); return; }
    if (feature == "sub")         { p.set (P::sub_level, uni (0.3f, 0.6f)); return; }
    if (feature == "noise")       { p.set (P::noise_level, uni (0.12f, 0.25f)); return; }
    if (prefix == "unison")
    {
        if (value == "1") p.set (P::unison_voices, 1.0f);
        else { p.set (P::unison_voices, value == "2-4" ? (float) (2 + rng.nextInt (3)) : (float) (5 + rng.nextInt (3))); if (p.get (P::unison_detune) < 6.0f) p.set (P::unison_detune, uni (10.0f, 20.0f)); }
        return;
    }
    if (prefix == "detune")       { if (p.get (P::unison_voices) > 1.5f) into (P::unison_detune, { { "tight", 2.0f, 7.0f }, { "normal", 9.0f, 18.0f }, { "wide", 22.0f, 32.0f } }); return; }
    if (prefix == "spread")       { if (p.get (P::unison_voices) > 1.5f) into (P::unison_spread, { { "narrow", 0.0f, 0.3f }, { "wide", 0.6f, 0.95f } }); return; }
    if (feature == "uni morph")   { p.set (P::unison_morph, uni (0.2f, 0.5f)); if (p.get (P::unison_voices) < 1.5f) p.set (P::unison_voices, 3.0f); return; }
    if (prefix == "warp")         { if (warpNames().contains (value)) { p.set (P::oscA_warp, nameIndex (warpNames(), value)); p.set (P::oscA_warp_amt, uni (0.3f, 0.6f)); } return; }
    if (prefix == "table phases") { if (! p.waves[0].isEmpty()) p.waves[0].scatter = value == "round" ? uni (0.5f, 1.0f) : 0.0f; return; }
    if (prefix == "table tail")   { if (! p.waves[0].isEmpty()) p.waves[0].tail = value == "bright" ? uni (0.5f, 0.9f) : uni (0.0f, 0.3f); return; }
    if (prefix == "filter")       { if (filterTypeNames().contains (value)) p.set (P::filter_type, nameIndex (filterTypeNames(), value)); return; }
    if (prefix == "filter 2")     { if (filterRoutingNames().contains (value)) p.set (P::filter_routing, nameIndex (filterRoutingNames(), value)); return; }
    if (prefix == "filter 2 type"){ if (filterTypeNames().contains (value)) { p.set (P::filter2_type, nameIndex (filterTypeNames(), value)); if ((int) p.get (P::filter_routing) == 0) p.set (P::filter_routing, 1.0f); } return; }
    if (prefix == "cutoff")       { into (P::filter_cutoff, { { "<300", 120.0f, 280.0f }, { "300-1k", 350.0f, 950.0f }, { "1k-4k", 1100.0f, 3800.0f }, { ">4k", 4500.0f, 9000.0f } }); return; }
    if (prefix == "cutoff 2")     { into (P::filter2_cutoff, { { "<300", 120.0f, 280.0f }, { "300-1k", 350.0f, 950.0f }, { "1k-4k", 1100.0f, 3800.0f }, { ">4k", 4500.0f, 9000.0f } }); return; }
    if (prefix == "resonance")    { into (P::filter_res, { { "low", 0.0f, 0.25f }, { "mid", 0.35f, 0.55f }, { "high", 0.65f, 0.85f } }); return; }
    if (prefix == "filter env")   { p.set (P::filter_env, value == "up" ? uni (1.6f, 3.0f) : value == "down" ? uni (-3.0f, -1.6f) : uni (-0.8f, 0.8f)); return; }
    if (prefix == "fenv decay")   { into (P::fenv_decay, { { "<100ms", 0.03f, 0.09f }, { "100-500ms", 0.12f, 0.45f }, { ">500ms", 0.6f, 1.5f } }); return; }
    if (prefix == "drive")        { into (P::filter_drive, { { "clean", 1.0f, 1.3f }, { "driven", 1.8f, 3.2f }, { "hot", 4.0f, 7.0f } }); return; }
    if (prefix == "keytrack")     { into (P::filter_keytrack, { { "none", 0.0f, 0.05f }, { "some", 0.2f, 0.5f }, { "full", 0.7f, 1.0f } }); return; }
    if (prefix == "circuit")      { if (filterModelNames().contains (value)) p.set (P::filter_model, nameIndex (filterModelNames(), value)); return; }
    if (prefix == "drift")        { into (P::char_drift, { { "none", 0.0f, 0.03f }, { "some", 0.1f, 0.35f }, { "much", 0.45f, 0.8f } }); return; }
    if (prefix == "warmth")       { into (P::char_sat, { { "none", 0.0f, 0.03f }, { "some", 0.1f, 0.35f }, { "much", 0.45f, 0.8f } }); return; }
    if (prefix == "attack")       { into (P::aenv_attack, { { "<10ms", 0.001f, 0.008f }, { "10-100ms", 0.015f, 0.09f }, { "100ms-1s", 0.15f, 0.9f }, { ">1s", 1.2f, 3.0f } }); return; }
    if (prefix == "decay")        { into (P::aenv_decay, { { "<80ms", 0.02f, 0.07f }, { "80-300ms", 0.1f, 0.28f }, { "300ms-1s", 0.35f, 0.95f }, { ">1s", 1.2f, 3.0f } }); return; }
    if (prefix == "sustain")      { into (P::aenv_sustain, { { "none", 0.0f, 0.1f }, { "some", 0.2f, 0.55f }, { "high", 0.65f, 1.0f } }); return; }
    if (prefix == "release")      { into (P::aenv_release, { { "<100ms", 0.03f, 0.09f }, { "100-500ms", 0.12f, 0.45f }, { "0.5-2s", 0.6f, 1.9f }, { ">2s", 2.2f, 5.0f } }); return; }
    if (feature == "glide")       { p.set (P::glide, uni (0.03f, 0.12f)); return; }
    if (prefix == "mod")
    {
        if (value == "none") return;
        const auto src = value.upToFirstOccurrenceOf (">", false, false).trim(), dst = value.fromFirstOccurrenceOf (">", false, false).trim();
        const int s = modSourceNames().indexOf (src), d = modTargetNames().indexOf (dst);
        if (s <= 0 || d <= 0) return;
        for (int i = 0; i < kNumModSlots; ++i)
            if ((int) p.get (modSourceParam (i)) == SrcOff || (int) p.get (modDestParam (i)) == TargetOff)
            {
                p.set (modSourceParam (i), (float) s); p.set (modDestParam (i), (float) d);
                p.set (modAmountParam (i), d == TargetPitch || d == TargetPitchB ? 0.02f : uni (0.25f, 0.4f));
                return;
            }
        return;
    }
    if (prefix == "lfo shape")    { if (lfoShapeNames().contains (value)) p.set (lfoShapeParam (0), nameIndex (lfoShapeNames(), value)); return; }
    if (prefix == "lfo")          { if (value == "synced") p.set (lfoSyncParam (0), 3.0f); else if (value == "free") p.set (lfoSyncParam (0), 0.0f); else if (value == "note mode") p.set (lfoModeParam (0), 1.0f); return; }
    if (prefix == "lfo rate")     { into (lfoRateParam (0), { { "slow", 0.05f, 0.4f }, { "medium", 0.6f, 2.5f }, { "fast", 3.5f, 8.0f } }); return; }
    if (prefix == "menv decay")   { into (P::menv_decay, { { "<100ms", 0.03f, 0.09f }, { "100-500ms", 0.12f, 0.45f }, { ">500ms", 0.6f, 1.5f } }); return; }
    if (prefix == "fx")
    {
        if (value == "reverb") p.set (P::reverb_mix, juce::jmax (p.get (P::reverb_mix), uni (0.2f, 0.35f)));
        else if (value == "delay") p.set (P::delay_mix, juce::jmax (p.get (P::delay_mix), uni (0.15f, 0.3f)));
        else if (value == "chorus") p.set (P::chorus_mix, juce::jmax (p.get (P::chorus_mix), uni (0.2f, 0.4f)));
        else if (value == "hyper") p.set (P::hyper_amount, juce::jmax (p.get (P::hyper_amount), uni (0.3f, 0.5f)));
        else if (value == "tape") p.set (P::tape_amount, juce::jmax (p.get (P::tape_amount), uni (0.3f, 0.5f)));
        else if (value == "distortion") p.set (P::dist_mix, juce::jmax (p.get (P::dist_mix), uni (0.2f, 0.4f)));
        else if (value == "compressor") p.set (P::comp_mix, juce::jmax (p.get (P::comp_mix), uni (0.4f, 0.8f)));
        else if (value == "eq") p.set (P::eq_high_gain, 2.5f);
        return;
    }
    if (prefix == "reverb")
    {
        const auto type = value.upToFirstOccurrenceOf (" ", false, false), band = value.fromFirstOccurrenceOf (" ", false, false);
        if (reverbTypeNames().contains (type)) p.set (P::reverb_type, nameIndex (reverbTypeNames(), type));
        if (band == "light") p.set (P::reverb_mix, uni (0.08f, 0.18f)); else if (band == "medium") p.set (P::reverb_mix, uni (0.22f, 0.38f)); else if (band == "wet") p.set (P::reverb_mix, uni (0.42f, 0.6f));
        return;
    }
    if (prefix == "reverb size")  { into (P::reverb_size, { { "small", 0.1f, 0.35f }, { "medium", 0.45f, 0.65f }, { "big", 0.72f, 0.95f } }); return; }
    if (prefix == "reverb damp")  { into (P::reverb_damp, { { "bright", 0.05f, 0.3f }, { "medium", 0.4f, 0.6f }, { "dark", 0.7f, 0.95f } }); return; }
    if (feature == "shimmer")     { p.set (P::reverb_shimmer, uni (0.3f, 0.6f)); if (p.get (P::reverb_mix) < 0.05f) p.set (P::reverb_mix, uni (0.2f, 0.35f)); return; }
    if (prefix == "delay")
    {
        const auto mode = value.upToFirstOccurrenceOf (" ", false, false);
        if (delayModeNames().contains (mode)) p.set (P::delay_mode, nameIndex (delayModeNames(), mode));
        p.set (P::delay_sync, value.endsWith ("synced") ? 3.0f : 0.0f);
        if (p.get (P::delay_mix) < 0.05f) p.set (P::delay_mix, uni (0.15f, 0.3f));
        return;
    }
    if (prefix == "delay feedback") { into (P::delay_feedback, { { "few", 0.15f, 0.3f }, { "some", 0.4f, 0.55f }, { "many", 0.65f, 0.85f } }); return; }
    if (prefix == "delay mix")    { into (P::delay_mix, { { "light", 0.08f, 0.18f }, { "medium", 0.22f, 0.38f }, { "wet", 0.42f, 0.6f } }); return; }
    if (prefix == "chorus")       { if (chorusModeNames().contains (value)) { p.set (P::chorus_mode, nameIndex (chorusModeNames(), value)); if (p.get (P::chorus_mix) < 0.05f) p.set (P::chorus_mix, uni (0.2f, 0.4f)); } return; }
    if (prefix == "distortion")   { if (distModeNames().contains (value)) { p.set (P::dist_mode, nameIndex (distModeNames(), value)); if (p.get (P::dist_mix) < 0.05f) p.set (P::dist_mix, uni (0.2f, 0.4f)); } return; }
    if (prefix == "distortion drive") { into (P::dist_drive, { { "light", 4.0f, 11.0f }, { "medium", 13.0f, 23.0f }, { "heavy", 25.0f, 34.0f } }); return; }
    if (prefix == "hyper")        { into (P::hyper_amount, { { "light", 0.1f, 0.28f }, { "medium", 0.35f, 0.55f }, { "wide", 0.65f, 0.9f } }); return; }
    if (prefix == "tape")         { into (P::tape_amount, { { "light", 0.1f, 0.28f }, { "medium", 0.35f, 0.55f }, { "heavy", 0.65f, 0.9f } }); return; }
    if (prefix == "eq")
    {
        const float g = value.endsWith ("boost") ? uni (2.0f, 4.5f) : uni (-4.5f, -2.0f);
        if (value.startsWith ("low")) p.set (P::eq_low_gain, g); else if (value.startsWith ("mid")) p.set (P::eq_mid_gain, g); else p.set (P::eq_high_gain, g);
        return;
    }
    if (feature == "compressed")  { p.set (P::comp_mix, uni (0.4f, 0.8f)); return; }
    if (feature == "arp")         { p.set (P::arp_mode, 1.0f); return; }
    // "envelope: class", "table: sparse/full", "osc C", "sample", "fx: none", "no lfo", "recipe: x": nothing a single knob gives.
}

bool leanTowardFavoured (Patch& p, const TasteProfile& taste, juce::int64 seed)
{
    if (taste.favours.empty()) return false;
    auto rng = seededRandom (seed);
    if (rng.nextFloat() >= TasteProfile::kLeanChance) return false;
    const auto missing = taste.favouredTraitsMissingIn (p);
    if (missing.isEmpty()) return false;
    const auto before = tasteFeatures (p);
    nudgeToward (p, missing[rng.nextInt (missing.size())], seed ^ 0x2545F491);
    keepPatchInTune (p);
    return tasteFeatures (p) != before;
}

std::vector<int> drawRecipes (int count, const std::vector<juce::String>& tags, const TasteProfile& taste, juce::Random& rng, const std::vector<juce::String>& slotKinds)
{
    std::vector<int> picked;
    if (tags.empty() || count <= 0) return picked;
    auto kindOf = [&] (int slot) { return slotKinds.empty() ? juce::String() : slotKinds[(size_t) juce::jmin (slot, (int) slotKinds.size() - 1)]; };
    std::vector<int> pool;
    auto refill = [&] { pool.clear(); for (int i = 0; i < (int) tags.size(); ++i) pool.push_back (i); };
    refill();
    int wildcardAt = rng.nextInt (5);
    for (int slot = 0; slot < count; ++slot)
    {
        if (pool.empty()) refill();
        if (slot % 5 == 0 && slot > 0) wildcardAt = rng.nextInt (5);
        const bool wildcard = slot % 5 == wildcardAt;   // one even draw in every run of five, at a random position
        int chosen = 0;
        const auto kind = kindOf (slot);
        if (wildcard || kind.isEmpty() || ! taste.recipeWeights.count (kind))
            chosen = (int) rng.nextInt ((int) pool.size());
        else
        {
            double total = 0.0;
            for (int i : pool) total += taste.weightFor (kind, tags[(size_t) i]);
            double r = rng.nextDouble() * total;
            chosen = (int) pool.size() - 1;
            for (size_t k = 0; k < pool.size(); ++k)
            {
                r -= taste.weightFor (kind, tags[(size_t) pool[k]]);
                if (r <= 0.0) { chosen = (int) k; break; }
            }
        }
        picked.push_back (pool[(size_t) chosen]);
        pool.erase (pool.begin() + chosen);
    }
    return picked;
}

} // namespace stacks
