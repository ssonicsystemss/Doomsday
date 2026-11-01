#pragma once
#include <juce_core/juce_core.h>
#include <cstring>

namespace ParamIDs
{
    inline constexpr const char* versionText = "0.5";      // keep in step with CMakeLists.txt
    // TRIGGER / TIMING
    inline constexpr const char* trigger   = "trg_mode";
    inline constexpr const char* timing    = "trg_timing";
    inline constexpr const char* step      = "trg_step";
    inline constexpr const char* step1     = "trg_step1";
    inline constexpr const char* step2     = "trg_step2";
    inline constexpr const char* step3     = "trg_step3";
    inline constexpr const char* step4     = "trg_step4";
    inline constexpr const char* offset    = "trg_offset";
    inline constexpr const char* beats     = "trg_beats";
    // ENVELOPE
    inline constexpr const char* delayPct  = "env_delay_pct";
    inline constexpr const char* holdPct   = "env_hold_pct";
    inline constexpr const char* decayPct  = "env_decay_pct";
    inline constexpr const char* gatePct   = "env_gate_pct";
    inline constexpr const char* delayMs   = "env_delay_ms";
    inline constexpr const char* holdMs    = "env_hold_ms";
    inline constexpr const char* decayMs   = "env_decay_ms";
    inline constexpr const char* gateMs    = "env_gate_ms";
    inline constexpr const char* duck      = "env_duck";
    inline constexpr const char* initial   = "env_initial";
    inline constexpr const char* swell     = "env_swell";
    inline constexpr const char* curve     = "env_curve";
    inline constexpr const char* sustain   = "env_sustain";
    inline constexpr const char* decayCurve = "env_decay_curve";
    inline constexpr const char* fade      = "env_fade";
    // SOURCE
    inline constexpr const char* source    = "src_type";
    inline constexpr const char* blend     = "osc_blend";
    inline constexpr const char* width     = "osc_pw";
    inline constexpr const char* sine      = "osc_sine";
    inline constexpr const char* warp      = "osc_warp";
    inline constexpr const char* warpAmt   = "osc_warp_amt";
    inline constexpr const char* warpEnv   = "osc_warp_env";
    inline constexpr const char* tailStart = "tail_start";
    inline constexpr const char* tailDrop  = "tail_drop";
    inline constexpr const char* tailCurve = "tail_curve";
    inline constexpr const char* tailWave  = "tail_wave";
    inline constexpr const char* tailDist  = "tail_dist";
    inline constexpr const char* tailDrive = "tail_drive";
    inline constexpr const char* tailTone  = "tail_tone";
    inline constexpr const char* tailHighCut = "tail_high_cut";
    inline constexpr const char* tailScream = "tail_scream";
    inline constexpr const char* tailScreamHz = "tail_scream_hz";
    inline constexpr const char* tailLen   = "tail_len";
    inline constexpr const char* tailRoom  = "tail_room";
    inline constexpr const char* tailRoomSize = "tail_room_size";
    inline constexpr const char* smpReverse = "smp_reverse";
    inline constexpr const char* smpRoot   = "smp_root";
    inline constexpr const char* smpStart  = "smp_start";
    // PITCH
    inline constexpr const char* pitchMode = "pitch_mode";
    inline constexpr const char* pitchAmt  = "pitch_amt";
    inline constexpr const char* pitchTime = "pitch_time";
    inline constexpr const char* tune      = "pitch_tune";
    inline constexpr const char* fine      = "pitch_fine";
    inline constexpr const char* keyTrack  = "pitch_key_track";
    inline constexpr const char* fixedNote = "pitch_fixed_note";
    // TONE
    inline constexpr const char* preType   = "flt_pre_type";
    inline constexpr const char* preHz     = "flt_pre_hz";
    inline constexpr const char* preReso   = "flt_pre_reso";
    inline constexpr const char* distType  = "dist_type";
    inline constexpr const char* drive     = "dist_drive";
    inline constexpr const char* driveEnv  = "dist_env";
    inline constexpr const char* bias      = "dist_bias";
    inline constexpr const char* mix       = "dist_mix";
    inline constexpr const char* lowCut    = "eq_low_cut";
    inline constexpr const char* highCut   = "eq_high_cut";
    inline constexpr const char* bellHz    = "eq_bell_hz";
    inline constexpr const char* bellGain  = "eq_bell_gain";
    inline constexpr const char* bellQ     = "eq_bell_q";
    inline constexpr const char* subHz     = "sub_hz";
    inline constexpr const char* subLevel  = "sub_level";
    inline constexpr const char* clip      = "out_clip";
    inline constexpr const char* level     = "out_level";
    inline constexpr const char* velocity  = "midi_velocity";
    // UNISON / WIDTH (v0.2)
    inline constexpr const char* uniVoices = "uni_voices";
    inline constexpr const char* uniDetune = "uni_detune";
    inline constexpr const char* uniDrift  = "uni_drift";
    inline constexpr const char* uniMix    = "uni_mix";
    inline constexpr const char* stereoWidth = "st_width";     // ("width" is the pulse width)
    inline constexpr const char* widthSplit = "st_split";

    // ---- v0.4: step 1 (the core) - new IDs only ever appended (Tests/param_ids.txt)
    inline constexpr const char* osc1Level = "osc1_level";
    inline const char* oscId (int osc, const char* what)      // osc 2..4: "osc2_wave", "osc3_level", ...
    {
        static const char* ids[3][9] {
            { "osc2_wave", "osc2_level", "osc2_oct", "osc2_semi", "osc2_fine", "osc2_phase", "osc2_width", "osc2_sync", "osc2_pm" },
            { "osc3_wave", "osc3_level", "osc3_oct", "osc3_semi", "osc3_fine", "osc3_phase", "osc3_width", "osc3_sync", "osc3_pm" },
            { "osc4_wave", "osc4_level", "osc4_oct", "osc4_semi", "osc4_fine", "osc4_phase", "osc4_width", "osc4_sync", "osc4_pm" } };
        static const char* names[9] { "wave", "level", "oct", "semi", "fine", "phase", "width", "sync", "pm" };
        for (int k = 0; k < 9; ++k) if (std::strcmp (names[k], what) == 0) return ids[juce::jlimit (2, 4, osc) - 2][k];
        jassertfalse; return ids[0][0];
    }
    inline constexpr const char* phaseMode = "phase_mode";
    inline constexpr const char* preEnv    = "flt_pre_env";
    inline constexpr const char* preKey    = "flt_pre_key";
    inline constexpr const char* dist2Type = "dist2_type";
    inline constexpr const char* dist2Drive = "dist2_drive";
    inline constexpr const char* dist2Mix  = "dist2_mix";
    inline constexpr const char* macros[8] { "mac_body", "mac_weight", "mac_reverse", "mac_pitch", "mac_grit", "mac_bite", "mac_movement", "mac_space" };
    // ---- v0.5: step 1, part 2
    inline constexpr const char* subOct    = "sub_oct";
    inline constexpr const char* subHarm   = "sub_harm";
    inline constexpr const char* subSat    = "sub_sat";
    inline constexpr const char* clickLevel = "click_level";
    inline constexpr const char* clickTone = "click_tone";
    inline constexpr const char* clickLen  = "click_len";
    inline constexpr const char* clickAt   = "click_at";
    inline constexpr const char* glide     = "pitch_glide";
    inline constexpr const char* smear     = "st_smear";
    inline constexpr const char* smearSize = "st_smear_size";
    inline const juce::StringArray macroNames { "BODY", "WEIGHT", "REVERSE", "PITCH", "GRIT", "BITE", "MOVEMENT", "SPACE" };

    // v0.1 / v0.2 IDs -> v0.3 (old user presets still load); the v0.3 IDs are FROZEN: never rename (Tests/param_ids.txt)
    inline const char* legacy (const char* old)
    {
        static const char* map[][2] {
            { "trigger", "trg_mode" },
            { "timing", "trg_timing" },
            { "step", "trg_step" },
            { "step1", "trg_step1" },
            { "step2", "trg_step2" },
            { "step3", "trg_step3" },
            { "step4", "trg_step4" },
            { "offset", "trg_offset" },
            { "beats", "trg_beats" },
            { "delay_pct", "env_delay_pct" },
            { "hold_pct", "env_hold_pct" },
            { "decay_pct", "env_decay_pct" },
            { "gate_pct", "env_gate_pct" },
            { "delay_ms", "env_delay_ms" },
            { "hold_ms", "env_hold_ms" },
            { "decay_ms", "env_decay_ms" },
            { "gate_ms", "env_gate_ms" },
            { "duck", "env_duck" },
            { "initial", "env_initial" },
            { "swell", "env_swell" },
            { "curve", "env_curve" },
            { "sustain", "env_sustain" },
            { "decay_curve", "env_decay_curve" },
            { "fade", "env_fade" },
            { "source", "src_type" },
            { "blend", "osc_blend" },
            { "width", "osc_pw" },
            { "sine", "osc_sine" },
            { "warp", "osc_warp" },
            { "warp_amt", "osc_warp_amt" },
            { "warp_env", "osc_warp_env" },
            { "tail_highcut", "tail_high_cut" },
            { "tune", "pitch_tune" },
            { "fine", "pitch_fine" },
            { "key_track", "pitch_key_track" },
            { "fixed_note", "pitch_fixed_note" },
            { "pre_type", "flt_pre_type" },
            { "pre_hz", "flt_pre_hz" },
            { "pre_reso", "flt_pre_reso" },
            { "drive", "dist_drive" },
            { "drive_env", "dist_env" },
            { "bias", "dist_bias" },
            { "mix", "dist_mix" },
            { "low_cut", "eq_low_cut" },
            { "high_cut", "eq_high_cut" },
            { "bell_hz", "eq_bell_hz" },
            { "bell_gain", "eq_bell_gain" },
            { "bell_q", "eq_bell_q" },
            { "clip", "out_clip" },
            { "level", "out_level" },
            { "velocity", "midi_velocity" },
            { "stereo_width", "st_width" },
            { "width_split", "st_split" },
        };
        for (auto& e : map) if (std::strcmp (e[0], old) == 0) return e[1];
        return old;
    }
}

namespace Choices
{
    inline const juce::StringArray triggers { "PATTERN", "GATE" };
    inline const juce::StringArray timings { "SYNC", "MS" };
    inline const juce::StringArray steps { "1/4", "1/8", "1/16" };
    inline const juce::StringArray beats { "1", "2", "4", "8" };
    inline const juce::StringArray sources { "SYNTH", "TAIL", "SAMPLE" };
    inline const juce::StringArray warps { "OFF", "BEND", "SQUEEZE", "SYNC", "MIRROR" };
    inline const juce::StringArray distTypes { "SOFT", "TUBE", "HARD", "FOLD", "RAW", "CRUSH" };
    inline const juce::StringArray filterTypes { "OFF", "LP", "HP", "BP", "NOTCH" };
    inline const juce::StringArray oscWaves { "SINE", "TRI", "SAW", "SQUARE", "PULSE", "NOISE" };
    inline const juce::StringArray subOcts { "0", "-1" };
    inline const juce::StringArray clickAts { "START", "PEAK" };
    inline const juce::StringArray phaseModes { "RESET", "FREE" };
    inline const juce::StringArray pitchModes { "STABLE", "DROP", "RISE", "BOUNCE" };
}
