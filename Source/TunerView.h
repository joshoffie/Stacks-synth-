#pragma once

#include <JuceHeader.h>

namespace stacks
{

class StacksAudioProcessor;

// Hold a note: the tuner hears the dry synth, finds its pitch and shows how
// far it sits from the note you played. Auto-tune fixes the patch's tuning.
class TunerView : public juce::Component,
                  public juce::SettableTooltipClient,
                  private juce::Timer
{
public:
    explicit TunerView (StacksAudioProcessor&);
    ~TunerView() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    static float detectPitch (const float* x, int n, double sampleRate, float& clarity);

    StacksAudioProcessor& processor;
    juce::TextButton autoTuneButton;
    std::vector<float> window;
    std::vector<float> recent;          // last few cent readings, for a steady needle
    float cents = 0.0f;                 // shown
    int note = -1;                      // MIDI note the reading is relative to
    bool hearing = false;               // a note is sounding and the pitch is clear
    juce::String flash;                 // what Auto-tune just did
    double flashUntil = 0.0;
};

} // namespace stacks
