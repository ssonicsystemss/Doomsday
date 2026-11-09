#pragma once
// =====================================================================
//  4GE module 13 - STYLE ENGINE (message thread only: a preset-like tool, never DSP / GUI code)
//   EARLY / MODERN / RAW / EXPERIMENTAL = a STARTING TEMPLATE for the character of the kick (RULES.md 5): the core generators
//   (PUNCH / BODY / TAIL / SUB), the pitch drop, noise / transient, screech, the master clip and the owner's default DISTORTION CHAIN
//   per style (EARLY SOFT -> TUBE, MODERN HARD -> SOFT -> DIGITAL, RAW ASYMMETRIC -> HARD -> WAVEFOLD, EXPERIMENTAL WAVEFOLD -> RING
//   (rectifier, until module 05 has a ring mod stage) -> CUSTOM).
//   Rules: STYLE NEVER TOUCHES LENGTH (no *_length / *_decay / *_release / AUTO LENGTH - module 14 is independent), never the layers,
//   samples, EQ, dynamics, modulation, stereo, macros or the master output. Nothing is locked: every knob stays free afterwards.
//   KEEP MY CHANGES (default): a parameter the user moved himself (a gesture on a knob / host automation) is never overwritten -
//   the processor remembers those ("touched", saved with the project, cleared by loading a preset). One change of style = one UNDO.
// =====================================================================
#include "../PluginProcessor.h"

namespace fge::style
{
    enum Style { Early = 0, Modern, Raw, Experimental, NumStyles };
    inline const juce::StringArray& names() { static const juce::StringArray a { "EARLY", "MODERN", "RAW", "EXPERIMENTAL" }; return a; }
    inline const char* description (int s)
    {
        static const char* d[NumStyles] {
            "Classic, melodic hardstyle: a springy punch, a tonal body, simpler warm distortion (SOFT CLIP -> TUBE) and a clear pitch drop.",
            "Strong, clean and controlled: a precise transient, layered body, a polished tail and dense, precise distortion (HARD -> SOFT -> DIGITAL CLIP).",
            "Aggressive: heavy asymmetric distortion (ASYMMETRIC -> HARD -> WAVEFOLD), a sharp screech, strong harmonics, the bass entering early.",
            "Unusual but still a kick: a double drop, a metallic tail, folds / a rectifier / a custom shaper blended into a solid core." };
        return d[juce::jlimit (0, NumStyles - 1, s)];
    }
    inline int indexOf (const juce::String& id) { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; }

    // ---- what a style may set (and what it never touches)
    inline bool isLength (const juce::String& id)
    {
        return id == "core_length" || id == "core_auto_length" || id.endsWith ("_length") || id.endsWith ("_decay") || id.endsWith ("_release")
               || id.endsWith ("_sustain") || id.contains ("_length_") || id.endsWith ("_hold");
    }
    inline bool inScope (int i)
    {
        const juce::String id (def (i).id);
        if (isLength (id)) return false;
        for (auto* p : { "punch_", "body_", "tail_", "sub_", "pe_", "noiseA_", "noiseB_", "tr_", "scr_" }) if (id.startsWith (p)) return ! id.startsWith ("tail_penv_draw");
        if (id.startsWith ("dist")) return true;                         // the whole distortion pool (dist1..16 + dist_ globals)
        return id == "master_clip_drive";
    }

    // ---- the templates: (id, plain value) over the defaults of everything in scope
    struct Stage { int loc; std::vector<std::pair<const char*, float>> v; };
    struct Template { std::vector<std::pair<const char*, float>> values; std::vector<Stage> chain; };
    inline const Template& templ (int s)
    {
        namespace dd = fge::dsp::dist;
        static const std::array<Template, NumStyles> t {
            // EARLY - from "EARLY - Short Tonal" (the tonal v0.4 match), the drive brought down, warm SOFT -> TUBE chain on the tail
            Template { { { "tail_character", 5 }, { "tail_source", 2 }, { "tail_st2_type", 2 }, { "pe_start", 240 }, { "pe_time", 160 }, { "pe_curve", 0.15f },
                         { "tail_drive", 0.55f }, { "tail_tone", 4463 }, { "tail_resonance", 0.06f }, { "body_level", -5.7f }, { "body_pitch", 187.7f },
                         { "sub_level", -8 }, { "punch_level", 0 }, { "punch_pitch", 162 }, { "punch_drive", 0.6f }, { "tail_level", 0 }, { "tail_uni_drift", 14 },
                         { "tail_uni_mix", 1 }, { "tail_st2_drive", 14 }, { "tail_mid_hz", 596 }, { "tail_mid_gain", 8 }, { "tail_low_cut", 40 },
                         { "tail_high_cut", 19000 }, { "master_clip_drive", 2 }, { "tail_attack", 3.6f }, { "tail_hold_level", 0 }, { "punch_source", 5 } },
                       { { dd::LocTail, { { "type", dd::Soft }, { "drive", 0.35f }, { "shape", 0.5f } } }, { dd::LocTail, { { "type", dd::Tube }, { "drive", 0.3f } } } } },
            // MODERN - from "MODERN - Festival Drive", HARD -> SOFT -> DIGITAL CLIP on the tail (controlled), the bass in at 75 ms
            Template { { { "tail_character", 1 }, { "tail_source", 0 }, { "tail_st2_type", 0 }, { "pe_start", 483 }, { "pe_time", 15.5f }, { "tail_penv_offset", 126 },
                         { "pe_curve", 0.8f }, { "tail_drive", 0.9f }, { "tail_tone", 10188 }, { "tail_resonance", 0.45f }, { "body_level", -5.2f },
                         { "body_pitch", 159.4f }, { "sub_level", -0.7f }, { "punch_level", 4 }, { "punch_pitch", 453 }, { "punch_drive", 0.6f }, { "tail_level", -6.9f },
                         { "tail_uni_detune", 22.8f }, { "tail_uni_drift", 13.8f }, { "tail_uni_mix", 0.76f }, { "tail_st2_drive", 12 }, { "tail_mid_hz", 1800 },
                         { "tail_mid_gain", 4 }, { "tail_low_cut", 56 }, { "tail_high_cut", 19000 }, { "master_clip_drive", 0 }, { "tail_attack", 5.2f },
                         { "tail_hold_level", -4 }, { "sub_amp_env_delay", 75 }, { "body_amp_env_delay", 0 }, { "body_source", 4 }, { "body_tone", 7000 },
                         { "punch_source", 5 }, { "noiseB_on", 1 }, { "noiseB_type", 0 }, { "noiseB_env", 3 }, { "noiseB_to_texture", -60 }, { "noiseB_to_tail", -35.2f },
                         { "noiseB_level", 0 }, { "noiseB_filter", 0 }, { "noiseB_cutoff", 12034 } },
                       { { dd::LocTail, { { "type", dd::Hard }, { "drive", 0.3f }, { "ceiling", -1 } } }, { dd::LocTail, { { "type", dd::Soft }, { "drive", 0.25f } } },
                         { dd::LocTail, { { "type", dd::Digital }, { "drive", 0.2f }, { "ceiling", -1 } } } } },
            // RAW - from "RAW - Buzz Saw", ASYMMETRIC -> HARD -> WAVEFOLD, a screech, the bass under the tok (55 ms: "lepiej buja")
            Template { { { "tail_character", 1 }, { "tail_source", 0 }, { "tail_st2_type", 0 }, { "pe_start", 820 }, { "pe_time", 12 }, { "tail_penv_offset", 110 },
                         { "pe_curve", 0.72f }, { "tail_drive", 1 }, { "tail_tone", 11932 }, { "tail_resonance", 0.43f }, { "body_level", -1.05f },
                         { "body_pitch", 124.1f }, { "sub_level", -0.06f }, { "punch_level", -2.8f }, { "punch_pitch", 303.6f }, { "punch_drive", 1 }, { "tail_level", -3.3f },
                         { "tail_uni_detune", 17.4f }, { "tail_uni_drift", 5.8f }, { "tail_uni_mix", 0.49f }, { "tail_st2_drive", 12.8f }, { "tail_mid_hz", 1800 },
                         { "tail_mid_gain", 5 }, { "tail_low_cut", 150 }, { "tail_high_cut", 18917 }, { "master_clip_drive", 0 }, { "tail_attack", 3.6f },
                         { "tail_hold_level", -6.4f }, { "sub_amp_env_delay", 55 }, { "body_amp_env_delay", 0 }, { "body_source", 4 }, { "body_tone", 7000 },
                         { "punch_source", 5 }, { "noiseB_on", 1 }, { "noiseB_type", 1 }, { "noiseB_env", 3 }, { "noiseB_to_texture", -60 }, { "noiseB_to_tail", -27.7f },
                         { "noiseB_level", 0 }, { "noiseB_filter", 0 }, { "noiseB_cutoff", 4204 }, { "scr_on", 1 }, { "scr_level", -14 } },
                       { { dd::LocTail, { { "type", dd::Asym }, { "drive", 0.55f }, { "bias", 0.2f } } }, { dd::LocTail, { { "type", dd::Hard }, { "drive", 0.35f } } },
                         { dd::LocTail, { { "type", dd::Wavefold }, { "drive", 0.3f }, { "mix", 0.6f } } } } },
            // EXPERIMENTAL - a WORKING kick (the MODERN core: click, tok, clean sub, noise density) + the unusual part: a DOUBLE DROP, a METALLIC
            // character and a WAVEFOLD -> RECTIFIER -> CUSTOM chain blended in, not a wall of high noise (owner 2026-11-09: the old one "tragedia")
            Template { { { "tail_character", 4 }, { "tail_source", 2 }, { "tail_st2_type", 0 }, { "pe_mode", 3 }, { "pe_start", 520 }, { "pe_time", 90 },
                         { "tail_penv_offset", 60 }, { "pe_curve", 0.6f }, { "tail_drive", 0.75f }, { "tail_tone", 7000 }, { "tail_resonance", 0.3f },
                         { "body_level", -6 }, { "body_pitch", 150 }, { "sub_level", -1 }, { "punch_level", 2 }, { "punch_pitch", 600 }, { "punch_drive", 0.6f },
                         { "tail_level", -5 }, { "tail_uni_detune", 28 }, { "tail_uni_drift", 30 }, { "tail_uni_mix", 0.7f }, { "tail_st2_drive", 10 },
                         { "tail_low_cut", 80 }, { "tail_high_cut", 16000 }, { "master_clip_drive", 0 }, { "tail_attack", 4 }, { "tail_hold_level", -3 },
                         { "sub_amp_env_delay", 50 }, { "body_source", 4 }, { "body_tone", 6000 }, { "punch_source", 5 }, { "noiseB_on", 1 }, { "noiseB_type", 1 },
                         { "noiseB_env", 3 }, { "noiseB_to_texture", -60 }, { "noiseB_to_tail", -30 }, { "noiseB_level", 0 }, { "noiseB_cutoff", 8000 } },
                       { { dd::LocTail, { { "type", dd::Wavefold }, { "drive", 0.3f }, { "mix", 0.45f } } }, { dd::LocTail, { { "type", dd::Rectifier }, { "drive", 0.25f }, { "mix", 0.25f } } },
                         { dd::LocTail, { { "type", dd::Custom }, { "drive", 0.25f }, { "mix", 0.35f } } } } },
        };
        return t[(size_t) juce::jlimit (0, NumStyles - 1, s)];
    }
    // every in-scope parameter's value for a style (defaults + the template + the chain)
    inline std::vector<std::pair<int, float>> values (int s)
    {
        std::vector<float> v ((size_t) P::count, 0.f);
        std::vector<bool> scope ((size_t) P::count, false);
        for (int i = 0; i < P::count; ++i) if (inScope (i)) { scope[(size_t) i] = true; v[(size_t) i] = def (i).def; }
        const auto& t = templ (s);
        for (auto& [id, x] : t.values) if (const int i = indexOf (id); i >= 0 && scope[(size_t) i]) v[(size_t) i] = x;
        std::array<int, 16> ord {};
        for (size_t k = 0; k < t.chain.size(); ++k)
        {
            const juce::String pre = "dist" + juce::String ((int) k + 1) + "_";
            auto set = [&] (const juce::String& key, float x) { if (const int i = indexOf (pre + key); i >= 0) v[(size_t) i] = x; };
            set ("on", 1.f); set ("location", (float) t.chain[k].loc); set ("order", (float) ++ord[(size_t) t.chain[k].loc]);
            for (auto& [key, x] : t.chain[k].v) set (key, x);
        }
        std::vector<std::pair<int, float>> out;
        for (int i = 0; i < P::count; ++i) if (scope[(size_t) i]) out.push_back ({ i, v[(size_t) i] });
        return out;
    }
    // the same on a frame (PREVIEW: what the style would sound like, nothing changed)
    inline ParamFrame onFrame (ParamFrame f, int s, const std::function<bool (int)>& keep = {})
    {
        for (auto& [i, x] : values (s)) if (! keep || ! keep (i)) f[i] = x;
        return f;
    }

    struct Report { int changed = 0, kept = 0; juce::StringArray keptNames; };
    // apply on the plugin: one UNDO step; KEEP MY CHANGES leaves the touched parameters alone
    inline Report apply (GeProcessor& p, int s, bool keepTouched)
    {
        Report r;
        p.beginUndoStep ("Style " + names()[s]);
        for (auto& [i, x] : values (s))
        {
            auto* prm = p.apvts.getParameter (def (i).id);
            if (prm == nullptr) continue;
            if (keepTouched && p.isTouched (i))
            {
                if (std::abs (p.apvts.getRawParameterValue (def (i).id)->load() - x) > 1.0e-6f) { ++r.kept; r.keptNames.add (def (i).name); }
                continue;
            }
            if (std::abs (p.apvts.getRawParameterValue (def (i).id)->load() - x) > 1.0e-6f) { p.setParamInUndoStep (def (i).id, x); ++r.changed; }
        }
        p.apvts.state.setProperty ("style", s, &p.undo);
        return r;
    }
}
