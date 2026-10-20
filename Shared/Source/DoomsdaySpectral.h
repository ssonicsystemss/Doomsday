#pragma once
// =====================================================================
//  DOOMSDAY shared: spectral processing (namespace doomsday::spectral)
//
//   ResonanceSuppressor   soothe-style dynamic resonance suppression (see Shared/RESEARCH.md: soothe2).
//     Short FFT frames (2048, 75 % overlap). In every frame each bin is compared with a smoothed version of the spectrum
//     around it; what sticks out above it (a resonance, a whistle, a harsh screech peak) is turned down, only while it
//     sticks out. Controls:
//       DEPTH (0..1)        how much of the sticking-out part is removed (1 = flattened to its surroundings), max cut 30 dB
//       SHARPNESS (0..1)    how narrow the surroundings are: high = only narrow peaks are touched, low = broad build-ups too
//       SELECTIVITY (0..1)  how far a bin must stick out before it is touched (0 = almost everything, 1 = only big peaks)
//       ATTACK / RELEASE    how fast the cuts come and go (ms, per bin)
//       FOCUS low / high Hz where it works (outside: untouched)
//       MIX, DELTA (hear only what is removed)
//     Gains are linked between L and R (stereo image stays). Latency = 2047 samples (latencySamples (); report it to the host).
//
//  Allocates in prepare () only.  Tested in Shared/Tests (peak cut, flat noise untouched, delta, latency).
// =====================================================================
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <cmath>

namespace doomsday::spectral
{
    class ResonanceSuppressor
    {
    public:
        static constexpr int order = 11, N = 1 << order, hop = N / 4, bins = N / 2 + 1;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            fft = std::make_unique<juce::dsp::FFT> (order);
            win.resize ((size_t) N);
            for (int i = 0; i < N; ++i) win[(size_t) i] = std::sqrt (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N));
            for (int ch = 0; ch < 2; ++ch)
            {
                inBuf[(size_t) ch].assign ((size_t) N, 0.f);
                outAcc[(size_t) ch].assign ((size_t) N * 2, 0.f);
                frame[(size_t) ch].assign ((size_t) N * 2, 0.f);
                dry[(size_t) ch].assign ((size_t) N + 4, 0.f);
            }
            magDb.assign ((size_t) bins, -120.f); smooth.assign ((size_t) bins, -120.f); gainDb.assign ((size_t) bins, 0.f);
            prefix.assign ((size_t) bins + 1, 0.0);
            reset();
            setTimes (5.f, 80.f);
        }
        void reset()
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                std::fill (inBuf[(size_t) ch].begin(), inBuf[(size_t) ch].end(), 0.f);
                std::fill (outAcc[(size_t) ch].begin(), outAcc[(size_t) ch].end(), 0.f);
                std::fill (dry[(size_t) ch].begin(), dry[(size_t) ch].end(), 0.f);
            }
            std::fill (gainDb.begin(), gainDb.end(), 0.f);
            fill = 0; outPos = 0; dryPos = 0;
        }
        int latencySamples() const { return N - 1; }

        void setDepth (float d) { depth = juce::jlimit (0.f, 1.f, d); }
        void setSharpness (float s) { sharp = juce::jlimit (0.f, 1.f, s); }
        void setSelectivity (float s) { select = juce::jlimit (0.f, 1.f, s); }
        void setTimes (float attackMs, float releaseMs)
        {
            const float frameSec = (float) hop / (float) sr;
            att = 1.f - std::exp (-frameSec / juce::jmax (0.001f, attackMs * 0.001f));
            rel = 1.f - std::exp (-frameSec / juce::jmax (0.001f, releaseMs * 0.001f));
        }
        void setFocus (float lowHz, float highHz) { loBin = juce::jlimit (1, bins - 1, (int) (lowHz / sr * N)); hiBin = juce::jlimit (loBin, bins - 1, (int) (highHz / sr * N)); }
        void setMix (float m) { mix = juce::jlimit (0.f, 1.f, m); }
        void setDelta (bool d) { delta = d; }
        // current cut per bin in dB (<= 0), for drawing
        const std::vector<float>& cutDb() const { return gainDb; }
        float binHz (int b) const { return (float) (b * sr / N); }

        void process (float* L, float* R, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                float* io[2] = { &L[i], R != nullptr ? &R[i] : &L[i] };
                for (int ch = 0; ch < 2; ++ch)
                {
                    inBuf[(size_t) ch][(size_t) fill] = *io[ch];
                    dry[(size_t) ch][(size_t) dryPos] = *io[ch];
                }
                if (++fill == N) { runFrame(); }
                // output: the overlap-added result, one frame (N) late; dry delayed by the same amount
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float wet = outAcc[(size_t) ch][(size_t) outPos];
                    const float d = dry[(size_t) ch][(size_t) ((dryPos + 1) % (N))];
                    const float out = d + mix * (wet - d);
                    *io[ch] = delta ? d - out : out;
                    if (R == nullptr) break;
                }
                outAcc[0][(size_t) outPos] = 0.f; outAcc[1][(size_t) outPos] = 0.f;
                outPos = (outPos + 1) % (N * 2);
                dryPos = (dryPos + 1) % N;
            }
        }
    private:
        void runFrame()
        {
            // magnitude of the linked (L+R) spectrum
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& f = frame[(size_t) ch];
                for (int i = 0; i < N; ++i) f[(size_t) i] = inBuf[(size_t) ch][(size_t) i] * win[(size_t) i];
                std::fill (f.begin() + N, f.end(), 0.f);
                fft->performRealOnlyForwardTransform (f.data(), true);
            }
            for (int b = 0; b < bins; ++b)
            {
                float p = 0.f;
                for (int ch = 0; ch < 2; ++ch) { const float re = frame[(size_t) ch][(size_t) (2 * b)], im = frame[(size_t) ch][(size_t) (2 * b + 1)]; p += re * re + im * im; }
                magDb[(size_t) b] = 10.f * std::log10 (p + 1.0e-12f);
            }
            // surroundings: moving average in dB, width from SHARPNESS (wide = only narrow peaks stick out)
            const int w = 2 + (int) (sharp * 40.f);
            prefix[0] = 0.0;
            for (int b = 0; b < bins; ++b) prefix[(size_t) b + 1] = prefix[(size_t) b] + magDb[(size_t) b];
            for (int b = 0; b < bins; ++b)
            {
                const int a = juce::jmax (0, b - w), e = juce::jmin (bins, b + w + 1);
                smooth[(size_t) b] = (float) ((prefix[(size_t) e] - prefix[(size_t) a]) / (e - a));
            }
            const float thr = 1.f + select * 11.f;           // dB above the surroundings before anything happens
            for (int b = 0; b < bins; ++b)
            {
                float target = 0.f;
                if (b >= loBin && b <= hiBin)
                {
                    const float excess = magDb[(size_t) b] - smooth[(size_t) b] - thr;
                    if (excess > 0.f) target = -juce::jmin (30.f, excess * depth);
                }
                auto& g = gainDb[(size_t) b];
                g += (target < g ? att : rel) * (target - g);
            }
            // apply, inverse, overlap-add
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& f = frame[(size_t) ch];
                for (int b = 0; b < bins; ++b)
                {
                    const float k = std::pow (10.f, gainDb[(size_t) b] / 20.f);
                    f[(size_t) (2 * b)] *= k; f[(size_t) (2 * b + 1)] *= k;
                }
                fft->performRealOnlyInverseTransform (f.data());
                for (int i = 0; i < N; ++i)
                    outAcc[(size_t) ch][(size_t) ((outPos + i) % (N * 2))] += f[(size_t) i] * win[(size_t) i] * 0.5f;   // sqrt-Hann x2, 75 % overlap -> x2
                // keep the last N - hop input samples
                auto& in = inBuf[(size_t) ch];
                std::copy (in.begin() + hop, in.end(), in.begin());
            }
            fill = N - hop;
        }

        double sr = 44100.0;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> win, magDb, smooth, gainDb;
        std::vector<double> prefix;
        std::array<std::vector<float>, 2> inBuf, outAcc, frame, dry;
        int fill = 0, outPos = 0, dryPos = 0, loBin = 1, hiBin = bins - 1;
        float depth = 0.5f, sharp = 0.5f, select = 0.5f, att = 0.5f, rel = 0.1f, mix = 1.f;
        bool delta = false;
    };
}
