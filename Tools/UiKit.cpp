// Paints every screen of the Stacks editor to PNG files, without a window.
//
//   StacksUiKit <output directory> [scale = 2] [preset.json]
//
// The screens show the given preset (or a factory pad) so knobs, displays and
// the stack carry real values. Hand the folder to a designer; see Design/UI handoff.md.
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <functional>

#include "Skin.h"
#include "HomeView.h"
#include "GardenView.h"
#include "Controls.h"
#include "StacksLookAndFeel.h"

namespace
{
    // Renders one piece of the current look into a PNG at `k` image px per logical px.
    struct Exporter
    {
        juce::File dir;
        float k = 2.0f;
        int written = 0;

        void save (const juce::String& name, float w, float h, const std::function<void (juce::Graphics&)>& paint)
        {
            juce::Image image (juce::Image::ARGB, (int) std::round (w * k), (int) std::round (h * k), true);
            {
                juce::Graphics g (image);
                g.addTransform (juce::AffineTransform::scale (k));
                paint (g);
            }
            const auto file = dir.getChildFile (name + ".png");
            file.deleteFile();
            juce::FileOutputStream out (file);
            if (out.openedOk() && juce::PNGImageFormat().writeImageToStream (image, out))
                ++written;
        }

        void run (stacks::StacksLookAndFeel& lnf)
        {
            using namespace stacks;
            dir.createDirectory();
            const auto widget = lnf.findColour (juce::TextButton::buttonColourId);
            const auto comboBg = lnf.findColour (juce::ComboBox::backgroundColourId);
            const auto arrow = lnf.findColour (juce::ComboBox::arrowColourId);

            // Knobs: 101 frames top to bottom, the body only (the coloured arc stays code-drawn).
            juce::Slider probe;
            const auto rot = probe.getRotaryParameters();
            auto strip = [&] (const juce::String& name, float side)
            {
                const int frames = 101;
                save (name, side, side * (float) frames, [&] (juce::Graphics& g)
                {
                    for (int i = 0; i < frames; ++i)
                    {
                        const float pos = (float) i / (float) (frames - 1);
                        const float angle = rot.startAngleRadians + pos * (rot.endAngleRadians - rot.startAngleRadians);
                        StacksLookAndFeel::drawKnobBody (g, { 0.0f, side * (float) i, side, side }, angle, true, colours::accent);
                    }
                });
            };
            strip ("knob", 48.0f);
            strip ("knob-master", 44.0f);

            // Buttons (nine-slice, 8 px margins) and tabs.
            auto button = [&] (const juce::String& name, bool on, bool hover, bool down)
            {
                save (name, 64.0f, 24.0f, [&] (juce::Graphics& g)
                {
                    StacksLookAndFeel::drawButtonFace (g, juce::Rectangle<float> (0.0f, 0.0f, 64.0f, 24.0f).reduced (0.5f), on ? colours::accentDim : widget, on, hover, down);
                });
            };
            button ("button", false, false, false);
            button ("button-hover", false, true, false);
            button ("button-down", false, false, true);
            button ("button-on", true, false, false);
            save ("button-bright", 64.0f, 24.0f, [&] (juce::Graphics& g)
            {
                StacksLookAndFeel::drawButtonFace (g, juce::Rectangle<float> (0.0f, 0.0f, 64.0f, 24.0f).reduced (0.5f), colours::accent, false, false, false);
            });
            save ("tab-on", 80.0f, 24.0f, [&] (juce::Graphics& g)
            {
                g.setColour (colours::accent);
                g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 22.0f, 80.0f, 2.0f).reduced (6.0f, 0.0f), 1.0f);
            });
            save ("tab-hover", 80.0f, 24.0f, [&] (juce::Graphics& g)
            {
                g.setColour (juce::Colours::white.withAlpha (0.035f));
                g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f, 80.0f, 24.0f).reduced (1.0f), 4.0f);
            });
            save ("toggle-off", 17.0f, 17.0f, [&] (juce::Graphics& g) { StacksLookAndFeel::drawToggleBox (g, { 2.0f, 2.0f, 13.0f, 13.0f }, false, false, colours::accent); });
            save ("toggle-on",  17.0f, 17.0f, [&] (juce::Graphics& g) { StacksLookAndFeel::drawToggleBox (g, { 2.0f, 2.0f, 13.0f, 13.0f }, true,  false, colours::accent); });
            save ("combo", 80.0f, 22.0f, [&] (juce::Graphics& g) { StacksLookAndFeel::drawComboFace (g, { 0.0f, 0.0f, 80.0f, 22.0f }, comboBg, true, arrow); });

            // Slider: track, fill, thumb.
            save ("slider-track", 80.0f, 11.0f, [&] (juce::Graphics& g) { g.setColour (juce::Colour (0xff2a2f36)); g.fillRoundedRectangle (0.0f, 4.0f, 80.0f, 3.0f, 1.5f); });
            save ("slider-fill",  80.0f, 11.0f, [&] (juce::Graphics& g) { g.setColour (colours::accent); g.fillRoundedRectangle (0.0f, 4.0f, 80.0f, 3.0f, 1.5f); });
            save ("slider-thumb", 12.0f, 12.0f, [&] (juce::Graphics& g)
            {
                g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillEllipse (1.0f, 2.0f, 10.0f, 10.0f);
                g.setColour (colours::text); g.fillEllipse (1.0f, 1.0f, 10.0f, 10.0f);
            });

            // Surfaces.
            save ("card",  64.0f, 64.0f, [&] (juce::Graphics& g) { StacksLookAndFeel::drawCard (g, { 0.0f, 0.0f, 64.0f, 64.0f }); });
            save ("inset", 64.0f, 64.0f, [&] (juce::Graphics& g) { StacksLookAndFeel::drawInset (g, { 0.0f, 0.0f, 64.0f, 64.0f }); });
            save ("texteditor", 80.0f, 24.0f, [&] (juce::Graphics& g)
            {
                g.setColour (colours::background); g.fillRoundedRectangle (0.5f, 0.5f, 79.0f, 23.0f, 5.0f);
                g.setColour (colours::outline);    g.drawRoundedRectangle (0.5f, 0.5f, 79.0f, 23.0f, 5.0f, 1.0f);
            });
            save ("popup", 80.0f, 80.0f, [&] (juce::Graphics& g)
            {
                g.setColour (colours::panel);   g.fillRoundedRectangle (0.0f, 0.0f, 80.0f, 80.0f, 6.0f);
                g.setColour (colours::outline); g.drawRoundedRectangle (0.5f, 0.5f, 79.0f, 79.0f, 6.0f, 1.0f);
            });
            save ("scrollbar-thumb", 8.0f, 40.0f, [&] (juce::Graphics& g) { g.setColour (juce::Colour (0xff3a4048)); g.fillRoundedRectangle (0.0f, 0.0f, 8.0f, 40.0f, 4.0f); });
            save ("background", 1500.0f, 860.0f, [&] (juce::Graphics& g) { g.fillAll (colours::background); });
            save ("header", 1500.0f, 44.0f, [&] (juce::Graphics& g)
            {
                g.fillAll (colours::background);
                g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillRect (0.0f, 43.0f, 1500.0f, 1.0f);
            });
            save ("lab", 392.0f, 400.0f, [&] (juce::Graphics& g)
            {
                g.setColour (colours::panel); g.fillRoundedRectangle (0.0f, 0.0f, 392.0f, 400.0f, 7.0f);
                g.setColour (juce::Colours::white.withAlpha (0.04f)); g.drawRoundedRectangle (0.5f, 0.5f, 391.0f, 399.0f, 7.0f, 1.0f);
            });

            // Home tiles, one per kind, normal and hover.
            struct Kind { const char* name; juce::uint32 colour; };
            const Kind kinds[] = { { "pad", 0xff7fa7d8 }, { "keys", 0xffe0c070 }, { "bass", 0xffe8775a }, { "lead", 0xfff2a541 },
                                   { "pluck", 0xff5ec8c0 }, { "bell", 0xffd08ab8 }, { "texture", 0xff9ad17b } };
            for (const auto& kind : kinds)
                for (bool hover : { false, true })
                    save (juce::String ("tile-") + kind.name + (hover ? "-hover" : ""), 334.0f, 222.0f, [&] (juce::Graphics& g)
                    {
                        HomeView::paintTileFace (g, juce::Rectangle<float> (0.0f, 0.0f, 334.0f, 222.0f).reduced (2.0f), juce::Colour (kind.colour), hover);
                    });

            // Blocks and the slab, painted white: the app tints them with each sound's colour.
            auto block = [&] (const juce::String& name, float w, float h, float depth, bool lit)
            {
                save (name, w, h, [&] (juce::Graphics& g)
                {
                    GardenView::Style style;
                    style.colour = juce::Colours::white;
                    style.depth = depth;
                    GardenView::paintBlock (g, { 0.0f, depth, w - depth, h - depth }, style, {}, lit, false, 0);
                });
            };
            block ("block", 120.0f, 60.0f, 10.0f, false);
            block ("block-selected", 120.0f, 60.0f, 10.0f, true);
            block ("slab", 220.0f, 34.0f, 8.0f, false);

            // The link between two effect cards, in white (tinted with the row colour).
            auto link = [&] (const juce::String& name, bool on)
            {
                save (name, 22.0f, 14.0f, [&] (juce::Graphics& g)
                {
                    const float y = 7.0f, x0 = 1.0f, x1 = 21.0f;
                    g.setColour (juce::Colours::white.withAlpha (on ? 0.9f : 0.6f));
                    if (on) g.drawLine (x0, y, x1 - 5.0f, y, 1.6f);
                    else
                    {
                        const float dashes[] = { 3.0f, 3.0f };
                        juce::Path dash; dash.startNewSubPath (x0, y); dash.lineTo (x1 - 5.0f, y);
                        juce::Path dashed; juce::PathStrokeType (1.4f).createDashedStroke (dashed, dash, dashes, 2);
                        g.fillPath (dashed);
                    }
                    juce::Path arrow; arrow.addTriangle (x1 - 6.0f, y - 4.5f, x1 - 6.0f, y + 4.5f, x1 + 1.0f, y); g.fillPath (arrow);
                    const float cx = 9.0f;
                    g.setColour (colours::background); g.fillEllipse (cx - 5.0f, y - 5.0f, 10.0f, 10.0f);
                    g.setColour (juce::Colours::white.withAlpha (on ? 1.0f : 0.6f)); g.drawEllipse (cx - 5.0f, y - 5.0f, 10.0f, 10.0f, 1.2f);
                    if (on) g.fillEllipse (cx - 2.5f, y - 2.5f, 5.0f, 5.0f);
                });
            };
            link ("link-on", true);
            link ("link-off", false);

            // Keyboard keys.
            for (auto state : { "", "-hover", "-down" })
            {
                const bool down = juce::String (state) == "-down", over = juce::String (state) == "-hover";
                save (juce::String ("key-white") + state, 28.0f, 58.0f, [&] (juce::Graphics& g) { StacksKeyboard::paintWhiteKey (g, { 0.0f, 0.0f, 28.0f, 58.0f }, down, over); });
                save (juce::String ("key-black") + state, 18.0f, 36.0f, [&] (juce::Graphics& g) { StacksKeyboard::paintBlackKey (g, { 0.0f, 0.0f, 18.0f, 36.0f }, down, over); });
            }

            dir.getChildFile ("skin.json").replaceWithText (R"({
  "scale": 2,
  "knobArc": true,
  "slices": {
    "card": [10, 10, 10, 10], "inset": [8, 8, 8, 8], "lab": [12, 12, 12, 12], "popup": [10, 10, 10, 10],
    "button": [8, 8, 8, 8], "button-hover": [8, 8, 8, 8], "button-down": [8, 8, 8, 8], "button-on": [8, 8, 8, 8], "button-bright": [8, 8, 8, 8],
    "tab-on": [8, 8, 8, 8], "tab-hover": [8, 8, 8, 8], "combo": [8, 8, 20, 8], "texteditor": [8, 8, 8, 8],
    "slider-track": [6, 0, 6, 0], "slider-fill": [6, 0, 6, 0], "scrollbar-thumb": [4, 4, 4, 4]
  }
})");
        }
    };
}


namespace
{
    // What a new user does on first launch, without a window: pick Pad, wait
    // for the five sounds, play one, save it, load it back. Fails loudly.
    int smokeTest()
    {
        using namespace stacks;
        int failures = 0;
        auto check = [&] (bool ok, const juce::String& what)
        {
            std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
            if (! ok) ++failures;
        };
        auto visibleWordsOk = [] (const juce::String& text)
        {
            for (auto* bad : { "AI ", " AI", "Qwen", "qwen", "llama", "Llama", "Ollama", "model", "Model", "GGUF", "gguf" })
                if (text.contains (bad)) return false;
            return true;
        };

        std::printf ("Stacks smoke test\n");
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto processor = std::make_unique<StacksAudioProcessor>();
        processor->prepareToPlay (48000.0, 512);
        const auto library = StacksAudioProcessor::libraryRoot();
        int presets = 0;
        for (const auto& f : library.getChildFile ("Factory").findChildFiles (juce::File::findFiles, true, "*.json")) { (void) f; ++presets; }
        // The presets are copied from the app bundle; this bare tool has none, so the check only applies inside one.
        const bool bundled = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getParentDirectory()
                                 .getChildFile ("Resources").getChildFile ("Factory").isDirectory();
        if (bundled) check (presets >= 100, "factory presets installed: " + juce::String (presets) + " in " + library.getFullPathName());
        else         std::printf ("  skip  factory presets (no app bundle around this tool)\n");

        processor->requestFromKind ("Pad");
        juce::StringArray statuses;
        double firstAt = -1.0;
        const double deadline = juce::Time::getMillisecondCounterHiRes() + 15.0 * 60.0 * 1000.0;
        bool started = false;
        while (juce::Time::getMillisecondCounterHiRes() < deadline)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            const auto& lab = processor->lab();
            started = started || lab.generating || processor->isDownloading();
            for (const auto& text : { lab.status, lab.progressDetail })
                if (text.isNotEmpty() && ! statuses.contains (text)) statuses.add (text);
            if (firstAt < 0.0 && ! lab.candidates.empty()) firstAt = juce::Time::getMillisecondCounterHiRes();
            if (started && ! lab.generating && ! processor->isDownloading()) break;
        }
        const auto& lab = processor->lab();
        const auto secs = [&] (double at) { return juce::String ((at - t0) / 1000.0, 1) + " s"; };
        check (started, "a Pad batch started");
        check (! processor->isDownloading(), "no download needed (engine data found)");
        check ((int) lab.candidates.size() == StacksAudioProcessor::kAiPatchesPerBatch,
               "sounds grown: " + juce::String ((int) lab.candidates.size()) + (firstAt > 0 ? ", first after " + secs (firstAt) : juce::String()) + ", all after " + secs (juce::Time::getMillisecondCounterHiRes()));
        int grown = 0;
        for (const auto& c : lab.candidates) if (c.isGrown()) ++grown;
        check (grown == (int) lab.candidates.size() && grown > 0, juce::String (grown) + " designed by the engine, not the fallback");
        bool wordsOk = true;
        for (const auto& t : statuses)
        {
            const bool ok = visibleWordsOk (t);
            wordsOk = wordsOk && ok;
            if (! ok) std::printf ("        shown: %s\n", t.toRawUTF8());
        }
        std::printf ("        %d distinct status lines; first \"%s\", last \"%s\"\n", statuses.size(),
                     statuses.isEmpty() ? "" : statuses[0].toRawUTF8(), statuses.isEmpty() ? "" : statuses[statuses.size() - 1].toRawUTF8());
        for (const auto& c : lab.candidates) wordsOk = wordsOk && visibleWordsOk (c.name) && visibleWordsOk (c.description);
        check (wordsOk, "nothing shown names the engine");

        if (! lab.candidates.empty())
        {
            processor->audition (0);
            juce::AudioBuffer<float> buffer (2, 512);
            double sum = 0.0; long count = 0; bool finite = true;
            for (int block = 0; block < 190; ++block)   // two seconds: note on, held, released
            {
                juce::MidiBuffer midi;
                if (block == 4)   midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                if (block == 150) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
                buffer.clear();
                processor->processBlock (buffer, midi);
                if (block >= 10 && block < 150)
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 512; ++i)
                        {
                            const float v = buffer.getSample (ch, i);
                            finite = finite && std::isfinite (v);
                            sum += (double) v * v; ++count;
                        }
            }
            const double rms = count > 0 ? std::sqrt (sum / (double) count) : 0.0;
            check (finite, "audio is finite");
            check (rms > 0.003 && rms < 1.0, "\"" + lab.candidates[0].name + "\" makes sound (rms " + juce::String (rms, 4) + ")");

            const auto mine = library.getChildFile ("My Sounds");
            mine.createDirectory();
            const auto saved = processor->savePreset ("Smoke Test", mine, true);
            check (saved.existsAsFile(), "saved to " + saved.getFullPathName());
            check (saved.existsAsFile() && processor->loadLibraryPatch (saved), "loaded back");
            saved.deleteFile();
        }
        processor.reset();
        std::printf ("%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures, failures == 1 ? "" : "s");
        return failures == 0 ? 0 : 1;
    }
}

int main (int argc, char** argv)
{
    if (argc < 2 || (argc == 2 && juce::String (argv[1]) != "--smoke"))
    {
        std::printf ("usage: StacksUiKit <output directory> [scale] [preset.json]   (screens labelled with skin file names)\n"
                     "       StacksUiKit --export-skin <output directory> [scale]\n"
                     "       StacksUiKit --plain <output directory> [scale]  (screens without the file-name labels)\n"
                     "       StacksUiKit --smoke                              (first-launch test: grow five pads, play, save)\n");
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI init;
    if (juce::String (argv[1]) == "--smoke")
        return smokeTest();
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const bool plain = juce::String (argv[1]) == "--plain" && argc > 2;   // screens without the file-name labels
    if (juce::String (argv[1]) == "--export-skin" && argc > 2)
    {
        stacks::StacksLookAndFeel lnf;
        Exporter e;
        e.dir = cwd.getChildFile (argv[2]);
        e.k = argc > 3 ? (float) juce::String (argv[3]).getDoubleValue() : 2.0f;
        if (e.k <= 0.0f) e.k = 2.0f;
        e.run (lnf);
        std::printf ("%d skin files written to %s\n", e.written, e.dir.getFullPathName().toRawUTF8());
        return 0;
    }
    const int first = plain ? 2 : 1;
    const auto outDir = cwd.getChildFile (argv[first]);
    const float scale = argc > first + 1 ? (float) juce::String (argv[first + 1]).getDoubleValue() : 2.0f;
    const auto repo = juce::File (__FILE__).getParentDirectory().getParentDirectory();
    const auto preset = argc > first + 2 ? cwd.getChildFile (argv[first + 2]) : repo.getChildFile ("Factory/Pad/Warm Analog.json");

    auto processor = std::make_unique<stacks::StacksAudioProcessor>();
    processor->prepareToPlay (48000.0, 512);
    if (preset.existsAsFile())
        processor->loadLibraryPatch (preset);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
    auto* stacksEditor = dynamic_cast<stacks::StacksAudioProcessorEditor*> (editor.get());
    if (stacksEditor == nullptr)
        return 1;
    stacksEditor->setSize (1500, 860);
    stacksEditor->renderScreens (outDir, scale > 0.0f ? scale : 2.0f, ! plain);
    std::printf ("screens written to %s\n", outDir.getFullPathName().toRawUTF8());
    editor.reset();
    processor.reset();
    return 0;
}
