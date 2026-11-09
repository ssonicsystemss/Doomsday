#pragma once
// =====================================================================
//  4GE module 12 - the last steps of an export (pure DSP): NORMALIZATION (OFF / PEAK / LUFS - a plain gain, NEVER a hidden limiter:
//  when the target would push the true peak over the TRUE PEAK CEILING the gain stops at the ceiling and the result says so),
//  OUTPUT MODE mono, and DITHER (OFF / TPDF / NOISE SHAPING) - only for 16 / 24-bit integer files, once, at the very end,
//  seeded (the same export = the same file).
// =====================================================================
#include "../Meter/MasterMeter.h"

namespace fge::dsp::xport
{
    enum Norm { NormOff = 0, NormPeak, NormLufs };
    enum Dither { DitherOff = 0, DitherTpdf, DitherShaped };

    struct NormResult { float gainDb = 0.f, wantedDb = 0.f; bool limitedByCeiling = false, impossible = false; juce::String note; };
    // the gain the normalization would apply (also the PREVIEW: "predicted gain")
    inline NormResult planNormalize (const meter::Measure& m, int mode, float target, float ceilingDb)
    {
        NormResult r;
        if (mode == NormOff) return r;
        if (mode == NormLufs && ! m.lufsValid) { r.impossible = true; r.note = "LUFS needs at least 400 ms of sound - nothing changed"; return r; }
        if (m.samplePeakDb < -170.f) { r.impossible = true; r.note = "silence - nothing to normalize"; return r; }
        r.wantedDb = mode == NormPeak ? target - m.samplePeakDb : target - m.lufs;
        r.gainDb = r.wantedDb;
        if (m.truePeakDb + r.gainDb > ceilingDb)
        {
            r.gainDb = ceilingDb - m.truePeakDb;
            r.limitedByCeiling = true;
            r.note = "the target needs " + juce::String (r.wantedDb - r.gainDb, 1) + " dB more than the TRUE PEAK CEILING allows - a limiter would be needed (not added)";
        }
        return r;
    }
    inline void applyGain (juce::AudioBuffer<float>& b, float db) { if (std::abs (db) > 1.0e-6f) b.applyGain (juce::Decibels::decibelsToGain (db, -1000.f)); }
    inline void toMono (juce::AudioBuffer<float>& b)
    {
        if (b.getNumChannels() < 2) return;
        juce::AudioBuffer<float> m (1, b.getNumSamples());
        for (int i = 0; i < b.getNumSamples(); ++i) m.setSample (0, i, 0.5f * (b.getSample (0, i) + b.getSample (1, i)));
        b = std::move (m);
    }
    // DITHER to 'bits' (16 / 24): TPDF of +-1 LSB, NOISE SHAPING = TPDF with first-order error feedback (noise moved up)
    inline void dither (juce::AudioBuffer<float>& b, int bits, int mode, uint32_t seed)
    {
        if (mode == DitherOff || bits >= 32) return;
        const double q = 1.0 / std::pow (2.0, bits - 1);
        uint32_t x = seed * 2654435761u + 0x9E3779B9u;
        auto rnd = [&x] { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return (double) x / 4294967296.0; };
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            float* p = b.getWritePointer (c);
            double err = 0.0;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double d = (rnd() - rnd()) * q;                     // TPDF
                const double in = (double) p[i] - (mode == DitherShaped ? err : 0.0);
                const double y = std::round ((in + d) / q) * q;
                err = y - in;
                p[i] = (float) juce::jlimit (-1.0, 1.0 - q, y);
            }
        }
    }
}
