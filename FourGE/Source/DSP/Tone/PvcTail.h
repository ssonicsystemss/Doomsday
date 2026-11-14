#pragma once
// =====================================================================
//  4GE - TAIL ENGINE PVC (owner 2026-11-14, his spec + his 2 PVC samples): the KLOENK tail.
//  SOURCE (KICK BODY / SINE / SAW / HYBRID, + pitch START -> DROP -> ROOT -> HOLD / SLIDE, + RESONANCE)
//  -> REVERB (the MATERIAL of the tail, not an effect at the end) -> HIGH CUT -> PRE-COMP -> DRIVE 1 -> CLIP
//  -> GATE (SINGLE curve / PULSE / MOVEMENT) -> DRIVE 2 (+ asymmetry) -> moving FILTER -> SPECTRUM (low / low mid / mid / high)
//  -> MONO LOW / STEREO HIGH (micro delay width growing over the tail + the reverb's sides) -> FINAL SHAPE (attack / length / fade).
//  DSP only: buffers made in prepare(), coefficients per 32 samples, no allocation / locks. reset() at every new kick.
// =====================================================================
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace fge::dsp::pvctail
{
    struct Settings
    {
        bool on = false;
        int source = 0;                                        // 0 KICK BODY, 1 SINE, 2 SAW, 3 HYBRID
        float startSt = 7.f, holdMs = 0.f, dropMs = 30.f, rootSt = 0.f, slideSt = 0.f;
        float resHz = 300.f, resQ = 4.f, resAmt = 0.f;
        float revSize = 1.f, revDecayMs = 1200.f, preMs = 0.f, highCut = 450.f, wet = 1.f;
        float comp = 0.5f, drive1Db = 18.f, clip = 0.7f, drive2Db = 16.f, asym = 0.2f;
        int gateMode = 0, curve = 0, div = 1;                 // SINGLE / PULSE / MOVEMENT; HARD / SOFT / EXP / STEPS; 1/8 1/16 1/32
        float gateMs = 380.f, depth = 0.9f, movement = 0.5f, bpm = 150.f;
        float filterHz = 2000.f, reso = 2.f, moveHz = 6.f, moveDepth = 0.3f;
        float low = 0.f, lowMid = 0.f, mid = 0.f, high = 0.f;
        float monoBelow = 150.f, width = 0.8f, side = 0.5f;
        float delayMs = 170.f, attackMs = 6.f, lengthMs = 330.f, fadeMs = 5.f;
    };

    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float tick (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void clear() { z1 = z2 = 0.f; }
        void set (double B0, double B1, double B2, double A0, double A1, double A2)
        { b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0); }
        void lp (double fs, double f, double q)
        { const double w = 6.283185307 * std::min (f, fs * 0.45) / fs, al = std::sin (w) / (2 * q), c = std::cos (w); set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
        void hp (double fs, double f, double q)
        { const double w = 6.283185307 * std::min (f, fs * 0.45) / fs, al = std::sin (w) / (2 * q), c = std::cos (w); set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
        void bp (double fs, double f, double q)      // 0 dB peak
        { const double w = 6.283185307 * std::min (f, fs * 0.45) / fs, al = std::sin (w) / (2 * q), c = std::cos (w); set (al, 0, -al, 1 + al, -2 * c, 1 - al); }
        void bell (double fs, double f, double db, double q)
        { const double A = std::pow (10.0, db / 40.0), w = 6.283185307 * std::min (f, fs * 0.45) / fs, al = std::sin (w) / (2 * q), c = std::cos (w);
          set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); }
        void shelf (double fs, double f, double db, bool highShelf)
        {
            const double A = std::pow (10.0, db / 40.0), w = 6.283185307 * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / 2 * std::sqrt (2.0), sa = 2 * std::sqrt (A) * al;
            if (highShelf) set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa), (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
            else           set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa), (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
        }
    };

    class Unit
    {
    public:
        static constexpr int NL = 8;
        void prepare (double rate)
        {
            fs = rate;
            for (int k = 0; k < NL; ++k) line[k].assign ((size_t) (baseD[k] * 2.0 * fs / 44100.0) + 8, 0.f);
            pre.assign ((size_t) (0.105 * fs) + 8, 0.f);
            micro.assign ((size_t) (0.003 * fs) + 8, 0.f);
            reset();
        }
        void reset()
        {
            for (auto& l : line) std::fill (l.begin(), l.end(), 0.f);
            std::fill (pre.begin(), pre.end(), 0.f); std::fill (micro.begin(), micro.end(), 0.f);
            for (int k = 0; k < NL; ++k) { idx[k] = 0; damp[k] = 0.f; }
            preIdx = 0; microIdx = 0;
            for (auto* b : { &res, &cut1, &cut2, &mv, &eqL, &eqLM, &eqM, &eqH, &xl1, &xl2, &xh1, &xh2, &sh1 }) b->clear();
            env = 0.f; gs = 0.f; ph = 0.0; lfo = 0.0; blk = 0;
        }
        // body = the raw BODY bus of this sample; ms = since the hit; noteHz = the played note (x fine)
        void process (const Settings& s, float body, float ms, float noteHz, float& outL, float& outR)
        {
            if (blk-- <= 0) { blk = 32; cook (s); }
            const float t = ms - s.delayMs;                                      // time in the tail
            // ---- SOURCE
            float src = 0.f;
            if (s.source != 0)
            {
                const float to = ms - (s.delayMs - 20.f);                        // the oscillator starts 20 ms early: the reverb has material at the start
                if (to >= 0.f)
                {
                    float st;
                    if (to < s.holdMs) st = s.startSt;
                    else { const float u = std::min (1.f, (to - s.holdMs) / std::max (1.f, s.dropMs)); st = s.startSt * (1.f - u) * (1.f - u); }
                    st += s.slideSt * std::clamp ((to - s.holdMs - s.dropMs) / std::max (1.f, s.lengthMs), 0.f, 1.f);
                    const double hz = std::clamp ((double) noteHz * std::exp2 ((s.rootSt + st) / 12.0), 5.0, fs * 0.4);
                    const float osc = s.source == 2 ? (float) (2.0 * ph - 1.0) : (float) std::sin (6.283185307 * ph);
                    ph += hz / fs; if (ph >= 1.0) ph -= 1.0;
                    src = s.source == 3 ? 0.5f * osc + 0.5f * body : osc;
                }
                else if (s.source == 3) src = 0.5f * body;
            }
            else src = body;
            if (s.resAmt > 0.f) src += s.resAmt * 3.f * res.tick (src);
            // ---- REVERB (8-line FDN, Householder mix, damped lines; pre-delay)
            pre[(size_t) preIdx] = src;
            int rp = preIdx - preLen; if (rp < 0) rp += (int) pre.size();
            const float x = pre[(size_t) rp];
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
            for (int k = 0; k < NL; ++k)
            {
                line[k][(size_t) idx[k]] = o[k] - hh + x;
                if (++idx[k] >= (int) line[k].size()) idx[k] = 0;
            }
            rl *= 0.3f; rr *= 0.3f;
            float m = (1.f - s.wet) * src + s.wet * 0.5f * (rl + rr);
            const float sideRev = s.wet * 0.5f * (rl - rr);
            // ---- HIGH CUT (the bass of the reverb), PRE-COMP, DRIVE 1, CLIP
            m = cut2.tick (cut1.tick (m));
            const float a = std::abs (m), co = a > env ? atk : rel;
            env = co * env + (1.f - co) * a;
            m *= std::min (8.f, std::pow (std::max (env, 1.0e-3f) / 0.3f, -s.comp));
            m = std::tanh (g1 * m);
            if (s.clip > 0.f) m = std::clamp (m * (1.f + 3.f * s.clip), -1.f, 1.f);
            // ---- GATE (before DRIVE 2)
            float g = 0.f;
            if (t >= 0.f)
            {
                const float u = std::min (1.f, t / std::max (1.f, s.gateMs));
                float lv;
                switch (s.curve)
                {
                    case 1:  lv = 1.f - s.depth * 0.5f * (1.f - std::cos (3.14159265f * u)); break;
                    case 2:  lv = 1.f - s.depth * (1.f - std::exp (-4.f * u)) / (1.f - std::exp (-4.f)); break;
                    case 3:  lv = 1.f - s.depth * std::floor (u * 6.f) / 6.f; if (u >= 1.f) lv = 1.f - s.depth; break;
                    default: lv = u < 1.f ? 1.f : 1.f - s.depth; break;
                }
                if (s.gateMode != 0)
                {
                    static constexpr float divs[3] { 8.f, 16.f, 32.f };
                    const float per = 60000.f / std::max (30.f, s.bpm) / (divs[std::clamp (s.div, 0, 2)] / 4.f);
                    if (s.gateMode == 1) { if (std::fmod (t, per) / per >= 0.55f) lv *= 1.f - s.movement; }
                    else
                    {
                        uint32_t h = (uint32_t) (t / per) * 2654435761u ^ 0x9E3779B9u; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
                        lv *= 1.f - s.movement * (float) (h & 1023u) / 1023.f;
                    }
                }
                g = lv;
            }
            gs += (g - gs) * sm;
            m *= gs;
            // ---- DRIVE 2 (asymmetric), moving FILTER, SPECTRUM
            m = (m >= 0.f ? std::tanh (g2 * m) : std::tanh (g2 * (1.f - 0.8f * s.asym) * m)) * n2;
            lfo += 6.283185307 * s.moveHz / fs; if (lfo > 6.283185307) lfo -= 6.283185307;
            m = mv.tick (m);
            m = eqH.tick (eqM.tick (eqLM.tick (eqL.tick (m))));
            // ---- MONO LOW / STEREO HIGH
            const float lo = xl2.tick (xl1.tick (m)), hi = xh2.tick (xh1.tick (m));
            micro[(size_t) microIdx] = hi;
            int mp = microIdx - microLen; if (mp < 0) mp += (int) micro.size();
            const float hd = micro[(size_t) mp];
            if (++microIdx >= (int) micro.size()) microIdx = 0;
            const float open = 0.35f + 0.65f * std::clamp (t / std::max (1.f, s.lengthMs), 0.f, 1.f);   // the width opens up over the tail (the samples)
            const float sd = 0.5f * (hi - hd) * s.width * open + sh1.tick (sideRev) * s.side * 2.f;
            // ---- FINAL SHAPE
            float e = 0.f;
            if (t >= 0.f && t < s.lengthMs)
            {
                e = s.attackMs > 0.01f ? std::min (1.f, t / s.attackMs) : 1.f;
                const float left = s.lengthMs - t;
                if (left < s.fadeMs) e *= left / std::max (0.01f, s.fadeMs);
            }
            outL = (lo + hi + sd) * e;
            outR = (lo + hi - sd) * e;
        }
    private:
        void cook (const Settings& s)
        {
            for (int k = 0; k < NL; ++k)
            {
                dLen[k] = std::clamp ((int) (baseD[k] * s.revSize * fs / 44100.0), 1, (int) line[k].size() - 1);
                gk[k] = (float) std::pow (10.0, -3.0 * dLen[k] / (fs * std::max (0.05f, s.revDecayMs * 0.001f)));
            }
            dA = (float) std::exp (-6.283185307 * 5000.0 / fs);
            preLen = std::clamp ((int) (s.preMs * 0.001 * fs), 0, (int) pre.size() - 1);
            microLen = std::clamp ((int) (s.width * 0.0012 * fs), 0, (int) micro.size() - 1);
            res.bp (fs, s.resHz, s.resQ);
            cut1.lp (fs, s.highCut, 0.54); cut2.lp (fs, s.highCut, 1.31);
            g1 = std::pow (10.f, s.drive1Db / 20.f);
            g2 = std::pow (10.f, s.drive2Db / 20.f); n2 = 1.f / std::tanh (std::max (1.f, g2));
            mv.lp (fs, std::clamp (s.filterHz * std::exp2 (s.moveDepth * 2.f * (float) std::sin (lfo)), 40.f, (float) fs * 0.45f), std::max (0.5f, s.reso));
            eqL.shelf (fs, 100.0, s.low, false); eqLM.bell (fs, 300.0, s.lowMid, 0.8); eqM.bell (fs, 1000.0, s.mid, 0.8); eqH.shelf (fs, 3500.0, s.high, true);
            xl1.lp (fs, s.monoBelow, 0.7071); xl2.lp (fs, s.monoBelow, 0.7071); xh1.hp (fs, s.monoBelow, 0.7071); xh2.hp (fs, s.monoBelow, 0.7071);
            sh1.hp (fs, s.monoBelow, 0.7071);
            atk = std::exp (-1.f / (0.002f * (float) fs)); rel = std::exp (-1.f / (0.04f * (float) fs)); sm = 1.f - std::exp (-1.f / (0.001f * (float) fs));
        }
        static constexpr double baseD[NL] { 1117, 1301, 1493, 1607, 1777, 1913, 2081, 2267 };
        double fs = 48000.0, ph = 0.0, lfo = 0.0;
        std::vector<float> line[NL], pre, micro;
        int idx[NL] {}, dLen[NL] {}, preIdx = 0, preLen = 0, microIdx = 0, microLen = 0, blk = 0;
        float gk[NL] {}, damp[NL] {}, dA = 0.f;
        Biquad res, cut1, cut2, mv, eqL, eqLM, eqM, eqH, xl1, xl2, xh1, xh2, sh1;
        float env = 0.f, gs = 0.f, atk = 0.f, rel = 0.f, sm = 0.f, g1 = 1.f, g2 = 1.f, n2 = 1.f;
    };
}
