#pragma once
// =====================================================================
//  4GE - PARAMETER REGISTRY (step 0, the foundation of every module - MODULE09 9.1)
//  ONE table of every parameter: stable ID, name, module, kind, unit, range, default, skew, flags.
//  From it come: the APVTS layout, the enum P:: the DSP uses (no string lookups on the audio thread),
//  the frozen ID list (Tests/param_ids.txt), the GUI labels / tooltips and (module 09) the modulation targets.
//  RULES: the ORDER is the DAW automation index -> new parameters are only APPENDED at the end of the list,
//         an existing ID never changes (saved projects).  A macro is NOT a parameter here (RULES.md 2).
// =====================================================================
#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>

namespace fge
{
    enum class Kind : uint8_t { Float, Bool, Choice };
    enum class Unit : uint8_t { None, Hz, Ms, Db, Pct, Semi, Cents, Deg, Bip };
    enum Flags : uint32_t
    {
        FNone   = 0,
        FMod    = 1u << 0,      // may be a modulation / macro-mapping target (module 09)
        FSafety = 1u << 1,      // safety-critical: hard limits a modulation can never override
        FSmooth = 1u << 2,      // the DSP smooths it (gains / pans) - changes never click
        FGlobal = 1u << 3       // a global / mixer control (not part of one component's sound design)
    };

    // choice lists (indices are saved - only append items at the end)
    namespace Choices
    {
        const juce::StringArray& punchSources();
        const juce::StringArray& bodySources();
        const juce::StringArray& tailSources();
        const juce::StringArray& subSources();
        const juce::StringArray& tailCharacters();
        const juce::StringArray& pitchModes();
        const juce::StringArray& voiceModes();
        const juce::StringArray& envModes();
        const juce::StringArray& polarities();
        const juce::StringArray& triggers();
        const juce::StringArray& linkModes();
        const juce::StringArray& parts();
        const juce::StringArray& stageTypes();
        const juce::StringArray& none();
    }

    // ------------------------------------------------------------------ the table
    //  F (enum, id, name, unit, min, max, default, skewCentre (0 = linear), flags)
    //  B (enum, id, name, default, flags)
    //  C (enum, id, name, choices, default, flags)
    //  MODULE 01 - CORE KICK ENGINE
#define FGE_PARAMS(F, B, C) \
    /* ---- core / global */ \
    F (coreLength,   "core_length",     "Length",              Unit::Ms,    100.f, 2500.f, 450.f, 450.f, FMod | FGlobal) \
    F (coreMaster,   "core_master",     "Master",              Unit::Db,    -60.f,  6.f,    0.f,   0.f,  FSmooth | FSafety | FGlobal) \
    B (coreAutoLen,  "core_auto_length","Auto Length",         true,  FGlobal) \
    B (corePhaseRst, "core_phase_reset","Phase Reset",         true,  FGlobal) \
    B (coreKeyTrack, "core_key_track",  "Pitch Tracking",      true,  FGlobal) \
    C (coreVoice,    "core_voice_mode", "Voice Mode",          voiceModes, 0, FGlobal) \
    F (coreVelocity, "core_velocity",   "Velocity Sensitivity",Unit::Pct,   0.f,    1.f,    0.f,   0.f,  FGlobal) \
    /* ---- pitch engine (basic, module 01 - module 02 makes it multipoint per component) */ \
    C (peMode,       "pe_mode",         "Pitch Mode",          pitchModes, 0, FNone) \
    F (peStart,      "pe_start",        "Start Pitch",         Unit::Hz,    20.f,  2000.f, 220.f, 220.f, FMod) \
    F (peEnd,        "pe_end",          "End Pitch",           Unit::Hz,    20.f,  2000.f, 49.f,  100.f, FMod) \
    F (peTime,       "pe_time",         "Pitch Time",          Unit::Ms,    1.f,   1000.f, 80.f,  80.f,  FMod) \
    F (peCurve,      "pe_curve",        "Pitch Curve",         Unit::Pct,   0.f,    1.f,   0.3f,  0.f,   FMod) \
    F (peAmount,     "pe_amount",       "Pitch Amount",        Unit::Semi,  -48.f, 48.f,   0.f,   0.f,   FMod) \
    F (peFine,       "pe_fine",         "Pitch Fine",          Unit::Cents, -100.f, 100.f, 0.f,   0.f,   FMod) \
    /* ---- PUNCH */ \
    C (punchSrc,     "punch_source",    "Punch Source",        punchSources, 0, FNone) \
    F (punchLevel,   "punch_level",     "Punch Level",         Unit::Db,    -60.f, 12.f,  -9.f,   0.f,   FMod | FSmooth) \
    F (punchPitch,   "punch_pitch",     "Punch Pitch",         Unit::Hz,    20.f,  2000.f, 320.f, 300.f, FMod) \
    F (punchGlide,   "punch_glide",     "Punch Glide",         Unit::Ms,    0.5f,  100.f,  9.f,   12.f,  FMod) \
    F (punchLen,     "punch_length",    "Punch Length",        Unit::Ms,    1.f,   150.f,  35.f,  35.f,  FMod) \
    F (punchAttack,  "punch_attack",    "Punch Attack",        Unit::Ms,    0.f,   20.f,   0.f,   3.f,   FMod) \
    F (punchDecay,   "punch_decay",     "Punch Decay",         Unit::Ms,    1.f,   150.f,  22.f,  30.f,  FMod) \
    F (punchSustain, "punch_sustain",   "Punch Sustain",       Unit::Pct,   0.f,    1.f,   0.35f, 0.f,   FMod) \
    F (punchRelease, "punch_release",   "Punch Release",       Unit::Ms,    0.f,   50.f,   6.f,   8.f,   FMod) \
    F (punchTone,    "punch_tone",      "Punch Tone",          Unit::Hz,    20.f,  20000.f, 9000.f, 1500.f, FMod) \
    F (punchDrive,   "punch_drive",     "Punch Drive",         Unit::Pct,   0.f,    1.f,   0.25f, 0.f,   FMod) \
    F (punchMix,     "punch_mix",       "Punch Mix",           Unit::Pct,   0.f,    1.f,   1.f,   0.f,   FMod) \
    F (punchPhase,   "punch_phase",     "Punch Phase",         Unit::Deg,   0.f,   360.f,  0.f,   0.f,   FNone) \
    /* ---- BODY */ \
    C (bodySrc,      "body_source",     "Body Source",         bodySources, 0, FNone) \
    F (bodyLevel,    "body_level",      "Body Level",          Unit::Db,    -60.f, 12.f,  -13.f,   0.f,   FMod | FSmooth) \
    F (bodyPitch,    "body_pitch",      "Body Pitch",          Unit::Hz,    20.f,  2000.f, 150.f, 200.f, FMod) \
    F (bodyGlide,    "body_glide",      "Body Glide",          Unit::Ms,    1.f,   500.f,  45.f,  50.f,  FMod) \
    F (bodyLen,      "body_length",     "Body Length",         Unit::Ms,    10.f,  1000.f, 220.f, 220.f, FMod) \
    F (bodyAttack,   "body_attack",     "Body Attack",         Unit::Ms,    0.f,   100.f,  1.5f,  10.f,  FMod) \
    F (bodyDecay,    "body_decay",      "Body Decay",          Unit::Ms,    1.f,   1000.f, 120.f, 120.f, FMod) \
    F (bodySustain,  "body_sustain",    "Body Sustain",        Unit::Pct,   0.f,    1.f,   0.3f,  0.f,   FMod) \
    F (bodyRelease,  "body_release",    "Body Release",        Unit::Ms,    0.f,   500.f,  50.f,  60.f,  FMod) \
    F (bodyTone,     "body_tone",       "Body Tone",           Unit::Hz,    20.f,  20000.f, 2400.f, 1500.f, FMod) \
    F (bodyReso,     "body_resonance",  "Body Resonance",      Unit::Pct,   0.f,    1.f,   0.25f, 0.f,   FMod) \
    F (bodyDrive,    "body_drive",      "Body Drive",          Unit::Pct,   0.f,    1.f,   0.3f,  0.f,   FMod) \
    F (bodyMix,      "body_mix",        "Body Mix",            Unit::Pct,   0.f,    1.f,   1.f,   0.f,   FMod) \
    F (bodyPhase,    "body_phase",      "Body Phase",          Unit::Deg,   0.f,   360.f,  0.f,   0.f,   FNone) \
    /* ---- TAIL (its pitch = the pitch engine: START -> END / the played note) */ \
    C (tailSrc,      "tail_source",     "Tail Source",         tailSources, 0, FNone) \
    F (tailLevel,    "tail_level",      "Tail Level",          Unit::Db,    -60.f, 12.f,  -6.f,   0.f,   FMod | FSmooth) \
    F (tailLen,      "tail_length",     "Tail Length",         Unit::Ms,    10.f,  2500.f, 420.f, 420.f, FMod) \
    F (tailAttack,   "tail_attack",     "Tail Attack",         Unit::Ms,    0.f,   200.f,  0.f,   10.f,  FMod) \
    F (tailDecay,    "tail_decay",      "Tail Decay",          Unit::Ms,    1.f,   2500.f, 700.f, 400.f, FMod) \
    F (tailSustain,  "tail_sustain",    "Tail Sustain",        Unit::Pct,   0.f,    1.f,   0.8f,  0.f,   FMod) \
    F (tailRelease,  "tail_release",    "Tail Release",        Unit::Ms,    0.f,   1000.f, 70.f,  80.f,  FMod) \
    F (tailTone,     "tail_tone",       "Tail Tone",           Unit::Hz,    20.f,  20000.f, 9000.f, 1500.f, FMod) \
    F (tailReso,     "tail_resonance",  "Tail Resonance",      Unit::Pct,   0.f,    1.f,   0.15f, 0.f,   FMod) \
    F (tailDrive,    "tail_drive",      "Tail Drive",          Unit::Pct,   0.f,    1.f,   0.7f,  0.f,   FMod) \
    C (tailChar,     "tail_character",  "Tail Character",      tailCharacters, 2, FNone) \
    F (tailMix,      "tail_mix",        "Tail Mix",            Unit::Pct,   0.f,    1.f,   1.f,   0.f,   FMod) \
    F (tailPw,       "tail_pulse_width","Tail Pulse Width",    Unit::Pct,   0.05f, 0.95f,  0.5f,  0.f,   FMod) \
    F (tailPhase,    "tail_phase",      "Tail Phase",          Unit::Deg,   0.f,   360.f,  0.f,   0.f,   FNone) \
    /* ---- SUB (on the end pitch / the played note when tracking is on) */ \
    C (subSrc,       "sub_source",      "Sub Source",          subSources, 0, FNone) \
    F (subLevel,     "sub_level",       "Sub Level",           Unit::Db,    -60.f, 12.f,  -12.f,   0.f,   FMod | FSmooth) \
    F (subPitch,     "sub_pitch",       "Sub Pitch",           Unit::Hz,    20.f,  120.f,  49.f,  50.f,  FMod) \
    F (subLen,       "sub_length",      "Sub Length",          Unit::Ms,    10.f,  1500.f, 380.f, 380.f, FMod) \
    F (subAttack,    "sub_attack",      "Sub Attack",          Unit::Ms,    0.f,   100.f,  12.f,  15.f,  FMod) \
    F (subDecay,     "sub_decay",       "Sub Decay",           Unit::Ms,    1.f,   1500.f, 900.f, 400.f, FMod) \
    F (subSustain,   "sub_sustain",     "Sub Sustain",         Unit::Pct,   0.f,    1.f,   0.85f, 0.f,   FMod) \
    F (subRelease,   "sub_release",     "Sub Release",         Unit::Ms,    0.f,   1000.f, 80.f,  80.f,  FMod) \
    F (subDrive,     "sub_drive",       "Sub Drive",           Unit::Pct,   0.f,    1.f,   0.f,   0.f,   FMod) \
    F (subPhase,     "sub_phase",       "Sub Phase",           Unit::Deg,   0.f,   360.f,  0.f,   0.f,   FNone) \
    /* ---- CORE MIXER (channel = PUNCH / BODY / TAIL / SUB) */ \
    F (mixPunchLvl,  "mix_punch_level", "Mixer Punch Level",   Unit::Db,    -60.f,  6.f,   0.f,   0.f,   FMod | FSmooth | FGlobal) \
    F (mixPunchPan,  "mix_punch_pan",   "Mixer Punch Pan",     Unit::Bip,   -1.f,   1.f,   0.f,   0.f,   FMod | FSmooth | FGlobal) \
    B (mixPunchMute, "mix_punch_mute",  "Punch Mute",          false, FGlobal) \
    B (mixPunchSolo, "mix_punch_solo",  "Punch Solo",          false, FGlobal) \
    B (mixPunchInv,  "mix_punch_invert","Punch Phase Invert",  false, FGlobal) \
    F (mixBodyLvl,   "mix_body_level",  "Mixer Body Level",    Unit::Db,    -60.f,  6.f,   0.f,   0.f,   FMod | FSmooth | FGlobal) \
    F (mixBodyPan,   "mix_body_pan",    "Mixer Body Pan",      Unit::Bip,   -1.f,   1.f,   0.f,   0.f,   FMod | FSmooth | FGlobal) \
    B (mixBodyMute,  "mix_body_mute",   "Body Mute",           false, FGlobal) \
    B (mixBodySolo,  "mix_body_solo",   "Body Solo",           false, FGlobal) \
    B (mixBodyInv,   "mix_body_invert", "Body Phase Invert",   false, FGlobal) \
    F (mixTailLvl,   "mix_tail_level",  "Mixer Tail Level",    Unit::Db,    -60.f,  6.f,   0.f,   0.f,   FMod | FSmooth | FGlobal) \
    F (mixTailPan,   "mix_tail_pan",    "Mixer Tail Pan",      Unit::Bip,   -1.f,   1.f,   0.f,   0.f,   FMod | FSmooth | FGlobal) \
    B (mixTailMute,  "mix_tail_mute",   "Tail Mute",           false, FGlobal) \
    B (mixTailSolo,  "mix_tail_solo",   "Tail Solo",           false, FGlobal) \
    B (mixTailInv,   "mix_tail_invert", "Tail Phase Invert",   false, FGlobal) \
    F (mixSubLvl,    "mix_sub_level",   "Mixer Sub Level",     Unit::Db,    -60.f,  6.f,   0.f,   0.f,   FMod | FSmooth | FGlobal) \
    F (mixSubPan,    "mix_sub_pan",     "Mixer Sub Pan",       Unit::Bip,   -1.f,   1.f,   0.f,   0.f,   FMod | FSmooth | FGlobal) \
    B (mixSubMute,   "mix_sub_mute",    "Sub Mute",            false, FGlobal) \
    B (mixSubSolo,   "mix_sub_solo",    "Sub Solo",            false, FGlobal) \
    B (mixSubInv,    "mix_sub_invert",  "Sub Phase Invert",    false, FGlobal) \
    /* ================= MODULE 02 - ADVANCED PITCH & ENVELOPE ENGINE ================= */ \
    F (envTimeScale, "env_time_scale",  "Envelope Time Scale", Unit::Pct,   0.1f,   4.f,   1.f,   1.f,   FMod | FGlobal) \
    F (envTimeOffset,"env_time_offset", "Envelope Time Offset",Unit::Ms,    0.f,  2500.f,  0.f,  200.f,  FMod | FGlobal) \
    B (envSync,      "env_sync",        "Envelope Global Sync",false, FGlobal) \
    F (envLocalBpm,  "env_local_bpm",   "Local BPM",           Unit::None,  60.f,  220.f, 150.f,  0.f,   FGlobal) \
    FGE_PITCH_PART (F, B, C, punch, "punch", "Punch") \
    FGE_PITCH_PART (F, B, C, body,  "body",  "Body") \
    FGE_PITCH_PART (F, B, C, tail,  "tail",  "Tail") \
    FGE_PITCH_PART (F, B, C, sub,   "sub",   "Sub") \
    C (punchPMode,   "punch_penv_mode", "Punch Pitch Mode",    pitchModes, 0, FNone) \
    F (punchPCurve,  "punch_penv_curve","Punch Pitch Curve",   Unit::Pct,   0.f,    1.f,   0.15f, 0.f,   FMod) \
    C (bodyPMode,    "body_penv_mode",  "Body Pitch Mode",     pitchModes, 0, FNone) \
    F (bodyPCurve,   "body_penv_curve", "Body Pitch Curve",    Unit::Pct,   0.f,    1.f,   0.3f,  0.f,   FMod) \
    C (subPMode,     "sub_penv_mode",   "Sub Pitch Mode",      pitchModes, 0, FNone) \
    F (subPCurve,    "sub_penv_curve",  "Sub Pitch Curve",     Unit::Pct,   0.f,    1.f,   0.3f,  0.f,   FMod) \
    F (subPDepth,    "sub_penv_depth",  "Sub Pitch Depth",     Unit::Semi,  -48.f, 48.f,   0.f,   0.f,   FMod) \
    F (subPTime,     "sub_penv_time",   "Sub Pitch Time",      Unit::Ms,    1.f,  1000.f, 60.f,  60.f,   FMod) \
    FGE_AMP_SLOT (F, B, C, punchAmp, "punch_amp_env", "Punch Amp Env") \
    FGE_AMP_SLOT (F, B, C, bodyAmp,  "body_amp_env",  "Body Amp Env") \
    FGE_AMP_SLOT (F, B, C, tailAmp,  "tail_amp_env",  "Tail Amp Env") \
    FGE_AMP_SLOT (F, B, C, subAmp,   "sub_amp_env",   "Sub Amp Env") \
    FGE_MOD_SLOT (F, B, C, punchFlt, "punch_filter_env", "Punch Filter Env") \
    FGE_MOD_SLOT (F, B, C, bodyFlt,  "body_filter_env",  "Body Filter Env") \
    FGE_MOD_SLOT (F, B, C, tailFlt,  "tail_filter_env",  "Tail Filter Env") \
    FGE_MOD_SLOT (F, B, C, punchDrv, "punch_drive_env",  "Punch Drive Env") \
    FGE_MOD_SLOT (F, B, C, bodyDrv,  "body_drive_env",   "Body Drive Env") \
    FGE_MOD_SLOT (F, B, C, tailDrv,  "tail_drive_env",   "Tail Drive Env") \
    FGE_MOD_SLOT (F, B, C, subDrv,   "sub_drive_env",    "Sub Drive Env") \
    FGE_MOD_SLOT (F, B, C, bodyRes,  "body_reso_env",    "Body Resonance Env") \
    FGE_MOD_SLOT (F, B, C, tailRes,  "tail_reso_env",    "Tail Resonance Env") \
    /* ================= SOUND PASS (2026-11-04, after the owner's first A/B) - early parts of modules 03 / 05 / 07 / 08 ================= */ \
    F (tailUniVoices,"tail_uni_voices", "Tail Unison Voices",  Unit::None,  1.f,    7.f,   5.f,   0.f,   FNone) \
    F (tailUniDetune,"tail_uni_detune", "Tail Unison Detune",  Unit::Cents, 0.f,   60.f,  18.f,   0.f,   FMod) \
    F (tailUniDrift, "tail_uni_drift",  "Tail Unison Drift",   Unit::Cents, 0.f,   30.f,   6.f,   0.f,   FMod) \
    F (tailUniMix,   "tail_uni_mix",    "Tail Unison Mix",     Unit::Pct,   0.f,    1.f,   0.6f,  0.f,   FMod) \
    F (tailWidth,    "tail_width",      "Tail Width",          Unit::Pct,   0.f,    1.f,   0.4f,  0.f,   FMod) \
    F (tailWidthFrom,"tail_width_from", "Tail Width From",     Unit::Hz,    80.f, 1500.f, 350.f, 350.f,  FMod) \
    C (tailSt2Type,  "tail_st2_type",   "Tail Stage 2 Type",   stageTypes, 0, FNone) \
    F (tailSt2Drive, "tail_st2_drive",  "Tail Stage 2 Drive",  Unit::Db,    0.f,   36.f,   6.f,  10.f,   FMod) \
    F (tailSt2Bias,  "tail_st2_bias",   "Tail Stage 2 Bias",   Unit::Bip,   -1.f,   1.f,   0.f,   0.f,   FMod) \
    F (tailSt2Mix,   "tail_st2_mix",    "Tail Stage 2 Mix",    Unit::Pct,   0.f,    1.f,   1.f,   0.f,   FMod) \
    F (tailMidHz,    "tail_mid_hz",     "Tail Mid EQ Freq",    Unit::Hz,    80.f, 8000.f, 800.f, 800.f,  FMod) \
    F (tailMidGain,  "tail_mid_gain",   "Tail Mid EQ Gain",    Unit::Db,   -18.f,  18.f,   0.f,   0.f,   FMod) \
    F (tailMidQ,     "tail_mid_q",      "Tail Mid EQ Q",       Unit::None,  0.3f,   8.f,   1.f,   1.2f,  FMod) \
    F (tailLowCut,   "tail_low_cut",    "Tail Low Cut",        Unit::Hz,    20.f,  400.f,  20.f,  80.f,  FMod) \
    F (tailHighCut,  "tail_high_cut",   "Tail High Cut",       Unit::Hz,   500.f, 20000.f, 20000.f, 5000.f, FMod) \
    F (clipDrive,    "master_clip_drive","Clip Drive",         Unit::Db,    0.f,   24.f,   0.f,   6.f,   FMod | FGlobal) \
    F (clipSoft,     "master_clip_soft","Clip Softness",       Unit::Pct,   0.f,    1.f,   0.5f,  0.f,   FMod | FGlobal) \
    F (tailHoldLvl,  "tail_hold_level", "Tok Level",           Unit::Db,   -24.f,   6.f,   0.f,   0.f,   FMod)
    // ---- APPEND NEW MODULES BELOW (module 03 ...) - never insert above

    // per part pitch envelope (module 02, 5 + 6): ENABLE / AMOUNT / TIME SCALE / OFFSET + the LINK system
#define FGE_PITCH_PART(F, B, C, e, id, nm) \
    B (e##POn,      id "_penv_on",      nm " Pitch Env On",      true, FNone) \
    F (e##PAmount,  id "_penv_amount",  nm " Pitch Env Amount",  Unit::Pct,  0.f,   2.f,   1.f,   1.f,   FMod) \
    F (e##PScale,   id "_penv_scale",   nm " Pitch Time Scale",  Unit::Pct,  0.1f,  4.f,   1.f,   1.f,   FMod) \
    F (e##POffset,  id "_penv_offset",  nm " Pitch Env Offset",  Unit::Ms,   0.f, 2500.f,  0.f,  100.f,  FMod) \
    C (e##Link,     id "_link",         nm " Pitch Link",        linkModes, 0, FNone) \
    C (e##LinkSrc,  id "_link_source",  nm " Pitch Link Source", parts, 2, FNone) \
    F (e##FDepth,   id "_follow_depth", nm " Follow Depth",      Unit::Pct,  0.f,   2.f,   1.f,   1.f,   FMod) \
    F (e##FOffset,  id "_follow_offset",nm " Follow Offset",     Unit::Semi, -48.f, 48.f,  0.f,   0.f,   FMod) \
    F (e##FTime,    id "_follow_time",  nm " Follow Time",       Unit::Pct,  0.1f,  4.f,   1.f,   1.f,   FMod)
    // amplitude envelope (the A / D / S / R knobs of module 01 are its ADSR mode)
#define FGE_AMP_SLOT(F, B, C, e, id, nm) \
    C (e##Mode,     id "_mode",         nm " Mode",              envModes, 0, FNone) \
    F (e##Delay,    id "_delay",        nm " Delay",             Unit::Ms,   0.f, 2500.f,  0.f,  100.f,  FMod) \
    F (e##Curve,    id "_curve",        nm " Curve",             Unit::Pct,  0.f,   1.f,   0.5f,  0.f,   FMod) \
    F (e##Amount,   id "_amount",       nm " Amount",            Unit::Pct,  0.f,   1.f,   1.f,   0.f,   FMod) \
    C (e##Trig,     id "_trigger",      nm " Trigger",           triggers, 0, FNone) \
    B (e##Sync,     id "_sync",         nm " Sync",              false, FNone)
    // a modulation envelope (filter cutoff / drive / resonance)
#define FGE_MOD_SLOT(F, B, C, e, id, nm) \
    B (e##On,       id "_on",           nm " On",                false, FNone) \
    C (e##Mode,     id "_mode",         nm " Mode",              envModes, 0, FNone) \
    F (e##Attack,   id "_attack",       nm " Attack",            Unit::Ms,   0.f, 2500.f,  0.f,   20.f,  FMod) \
    F (e##Decay,    id "_decay",        nm " Decay",             Unit::Ms,   0.f, 2500.f, 120.f, 150.f,  FMod) \
    F (e##Sustain,  id "_sustain",      nm " Sustain",           Unit::Pct,  0.f,   1.f,   0.f,   0.f,   FMod) \
    F (e##Release,  id "_release",      nm " Release",           Unit::Ms,   0.f, 2500.f,  40.f,  80.f,  FMod) \
    F (e##Delay,    id "_delay",        nm " Delay",             Unit::Ms,   0.f, 2500.f,  0.f,  100.f,  FMod) \
    F (e##Curve,    id "_curve",        nm " Curve",             Unit::Pct,  0.f,   1.f,   0.5f,  0.f,   FMod) \
    F (e##Amount,   id "_amount",       nm " Amount",            Unit::Bip,  -1.f,  1.f,   0.5f,  0.f,   FMod) \
    C (e##Polarity, id "_polarity",     nm " Polarity",          polarities, 0, FNone) \
    C (e##Trig,     id "_trigger",      nm " Trigger",           triggers, 0, FNone) \
    B (e##Sync,     id "_sync",         nm " Sync",              false, FNone)

    namespace P
    {
#define FGE_ENUM_F(e, ...) e,
#define FGE_ENUM_B(e, ...) e,
#define FGE_ENUM_C(e, ...) e,
        enum Id : int { FGE_PARAMS (FGE_ENUM_F, FGE_ENUM_B, FGE_ENUM_C) count };
#undef FGE_ENUM_F
#undef FGE_ENUM_B
#undef FGE_ENUM_C
    }

    struct ParamDef
    {
        const char* id;
        const char* name;
        Kind kind;
        Unit unit;
        float min, max, def, centre;
        const juce::StringArray& (*choices)();
        uint32_t flags;
        bool has (uint32_t f) const { return (flags & f) != 0; }
    };

    // the whole table, index = P::Id
    const std::array<ParamDef, P::count>& registry();
    inline const ParamDef& def (int id) { return registry()[(size_t) id]; }

    // text for a value in the parameter's unit (GUI, DAW)
    juce::String formatValue (const ParamDef&, float v);
    float parseValue (const ParamDef&, const juce::String&);
    // a short tooltip text for each parameter (what it does to the sound)
    juce::String tooltip (int id);
}
