#pragma once
// =====================================================================
//  4GE - CRACK MOVEMENT (owner 2026-11-21 "brakuje ruchu"): movement on the CRACK REVERB's wet path, locked to the crack start
//  (and an LFO RATE for the comb effects). Research: On Point Samples puts the rawstyle punch through Serum's FLANGER (Flg HL6-, drive
//  maxed) and gives the crunch a long DOWNWARD pitch envelope ("pogo"); Serum hardstyle kick guides use FLANGER + PHASER for movement;
//  reverb tails get movement from a phaser / flanger after the reverb.
//    SWEEP   : the wet's low-pass moves - up (+) or down (-) by up to 3 octaves over TIME
//    FLANGER : a short modulated delay with negative feedback (HL6- like), the delay sweeps down over TIME (a "jet"), + the LFO RATE
//    PHASER  : 6 all-pass stages, the notch centre sweeps over TIME, + the LFO RATE
//    PITCH   : a doppler delay - its growing delay pulls the wet's pitch down ("pogo"), up to -24 st over TIME
//  DSP only; buffers made in prepare(), clear() per kick.
// =====================================================================
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include "../Tone/Biquad.h"

namespace fge::dsp::front
{
    struct CrackMoveSettings
    {
        float sweep = 0.f, flange = 0.f, phaser = 0.f, pitchSt = 0.f, rateHz = 2.f, timeMs = 200.f;
        bool any() const { return std::abs (sweep) > 0.001f || flange > 0.001f || phaser > 0.001f || pitchSt < -0.01f; }
    };

    class CrackMove
    {
    public:
        void prepare (double rate)
        {
            fs = rate;
            fl.assign ((size_t) (fs * 0.012) + 8, 0.f);
            pd.assign ((size_t) (fs * 0.60) + 8, 0.f);
            clear();
        }
        void clear()
        {
            std::fill (fl.begin(), fl.end(), 0.f); std::fill (pd.begin(), pd.end(), 0.f); fw = pw = 0; flFb = phFb = pDelay = 0.f; lfo = 0.0; blk = 0;
            for (auto& a : ap) a = 0.f;
            sw.clear(); sw2.clear();
        }
        // baseLpHz = the reverb filter's LP (the SWEEP moves around it); t = ms since the crack started
        float process (const CrackMoveSettings& s, float x, float t, float baseLpHz)
        {
            if (fl.empty()) return x;
            const float u = std::clamp (t / std::max (10.f, s.timeMs), 0.f, 1.f);       // 0 -> 1 over TIME
            const float k = 1.f - std::exp (-3.f * u);                                    // fast first, then it settles (an envelope, not a ramp)
            lfo += s.rateHz / fs; if (lfo >= 1.0) lfo -= 1.0;
            const float l = (float) std::sin (6.283185307 * lfo);
            // PITCH ("pogo"): the delay grows -> the pitch falls; ratio r = 2^(st/12): d(delay)/dt = 1 - r, ramped in over TIME
            if (s.pitchSt < -0.01f)
            {
                pd[(size_t) pw] = x;
                const float r = std::exp2 (s.pitchSt * k / 12.f);
                pDelay = std::min (pDelay + (1.f - r), (float) pd.size() - 4.f);
                x = read (pd, pw, pDelay);
                if (++pw >= (int) pd.size()) pw = 0;
            }
            else pDelay = 0.f;
            // FLANGER: 0.25..6 ms, the delay sweeps DOWN over TIME (a rising "jet") + the LFO, negative feedback (HL6-)
            if (s.flange > 0.001f)
            {
                const float dMs = 0.25f + 5.75f * (1.f - k) * (0.6f + 0.4f * (0.5f + 0.5f * l));
                const float d = std::clamp (dMs * 0.001f * (float) fs, 1.f, (float) fl.size() - 4.f);
                fl[(size_t) fw] = x - 0.7f * flFb;
                const float y = read (fl, fw, d);
                flFb = y;
                if (++fw >= (int) fl.size()) fw = 0;
                x = (x + s.flange * y) / (1.f + 0.6f * s.flange);
            }
            // PHASER: 6 first-order all-passes, the centre 400 Hz .. 6 kHz sweeps over TIME (down) + the LFO
            if (s.phaser > 0.001f)
            {
                const double fc = 400.0 * std::pow (15.0, (double) ((1.f - k) * 0.7f + 0.3f * (0.5f + 0.5f * l)));
                const float tn = (float) std::tan (3.14159265 * std::min (fc, fs * 0.45) / fs), c = (tn - 1.f) / (tn + 1.f);
                float y = x + 0.5f * phFb;
                for (auto& z : ap) { const float o = c * y + z; z = y - c * o; y = o; }
                phFb = y;
                x = x * (1.f - 0.5f * s.phaser) + y * s.phaser;
            }
            // SWEEP: the wet's low-pass moves +- 3 octaves from the reverb LP over TIME
            if (std::abs (s.sweep) > 0.001f)
            {
                if (blk-- <= 0)
                {
                    blk = 16;
                    const double f = std::clamp ((double) baseLpHz * std::exp2 (3.0 * s.sweep * k - (s.sweep > 0.f ? 3.0 * s.sweep : 0.0))   /* UP: base/8 -> base, DOWN: base -> base/8 */, 200.0, fs * 0.45);
                    sw.lp (fs, f, 1.2); sw2.lp (fs, f, 0.7071);
                }
                x = sw2.tick (sw.tick (x));
            }
            return x;
        }

    private:
        static float read (const std::vector<float>& b, int w, float d)
        {
            const int n = (int) b.size();
            float pos = (float) w - d; while (pos < 0.f) pos += (float) n;
            const int i0 = (int) pos; const float f = pos - (float) i0;
            const int i1 = (i0 + 1) % n;
            return b[(size_t) i0] + f * (b[(size_t) i1] - b[(size_t) i0]);
        }
        double fs = 48000.0, lfo = 0.0;
        std::vector<float> fl, pd;
        int fw = 0, pw = 0, blk = 0;
        float flFb = 0.f, phFb = 0.f, pDelay = 0.f;
        std::array<float, 6> ap {};
        bq::Biquad sw, sw2;
    };
}
