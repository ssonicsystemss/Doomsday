#pragma once
// =====================================================================
//  ABYSS OF ECHOES - mix reverb / delay / stereo
//
//   in -> DELAY (stereo / ping-pong / wide, sync, filters, drive, wobble) -----------+
//      -> REVERB (pre-delay, 8-line FDN, damping, low cut, mod, SHIMMER, FREEZE) <---+ (D > R: delay into reverb)
//   wet = (delay * duck + reverb * duck) * MIX ;  out = dry (DRY on) + wet
//   -> STEREO: WIDTH (M/S), SPREAD (decorrelated width), MONO BASS -> OUTPUT
//  Blocks live in Shared (DoomsdaySpace.h, DoomsdayDSP.h); this file only wires them.
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include "DoomsdaySpace.h"
#include "DoomsdayDSP.h"
#include <atomic>

struct EchoParams
{
    bool  dlyOn = true;
    doomsday::space::EchoParams dly;
    float dlyDuck = 0.f, dlyMix = 0.25f;
    bool  revOn = true;
    doomsday::space::ReverbParams rev;
    float revDuck = 0.f, revMix = 0.25f, d2r = 0.f;
    float width = 1.f, spread = 0.f, monoBassHz = 0.f, mix = 1.f, outDb = 0.f;
    bool  dry = true;
};

class EchoEngine
{
public:
    void prepare (double sampleRate, int)
    {
        sr = sampleRate;
        delay.prepare (sr); reverb.prepare (sr); spread.prepare (sr);
        duckD.prepare (sr); duckR.prepare (sr);
        for (auto* s : { &sDly, &sRev, &sMix, &sWidth, &sOut, &sDry, &sD2r }) s->reset (sr, 0.03);
        sDly.setCurrentAndTargetValue (p.dlyOn ? p.dlyMix : 0.f);
        sRev.setCurrentAndTargetValue (p.revOn ? p.revMix : 0.f);
        sMix.setCurrentAndTargetValue (p.mix); sWidth.setCurrentAndTargetValue (p.width);
        sOut.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.outDb)); sDry.setCurrentAndTargetValue (p.dry ? 1.f : 0.f);
        sD2r.setCurrentAndTargetValue (p.d2r);
        mono = {};
    }
    void reset() { delay.reset(); reverb.reset(); spread.reset(); mono = {}; }
    void setParams (const EchoParams& np) { p = np; }

    void process (float* L, float* R, int n)
    {
        sDly.setTargetValue (p.dlyOn ? p.dlyMix : 0.f);
        sRev.setTargetValue (p.revOn ? p.revMix : 0.f);
        sMix.setTargetValue (p.mix); sWidth.setTargetValue (p.width);
        sOut.setTargetValue (juce::Decibels::decibelsToGain (p.outDb)); sDry.setTargetValue (p.dry ? 1.f : 0.f);
        sD2r.setTargetValue (p.d2r);
        float pkD = 0.f, pkR = 0.f, pkO = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];
            const float dm = sDly.getNextValue(), rm = sRev.getNextValue(), d2r = sD2r.getNextValue();
            float dL = 0.f, dR = 0.f, rL = 0.f, rR = 0.f;
            // the delay keeps running while switched off, so its tail fades out with the smoother
            if (p.dlyOn || dm > 0.0001f) delay.process (inL, inR, dL, dR, p.dly);
            if (p.revOn || rm > 0.0001f) reverb.process (inL + dL * d2r, inR + dR * d2r, rL, rR, p.rev);
            const float gD = duckD.gain (inL, inR, p.dlyDuck), gR = duckR.gain (inL, inR, p.revDuck);
            const float mx = sMix.getNextValue();
            const float wL = (dL * dm * gD + rL * rm * gR) * mx, wR = (dR * dm * gD + rR * rm * gR) * mx;
            pkD = juce::jmax (pkD, std::abs (dL * dm), std::abs (dR * dm));
            pkR = juce::jmax (pkR, std::abs (rL * rm), std::abs (rR * rm));
            const float dryG = sDry.getNextValue();
            float l = inL * dryG + wL, r = inR * dryG + wR;
            // stereo
            const float w = sWidth.getNextValue();
            const float m = 0.5f * (l + r), s = 0.5f * (l - r) * w;
            l = m + s; r = m - s;
            spread.process (l, r, p.spread);
            mono.process (l, r, p.monoBassHz, sr);
            const float og = sOut.getNextValue();
            l *= og; r *= og;
            L[i] = l; R[i] = r;
            pkO = juce::jmax (pkO, std::abs (l), std::abs (r));
            // stereo field feed for the GUI (every 2nd sample)
            if ((i & 1) == 0)
            {
                const int wpos = scopeWrite.load (std::memory_order_relaxed);
                scopeL[(size_t) wpos] = l; scopeR[(size_t) wpos] = r;
                scopeWrite.store ((wpos + 1) & (scopeSize - 1), std::memory_order_release);
            }
        }
        delayPeak.store (juce::jmax (delayPeak.load(), pkD));
        reverbPeak.store (juce::jmax (reverbPeak.load(), pkR));
        outPeak.store (juce::jmax (outPeak.load(), pkO));
    }

    static constexpr int scopeSize = 2048;
    std::array<float, scopeSize> scopeL {}, scopeR {};
    std::atomic<int> scopeWrite { 0 };
    std::atomic<float> delayPeak { 0.f }, reverbPeak { 0.f }, outPeak { 0.f };

private:
    double sr = 44100.0;
    EchoParams p;
    doomsday::space::EchoDelay delay;
    doomsday::space::FdnReverb reverb;
    doomsday::space::Ducker duckD, duckR;
    doomsday::space::MonoBass mono;
    doomsday::dsp::StereoSpread spread;
    juce::SmoothedValue<float> sDly, sRev, sMix, sWidth, sOut, sDry, sD2r;
};
