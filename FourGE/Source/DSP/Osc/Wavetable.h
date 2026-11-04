#pragma once
// =====================================================================
//  4GE module 03 - band-limited single-cycle tables: WAVETABLE (several frames), HARMONIC ENGINE (additive profile),
//  CUSTOM (drawn / imported cycle). Every table is built OFF the audio thread (message thread / offline render):
//  each frame -> FFT -> one copy per octave (mipmap) with the harmonics above that octave's Nyquist removed, DC removed,
//  one normalisation for the whole table. The audio thread only reads (linear interpolation in the cycle and between frames,
//  the mip level from the phase increment), so FM / pitch drops never alias from the table.
// =====================================================================
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include <vector>

namespace fge::dsp
{
    struct Table
    {
        static constexpr int N = 1024, Mips = 10, order = 10;     // 1024-sample cycle; mip m keeps harmonics <= 512 >> m
        int frames = 1;
        std::vector<float> data;                                    // frames x Mips x (N + 1) (one guard sample for interpolation)
        const float* frame (int f, int m) const { return data.data() + ((size_t) f * Mips + (size_t) m) * (N + 1); }

        // mip level for a phase increment (cycles per sample): the highest kept harmonic stays under ~0.45 x sample rate
        static int mipFor (float dt)
        {
            const float maxH = 0.45f / juce::jmax (1.0e-7f, std::abs (dt));
            int m = 0;
            while (m < Mips - 1 && (float) ((N / 2) >> m) > maxH) ++m;
            return m;
        }
        // value at phase 0..1 and table position 0..1 (frames), morph shapes the crossfade between neighbouring frames
        inline float read (float ph, float pos01, float morph, int mip) const
        {
            const float fp = juce::jlimit (0.f, 1.f, pos01) * (float) (frames - 1);
            const int f0 = juce::jmin ((int) fp, frames - 1), f1 = juce::jmin (f0 + 1, frames - 1);
            float fr = fp - (float) f0;
            if (morph > 0.f)                                        // MORPH: 0 = smooth crossfade ... 100 % = jumps frame to frame
            {
                const float w = 1.f - 0.98f * juce::jlimit (0.f, 1.f, morph);
                fr = juce::jlimit (0.f, 1.f, (fr - 0.5f) / w + 0.5f);
            }
            const float x = ph * (float) N;
            const int i = juce::jlimit (0, N - 1, (int) x);
            const float u = x - (float) i;
            const float* a = frame (f0, mip);
            const float va = a[i] + (a[i + 1] - a[i]) * u;
            if (f1 == f0 || fr <= 0.f) return va;
            const float* b = frame (f1, mip);
            const float vb = b[i] + (b[i + 1] - b[i]) * u;
            return va + (vb - va) * fr;
        }
    };
    using TablePtr = std::shared_ptr<const Table>;

    // ---- building (never on the audio thread)
    namespace tables
    {
        // cycles: each N samples (one period). Returns a mipmapped, DC-free, normalised table.
        inline TablePtr fromCycles (const std::vector<std::vector<float>>& cycles)
        {
            auto t = std::make_shared<Table>();
            t->frames = juce::jmax (1, (int) cycles.size());
            t->data.assign ((size_t) t->frames * Table::Mips * (Table::N + 1), 0.f);
            juce::dsp::FFT fft (Table::order);
            std::vector<float> buf ((size_t) Table::N * 2), spec ((size_t) Table::N * 2);
            float peak = 0.f;
            for (int f = 0; f < t->frames; ++f)
            {
                std::fill (spec.begin(), spec.end(), 0.f);
                const auto& c = cycles.empty() ? std::vector<float> ((size_t) Table::N, 0.f) : cycles[(size_t) f];
                for (int i = 0; i < Table::N; ++i) spec[(size_t) i] = i < (int) c.size() && std::isfinite (c[(size_t) i]) ? c[(size_t) i] : 0.f;
                fft.performRealOnlyForwardTransform (spec.data());       // interleaved re / im, bins 0..N/2
                spec[0] = spec[1] = 0.f;                                  // no DC
                for (int m = 0; m < Table::Mips; ++m)
                {
                    const int keep = (Table::N / 2) >> m;
                    std::copy (spec.begin(), spec.end(), buf.begin());
                    for (int k = 1; k < Table::N; ++k)                    // bin k and its mirror N - k are the same harmonic
                    {
                        const int h = k <= Table::N / 2 ? k : Table::N - k;
                        if (h > keep) { buf[(size_t) (2 * k)] = 0.f; buf[(size_t) (2 * k + 1)] = 0.f; }
                    }
                    fft.performRealOnlyInverseTransform (buf.data());
                    float* d = t->data.data() + ((size_t) f * Table::Mips + (size_t) m) * (Table::N + 1);
                    for (int i = 0; i < Table::N; ++i) d[i] = buf[(size_t) i];
                    d[Table::N] = d[0];
                    if (m == 0) for (int i = 0; i < Table::N; ++i) peak = juce::jmax (peak, std::abs (d[i]));
                }
            }
            if (peak > 1.0e-9f) for (auto& v : t->data) v /= peak;
            return t;
        }
        inline std::vector<float> cycleOf (const std::function<float (float)>& fn)
        {
            std::vector<float> c ((size_t) Table::N);
            for (int i = 0; i < Table::N; ++i) c[(size_t) i] = fn ((float) i / (float) Table::N);
            return c;
        }
        // additive cycle from harmonic amplitudes / phases (index 0 = fundamental)
        inline std::vector<float> additive (const float* amp, const float* phase, int count)
        {
            std::vector<float> c ((size_t) Table::N, 0.f);
            const float tp = juce::MathConstants<float>::twoPi;
            for (int h = 0; h < count && h < Table::N / 2; ++h)
            {
                if (std::abs (amp[h]) < 1.0e-6f) continue;
                for (int i = 0; i < Table::N; ++i) c[(size_t) i] += amp[h] * std::sin (tp * (float) (h + 1) * (float) i / (float) Table::N + phase[h]);
            }
            return c;
        }

        // built-in wavetables (each a real morph across frames)
        enum Builtin { Basic = 0, Hardstyle, Formant, Metal, PwmSweep, NumBuiltin };
        inline TablePtr builtin (int which)
        {
            std::vector<std::vector<float>> fr;
            const float tp = juce::MathConstants<float>::twoPi;
            switch (which)
            {
                case Hardstyle:               // a sine driven harder and harder (the classic distorted kick tail), asymmetric towards the end
                    for (int k = 0; k < 8; ++k)
                    {
                        const float g = std::pow (1.6f, (float) k * 1.2f), off = 0.05f * (float) k;
                        fr.push_back (cycleOf ([=] (float x) { return std::tanh (g * std::sin (tp * x) + off) - std::tanh (off); }));
                    }
                    break;
                case Formant:                 // a saw through moving vowel peaks (A -> E -> I -> O -> U ...)
                {
                    static const float f1[8] { 3.f, 2.f, 1.f, 2.5f, 1.5f, 4.f, 6.f, 9.f }, f2[8] { 7.f, 12.f, 16.f, 6.f, 5.f, 11.f, 15.f, 22.f };
                    for (int k = 0; k < 8; ++k)
                    {
                        std::vector<float> a (64), p (64, 0.f);
                        for (int h = 0; h < 64; ++h)
                        {
                            const float n = (float) (h + 1);
                            a[(size_t) h] = (1.f / n) * (0.25f + std::exp (-0.5f * std::pow ((n - f1[k]) / 1.2f, 2.f)) * 3.f
                                                               + std::exp (-0.5f * std::pow ((n - f2[k]) / 2.f, 2.f)) * 2.f);
                        }
                        fr.push_back (additive (a.data(), p.data(), 64));
                    }
                    break;
                }
                case Metal:                   // sparse, bell-like harmonic sets getting brighter
                {
                    static const int sets[8][6] { { 1, 3, 7, 0, 0, 0 }, { 1, 4, 9, 13, 0, 0 }, { 1, 5, 11, 17, 0, 0 }, { 1, 6, 13, 19, 27, 0 },
                                                  { 1, 7, 15, 23, 31, 0 }, { 1, 9, 17, 25, 33, 41 }, { 1, 11, 21, 31, 43, 53 }, { 1, 13, 25, 37, 49, 61 } };
                    for (int k = 0; k < 8; ++k)
                    {
                        std::vector<float> a (64, 0.f), p (64, 0.f);
                        for (int j = 0; j < 6; ++j) if (sets[k][j] > 0) { a[(size_t) sets[k][j] - 1] = 1.f / (1.f + 0.15f * (float) j); p[(size_t) sets[k][j] - 1] = (float) j; }
                        fr.push_back (additive (a.data(), p.data(), 64));
                    }
                    break;
                }
                case PwmSweep:                // pulse width 50 % -> 5 %
                    for (int k = 0; k < 8; ++k)
                    {
                        const float w = 0.5f - 0.45f * (float) k / 7.f;
                        fr.push_back (cycleOf ([=] (float x) { return (x < w ? 1.f : -1.f) - (2.f * w - 1.f); }));
                    }
                    break;
                default:                      // BASIC: sine -> triangle -> saw -> square
                    fr.push_back (cycleOf ([=] (float x) { return std::sin (tp * x); }));
                    fr.push_back (cycleOf ([] (float x) { return 1.f - 4.f * std::abs (x - 0.5f); }));
                    fr.push_back (cycleOf ([] (float x) { return 2.f * (x < 0.5f ? x + 0.5f : x - 0.5f) - 1.f; }));
                    fr.push_back (cycleOf ([] (float x) { return x < 0.5f ? 1.f : -1.f; }));
                    break;
            }
            return fromCycles (fr);
        }
        // a WAV turned into wavetable frames: cut into equal cycles of 'cycleLen' samples (or the whole file = 1 cycle),
        // each resampled to N. The import is a cycle SOURCE, never played as a kick sample.
        inline std::vector<std::vector<float>> framesFromAudio (const float* x, int n, int cycleLen, int maxFrames = 64)
        {
            std::vector<std::vector<float>> out;
            if (n < 8) return out;
            if (cycleLen <= 0 || cycleLen > n) cycleLen = n;
            const int count = juce::jlimit (1, maxFrames, n / cycleLen);
            const int step = juce::jmax (1, (n / cycleLen) / count);
            for (int k = 0; k < count; ++k)
            {
                const int start = k * step * cycleLen;
                if (start + cycleLen > n) break;
                std::vector<float> c ((size_t) Table::N);
                for (int i = 0; i < Table::N; ++i)
                {
                    const float pos = (float) i * (float) cycleLen / (float) Table::N;
                    const int j = (int) pos; const float u = pos - (float) j;
                    const float a = x[start + j], b = x[start + juce::jmin (cycleLen - 1, j + 1)];
                    c[(size_t) i] = a + (b - a) * u;
                }
                out.push_back (std::move (c));
            }
            return out;
        }
    }
}
