#pragma once
// =====================================================================
//  4GE module 05 - parameters -> the cooked distortion bank (per block). The transfer tables live in DistTables
//  (sized once; rebuilt in place only when a curve setting changes - no allocation on the audio thread after prepare()).
// =====================================================================
#include "Distortion.h"
#include "../../Parameters/ParamFrame.h"

namespace fge::dsp::dist
{
    struct DistTables
    {
        std::array<Table, numStages> a, b, m;
        void allocate() { for (auto* arr : { &a, &b, &m }) for (auto& t : *arr) t.allocate(); }
    };

    inline void cookDist (const ParamFrame& f, const DistData* data, const env::Bank& eb, DistBank& out, DistTables& tb, double rate, bool offline)
    {
        static const DistData defaults;
        const DistData& d = data != nullptr ? *data : defaults;
        const int st = P::dist2On - P::dist1On;
        auto db = [] (float v) { return std::pow (10.f, v / 20.f); };
        out.any = false;
        for (auto& c : out.count) c = 0;
        struct Item { int order, idx; };
        std::array<std::array<Item, numStages>, NumLocs> items {};
        for (int i = 0; i < numStages; ++i)
        {
            auto at = [&] (int first) { return first + i * st; };
            auto& s = out.st[(size_t) i];
            s.on = f.on (at (P::dist1On));
            if (! s.on) continue;
            out.any = true;
            s.loc = f.idx (at (P::dist1Loc)); s.order = (int) std::lround (f[at (P::dist1Order)]);
            s.path = f.idx (at (P::dist1Path)); s.band = f.idx (at (P::dist1Band));
            s.type = f.idx (at (P::dist1Type)); s.morphType = f.idx (at (P::dist1MorphT)); s.morph = f[at (P::dist1Morph)];
            s.drive = f[at (P::dist1Drive)]; s.driveCurve = f.idx (at (P::dist1DCurve));
            s.inG = db (f[at (P::dist1In)]); s.outG = db (f[at (P::dist1Out)]); s.mix = f[at (P::dist1Mix)]; s.autoGain = f.on (at (P::dist1Auto));
            s.bias = f[at (P::dist1Bias)];
            s.g0 = s.inG * driveGain (s.drive, s.driveCurve);
            s.sm = 1.f - std::exp (-1.f / (0.002f * (float) rate));
            s.rDc = 1.f - (float) (juce::MathConstants<double>::twoPi * 10.0 / rate);
            auto& sp = s.sp;
            sp.type = s.type; sp.sym = f[at (P::dist1Sym)]; sp.thr = f[at (P::dist1Thr)]; sp.shape = f[at (P::dist1Shape)];
            sp.ceil = db (f[at (P::dist1Ceil)]); sp.even = f[at (P::dist1Even)]; sp.odd = f[at (P::dist1Odd)]; sp.stages = f[at (P::dist1Stages)];
            sp.negDrive = f[at (P::dist1NegDrv)]; sp.negThr = f[at (P::dist1NegThr)]; sp.rect = f.idx (at (P::dist1Rect)); sp.interp = f.idx (at (P::dist1Interp));
            sp.curve = &d.curve[(size_t) i]; sp.curveVer = d.curveVersion[(size_t) i];
            s.bits = f[at (P::dist1Bits)]; s.srr = f[at (P::dist1Srr)]; s.dither = f.idx (at (P::dist1Dither));
            s.os = f.idx (at (P::dist1Os));
            if (offline && f.idx (P::distOffline) == 1) s.os = juce::jmin (2, s.os + 1);           // OFFLINE QUALITY: one step more
            auto filt = [&] (FilterSet& fs, int on, int type, int cut, int res, int drv, int slope)
            { fs.on = f.on (at (on)); fs.type = f.idx (at (type)); fs.cut = f[at (cut)]; fs.reso = f[at (res)]; fs.drive = f[at (drv)]; fs.slope = f.idx (at (slope)); cookFilter (fs, rate); };
            filt (s.pre, P::dist1PreOn, P::dist1PreType, P::dist1PreCut, P::dist1PreRes, P::dist1PreDrv, P::dist1PreSlope); s.pre.mix = 1.f;
            filt (s.post, P::dist1PostOn, P::dist1PostType, P::dist1PostCut, P::dist1PostRes, P::dist1PostDrv, P::dist1PostSlope); s.post.mix = f[at (P::dist1PostMix)];
            s.modSrc = f.idx (at (P::dist1ModSrc)); s.modA = f[at (P::dist1ModA)]; s.modD = f[at (P::dist1ModD)]; s.drawn = &eb.s[(size_t) (env::ModDist1 + i)];
            s.mDrive = f[at (P::dist1MDrive)]; s.mBias = f[at (P::dist1MBias)]; s.mThr = f[at (P::dist1MThr)]; s.mFold = f[at (P::dist1MFold)];
            s.mMix = f[at (P::dist1MMix)]; s.mOut = f[at (P::dist1MOut)]; s.mCut = f[at (P::dist1MCut)];
            // the tables: the curve over the input range this stage can reach (drive incl. its modulation, the threshold modulation)
            const float maxDrive = juce::jlimit (0.f, 1.f, s.drive + std::abs (s.mDrive));
            const float R = juce::jlimit (2.f, 64.f, s.inG * driveGain (maxDrive, s.driveCurve) * (s.mThr != 0.f ? 4.f : 1.f) * 1.6f + 1.5f);
            if (s.type != Bitcrush)
            {
                tb.a[(size_t) i].build (sp, R); s.tA = &tb.a[(size_t) i];
                if (s.morph > 0.f && s.morphType != Bitcrush) { auto sb = sp; sb.type = s.morphType; tb.b[(size_t) i].build (sb, R); s.tB = &tb.b[(size_t) i]; }
                else s.tB = nullptr;
                if (s.mFold != 0.f)
                {
                    auto sm = sp;
                    sm.shape = juce::jlimit (0.f, 1.f, sp.shape + (s.mFold > 0.f ? 1.f : -1.f));
                    if (sp.type == Wavefold) sm.stages = juce::jlimit (1.f, 8.f, sp.stages + (s.mFold > 0.f ? 4.f : -4.f));
                    tb.m[(size_t) i].build (sm, R); s.tMod = &tb.m[(size_t) i];
                }
                else s.tMod = nullptr;
            }
            else { s.tA = s.tB = s.tMod = nullptr; s.sp.shape = f[at (P::dist1Shape)]; }
            auto& li = items[(size_t) juce::jlimit (0, NumLocs - 1, s.loc)];
            li[(size_t) out.count[(size_t) s.loc]++] = { s.order, i };
        }
        for (int l = 0; l < NumLocs; ++l)
        {
            auto& li = items[(size_t) l];
            const int n = out.count[(size_t) l];
            std::sort (li.begin(), li.begin() + n, [] (const Item& a, const Item& b) { return a.order != b.order ? a.order < b.order : a.idx < b.idx; });
            for (int k = 0; k < n; ++k) out.list[(size_t) l][(size_t) k] = li[(size_t) k].idx;
        }
        out.xLow = doomsday::dsp::svfCoefs (f[P::distXLow], 1.41421356f, rate);
        out.xHigh = doomsday::dsp::svfCoefs (juce::jmax (f[P::distXHigh], f[P::distXLow] * 1.2f), 1.41421356f, rate);
        out.bandSolo = f.idx (P::distBandSolo);
        out.globalPos = f.idx (P::distGlobPos);
    }
}
