#include "AudioAudit.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>

#include "SynthVoice.h"
#include "Patch.h"
#include "Effects.h"

namespace stacks
{

namespace
{
    // The processor's safety clip, copied so the offline render matches the plug-in.
    inline float softClip (float x) noexcept
    {
        constexpr float t = 0.8f;
        const float a = std::abs (x);
        if (a <= t) return x;
        const float y = t + (1.0f - t) * std::tanh ((a - t) / (1.0f - t));
        return x < 0.0f ? -y : y;
    }

    inline float db (float x) noexcept { return 20.0f * std::log10 (juce::jmax (1.0e-9f, x)); }

    // The synth without the plug-in around it: voices, the shared context the
    // processor would fill per block, and the effect chain in its saved order.
    class OfflineSynth
    {
    public:
        OfflineSynth (double sr, int block) : sampleRate (sr), blockSize (block)
        {
            ctx.bank = &*bank;
            ctx.user = &user;
            ctx.params = &params;
            ctx.lfoTables = &lfoTables;
            ctx.samples = &samples;
            ctx.randomSeed = 2026;   // the same patch renders the same twice
            for (int i = 0; i < 16; ++i) synth.addVoice (new SynthVoice (ctx));
            synth.addSound (new SynthSound());
            synth.setNoteStealingEnabled (true);
            synth.setCurrentPlaybackSampleRate (sr);
            distortion.prepare (sr, block); eq.prepare (sr, block); compressor.prepare (sr, block); chorus.prepare (sr, block);
            delay.prepare (sr, block); reverb.prepare (sr, block); hyper.prepare (sr, block); tape.prepare (sr, block);
        }

        void load (const Patch& p)
        {
            patch = p;
            for (int i = 0; i < kNumParams; ++i) params.v[i] = p.values[(size_t) i];
            for (int osc = 0; osc < kNumOscs; ++osc)
                user.set (UserWavetables::customSlot (osc), p.waves[(size_t) osc].isEmpty() ? nullptr : buildSpectralTable (p.waves[(size_t) osc]));
            for (int k = 0; k < kNumLfos; ++k)
                lfoTables.set (k, (int) p.get (lfoShapeParam (k)), LfoPoints::fromJson (p.lfoShapes[(size_t) k]));
            synth.allNotesOff (0, false);
            distortion.reset(); eq.reset(); compressor.reset(); chorus.reset(); delay.reset(); reverb.reset(); hyper.reset(); tape.reset();
            for (auto& ph : lfoPhase) ph = 0.0f;
            for (auto& h : lfoHeld) h = 0.0f;
            ctx.lastNote.store (-1.0f);
            ctx.modWheel.store (0.0f);
        }

        // One note: on at 0, off at `holdSeconds`, rendered for `totalSeconds`.
        juce::AudioBuffer<float> render (int note, float velocity, double holdSeconds, double totalSeconds, bool withEffects)
        {
            const int total = (int) std::lround (totalSeconds * sampleRate);
            const int offAt = (int) std::lround (holdSeconds * sampleRate);
            juce::AudioBuffer<float> out (2, total);
            out.clear();
            juce::AudioBuffer<float> block (2, blockSize);
            juce::MidiBuffer midi;
            for (int pos = 0; pos < total; pos += blockSize)
            {
                const int n = juce::jmin (blockSize, total - pos);
                midi.clear();
                if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, velocity), 0);
                if (offAt >= pos && offAt < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, note), offAt - pos);
                block.clear();
                juce::AudioBuffer<float> part (block.getArrayOfWritePointers(), 2, 0, n);
                processBlock (part, midi, withEffects);
                for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, part, c, 0, n);
            }
            return out;
        }

    private:
        void processBlock (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, bool withEffects)
        {
            const int n = buffer.getNumSamples();
            ctx.bpm = 120.0;
            for (int k = 0; k < kNumLfos; ++k)
            {
                const float beats = lfoSyncBeats (params.geti (lfoSyncParam (k)));
                const float rate = beats > 0.0f ? (float) (ctx.bpm / 60.0) / beats : params.get (lfoRateParam (k));
                const int shape = params.geti (lfoShapeParam (k));
                ctx.lfoRateHz[k] = rate;
                ctx.lfoBlockPhase[k] = lfoPhase[k];
                ctx.lfoGlobalValue[k] = shape == ShapeRandom ? lfoHeld[k] : lfoTables.get (k).at (lfoPhase[k] + params.get (lfoPhaseParam (k)));
            }
            applyGlobalModulation();
            for (int k = 0; k < kNumLfos; ++k)
            {
                const float beats = lfoSyncBeats (params.geti (lfoSyncParam (k)));
                const float rate = beats > 0.0f ? (float) (ctx.bpm / 60.0) / beats : fxParams.get (lfoRateParam (k));
                lfoPhase[k] += rate * (float) n / (float) sampleRate;
                if (lfoPhase[k] >= 1.0f) { lfoPhase[k] -= std::floor (lfoPhase[k]); lfoHeld[k] = rng.nextFloat() * 2.0f - 1.0f; }
            }
            synth.renderNextBlock (buffer, midi, 0, n);
            if (withEffects)
                processEffects (buffer);
            const float g = juce::Decibels::decibelsToGain (fxParams.get (P::master_gain));
            for (int c = 0; c < 2; ++c)
            {
                float* x = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i) x[i] = softClip (x[i] * g);
            }
        }

        void applyGlobalModulation()
        {
            fxParams = params;
            for (int i = 0; i < kNumModSlots; ++i)
            {
                const int src = params.geti (modSourceParam (i));
                const int target = params.geti (modDestParam (i));
                const float amount = params.get (modAmountParam (i));
                const int paramIndex = modTargetParamIndex (target);
                if (src == SrcOff || paramIndex < 0 || std::abs (amount) < 1.0e-4f) continue;
                float s = 0.0f;
                switch (src)
                {
                    case SrcLfo1: case SrcLfo2: case SrcLfo3: case SrcLfo4: s = ctx.lfoGlobalValue[src - SrcLfo1]; break;
                    case SrcMacro1: case SrcMacro2: case SrcMacro3: case SrcMacro4: case SrcMacro5: case SrcMacro6: s = params.get (macroParam (src - SrcMacro1)); break;
                    default: continue;
                }
                nudgeParam (fxParams, paramIndex, amount * s);
            }
        }

        void processEffects (juce::AudioBuffer<float>& buffer)
        {
            DistortionFx::Params dist; dist.mode = fxParams.geti (P::dist_mode); dist.driveDb = fxParams.get (P::dist_drive); dist.toneHz = fxParams.get (P::dist_tone); dist.mix = fxParams.get (P::dist_mix);
            EqFx::Params eqp; eqp.lowGainDb = fxParams.get (P::eq_low_gain); eqp.lowHz = fxParams.get (P::eq_low_freq); eqp.midGainDb = fxParams.get (P::eq_mid_gain); eqp.midHz = fxParams.get (P::eq_mid_freq); eqp.midQ = fxParams.get (P::eq_mid_q); eqp.highGainDb = fxParams.get (P::eq_high_gain); eqp.highHz = fxParams.get (P::eq_high_freq);
            ChorusFx::Params cp; cp.mode = fxParams.geti (P::chorus_mode); cp.rate = fxParams.get (P::chorus_rate); cp.depth = fxParams.get (P::chorus_depth); cp.mix = fxParams.get (P::chorus_mix); cp.voices = fxParams.geti (P::chorus_voices); cp.feedback = fxParams.get (P::chorus_feedback); cp.spread = fxParams.get (P::chorus_spread); cp.toneHz = fxParams.get (P::chorus_tone);
            HyperFx::Params hp; hp.amount = fxParams.get (P::hyper_amount); hp.detune = fxParams.get (P::hyper_detune); hp.rateHz = fxParams.get (P::hyper_rate); hp.voices = fxParams.geti (P::hyper_voices);
            TapeFx::Params tp; tp.amount = fxParams.get (P::tape_amount); tp.wow = fxParams.get (P::tape_wow); tp.flutter = fxParams.get (P::tape_flutter); tp.sat = fxParams.get (P::tape_sat); tp.hiss = fxParams.get (P::tape_hiss); tp.toneHz = fxParams.get (P::tape_tone);
            DelayFx::Params dp; dp.mode = fxParams.geti (P::delay_mode); dp.sync = fxParams.geti (P::delay_sync); dp.timeSec = fxParams.get (P::delay_time); dp.feedback = fxParams.get (P::delay_feedback); dp.mix = fxParams.get (P::delay_mix); dp.toneHz = fxParams.get (P::delay_tone); dp.hpfHz = fxParams.get (P::delay_hpf); dp.wow = fxParams.get (P::delay_wow); dp.width = fxParams.get (P::delay_width);
            ReverbFx::Params rp; rp.type = fxParams.geti (P::reverb_type); rp.size = fxParams.get (P::reverb_size); rp.damp = fxParams.get (P::reverb_damp); rp.mix = fxParams.get (P::reverb_mix); rp.predelayMs = fxParams.get (P::reverb_predelay); rp.lowCutHz = fxParams.get (P::reverb_lowcut); rp.highCutHz = fxParams.get (P::reverb_highcut); rp.mod = fxParams.get (P::reverb_mod); rp.shimmer = fxParams.get (P::reverb_shimmer); rp.width = fxParams.get (P::reverb_width);
            CompressorFx::Params comp; comp.thresholdDb = fxParams.get (P::comp_threshold); comp.ratio = fxParams.get (P::comp_ratio); comp.attackMs = fxParams.get (P::comp_attack); comp.releaseMs = fxParams.get (P::comp_release); comp.makeupDb = fxParams.get (P::comp_makeup); comp.mix = fxParams.get (P::comp_mix);

            int order[kNumFx];
            for (int e = 0; e < kNumFx; ++e) order[e] = e;
            std::stable_sort (order, order + kNumFx, [&] (int a, int b) { return fxParams.geti (fxPosParam (a)) < fxParams.geti (fxPosParam (b)); });
            for (int e : order)
            {
                if (fxParams.geti (fxOnParam (e)) == 0) continue;
                switch (e)
                {
                    case 0: distortion.process (buffer, dist); break;
                    case 1: eq.process (buffer, eqp); break;
                    case 2: if (cp.mix > 0.0005f) chorus.process (buffer, cp); break;
                    case 3: hyper.process (buffer, hp); break;
                    case 4: tape.process (buffer, tp); break;
                    case 5: delay.process (buffer, dp, ctx.bpm); break;
                    case 6: if (rp.mix > 0.0005f) reverb.process (buffer, rp); break;
                    default: compressor.process (buffer, comp); break;
                }
            }
        }

        double sampleRate;
        int blockSize;
        juce::SharedResourcePointer<WavetableBank> bank;
        UserWavetables user;
        LfoTableBank lfoTables;
        SampleBank samples;
        SynthParams params, fxParams;
        VoiceContext ctx;
        juce::Synthesiser synth;
        Patch patch;
        float lfoPhase[kNumLfos] {}, lfoHeld[kNumLfos] {};
        juce::Random rng { 1234 };
        DistortionFx distortion; EqFx eq; CompressorFx compressor; ChorusFx chorus; DelayFx delay; ReverbFx reverb; HyperFx hyper; TapeFx tape;
    };

    struct Metrics
    {
        float peakDb = -100.0f, rmsDb = -100.0f, crestDb = 0.0f, dcDb = -100.0f;
        float attackMs = 0.0f, releaseMs = 0.0f, shape = 0.0f;   // shape: t(-6 dB) / t(-30 dB) of the release, 0.2 exponential, 0.5 linear
        float onsetDb = -100.0f;        // the first millisecond's peak against the note's peak: 0 dB is a hard edge
        float centroidHz = 0.0f, highPct = 0.0f, lowPct = 0.0f, width = 0.0f;
        float aliasDb = -100.0f;        // oscillator A alone on a high note: energy between the harmonics against the harmonics
        float endDb = -100.0f;          // what is still sounding when the render ends
        bool diedEarly = false;         // gone before the key was let go: release and shape mean nothing then
    };

    // RMS in short windows: the amplitude envelope.
    std::vector<float> envelope (const juce::AudioBuffer<float>& b, int window)
    {
        const int n = b.getNumSamples();
        std::vector<float> env ((size_t) ((n + window - 1) / window), 0.0f);
        for (int i = 0; i < n; i += window)
        {
            double sum = 0.0; int count = 0;
            for (int c = 0; c < b.getNumChannels(); ++c)
                for (int j = i; j < juce::jmin (n, i + window); ++j) { const float x = b.getSample (c, j); sum += x * x; ++count; }
            env[(size_t) (i / window)] = count > 0 ? (float) std::sqrt (sum / count) : 0.0f;
        }
        return env;
    }

    Metrics measure (const juce::AudioBuffer<float>& b, double sr, double holdSeconds)
    {
        Metrics m;
        const int n = b.getNumSamples();
        const int offAt = (int) (holdSeconds * sr);
        float peak = 0.0f; double sum = 0.0, dc = 0.0; int count = 0;
        for (int c = 0; c < 2; ++c)
        {
            const float* x = b.getReadPointer (c);
            for (int i = 0; i < n; ++i) { peak = juce::jmax (peak, std::abs (x[i])); if (i < offAt) { sum += x[i] * x[i]; dc += x[i]; ++count; } }
        }
        m.peakDb = db (peak);
        const float rms = count > 0 ? (float) std::sqrt (sum / count) : 0.0f;
        m.rmsDb = db (rms);
        m.crestDb = m.peakDb - m.rmsDb;
        m.dcDb = db (count > 0 ? (float) std::abs (dc / count) : 0.0f);

        // Onset: how hard the first millisecond hits.
        float first = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < juce::jmin (n, (int) (sr / 1000.0)); ++i) first = juce::jmax (first, std::abs (b.getSample (c, i)));
        m.onsetDb = db (first) - m.peakDb;

        const int window = (int) (sr / 500.0);   // 2 ms
        const auto env = envelope (b, window);
        auto at = [&] (int sample) { return env[(size_t) juce::jlimit (0, (int) env.size() - 1, sample / window)]; };

        // Attack: time to 90% of the loudest moment of the held part.
        float holdPeak = 0.0f;
        for (int i = 0; i < offAt / window; ++i) holdPeak = juce::jmax (holdPeak, env[(size_t) i]);
        for (int i = 0; i < offAt / window; ++i)
            if (env[(size_t) i] >= 0.9f * holdPeak) { m.attackMs = (float) (i * window * 1000.0 / sr); break; }

        // Release: from the level at note-off, how long to -6, -30 and -40 dB.
        // A sound that had already died by then (a pluck) has no release to measure.
        const float e0 = juce::jmax (1.0e-6f, at (offAt - window));
        m.diedEarly = e0 < holdPeak * 0.0316f;   // more than 30 dB under its own loudest moment
        int t6 = -1, t30 = -1, t40 = -1;
        for (int i = offAt / window; i < (int) env.size(); ++i)
        {
            const float d = db (env[(size_t) i] / e0);
            if (t6 < 0 && d <= -6.0f) t6 = i * window - offAt;
            if (t30 < 0 && d <= -30.0f) t30 = i * window - offAt;
            if (t40 < 0 && d <= -40.0f) { t40 = i * window - offAt; break; }
        }
        m.releaseMs = t40 >= 0 ? (float) (t40 * 1000.0 / sr) : (float) ((n - offAt) * 1000.0 / sr);
        m.shape = t6 >= 0 && t30 > 0 ? (float) t6 / (float) t30 : 0.0f;
        m.endDb = db (env.back()) - m.peakDb;

        // Spectrum of the held part: eight Hann windows from 0.5 s on (or what there is).
        constexpr int order = 12, size = 1 << order;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> hann ((size_t) size, juce::dsp::WindowingFunction<float>::hann);
        std::vector<float> acc ((size_t) size / 2, 0.0f), buf ((size_t) size * 2);
        int frames = 0;
        const int start = juce::jmin (offAt - size, (int) (0.5 * sr));
        for (int f = 0; f < 8; ++f)
        {
            const int from = juce::jmax (0, start) + f * size / 2;
            if (from + size > offAt || from + size > n) break;
            std::fill (buf.begin(), buf.end(), 0.0f);
            for (int i = 0; i < size; ++i) buf[(size_t) i] = 0.5f * (b.getSample (0, from + i) + b.getSample (1, from + i));
            hann.multiplyWithWindowingTable (buf.data(), (size_t) size);
            fft.performFrequencyOnlyForwardTransform (buf.data());
            for (int k = 0; k < size / 2; ++k) acc[(size_t) k] += buf[(size_t) k] * buf[(size_t) k];
            ++frames;
        }
        if (frames > 0)
        {
            double total = 0.0, weighted = 0.0, high = 0.0, low = 0.0;
            const double binHz = sr / size;
            for (int k = 1; k < size / 2; ++k)
            {
                const double e = acc[(size_t) k], hz = k * binHz;
                total += e; weighted += e * hz;
                if (hz >= 5000.0) high += e;
                if (hz < 150.0) low += e;
            }
            if (total > 0.0) { m.centroidHz = (float) (weighted / total); m.highPct = (float) (100.0 * high / total); m.lowPct = (float) (100.0 * low / total); }
        }

        // Width: 1 - correlation of left and right over the held part.
        {
            double ll = 0.0, rr = 0.0, lr = 0.0;
            const float* l = b.getReadPointer (0); const float* r = b.getReadPointer (1);
            for (int i = 0; i < offAt; ++i) { ll += l[i] * l[i]; rr += r[i] * r[i]; lr += l[i] * r[i]; }
            m.width = ll > 0.0 && rr > 0.0 ? (float) (1.0 - lr / std::sqrt (ll * rr)) : 0.0f;
        }
        return m;
    }

    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& b, double sr);

    // Oscillator A on its own, wide open, on a high note: what lies between the
    // harmonics is interpolation and mip noise, i.e. aliasing.
    float aliasIndex (OfflineSynth& synth, const Patch& p, double sr)
    {
        Patch bare = p;
        bare.set (P::oscB_level, 0.0f); bare.set (P::oscC_level, 0.0f); bare.set (P::fm_amount, 0.0f);
        bare.set (P::sub_level, 0.0f); bare.set (P::noise_level, 0.0f); bare.set (P::smp_mode, 0.0f);
        bare.set (P::unison_voices, 1.0f); bare.set (P::oscA_coarse, 0.0f); bare.set (P::oscA_fine, 0.0f); bare.set (P::oscA_level, 0.8f);
        bare.set (P::filter_type, 0.0f); bare.set (P::filter_cutoff, 20000.0f); bare.set (P::filter_res, 0.0f); bare.set (P::filter_env, 0.0f);
        bare.set (P::filter_drive, 1.0f); bare.set (P::filter_routing, 0.0f); bare.set (P::filter_model, 0.0f); bare.set (P::filter_keytrack, 0.0f);
        bare.set (P::char_sat, 0.0f); bare.set (P::char_drift, 0.0f); bare.set (P::char_env_var, 0.0f); bare.set (P::life, 0.0f);
        bare.set (P::aenv_attack, 0.005f); bare.set (P::aenv_decay, 1.0f); bare.set (P::aenv_sustain, 1.0f); bare.set (P::aenv_release, 0.05f);
        bare.set (P::glide, 0.0f); bare.set (P::master_gain, 0.0f);
        for (int i = 0; i < kNumModSlots; ++i) bare.set (modSourceParam (i), 0.0f);
        synth.load (bare);
        const int note = 96;   // C7, 2093 Hz
        auto b = synth.render (note, 1.0f, 1.0, 1.0, false);
        if (const char* dump = std::getenv ("STACKS_AUDIT_ALIAS_WAV"))   // the bare render, for looking at its spectrum elsewhere
            writeWav (juce::File (juce::String::fromUTF8 (dump)), b, sr);
        constexpr int order = 13, size = 1 << order;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> win ((size_t) size, juce::dsp::WindowingFunction<float>::blackmanHarris);
        std::vector<float> buf ((size_t) size * 2, 0.0f);
        const int from = (int) (0.5 * sr);
        if (from + size > b.getNumSamples()) return -100.0f;
        for (int i = 0; i < size; ++i) buf[(size_t) i] = b.getSample (0, from + i);
        win.multiplyWithWindowingTable (buf.data(), (size_t) size);
        fft.performFrequencyOnlyForwardTransform (buf.data());
        const double f0 = 440.0 * std::exp2 ((note - 69) / 12.0), binHz = sr / size;
        double harmonic = 0.0, other = 0.0;
        for (int k = 4; k < size / 2; ++k)
        {
            const double hz = k * binHz, ratio = hz / f0;
            const int nearest = (int) std::lround (ratio);
            const bool onHarmonic = nearest >= 1 && std::abs (ratio - nearest) * f0 <= 5.0 * binHz;   // five bins either side: the window's main lobe
            const double e = (double) buf[(size_t) k] * buf[(size_t) k];
            (onHarmonic ? harmonic : other) += e;
        }
        return harmonic > 0.0 ? (float) (10.0 * std::log10 (juce::jmax (1.0e-20, other) / harmonic)) : -100.0f;
    }

    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& b, double sr)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;
        if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0)))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            }
    }

    struct Row { juce::String name, category; Metrics m; juce::StringArray flags; };

    juce::StringArray flagsFor (const Row& r)
    {
        juce::StringArray f;
        const auto cat = r.category.toLowerCase();
        const bool perc = cat.contains ("perc") || cat.contains ("drum") || cat.contains ("hit");
        const bool sustained = r.m.releaseMs > 150.0f;
        if (r.m.rmsDb > -8.0f) f.add ("loud");
        if (r.m.rmsDb < -30.0f) f.add ("quiet");
        if (r.m.onsetDb > -6.0f && r.m.attackMs < 3.0f) f.add ("hard edge");
        if (r.m.diedEarly) { if (cat.contains ("pad") || cat.contains ("key") || cat.contains ("lead") || cat.contains ("drone") || cat.contains ("texture")) f.add ("dies early"); }
        else if (! perc && r.m.releaseMs < 30.0f) f.add ("cut off");
        if (sustained && ! r.m.diedEarly && r.m.shape > 0.4f) f.add ("linear tail");
        if (r.m.centroidHz < 350.0f && ! cat.contains ("bass")) f.add ("dull");
        if (r.m.highPct > 35.0f) f.add ("fizzy");
        if (cat.contains ("bass") && r.m.lowPct < 5.0f) f.add ("no low end");
        if (r.m.aliasDb > -40.0f) f.add ("aliasing");
        if (r.m.dcDb > -40.0f) f.add ("dc offset");
        if (r.m.endDb > -50.0f) f.add ("still ringing");
        return f;
    }
}

int levelPatches (const juce::StringArray& args)
{
    juce::Array<juce::File> files;
    float targetDb = -10.0f;
    bool dryRun = false;
    for (int i = 0; i < args.size(); ++i)
    {
        const auto& a = args[i];
        if (a == "--target" && i + 1 < args.size()) targetDb = args[++i].getFloatValue();
        else if (a == "--dry-run") dryRun = true;
        else
        {
            const auto f = juce::File::getCurrentWorkingDirectory().getChildFile (a);
            if (f.isDirectory()) files.addArray (f.findChildFiles (juce::File::findFiles, true, "*.json"));
            else if (f.existsAsFile()) files.add (f);
        }
    }
    if (files.isEmpty()) { std::printf ("usage: StacksTests --level <patch.json | folder>... [--target <dBFS>] [--dry-run]\n"); return 2; }
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFullPathName() < b.getFullPathName(); });

    const double sr = 48000.0;
    OfflineSynth synth (sr, 256);
    int changed = 0, capped = 0;
    for (const auto& file : files)
    {
        auto p = Patch::fromJson (file.loadFileAsString());
        if (! p) continue;
        const auto cat = p->category.toLowerCase();
        const int note = cat.contains ("bass") ? 48 : cat.contains ("bell") ? 72 : 60;   // where each kind is usually played
        synth.load (*p);
        auto b = synth.render (note, 0.8f, 1.5, 3.0, true);
        float peak = 0.0f;
        for (int c = 0; c < 2; ++c) peak = juce::jmax (peak, b.getMagnitude (c, 0, b.getNumSamples()));
        const float peakDb = db (peak);
        const float wanted = p->get (P::master_gain) + (targetDb - peakDb);
        const float gain = juce::jlimit (spec (P::master_gain).min, spec (P::master_gain).max, wanted);
        if (std::abs (gain - wanted) > 0.1f) ++capped;
        const bool change = std::abs (gain - p->get (P::master_gain)) > 1.0f;
        std::printf ("  %-28s %-8s note %3d  peak %6.1f dB  master %5.1f -> %5.1f%s\n", p->name.substring (0, 28).toRawUTF8(), p->category.substring (0, 8).toRawUTF8(), note, peakDb,
                     p->get (P::master_gain), gain, std::abs (gain - wanted) > 0.1f ? "  (at the limit)" : "");
        if (change && ! dryRun)
        {
            p->set (P::master_gain, gain);
            file.replaceWithText (p->toJson());
        }
        if (change) ++changed;
    }
    std::printf ("%s %d of %d patches (%d at Master's limit)\n", dryRun ? "would change" : "changed", changed, files.size(), capped);
    return 0;
}

int audioAudit (const juce::StringArray& args)
{
    juce::Array<juce::File> files;
    juce::File wavDir, outFile;
    int note = 60;
    for (int i = 0; i < args.size(); ++i)
    {
        const auto& a = args[i];
        if (a == "--wav" && i + 1 < args.size())       wavDir = juce::File::getCurrentWorkingDirectory().getChildFile (args[++i]);
        else if (a == "--out" && i + 1 < args.size())  outFile = juce::File::getCurrentWorkingDirectory().getChildFile (args[++i]);
        else if (a == "--note" && i + 1 < args.size()) note = args[++i].getIntValue();
        else
        {
            const auto f = juce::File::getCurrentWorkingDirectory().getChildFile (a);
            if (f.isDirectory()) files.addArray (f.findChildFiles (juce::File::findFiles, true, "*.json"));
            else if (f.existsAsFile()) files.add (f);
            else std::printf ("not found: %s\n", a.toRawUTF8());
        }
    }
    if (files.isEmpty())
    {
        std::printf ("usage: StacksTests --audit <patch.json | folder>... [--wav <dir>] [--out <file.md>] [--note <midi>]\n");
        return 2;
    }
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFullPathName() < b.getFullPathName(); });
    if (wavDir != juce::File()) wavDir.createDirectory();

    const double sr = 48000.0;
    const double hold = 1.5, total = 4.0;
    OfflineSynth synth (sr, 256);
    std::vector<Row> rows;
    juce::String md;
    md << "| patch | kind | rms dB | peak dB | onset dB | attack ms | release ms | shape | centroid Hz | >5k % | <150 % | width | alias dB | flags |\n|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n";
    std::printf ("%-28s %-8s %6s %6s %6s %7s %8s %5s %7s %5s %5s %5s %6s  %s\n", "patch", "kind", "rms", "peak", "onset", "attack", "release", "shape", "centr", ">5k", "<150", "width", "alias", "flags");
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (const auto& file : files)
    {
        auto p = Patch::fromJson (file.loadFileAsString());
        if (! p) { std::printf ("  skipped %s\n", file.getFileName().toRawUTF8()); continue; }
        Row r;
        r.name = p->name.isNotEmpty() ? p->name : file.getFileNameWithoutExtension();
        r.category = p->category;
        r.m.aliasDb = aliasIndex (synth, *p, sr);
        synth.load (*p);
        auto dryRender = synth.render (note, 0.8f, hold, total, false);   // the envelope's own shape, before any tail is added
        const auto dry = measure (dryRender, sr, hold);
        synth.load (*p);
        auto b = synth.render (note, 0.8f, hold, total, true);
        const float alias = r.m.aliasDb;
        r.m = measure (b, sr, hold);
        r.m.aliasDb = alias;
        r.m.attackMs = dry.attackMs; r.m.releaseMs = dry.releaseMs; r.m.shape = dry.shape; r.m.onsetDb = dry.onsetDb; r.m.diedEarly = dry.diedEarly;
        r.flags = flagsFor (r);
        if (wavDir != juce::File())
            writeWav (wavDir.getChildFile (juce::File::createLegalFileName (r.name) + ".wav"), b, sr);
        std::printf ("%-28s %-8s %6.1f %6.1f %6.1f %7.1f %8.0f %5.2f %7.0f %5.1f %5.1f %5.2f %6.1f  %s\n", r.name.substring (0, 28).toRawUTF8(), r.category.substring (0, 8).toRawUTF8(),
                     r.m.rmsDb, r.m.peakDb, r.m.onsetDb, r.m.attackMs, r.m.releaseMs, r.m.shape, r.m.centroidHz, r.m.highPct, r.m.lowPct, r.m.width, r.m.aliasDb, r.flags.joinIntoString (", ").toRawUTF8());
        std::fflush (stdout);
        md << "| " << r.name << " | " << r.category << " | " << juce::String (r.m.rmsDb, 1) << " | " << juce::String (r.m.peakDb, 1) << " | " << juce::String (r.m.onsetDb, 1) << " | " << juce::String (r.m.attackMs, 1)
           << " | " << juce::String (r.m.releaseMs, 0) << " | " << juce::String (r.m.shape, 2) << " | " << juce::String (r.m.centroidHz, 0) << " | " << juce::String (r.m.highPct, 1) << " | " << juce::String (r.m.lowPct, 1)
           << " | " << juce::String (r.m.width, 2) << " | " << juce::String (r.m.aliasDb, 1) << " | " << r.flags.joinIntoString (", ") << " |\n";
        rows.push_back (r);
    }
    if (rows.empty()) return 1;

    // Summary: medians, spreads and how many patches raise each flag.
    auto median = [&] (std::function<float (const Row&)> get)
    {
        std::vector<float> v; for (const auto& r : rows) v.push_back (get (r));
        std::sort (v.begin(), v.end());
        return v[v.size() / 2];
    };
    auto spread = [&] (std::function<float (const Row&)> get)
    {
        std::vector<float> v; for (const auto& r : rows) v.push_back (get (r));
        std::sort (v.begin(), v.end());
        return v[v.size() * 9 / 10] - v[v.size() / 10];
    };
    std::map<juce::String, int> flagCount;
    for (const auto& r : rows) for (const auto& f : r.flags) ++flagCount[f];
    juce::String summary;
    summary << "\n" << rows.size() << " patches at MIDI " << note << ", " << juce::String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 0) << " s.\n"
            << "median rms " << juce::String (median ([] (const Row& r) { return r.m.rmsDb; }), 1) << " dB (10-90% spread " << juce::String (spread ([] (const Row& r) { return r.m.rmsDb; }), 1) << " dB)"
            << ", median peak " << juce::String (median ([] (const Row& r) { return r.m.peakDb; }), 1) << " dB"
            << ", median attack " << juce::String (median ([] (const Row& r) { return r.m.attackMs; }), 0) << " ms"
            << ", median release " << juce::String (median ([] (const Row& r) { return r.m.releaseMs; }), 0) << " ms"
            << ", median release shape " << juce::String (median ([] (const Row& r) { return r.m.shape; }), 2)
            << ", median centroid " << juce::String (median ([] (const Row& r) { return r.m.centroidHz; }), 0) << " Hz"
            << ", median width " << juce::String (median ([] (const Row& r) { return r.m.width; }), 2)
            << ", median alias " << juce::String (median ([] (const Row& r) { return r.m.aliasDb; }), 1) << " dB"
            << ", worst alias " << juce::String ((*std::max_element (rows.begin(), rows.end(), [] (const Row& a, const Row& b) { return a.m.aliasDb < b.m.aliasDb; })).m.aliasDb, 1) << " dB.\n";
    summary << "flags:";
    for (const auto& fc : flagCount) summary << " " << fc.first << " " << fc.second << ",";
    summary = summary.trimCharactersAtEnd (",") + "\n";
    std::printf ("%s", summary.toRawUTF8());
    if (outFile != juce::File())
    {
        outFile.getParentDirectory().createDirectory();
        outFile.replaceWithText ("# Audio audit " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H.%M") + "\n\n"
                                 "One note (MIDI " + juce::String (note) + ", velocity 0.8) held 1.5 s then released, 4 s in all, at 48 kHz through the voices and the effect chain. "
                                 "onset = the first millisecond's peak against the note's peak (0 = a hard edge); attack = time to 90% of the held level; release = time from note-off to -40 dB; "
                                 "shape = t(-6 dB) / t(-30 dB) of the release (0.2 exponential, 0.5 linear); onset, attack, release and shape are measured on the dry voice, the rest after the effects; alias = oscillator A alone at C7, energy between the harmonics against the harmonics.\n\n"
                                 + md + summary);
        std::printf ("written %s\n", outFile.getFullPathName().toRawUTF8());
    }
    return 0;
}

} // namespace stacks
