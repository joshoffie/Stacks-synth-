#include "SampleAnalyser.h"

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace stacks
{

namespace
{
    constexpr int kHop = 512, kFrame = 1024;          // envelope frames
    constexpr int kPitchFrame = 2048, kPitchHop = 1024;
    constexpr int kFftOrder = 12, kFftSize = 1 << kFftOrder;

    float median (std::vector<float> v)
    {
        if (v.empty()) return 0.0f;
        std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
        return v[v.size() / 2];
    }

    // McLeod's normalised square difference: the first strong peak is the period.
    float pitchOfFrame (const float* x, int n, double sr, float& clarity)
    {
        const int tauMin = juce::jmax (2, (int) (sr / 2000.0)), tauMax = juce::jmin (n / 2, (int) (sr / 40.0));
        std::vector<float> nsdf ((size_t) tauMax + 1, 0.0f);
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            double acf = 0.0, m = 0.0;
            for (int i = 0; i + tau < n; ++i)
            {
                acf += (double) x[i] * x[i + tau];
                m   += (double) x[i] * x[i] + (double) x[i + tau] * x[i + tau];
            }
            nsdf[(size_t) tau] = m > 0.0 ? (float) (2.0 * acf / m) : 0.0f;
        }
        // key maxima between positive zero crossings; pick the first above 0.9 x the highest
        std::vector<std::pair<int, float>> maxima;
        int tau = tauMin;
        while (tau <= tauMax && nsdf[(size_t) tau] > 0.0f) ++tau;          // skip the initial lobe
        while (tau <= tauMax)
        {
            while (tau <= tauMax && nsdf[(size_t) tau] <= 0.0f) ++tau;
            int best = -1; float bestV = 0.0f;
            while (tau <= tauMax && nsdf[(size_t) tau] > 0.0f)
            {
                if (nsdf[(size_t) tau] > bestV) { bestV = nsdf[(size_t) tau]; best = tau; }
                ++tau;
            }
            if (best > 0) maxima.emplace_back (best, bestV);
        }
        if (maxima.empty()) { clarity = 0.0f; return 0.0f; }
        float highest = 0.0f;
        for (const auto& m : maxima) highest = juce::jmax (highest, m.second);
        for (const auto& m : maxima)
            if (m.second >= 0.9f * highest)
            {
                // parabolic refinement
                const int t = m.first;
                float refined = (float) t;
                if (t > tauMin && t < tauMax)
                {
                    const float a = nsdf[(size_t) t - 1], b = nsdf[(size_t) t], c = nsdf[(size_t) t + 1];
                    const float denom = a - 2.0f * b + c;
                    if (std::abs (denom) > 1.0e-9f) refined += 0.5f * (a - c) / denom;
                }
                clarity = m.second;
                return (float) (sr / (double) refined);
            }
        clarity = 0.0f;
        return 0.0f;
    }

    struct Spectrum { std::vector<float> mag; float binHz = 1.0f; };

    Spectrum spectrumAt (const std::vector<float>& x, int centre, double sr)
    {
        Spectrum s;
        s.binHz = (float) (sr / kFftSize);
        std::vector<float> buf ((size_t) kFftSize * 2, 0.0f);
        const int start = juce::jlimit (0, juce::jmax (0, (int) x.size() - kFftSize), centre - kFftSize / 2);
        for (int i = 0; i < kFftSize && start + i < (int) x.size(); ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kFftSize - 1));
            buf[(size_t) i] = x[(size_t) (start + i)] * w;
        }
        static juce::dsp::FFT fft (kFftOrder);
        fft.performFrequencyOnlyForwardTransform (buf.data());
        s.mag.assign (buf.begin(), buf.begin() + kFftSize / 2);
        return s;
    }

    float centroidOf (const Spectrum& s, float maxHz)
    {
        double num = 0.0, den = 0.0;
        const int bins = juce::jmin ((int) s.mag.size(), (int) (maxHz / s.binHz));
        for (int b = 1; b < bins; ++b) { num += (double) b * s.binHz * s.mag[(size_t) b]; den += s.mag[(size_t) b]; }
        return den > 0.0 ? (float) (num / den) : 1000.0f;
    }

    // Amplitude of harmonic k (1-based) of f0: the strongest bin within 3 %.
    float harmonicAmp (const Spectrum& s, float f0, int k)
    {
        const float target = f0 * (float) k / s.binHz;
        const int lo = juce::jmax (1, (int) (target * 0.97f)), hi = juce::jmin ((int) s.mag.size() - 1, (int) std::ceil (target * 1.03f) + 1);
        float best = 0.0f;
        for (int b = lo; b <= hi; ++b) best = juce::jmax (best, s.mag[(size_t) b]);
        return best;
    }

    std::vector<float> harmonicFrame (const Spectrum& s, float f0, float& tailRatio)
    {
        std::vector<float> amps (32, 0.0f);
        for (int k = 1; k <= 32; ++k) amps[(size_t) k - 1] = harmonicAmp (s, f0, k);
        float peak = 1.0e-9f;
        for (int k = 0; k < WaveSpec::kHarmonics; ++k) peak = juce::jmax (peak, amps[(size_t) k]);
        std::vector<float> frame ((size_t) WaveSpec::kHarmonics);
        for (int k = 0; k < WaveSpec::kHarmonics; ++k) frame[(size_t) k] = juce::jlimit (0.0f, 1.0f, amps[(size_t) k] / peak);
        float high = 0.0f, mid = 0.0f;
        for (int k = 16; k < 32; ++k) high += amps[(size_t) k];
        for (int k = 8; k < 16; ++k) mid += amps[(size_t) k];
        tailRatio = mid > 1.0e-6f ? juce::jlimit (0.0f, 1.0f, (high / 16.0f) / (mid / 8.0f)) : 0.0f;
        return frame;
    }

    float noiseShare (const Spectrum& s, float f0)
    {
        double total = 0.0, harmonic = 0.0;
        const int bins = juce::jmin ((int) s.mag.size(), (int) (16000.0f / s.binHz));
        for (int b = 1; b < bins; ++b)
        {
            const double e = (double) s.mag[(size_t) b] * s.mag[(size_t) b];
            total += e;
            const float harmonicIndex = (float) b * s.binHz / f0;
            const float nearest = std::round (harmonicIndex);
            if (nearest >= 1.0f && nearest <= 60.0f && std::abs (harmonicIndex - nearest) <= 0.06f * nearest)
                harmonic += e;
        }
        return total > 0.0 ? (float) juce::jlimit (0.0, 1.0, 1.0 - harmonic / total) : 1.0f;
    }

    juce::String seconds (float s)
    {
        return s < 1.0f ? juce::String ((int) std::round (s * 1000.0f)) + " ms" : juce::String (s, 1) + " s";
    }

    juce::String noteName (float hz)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int midi = (int) std::round (69.0 + 12.0 * std::log2 (hz / 440.0));
        return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
    }
}

SampleAnalysis analyseSample (const SampleData& sample)
{
    SampleAnalysis a;
    const double sr = sample.sampleRate;
    const int len = sample.length();
    if (len < kFrame * 2)
        return a;

    // Mono mix (and the width of a stereo file).
    std::vector<float> x ((size_t) len);
    const int chans = sample.audio.getNumChannels();
    const float* L = sample.audio.getReadPointer (0);
    const float* R = sample.audio.getReadPointer (chans > 1 ? 1 : 0);
    double ll = 0.0, rr = 0.0, lr = 0.0;
    for (int i = 0; i < len; ++i)
    {
        x[(size_t) i] = 0.5f * (L[i] + R[i]);
        ll += (double) L[i] * L[i]; rr += (double) R[i] * R[i]; lr += (double) L[i] * R[i];
    }
    if (chans > 1 && ll > 0.0 && rr > 0.0)
        a.width = (float) juce::jlimit (0.0, 1.0, 1.0 - std::abs (lr) / std::sqrt (ll * rr));
    float peak = 1.0e-6f;
    for (float v : x) peak = juce::jmax (peak, std::abs (v));
    for (float& v : x) v /= peak;

    // Envelope.
    const int frames = (len - kFrame) / kHop + 1;
    std::vector<float> rms ((size_t) frames);
    float peakRms = 1.0e-6f; int peakIdx = 0;
    for (int f = 0; f < frames; ++f)
    {
        double e = 0.0;
        for (int i = 0; i < kFrame; ++i) { const float v = x[(size_t) (f * kHop + i)]; e += (double) v * v; }
        rms[(size_t) f] = (float) std::sqrt (e / kFrame);
        if (rms[(size_t) f] > peakRms) { peakRms = rms[(size_t) f]; peakIdx = f; }
    }
    const float floorLevel = peakRms * 0.01f;   // -40 dB
    int start = 0; while (start < frames - 1 && rms[(size_t) start] < floorLevel) ++start;
    int end = frames - 1; while (end > start && rms[(size_t) end] < floorLevel) --end;
    const auto secondsOf = [&] (int nFrames) { return (float) (nFrames * kHop / sr); };
    a.duration = secondsOf (end - start + 1);

    int attackIdx = start; while (attackIdx < peakIdx && rms[(size_t) attackIdx] < 0.9f * peakRms) ++attackIdx;
    a.attack = juce::jlimit (0.001f, 4.0f, secondsOf (attackIdx - start));

    const int remaining = end - peakIdx;
    const int midIdx = peakIdx + (int) (remaining * 0.4f);
    const float sustainRatio = remaining >= 3 ? rms[(size_t) midIdx] / peakRms : 0.0f;
    a.decaying = sustainRatio < 0.6f;
    if (a.decaying)
    {
        int tenth = peakIdx; while (tenth < end && rms[(size_t) tenth] > 0.1f * peakRms) ++tenth;
        a.decay = juce::jlimit (0.02f, 8.0f, tenth < end ? secondsOf (tenth - peakIdx) : a.duration * 1.5f);
        a.sustain = juce::jlimit (0.0f, 0.4f, sustainRatio * 0.8f);
        a.release = juce::jlimit (0.05f, 4.0f, a.decay * 0.6f);
    }
    else
    {
        a.decay = juce::jlimit (0.05f, 2.0f, secondsOf (midIdx - peakIdx));
        a.sustain = juce::jlimit (0.3f, 1.0f, sustainRatio);
        int lastHalf = end; while (lastHalf > peakIdx && rms[(size_t) lastHalf] < 0.5f * peakRms) --lastHalf;
        a.release = juce::jlimit (0.05f, 6.0f, secondsOf (end - lastHalf) * 1.2f);
        if (a.release < 0.08f) a.release = 0.25f;   // cut off abruptly: give the synth a natural tail
    }
    a.percussive = a.decaying && a.duration < 1.5f && a.attack < 0.03f;

    // Spectrum at the attack, in the body and late on (the body spectrum also
    // backs up the pitch detector).
    const int firstSample = start * kHop, peakSample = peakIdx * kHop, endSample = end * kHop;
    const int attackPos = juce::jmin (len - 1, firstSample + (int) (0.015 * sr) + kFftSize / 4);
    const int midPos = peakSample + (int) ((endSample - peakSample) * 0.3f) + kFftSize / 4;
    const int latePos = peakSample + (int) ((endSample - peakSample) * 0.7f) + kFftSize / 4;
    const auto sa = spectrumAt (x, attackPos, sr), sm = spectrumAt (x, midPos, sr), sl = spectrumAt (x, latePos, sr);

    // Pitch track over the loud part, skipping the first 25 ms (the transient).
    std::vector<float> f0s;
    int pitchFrames = 0;
    const int skip = (int) (0.025 * sr);
    const int lastSample = juce::jmin (len - kPitchFrame, end * kHop + kFrame);
    for (int pos = firstSample + skip; pos + kPitchFrame <= lastSample + kPitchFrame && pos + kPitchFrame <= len; pos += kPitchHop)
    {
        const int f = juce::jlimit (0, frames - 1, pos / kHop);
        if (rms[(size_t) f] < 0.1f * peakRms) continue;
        ++pitchFrames;
        float clarity = 0.0f;
        const float hz = pitchOfFrame (x.data() + pos, kPitchFrame, sr, clarity);
        f0s.push_back (clarity > 0.65f && hz > 30.0f && hz < 2500.0f ? hz : 0.0f);
    }
    std::vector<float> voiced;
    for (float hz : f0s) if (hz > 0.0f) voiced.push_back (hz);
    a.pitched = pitchFrames > 0 && (float) voiced.size() >= 0.3f * (float) pitchFrames;
    a.f0 = a.pitched ? median (voiced) : 110.0f;

    // No clear period, but one spectral peak that towers over the rest (a kick's
    // body, an 808, a tom): take that as a weak pitch rather than calling it noise.
    if (! a.pitched)
    {
        const auto& ref = a.percussive ? sa : sm;
        const int lo = juce::jmax (1, (int) (40.0f / ref.binHz)), hi = juce::jmin ((int) ref.mag.size() - 2, (int) (1000.0f / ref.binHz));
        int best = lo; float bestMag = 0.0f;
        for (int b = lo; b <= hi; ++b) if (ref.mag[(size_t) b] > bestMag) { bestMag = ref.mag[(size_t) b]; best = b; }
        std::vector<float> rest (ref.mag.begin() + lo, ref.mag.begin() + juce::jmin ((int) ref.mag.size(), (int) (8000.0f / ref.binHz)));
        const float medianMag = median (rest);
        if (bestMag > 4.0f * juce::jmax (1.0e-6f, medianMag))
        {
            const float m0 = ref.mag[(size_t) best - 1], m1 = bestMag, m2 = ref.mag[(size_t) best + 1];
            const float denom = m0 - 2.0f * m1 + m2;
            const float offset = std::abs (denom) > 1.0e-9f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (m0 - m2) / denom) : 0.0f;
            a.f0 = ((float) best + offset) * ref.binHz;
            a.pitched = true;
            a.weakPitch = true;
        }
    }

    // Pitch drop (a kick's sweep): the first voiced frames sit above the rest.
    if (a.pitched && voiced.size() >= 3)
    {
        const float early = voiced[0], body = median (voiced);
        if (early > 1.25f * body) a.pitchDropOctaves = juce::jlimit (0.3f, 3.0f, std::log2 (early / body));
    }

    // Vibrato.
    if (a.pitched && ! a.weakPitch && voiced.size() >= 8)
    {
        std::vector<float> deviations;
        for (float hz : f0s) if (hz > 0.0f) deviations.push_back (12.0f * std::log2 (hz / a.f0));
        double mean = 0.0; for (float d : deviations) mean += d; mean /= (double) deviations.size();
        double var = 0.0; for (float d : deviations) var += (d - mean) * (d - mean); var /= (double) deviations.size();
        const float depth = (float) std::sqrt (var);
        if (depth > 0.1f && depth < 2.0f)
        {
            const int n = (int) deviations.size();
            float bestCorr = 0.3f; int bestLag = 0;
            for (int lag = 2; lag <= juce::jmin (40, n / 2); ++lag)
            {
                double c = 0.0;
                for (int i = 0; i + lag < n; ++i) c += (deviations[(size_t) i] - mean) * (deviations[(size_t) i + lag] - mean);
                c /= (double) (n - lag) * var;
                if (c > bestCorr) { bestCorr = (float) c; bestLag = lag; }
            }
            if (bestLag > 0)
            {
                a.vibratoHz = (float) (sr / (kPitchHop * (double) bestLag));
                a.vibratoSemis = depth * 1.4f;   // standard deviation to peak
                if (a.vibratoHz < 1.5f || a.vibratoHz > 12.0f) { a.vibratoHz = 0.0f; a.vibratoSemis = 0.0f; }
            }
        }
    }

    a.centroidAttack = centroidOf (sa, 16000.0f);
    a.centroidSustain = centroidOf (sm, 16000.0f);
    a.brightnessDrop = juce::jlimit (-3.0f, 4.0f, std::log2 (juce::jmax (50.0f, a.centroidAttack) / juce::jmax (50.0f, a.centroidSustain)));
    a.noiseRatio = a.pitched ? noiseShare (sm, a.f0) : 0.8f;
    if (a.weakPitch) a.noiseRatio = juce::jmax (a.noiseRatio, 0.3f);

    float tailA = 0.0f, tailM = 0.0f, tailL = 0.0f;
    a.wave.frames = { harmonicFrame (sa, a.f0, tailA), harmonicFrame (sm, a.f0, tailM), harmonicFrame (sl, a.f0, tailL) };
    a.wave.tail = juce::jlimit (0.0f, 1.0f, (tailA + tailM + tailL) / 3.0f);
    a.wave.name = "Recreated";

    // Category and the brief.
    if (a.percussive && (! a.pitched || a.weakPitch || a.f0 < 130.0f))  a.category = "Perc";
    else if (! a.pitched)                         a.category = "Texture";
    else if (a.attack > 0.4f)                     a.category = "Pad";
    else if (a.decaying && a.decay > 1.5f)        a.category = "Bell";
    else if (a.decaying && a.decay <= 0.6f)       a.category = "Pluck";
    else if (a.decaying)                          a.category = "Keys";
    else if (a.f0 < 130.0f)                       a.category = "Bass";
    else                                          a.category = "Lead";

    juce::String b = "Recreate this recording with the synth: ";
    if (a.category == "Perc")
    {
        b << "a percussive hit, drum-like";
        if (a.pitched) b << " with a body around " << juce::String ((int) std::round (a.f0)) << " Hz";
        if (a.pitchDropOctaves > 0.0f) b << " that drops in pitch by " << juce::String (a.pitchDropOctaves, 1) << " octaves";
        if (! a.pitched || a.noiseRatio > 0.5f) b << ", mostly noise";
    }
    else if (a.pitched) b << "a pitched " << a.category.toLowerCase() << " around " << juce::String ((int) std::round (a.f0)) << " Hz (" << noteName (a.f0) << ")" << (a.weakPitch ? ", pitch faint" : "");
    else                b << "an unpitched, noisy texture (no clear note)";
    b << ", attack " << seconds (a.attack);
    if (a.decaying) b << ", dies away over " << seconds (a.decay) << (a.sustain > 0.15f ? " to a quiet sustain" : "");
    else            b << ", holds at " << juce::String ((int) std::round (a.sustain * 100.0f)) << " % after " << seconds (a.decay);
    b << ", release " << seconds (a.release);
    if (a.brightnessDrop > 0.4f)       b << "; bright at the start, darkening by " << juce::String (a.brightnessDrop, 1) << " octaves";
    else if (a.brightnessDrop < -0.4f) b << "; opens up, getting brighter as it holds";
    else                               b << "; steady brightness around " << juce::String ((int) std::round (a.centroidSustain)) << " Hz";
    if (a.category != "Perc")
    {
        if (a.noiseRatio > 0.6f)        b << "; a lot of noise and air";
        else if (a.noiseRatio > 0.35f)  b << "; some noise and breath";
    }
    if (a.width > 0.35f)            b << "; wide stereo";
    if (a.vibratoHz > 0.0f)         b << "; vibrato about " << juce::String (a.vibratoHz, 1) << " Hz";
    b << ". Keep its character, make it musical.";
    a.brief = b;
    a.heard = a.category == "Perc" ? juce::String ("percussive hit") + (a.pitched ? ", body " + juce::String ((int) std::round (a.f0)) + " Hz" : ", noise")
            : a.pitched ? a.category.toLowerCase() + " at " + noteName (a.f0) + " (" + juce::String ((int) std::round (a.f0)) + " Hz)"
                        : juce::String ("unpitched texture");
    a.heard << ", " << seconds (a.attack) << " attack, " << (a.decaying ? "dies away in " + seconds (a.decay) : "holds");
    return a;
}

Patch patchFromAnalysis (const SampleAnalysis& a, const juce::String& name)
{
    Patch p;
    p.name = name;
    p.category = a.category;
    p.description = a.brief.fromFirstOccurrenceOf (": ", false, false).upToLastOccurrenceOf (". Keep", false, false);
    p.origin = "Analysis";
    p.tags = { "recreated", a.category.toLowerCase() };
    p.prompt = a.brief;

    p.set (P::aenv_attack, a.attack);
    p.set (P::aenv_decay, a.decay);
    p.set (P::aenv_sustain, a.sustain);
    p.set (P::aenv_release, a.release);
    p.set (P::oscB_level, 0.0f);
    p.set (P::reverb_mix, 0.08f);

    if (a.category == "Perc")
    {
        // A drum-like hit: a sine body (with a pitch drop when there was one), a
        // noise click shaped by its own fast decay, nothing sustaining.
        const bool lowBody = a.pitched && a.f0 < 300.0f;
        p.set (P::oscA_wave, lowBody ? (float) waveNames().indexOf ("Sine") : (float) kCustomWave);
        if (! lowBody && ! a.wave.isEmpty()) p.waves[0] = a.wave;
        p.set (P::oscA_level, a.pitched ? 0.9f : 0.25f);
        p.set (P::oscA_coarse, a.pitched && a.f0 < 70.0f ? -24.0f : a.pitched && a.f0 < 140.0f ? -12.0f : 0.0f);   // a middle key plays it where it lived
        p.set (P::noise_level, a.pitched && a.noiseRatio < 0.5f ? 0.2f : 0.7f);
        p.set (P::aenv_sustain, 0.0f);
        p.set (P::aenv_release, juce::jmin (a.release, 0.25f));
        p.set (P::filter_type, (float) filterTypeNames().indexOf (a.pitched ? "LP24" : "BP12"));
        p.set (P::filter_cutoff, juce::jlimit (150.0f, 16000.0f, a.pitched ? juce::jmax (a.centroidAttack * 1.5f, a.f0 * 6.0f) : a.centroidAttack));
        p.set (P::filter_res, 0.2f);
        p.set (P::filter_env, 2.0f);                        // the click: bright for a moment, then the body
        p.set (P::fenv_attack, 0.001f);
        p.set (P::fenv_decay, juce::jlimit (0.02f, 0.3f, a.decay * 0.3f));
        p.set (P::fenv_sustain, 0.0f);
        if (a.pitchDropOctaves > 0.0f)
        {
            p.set (P::menv_attack, 0.001f);
            p.set (P::menv_decay, juce::jlimit (0.03f, 0.4f, a.decay * 0.35f));
            p.set (P::menv_sustain, 0.0f);
            p.set (modSourceParam (0), (float) SrcModEnv);
            p.set (modDestParam (0), (float) TargetPitch);
            p.set (modAmountParam (0), juce::jlimit (0.1f, 1.0f, a.pitchDropOctaves));   // x12 semitones
        }
        ensureMacroRoutings (p);
        return p;
    }

    if (! a.wave.isEmpty())
    {
        p.waves[0] = a.wave;
        p.set (P::oscA_wave, (float) kCustomWave);
    }
    p.set (P::oscA_level, a.pitched ? 0.85f : 0.35f);
    p.set (P::noise_level, juce::jlimit (0.0f, 0.7f, a.pitched ? (a.noiseRatio > 0.3f ? (a.noiseRatio - 0.2f) * 0.8f : 0.0f) : 0.6f));
    if (a.pitched && a.f0 < 100.0f) p.set (P::sub_level, 0.3f);
    if (a.pitched && a.f0 < 130.0f) p.set (P::oscA_coarse, a.f0 < 70.0f ? -24.0f : -12.0f);

    p.set (P::filter_type, (float) filterTypeNames().indexOf (a.pitched ? "LP24" : "BP12"));
    p.set (P::filter_cutoff, juce::jlimit (200.0f, 16000.0f, a.centroidSustain * (a.pitched ? 2.2f : 1.0f)));
    p.set (P::filter_res, a.pitched ? 0.15f : 0.3f);
    if (a.brightnessDrop > 0.3f)
    {
        p.set (P::filter_env, juce::jmin (4.0f, a.brightnessDrop * 1.5f));
        p.set (P::fenv_attack, 0.001f);
        p.set (P::fenv_decay, juce::jlimit (0.03f, 3.0f, a.decay * 0.6f));
        p.set (P::fenv_sustain, 0.1f);
    }
    if (a.width > 0.25f)
    {
        p.set (P::unison_voices, 3.0f);
        p.set (P::unison_detune, 12.0f);
        p.set (P::unison_spread, 0.7f);
    }
    if (a.vibratoHz > 0.0f)
    {
        p.set (lfoShapeParam (0), (float) ShapeSine);
        p.set (lfoRateParam (0), a.vibratoHz);
        p.set (lfoSyncParam (0), 0.0f);
        p.set (modSourceParam (0), (float) SrcLfo1);
        p.set (modDestParam (0), (float) TargetPitch);
        p.set (modAmountParam (0), juce::jlimit (0.005f, 0.03f, a.vibratoSemis / 12.0f));
    }
    else if (! a.pitched)
    {
        // A noise texture that stands still is just static: give the filter a slow drift.
        p.set (lfoShapeParam (0), (float) ShapeSine);
        p.set (lfoRateParam (0), 0.15f);
        p.set (lfoSyncParam (0), 0.0f);
        p.set (modSourceParam (0), (float) SrcLfo1);
        p.set (modDestParam (0), (float) modTargetForParam ((int) P::filter_cutoff));
        p.set (modAmountParam (0), 0.25f);
        p.set (P::reverb_mix, 0.3f);
    }
    keepPatchInTune (p);
    ensureMacroRoutings (p);
    return p;
}

} // namespace stacks
