#pragma once
// =====================================================================
//  4GE - KICK VARIATION (owner 2026-11-20: "not a plugin that randomizes parameters - a plugin that DESIGNS the next versions of a kick").
//  Message thread only (a tool: never the audio thread, never GUI code). The data = fge::kv::Workspace in the processor.
//   DNA        9 parts (PITCH / PUNCH / BODY / TAIL / WEIGHT / DIRT / LENGTH / SPACE / SCREECH), each KEEP (never changes),
//              EVOLVE (a small step - in the DIRECTION picked: DARKER / BRIGHTER, CLEANER / DIRTIER ...) or FREE (anything in the STYLE).
//   STYLE      EARLY / MODERN / RAW / EXPERIMENTAL / EUPHORIC / PVC / ZAAG = the world the values come from (its ranges + its template).
//   MUTATE     7 versions that PRESERVE THE IDENTITY OF THE BASE.   NEW KICK IN STYLE: 7 kicks BUILT FROM SCRATCH within the STYLE.
//   VARIATION  how far the versions go from the BASE.
//   MUTATE     7 new versions of the BASE at once (the level of each matched to the base after its render - louder is not better).
//   MORPH      from the BASE (A) to a version (B): numbers glide, switches jump at 50 % (one undo step per gesture).
//   HUMANIZE   = the parameter kv_humanize (every hit a tiny bit different, in the engine).
// =====================================================================
#include "VariantRandomizer.h"
#include "PageRandomizer.h"
#include "KickDna.h"
#include "StyleEngine.h"

namespace fge::kv
{
    inline juce::String geneTip (int g)
    {
        static const char* t[NumGenes] {
            "PITCH: the pitch drop (start, time, curve).", "PUNCH: the punch / tok (its type and its knobs).", "BODY: the body under the punch (the crunch).",
            "TAIL: the tail (its type, tone, resonance, colour).", "WEIGHT: the sub / the bass of the kick.", "DIRT: every drive and the clipper.",
            "LENGTH: the kick length and the part decays.", "SPACE: room and stereo width.", "SCREECH: the screech layer (only where the style has one)." };
        return t[juce::jlimit (0, (int) NumGenes - 1, g)];
    }

    struct Settings
    {
        Dna mode = defaultDna();
        Dirs dir {};                        // EVOLVE: -1 / 0 (any) / +1 per part
        float variation = 0.5f;             // 0.05..1 (= vrAmount)
        int seed = 1;
    };
    inline Settings load (GeProcessor& p)
    {
        Settings s; auto t = p.apvts.state;
        s.mode = loadDna (t);
        s.variation = fge::pagernd::strength (p);                // ONE amount: VARIATION = RANDOMIZE + every page dice (vrAmount)
        s.dir = loadDirs (t);
        s.seed = juce::jmax (1, (int) t.getProperty ("kvSeed", 1));
        return s;
    }
    inline void save (GeProcessor& p, const Settings& s)
    {
        auto t = p.apvts.state; saveDna (t, s.mode);
        saveDirs (t, s.dir); t.setProperty ("kvSeed", s.seed, nullptr);
    }

    // ---- which parameters belong to which gene (the union of every style's ranges + a few of each gene's own)
    inline const std::array<std::vector<int>, NumGenes>& geneIds()
    {
        static const std::array<std::vector<int>, NumGenes> g = []
        {
            std::array<std::vector<int>, NumGenes> x;
            auto add = [&x] (int gene, const juce::String& id) { const int k = fge::modstate::indexOf (id); if (k >= 0 && std::find (x[(size_t) gene].begin(), x[(size_t) gene].end(), k) == x[(size_t) gene].end()) x[(size_t) gene].push_back (k); };
            using namespace fge::variant;
            for (int st = 0; st < NumStyles; ++st)
                for (int a = 0; a < NumAreas; ++a)
                    for (auto& r : table (st)[(size_t) a])
                    {
                        const juce::String id (r.id);
                        add (geneOf (id, a), id);
                    }
            for (auto* id : { "core_length", "tail_decay", "sub_length", "body_decay" }) add (GLength, id);
            for (auto* id : { "tail_room", "tail_room_decay", "spc_punch_width", "spc_body_width", "spc_tail_width", "spc_master_width", "tl_space", "tl_width" }) add (GSpace, id);
            for (auto* id : { "tail_drive", "body_drive", "punch_drive", "sub_drive", "tl_drive", "master_clip_drive" }) add (GDirt, id);
            for (auto* id : { "tail_tone", "tail_resonance", "tl_tone", "tl_resonance", "tl_movement" }) add (GTail, id);
            for (auto* id : { "scr_base_hz", "scr_fm_index", "scr_metal", "scr_vowel", "scr_fb_amount", "scr_mix_tonal", "scr_mix_metal", "scr_mix_formant" }) add (GScreech, id);
            return x;
        }();
        return g;
    }
    // the STYLE's range for an id (none: the generic musical target)
    struct Range { bool has = false; float lo = 0.f, hi = 0.f; std::vector<int> choices; };
    inline Range rangeFor (int style, const char* id)
    {
        Range r;
        if (style < 0 || style >= fge::variant::NumStyles) return r;
        for (auto& area : fge::variant::table (style))
            for (auto& x : area) if (juce::String (x.id) == id) { r.has = true; r.lo = x.lo; r.hi = x.hi; r.choices = x.choices; return r; }
        return r;
    }
    inline int styleOf (GeProcessor& p) { return fge::variant::load (p).style; }      // the STYLE on the VARIATION page (= RANDOMIZE's)
    inline void setStyle (GeProcessor& p, int st) { p.apvts.state.setProperty ("vrStyle", juce::jlimit (0, fge::variant::NumStyles - 1, st), nullptr); }


    // ---- one version of the base (pure: the same base + settings + seed = the same version)
    inline ParamFrame mutateOne (const ParamFrame& base, const Settings& s, int style, uint32_t seed)
    {
        juce::Random r ((juce::int64) seed * 7919 + 17);
        ParamFrame f = base;
        for (int g = 0; g < NumGenes; ++g)
        {
            const int m = s.mode[(size_t) g];
            const int dir = m == Evolve ? s.dir[(size_t) g] : 0;
            const float amt = s.variation * (m == Evolve ? evolveShare * (dir != 0 ? 0.5f : 1.f) : 1.f);   // (with a direction: the rest of the part only drifts)
            const float way = r.nextFloat();
            if (m != Keep && dir != 0 && s.variation > 0.f) push (f, g, (float) dir * s.variation * (0.2f + 0.5f * way));
            for (int k : geneIds()[(size_t) g])
            {
                const float u = r.nextFloat(), w = r.nextFloat(), spread = 0.45f + 0.55f * r.nextFloat();     // (drawn always: a KEEP part never shifts the others)
                if (m == Keep || amt <= 0.f) continue;
                const auto& d = def (k);
                if (d.kind == Kind::Bool) continue;
                if (dir != 0 && isDirId (g, d.id)) continue;                                       // (already moved that way)
                const Range rg = rangeFor (style, d.id);
                if (d.kind == Kind::Choice || ! rg.choices.empty())
                {   // a type / a fixed value: only FREE, only to a value the style allows
                    if (m != Free || rg.choices.empty() || w > amt * 0.6f) continue;
                    f[k] = (float) rg.choices[(size_t) juce::jlimit (0, (int) rg.choices.size() - 1, (int) (u * (float) rg.choices.size()))];
                    continue;
                }
                float target;
                if (rg.has) target = rg.lo + (rg.hi - rg.lo) * u;
                else { juce::Random tr ((juce::int64) (u * 1.0e6f) + 3); target = fge::pagernd::target (k, tr); }
                float v = f[k] + (target - f[k]) * amt * spread;
                if (d.unit == Unit::None && d.max - d.min > 4.f) v = std::round (v);
                f[k] = juce::jlimit (d.min, d.max, v);
            }
        }
        return f;
    }
    // NEW KICK IN STYLE: built from scratch within the STYLE - its template (generators, pitch drop, distortion chain ...) + a roll of its
    // ranges. Your LENGTH / KICK END / macros / global stay (the style never sets lengths); nothing of the BASE's character is kept.
    inline ParamFrame freshOne (const ParamFrame& current, int style, uint32_t seed)
    {
        ParamFrame f = fge::style::onFrame (current, style);
        Settings s; s.mode.fill (Free); s.mode[GLength] = Keep; s.variation = 0.7f;
        return mutateOne (f, s, style, seed);
    }
    // how far a frame is from the base: all genes (0..100 %), or one gene
    inline float distance (const ParamFrame& a, const ParamFrame& b, int gene = -1)
    {
        double e = 0.0; int n = 0;
        for (int g = 0; g < NumGenes; ++g)
        {
            if (gene >= 0 && g != gene) continue;
            for (int k : geneIds()[(size_t) g])
            {
                const auto& d = def (k);
                const float dn = d.kind == Kind::Float ? norm (k, a[k]) - norm (k, b[k]) : (std::lround (a[k]) != std::lround (b[k]) ? 0.5f : 0.f);
                e += (double) dn * dn; ++n;
            }
        }
        return n == 0 ? 0.f : juce::jlimit (0.f, 100.f, (float) (std::sqrt (e / n) * 250.0));
    }
    // a short text of the gene's main value (the DNA list)
    inline juce::String readout (const ParamFrame& f, int gene)
    {
        auto val = [&f] (const char* id) { const int k = fge::modstate::indexOf (id); return k < 0 ? juce::String() : formatValue (def (k), f[k]); };
        auto cho = [&f] (const char* id) { const int k = fge::modstate::indexOf (id); return k < 0 ? juce::String() : def (k).choices()[f.idx (k)]; };
        switch (gene)
        {
            case GPitch:   return "Start " + val ("pe_start");
            case GPunch:   return cho ("punch_mode");
            case GBody:    return "Level " + val ("body_level");
            case GTail:    return cho ("tail_engine");
            case GWeight:  return "Sub " + val ("sub_level");
            case GDirt:    return "Clip " + val ("master_clip_drive");
            case GLength:  return juce::String (juce::roundToInt (kickLengthMs (f, 150.0))) + " ms";
            case GSpace:   return "Room " + val ("tail_room");
            case GScreech: return f.on (P::scrOn) ? juce::String ("On") : juce::String ("Off");
            default:       return {};
        }
    }

    // ---- applying frames to the plugin
    inline bool differs (float a, float b) { return std::abs (a - b) > 1.0e-6f * juce::jmax (1.f, std::abs (a)); }
    inline void applyFrame (GeProcessor& p, const ParamFrame& to, const juce::String& undoName)
    {
        p.beginUndoStep (undoName);
        const auto cur = p.frame();
        for (int k = 0; k < P::count; ++k) if (differs (to[k], cur[k])) p.setParamInUndoStep (def (k).id, to[k]);
    }
    inline void setLive (GeProcessor& p, const ParamFrame& to)       // (during a MORPH drag: no undo steps)
    {
        const auto cur = p.frame();
        for (int k = 0; k < P::count; ++k)
            if (differs (to[k], cur[k])) if (auto* a = p.apvts.getParameter (def (k).id)) a->setValueNotifyingHost (a->convertTo0to1 (to[k]));
    }
    inline void commit (GeProcessor& p, const ParamFrame& start, const ParamFrame& to, const juce::String& undoName)   // one undo step: start -> to
    {
        p.beginUndoStep (undoName);
        for (int k = 0; k < P::count; ++k)
            if (differs (to[k], start[k]))
            {
                if (auto* a = p.apvts.getParameter (def (k).id)) a->setValueNotifyingHost (a->convertTo0to1 (start[k]));
                p.setParamInUndoStep (def (k).id, to[k]);
            }
    }
    inline ParamFrame morphFrame (const ParamFrame& a, const ParamFrame& b, float t)
    {
        ParamFrame f = a;
        for (int k = 0; k < P::count; ++k)
            f[k] = def (k).kind == Kind::Float ? a[k] + (b[k] - a[k]) * t : (t < 0.5f ? a[k] : b[k]);
        return f;
    }

    // ---- the actions
    inline void ensureBase (GeProcessor& p) { auto& w = p.kvWork; if (! w.hasBase) { w.base = p.frame(); w.hasBase = true; } }
    inline void select (GeProcessor& p, int v)            // -1 = BASE
    {
        auto& w = p.kvWork;
        if (! w.hasBase) return;
        if (v >= (int) w.versions.size()) return;
        applyFrame (p, v < 0 ? w.base : w.versions[(size_t) v], v < 0 ? juce::String ("Back to base") : "Version V" + juce::String (v + 1));
        w.selected = v;
        if (v >= 0) { w.morphB = v; w.morph = 1.f; } else w.morph = 0.f;
    }
    inline void setAsBase (GeProcessor& p) { auto& w = p.kvWork; w.base = p.frame(); w.hasBase = true; w.selected = -1; w.morphB = -1; w.morph = 0.f; }   // (then a new MUTATE)
}
