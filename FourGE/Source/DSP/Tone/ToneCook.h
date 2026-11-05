#pragma once
// =====================================================================
//  4GE module 07 - parameters -> the cooked tone bank (per block, no allocation)
// =====================================================================
#include "Tone.h"
#include "../../Parameters/ParamFrame.h"

namespace fge::dsp::tone
{
    inline void cookBank (const ParamFrame& f, Bank& out, double rate)
    {
        auto db = [] (float v) { return std::pow (10.f, v / 20.f); };
        out.any = false;
        for (auto& c : out.eqCount) c = { 0, 0 };
        for (auto& c : out.deqCount) c = { 0, 0 };
        const int es = P::eq2On - P::eq1On, ds = P::deq2On - P::deq1On, ms = P::mb2Gain - P::mb1Gain;
        auto layerLoc = [] (int loc) { return loc >= m06::LLayer1 && loc < m06::LScreech; };
        for (int i = 0; i < numEq; ++i)
        {
            auto at = [&] (int first) { return first + i * es; };
            auto& e = out.eq[(size_t) i];
            e.on = f.on (at (P::eq1On));
            if (! e.on) continue;
            out.any = true;
            e.loc = juce::jlimit (0, NumLocs - 1, f.idx (at (P::eq1Loc))); e.pos = e.loc == m06::LScreech ? 0 : f.idx (at (P::eq1Pos));
            e.type = f.idx (at (P::eq1Type)); e.freq = f[at (P::eq1Freq)]; e.gain = f[at (P::eq1Gain)]; e.q = f[at (P::eq1Q)];
            e.slope = f.idx (at (P::eq1Slope)); e.mix = f[at (P::eq1Mix)]; e.stereo = layerLoc (e.loc) ? EsStereo : f.idx (at (P::eq1Stereo));
            auto& c = out.eqCount[(size_t) e.loc][(size_t) e.pos];
            out.eqList[(size_t) e.loc][(size_t) e.pos][(size_t) c++] = i;
        }
        for (int i = 0; i < numDeq; ++i)
        {
            auto at = [&] (int first) { return first + i * ds; };
            auto& d = out.deq[(size_t) i];
            d.on = f.on (at (P::deq1On));
            if (! d.on) continue;
            out.any = true;
            d.loc = juce::jlimit (0, NumLocs - 1, f.idx (at (P::deq1Loc))); d.pos = d.loc == m06::LScreech ? 0 : f.idx (at (P::deq1Pos));
            d.mode = f.idx (at (P::deq1Mode)); d.src = f.idx (at (P::deq1Src)); d.det = f.idx (at (P::deq1Det)); d.win = f[at (P::deq1Win)];
            d.freq = f[at (P::deq1Freq)]; d.q = f[at (P::deq1Q)]; d.thr = f[at (P::deq1Thr)]; d.range = f[at (P::deq1Range)]; d.ratio = f[at (P::deq1Ratio)];
            d.knee = f[at (P::deq1Knee)]; d.atk = f[at (P::deq1Atk)]; d.rel = f[at (P::deq1Rel)]; d.mix = f[at (P::deq1Mix)];
            auto& c = out.deqCount[(size_t) d.loc][(size_t) d.pos];
            out.deqList[(size_t) d.loc][(size_t) d.pos][(size_t) c++] = i;
        }
        // ---- multiband
        auto& m = out.mb;
        m.on = f.on (P::mbOn);
        if (m.on)
        {
            out.any = true;
            m.loc = juce::jlimit (0, NumLocs - 1, f.idx (P::mbLoc)); m.pos = m.loc == m06::LScreech ? 0 : f.idx (P::mbPos); m.listen = f.idx (P::mbListen);
            const float x1 = f[P::mbX1], x2 = juce::jmax (f[P::mbX2], x1 * 1.25f), x3 = juce::jmax (f[P::mbX3], x2 * 1.25f);
            const int xt = f.idx (P::mbXType);
            cookX (m.x[0], xt, x1, rate); cookX (m.x[1], xt, x2, rate); cookX (m.x[2], xt, x3, rate);
            m.anySolo = false;
            for (int k = 0; k < numMb; ++k)
            {
                auto at = [&] (int first) { return first + k * ms; };
                auto& b = m.b[(size_t) k];
                b.gain = db (f[at (P::mb1Gain)]); b.width = layerLoc (m.loc) ? 1.f : f[at (P::mb1Width)]; b.inv = f.on (at (P::mb1Inv)); b.mute = f.on (at (P::mb1Mute)); b.solo = f.on (at (P::mb1Solo));
                b.dyn = f.idx (at (P::mb1Dyn)); b.thr = f[at (P::mb1Thr)]; b.ratio = f[at (P::mb1Ratio)]; b.atk = f[at (P::mb1Atk)]; b.rel = f[at (P::mb1Rel)];
                b.knee = f[at (P::mb1Knee)]; b.makeup = db (f[at (P::mb1Makeup)]); b.mix = f[at (P::mb1Mix)]; b.trA = f[at (P::mb1TrA)]; b.trS = f[at (P::mb1TrS)];
                m.anySolo |= b.solo;
            }
        }
        // ---- spectral shaper
        auto& s = out.sp;
        s.on = f.on (P::spOn);
        if (s.on)
        {
            out.any = true;
            s.loc = juce::jlimit (0, NumLocs - 1, f.idx (P::spLoc)); s.pos = s.loc == m06::LScreech ? 0 : f.idx (P::spPos);
            s.tilt = f[P::spTilt]; s.pivot = f[P::spPivot]; s.tiltInt = f[P::spTiltInt]; s.tiltMix = f[P::spTiltMix];
            const double tdb = 12.0 * s.tilt * s.tiltInt;
            s.tLo = coef (SLowShelf, s.pivot, 0.5, -tdb, rate); s.tHi = coef (SHighShelf, s.pivot, 0.5, tdb, rate);
            s.hOn = f.on (P::spHOn); s.hEven = f[P::spHEven]; s.hOdd = f[P::spHOdd]; s.hLow = f[P::spHLow]; s.hHigh = f[P::spHHigh];
            s.hSplit = f[P::spHSplit]; s.hDrive = f[P::spHDrive]; s.hMix = f[P::spHMix]; s.hBal = f[P::spHBal];
            cookX (s.hX, XLr24, s.hSplit, rate);
            s.tone = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 2500.0 / rate));
            s.xOn = f.on (P::spXOn); s.xMode = f.idx (P::spXMode); s.xHarm = f.idx (P::spXHarm);
            s.xLo = f[P::spXLo]; s.xHi = juce::jmax (f[P::spXHi], s.xLo * 1.2f); s.xAmt = f[P::spXAmt]; s.xColor = f[P::spXColor]; s.xDrive = f[P::spXDrive]; s.xMix = f[P::spXMix];
            s.xHp = coef (SHp, s.xLo, 0.7071, 0, rate); s.xLp = coef (SLp, s.xHi, 0.7071, 0, rate); s.xHp2 = coef (SHp, s.xLo, 0.7071, 0, rate);
            s.cOn = f.on (P::spCOn); s.cSens = f[P::spCSens]; s.cThr = f[P::spCThr]; s.cRed = f[P::spCRed]; s.cAtk = f[P::spCAtk]; s.cRel = f[P::spCRel];
            s.cLo = f[P::spCLo]; s.cHi = juce::jmax (f[P::spCHi], s.cLo * 2.f); s.cSmooth = f[P::spCSmooth]; s.cMix = f[P::spCMix];
            s.fOn = f.on (P::spFOn); s.fCap = f[P::spFCap]; s.fDecay = f[P::spFDecay]; s.fSmooth = f[P::spFSmooth]; s.fPitch = f[P::spFPitch]; s.fMix = f[P::spFMix];
            if (s.cOn || s.fOn)
            {
                // the bands: log spaced over the range (compression) / over 60 Hz .. 16 kHz (freeze uses the compression range too)
                const double lo = s.cLo, hi = s.cHi, q = 2.0 + (1.0 - s.cSmooth) * 8.0;
                const double sh = std::exp2 (s.fPitch / 12.0);
                for (int k = 0; k < numSpecBands; ++k)
                {
                    const double hz = lo * std::pow (hi / lo, (k + 0.5) / numSpecBands);
                    s.band[(size_t) k] = coef (SBpUnity, hz, q, 0, rate);
                    s.synth[(size_t) k] = coef (SBpUnity, hz * sh, q, 0, rate);
                }
                s.aCo = 1.f - std::exp (-1.f / (0.001f * s.cAtk * (float) rate)); s.rCo = 1.f - std::exp (-1.f / (0.001f * s.cRel * (float) rate));
            }
        }
        // ---- stereo engine (after the place's distortion; layers are mono before their pan, so not offered)
        auto& w = out.stw;
        w.on = f.on (P::stwOn);
        if (w.on)
        {
            out.any = true;
            w.loc = stwLocs[juce::jlimit (0, 6, f.idx (P::stwLoc))];
            w.width = f[P::stwWidth]; w.lowW = f[P::stwLowW]; w.midW = f[P::stwMidW]; w.highW = f[P::stwHighW]; w.mix = f[P::stwMix]; w.monoListen = f.on (P::stwMonoLis);
            w.midG = { db (f[P::stwLowMid]), db (f[P::stwMidMid]), db (f[P::stwHiMid]) };
            w.sideG = { db (f[P::stwLowSide]), db (f[P::stwMidSide]), db (f[P::stwHiSide]) };
            w.midNeutral = f[P::stwLowMid] == 0.f && f[P::stwMidMid] == 0.f && f[P::stwHiMid] == 0.f;
            const float xl = f[P::stwXLow], xh = juce::jmax (f[P::stwXHigh], xl * 1.25f);
            cookX (w.xLow, XLr24, xl, rate); cookX (w.xHigh, XLr24, xh, rate); cookX (w.mono, XLr24, f[P::stwMono], rate);
        }
        out.order = f.idx (P::toneOrder);
    }
}
