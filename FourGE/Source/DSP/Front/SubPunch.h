#pragma once
// =====================================================================
//  4GE - SUB PUNCH (owner 2026-11-16): SUB PUNCH != SUB. A short impulse of the low / lower mid that gives the kick its "hit",
//  10 - 70 ms (from ~10 ms). It may have a pitch envelope, but it must be gone before the real SUB dominates: the pitch falls to
//  END (never below the sub's note region it hands over to) and the level falls away fast in its second half. Mono.
//  Shared by every PUNCH TYPE; the type only changes how it is used (FrontProfiles.h). DSP only; reset() per kick.
// =====================================================================
#include <cmath>
#include <algorithm>
#include "../Tone/Biquad.h"

namespace fge::dsp::front
{
    struct SubPunchSettings
    {
        bool on = false;
        float gain = 0.f;             // linear
        float startHz = 180.f, endHz = 90.f, delayMs = 10.f, lengthMs = 50.f, drive = 0.3f, tone = 0.4f;
    };

    class SubPunchUnit
    {
    public:
        void prepare (double rate) { fs = rate; reset(); }
        void reset() { ph = 0.0; lp.clear(); blk = 0; }

        float process (const SubPunchSettings& s, float ms)
        {
            const float t = ms - s.delayMs;
            if (! s.on || s.gain <= 0.f || t < 0.f || t > s.lengthMs) return 0.f;
            if (blk-- <= 0) { blk = 32; lp.lp (fs, 250.0 + 1200.0 * s.tone, 0.7071); }
            const float len = std::max (5.f, s.lengthMs), u = t / len;
            const double f = s.endHz + (s.startHz - s.endHz) * std::exp (-4.0 * u);                 // pitch envelope: start -> end
            ph += f / fs; if (ph >= 1.0) ph -= 1.0;
            // rise 1.5 ms, hold, then gone fast in the second half (before the SUB takes over)
            const float env = std::min (1.f, t / 1.5f) * (u < 0.5f ? 1.f - 0.3f * u : 0.85f * std::pow (std::max (0.f, (1.f - u) * 2.f), 2.f));
            const float g = 1.f + 12.f * s.drive;
            return s.gain * lp.tick (std::tanh ((float) std::sin (6.283185307 * ph) * g) / std::tanh (g) * env);
        }
    private:
        double fs = 48000.0, ph = 0.0;
        int blk = 0;
        bq::Biquad lp;
    };
}
