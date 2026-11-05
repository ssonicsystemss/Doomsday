#pragma once
// =====================================================================
//  4GE module 07 - EQ / MULTIBAND / SPECTRAL / STEREO tools (never in the GUI, never on the audio thread):
//   factory presets (EARLY / MODERN / RAWSTYLE / EXPERIMENTAL - the whole module), files (.4geq one EQ band, .4gm07 the module),
//   COPY / PASTE / DUPLICATE / RESET of an EQ band, A / B COMPARE, RANDOMIZE with LOCK per engine - every action one UNDO step.
// =====================================================================
#include "M06Tools.h"

namespace fge::m07t
{
    enum Engine { EEq = 0, EDyn, EMulti, ESpectral, EStereo, NumEngines };
    inline juce::String eid (int i, const juce::String& k) { return "eq" + juce::String (i + 1) + "_" + k; }
    inline juce::String did (int i, const juce::String& k) { return "deq" + juce::String (i + 1) + "_" + k; }
    inline juce::StringArray idsOf (const juce::String& prefix) { return layers::prefixedIds (prefix); }
    inline juce::StringArray engineIds (int e)
    {
        juce::StringArray s;
        if (e == EEq) for (int i = 0; i < 16; ++i) s.addArray (idsOf (eid (i, "")));
        else if (e == EDyn) for (int i = 0; i < 4; ++i) s.addArray (idsOf (did (i, "")));
        else if (e == EMulti) { s.addArray (idsOf ("mb_")); for (int i = 1; i <= 4; ++i) s.addArray (idsOf ("mb" + juce::String (i) + "_")); }
        else if (e == ESpectral) { s.addArray (idsOf ("sp_")); s.add ("tone_order"); }
        else s.addArray (idsOf ("stw_"));
        return s;
    }
    inline juce::StringArray moduleIds() { juce::StringArray s; for (int e = 0; e < NumEngines; ++e) s.addArray (engineIds (e)); return s; }
    inline float value (GeProcessor& p, const juce::String& id) { return layers::value (p, id); }

    // ---- bands
    inline std::unique_ptr<juce::XmlElement> bandToXml (GeProcessor& p, int i) { return m06t::toXml (p, "EQBAND4GE", idsOf (eid (i, "")), eid (i, "")); }
    inline void pasteBand (GeProcessor& p, int i, const juce::XmlElement& x, bool newStep = true)
    {
        if (newStep) p.undo.beginNewTransaction ("Paste EQ band");
        m06t::fromXml (p, x, eid (i, ""));
    }
    inline int freeBand (GeProcessor& p) { for (int i = 0; i < 16; ++i) if (value (p, eid (i, "on")) < 0.5f) return i; return -1; }
    inline int addBand (GeProcessor& p, float hz, float gainDb, int loc)
    {
        const int i = freeBand (p); if (i < 0) return -1;
        p.undo.beginNewTransaction ("Add EQ band");
        m06t::reset (p, idsOf (eid (i, "")));
        p.setParamInUndoStep (eid (i, "on"), 1.f); p.setParamInUndoStep (eid (i, "freq"), hz); p.setParamInUndoStep (eid (i, "gain"), gainDb);
        p.setParamInUndoStep (eid (i, "location"), (float) loc);
        return i;
    }
    inline int duplicateBand (GeProcessor& p, int i)
    {
        const int to = freeBand (p); if (to < 0) return -1;
        p.undo.beginNewTransaction ("Duplicate EQ band");
        auto x = bandToXml (p, i); pasteBand (p, to, *x, false);
        return to;
    }
    inline void resetBand (GeProcessor& p, int i) { p.undo.beginNewTransaction ("Delete EQ band"); m06t::reset (p, idsOf (eid (i, ""))); }

    // ---- files
    inline std::unique_ptr<juce::XmlElement> moduleToXml (GeProcessor& p) { return m06t::toXml (p, "M074GE", moduleIds()); }
    inline bool load (GeProcessor& p, const juce::XmlElement& x, int bandSel)
    {
        if (x.hasTagName ("EQBAND4GE")) { pasteBand (p, bandSel, x); return true; }
        if (x.hasTagName ("M074GE")) { p.undo.beginNewTransaction ("Load module 07"); m06t::reset (p, moduleIds()); m06t::fromXml (p, x); return true; }
        return false;
    }
    inline const char* extensionOf (const juce::String& tag) { return tag == "EQBAND4GE" ? ".4geq" : ".4gm07"; }

    // ---- A / B
    struct Compare
    {
        std::unique_ptr<juce::XmlElement> a, b; bool onB = false;
        void toggle (GeProcessor& p)
        {
            auto now = moduleToXml (p);
            auto& other = onB ? a : b;
            (onB ? b : a) = std::move (now);
            onB = ! onB;
            if (other) { p.undo.beginNewTransaction ("A / B"); m06t::fromXml (p, *other); }
        }
    };

    // ---- factory presets (the whole module)
    struct Preset { const char* category; const char* name; std::vector<std::pair<const char*, float>> v; };
    inline const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> l {
            { "EARLY", "CLASSIC WARMTH",     { { "eq1_on", 1 }, { "eq1_type", 1 }, { "eq1_freq", 90 }, { "eq1_gain", 2.5f }, { "eq2_on", 1 }, { "eq2_freq", 450 }, { "eq2_gain", 2 }, { "eq2_q", 0.8f },
                                               { "eq3_on", 1 }, { "eq3_type", 4 }, { "eq3_freq", 11000 }, { "eq3_slope", 1 }, { "sp_on", 1 }, { "sp_tilt", -0.25f }, { "sp_tilt_pivot", 1500 },
                                               { "sp_harm_on", 1 }, { "sp_harm_even", 0.7f }, { "sp_harm_odd", 0.2f }, { "sp_harm_mix", 0.3f } } },
            { "EARLY", "OLDSCHOOL PUNCH",    { { "eq1_on", 1 }, { "eq1_freq", 120 }, { "eq1_gain", 3 }, { "eq1_q", 1.2f }, { "eq2_on", 1 }, { "eq2_freq", 2500 }, { "eq2_gain", 2.5f },
                                               { "mb_on", 1 }, { "mb2_dynamics", 5 }, { "mb2_tr_attack", 5 }, { "mb4_dynamics", 5 }, { "mb4_tr_attack", 4 } } },
            { "MODERN", "MODERN BALANCE",    { { "eq1_on", 1 }, { "eq1_type", 3 }, { "eq1_freq", 25 }, { "eq1_slope", 2 }, { "eq2_on", 1 }, { "eq2_freq", 300 }, { "eq2_gain", -2 }, { "eq2_q", 1.4f },
                                               { "deq1_on", 1 }, { "deq1_freq", 3500 }, { "deq1_threshold", -30 }, { "deq1_range", 6 },
                                               { "mb_on", 1 }, { "mb1_dynamics", 1 }, { "mb1_threshold", -14 }, { "mb1_ratio", 3 }, { "stw_on", 1 }, { "stw_high_width", 1.3f } } },
            { "MODERN", "BRIGHT FESTIVAL",   { { "eq1_on", 1 }, { "eq1_type", 2 }, { "eq1_freq", 6000 }, { "eq1_gain", 3 }, { "sp_on", 1 }, { "sp_exc_on", 1 }, { "sp_exc_mode", 1 },
                                               { "sp_exc_low", 3000 }, { "sp_exc_amount", 0.4f }, { "stw_on", 1 }, { "stw_high_width", 1.5f }, { "stw_mid_width", 1.2f } } },
            { "RAWSTYLE", "RAW AGGRESSION",  { { "eq1_on", 1 }, { "eq1_freq", 900 }, { "eq1_gain", 3 }, { "eq2_on", 1 }, { "eq2_freq", 3200 }, { "eq2_gain", 3 }, { "eq2_q", 1.5f },
                                               { "sp_on", 1 }, { "sp_harm_on", 1 }, { "sp_harm_odd", 0.8f }, { "sp_harm_high", 0.8f }, { "sp_harm_drive", 0.7f },
                                               { "sp_exc_on", 1 }, { "sp_exc_mode", 4 }, { "sp_exc_amount", 0.3f }, { "mb_on", 1 }, { "mb3_dynamics", 1 }, { "mb3_threshold", -20 } } },
            { "RAWSTYLE", "HEAVY SUB CONTROL", { { "eq1_on", 1 }, { "eq1_type", 3 }, { "eq1_freq", 30 }, { "eq1_slope", 3 }, { "mb_on", 1 }, { "mb1_dynamics", 1 }, { "mb1_threshold", -12 },
                                                 { "mb1_ratio", 6 }, { "mb1_attack", 10 }, { "mb2_dynamics", 1 }, { "mb2_threshold", -16 }, { "stw_on", 1 }, { "stw_mono_below", 150 } } },
            { "EXPERIMENTAL", "SPECTRAL CHAOS", { { "sp_on", 1 }, { "sp_comp_on", 1 }, { "sp_comp_reduction", 15 }, { "sp_comp_threshold", 2 }, { "sp_freeze", 1 }, { "sp_freeze_pitch", 7 },
                                                  { "sp_freeze_mix", 0.4f }, { "sp_harm_on", 1 }, { "sp_harm_drive", 0.9f }, { "eq1_on", 1 }, { "eq1_type", 8 }, { "eq1_freq", 1800 }, { "eq1_gain", 8 } } },
            { "EXPERIMENTAL", "METALLIC FREQUENCY", { { "eq1_on", 1 }, { "eq1_type", 8 }, { "eq1_freq", 2600 }, { "eq1_gain", 9 }, { "eq1_q", 6 }, { "eq2_on", 1 }, { "eq2_type", 8 }, { "eq2_freq", 5200 },
                                                      { "eq2_gain", 6 }, { "eq2_q", 8 }, { "sp_on", 1 }, { "sp_exc_on", 1 }, { "sp_exc_mode", 3 }, { "sp_exc_amount", 0.5f } } },
        };
        return l;
    }
    inline void apply (GeProcessor& p, const Preset& pr)
    {
        p.undo.beginNewTransaction ("Module 07 preset");
        m06t::reset (p, moduleIds());
        for (auto& [id, v] : pr.v) p.setParamInUndoStep (id, v);
    }

    // ---- RANDOMIZE (musical ranges; LOCKED engines untouched; one undo step)
    inline void randomize (GeProcessor& p, std::array<bool, NumEngines> locked, int seed, float intensity)
    {
        juce::Random r (seed);
        p.undo.beginNewTransaction ("Randomize module 07");
        auto move = [&] (const juce::String& id, float lo, float hi, bool logScale = false)
        {
            const float cur = value (p, id);
            const float t = logScale ? std::exp2 (std::log2 (lo) + r.nextFloat() * (std::log2 (hi) - std::log2 (lo))) : lo + r.nextFloat() * (hi - lo);
            p.setParamInUndoStep (id, cur + (t - cur) * intensity);
        };
        if (! locked[EEq])
            for (int i = 0; i < 16; ++i)
            {
                if (value (p, eid (i, "on")) < 0.5f) continue;
                move (eid (i, "freq"), 40.f, 12000.f, true); move (eid (i, "gain"), -6.f, 6.f); move (eid (i, "q"), 0.5f, 3.f, true);
            }
        if (! locked[EDyn])
            for (int i = 0; i < 4; ++i)
            {
                if (value (p, did (i, "on")) < 0.5f) continue;
                move (did (i, "freq"), 200.f, 9000.f, true); move (did (i, "threshold"), -40.f, -10.f); move (did (i, "range"), 2.f, 12.f);
            }
        if (! locked[EMulti] && value (p, "mb_on") > 0.5f)
            for (int i = 1; i <= 4; ++i) { const juce::String o = "mb" + juce::String (i) + "_"; move (o + "gain", -4.f, 4.f); move (o + "width", 0.6f, 1.4f); move (o + "threshold", -30.f, -8.f); }
        if (! locked[ESpectral] && value (p, "sp_on") > 0.5f)
        { move ("sp_tilt", -0.4f, 0.4f); move ("sp_harm_drive", 0.1f, 0.8f); move ("sp_exc_amount", 0.f, 0.6f); move ("sp_tilt_pivot", 300.f, 4000.f, true); }
        if (! locked[EStereo] && value (p, "stw_on") > 0.5f)
        { move ("stw_mid_width", 0.6f, 1.6f); move ("stw_high_width", 0.8f, 1.8f); move ("stw_mono_below", 80.f, 200.f, true); }
    }
}
