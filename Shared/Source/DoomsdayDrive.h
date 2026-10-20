#pragma once
// =====================================================================
//  DOOMSDAY shared: clean distortion (namespace doomsday::drive)
//
//   Adaa1        anti-aliased waveshaper (1st-order antiderivative anti-aliasing): the same curves as a plain
//                shaper, but far less digital "sand" (aliasing) on high notes / hard drive. Half a sample of delay.
//                Curves: SOFT (tanh), HARD (clip), TUBE (asymmetric tanh), FOLD (sine fold), CUBIC (gentle).
//   plain (x, c) the same curves without anti-aliasing (for comparison / cheap paths)
//   OverSampler  1x / 2x / 4x / 8x wrapper around juce::dsp::Oversampling with every factor prepared up front
//                (switch factor without allocating), latency in samples, a per-sample callback at the high rate.
//   DcBlock      one-pole DC blocker (after asymmetric curves)
//   makeup (c, driveLin)  rough level compensation so turning the drive up does not just make it louder
//
//  Real-time safe: no allocation after prepare().  Tested in Shared/Tests (aliasing, level, latency).
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <functional>

namespace doomsday::drive
{
    enum Curve { Soft = 0, Hard, Tube, Fold, Cubic, numCurves };
    inline const char* curveName (int c)
    {
        static const char* n[] { "SOFT", "HARD", "TUBE", "FOLD", "CUBIC" };
        return n[juce::jlimit (0, (int) numCurves - 1, c)];
    }

    namespace detail
    {
        constexpr double tubeBias = 0.3, tubeOffset = 0.29131261245159090;   // tanh (0.3)
        constexpr double tubeNorm = 1.0 / (1.0 - tubeOffset * tubeOffset);  // slope 1 at zero: quiet signals pass at unity
        constexpr double halfPi = 1.5707963267948966;
        inline double logCosh (double x) { const double a = std::abs (x); return a + std::log1p (std::exp (-2.0 * a)) - 0.69314718055994531; }
        inline double f (double x, int c)
        {
            switch (c)
            {
                case Soft:  return std::tanh (x);
                case Hard:  return juce::jlimit (-1.0, 1.0, x);
                case Tube:  return (std::tanh (x + tubeBias) - tubeOffset) * tubeNorm;
                case Fold:  return std::sin (juce::jlimit (-8.0, 8.0, x) * halfPi);
                default:    { const double y = juce::jlimit (-1.5, 1.5, x); return y - y * y * y / 6.75; }   // CUBIC: smooth, saturates at |x| = 1.5
            }
        }
        // antiderivatives F' = f
        inline double F (double x, int c)
        {
            switch (c)
            {
                case Soft:  return logCosh (x);
                case Hard:  return std::abs (x) <= 1.0 ? 0.5 * x * x : std::abs (x) - 0.5;
                case Tube:  return (logCosh (x + tubeBias) - tubeOffset * x) * tubeNorm;
                case Fold:
                {
                    // sin fold is clamped at |x| = 8 (value sin (4 pi) = 0, so F continues flat beyond)
                    const double xc = juce::jlimit (-8.0, 8.0, x);
                    return -std::cos (xc * halfPi) / halfPi;
                }
                default:
                {
                    const double a = std::abs (x);
                    if (a <= 1.5) return 0.5 * x * x - x * x * x * x / 27.0;
                    const double edge = 0.5 * 2.25 - 5.0625 / 27.0;       // F (1.5)
                    return edge + (a - 1.5) * 1.0;                         // f = +-1 beyond (1.5 - 3.375 / 6.75 = 1)
                }
            }
        }
    }

    inline float plain (float x, int curve) { return (float) detail::f ((double) x, curve); }

    // level compensation for a drive gain (approximate, keeps loudness roughly steady while the drive goes up)
    inline float makeup (int curve, float driveLin)
    {
        const float d = juce::jmax (1.f, driveLin);
        return curve == Fold ? 1.f : 1.f / std::pow (d, 0.35f);
    }

    class Adaa1
    {
    public:
        void reset() { x1 = 0.0; F1 = 0.0; curveNow = -1; }
        inline float process (float xf, int curve)
        {
            const double x = xf;
            if (curve != curveNow) { curveNow = curve; F1 = detail::F (x1, curve); }
            const double Fx = detail::F (x, curve);
            const double dx = x - x1;
            double y;
            if (std::abs (dx) > 1.0e-5) y = (Fx - F1) / dx;
            else                        y = detail::f (0.5 * (x + x1), curve);
            x1 = x; F1 = Fx;
            return (float) y;
        }
    private:
        double x1 = 0.0, F1 = 0.0;
        int curveNow = -1;
    };

    struct DcBlock
    {
        float x1 = 0.f, y1 = 0.f, r = 0.9995f;
        void prepare (double rate, float hz = 10.f) { r = 1.f - juce::MathConstants<float>::twoPi * hz / (float) rate; }
        void reset() { x1 = y1 = 0.f; }
        inline float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    };

    // every factor prepared once; process() runs `perSample (channel, sample)` at the high rate
    class OverSampler
    {
    public:
        void prepare (int numChannels, int maxBlock)
        {
            channels = numChannels;
            for (int i = 1; i < 4; ++i)
            {
                os[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numChannels, (size_t) i,
                                     juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
                os[(size_t) i]->initProcessing ((size_t) maxBlock);
            }
        }
        void reset() { for (auto& o : os) if (o) o->reset(); }
        // 0 = 1x, 1 = 2x, 2 = 4x, 3 = 8x
        void setFactorIndex (int idx) { factor = juce::jlimit (0, 3, idx); }
        int factorIndex() const { return factor; }
        int ratio() const { return 1 << factor; }
        float latencySamples() const { return factor == 0 ? 0.f : os[(size_t) factor]->getLatencyInSamples(); }

        template <typename Fn>
        void process (juce::dsp::AudioBlock<float> block, Fn&& perSample)
        {
            if (factor == 0)
            {
                for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                {
                    auto* d = block.getChannelPointer (ch);
                    for (size_t i = 0; i < block.getNumSamples(); ++i) d[i] = perSample ((int) ch, d[i]);
                }
                return;
            }
            auto& o = *os[(size_t) factor];
            auto up = o.processSamplesUp (block);
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = perSample ((int) ch, d[i]);
            }
            o.processSamplesDown (block);
        }
    private:
        int channels = 2, factor = 0;
        std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 4> os;
    };
}
