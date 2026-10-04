// Stacks engine-side tests: wavetable design, patch JSON, the tuning guard,
// the AI grammar/prompt and both generators (the AI one through a fake model).
// Built as the StacksTests console app; exits non-zero on any failure.

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <cstdio>
#include <set>

#include "Parameters.h"
#include "Wavetable.h"
#include "Patch.h"
#include "PatchGenerator.h"
#include "ai/LlmPatchGenerator.h"
#include "ai/LlamaBackend.h"
#include "ai/ModelManager.h"

using namespace stacks;

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; const double _a = (double) (a), _b = (double) (b); \
    if (std::abs (_a - _b) > (double) (tol)) { ++failures; std::printf ("  FAIL %s:%d  %s = %g, expected %g (+-%g)\n", __FILE__, __LINE__, #a, _a, _b, (double) (tol)); } } while (0)
static void section (const char* name) { std::printf ("%s\n", name); }

static std::vector<float> series (float exponent, bool oddOnly = false)
{
    std::vector<float> f ((size_t) WaveSpec::kHarmonics);
    for (int k = 1; k <= WaveSpec::kHarmonics; ++k)
        f[(size_t) (k - 1)] = (oddOnly && k % 2 == 0) ? 0.0f : std::pow ((float) k, -exponent);
    return f;
}

static WaveSpec analyseTable (const UserTable& t, const juce::String& name = "back")
{
    return analyseWave ([&] (float m, float ph) { return t.read (0, m, ph); }, name);
}

//==============================================================================
static void testWaveSpecDigits()
{
    section ("WaveSpec digits");
    CHECK_NEAR (WaveSpec::digitToAmplitude (9), 1.0, 1.0e-6);
    CHECK_NEAR (WaveSpec::digitToAmplitude (0), 0.0, 1.0e-9);
    for (int d = 0; d <= 9; ++d)
        CHECK (WaveSpec::amplitudeToDigit (WaveSpec::digitToAmplitude (d)) == d);
    const int half = WaveSpec::amplitudeToDigit (0.5f);
    CHECK (half == 7 || half == 8);

    auto digits = WaveSpec::fromJson (R"json({"name":"Glass Choir","tail":4,"spectra":[[9,0,0,6,0,0,5,0,0,0,4,0,0,0,0,3],[9,5,3,2,2,1,1,1,0,0,0,0,0,0,0,0]]})json");
    CHECK (digits.has_value());
    if (digits)
    {
        CHECK (digits->frames.size() == 2);
        CHECK_NEAR (digits->frames[0][0], 1.0, 1.0e-6);
        CHECK_NEAR (digits->frames[0][1], 0.0, 1.0e-9);
        CHECK_NEAR (digits->tail, 4.0 / 9.0, 1.0e-6);
        CHECK (digits->name == "Glass Choir");
        auto floats = WaveSpec::fromJson (digits->toJson());
        CHECK (floats && *floats == *digits);
        auto again = WaveSpec::fromJson (digits->toJson (true));
        CHECK (again && *again == *digits);
    }
    CHECK (! WaveSpec::fromJson (R"json({"name":"x","tail":0,"spectra":[[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]]})json"));
    CHECK (! WaveSpec::fromJson (""));
    CHECK (! WaveSpec::fromJson ("[1,2]"));
    CHECK (! WaveSpec::fromJson (R"json({"name":"x"})json"));
}

static void testSpectralTables()
{
    section ("buildSpectralTable / analyseWave");
    WaveSpec saw;
    saw.name = "Saw";
    saw.tail = 1.0f;
    saw.frames = { series (1.0f), series (1.0f), series (1.0f) };
    auto table = buildSpectralTable (saw);
    CHECK (table != nullptr);
    if (table == nullptr) return;
    CHECK (table->frames == 16);
    CHECK (table->data.size() == (size_t) 16 * Tables::kLevels * Tables::kStride);

    float peak = 0.0f;
    for (int i = 0; i < Tables::kSize; ++i) peak = std::max (peak, std::abs (table->data[(size_t) i]));
    CHECK_NEAR (peak, 1.0, 0.02);
    CHECK_NEAR (table->data[(size_t) Tables::kSize], table->data[0], 1.0e-6);   // guard sample

    auto back = analyseTable (*table);
    CHECK_NEAR (back.frames[0][0], 1.0, 0.02);
    CHECK_NEAR (back.frames[0][1], 0.5, 0.06);
    CHECK_NEAR (back.frames[0][3], 0.25, 0.05);
    CHECK (back.tail > 0.6);

    WaveSpec dull = saw;
    dull.tail = 0.0f;
    auto back2 = analyseTable (*buildSpectralTable (dull));
    CHECK (back2.tail < 0.15);

    WaveSpec square;
    square.frames = { series (1.0f, true), series (1.0f, true) };
    auto back3 = analyseTable (*buildSpectralTable (square));
    CHECK (back3.frames[0][1] < 0.05 && back3.frames[0][3] < 0.05);
    CHECK_NEAR (back3.frames[0][2], 1.0 / 3.0, 0.05);

    WaveSpec morph;                               // sine at morph 0, saw at morph 1
    morph.frames = { std::vector<float> ((size_t) WaveSpec::kHarmonics, 0.0f), series (1.0f) };
    morph.frames[0][0] = 1.0f;
    auto mt = buildSpectralTable (morph);
    auto back4 = analyseTable (*mt);
    CHECK (back4.frames[0][1] < 0.05);           // morph 0: pure
    CHECK (back4.frames[2][1] > 0.4);            // morph 1: saw-like
    CHECK (back4.frames[1][1] > 0.1 && back4.frames[1][1] < back4.frames[2][1]); // in between

    // The top mip level only keeps the first two harmonics.
    auto top = analyseWave ([&] (float m, float ph) { return mt->read (Tables::kLevels - 1, m, ph); }, "top");
    CHECK (top.frames[2][3] < 0.05);

    juce::SharedResourcePointer<WavetableBank> bank;
    const int sawIndex = waveNames().indexOf ("Saw"), sineIndex = waveNames().indexOf ("Sine");
    auto builtInSaw = analyseWave ([&] (float m, float ph) { return bank->read (sawIndex, 0, m, ph); }, "Saw");
    CHECK_NEAR (builtInSaw.frames[2][0], 1.0, 0.02);
    CHECK (builtInSaw.frames[2][1] > 0.3);
    CHECK (builtInSaw.tail > 0.3);
    auto builtInSine = analyseWave ([&] (float m, float ph) { return bank->read (sineIndex, 0, m, ph); }, "Sine");
    CHECK (builtInSine.frames[0][1] < 0.05);
    CHECK (builtInSine.tail < 0.1);

    // Re-rendering an analysed built-in gives a close relative of it.
    auto twin = analyseTable (*buildSpectralTable (builtInSaw));
    CHECK_NEAR (twin.frames[2][1], builtInSaw.frames[2][1], 0.1);
}

static void testParameterTable()
{
    section ("parameter table");
    // Connections are saved by target *name*, so every knob name must be unique.
    std::set<juce::String> names;
    for (const auto& spec : paramSpecs())
    {
        ++checks;
        if (! names.insert (spec.name).second) { ++failures; std::printf ("  FAIL duplicate parameter name '%s'\n", spec.name); }
    }
    CHECK (modTargetNames().indexOf ("Chorus Spread") > 0 && modTargetNames().indexOf ("Spread") > 0);
    CHECK (paramRange ((int) P::filter_cutoff).end > 10000.0f);
}

static void testPatchJson()
{
    section ("Patch JSON");
    CHECK (waveNames()[kCustomWave] == "Custom");
    CHECK (waveNames().size() == kCustomWave + 1);
    CHECK (UserWavetables::customSlot (0) == UserWavetables::kCustomSlotA && UserWavetables::customSlot (1) == UserWavetables::kCustomSlotB);
    CHECK (UserWavetables::kSlots > UserWavetables::kCustomSlotB);

    Patch p;
    p.name = "Test";
    p.waves[0] = *WaveSpec::fromJson (R"json({"name":"Neon","tail":2,"spectra":[[9,8,7,6,5,5,5,4,4,4,4,3,3,3,3,3],[9,0,7,0,6,0,5,0,5,0,4,0,4,0,4,0]]})json");
    p.set (P::oscA_wave, (float) kCustomWave);
    p.set (P::filter_cutoff, 1234.0f);
    const auto json = p.toJson();
    CHECK (json.contains ("\"waveA\"") && ! json.contains ("\"waveB\""));
    auto q = Patch::fromJson (json);
    CHECK (q.has_value());
    if (q)
    {
        CHECK (q->waves[0] == p.waves[0]);
        CHECK (q->waves[1].isEmpty());
        CHECK ((int) q->get (P::oscA_wave) == kCustomWave);
        CHECK_NEAR (q->get (P::filter_cutoff), 1234.0, 0.01);
        CHECK (q->sameValuesAs (p));
    }
    auto child = Patch::fromJson (R"json({"name":"Kid","params":{"filter_cutoff":500}})json", &p);
    CHECK (child && child->waves[0] == p.waves[0] && (int) child->get (P::oscA_wave) == kCustomWave);
    CHECK (describePatch (p).contains ("Neon"));

    Patch m = p;
    mutatePatch (m, 0.5f, 1234);
    CHECK (! m.sameValuesAs (p));
    const auto mw = mutateWave (p.waves[0], 0.5f, 1234);
    CHECK (mw.frames.size() == p.waves[0].frames.size() && mw != p.waves[0]);
}

static void testTuningGuard()
{
    section ("keepPatchInTune");
    auto connect = [] (Patch& p, int slot, int src, int target, float amount)
    {
        p.set (modSourceParam (slot), (float) src);
        p.set (modDestParam (slot), (float) target);
        p.set (modAmountParam (slot), amount);
    };
    const int cutoff = modTargetForParam ((int) P::filter_cutoff);
    const int fineA  = modTargetForParam ((int) P::oscA_fine);

    Patch a; a.set (lfoShapeParam (0), (float) ShapeSquare); connect (a, 0, SrcLfo1, TargetPitch, 0.5f); keepPatchInTune (a);
    CHECK ((int) a.get (modDestParam (0)) == cutoff);
    CHECK_NEAR (a.get (modAmountParam (0)), 0.3, 1.0e-6);
    Patch a2 = a; keepPatchInTune (a2); CHECK (a2.sameValuesAs (a));

    Patch b; b.set (lfoShapeParam (0), (float) ShapeSine); connect (b, 0, SrcLfo1, TargetPitch, 0.5f); keepPatchInTune (b);
    CHECK ((int) b.get (modDestParam (0)) == TargetPitch);
    CHECK_NEAR (b.get (modAmountParam (0)), 0.03, 1.0e-6);

    Patch c; c.set (P::menv_sustain, 0.6f); connect (c, 0, SrcModEnv, TargetPitch, 0.3f); keepPatchInTune (c);
    CHECK_NEAR (c.get (P::menv_sustain), 0.0, 1.0e-6);
    CHECK_NEAR (c.get (modAmountParam (0)), 0.3, 1.0e-6);

    Patch d; d.set (P::fenv_sustain, 0.5f); connect (d, 0, SrcFilterEnv, TargetPitch, 0.3f); keepPatchInTune (d);
    CHECK ((int) d.get (modSourceParam (0)) == SrcOff);

    Patch e; connect (e, 0, SrcRandom, TargetPitch, 0.2f); keepPatchInTune (e);
    CHECK ((int) e.get (modSourceParam (0)) == SrcOff);

    Patch f; f.set (lfoShapeParam (1), (float) ShapeTriangle); connect (f, 0, SrcLfo2, fineA, 0.5f); keepPatchInTune (f);
    CHECK_NEAR (f.get (modAmountParam (0)), 0.08, 1.0e-6);

    Patch r; connect (r, 0, SrcRandom, fineA, 0.5f); keepPatchInTune (r);
    CHECK_NEAR (r.get (modAmountParam (0)), 0.03, 1.0e-6);

    Patch g; g.set (P::oscA_coarse, 7.0f); g.set (P::oscB_coarse, 7.0f); g.set (P::oscB_level, 0.5f); keepPatchInTune (g);
    CHECK_NEAR (g.get (P::oscA_coarse), 12.0, 1.0e-6);
    CHECK_NEAR (g.get (P::oscB_coarse), 12.0, 1.0e-6);

    Patch h; h.set (P::oscB_coarse, 7.0f); h.set (P::oscB_level, 0.0f); keepPatchInTune (h);
    CHECK_NEAR (h.get (P::oscB_coarse), 7.0, 1.0e-6);

    Patch i; connect (i, 0, SrcRandom, cutoff, 0.4f); keepPatchInTune (i);
    CHECK ((int) i.get (modSourceParam (0)) == SrcRandom);
    CHECK_NEAR (i.get (modAmountParam (0)), 0.4, 1.0e-6);

    Patch w; connect (w, 0, SrcModWheel, TargetPitch, 0.5f); keepPatchInTune (w);
    CHECK_NEAR (w.get (modAmountParam (0)), 0.17, 1.0e-6);
}

// Every rule the grammar references must be defined; names may not contain '_'.
static void checkGrammarConsistency (const juce::String& g)
{
    std::set<juce::String> defined, referenced;
    juce::StringArray lines;
    lines.addLines (g);
    for (const auto& line : lines)
    {
        if (line.trim().isEmpty()) continue;
        const auto name = line.upToFirstOccurrenceOf ("::=", false, false).trim();
        CHECK (name.isNotEmpty() && ! name.contains ("_") && ! name.contains (" "));
        defined.insert (name);

        const auto body = line.fromFirstOccurrenceOf ("::=", false, false);
        bool inString = false, inClass = false, escape = false;
        juce::String ident;
        auto flush = [&] { if (ident.isNotEmpty()) { referenced.insert (ident); ident.clear(); } };
        for (auto c : body)
        {
            if (inString)      { if (escape) escape = false; else if (c == '\\') escape = true; else if (c == '"') inString = false; continue; }
            if (inClass)       { if (escape) escape = false; else if (c == '\\') escape = true; else if (c == ']') inClass = false; continue; }
            if (c == '"')      { flush(); inString = true; continue; }
            if (c == '[')      { flush(); inClass = true; continue; }
            const bool wordChar = juce::CharacterFunctions::isLetterOrDigit (c) || c == '-';
            if (wordChar && (ident.isNotEmpty() || juce::CharacterFunctions::isLetter (c))) ident << juce::String::charToString (c);
            else flush();
        }
        flush();
    }
    CHECK (defined.count ("root") == 1);
    for (const auto& r : referenced)
        if (defined.count (r) == 0)
        {
            ++checks; ++failures;
            std::printf ("  FAIL grammar references undefined rule '%s'\n", r.toRawUTF8());
        }
}

static void testGrammarAndPrompt()
{
    section ("AI grammar and prompt");
    const auto g = LlmPatchGenerator::grammar (5, true);
    CHECK (g.contains ("root ::="));
    CHECK (g.contains ("(\",\" patch){4}"));
    CHECK (g.contains ("core ::="));
    CHECK (g.contains ("conn1 ::=") && g.contains ("conn6 ::=") && ! g.contains ("conn7 ::="));
    CHECK (g.contains ("lfo1-conn1 ::=") && g.contains ("menv-conn3 ::=") && g.contains ("perf-conn6 ::="));
    CHECK (g.contains ("\nbase-conn1 ::=") && g.contains ("\nlfo-conn2 ::="));
    CHECK (g.contains ("base-conn1 \",\" lfo-conn2"));                 // one performance/envelope connection, then one LFO
    CHECK (! g.contains ("| conn2") && g.contains ("| conn3"));           // extras start at slot 3
    CHECK (g.contains ("\nwave ::=") && g.contains ("\nspectrum ::=") && g.contains ("digit ::= [0-9]"));
    CHECK (g.contains ("\\\"Custom\\\""));
    CHECK (! g.contains ("p-mod1-source") && ! g.contains ("p-lfo3-rate"));
    checkGrammarConsistency (g);

    const auto plain = LlmPatchGenerator::grammar (3, false);
    CHECK (! plain.contains ("\nwave ::=") && ! plain.contains ("waveA"));
    CHECK (plain.contains ("(\",\" patch){2}"));
    checkGrammarConsistency (plain);

    const auto sys = LlmPatchGenerator::systemPrompt (true);
    CHECK (sys.contains ("waveA") && sys.contains ("spectra") && sys.contains ("Connections route modulation"));
    CHECK (sys.contains ("oscA_wave, oscA_morph, oscA_level"));
    CHECK (! sys.contains ("Velvet Horizon") && sys.contains ("<wave>"));
    CHECK (! LlmPatchGenerator::systemPrompt (false).contains ("Wavetable design"));

    GenerationRequest r;
    r.hint = "in the style of MGMT";
    r.brief = "Bright detuned saws.";
    const auto user = LlmPatchGenerator::userPrompt (r);
    CHECK (user.contains ("in the style of MGMT") && user.contains ("Sound brief") && user.contains ("Bright detuned saws."));
    CHECK (user.contains ("exactly 10 entries"));

    // The parent dump: core + non-default values + live connections, not the whole table.
    Patch parent;
    parent.name = "Parent";
    parent.set (P::filter_cutoff, 800.0f);
    parent.set (P::chorus_mix, 0.4f);
    GenerationRequest ev;
    ev.parents = { parent };
    ev.designWaves = false;
    const auto evolve = LlmPatchGenerator::userPrompt (ev);
    CHECK (evolve.contains ("\"filter_cutoff\": 800") && evolve.contains ("\"chorus_mix\": 0.4") && evolve.contains ("\"oscA_wave\""));
    if (! (evolve.contains ("\"filter_cutoff\": 800") && evolve.contains ("\"chorus_mix\": 0.4") && evolve.contains ("\"oscA_wave\"")))
        std::printf ("  parent dump was: %s\n", evolve.fromFirstOccurrenceOf ("params:", false, false).upToFirstOccurrenceOf ("\n", false, false).toRawUTF8());
    CHECK (! evolve.contains ("chorus_tone") && ! evolve.contains ("reverb_predelay") && ! evolve.contains ("mod1_source") && ! evolve.contains ("master_gain"));
}

static void testRandomGenerator()
{
    section ("RandomPatchGenerator");
    RandomPatchGenerator rnd;
    GenerationProgress quiet;
    GenerationRequest r;
    r.count = 24;
    r.designWaves = true;
    auto out = rnd.generate (r, quiet);
    CHECK (out.size() == 24);
    int custom = 0;
    for (const auto& p : out)
    {
        if ((int) p.get (P::oscA_wave) == kCustomWave) { ++custom; CHECK (! p.waves[0].isEmpty()); CHECK (p.waves[0].name.isNotEmpty()); }
        Patch q = p; keepPatchInTune (q); CHECK (q.sameValuesAs (p));
        CHECK (p.name.isNotEmpty() && p.origin == "Random");
    }
    CHECK (custom >= 4);

    Patch parent = out[0];
    parent.set (P::oscA_wave, (float) waveNames().indexOf ("Saw"));
    parent.waves[0] = WaveSpec();
    parent.name = "Golden Sunrise";
    GenerationRequest e;
    e.count = 12;
    e.parents = { parent };
    e.designWaves = true;
    auto kids = rnd.generate (e, quiet);
    int grown = 0;
    for (const auto& k : kids)
    {
        CHECK ((int) k.get (P::oscA_wave) != kCustomWave || ! k.waves[0].isEmpty());
        if ((int) k.get (P::oscA_wave) == kCustomWave) ++grown;
    }
    CHECK (grown >= 2);

    GenerationRequest off;
    off.count = 10;
    off.designWaves = false;
    for (const auto& p : rnd.generate (off, quiet))
        CHECK ((int) p.get (P::oscA_wave) != kCustomWave && p.waves[0].isEmpty());
}

// A model that answers with canned JSON, streamed in awkward chunks.
struct FakeBackend : public LlmBackend
{
    int calls = 0;
    juce::String lastUser, lastGrammar;
    juce::String name() const override      { return "Fake"; }
    juce::String modelName() const override { return "fake-1b"; }
    bool isAvailable (juce::String&) override { return true; }
    bool chat (const juce::String&, const juce::String& user, const juce::String& grammar,
               const std::function<void (const juce::String&)>& onText,
               const std::function<void (const juce::String&)>&,
               const std::function<bool()>&, juce::String&) override
    {
        ++calls;
        lastUser = user;
        lastGrammar = grammar;
        if (grammar.startsWith ("root ::= [\\x20"))
        {
            onText ("A bright, detuned saw lead with portamento and chorus.");
            return true;
        }
        const juce::String reply = R"json({"patches":[{"name":"Velvet Horizon","category":"Lead","description":"Bright detuned saw lead with a slow filter sweep","parent":1,"params":{"oscA_wave":"Saw","oscA_morph":0.7,"oscA_level":0.9,"oscB_wave":"Saw","oscB_coarse":0,"oscB_level":0.5,"fm_amount":0,"sub_level":0.1,"filter_type":"LP24","filter_cutoff":2400,"filter_res":0.2,"filter_env":1.5,"fenv_decay":0.6,"fenv_sustain":0.2,"aenv_attack":0.01,"aenv_decay":0.4,"aenv_sustain":0.8,"aenv_release":0.4,"unison_voices":3,"reverb_mix":0.2,"lfo1_shape":"Square","lfo1_rate":5,"lfo1_sync":"Free","mod1_source":"LFO 1","mod1_dest":"Pitch","mod1_amount":0.5,"unison_detune":22},"waveA":{"name":"Neon Saw","tail":7,"spectra":[[9,8,7,6,5,5,5,4,4,4,4,3,3,3,3,3],[9,5,0,4,0,3,0,2,0,2,0,1,0,1,0,1]]}},{"name":"Glass Keys","category":"Keys","description":"Sparse glassy keys","parent":1,"params":{"oscA_wave":"Glass","oscA_morph":0.3,"oscA_level":0.8,"oscB_wave":"Sine","oscB_coarse":12,"oscB_level":0.3,"fm_amount":0.2,"sub_level":0,"filter_type":"LP12","filter_cutoff":6000,"filter_res":0.1,"filter_env":0,"fenv_decay":0.3,"fenv_sustain":0.5,"aenv_attack":0.005,"aenv_decay":1.2,"aenv_sustain":0.3,"aenv_release":0.8,"unison_voices":1,"reverb_mix":0.4,"mod1_source":"Velocity","mod1_dest":"Cutoff","mod1_amount":0.4}}]})json";
        for (int i = 0; i < reply.length(); i += 7)
            onText (reply.substring (i, i + 7));
        return true;
    }
};

static void testLlmGenerator()
{
    section ("LlmPatchGenerator with a fake model");
    auto fake = std::make_shared<FakeBackend>();
    LlmPatchGenerator gen (fake, std::make_shared<RandomPatchGenerator>());

    std::vector<Patch> streamed;
    GenerationProgress progress;
    progress.onPatch = [&] (const Patch& p) { streamed.push_back (p); };

    GenerationRequest r;
    r.count = 2;
    r.hint = "in the style of MGMT";
    r.designWaves = true;
    auto out = gen.generate (r, progress);
    CHECK (fake->calls == 2);                                   // the brief, then the patches
    CHECK (fake->lastUser.contains ("Sound brief") && fake->lastUser.contains ("detuned saw lead"));
    CHECK (fake->lastGrammar.contains ("\nwave ::="));
    CHECK (out.size() == 2 && streamed.size() == 2);
    if (out.size() == 2)
    {
        CHECK (out[0].name != "Velvet Horizon");                 // the example's name is never kept
        CHECK ((int) out[0].get (P::oscA_wave) == kCustomWave);
        CHECK (out[0].waves[0].name == "Neon Saw" && out[0].waves[0].frames.size() == 2);
        CHECK (out[0].waves[1].isEmpty());
        CHECK ((int) out[0].get (P::oscB_wave) == waveNames().indexOf ("Saw"));
        CHECK ((int) out[0].get (modDestParam (0)) == modTargetForParam ((int) P::filter_cutoff)); // square LFO off pitch
        CHECK (out[0].origin == "AI");
        CHECK_NEAR (out[0].get (P::unison_detune), 22.0, 1.0e-4);
        CHECK ((int) out[1].get (P::oscA_wave) == waveNames().indexOf ("Glass"));
        CHECK (out[1].waves[0].isEmpty());
        CHECK ((int) out[1].get (modSourceParam (0)) == SrcVelocity);
    }

    GenerationRequest e;
    e.count = 2;
    e.parents = { out[0] };
    e.designWaves = true;
    fake->calls = 0;
    auto kids = gen.generate (e, progress);
    CHECK (fake->calls == 1);                                   // no hint: no brief
    CHECK (fake->lastUser.contains ("waveA (its designed table)") && fake->lastUser.contains ("\"spectra\""));
    CHECK (kids.size() == 2);
    if (kids.size() == 2)
    {
        CHECK (kids[1].waves[0] == out[0].waves[0]);          // no waveA in the reply: the parent's table is inherited
        CHECK (! kids[0].sameValuesAs (out[0]));              // a verbatim copy of the parent is nudged into a relative
        CHECK (kids[0].name.endsWith (" II"));
    }

    GenerationRequest e2 = e;
    e2.parents = { out[1] };
    gen.generate (e2, progress);
    CHECK (fake->lastUser.contains ("spectrum of its Glass table"));

    GenerationRequest noWaves = r;
    noWaves.hint.clear();
    noWaves.designWaves = false;
    gen.generate (noWaves, progress);
    CHECK (! fake->lastGrammar.contains ("\nwave ::="));
    CHECK (! fake->lastUser.contains ("Sound brief"));
}

//==============================================================================
// `StacksTests --live [direction]`: one real batch with the installed built-in
// model, to see what the grammar and prompt get out of it (takes a minute).
static void describe (const Patch& p)
{
    const int wave = juce::jlimit (0, waveNames().size() - 1, (int) p.get (P::oscA_wave));
    std::printf ("  %-26s [%-7s] A=%s", p.name.toRawUTF8(), p.category.toRawUTF8(), waveNames()[wave].toRawUTF8());
    if (! p.waves[0].isEmpty())
        std::printf ("  waveA='%s' frames=%d tail=%.1f", p.waves[0].name.toRawUTF8(), (int) p.waves[0].frames.size(), p.waves[0].tail);
    std::printf ("\n     ");
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int src = (int) p.get (modSourceParam (i)), dst = (int) p.get (modDestParam (i));
        if (src == SrcOff || dst == TargetOff) continue;
        std::printf ("%s > %s %.2f;  ", modSourceNames()[src].toRawUTF8(), modTargetNames()[dst].toRawUTF8(), p.get (modAmountParam (i)));
    }
    std::printf ("\n     %s\n", p.description.toRawUTF8());
}

static int liveTest (const juce::String& hint)
{
    std::optional<ModelInfo> chosen;
    for (const auto& m : ModelManager::catalogue())
        if (m.installed && (! chosen || m.id.containsIgnoreCase ("4b")))
            chosen = m;
    if (! chosen)
    {
        std::printf ("no built-in model is installed\n");
        return 2;
    }
    std::printf ("Live test with %s\n", chosen->label.toRawUTF8());
    auto backend = std::make_shared<LlamaBackend> (chosen->file, chosen->label);
    LlmPatchGenerator gen (backend, std::make_shared<RandomPatchGenerator>());

    GenerationProgress progress;
    progress.onStatus = [] (const juce::String& s) { std::printf ("  status: %s\n", s.toRawUTF8()); std::fflush (stdout); };

    GenerationRequest r;
    r.count = 3;
    r.hint = hint;
    r.designWaves = true;
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto out = gen.generate (r, progress);
    std::printf ("Fresh: %d patches in %.0f s\n", (int) out.size(), (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0);
    for (const auto& p : out) describe (p);

    if (! out.empty())
    {
        GenerationRequest e;
        e.count = 2;
        e.parents = { out.front() };
        e.designWaves = true;
        t0 = juce::Time::getMillisecondCounterHiRes();
        auto kids = gen.generate (e, progress);
        std::printf ("Evolve: %d patches in %.0f s\n", (int) kids.size(), (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0);
        for (const auto& p : kids) describe (p);
    }
    return out.empty() ? 1 : 0;
}

int main (int argc, char** argv)
{
    if (argc > 1 && juce::String (argv[1]) == "--live")
        return liveTest (argc > 2 ? juce::String::fromUTF8 (argv[2]) : juce::String());

    testWaveSpecDigits();
    testSpectralTables();
    testParameterTable();
    testPatchJson();
    testTuningGuard();
    testGrammarAndPrompt();
    testRandomGenerator();
    testLlmGenerator();
    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
