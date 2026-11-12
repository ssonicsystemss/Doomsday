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
        if (id.startsWith ("punch_")) return false;                    // PUNCH / TOK: the tok's own length / envelope is its character, not the kick length
        if (id.startsWith ("tail_room")) return false;                 // the TAIL ROOM decay is its character (it never makes the kick longer)
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
            // (all styles, owner 2026-11-10 "słychać mocno moment przejścia z toka na tail"): the chain runs on the TAIL and again on the PUNCH (the tok),
            // the tail starts from the tok note in phase, an equal-power OVERLAP crossfade - the timbre does not break at the hand-over
            // (all styles, owner 2026-11-09 "BODY robi łup łup, goofy"): the BODY has no pitch slide of its own - it FOLLOWS the tail at a fixed
            // interval (0 / +7 / +12 st), so it thickens the tail instead of a separate 808-like "boing"; it starts with the tail after the tok
            // EARLY - a classic PUNCH (a short thump that falls into the kick, the tail from the hit - no tonal tok "pup", owner 2026-11-10); from "EARLY - Short Tonal" (the tonal v0.4 match), the drive brought down, warm SOFT -> TUBE chain on the tail
            Template { { { "body_resonance", 0.1f }, { "body_tone", 3000 }, { "body_drive", 0.3f }, { "body_source", 1 }, { "body_follow_offset", 0 }, { "body_follow_depth", 0 }, { "body_link_source", 2 }, { "body_link", 1 }, { "punch_overlap", 20 }, { "punch_transient_tone", 4000 }, { "punch_transient", 0.4f }, { "punch_release", 5 }, { "punch_sustain", 0 }, { "punch_decay", 16 }, { "punch_attack", 0 }, { "punch_tone", 6000 }, { "punch_length", 28 }, { "punch_tok_decay", -2 }, { "punch_tok_bend", 0 }, { "punch_note_offset", 12 }, { "tail_start_mode", 0 }, { "tail_start", 100 }, { "punch_note_mode", 0 }, { "punch_mode", 0 }, { "tail_character", 5 }, { "tail_source", 2 }, { "tail_st2_type", 2 }, { "pe_start", 349.23f }, { "tail_room", 0.4f }, { "tail_room_decay", 450 }, { "tail_room_tone", 4500 }, { "pe_start_mode", 1 }, { "pe_time", 130 }, { "pe_curve", 0.15f },
                         { "tail_drive", 0.55f }, { "tail_tone", 4463 }, { "tail_resonance", 0.06f }, { "body_level", -8 }, { "body_pitch", 187.7f },
                         { "sub_level", -8 }, { "punch_level", -3 }, { "punch_pitch", 1000 }, { "punch_glide", 4 }, { "punch_drive", 0.6f }, { "tail_level", 0 }, { "tail_uni_drift", 14 },
                         { "tail_uni_mix", 1 }, { "tail_st2_drive", 14 }, { "tail_mid_hz", 596 }, { "tail_mid_gain", 8 }, { "tail_low_cut", 40 },
                         { "tail_high_cut", 19000 }, { "master_clip_drive", 2 }, { "tail_attack", 3.6f }, { "tail_hold_level", 0 }, { "punch_source", 0 } },
                       { { dd::LocTail, { { "type", dd::Soft }, { "drive", 0.35f }, { "shape", 0.5f } } }, { dd::LocTail, { { "type", dd::Tube }, { "drive", 0.3f } } },
                         { dd::LocPunch, { { "type", dd::Soft }, { "drive", 0.35f }, { "shape", 0.5f } } }, { dd::LocPunch, { { "type", dd::Tube }, { "drive", 0.3f } } } } },
            // MODERN - from "MODERN - Festival Drive", HARD -> SOFT -> DIGITAL CLIP on the tail (controlled), the bass in at 75 ms
            Template { { { "body_resonance", 0.15f }, { "body_drive", 0.25f }, { "body_follow_offset", 12 }, { "body_follow_depth", 0 }, { "body_link_source", 2 }, { "body_link", 1 }, { "noiseB_to_punch", -24 }, { "punch_overlap", 20 }, { "punch_transient_tone", 6000 }, { "punch_transient", 0.55f }, { "punch_release", 6 }, { "punch_sustain", 1 }, { "punch_decay", 150 }, { "punch_attack", 0 }, { "punch_tone", 9000 }, { "tail_room", 0 }, { "punch_length", 125 }, { "punch_tok_decay", -4 }, { "punch_tok_bend", 0 }, { "punch_note_offset", 12 }, { "tail_start_mode", 0 }, { "tail_start", 100 }, { "punch_note_mode", 0 }, { "punch_mode", 1 }, { "tail_character", 1 }, { "tail_source", 2 }, { "tail_st2_type", 0 }, { "pe_start", 110.f }, { "pe_start_mode", 0 }, { "pe_time", 18 }, { "tail_penv_offset", 0 },
                         { "pe_curve", 0.8f }, { "tail_drive", 0.55f }, { "tail_tone", 10188 }, { "tail_resonance", 0.25f }, { "body_level", -14.f },
                         { "body_pitch", 159.4f }, { "sub_level", -0.7f }, { "punch_level", -10.f }, { "punch_pitch", 493.88f }, { "punch_drive", 0.5f }, { "tail_level", -3.f },
                         { "tail_uni_detune", 12.f }, { "tail_uni_drift", 8.f }, { "tail_uni_mix", 0.76f }, { "tail_st2_drive", 6 }, { "tail_mid_hz", 1800 },
                         { "tail_mid_gain", 4 }, { "tail_low_cut", 56 }, { "tail_high_cut", 19000 }, { "master_clip_drive", 0 }, { "tail_attack", 5.2f },
                         { "tail_hold_level", 0 }, { "sub_amp_env_delay", 25 }, { "sub_attack", 50 }, { "body_amp_env_delay", 0 }, { "body_source", 4 }, { "body_tone", 7000 },
                         { "punch_source", 2 }, { "noiseB_on", 1 }, { "noiseB_type", 0 }, { "noiseB_env", 3 }, { "noiseB_to_texture", -60 }, { "noiseB_to_tail", -35.2f },
                         { "noiseB_level", 0 }, { "noiseB_filter", 0 }, { "noiseB_cutoff", 12034 } },
                       { { dd::LocTail, { { "type", dd::Hard }, { "drive", 0.18f }, { "ceiling", -1 } } }, { dd::LocTail, { { "type", dd::Soft }, { "drive", 0.15f } } }, { dd::LocTail, { { "type", dd::Digital }, { "drive", 0.12f }, { "ceiling", -1 } } },
                         { dd::LocPunch, { { "type", dd::Hard }, { "drive", 0.18f }, { "ceiling", -1 } } }, { dd::LocPunch, { { "type", dd::Soft }, { "drive", 0.15f } } }, { dd::LocPunch, { { "type", dd::Digital }, { "drive", 0.12f }, { "ceiling", -1 } } } } },
            // RAW - from "RAW - Buzz Saw", ASYMMETRIC -> HARD -> WAVEFOLD, a screech, the bass under the tok (55 ms: "lepiej buja")
            Template { { { "body_resonance", 0.2f }, { "body_drive", 0.6f }, { "body_follow_offset", 12 }, { "body_follow_depth", 0 }, { "body_link_source", 2 }, { "body_link", 1 }, { "noiseB_to_punch", -22 }, { "punch_overlap", 20 }, { "punch_transient_tone", 7000 }, { "punch_transient", 0.5f }, { "punch_release", 5 }, { "punch_sustain", 1 }, { "punch_decay", 150 }, { "punch_attack", 0 }, { "punch_tone", 12000 }, { "tail_room", 0 }, { "punch_length", 110 }, { "punch_tok_decay", -5 }, { "punch_tok_bend", 0 }, { "punch_note_offset", 12 }, { "tail_start_mode", 0 }, { "tail_start", 100 }, { "punch_note_mode", 0 }, { "punch_mode", 1 }, { "tail_character", 1 }, { "tail_source", 0 }, { "tail_st2_type", 0 }, { "pe_start", 130.f }, { "pe_start_mode", 0 }, { "pe_time", 15 }, { "tail_penv_offset", 0 },
                         { "pe_curve", 0.72f }, { "tail_drive", 1 }, { "tail_tone", 11932 }, { "tail_resonance", 0.43f }, { "body_level", -4.f },
                         { "body_pitch", 124.1f }, { "sub_level", -0.06f }, { "punch_level", -9.f }, { "punch_pitch", 830.61f }, { "punch_drive", 1 }, { "tail_level", -3.3f },
                         { "tail_uni_detune", 17.4f }, { "tail_uni_drift", 5.8f }, { "tail_uni_mix", 0.49f }, { "tail_st2_drive", 12.8f }, { "tail_mid_hz", 1800 },
                         { "tail_mid_gain", 5 }, { "tail_low_cut", 150 }, { "tail_high_cut", 18917 }, { "master_clip_drive", 0 }, { "tail_attack", 3.6f },
                         { "tail_hold_level", 0 }, { "sub_amp_env_delay", 55 }, { "body_amp_env_delay", 0 }, { "body_source", 4 }, { "body_tone", 7000 },
                         { "punch_source", 3 }, { "noiseB_on", 1 }, { "noiseB_type", 1 }, { "noiseB_env", 3 }, { "noiseB_to_texture", -60 }, { "noiseB_to_tail", -27.7f },
                         { "noiseB_level", 0 }, { "noiseB_filter", 0 }, { "noiseB_cutoff", 4204 }, { "scr_on", 1 }, { "scr_level", -14 } },
                       { { dd::LocTail, { { "type", dd::Asym }, { "drive", 0.55f }, { "bias", 0.2f } } }, { dd::LocTail, { { "type", dd::Hard }, { "drive", 0.35f } } }, { dd::LocTail, { { "type", dd::Wavefold }, { "drive", 0.3f }, { "mix", 0.6f } } },
                         { dd::LocPunch, { { "type", dd::Asym }, { "drive", 0.55f }, { "bias", 0.2f } } }, { dd::LocPunch, { { "type", dd::Hard }, { "drive", 0.35f } } }, { dd::LocPunch, { { "type", dd::Wavefold }, { "drive", 0.3f }, { "mix", 0.6f } } } } },
            // EXPERIMENTAL - a WORKING kick (the MODERN core: click, tok, clean sub, noise density) + the unusual part: a DOUBLE DROP, a METALLIC
            // character and a WAVEFOLD -> RECTIFIER -> CUSTOM chain blended in, not a wall of high noise (owner 2026-11-09: the old one "tragedia")
            Template { { { "body_resonance", 0.3f }, { "body_drive", 0.6f }, { "body_follow_offset", 7 }, { "body_follow_depth", 0 }, { "body_link_source", 2 }, { "body_link", 1 }, { "noiseB_to_punch", -26 }, { "punch_overlap", 20 }, { "punch_transient_tone", 5000 }, { "punch_transient", 0.45f }, { "punch_release", 5 }, { "punch_sustain", 1 }, { "punch_decay", 150 }, { "punch_attack", 0 }, { "punch_tone", 9000 }, { "tail_room", 0 }, { "punch_length", 60 }, { "punch_tok_decay", -3 }, { "punch_tok_bend", 0 }, { "punch_note_offset", 12 }, { "tail_start_mode", 0 }, { "tail_start", 100 }, { "punch_note_mode", 0 }, { "punch_mode", 1 }, { "tail_character", 4 }, { "tail_source", 2 }, { "tail_st2_type", 0 }, { "pe_mode", 3 }, { "pe_start", 300.f }, { "pe_start_mode", 0 }, { "pe_time", 90 },
                         { "tail_penv_offset", 0 }, { "pe_curve", 0.6f }, { "tail_drive", 0.75f }, { "tail_tone", 7000 }, { "tail_resonance", 0.3f },
                         { "body_level", -8 }, { "body_pitch", 150 }, { "sub_level", -1 }, { "punch_level", -1.5f }, { "punch_pitch", 523.25f }, { "punch_drive", 0.8f },
                         { "tail_level", -5 }, { "tail_uni_detune", 28 }, { "tail_uni_drift", 30 }, { "tail_uni_mix", 0.7f }, { "tail_st2_drive", 10 },
                         { "tail_low_cut", 80 }, { "tail_high_cut", 16000 }, { "master_clip_drive", 0 }, { "tail_attack", 4 }, { "tail_hold_level", 0 },
                         { "sub_amp_env_delay", 50 }, { "body_source", 2 }, { "body_tone", 6000 }, { "punch_source", 2 }, { "noiseB_on", 1 }, { "noiseB_type", 1 },
                         { "noiseB_env", 3 }, { "noiseB_to_texture", -60 }, { "noiseB_to_tail", -30 }, { "noiseB_level", 0 }, { "noiseB_cutoff", 8000 } },
                       { { dd::LocTail, { { "type", dd::Wavefold }, { "drive", 0.3f }, { "mix", 0.45f } } }, { dd::LocTail, { { "type", dd::Rectifier }, { "drive", 0.25f }, { "mix", 0.25f } } }, { dd::LocTail, { { "type", dd::Custom }, { "drive", 0.25f }, { "mix", 0.35f } } },
                         { dd::LocPunch, { { "type", dd::Wavefold }, { "drive", 0.3f }, { "mix", 0.45f } } }, { dd::LocPunch, { { "type", dd::Rectifier }, { "drive", 0.25f }, { "mix", 0.25f } } }, { dd::LocPunch, { { "type", dd::Custom }, { "drive", 0.25f }, { "mix", 0.35f } } } } },
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
