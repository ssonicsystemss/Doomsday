#pragma once
// =====================================================================
//  4GE module 05 - DISTORTION tools (never in the GUI, never on the audio thread):
//   the stage pool per LOCATION (list in ORDER, add / duplicate / copy / paste / move up / down / delete),
//   factory presets (EARLY / MODERN / RAWSTYLE / EXPERIMENTAL / UTILITY) and files:
//   .4gstage (one stage), .4gchain (one location), .4gmband (the multiband block of a location + crossovers),
//   .4gpar (the parallel block of a location), .4gdist (everything: 16 stages + the global settings + the curves).
//   Every action = one UNDO step.
// =====================================================================
#include "LayerTools.h"
#include "../DSP/Dist/Distortion.h"

namespace fge::distt
{
    namespace dd = fge::dsp::dist;
    constexpr int stages = dd::numStages;
    inline juce::String sid (int stage, const juce::String& suffix) { return "dist" + juce::String (stage + 1) + "_" + suffix; }
    inline juce::StringArray suffixes()
    {
        juce::StringArray s; const juce::String pre ("dist1_");
        for (int i = 0; i < P::count; ++i) { const juce::String id (def (i).id); if (id.startsWith (pre)) s.add (id.substring (pre.length())); }
        return s;
    }
    inline float value (GeProcessor& p, const juce::String& id) { return layers::value (p, id); }
    inline bool on (GeProcessor& p, int st) { return value (p, sid (st, "on")) > 0.5f; }
    inline int location (GeProcessor& p, int st) { return juce::roundToInt (value (p, sid (st, "location"))); }

    // the stages of a location, in the order they run (ORDER, then the stage number - the same rule as the DSP)
    inline std::vector<int> chain (GeProcessor& p, int loc)
    {
        std::vector<int> v;
        for (int i = 0; i < stages; ++i) if (on (p, i) && location (p, i) == loc) v.push_back (i);
        std::stable_sort (v.begin(), v.end(), [&] (int a, int b) { const float oa = value (p, sid (a, "order")), ob = value (p, sid (b, "order")); return oa != ob ? oa < ob : a < b; });
        return v;
    }
    inline int freeStage (GeProcessor& p) { for (int i = 0; i < stages; ++i) if (! on (p, i)) return i; return -1; }
    inline void renumber (GeProcessor& p, const std::vector<int>& order)      // ORDER = 1, 2, 3 ... in this sequence
    {
        for (size_t k = 0; k < order.size(); ++k) p.setParamInUndoStep (sid (order[k], "order"), (float) (k + 1));
    }
    inline void resetStage (GeProcessor& p, int st)
    {
        layers::resetPrefix (p, sid (st, ""));
        p.setDistCurve (st, dd::CurveData::preset (1), false);
    }

    // ---- one stage <-> XML (the parameters without the "distN_" prefix + the CUSTOM curve)
    inline std::unique_ptr<juce::XmlElement> stageToXml (GeProcessor& p, int st)
    {
        auto x = std::make_unique<juce::XmlElement> ("STAGE4GE");
        for (auto& k : suffixes()) { auto* e = x->createNewChildElement ("PARAM"); e->setAttribute ("id", k); e->setAttribute ("value", value (p, sid (st, k))); }
        x->setAttribute ("curve", p.distData().curve[(size_t) st].toString());
        return x;
    }
    inline bool xmlToStage (GeProcessor& p, int st, const juce::XmlElement& x, int forceLoc = -1)
    {
        if (! x.hasTagName ("STAGE4GE") || ! juce::isPositiveAndBelow (st, stages)) return false;
        for (auto* e : x.getChildWithTagNameIterator ("PARAM")) p.setParamInUndoStep (sid (st, e->getStringAttribute ("id")), (float) e->getDoubleAttribute ("value"));
        if (forceLoc >= 0) p.setParamInUndoStep (sid (st, "location"), (float) forceLoc);
        dd::CurveData c;
        if (dd::CurveData::fromString (x.getStringAttribute ("curve"), c)) p.setDistCurve (st, c, false);
        return true;
    }

    // ---- the pool: add / duplicate / paste / move / delete (one undo step each)
    inline int addStage (GeProcessor& p, int loc, int type = dd::Soft)
    {
        const int st = freeStage (p);
        if (st < 0) return -1;
        p.undo.beginNewTransaction ("Add distortion stage");
        resetStage (p, st);
        auto c = chain (p, loc);
        p.setParamInUndoStep (sid (st, "on"), 1.f);
        p.setParamInUndoStep (sid (st, "location"), (float) loc);
        p.setParamInUndoStep (sid (st, "type"), (float) type);
        c.push_back (st); renumber (p, c);
        return st;
    }
    inline int duplicate (GeProcessor& p, int st)                    // a copy right after the original
    {
        const int to = freeStage (p);
        if (to < 0 || ! on (p, st)) return -1;
        p.undo.beginNewTransaction ("Duplicate distortion stage");
        auto x = stageToXml (p, st);
        xmlToStage (p, to, *x);
        auto c = chain (p, location (p, st));
        c.erase (std::remove (c.begin(), c.end(), to), c.end());
        c.insert (std::find (c.begin(), c.end(), st) + 1, to);
        renumber (p, c);
        return to;
    }
    inline bool paste (GeProcessor& p, int st, const juce::XmlElement& x, bool newStep = true)   // into this stage (keeps its location and its place)
    {
        if (newStep) p.undo.beginNewTransaction ("Paste distortion stage");
        const float loc = value (p, sid (st, "location")), ord = value (p, sid (st, "order"));
        if (! xmlToStage (p, st, x)) return false;
        p.setParamInUndoStep (sid (st, "on"), 1.f);
        p.setParamInUndoStep (sid (st, "location"), loc); p.setParamInUndoStep (sid (st, "order"), ord);
        return true;
    }
    inline void move (GeProcessor& p, int st, int delta)               // earlier / later in its chain
    {
        auto c = chain (p, location (p, st));
        auto it = std::find (c.begin(), c.end(), st);
        if (it == c.end()) return;
        const int i = (int) (it - c.begin()), j = juce::jlimit (0, (int) c.size() - 1, i + delta);
        if (i == j) return;
        p.undo.beginNewTransaction ("Move distortion stage");
        std::swap (c[(size_t) i], c[(size_t) j]);
        renumber (p, c);
    }
    inline void remove (GeProcessor& p, int st)
    {
        p.undo.beginNewTransaction ("Delete distortion stage");
        const int loc = location (p, st);
        resetStage (p, st);
        auto c = chain (p, loc); renumber (p, c);
    }

    // ---- factory presets: a list of stages (location + values); applying one replaces the whole distortion
    struct Stage { int loc; std::vector<std::pair<const char*, float>> v; };
    struct Preset { const char* category; const char* name; std::vector<Stage> st; std::vector<std::pair<const char*, float>> global; };
    enum { Gl = dd::LocGlobal, Pu = dd::LocPunch, Bo = dd::LocBody, Ta = dd::LocTail, Su = dd::LocSub, Tx = dd::LocTexture };
    inline const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> l {
            { "EARLY", "CLASSIC SATURATION", { { Gl, { { "type", dd::Tube }, { "drive", 0.45f }, { "even", 0.55f }, { "odd", 0.7f }, { "post_on", 1 }, { "post_cutoff", 9000 } } } } },
            { "EARLY", "SOFT PUNCH",         { { Pu, { { "type", dd::Soft }, { "drive", 0.5f }, { "shape", 0.6f } } }, { Bo, { { "type", dd::Tube }, { "drive", 0.3f } } } } },
            { "EARLY", "VINTAGE CLIP",       { { Gl, { { "type", dd::Soft }, { "drive", 0.6f }, { "post_on", 1 }, { "post_cutoff", 6000 }, { "post_slope", 1 } } },
                                               { Gl, { { "type", dd::Hard }, { "drive", 0.2f }, { "ceiling", -2 } } } } },
            { "MODERN", "MODERN HARD CLIP",  { { Gl, { { "type", dd::Hard }, { "drive", 0.55f }, { "ceiling", -1 }, { "oversampling", 1 } } } } },
            { "MODERN", "CLEAN AGGRESSION",  { { Ta, { { "type", dd::Asym }, { "drive", 0.6f }, { "pre_on", 1 }, { "pre_type", 1 }, { "pre_cutoff", 120 }, { "post_on", 1 }, { "post_cutoff", 10000 } } },
                                               { Gl, { { "type", dd::Digital }, { "drive", 0.3f } } } } },
            { "MODERN", "DENSE DISTORTION",  { { Ta, { { "type", dd::Soft }, { "drive", 0.6f } } }, { Ta, { { "type", dd::Tube }, { "drive", 0.5f } } },
                                               { Ta, { { "type", dd::Hard }, { "drive", 0.4f } } } } },
            { "RAWSTYLE", "RAW ASYMMETRIC",  { { Ta, { { "type", dd::Asym }, { "drive", 0.8f }, { "threshold", 0.7f }, { "neg_drive", 0.9f }, { "neg_threshold", 0.35f }, { "bias", 0.2f },
                                                       { "post_on", 1 }, { "post_cutoff", 7000 }, { "mod_drive", 0.25f }, { "mod_decay", 300 } } } } },
            { "RAWSTYLE", "BRUTAL FOLD",     { { Ta, { { "type", dd::Wavefold }, { "drive", 0.7f }, { "fold_stages", 4 }, { "shape", 0.6f }, { "oversampling", 1 } } },
                                               { Ta, { { "type", dd::Hard }, { "drive", 0.4f } } } } },
            { "RAWSTYLE", "HARD DIGITAL",    { { Ta, { { "type", dd::Digital }, { "drive", 0.85f }, { "shape", 0.9f }, { "ceiling", -1 }, { "oversampling", 2 } } } } },
            { "RAWSTYLE", "CRUSHED TAIL",    { { Ta, { { "type", dd::Bitcrush }, { "bits", 6 }, { "sr_reduce", 0.5f }, { "path", 1 } } },
                                               { Ta, { { "type", dd::Soft }, { "drive", 0.6f }, { "path", 2 } } } } },
            { "EXPERIMENTAL", "METALLIC FOLD",   { { Ta, { { "type", dd::Foldback }, { "drive", 0.7f }, { "shape", 0.7f }, { "morph_type", dd::Wavefold }, { "morph", 0.4f }, { "mod_fold", 0.5f } } } } },
            { "EXPERIMENTAL", "FM CRUSH",        { { Ta, { { "type", dd::Rectifier }, { "drive", 0.6f }, { "shape", 0.8f } } },
                                                   { Ta, { { "type", dd::Bitcrush }, { "bits", 8 }, { "sr_reduce", 0.7f }, { "mod_source", 2 }, { "mod_drive", 0.6f } } } } },
            { "EXPERIMENTAL", "DIGITAL CHAOS",   { { Ta, { { "type", dd::Soft }, { "drive", 0.3f }, { "band", 1 } } }, { Ta, { { "type", dd::Digital }, { "drive", 0.8f }, { "band", 2 } } },
                                                   { Ta, { { "type", dd::Bitcrush }, { "bits", 5 }, { "band", 3 } } } } },
            { "EXPERIMENTAL", "COMPLEX HARMONICS", { { Ta, { { "type", dd::Tube }, { "drive", 0.7f }, { "path", 1 } } }, { Ta, { { "type", dd::Rectifier }, { "drive", 0.6f }, { "path", 2 } } },
                                                     { Ta, { { "type", dd::Wavefold }, { "drive", 0.5f }, { "path", 3 } } } } },
            { "UTILITY", "SUB CLEANUP",      { { Su, { { "type", dd::Soft }, { "drive", 0.15f }, { "post_on", 1 }, { "post_cutoff", 200 }, { "post_slope", 2 } } } } },
            { "UTILITY", "PUNCH CONTROL",    { { Pu, { { "type", dd::Hard }, { "drive", 0.3f }, { "ceiling", -4 } } } } },
            { "UTILITY", "HIGH TEXTURE",     { { Tx, { { "type", dd::Digital }, { "drive", 0.7f }, { "mix", 0.5f }, { "pre_on", 1 }, { "pre_type", 1 }, { "pre_cutoff", 3000 } } } } },
            { "UTILITY", "MASTER CLIP",      { { Gl, { { "type", dd::Soft }, { "drive", 0.35f }, { "shape", 0.9f }, { "threshold", 0.9f } } } }, { { "dist_global_position", 1 } } },
        };
        return l;
    }
    inline void resetAll (GeProcessor& p)
    {
        for (int i = 0; i < stages; ++i) resetStage (p, i);
        for (auto* g : { "dist_xover_low", "dist_xover_high", "dist_band_solo", "dist_global_position", "dist_offline_quality" })
            if (auto* prm = p.apvts.getParameter (g)) p.setParamInUndoStep (g, prm->convertFrom0to1 (prm->getDefaultValue()));
    }
    inline void apply (GeProcessor& p, const Preset& pr)
    {
        p.undo.beginNewTransaction ("Distortion preset");
        resetAll (p);
        std::array<int, dd::NumLocs> ord {};
        for (size_t k = 0; k < pr.st.size() && k < (size_t) stages; ++k)
        {
            const int st = (int) k, loc = pr.st[k].loc;
            p.setParamInUndoStep (sid (st, "on"), 1.f);
            p.setParamInUndoStep (sid (st, "location"), (float) loc);
            p.setParamInUndoStep (sid (st, "order"), (float) ++ord[(size_t) loc]);
            for (auto& [key, v] : pr.st[k].v) p.setParamInUndoStep (sid (st, key), v);
        }
        for (auto& [id, v] : pr.global) p.setParamInUndoStep (id, v);
    }

    // ---- files
    enum Scope { ScStage = 0, ScChain, ScMultiband, ScParallel, ScFull };
    inline const char* extension (int scope) { static const char* e[5] { ".4gstage", ".4gchain", ".4gmband", ".4gpar", ".4gdist" }; return e[juce::jlimit (0, 4, scope)]; }
    inline const char* scopeName (int scope) { static const char* n[5] { "STAGE", "CHAIN", "MULTIBAND", "PARALLEL", "FULL DISTORTION" }; return n[juce::jlimit (0, 4, scope)]; }
    inline bool inScope (GeProcessor& p, int st, int scope)
    {
        if (scope == ScMultiband) return value (p, sid (st, "band")) > 0.5f;
        if (scope == ScParallel) return value (p, sid (st, "path")) > 0.5f && value (p, sid (st, "band")) < 0.5f;
        return true;
    }
    // stage / location given: what the scope takes from it
    inline std::unique_ptr<juce::XmlElement> save (GeProcessor& p, int scope, int stage, int loc)
    {
        auto x = std::make_unique<juce::XmlElement> ("DIST4GE");
        x->setAttribute ("scope", scope);
        if (scope == ScStage) x->addChildElement (stageToXml (p, stage).release());
        else if (scope == ScFull)
        {
            for (int i = 0; i < stages; ++i) { auto* e = stageToXml (p, i).release(); e->setAttribute ("index", i); x->addChildElement (e); }
        }
        else for (int st : chain (p, loc)) if (inScope (p, st, scope)) x->addChildElement (stageToXml (p, st).release());
        if (scope == ScMultiband || scope == ScFull)
            for (auto* g : { "dist_xover_low", "dist_xover_high", "dist_band_solo" }) { auto* e = x->createNewChildElement ("GLOBAL"); e->setAttribute ("id", g); e->setAttribute ("value", value (p, g)); }
        if (scope == ScFull)
            for (auto* g : { "dist_global_position", "dist_offline_quality" }) { auto* e = x->createNewChildElement ("GLOBAL"); e->setAttribute ("id", g); e->setAttribute ("value", value (p, g)); }
        return x;
    }
    // STAGE -> into the selected stage; CHAIN / MULTIBAND / PARALLEL -> replace that part of the selected location; FULL -> everything
    inline bool load (GeProcessor& p, const juce::XmlElement& x, int stage, int loc)
    {
        if (! x.hasTagName ("DIST4GE")) return false;
        const int scope = x.getIntAttribute ("scope", -1);
        if (scope < ScStage || scope > ScFull) return false;
        p.undo.beginNewTransaction (juce::String ("Load ") + scopeName (scope));
        if (scope == ScStage) { if (auto* e = x.getChildByName ("STAGE4GE")) paste (p, stage, *e, false); }
        else if (scope == ScFull)
        {
            resetAll (p);
            for (auto* e : x.getChildWithTagNameIterator ("STAGE4GE")) xmlToStage (p, e->getIntAttribute ("index", 0), *e);
        }
        else
        {
            for (int st : chain (p, loc)) if (scope == ScChain || inScope (p, st, scope)) resetStage (p, st);
            auto keep = chain (p, loc);
            for (auto* e : x.getChildWithTagNameIterator ("STAGE4GE"))
            {
                const int st = freeStage (p); if (st < 0) break;
                xmlToStage (p, st, *e, loc);
                p.setParamInUndoStep (sid (st, "on"), 1.f);
                keep.push_back (st);
            }
            renumber (p, keep);
        }
        for (auto* e : x.getChildWithTagNameIterator ("GLOBAL")) p.setParamInUndoStep (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));
        return true;
    }
}
