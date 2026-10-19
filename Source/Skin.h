#pragma once

#include <juce_graphics/juce_graphics.h>
#include <map>

namespace stacks
{

// Bitmap skin: PNG files that replace the code-drawn look piece by piece.
// Every piece has a name ("knob", "card", "tile-pad"); when a file with that
// name exists the UI draws the image, otherwise it draws itself as before, so
// a skin can arrive one file at a time. Images are painted at 2x (one logical
// pixel = two image pixels) unless skin.json says otherwise.
//
// Looked up in order, later folders override earlier ones:
//   <bundle>/Contents/Resources/Skin          what ships with Stacks
//   ~/Music/Stacks Patches/skin               the user's (or designer's) drop folder
//   $STACKS_SKIN_DIR                          for tools
//
// Design/Skin spec.md lists every name with its size, frames and slice margins.
namespace skin
{
    void load();                                   // (re)reads every folder; call before painting (the editor does on open)
    void loadFrom (const juce::File& folder);      // adds one folder on top
    bool any();                                    // at least one image is loaded
    bool has (const juce::String& name);
    const juce::Image* image (const juce::String& name);   // null when absent
    float scale();                                 // image px per logical px (2 by default)
    bool knobArc();                                // draw the coloured value arc over a skinned knob (default true)
    juce::File userFolder();

    // The image stretched over `area` (logical px).
    void drawStretched (juce::Graphics&, const juce::String& name, juce::Rectangle<float> area, float opacity = 1.0f);
    // The image at its own logical size, centred in `area`.
    void drawCentred (juce::Graphics&, const juce::String& name, juce::Rectangle<float> area, float opacity = 1.0f);
    // Nine-slice: corners keep their size, edges and middle stretch. Margins in
    // logical px come from skin.json ("slices": {"card": [l, t, r, b]}) or `fallbackMargin`.
    void drawNine (juce::Graphics&, const juce::String& name, juce::Rectangle<float> area, float fallbackMargin, float opacity = 1.0f);
    // Filmstrip: square frames stacked top to bottom; `position` 0..1 picks the frame.
    void drawFilmstrip (juce::Graphics&, const juce::String& name, juce::Rectangle<float> area, float position, float opacity = 1.0f);
    // The image multiplied by a colour (paint the piece in white or grey, the app colours it).
    void drawTinted (juce::Graphics&, const juce::String& name, juce::Rectangle<float> area, juce::Colour tint, float opacity = 1.0f);
}

} // namespace stacks
