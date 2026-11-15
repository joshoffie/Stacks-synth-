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

    struct Count { int withTrash = 0, withOthers = 0; };

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

    // Avoids: per kind, then across every kind.
    auto findAvoids = [&] (const juce::String& kind)
    {
        std::map<juce::String, Count> counts;
        int nTrash = 0, nOthers = 0;
        for (const auto& r : ratings)
        {
            if (kind.isNotEmpty() && tasteKind (r.category) != kind) continue;
            const bool isTrash = r.verdict <= 2;
            (isTrash ? nTrash : nOthers)++;
            std::set<juce::String> seen;
            for (const auto& f : r.features)
            {
                if (! avoidable (f) || seen.count (f)) continue;
                seen.insert (f);
                (isTrash ? counts[f].withTrash : counts[f].withOthers)++;
            }
        }
        std::vector<Avoid> found;
        if (nTrash < kMinTrash || nOthers < kMinOthers)
            return found;
        for (const auto& kv : counts)
        {
            const auto& c = kv.second;
            if (c.withTrash < kMinTrash) continue;
            const float pTrash = (float) c.withTrash / (float) nTrash;
            const float pOthers = ((float) c.withOthers + 0.5f) / ((float) nOthers + 1.0f);   // smoothed: a trait no good sound has is not infinitely bad
            const float lift = pTrash / pOthers;
            if (pTrash < 0.5f || lift < kMinLift) continue;
            found.push_back ({ kind, kv.first, c.withTrash, nTrash, c.withOthers, nOthers, lift });
        }
        std::sort (found.begin(), found.end(), [] (const Avoid& a, const Avoid& b) { return a.lift * (float) a.trash > b.lift * (float) b.trash; });
        if ((int) found.size() > kMaxAvoidsPerKind) found.resize ((size_t) kMaxAvoidsPerKind);
        return found;
    };
    for (auto* kind : kKinds)
        for (auto& a : findAvoids (kind)) t.avoids.push_back (a);
    for (auto& a : findAvoids ({}))
    {
        bool dup = false;   // a kind already avoids it: the kind-wide line adds nothing
        for (const auto& k : t.avoids) dup = dup || (k.feature == a.feature && k.kind.isNotEmpty());
        if (! dup) t.avoids.push_back (a);
    }

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

juce::String TasteProfile::promptLines() const
{
    if (avoids.empty()) return {};
    std::map<juce::String, juce::StringArray> byKind;
    for (const auto& a : avoids) byKind[a.kind].addIfNotAlreadyThere (a.feature);
    juce::String s = "This player has rated what you made before. ";
    for (const auto& kv : byKind)
        s << (kv.first.isEmpty() ? juce::String ("Avoid on every kind: ") : "Avoid on " + plural (kv.first) + ": ") << kv.second.joinIntoString ("; ") << ". ";
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
    juce::StringArray up, down;
    for (const auto& kindEntry : recipeWeights)
        for (const auto& kv : kindEntry.second)
        {
            if (kv.second >= 1.25f) up.add (kv.first + " on " + plural (kindEntry.first));
            if (kv.second <= 0.7f)  down.add (kv.first + " on " + plural (kindEntry.first));
        }
    if (! up.isEmpty())   s << " Drawing a little more often: " << up.joinIntoString (", ") << ".";
    if (! down.isEmpty()) s << " Drawing less often: " << down.joinIntoString (", ") << ".";
    if (avoids.empty() && up.isEmpty() && down.isEmpty())
        s << " Nothing has changed yet: a pattern needs five sounds rated 1 or 2 and five rated higher of one kind before it counts.";
    return s;
}

//==============================================================================
void nudgeAway (Patch& p, const juce::String& feature, juce::int64 seed)
{
    juce::Random rng (seed);
    auto uni = [&] (float a, float b) { return a + (b - a) * rng.nextFloat(); };
    auto nameIndex = [] (const juce::StringArray& names, const char* name) { return (float) juce::jmax (0, names.indexOf (name)); };
    const auto prefix = feature.upToFirstOccurrenceOf (":", false, false).trim();
    const auto value  = feature.fromFirstOccurrenceOf (":", false, false).trim();

    if (prefix == "filter")
    {
        p.set (P::filter_type, nameIndex (filterTypeNames(), value == "LP12" ? "LP24" : "LP12"));
        if (value.startsWith ("HP") || value.startsWith ("BP")) p.set (P::filter_cutoff, juce::jmax (p.get (P::filter_cutoff), 800.0f));
        return;
    }
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
