#pragma once
// =====================================================================
//  DOOMSDAY shared: DISTORTION LAB curves with anti-aliasing (namespace doomsday::lab)   (first used by RE4GE v0.7)
//
//   Every curve has its antiderivative, so it runs through 1st-order ADAA (far less aliasing "sand" at high drive).
//   HARDNESS (0..1) changes the character of each curve; blends of two ADAA'd curves stay ADAA (it is linear).
//     SATURATION  tanh  <->  hard clip
//     DIODE       asymmetric: soft exponential top, a fast tight bottom (even harmonics, "germanium")  <->  hard clip
//     WAVEFOLD    sine folding (more folds the harder you drive)  <->  hard clip
//     RECTIFIER   tanh with the negative half flipped: 0 = plain, 0.5 = half-wave, 1 = full-wave (an octave up)
//     BITCRUSH    sample hold + bit depth (HARDNESS = fewer bits / lower rate). Aliases on purpose - no ADAA.
//     CUSTOM      a drawn curve (mod::Shape, x 0..1000 = input -1..+1, y 0..1 = output -1..+1, optional SYMMETRIC);
//                 the table and its running integral are rebuilt only when the drawing changes (call setCustom off the audio path)
//   Tilt         a one-pole tilt EQ around 800 Hz (TONE -1..+1: darker / brighter), used PRE or POST the curve
//
//  Real-time safe (no allocation; the custom tables are fixed arrays).  Tested in Shared/Tests.
// =====================================================================
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include "DoomsdayMod.h"

namespace doomsday::lab
{
    enum Algo { Saturation = 0, Diode, Wavefold, Rectifier, Bitcrush, Custom, numAlgos };

    namespace detail
    {
        constexpr double halfPi = 1.5707963267948966;
        inline double logCosh (double x) { const double a = std::abs (x); return a + std::log1p (std::exp (-2.0 * a)) - 0.69314718055994531; }
        inline double clipF (double x)  { return juce::jlimit (-1.0, 1.0, x); }
        inline double clipFI (double x) { return std::abs (x) <= 1.0 ? 0.5 * x * x : std::abs (x) - 0.5; }
        // DIODE: x >= 0: 1 - e^-x (soft, slope 1 at 0, -> 1); x < 0: 0.4 tanh (x / 0.4) (tight, -> -0.4)
        inline double diodeF (double x)  { return x >= 0.0 ? 1.0 - std::exp (-x) : 0.4 * std::tanh (x / 0.4); }
        inline double diodeFI (double x) { return x >= 0.0 ? x + std::exp (-x) - 1.0 : 0.16 * logCosh (x / 0.4); }
        inline double foldF (double x)   { return std::sin (x * halfPi); }
        inline double foldFI (double x)  { return (1.0 - std::cos (x * halfPi)) / halfPi; }
    }

    struct CustomTable
    {
        static constexpr int size = 512;
        std::array<float, size + 1> f {};            // y at x = -1 .. +1
        std::array<double, size + 1> F {};           // running integral of f from -1
        void build (const doomsday::mod::Shape& s, bool symmetric)
        {
            for (int i = 0; i <= size; ++i)
            {
                const float in = -1.f + 2.f * (float) i / (float) size;
                float y;
                if (symmetric) { const float a = std::abs (in); y = (s.at (500.f + a * 500.f) * 2.f - 1.f) * (in < 0.f ? -1.f : 1.f); }
                else y = s.at ((in + 1.f) * 500.f) * 2.f - 1.f;
                f[(size_t) i] = y;
            }
            const double dx = 2.0 / size;
            F[0] = 0.0;
            for (int i = 1; i <= size; ++i) F[(size_t) i] = F[(size_t) i - 1] + 0.5 * dx * (f[(size_t) i - 1] + f[(size_t) i]);
        }
        inline double eval (double x) const
        {
            const double pos = (juce::jlimit (-1.0, 1.0, x) + 1.0) * 0.5 * size;
            const int i = juce::jmin (size - 1, (int) pos);
            const double fr = pos - i;
            return f[(size_t) i] + fr * (f[(size_t) i + 1] - f[(size_t) i]);
        }
        // the integral, also beyond +-1 (the curve holds its end values there)
        inline double integral (double x) const
        {
            const double dx = 2.0 / size;
            if (x <= -1.0) return (x + 1.0) * f[0];
            if (x >= 1.0) return F[size] + (x - 1.0) * f[size];
            const double pos = (x + 1.0) / dx;
            const int i = juce::jmin (size - 1, (int) pos);
            const double t = (pos - i) * dx;                               // distance into segment i
            const double a = f[(size_t) i], slope = (f[(size_t) i + 1] - a) / dx;
            return F[(size_t) i] + a * t + 0.5 * slope * t * t;
        }
    };

    // the curve and its antiderivative for (algo, hardness); custom = the drawn table
    inline double curve (double x, int algo, float h, const CustomTable* c)
    {
        using namespace detail;
        switch (algo)
        {
            case Saturation: return (1.0 - h) * std::tanh (x) + h * clipF (x);
            case Diode:      return (1.0 - h) * diodeF (x) + h * clipF (x);
            case Wavefold:   return (1.0 - h) * foldF (x) + h * clipF (x);
            case Rectifier:  { const double t = std::tanh (x); return x >= 0.0 ? t : t * (1.0 - 2.0 * h); }
            case Custom:     return c != nullptr ? c->eval (x) : clipF (x);
            default:         return clipF (x);
        }
    }
    inline double antiderivative (double x, int algo, float h, const CustomTable* c)
    {
        using namespace detail;
        switch (algo)
        {
            case Saturation: return (1.0 - h) * logCosh (x) + h * clipFI (x);
            case Diode:      return (1.0 - h) * diodeFI (x) + h * clipFI (x);
            case Wavefold:   return (1.0 - h) * foldFI (x) + h * clipFI (x);
            case Rectifier:  return x >= 0.0 ? logCosh (x) : (1.0 - 2.0 * h) * logCosh (x);
            case Custom:     return c != nullptr ? c->integral (x) : clipFI (x);
            default:         return clipFI (x);
        }
    }

    // one stage of the lab (per channel): ADAA curve, or the crusher
    class Shaper
    {
    public:
        void reset() { x1 = 0.0; F1 = 0.0; key = -1.f; held = 0.f; ph = 1.f; }
        inline float process (float xf, int algo, float h, const CustomTable* c)
        {
            if (algo == Bitcrush)
            {
                // fewer bits and a lower rate the harder: 12 bits / every sample  ->  3 bits / every 24th sample
                const float levels = std::exp2 (11.f - 9.f * h), step = 1.f / (1.f + 23.f * h * h);
                ph += step;
                if (ph >= 1.f) { ph -= 1.f; held = std::round ((float) std::tanh (xf) * levels) / levels; }
                return held;
            }
            const double x = xf;
            const float k = (float) algo * 10.f + h;
            if (k != key) { key = k; F1 = antiderivative (x1, algo, h, c); }       // the curve changed: no jump in the integral
            const double Fx = antiderivative (x, algo, h, c);
            const double dx = x - x1;
            const double y = std::abs (dx) > 1.0e-5 ? (Fx - F1) / dx : curve (0.5 * (x + x1), algo, h, c);
            x1 = x; F1 = Fx;
            return (float) y;
        }
        void customChanged() { key = -1.f; }
    private:
        double x1 = 0.0, F1 = 0.0;
        float key = -1.f, held = 0.f, ph = 1.f;
    };

    // TONE: tilt around ~800 Hz (one-pole split), -1 = darker, +1 = brighter, +-9 dB at the extremes
    struct Tilt
    {
        float lp = 0.f, a = 0.03f, gLo = 1.f, gHi = 1.f;
        void set (float tone, double rate)
        {
            a = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 800.0 / rate));
            const float db = 9.f * juce::jlimit (-1.f, 1.f, tone);
            gHi = std::pow (10.f, db / 40.f); gLo = 1.f / gHi;
        }
        inline float process (float x) { lp += (x - lp) * a; return lp * gLo + (x - lp) * gHi; }
        void reset() { lp = 0.f; }
    };
}
