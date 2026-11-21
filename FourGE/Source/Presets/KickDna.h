#pragma once
// =====================================================================
//  4GE - the kick's DNA (owner 2026-11-20): 9 GENES, each KEEP / EVOLVE / FREE (state property "kvGenes").
//  ONE DNA for the whole plugin: KICK VARIATION (MUTATE) and the RANDOMIZE button both obey it.
//  Message thread only (tools); no GUI, no DSP.
// =====================================================================
#include "../PluginProcessor.h"
#include "ModState.h"
#include <array>

namespace fge::kv
{
    enum Gene { GPitch = 0, GPunch, GBody, GTail, GWeight, GDirt, GLength, GSpace, GScreech, NumGenes };
    enum Mode { Keep = 0, Evolve, Free };
    using Dna = std::array<int, NumGenes>;
    inline const juce::StringArray& geneNames() { static const juce::StringArray a { "PITCH", "PUNCH", "BODY", "TAIL", "WEIGHT", "DIRT", "LENGTH", "SPACE", "SCREECH" }; return a; }
    inline const juce::StringArray& modeNames() { static const juce::StringArray a { "KEEP", "EVOLVE", "FREE" }; return a; }
    inline Dna defaultDna() { Dna d; d.fill (Free); return d; }        // (all FREE = RANDOMIZE as it always was)
    inline Dna loadDna (const juce::ValueTree& state)
    {
        Dna d = defaultDna();
        const juce::String g = state.getProperty ("kvGenes", "").toString();
        for (int i = 0; i < NumGenes && i < g.length(); ++i) d[(size_t) i] = juce::jlimit (0, 2, (int) (g[i] - '0'));
        return d;
    }
    inline void saveDna (juce::ValueTree state, const Dna& d) { juce::String g; for (int m : d) g << juce::String (m); state.setProperty ("kvGenes", g, nullptr); }
    // the DIRECTION of an EVOLVE part (owner 2026-11-20): -1 / 0 = any / +1 (TAIL: DARKER / BRIGHTER, DIRT: CLEANER / DIRTIER ...)
    using Dirs = std::array<int, NumGenes>;
    inline Dirs loadDirs (const juce::ValueTree& state)
    {
        Dirs d {}; const juce::String g = state.getProperty ("kvDirs", "").toString();
        for (int i = 0; i < NumGenes && i < g.length(); ++i) d[(size_t) i] = juce::jlimit (-1, 1, (int) (g[i] - '1'));
        return d;
    }
    inline void saveDirs (juce::ValueTree state, const Dirs& d) { juce::String g; for (int m : d) g << juce::String (m + 1); state.setProperty ("kvDirs", g, nullptr); }
    inline juce::String dirName (int gene, bool up)
    {
        static const char* n[NumGenes][2] { { "LOWER", "HIGHER" }, { "SOFTER", "HARDER" }, { "LESS", "MORE" }, { "DARKER", "BRIGHTER" }, { "LIGHTER", "HEAVIER" },
                                            { "CLEANER", "DIRTIER" }, { "SHORTER", "LONGER" }, { "NARROWER", "WIDER" }, { "LESS", "MORE" } };
        return n[juce::jlimit (0, (int) NumGenes - 1, gene)][up ? 1 : 0];
    }
    // EVOLVE = this part of the step (and never a type / switch); FREE = all of it
    constexpr float evolveShare = 0.4f;
    // the parameters each DIRECTION moves: (id, +1 = this way when going UP)
    inline const std::vector<std::pair<const char*, float>>& dirIds (int gene)
    {
        using V = std::vector<std::pair<const char*, float>>;
        static const std::array<V, NumGenes> t {
            V { { "pe_start", 1.f }, { "zg_start", 1.f }, { "pvc_jump", 1.f }, { "pt_start", 1.f } },                                                   // PITCH  lower / higher
            V { { "punch_level", 1.f }, { "punch_transient", 1.f }, { "psh_drive", 1.f }, { "psh_click", 1.f }, { "cp_sat", 0.6f }, { "cp_trans_ctrl", 1.f },   // PUNCH  softer / harder
                { "zg_piep", 1.f }, { "zg_trask", 1.f }, { "zg_click", 1.f }, { "punch_attack", -1.f } },
            V { { "body_level", 1.f }, { "body_resonance", 0.5f }, { "zg_body", 1.f } },                                                                 // BODY   less / more
            V { { "tail_tone", 1.f }, { "tail_mid_gain", 0.5f }, { "tail_color_gain", 0.6f }, { "tl_tone", 1.f }, { "pvc_bright", 1.f }, { "pt_high", 1.f },  // TAIL   darker / brighter
                { "zg_character", 0.6f }, { "zg_resonance", 0.5f }, { "tail_room_tone", 0.5f } },
            V { { "sub_level", 1.f }, { "sub_drive", 0.4f }, { "zg_sub", 1.f }, { "zg_bass", 1.f }, { "psh_sub", 0.6f } },                               // WEIGHT lighter / heavier
            V { { "tail_drive", 1.f }, { "body_drive", 1.f }, { "punch_drive", 1.f }, { "master_clip_drive", 1.f }, { "tail_st2_drive", 1.f },          // DIRT   cleaner / dirtier
                { "tl_drive", 1.f }, { "zg_grit", 1.f }, { "zg_crush", 1.f }, { "pt_drive1", 1.f }, { "pt_drive2", 1.f }, { "pvc_drive", 1.f } },
            V { { "core_length", 1.f }, { "tail_decay", 0.6f }, { "sub_length", 0.6f }, { "zg_decay", 1.f } },                                        // LENGTH shorter / longer
            V { { "spc_tail_width", 1.f }, { "spc_body_width", 0.6f }, { "tl_width", 1.f }, { "zg_stereo", 1.f }, { "tail_room", 0.6f }, { "tl_space", 0.6f } },   // SPACE narrower / wider
            V { { "scr_level", 1.f }, { "scr_fb_amount", 0.8f }, { "scr_metal", 0.6f }, { "scr_fm_index", 0.6f } } };                                    // SCREECH less / more
        return t[(size_t) juce::jlimit (0, (int) NumGenes - 1, gene)];
    }
    inline bool isDirId (int gene, const juce::String& id)
    {
        for (auto& x : dirIds (gene)) if (id == x.first) return true;
        return false;
    }
    // normalized (0..1 over the knob's travel, with its skew - the same range as the APVTS; no processor needed: thread-safe)
    inline juce::NormalisableRange<float> rangeOf (int k)
    {
        const auto& d = def (k);
        juce::NormalisableRange<float> r (d.min, juce::jmax (d.min + 1.0e-6f, d.max));
        if (d.kind == Kind::Float && d.centre > d.min && d.centre < d.max) r.setSkewForCentre (d.centre);
        return r;
    }
    inline float norm (int k, float v) { return rangeOf (k).convertTo0to1 (juce::jlimit (def (k).min, def (k).max, v)); }
    inline float plain (int k, float n) { return rangeOf (k).convertFrom0to1 (juce::jlimit (0.f, 1.f, n)); }
    // move a part that way on a frame
    inline void push (ParamFrame& f, int gene, float amount)
    {
        for (auto& [id, sign] : dirIds (gene))
        {
            const int k = fge::modstate::indexOf (id);
            if (k < 0 || def (k).kind != Kind::Float) continue;
            f[k] = plain (k, norm (k, f[k]) + amount * sign);
        }
        if (gene == GSpace && amount > 0.f)                       // WIDER: the tail's stereo unit has to be on to widen anything
            if (const int k = fge::modstate::indexOf ("spc_tail_on"); k >= 0) f[k] = 1.f;
    }
    // which gene a parameter belongs to ('area' = the randomizer area it was listed under: PUNCH BODY TAIL SUB PITCH DRIVE SCREECH)
    inline bool isLength (const juce::String& id)
    {
        for (auto* k : { "core_length", "tail_decay", "sub_length", "body_decay", "punch_length", "pvc_length", "gt_gate_len", "zg_decay" }) if (id == k) return true;
        return false;
    }
    inline bool isSpace (const juce::String& id) { return id.startsWith ("tail_room") || id == "tl_space" || id == "tl_width" || id.startsWith ("spc_") || id == "tail_uni_voices"; }
    inline bool isDirt (const juce::String& id) { return id.contains ("drive") && ! id.startsWith ("scr_"); }
    inline int geneOf (const juce::String& id, int area)
    {
        static const int areaGene[7] { GPunch, GBody, GTail, GWeight, GPitch, GDirt, GScreech };
        return isLength (id) ? GLength : isSpace (id) ? GSpace : isDirt (id) ? GDirt : areaGene[juce::jlimit (0, 6, area)];
    }
}
