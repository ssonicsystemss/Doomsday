#pragma once
#include <juce_core/juce_core.h>

namespace ParamIDs
{
    inline constexpr const char* versionText = "0.2";      // keep in step with CMakeLists.txt
    // TRIGGER / TIMING
    inline constexpr const char* trigger   = "trigger";
    inline constexpr const char* timing    = "timing";
    inline constexpr const char* step      = "step";
    inline constexpr const char* step1     = "step1";
    inline constexpr const char* step2     = "step2";
    inline constexpr const char* step3     = "step3";
    inline constexpr const char* step4     = "step4";
    inline constexpr const char* offset    = "offset";
    inline constexpr const char* beats     = "beats";
    // ENVELOPE
    inline constexpr const char* delayPct  = "delay_pct";
    inline constexpr const char* holdPct   = "hold_pct";
    inline constexpr const char* decayPct  = "decay_pct";
    inline constexpr const char* gatePct   = "gate_pct";
    inline constexpr const char* delayMs   = "delay_ms";
    inline constexpr const char* holdMs    = "hold_ms";
    inline constexpr const char* decayMs   = "decay_ms";
    inline constexpr const char* gateMs    = "gate_ms";
    inline constexpr const char* duck      = "duck";
    inline constexpr const char* initial   = "initial";
    inline constexpr const char* swell     = "swell";
    inline constexpr const char* curve     = "curve";
    inline constexpr const char* sustain   = "sustain";
    inline constexpr const char* decayCurve = "decay_curve";
    inline constexpr const char* fade      = "fade";
    // SOURCE
    inline constexpr const char* source    = "source";
    inline constexpr const char* blend     = "blend";
    inline constexpr const char* width     = "width";
    inline constexpr const char* sine      = "sine";
    inline constexpr const char* warp      = "warp";
    inline constexpr const char* warpAmt   = "warp_amt";
    inline constexpr const char* warpEnv   = "warp_env";
    inline constexpr const char* tailStart = "tail_start";
    inline constexpr const char* tailDrop  = "tail_drop";
    inline constexpr const char* tailCurve = "tail_curve";
    inline constexpr const char* tailWave  = "tail_wave";
    inline constexpr const char* tailDist  = "tail_dist";
    inline constexpr const char* tailDrive = "tail_drive";
    inline constexpr const char* tailTone  = "tail_tone";
    inline constexpr const char* tailHighCut = "tail_highcut";
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
    inline constexpr const char* tune      = "tune";
    inline constexpr const char* fine      = "fine";
    inline constexpr const char* keyTrack  = "key_track";
    inline constexpr const char* fixedNote = "fixed_note";
    // TONE
    inline constexpr const char* preType   = "pre_type";
    inline constexpr const char* preHz     = "pre_hz";
    inline constexpr const char* preReso   = "pre_reso";
    inline constexpr const char* distType  = "dist_type";
    inline constexpr const char* drive     = "drive";
    inline constexpr const char* driveEnv  = "drive_env";
    inline constexpr const char* bias      = "bias";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* lowCut    = "low_cut";
    inline constexpr const char* highCut   = "high_cut";
    inline constexpr const char* bellHz    = "bell_hz";
    inline constexpr const char* bellGain  = "bell_gain";
    inline constexpr const char* bellQ     = "bell_q";
    inline constexpr const char* subHz     = "sub_hz";
    inline constexpr const char* subLevel  = "sub_level";
    inline constexpr const char* clip      = "clip";
    inline constexpr const char* level     = "level";
    inline constexpr const char* velocity  = "velocity";
    // UNISON / WIDTH (v0.2)
    inline constexpr const char* uniVoices = "uni_voices";
    inline constexpr const char* uniDetune = "uni_detune";
    inline constexpr const char* uniDrift  = "uni_drift";
    inline constexpr const char* uniMix    = "uni_mix";
    inline constexpr const char* stereoWidth = "stereo_width";     // ("width" is the pulse width)
    inline constexpr const char* widthSplit = "width_split";
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
    inline const juce::StringArray pitchModes { "STABLE", "DROP", "RISE", "BOUNCE" };
}
