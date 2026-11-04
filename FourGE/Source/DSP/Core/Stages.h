#pragma once
// =====================================================================
//  4GE distortion stage curves (module 05 types, used early by the TAIL stage 2 - the "sound pass" after the owner's
//  first A/B, 2026-11-04). Each type is its own transfer function (not one tanh with different names):
//   1 SOFT CLIP   2 HARD CLIP   3 TUBE (asym, even)   4 DIODE (exp knee, asym)   5 WAVEFOLD (sine fold)
//   6 BITCRUSH    7 RECTIFIER (full-wave blend)        8 ASYMMETRIC (pos / neg drive differ)
//   9 FOLDBACK (reflects at the threshold)             10 DIGITAL CLIP (hard, then a fixed ceiling step)
//  stage(): y = curve (g x + bias) - curve (bias)  (bias = even harmonics without DC), dry / wet MIX, level kept near unity.
// =====================================================================
#include <juce_core/juce_core.h>
#include <cmath>

namespace fge::dsp
{
    inline float stageCurve (float x, int type)
    {
        switch (type)
        {
            case 1:  return x / std::sqrt (1.f + x * x);                                                    // SOFT CLIP (algebraic)
            case 2:  return juce::jlimit (-1.f, 1.f, x);                                                    // HARD CLIP
            case 3:  return std::tanh (x + 0.3f) - 0.2913126f;                                              // TUBE
            case 4:  return x >= 0.f ? 1.f - std::exp (-x) : -0.6f * (1.f - std::exp (1.6f * x));           // DIODE
            case 5:  return std::sin (juce::jlimit (-12.f, 12.f, x) * 1.5707963f);                          // WAVEFOLD
            case 6:  { const float q = 7.f; return std::round (juce::jlimit (-1.f, 1.f, x) * q) / q; }     // BITCRUSH (3 bits)
            case 7:  { const float c = juce::jlimit (-1.f, 1.f, x); return 0.35f * c + 0.65f * (2.f * std::abs (c) - 1.f); }   // RECTIFIER
            case 8:  return x >= 0.f ? std::tanh (x) : std::tanh (2.5f * x) * 0.7f;                          // ASYMMETRIC
            case 9:                                                                                        // FOLDBACK
            {
                float y = x;
                for (int i = 0; i < 6 && std::abs (y) > 1.f; ++i) y = y > 1.f ? 2.f - y : -2.f - y;
                return juce::jlimit (-1.f, 1.f, y);
            }
            case 10: { const float c = juce::jlimit (-1.f, 1.f, 1.3f * x); return std::abs (c) > 0.92f ? std::copysign (0.92f, c) : c; }   // DIGITAL
            default: return x;
        }
    }
    inline float stage (float x, int type, float g, float bias, float mix)
    {
        if (type <= 0) return x;
        const float y = stageCurve (g * x + bias, type) - (std::abs (bias) > 1.0e-4f ? stageCurve (bias, type) : 0.f);
        return x + (y - x) * mix;
    }
}
