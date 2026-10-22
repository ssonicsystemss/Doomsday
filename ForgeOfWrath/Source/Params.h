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
    // REVERSE BASS (v2.1)
    inline constexpr const char* rbOn      = "rb_on";
    inline constexpr const char* rbPattern = "rb_pattern";
    inline constexpr const char* rbIn      = "rb_in";
    inline constexpr const char* rbOut     = "rb_out";
    inline constexpr const char* rbCurve   = "rb_curve";
    inline constexpr const char* rbRise    = "rb_rise";
    inline constexpr const char* rbTune    = "rb_tune";
    inline constexpr const char* rbDrive   = "rb_drive";
    inline constexpr const char* rbTone    = "rb_tone";
    inline constexpr const char* rbLevel   = "rb_level";
    inline constexpr const char* tailFine  = "tail_fine";
    inline constexpr const char* punchInto = "punch_into";
    inline constexpr const char* headOn    = "head_on";      // v2.1: FIXED HEAD - the start of the kick is the same on every note
    inline constexpr const char* headLen   = "head_len";
    inline constexpr const char* headHz    = "head_hz";
    inline constexpr const char* punchLen  = "punch_len";
    inline constexpr const char* rbTail    = "rb_tail";
    inline constexpr const char* rbPeak    = "rb_peak";
    inline constexpr const char* rbPump    = "rb_pump";      // v2.1: SIDECHAIN shape (duck, release, shelf, cut) instead of SWELL
    inline constexpr const char* rbRelease = "rb_release";
    inline constexpr const char* rbFade    = "rb_fade";      // v2.1: where the reverse bass is loudest (then it fades to OUT)      // v2.1: REVERSE BASS from the kick's own tail played backwards    // v2.1: TOK punch length (0 = old decaying punch)   // v2.1: the punch goes through the tail's distortion    // v2.1: fine pitch of the tail in cents
}

namespace Choices
{
    inline const juce::StringArray distTypes { "SOFT", "TUBE", "HARD", "FOLD", "RAW" };
    inline const juce::StringArray dist2Types { "OFF", "SOFT", "TUBE", "HARD", "FOLD", "RAW" };
    inline const juce::StringArray pitchModes { "CURVE", "DRAWN" };
    inline const juce::StringArray rbPatterns { "1/4", "1/8", "1/16" };
}
