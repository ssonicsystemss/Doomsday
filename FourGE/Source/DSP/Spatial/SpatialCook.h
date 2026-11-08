#pragma once
// 4GE module 11 - parameters -> cooked spatial settings (per block; also re-cooked by module 09 with the tone group)
#include "Spatial.h"
#include "../../Parameters/ParamFrame.h"

namespace fge::dsp::spatial
{
    inline void cookBank (const ParamFrame& f, Bank& out, double rate)
    {
        const int stride = P::spcBodyOn - P::spcPunchOn;
        auto db = [] (float v) { return std::pow (10.f, v / 20.f); };
        const bool lock = f.on (P::spcSubLock);
        out.any = false;
        for (int t = 0; t < NumTargets; ++t)
        {
            auto at = [&] (int first) { return first + t * stride; };
            auto& s = out.t[(size_t) t];
            s = Settings();
            s.on = f.on (at (P::spcPunchOn));
            if (s.on)
            {
                s.swap = f.on (at (P::spcPunchSwap)); s.msOn = f.on (at (P::spcPunchMsOn));
                s.midSolo = f.on (at (P::spcPunchMidSo)); s.sideSolo = f.on (at (P::spcPunchSideSo));
                s.lowMono = f.on (at (P::spcPunchLoMono));
                s.mode = f.idx (at (P::spcPunchMode)); s.pol = f.idx (at (P::spcPunchPol)); s.haasCh = f.idx (at (P::spcPunchHCh));
                s.width = f[at (P::spcPunchWidth)]; s.mix = f[at (P::spcPunchMix)]; s.outG = db (f[at (P::spcPunchGain)]);
                s.midG = db (f[at (P::spcPunchMid)]); s.sideG = db (f[at (P::spcPunchSide)]); s.msWidth = f[at (P::spcPunchMsW)];
                const float pan = juce::jlimit (-1.f, 1.f, f[at (P::spcPunchPan)]);
                const float a = (pan + 1.f) * juce::MathConstants<float>::pi * 0.25f;
                s.panL = std::cos (a) * 1.41421356f; s.panR = std::sin (a) * 1.41421356f;
                const float bal = juce::jlimit (-1.f, 1.f, f[at (P::spcPunchBal)]);
                s.balL = bal > 0.f ? 1.f - bal : 1.f; s.balR = bal < 0.f ? 1.f + bal : 1.f;
                const float d = f[at (P::spcPunchDelay)];
                s.delayL = d < 0.f ? -d * 0.001f * (float) rate : 0.f; s.delayR = d > 0.f ? d * 0.001f * (float) rate : 0.f;
                s.haasSamples = f[at (P::spcPunchHTime)] * 0.001f * (float) rate;
                float xo = f[at (P::spcPunchXover)];
                if (t == TMaster && lock) { s.lowMono = true; xo = juce::jmax (xo, 120.f); }       // SUB MONO LOCK beats the global WIDTH
                tone::cookX (s.x, tone::XLr24, xo, rate);
            }
            if (t == TSub && lock) { if (! s.on) { s = Settings(); s.on = true; s.mode = MOff; } s.forceMono = true; }
            out.any |= s.on;
        }
    }
}
