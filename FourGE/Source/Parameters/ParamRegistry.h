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
        // module 03
        const juce::StringArray& oscWaves();
        const juce::StringArray& noiseTypes();
        const juce::StringArray& oscTracks();
        const juce::StringArray& phaseModes();
        const juce::StringArray& oscSelect();
        const juce::StringArray& uniModes();
        const juce::StringArray& uniPhases();
        const juce::StringArray& ringSources();
        const juce::StringArray& syncSources();
        const juce::StringArray& syncModes();
        const juce::StringArray& wtTables();
        const juce::StringArray& wtModes();
        // module 04
        const juce::StringArray& noiseKinds();
        const juce::StringArray& filterTypes();
        const juce::StringArray& noiseEnvSources();
        const juce::StringArray& randomModes();
        const juce::StringArray& trSources();
        const juce::StringArray& trSyncs();
        const juce::StringArray& busOutputs();
        const juce::StringArray& layerSources();
        const juce::StringArray& layerGroups();
        const juce::StringArray& layerEnvModes();
        const juce::StringArray& routeModes();
        const juce::StringArray& layerLinks();
        const juce::StringArray& soloModes();
        // module 05
        const juce::StringArray& distTypes();
        const juce::StringArray& distLocations();
        const juce::StringArray& distPaths();
        const juce::StringArray& distBands();
        const juce::StringArray& driveCurves();
        const juce::StringArray& ditherModes();
        const juce::StringArray& rectModes();
        const juce::StringArray& curveInterps();
        const juce::StringArray& stageOversampling();
        const juce::StringArray& distFilterTypes();
        const juce::StringArray& filterSlopes();
        const juce::StringArray& distModSources();
        const juce::StringArray& bandSolo();
        const juce::StringArray& globalDistPos();
        const juce::StringArray& offlineQuality();
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
    F (tailHoldLvl,  "tail_hold_level", "Tok Level",           Unit::Db,   -24.f,   6.f,   0.f,   0.f,   FMod) \
    FGE_OSC (F, B, C, 1) FGE_OSC (F, B, C, 2) FGE_OSC (F, B, C, 3) FGE_OSC (F, B, C, 4) \
    FGE_FM (F, 1, 2) FGE_FM (F, 1, 3) FGE_FM (F, 1, 4) FGE_FM (F, 2, 1) FGE_FM (F, 2, 3) FGE_FM (F, 2, 4) \
    FGE_FM (F, 3, 1) FGE_FM (F, 3, 2) FGE_FM (F, 3, 4) FGE_FM (F, 4, 1) FGE_FM (F, 4, 2) FGE_FM (F, 4, 3) \
    B (osc1Mute, "osc1_mute", "Osc 1 Mute", false, FNone) B (osc1Solo, "osc1_solo", "Osc 1 Solo", false, FNone) \
    B (osc2Mute, "osc2_mute", "Osc 2 Mute", false, FNone) B (osc2Solo, "osc2_solo", "Osc 2 Solo", false, FNone) \
    B (osc3Mute, "osc3_mute", "Osc 3 Mute", false, FNone) B (osc3Solo, "osc3_solo", "Osc 3 Solo", false, FNone) \
    B (osc4Mute, "osc4_mute", "Osc 4 Mute", false, FNone) B (osc4Solo, "osc4_solo", "Osc 4 Solo", false, FNone) \
    FGE_NOISE (F, B, C, NA, "noiseA", "Noise A") FGE_NOISE (F, B, C, NB, "noiseB", "Noise B") \
    FGE_TRANSIENT (F, B, C) \
    FGE_LAYER (F, B, C, 1) FGE_LAYER (F, B, C, 2) FGE_LAYER (F, B, C, 3) FGE_LAYER (F, B, C, 4) \
    FGE_LAYER (F, B, C, 5) FGE_LAYER (F, B, C, 6) FGE_LAYER (F, B, C, 7) FGE_LAYER (F, B, C, 8) \
    FGE_GROUP (F, B, C, Punch, "punch", "Punch", 0) FGE_GROUP (F, B, C, Body, "body", "Body", 1) FGE_GROUP (F, B, C, Tail, "tail", "Tail", 2) \
    FGE_GROUP (F, B, C, Sub, "sub", "Sub", 3) FGE_GROUP (F, B, C, Texture, "texture", "Texture", 4) FGE_GROUP (F, B, C, Custom, "custom", "Custom", 5) \
    F (busTextureLvl, "bus_texture_level", "Texture Bus Level",  Unit::Db, -60.f, 12.f, 0.f, 0.f, FMod | FSmooth | FGlobal) \
    F (busCustomLvl,  "bus_custom_level",  "Custom Bus Level",   Unit::Db, -60.f, 12.f, 0.f, 0.f, FMod | FSmooth | FGlobal) \
    C (layerSoloMode, "layer_solo_mode",   "Layer Solo Mode",    soloModes, 0, FGlobal) \
    FGE_DIST (F, B, C, 1) FGE_DIST (F, B, C, 2) FGE_DIST (F, B, C, 3) FGE_DIST (F, B, C, 4) FGE_DIST (F, B, C, 5) FGE_DIST (F, B, C, 6) \
    FGE_DIST (F, B, C, 7) FGE_DIST (F, B, C, 8) FGE_DIST (F, B, C, 9) FGE_DIST (F, B, C, 10) FGE_DIST (F, B, C, 11) FGE_DIST (F, B, C, 12) \
    FGE_DIST (F, B, C, 13) FGE_DIST (F, B, C, 14) FGE_DIST (F, B, C, 15) FGE_DIST (F, B, C, 16) \
    F (distXLow,    "dist_xover_low",    "Distortion Low / Mid Crossover", Unit::Hz,  20.f,  2000.f, 200.f,  200.f, FMod) \
    F (distXHigh,   "dist_xover_high",   "Distortion Mid / High Crossover",Unit::Hz, 200.f, 20000.f, 2500.f, 2500.f, FMod) \
    C (distBandSolo,"dist_band_solo",    "Distortion Band Solo",           bandSolo, 0, FNone) \
    C (distGlobPos, "dist_global_position","Global Distortion Position",   globalDistPos, 0, FNone) \
    C (distOffline, "dist_offline_quality","Offline Render Quality",       offlineQuality, 0, FNone)
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
    // module 03: one oscillator (4 now, the engine is sized for 8). Routed INTO the parts (before their tone / drive),
    // so an oscillator sent to TAIL goes through the tail's character, envelope and cuts.
#define FGE_OSC(F, B, C, n) \
    B (osc##n##On,       "osc" #n "_on",         "Osc " #n " On",            false, FNone) \
    C (osc##n##Wave,     "osc" #n "_wave",       "Osc " #n " Waveform",      oscWaves, 2, FNone) \
    C (osc##n##Noise,    "osc" #n "_noise",      "Osc " #n " Noise Type",    noiseTypes, 0, FNone) \
    C (osc##n##Track,    "osc" #n "_track",      "Osc " #n " Pitch From",    oscTracks, 2, FNone) \
    F (osc##n##Freq,     "osc" #n "_freq",       "Osc " #n " Frequency",     Unit::Hz,    20.f,  4000.f, 110.f, 200.f, FMod) \
    F (osc##n##Oct,      "osc" #n "_octave",     "Osc " #n " Octave",        Unit::None,  -4.f,   4.f,   0.f,   0.f,   FMod) \
    F (osc##n##Semi,     "osc" #n "_semitone",   "Osc " #n " Semitone",      Unit::Semi, -24.f,  24.f,   0.f,   0.f,   FMod) \
    F (osc##n##Fine,     "osc" #n "_fine",       "Osc " #n " Fine",          Unit::Cents,-100.f, 100.f,  0.f,   0.f,   FMod) \
    F (osc##n##Ratio,    "osc" #n "_ratio",      "Osc " #n " FM Ratio",      Unit::None,   0.1f,  32.f,  1.f,   2.f,   FMod) \
    F (osc##n##RatioFine,"osc" #n "_ratio_fine", "Osc " #n " FM Fine",       Unit::Bip,   -1.f,   1.f,   0.f,   0.f,   FMod) \
    F (osc##n##Level,    "osc" #n "_level",      "Osc " #n " Level",         Unit::Db,   -60.f,  12.f,  -6.f,   0.f,   FMod | FSmooth) \
    F (osc##n##Pan,      "osc" #n "_pan",        "Osc " #n " Pan",           Unit::Bip,   -1.f,   1.f,   0.f,   0.f,   FMod) \
    F (osc##n##Phase,    "osc" #n "_phase",      "Osc " #n " Phase",         Unit::Deg,    0.f, 360.f,   0.f,   0.f,   FNone) \
    C (osc##n##PhMode,   "osc" #n "_phase_mode", "Osc " #n " Phase Mode",    phaseModes, 0, FNone) \
    F (osc##n##PhRand,   "osc" #n "_phase_random","Osc " #n " Phase Random", Unit::Pct,   0.f,   1.f,   1.f,   0.f,   FNone) \
    C (osc##n##PhSrc,    "osc" #n "_phase_source","Osc " #n " Phase Source", oscSelect, 0, FNone) \
    B (osc##n##Invert,   "osc" #n "_invert",     "Osc " #n " Phase Invert",  false, FNone) \
    F (osc##n##Pw,       "osc" #n "_pw",         "Osc " #n " Pulse Width",   Unit::Pct,  0.01f,  0.99f, 0.5f,  0.f,   FMod) \
    F (osc##n##Uni,      "osc" #n "_unison",     "Osc " #n " Unison",        Unit::None,   1.f,  16.f,  1.f,   0.f,   FMod) \
    F (osc##n##Detune,   "osc" #n "_detune",     "Osc " #n " Detune",        Unit::Pct,    0.f,   1.f,   0.2f,  0.f,   FMod) \
    F (osc##n##Spread,   "osc" #n "_spread",     "Osc " #n " Stereo Spread", Unit::Pct,    0.f,   1.f,   0.5f,  0.f,   FMod) \
    F (osc##n##Blend,    "osc" #n "_blend",      "Osc " #n " Unison Blend",  Unit::Pct,    0.f,   1.f,   1.f,   0.f,   FMod) \
    C (osc##n##UniMode,  "osc" #n "_unison_mode","Osc " #n " Unison Mode",   uniModes, 0, FNone) \
    C (osc##n##UniPh,    "osc" #n "_unison_phase","Osc " #n " Unison Phase", uniPhases, 1, FNone) \
    F (osc##n##Seed,     "osc" #n "_seed",       "Osc " #n " Seed",          Unit::None,   0.f, 9999.f, (float) n, 0.f, FNone) \
    F (osc##n##FmFb,     "osc" #n "_fm_feedback","Osc " #n " FM Feedback",   Unit::Pct,    0.f,   1.f,   0.f,   0.f,   FMod) \
    F (osc##n##FmEnv,    "osc" #n "_fm_env",     "Osc " #n " FM Envelope",   Unit::Pct,    0.f,   1.f,   0.f,   0.f,   FMod) \
    F (osc##n##ModDecay, "osc" #n "_mod_decay",  "Osc " #n " FM / Ring Decay",Unit::Ms,    1.f, 2000.f, 150.f, 150.f, FMod) \
    C (osc##n##RingSrc,  "osc" #n "_ring_source","Osc " #n " Ring Source",   ringSources, 0, FNone) \
    F (osc##n##RingAmt,  "osc" #n "_ring_amount","Osc " #n " Ring Amount",   Unit::Pct,    0.f,   1.f,   1.f,   0.f,   FMod) \
    F (osc##n##RingMix,  "osc" #n "_ring_mix",   "Osc " #n " Ring Mix",      Unit::Pct,    0.f,   1.f,   1.f,   0.f,   FMod) \
    F (osc##n##RingHz,   "osc" #n "_ring_freq",  "Osc " #n " Ring Frequency",Unit::Hz,    20.f, 8000.f, 440.f, 500.f, FMod) \
    F (osc##n##RingEnv,  "osc" #n "_ring_env",   "Osc " #n " Ring Envelope", Unit::Pct,    0.f,   1.f,   0.f,   0.f,   FMod) \
    C (osc##n##SyncSrc,  "osc" #n "_sync_source","Osc " #n " Sync Master",   syncSources, 0, FNone) \
    C (osc##n##SyncMode, "osc" #n "_sync_mode",  "Osc " #n " Sync Mode",     syncModes, 0, FNone) \
    F (osc##n##SyncAmt,  "osc" #n "_sync_amount","Osc " #n " Sync Amount",   Unit::Pct,    0.f,   1.f,   0.3f,  0.f,   FMod) \
    F (osc##n##ToPunch,  "osc" #n "_to_punch",   "Osc " #n " -> Punch",      Unit::Db,   -60.f,  12.f, -60.f,   0.f,   FMod | FSmooth) \
    F (osc##n##ToBody,   "osc" #n "_to_body",    "Osc " #n " -> Body",       Unit::Db,   -60.f,  12.f, -60.f,   0.f,   FMod | FSmooth) \
    F (osc##n##ToTail,   "osc" #n "_to_tail",    "Osc " #n " -> Tail",       Unit::Db,   -60.f,  12.f,   0.f,   0.f,   FMod | FSmooth) \
    F (osc##n##ToSub,    "osc" #n "_to_sub",     "Osc " #n " -> Sub",        Unit::Db,   -60.f,  12.f, -60.f,   0.f,   FMod | FSmooth) \
    C (osc##n##WtTable,  "osc" #n "_wt_table",   "Osc " #n " Wavetable",     wtTables, 0, FNone) \
    F (osc##n##WtPos,    "osc" #n "_wt_position","Osc " #n " WT Position",   Unit::Pct,    0.f,   1.f,   0.f,   0.f,   FMod) \
    F (osc##n##WtMorph,  "osc" #n "_wt_morph",   "Osc " #n " WT Morph",      Unit::Pct,    0.f,   1.f,   0.f,   0.f,   FMod) \
    C (osc##n##WtMode,   "osc" #n "_wt_mode",    "Osc " #n " WT Mode",       wtModes, 0, FNone) \
    F (osc##n##WtStart,  "osc" #n "_wt_start",   "Osc " #n " WT Start",      Unit::Pct,    0.f,   1.f,   0.f,   0.f,   FMod) \
    F (osc##n##WtEnd,    "osc" #n "_wt_end",     "Osc " #n " WT End",        Unit::Pct,    0.f,   1.f,   1.f,   0.f,   FMod) \
    F (osc##n##WtRate,   "osc" #n "_wt_scan",    "Osc " #n " WT Scan Time",  Unit::Ms,     5.f, 4000.f, 300.f, 300.f, FMod) \
    F (osc##n##HAmount,  "osc" #n "_h_amount",   "Osc " #n " Harmonic Amount",Unit::Pct,   0.f,   2.f,   1.f,   1.f,   FMod) \
    F (osc##n##HOdd,     "osc" #n "_h_odd",      "Osc " #n " Odd Harmonics", Unit::Pct,    0.f,   2.f,   1.f,   1.f,   FMod) \
    F (osc##n##HEven,    "osc" #n "_h_even",     "Osc " #n " Even Harmonics",Unit::Pct,    0.f,   2.f,   1.f,   1.f,   FMod) \
    F (osc##n##HRoll,    "osc" #n "_h_rolloff",  "Osc " #n " Harmonic Roll-off",Unit::Pct, 0.f,   1.f,   0.f,   0.f,   FMod) \
    F (osc##n##HTilt,    "osc" #n "_h_tilt",     "Osc " #n " Harmonic Tilt", Unit::Bip,   -1.f,   1.f,   0.f,   0.f,   FMod) \
    F (osc##n##HSpread,  "osc" #n "_h_spread",   "Osc " #n " Harmonic Spread",Unit::Pct,   0.f,   1.f,   0.f,   0.f,   FMod) \
    F (osc##n##HPhase,   "osc" #n "_h_phase",    "Osc " #n " Harmonic Phase",Unit::Pct,    0.f,   1.f,   0.f,   0.f,   FMod) \
    F (osc##n##HRandom,  "osc" #n "_h_random",   "Osc " #n " Harmonic Random",Unit::Pct,   0.f,   1.f,   0.f,   0.f,   FMod)
    // module 04: NOISE generator (A / B)
#define FGE_NOISE(F, B, C, X, id, nm) \
    B (noise##X##On,     id "_on",          nm " On",               false, FNone) \
    C (noise##X##Type,   id "_type",        nm " Source",           noiseKinds, 0, FNone) \
    F (noise##X##Level,  id "_level",       nm " Level",            Unit::Db,  -60.f,  12.f, -12.f,  0.f,  FMod | FSmooth) \
    F (noise##X##Color,  id "_color",       nm " Color",            Unit::Bip,  -1.f,   1.f,   0.f,  0.f,  FMod) \
    C (noise##X##Filter, id "_filter",      nm " Filter",           filterTypes, 0, FNone) \
    F (noise##X##Cutoff, id "_cutoff",      nm " Cutoff",           Unit::Hz,   20.f, 20000.f, 20000.f, 1000.f, FMod) \
    F (noise##X##Reso,   id "_resonance",   nm " Resonance",        Unit::Pct,   0.f,   1.f,   0.f,  0.f,  FMod) \
    F (noise##X##Drive,  id "_drive",       nm " Drive",            Unit::Pct,   0.f,   1.f,   0.f,  0.f,  FMod) \
    F (noise##X##Attack, id "_attack",      nm " Attack",           Unit::Ms,    0.f, 2500.f,  0.5f, 20.f, FMod) \
    F (noise##X##Decay,  id "_decay",       nm " Decay",            Unit::Ms,    1.f, 2500.f, 60.f,  80.f, FMod) \
    F (noise##X##Length, id "_length",      nm " Length",           Unit::Ms,    1.f, 2500.f, 150.f, 150.f, FMod) \
    F (noise##X##Mix,    id "_mix",         nm " Mix",              Unit::Pct,   0.f,   1.f,   1.f,  0.f,  FMod) \
    F (noise##X##Width,  id "_width",       nm " Stereo Width",     Unit::Pct,   0.f,   1.f,   0.f,  0.f,  FMod) \
    B (noise##X##Track,  id "_track",       nm " Pitch Tracking",   false, FNone) \
    C (noise##X##Env,    id "_env",         nm " Envelope",         noiseEnvSources, 0, FNone) \
    F (noise##X##ResHz,  id "_res_freq",    nm " Resonator Freq",   Unit::Hz,  100.f, 12000.f, 3000.f, 2000.f, FMod) \
    F (noise##X##ResQ,   id "_res_q",       nm " Resonator Q",      Unit::Pct,   0.f,   1.f,   0.5f, 0.f,  FMod) \
    F (noise##X##ResAmt, id "_res_amount",  nm " Resonator Amount", Unit::Pct,   0.f,   1.f,   0.8f, 0.f,  FMod) \
    F (noise##X##Metal,  id "_metal",       nm " Metallic Character",Unit::Pct,  0.f,   1.f,   0.5f, 0.f,  FMod) \
    F (noise##X##Bits,   id "_bits",        nm " Bit Depth",        Unit::None,  1.f,  16.f,   6.f,  0.f,  FMod) \
    F (noise##X##Srr,    id "_sr_reduce",   nm " Sample Rate Reduce",Unit::Pct,  0.f,   1.f,   0.4f, 0.f,  FMod) \
    F (noise##X##Quant,  id "_quant",       nm " Quantization",     Unit::Pct,   0.f,   1.f,   0.f,  0.f,  FMod) \
    F (noise##X##Grain,  id "_grain",       nm " Grain Amount",     Unit::Pct,   0.f,   1.f,   0.f,  0.f,  FMod) \
    F (noise##X##Seed,   id "_seed",        nm " Seed",             Unit::None,  0.f, 9999.f, 11.f,  0.f,  FNone) \
    C (noise##X##RMode,  id "_random_mode", nm " Random Mode",      randomModes, 1, FNone) \
    F (noise##X##RAmt,   id "_random_amount",nm " Random Amount",   Unit::Pct,   0.f,   1.f,   1.f,  0.f,  FNone) \
    F (noise##X##ToPunch,id "_to_punch",    nm " -> Punch",         Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (noise##X##ToBody, id "_to_body",     nm " -> Body",          Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (noise##X##ToTail, id "_to_tail",     nm " -> Tail",          Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (noise##X##ToSub,  id "_to_sub",      nm " -> Sub",           Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (noise##X##ToTex,  id "_to_texture",  nm " -> Texture Bus",   Unit::Db,  -60.f,  12.f,   0.f,  0.f,  FMod | FSmooth) \
    B (noise##X##ModOn,  id "_mod_on",      nm " Mod Env On",       false, FNone) \
    C (noise##X##ModMode,id "_mod_mode",    nm " Mod Env Mode",     envModes, 0, FNone) \
    F (noise##X##ModA,   id "_mod_attack",  nm " Mod Env Attack",   Unit::Ms,    0.f, 2500.f,  0.f,  20.f, FMod) \
    F (noise##X##ModD,   id "_mod_decay",   nm " Mod Env Decay",    Unit::Ms,    1.f, 2500.f, 40.f,  80.f, FMod) \
    F (noise##X##ModLvl, id "_mod_level",   nm " Mod -> Level",     Unit::Bip,  -1.f,   1.f,   0.f,  0.f,  FMod) \
    F (noise##X##ModCut, id "_mod_cutoff",  nm " Mod -> Cutoff",    Unit::Bip,  -1.f,   1.f,   0.f,  0.f,  FMod) \
    F (noise##X##ModRes, id "_mod_resonance",nm " Mod -> Resonance",Unit::Bip,  -1.f,   1.f,   0.f,  0.f,  FMod) \
    F (noise##X##ModDrv, id "_mod_drive",   nm " Mod -> Drive",     Unit::Bip,  -1.f,   1.f,   0.f,  0.f,  FMod) \
    F (noise##X##ModCol, id "_mod_color",   nm " Mod -> Color",     Unit::Bip,  -1.f,   1.f,   0.f,  0.f,  FMod) \
    F (noise##X##ModWid, id "_mod_width",   nm " Mod -> Width",     Unit::Bip,  -1.f,   1.f,   0.f,  0.f,  FMod) \
    F (noise##X##ModGrn, id "_mod_grain",   nm " Mod -> Grain",     Unit::Bip,  -1.f,   1.f,   0.f,  0.f,  FMod)
    // module 05: one DISTORTION STAGE (16 in a pool; each stage sits at a LOCATION in an ORDER, serial / parallel / multiband)
#define FGE_DIST(F, B, C, n) \
    B (dist##n##On,      "dist" #n "_on",          "Dist " #n " On",              false, FNone) \
    C (dist##n##Loc,     "dist" #n "_location",    "Dist " #n " Location",        distLocations, 0, FNone) \
    F (dist##n##Order,   "dist" #n "_order",       "Dist " #n " Order",           Unit::None,  1.f,  16.f, (float) n, 0.f, FNone) \
    C (dist##n##Path,    "dist" #n "_path",        "Dist " #n " Path",            distPaths, 0, FNone) \
    C (dist##n##Band,    "dist" #n "_band",        "Dist " #n " Band",            distBands, 0, FNone) \
    C (dist##n##Type,    "dist" #n "_type",        "Dist " #n " Type",            distTypes, 0, FNone) \
    C (dist##n##MorphT,  "dist" #n "_morph_type",  "Dist " #n " Morph To",        distTypes, 1, FNone) \
    F (dist##n##Morph,   "dist" #n "_morph",       "Dist " #n " Morph",           Unit::Pct,   0.f,   1.f,  0.f,  0.f,  FMod) \
    F (dist##n##Drive,   "dist" #n "_drive",       "Dist " #n " Drive",           Unit::Pct,   0.f,   1.f,  0.4f, 0.f,  FMod) \
    C (dist##n##DCurve,  "dist" #n "_drive_curve", "Dist " #n " Drive Curve",     driveCurves, 1, FNone) \
    F (dist##n##In,      "dist" #n "_input",       "Dist " #n " Input Gain",      Unit::Db,  -24.f,  24.f,  0.f,  0.f,  FMod) \
    F (dist##n##Out,     "dist" #n "_output",      "Dist " #n " Output Gain",     Unit::Db,  -24.f,  12.f,  0.f,  0.f,  FMod) \
    F (dist##n##Mix,     "dist" #n "_mix",         "Dist " #n " Mix",             Unit::Pct,   0.f,   1.f,  1.f,  0.f,  FMod) \
    B (dist##n##Auto,    "dist" #n "_auto_gain",   "Dist " #n " Auto Gain",       true, FNone) \
    F (dist##n##Bias,    "dist" #n "_bias",        "Dist " #n " Bias",            Unit::Bip,  -1.f,   1.f,  0.f,  0.f,  FMod) \
    F (dist##n##Sym,     "dist" #n "_symmetry",    "Dist " #n " Symmetry",        Unit::Bip,  -1.f,   1.f,  0.f,  0.f,  FMod) \
    F (dist##n##Thr,     "dist" #n "_threshold",   "Dist " #n " Threshold",       Unit::Pct,   0.f,   1.f,  0.8f, 0.f,  FMod) \
    F (dist##n##Shape,   "dist" #n "_shape",       "Dist " #n " Knee / Hardness / Warmth / Amount", Unit::Pct, 0.f, 1.f, 0.5f, 0.f, FMod) \
    F (dist##n##Ceil,    "dist" #n "_ceiling",     "Dist " #n " Ceiling",         Unit::Db,  -24.f,   0.f, -3.f,  0.f,  FMod) \
    F (dist##n##Even,    "dist" #n "_even",        "Dist " #n " Even Harmonics",  Unit::Pct,   0.f,   1.f,  0.4f, 0.f,  FMod) \
    F (dist##n##Odd,     "dist" #n "_odd",         "Dist " #n " Odd Harmonics",   Unit::Pct,   0.f,   1.f,  0.8f, 0.f,  FMod) \
    F (dist##n##Stages,  "dist" #n "_fold_stages", "Dist " #n " Fold Stages",     Unit::None,  1.f,   8.f,  3.f,  0.f,  FMod) \
    F (dist##n##NegDrv,  "dist" #n "_neg_drive",   "Dist " #n " Negative Drive",  Unit::Pct,   0.f,   1.f,  0.7f, 0.f,  FMod) \
    F (dist##n##NegThr,  "dist" #n "_neg_threshold","Dist " #n " Negative Threshold",Unit::Pct, 0.f,   1.f,  0.5f, 0.f,  FMod) \
    F (dist##n##Bits,    "dist" #n "_bits",        "Dist " #n " Bit Depth",       Unit::None,  1.f,  24.f,  8.f,  0.f,  FMod) \
    F (dist##n##Srr,     "dist" #n "_sr_reduce",   "Dist " #n " Sample Rate Reduction", Unit::Pct, 0.01f, 1.f, 0.3f, 0.f, FMod) \
    C (dist##n##Dither,  "dist" #n "_dither",      "Dist " #n " Dither",          ditherModes, 0, FNone) \
    C (dist##n##Rect,    "dist" #n "_rect_mode",   "Dist " #n " Rectifier Mode",  rectModes, 1, FNone) \
    C (dist##n##Interp,  "dist" #n "_curve_interp","Dist " #n " Curve Interpolation", curveInterps, 1, FNone) \
    C (dist##n##Os,      "dist" #n "_oversampling","Dist " #n " Oversampling",    stageOversampling, 0, FNone) \
    B (dist##n##PreOn,   "dist" #n "_pre_on",      "Dist " #n " Pre Filter On",   false, FNone) \
    C (dist##n##PreType, "dist" #n "_pre_type",    "Dist " #n " Pre Filter Type", distFilterTypes, 1, FNone) \
    F (dist##n##PreCut,  "dist" #n "_pre_cutoff",  "Dist " #n " Pre Cutoff",      Unit::Hz,   20.f, 20000.f, 120.f, 1000.f, FMod) \
    F (dist##n##PreRes,  "dist" #n "_pre_resonance","Dist " #n " Pre Resonance",  Unit::Pct,   0.f,   1.f,  0.f,  0.f,  FMod) \
    F (dist##n##PreDrv,  "dist" #n "_pre_drive",   "Dist " #n " Pre Filter Drive",Unit::Pct,   0.f,   1.f,  0.f,  0.f,  FMod) \
    C (dist##n##PreSlope,"dist" #n "_pre_slope",   "Dist " #n " Pre Slope",       filterSlopes, 1, FNone) \
    B (dist##n##PostOn,  "dist" #n "_post_on",     "Dist " #n " Post Filter On",  false, FNone) \
    C (dist##n##PostType,"dist" #n "_post_type",   "Dist " #n " Post Filter Type",distFilterTypes, 0, FNone) \
    F (dist##n##PostCut, "dist" #n "_post_cutoff", "Dist " #n " Post Cutoff",     Unit::Hz,   20.f, 20000.f, 8000.f, 1000.f, FMod) \
    F (dist##n##PostRes, "dist" #n "_post_resonance","Dist " #n " Post Resonance",Unit::Pct,   0.f,   1.f,  0.f,  0.f,  FMod) \
    F (dist##n##PostDrv, "dist" #n "_post_drive",  "Dist " #n " Post Filter Drive",Unit::Pct,  0.f,   1.f,  0.f,  0.f,  FMod) \
    C (dist##n##PostSlope,"dist" #n "_post_slope", "Dist " #n " Post Slope",      filterSlopes, 1, FNone) \
    F (dist##n##PostMix, "dist" #n "_post_mix",    "Dist " #n " Post Filter Mix", Unit::Pct,   0.f,   1.f,  1.f,  0.f,  FMod) \
    C (dist##n##ModSrc,  "dist" #n "_mod_source",  "Dist " #n " Mod Source",      distModSources, 0, FNone) \
    F (dist##n##ModA,    "dist" #n "_mod_attack",  "Dist " #n " Mod Attack",      Unit::Ms,    0.f, 2500.f, 20.f, 50.f,  FMod) \
    F (dist##n##ModD,    "dist" #n "_mod_decay",   "Dist " #n " Mod Decay",       Unit::Ms,    1.f, 2500.f, 200.f, 200.f, FMod) \
    F (dist##n##MDrive,  "dist" #n "_mod_drive",   "Dist " #n " Mod -> Drive",    Unit::Bip,  -1.f,   1.f,  0.f,  0.f,  FMod) \
    F (dist##n##MBias,   "dist" #n "_mod_bias",    "Dist " #n " Mod -> Bias",     Unit::Bip,  -1.f,   1.f,  0.f,  0.f,  FMod) \
    F (dist##n##MThr,    "dist" #n "_mod_threshold","Dist " #n " Mod -> Threshold",Unit::Bip, -1.f,   1.f,  0.f,  0.f,  FMod) \
    F (dist##n##MFold,   "dist" #n "_mod_fold",    "Dist " #n " Mod -> Fold / Shape",Unit::Bip,-1.f,  1.f,  0.f,  0.f,  FMod) \
    F (dist##n##MMix,    "dist" #n "_mod_mix",     "Dist " #n " Mod -> Mix",      Unit::Bip,  -1.f,   1.f,  0.f,  0.f,  FMod) \
    F (dist##n##MOut,    "dist" #n "_mod_output",  "Dist " #n " Mod -> Output",   Unit::Bip,  -1.f,   1.f,  0.f,  0.f,  FMod) \
    F (dist##n##MCut,    "dist" #n "_mod_cutoff",  "Dist " #n " Mod -> Post Cutoff",Unit::Bip, -1.f,  1.f,  0.f,  0.f,  FMod)
    // module 04: TRANSIENT engine (click generator + attack gain + transient shaper)
#define FGE_TRANSIENT(F, B, C) \
    B (trOn,        "tr_on",           "Transient On",          false, FNone) \
    C (trSource,    "tr_source",       "Transient Source",      trSources, 0, FNone) \
    F (trClickLvl,  "tr_click_level",  "Click Level",           Unit::Db,  -60.f,  12.f,  -6.f,   0.f,  FMod | FSmooth) \
    F (trClickLen,  "tr_click_length", "Click Length",          Unit::Ms,   0.1f, 100.f,   8.f,  10.f,  FMod) \
    F (trClickHz,   "tr_click_pitch",  "Click Pitch",           Unit::Hz,   20.f, 20000.f, 1800.f, 1500.f, FMod) \
    F (trClickTone, "tr_click_tone",   "Click Tone",            Unit::Pct,   0.f,   1.f,   0.6f,  0.f,  FMod) \
    F (trClickDrv,  "tr_click_drive",  "Click Drive",           Unit::Pct,   0.f,   1.f,   0.2f,  0.f,  FMod) \
    F (trClickMix,  "tr_click_mix",    "Click Mix",             Unit::Pct,   0.f,   1.f,   1.f,   0.f,  FMod) \
    F (trWidth,     "tr_width",        "Transient Width",       Unit::Pct,   0.f,   1.f,   0.f,   0.f,  FMod) \
    F (trPosition,  "tr_position",     "Transient Position",    Unit::Ms,    0.f, 250.f,   0.f,  30.f,  FMod) \
    C (trSync,      "tr_sync",         "Transient Sync To",     trSyncs, 0, FNone) \
    C (trOutput,    "tr_output",       "Transient Output",      busOutputs, 0, FNone) \
    F (trAtkLevel,  "tr_attack_level", "Attack Level",          Unit::Db,  -24.f,  24.f,   0.f,   0.f,  FMod) \
    F (trAtkTime,   "tr_attack_time",  "Attack Time",           Unit::Ms,    0.f,  50.f,   2.f,   5.f,  FMod) \
    F (trDecTime,   "tr_decay_time",   "Attack Decay Time",     Unit::Ms,    1.f, 200.f,  30.f,  40.f,  FMod) \
    F (trLow,       "tr_low",          "Low Transient",         Unit::Db,  -24.f,  24.f,   0.f,   0.f,  FMod) \
    F (trHigh,      "tr_high",         "High Transient",        Unit::Db,  -24.f,  24.f,   0.f,   0.f,  FMod) \
    B (shOn,        "shaper_on",       "Transient Shaper On",   false, FNone) \
    F (shAttack,    "shaper_attack",   "Shaper Attack",         Unit::Bip,  -1.f,   1.f,   0.f,   0.f,  FMod) \
    F (shSustain,   "shaper_sustain",  "Shaper Sustain",        Unit::Bip,  -1.f,   1.f,   0.f,   0.f,  FMod) \
    F (shSens,      "shaper_sensitivity","Shaper Sensitivity",  Unit::Pct,   0.f,   1.f,   0.5f,  0.f,  FMod) \
    F (shTime,      "shaper_time",     "Shaper Time",           Unit::Ms,    1.f, 500.f,  50.f,  60.f,  FMod) \
    F (shMix,       "shaper_mix",      "Shaper Mix",            Unit::Pct,   0.f,   1.f,   1.f,   0.f,  FMod)
    // module 04: LAYER (8 now, the engine is sized for 16)
#define FGE_LAYER(F, B, C, n) \
    B (layer##n##On,      "layer" #n "_on",          "Layer " #n " On",           false, FNone) \
    C (layer##n##Src,     "layer" #n "_source",      "Layer " #n " Source",       layerSources, 0, FNone) \
    C (layer##n##Group,   "layer" #n "_group",       "Layer " #n " Group",        layerGroups, 4, FNone) \
    F (layer##n##Level,   "layer" #n "_level",       "Layer " #n " Level",        Unit::Db,  -60.f,  12.f,  -6.f,   0.f,  FMod | FSmooth) \
    F (layer##n##Pan,     "layer" #n "_pan",         "Layer " #n " Pan",          Unit::Bip,  -1.f,   1.f,   0.f,   0.f,  FMod) \
    F (layer##n##Pitch,   "layer" #n "_pitch",       "Layer " #n " Pitch",        Unit::Semi,-48.f,  48.f,   0.f,   0.f,  FMod) \
    F (layer##n##Fine,    "layer" #n "_fine",        "Layer " #n " Fine",         Unit::Cents,-100.f,100.f,  0.f,   0.f,  FMod) \
    F (layer##n##Len,     "layer" #n "_length",      "Layer " #n " Length",       Unit::Ms,    1.f, 2500.f, 300.f, 300.f, FMod) \
    F (layer##n##Offset,  "layer" #n "_offset",      "Layer " #n " Start Offset", Unit::Ms,    0.f, 2500.f,  0.f,  100.f, FMod) \
    F (layer##n##Phase,   "layer" #n "_phase",       "Layer " #n " Phase",        Unit::Deg,   0.f, 360.f,   0.f,   0.f,  FNone) \
    B (layer##n##Inv,     "layer" #n "_invert",      "Layer " #n " Phase Invert", false, FNone) \
    F (layer##n##Width,   "layer" #n "_width",       "Layer " #n " Stereo Width", Unit::Pct,   0.f,   1.f,   0.f,   0.f,  FMod) \
    B (layer##n##FltOn,   "layer" #n "_filter_on",   "Layer " #n " Filter On",    false, FNone) \
    C (layer##n##FltType, "layer" #n "_filter_type", "Layer " #n " Filter Type",  filterTypes, 0, FNone) \
    F (layer##n##Cutoff,  "layer" #n "_cutoff",      "Layer " #n " Cutoff",       Unit::Hz,   20.f, 20000.f, 5000.f, 1000.f, FMod) \
    F (layer##n##Reso,    "layer" #n "_resonance",   "Layer " #n " Resonance",    Unit::Pct,   0.f,   1.f,   0.2f,  0.f,  FMod) \
    F (layer##n##Drive,   "layer" #n "_drive",       "Layer " #n " Drive",        Unit::Pct,   0.f,   1.f,   0.f,   0.f,  FMod) \
    C (layer##n##EnvMode, "layer" #n "_env_mode",    "Layer " #n " Env Mode",     layerEnvModes, 1, FNone) \
    F (layer##n##Atk,     "layer" #n "_attack",      "Layer " #n " Attack",       Unit::Ms,    0.f, 2500.f,  1.f,  20.f,  FMod) \
    F (layer##n##Dec,     "layer" #n "_decay",       "Layer " #n " Decay",        Unit::Ms,    1.f, 2500.f, 120.f, 150.f, FMod) \
    F (layer##n##Sus,     "layer" #n "_sustain",     "Layer " #n " Sustain",      Unit::Pct,   0.f,   1.f,   0.3f,  0.f,  FMod) \
    F (layer##n##Rel,     "layer" #n "_release",     "Layer " #n " Release",      Unit::Ms,    0.f, 2500.f,  30.f,  60.f,  FMod) \
    F (layer##n##Delay,   "layer" #n "_env_delay",   "Layer " #n " Env Delay",    Unit::Ms,    0.f, 2500.f,  0.f, 100.f,  FMod) \
    F (layer##n##EnvAmt,  "layer" #n "_env_amount",  "Layer " #n " Env Amount",   Unit::Pct,   0.f,   1.f,   1.f,   0.f,  FMod) \
    F (layer##n##TScale,  "layer" #n "_time_scale",  "Layer " #n " Time Scale",   Unit::Pct,  0.1f,   4.f,   1.f,   1.f,  FMod) \
    B (layer##n##Mute,    "layer" #n "_mute",        "Layer " #n " Mute",         false, FNone) \
    B (layer##n##Solo,    "layer" #n "_solo",        "Layer " #n " Solo",         false, FNone) \
    C (layer##n##Route,   "layer" #n "_route_mode",  "Layer " #n " Routing",      routeModes, 0, FNone) \
    B (layer##n##PreFdr,  "layer" #n "_send_pre",    "Layer " #n " Pre-Fader Sends", false, FNone) \
    F (layer##n##SPunch,  "layer" #n "_send_punch",  "Layer " #n " Send Punch",   Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (layer##n##SBody,   "layer" #n "_send_body",   "Layer " #n " Send Body",    Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (layer##n##STail,   "layer" #n "_send_tail",   "Layer " #n " Send Tail",    Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (layer##n##SSub,    "layer" #n "_send_sub",    "Layer " #n " Send Sub",     Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (layer##n##STex,    "layer" #n "_send_texture","Layer " #n " Send Texture", Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (layer##n##SCust,   "layer" #n "_send_custom", "Layer " #n " Send Custom",  Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (layer##n##SMaster, "layer" #n "_send_master", "Layer " #n " Send Master",  Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    C (layer##n##LinkSrc, "layer" #n "_link_source", "Layer " #n " Link To",      layerLinks, 0, FNone) \
    B (layer##n##LinkLvl, "layer" #n "_link_level",  "Layer " #n " Link Level",   false, FNone) \
    B (layer##n##LinkPit, "layer" #n "_link_pitch",  "Layer " #n " Link Pitch",   false, FNone) \
    B (layer##n##LinkLen, "layer" #n "_link_length", "Layer " #n " Link Length",  false, FNone) \
    B (layer##n##LinkEnv, "layer" #n "_link_env",    "Layer " #n " Link Envelope",false, FNone)
    // module 04: LAYER GROUP control
#define FGE_GROUP(F, B, C, X, id, nm, def) \
    B (grp##X##On,    "group_" id "_on",     nm " Group On",     true, FNone) \
    F (grp##X##Level, "group_" id "_level",  nm " Group Level",  Unit::Db,  -60.f,  12.f,   0.f,   0.f,  FMod | FSmooth) \
    F (grp##X##Pan,   "group_" id "_pan",    nm " Group Pan",    Unit::Bip,  -1.f,   1.f,   0.f,   0.f,  FMod) \
    F (grp##X##Width, "group_" id "_width",  nm " Group Width",  Unit::Pct,   0.f,   1.f,   1.f,   0.f,  FMod) \
    B (grp##X##Mute,  "group_" id "_mute",   nm " Group Mute",   false, FNone) \
    B (grp##X##Solo,  "group_" id "_solo",   nm " Group Solo",   false, FNone) \
    C (grp##X##Out,   "group_" id "_output", nm " Group Output", busOutputs, def, FNone)
    // FM MATRIX: modulator a -> carrier b (frequency modulation, the diagonal is each oscillator's FEEDBACK)
#define FGE_FM(F, a, b) \
    F (fm##a##to##b,     "fm_" #a "_to_" #b,     "FM Osc " #a " -> Osc " #b, Unit::Pct,    0.f,   1.f,   0.f,   0.f,   FMod)

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
