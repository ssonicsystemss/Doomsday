#pragma once
// =====================================================================
//  4GE - a neutral biquad (RBJ) shared by the engines (ARCHITECTURE.md rule 0: engines never include each other).
//  DSP only; set the coefficients per block, tick() per sample.
// =====================================================================
#include <cmath>
#include <algorithm>

namespace fge::dsp::bq
{
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
}
