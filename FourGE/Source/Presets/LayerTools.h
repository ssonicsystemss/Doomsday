#pragma once
// =====================================================================
//  4GE module 04 - LAYER / NOISE / TRANSIENT tools (never in the GUI, never on the audio thread):
//   layer NAMES / LOCK / ORDER (state child LAYERS - display data, never routing or sound),
//   COPY / PASTE / DUPLICATE / DELETE, the LAYER RANDOMIZER (modes, INTENSITY, SEED, LOCK, PRESERVE GROUPS / ROUTING, one undo step),
//   factory presets (NOISE / TRANSIENT / LAYER stacks) and files (.4gnoise / .4gtrans / .4glayer / .4glayers).
// =====================================================================
#include "OscFiles.h"

namespace fge::layers
{
    constexpr int count = 8;
    inline juce::String lid (int layer, const juce::String& suffix) { return "layer" + juce::String (layer + 1) + "_" + suffix; }
    inline juce::StringArray layerSuffixes()
    {
        juce::StringArray s; const juce::String pre ("layer1_");
        for (int i = 0; i < P::count; ++i) { const juce::String id (def (i).id); if (id.startsWith (pre)) s.add (id.substring (pre.length())); }
        return s;
    }
    inline juce::StringArray prefixedIds (const juce::String& pre)
    {
        juce::StringArray s;
        for (int i = 0; i < P::count; ++i) { const juce::String id (def (i).id); if (id.startsWith (pre)) s.add (id); }
        return s;
    }
    inline float value (GeProcessor& p, const juce::String& id) { auto* v = p.apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.f; }

    // ---- display data (names / lock / order): state child LAYERS, undoable, never touches the sound
    inline juce::ValueTree tree (GeProcessor& p)
    {
        auto t = p.apvts.state.getChildWithName ("LAYERS");
        if (! t.isValid()) { t = juce::ValueTree ("LAYERS"); p.apvts.state.appendChild (t, nullptr); }
        return t;
    }
    inline juce::String name (GeProcessor& p, int i) { return tree (p).getProperty ("name" + juce::String (i + 1), "LAYER " + juce::String (i + 1)).toString(); }
    inline void setName (GeProcessor& p, int i, const juce::String& n) { p.undo.beginNewTransaction ("Rename layer"); tree (p).setProperty ("name" + juce::String (i + 1), n.substring (0, 24), &p.undo); }
    inline bool locked (GeProcessor& p, int i) { return (bool) tree (p).getProperty ("lock" + juce::String (i + 1), false); }
    inline void setLocked (GeProcessor& p, int i, bool on) { tree (p).setProperty ("lock" + juce::String (i + 1), on, &p.undo); }
    inline std::array<int, count> order (GeProcessor& p)
    {
        std::array<int, count> o {};
        auto t = juce::StringArray::fromTokens (tree (p).getProperty ("order", "").toString(), ",", "");
        bool ok = t.size() == count;
        for (int i = 0; i < count; ++i) { o[(size_t) i] = ok ? t[i].getIntValue() : i; ok &= o[(size_t) i] >= 0 && o[(size_t) i] < count; }
        if (! ok) for (int i = 0; i < count; ++i) o[(size_t) i] = i;
        return o;
    }
    // moving a row only changes the DISPLAY order (routing, groups and settings stay)
    inline void move (GeProcessor& p, int row, int delta)
    {
        auto o = order (p);
        const int to = juce::jlimit (0, count - 1, row + delta);
        if (to == row) return;
        std::swap (o[(size_t) row], o[(size_t) to]);
        juce::StringArray s; for (auto v : o) s.add (juce::String (v));
        p.undo.beginNewTransaction ("Move layer");
        tree (p).setProperty ("order", s.joinIntoString (","), &p.undo);
    }

    // ---- copy / paste / duplicate / delete
    inline std::unique_ptr<juce::XmlElement> layerToXml (GeProcessor& p, int i)
    {
        auto x = std::make_unique<juce::XmlElement> ("LAYER4GE");
        for (auto& k : layerSuffixes()) { auto* e = x->createNewChildElement ("PARAM"); e->setAttribute ("id", k); e->setAttribute ("value", value (p, lid (i, k))); }
        x->setAttribute ("name", name (p, i));
        x->setAttribute ("shape", p.shape (env::AmpLayer1 + i).toString());
        return x;
    }
    inline bool xmlToLayer (GeProcessor& p, int i, const juce::XmlElement& x, bool newStep = true)
    {
        if (! x.hasTagName ("LAYER4GE")) return false;
        if (newStep) p.undo.beginNewTransaction ("Layer");
        for (auto* e : x.getChildWithTagNameIterator ("PARAM")) p.setParamInUndoStep (lid (i, e->getStringAttribute ("id")), (float) e->getDoubleAttribute ("value"));
        env::Shape s;
        if (env::Shape::fromString (x.getStringAttribute ("shape"), s, 0.f, 1.f)) p.setShape (env::AmpLayer1 + i, s, true, false);
        tree (p).setProperty ("name" + juce::String (i + 1), x.getStringAttribute ("name", "LAYER " + juce::String (i + 1)), &p.undo);
        return true;
    }
    inline void deleteLayer (GeProcessor& p, int i)            // = back to the defaults, OFF
    {
        p.undo.beginNewTransaction ("Delete layer");
        for (auto& k : layerSuffixes()) if (auto* prm = p.apvts.getParameter (lid (i, k))) p.setParamInUndoStep (lid (i, k), prm->convertFrom0to1 (prm->getDefaultValue()));
        tree (p).removeProperty ("name" + juce::String (i + 1), &p.undo);
    }
    inline int duplicate (GeProcessor& p, int i)               // into the first OFF layer; returns it (-1 = all in use)
    {
        for (int k = 0; k < count; ++k)
            if (k != i && value (p, lid (k, "on")) < 0.5f)
            {
                auto x = layerToXml (p, i);
                xmlToLayer (p, k, *x);
                tree (p).setProperty ("name" + juce::String (k + 1), name (p, i) + " COPY", &p.undo);
                return k;
            }
        return -1;
    }

    // ---- RANDOMIZER: deterministic (SEED), INTENSITY scales every change, LOCKED layers untouched, never above 0 dB
    enum RandMode { RLevel = 0, RPitch, RLength, RSource, RFilter, RPhase, REnvelope, RAll };
    inline juce::StringArray randModeNames() { return { "LAYER LEVEL", "PITCH", "LENGTH", "SOURCE", "FILTER", "PHASE", "ENVELOPE", "ALL" }; }
    struct RandSettings { int mode = RAll; float intensity = 0.5f; int seed = 1; bool preserveGroups = true, preserveRouting = true; };
    inline void randomize (GeProcessor& p, const RandSettings& rs)
    {
        juce::Random r ((juce::int64) rs.seed * 7919 + 13);
        p.undo.beginNewTransaction ("Randomize layers");
        auto want = [&] (int m) { return rs.mode == RAll || rs.mode == m; };
        auto move = [&] (const juce::String& id, float lo, float hi, bool log = false)
        {
            const float cur = value (p, id);
            float v;
            if (log) v = cur * std::exp2 ((r.nextFloat() * 2.f - 1.f) * 3.f * rs.intensity);
            else v = cur + (r.nextFloat() * 2.f - 1.f) * (hi - lo) * 0.5f * rs.intensity;
            p.setParamInUndoStep (id, juce::jlimit (lo, hi, v));
        };
        for (int i = 0; i < count; ++i)
        {
            if (locked (p, i) || value (p, lid (i, "on")) < 0.5f) { for (int k = 0; k < 12; ++k) r.nextFloat(); continue; }   // the same sequence for the others
            if (want (RLevel))  move (lid (i, "level"), -30.f, 0.f);
            if (want (RPitch))  move (lid (i, "pitch"), -24.f, 24.f);
            if (want (RLength)) move (lid (i, "length"), 20.f, 1500.f, true);
            if (want (RSource) && r.nextFloat() < rs.intensity) p.setParamInUndoStep (lid (i, "source"), (float) r.nextInt (11));
            if (want (RFilter)) { if (r.nextFloat() < rs.intensity) p.setParamInUndoStep (lid (i, "filter_on"), r.nextBool() ? 1.f : 0.f);
                                  move (lid (i, "cutoff"), 80.f, 16000.f, true); move (lid (i, "resonance"), 0.f, 0.8f); }
            if (want (RPhase))  move (lid (i, "phase"), 0.f, 360.f);
            if (want (REnvelope)) { move (lid (i, "attack"), 0.f, 40.f); move (lid (i, "decay"), 5.f, 1200.f, true); move (lid (i, "sustain"), 0.f, 1.f); }
            if (rs.mode == RAll && ! rs.preserveGroups && r.nextFloat() < rs.intensity * 0.5f) p.setParamInUndoStep (lid (i, "group"), (float) r.nextInt (6));
            if (rs.mode == RAll && ! rs.preserveRouting && r.nextFloat() < rs.intensity * 0.5f) p.setParamInUndoStep (lid (i, "route_mode"), r.nextBool() ? 1.f : 0.f);
        }
    }

    // ---- factory presets (NOISE / TRANSIENT apply to a generator; LAYER stacks set the 8 layers)
    struct Preset { const char* category; const char* name; std::vector<std::pair<const char*, float>> values; };
    inline const std::vector<Preset>& noisePresets()
    {
        static const std::vector<Preset> l {
            { "NOISE", "WHITE IMPACT",   { { "type", 0 }, { "decay", 18 }, { "length", 60 }, { "filter", 1 }, { "cutoff", 1500 } } },
            { "NOISE", "DARK NOISE",     { { "type", 2 }, { "decay", 120 }, { "length", 300 }, { "color", -0.6f } } },
            { "NOISE", "METALLIC NOISE", { { "type", 5 }, { "res_freq", 3200 }, { "res_q", 0.7f }, { "decay", 80 } } },
            { "NOISE", "DIGITAL CRUSH",  { { "type", 6 }, { "bits", 4 }, { "sr_reduce", 0.7f }, { "grain", 0.3f }, { "decay", 90 } } },
            { "NOISE", "HIGH TEXTURE",   { { "type", 3 }, { "filter", 1 }, { "cutoff", 6000 }, { "decay", 200 }, { "length", 500 }, { "width", 0.8f } } },
            { "NOISE", "LOW TEXTURE",    { { "type", 1 }, { "filter", 0 }, { "cutoff", 900 }, { "decay", 250 }, { "length", 600 }, { "to_texture", -60 }, { "to_body", -3 } } },
        };
        return l;
    }
    inline const std::vector<Preset>& transientPresets()
    {
        static const std::vector<Preset> l {
            { "TRANSIENT", "CLASSIC CLICK",  { { "on", 1 }, { "source", 0 }, { "click_pitch", 2200 }, { "click_length", 6 } } },
            { "TRANSIENT", "HARD IMPACT",    { { "on", 1 }, { "source", 1 }, { "click_pitch", 3000 }, { "click_drive", 0.6f }, { "attack_level", 6 } } },
            { "TRANSIENT", "METALLIC SNAP",  { { "on", 1 }, { "source", 4 }, { "click_pitch", 4500 }, { "click_length", 12 } } },
            { "TRANSIENT", "SHORT PUNCH",    { { "on", 1 }, { "source", 3 }, { "click_pitch", 400 }, { "click_length", 5 }, { "low", 4 } } },
            { "TRANSIENT", "DEEP IMPACT",    { { "on", 1 }, { "source", 5 }, { "click_pitch", 900 }, { "click_length", 25 }, { "low", 6 } } },
            { "TRANSIENT", "DIGITAL ATTACK", { { "on", 1 }, { "source", 2 }, { "click_pitch", 6000 }, { "high", 6 } } },
        };
        return l;
    }
    struct Stack { const char* name; std::vector<std::vector<std::pair<const char*, float>>> layers; };
    inline const std::vector<Stack>& layerStacks()
    {
        static const std::vector<Stack> l {
            { "CLASSIC PUNCH STACK", { { { "group", 0 }, { "source", 0 }, { "pitch", 36 }, { "length", 40 }, { "decay", 15 } },
                                       { { "group", 0 }, { "source", 4 }, { "length", 20 }, { "decay", 6 }, { "filter_on", 1 }, { "filter_type", 1 }, { "cutoff", 2500 }, { "level", -14 } } } },
            { "MODERN BODY STACK",   { { { "group", 1 }, { "source", 2 }, { "pitch", 24 }, { "length", 220 }, { "decay", 90 }, { "filter_on", 1 }, { "cutoff", 2200 } },
                                       { { "group", 1 }, { "source", 1 }, { "pitch", 12 }, { "length", 260 }, { "decay", 120 }, { "level", -10 } } } },
            { "RAW TAIL TEXTURE",    { { { "group", 2 }, { "source", 4 }, { "length", 500 }, { "decay", 300 }, { "drive", 0.6f }, { "filter_on", 1 }, { "filter_type", 2 }, { "cutoff", 1800 } } } },
            { "METALLIC LAYER",      { { { "group", 4 }, { "source", 5 }, { "length", 200 }, { "decay", 80 }, { "width", 0.7f } } } },
            { "SUB FOUNDATION",      { { { "group", 3 }, { "source", 0 }, { "length", 600 }, { "env_mode", 0 }, { "sustain", 0.9f }, { "level", -8 } } } },
            { "EXPERIMENTAL STACK",  { { { "group", 4 }, { "source", 6 }, { "pitch", -12 }, { "length", 80 } },
                                       { { "group", 5 }, { "source", 3 }, { "pitch", 31 }, { "length", 150 }, { "drive", 0.8f }, { "width", 1 } },
                                       { { "group", 4 }, { "source", 5 }, { "offset", 60 }, { "length", 120 } } } },
        };
        return l;
    }
    inline void resetPrefix (GeProcessor& p, const juce::String& pre)
    {
        for (auto& id : prefixedIds (pre)) if (auto* prm = p.apvts.getParameter (id)) p.setParamInUndoStep (id, prm->convertFrom0to1 (prm->getDefaultValue()));
    }
    inline void applyNoise (GeProcessor& p, int gen, const Preset& pr)
    {
        const juce::String pre = gen == 0 ? "noiseA_" : "noiseB_";
        p.undo.beginNewTransaction ("Noise preset");
        resetPrefix (p, pre);
        p.setParamInUndoStep (pre + "on", 1.f);
        for (auto& [k, v] : pr.values) p.setParamInUndoStep (pre + k, v);
    }
    inline void applyTransient (GeProcessor& p, const Preset& pr)
    {
        p.undo.beginNewTransaction ("Transient preset");
        resetPrefix (p, "tr_");
        for (auto& [k, v] : pr.values) p.setParamInUndoStep (juce::String ("tr_") + k, v);
    }
    inline void applyStack (GeProcessor& p, const Stack& st)
    {
        p.undo.beginNewTransaction ("Layer stack");
        for (int i = 0; i < count; ++i) resetPrefix (p, lid (i, ""));
        for (size_t i = 0; i < st.layers.size() && i < (size_t) count; ++i)
        {
            p.setParamInUndoStep (lid ((int) i, "on"), 1.f);
            for (auto& [k, v] : st.layers[i]) p.setParamInUndoStep (lid ((int) i, k), v);
        }
    }

    // ---- files: one generator / the transient / one layer / the whole layer set (+ groups)
    inline std::unique_ptr<juce::XmlElement> prefixToXml (GeProcessor& p, const juce::String& tag, const juce::StringArray& prefixes)
    {
        auto x = std::make_unique<juce::XmlElement> (tag);
        for (auto& pre : prefixes)
            for (auto& id : prefixedIds (pre)) { auto* e = x->createNewChildElement ("PARAM"); e->setAttribute ("id", id); e->setAttribute ("value", value (p, id)); }
        return x;
    }
    inline bool xmlToParams (GeProcessor& p, const juce::XmlElement& x, const juce::String& renameFrom = {}, const juce::String& renameTo = {})
    {
        p.undo.beginNewTransaction ("Load");
        for (auto* e : x.getChildWithTagNameIterator ("PARAM"))
        {
            auto id = e->getStringAttribute ("id");
            if (renameFrom.isNotEmpty() && id.startsWith (renameFrom)) id = renameTo + id.substring (renameFrom.length());
            p.setParamInUndoStep (id, (float) e->getDoubleAttribute ("value"));
        }
        return true;
    }
    inline std::unique_ptr<juce::XmlElement> setToXml (GeProcessor& p)
    {
        auto x = std::make_unique<juce::XmlElement> ("LAYERSET4GE");
        for (int i = 0; i < count; ++i) x->addChildElement (layerToXml (p, i).release());
        x->addChildElement (prefixToXml (p, "GROUPS", { "group_", "bus_", "layer_solo_mode" }).release());
        return x;
    }
    inline bool xmlToSet (GeProcessor& p, const juce::XmlElement& x)
    {
        if (! x.hasTagName ("LAYERSET4GE")) return false;
        p.undo.beginNewTransaction ("Load layer set");
        int i = 0;
        for (auto* e : x.getChildWithTagNameIterator ("LAYER4GE")) if (i < count) xmlToLayer (p, i++, *e, false);
        if (auto* g = x.getChildByName ("GROUPS")) for (auto* e : g->getChildWithTagNameIterator ("PARAM")) p.setParamInUndoStep (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));
        return true;
    }
}
