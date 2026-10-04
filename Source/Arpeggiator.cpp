#include "Arpeggiator.h"

namespace stacks
{

double Arpeggiator::beatsPerStep (int rate) noexcept
{
    switch (rate)
    {
        case 0:  return 1.0;
        case 1:  return 0.5;
        case 2:  return 1.0 / 3.0;
        case 3:  return 0.25;
        case 4:  return 1.0 / 6.0;
        default: return 0.125;
    }
}

void Arpeggiator::prepare (double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
}

void Arpeggiator::reset()
{
    held.clear();
    samplesIntoStep = 0.0;
    stepIndex = 0;
    playingNote = -1;
    noteOffIn = -1.0;
}

void Arpeggiator::allNotesOff (juce::MidiBuffer& out, int sample)
{
    if (playingNote >= 0)
    {
        out.addEvent (juce::MidiMessage::noteOff (playingChannel, playingNote), sample);
        playingNote = -1;
        noteOffIn = -1.0;
    }
}

int Arpeggiator::nextNote (const Params& p)
{
    if (held.empty())
        return -1;

    std::vector<Held> order = held;
    if (p.mode != 5)
        std::sort (order.begin(), order.end(), [] (const Held& a, const Held& b) { return a.note < b.note; });
    else
        std::sort (order.begin(), order.end(), [] (const Held& a, const Held& b) { return a.order < b.order; });

    // The pattern over all octaves
    std::vector<int> pattern;
    const int octaves = juce::jlimit (1, 4, p.octaves);
    for (int o = 0; o < octaves; ++o)
        for (const auto& h : order)
            pattern.push_back (h.note + 12 * o);
    const int n = (int) pattern.size();

    int pick = 0;
    switch (p.mode)
    {
        case 2:  pick = n - 1 - (stepIndex % n); break;                     // Down
        case 3:                                                             // Up-Down, ends not repeated
        {
            const int cycle = n > 1 ? 2 * n - 2 : 1;
            const int k = stepIndex % cycle;
            pick = k < n ? k : cycle - k;
            break;
        }
        case 4:  pick = rng.nextInt (n); break;                             // Random
        default: pick = stepIndex % n; break;                               // Up, As Played
    }
    ++stepIndex;
    return juce::jlimit (0, 127, pattern[(size_t) pick]);
}

void Arpeggiator::process (juce::MidiBuffer& midi, int numSamples, const Params& p, double bpm, std::optional<double> ppq, bool hostPlaying)
{
    const bool off = p.mode == 0;
    if (off)
    {
        if (! wasOff)                 // just switched off: let any arp note go and forget the held list
        {
            juce::MidiBuffer passthrough (midi);
            midi.clear();
            allNotesOff (midi, 0);
            midi.addEvents (passthrough, 0, numSamples, 0);
            held.clear();
            wasOff = true;
        }
        return;
    }
    wasOff = false;

    // Take the note messages, keep the rest.
    juce::MidiBuffer out;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            bool known = false;
            for (auto& h : held) if (h.note == m.getNoteNumber()) { known = true; h.velocity = m.getVelocity(); }
            if (! known) held.push_back ({ m.getNoteNumber(), (int) m.getVelocity(), orderCounter++ });
            playingChannel = juce::jlimit (1, 16, m.getChannel());
        }
        else if (m.isNoteOff())
        {
            held.erase (std::remove_if (held.begin(), held.end(), [&] (const Held& h) { return h.note == m.getNoteNumber(); }), held.end());
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            held.clear();
            out.addEvent (m, meta.samplePosition);
        }
        else
            out.addEvent (m, meta.samplePosition);
    }

    const double beats = beatsPerStep (p.rate);
    const double stepSamples = juce::jmax (1.0, beats * 60.0 / juce::jmax (20.0, bpm) * sr);

    // Lock the step clock to the host's grid when it runs; free-run otherwise.
    if (hostPlaying && ppq)
    {
        const double stepsIn = *ppq / beats;
        samplesIntoStep = (stepsIn - std::floor (stepsIn)) * stepSamples;
    }

    if (held.empty())
    {
        if (playingNote >= 0 && noteOffIn >= 0.0 && noteOffIn <= (double) numSamples)
            allNotesOff (out, (int) noteOffIn);
        else if (playingNote >= 0 && noteOffIn >= 0.0)
            noteOffIn -= numSamples;
        else
            allNotesOff (out, 0);
        samplesIntoStep += numSamples;
        while (samplesIntoStep >= stepSamples) samplesIntoStep -= stepSamples;
        stepIndex = 0;
        midi.swapWith (out);
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        if (noteOffIn >= 0.0 && noteOffIn <= 0.0)
            allNotesOff (out, i);
        if (noteOffIn > 0.0) noteOffIn -= 1.0;

        if (samplesIntoStep >= stepSamples)
        {
            samplesIntoStep -= stepSamples;
            // Swing: every other step starts late.
            const bool odd = (stepIndex % 2) == 1;
            if (odd && p.swing > 0.001f && samplesIntoStep < 0.5 * p.swing * stepSamples)
            {
                samplesIntoStep += stepSamples;   // not yet: push this step's boundary later
            }
            else
            {
                allNotesOff (out, i);
                const int note = nextNote (p);
                if (note >= 0)
                {
                    int velocity = 100;
                    for (const auto& h : held) if (h.note == note % 128 || (note - h.note) % 12 == 0) velocity = h.velocity;
                    out.addEvent (juce::MidiMessage::noteOn (playingChannel, note, (juce::uint8) juce::jlimit (1, 127, velocity)), i);
                    playingNote = note;
                    noteOffIn = juce::jmax (1.0, stepSamples * juce::jlimit (0.1f, 1.0f, p.gate));
                }
            }
        }
        samplesIntoStep += 1.0;
    }
    midi.swapWith (out);
}

} // namespace stacks
