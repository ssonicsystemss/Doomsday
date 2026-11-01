#pragma once
// RE4GE - TONE: PRE FILTER -> DRIVE (4GE stage: type, BIAS, MIX, auto gain; DRIVE ENV = dirtier at the peak) -> DC 5 Hz
//         -> LOW CUT (off by default: the owner's rule, the low end stays) -> HIGH CUT (4-pole) -> BELL
// One ToneChain per chain (centre / left / right); the coefficients are made once per chunk (ToneCoefs).
#include "ReParams.h"

namespace re
{
    struct ToneCoefs
    {
        doomsday::dsp::SvfCoefs pre, lc, hc1, hc2;
        doomsday::dsp::BiquadCoefs bell;
        float dcR = 0.999f;
        void cook (const Params& p, double osr)
        {
            using namespace doomsday::dsp;
            pre = svfCoefs (p.preHz, ge::resoToK (p.preReso), osr);
            lc = svfCoefs (juce::jmax (20.f, p.lowCutHz), 1.f / 0.707f, osr);
            hc1 = svfCoefs (p.highCutHz, 1.f / 0.54f, osr); hc2 = svfCoefs (p.highCutHz, 1.f / 1.31f, osr);
            bell = BiquadCoefs::make (BiquadCoefs::Bell, osr, p.bellHz, juce::jmax (0.2f, p.bellQ), p.bellDb);
            dcR = (float) std::exp (-juce::MathConstants<double>::twoPi * 5.0 / osr);       // rate-independent (LESSONS: Forge's 0.9995)
        }
    };

    struct ToneChain
    {
        doomsday::dsp::Svf pre, hc1, hc2, lcF;
        doomsday::dsp::Biquad bell;
        float dcx = 0.f, dcy = 0.f;
        void clear() { *this = ToneChain(); }
        // g = drive gain (linear) for this sample
        inline float process (float x, float g, const Params& p, const ToneCoefs& c)
        {
            x = ge::filt (pre, x, c.pre, p.preType);
            x = ge::stage (x, g, p.distType, p.bias, p.mix, true);
            const float dc = x - dcx + c.dcR * dcy; dcx = x; dcy = dc; x = dc;
            if (p.lowCutHz > 20.5f) x = lcF.hp (x, c.lc);
            x = hc2.lp (hc1.lp (x, c.hc1), c.hc2);
            if (std::abs (p.bellDb) > 0.05f) x = bell.process (x, c.bell);
            return x;
        }
    };
}
