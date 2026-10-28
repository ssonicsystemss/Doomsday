#pragma once
#include <juce_core/juce_core.h>

namespace ParamIDs
{
    inline constexpr const char* versionText = "0.3";      // keep in step with CMakeLists.txt
    // TAIL
    inline constexpr const char* tailLevel = "tail_level";
    inline constexpr const char* tailStart = "tail_start";    // start pitch = START x note
    inline constexpr const char* tailDrop  = "tail_drop";
    inline constexpr const char* tailCurve = "tail_curve";
    inline constexpr const char* tailHold  = "tail_hold";
    inline constexpr const char* tailWave  = "tail_wave";
    inline constexpr const char* tune      = "tune";
    inline constexpr const char* pitchMode = "pitch_mode";
    // DISTORTION CHAIN
    inline constexpr const char* midHz     = "mid_hz";
    inline constexpr const char* midGain   = "mid_gain";
    inline constexpr const char* midQ      = "mid_q";
    inline constexpr const char* dist2Type = "dist2_type";
    inline constexpr const char* drive2    = "drive2";
    // ROOM / OTT
    inline constexpr const char* room      = "room";
    inline constexpr const char* roomSize  = "room_size";
    inline constexpr const char* roomInto  = "room_into";
    inline constexpr const char* ott       = "ott";
    // SAMPLE
    inline constexpr const char* smpLevel  = "smp_level";
    inline constexpr const char* smpPitch  = "smp_pitch";
    inline constexpr const char* smpStart  = "smp_start";
    // DISTORTION
    inline constexpr const char* distType  = "dist_type";
    inline constexpr const char* drive     = "drive";
    inline constexpr const char* tone      = "tone";
    inline constexpr const char* lowCut    = "low_cut";
    inline constexpr const char* highCut   = "high_cut";
    inline constexpr const char* scream    = "scream";
    inline constexpr const char* screamHz  = "scream_hz";
    inline constexpr const char* crunch    = "crunch";
    inline constexpr const char* crunchDec = "crunch_dec";
    inline constexpr const char* body      = "body";
    inline constexpr const char* grit      = "grit";
    inline constexpr const char* cleanSub  = "clean_sub";
    // PUNCH
    inline constexpr const char* punchOn    = "punch_on";
    inline constexpr const char* punchLevel = "punch_level";
    inline constexpr const char* punchStart = "punch_start";
    inline constexpr const char* punchEnd   = "punch_end";
    inline constexpr const char* punchDrop  = "punch_drop";
    inline constexpr const char* punchDecay = "punch_decay";
    inline constexpr const char* punchDrive = "punch_drive";
    // CLICK
    inline constexpr const char* chaos      = "chaos";
    inline constexpr const char* chaosDec   = "chaos_dec";
    inline constexpr const char* chaosHz    = "chaos_hz";
    inline constexpr const char* clickLevel = "click_level";
    inline constexpr const char* clickHz    = "click_hz";
    inline constexpr const char* clickLen   = "click_len";
    // MASTER
    inline constexpr const char* length   = "length";
    inline constexpr const char* clipDrive = "clip_drive";
    inline constexpr const char* clipSoft = "clip_soft";
    inline constexpr const char* velocity = "velocity";
    inline constexpr const char* output   = "output";
    inline constexpr const char* tailFine  = "tail_fine";
    inline constexpr const char* punchInto = "punch_into";
    inline constexpr const char* headOn    = "head_on";      // v2.1: FIXED HEAD - the start of the kick is the same on every note
    inline constexpr const char* headLen   = "head_len";
    inline constexpr const char* headHz    = "head_hz";
    inline constexpr const char* punchLen  = "punch_len";
    // ---- 4GE (new modules, 2026-10-23)
    inline constexpr const char* tailStartMode = "tail_start_mode";   // RATIO (x note) / FIXED Hz
    inline constexpr const char* tailStartHz   = "tail_start_hz";
    inline constexpr const char* oscALevel     = "osc_a_level";
    inline constexpr const char* oscBLevel     = "osc_b_level";
    inline constexpr const char* oscBType      = "osc_b_type";
    inline constexpr const char* oscBWidth     = "osc_b_width";
    inline constexpr const char* tailPhase     = "tail_phase";
    inline constexpr const char* tailAttack    = "tail_attack";
    inline constexpr const char* duck          = "duck";
    inline constexpr const char* preType       = "pre_type";
    inline constexpr const char* preHz         = "pre_hz";
    inline constexpr const char* preReso       = "pre_reso";
    inline constexpr const char* preEnv        = "pre_env";
    inline constexpr const char* preDecay      = "pre_decay";
    inline constexpr const char* bias1         = "bias1";
    inline constexpr const char* mix1          = "mix1";
    inline constexpr const char* bias2         = "bias2";
    inline constexpr const char* mix2          = "mix2";
    inline constexpr const char* autoGain      = "auto_gain";
    inline constexpr const char* subLevel      = "sub_level";
    inline constexpr const char* roomLowCut    = "room_lowcut";
    inline constexpr const char* punchType     = "punch_type";
    inline constexpr const char* punchAmount   = "punch_amount";
    inline constexpr const char* punchAttack   = "punch_attack";
    inline constexpr const char* punchPhase    = "punch_phase";
    inline constexpr const char* punchFiltType = "punch_filt_type";
    inline constexpr const char* punchFiltHz   = "punch_filt_hz";
    inline constexpr const char* punchFiltReso = "punch_filt_reso";
    inline constexpr const char* punchDistType = "punch_dist_type";
    inline constexpr const char* texSource     = "tex_source";
    inline constexpr const char* texLevel      = "tex_level";
    inline constexpr const char* texHz         = "tex_hz";
    inline constexpr const char* texReso       = "tex_reso";
    inline constexpr const char* texDecay      = "tex_decay";
    inline constexpr const char* texDrive      = "tex_drive";
    inline constexpr const char* texSemis      = "tex_semis";
    inline constexpr const char* texHp         = "tex_hp";
    inline constexpr const char* busType       = "bus_type";
    inline constexpr const char* busDrive      = "bus_drive";
    inline constexpr const char* busMix        = "bus_mix";
}

namespace Choices
{
    inline const juce::StringArray distTypes { "SOFT", "TUBE", "HARD", "FOLD", "RAW", "CRUSH" };
    inline const juce::StringArray dist2Types { "OFF", "SOFT", "TUBE", "HARD", "FOLD", "RAW", "CRUSH" };
    inline const juce::StringArray pitchModes { "CURVE", "DRAWN" };
    inline const juce::StringArray startModes { "RATIO", "HZ" };
    inline const juce::StringArray oscBTypes { "SAW", "PULSE", "TRI" };
    inline const juce::StringArray filterTypes { "OFF", "LP", "HP", "BP", "NOTCH" };
    inline const juce::StringArray punchTypes { "SINE", "TRI", "SAW", "NOISE", "RESO" };
    inline const juce::StringArray texSources { "WHITE", "PINK", "PULSE", "SAW" };
}
