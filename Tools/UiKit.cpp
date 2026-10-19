// Paints every screen of the Stacks editor to PNG files, without a window.
//
//   StacksUiKit <output directory> [scale = 2] [preset.json]
//
// The screens show the given preset (or a factory pad) so knobs, displays and
// the stack carry real values. Hand the folder to a designer; see Design/UI handoff.md.
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf ("usage: StacksUiKit <output directory> [scale] [preset.json]\n");
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI init;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto outDir = cwd.getChildFile (argv[1]);
    const float scale = argc > 2 ? (float) juce::String (argv[2]).getDoubleValue() : 2.0f;
    const auto repo = juce::File (__FILE__).getParentDirectory().getParentDirectory();
    const auto preset = argc > 3 ? cwd.getChildFile (argv[3]) : repo.getChildFile ("Factory/Pad/Warm Analog.json");

    auto processor = std::make_unique<stacks::StacksAudioProcessor>();
    processor->prepareToPlay (48000.0, 512);
    if (preset.existsAsFile())
        processor->loadLibraryPatch (preset);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
    auto* stacksEditor = dynamic_cast<stacks::StacksAudioProcessorEditor*> (editor.get());
    if (stacksEditor == nullptr)
        return 1;
    stacksEditor->setSize (1500, 860);
    stacksEditor->renderScreens (outDir, scale > 0.0f ? scale : 2.0f);
    std::printf ("screens written to %s\n", outDir.getFullPathName().toRawUTF8());
    editor.reset();
    processor.reset();
    return 0;
}
