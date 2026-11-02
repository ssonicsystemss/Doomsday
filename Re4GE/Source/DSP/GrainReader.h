#pragma once
// RE4GE - KEEP LENGTH pitch for the SAMPLE source (PITCH PROCESSED): WSOLA grains, after Ancient Drums' KickEngine::grainSample.
// The read position moves through the sample at normal speed (TIME, forwards or backwards); every grain plays at the new pitch.
// So a kick tail played on a higher note keeps its length (varispeed would also make it shorter / faster).
// Each new grain starts where the waveform continues the grain that is playing (a short correlation search) - no phasing.
// Real-time safe: fixed state, no allocation. Grain: 2 x 21 ms, half overlap, sin^2 windows.
#include <array>
#include <cmath>
#include <juce_core/juce_core.h>

namespace re
{
    class GrainReader
    {
    public:
        void reset() { age = 0; for (auto& g : grains) g = {}; }
        // d / len = the sample; pos = where the read is in TIME (source samples); step = source samples per output sample at the
        // wanted pitch (signed: < 0 = backwards);
        // align = do the correlation search (the main head); otherwise the new grain starts at alignedStart (from the main head)
        inline float next (const float* d, int len, double pos, double step, double outRate, double srcRate, bool align, double& alignedStart)
        {
            const int G = juce::jmax (64, 2 * (int) (0.021 * outRate)), hop = G / 2;
            if (age % hop == 0)
            {
                const int k = age / hop;
                auto& gnew = grains[(size_t) (k & 1)];
                const auto& gold = grains[(size_t) ((k + 1) & 1)];
                double start = pos;
                if (align) { if (k > 0 && gold.active) start = alignTo (d, len, gold.src + gold.age * step, pos, step, outRate, srcRate); alignedStart = start; }
                else if (k > 0) start = alignedStart;                                   // the unison heads follow the main head's choice
                gnew = { start, 0, true, k == 0 };
            }
            ++age;
            float y = 0.f;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                float w;
                if (g.first && g.age < hop) w = 1.f;                                   // the very first grain is not faded in
                else { const float sn = std::sin (juce::MathConstants<float>::pi * (float) g.age / (float) G); w = sn * sn; }
                y += w * read (d, len, g.src + g.age * step);
                if (++g.age >= G) g.active = false;
            }
            return y;
        }
    private:
        struct Grain { double src = 0.0; int age = 0; bool active = false, first = false; };
        std::array<Grain, 2> grains {};
        int age = 0;
        static inline float read (const float* d, int len, double x)
        {
            if (x < 0.0 || x >= len - 1) return 0.f;
            const int i = (int) x; const float f = (float) (x - i);
            return d[i] + f * (d[i + 1] - d[i]);
        }
        static inline float nearest (const float* d, int len, double x) { const int i = (int) (x + 0.5); return i >= 0 && i < len ? d[i] : 0.f; }
        // the start near 'target' that continues the waveform the old grain plays at 'prev' (~16 ms compared, +-12.5 ms searched)
        static double alignTo (const float* d, int len, double prev, double target, double step, double outRate, double srcRate)
        {
            constexpr int M = 64;
            const double stride = 0.016 * outRate / M * step;                // 16 ms of playback, in source samples (signed)
            std::array<float, M> ref {};
            float refE = 0.f;
            for (int j = 0; j < M; ++j) { ref[(size_t) j] = nearest (d, len, prev + j * stride); refE += ref[(size_t) j] * ref[(size_t) j]; }
            if (refE < 1e-7f) return target;
            const double range = 0.0125 * srcRate, dStep = juce::jmax (1.0, range / 300.0);
            double best = target; float bestScore = -1e9f;
            for (double dd = -range; dd <= range; dd += dStep)
            {
                float xy = 0.f, yy = 1e-9f;
                for (int j = 0; j < M; ++j) { const float y = nearest (d, len, target + dd + j * stride); xy += ref[(size_t) j] * y; yy += y * y; }
                const float score = xy / std::sqrt (yy * refE) - 0.08f * (float) (std::abs (dd) / range);
                if (score > bestScore) { bestScore = score; best = target + dd; }
            }
            return best;
        }
    };
}
