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
        // module 06
        const juce::StringArray& m06Locations();
        const juce::StringArray& m06Positions();
        const juce::StringArray& eqTypes(); const juce::StringArray& eqStereo(); const juce::StringArray& deqModes(); const juce::StringArray& deqSources();
        const juce::StringArray& deqDetectors(); const juce::StringArray& mbCrossovers(); const juce::StringArray& mbListen(); const juce::StringArray& mbDynModes();
        const juce::StringArray& excModes(); const juce::StringArray& dynLocations(); const juce::StringArray& cmpTypes(); const juce::StringArray& cmpDetections();
        const juce::StringArray& autoReleases(); const juce::StringArray& scSources(); const juce::StringArray& tsDetections(); const juce::StringArray& tsModes();
        const juce::StringArray& clipTypes(); const juce::StringArray& limModes(); const juce::StringArray& limOversampling(); const juce::StringArray& autoGainModes();
        const juce::StringArray& smpModes(); const juce::StringArray& smpLinks();
        const juce::StringArray& spcModes(); const juce::StringArray& spcPolarity(); const juce::StringArray& spcHaasCh(); const juce::StringArray& outModes(); const juce::StringArray& noteCutModes(); const juce::StringArray& lenSyncs(); const juce::StringArray& tokModes(); const juce::StringArray& punchModes(); const juce::StringArray& tokNoteModes(); const juce::StringArray& tailStartModes(); const juce::StringArray& roomInputs();
        const juce::StringArray& folSources(); const juce::StringArray& folDetects(); const juce::StringArray& lfoInterps();
        const juce::StringArray& lfoWaves(); const juce::StringArray& modDivisions(); const juce::StringArray& modRetrigs(); const juce::StringArray& lfoModes();
        const juce::StringArray& modPolarities(); const juce::StringArray& seqDirections(); const juce::StringArray& seqSkips(); const juce::StringArray& rndModes();
        const juce::StringArray& dynOrders(); const juce::StringArray& mbLinks(); const juce::StringArray& excHarm(); const juce::StringArray& toneOrders(); const juce::StringArray& stwLocations();
        const juce::StringArray& m06FilterTypes();
        const juce::StringArray& stereoModes();
        const juce::StringArray& resModes();
        const juce::StringArray& resSources();
        const juce::StringArray& scrModes();
        const juce::StringArray& scrWaves();
        const juce::StringArray& scrOscSel();
        const juce::StringArray& scrOutputs();
        const juce::StringArray& fbFilters();
        const juce::StringArray& scrPitchModes();
        const juce::StringArray& scrStartModes();
        const juce::StringArray& quantizeModes();
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
    C (distOffline, "dist_offline_quality","Offline Render Quality",       offlineQuality, 0, FNone) \
    FGE_FLT (F, B, C, 1) FGE_FLT (F, B, C, 2) FGE_FLT (F, B, C, 3) FGE_FLT (F, B, C, 4) FGE_FLT (F, B, C, 5) FGE_FLT (F, B, C, 6) \
    FGE_FLT (F, B, C, 7) FGE_FLT (F, B, C, 8) \
    F (fltXLow,  "filter_xover_low",  "Filter Split Low / Mid",  Unit::Hz,  20.f,  2000.f, 200.f,  200.f, FMod) \
    F (fltXHigh, "filter_xover_high", "Filter Split Mid / High", Unit::Hz, 200.f, 20000.f, 2500.f, 2500.f, FMod) \
    FGE_RES (F, B, C, 1) FGE_RES (F, B, C, 2) \
    FGE_SCREECH (F, B, C) \
    FGE_SCR_OSC (F, B, C, 1) FGE_SCR_OSC (F, B, C, 2) FGE_SCR_OSC (F, B, C, 3) FGE_SCR_OSC (F, B, C, 4) \
    FGE_SCR_FLT (F, B, C, 1) FGE_SCR_FLT (F, B, C, 2) \
    FGE_EQ (F, B, C, 1) FGE_EQ (F, B, C, 2) FGE_EQ (F, B, C, 3) FGE_EQ (F, B, C, 4) FGE_EQ (F, B, C, 5) FGE_EQ (F, B, C, 6) \
    FGE_EQ (F, B, C, 7) FGE_EQ (F, B, C, 8) FGE_EQ (F, B, C, 9) FGE_EQ (F, B, C, 10) FGE_EQ (F, B, C, 11) FGE_EQ (F, B, C, 12) \
    FGE_EQ (F, B, C, 13) FGE_EQ (F, B, C, 14) FGE_EQ (F, B, C, 15) FGE_EQ (F, B, C, 16) \
    FGE_DEQ (F, B, C, 1) FGE_DEQ (F, B, C, 2) FGE_DEQ (F, B, C, 3) FGE_DEQ (F, B, C, 4) \
    FGE_MB (F, B, C) FGE_MBB (F, B, C, 1) FGE_MBB (F, B, C, 2) FGE_MBB (F, B, C, 3) FGE_MBB (F, B, C, 4) \
    FGE_SPEC (F, B, C) FGE_STW (F, B, C) \
    FGE_CMP (F, B, C, 1) FGE_CMP (F, B, C, 2) FGE_CMP (F, B, C, 3) FGE_CMP (F, B, C, 4) \
    FGE_TS (F, B, C, 1, 1) FGE_TS (F, B, C, 2, 2) FGE_TS (F, B, C, 3, 3) FGE_TS (F, B, C, 4, 4) FGE_TS (F, B, C, 5, 5) \
    FGE_CLP (F, B, C, 1) FGE_CLP (F, B, C, 2) FGE_LIM (F, B, C, 1) FGE_LIM (F, B, C, 2) FGE_DYNG (F, B, C) \
    /* ================= MODULE 09 - MODULATION: MASTER MACROS (sources only - no DSP reads them), LFO 1-8, STEP SEQ, RANDOM 1-2 ================= */ \
    FGE_MACRO (F, impact, "impact", "IMPACT") FGE_MACRO (F, body, "body", "BODY") FGE_MACRO (F, tail, "tail", "TAIL") FGE_MACRO (F, pitch, "pitch", "PITCH") \
    FGE_MACRO (F, grit, "grit", "GRIT") FGE_MACRO (F, scream, "scream", "SCREAM") FGE_MACRO (F, weight, "weight", "WEIGHT") FGE_MACRO (F, chaos, "chaos", "CHAOS") \
    FGE_MACRO (F, user1, "user1", "USER 1") FGE_MACRO (F, user2, "user2", "USER 2") FGE_MACRO (F, user3, "user3", "USER 3") FGE_MACRO (F, user4, "user4", "USER 4") \
    FGE_LFO (F, B, C, 1) FGE_LFO (F, B, C, 2) FGE_LFO (F, B, C, 3) FGE_LFO (F, B, C, 4) FGE_LFO (F, B, C, 5) FGE_LFO (F, B, C, 6) FGE_LFO (F, B, C, 7) FGE_LFO (F, B, C, 8) \
    FGE_SEQ (F, B, C) FGE_RND (F, B, C, 1) FGE_RND (F, B, C, 2) \
    F (modMovement,  "mod_movement",    "MOVEMENT",                Unit::Pct,   0.f,   2.f,   1.f,   1.f,   FMod) \
    /* module 09 part 2: MIDI CC 1-4, AUDIO FOLLOWER, CUSTOM LFO interpolation, MACRO MORPH */ \
    FGE_CC (F, 1, 1.f) FGE_CC (F, 2, 74.f) FGE_CC (F, 3, 71.f) FGE_CC (F, 4, 11.f) \
    C (folSource,    "fol_source",      "Follower Source",         folSources, 0, FNone) \
    C (folDetect,    "fol_detect",      "Follower Detection",      folDetects, 0, FNone) \
    F (folAttack,    "fol_attack",      "Follower Attack",         Unit::Ms,    0.1f, 200.f,  2.f,  10.f,   FMod) \
    F (folRelease,   "fol_release",     "Follower Release",        Unit::Ms,    1.f, 2000.f, 80.f, 100.f,   FMod) \
    F (folGain,      "fol_gain",        "Follower Input Gain",     Unit::Db,   -24.f,  24.f,  0.f,   0.f,   FMod) \
    F (folThresh,    "fol_threshold",   "Follower Threshold",      Unit::Db,   -80.f,   0.f, -40.f,  0.f,   FMod) \
    F (folRange,     "fol_range",       "Follower Range",          Unit::Db,     3.f,  60.f,  30.f, 20.f,   FMod) \
    F (folSens,      "fol_sensitivity", "Follower Gate",           Unit::Pct,    0.f,   0.5f, 0.02f, 0.f,   FMod) \
    F (folSmooth,    "fol_smooth",      "Follower Smoothing",      Unit::Ms,     0.f, 200.f,   0.f,  20.f,   FMod) \
    C (lfo1Interp,   "lfo1_interp",     "LFO 1 Custom Interpolation", lfoInterps, 0, FNone) C (lfo2Interp, "lfo2_interp", "LFO 2 Custom Interpolation", lfoInterps, 0, FNone) \
    C (lfo3Interp,   "lfo3_interp",     "LFO 3 Custom Interpolation", lfoInterps, 0, FNone) C (lfo4Interp, "lfo4_interp", "LFO 4 Custom Interpolation", lfoInterps, 0, FNone) \
    C (lfo5Interp,   "lfo5_interp",     "LFO 5 Custom Interpolation", lfoInterps, 0, FNone) C (lfo6Interp, "lfo6_interp", "LFO 6 Custom Interpolation", lfoInterps, 0, FNone) \
    C (lfo7Interp,   "lfo7_interp",     "LFO 7 Custom Interpolation", lfoInterps, 0, FNone) C (lfo8Interp, "lfo8_interp", "LFO 8 Custom Interpolation", lfoInterps, 0, FNone) \
    B (macroMorphOn, "macro_morph_on",  "Macro Morph On",          false, FGlobal) \
    F (macroMorph,   "macro_morph",     "Macro Morph",             Unit::Pct,    0.f,   1.f,   0.f,   0.f,   FGlobal) \
    /* ================= MODULE 10 - ADVANCED SAMPLE ENGINE: 8 sample slots (the audio itself is state data, SAMPLES) ================= */ \
    FGE_SMP (F, B, C, 1) FGE_SMP (F, B, C, 2) FGE_SMP (F, B, C, 3) FGE_SMP (F, B, C, 4) FGE_SMP (F, B, C, 5) FGE_SMP (F, B, C, 6) FGE_SMP (F, B, C, 7) FGE_SMP (F, B, C, 8) \
    /* ================= MODULE 11 - STEREO & SPATIAL ENGINE: one unit per group (PUNCH ... CUSTOM) + MASTER ================= */ \
    FGE_SPC (F, B, C, Punch, "punch", "Punch", 1.f, true) FGE_SPC (F, B, C, Body, "body", "Body", 1.f, false) FGE_SPC (F, B, C, Tail, "tail", "Tail", 1.2f, false) \
    FGE_SPC (F, B, C, Sub, "sub", "Sub", 0.f, true) FGE_SPC (F, B, C, Tex, "texture", "Texture", 1.3f, false) FGE_SPC (F, B, C, Cust, "custom", "Custom", 1.f, false) \
    FGE_SPC (F, B, C, Master, "master", "Master", 1.f, false) \
    B (spcSubLock,   "spc_sub_mono_lock", "Sub Mono Lock",         true, FNone) \
    /* ================= MODULE 12 - MASTER OUTPUT (the level = core_master) ================= */ \
    B (masterMute,     "master_mute",          "Master Mute",          false, FGlobal) \
    B (masterBypass,   "master_bypass",        "Master Bypass",        false, FGlobal) \
    C (masterOutMode,  "master_output_mode",   "Output Mode",          outModes, 0, FGlobal) \
    B (masterSafeClip, "master_safety_clip",   "Safety Clip",          false, FGlobal) \
    B (masterSafeLim,  "master_safety_limiter","Safety Limiter",       false, FGlobal) \
    /* ================= MODULE 14 (part) - the kick ends where the MIDI note ends ================= */ \
    C (coreNoteCut,    "core_note_cut",        "Kick End",             noteCutModes, 0, FGlobal) \
    /* ================= MODULE 14 - KICK LENGTH ENGINE ================= */ \
    C (lenSync,        "len_sync",             "Length Sync",          lenSyncs, 0, FGlobal) \
    F (lenFollowPunch, "len_follow_punch",     "Punch Follows Length", Unit::Pct, 0.f, 1.f, 0.f,  0.f, FNone) \
    F (lenFollowBody,  "len_follow_body",      "Body Follows Length",  Unit::Pct, 0.f, 1.f, 0.5f, 0.f, FNone) \
    F (lenFollowTail,  "len_follow_tail",      "Tail Follows Length",  Unit::Pct, 0.f, 1.f, 1.f,  0.f, FNone) \
    F (lenFollowSub,   "len_follow_sub",       "Sub Follows Length",   Unit::Pct, 0.f, 1.f, 1.f,  0.f, FNone) \
    /* ================= owner 2026-11-09: the TOK (pitch START) at a FIXED pitch while the tail lands on the played note ================= */ \
    C (peStartMode,    "pe_start_mode",        "Tok Pitch Mode",       tokModes, 0, FNone) \
    /* ================= owner 2026-11-09: PUNCH / TOK -> TAIL (two parts): PUNCH = TRANSIENT + TOK CORE before the tail ================= */ \
    C (punchMode,      "punch_mode",           "Punch Mode",           punchModes, 0, FNone) \
    C (punchNoteMode,  "punch_note_mode",      "Tok Note Mode",        tokNoteModes, 0, FNone) \
    F (punchOverlap,   "punch_overlap",        "Tok To Tail Overlap",  Unit::Ms,  0.f,  60.f,  20.f,  15.f, FNone) \
    F (punchTrLevel,   "punch_transient",      "Tok Transient",        Unit::Pct, 0.f,   1.f,   0.f,   0.f, FMod) \
    F (punchTrLength,  "punch_transient_len",  "Tok Transient Length", Unit::Ms,  0.2f, 10.f,   1.5f,  2.f, FNone) \
    F (punchTrTone,    "punch_transient_tone", "Tok Transient Tone",   Unit::Hz,  500.f, 16000.f, 5000.f, 4000.f, FMod) \
    /* ================= owner 2026-11-10: TOK LENGTH per style, TAIL START MODE, SUB START, the tok's own decay / pitch bend / OFFSET ================= */ \
    C (tailStartMode,  "tail_start_mode",      "Tail Start Mode",      tailStartModes, 0, FNone) \
    F (tailStart,      "tail_start",           "Tail Start",           Unit::Ms,  0.f, 300.f, 100.f, 80.f, FNone) \
    F (punchTokDecay,  "punch_tok_decay",      "Tok Decay",            Unit::Db, -24.f,  0.f,   0.f,   0.f, FMod) \
    F (punchTokBend,   "punch_tok_bend",       "Tok Pitch Bend",       Unit::Semi, -24.f, 24.f, 0.f,   0.f, FMod) \
    F (punchNoteOffset,"punch_note_offset",    "Tok Note Offset",      Unit::Semi, -24.f, 36.f, 12.f,  0.f, FNone) \
    /* ================= owner 2026-11-12: the EARLY kick - a room on the tail BEFORE its distortion ================= */ \
    F (tailRoom,       "tail_room",            "Tail Room",            Unit::Pct, 0.f, 1.f, 0.f, 0.f, FMod) \
    F (tailRoomDecay,  "tail_room_decay",      "Tail Room Decay",      Unit::Ms,  80.f, 1500.f, 450.f, 400.f, FNone) \
    F (tailRoomTone,   "tail_room_tone",       "Tail Room Tone",       Unit::Hz,  500.f, 16000.f, 5000.f, 3000.f, FNone) \
    /* owner 2026-11-12: the euphoric CRACK - the room also on the tok (before its distortion) + a pre-delay */ \
    C (tailRoomInput,  "tail_room_input",      "Tail Room Input",      roomInputs, 0, FNone) \
    F (tailRoomDelay,  "tail_room_delay",      "Tail Room Delay",      Unit::Ms,  0.f, 300.f, 0.f, 80.f, FNone)
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
    // module 06: FILTER ENGINE instance (8)
#define FGE_FLT(F, B, C, n) \
    B (flt##n##On,      "flt" #n "_on",          "Filter " #n " On",            false, FNone) \
    C (flt##n##Loc,     "flt" #n "_location",    "Filter " #n " Location",      m06Locations, 3, FNone) \
    C (flt##n##Pos,     "flt" #n "_position",    "Filter " #n " Position",      m06Positions, 0, FNone) \
    F (flt##n##Order,   "flt" #n "_order",       "Filter " #n " Order",         Unit::None,  1.f,   8.f, (float) n, 0.f, FNone) \
    C (flt##n##Path,    "flt" #n "_path",        "Filter " #n " Path",          distPaths, 0, FNone) \
    C (flt##n##Band,    "flt" #n "_band",        "Filter " #n " Split Band",    distBands, 0, FNone) \
    C (flt##n##Type,    "flt" #n "_type",        "Filter " #n " Type",          m06FilterTypes, 0, FNone) \
    F (flt##n##Cut,     "flt" #n "_cutoff",      "Filter " #n " Cutoff",        Unit::Hz,   20.f, 20000.f, 2000.f, 1000.f, FMod) \
    F (flt##n##Res,     "flt" #n "_resonance",   "Filter " #n " Resonance",     Unit::Pct,   0.f,   1.f,  0.2f,  0.f,  FMod) \
    F (flt##n##Drive,   "flt" #n "_drive",       "Filter " #n " Drive",         Unit::Pct,   0.f,   1.f,  0.f,   0.f,  FMod) \
    F (flt##n##Mix,     "flt" #n "_mix",         "Filter " #n " Mix",           Unit::Pct,   0.f,   1.f,  1.f,   0.f,  FMod) \
    C (flt##n##Slope,   "flt" #n "_slope",       "Filter " #n " Slope",         filterSlopes, 1, FNone) \
    F (flt##n##Key,     "flt" #n "_key_track",   "Filter " #n " Key Track",     Unit::Pct,   0.f,   2.f,  0.f,   0.f,  FMod) \
    F (flt##n##EnvAmt,  "flt" #n "_env_amount",  "Filter " #n " Env Amount",    Unit::Bip,  -1.f,   1.f,  0.f,   0.f,  FMod) \
    F (flt##n##Vel,     "flt" #n "_velocity",    "Filter " #n " Velocity",      Unit::Bip,  -1.f,   1.f,  0.f,   0.f,  FMod) \
    C (flt##n##Stereo,  "flt" #n "_stereo_mode", "Filter " #n " Stereo Mode",   stereoModes, 0, FNone) \
    F (flt##n##SvfMode, "flt" #n "_svf_mode",    "Filter " #n " SVF Mode (LP-BP-HP)", Unit::Pct, 0.f, 1.f, 0.f, 0.f, FMod) \
    F (flt##n##Vowel,   "flt" #n "_vowel",       "Filter " #n " Vowel (A-E-I-O-U)",   Unit::None, 0.f, 4.f, 0.f, 0.f, FMod) \
    F (flt##n##EnvA,    "flt" #n "_env_attack",  "Filter " #n " Env Attack",    Unit::Ms,    0.f, 2000.f,  1.f,  20.f,  FMod) \
    F (flt##n##EnvD,    "flt" #n "_env_decay",   "Filter " #n " Env Decay",     Unit::Ms,    0.f, 5000.f, 150.f, 200.f, FMod) \
    F (flt##n##EnvS,    "flt" #n "_env_sustain", "Filter " #n " Env Sustain",   Unit::Pct,   0.f,   1.f,  0.f,   0.f,  FMod) \
    F (flt##n##EnvR,    "flt" #n "_env_release", "Filter " #n " Env Release",   Unit::Ms,    0.f, 5000.f, 100.f, 200.f, FMod) \
    C (flt##n##EnvCurve,"flt" #n "_env_curve",   "Filter " #n " Env Curve",     driveCurves, 1, FNone) \
    B (flt##n##EnvInv,  "flt" #n "_env_invert",  "Filter " #n " Env Invert",    false, FNone) \
    B (flt##n##EnvSync, "flt" #n "_env_sync",    "Filter " #n " Env Sync To Kick Length", false, FNone) \
    F (flt##n##EnvRes,  "flt" #n "_env_to_reso", "Filter " #n " Env -> Resonance", Unit::Bip, -1.f,  1.f,  0.f,   0.f,  FMod) \
    F (flt##n##EnvDrv,  "flt" #n "_env_to_drive","Filter " #n " Env -> Drive",  Unit::Bip,  -1.f,   1.f,  0.f,   0.f,  FMod)
    // module 06: RESONANCE ENGINE (2 resonators)
#define FGE_RES(F, B, C, n) \
    B (res##n##On,      "res" #n "_on",          "Resonator " #n " On",         false, FNone) \
    C (res##n##Loc,     "res" #n "_location",    "Resonator " #n " Location",   m06Locations, 3, FNone) \
    C (res##n##Pos,     "res" #n "_position",    "Resonator " #n " Position",   m06Positions, 0, FNone) \
    C (res##n##Mode,    "res" #n "_mode",        "Resonator " #n " Mode",       resModes, 0, FNone) \
    C (res##n##Src,     "res" #n "_source",      "Resonator " #n " Excitation Source", resSources, 0, FNone) \
    F (res##n##Freq,    "res" #n "_freq",        "Resonator " #n " Frequency",  Unit::Hz,   20.f, 20000.f, 1800.f, 1000.f, FMod) \
    F (res##n##Q,       "res" #n "_q",           "Resonator " #n " Q",          Unit::None, 0.1f, 100.f,  12.f,  8.f,  FMod) \
    F (res##n##Decay,   "res" #n "_decay",       "Resonator " #n " Decay",      Unit::Ms,    1.f, 5000.f, 300.f, 300.f, FMod) \
    F (res##n##Amount,  "res" #n "_amount",      "Resonator " #n " Amount",     Unit::Pct,   0.f,   1.f,  0.5f,  0.f,  FMod) \
    F (res##n##Damp,    "res" #n "_damping",     "Resonator " #n " Damping",    Unit::Pct,   0.f,   1.f,  0.3f,  0.f,  FMod) \
    F (res##n##Exc,     "res" #n "_excitation",  "Resonator " #n " Excitation", Unit::Pct,   0.f,   1.f,  0.5f,  0.f,  FMod) \
    F (res##n##Fb,      "res" #n "_feedback",    "Resonator " #n " Feedback",   Unit::Pct,   0.f, 0.95f,  0.f,   0.f,  FMod) \
    F (res##n##Track,   "res" #n "_pitch_track", "Resonator " #n " Pitch Track",Unit::Pct,   0.f,   2.f,  0.f,   0.f,  FMod) \
    F (res##n##Mix,     "res" #n "_mix",         "Resonator " #n " Mix",        Unit::Pct,   0.f,   1.f,  1.f,   0.f,  FMod) \
    F (res##n##PAmt,    "res" #n "_penv_amount", "Resonator " #n " Pitch Env Amount", Unit::Pct, 0.f, 1.f, 0.f,  0.f,  FMod) \
    F (res##n##PStart,  "res" #n "_penv_start",  "Resonator " #n " Pitch Env Start", Unit::Hz, 20.f, 20000.f, 8000.f, 1000.f, FMod) \
    F (res##n##PEnd,    "res" #n "_penv_end",    "Resonator " #n " Pitch Env End",   Unit::Hz, 20.f, 20000.f, 1800.f, 1000.f, FMod) \
    F (res##n##PA,      "res" #n "_penv_attack", "Resonator " #n " Pitch Env Attack", Unit::Ms, 0.f, 2000.f, 0.f,  20.f,  FMod) \
    F (res##n##PD,      "res" #n "_penv_decay",  "Resonator " #n " Pitch Env Decay",  Unit::Ms, 1.f, 5000.f, 250.f, 250.f, FMod) \
    F (res##n##PS,      "res" #n "_penv_sustain","Resonator " #n " Pitch Env Sustain",Unit::Pct, 0.f, 1.f,  0.f,  0.f,  FMod) \
    F (res##n##PR,      "res" #n "_penv_release","Resonator " #n " Pitch Env Release",Unit::Ms, 0.f, 5000.f, 100.f, 200.f, FMod) \
    C (res##n##PCurve,  "res" #n "_penv_curve",  "Resonator " #n " Pitch Env Curve",  driveCurves, 1, FNone)
    // module 06: SCREECH ENGINE (one generator per kick)
#define FGE_SCREECH(F, B, C) \
    B (scrOn,        "scr_on",            "Screech On",               false, FNone) \
    C (scrMode,      "scr_mode",          "Screech Mode",             scrModes, 0, FNone) \
    F (scrLevel,     "scr_level",         "Screech Level",            Unit::Db,  -60.f,  12.f,  0.f,   0.f,  FMod | FSmooth) \
    C (scrOutput,    "scr_output",        "Screech Output",           scrOutputs, 6, FNone) \
    F (scrWidth,     "scr_width",         "Screech Width",            Unit::Pct,   0.f,   1.f,  0.5f,  0.f,  FMod) \
    C (scrPitchMode, "scr_pitch_mode",    "Screech Pitch Mode",       scrPitchModes, 1, FNone) \
    F (scrBaseHz,    "scr_base_hz",       "Screech Base Frequency",   Unit::Hz,   20.f, 5000.f, 200.f, 300.f, FMod) \
    F (scrFmRatio,   "scr_fm_ratio",      "Screech FM Ratio",         Unit::None, 0.25f, 16.f,  2.f,   2.f,  FMod) \
    F (scrFmIndex,   "scr_fm_index",      "Screech FM Index",         Unit::Pct,   0.f,   1.f,  0.5f,  0.f,  FMod) \
    F (scrFmFb,      "scr_fm_feedback",   "Screech FM Feedback",      Unit::Pct,   0.f,   1.f,  0.2f,  0.f,  FMod) \
    F (scrMetal,     "scr_metal",         "Screech Metal (inharmonic)",Unit::Pct,  0.f,   1.f,  0.5f,  0.f,  FMod) \
    F (scrMetalQ,    "scr_metal_q",       "Screech Metal Resonance",  Unit::Pct,   0.f,   1.f,  0.6f,  0.f,  FMod) \
    F (scrNoiseCol,  "scr_noise_color",   "Screech Noise Color",      Unit::Bip,  -1.f,   1.f,  0.f,   0.f,  FMod) \
    F (scrNoiseQ,    "scr_noise_q",       "Screech Noise Resonance",  Unit::Pct,   0.f,   1.f,  0.85f, 0.f,  FMod) \
    F (scrVowel,     "scr_vowel",         "Screech Formant Vowel",    Unit::None,  0.f,   4.f,  0.f,   0.f,  FMod) \
    F (scrVowelEnd,  "scr_vowel_end",     "Screech Formant Vowel End",Unit::None,  0.f,   4.f,  2.f,   0.f,  FMod) \
    B (scrVowelAuto, "scr_vowel_auto",    "Screech Formant Auto Morph", false, FNone) \
    F (scrFmtWidth,  "scr_formant_width", "Screech Formant Width",    Unit::Pct,   0.f,   1.f,  0.5f,  0.f,  FMod) \
    F (scrFmtReso,   "scr_formant_reso",  "Screech Formant Resonance",Unit::Pct,   0.f,   1.f,  0.6f,  0.f,  FMod) \
    F (scrFbAmt,     "scr_fb_amount",     "Screech Feedback",         Unit::Pct,   0.f, 0.95f,  0.6f,  0.f,  FMod) \
    F (scrFbTone,    "scr_fb_tone",       "Screech Feedback Tone",    Unit::Pct,   0.f,   1.f,  0.5f,  0.f,  FMod) \
    C (scrFbFilter,  "scr_fb_filter",     "Screech Feedback Filter",  fbFilters, 2, FNone) \
    F (scrFbDelay,   "scr_fb_delay",      "Screech Feedback Delay",   Unit::Ms,    0.f, 100.f,  0.f,  10.f,  FMod) \
    F (scrFbDrive,   "scr_fb_drive",      "Screech Feedback Drive",   Unit::Pct,   0.f,   1.f,  0.5f,  0.f,  FMod) \
    F (scrFbMix,     "scr_fb_mix",        "Screech Feedback Mix",     Unit::Pct,   0.f,   1.f,  1.f,   0.f,  FMod) \
    F (scrFbStab,    "scr_fb_stability",  "Screech Feedback Stability",Unit::Pct,  0.f,   1.f,  0.5f,  0.f,  FMod) \
    F (scrMixTonal,  "scr_mix_tonal",     "Screech Custom: Tonal",    Unit::Pct,   0.f,   1.f,  0.5f,  0.f,  FMod) \
    F (scrMixMetal,  "scr_mix_metal",     "Screech Custom: Metallic", Unit::Pct,   0.f,   1.f,  0.f,   0.f,  FMod) \
    F (scrMixFmt,    "scr_mix_formant",   "Screech Custom: Formant",  Unit::Pct,   0.f,   1.f,  0.f,   0.f,  FMod) \
    F (scrMixFm,     "scr_mix_fm",        "Screech Custom: FM",       Unit::Pct,   0.f,   1.f,  0.5f,  0.f,  FMod) \
    F (scrMixNoise,  "scr_mix_noise",     "Screech Custom: Noise",    Unit::Pct,   0.f,   1.f,  0.f,   0.f,  FMod) \
    F (scrMixFb,     "scr_mix_feedback",  "Screech Custom: Feedback", Unit::Pct,   0.f,   1.f,  0.f,   0.f,  FMod) \
    F (scrAtk,       "scr_attack",        "Screech Attack",           Unit::Ms,    0.f, 2000.f,  2.f,  20.f,  FMod) \
    F (scrHold,      "scr_hold",          "Screech Hold",             Unit::Ms,    0.f, 2000.f,  0.f,  50.f,  FMod) \
    F (scrDec,       "scr_decay",         "Screech Decay",            Unit::Ms,    0.f, 5000.f, 300.f, 300.f, FMod) \
    F (scrSus,       "scr_sustain",       "Screech Sustain",          Unit::Pct,   0.f,   1.f,  0.6f,  0.f,  FMod) \
    F (scrRel,       "scr_release",       "Screech Release",          Unit::Ms,    0.f, 5000.f,  60.f, 100.f, FMod) \
    C (scrCurve,     "scr_curve",         "Screech Amp Curve",        driveCurves, 1, FNone) \
    F (scrModA,      "scr_mod_attack",    "Screech Mod Env Attack",   Unit::Ms,    0.f, 2000.f,  0.f,  20.f,  FMod) \
    F (scrModD,      "scr_mod_decay",     "Screech Mod Env Decay",    Unit::Ms,    1.f, 5000.f, 200.f, 200.f, FMod) \
    C (scrModCurve,  "scr_mod_curve",     "Screech Mod Env Curve",    driveCurves, 1, FNone) \
    F (scrMPitch,    "scr_mod_pitch",     "Screech Mod -> Pitch",     Unit::Semi,-48.f,  48.f,  0.f,   0.f,  FMod) \
    F (scrMFm,       "scr_mod_fm",        "Screech Mod -> FM",        Unit::Bip,  -1.f,   1.f,  0.f,   0.f,  FMod) \
    F (scrMCut,      "scr_mod_cutoff",    "Screech Mod -> Cutoff",    Unit::Bip,  -1.f,   1.f,  0.f,   0.f,  FMod) \
    F (scrMRes,      "scr_mod_reso",      "Screech Mod -> Resonance", Unit::Bip,  -1.f,   1.f,  0.f,   0.f,  FMod) \
    F (scrMFb,       "scr_mod_feedback",  "Screech Mod -> Feedback",  Unit::Bip,  -1.f,   1.f,  0.f,   0.f,  FMod) \
    F (scrMFmt,      "scr_mod_formant",   "Screech Mod -> Formant",   Unit::Bip,  -1.f,   1.f,  0.f,   0.f,  FMod) \
    F (scrMDrive,    "scr_mod_drive",     "Screech Mod -> Drive",     Unit::Bip,  -1.f,   1.f,  0.f,   0.f,  FMod) \
    F (scrMDetune,   "scr_mod_detune",    "Screech Mod -> Detune",    Unit::Bip,  -1.f,   1.f,  0.f,   0.f,  FMod) \
    F (scrPStart,    "scr_pitch_start",   "Screech Pitch Env Start",  Unit::Semi,-48.f,  48.f, 12.f,   0.f,  FMod) \
    F (scrPEnd,      "scr_pitch_end",     "Screech Pitch Env End",    Unit::Semi,-48.f,  48.f,  0.f,   0.f,  FMod) \
    F (scrPAmt,      "scr_pitch_amount",  "Screech Pitch Env Amount", Unit::Pct,   0.f,   1.f,  0.f,   0.f,  FMod) \
    F (scrPA,        "scr_pitch_attack",  "Screech Pitch Env Attack", Unit::Ms,    0.f, 2000.f,  0.f,  20.f,  FMod) \
    F (scrPD,        "scr_pitch_decay",   "Screech Pitch Env Decay",  Unit::Ms,    1.f, 5000.f, 150.f, 200.f, FMod) \
    F (scrPR,        "scr_pitch_release", "Screech Pitch Env Release (end drop)", Unit::Ms, 0.f, 2000.f, 0.f, 50.f, FMod) \
    C (scrPCurve,    "scr_pitch_curve",   "Screech Pitch Env Curve",  driveCurves, 1, FNone) \
    C (scrStartMode, "scr_start_mode",    "Screech Start Mode",       scrStartModes, 0, FNone) \
    F (scrStart,     "scr_start",         "Screech Start",            Unit::Ms,    0.f, 2500.f, 180.f, 300.f, FMod) \
    F (scrOffset,    "scr_offset",        "Screech Start Offset",     Unit::Ms, -250.f, 1000.f,  0.f,   0.f,  FMod) \
    F (scrLength,    "scr_length",        "Screech Length",           Unit::Ms,    1.f, 2500.f, 450.f, 400.f, FMod) \
    F (scrFadeIn,    "scr_fade_in",       "Screech Fade In",          Unit::Ms,    0.f, 1000.f,  5.f,  30.f,  FMod) \
    F (scrFadeOut,   "scr_fade_out",      "Screech Fade Out",         Unit::Ms,    0.f, 2000.f,  60.f, 100.f, FMod) \
    B (scrPreTrig,   "scr_pre_trigger",   "Screech Pre-Trigger",      false, FNone) \
    B (scrSyncTail,  "scr_sync_tail",     "Screech Length = Tail",    false, FNone) \
    B (scrAutoLen,   "scr_auto_length",   "Screech Auto Length",      false, FNone) \
    C (scrQuant,     "scr_quantize",      "Screech Start Quantize",   quantizeModes, 0, FNone) \
    B (scrFollowBody,"scr_follow_body",   "Screech Follow Body Release", true, FNone) \
    B (scrFree,      "scr_independent",   "Screech Independent Tail", false, FNone)
#define FGE_SCR_OSC(F, B, C, n) \
    B (so##n##On,     "scr_osc" #n "_on",        "Screech Osc " #n " On",      n == 1, FNone) \
    C (so##n##Wave,   "scr_osc" #n "_wave",      "Screech Osc " #n " Wave",    scrWaves, 1, FNone) \
    F (so##n##Level,  "scr_osc" #n "_level",     "Screech Osc " #n " Level",   Unit::Pct,   0.f,   1.f,  0.8f,  0.f,  FMod) \
    F (so##n##Pitch,  "scr_osc" #n "_pitch",     "Screech Osc " #n " Pitch",   Unit::Semi,-48.f,  48.f, (n - 1) * 7.f, 0.f, FMod) \
    F (so##n##Fine,   "scr_osc" #n "_fine",      "Screech Osc " #n " Fine",    Unit::Cents,-100.f,100.f, 0.f,  0.f,  FMod) \
    F (so##n##Phase,  "scr_osc" #n "_phase",     "Screech Osc " #n " Phase",   Unit::Deg,   0.f, 360.f,  0.f,   0.f,  FNone) \
    F (so##n##Detune, "scr_osc" #n "_detune",    "Screech Osc " #n " Detune",  Unit::Pct,   0.f,   1.f,  0.f,   0.f,  FMod) \
    F (so##n##Pw,     "scr_osc" #n "_pw",        "Screech Osc " #n " Pulse Width", Unit::Pct, 0.05f, 0.95f, 0.3f, 0.f, FMod) \
    C (so##n##FmSrc,  "scr_osc" #n "_fm_source", "Screech Osc " #n " FM Source", scrOscSel, 0, FNone) \
    F (so##n##Fm,     "scr_osc" #n "_fm_amount", "Screech Osc " #n " FM Amount", Unit::Pct,   0.f,   1.f,  0.f,   0.f,  FMod) \
    C (so##n##RingSrc,"scr_osc" #n "_ring_source","Screech Osc " #n " Ring Source", scrOscSel, 0, FNone) \
    F (so##n##Ring,   "scr_osc" #n "_ring",      "Screech Osc " #n " Ring Mod", Unit::Pct,   0.f,   1.f,  0.f,   0.f,  FMod) \
    F (so##n##Pan,    "scr_osc" #n "_pan",       "Screech Osc " #n " Pan",     Unit::Bip,  -1.f,   1.f,  0.f,   0.f,  FMod)
    // module 07: one PARAMETRIC EQ band (16 in a pool; each placed on a LOCATION, BEFORE / AFTER that place's distortion)
#define FGE_EQ(F, B, C, n) \
    B (eq##n##On,     "eq" #n "_on",        "EQ " #n " On",          false, FNone) \
    C (eq##n##Loc,    "eq" #n "_location",  "EQ " #n " Location",    m06Locations, 0, FNone) \
    C (eq##n##Pos,    "eq" #n "_position",  "EQ " #n " Position",    m06Positions, 1, FNone) \
    C (eq##n##Type,   "eq" #n "_type",      "EQ " #n " Type",        eqTypes, 0, FNone) \
    F (eq##n##Freq,   "eq" #n "_freq",      "EQ " #n " Frequency",   Unit::Hz,   20.f, 20000.f, 25.f * (float) (n * n) + 15.f, 1000.f, FMod | FSmooth) \
    F (eq##n##Gain,   "eq" #n "_gain",      "EQ " #n " Gain",        Unit::Db,  -24.f,  24.f,   0.f,   0.f,  FMod | FSmooth) \
    F (eq##n##Q,      "eq" #n "_q",         "EQ " #n " Q",           Unit::None, 0.1f,  30.f,   0.9f,  1.f,  FMod | FSmooth) \
    C (eq##n##Slope,  "eq" #n "_slope",     "EQ " #n " Slope",       filterSlopes, 1, FNone) \
    F (eq##n##Mix,    "eq" #n "_mix",       "EQ " #n " Mix",         Unit::Pct,   0.f,   1.f,   1.f,   0.f,  FMod | FSmooth) \
    C (eq##n##Stereo, "eq" #n "_stereo",    "EQ " #n " Stereo Mode", eqStereo, 0, FNone)
    // module 07: one DYNAMIC EQ band (4 in a pool)
#define FGE_DEQ(F, B, C, n) \
    B (deq##n##On,    "deq" #n "_on",        "Dyn EQ " #n " On",          false, FNone) \
    C (deq##n##Loc,   "deq" #n "_location",  "Dyn EQ " #n " Location",    m06Locations, 0, FNone) \
    C (deq##n##Pos,   "deq" #n "_position",  "Dyn EQ " #n " Position",    m06Positions, 1, FNone) \
    C (deq##n##Mode,  "deq" #n "_mode",      "Dyn EQ " #n " Mode",        deqModes, 0, FNone) \
    C (deq##n##Src,   "deq" #n "_detector_source","Dyn EQ " #n " Detector Source", deqSources, 0, FNone) \
    C (deq##n##Det,   "deq" #n "_detector",  "Dyn EQ " #n " Detector",    deqDetectors, 0, FNone) \
    F (deq##n##Win,   "deq" #n "_window",    "Dyn EQ " #n " RMS Window",  Unit::Ms,   0.5f,  50.f,   5.f,   5.f,  FMod) \
    F (deq##n##Freq,  "deq" #n "_freq",      "Dyn EQ " #n " Frequency",   Unit::Hz,   20.f, 20000.f, 3500.f, 1000.f, FMod | FSmooth) \
    F (deq##n##Q,     "deq" #n "_q",         "Dyn EQ " #n " Q",           Unit::None, 0.1f,  30.f,   2.f,   1.f,  FMod) \
    F (deq##n##Thr,   "deq" #n "_threshold", "Dyn EQ " #n " Threshold",   Unit::Db,  -60.f,   0.f, -24.f, -20.f, FMod) \
    F (deq##n##Range, "deq" #n "_range",     "Dyn EQ " #n " Range",       Unit::Db,    0.f,  24.f,   9.f,   0.f,  FMod) \
    F (deq##n##Ratio, "deq" #n "_ratio",     "Dyn EQ " #n " Ratio",       Unit::None,  1.f,  20.f,   4.f,   4.f,  FMod) \
    F (deq##n##Knee,  "deq" #n "_knee",      "Dyn EQ " #n " Knee",        Unit::Db,    0.f,  24.f,   6.f,   0.f,  FMod) \
    F (deq##n##Atk,   "deq" #n "_attack",    "Dyn EQ " #n " Attack",      Unit::Ms,   0.1f, 500.f,   2.f,  10.f,  FMod) \
    F (deq##n##Rel,   "deq" #n "_release",   "Dyn EQ " #n " Release",     Unit::Ms,    1.f, 2000.f, 80.f, 100.f, FMod) \
    F (deq##n##Mix,   "deq" #n "_mix",       "Dyn EQ " #n " Mix",         Unit::Pct,   0.f,   1.f,   1.f,   0.f,  FMod | FSmooth)
    // module 07: the MULTIBAND engine (4 bands, LR crossovers) + its bands
#define FGE_MB(F, B, C) \
    B (mbOn,     "mb_on",         "Multiband On",            false, FNone) \
    C (mbLoc,    "mb_location",   "Multiband Location",      m06Locations, 0, FNone) \
    C (mbPos,    "mb_position",   "Multiband Position",      m06Positions, 1, FNone) \
    C (mbXType,  "mb_crossover",  "Multiband Crossover",     mbCrossovers, 1, FNone) \
    F (mbX1,     "mb_xover1",     "Multiband Split SUB / LOW",  Unit::Hz, 20.f,  500.f,   80.f,  80.f, FMod) \
    F (mbX2,     "mb_xover2",     "Multiband Split LOW / MID",  Unit::Hz, 40.f, 2000.f,  250.f, 250.f, FMod) \
    F (mbX3,     "mb_xover3",     "Multiband Split MID / HIGH", Unit::Hz, 200.f, 18000.f, 2500.f, 2500.f, FMod) \
    C (mbListen, "mb_listen",     "Multiband Listen",        mbListen, 0, FNone)
#define FGE_MBB(F, B, C, n) \
    F (mb##n##Gain,  "mb" #n "_gain",      "Band " #n " Gain",          Unit::Db,  -24.f,  24.f,   0.f,   0.f,  FMod | FSmooth) \
    F (mb##n##Width, "mb" #n "_width",     "Band " #n " Width",         Unit::Pct,   0.f,   2.f,   1.f,   0.f,  FMod | FSmooth) \
    B (mb##n##Inv,   "mb" #n "_invert",    "Band " #n " Phase Invert",  false, FNone) \
    B (mb##n##Mute,  "mb" #n "_mute",      "Band " #n " Mute",          false, FNone) \
    B (mb##n##Solo,  "mb" #n "_solo",      "Band " #n " Solo",          false, FNone) \
    C (mb##n##Dyn,   "mb" #n "_dynamics",  "Band " #n " Dynamics",      mbDynModes, 0, FNone) \
    F (mb##n##Thr,   "mb" #n "_threshold", "Band " #n " Threshold",     Unit::Db,  -60.f,   0.f, -18.f, -20.f, FMod) \
    F (mb##n##Ratio, "mb" #n "_ratio",     "Band " #n " Ratio",         Unit::None,  1.f,  20.f,   4.f,   4.f,  FMod) \
    F (mb##n##Atk,   "mb" #n "_attack",    "Band " #n " Attack",        Unit::Ms,   0.1f, 500.f,   5.f,  10.f,  FMod) \
    F (mb##n##Rel,   "mb" #n "_release",   "Band " #n " Release",       Unit::Ms,    1.f, 2000.f, 120.f, 100.f, FMod) \
    F (mb##n##Knee,  "mb" #n "_knee",      "Band " #n " Knee",          Unit::Db,    0.f,  24.f,   6.f,   0.f,  FMod) \
    F (mb##n##Makeup,"mb" #n "_makeup",    "Band " #n " Makeup",        Unit::Db,  -24.f,  24.f,   0.f,   0.f,  FMod) \
    F (mb##n##Mix,   "mb" #n "_mix",       "Band " #n " Dynamics Mix",  Unit::Pct,   0.f,   1.f,   1.f,   0.f,  FMod) \
    F (mb##n##TrA,   "mb" #n "_tr_attack", "Band " #n " Transient Attack",  Unit::Db, -24.f, 24.f, 0.f, 0.f, FMod) \
    F (mb##n##TrS,   "mb" #n "_tr_sustain","Band " #n " Transient Sustain", Unit::Db, -24.f, 24.f, 0.f, 0.f, FMod)
    // module 07: the SPECTRAL SHAPER (tilt, harmonic shaper, exciter, spectral compression, spectral freeze)
#define FGE_SPEC(F, B, C) \
    B (spOn,       "sp_on",              "Spectral On",                false, FNone) \
    C (spLoc,      "sp_location",        "Spectral Location",          m06Locations, 0, FNone) \
    C (spPos,      "sp_position",        "Spectral Position",          m06Positions, 1, FNone) \
    F (spTilt,     "sp_tilt",            "Spectral Tilt",              Unit::Bip,  -1.f,   1.f,   0.f,   0.f,  FMod | FSmooth) \
    F (spPivot,    "sp_tilt_pivot",      "Tilt Pivot",                 Unit::Hz,   20.f, 20000.f, 1000.f, 1000.f, FMod) \
    F (spTiltInt,  "sp_tilt_intensity",  "Tilt Intensity",             Unit::Pct,   0.f,   1.f,   0.5f,  0.f,  FMod) \
    F (spTiltMix,  "sp_tilt_mix",        "Tilt Mix",                   Unit::Pct,   0.f,   1.f,   1.f,   0.f,  FMod) \
    B (spHOn,      "sp_harm_on",         "Harmonic Shaper On",         false, FNone) \
    F (spHEven,    "sp_harm_even",       "Even Harmonics",             Unit::Pct,   0.f,   1.f,   0.5f,  0.f,  FMod) \
    F (spHOdd,     "sp_harm_odd",        "Odd Harmonics",              Unit::Pct,   0.f,   1.f,   0.5f,  0.f,  FMod) \
    F (spHLow,     "sp_harm_low",        "Low Harmonics",              Unit::Pct,   0.f,   1.f,   0.5f,  0.f,  FMod) \
    F (spHHigh,    "sp_harm_high",       "High Harmonics",             Unit::Pct,   0.f,   1.f,   0.5f,  0.f,  FMod) \
    F (spHSplit,   "sp_harm_split",      "Harmonic Low / High Split",  Unit::Hz,  100.f, 8000.f, 800.f, 800.f, FMod) \
    F (spHDrive,   "sp_harm_drive",      "Harmonic Drive",             Unit::Pct,   0.f,   1.f,   0.4f,  0.f,  FMod) \
    F (spHMix,     "sp_harm_mix",        "Harmonic Mix",               Unit::Pct,   0.f,   1.f,   0.5f,  0.f,  FMod) \
    F (spHBal,     "sp_harm_balance",    "Harmonic Balance",           Unit::Bip,  -1.f,   1.f,   0.f,   0.f,  FMod) \
    B (spXOn,      "sp_exc_on",          "Exciter On",                 false, FNone) \
    C (spXMode,    "sp_exc_mode",        "Exciter Mode",               excModes, 1, FNone) \
    C (spXHarm,    "sp_exc_harmonics",   "Exciter Harmonic Mode",      excHarm, 2, FNone) \
    F (spXLo,      "sp_exc_low",         "Exciter Range Low",          Unit::Hz,   20.f, 20000.f, 2500.f, 2000.f, FMod) \
    F (spXHi,      "sp_exc_high",        "Exciter Range High",         Unit::Hz,   20.f, 20000.f, 12000.f, 5000.f, FMod) \
    F (spXAmt,     "sp_exc_amount",      "Excitation",                 Unit::Pct,   0.f,   1.f,   0.4f,  0.f,  FMod) \
    F (spXColor,   "sp_exc_color",       "Exciter Color",              Unit::Pct,   0.f,   1.f,   0.5f,  0.f,  FMod) \
    F (spXDrive,   "sp_exc_drive",       "Exciter Drive",              Unit::Pct,   0.f,   1.f,   0.5f,  0.f,  FMod) \
    F (spXMix,     "sp_exc_mix",         "Exciter Mix",                Unit::Pct,   0.f,   1.f,   1.f,   0.f,  FMod) \
    B (spCOn,      "sp_comp_on",         "Spectral Compression On",    false, FNone) \
    F (spCSens,    "sp_comp_sensitivity","Spectral Sensitivity",       Unit::Pct,   0.f,   1.f,   0.5f,  0.f,  FMod) \
    F (spCThr,     "sp_comp_threshold",  "Spectral Threshold",         Unit::Db,    0.f,  24.f,   6.f,   0.f,  FMod) \
    F (spCRed,     "sp_comp_reduction",  "Spectral Max Reduction",     Unit::Db,    0.f,  24.f,   9.f,   0.f,  FMod) \
    F (spCAtk,     "sp_comp_attack",     "Spectral Attack",            Unit::Ms,   0.1f, 200.f,   3.f,  10.f,  FMod) \
    F (spCRel,     "sp_comp_release",    "Spectral Release",           Unit::Ms,    1.f, 2000.f, 80.f, 100.f, FMod) \
    F (spCLo,      "sp_comp_low",        "Spectral Range Low",         Unit::Hz,   20.f, 20000.f, 300.f, 1000.f, FMod) \
    F (spCHi,      "sp_comp_high",       "Spectral Range High",        Unit::Hz,   20.f, 20000.f, 12000.f, 5000.f, FMod) \
    F (spCSmooth,  "sp_comp_smoothing",  "Spectral Smoothing",         Unit::Pct,   0.f,   1.f,   0.5f,  0.f,  FMod) \
    F (spCMix,     "sp_comp_mix",        "Spectral Compression Mix",   Unit::Pct,   0.f,   1.f,   1.f,   0.f,  FMod) \
    B (spFOn,      "sp_freeze",          "Spectral Freeze",            false, FNone) \
    F (spFCap,     "sp_freeze_capture",  "Freeze Capture Time",        Unit::Ms,    1.f, 1500.f, 150.f, 150.f, FMod) \
    F (spFDecay,   "sp_freeze_decay",    "Freeze Spectral Decay",      Unit::Ms,   10.f, 5000.f, 600.f, 500.f, FMod) \
    F (spFSmooth,  "sp_freeze_smoothing","Freeze Smoothing",           Unit::Pct,   0.f,   1.f,   0.3f,  0.f,  FMod) \
    F (spFPitch,   "sp_freeze_pitch",    "Freeze Pitch Shift",         Unit::Semi,-24.f,  24.f,   0.f,   0.f,  FMod) \
    F (spFMix,     "sp_freeze_mix",      "Freeze Mix",                 Unit::Pct,   0.f,   1.f,   0.5f,  0.f,  FMod) \
    C (toneOrder,  "tone_order",         "EQ / Spectral Order",        toneOrders, 0, FNone)
    // module 07: the STEREO FREQUENCY engine
    // module 08: one COMPRESSOR (4 in a pool; every place incl. MASTER OUTPUT, where LOOKAHEAD works)
#define FGE_CMP(F, B, C, n) \
    B (cmp##n##On,    "cmp" #n "_on",          "Comp " #n " On",              false, FNone) \
    C (cmp##n##Loc,   "cmp" #n "_location",    "Comp " #n " Location",        dynLocations, 0, FNone) \
    C (cmp##n##Pos,   "cmp" #n "_position",    "Comp " #n " Position",        m06Positions, 1, FNone) \
    C (cmp##n##Type,  "cmp" #n "_type",        "Comp " #n " Type",            cmpTypes, 0, FNone) \
    C (cmp##n##Det,   "cmp" #n "_detection",   "Comp " #n " Detection",       cmpDetections, 0, FNone) \
    F (cmp##n##Thr,   "cmp" #n "_threshold",   "Comp " #n " Threshold",       Unit::Db,  -60.f,   0.f, -18.f, -20.f, FMod) \
    F (cmp##n##Ratio, "cmp" #n "_ratio",       "Comp " #n " Ratio",           Unit::None,  1.f,  50.f,   4.f,   4.f, FMod) \
    F (cmp##n##Atk,   "cmp" #n "_attack",      "Comp " #n " Attack",          Unit::Ms,  0.01f, 500.f,  10.f,  10.f, FMod) \
    F (cmp##n##Rel,   "cmp" #n "_release",     "Comp " #n " Release",         Unit::Ms,    1.f, 5000.f, 120.f, 150.f, FMod) \
    C (cmp##n##ARel,  "cmp" #n "_auto_release","Comp " #n " Auto Release",    autoReleases, 0, FNone) \
    F (cmp##n##Knee,  "cmp" #n "_knee",        "Comp " #n " Knee",            Unit::Db,    0.f,  24.f,   6.f,   0.f, FMod) \
    F (cmp##n##In,    "cmp" #n "_input",       "Comp " #n " Input Gain",      Unit::Db,  -24.f,  24.f,   0.f,   0.f, FMod | FSmooth) \
    F (cmp##n##Out,   "cmp" #n "_output",      "Comp " #n " Output Gain",     Unit::Db,  -24.f,  24.f,   0.f,   0.f, FMod | FSmooth) \
    F (cmp##n##Mix,   "cmp" #n "_mix",         "Comp " #n " Mix",             Unit::Pct,   0.f,   1.f,   1.f,   0.f, FMod | FSmooth) \
    F (cmp##n##Dry,   "cmp" #n "_dry_level",   "Comp " #n " Parallel Dry",    Unit::Db,  -60.f,   6.f,   0.f,   0.f, FMod) \
    F (cmp##n##Wet,   "cmp" #n "_wet_level",   "Comp " #n " Parallel Wet",    Unit::Db,  -60.f,   6.f,   0.f,   0.f, FMod) \
    F (cmp##n##Look,  "cmp" #n "_lookahead",   "Comp " #n " Lookahead",       Unit::Ms,    0.f,  20.f,   0.f,   2.f, FNone) \
    F (cmp##n##Link,  "cmp" #n "_stereo_link", "Comp " #n " Stereo Link",     Unit::Pct,   0.f,   1.f,   1.f,   0.f, FMod) \
    C (cmp##n##ScSrc, "cmp" #n "_sc_source",   "Comp " #n " Sidechain Source", scSources, 0, FNone) \
    F (cmp##n##ScHp,  "cmp" #n "_sc_hpf",      "Comp " #n " Sidechain HPF",   Unit::Hz,   20.f, 2000.f,  20.f, 100.f, FMod) \
    F (cmp##n##ScLp,  "cmp" #n "_sc_lpf",      "Comp " #n " Sidechain LPF",   Unit::Hz,  200.f, 20000.f, 20000.f, 2000.f, FMod) \
    F (cmp##n##ScGain,"cmp" #n "_sc_gain",     "Comp " #n " Sidechain Gain",  Unit::Db,  -24.f,  24.f,   0.f,   0.f, FMod) \
    F (cmp##n##ScMix, "cmp" #n "_sc_mix",      "Comp " #n " Sidechain Mix",   Unit::Pct,   0.f,   1.f,   1.f,   0.f, FMod)
    // module 08: one TRANSIENT SHAPER (5, by default one per part / the texture)
#define FGE_TS(F, B, C, n, defLoc) \
    B (ts##n##On,     "ts" #n "_on",           "Transient " #n " On",         false, FNone) \
    C (ts##n##Loc,    "ts" #n "_location",     "Transient " #n " Location",   dynLocations, defLoc, FNone) \
    C (ts##n##Pos,    "ts" #n "_position",     "Transient " #n " Position",   m06Positions, 1, FNone) \
    C (ts##n##Det,    "ts" #n "_detection",    "Transient " #n " Detection",  tsDetections, 0, FNone) \
    C (ts##n##Mode,   "ts" #n "_mode",         "Transient " #n " Mode",       tsModes, 0, FNone) \
    F (ts##n##Atk,    "ts" #n "_attack",       "Transient " #n " Attack",     Unit::Bip,  -1.f,   1.f,   0.f,   0.f, FMod) \
    F (ts##n##Sus,    "ts" #n "_sustain",      "Transient " #n " Sustain",    Unit::Bip,  -1.f,   1.f,   0.f,   0.f, FMod) \
    F (ts##n##Sens,   "ts" #n "_sensitivity",  "Transient " #n " Sensitivity",Unit::Pct,   0.f,   1.f,   0.5f,  0.f, FMod) \
    F (ts##n##Time,   "ts" #n "_time",         "Transient " #n " Time",       Unit::Ms,    1.f, 500.f,  20.f,  30.f, FMod) \
    F (ts##n##Rel,    "ts" #n "_release",      "Transient " #n " Release",    Unit::Ms,    1.f, 2000.f, 100.f, 150.f, FMod) \
    F (ts##n##Mix,    "ts" #n "_mix",          "Transient " #n " Mix",        Unit::Pct,   0.f,   1.f,   1.f,   0.f, FMod) \
    F (ts##n##Out,    "ts" #n "_output",       "Transient " #n " Output",     Unit::Db,  -24.f,  12.f,   0.f,   0.f, FMod | FSmooth)
    // module 08: CLIPPER (2)
#define FGE_CLP(F, B, C, n) \
    B (clp##n##On,    "clp" #n "_on",          "Clipper " #n " On",           false, FNone) \
    C (clp##n##Loc,   "clp" #n "_location",    "Clipper " #n " Location",     dynLocations, 0, FNone) \
    C (clp##n##Pos,   "clp" #n "_position",    "Clipper " #n " Position",     m06Positions, 1, FNone) \
    C (clp##n##Type,  "clp" #n "_type",        "Clipper " #n " Type",         clipTypes, 0, FNone) \
    F (clp##n##In,    "clp" #n "_input",       "Clipper " #n " Input Gain",   Unit::Db,  -24.f,  24.f,   0.f,   0.f, FMod | FSmooth) \
    F (clp##n##Ceil,  "clp" #n "_ceiling",     "Clipper " #n " Ceiling",      Unit::Db,  -24.f,   0.f,  -1.f,   0.f, FMod) \
    F (clp##n##Drive, "clp" #n "_drive",       "Clipper " #n " Drive",        Unit::Pct,   0.f,   1.f,   0.3f,  0.f, FMod) \
    F (clp##n##Sym,   "clp" #n "_symmetry",    "Clipper " #n " Symmetry",     Unit::Bip,  -1.f,   1.f,   0.f,   0.f, FMod) \
    F (clp##n##Soft,  "clp" #n "_softness",    "Clipper " #n " Softness",     Unit::Pct,   0.f,   1.f,   0.3f,  0.f, FMod) \
    F (clp##n##Mix,   "clp" #n "_mix",         "Clipper " #n " Mix",          Unit::Pct,   0.f,   1.f,   1.f,   0.f, FMod | FSmooth) \
    F (clp##n##Out,   "clp" #n "_output",      "Clipper " #n " Output Gain",  Unit::Db,  -24.f,  12.f,   0.f,   0.f, FMod | FSmooth) \
    C (clp##n##Os,    "clp" #n "_oversampling","Clipper " #n " Oversampling", stageOversampling, 0, FNone)
    // module 08: LIMITER (2)
#define FGE_LIM(F, B, C, n) \
    B (lim##n##On,    "lim" #n "_on",          "Limiter " #n " On",           false, FNone) \
    C (lim##n##Loc,   "lim" #n "_location",    "Limiter " #n " Location",     dynLocations, n == 1 ? 15 : 0, FNone) \
    C (lim##n##Pos,   "lim" #n "_position",    "Limiter " #n " Position",     m06Positions, 1, FNone) \
    C (lim##n##Mode,  "lim" #n "_mode",        "Limiter " #n " Mode",         limModes, 0, FNone) \
    F (lim##n##Ceil,  "lim" #n "_ceiling",     "Limiter " #n " Ceiling",      Unit::Db,  -24.f,   0.f,  -0.3f,  0.f, FMod) \
    F (lim##n##In,    "lim" #n "_input",       "Limiter " #n " Input Gain",   Unit::Db,  -24.f,  24.f,   0.f,   0.f, FMod | FSmooth) \
    F (lim##n##Rel,   "lim" #n "_release",     "Limiter " #n " Release",      Unit::Ms,    1.f, 2000.f,  60.f, 100.f, FMod) \
    F (lim##n##Look,  "lim" #n "_lookahead",   "Limiter " #n " Lookahead",    Unit::Ms,    0.f,  20.f,   1.5f,  2.f, FNone) \
    F (lim##n##Atk,   "lim" #n "_attack",      "Limiter " #n " Attack",       Unit::Ms,    0.f, 100.f,   0.f,  5.f, FMod) \
    F (lim##n##Knee,  "lim" #n "_knee",        "Limiter " #n " Knee",         Unit::Db,    0.f,  12.f,   1.f,   0.f, FMod) \
    F (lim##n##Link,  "lim" #n "_stereo_link", "Limiter " #n " Stereo Link",  Unit::Pct,   0.f,   1.f,   1.f,   0.f, FMod) \
    B (lim##n##Tp,    "lim" #n "_true_peak",   "Limiter " #n " True Peak",    false, FNone) \
    C (lim##n##Os,    "lim" #n "_oversampling","Limiter " #n " Oversampling", limOversampling, 2, FNone) \
    F (lim##n##Mix,   "lim" #n "_mix",         "Limiter " #n " Mix",          Unit::Pct,   0.f,   1.f,   1.f,   0.f, FMod | FSmooth)
    // module 08: CREST FACTOR control, AUTO GAIN / LEVEL MATCH, order
#define FGE_DYNG(F, B, C) \
    B (crOn,       "crest_on",          "Crest Control On",        false, FNone) \
    C (crLoc,      "crest_location",    "Crest Control Location",  dynLocations, 0, FNone) \
    C (crPos,      "crest_position",    "Crest Control Position",  m06Positions, 1, FNone) \
    F (crTarget,   "crest_target",      "Target Crest",            Unit::Db,    0.f,  30.f,  12.f,  10.f, FMod) \
    F (crPunch,    "crest_punch",       "Crest Punch Control",     Unit::Bip,  -1.f,   1.f,   0.f,   0.f, FMod) \
    F (crBody,     "crest_body",        "Crest Body Control",      Unit::Bip,  -1.f,   1.f,   0.f,   0.f, FMod) \
    F (crTail,     "crest_tail",        "Crest Tail Control",      Unit::Bip,  -1.f,   1.f,   0.f,   0.f, FMod) \
    F (crSens,     "crest_sensitivity", "Crest Sensitivity",       Unit::Pct,   0.f,   1.f,   0.5f,  0.f, FMod) \
    F (crMix,      "crest_mix",         "Crest Mix",               Unit::Pct,   0.f,   1.f,   1.f,   0.f, FMod) \
    C (dynAuto,    "dyn_auto_gain",     "Dynamics Auto Gain",      autoGainModes, 0, FNone) \
    B (dynMatch,   "dyn_level_match",   "Dynamics Level Match (bypass)", false, FNone) \
    C (dynOrder,   "dyn_order",         "Dynamics Order",          dynOrders, 0, FNone) \
    C (mbLink,     "mb_link",           "Multiband Band Link",     mbLinks, 0, FNone)
#define FGE_STW(F, B, C) \
    B (stwOn,      "stw_on",             "Stereo Engine On",           false, FNone) \
    C (stwLoc,     "stw_location",       "Stereo Engine Location",     stwLocations, 0, FNone) \
    F (stwWidth,   "stw_width",          "Stereo Width",               Unit::Pct,   0.f,   2.f,   1.f,   0.f,  FMod | FSmooth) \
    F (stwLowW,    "stw_low_width",      "Low Width",                  Unit::Pct,   0.f,   2.f,   1.f,   0.f,  FMod | FSmooth) \
    F (stwMidW,    "stw_mid_width",      "Mid Width",                  Unit::Pct,   0.f,   2.f,   1.f,   0.f,  FMod | FSmooth) \
    F (stwHighW,   "stw_high_width",     "High Width",                 Unit::Pct,   0.f,   2.f,   1.f,   0.f,  FMod | FSmooth) \
    F (stwXLow,    "stw_xover_low",      "Stereo Split Low / Mid",     Unit::Hz,   40.f, 2000.f, 250.f, 250.f, FMod) \
    F (stwXHigh,   "stw_xover_high",     "Stereo Split Mid / High",    Unit::Hz,  300.f, 16000.f, 3000.f, 3000.f, FMod) \
    F (stwMono,    "stw_mono_below",     "Mono Below",                 Unit::Hz,   20.f,  500.f, 120.f, 120.f, FMod) \
    F (stwMix,     "stw_mix",            "Stereo Mix",                 Unit::Pct,   0.f,   1.f,   1.f,   0.f,  FMod | FSmooth) \
    F (stwLowMid,  "stw_low_mid_gain",   "Low Mid Gain",               Unit::Db,  -24.f,  12.f,   0.f,   0.f,  FMod) \
    F (stwLowSide, "stw_low_side_gain",  "Low Side Gain",              Unit::Db,  -24.f,  12.f,   0.f,   0.f,  FMod) \
    F (stwMidMid,  "stw_mid_mid_gain",   "Mid Mid Gain",               Unit::Db,  -24.f,  12.f,   0.f,   0.f,  FMod) \
    F (stwMidSide, "stw_mid_side_gain",  "Mid Side Gain",              Unit::Db,  -24.f,  12.f,   0.f,   0.f,  FMod) \
    F (stwHiMid,   "stw_high_mid_gain",  "High Mid Gain",              Unit::Db,  -24.f,  12.f,   0.f,   0.f,  FMod) \
    F (stwHiSide,  "stw_high_side_gain", "High Side Gain",             Unit::Db,  -24.f,  12.f,   0.f,   0.f,  FMod) \
    B (stwMonoLis, "stw_mono_listen",    "Mono Listen",                false, FNone)
#define FGE_SCR_FLT(F, B, C, n) \
    B (sf##n##On,     "scr_f" #n "_on",          "Screech Filter " #n " On",   n == 1, FNone) \
    C (sf##n##Type,   "scr_f" #n "_type",        "Screech Filter " #n " Type", m06FilterTypes, n == 1 ? 2 : 0, FNone) \
    F (sf##n##Cut,    "scr_f" #n "_cutoff",      "Screech Filter " #n " Cutoff", Unit::Hz, 20.f, 20000.f, n == 1 ? 3000.f : 9000.f, 1000.f, FMod) \
    F (sf##n##Res,    "scr_f" #n "_resonance",   "Screech Filter " #n " Resonance", Unit::Pct, 0.f, 1.f, 0.5f, 0.f, FMod) \
    F (sf##n##Drive,  "scr_f" #n "_drive",       "Screech Filter " #n " Drive", Unit::Pct,  0.f,   1.f,  0.f,   0.f,  FMod) \
    F (sf##n##Key,    "scr_f" #n "_key_track",   "Screech Filter " #n " Key Track", Unit::Pct, 0.f, 2.f, 1.f,  0.f,  FMod) \
    F (sf##n##Env,    "scr_f" #n "_env_amount",  "Screech Filter " #n " Env Amount", Unit::Bip, -1.f, 1.f, 0.f, 0.f,  FMod) \
    F (sf##n##Mix,    "scr_f" #n "_mix",         "Screech Filter " #n " Mix",  Unit::Pct,   0.f,   1.f,  1.f,   0.f,  FMod)
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
    // module 10: a SAMPLE SLOT = the layer path (level, pan, width, pitch, envelope, filter, drive, group / sends) + the sample playback
#define FGE_SPC(F, B, C, T, t, N, wDef, lmDef) \
    B (spc##T##On,     "spc_" t "_on",           N " Stereo On",          false, FNone) \
    F (spc##T##Pan,    "spc_" t "_pan",          N " Stereo Pan",         Unit::Bip,  -1.f,  1.f,  0.f,   0.f,  FMod) \
    F (spc##T##Bal,    "spc_" t "_balance",      N " Balance",            Unit::Bip,  -1.f,  1.f,  0.f,   0.f,  FMod) \
    F (spc##T##Width,  "spc_" t "_width",        N " Stereo Width",       Unit::Pct,   0.f,  2.f,  wDef,  0.f,  FMod) \
    C (spc##T##Mode,   "spc_" t "_mode",         N " Width Mode",         spcModes, 0, FNone) \
    F (spc##T##Mix,    "spc_" t "_mix",          N " Stereo Mix",         Unit::Pct,   0.f,  1.f,  1.f,   0.f,  FMod) \
    F (spc##T##Gain,   "spc_" t "_gain",         N " Stereo Output",      Unit::Db,  -24.f, 12.f,  0.f,   0.f,  FMod) \
    B (spc##T##Swap,   "spc_" t "_swap",         N " Channel Swap",       false, FNone) \
    B (spc##T##MsOn,   "spc_" t "_ms_on",        N " M/S On",             false, FNone) \
    F (spc##T##Mid,    "spc_" t "_mid",          N " Mid Level",          Unit::Db,  -24.f, 12.f,  0.f,   0.f,  FMod) \
    F (spc##T##Side,   "spc_" t "_side",         N " Side Level",         Unit::Db,  -24.f, 12.f,  0.f,   0.f,  FMod) \
    F (spc##T##MsW,    "spc_" t "_ms_width",     N " M/S Width",          Unit::Pct,   0.f,  2.f,  1.f,   0.f,  FMod) \
    B (spc##T##MidSo,  "spc_" t "_mid_solo",     N " Mid Solo",           false, FNone) \
    B (spc##T##SideSo, "spc_" t "_side_solo",    N " Side Solo",          false, FNone) \
    B (spc##T##LoMono, "spc_" t "_low_mono",     N " Low Mono",           lmDef, FNone) \
    F (spc##T##Xover,  "spc_" t "_crossover",    N " Low Mono Crossover", Unit::Hz,   20.f, 300.f, 120.f, 120.f, FMod) \
    C (spc##T##Pol,    "spc_" t "_polarity",     N " Polarity Invert",    spcPolarity, 0, FNone) \
    F (spc##T##Delay,  "spc_" t "_delay",        N " Channel Delay",      Unit::Ms,  -10.f, 10.f,  0.f,   0.f,  FMod) \
    F (spc##T##HTime,  "spc_" t "_haas_time",    N " Haas Time",          Unit::Ms,    1.f, 40.f, 15.f,   0.f,  FMod) \
    C (spc##T##HCh,    "spc_" t "_haas_channel", N " Haas Channel",       spcHaasCh, 1, FNone)

#define FGE_SMP(F, B, C, n) \
    B (smp##n##On,      "smp" #n "_on",          "Sample " #n " On",           false, FNone) \
    C (smp##n##Group,   "smp" #n "_group",       "Sample " #n " Group",        layerGroups, 4, FNone) \
    F (smp##n##Level,   "smp" #n "_level",       "Sample " #n " Level",        Unit::Db,  -60.f,  12.f,  -6.f,   0.f,  FMod | FSmooth) \
    F (smp##n##Pan,     "smp" #n "_pan",         "Sample " #n " Pan",          Unit::Bip,  -1.f,   1.f,   0.f,   0.f,  FMod) \
    F (smp##n##Pitch,   "smp" #n "_pitch",       "Sample " #n " Pitch",        Unit::Semi,-48.f,  48.f,   0.f,   0.f,  FMod) \
    F (smp##n##Fine,    "smp" #n "_fine",        "Sample " #n " Fine",         Unit::Cents,-100.f,100.f,  0.f,   0.f,  FMod) \
    F (smp##n##Len,     "smp" #n "_length",      "Sample " #n " Length",       Unit::Ms,    1.f, 2500.f, 2500.f, 300.f, FMod) \
    F (smp##n##Offset,  "smp" #n "_offset",      "Sample " #n " Start Offset", Unit::Ms,    0.f, 2500.f,  0.f,  100.f, FMod) \
    F (smp##n##Phase,   "smp" #n "_phase",       "Sample " #n " Phase",        Unit::Deg,   0.f, 360.f,   0.f,   0.f,  FNone) \
    B (smp##n##Inv,     "smp" #n "_invert",      "Sample " #n " Phase Invert", false, FNone) \
    F (smp##n##Width,   "smp" #n "_width",       "Sample " #n " Stereo Width", Unit::Pct,   0.f,   1.f,   0.f,   0.f,  FMod) \
    B (smp##n##FltOn,   "smp" #n "_filter_on",   "Sample " #n " Filter On",    false, FNone) \
    C (smp##n##FltType, "smp" #n "_filter_type", "Sample " #n " Filter Type",  filterTypes, 0, FNone) \
    F (smp##n##Cutoff,  "smp" #n "_cutoff",      "Sample " #n " Cutoff",       Unit::Hz,   20.f, 20000.f, 5000.f, 1000.f, FMod) \
    F (smp##n##Reso,    "smp" #n "_resonance",   "Sample " #n " Resonance",    Unit::Pct,   0.f,   1.f,   0.2f,  0.f,  FMod) \
    F (smp##n##Drive,   "smp" #n "_drive",       "Sample " #n " Drive",        Unit::Pct,   0.f,   1.f,   0.f,   0.f,  FMod) \
    C (smp##n##EnvMode, "smp" #n "_env_mode",    "Sample " #n " Env Mode",     layerEnvModes, 1, FNone) \
    F (smp##n##Atk,     "smp" #n "_attack",      "Sample " #n " Attack",       Unit::Ms,    0.f, 2500.f,  1.f,  20.f,  FMod) \
    F (smp##n##Dec,     "smp" #n "_decay",       "Sample " #n " Decay",        Unit::Ms,    1.f, 2500.f, 120.f, 150.f, FMod) \
    F (smp##n##Sus,     "smp" #n "_sustain",     "Sample " #n " Sustain",      Unit::Pct,   0.f,   1.f,   0.3f,  0.f,  FMod) \
    F (smp##n##Rel,     "smp" #n "_release",     "Sample " #n " Release",      Unit::Ms,    0.f, 2500.f,  30.f,  60.f,  FMod) \
    F (smp##n##Delay,   "smp" #n "_env_delay",   "Sample " #n " Env Delay",    Unit::Ms,    0.f, 2500.f,  0.f, 100.f,  FMod) \
    F (smp##n##EnvAmt,  "smp" #n "_env_amount",  "Sample " #n " Env Amount",   Unit::Pct,   0.f,   1.f,   0.f,   0.f,  FMod) \
    F (smp##n##TScale,  "smp" #n "_time_scale",  "Sample " #n " Time Scale",   Unit::Pct,  0.1f,   4.f,   1.f,   1.f,  FMod) \
    B (smp##n##Mute,    "smp" #n "_mute",        "Sample " #n " Mute",         false, FNone) \
    B (smp##n##Solo,    "smp" #n "_solo",        "Sample " #n " Solo",         false, FNone) \
    C (smp##n##Route,   "smp" #n "_route_mode",  "Sample " #n " Routing",      routeModes, 0, FNone) \
    B (smp##n##PreFdr,  "smp" #n "_send_pre",    "Sample " #n " Pre-Fader Sends", false, FNone) \
    F (smp##n##SPunch,  "smp" #n "_send_punch",  "Sample " #n " Send Punch",   Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (smp##n##SBody,   "smp" #n "_send_body",   "Sample " #n " Send Body",    Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (smp##n##STail,   "smp" #n "_send_tail",   "Sample " #n " Send Tail",    Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (smp##n##SSub,    "smp" #n "_send_sub",    "Sample " #n " Send Sub",     Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (smp##n##STex,    "smp" #n "_send_texture","Sample " #n " Send Texture", Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (smp##n##SCust,   "smp" #n "_send_custom", "Sample " #n " Send Custom",  Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    F (smp##n##SMaster, "smp" #n "_send_master", "Sample " #n " Send Master",  Unit::Db,  -60.f,  12.f, -60.f,  0.f,  FMod | FSmooth) \
    C (smp##n##LinkSrc, "smp" #n "_link_source", "Sample " #n " Link To",      smpLinks, 0, FNone) \
    B (smp##n##LinkLvl, "smp" #n "_link_level",  "Sample " #n " Link Level",   false, FNone) \
    B (smp##n##LinkPit, "smp" #n "_link_pitch",  "Sample " #n " Link Pitch",   false, FNone) \
    B (smp##n##LinkLen, "smp" #n "_link_length", "Sample " #n " Link Length",  false, FNone) \
    B (smp##n##LinkEnv, "smp" #n "_link_env",    "Sample " #n " Link Envelope",false, FNone) \
    F (smp##n##Start,     "smp" #n "_start",         "Sample " #n " Start",       Unit::Pct,   0.f,   1.f,   0.f,   0.f,  FMod) \
    F (smp##n##End,       "smp" #n "_end",           "Sample " #n " End",         Unit::Pct,   0.f,   1.f,   1.f,   0.f,  FMod) \
    F (smp##n##FadeIn,    "smp" #n "_fade_in",       "Sample " #n " Fade In",     Unit::Ms,    0.f, 500.f,   0.f,  50.f,  FMod) \
    F (smp##n##FadeOut,   "smp" #n "_fade_out",      "Sample " #n " Fade Out",    Unit::Ms,    0.f, 500.f,   3.f,  50.f,  FMod) \
    F (smp##n##Rate,      "smp" #n "_rate",          "Sample " #n " Playback Rate", Unit::None, 0.25f, 4.f,  1.f,   1.f,  FMod) \
    B (smp##n##Rev,       "smp" #n "_reverse",       "Sample " #n " Reverse",     false, FNone) \
    C (smp##n##Mode,      "smp" #n "_play_mode",     "Sample " #n " Play Mode",   smpModes, 0, FNone) \
    F (smp##n##LStart,    "smp" #n "_loop_start",    "Sample " #n " Loop Start",  Unit::Pct,   0.f,   1.f,   0.25f, 0.f,  FMod) \
    F (smp##n##LEnd,      "smp" #n "_loop_end",      "Sample " #n " Loop End",    Unit::Pct,   0.f,   1.f,   0.75f, 0.f,  FMod) \
    F (smp##n##XFade,     "smp" #n "_loop_xfade",    "Sample " #n " Loop Crossfade", Unit::Ms, 0.f, 200.f,  10.f,  30.f,  FMod) \
    F (smp##n##Root,      "smp" #n "_root",          "Sample " #n " Root Note",   Unit::None,  0.f, 127.f,  31.f,  0.f,  FNone) \
    B (smp##n##Track,     "smp" #n "_tracking",      "Sample " #n " Pitch Tracking", false, FNone)
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

    // module 09: a macro (0..1; a modulation SOURCE only - FINAL = BASE + mapping, RULES.md 1 / 2)
#define FGE_MACRO(F, e, id, nm) \
    F (macro_##e,    "macro_" id,       nm,                        Unit::Pct,   0.f,   1.f,   0.f,   0.f,   FGlobal)
    // module 09: a MIDI CC source (MIDI LEARN sets the number)
#define FGE_CC(F, n, defCc) \
    F (cc##n##Num,    "cc" #n "_number",    "MIDI CC " #n " Number",   Unit::None,  0.f, 127.f, defCc, 0.f, FNone) \
    F (cc##n##Smooth, "cc" #n "_smooth",    "MIDI CC " #n " Smoothing",Unit::Ms,    0.f, 500.f, 10.f, 50.f, FMod)
    // module 09: an LFO (a modulation signal, not audio)
#define FGE_LFO(F, B, C, n) \
    C (lfo##n##Wave,  "lfo" #n "_wave",     "LFO " #n " Wave",        lfoWaves, 0, FNone) \
    F (lfo##n##Rate,  "lfo" #n "_rate",     "LFO " #n " Rate",        Unit::Hz,    0.01f, 40.f,  2.f,   2.f,   FMod) \
    B (lfo##n##Sync,  "lfo" #n "_sync",     "LFO " #n " Host Sync",   false, FNone) \
    C (lfo##n##Div,   "lfo" #n "_division", "LFO " #n " Division",    modDivisions, 2, FNone) \
    F (lfo##n##Phase, "lfo" #n "_phase",    "LFO " #n " Phase",       Unit::Deg,   0.f,  360.f,  0.f,   0.f,   FMod) \
    C (lfo##n##Retrig,"lfo" #n "_retrigger","LFO " #n " Retrigger",   modRetrigs, 2, FNone) \
    C (lfo##n##Mode,  "lfo" #n "_mode",     "LFO " #n " Mode",        lfoModes, 0, FNone) \
    C (lfo##n##Pol,   "lfo" #n "_polarity", "LFO " #n " Polarity",    modPolarities, 0, FNone) \
    F (lfo##n##FadeIn,"lfo" #n "_fade_in",  "LFO " #n " Fade In",     Unit::Ms,    0.f, 5000.f,  0.f, 300.f,   FMod) \
    F (lfo##n##FadeOut,"lfo" #n "_fade_out","LFO " #n " Fade Out",    Unit::Ms,    0.f, 5000.f,  0.f, 300.f,   FMod) \
    F (lfo##n##Smooth,"lfo" #n "_smooth",   "LFO " #n " Smoothing",   Unit::Ms,    0.f,  200.f,  0.f,  20.f,   FMod) \
    F (lfo##n##Pw,    "lfo" #n "_pw",       "LFO " #n " Pulse Width", Unit::Pct,   0.02f, 0.98f, 0.5f,  0.f,   FMod)
    // module 09: the step sequencer (a modulation source; the steps are state data, not parameters)
#define FGE_SEQ(F, B, C) \
    F (seqLength,    "seq_length",      "Seq Length",              Unit::None,  1.f,   64.f,  16.f,  0.f,   FNone) \
    B (seqSync,      "seq_sync",        "Seq Host Sync",           true,  FNone) \
    F (seqRate,      "seq_rate",        "Seq Rate",                Unit::Hz,    0.1f,  64.f,  8.f,   8.f,   FMod) \
    C (seqDiv,       "seq_division",    "Seq Division",            modDivisions, 4, FNone) \
    C (seqDir,       "seq_direction",   "Seq Direction",           seqDirections, 0, FNone) \
    C (seqReset,     "seq_reset",       "Seq Reset",               modRetrigs, 0, FNone) \
    F (seqSwing,     "seq_swing",       "Seq Swing",               Unit::Pct,   0.f,   0.75f, 0.f,   0.f,   FMod) \
    F (seqHuman,     "seq_humanize",    "Seq Humanize",            Unit::Pct,   0.f,   1.f,   0.f,   0.f,   FMod) \
    C (seqSkip,      "seq_skip",        "Seq Skipped Step",        seqSkips, 0, FNone) \
    C (seqPol,       "seq_polarity",    "Seq Polarity",            modPolarities, 0, FNone) \
    F (seqSmooth,    "seq_smooth",      "Seq Smoothing",           Unit::Ms,    0.f,  200.f,  2.f,  20.f,   FMod) \
    F (seqAccent,    "seq_accent",      "Seq Accent",              Unit::Pct,   0.f,   1.f,   0.25f, 0.f,   FMod) \
    F (seqSeed,      "seq_seed",        "Seq Seed",                Unit::None,  0.f, 9999.f,  1.f,   0.f,   FNone)
    // module 09: a random source (deterministic with a SEED)
#define FGE_RND(F, B, C, n) \
    C (rnd##n##Mode,  "rnd" #n "_mode",     "Random " #n " Mode",     rndModes, 1, FNone) \
    F (rnd##n##Seed,  "rnd" #n "_seed",     "Random " #n " Seed",     Unit::None,  0.f, 9999.f, (float) n, 0.f, FNone) \
    F (rnd##n##Rate,  "rnd" #n "_rate",     "Random " #n " Rate",     Unit::Hz,    0.01f, 40.f,  2.f,   2.f,   FMod) \
    F (rnd##n##Smooth,"rnd" #n "_smooth",   "Random " #n " Smoothing",Unit::Ms,    0.f,  500.f,  0.f,  50.f,   FMod) \
    F (rnd##n##Amount,"rnd" #n "_amount",   "Random " #n " Amount",   Unit::Pct,   0.f,   1.f,   1.f,   0.f,   FMod) \
    F (rnd##n##Dev,   "rnd" #n "_deviation","Random " #n " Max Step", Unit::Pct,   0.01f, 1.f,  0.25f, 0.f,   FMod) \
    C (rnd##n##Pol,   "rnd" #n "_polarity", "Random " #n " Polarity", modPolarities, 0, FNone)

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
    float noteToHz (const juce::String&);                          // "G4" -> 392 Hz (0 = not a note name)
    // a short tooltip text for each parameter (what it does to the sound)
    juce::String tooltip (int id);
}
