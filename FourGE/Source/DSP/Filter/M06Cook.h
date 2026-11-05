#pragma once
// =====================================================================
//  4GE module 06 - parameters -> the cooked filter / resonator bank and the screech settings (per block, no allocation)
// =====================================================================
#include "Screech.h"
#include "../../Parameters/ParamFrame.h"

namespace fge::dsp::m06
{
    inline void cookBank (const ParamFrame& f, const env::Bank& eb, Bank& out, double rate)
    {
        const int st = P::flt2On - P::flt1On, rs = P::res2On - P::res1On;
        out.any = false;
        for (auto& l : out.count) l = { 0, 0 };
        for (auto& l : out.rcount) l = { 0, 0 };
        struct Item { float order; int idx; };
        std::array<std::array<std::array<Item, numFilters>, 2>, NumLocs> items {};
        for (int i = 0; i < numFilters; ++i)
        {
            auto at = [&] (int first) { return first + i * st; };
            auto& s = out.f[(size_t) i];
            s.on = f.on (at (P::flt1On));
            if (! s.on) continue;
            out.any = true;
            s.loc = juce::jlimit (0, NumLocs - 1, f.idx (at (P::flt1Loc))); s.pos = f.idx (at (P::flt1Pos)); s.order = (int) std::lround (f[at (P::flt1Order)]);
            s.path = f.idx (at (P::flt1Path)); s.band = f.idx (at (P::flt1Band)); s.type = f.idx (at (P::flt1Type));
            s.cut = f[at (P::flt1Cut)]; s.res = f[at (P::flt1Res)]; s.drive = f[at (P::flt1Drive)]; s.mix = f[at (P::flt1Mix)];
            s.slope = slopeWorks (s.type) ? f.idx (at (P::flt1Slope)) : 1;
            s.key = f[at (P::flt1Key)]; s.env = f[at (P::flt1EnvAmt)]; s.vel = f[at (P::flt1Vel)]; s.stereo = f.idx (at (P::flt1Stereo));
            s.svfMode = f[at (P::flt1SvfMode)]; s.vowel = f[at (P::flt1Vowel)];
            s.e.a = f[at (P::flt1EnvA)]; s.e.d = f[at (P::flt1EnvD)]; s.e.s = f[at (P::flt1EnvS)]; s.e.r = f[at (P::flt1EnvR)];
            s.e.curve = f.idx (at (P::flt1EnvCurve)); s.e.inv = f.on (at (P::flt1EnvInv)); s.e.sync = f.on (at (P::flt1EnvSync));
            s.envRes = f[at (P::flt1EnvRes)]; s.envDrv = f[at (P::flt1EnvDrv)];
            if (s.loc >= LLayer1 && s.loc < LScreech) s.stereo = StStereo;          // (a layer is mono before its pan)
            if (s.loc == LScreech) s.pos = 0;
            auto& c = out.count[(size_t) s.loc][(size_t) s.pos];
            items[(size_t) s.loc][(size_t) s.pos][(size_t) c++] = { (float) s.order, i };
        }
        for (int l = 0; l < NumLocs; ++l)
            for (int p = 0; p < 2; ++p)
            {
                auto& li = items[(size_t) l][(size_t) p];
                const int n = out.count[(size_t) l][(size_t) p];
                std::sort (li.begin(), li.begin() + n, [] (const Item& a, const Item& b) { return a.order != b.order ? a.order < b.order : a.idx < b.idx; });
                for (int k = 0; k < n; ++k) out.list[(size_t) l][(size_t) p][(size_t) k] = li[(size_t) k].idx;
            }
        for (int i = 0; i < numRes; ++i)
        {
            auto at = [&] (int first) { return first + i * rs; };
            auto& r = out.r[(size_t) i];
            r.on = f.on (at (P::res1On));
            if (! r.on) continue;
            out.any = true;
            r.loc = juce::jlimit (0, NumLocs - 1, f.idx (at (P::res1Loc))); r.pos = r.loc == LScreech ? 0 : f.idx (at (P::res1Pos));
            r.mode = f.idx (at (P::res1Mode)); r.src = f.idx (at (P::res1Src));
            r.freq = f[at (P::res1Freq)]; r.q = f[at (P::res1Q)]; r.decay = f[at (P::res1Decay)]; r.amount = f[at (P::res1Amount)];
            r.damp = f[at (P::res1Damp)]; r.exc = f[at (P::res1Exc)]; r.fb = f[at (P::res1Fb)]; r.track = f[at (P::res1Track)]; r.mix = f[at (P::res1Mix)];
            r.pAmt = f[at (P::res1PAmt)]; r.pStart = f[at (P::res1PStart)]; r.pEnd = f[at (P::res1PEnd)];
            r.pe.a = f[at (P::res1PA)]; r.pe.d = f[at (P::res1PD)]; r.pe.s = f[at (P::res1PS)]; r.pe.r = f[at (P::res1PR)]; r.pe.curve = f.idx (at (P::res1PCurve));
            r.custom = &eb.s[(size_t) (env::ResCustom1 + i)];
            auto& c = out.rcount[(size_t) r.loc][(size_t) r.pos];
            out.rlist[(size_t) r.loc][(size_t) r.pos][(size_t) c++] = i;
        }
        out.xLow = doomsday::dsp::svfCoefs (f[P::fltXLow], 1.41421356f, rate);
        out.xHigh = doomsday::dsp::svfCoefs (juce::jmax (f[P::fltXHigh], f[P::fltXLow] * 1.2f), 1.41421356f, rate);
    }
}

namespace fge::dsp::scr
{
    inline void cook (const ParamFrame& f, Settings& s)
    {
        auto db = [] (float v) { return v <= -59.9f ? 0.f : std::pow (10.f, v / 20.f); };
        s.on = f.on (P::scrOn);
        if (! s.on) return;
        s.mode = f.idx (P::scrMode); s.level = db (f[P::scrLevel]); s.output = f.idx (P::scrOutput); s.width = f[P::scrWidth];
        s.pitchMode = f.idx (P::scrPitchMode); s.baseHz = f[P::scrBaseHz];
        s.fmRatio = f[P::scrFmRatio]; s.fmIndex = f[P::scrFmIndex]; s.fmFb = f[P::scrFmFb]; s.metal = f[P::scrMetal]; s.metalQ = f[P::scrMetalQ];
        s.noiseCol = f[P::scrNoiseCol]; s.noiseQ = f[P::scrNoiseQ];
        s.vowel = f[P::scrVowel]; s.vowelEnd = f[P::scrVowelEnd]; s.vowelAuto = f.on (P::scrVowelAuto); s.fmtWidth = f[P::scrFmtWidth]; s.fmtReso = f[P::scrFmtReso];
        s.fbAmt = f[P::scrFbAmt]; s.fbTone = f[P::scrFbTone]; s.fbFilter = f.idx (P::scrFbFilter); s.fbDelay = f[P::scrFbDelay];
        s.fbDrive = f[P::scrFbDrive]; s.fbMix = f[P::scrFbMix]; s.fbStab = f[P::scrFbStab];
        s.custom = { f[P::scrMixTonal], f[P::scrMixMetal], f[P::scrMixFmt], f[P::scrMixFm], f[P::scrMixNoise], f[P::scrMixFb] };
        s.a = f[P::scrAtk]; s.hold = f[P::scrHold]; s.d = f[P::scrDec]; s.s = f[P::scrSus]; s.r = f[P::scrRel]; s.curve = f.idx (P::scrCurve);
        s.modA = f[P::scrModA]; s.modD = f[P::scrModD]; s.modCurve = f.idx (P::scrModCurve);
        s.mPitch = f[P::scrMPitch]; s.mFm = f[P::scrMFm]; s.mCut = f[P::scrMCut]; s.mRes = f[P::scrMRes]; s.mFb = f[P::scrMFb];
        s.mFmt = f[P::scrMFmt]; s.mDrive = f[P::scrMDrive]; s.mDetune = f[P::scrMDetune];
        s.pStart = f[P::scrPStart]; s.pEnd = f[P::scrPEnd]; s.pAmt = f[P::scrPAmt]; s.pA = f[P::scrPA]; s.pD = f[P::scrPD]; s.pR = f[P::scrPR]; s.pCurve = f.idx (P::scrPCurve);
        s.startMode = f.idx (P::scrStartMode); s.start = f[P::scrStart]; s.offset = f[P::scrOffset]; s.length = f[P::scrLength];
        s.fadeIn = f[P::scrFadeIn]; s.fadeOut = f[P::scrFadeOut]; s.preTrig = f.on (P::scrPreTrig); s.syncTail = f.on (P::scrSyncTail);
        s.autoLen = f.on (P::scrAutoLen); s.quant = f.idx (P::scrQuant); s.followBody = f.on (P::scrFollowBody); s.freeTail = f.on (P::scrFree);
        const int os = P::so2On - P::so1On;
        for (int i = 0; i < 4; ++i)
        {
            auto at = [&] (int first) { return first + i * os; };
            auto& o = s.osc[(size_t) i];
            o.on = f.on (at (P::so1On)); o.wave = f.idx (at (P::so1Wave)); o.level = f[at (P::so1Level)]; o.pitch = f[at (P::so1Pitch)]; o.fine = f[at (P::so1Fine)];
            o.phase01 = f[at (P::so1Phase)] / 360.f; o.detune = f[at (P::so1Detune)]; o.pw = f[at (P::so1Pw)];
            o.fmSrc = f.idx (at (P::so1FmSrc)); o.fm = f[at (P::so1Fm)]; o.ringSrc = f.idx (at (P::so1RingSrc)); o.ring = f[at (P::so1Ring)]; o.pan = f[at (P::so1Pan)];
            if (o.fmSrc == i + 1) o.fmSrc = 0;                                       // (not from itself)
            if (o.ringSrc == i + 1) o.ringSrc = 0;
        }
        const int fs = P::sf2On - P::sf1On;
        for (int k = 0; k < 2; ++k)
        {
            auto at = [&] (int first) { return first + k * fs; };
            auto& x = s.flt[(size_t) k];
            x.on = f.on (at (P::sf1On)); x.type = f.idx (at (P::sf1Type)); x.cut = f[at (P::sf1Cut)]; x.res = f[at (P::sf1Res)];
            x.drive = f[at (P::sf1Drive)]; x.key = f[at (P::sf1Key)]; x.env = f[at (P::sf1Env)]; x.mix = f[at (P::sf1Mix)];
            auto& m = s.fs[(size_t) k];
            m.on = x.on; m.type = x.type; m.cut = x.cut; m.res = x.res; m.drive = x.drive; m.key = x.key; m.mix = x.mix; m.slope = 1; m.stereo = m06::StStereo;
            m.env = juce::jlimit (-1.f, 1.f, x.env + s.mCut); m.envRes = s.mRes; m.envDrv = s.mDrive;      // the MOD envelope moves the filters
            m.e.a = s.modA; m.e.d = s.modD; m.e.s = 0.f; m.e.r = 1.f; m.e.curve = s.modCurve; m.e.inv = false; m.e.sync = false;
            m.vowel = 1.f; m.svfMode = 0.5f;
        }
    }
}
