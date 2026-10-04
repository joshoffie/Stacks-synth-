#include "TunerView.h"
#include "PluginProcessor.h"
#include "Controls.h"
#include "StacksLookAndFeel.h"

namespace stacks
{

namespace
{
    constexpr int kWindow = 4096;

    juce::String noteName (int midi)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
    }
}

TunerView::TunerView (StacksAudioProcessor& p) : processor (p)
{
    window.assign ((size_t) kWindow, 0.0f);
    autoTuneButton.setButtonText ("Auto-tune");
    autoTuneButton.setTooltip ("Puts this patch in tune without changing its character: oscillators snap to octaves, fine tune is centred, "
                               "pitch modulation is capped, and whatever the tuner still hears off is compensated.");
    autoTuneButton.onClick = [this]
    {
        flash = processor.autoTune (hearing ? cents : 0.0f);
        flashUntil = juce::Time::getMillisecondCounterHiRes() + 4000.0;
        repaint();
    };
    addAndMakeVisible (autoTuneButton);
    setTooltip ("Hold one note and the tuner shows how far the sound sits from it. Green within 8 cents.");
    startTimerHz (8);
}

TunerView::~TunerView() { stopTimer(); }

void TunerView::resized()
{
    autoTuneButton.setBounds (getLocalBounds().removeFromRight (68).withSizeKeepingCentre (66, 24));
}

// Normalised autocorrelation (McLeod-style): the first clear peak gives the period.
float TunerView::detectPitch (const float* x, int n, double sr, float& clarity)
{
    clarity = 0.0f;
    const int minLag = juce::jmax (2, (int) (sr / 1500.0));
    const int maxLag = juce::jmin (n / 2, (int) (sr / 32.0));
    std::vector<float> nsdf ((size_t) maxLag + 2, 0.0f);
    for (int tau = minLag; tau <= maxLag; ++tau)
    {
        double acf = 0.0, m = 0.0;
        for (int i = 0; i + tau < n; ++i)
        {
            acf += (double) x[i] * x[i + tau];
            m   += (double) x[i] * x[i] + (double) x[i + tau] * x[i + tau];
        }
        nsdf[(size_t) tau] = m > 0.0 ? (float) (2.0 * acf / m) : 0.0f;
    }
    float best = 0.0f;
    for (int tau = minLag; tau <= maxLag; ++tau) best = juce::jmax (best, nsdf[(size_t) tau]);
    if (best < 0.6f) return 0.0f;
    const float threshold = 0.85f * best;
    for (int tau = minLag + 1; tau < maxLag; ++tau)
    {
        const float a = nsdf[(size_t) tau - 1], b = nsdf[(size_t) tau], c = nsdf[(size_t) tau + 1];
        if (b > threshold && b >= a && b >= c)
        {
            const float denom = a - 2.0f * b + c;
            const float shift = std::abs (denom) > 1.0e-9f ? (a - c) / (2.0f * denom) : 0.0f;
            clarity = b;
            return (float) sr / ((float) tau + shift);
        }
    }
    return 0.0f;
}

void TunerView::timerCallback()
{
    processor.copyRecentDry (window.data(), kWindow);
    double sumSq = 0.0;
    for (float v : window) sumSq += (double) v * v;
    const float rms = (float) std::sqrt (sumSq / kWindow);
    const int played = processor.lastPlayedNote();
    const bool wasHearing = hearing;

    if (rms < 0.004f || played < 0)
    {
        hearing = false;
        recent.clear();
    }
    else
    {
        float clarity = 0.0f;
        const float f = detectPitch (window.data(), kWindow, processor.getSampleRate() > 0 ? processor.getSampleRate() : 48000.0, clarity);
        if (f > 20.0f && clarity > 0.8f)
        {
            // Compare with the note played, or its octave neighbours (autocorrelation can land an octave off).
            float bestCents = 1.0e9f;
            for (int octave = -2; octave <= 2; ++octave)
            {
                const float target = 440.0f * std::pow (2.0f, (float) (played + 12 * octave - 69) / 12.0f);
                const float c = 1200.0f * std::log2 (f / target);
                if (std::abs (c) < std::abs (bestCents)) bestCents = c;
            }
            if (std::abs (bestCents) < 400.0f)
            {
                recent.push_back (bestCents);
                if (recent.size() > 5) recent.erase (recent.begin());
                auto sorted = recent;
                std::sort (sorted.begin(), sorted.end());
                cents = sorted[sorted.size() / 2];
                note = played;
                hearing = true;
            }
        }
        else
            hearing = false;
    }

    const bool flashing = juce::Time::getMillisecondCounterHiRes() < flashUntil;
    if (! flashing && flash.isNotEmpty()) flash.clear();
    if (hearing || wasHearing || flashing || flash.isNotEmpty())
        repaint();
}

void TunerView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().withTrimmedRight (72.0f);
    g.setColour (colours::background);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

    if (flash.isNotEmpty())
    {
        g.setColour (colours::accent);
        g.setFont (StacksLookAndFeel::font (10.5f));
        g.drawFittedText (flash, r.reduced (6.0f, 2.0f).toNearestInt(), juce::Justification::centredLeft, 3, 0.9f);
        return;
    }

    auto top = r.removeFromTop (r.getHeight() * 0.55f).reduced (8.0f, 2.0f);
    const bool inTune = hearing && std::abs (cents) <= 8.0f;
    const auto colour = ! hearing ? colours::muted : inTune ? juce::Colour (0xff5fcf8a) : std::abs (cents) < 25.0f ? colours::accent : juce::Colour (0xffe06060);

    g.setColour (hearing ? colours::text : colours::muted);
    g.setFont (StacksLookAndFeel::font (15.0f, true));
    g.drawText (hearing ? noteName (note) : juce::String ("Tuner"), top.removeFromLeft (46.0f), juce::Justification::centredLeft, false);
    g.setColour (colour);
    g.setFont (StacksLookAndFeel::font (12.0f, true));
    g.drawText (hearing ? (cents >= 0.0f ? "+" : "") + juce::String ((int) std::round (cents)) + juce::String::fromUTF8 (" \xc2\xa2 ") + (inTune ? "in tune" : cents > 0 ? "sharp" : "flat")
                        : juce::String ("hold a note"),
                top, juce::Justification::centredLeft, false);

    // Needle: -50 .. +50 cents
    auto scale = r.reduced (8.0f, 3.0f);
    g.setColour (colours::muted.withAlpha (0.35f));
    g.drawHorizontalLine ((int) scale.getCentreY(), scale.getX(), scale.getRight());
    for (int c = -50; c <= 50; c += 25)
    {
        const float x = scale.getX() + (c + 50.0f) / 100.0f * scale.getWidth();
        g.drawVerticalLine ((int) x, scale.getCentreY() - (c == 0 ? 5.0f : 3.0f), scale.getCentreY() + (c == 0 ? 5.0f : 3.0f));
    }
    if (hearing)
    {
        const float x = scale.getX() + (juce::jlimit (-50.0f, 50.0f, cents) + 50.0f) / 100.0f * scale.getWidth();
        g.setColour (colour);
        g.fillRoundedRectangle (x - 1.5f, scale.getY(), 3.0f, scale.getHeight(), 1.5f);
    }
}

} // namespace stacks
