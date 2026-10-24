#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

namespace stacks
{

// A short first-visit tour: dims the window, lights one area at a time and
// puts one line of text next to it, with Next and Skip. Lives on top of the
// editor; deletes nothing, owns nothing but its two buttons. Never takes the
// keyboard, so a host's musical typing keeps working while it is up.
class TourOverlay : public juce::Component
{
public:
    struct Step
    {
        std::function<juce::Rectangle<int>()> target;   // in the overlay's coordinates; empty = centred message
        juce::String text;
    };

    TourOverlay();

    // Shows the steps from the first; `onDone` runs when the last is passed or Skip is pressed.
    void start (std::vector<Step> steps, std::function<void()> onDone);
    void finish();
    bool isRunning() const noexcept { return running; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override {}   // clicks outside the bubble are swallowed: only Next and Skip move the tour

private:
    void show (int index);
    juce::Rectangle<int> bubbleBounds() const;

    std::vector<Step> steps;
    std::function<void()> onDone;
    int current = 0;
    bool running = false;
    juce::Rectangle<int> hole;
    juce::TextButton nextButton { "Next" }, skipButton { "Skip" };
};

} // namespace stacks
