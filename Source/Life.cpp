#include "Life.h"
#include <cmath>

namespace stacks
{

const Life::Target Life::targets[Life::kTargets] = {
    // the wavetables themselves move most: a whisper at level 1, a long slow sweep at 5
    { P::oscA_morph, 0.65f }, { P::oscB_morph, 0.55f }, { P::oscC_morph, 0.55f }, { P::oscA_warp_amt, 0.30f }, { P::fm_amount, 0.08f },
    { P::filter_cutoff, 0.30f }, { P::filter_res, 0.20f }, { P::filter_env, 0.15f },
    { P::unison_detune, 0.25f }, { P::unison_spread, 0.30f },
    { P::lfo1_rate, 0.25f }, { P::lfo2_rate, 0.20f },
    { P::chorus_mix, 0.25f }, { P::delay_mix, 0.20f }, { P::reverb_mix, 0.25f }, { P::dist_drive, 0.15f }, { P::char_sat, 0.20f },
    { P::aenv_attack, 0.20f }, { P::aenv_release, 0.30f },
};

bool Life::isTarget (int paramIndex) noexcept
{
    for (const auto& t : targets) if ((int) t.param == paramIndex) return true;
    return false;
}

float Life::levelAmount (int level) noexcept
{
    static const float amounts[kLevels + 1] = { 0.0f, 0.07f, 0.17f, 0.33f, 0.6f, 1.0f };
    return amounts[juce::jlimit (0, kLevels, level)];
}

Life::Life() : rng ((juce::int64) juce::Time::getHighResolutionTicks())
{
    // Three slow sines per target at unrelated rates: periods of 12 to 100 seconds.
    for (int t = 0; t < kTargets; ++t)
        for (int k = 0; k < 3; ++k)
        {
            phase[t][k] = rng.nextFloat();
            rate[t][k] = 0.01f + 0.07f * rng.nextFloat();
        }
}

void Life::prepare (double sr)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    reset();
}

void Life::reset()
{
    for (auto& k : kick) k = 0.0f;
    pendingKicks = 0;
}

void Life::noteOn() noexcept
{
    ++pendingKicks;
}

void Life::apply (SynthParams& p, int level, int numSamples) noexcept
{
    if (level <= 0 || numSamples <= 0)
        return;   // Off: the snapshot stays exactly the preset
    const float amount = levelAmount (level);
    const float dt = (float) numSamples / (float) sampleRate;
    const float speed = 1.0f + 0.15f * (float) level;     // higher levels also drift a little faster
    const float decay = std::exp (-dt / 4.0f);             // a note's nudge fades over a few seconds

    // Every note that started this block pushes each target somewhere new.
    while (pendingKicks > 0)
    {
        --pendingKicks;
        for (int t = 0; t < kTargets; ++t)
            kick[t] = juce::jlimit (-1.0f, 1.0f, kick[t] + 0.5f * (rng.nextFloat() * 2.0f - 1.0f));
    }

    for (int t = 0; t < kTargets; ++t)
    {
        float noise = 0.0f;
        const float weights[3] = { 0.5f, 0.3f, 0.2f };
        for (int k = 0; k < 3; ++k)
        {
            phase[t][k] += rate[t][k] * speed * dt;
            if (phase[t][k] >= 1.0f) phase[t][k] -= 1.0f;
            noise += weights[k] * std::sin (phase[t][k] * juce::MathConstants<float>::twoPi);
        }
        kick[t] *= decay;
        const float offset = targets[t].excursion * amount * (0.7f * noise + 0.3f * kick[t]);

        const int index = (int) targets[t].param;
        const auto& range = paramRange (index);
        const float normalised = range.convertTo0to1 (juce::jlimit (range.start, range.end, p.v[index]));
        p.v[index] = range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalised + offset));
    }
}

} // namespace stacks
