#pragma once
// =====================================================================
//  4GE - PRE-DIST ROOM (owner 2026-11-12, "early kick"): a small room on the TAIL bus BEFORE its distortion.
//  The distortion then eats the reverb and turns it into the full, "gated" early-hardstyle tail (909 + room + drive).
//  A 4-line feedback delay network (Householder mix), damping in the loop, stereo in / out.
//  DSP only: buffers made in prepare(), process() never allocates. Cleared at every new kick (a kick is one hit).
// =====================================================================
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace fge::dsp::room
{
    struct Settings
    {
        bool on = false;
        float mix = 0.f;          // 0..1 wet (dry stays at 1: the room adds to the tail)
        float t60Ms = 450.f;      // decay
        float tone = 5000.f;      // damping (Hz)
        float delayMs = 0.f;      // pre-delay
        bool onTok = false;       // TOK + TAIL: the tok goes through the room too
    };

    class Room
    {
    public:
        void prepare (double rate)
        {
            fs = rate;
            const int maxLen = (int) (rate * 0.065) + 8;              // the longest line (43.7 ms x 1.4) + margin
            for (auto& d : line) d.assign ((size_t) maxLen, 0.f);
            for (auto& d : pre) d.assign ((size_t) (rate * 0.31) + 8, 0.f);
            // input diffusion: 4 short allpasses per channel (owner 2026-11-13: "the reverb sounds odd" - the bare FDN rang metallic)
            static constexpr float apMs[4] { 4.7f, 3.6f, 12.7f, 9.3f };
            for (int c = 0; c < 2; ++c) for (int k = 0; k < 4; ++k)
            { const int n = (int) (apMs[k] * (c ? 1.07f : 1.f) * 0.001 * rate) + 1; ap[(size_t) c][(size_t) k].assign ((size_t) n, 0.f); apPos[(size_t) c][(size_t) k] = 0; }
            clear();
            cooked = -1.f;
        }
        void clear()
        {
            for (auto& d : line) std::fill (d.begin(), d.end(), 0.f);
            for (auto& d : pre) std::fill (d.begin(), d.end(), 0.f);
            for (auto& ch : ap) for (auto& d : ch) std::fill (d.begin(), d.end(), 0.f);
            lp.fill (0.f); pos = 0; prePos = 0;
        }
        // (per block) delay lengths / feedback / damping from the settings
        void cook (const Settings& s)
        {
            const float key = s.t60Ms * 7.f + s.tone + s.delayMs * 131.f;
            if (key == cooked || line[0].empty()) return;
            cooked = key;
            // the room grows a little with the decay (a small room ~ 0.85, a hall ~ 1.35)
            const float sz = std::clamp (0.75f + s.t60Ms / 2500.f, 0.8f, 1.4f);
            static constexpr float baseMs[4] { 29.7f, 37.1f, 41.1f, 43.7f };
            for (int k = 0; k < 4; ++k)
            {
                len[(size_t) k] = std::clamp ((int) (baseMs[k] * sz * 0.001 * fs), 8, (int) line[(size_t) k].size() - 1);
                const float dSec = (float) len[(size_t) k] / (float) fs;
                g[(size_t) k] = std::pow (10.f, -3.f * dSec / std::max (0.02f, s.t60Ms * 0.001f));
            }
            damp = std::exp (-6.2831853f * std::clamp (s.tone, 500.f, 20000.f) / (float) fs);
            preLen = pre[0].empty() ? 0 : std::clamp ((int) (s.delayMs * 0.001 * fs), 0, (int) pre[0].size() - 1);
        }
        void process (const Settings& s, float& l, float& r, float gate = 1.f)       // gate: 1 -> 0 at the kick's end (the room never makes the kick longer)
        {
            const int n = (int) line[0].size();
            std::array<float, 4> o;
            for (int k = 0; k < 4; ++k)
            {
                int rp = pos - len[(size_t) k]; if (rp < 0) rp += n;
                float y = line[(size_t) k][(size_t) rp];
                lp[(size_t) k] = y + (lp[(size_t) k] - y) * damp;               // damping in the loop
                o[(size_t) k] = lp[(size_t) k];
            }
            const float sum = 0.5f * (o[0] + o[1] + o[2] + o[3]);              // Householder: x - 2/N * sum
            float il = l * gate, ir = r * gate;
            if (preLen > 0)                                                         // pre-delay
            {
                const int pn = (int) pre[0].size();
                int rp = prePos - preLen; if (rp < 0) rp += pn;
                pre[0][(size_t) prePos] = il; pre[1][(size_t) prePos] = ir;
                il = pre[0][(size_t) rp]; ir = pre[1][(size_t) rp];
                if (++prePos >= pn) prePos = 0;
            }
            il = diffuse (0, il); ir = diffuse (1, ir);
            const float in[4] { il, ir, il, ir };
            for (int k = 0; k < 4; ++k)
                line[(size_t) k][(size_t) pos] = (o[(size_t) k] - sum) * g[(size_t) k] + in[k] * 0.5f;
            if (++pos >= n) pos = 0;
            const float wl = (o[0] + o[2]) * 0.5f, wr = (o[1] + o[3]) * 0.5f;
            l += wl * s.mix * gate; r += wr * s.mix * gate;
        }
    private:
        float diffuse (int c, float x)                                  // 4 series allpasses (g 0.65)
        {
            for (int k = 0; k < 4; ++k)
            {
                auto& d = ap[(size_t) c][(size_t) k]; if (d.empty()) continue;
                int& p = apPos[(size_t) c][(size_t) k];
                const float z = d[(size_t) p], v = x + 0.65f * z;
                d[(size_t) p] = v; x = z - 0.65f * v;
                if (++p >= (int) d.size()) p = 0;
            }
            return x;
        }
        std::array<std::array<std::vector<float>, 4>, 2> ap;
        std::array<std::array<int, 4>, 2> apPos {};
        double fs = 48000.0;
        std::array<std::vector<float>, 4> line;
        std::array<std::vector<float>, 2> pre;
        int prePos = 0, preLen = 0;
        std::array<int, 4> len { 100, 120, 140, 160 };
        std::array<float, 4> g {}, lp {};
        float damp = 0.f, cooked = -1.f;
        int pos = 0;
    };
}
