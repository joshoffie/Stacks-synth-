#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <optional>
#include <vector>

namespace stacks
{

// Turns held notes into a pattern in time with the host. Sits in front of the
// synth: note on/off messages are consumed, everything else passes through.
class Arpeggiator
{
public:
    struct Params
    {
        int mode = 0;        // 0 Off, 1 Up, 2 Down, 3 Up-Down, 4 Random, 5 As Played
        int rate = 1;        // 0 1/4, 1 1/8, 2 1/8T, 3 1/16, 4 1/16T, 5 1/32
        int octaves = 1;
        float gate = 0.6f;
        float swing = 0.0f;
    };

    void prepare (double sampleRate);
    void reset();
    void process (juce::MidiBuffer& midi, int numSamples, const Params&, double bpm, std::optional<double> ppqAtBlockStart, bool hostPlaying);

    static double beatsPerStep (int rate) noexcept;

private:
    struct Held { int note, velocity, order; };
    void allNotesOff (juce::MidiBuffer& out, int sample);
    int nextNote (const Params&);

    std::vector<Held> held;
    double sr = 48000.0;
    double samplesIntoStep = 0.0;
    int stepIndex = 0, orderCounter = 0;
    int playingNote = -1, playingChannel = 1;
    double noteOffIn = -1.0;           // samples until the sounding note ends, <0 = none
    bool wasOff = true;
    juce::Random rng;
};

} // namespace stacks
