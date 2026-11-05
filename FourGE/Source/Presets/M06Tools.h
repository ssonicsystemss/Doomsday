#pragma once
// =====================================================================
//  4GE module 06 - FILTER / RESONANCE / SCREECH tools (never in the GUI, never on the audio thread):
//   factory presets (EARLY / MODERN / RAWSTYLE / EXPERIMENTAL), files (.4gfilter one filter, .4gres one resonator,
//   .4gscreech the screech, .4gm06 the whole module), COPY / PASTE of a filter / resonator, A / B COMPARE (two snapshots),
//   RANDOMIZE with LOCK per engine (musical ranges; one UNDO step).
// =====================================================================
#include "LayerTools.h"

namespace fge::m06t
{
    enum Engine { EFilter = 0, EResonance, EScreech, NumEngines };
    inline juce::String fid (int i, const juce::String& k) { return "flt" + juce::String (i + 1) + "_" + k; }
    inline juce::String rid (int i, const juce::String& k) { return "res" + juce::String (i + 1) + "_" + k; }
    inline juce::StringArray idsOf (const juce::String& prefix) { return layers::prefixedIds (prefix); }
    inline juce::StringArray engineIds (int e)
    {
        juce::StringArray s;
        if (e == EFilter) { for (int i = 0; i < 8; ++i) s.addArray (idsOf (fid (i, ""))); s.add ("filter_xover_low"); s.add ("filter_xover_high"); }
        else if (e == EResonance) { s.addArray (idsOf ("res1_")); s.addArray (idsOf ("res2_")); }
        else s.addArray (idsOf ("scr_"));
        return s;
    }
    inline juce::StringArray moduleIds() { juce::StringArray s; for (int e = 0; e < NumEngines; ++e) s.addArray (engineIds (e)); return s; }
    inline float value (GeProcessor& p, const juce::String& id) { return layers::value (p, id); }
    inline void reset (GeProcessor& p, const juce::StringArray& ids)
    {
        for (auto& id : ids) if (auto* prm = p.apvts.getParameter (id)) p.setParamInUndoStep (id, prm->convertFrom0to1 (prm->getDefaultValue()));
    }

    // ---- XML: a list of parameters (+ the drawn CUSTOM resonator spectra for resonator / module files)
    inline std::unique_ptr<juce::XmlElement> toXml (GeProcessor& p, const juce::String& tag, const juce::StringArray& ids, const juce::String& strip = {}, int resShapes = -1)
    {
        auto x = std::make_unique<juce::XmlElement> (tag);
        for (auto& id : ids) { auto* e = x->createNewChildElement ("PARAM"); e->setAttribute ("id", strip.isNotEmpty() ? id.substring (strip.length()) : id); e->setAttribute ("value", value (p, id)); }
        if (resShapes == 2) for (int i = 0; i < 2; ++i) x->setAttribute ("res_custom" + juce::String (i + 1), p.shape (env::ResCustom1 + i).toString());
        else if (resShapes >= 0) x->setAttribute ("res_custom", p.shape (env::ResCustom1 + resShapes).toString());
        return x;
    }
    inline void fromXml (GeProcessor& p, const juce::XmlElement& x, const juce::String& prefix = {}, int resShape = -1)
    {
        for (auto* e : x.getChildWithTagNameIterator ("PARAM")) p.setParamInUndoStep (prefix + e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));
        env::Shape s;
        if (resShape >= 0 && env::Shape::fromString (x.getStringAttribute ("res_custom"), s, 0.f, 1.f)) p.setShape (env::ResCustom1 + resShape, s, true, false);
        for (int i = 0; i < 2; ++i)
            if (env::Shape::fromString (x.getStringAttribute ("res_custom" + juce::String (i + 1)), s, 0.f, 1.f)) p.setShape (env::ResCustom1 + i, s, true, false);
    }
    inline std::unique_ptr<juce::XmlElement> filterToXml (GeProcessor& p, int i) { return toXml (p, "FILTER4GE", idsOf (fid (i, "")), fid (i, "")); }
    inline std::unique_ptr<juce::XmlElement> resToXml (GeProcessor& p, int i) { return toXml (p, "RES4GE", idsOf (rid (i, "")), rid (i, ""), i); }
    inline std::unique_ptr<juce::XmlElement> screechToXml (GeProcessor& p) { return toXml (p, "SCREECH4GE", engineIds (EScreech)); }
    inline std::unique_ptr<juce::XmlElement> moduleToXml (GeProcessor& p) { return toXml (p, "M064GE", moduleIds(), {}, 2); }
    // loads any of the four into the selected filter / resonator (or the screech / the module); one undo step
    inline bool load (GeProcessor& p, const juce::XmlElement& x, int filterSel, int resSel)
    {
        p.undo.beginNewTransaction ("Load " + x.getTagName());
        if (x.hasTagName ("FILTER4GE")) { fromXml (p, x, fid (filterSel, "")); return true; }
        if (x.hasTagName ("RES4GE")) { fromXml (p, x, rid (resSel, ""), resSel); return true; }
        if (x.hasTagName ("SCREECH4GE")) { reset (p, engineIds (EScreech)); fromXml (p, x); return true; }
        if (x.hasTagName ("M064GE")) { reset (p, moduleIds()); fromXml (p, x); return true; }
        return false;
    }
    inline const char* extensionOf (const juce::String& tag) { return tag == "FILTER4GE" ? ".4gfilter" : tag == "RES4GE" ? ".4gres" : tag == "SCREECH4GE" ? ".4gscreech" : ".4gm06"; }

    // ---- A / B COMPARE: two snapshots of the whole module (in memory, per editor session)
    struct Compare
    {
        std::unique_ptr<juce::XmlElement> a, b; bool onB = false;
        void toggle (GeProcessor& p)
        {
            auto now = moduleToXml (p);
            auto& other = onB ? a : b;
            (onB ? b : a) = std::move (now);
            onB = ! onB;
            if (other) { p.undo.beginNewTransaction ("A / B"); fromXml (p, *other); }
        }
    };

    // ---- factory presets (the whole module; starting points, nothing is locked)
    struct Preset { const char* category; const char* name; std::vector<std::pair<const char*, float>> v; };
    inline const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> l {
            { "EARLY", "CLASSIC RESONANCE", { { "res1_on", 1 }, { "res1_location", 3 }, { "res1_mode", 0 }, { "res1_freq", 1200 }, { "res1_q", 8 }, { "res1_amount", 0.35f },
                                              { "res1_decay", 250 }, { "flt1_on", 1 }, { "flt1_location", 3 }, { "flt1_type", 0 }, { "flt1_cutoff", 6000 }, { "flt1_resonance", 0.25f } } },
            { "EARLY", "OLDSCHOOL SCREECH", { { "scr_on", 1 }, { "scr_mode", 0 }, { "scr_level", -4 }, { "scr_start_mode", 1 }, { "scr_osc1_wave", 1 },
                                              { "scr_f1_type", 2 }, { "scr_f1_cutoff", 2500 }, { "scr_f1_resonance", 0.6f }, { "scr_pitch_amount", 0.5f }, { "scr_pitch_start", 7 } } },
            { "MODERN", "WIDE SCREECH",     { { "scr_on", 1 }, { "scr_mode", 0 }, { "scr_level", -2 }, { "scr_start_mode", 1 }, { "scr_width", 1 },
                                              { "scr_osc1_detune", 0.4f }, { "scr_osc1_pan", -0.6f }, { "scr_osc2_on", 1 }, { "scr_osc2_pitch", 0 }, { "scr_osc2_detune", 0.5f }, { "scr_osc2_pan", 0.6f },
                                              { "scr_osc3_on", 1 }, { "scr_osc3_pitch", 12 }, { "scr_osc3_level", 0.4f }, { "scr_f1_cutoff", 3500 }, { "scr_f2_on", 1 }, { "scr_f2_cutoff", 9000 },
                                              { "scr_mod_cutoff", 0.3f } } },
            { "MODERN", "MELODIC TAIL",     { { "scr_on", 1 }, { "scr_mode", 2 }, { "scr_pitch_mode", 1 }, { "scr_vowel_auto", 1 }, { "scr_vowel", 0 }, { "scr_vowel_end", 3 },
                                              { "scr_start_mode", 1 }, { "res1_on", 1 }, { "res1_location", 3 }, { "res1_pitch_track", 1 }, { "res1_freq", 784 }, { "res1_amount", 0.3f },
                                              { "flt1_on", 1 }, { "flt1_location", 3 }, { "flt1_cutoff", 4000 }, { "flt1_env_amount", 0.4f } } },
            { "RAWSTYLE", "AGGRESSIVE METAL", { { "scr_on", 1 }, { "scr_mode", 1 }, { "scr_metal", 0.8f }, { "scr_metal_q", 0.8f }, { "scr_start_mode", 1 }, { "scr_level", 0 },
                                                { "res1_on", 1 }, { "res1_location", 14 }, { "res1_mode", 1 }, { "res1_freq", 2400 }, { "res1_amount", 0.5f },
                                                { "flt1_on", 1 }, { "flt1_location", 3 }, { "flt1_position", 1 }, { "flt1_type", 5 }, { "flt1_cutoff", 5000 }, { "flt1_resonance", 0.7f } } },
            { "RAWSTYLE", "FEEDBACK TERROR",  { { "scr_on", 1 }, { "scr_mode", 5 }, { "scr_fb_amount", 0.9f }, { "scr_fb_drive", 0.9f }, { "scr_fb_stability", 0.3f },
                                                { "scr_mod_feedback", 0.3f }, { "scr_start_mode", 1 }, { "scr_level", -2 }, { "scr_f1_cutoff", 3000 } } },
            { "EXPERIMENTAL", "FORMANT MACHINE", { { "scr_on", 1 }, { "scr_mode", 6 }, { "scr_vowel_auto", 1 }, { "scr_vowel", 0 }, { "scr_vowel_end", 4 }, { "scr_fm_index", 0.6f },
                                                   { "flt1_on", 1 }, { "flt1_location", 3 }, { "flt1_type", 8 }, { "flt1_vowel", 2 }, { "flt1_mix", 0.6f } } },
            { "EXPERIMENTAL", "BROKEN RESONATOR", { { "res1_on", 1 }, { "res1_location", 3 }, { "res1_mode", 2 }, { "res1_source", 4 }, { "res1_freq", 900 }, { "res1_feedback", 0.7f },
                                                    { "res1_penv_amount", 1 }, { "res1_penv_start", 6000 }, { "res1_penv_end", 300 }, { "res1_amount", 0.6f },
                                                    { "res2_on", 1 }, { "res2_location", 3 }, { "res2_mode", 3 }, { "res2_source", 5 }, { "res2_freq", 220 }, { "res2_amount", 0.5f } } },
        };
        return l;
    }
    inline void apply (GeProcessor& p, const Preset& pr)
    {
        p.undo.beginNewTransaction ("Module 06 preset");
        reset (p, moduleIds());
        for (auto& [id, v] : pr.v) p.setParamInUndoStep (id, v);
    }

    // ---- RANDOMIZE (musical ranges; LOCKED engines untouched; one undo step)
    inline void randomize (GeProcessor& p, std::array<bool, NumEngines> locked, int seed, float intensity)
    {
        juce::Random r (seed);
        p.undo.beginNewTransaction ("Randomize module 06");
        auto move = [&] (const juce::String& id, float lo, float hi, bool logScale = false)
        {
            const float cur = value (p, id);
            float t = logScale ? std::exp2 (std::log2 (lo) + r.nextFloat() * (std::log2 (hi) - std::log2 (lo))) : lo + r.nextFloat() * (hi - lo);
            p.setParamInUndoStep (id, cur + (t - cur) * intensity);
        };
        auto pick = [&] (const juce::String& id, int n) { if (r.nextFloat() < intensity) p.setParamInUndoStep (id, (float) r.nextInt (n)); };
        if (! locked[EFilter])
            for (int i = 0; i < 8; ++i)
            {
                if (value (p, fid (i, "on")) < 0.5f) continue;
                pick (fid (i, "type"), 10);
                move (fid (i, "cutoff"), 200.f, 12000.f, true); move (fid (i, "resonance"), 0.f, 0.85f); move (fid (i, "drive"), 0.f, 0.6f);
                move (fid (i, "env_amount"), -0.6f, 0.6f); move (fid (i, "env_decay"), 20.f, 800.f, true);
            }
        if (! locked[EResonance])
            for (int i = 0; i < 2; ++i)
            {
                if (value (p, rid (i, "on")) < 0.5f) continue;
                pick (rid (i, "mode"), 6);
                move (rid (i, "freq"), 300.f, 6000.f, true); move (rid (i, "q"), 2.f, 40.f, true); move (rid (i, "decay"), 50.f, 1200.f, true);
                move (rid (i, "amount"), 0.1f, 0.7f); move (rid (i, "feedback"), 0.f, 0.6f);
            }
        if (! locked[EScreech] && value (p, "scr_on") > 0.5f)
        {
            pick ("scr_mode", 8);
            move ("scr_f1_cutoff", 800.f, 8000.f, true); move ("scr_f1_resonance", 0.2f, 0.85f);
            move ("scr_fm_index", 0.f, 0.9f); move ("scr_fm_ratio", 0.5f, 6.f); move ("scr_metal", 0.f, 1.f); move ("scr_vowel", 0.f, 4.f);
            move ("scr_fb_amount", 0.2f, 0.85f); move ("scr_pitch_amount", 0.f, 1.f); move ("scr_pitch_start", -12.f, 24.f);
            for (int o = 1; o <= 4; ++o) move ("scr_osc" + juce::String (o) + "_detune", 0.f, 0.6f);
        }
    }
}
