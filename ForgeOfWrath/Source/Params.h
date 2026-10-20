#pragma once
#include <juce_core/juce_core.h>

namespace ParamIDs
{
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
}

namespace Choices
{
    inline const juce::StringArray distTypes { "SOFT", "TUBE", "HARD", "FOLD", "RAW" };
    inline const juce::StringArray dist2Types { "OFF", "SOFT", "TUBE", "HARD", "FOLD", "RAW" };
    inline const juce::StringArray pitchModes { "CURVE", "DRAWN" };
}
