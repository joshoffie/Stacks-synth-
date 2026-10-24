#include "TourOverlay.h"
#include "Controls.h"
#include "StacksLookAndFeel.h"

namespace stacks
{

namespace
{
    constexpr int kBubbleW = 300, kBubbleH = 96, kGap = 14;
}

TourOverlay::TourOverlay()
{
    setInterceptsMouseClicks (true, true);
    setWantsKeyboardFocus (false);
    for (auto* b : { &nextButton, &skipButton })
    {
        b->setWantsKeyboardFocus (false);
        b->setMouseClickGrabsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }
    nextButton.setColour (juce::TextButton::buttonColourId, colours::accent);
    nextButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    nextButton.onClick = [this] { show (current + 1); };
    skipButton.onClick = [this] { finish(); };
    setVisible (false);
}

void TourOverlay::start (std::vector<Step> s, std::function<void()> done)
{
    if (s.empty()) return;
    steps = std::move (s);
    onDone = std::move (done);
    running = true;
    setVisible (true);
    toFront (false);
    show (0);
}

void TourOverlay::finish()
{
    if (! running) return;
    running = false;
    setVisible (false);
    steps.clear();
    if (auto done = std::move (onDone)) done();
}

void TourOverlay::show (int index)
{
    if (index >= (int) steps.size()) { finish(); return; }
    current = index;
    hole = steps[(size_t) current].target ? steps[(size_t) current].target().expanded (6) : juce::Rectangle<int>();
    nextButton.setButtonText (current + 1 == (int) steps.size() ? "Got it" : "Next");
    skipButton.setVisible (current + 1 < (int) steps.size());
    resized();
    repaint();
}

// Below the lit area if there is room, else above, else beside; always on screen.
juce::Rectangle<int> TourOverlay::bubbleBounds() const
{
    const auto area = getLocalBounds().reduced (12);
    if (hole.isEmpty())
        return juce::Rectangle<int> (kBubbleW, kBubbleH).withCentre (area.getCentre());
    juce::Rectangle<int> b (kBubbleW, kBubbleH);
    if (hole.getBottom() + kGap + kBubbleH <= area.getBottom())      b.setPosition (hole.getX(), hole.getBottom() + kGap);
    else if (hole.getY() - kGap - kBubbleH >= area.getY())           b.setPosition (hole.getX(), hole.getY() - kGap - kBubbleH);
    else if (hole.getRight() + kGap + kBubbleW <= area.getRight())   b.setPosition (hole.getRight() + kGap, hole.getCentreY() - kBubbleH / 2);
    else                                                             b.setPosition (hole.getX() - kGap - kBubbleW, hole.getCentreY() - kBubbleH / 2);
    return b.constrainedWithin (area);
}

void TourOverlay::resized()
{
    auto b = bubbleBounds().reduced (12).removeFromBottom (26);
    nextButton.setBounds (b.removeFromRight (78));
    b.removeFromRight (8);
    skipButton.setBounds (b.removeFromRight (64));
}

void TourOverlay::paint (juce::Graphics& g)
{
    if (! running) return;
    // dim everything but the lit area
    juce::Path shade;
    shade.addRectangle (getLocalBounds().toFloat());
    if (! hole.isEmpty())
    {
        shade.addRoundedRectangle (hole.toFloat(), 8.0f);
        shade.setUsingNonZeroWinding (false);
    }
    g.setColour (juce::Colours::black.withAlpha (0.62f));
    g.fillPath (shade);
    if (! hole.isEmpty())
    {
        g.setColour (colours::accent);
        g.drawRoundedRectangle (hole.toFloat(), 8.0f, 2.0f);
    }

    const auto b = bubbleBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (b.translated (0.0f, 3.0f), 10.0f);
    g.setColour (colours::panel.brighter (0.08f));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (colours::accent.withAlpha (0.8f));
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.2f);

    auto text = b.reduced (14.0f, 12.0f);
    g.setColour (colours::muted);
    g.setFont (StacksLookAndFeel::font (11.0f, true));
    g.drawText (juce::String (current + 1) + " of " + juce::String ((int) steps.size()), text.removeFromTop (14.0f), juce::Justification::topLeft);
    text.removeFromTop (4.0f);
    g.setColour (colours::text);
    g.setFont (StacksLookAndFeel::font (14.5f, true));
    g.drawFittedText (steps[(size_t) current].text, text.withTrimmedBottom (30.0f).toNearestInt(), juce::Justification::topLeft, 2);
}

} // namespace stacks
