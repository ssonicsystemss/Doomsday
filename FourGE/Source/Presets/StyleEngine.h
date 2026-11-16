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
    enum Style { Early = 0, Modern, Raw, Experimental, Euphoric, Pvc, Zaag, NumStyles };     // (EUPHORIC appended: saved projects keep their ids)
    inline const juce::StringArray& names() { static const juce::StringArray a { "EARLY", "MODERN", "RAW", "EXPERIMENTAL", "EUPHORIC", "PVC", "ZAAG" }; return a; }
    inline const char* description (int s)
    {
        static const char* d[NumStyles] {
            "Early 2004-08: a 909-like punch (no tonal tok), a room before the drive, a tonal body, warm distortion (SOFT CLIP -> TUBE), a clear pitch drop.",
            "Strong, clean and controlled: a precise transient, layered body, a polished tail and dense, precise distortion (HARD -> SOFT -> DIGITAL CLIP).",
            "Aggressive: heavy asymmetric distortion (ASYMMETRIC -> HARD -> WAVEFOLD), a sharp screech, strong harmonics, the bass entering early.",
            "Unusual but still a kick: a double drop, a metallic tail, folds / a rectifier / a custom shaper blended into a solid core.",
            "Euphoric 2009-12 (Wildstylez, Headhunterz, Brennan Heart, Noisecontrollers): a low driven punch (the hit), a high long BODY (the crunch), "
            "a tail that falls fast from ~760 Hz through the drive (the crack), soft -> wavefold, the 6th harmonic coloured, a small room.",
            "PVC / KLOENK (owner 2026-11-14, from his PVC samples): a plastic TUBE punch (~1 kHz, 'ciup', phaser), then a gated tail made of a REVERB "
            "(high cut -> drive -> clip -> gate -> drive), low end mono, highs wide.",
            "ZAAG (owner 2026-11-14 / 15): PUNCH TYPE ZAAG (piep -> trask, punch reverb) + TAIL TYPE ZAAG (2 saws -> 3 serial EQ / waveshaper stages -> "
            "squash + soft clip -> filter movement -> sub support -> shape)." };
        return d[juce::jlimit (0, NumStyles - 1, s)];
    }
    inline int indexOf (const juce::String& id) { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; }

    // ---- what a style may set (and what it never touches)
    inline bool isLength (const juce::String& id)
    {
        if (id.startsWith ("punch_")) return false;                    // PUNCH / TOK: the tok's own length / envelope is its character, not the kick length
        if (id.startsWith ("tail_room")) return false;                 // the TAIL ROOM decay is its character (it never makes the kick longer)
        if (id.startsWith ("noise")) return false;
        if (id.startsWith ("tk_") || id.startsWith ("clk_") || id.startsWith ("spk_") || id.startsWith ("cp_") || id.startsWith ("rw_") || id.startsWith ("ex_")) return false;   // the FRONT layers' own lengths = their character
        if (id.startsWith ("pvc_") || id.startsWith ("pt_") || id.startsWith ("zg_")) return false;   // PVC punch length / tail length = the PVC character (owner 2026-11-14)                     // a noise layer's decay / length is its character (EUPHORIC's grain), not the kick length
        return id == "core_length" || id == "core_auto_length" || id.endsWith ("_length") || id.endsWith ("_decay") || id.endsWith ("_release")
               || id.endsWith ("_sustain") || id.contains ("_length_") || id.endsWith ("_hold");
    }
    inline bool inScope (int i)
    {
        const juce::String id (def (i).id);
        if (isLength (id)) return false;
        for (auto* p : { "punch_", "body_", "tail_", "sub_", "pe_", "noiseA_", "noiseB_", "tr_", "scr_", "psh_", "pvc_", "pt_", "gt_", "zg_", "cp_", "rw_", "ex_" }) if (id.startsWith (p)) return ! id.startsWith ("tail_penv_draw");
        if (id.startsWith ("dist")) return true;                         // the whole distortion pool (dist1..16 + dist_ globals)
        if ((id.startsWith ("tk_") || id.startsWith ("clk_") || id.startsWith ("spk_")) && ! id.endsWith ("_mode")) return true;   // the FRONT profiles (never the user's AUTO / ON / OFF)
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
            // EARLY (EARLY REWORK, owner 2026-11-16): PUNCH TYPE CLASSIC (cp_ defaults = fit P1) + TOK / SUB PUNCH / BODY / TAIL fitted to the
            // owner's 20 EARLY refs by measurements in time windows (spectra / pitch track / levels, score 25 -> 10): the CLASSIC punch carries
            // the distortion (SOFT -> TUBE on the PUNCH), the tail is clean, low-passed and starts from the hit (tail_start_mode 1) - its
            // pitch falls slowly (pe 170 ms, curve 0.04) and the SUB glides down with it (SLOW DROP 12 st / 120 ms: the refs sit at 90 / 72 / 63 / 56 Hz
            // over 80-130 ms, not on the note at once); body a clean octave layer; the master clip squeezes it like the refs
            Template { { { "punch_mode", 5 }, { "punch_level", 6 }, { "cp_decay", 41.2f }, { "cp_end", 11.8f },
                         { "cp_sat", 24 }, { "cp_ws_type", 0 }, { "cp_ws_drive", 12 }, { "cp_high_cut", 7000 },   // owner 2026-11-16 "drapie przy punchu" -> his pick D: a warmer, softer punch shaper, top cut at 7 kHz
                         { "punch_pitch", 1000 }, { "punch_glide", 4 }, { "punch_drive", 0.6f }, { "punch_tone", 6000 }, { "punch_transient", 0.4f }, { "punch_transient_tone", 4000 },
                         { "punch_attack", 0 }, { "punch_tok_decay", -2 }, { "punch_tok_bend", 0 }, { "punch_note_offset", 12 }, { "punch_note_mode", 0 }, { "punch_source", 0 },
                         { "punch_overlap", 20 }, { "punch_length", 28 }, { "punch_decay", 16 }, { "punch_sustain", 0 }, { "punch_release", 5 },
                         { "body_source", 1 }, { "body_level", 0 }, { "body_pitch", 162.4f }, { "body_tone", 3000 }, { "body_resonance", 0.1f }, { "body_drive", 0 },
                         { "body_follow_offset", 0 }, { "body_follow_depth", 0 }, { "body_link_source", 2 }, { "body_link", 1 },
                         { "tail_source", 2 }, { "tail_character", 5 }, { "tail_level", -15.9f }, { "tail_attack", 40 }, { "tail_tone", 300 }, { "tail_resonance", 0.06f },
                         { "tail_drive", 0 }, { "tail_st2_type", 2 }, { "tail_st2_drive", 0 }, { "tail_mid_hz", 545 }, { "tail_mid_gain", -6 }, { "tail_low_cut", 40 },
                         { "tail_high_cut", 19000 }, { "tail_uni_drift", 14 }, { "tail_uni_mix", 1 }, { "tail_hold_level", 0 }, { "tail_color_gain", 0 },
                         { "tail_start_mode", 1 }, { "tail_start", 120 }, { "tail_room", 0.4f }, { "tail_room_decay", 450 }, { "tail_room_tone", 4500 }, { "tail_room_input", 0 },
                         { "pe_start_mode", 1 }, { "pe_start", 127 }, { "pe_time", 170.5f }, { "pe_curve", 0.044f },
                         { "sub_level", -1.1f }, { "sub_penv_mode", 2 }, { "sub_penv_depth", 12 }, { "sub_penv_time", 120 }, { "sub_penv_curve", 0.364f },
                         { "master_clip_drive", 3 } },                     // (pick D: 7 -> 3, the punch + sub + body no longer grind together in the clip)
                       { { dd::LocTail, { { "type", dd::Soft }, { "drive", 0 }, { "shape", 0.5f } } }, { dd::LocTail, { { "type", dd::Tube }, { "drive", 0 } } },   // the chain stays (owner's EARLY SOFT -> TUBE) at 0 - the
                         { dd::LocPunch, { { "type", dd::Soft }, { "drive", 0 }, { "shape", 0.5f } } }, { dd::LocPunch, { { "type", dd::Tube }, { "drive", 0 } } } } },   // CLASSIC punch saturates itself (pick D)
            // MODERN - fitted as a whole kick to the owner's OPS FREE KICK 1-4 (2026-11-16, score 25.6 -> 12.6): a high tonal TOK (+22 st, 65 ms)
            // carries the first ~40 ms (the refs hold ~700 Hz), a loud global click, a long sub punch, the tail sits low from the start; from "MODERN - Festival Drive", HARD -> SOFT -> DIGITAL CLIP on the tail (controlled), the bass in at 75 ms
            Template { { { "body_resonance", 0.15f }, { "body_drive", 0.25f }, { "body_follow_offset", 12 }, { "body_follow_depth", 0 }, { "body_link_source", 2 }, { "body_link", 1 }, { "noiseB_to_punch", -24 }, { "punch_overlap", 20 }, { "punch_transient_tone", 6000 }, { "punch_transient", 0.55f }, { "punch_release", 6 }, { "punch_sustain", 1 }, { "punch_decay", 150 }, { "punch_attack", 0 }, { "punch_tone", 9000 }, { "tail_room", 0 }, { "tail_color_gain", 0 }, { "tail_room_input", 0 }, { "punch_length", 125 }, { "punch_tok_decay", -4 }, { "punch_tok_bend", 0 }, { "punch_note_offset", 12 }, { "tail_start_mode", 0 }, { "tail_start", 100 }, { "punch_note_mode", 0 }, { "punch_mode", 1 }, { "tail_character", 1 }, { "tail_source", 2 }, { "tail_st2_type", 0 }, { "pe_start", 60.f }, { "pe_start_mode", 0 }, { "pe_time", 18 }, { "tail_penv_offset", 0 },
                         { "pe_curve", 0.63f }, { "tail_drive", 0.4f }, { "tail_tone", 10168 }, { "tail_resonance", 0.16f }, { "body_level", -28.7f },
                         { "body_pitch", 81.6f }, { "sub_level", -0.7f }, { "punch_level", -10.f }, { "punch_pitch", 493.88f }, { "punch_drive", 0.56f }, { "tail_level", -4.5f },
                         { "tail_uni_detune", 12.f }, { "tail_uni_drift", 8.f }, { "tail_uni_mix", 0.76f }, { "tail_st2_drive", 13.9f }, { "tail_mid_hz", 2747 },
                         { "tail_mid_gain", -2.8f }, { "tail_low_cut", 58.7f }, { "tail_high_cut", 19000 }, { "master_clip_drive", 5.57f }, { "tail_attack", 23.4f },
                         { "tail_hold_level", 0 }, { "sub_amp_env_delay", 12.5f }, { "sub_attack", 50 }, { "body_amp_env_delay", 0 }, { "body_source", 4 }, { "body_tone", 7000 },
                         { "punch_source", 2 }, { "noiseB_on", 1 }, { "noiseB_type", 0 }, { "noiseB_env", 3 }, { "noiseB_to_texture", -60 }, { "noiseB_to_tail", -35.2f },
                         { "noiseB_level", 0 }, { "noiseB_filter", 0 }, { "noiseB_cutoff", 12034 } },
                       { { dd::LocTail, { { "type", dd::Hard }, { "drive", 0.36f }, { "ceiling", -1 } } }, { dd::LocTail, { { "type", dd::Soft }, { "drive", 0.15f } } }, { dd::LocTail, { { "type", dd::Digital }, { "drive", 0.05f }, { "ceiling", -1 } } },
                         { dd::LocPunch, { { "type", dd::Hard }, { "drive", 0 }, { "ceiling", -1 } } }, { dd::LocPunch, { { "type", dd::Soft }, { "drive", 0.8f } } }, { dd::LocPunch, { { "type", dd::Digital }, { "drive", 0.35f }, { "ceiling", -1 } } } } },
            // RAW - TAIL TYPE RAW fitted to FHSP 038 (2026-11-16); the CLASSIC-tail values below stay for the old tail; from "RAW - Buzz Saw", ASYMMETRIC -> HARD -> WAVEFOLD, a screech, the bass under the tok (55 ms: "lepiej buja")
            Template { { { "body_resonance", 0.2f }, { "body_drive", 0.6f }, { "body_follow_offset", 12 }, { "body_follow_depth", 0 }, { "body_link_source", 2 }, { "body_link", 1 }, { "noiseB_to_punch", -22 }, { "punch_overlap", 20 }, { "punch_transient_tone", 7000 }, { "punch_transient", 0.5f }, { "punch_release", 5 }, { "punch_sustain", 1 }, { "punch_decay", 150 }, { "punch_attack", 0 }, { "punch_tone", 12000 }, { "tail_room", 0 }, { "tail_color_gain", 0 }, { "tail_room_input", 0 }, { "punch_length", 110 }, { "punch_tok_decay", -5 }, { "punch_tok_bend", 0 }, { "punch_note_offset", 12 }, { "tail_start_mode", 0 }, { "tail_start", 100 }, { "punch_note_mode", 0 }, { "punch_mode", 1 }, { "tail_character", 1 }, { "tail_source", 0 }, { "tail_st2_type", 0 }, { "pe_start", 130.f }, { "pe_start_mode", 0 }, { "pe_time", 15 }, { "tail_penv_offset", 0 },
                         { "pe_curve", 0.72f }, { "tail_drive", 1 }, { "tail_tone", 11932 }, { "tail_resonance", 0.43f }, { "body_level", -36 },
                         { "body_pitch", 124.1f }, { "sub_level", -7.8f }, { "punch_level", -11.6f }, { "punch_pitch", 830.61f }, { "punch_drive", 1 }, { "tail_level", -6.4f },
                         { "tail_uni_detune", 17.4f }, { "tail_uni_drift", 5.8f }, { "tail_uni_mix", 0.49f }, { "tail_st2_drive", 12.8f }, { "tail_mid_hz", 1800 },
                         { "tail_mid_gain", 5 }, { "tail_low_cut", 150 }, { "tail_high_cut", 18917 }, { "master_clip_drive", 2.85f }, { "tail_attack", 3.6f },
                         { "tail_hold_level", 0 }, { "sub_amp_env_delay", 55 }, { "body_amp_env_delay", 0 }, { "body_source", 4 }, { "body_tone", 7000 },
                         { "punch_source", 3 }, { "noiseB_on", 1 }, { "noiseB_type", 1 }, { "noiseB_env", 3 }, { "noiseB_to_texture", -60 }, { "noiseB_to_tail", -27.7f },
                         { "noiseB_level", 0 }, { "noiseB_filter", 0 }, { "noiseB_cutoff", 4204 }, { "scr_on", 1 }, { "scr_level", -14 },
                         // FULL KICK fitted to FHSP 038 (owner 2026-11-16: "038 to klasyczny rawowy"): TAIL TYPE RAW (its defaults = the 038 tail fit) +
                         // the levels / TOK / CLICK / SUB PUNCH fitted on the whole kick (score 14.2 -> 10.6, no extra top on the hit)
                         { "tail_engine", 3 }, { "rw_start", 43 }, { "rw_d1_drive", 30.5f }, { "rw_d3_drive", 10.1f }, { "rw_flt_mix", 0.31f },
                         { "rw_attack", 3 }, { "rw_body", 0.39f }, { "rw_tail_decay", 0.39f } },
                       { { dd::LocTail, { { "type", dd::Asym }, { "drive", 0.55f }, { "bias", 0.2f } } }, { dd::LocTail, { { "type", dd::Hard }, { "drive", 0.35f } } }, { dd::LocTail, { { "type", dd::Wavefold }, { "drive", 0.3f }, { "mix", 0.6f } } },
                         { dd::LocPunch, { { "type", dd::Asym }, { "drive", 0.55f }, { "bias", 0.2f } } }, { dd::LocPunch, { { "type", dd::Hard }, { "drive", 0.35f } } }, { dd::LocPunch, { { "type", dd::Wavefold }, { "drive", 0.3f }, { "mix", 0.6f } } } } },
            // EXPERIMENTAL - a WORKING kick (the MODERN core: click, tok, clean sub, noise density) + the unusual part: a DOUBLE DROP, a METALLIC
            // character and a WAVEFOLD -> RECTIFIER -> CUSTOM chain blended in, not a wall of high noise (owner 2026-11-09: the old one "tragedia")
            Template { { { "body_resonance", 0.3f }, { "body_drive", 0.6f }, { "body_follow_offset", 7 }, { "body_follow_depth", 0 }, { "body_link_source", 2 }, { "body_link", 1 }, { "noiseB_to_punch", -26 }, { "punch_overlap", 20 }, { "punch_transient_tone", 5000 }, { "punch_transient", 0.45f }, { "punch_release", 5 }, { "punch_sustain", 1 }, { "punch_decay", 150 }, { "punch_attack", 0 }, { "punch_tone", 9000 }, { "tail_room", 0 }, { "tail_color_gain", 0 }, { "tail_room_input", 0 }, { "punch_length", 60 }, { "punch_tok_decay", -3 }, { "punch_tok_bend", 0 }, { "punch_note_offset", 12 }, { "tail_start_mode", 0 }, { "tail_start", 100 }, { "punch_note_mode", 0 }, { "punch_mode", 1 }, { "tail_character", 4 }, { "tail_source", 2 }, { "tail_st2_type", 0 }, { "pe_mode", 3 }, { "pe_start", 300.f }, { "pe_start_mode", 0 }, { "pe_time", 90 },
                         { "tail_penv_offset", 0 }, { "pe_curve", 0.6f }, { "tail_drive", 0.75f }, { "tail_tone", 7000 }, { "tail_resonance", 0.3f },
                         { "body_level", -8 }, { "body_pitch", 150 }, { "sub_level", -1 }, { "punch_level", -1.5f }, { "punch_pitch", 523.25f }, { "punch_drive", 0.8f },
                         { "tail_level", -5 }, { "tail_uni_detune", 28 }, { "tail_uni_drift", 30 }, { "tail_uni_mix", 0.7f }, { "tail_st2_drive", 10 },
                         { "tail_low_cut", 80 }, { "tail_high_cut", 16000 }, { "master_clip_drive", 0 }, { "tail_attack", 4 }, { "tail_hold_level", 0 },
                         { "sub_amp_env_delay", 50 }, { "body_source", 2 }, { "body_tone", 6000 }, { "punch_source", 2 }, { "noiseB_on", 1 }, { "noiseB_type", 1 },
                         { "noiseB_env", 3 }, { "noiseB_to_texture", -60 }, { "noiseB_to_tail", -30 }, { "noiseB_level", 0 }, { "noiseB_cutoff", 8000 } },
                       { { dd::LocTail, { { "type", dd::Wavefold }, { "drive", 0.3f }, { "mix", 0.45f } } }, { dd::LocTail, { { "type", dd::Rectifier }, { "drive", 0.25f }, { "mix", 0.25f } } }, { dd::LocTail, { { "type", dd::Custom }, { "drive", 0.25f }, { "mix", 0.35f } } },
                         { dd::LocPunch, { { "type", dd::Wavefold }, { "drive", 0.3f }, { "mix", 0.45f } } }, { dd::LocPunch, { { "type", dd::Rectifier }, { "drive", 0.25f }, { "mix", 0.25f } } }, { dd::LocPunch, { { "type", dd::Custom }, { "drive", 0.25f }, { "mix", 0.35f } } } } },
            // EUPHORIC 2009-12 - the OWNER'S OWN kick (2026-11-13: "u mnie takie ustawienia sa najbliżej", made from INIT): a LOW tok (231 Hz, 91 ms,
            // drive 67 %) = the hit; a HIGH long BODY (1.14 kHz, 370 ms, drive 37 %) = the crunch ("chrupnięcie", the 500-1500 Hz band); the TAIL
            // starts at 760 Hz and falls in 161 ms (curve 24 %, -13.6 st) through drive 63 % = the crack; the three layers about equally loud.
            // Effects = "between H2 and H3": SOFT -> WAVEFOLD on the tail, TAIL COLOR the 6th harmonic (KEY TRACK, "GRRRAAANG"), a small room
            // before the drive, a light clip. Everything else stays INIT (the owner's base).
            Template { { { "punch_mode", 2 },                     // SHAPED PUNCH (owner 2026-11-14 "mamy to"): P1 = mid 1 kHz, the psh_ defaults
                         { "punch_level", -1.8f }, { "tail_attack", 20 }, { "body_attack", 15 },   // L3 (2026-11-14: the punch got lost under the tail)
                         { "punch_pitch", 231 }, { "punch_length", 91 }, { "punch_drive", 0.67f },
                         { "body_level", -4 }, { "body_pitch", 1250 }, { "body_drive", 0.5f },   // "Crunch Body" = "jest git" (2026-11-13)
                         { "tail_level", -4.4f }, { "tail_drive", 0.63f },
                         { "pe_start", 760 }, { "pe_end", 20.3f }, { "pe_time", 161 }, { "pe_curve", 0.24f }, { "pe_amount", -13.6f },
                         { "tail_color_gain", 6 }, { "tail_color_track", 1 }, { "tail_color_harm", 7 }, { "tail_color_q", 1.2f },
                         { "tail_room", 0.2f }, { "tail_room_decay", 450 }, { "tail_room_tone", 6000 }, { "tail_room_input", 0 },
                         { "master_clip_drive", 1.5f } },
                       { { dd::LocTail, { { "type", dd::Soft }, { "drive", 0.5f } } }, { dd::LocTail, { { "type", dd::Wavefold }, { "drive", 0.3f } } } } },
            // PVC (owner 2026-11-14): between his picks R02 / R04 / R09 - TUBE punch ~1 kHz + TAIL ENGINE PVC (its own distortion stages: no pool chain)
            Template { { { "punch_mode", 3 }, { "punch_level", 0 }, { "pvc_model", 1 }, { "pvc_tube_hz", 1000 }, { "pvc_tube_comb", 0.25f }, { "pvc_jump", 5 },
                         { "pvc_drop", -6 }, { "pvc_drop_time", 50 }, { "pvc_phaser", 0.4f }, { "pvc_ph_fb", 0.7f }, { "pvc_bright", 7500 }, { "pvc_drive", 10 },
                         { "pvc_attack", 0.5f }, { "pvc_length", 175 }, { "pvc_decay", 15 },
                         { "tail_engine", 1 }, { "tail_level", -2 }, { "body_level", -60 }, { "sub_level", -60 },
                         { "pt_source", 3 }, { "pt_start", 8 }, { "pt_drop_time", 50 }, { "pt_rev_size", 1.1f }, { "pt_rev_decay", 1500 }, { "pt_rev_cut", 420 },
                         { "pt_drive1", 17 }, { "pt_clip", 0.6f }, { "pt_drive2", 13 }, { "pt_asym", 0.15f }, { "pt_gate_curve", 2 },
                         { "gt_gate_len", 260 }, { "gt_gate_depth", 0.5f }, { "pt_low_mid", -1.5f }, { "pt_high", 2 }, { "pt_width", 0.9f },
                         { "pt_delay", 175 }, { "pt_length", 320 }, { "master_clip_drive", 3 } },
                       {} },
            // ZAAG (owner 2026-11-15): TAIL ENGINE ZAAG = the whole kick (the punch / body / sub generators silent); the values = his favourite X7
            // (RESONANT, hard sharp saw, high sweep + a little reopen; PUNCH: click + heavy THUMP + the scratchy PUP with the punch reverb)
            Template { { { "punch_mode", 4 }, { "tail_engine", 2 }, { "tail_level", 0 }, { "body_level", -60 }, { "sub_level", -60 },    // a STYLE only picks the TYPES (+ values)
                         { "zg_variant", 1 }, { "zg_s3_type", 1 }, { "zg_start", 16 }, { "zg_drop", 140 }, { "zg_sw_plateau", 12 }, { "zg_sw_start", 30 }, { "zg_sw_reopen", 0.3f },
                         { "zg_resonance", 0.7f }, { "zg_sawb_level", 0.6f }, { "zg_low_ctrl", 0.3f }, { "zg_final_tone", 1 }, { "zg_s1_curve", 1 },
                         { "zg_grit", 0.85f }, { "zg_drive", 20 }, { "zg_zaag", 0.7f }, { "zg_character", 0.68f }, { "zg_crush", 0.78f }, { "zg_dirt", 0.2f }, { "zg_movement", 0.62f },
                         { "zg_click", 1 }, { "zg_thump", 1.5f }, { "zg_thump_hz", 170 }, { "zg_thump_ms", 90 }, { "zg_thump_drive", 30 },
                         { "zg_pup", 6 }, { "zg_piep", 1 }, { "zg_piep_hz", 1000 }, { "zg_piep_ms", 30 }, { "zg_trask", 1 }, { "zg_trask_delay", 8 }, { "zg_trask_ms", 45 }, { "zg_trask_tone", 6000 },
                         { "zg_verb", 2.5f }, { "zg_verb_decay", 1600 }, { "zg_verb_size", 1.4f }, { "zg_verb_send", 60 }, { "zg_verb_damp", 9000 }, { "zg_verb_low_cut", 600 },
                         { "master_clip_drive", 4 } },    // MASTER: the final waveshaper squeezes the kick to 0 dB (oversampled; not the only level control)
                       {} },
        };
        return t[(size_t) juce::jlimit (0, NumStyles - 1, s)];
    }
    // ---- THE FRONT (owner 2026-11-16): the style has no TOK / CLICK / SUB PUNCH of its own - it only gives the shared modules a
    //      PROFILE (on / off in AUTO + character). Levels from the owner's balances (TOK / PUNCH / CLICK / SUB PUNCH, 100 % = -6 dB):
    //      EARLY a short characteristic tok, NO global click (EARLY REWORK: the classic punch makes its own transient), a short controlled sub punch; MODERN precise tok, short selective click, controlled sub punch;
    //      RAW harder tok, aggressive click, strong sub punch; EXPERIMENTAL = by the preset (off); ZAAG = optional (off: its own punch
    //      engine has the click / pup, ZAAG SEND mixes the zaag into the TOK / PUNCH); EUPHORIC / PVC = their models' own click / sub (off).
    inline std::vector<std::pair<const char*, float>> frontProfile (int s)
    {
        switch (s)
        {
            // EARLY (fit to the 20 refs, 2026-11-16): the CLASSIC punch already is the tok - a separate TOK only got quieter in every fit
            // round (-40 dB), so AUTO = OFF (ON in the FRONT tab brings the short sine tok back); the sub punch stays, quieter and longer
            case Early: return { { "tk_auto", 0 }, { "tk_type", 0 }, { "tk_level", -14 }, { "tk_tune", 1 }, { "tk_length", 14 }, { "tk_peak", 4 }, { "tk_fade", 0.25f },
                                 { "tk_tone", 0.45f }, { "tk_penv_start", 10 }, { "tk_drive", 0.15f },
                                 { "clk_auto", 0 },                                                   // (EARLY REWORK: the punch makes its own transient - global CLICK OFF)
                                 { "spk_auto", 1 }, { "spk_level", -19.7f }, { "spk_length", 50 }, { "spk_drive", 0.2f }, { "spk_tone", 0.35f } };   // controlled (fit)
            case Modern: return { { "tk_auto", 1 }, { "tk_type", 4 }, { "tk_level", -3.6f }, { "tk_tune", 22.1f }, { "tk_length", 65 }, { "tk_peak", 10 }, { "tk_fade", 0.5f },   // (fit to OPS FREE KICK 1-4, 2026-11-16)
                                  { "tk_tone", 1 }, { "tk_penv_start", 10.5f }, { "tk_drive", 0.66f },
                                  { "clk_auto", 1 }, { "clk_type", 1 }, { "clk_level", 3.5f }, { "clk_length", 23.8f }, { "clk_decay", 0.7f }, { "clk_tone", 0.31f }, { "clk_drive", 0.25f },
                                  { "spk_auto", 1 }, { "spk_level", -6.4f }, { "spk_length", 110 }, { "spk_drive", 0.76f }, { "spk_tone", 0.3f } };
            case Raw: return { { "tk_auto", 1 }, { "tk_type", 2 }, { "tk_level", -11 }, { "tk_tune", -4.6f }, { "tk_length", 58.6f }, { "tk_peak", 2 }, { "tk_fade", 0.5f },   // (fit to FHSP 038, 2026-11-16)
                               { "tk_tone", 0.8f }, { "tk_penv_start", 0 }, { "tk_drive", 0.12f },
                               { "clk_auto", 1 }, { "clk_type", 5 }, { "clk_level", -11.2f }, { "clk_length", 20.5f }, { "clk_tone", 0.52f }, { "clk_drive", 0.6f },
                               { "spk_auto", 1 }, { "spk_level", -8 }, { "spk_length", 31.6f }, { "spk_drive", 0.85f }, { "spk_tone", 0.5f } };
            default: return {};                                                // EXPERIMENTAL / EUPHORIC / PVC / ZAAG: AUTO = off (the defaults)
        }
    }
    // every in-scope parameter's value for a style (defaults + the template + the chain)
    inline std::vector<std::pair<int, float>> values (int s)
    {
        std::vector<float> v ((size_t) P::count, 0.f);
        std::vector<bool> scope ((size_t) P::count, false);
        for (int i = 0; i < P::count; ++i) if (inScope (i)) { scope[(size_t) i] = true; v[(size_t) i] = def (i).def; }
        const auto& t = templ (s);
        for (auto& [id, x] : t.values) if (const int i = indexOf (id); i >= 0 && scope[(size_t) i]) v[(size_t) i] = x;
        for (auto& [id, x] : frontProfile (s)) if (const int i = indexOf (id); i >= 0 && scope[(size_t) i]) v[(size_t) i] = x;
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
            {   // THE FRONT: a layer the user forced ON / OFF keeps its settings (a style updates AUTO layers only)
                const juce::String id (def (i).id);
                const char* mode = id.startsWith ("tk_") ? "tk_mode" : id.startsWith ("clk_") ? "clk_mode" : id.startsWith ("spk_") ? "spk_mode" : nullptr;
                if (mode != nullptr && (int) p.apvts.getRawParameterValue (mode)->load() != 0) continue;
            }
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
