#pragma once
// =====================================================================
//  4GE - a neutral stereo FDN reverb (8 damped lines, Householder mix, pre-delay) for any engine that needs its own
//  (ARCHITECTURE.md rule 0: engines never share sound, they may share a helper). DSP only: buffers in prepare(), no allocation.
// =====================================================================
#include <vector>
#include <cmath>
#include <algorithm>

namespace fge::dsp::fdn
{
    class Fdn
    {
    public:
        static constexpr int NL = 8;
        void prepare (double rate, double maxSize = 2.0, double maxPreMs = 120.0)
        {
            fs = rate;
            for (int k = 0; k < NL; ++k) line[k].assign ((size_t) (baseD[k] * maxSize * fs / 44100.0) + 8, 0.f);
            pre.assign ((size_t) (maxPreMs * 0.001 * fs) + 8, 0.f);
            reset();
        }
        void reset()
        {
            for (auto& l : line) std::fill (l.begin(), l.end(), 0.f);
            std::fill (pre.begin(), pre.end(), 0.f);
            for (int k = 0; k < NL; ++k) { idx[k] = 0; damp[k] = 0.f; }
            preIdx = 0;
        }
        // per block
        void set (float size, float decayMs, float dampHz, float preMs)
        {
            for (int k = 0; k < NL; ++k)
            {
                dLen[k] = std::clamp ((int) (baseD[k] * size * fs / 44100.0), 1, (int) line[k].size() - 1);
                gk[k] = (float) std::pow (10.0, -3.0 * dLen[k] / (fs * std::max (0.05f, decayMs * 0.001f)));
            }
            dA = (float) std::exp (-6.283185307 * dampHz / fs);
            preLen = std::clamp ((int) (preMs * 0.001 * fs), 0, (int) pre.size() - 1);
        }
        // per sample: mono in, stereo out (~unity-ish level)
        void tick (float x, float& l, float& r)
        {
            pre[(size_t) preIdx] = x;
            int rp = preIdx - preLen; if (rp < 0) rp += (int) pre.size();
            const float xin = pre[(size_t) rp];
            if (++preIdx >= (int) pre.size()) preIdx = 0;
            float o[NL], sum = 0.f, rl = 0.f, rr = 0.f;
            for (int k = 0; k < NL; ++k)
            {
                int j = idx[k] - dLen[k]; if (j < 0) j += (int) line[k].size();
                damp[k] = (1.f - dA) * line[k][(size_t) j] + dA * damp[k];
                o[k] = damp[k] * gk[k]; sum += o[k];
                if (k & 1) rr += damp[k]; else rl += damp[k];
            }
            const float hh = sum * (2.f / NL);
            for (int k = 0; k < NL; ++k) { line[k][(size_t) idx[k]] = o[k] - hh + xin; if (++idx[k] >= (int) line[k].size()) idx[k] = 0; }
            l = rl * 0.25f; r = rr * 0.25f;
        }
    private:
        static constexpr double baseD[NL] { 1117, 1301, 1493, 1607, 1777, 1913, 2081, 2267 };
        double fs = 48000.0;
        std::vector<float> line[NL], pre;
        int idx[NL] {}, dLen[NL] {}, preIdx = 0, preLen = 0;
        float gk[NL] {}, damp[NL] {}, dA = 0.f;
    };
}
