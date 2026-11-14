#pragma once
// =====================================================================
//  4GE - TAIL ENGINE ZAAG (owner 2026-11-14, from the OPS zaag kicks + his picks of the prototype rounds):
//  NO separate tok - ONE huge distorted bass from 0 ms: sine / saw at the note with a fast pitch drop (START st, DROP ms),
//  dirt (noise, different L / R = width) mixed BEFORE a huge DRIVE (square, full scale), the lows split off mono + a clean SUB,
//  the highs wide; on top the PIEP first (a short tonal pip) and THEN the TRASK (crunch: the same falling saw x3, hard,
//  700 Hz - 3.5 kHz); a REVERB on the PUNCH only (the first SEND ms, no lows; pre-delay, damping, a gate = gated reverb);
//  at the end PRESS (pressed to full scale). DSP only: buffers made in prepare(), coefficients per 32 samples, reset() per kick.
// =====================================================================
#include "PvcTail.h"          // (pvctail::Biquad)

namespace fge::dsp::zaag
{
    struct Settings
    {
        bool on = false;
        float startSt = 14.f, dropMs = 22.f, saw = 0.35f, driveDb = 28.f, dirt = 0.06f, dirtHz = 5000.f;
        float sub = 1.2f, press = 3.f, width = 0.6f, click = 0.5f;
        float piep = 0.6f, piepHz = 1400.f, piepMs = 15.f;
        float trask = 0.7f, traskDelay = 15.f, traskMs = 40.f;
        float verb = 2.f, verbDecayMs = 1600.f, verbSize = 1.2f, sendMs = 80.f, preMs = 0.f, gateMs = 0.f, dampHz = 4500.f, verbLowCut = 400.f;
        float lengthMs = 400.f;
    };

    class Unit
    {
    public:
        static constexpr int NL = 8;
        void prepare (double rate)
        {
            fs = rate;
            for (int k = 0; k < NL; ++k) line[k].assign ((size_t) (baseD[k] * 2.0 * fs / 44100.0) + 8, 0.f);
            pre.assign ((size_t) (0.065 * fs) + 8, 0.f);
            reset();
        }
        void reset()
        {
            for (auto& l : line) std::fill (l.begin(), l.end(), 0.f);
            std::fill (pre.begin(), pre.end(), 0.f);
            for (int k = 0; k < NL; ++k) { idx[k] = 0; damp[k] = 0.f; }
            preIdx = 0; ph = 0.0; php = 0.0; blk = 0; seed = 0x1234567u;
            for (auto* b : { &nL, &nR, &yL, &yR, &lo1, &lo2, &hiL1, &hiL2, &hiR1, &hiR2, &ck, &tb1, &tb2, &vh1, &vh2 }) b->clear();
        }
        void process (const Settings& s, float ms, float noteHz, float& outL, float& outR)
        {
            if (blk-- <= 0) { blk = 32; cook (s); }
            // ---- the bass: one oscillator from 0 ms, fast pitch drop to the note
            const double hz = std::clamp ((double) noteHz * (1.0 + kStart * std::exp (-ms / std::max (1.f, s.dropMs))), 5.0, fs * 0.4);
            const float sn = (float) std::sin (6.283185307 * ph), sw = (float) (2.0 * ph - 1.0);
            const double ph3 = std::fmod (ph * 3.0, 1.0);
            ph += hz / fs; if (ph >= 1.0) ph -= 1.0;
            const float x = (1.f - s.saw) * sn + s.saw * sw;
            const float dl = nL.tick (noise()) * nk, dr = nR.tick (noise()) * nk;           // the dirt, different L / R
            float l = yL.tick (std::tanh (gDrive * (x + s.dirt * dl))), r = yR.tick (std::tanh (gDrive * (x + s.dirt * dr)));
            const float lo = lo2.tick (lo1.tick (0.5f * (l + r)));
            const float hl = hiL2.tick (hiL1.tick (l)), hr = hiR2.tick (hiR1.tick (r));
            const float mid = 0.5f * (hl + hr), side = 0.8f * (hl - hr) * (1.f + 3.f * s.width);
            const float body = lo + s.sub * sn;
            float L = body + mid + side, R = body + mid - side;
            // ---- click, PIEP (first), TRASK (then)
            float top = s.click * std::exp (-ms / 2.f) * ck.tick (noise());
            if (s.piep > 0.f)
            {
                const float pf = s.piepHz * (0.5f + 0.5f * std::exp (-ms / std::max (1.f, s.piepMs * 0.6f)));
                const float e = ms < 1.f ? ms : ms < s.piepMs ? 1.f : std::max (0.f, 1.f - (ms - s.piepMs) / (0.6f * s.piepMs));
                top += s.piep * std::tanh (3.f * (float) std::sin (6.283185307 * php)) * e;
                php += pf / fs; if (php >= 1.0) php -= 1.0;
            }
            if (s.trask > 0.f)
            {
                const float t = ms - s.traskDelay;
                float e = 0.f;
                if (t >= 0.f) e = t < 2.f ? t * 0.5f : t < s.traskMs ? 1.f - 0.3f * (t - 2.f) / std::max (1.f, s.traskMs - 2.f) : std::max (0.f, 0.7f * (1.f - (t - s.traskMs) / (0.8f * s.traskMs)));
                if (e > 0.f) top += s.trask * 0.5f * tb2.tick (tb1.tick (std::tanh (40.f * ((float) (2.0 * ph3 - 1.0) + 0.3f * noise())))) * e;
            }
            top *= 2.5f;                                                         // (the bass sums to ~2.5: the prototype normalised it first)
            L += top; R += top;
            // ---- REVERB on the PUNCH (only the first SEND ms, no lows)
            if (s.verb > 0.f)
            {
                const float sendG = std::clamp ((s.sendMs - ms) / 10.f, 0.f, 1.f);
                pre[(size_t) preIdx] = vh2.tick (vh1.tick (0.5f * (L + R) * sendG));
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
                float gg = 1.f;
                if (s.gateMs > 0.f) gg = std::clamp ((s.gateMs + 8.f - ms) / 8.f, 0.f, 1.f);                  // GATED reverb
                L += s.verb * 0.25f * rl * gg; R += s.verb * 0.25f * rr * gg;
            }
            // ---- PRESS to full scale, the end of the kick
            const float pn = 1.f / std::tanh (s.press);
            float e = 1.f;
            if (ms > s.lengthMs - 5.f) e = std::max (0.f, (s.lengthMs - ms) / 5.f);
            outL = std::tanh (s.press * L * 0.4f) * pn * e;
            outR = std::tanh (s.press * R * 0.4f) * pn * e;
        }
    private:
        float noise() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.f / 2147483648.f) * 1.7f; }
        void cook (const Settings& s)
        {
            kStart = std::exp2 (s.startSt / 12.0) - 1.0;
            gDrive = std::pow (10.f, s.driveDb / 20.f);
            nL.lp (fs, s.dirtHz, 0.7071); nR.lp (fs, s.dirtHz, 0.7071); nk = 1.f;
            yL.lp (fs, 9000.0, 0.7071); yR.lp (fs, 9000.0, 0.7071);
            lo1.lp (fs, 180.0, 0.7071); lo2.lp (fs, 180.0, 0.7071);
            hiL1.hp (fs, 180.0, 0.7071); hiL2.hp (fs, 180.0, 0.7071); hiR1.hp (fs, 180.0, 0.7071); hiR2.hp (fs, 180.0, 0.7071);
            ck.hp (fs, 2500.0, 0.7071); tb1.hp (fs, 700.0, 0.7071); tb2.lp (fs, 3500.0, 0.7071);
            vh1.hp (fs, s.verbLowCut, 0.7071); vh2.hp (fs, s.verbLowCut, 0.7071);
            for (int k = 0; k < NL; ++k)
            {
                dLen[k] = std::clamp ((int) (baseD[k] * s.verbSize * fs / 44100.0), 1, (int) line[k].size() - 1);
                gk[k] = (float) std::pow (10.0, -3.0 * dLen[k] / (fs * std::max (0.05f, s.verbDecayMs * 0.001f)));
            }
            dA = (float) std::exp (-6.283185307 * s.dampHz / fs);
            preLen = std::clamp ((int) (s.preMs * 0.001 * fs), 0, (int) pre.size() - 1);
        }
        static constexpr double baseD[NL] { 1117, 1301, 1493, 1607, 1777, 1913, 2081, 2267 };
        double fs = 48000.0, ph = 0.0, php = 0.0, kStart = 1.3;
        std::vector<float> line[NL], pre;
        int idx[NL] {}, dLen[NL] {}, preIdx = 0, preLen = 0, blk = 0;
        float gk[NL] {}, damp[NL] {}, dA = 0.f, gDrive = 25.f, nk = 1.f;
        uint32_t seed = 0x1234567u;
        pvctail::Biquad nL, nR, yL, yR, lo1, lo2, hiL1, hiL2, hiR1, hiR2, ck, tb1, tb2, vh1, vh2;
    };
}
