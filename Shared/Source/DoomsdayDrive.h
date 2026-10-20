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
//   Bitcrusher   sample-rate reduction (hold) + bit depth, with mix  (the "artefact" on raw kick tails)
//   DrawnShaper  waveshaper drawn as a mod::Shape (x: input -1..1, y: output -1..1), table-based, optional symmetry
//                (like Trash 2's drawn curve); use 2x+ oversampling for hard curves
//   EnvFollower  attack / release follower (linear magnitude)
//   DynamicDrive drive that reacts to transients (Saturn "DYNAMICS"): amount > 0 = less drive on hits (punch stays clean),
//                < 0 = more drive on hits; returns a multiplier for the drive gain
//   TransientShaper  ATTACK / SUSTAIN in dB (fast vs slow envelopes, level independent)
//   AutoGain     keeps the output loudness equal to the input (slow RMS match): hear the character, not "louder"
//
//  Real-time safe: no allocation after prepare().  Tested in Shared/Tests (aliasing, level, latency).
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <functional>
#include "DoomsdayMod.h"

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

    // ------------------------------------------------------------------ Bitcrusher
    class Bitcrusher
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; reset(); }
        void reset() { phase = 1.f; held = 0.f; }
        void set (float targetRateHz, float bits, float mix01)
        {
            step = (float) juce::jlimit (0.0, 1.0, targetRateHz / sr);
            levels = std::pow (2.f, juce::jlimit (1.f, 24.f, bits) - 1.f);
            mix = juce::jlimit (0.f, 1.f, mix01);
        }
        inline float process (float x)
        {
            phase += step;
            if (phase >= 1.f) { phase -= 1.f; held = std::round (x * levels) / levels; }
            return mix >= 0.999f ? held : x + mix * (held - x);
        }
    private:
        double sr = 44100.0;
        float step = 1.f, levels = 32768.f, mix = 1.f, phase = 1.f, held = 0.f;
    };

    // ------------------------------------------------------------------ DrawnShaper
    class DrawnShaper
    {
    public:
        static constexpr int tableSize = 1024;
        // shape: x 0..1000 = input -1..+1, y 0..1 = output -1..+1. symmetric: only the right half (input >= 0) is used, mirrored
        void setShape (const doomsday::mod::Shape& s, bool symmetric)
        {
            for (int i = 0; i <= tableSize; ++i)
            {
                const float in = -1.f + 2.f * (float) i / (float) tableSize;
                float y;
                if (symmetric) { const float a = std::abs (in); y = (s.at (500.f + a * 500.f) * 2.f - 1.f) * (in < 0.f ? -1.f : 1.f); }
                else y = s.at ((in + 1.f) * 500.f) * 2.f - 1.f;
                table[(size_t) i] = y;
            }
        }
        // a straight line = no change
        static doomsday::mod::Shape identity() { return doomsday::mod::Shape::make ({ { 0.f, 0.f }, { 1000.f, 1.f } }); }
        inline float process (float x) const
        {
            const float pos = (juce::jlimit (-1.f, 1.f, x) + 1.f) * 0.5f * (float) tableSize;
            const int i = juce::jmin (tableSize - 1, (int) pos);
            const float f = pos - (float) i;
            return table[(size_t) i] + f * (table[(size_t) i + 1] - table[(size_t) i]);
        }
    private:
        std::array<float, tableSize + 1> table {};
    };

    // ------------------------------------------------------------------ EnvFollower
    struct EnvFollower
    {
        float env = 0.f, a = 0.f, r = 0.f;
        void prepare (double rate, float attackMs, float releaseMs)
        {
            a = 1.f - std::exp (-1.f / (juce::jmax (0.01f, attackMs) * 0.001f * (float) rate));
            r = 1.f - std::exp (-1.f / (juce::jmax (0.01f, releaseMs) * 0.001f * (float) rate));
        }
        void reset() { env = 0.f; }
        inline float process (float x) { const float m = std::abs (x); env += (m > env ? a : r) * (m - env); return env; }
    };

    // ------------------------------------------------------------------ DynamicDrive
    class DynamicDrive
    {
    public:
        void prepare (double rate) { fast.prepare (rate, 0.5f, 60.f); slow.prepare (rate, 25.f, 60.f); }
        void reset() { fast.reset(); slow.reset(); }
        // amount -1..1 ; returns the multiplier for the drive gain (1 = unchanged)
        inline float process (float x, float amount)
        {
            const float f = fast.process (x), s = slow.process (x);
            const float tr = juce::jlimit (0.f, 1.f, (f - s) / (f + 1.0e-6f) * 2.f);      // 0 steady .. 1 sharp hit
            return amount >= 0.f ? 1.f - amount * 0.9f * tr : 1.f - amount * 2.f * tr;
        }
    private:
        EnvFollower fast, slow;
    };

    // ------------------------------------------------------------------ TransientShaper
    class TransientShaper
    {
    public:
        void prepare (double rate)
        {
            fastA.prepare (rate, 0.3f, 120.f); slowA.prepare (rate, 20.f, 120.f);
            fastR.prepare (rate, 0.3f, 15.f);
            sm.prepare (rate, 1.f);
        }
        void reset() { fastA.reset(); slowA.reset(); fastR.reset(); sm.reset (0.f); }
        // attackDb / sustainDb: -24..+24 (how much the hit / the ring-out is boosted or cut)
        inline float process (float x, float attackDb, float sustainDb)
        {
            const float e1 = fastA.process (x) + 1.0e-6f, e2 = slowA.process (x) + 1.0e-6f, e3 = fastR.process (x) + 1.0e-6f;
            const float atk = juce::jlimit (0.f, 1.f, std::log2 (e1 / e2) * 0.5f);     // > 0 while the hit rises
            const float sus = juce::jlimit (0.f, 1.f, std::log2 (e1 / e3) * 0.5f);     // > 0 while it rings out
            const float db = sm.next (attackDb * atk + sustainDb * sus);
            return x * std::pow (10.f, juce::jlimit (-30.f, 30.f, db) / 20.f);
        }
    private:
        EnvFollower fastA, slowA, fastR;
        doomsday::mod::Smoother sm;
    };

    // ------------------------------------------------------------------ AutoGain
    class AutoGain
    {
    public:
        void prepare (double rate) { c = 1.f - std::exp (-1.f / (0.3f * (float) rate)); gs.prepare (rate, 50.f); gs.reset (1.f); }
        void reset() { pin = pout = 1.0e-6f; gs.reset (1.f); }
        // feed the dry input and the processed output; returns the corrected output
        inline float process (float in, float out)
        {
            pin += c * (in * in - pin); pout += c * (out * out - pout);
            const float g = pin > 1.0e-8f ? std::sqrt (pin / juce::jmax (1.0e-9f, pout)) : 1.f;
            return out * gs.next (juce::jlimit (0.06f, 16.f, g));
        }
    private:
        float c = 0.001f, pin = 1.0e-6f, pout = 1.0e-6f;
        doomsday::mod::Smoother gs;
    };
}
