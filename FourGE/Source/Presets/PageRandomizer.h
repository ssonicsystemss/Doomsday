#pragma once
// =====================================================================
//  4GE - PAGE RANDOMIZE (owner 2026-11-14): a dice on every effect page / tab - it randomizes only THAT page, with the one
//  RANDOMIZE STRENGTH (= VARIATION on the KICK VARIATION page, the state property vrAmount): 10 % = a small variation of
//  what you have, 100 % = something new. The STYLE stays: engine / model / mode switches are never touched; the DIST dice
//  may ADD a stage (a random effect, more often at a higher strength). The level is matched after (louder is not better).
//  One click = one UNDO step. Message thread only (tools, never the audio thread, never GUI code).
// =====================================================================
#include "VariantRandomizer.h"
#include "DistTools.h"

namespace fge::pagernd
{
    inline float strength (GeProcessor& p) { return juce::jlimit (0.05f, 1.f, (float) (double) p.apvts.state.getProperty ("vrAmount", 0.5)); }
    inline void setStrength (GeProcessor& p, float s) { p.apvts.state.setProperty ("vrAmount", juce::jlimit (0.05f, 1.f, s), nullptr); }

    // the switches that ARE the style / the routing - a page dice never flips them
    inline bool keep (const juce::String& id)
    {
        for (auto* k : { "engine", "punch_mode", "pvc_model", "_mute", "_solo", "_invert", "core_", "_location", "_order", "_on", "_bypass", "master_", "_link",
                         "_sync", "_note_mode", "style", "len_", "_channel", "_range" })
            if (id.contains (k)) return true;
        return false;
    }
    // a musical target inside the parameter's range (not the extremes)
    inline float target (int k, juce::Random& r)
    {
        const auto& d = def (k);
        const float u = r.nextFloat();
        auto lg = [u] (float lo, float hi) { lo = std::max (lo, 1.0e-3f); hi = std::max (hi, lo * 1.001f); return std::exp2 (std::log2 (lo) + u * (std::log2 (hi) - std::log2 (lo))); };
        switch (d.unit)
        {
            case Unit::Hz:   return lg (std::max (d.min, 40.f), std::min (d.max, 12000.f));
            case Unit::Ms:   return lg (std::max (d.min, 1.f), d.min + (d.max - d.min) * 0.6f);
            case Unit::Db:   return d.min * 0.5f + u * (d.max - d.min) * 0.5f;
            case Unit::Semi: return d.min * 0.5f + u * (d.max - d.min) * 0.5f;
            default:         return d.min + u * (d.max - d.min);
        }
    }
    inline void move (GeProcessor& p, const juce::String& id, float s, juce::Random& r)
    {
        const int k = fge::modstate::indexOf (id);
        if (k < 0 || keep (id)) return;
        const auto& d = def (k);
        const float cur = p.frame()[k];
        if (d.kind == Kind::Bool) return;
        if (d.kind == Kind::Choice)
        {
            if (r.nextFloat() < s * 0.5f) p.setParamInUndoStep (id, (float) r.nextInt ((int) d.choices().size()));
            return;
        }
        float v = cur + (target (k, r) - cur) * s;
        if (d.unit == Unit::None && d.max - d.min > 4.f) v = std::round (v);
        p.setParamInUndoStep (id, juce::jlimit (d.min, d.max, v));
    }
    inline void matchLevel (GeProcessor& p, double before)
    {
        const double after = fge::variant::kickRms (p);
        if (before > 1.0e-6 && after > 1.0e-6)
            p.setParamInUndoStep ("core_master", juce::jlimit (-24.f, 3.f, p.frame()[P::coreMaster] + (float) (20.0 * std::log10 (before / after))));
    }
    // ---- a page / tab: these parameter ids
    inline void randomizeIds (GeProcessor& p, const juce::StringArray& ids, const juce::String& what)
    {
        if (ids.isEmpty()) return;
        const float s = strength (p);
        juce::Random r ((juce::int64) juce::Time::getMillisecondCounterHiRes());
        p.undo.beginNewTransaction ("Randomize " + what);
        const double before = fge::variant::kickRms (p);
        for (auto& id : ids) move (p, id, s, r);
        matchLevel (p, before);
    }
    // ---- DIST: the stages of one location (type sometimes, every knob), and maybe ADD a random stage (a random effect)
    inline void randomizeDist (GeProcessor& p, int loc)
    {
        namespace dd = fge::dsp::dist;
        const float s = strength (p);
        juce::Random r ((juce::int64) juce::Time::getMillisecondCounterHiRes());
        p.undo.beginNewTransaction ("Randomize distortion");
        const double before = fge::variant::kickRms (p);
        auto c = fge::distt::chain (p, loc);
        if ((c.empty() || r.nextFloat() < s * 0.6f) && c.size() < 4)                    // ADD a random effect
            if (const int st = fge::distt::freeStage (p); st >= 0)
            {
                fge::distt::resetStage (p, st);
                p.setParamInUndoStep (fge::distt::sid (st, "on"), 1.f);
                p.setParamInUndoStep (fge::distt::sid (st, "location"), (float) loc);
                static const int nice[] { dd::Soft, dd::Hard, dd::Tube, dd::Diode, dd::Wavefold, dd::Asym, dd::Foldback, dd::Digital, dd::Bitcrush, dd::Rectifier };
                p.setParamInUndoStep (fge::distt::sid (st, "type"), (float) nice[r.nextInt (10)]);
                p.setParamInUndoStep (fge::distt::sid (st, "drive"), 0.15f + 0.35f * r.nextFloat());
                c.push_back (st); fge::distt::renumber (p, c);
            }
        for (int st : c)
        {
            const juce::String pre = fge::distt::sid (st, "");
            for (int k = 0; k < P::count; ++k)
            {
                const juce::String id (def (k).id);
                if (! id.startsWith (pre)) continue;
                if (id.endsWith ("_type")) { if (r.nextFloat() < s * 0.4f) p.setParamInUndoStep (id, (float) r.nextInt (dd::Custom)); continue; }   // (no CUSTOM: it needs a drawn curve)
                if (id.endsWith ("_drive")) { const float cur = p.frame()[k]; p.setParamInUndoStep (id, cur + (0.1f + 0.6f * r.nextFloat() - cur) * s); continue; }
                move (p, id, s, r);
            }
        }
        matchLevel (p, before);
    }
}
