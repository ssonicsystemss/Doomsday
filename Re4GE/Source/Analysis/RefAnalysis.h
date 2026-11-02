#pragma once
// RE4GE - REFERENCE ANALYZER (step 7): a reverse bass you like (a WAV) vs the current sound, both looked at over ONE BEAT:
//   ENVELOPE  the level over the beat (100 points, normalised to its peak) + where the peak is (% of the beat)
//   BANDS     the share of energy in 6 bands (dB, loudness-independent): SUB / LOW / BODY / MID / PRESENCE / AIR
//   HINTS     what differs by more than 2 dB / 8 % of the beat, and which knob moves it
// Pure analysis on buffers (no GUI, no files): the GUI loads the WAV and renders the plugin (Exporter) on a background thread.
#include <juce_dsp/juce_dsp.h>
#include <array>

namespace re4ge
{
    struct RefProfile
    {
        bool valid = false;
        std::array<float, 100> env {};            // 0..1 over the beat
        float peakPct = 0.f;                      // where the loudest moment is (% of the beat)
        float startPct = 0.f;                     // where it gets louder than -24 dB of the peak (the bass "starts")
        std::array<float, 6> bandDb {};           // share of energy per band (dB re the total)
        static const juce::StringArray& bandNames() { static const juce::StringArray n { "SUB 20-60", "LOW 60-150", "BODY 150-400", "MID 400-1.5k", "PRESENCE 1.5-5k", "AIR 5-16k" }; return n; }
        static constexpr float bandEdges[7] { 20.f, 60.f, 150.f, 400.f, 1500.f, 5000.f, 16000.f };
    };

    struct RefAnalysis
    {
        // mono samples of one beat (or more: only the first beatSamples are used)
        static RefProfile analyse (const float* x, int n, double rate, int beatSamples)
        {
            RefProfile r;
            n = juce::jmin (n, juce::jmax (64, beatSamples));
            if (x == nullptr || n < 256) return r;
            // ENVELOPE: 100 points, each the RMS over ~20 ms around it (a bass cycle is ~20 ms: shorter windows draw the waveform, not the level)
            float pk = 0.f;
            const int half = juce::jmax (n / 200, (int) (0.010 * rate));
            for (int k = 0; k < 100; ++k)
            {
                const int c = (2 * k + 1) * n / 200, a = juce::jmax (0, c - half), e = juce::jmin (n, juce::jmax (a + 1, c + half));
                double acc = 0; for (int i = a; i < e; ++i) acc += (double) x[i] * x[i];
                r.env[(size_t) k] = (float) std::sqrt (acc / (e - a));
                if (r.env[(size_t) k] > pk) { pk = r.env[(size_t) k]; r.peakPct = (float) k; }
            }
            if (pk < 1.0e-6f) return r;
            for (auto& v : r.env) v /= pk;
            for (int k = 0; k < 100; ++k) if (r.env[(size_t) k] > 0.063f) { r.startPct = (float) k; break; }      // -24 dB
            // BANDS: averaged power spectrum (Hann, 4096, 50 % overlap)
            constexpr int order = 12, N = 1 << order;
            juce::dsp::FFT fft (order);
            std::vector<float> buf ((size_t) (2 * N)), power ((size_t) (N / 2), 0.f);
            int frames = 0;
            for (int start = 0; start == 0 || start + N <= n; start += N / 2)
            {
                std::fill (buf.begin(), buf.end(), 0.f);
                for (int i = 0; i < N && start + i < n; ++i) buf[(size_t) i] = x[start + i] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N));
                fft.performFrequencyOnlyForwardTransform (buf.data());
                for (int b = 0; b < N / 2; ++b) power[(size_t) b] += buf[(size_t) b] * buf[(size_t) b];
                ++frames;
                if (start + N >= n) break;
            }
            double band[6] {}, total = 0;
            for (int b = 1; b < N / 2; ++b)
            {
                const double hz = b * rate / N;
                for (int k = 0; k < 6; ++k) if (hz >= RefProfile::bandEdges[k] && hz < RefProfile::bandEdges[k + 1]) { band[k] += power[(size_t) b]; total += power[(size_t) b]; }
            }
            if (total <= 0) return r;
            for (int k = 0; k < 6; ++k) r.bandDb[(size_t) k] = (float) (10.0 * std::log10 (band[k] / total + 1.0e-9));
            r.valid = true;
            return r;
        }
        // the differences that matter, plugin vs reference, in plain words
        static juce::StringArray hints (const RefProfile& ref, const RefProfile& mine)
        {
            juce::StringArray h;
            if (! ref.valid || ! mine.valid) return h;
            static const char* fix[6] { "SUB level / SUB Hz", "WEIGHT macro / EQ low shelf", "BODY macro / EQ bell ~220 Hz",
                                        "EQ bell 400-1500 Hz / drive", "BITE macro / EQ bell ~2 kHz", "HIGH CUT / EQ high shelf" };
            struct D { int k; float d; };
            std::vector<D> ds;
            for (int k = 0; k < 6; ++k) { const float d = mine.bandDb[(size_t) k] - ref.bandDb[(size_t) k]; if (std::abs (d) > 2.f) ds.push_back ({ k, d }); }
            std::sort (ds.begin(), ds.end(), [] (const D& a, const D& b) { return std::abs (a.d) > std::abs (b.d); });
            for (size_t i = 0; i < juce::jmin<size_t> (3, ds.size()); ++i)
                h.add (RefProfile::bandNames()[ds[i].k] + ": " + (ds[i].d > 0 ? "+" : "") + juce::String (ds[i].d, 1) + " dB "
                       + (ds[i].d > 0 ? "more" : "less") + " than the reference -> " + fix[ds[i].k]);
            const float dp = mine.peakPct - ref.peakPct, ds2 = mine.startPct - ref.startPct;
            if (std::abs (ds2) > 8.f) h.add ("The bass starts " + juce::String (std::abs (ds2), 0) + " % of the beat " + (ds2 < 0 ? "EARLIER" : "LATER") + " -> ENV DELAY");
            if (std::abs (dp) > 8.f) h.add ("The peak comes " + juce::String (std::abs (dp), 0) + " % of the beat " + (dp < 0 ? "EARLIER" : "LATER") + " -> SWELL / CURVE / GATE");
            if (h.isEmpty()) h.add ("Close to the reference: no band differs by more than 2 dB, the timing within 8 % of the beat");
            return h;
        }
    };
}
