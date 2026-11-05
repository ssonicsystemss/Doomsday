#pragma once
// =====================================================================
//  4GE module 08 - parameters -> the cooked dynamics bank (per block, no allocation)
// =====================================================================
#include "Dyn.h"
#include "../../Parameters/ParamFrame.h"

namespace fge::dsp::dyn
{
    // engineRate = the oversampled rate of the places; hostRate = MASTER OUTPUT
    inline void cookBank (const ParamFrame& f, const dist::DistData* dd, Bank& out, double engineRate, double hostRate)
    {
        auto lin = [] (float v) { return v <= -59.9f ? 0.f : std::pow (10.f, v / 20.f); };
        for (auto& m : out.mask) m = { 0, 0 };
        out.any = false; out.masterLook = 0;
        auto place = [&] (int kind, int i, int loc, int pos) { out.mask[(size_t) loc][(size_t) (loc == LMaster || loc == m06::LScreech ? (loc == LMaster ? 1 : 0) : pos)] |= 1ull << (kind * 8 + i); out.any = true; };
        auto rateOf = [&] (int loc) { return loc == LMaster ? hostRate : engineRate; };
        const int cs = P::cmp2On - P::cmp1On, ts = P::ts2On - P::ts1On, ls = P::clp2On - P::clp1On, ms = P::lim2On - P::lim1On;
        for (int i = 0; i < numCmp; ++i)
        {
            auto at = [&] (int first) { return first + i * cs; };
            auto& s = out.cmp[(size_t) i];
            s.on = f.on (at (P::cmp1On));
            if (! s.on) continue;
            s.loc = juce::jlimit (0, NumLocs - 1, f.idx (at (P::cmp1Loc))); s.pos = f.idx (at (P::cmp1Pos));
            const double rate = rateOf (s.loc);
            s.type = f.idx (at (P::cmp1Type)); s.det = f.idx (at (P::cmp1Det)); s.arel = f.idx (at (P::cmp1ARel));
            s.thr = f[at (P::cmp1Thr)]; s.ratio = f[at (P::cmp1Ratio)]; s.atk = f[at (P::cmp1Atk)]; s.rel = f[at (P::cmp1Rel)]; s.knee = f[at (P::cmp1Knee)];
            s.inG = lin (f[at (P::cmp1In)]); s.outG = lin (f[at (P::cmp1Out)]); s.mix = f[at (P::cmp1Mix)];
            s.dry = lin (f[at (P::cmp1Dry)]); s.wet = lin (f[at (P::cmp1Wet)]); s.link = f[at (P::cmp1Link)];
            s.sc = (s.loc >= m06::LLayer1 && s.loc < m06::LScreech) || s.loc == LMaster ? 0 : f.idx (at (P::cmp1ScSrc));   // (layers / master: own signal only)
            if (s.type == Sidechain && s.sc == 0) s.sc = 1;                                                               // SIDECHAIN without a source: the PUNCH
            s.scGain = lin (f[at (P::cmp1ScGain)]); s.scMix = f[at (P::cmp1ScMix)];
            const float hpHz = f[at (P::cmp1ScHp)], lpHz = f[at (P::cmp1ScLp)];
            s.hp = hpHz > 20.5f; s.lp = lpHz < 19500.f;
            s.scHp = tone::coef (tone::SHp, hpHz, 0.7071, 0, rate); s.scLp = tone::coef (tone::SLp, lpHz, 0.7071, 0, rate);
            s.aCo = coefMs (s.atk, rate); s.rCo = coefMs (s.rel, rate); s.rmsCo = coefMs (10.f, rate);
            s.look = s.loc == LMaster ? juce::jlimit (0, maxLook - 1, (int) std::lround (f[at (P::cmp1Look)] * 0.001 * hostRate)) : 0;
            out.masterLook += s.look;
            place (KCmp, i, s.loc, s.pos);
        }
        for (int i = 0; i < numTs; ++i)
        {
            auto at = [&] (int first) { return first + i * ts; };
            auto& s = out.ts[(size_t) i];
            s.on = f.on (at (P::ts1On));
            if (! s.on) continue;
            s.loc = juce::jlimit (0, NumLocs - 1, f.idx (at (P::ts1Loc))); s.pos = f.idx (at (P::ts1Pos));
            const double rate = rateOf (s.loc);
            s.det = f.idx (at (P::ts1Det)); s.mode = f.idx (at (P::ts1Mode));
            s.atk = f[at (P::ts1Atk)]; s.sus = f[at (P::ts1Sus)]; s.sens = f[at (P::ts1Sens)]; s.time = f[at (P::ts1Time)]; s.rel = f[at (P::ts1Rel)];
            s.mix = f[at (P::ts1Mix)]; s.outG = lin (f[at (P::ts1Out)]);
            // the two envelopes: FAST follows the attack, SLOW the body (detection = their speeds)
            const float k = s.det == TdSlow ? 4.f : s.det == TdPeak ? 0.25f : 1.f;
            s.fA = s.det == TdPeak ? 1.f : coefMs (0.3f * k, rate); s.fR = coefMs (s.time * 0.5f * k, rate);
            s.sA = coefMs (s.time * k, rate); s.sR = coefMs (s.rel * k, rate);
            if (s.det == TdHybrid) { s.fA = coefMs (0.15f, rate); s.sA = coefMs (s.time * 1.5f, rate); }
            s.gR = coefMs (s.rel * 0.25f, rate);
            place (KTs, i, s.loc, s.pos);
        }
        for (int i = 0; i < numClp; ++i)
        {
            auto at = [&] (int first) { return first + i * ls; };
            auto& s = out.clp[(size_t) i];
            s.on = f.on (at (P::clp1On));
            if (! s.on) continue;
            s.loc = juce::jlimit (0, NumLocs - 1, f.idx (at (P::clp1Loc))); s.pos = f.idx (at (P::clp1Pos));
            s.type = f.idx (at (P::clp1Type)); s.inG = lin (f[at (P::clp1In)]); s.ceil = juce::jmax (0.06f, lin (f[at (P::clp1Ceil)]));
            s.drive = f[at (P::clp1Drive)]; s.sym = f[at (P::clp1Sym)]; s.soft = f[at (P::clp1Soft)]; s.mix = f[at (P::clp1Mix)]; s.outG = lin (f[at (P::clp1Out)]);
            s.os = f.idx (at (P::clp1Os));
            static const dist::DistData defaults;
            s.curve = &(dd != nullptr ? *dd : defaults).clip[(size_t) i];
            place (KClp, i, s.loc, s.pos);
        }
        for (int i = 0; i < numLim; ++i)
        {
            auto at = [&] (int first) { return first + i * ms; };
            auto& s = out.lim[(size_t) i];
            s.on = f.on (at (P::lim1On));
            if (! s.on) continue;
            s.loc = juce::jlimit (0, NumLocs - 1, f.idx (at (P::lim1Loc))); s.pos = f.idx (at (P::lim1Pos));
            const double rate = rateOf (s.loc);
            s.mode = f.idx (at (P::lim1Mode)); s.ceil = lin (f[at (P::lim1Ceil)]); s.inG = lin (f[at (P::lim1In)]);
            s.rel = f[at (P::lim1Rel)]; s.atk = f[at (P::lim1Atk)]; s.knee = f[at (P::lim1Knee)]; s.link = f[at (P::lim1Link)]; s.mix = f[at (P::lim1Mix)];
            s.tp = f.on (at (P::lim1Tp)); s.os = f.idx (at (P::lim1Os));
            s.rCo = coefMs (s.mode == LAggressive ? s.rel * 0.5f : s.rel, rate); s.aCo = s.atk <= 0.f ? 1.f : coefMs (s.atk, rate);
            s.look = s.loc == LMaster ? juce::jlimit (0, maxLook - 1, (int) std::lround ((s.mode == LTrue ? juce::jmax (1.5f, f[at (P::lim1Look)]) : f[at (P::lim1Look)]) * 0.001 * hostRate)) : 0;
            out.masterLook += s.look;
            place (KLim, i, s.loc, s.pos);
        }
        auto& c = out.crest;
        c.on = f.on (P::crOn);
        if (c.on)
        {
            c.loc = juce::jlimit (0, NumLocs - 1, f.idx (P::crLoc)); c.pos = f.idx (P::crPos);
            const double rate = rateOf (c.loc);
            c.target = f[P::crTarget]; c.punch = f[P::crPunch]; c.body = f[P::crBody]; c.tail = f[P::crTail]; c.sens = f[P::crSens]; c.mix = f[P::crMix];
            c.pkR = coefMs (80.f, rate); c.rmsCo = coefMs (60.f, rate);
            place (KCrest, 0, c.loc, c.pos);
        }
        out.autoGain = f.idx (P::dynAuto); out.match = f.on (P::dynMatch); out.order = f.idx (P::dynOrder);
    }
}
