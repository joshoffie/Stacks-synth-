#include "Displays.h"
#include "PluginProcessor.h"
#include "Controls.h"
#include "StacksLookAndFeel.h"

namespace stacks
{

namespace
{
    constexpr float kMinHz = 20.0f, kMaxHz = 20000.0f;

    float xForHz (float hz, float width) noexcept
    {
        return width * std::log (juce::jlimit (kMinHz, kMaxHz, hz) / kMinHz) / std::log (kMaxHz / kMinHz);
    }

    void frame (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (colours::background);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    }
}

//==============================================================================
ScopeView::ScopeView (StacksAudioProcessor& p) : processor (p)
{
    samples.assign ((size_t) kFftSize, 0.0f);
    fftData.assign ((size_t) kFftSize * 2, 0.0f);
    spectrum.assign ((size_t) kFftSize / 2, -100.0f);
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

ScopeView::~ScopeView() { stopTimer(); }

void ScopeView::timerCallback()
{
    processor.copyRecentOutput (samples.data(), kFftSize);
    float peak = 0.0f;
    for (float v : samples) peak = juce::jmax (peak, std::abs (v));
    const bool nowSilent = peak < 1.0e-4f;

    std::copy (samples.begin(), samples.end(), fftData.begin());
    std::fill (fftData.begin() + kFftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) kFftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);
    for (int i = 0; i < kFftSize / 2; ++i)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[(size_t) i] / (float) kFftSize * 4.0f, -100.0f);
        spectrum[(size_t) i] = db > spectrum[(size_t) i] ? db : spectrum[(size_t) i] * 0.85f + db * 0.15f;   // fast up, slow down
    }
    if (! (nowSilent && silent))
        repaint();
    silent = nowSilent;
}

void ScopeView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float gap = 6.0f;
    auto wave = r.removeFromLeft (r.getWidth() * 0.42f);
    r.removeFromLeft (gap);
    auto spec = r;
    frame (g, wave);
    frame (g, spec);

    // Waveform: 1024 samples from the first rising zero crossing, so it holds still.
    {
        const int show = kFftSize / 2;
        int start = 0;
        for (int i = 1; i < kFftSize - show; ++i)
            if (samples[(size_t) i - 1] <= 0.0f && samples[(size_t) i] > 0.0f) { start = i; break; }
        auto inner = wave.reduced (4.0f, 3.0f);
        juce::Path path;
        for (int i = 0; i < show; ++i)
        {
            const float x = inner.getX() + inner.getWidth() * (float) i / (float) (show - 1);
            const float y = inner.getCentreY() - juce::jlimit (-1.0f, 1.0f, samples[(size_t) (start + i)]) * inner.getHeight() * 0.48f;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        g.setColour (colours::muted.withAlpha (0.25f));
        g.drawHorizontalLine ((int) inner.getCentreY(), inner.getX(), inner.getRight());
        g.setColour (colours::accent.withAlpha (silent ? 0.35f : 0.95f));
        g.strokePath (path, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Spectrum: log frequency, -90..0 dB, filled.
    {
        auto inner = spec.reduced (4.0f, 3.0f);
        const double sr = processor.getSampleRate() > 0 ? processor.getSampleRate() : 48000.0;
        juce::Path fill;
        fill.startNewSubPath (inner.getX(), inner.getBottom());
        const int points = (int) inner.getWidth();
        for (int px = 0; px <= points; ++px)
        {
            const float hz = kMinHz * std::pow (kMaxHz / kMinHz, (float) px / (float) points);
            const int bin = juce::jlimit (1, kFftSize / 2 - 1, (int) std::round (hz / (float) sr * kFftSize));
            const float db = spectrum[(size_t) bin];
            const float y = inner.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f) * inner.getHeight();
            fill.lineTo (inner.getX() + (float) px, y);
        }
        fill.lineTo (inner.getRight(), inner.getBottom());
        fill.closeSubPath();
        juce::ColourGradient grad (colours::accent.withAlpha (0.55f), 0.0f, inner.getY(), colours::accent.withAlpha (0.05f), 0.0f, inner.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (fill);
        g.setColour (colours::accent.withAlpha (0.9f));
        g.strokePath (fill, juce::PathStrokeType (1.0f));

        g.setColour (colours::muted.withAlpha (0.6f));
        g.setFont (StacksLookAndFeel::font (8.5f));
        for (float hz : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = inner.getX() + xForHz (hz, inner.getWidth());
            g.drawVerticalLine ((int) x, inner.getBottom() - 4.0f, inner.getBottom());
            g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz),
                        juce::Rectangle<float> (x - 14.0f, inner.getY(), 28.0f, 10.0f), juce::Justification::centred, false);
        }
    }
}

//==============================================================================
EnvelopeDisplay::EnvelopeDisplay (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix, juce::Colour c) : colour (c)
{
    a = apvts.getRawParameterValue (prefix + "_attack");
    d = apvts.getRawParameterValue (prefix + "_decay");
    s = apvts.getRawParameterValue (prefix + "_sustain");
    r = apvts.getRawParameterValue (prefix + "_release");
    setTooltip ("The envelope's shape: attack up, decay down to the sustain level, held while the key is down, then release.");
    setInterceptsMouseClicks (false, false);
    startTimerHz (15);
}

void EnvelopeDisplay::timerCallback()
{
    const float v[4] = { a->load(), d->load(), s->load(), r->load() };
    bool changed = false;
    for (int i = 0; i < 4; ++i) { changed = changed || std::abs (v[i] - shown[i]) > 1.0e-4f; shown[i] = v[i]; }
    if (changed) repaint();
}

void EnvelopeDisplay::paint (juce::Graphics& g)
{
    auto r0 = getLocalBounds().toFloat().reduced (2.0f, 15.0f).withTrimmedBottom (2.0f);
    frame (g, r0);
    auto inner = r0.reduced (4.0f, 4.0f);

    // Time axis: square-root scaling so a 5 ms attack and a 3 s release both read.
    auto len = [] (float seconds) { return std::sqrt (juce::jlimit (0.0f, 10.0f, seconds)); };
    const float hold = 0.6f;
    const float la = len (shown[0]), ld = len (shown[1]), lh = len (hold), lr = len (shown[2] > 0.001f ? shown[3] : shown[3] * 0.5f);
    const float total = juce::jmax (0.001f, la + ld + lh + lr);
    const float sustain = juce::jlimit (0.0f, 1.0f, shown[2]);
    auto x = [&] (float t) { return inner.getX() + inner.getWidth() * t / total; };
    auto y = [&] (float level) { return inner.getBottom() - level * inner.getHeight(); };

    juce::Path p;
    p.startNewSubPath (x (0.0f), y (0.0f));
    p.quadraticTo (x (la * 0.4f), y (0.9f), x (la), y (1.0f));
    p.quadraticTo (x (la + ld * 0.3f), y (sustain + (1.0f - sustain) * 0.25f), x (la + ld), y (sustain));
    p.lineTo (x (la + ld + lh), y (sustain));
    p.quadraticTo (x (la + ld + lh + lr * 0.3f), y (sustain * 0.25f), x (total), y (0.0f));

    juce::Path fill (p);
    fill.lineTo (x (total), inner.getBottom());
    fill.lineTo (x (0.0f), inner.getBottom());
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.18f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (colours::muted);
    g.setFont (StacksLookAndFeel::font (9.5f));
    g.drawText ("Envelope", getLocalBounds().removeFromTop (14), juce::Justification::centred, false);
}

//==============================================================================
FilterCurve::FilterCurve (juce::AudioProcessorValueTreeState& apvts, juce::Colour c) : colour (c)
{
    type = apvts.getRawParameterValue ("filter_type");
    cutoff = apvts.getRawParameterValue ("filter_cutoff");
    resonance = apvts.getRawParameterValue ("filter_res");
    drive = apvts.getRawParameterValue ("filter_drive");
    setTooltip ("What the filter lets through: low-pass keeps the lows, high-pass the highs, band-pass the middle. Resonance adds a peak at the cutoff.");
    setInterceptsMouseClicks (false, false);
    startTimerHz (15);
}

void FilterCurve::timerCallback()
{
    const float v[4] = { type->load(), cutoff->load(), resonance->load(), drive->load() };
    bool changed = false;
    for (int i = 0; i < 4; ++i) { changed = changed || std::abs (v[i] - shown[i]) > 1.0e-4f; shown[i] = v[i]; }
    if (changed) repaint();
}

void FilterCurve::paint (juce::Graphics& g)
{
    auto r0 = getLocalBounds().toFloat().reduced (2.0f, 15.0f).withTrimmedBottom (2.0f);
    frame (g, r0);
    auto inner = r0.reduced (4.0f, 4.0f);

    const int t = (int) shown[0];          // LP12, LP24, HP12, HP24, BP12, BP24
    const float fc = juce::jlimit (kMinHz, kMaxHz, shown[1]);
    const float res = juce::jlimit (0.0f, 1.0f, shown[2]);
    const float order = (t % 2 == 0) ? 2.0f : 4.0f;   // 12 or 24 dB/oct
    auto response = [&] (float hz)
    {
        const float w = hz / fc;
        float gain = 1.0f;
        if (t < 2)      gain = 1.0f / std::sqrt (1.0f + std::pow (w, 2.0f * order));
        else if (t < 4) gain = 1.0f / std::sqrt (1.0f + std::pow (1.0f / w, 2.0f * order));
        else            gain = 1.0f / std::sqrt (1.0f + std::pow (w, order)) / std::sqrt (1.0f + std::pow (1.0f / w, order)) * 2.0f;
        // resonance: a peak about a third of an octave wide around the cutoff
        const float octaves = std::log2 (w);
        gain *= 1.0f + res * 8.0f * std::exp (-octaves * octaves * 18.0f);
        return gain;
    };

    juce::Path p;
    const int points = (int) inner.getWidth();
    for (int px = 0; px <= points; ++px)
    {
        const float hz = kMinHz * std::pow (kMaxHz / kMinHz, (float) px / (float) points);
        const float db = juce::jlimit (-36.0f, 18.0f, juce::Decibels::gainToDecibels (response (hz), -60.0f));
        const float y = inner.getBottom() - (db + 36.0f) / 54.0f * inner.getHeight();
        if (px == 0) p.startNewSubPath (inner.getX(), y); else p.lineTo (inner.getX() + (float) px, y);
    }
    juce::Path fill (p);
    fill.lineTo (inner.getRight(), inner.getBottom());
    fill.lineTo (inner.getX(), inner.getBottom());
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.18f));
    g.fillPath (fill);
    g.setColour (colours::muted.withAlpha (0.25f));
    g.drawHorizontalLine ((int) (inner.getBottom() - 36.0f / 54.0f * inner.getHeight()), inner.getX(), inner.getRight()); // 0 dB
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (colours::muted);
    g.setFont (StacksLookAndFeel::font (9.5f));
    g.drawText ("Response", getLocalBounds().removeFromTop (14), juce::Justification::centred, false);
}

} // namespace stacks
