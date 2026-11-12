#pragma once
// =====================================================================
//  4GE - TAIL COLOR (owner 2026-11-12, the "2012 tail"): a resonant bell on the TAIL group AFTER its distortion.
//  The distortion makes the harmonics (60, 120, 180 ... Hz), this EQ chooses which ones stay:
//  low = "BOOOM", ~300-600 Hz = "GRRRAAANG", high = "KRAAASH".
//  KEY TRACK: the centre = HARMONIC x the played note, so the colour stays the same when the kick plays a bass line.
//  DSP only: per-block coefficients (RBJ peaking), no allocation.
// =====================================================================
#include <cmath>
#include <algorithm>

namespace fge::dsp::color
{
    struct Settings
    {
        bool on = false;
        float gainDb = 0.f, freq = 500.f, q = 2.f;
        bool track = false;
        float harm = 8.f;          // KEY TRACK: which harmonic of the note
    };

    class TailColor
    {
    public:
        void reset() { z1[0] = z2[0] = z1[1] = z2[1] = 0.f; }
        // per block: the centre from the settings (KEY TRACK: harmonic x note)
        void cook (const Settings& s, float noteHz, double fs)
        {
            const float f = std::clamp (s.track ? s.harm * std::max (10.f, noteHz) : s.freq, 30.f, (float) (fs * 0.45));
            const float key = f * 1.0e3f + s.gainDb * 7.f + s.q;
            if (key == cooked && fs == rate) return;
            cooked = key; rate = fs;
            const double A = std::pow (10.0, s.gainDb / 40.0), w = 2.0 * 3.14159265358979 * f / fs;
            const double alpha = std::sin (w) / (2.0 * std::max (0.1f, s.q)), cw = std::cos (w);
            const double a0 = 1.0 + alpha / A;
            b0 = (float) ((1.0 + alpha * A) / a0); b1 = (float) (-2.0 * cw / a0); b2 = (float) ((1.0 - alpha * A) / a0);
            a1 = (float) (-2.0 * cw / a0); a2 = (float) ((1.0 - alpha / A) / a0);
        }
        void process (float& l, float& r)
        {
            l = tick (0, l); r = tick (1, r);
        }
    private:
        float tick (int c, float x)
        {
            const float y = b0 * x + z1[c];                                   // transposed direct form II
            z1[c] = b1 * x - a1 * y + z2[c];
            z2[c] = b2 * x - a2 * y;
            return y;
        }
        float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
        float z1[2] {}, z2[2] {};
        float cooked = -1.f;
        double rate = 0.0;
    };
}
