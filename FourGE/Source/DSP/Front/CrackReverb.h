#pragma once
// =====================================================================
//  4GE - CRACK REVERB ENGINE (owner 2026-11-21, RAW_CRACK.md "CRACK REVERB"): a SHORT ROOM / EARLY REFLECTION reverb ONLY for the
//  TRANSIENT CRACK (never the tail / master reverb). Mono (the crack lives on the mono RAW attack bus).
//    IN -> PRE-DELAY -> EARLY REFLECTIONS (8 taps spread by SIZE)  ---------------------------> + -> OUT (raw reverb)
//                    -> LATE: 4 damped feedback combs (DECAY = RT60, DAMPING) -> 2 allpasses (DIFFUSION) -^
//  then (outside): REVERB FILTER (HP / LP / RES) -> REVERB DRIVE (SOFT / HARD / ASYM / FOLD) -> CRACK MIX.
//  Buffers are made in prepare() (never on the audio thread); clear() per kick.
// =====================================================================
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>

namespace fge::dsp::front
{
    struct CrackReverbSettings
    {
        float size = 0.4f, decayMs = 120.f, preDelayMs = 2.f, early = 0.7f, late = 0.5f, diffusion = 0.5f, damping = 0.4f;
    };

    class CrackReverb
    {
    public:
        void prepare (double rate)
        {
            fs = rate;
            buf.assign ((size_t) (fs * 0.075) + 8, 0.f);                                   // pre-delay (30 ms) + early taps (up to ~40 ms)
            for (auto& c : comb) c.d.assign ((size_t) (fs * 0.045) + 8, 0.f);
            for (auto& a : ap) a.d.assign ((size_t) (fs * 0.012) + 8, 0.f);
            clear();
        }
        void clear()
        {
            std::fill (buf.begin(), buf.end(), 0.f); w = 0;
            for (auto& c : comb) { std::fill (c.d.begin(), c.d.end(), 0.f); c.w = 0; c.lp = 0.f; }
            for (auto& a : ap) { std::fill (a.d.begin(), a.d.end(), 0.f); a.w = 0; }
        }
        // the coefficients (per block): SIZE spreads the reflections + the comb lengths, DECAY = RT60 of the combs
        void cook (const CrackReverbSettings& s)
        {
            if (buf.empty()) return;
            const float sz = 0.35f + 1.3f * std::clamp (s.size, 0.f, 1.f);
            pre = std::clamp ((int) (s.preDelayMs * 0.001 * fs), 0, (int) (fs * 0.031));
            static constexpr float erMs[8] { 1.3f, 2.9f, 4.1f, 6.2f, 8.3f, 11.1f, 14.7f, 19.3f };
            static constexpr float erG[8]  { 0.95f, -0.82f, 0.74f, -0.66f, 0.55f, -0.47f, 0.38f, -0.3f };
            for (int k = 0; k < 8; ++k) { erTap[(size_t) k] = std::min ((int) buf.size() - 2, pre + (int) (erMs[k] * sz * 0.001 * fs)); erGain[(size_t) k] = erG[k]; }
            static constexpr float cMs[4] { 7.31f, 9.13f, 11.71f, 13.93f };
            const float rt = std::max (10.f, s.decayMs);
            for (int k = 0; k < 4; ++k)
            {
                auto& c = comb[(size_t) k];
                const float L = cMs[k] * sz;
                c.len = std::clamp ((int) (L * 0.001 * fs), 8, (int) c.d.size() - 2);
                c.fb = std::pow (10.f, -3.f * L / rt);                                       // -60 dB after DECAY
                c.damp = 0.05f + 0.85f * std::clamp (s.damping, 0.f, 1.f);
            }
            static constexpr float aMs[2] { 2.13f, 5.37f };
            for (int k = 0; k < 2; ++k) ap[(size_t) k].len = std::clamp ((int) (aMs[k] * sz * 0.001 * fs), 4, (int) ap[(size_t) k].d.size() - 2);
            apG = 0.15f + 0.6f * std::clamp (s.diffusion, 0.f, 1.f);
            eGain = s.early; lGain = s.late;
        }
        float process (float x)
        {
            if (buf.empty()) return 0.f;
            const int n = (int) buf.size();
            buf[(size_t) w] = x;
            auto at = [&] (int d) { int i = w - d; if (i < 0) i += n; return buf[(size_t) i]; };
            float er = 0.f;
            for (int k = 0; k < 8; ++k) er += erGain[(size_t) k] * at (erTap[(size_t) k]);
            const float in = at (pre);
            if (++w >= n) w = 0;
            float lt = 0.f;
            for (auto& c : comb)
            {
                int r = c.w - c.len; if (r < 0) r += (int) c.d.size();
                const float y = c.d[(size_t) r];
                c.lp += (y - c.lp) * (1.f - c.damp);                                         // DAMPING: the highs die faster
                c.d[(size_t) c.w] = in + c.lp * c.fb;
                if (++c.w >= (int) c.d.size()) c.w = 0;
                lt += y;
            }
            lt *= 0.35f;
            for (auto& a : ap)                                                               // DIFFUSION: denser reflections
            {
                int r = a.w - a.len; if (r < 0) r += (int) a.d.size();
                const float d = a.d[(size_t) r];
                const float v = lt + apG * d;
                a.d[(size_t) a.w] = v;
                if (++a.w >= (int) a.d.size()) a.w = 0;
                lt = d - apG * v;
            }
            return eGain * 0.45f * er + lGain * lt;
        }

    private:
        struct Comb { std::vector<float> d; int w = 0, len = 100; float fb = 0.5f, damp = 0.3f, lp = 0.f; };
        struct Ap { std::vector<float> d; int w = 0, len = 50; };
        double fs = 48000.0;
        std::vector<float> buf;
        int w = 0, pre = 0;
        std::array<int, 8> erTap {};
        std::array<float, 8> erGain {};
        std::array<Comb, 4> comb;
        std::array<Ap, 2> ap;
        float apG = 0.5f, eGain = 0.7f, lGain = 0.5f;
    };
}
