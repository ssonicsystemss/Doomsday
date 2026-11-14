#pragma once
// =====================================================================
//  4GE - LIVE SPECTRUM (owner 2026-11-14 "widma na żywo przy graniu, na każdym EQ"): a lock-free tap of the plugin's output
//  (the audio thread pushes, nothing else) + the analysis the GUI asks for on its timer (FFT -> dB per log bin, the same bins
//  as EqScope: 20 Hz .. 20 kHz). DSP only: no GUI, no files; the tap never allocates or locks.
// =====================================================================
#include <atomic>
#include <array>
#include <vector>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

namespace fge::dsp::live
{
    struct Tap
    {
        static constexpr int N = 8192;                         // (power of two)
        std::array<std::atomic<float>, N> buf {};
        std::atomic<int> w { 0 };
        std::atomic<double> rate { 44100.0 };
        std::atomic<std::int64_t> pushed { 0 };
        // audio thread: one block of the output (mono sum)
        void push (const float* l, const float* r, int n, double fs)
        {
            int wi = w.load (std::memory_order_relaxed);
            for (int i = 0; i < n; ++i)
            {
                buf[(size_t) wi].store (r != nullptr ? 0.5f * (l[i] + r[i]) : l[i], std::memory_order_relaxed);
                wi = (wi + 1) & (N - 1);
            }
            w.store (wi, std::memory_order_release);
            rate.store (fs, std::memory_order_relaxed);
            pushed.fetch_add (n, std::memory_order_relaxed);
        }
    };

    // message thread: the last 4096 samples -> dB (0 = a full-scale sine) per log bin k (20 * 1000^(k / (bins - 1)) Hz)
    class Analyser
    {
    public:
        static constexpr int order = 12, len = 1 << order;
        void compute (const Tap& t, int bins, std::vector<float>& outDb)
        {
            work.assign ((size_t) len * 2, 0.f);
            const int end = t.w.load (std::memory_order_acquire);
            for (int i = 0; i < len; ++i)
            {
                const int k = (end - len + i) & (Tap::N - 1);
                const float win = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) (len - 1));
                work[(size_t) i] = t.buf[(size_t) k].load (std::memory_order_relaxed) * win;
            }
            fft.performFrequencyOnlyForwardTransform (work.data());
            const double fs = t.rate.load (std::memory_order_relaxed);
            outDb.assign ((size_t) bins, -120.f);
            for (int b = 0; b < bins; ++b)
            {
                const double f0 = 20.0 * std::pow (1000.0, (b - 0.5) / (bins - 1)), f1 = 20.0 * std::pow (1000.0, (b + 0.5) / (bins - 1));
                int i0 = (int) std::floor (f0 / fs * len), i1 = (int) std::ceil (f1 / fs * len);
                i0 = std::clamp (i0, 1, len / 2 - 1); i1 = std::clamp (i1, i0 + 1, len / 2);
                float m = 0.f;
                for (int i = i0; i < i1; ++i) m = std::max (m, work[(size_t) i]);
                outDb[(size_t) b] = 20.f * std::log10 (std::max (1.0e-7f, m / (float) (len / 4)));
            }
        }
    private:
        juce::dsp::FFT fft { order };
        std::vector<float> work;
    };
}
