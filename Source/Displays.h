#pragma once

#include <JuceHeader.h>

namespace stacks
{

class StacksAudioProcessor;

// The master output as a waveform and a spectrum, side by side.
class ScopeView : public juce::Component,
                  private juce::Timer
{
public:
    explicit ScopeView (StacksAudioProcessor&);
    ~ScopeView() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    StacksAudioProcessor& processor;
    static constexpr int kFftOrder = 11, kFftSize = 1 << kFftOrder;
    juce::dsp::FFT fft { kFftOrder };
    juce::dsp::WindowingFunction<float> window { kFftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> samples, fftData, spectrum;
    bool silent = true;
};

// An ADSR drawn from four parameters ("fenv", "aenv" or "menv" + _attack...).
class EnvelopeDisplay : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
{
public:
    EnvelopeDisplay (juce::AudioProcessorValueTreeState&, const juce::String& prefix, juce::Colour);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    std::atomic<float>* a; std::atomic<float>* d; std::atomic<float>* s; std::atomic<float>* r;
    float shown[4] { -1.0f, -1.0f, -1.0f, -1.0f };
    juce::Colour colour;
};

// The filter's magnitude response for the current type, cutoff and resonance.
class FilterCurve : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    FilterCurve (juce::AudioProcessorValueTreeState&, juce::Colour);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    std::atomic<float>* type; std::atomic<float>* cutoff; std::atomic<float>* resonance; std::atomic<float>* drive;
    std::atomic<float>* routing; std::atomic<float>* type2; std::atomic<float>* cutoff2; std::atomic<float>* resonance2;   // filter 2
    float shown[8] { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    juce::Colour colour;
};

} // namespace stacks
