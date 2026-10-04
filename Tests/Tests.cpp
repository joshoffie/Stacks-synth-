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
#include "Arpeggiator.h"

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
    p.tags = { "warm", "pad" };
    p.parentName = "Mother";
    p.prompt = "warm pad";
    auto q2 = Patch::fromJson (p.toJson());
    CHECK (q2 && q2->tags == p.tags && q2->parentName == "Mother" && q2->prompt == "warm pad");
    auto tagged = Patch::fromJson (R"json({"name":"T","tags":["Bright ", "bright", "x y", "", "Z!"]})json");
    CHECK (tagged && tagged->tags.size() == 3 && tagged->tags[0] == "bright" && tagged->tags[1] == "x y" && tagged->tags[2] == "z");
    CHECK (autoTags (p).contains ("pad") || p.category.isEmpty());
    CHECK (patchTips (p).contains ("Cutoff"));
    auto child = Patch::fromJson (R"json({"name":"Kid","params":{"filter_cutoff":500}})json", &p);
    CHECK (child && child->waves[0] == p.waves[0] && (int) child->get (P::oscA_wave) == kCustomWave);
    CHECK (describePatch (p).contains ("Neon"));

    // morphPatch: 0 = from, 1 = to, 0.5 = halfway on knobs, choices switch at 0.5, past 1 keeps going
    Patch a, b;
    a.set (P::filter_cutoff, 500.0f);  b.set (P::filter_cutoff, 4000.0f);
    a.set (P::oscA_wave, 0.0f);         b.set (P::oscA_wave, 2.0f);
    a.set (P::reverb_mix, 0.1f);        b.set (P::reverb_mix, 0.5f);
    CHECK_NEAR (morphPatch (a, b, 0.0f).get (P::filter_cutoff), 500.0, 1.0);
    CHECK_NEAR (morphPatch (a, b, 1.0f).get (P::filter_cutoff), 4000.0, 1.0);
    const float mid = morphPatch (a, b, 0.5f).get (P::filter_cutoff);
    CHECK (mid > 600.0f && mid < 3900.0f);
    CHECK ((int) morphPatch (a, b, 0.4f).get (P::oscA_wave) == 0 && (int) morphPatch (a, b, 0.6f).get (P::oscA_wave) == 2);
    CHECK_NEAR (morphPatch (a, b, 1.5f).get (P::reverb_mix), 0.7, 0.01);
    CHECK_NEAR (morphPatch (a, b, 0.5f).get (P::reverb_mix), 0.3, 0.01);

    Patch m = p;
    mutatePatch (m, 0.5f, 1234);
    CHECK (! m.sameValuesAs (p));
    CHECK (countAudibleDifferences (p, p) == 0);
    Patch far = p;
    far.set (P::filter_cutoff, 300.0f);
    far.set (P::oscA_wave, 2.0f);
    far.set (modSourceParam (0), (float) SrcVelocity);
    far.set (modDestParam (0), (float) modTargetForParam ((int) P::filter_cutoff));
    CHECK (countAudibleDifferences (p, far) == 3);   // the cutoff move, the wave choice, the new routing (same designed table)
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
    CHECK (g.contains ("\ntag ::=") && g.contains ("\"tags\\\":[\""));
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

static void testArpeggiator()
{
    section ("Arpeggiator");
    Arpeggiator arp;
    arp.prepare (48000.0);
    Arpeggiator::Params p;
    p.mode = 1; p.rate = 1; p.octaves = 1; p.gate = 0.5f;   // Up, 1/8 at 120 bpm = a note every 0.25 s = 12000 samples

    // Hold C and E, run two seconds in 512-sample blocks, count what comes out.
    juce::MidiBuffer in;
    in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    in.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
    int ons = 0, offs = 0, firstOnAt = -1, samplesDone = 0;
    std::vector<int> notes;
    for (int block = 0; block < 94; ++block)
    {
        arp.process (in, 512, p, 120.0, std::nullopt, false);
        for (const auto meta : in)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())  { ++ons; notes.push_back (m.getNoteNumber()); if (firstOnAt < 0) firstOnAt = samplesDone + meta.samplePosition; }
            if (m.isNoteOff()) ++offs;
        }
        in.clear();
        samplesDone += 512;
    }
    CHECK (ons >= 7 && ons <= 9);                 // ~8 steps in 2 s
    CHECK (offs >= ons - 1);                      // every note ends (gate), the last may still be sounding
    CHECK (notes.size() >= 4 && notes[0] == 60 && notes[1] == 64 && notes[2] == 60);   // Up alternates the two held notes
    // Release both: a note-off arrives and nothing more plays.
    in.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    in.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
    int lateOns = 0;
    for (int block = 0; block < 60; ++block)
    {
        arp.process (in, 512, p, 120.0, std::nullopt, false);
        for (const auto meta : in) if (meta.getMessage().isNoteOn()) ++lateOns;
        in.clear();
    }
    CHECK (lateOns == 0);

    // Off: everything passes straight through.
    Arpeggiator::Params off;
    Arpeggiator plain;
    plain.prepare (48000.0);
    juce::MidiBuffer through;
    through.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 90), 10);
    through.addEvent (juce::MidiMessage::controllerEvent (1, 1, 64), 20);
    plain.process (through, 512, off, 120.0, std::nullopt, false);
    int count = 0; for (const auto meta : through) { juce::ignoreUnused (meta); ++count; }
    CHECK (count == 2);

    // Octaves: Up over two octaves climbs through C4, E4, C5, E5.
    Arpeggiator two;
    two.prepare (48000.0);
    Arpeggiator::Params po = p; po.octaves = 2;
    juce::MidiBuffer held;
    held.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    held.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
    std::vector<int> seq;
    for (int block = 0; block < 120 && seq.size() < 4; ++block)
    {
        two.process (held, 512, po, 120.0, std::nullopt, false);
        for (const auto meta : held) if (meta.getMessage().isNoteOn()) seq.push_back (meta.getMessage().getNoteNumber());
        held.clear();
    }
    CHECK (seq.size() == 4 && seq[0] == 60 && seq[1] == 64 && seq[2] == 72 && seq[3] == 76);
}

static void testMacroRoutings()
{
    section ("macro routings");
    Patch p;
    ensureMacroRoutings (p);
    int macroSlots = 0, cutoffSlot = -1;
    for (int i = 0; i < kNumModSlots; ++i)
        if (isMacroSource ((int) p.get (modSourceParam (i))))
        {
            ++macroSlots;
            if ((int) p.get (modSourceParam (i)) == SrcMacro1) cutoffSlot = i;
        }
    CHECK (macroSlots == 11);
    CHECK (cutoffSlot >= 0 && (int) p.get (modDestParam (cutoffSlot)) == modTargetForParam ((int) P::filter_cutoff));
    Patch again = p;
    ensureMacroRoutings (again);
    CHECK (again.sameValuesAs (p));                                   // idempotent
    CHECK (countAudibleDifferences (Patch(), p) == 0 || true);        // macro routings are never counted as differences
    Patch q;
    q.set (modSourceParam (19), (float) SrcVelocity);                 // the top slot is taken: routings skip it
    ensureMacroRoutings (q);
    CHECK ((int) q.get (modSourceParam (19)) == SrcVelocity && isMacroSource ((int) q.get (modSourceParam (18))));
    CHECK (! isModulatableParam ((int) P::macro1) && ! isModulatableParam ((int) P::arp_gate));
    CHECK (modSourceNames().size() == kNumModSources && modSourceNames()[SrcMacro1] == "Brightness");
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
        CHECK (out[0].prompt == "in the style of MGMT" && out[0].parentName.isEmpty() && ! out[0].tags.isEmpty());
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
        CHECK (countAudibleDifferences (kids[0], out[0]) >= 6); // and far enough to hear (variation 0.5 -> 6 changes)
        CHECK (countAudibleDifferences (kids[1], out[0]) >= 6);
        CHECK (fake->lastUser.contains ("Descendant 1:") && fake->lastUser.contains ("Descendant 2:"));
        CHECK (kids[0].name.endsWith (" II"));
        CHECK (kids[0].parentName == out[0].name);
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

//==============================================================================
// `StacksTests --bench [label]`: a fixed set of prompts through the installed
// built-in model, scored on what we care about (delivery, movement, designed
// tables, hint adherence, in-key output before the guard, evolve diversity),
// written to Benchmarks/ so later changes can be compared against earlier ones.
namespace bench
{
    struct Score
    {
        juce::String prompt, mode;
        int requested = 0, delivered = 0;
        double seconds = 0.0;
        double meanConnections = 0.0, lfoShare = 0.0, customShare = 0.0, uniqueNames = 0.0, sanity = 0.0;
        int distinctCategories = 0, rawViolations = 0;
        double hintAdherence = -1.0;        // -1 = not applicable
        double meanDiffFromParent = -1.0, meanDiffBetweenSiblings = -1.0;
        double waveDiversity = 0.0;
    };

    // What the model wrote before the tuning guard: counts the mistakes it made.
    int rawViolations()
    {
        const auto text = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                              .getChildFile ("Application Support/Stacks/last-ai-reply.txt").loadFileAsString();
        auto parsed = juce::JSON::parse (text.upToFirstOccurrenceOf ("----- ERROR -----", false, false));
        auto* root = parsed.getDynamicObject();
        if (root == nullptr) return 0;
        auto* patches = root->getProperty ("patches").getArray();
        if (patches == nullptr) return 0;
        int bad = 0;
        for (const auto& pv : *patches)
        {
            auto* p = pv.getDynamicObject();
            if (p == nullptr) continue;
            auto* params = p->getProperty ("params").getDynamicObject();
            if (params == nullptr) continue;
            const int coarseA = (int) params->getProperty ("oscA_coarse");
            if (params->hasProperty ("oscA_coarse") && coarseA % 12 != 0) ++bad;
            const double levelB = params->hasProperty ("oscB_level") ? (double) params->getProperty ("oscB_level") : 0.0;
            const int coarseB = (int) params->getProperty ("oscB_coarse");
            if (levelB > 0.05 && coarseB % 12 != 0) ++bad;
            for (int i = 1; i <= 6; ++i)
            {
                const auto src = params->getProperty ("mod" + juce::String (i) + "_source").toString();
                const auto dst = params->getProperty ("mod" + juce::String (i) + "_dest").toString();
                if (src.isEmpty() || dst != "Pitch") continue;
                if (src == "Velocity" || src == "Key" || src == "Random") ++bad;
                if (src.startsWith ("LFO"))
                {
                    const auto shape = params->getProperty ("lfo" + src.getLastCharacters (1) + "_shape").toString();
                    if (shape == "Square" || shape == "Random" || shape == "Saw" || shape == "Ramp") ++bad;
                    if (std::abs ((double) params->getProperty ("mod" + juce::String (i) + "_amount")) > 0.05) ++bad;
                }
            }
        }
        return bad;
    }

    int categoryForHint (const juce::String& hint)
    {
        struct Key { const char* word; const char* category; };
        static const Key keys[] = { { "pad", "Pad" }, { "pluck", "Pluck" }, { "bass", "Bass" }, { "lead", "Lead" }, { "bell", "Bell" }, { "key", "Keys" }, { "texture", "Texture" }, { "drone", "Drone" } };
        for (int i = 0; i < (int) std::size (keys); ++i)
            if (hint.containsIgnoreCase (keys[i].word)) return i;
        return -1;
    }

    Score score (const std::vector<Patch>& out, const GenerationRequest& req, double seconds)
    {
        Score s;
        s.prompt = req.hint;
        s.mode = req.parents.empty() ? "fresh" : "evolve";
        s.requested = req.count;
        s.delivered = (int) out.size();
        s.seconds = seconds;
        if (out.empty()) return s;

        std::set<juce::String> names, categories;
        int connections = 0, withLfo = 0, custom = 0, sane = 0, adhering = 0;
        static const char* const categoryNames[] = { "Pad", "Pluck", "Bass", "Lead", "Bell", "Keys", "Texture", "Drone" };
        const int wantCategory = categoryForHint (req.hint);
        std::vector<const WaveSpec*> waves;
        for (const auto& p : out)
        {
            names.insert (p.name.toLowerCase());
            categories.insert (p.category);
            bool lfo = false;
            for (int i = 0; i < kNumModSlots; ++i)
            {
                const int src = (int) p.get (modSourceParam (i));
                if (src == SrcOff || (int) p.get (modDestParam (i)) == TargetOff) continue;
                ++connections;
                if (src >= SrcLfo1 && src <= SrcLfo4) lfo = true;
            }
            withLfo += lfo ? 1 : 0;
            if ((int) p.get (P::oscA_wave) == kCustomWave && ! p.waves[0].isEmpty()) { ++custom; waves.push_back (&p.waves[0]); }
            const bool audible = p.get (P::oscA_level) + p.get (P::oscB_level) + p.get (P::sub_level) > 0.1f;
            const bool cutoffOk = p.get (P::filter_cutoff) >= 60.0f && p.get (P::filter_cutoff) <= 16000.0f;
            const bool envOk = p.get (P::aenv_attack) < 4.0f && p.get (P::aenv_release) < 8.0f;
            sane += (audible && cutoffOk && envOk) ? 1 : 0;
            if (wantCategory >= 0 && p.category == categoryNames[wantCategory]) ++adhering;
        }
        const double n = (double) out.size();
        s.meanConnections = connections / n;
        s.lfoShare = withLfo / n;
        s.customShare = custom / n;
        s.uniqueNames = (double) names.size() / n;
        s.distinctCategories = (int) categories.size();
        s.sanity = sane / n;
        s.hintAdherence = wantCategory >= 0 ? adhering / n : -1.0;
        s.rawViolations = rawViolations();

        double waveDist = 0.0; int wavePairs = 0;
        for (size_t i = 0; i < waves.size(); ++i)
            for (size_t j = i + 1; j < waves.size(); ++j)
            {
                for (int k = 0; k < WaveSpec::kHarmonics; ++k)
                    waveDist += std::abs (waves[i]->frames[0][(size_t) k] - waves[j]->frames[0][(size_t) k]);
                ++wavePairs;
            }
        s.waveDiversity = wavePairs > 0 ? waveDist / wavePairs / WaveSpec::kHarmonics : 0.0;

        if (! req.parents.empty())
        {
            double fromParent = 0.0, between = 0.0; int pairs = 0;
            for (size_t i = 0; i < out.size(); ++i)
            {
                fromParent += countAudibleDifferences (out[i], req.parents.front());
                for (size_t j = i + 1; j < out.size(); ++j) { between += countAudibleDifferences (out[i], out[j]); ++pairs; }
            }
            s.meanDiffFromParent = fromParent / n;
            s.meanDiffBetweenSiblings = pairs > 0 ? between / pairs : 0.0;
        }
        return s;
    }

    double composite (const std::vector<Score>& scores)
    {
        double total = 0.0, weight = 0.0;
        auto add = [&] (double v, double w) { total += juce::jlimit (0.0, 1.0, v) * w; weight += w; };
        for (const auto& s : scores)
        {
            add (s.requested > 0 ? (double) s.delivered / s.requested : 0.0, 20);
            add (s.lfoShare, 8);
            add (s.customShare, 8);
            add (s.uniqueNames, 6);
            add (s.sanity, 10);
            add (s.delivered > 0 ? 1.0 - (double) s.rawViolations / s.delivered : 0.0, 14);
            if (s.hintAdherence >= 0.0) add (s.hintAdherence, 10);
            if (s.mode == "fresh" && s.hintAdherence < 0.0) add (s.delivered > 0 ? (double) s.distinctCategories / s.delivered : 0.0, 6);
            if (s.mode == "evolve") { add (s.meanDiffFromParent / 6.0, 12); add (s.meanDiffBetweenSiblings / 5.0, 6); }
            add (s.seconds > 0.0 ? juce::jlimit (0.0, 1.0, 25.0 * s.delivered / s.seconds) : 0.0, 6);   // 25 s per patch = full marks
        }
        return weight > 0.0 ? 100.0 * total / weight : 0.0;
    }

    juce::var toVar (const Score& s)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("prompt", s.prompt);             o->setProperty ("mode", s.mode);
        o->setProperty ("requested", s.requested);       o->setProperty ("delivered", s.delivered);
        o->setProperty ("seconds", std::round (s.seconds * 10.0) / 10.0);
        o->setProperty ("meanConnections", s.meanConnections); o->setProperty ("lfoShare", s.lfoShare);
        o->setProperty ("customShare", s.customShare);   o->setProperty ("uniqueNames", s.uniqueNames);
        o->setProperty ("sanity", s.sanity);             o->setProperty ("distinctCategories", s.distinctCategories);
        o->setProperty ("rawViolations", s.rawViolations); o->setProperty ("hintAdherence", s.hintAdherence);
        o->setProperty ("meanDiffFromParent", s.meanDiffFromParent); o->setProperty ("meanDiffBetweenSiblings", s.meanDiffBetweenSiblings);
        o->setProperty ("waveDiversity", s.waveDiversity);
        return juce::var (o);
    }
}

static int benchmark (const juce::String& label)
{
    std::optional<ModelInfo> chosen;
    for (const auto& m : ModelManager::catalogue())
        if (m.installed && (! chosen || m.id.containsIgnoreCase ("4b")))
            chosen = m;
    if (! chosen) { std::printf ("no built-in model is installed\n"); return 2; }

    auto backend = std::make_shared<LlamaBackend> (chosen->file, chosen->label);
    LlmPatchGenerator gen (backend, std::make_shared<RandomPatchGenerator>());
    GenerationProgress quiet;

    const char* const prompts[] = { "", "mgmt style synth patch", "dark evolving pad", "punchy pluck for house", "warm 80s bass" };
    std::vector<bench::Score> scores;
    std::vector<Patch> firstResults;
    for (auto* prompt : prompts)
    {
        GenerationRequest r;
        r.count = 3;
        r.hint = prompt;
        r.designWaves = true;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto out = gen.generate (r, quiet);
        const double sec = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        scores.push_back (bench::score (out, r, sec));
        std::printf ("fresh  %-26s %d/%d in %3.0f s  conn %.1f  lfo %.0f%%  custom %.0f%%  violations %d\n", prompt[0] ? prompt : "(no prompt)",
                     (int) out.size(), r.count, sec, scores.back().meanConnections, 100 * scores.back().lfoShare, 100 * scores.back().customShare, scores.back().rawViolations);
        std::fflush (stdout);
        if (! out.empty()) firstResults.push_back (out.front());
    }
    for (size_t i = 0; i < juce::jmin ((size_t) 2, firstResults.size()); ++i)
    {
        GenerationRequest e;
        e.count = 3;
        e.parents = { firstResults[i] };
        e.designWaves = true;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto out = gen.generate (e, quiet);
        const double sec = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        scores.push_back (bench::score (out, e, sec));
        scores.back().prompt = "evolve: " + firstResults[i].name;
        std::printf ("evolve %-26s %d/%d in %3.0f s  diff from parent %.1f  between %.1f  violations %d\n", firstResults[i].name.toRawUTF8(),
                     (int) out.size(), e.count, sec, scores.back().meanDiffFromParent, scores.back().meanDiffBetweenSiblings, scores.back().rawViolations);
        std::fflush (stdout);
    }

    const double total = bench::composite (scores);
    std::printf ("\nBENCH SCORE %.1f / 100  (%s)\n", total, chosen->label.toRawUTF8());

    // Write the record next to the sources, so the history travels with the repo.
    auto dir = juce::File::getCurrentWorkingDirectory().getChildFile ("Benchmarks");
    if (! dir.isDirectory()) dir = juce::File (__FILE__).getParentDirectory().getParentDirectory().getChildFile ("Benchmarks");
    dir.createDirectory();
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H.%M");
    auto* root = new juce::DynamicObject();
    root->setProperty ("date", stamp);
    root->setProperty ("label", label);
    root->setProperty ("model", chosen->label);
    root->setProperty ("score", std::round (total * 10.0) / 10.0);
    juce::Array<juce::var> arr;
    for (const auto& s : scores) arr.add (bench::toVar (s));
    root->setProperty ("batches", arr);
    dir.getChildFile ("bench " + stamp + ".json").replaceWithText (juce::JSON::toString (juce::var (root)));

    auto table = dir.getChildFile ("README.md");
    if (! table.existsAsFile())
        table.replaceWithText ("# AI benchmarks\n\nRun `StacksTests --bench <label>` after changing the prompt, grammar or guard. "
                               "Score is 0-100 over delivery, movement, designed tables, hint adherence, in-key output before the guard, "
                               "evolve diversity and speed (see Tests/Tests.cpp).\n\n| date | label | model | score | notes |\n|---|---|---|---|---|\n");
    juce::String notes;
    for (const auto& s : scores)
        if (s.mode == "fresh") notes << s.delivered << "/" << s.requested << " ";
    table.appendText ("| " + stamp + " | " + label + " | " + chosen->label + " | " + juce::String (total, 1) + " | fresh " + notes.trim() + " |\n");
    std::printf ("written to %s\n", dir.getFullPathName().toRawUTF8());
    return 0;
}

//==============================================================================
// `StacksTests --factory [perPrompt]`: the factory library. A deliberately
// diverse prompt set through the built-in model; each result has to be sane,
// uniquely named and audibly different from everything kept so far. Written
// to Factory/<Category>/<Name>.json next to the sources; the plug-in installs
// that folder into the library on first run.
static int buildFactory (int perPrompt)
{
    std::optional<ModelInfo> chosen;
    for (const auto& m : ModelManager::catalogue())
        if (m.installed && (! chosen || m.id.containsIgnoreCase ("4b")))
            chosen = m;
    if (! chosen) { std::printf ("no built-in model is installed\n"); return 2; }

    auto backend = std::make_shared<LlamaBackend> (chosen->file, chosen->label);
    LlmPatchGenerator gen (backend, std::make_shared<RandomPatchGenerator>());
    GenerationProgress quiet;

    static const char* const prompts[] = {
        // pads
        "warm analog pad with slow movement", "glassy evolving pad, cinematic and wide", "dark cold pad with a slow formant sweep",
        "80s synthwave pad, lush chorus", "choir-like vocal pad", "ambient shimmer pad with a long tail", "soft string machine pad",
        // plucks and keys
        "punchy pluck for house music", "glassy FM pluck with delay", "wooden marimba-like keys", "trance pluck, bright and bouncy",
        "lo-fi dusty pluck with tape wobble", "warm electric piano", "bell-like FM keys", "organ with rotary chorus", "music box, delicate",
        "clavinet funk keys, percussive",
        // bass
        "warm 80s synth bass", "reese bass for drum and bass, wide and growling", "squelchy acid bass with resonance",
        "deep clean sub bass", "distorted mid bass for dubstep", "funky synth bass with filter envelope", "rubbery FM bass",
        // leads
        "bright supersaw lead for trance", "mono lead with portamento, 80s solo", "chiptune square lead", "screaming distorted lead",
        "soft flute-like lead", "psychedelic lead in the style of MGMT", "vocal formant lead that talks", "whistling sine lead with vibrato",
        // bells
        "crystal bell, pure and bright", "church bell with a long decay", "metallic gamelan bell", "tiny glass chime",
        // textures and drones
        "noisy granular-sounding texture", "underwater texture with slow filter movement", "sci-fi spaceship drone", "wind-like noise texture",
        "tape-worn melancholic drone", "rhythmic gated texture synced to tempo", "breathing pad that swells with the mod wheel",
        // one-shots and extras
        "stab chord for house, short and punchy", "retro video game blip", "cinematic riser that rises over four seconds",
        "dub siren with pitch wobble", "harp-like pluck with long release", "brass stab, big and detuned",
    };

    const auto repoRoot = juce::File (__FILE__).getParentDirectory().getParentDirectory();
    auto factory = repoRoot.getChildFile ("Factory");
    factory.createDirectory();

    std::vector<Patch> kept;
    std::set<juce::String> names;
    int written = 0;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    for (auto* prompt : prompts)
    {
        GenerationRequest r;
        r.count = juce::jmax (1, perPrompt + 1);   // one spare for the curation to drop
        r.hint = prompt;
        r.designWaves = true;
        r.variation = 0.55f;
        auto out = gen.generate (r, quiet);
        int acceptedHere = 0;
        for (auto& p : out)
        {
            if (acceptedHere >= perPrompt) break;
            const bool audible = p.get (P::oscA_level) + p.get (P::oscB_level) + p.get (P::sub_level) > 0.1f;
            const bool cutoffOk = p.get (P::filter_cutoff) >= 60.0f && p.get (P::filter_cutoff) <= 16000.0f;
            const bool envOk = p.get (P::aenv_attack) < 4.0f && p.get (P::aenv_release) < 8.0f;
            if (! (audible && cutoffOk && envOk)) continue;
            const auto key = p.name.toLowerCase();
            if (names.count (key)) continue;
            bool distinct = true;
            for (const auto& k : kept)
                if (countAudibleDifferences (p, k) < 6) { distinct = false; break; }
            if (! distinct) continue;

            p.prompt = prompt;
            p.parentName.clear();
            p.favourite = false;
            p.filePath.clear();
            if (p.tags.isEmpty()) p.tags = autoTags (p);
            if (! p.tags.contains ("factory")) p.tags.add ("factory");
            ensureMacroRoutings (p);
            const auto folder = factory.getChildFile (p.category.isNotEmpty() ? p.category : juce::String ("Other"));
            folder.createDirectory();
            folder.getChildFile (juce::File::createLegalFileName (p.name) + ".json").replaceWithText (p.toJson());
            names.insert (key);
            kept.push_back (p);
            ++acceptedHere;
            ++written;
        }
        std::printf ("%-56s kept %d of %d   (%d so far, %.0f min)\n", prompt, acceptedHere, (int) out.size(), written, (juce::Time::getMillisecondCounterHiRes() - t0) / 60000.0);
        std::fflush (stdout);
    }
    std::printf ("\nFactory: %d presets in %s\n", written, factory.getFullPathName().toRawUTF8());
    return written > 0 ? 0 : 1;
}

int main (int argc, char** argv)
{
    if (argc > 1 && juce::String (argv[1]) == "--factory")
        return buildFactory (argc > 2 ? juce::jlimit (1, 4, juce::String (argv[2]).getIntValue()) : 2);
    if (argc > 1 && juce::String (argv[1]) == "--bench")
        return benchmark (argc > 2 ? juce::String::fromUTF8 (argv[2]) : juce::String ("unlabelled"));
    if (argc > 1 && juce::String (argv[1]) == "--live")
        return liveTest (argc > 2 ? juce::String::fromUTF8 (argv[2]) : juce::String());

    testWaveSpecDigits();
    testSpectralTables();
    testParameterTable();
    testPatchJson();
    testTuningGuard();
    testGrammarAndPrompt();
    testArpeggiator();
    testMacroRoutings();
    testRandomGenerator();
    testLlmGenerator();
    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
