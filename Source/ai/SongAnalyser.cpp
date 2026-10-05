#include "SongAnalyser.h"

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>

namespace stacks
{

namespace
{
    constexpr int kOrder = 10, kN = 1 << kOrder, kHop = 256;   // 1024-point frames, ~86 per second at 22 kHz
    const float kBandEdges[SongAnalysis::kBands + 1] = { 20.0f, 60.0f, 200.0f, 500.0f, 2000.0f, 6000.0f, 11000.0f };
    const char* const kBandNames[SongAnalysis::kBands] = { "sub (20-60 Hz)", "bass (60-200 Hz)", "low mids (200-500 Hz)",
                                                           "mids (500 Hz-2 kHz)", "upper mids (2-6 kHz)", "air (6 kHz and up)" };
    const char* const kNoteNames[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    // Krumhansl-Schmuckler key profiles.
    const float kMajor[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    const float kMinor[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

    float correlation (const float* a, const float* b, int n)
    {
        double ma = 0.0, mb = 0.0;
        for (int i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; }
        ma /= n; mb /= n;
        double num = 0.0, da = 0.0, db = 0.0;
        for (int i = 0; i < n; ++i) { num += (a[i] - ma) * (b[i] - mb); da += (a[i] - ma) * (a[i] - ma); db += (b[i] - mb) * (b[i] - mb); }
        return da > 0.0 && db > 0.0 ? (float) (num / std::sqrt (da * db)) : 0.0f;
    }
}

const char* SongAnalysis::bandName (int band) { return kBandNames[juce::jlimit (0, kBands - 1, band)]; }

juce::String SongAnalysis::keyName() const
{
    if (keyRoot < 0) return "no clear key";
    return juce::String (kNoteNames[keyRoot]) + (minor ? " minor" : " major");
}

bool readSongMono (const juce::File& file, std::vector<float>& mono, double& sampleRate, float& width, juce::String& error, double maxSeconds)
{
    if (! file.existsAsFile()) { error = "file not found"; return false; }
    static juce::AudioFormatManager formats;
    static std::once_flag once;
    std::call_once (once, [] { formats.registerBasicFormats(); });
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) { error = "not an audio file this build can read (wav, aiff, flac, mp3, ogg)"; return false; }

    const double srIn = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
    const int decimate = srIn >= 40000.0 ? 2 : 1;
    sampleRate = srIn / decimate;
    const juce::int64 total = juce::jmin (reader->lengthInSamples, (juce::int64) (maxSeconds * srIn));
    const int channels = juce::jlimit (1, 2, (int) reader->numChannels);
    mono.clear();
    mono.reserve ((size_t) (total / decimate + 1));
    juce::AudioBuffer<float> chunk (channels, 65536);
    double ll = 0.0, rr = 0.0, lr = 0.0;
    for (juce::int64 pos = 0; pos < total; pos += chunk.getNumSamples())
    {
        const int n = (int) juce::jmin ((juce::int64) chunk.getNumSamples(), total - pos);
        if (! reader->read (&chunk, 0, n, pos, true, channels > 1)) break;
        const float* L = chunk.getReadPointer (0);
        const float* R = chunk.getReadPointer (channels > 1 ? 1 : 0);
        for (int i = 0; i < n; i += decimate)
        {
            mono.push_back (0.5f * (L[i] + R[i]));
            ll += (double) L[i] * L[i]; rr += (double) R[i] * R[i]; lr += (double) L[i] * R[i];
        }
    }
    width = channels > 1 && ll > 0.0 && rr > 0.0 ? (float) juce::jlimit (0.0, 1.0, 1.0 - std::abs (lr) / std::sqrt (ll * rr)) : 0.0f;
    if (mono.size() < (size_t) (sampleRate * 2.0)) { error = "the file is too short to be a track"; return false; }
    return true;
}

SongAnalysis analyseSong (const std::vector<float>& x, double sr, float width)
{
    SongAnalysis s;
    s.width = width;
    const int len = (int) x.size();
    if (len < kN * 4) return s;
    s.duration = (float) (len / sr);

    // One pass of STFT frames: spectral flux (onsets), average power spectrum,
    // chroma, short-term loudness.
    juce::dsp::FFT fft (kOrder);
    std::vector<float> window ((size_t) kN), buf ((size_t) kN * 2), prevMag ((size_t) kN / 2, 0.0f), avgPower ((size_t) kN / 2, 0.0f);
    for (int i = 0; i < kN; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kN - 1));
    const float binHz = (float) (sr / kN);
    const int frames = (len - kN) / kHop;
    std::vector<float> flux ((size_t) frames, 0.0f), loudness;
    float chroma[12] {};
    double energyAcc = 0.0; int energyCount = 0;
    for (int f = 0; f < frames; ++f)
    {
        const int start = f * kHop;
        double e = 0.0;
        for (int i = 0; i < kN; ++i)
        {
            const float v = x[(size_t) (start + i)];
            buf[(size_t) i] = v * window[(size_t) i];
            e += (double) v * v;
        }
        std::fill (buf.begin() + kN, buf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (buf.data());
        float fl = 0.0f;
        for (int b = 1; b < kN / 2; ++b)
        {
            const float mag = std::log1p (buf[(size_t) b]);
            const float d = mag - prevMag[(size_t) b];
            if (d > 0.0f) fl += d;
            prevMag[(size_t) b] = mag;
            avgPower[(size_t) b] += buf[(size_t) b] * buf[(size_t) b];
        }
        flux[(size_t) f] = fl;
        energyAcc += e; ++energyCount;
        if (energyCount >= 34)   // ~400 ms of frames
        {
            loudness.push_back ((float) (10.0 * std::log10 (energyAcc / (energyCount * kN) + 1.0e-12)));
            energyAcc = 0.0; energyCount = 0;
        }
    }

    // Tempo: autocorrelation of the mean-removed onset curve over 60-200 BPM,
    // with a gentle preference for 85-150 so half and double tempos lose.
    const float fps = (float) (sr / kHop);
    {
        const int smooth = (int) (fps * 0.5f);
        std::vector<float> onset ((size_t) frames, 0.0f);
        double runningSum = 0.0;
        for (int f = 0; f < frames; ++f)
        {
            runningSum += flux[(size_t) f];
            if (f >= smooth) runningSum -= flux[(size_t) (f - smooth)];
            const float localMean = (float) (runningSum / juce::jmin (smooth, f + 1));
            onset[(size_t) f] = juce::jmax (0.0f, flux[(size_t) f] - localMean);
        }
        const int lagMin = (int) (60.0f * fps / 200.0f), lagMax = (int) (60.0f * fps / 60.0f);
        double norm = 0.0; for (float v : onset) norm += (double) v * v;
        float best = 0.0f; int bestLag = 0; double sumScore = 0.0; int scored = 0;
        for (int lag = lagMin; lag <= lagMax; ++lag)
        {
            double c = 0.0;
            for (int f = 0; f + lag < frames; ++f) c += (double) onset[(size_t) f] * onset[(size_t) f + lag];
            const float bpm = 60.0f * fps / (float) lag;
            const float prior = std::exp (-0.5f * std::pow ((std::log2 (bpm / 115.0f)) / 0.45f, 2.0f));
            const float score = norm > 0.0 ? (float) (c / norm) * (0.6f + 0.4f * prior) : 0.0f;
            sumScore += score; ++scored;
            if (score > best) { best = score; bestLag = lag; }
        }
        if (bestLag > 0)
        {
            s.bpm = 60.0f * fps / (float) bestLag;
            s.tempoConfidence = juce::jlimit (0.0f, 1.0f, scored > 0 ? (float) (best / (sumScore / scored)) / 6.0f : 0.0f);
        }
        // Density: onset peaks above the noise of the curve.
        double mean = 0.0, var = 0.0;
        for (float v : onset) mean += v; mean /= frames;
        for (float v : onset) var += (v - mean) * (v - mean); var /= frames;
        const float thr = (float) (mean + 1.5 * std::sqrt (var));
        int peaks = 0;
        for (int f = 1; f + 1 < frames; ++f)
            if (onset[(size_t) f] > thr && onset[(size_t) f] >= onset[(size_t) f - 1] && onset[(size_t) f] > onset[(size_t) f + 1]) ++peaks;
        s.onsetsPerSecond = (float) peaks / s.duration;
    }

    // Key: a second, high-resolution pass (8192-point frames, 2.7 Hz bins at 22 kHz,
    // so semitones stay apart down to 100 Hz). Chroma comes from the spectral
    // peaks of that average spectrum with a harmonic sieve: once a peak counts as
    // a note, its harmonics are taken down so a loud bass does not vote for the
    // major third through its fifth harmonic.
    {
        constexpr int kKeyOrder = 13, kKeyN = 1 << kKeyOrder, kKeyHop = kKeyN / 2;
        juce::dsp::FFT keyFft (kKeyOrder);
        std::vector<float> kbuf ((size_t) kKeyN * 2), kavg ((size_t) kKeyN / 2, 0.0f);
        const float kBinHz = (float) (sr / kKeyN);
        for (int start = 0; start + kKeyN <= len; start += kKeyHop)
        {
            for (int i = 0; i < kKeyN; ++i)
                kbuf[(size_t) i] = x[(size_t) (start + i)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kKeyN - 1)));
            std::fill (kbuf.begin() + kKeyN, kbuf.end(), 0.0f);
            keyFft.performFrequencyOnlyForwardTransform (kbuf.data());
            for (int b = 1; b < kKeyN / 2; ++b) kavg[(size_t) b] += kbuf[(size_t) b] * kbuf[(size_t) b];
        }
        std::vector<float> mag ((size_t) kKeyN / 2, 0.0f);
        for (int b = 1; b < kKeyN / 2; ++b) mag[(size_t) b] = std::sqrt (kavg[(size_t) b]);
        std::vector<int> peaks;
        for (int b = 2; b + 1 < kKeyN / 2; ++b)
        {
            const float hz = (float) b * kBinHz;
            if (hz >= 100.0f && hz <= 5000.0f && mag[(size_t) b] > mag[(size_t) b - 1] && mag[(size_t) b] >= mag[(size_t) b + 1])
                peaks.push_back (b);
        }
        std::sort (peaks.begin(), peaks.end(), [&] (int p, int q) { return mag[(size_t) p] > mag[(size_t) q]; });
        static const float sieve[] = { 0.6f, 0.5f, 0.45f, 0.4f, 0.35f, 0.3f };   // harmonics 2..7
        for (int b : peaks)
        {
            const float m = mag[(size_t) b];
            if (m <= 0.0f) continue;
            // quadratic interpolation of the peak frequency
            const float a0 = mag[(size_t) b - 1], a1 = m, a2 = mag[(size_t) b + 1];
            const float denom = a0 - 2.0f * a1 + a2;
            const float offset = std::abs (denom) > 1.0e-9f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (a0 - a2) / denom) : 0.0f;
            const float hz = ((float) b + offset) * kBinHz;
            const int pc = ((int) std::lround (12.0 * std::log2 (hz / 440.0)) % 12 + 12 + 9) % 12;   // 440 Hz is A = 9
            chroma[pc] += m;
            for (int h = 2; h <= 7; ++h)
                for (int d = -2; d <= 2; ++d)
                {
                    const int bin = b * h + d;
                    if (bin < kKeyN / 2) mag[(size_t) bin] = juce::jmax (0.0f, mag[(size_t) bin] - m * sieve[h - 2] / (d == 0 ? 1.0f : 1.0f + (float) std::abs (d)));
                }
        }
        // The bass line's favourite note breaks the relative major/minor tie.
        float bassChroma[12] {};
        for (int b = 2; b + 1 < kKeyN / 2; ++b)
        {
            const float hz = (float) b * kBinHz;
            if (hz >= 60.0f && hz <= 200.0f && kavg[(size_t) b] > kavg[(size_t) b - 1] && kavg[(size_t) b] >= kavg[(size_t) b + 1])
                bassChroma[((int) std::lround (12.0 * std::log2 (hz / 440.0)) % 12 + 12 + 9) % 12] += std::sqrt (kavg[(size_t) b]);
        }
        float strongest = 1.0e-9f;
        for (float c : chroma) strongest = juce::jmax (strongest, c);
        for (int i = 0; i < 12; ++i) s.chroma[i] = chroma[i] / strongest;
        float bestCorr = -2.0f, secondCorr = -2.0f;
        for (int root = 0; root < 12; ++root)
            for (int mode = 0; mode < 2; ++mode)
            {
                float rotated[12];
                for (int i = 0; i < 12; ++i) rotated[(i + root) % 12] = mode == 0 ? kMajor[i] : kMinor[i];
                const float c = correlation (chroma, rotated, 12);
                if (c > bestCorr) { secondCorr = bestCorr; bestCorr = c; s.keyRoot = root; s.minor = mode == 1; }
                else if (c > secondCorr) secondCorr = c;
            }
        s.keyConfidence = juce::jlimit (0.0f, 1.0f, (bestCorr - secondCorr) * 4.0f);
        if (bestCorr < 0.3f) s.keyRoot = -1;
        if (s.keyRoot >= 0)
        {
            const int relative = s.minor ? (s.keyRoot + 3) % 12 : (s.keyRoot + 9) % 12;
            if (bassChroma[relative] > 1.2f * bassChroma[s.keyRoot])
            {
                s.keyRoot = relative;
                s.minor = ! s.minor;
            }
        }
    }

    // Spectral balance: energy per octave in each band, relative to the median band.
    {
        float perOctave[SongAnalysis::kBands] {};
        double num = 0.0, den = 0.0;
        for (int b = 1; b < kN / 2; ++b)
        {
            const float hz = (float) b * binHz;
            num += hz * avgPower[(size_t) b]; den += avgPower[(size_t) b];
            for (int band = 0; band < SongAnalysis::kBands; ++band)
                if (hz >= kBandEdges[band] && hz < kBandEdges[band + 1]) { perOctave[band] += avgPower[(size_t) b]; break; }
        }
        s.centroidHz = den > 0.0 ? (float) (num / den) : 1000.0f;
        float db[SongAnalysis::kBands];
        for (int band = 0; band < SongAnalysis::kBands; ++band)
        {
            const float octaves = std::log2 (kBandEdges[band + 1] / kBandEdges[band]);
            db[band] = 10.0f * std::log10 (perOctave[band] / octaves + 1.0e-9f);
        }
        float sorted[SongAnalysis::kBands]; std::copy (db, db + SongAnalysis::kBands, sorted); std::sort (sorted, sorted + SongAnalysis::kBands);
        const float medianDb = 0.5f * (sorted[2] + sorted[3]);
        for (int band = 0; band < SongAnalysis::kBands; ++band) s.bandDb[band] = db[band] - medianDb;
        // The emptiest band among those a synth can use (bass through air), the fullest of all.
        s.openBand = 1; s.fullBand = 0;
        for (int band = 1; band < SongAnalysis::kBands; ++band) if (s.bandDb[band] < s.bandDb[s.openBand]) s.openBand = band;
        for (int band = 0; band < SongAnalysis::kBands; ++band) if (s.bandDb[band] > s.bandDb[s.fullBand]) s.fullBand = band;
    }

    // Dynamics: spread of the 400 ms loudness, ignoring the quietest 10 % (gaps and fades).
    if (loudness.size() > 10)
    {
        std::sort (loudness.begin(), loudness.end());
        const size_t from = loudness.size() / 10;
        double mean = 0.0; for (size_t i = from; i < loudness.size(); ++i) mean += loudness[i]; mean /= (double) (loudness.size() - from);
        double var = 0.0; for (size_t i = from; i < loudness.size(); ++i) var += (loudness[i] - mean) * (loudness[i] - mean); var /= (double) (loudness.size() - from);
        s.dynamicsDb = (float) std::sqrt (var);
    }

    // A feel, from tempo, lows and brightness. A guess, worded as one.
    const bool heavyLows = s.bandDb[0] > 2.0f || s.bandDb[1] > 2.0f;
    const bool bright = s.centroidHz > 1800.0f;
    const bool busy = s.onsetsPerSecond > 3.0f;
    if (s.bpm <= 0.0f)            s.feel = "free tempo";
    else if (s.bpm < 95.0f)       s.feel = heavyLows ? "hip-hop or trap" : "downtempo or ballad";
    else if (s.bpm < 115.0f)      s.feel = heavyLows ? "boom bap or dancehall" : "pop or soul";
    else if (s.bpm < 132.0f)      s.feel = bright ? "house" : "techno";
    else if (s.bpm < 150.0f)      s.feel = heavyLows ? "dubstep or trap (half-time)" : "trance";
    else if (s.bpm < 185.0f)      s.feel = "drum and bass or jungle";
    else                          s.feel = "footwork or very fast";

    juce::String b = "Design sounds that fit this track";
    if (s.bpm > 0.0f) b << ": " << juce::String ((int) std::round (s.bpm)) << " BPM";
    b << ", " << (s.keyRoot >= 0 && s.keyConfidence < 0.3f ? "probably " : "") << s.keyName();
    if (s.keyRoot >= 0)
        b << " (or its relative, " << kNoteNames[s.minor ? (s.keyRoot + 3) % 12 : (s.keyRoot + 9) % 12] << (s.minor ? " major" : " minor") << ")";
    b << " (feels like " << s.feel << "). ";
    b << "The mix is " << (bright ? "bright" : "dark") << " and " << (busy ? "busy" : "sparse")
      << (s.dynamicsDb < 3.0f ? ", dense and compressed" : s.dynamicsDb > 6.0f ? ", dynamic with room between parts" : "")
      << (s.width > 0.35f ? ", wide" : "") << "; heaviest in the " << SongAnalysis::bandName (s.fullBand)
      << ", emptiest in the " << SongAnalysis::bandName (s.openBand) << ": give the new sounds that space. ";
    if (s.keyRoot >= 0) b << "Stay in " << s.keyName() << ". ";
    if (s.bpm > 0.0f) b << "Sync LFOs and delays to the tempo. ";
    b << "Make five that would sit in the arrangement: one that fills the open band, one bass or low part that stays out of the "
      << SongAnalysis::bandName (s.fullBand) << ", one rhythmic element at the tempo, one pad or texture behind everything, one lead or hook.";
    s.brief = b;
    return s;
}

} // namespace stacks
