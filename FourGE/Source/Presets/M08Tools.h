#pragma once
// =====================================================================
//  4GE module 08 - DYNAMICS tools (never in the GUI, never on the audio thread):
//   factory presets (EARLY / MODERN / RAWSTYLE / EXPERIMENTAL - the whole chain incl. routing), files (.4gdyn one processor,
//   .4gm08 the module), COPY / PASTE of a processor, A / B, RESET; every action one UNDO step.
// =====================================================================
#include "M07Tools.h"

namespace fge::m08t
{
    enum Kind { KCmp = 0, KTs, KClp, KLim, KCrest, NumKinds };
    inline const char* prefixOf (int kind) { static const char* p[5] { "cmp", "ts", "clp", "lim", "crest" }; return p[juce::jlimit (0, 4, kind)]; }
    inline int countOf (int kind) { static const int n[5] { 4, 5, 2, 2, 1 }; return n[juce::jlimit (0, 4, kind)]; }
    inline juce::String pid (int kind, int i, const juce::String& k) { return kind == KCrest ? "crest_" + k : juce::String (prefixOf (kind)) + juce::String (i + 1) + "_" + k; }
    inline juce::StringArray idsOf (int kind, int i) { return layers::prefixedIds (kind == KCrest ? juce::String ("crest_") : juce::String (prefixOf (kind)) + juce::String (i + 1) + "_"); }
    inline juce::StringArray moduleIds()
    {
        juce::StringArray s;
        for (int k = 0; k < NumKinds; ++k) for (int i = 0; i < countOf (k); ++i) s.addArray (idsOf (k, i));
        for (auto* g : { "dyn_auto_gain", "dyn_level_match", "dyn_order", "mb_link" }) s.add (g);
        return s;
    }
    inline float value (GeProcessor& p, const juce::String& id) { return layers::value (p, id); }

    inline std::unique_ptr<juce::XmlElement> procToXml (GeProcessor& p, int kind, int i)
    {
        auto x = m06t::toXml (p, "DYN4GE", idsOf (kind, i), kind == KCrest ? juce::String ("crest_") : pid (kind, i, ""));
        x->setAttribute ("kind", kind);
        if (kind == KClp) x->setAttribute ("curve", p.distData().clip[(size_t) i].toString());
        return x;
    }
    inline void pasteProc (GeProcessor& p, int kind, int i, const juce::XmlElement& x, bool newStep = true)
    {
        if (x.getIntAttribute ("kind", -1) != kind) return;                      // (a compressor never lands on a limiter)
        if (newStep) p.undo.beginNewTransaction ("Paste dynamics");
        m06t::fromXml (p, x, kind == KCrest ? juce::String ("crest_") : pid (kind, i, ""));
        dsp::dist::CurveData c;
        if (kind == KClp && dsp::dist::CurveData::fromString (x.getStringAttribute ("curve"), c)) p.setClipCurve (i, c, false);
    }
    inline void resetProc (GeProcessor& p, int kind, int i) { p.undo.beginNewTransaction ("Reset dynamics"); m06t::reset (p, idsOf (kind, i)); }
    inline std::unique_ptr<juce::XmlElement> moduleToXml (GeProcessor& p)
    {
        auto x = m06t::toXml (p, "M084GE", moduleIds());
        for (int i = 0; i < 2; ++i) x->setAttribute ("clip" + juce::String (i + 1), p.distData().clip[(size_t) i].toString());
        return x;
    }
    inline bool load (GeProcessor& p, const juce::XmlElement& x, int kind, int sel)
    {
        if (x.hasTagName ("DYN4GE")) { pasteProc (p, kind, sel, x); return true; }
        if (! x.hasTagName ("M084GE")) return false;
        p.undo.beginNewTransaction ("Load module 08");
        m06t::reset (p, moduleIds()); m06t::fromXml (p, x);
        for (int i = 0; i < 2; ++i) { dsp::dist::CurveData c; if (dsp::dist::CurveData::fromString (x.getStringAttribute ("clip" + juce::String (i + 1)), c)) p.setClipCurve (i, c, false); }
        return true;
    }
    inline const char* extensionOf (const juce::String& tag) { return tag == "DYN4GE" ? ".4gdyn" : ".4gm08"; }
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

    // ---- factory presets: the whole chain + where each processor sits (locations: 0 GLOBAL, 1 PUNCH, 2 BODY, 3 TAIL, 4 SUB, 5 TEXTURE, 15 MASTER OUTPUT)
    struct Preset { const char* category; const char* name; std::vector<std::pair<const char*, float>> v; };
    inline const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> l {
            { "EARLY", "CLASSIC PUNCH",   { { "ts1_on", 1 }, { "ts1_attack", 0.5f }, { "cmp1_on", 1 }, { "cmp1_location", 2 }, { "cmp1_threshold", -20 }, { "cmp1_ratio", 2 },
                                            { "cmp1_attack", 15 }, { "lim1_on", 1 }, { "lim1_mode", 0 } } },
            { "EARLY", "SOFT DYNAMICS",   { { "cmp1_on", 1 }, { "cmp1_type", 2 }, { "cmp1_threshold", -18 }, { "cmp1_ratio", 2.5f }, { "ts2_on", 1 }, { "ts2_sustain", 0.2f }, { "ts2_mode", 2 },
                                            { "lim1_on", 1 } } },
            { "MODERN", "TIGHT IMPACT",   { { "ts1_on", 1 }, { "ts1_attack", 0.65f }, { "ts2_on", 1 }, { "ts2_attack", 0.1f }, { "ts2_sustain", 0.2f }, { "ts2_mode", 2 },
                                            { "ts3_on", 1 }, { "ts3_attack", -0.2f }, { "ts3_sustain", 0.3f }, { "ts3_mode", 3 }, { "ts4_on", 1 }, { "ts4_sustain", -0.1f }, { "ts4_mode", 1 },
                                            { "cmp1_on", 1 }, { "cmp1_location", 3 }, { "cmp1_threshold", -16 }, { "cmp1_ratio", 4 },
                                            { "clp1_on", 1 }, { "clp1_type", 0 }, { "clp1_ceiling", -1 }, { "clp1_drive", 0.3f }, { "lim1_on", 1 } } },
            { "MODERN", "FESTIVAL LOUD",  { { "cmp1_on", 1 }, { "cmp1_type", 6 }, { "cmp1_threshold", -30 }, { "cmp1_ratio", 8 }, { "cmp1_wet_level", -3 },
                                            { "clp1_on", 1 }, { "clp1_type", 5 }, { "clp1_drive", 0.5f }, { "clp1_ceiling", -1.5f },
                                            { "lim1_on", 1 }, { "lim1_mode", 3 }, { "lim1_lookahead", 2 } } },
            { "RAWSTYLE", "EXTREME CRUSH",{ { "clp1_on", 1 }, { "clp1_location", 3 }, { "clp1_type", 1 }, { "clp1_drive", 0.8f }, { "clp1_ceiling", -3 }, { "clp1_oversampling", 1 },
                                            { "clp2_on", 1 }, { "clp2_type", 2 }, { "clp2_drive", 0.5f }, { "cmp1_on", 1 }, { "cmp1_location", 3 }, { "cmp1_type", 1 },
                                            { "cmp1_threshold", -24 }, { "cmp1_ratio", 8 }, { "lim1_on", 1 }, { "lim1_mode", 1 } } },
            { "RAWSTYLE", "HEAVY PUNCH",  { { "ts1_on", 1 }, { "ts1_attack", 0.9f }, { "ts1_detection", 3 }, { "cmp1_on", 1 }, { "cmp1_location", 2 }, { "cmp1_threshold", -20 },
                                            { "clp1_on", 1 }, { "clp1_location", 1 }, { "clp1_type", 0 }, { "clp1_drive", 0.5f }, { "lim1_on", 1 }, { "lim1_mode", 2 } } },
            { "EXPERIMENTAL", "BROKEN DYNAMICS", { { "cmp1_on", 1 }, { "cmp1_location", 3 }, { "cmp1_type", 7 }, { "cmp1_sc_source", 1 }, { "cmp1_threshold", -30 }, { "cmp1_ratio", 20 },
                                                   { "ts3_on", 1 }, { "ts3_mode", 3 }, { "ts3_sustain", 0.8f }, { "crest_on", 1 }, { "crest_target", 6 }, { "crest_tail", 0.5f },
                                                   { "clp1_on", 1 }, { "clp1_type", 6 } } },
            { "EXPERIMENTAL", "MAXIMUM DENSITY", { { "cmp1_on", 1 }, { "cmp1_type", 3 }, { "cmp1_threshold", -30 }, { "cmp1_ratio", 10 }, { "cmp2_on", 1 }, { "cmp2_type", 5 },
                                                   { "cmp2_threshold", -30 }, { "cmp2_ratio", 3 }, { "crest_on", 1 }, { "crest_target", 6 }, { "lim1_on", 1 }, { "lim1_mode", 3 } } },
        };
        return l;
    }
    inline void apply (GeProcessor& p, const Preset& pr)
    {
        p.undo.beginNewTransaction ("Module 08 preset");
        m06t::reset (p, moduleIds());
        for (auto& [id, v] : pr.v) p.setParamInUndoStep (id, v);
    }
}
