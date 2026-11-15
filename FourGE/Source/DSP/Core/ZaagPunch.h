#pragma once
// =====================================================================
//  4GE - PUNCH TYPE ZAAG (owner 2026-11-14 / 15): the punch of the zaag kick, its own engine (ARCHITECTURE.md rule 0):
//  a 2 ms CLICK, the PIEP FIRST (a short tonal pip falling to half its pitch), THEN the TRASK (a falling saw x3 at the note,
//  hard, 700 Hz - 3.5 kHz) and a REVERB on these only (send, pre-delay, brightness, gated). Owner's picks: O2 (piep -> trask),
//  reverbs V2 (gated) / V4 (bright hall + pre-delay) / V5 (send 30 ms). DSP only; buffers in prepare(); reset() per kick.
// =====================================================================
#include <cstdint>
#include "../Tone/Biquad.h"
#include "../Tone/Fdn.h"

namespace fge::dsp::zaagpunch
{
    struct Settings
    {
        bool on = false;
        float click = 0.5f, piep = 0.6f, piepHz = 1400.f, piepMs = 15.f;
        float trask = 0.7f, traskDelay = 15.f, traskMs = 40.f, traskTone = 3500.f;
        float thump = 0.f, thumpHz = 300.f, thumpMs = 50.f, thumpDriveDb = 12.f;
        float verb = 2.f, verbDecayMs = 1600.f, verbSize = 1.2f, sendMs = 80.f, preMs = 0.f, gateMs = 0.f, dampHz = 4500.f, verbLowCut = 400.f;
    };

    class Unit
    {
    public:
        void prepare (double rate) { fs = rate; rev.prepare (rate, 2.0, 70.0); reset(); }
        void reset() { rev.reset(); php = 0.0; pht = 0.0; phk = 0.0; blk = 0; seed = 0x2468ace1u; for (auto* b : { &ck, &tb1, &tb2, &vh1, &vh2 }) b->clear(); }
        void process (const Settings& s, float ms, float noteHz, float& outL, float& outR)
        {
            if (blk-- <= 0)
            {
                blk = 32;
                ck.hp (fs, 2500.0, 0.7071); tb1.hp (fs, 700.0, 0.7071); tb2.lp (fs, s.traskTone, 0.7071);
                vh1.hp (fs, s.verbLowCut, 0.7071); vh2.hp (fs, s.verbLowCut, 0.7071);
                rev.set (s.verbSize, s.verbDecayMs, s.dampHz, s.preMs);
            }
            float top = s.click * std::exp (-ms / 2.f) * ck.tick (noise());
            if (s.piep > 0.f)                                                       // PIEP first
            {
                const float pf = s.piepHz * (0.5f + 0.5f * std::exp (-ms / std::max (1.f, s.piepMs * 0.6f)));
                const float e = ms < 1.f ? ms : ms < s.piepMs ? 1.f : std::max (0.f, 1.f - (ms - s.piepMs) / (0.6f * s.piepMs));
                top += s.piep * std::tanh (3.f * (float) std::sin (6.283185307 * php)) * e;
                php += pf / fs; if (php >= 1.0) php -= 1.0;
            }
            if (s.trask > 0.f)                                                      // THEN the TRASK
            {
                const double hz = std::clamp ((double) noteHz * 3.0 * (1.0 + 1.25 * std::exp (-ms / 22.0)), 20.0, fs * 0.4);
                const float t = ms - s.traskDelay;
                float e = 0.f;
                if (t >= 0.f) e = t < 2.f ? t * 0.5f : t < s.traskMs ? 1.f - 0.3f * (t - 2.f) / std::max (1.f, s.traskMs - 2.f)
                                                     : std::max (0.f, 0.7f * (1.f - (t - s.traskMs) / (0.8f * s.traskMs)));
                if (e > 0.f) top += s.trask * 0.5f * tb2.tick (tb1.tick (std::tanh (40.f * ((float) (2.0 * pht - 1.0) + 0.3f * noise())))) * e;
                pht += hz / fs; if (pht >= 1.0) pht -= 1.0;
            }
            float body = 0.f;
            if (s.thump > 0.f && ms < s.thumpMs * 1.5f)                             // THUMP: the hit (a driven body falling to the note)
            {
                const double hz = (double) noteHz + ((double) s.thumpHz - noteHz) * std::exp (-ms / std::max (1.f, s.thumpMs * 0.35f));
                const float e = (ms < 0.5f ? ms * 2.f : 1.f) * std::exp (-ms / std::max (1.f, s.thumpMs * 0.5f));
                const float g = std::pow (10.f, s.thumpDriveDb / 20.f);
                body = s.thump * std::tanh (g * (float) std::sin (6.283185307 * phk)) * e;
                phk += hz / fs; if (phk >= 1.0) phk -= 1.0;
            }
            float L = top + body, R = top + body;
            if (s.verb > 0.f)                                                       // the PUNCH REVERB
            {
                float rl = 0.f, rr = 0.f;
                rev.tick (vh2.tick (vh1.tick (top * std::clamp ((s.sendMs - ms) / 10.f, 0.f, 1.f))), rl, rr);
                const float gg = s.gateMs > 0.f ? std::clamp ((s.gateMs + 8.f - ms) / 8.f, 0.f, 1.f) : 1.f;
                L += s.verb * 1.2f * rl * gg; R += s.verb * 1.2f * rr * gg;
            }
            outL = L; outR = R;
        }
    private:
        float noise() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.f / 2147483648.f) * 1.7f; }
        double fs = 48000.0, php = 0.0, pht = 0.0, phk = 0.0;
        int blk = 0;
        uint32_t seed = 0x2468ace1u;
        fdn::Fdn rev;
        bq::Biquad ck, tb1, tb2, vh1, vh2;
    };
}
