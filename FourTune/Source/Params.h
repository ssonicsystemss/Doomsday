#pragma once
// =====================================================================
//  4TUNE - PARAMETER TABLE (one list: APVTS layout, enum P:: for the DSP, frozen IDs in Tests/param_ids.txt)
//  RULE: the ORDER is the DAW automation index -> new parameters are only APPENDED at the end, an ID never changes.
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace tune
{
    enum class Fm : uint8_t { Pct, Bip, Hz, Ms, Sec, Db, Semi, Cents, Oct, Pan, LfoHz, Num, Int };

    namespace Choices
    {
        inline const juce::StringArray waves      { "SAW", "SINE", "TRIANGLE", "SQUARE", "PULSE", "WAVETABLE" };
        inline const juce::StringArray filters    { "LP 12", "LP 24", "HP 12", "HP 24", "BAND", "NOTCH" };
        inline const juce::StringArray routes     { "A + B", "A ONLY", "B ONLY" };
        inline const juce::StringArray lfoShapes  { "SINE", "TRIANGLE", "SAW", "RAMP", "SQUARE", "S&H", "RANDOM" };
        inline const juce::StringArray modSources { "NONE", "AMP ENV", "FILTER ENV", "MOD ENV", "LFO 1", "LFO 2", "VELOCITY",
                                                    "MOD WHEEL", "AFTERTOUCH", "KEY TRACK", "RANDOM", "FORTUNE" };
        // destinations: per-voice ones first, then the global ones (SCREAM / DELAY / REVERB) - see Engine
        inline const juce::StringArray modDests   { "NONE", "PITCH", "OSC 1 PITCH", "OSC 2 PITCH", "OSC 3 PITCH", "OSC 4 PITCH", "FINE TUNE",
                                                    "WT POSITION", "WARP", "CUTOFF", "RESONANCE", "UNISON DETUNE", "STEREO SPREAD", "PAN",
                                                    "VOLUME", "OSC 1 LEVEL", "OSC 2 LEVEL", "OSC 3 LEVEL", "OSC 4 LEVEL", "GROUP A", "GROUP B",
                                                    "FILTER DRIVE", "SCREAM", "DELAY MIX", "REVERB MIX" };
        inline const juce::StringArray screamOs   { "1x", "2x", "4x" };
        inline const juce::StringArray playModes  { "POLY", "MONO", "LEGATO" };
        inline const juce::StringArray fxOrders   { "EQ > DIST > CHORUS > DELAY > REVERB", "DIST > EQ > CHORUS > DELAY > REVERB",
                                                    "EQ > DIST > DELAY > REVERB > CHORUS", "EQ > CHORUS > DIST > DELAY > REVERB",
                                                    "DIST > EQ > DELAY > CHORUS > REVERB" };
        inline juce::StringArray syncDivs()       { return { "1/64", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D",
                                                             "1/2T", "1/2", "1/2D", "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS" }; }
        inline double syncBeats (int i)
        {
            static const double b[] { 0.0625, 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 4.0 / 3.0, 2.0, 3.0, 4.0, 8.0, 16.0, 32.0, 64.0 };
            return b[juce::jlimit (0, 18, i)];
        }
    }

    // default sound (INIT = a pleasant euphoric pluck): per oscillator  on, wave, oct, level, voices, detune, phase random, pan
    struct OscDef { bool on; int wave; float oct, level; int voices; float detune, phRand, spread, mix; };
    inline constexpr OscDef oscDefs[4] { { true,  0,  0.f, 0.80f, 3, 12.f, 0.15f, 0.4f, 0.8f },
                                         { true,  3, -1.f, 0.35f, 1,  0.f, 0.f,   0.f,  1.f  },
                                         { true,  0,  0.f, 0.60f, 7, 30.f, 1.f,   1.f,  0.75f },
                                         { false, 0,  1.f, 0.40f, 5, 22.f, 1.f,   1.f,  0.7f } };

    // ---------------------------------------------------------------- the table
    //  F (enum, id, name, lo, hi, default, skewCentre (0 = linear), interval, format)
    //  C (enum, id, name, choices, default)      B (enum, id, name, default)
#define TUNE_OSC(F, C, B, n) \
    B (O##n##On,     "o" #n "_on",      "Osc " #n " On",              oscDefs[n - 1].on) \
    C (O##n##Wave,   "o" #n "_wave",    "Osc " #n " Wave",            Choices::waves, oscDefs[n - 1].wave) \
    F (O##n##Wt,     "o" #n "_wt",      "Osc " #n " Wave Position",   0.f, 1.f, 0.f, 0.f, 0.f, Fm::Pct) \
    F (O##n##Warp,   "o" #n "_warp",    "Osc " #n " Warp",            0.f, 1.f, 0.f, 0.f, 0.f, Fm::Pct) \
    F (O##n##Oct,    "o" #n "_oct",     "Osc " #n " Octave",          -3.f, 3.f, oscDefs[n - 1].oct, 0.f, 1.f, Fm::Int) \
    F (O##n##Semi,   "o" #n "_semi",    "Osc " #n " Semitone",        -12.f, 12.f, 0.f, 0.f, 1.f, Fm::Semi) \
    F (O##n##Fine,   "o" #n "_fine",    "Osc " #n " Fine Tune",       -100.f, 100.f, 0.f, 0.f, 0.f, Fm::Cents) \
    F (O##n##Level,  "o" #n "_level",   "Osc " #n " Level",           0.f, 1.f, oscDefs[n - 1].level, 0.f, 0.f, Fm::Pct) \
    F (O##n##Pan,    "o" #n "_pan",     "Osc " #n " Pan",             -1.f, 1.f, 0.f, 0.f, 0.f, Fm::Pan) \
    F (O##n##Width,  "o" #n "_width",   "Osc " #n " Stereo Spread",   0.f, 1.f, 1.f, 0.f, 0.f, Fm::Pct) \
    F (O##n##Phase,  "o" #n "_phase",   "Osc " #n " Phase",           0.f, 1.f, 0.f, 0.f, 0.f, Fm::Pct) \
    F (O##n##PhRnd,  "o" #n "_phrand",  "Osc " #n " Phase Random",    0.f, 1.f, oscDefs[n - 1].phRand, 0.f, 0.f, Fm::Pct) \
    B (O##n##Retrig, "o" #n "_retrig",  "Osc " #n " Retrigger",       true) \
    F (O##n##Voices, "o" #n "_voices",  "Osc " #n " Unison Voices",   1.f, 16.f, (float) oscDefs[n - 1].voices, 0.f, 1.f, Fm::Int) \
    F (O##n##Detune, "o" #n "_detune",  "Osc " #n " Unison Detune",   0.f, 100.f, oscDefs[n - 1].detune, 0.f, 0.f, Fm::Cents) \
    F (O##n##USpread,"o" #n "_uspread", "Osc " #n " Unison Spread",   0.f, 1.f, oscDefs[n - 1].spread, 0.f, 0.f, Fm::Pct) \
    F (O##n##UMix,   "o" #n "_umix",    "Osc " #n " Unison Mix",      0.f, 1.f, oscDefs[n - 1].mix, 0.f, 0.f, Fm::Pct)

#define TUNE_ENV(F, C, B, p, P, N, a, h, d, s, r, c) \
    F (P##A,     p "_a",     N " Attack",  0.f, 5000.f, a, 150.f, 0.f, Fm::Ms) \
    F (P##H,     p "_h",     N " Hold",    0.f, 2000.f, h, 150.f, 0.f, Fm::Ms) \
    F (P##D,     p "_d",     N " Decay",   1.f, 8000.f, d, 400.f, 0.f, Fm::Ms) \
    F (P##S,     p "_s",     N " Sustain", 0.f, 1.f, s, 0.f, 0.f, Fm::Pct) \
    F (P##R,     p "_r",     N " Release", 1.f, 8000.f, r, 400.f, 0.f, Fm::Ms) \
    F (P##Curve, p "_curve", N " Curve",   -1.f, 1.f, c, 0.f, 0.f, Fm::Bip)

#define TUNE_LFO(F, C, B, n) \
    C (L##n##Shape,  "lfo" #n "_shape",  "LFO " #n " Shape",    Choices::lfoShapes, 0) \
    F (L##n##Rate,   "lfo" #n "_rate",   "LFO " #n " Rate",     0.02f, 40.f, 2.f, 3.f, 0.f, Fm::LfoHz) \
    B (L##n##Sync,   "lfo" #n "_sync",   "LFO " #n " Sync",     false) \
    C (L##n##Div,    "lfo" #n "_div",    "LFO " #n " Division", Choices::syncDivs(), 9) \
    F (L##n##Depth,  "lfo" #n "_depth",  "LFO " #n " Depth",    0.f, 1.f, 1.f, 0.f, 0.f, Fm::Pct) \
    F (L##n##Phase,  "lfo" #n "_phase",  "LFO " #n " Phase",    0.f, 1.f, 0.f, 0.f, 0.f, Fm::Pct) \
    F (L##n##Fade,   "lfo" #n "_fade",   "LFO " #n " Fade In",  0.f, 5000.f, 0.f, 400.f, 0.f, Fm::Ms) \
    B (L##n##Retrig, "lfo" #n "_retrig", "LFO " #n " Retrigger", true)

#define TUNE_SLOT(F, C, B, n, src, dst, amt) \
    C (M##n##Src, "mm" #n "_src", "Mod " #n " Source",      Choices::modSources, src) \
    C (M##n##Dst, "mm" #n "_dst", "Mod " #n " Destination", Choices::modDests, dst) \
    F (M##n##Amt, "mm" #n "_amt", "Mod " #n " Amount",      -1.f, 1.f, amt, 0.f, 0.f, Fm::Bip)

#define TUNE_PARAMS(F, C, B) \
    TUNE_OSC (F, C, B, 1) TUNE_OSC (F, C, B, 2) TUNE_OSC (F, C, B, 3) TUNE_OSC (F, C, B, 4) \
    F (GaVol,   "ga_vol",   "Group A Volume", -48.f, 6.f, 0.f, 0.f, 0.f, Fm::Db) \
    F (GaPan,   "ga_pan",   "Group A Pan",    -1.f, 1.f, 0.f, 0.f, 0.f, Fm::Pan) \
    F (GaWidth, "ga_width", "Group A Width",  0.f, 2.f, 0.8f, 0.f, 0.f, Fm::Pct) \
    F (GbVol,   "gb_vol",   "Group B Volume", -48.f, 6.f, -2.f, 0.f, 0.f, Fm::Db) \
    F (GbPan,   "gb_pan",   "Group B Pan",    -1.f, 1.f, 0.f, 0.f, 0.f, Fm::Pan) \
    F (GbWidth, "gb_width", "Group B Width",  0.f, 2.f, 1.2f, 0.f, 0.f, Fm::Pct) \
    C (FltType,  "flt_type",  "Filter Type",        Choices::filters, 1) \
    F (FltCut,   "flt_cut",   "Filter Cutoff",      20.f, 20000.f, 1400.f, 1000.f, 0.f, Fm::Hz) \
    F (FltRes,   "flt_res",   "Filter Resonance",   0.f, 1.f, 0.18f, 0.f, 0.f, Fm::Pct) \
    F (FltDrive, "flt_drive", "Filter Drive",       0.f, 24.f, 3.f, 0.f, 0.f, Fm::Db) \
    F (FltKey,   "flt_key",   "Filter Key Track",   0.f, 1.f, 0.5f, 0.f, 0.f, Fm::Pct) \
    F (FltEnv,   "flt_env",   "Filter Env Amount",  -8.f, 8.f, 3.5f, 0.f, 0.f, Fm::Oct) \
    C (FltRoute, "flt_route", "Filter Route",       Choices::routes, 0) \
    TUNE_ENV (F, C, B, "fe", Fe, "Filter Env", 0.5f, 0.f, 240.f, 0.22f, 300.f, 0.5f) \
    TUNE_ENV (F, C, B, "ae", Ae, "Amp Env", 1.f, 0.f, 650.f, 0.4f, 220.f, 0.4f) \
    TUNE_ENV (F, C, B, "me", Me, "Mod Env", 0.f, 0.f, 400.f, 0.f, 300.f, 0.5f) \
    TUNE_LFO (F, C, B, 1) TUNE_LFO (F, C, B, 2) \
    TUNE_SLOT (F, C, B, 1, 6, 14, 0.3f)  TUNE_SLOT (F, C, B, 2, 7, 9, 0.5f)  TUNE_SLOT (F, C, B, 3, 0, 0, 0.f)  TUNE_SLOT (F, C, B, 4, 0, 0, 0.f) \
    TUNE_SLOT (F, C, B, 5, 0, 0, 0.f)    TUNE_SLOT (F, C, B, 6, 0, 0, 0.f)  TUNE_SLOT (F, C, B, 7, 0, 0, 0.f)  TUNE_SLOT (F, C, B, 8, 0, 0, 0.f) \
    TUNE_SLOT (F, C, B, 9, 0, 0, 0.f)    TUNE_SLOT (F, C, B, 10, 0, 0, 0.f) TUNE_SLOT (F, C, B, 11, 0, 0, 0.f) TUNE_SLOT (F, C, B, 12, 0, 0, 0.f) \
    TUNE_SLOT (F, C, B, 13, 0, 0, 0.f)   TUNE_SLOT (F, C, B, 14, 0, 0, 0.f) TUNE_SLOT (F, C, B, 15, 0, 0, 0.f) TUNE_SLOT (F, C, B, 16, 0, 0, 0.f) \
    F (ScrAmt,   "scr_amt",   "Scream",            0.f, 1.f, 0.12f, 0.f, 0.f, Fm::Pct) \
    F (ScrDrive, "scr_drive", "Scream Drive",      0.f, 24.f, 8.f, 0.f, 0.f, Fm::Db) \
    F (ScrSat,   "scr_sat",   "Scream Saturation", 0.f, 1.f, 0.4f, 0.f, 0.f, Fm::Pct) \
    F (ScrTone,  "scr_tone",  "Scream Tone",       -1.f, 1.f, 0.f, 0.f, 0.f, Fm::Bip) \
    F (ScrBite,  "scr_bite",  "Scream Bite",       0.f, 1.f, 0.4f, 0.f, 0.f, Fm::Pct) \
    F (ScrBody,  "scr_body",  "Scream Body",       0.f, 1.f, 0.5f, 0.f, 0.f, Fm::Pct) \
    F (ScrMix,   "scr_mix",   "Scream Mix",        0.f, 1.f, 1.f, 0.f, 0.f, Fm::Pct) \
    F (ScrOut,   "scr_out",   "Scream Output",     -12.f, 12.f, 0.f, 0.f, 0.f, Fm::Db) \
    C (ScrOs,    "scr_os",    "Scream Quality",    Choices::screamOs, 1) \
    F (Fortune,  "fortune",   "FORTUNE",           0.f, 1.f, 0.f, 0.f, 0.f, Fm::Pct) \
    F (FtCut,    "ft_cut",    "Fortune > Cutoff",      0.f, 1.f, 1.f, 0.f, 0.f, Fm::Pct) \
    F (FtDet,    "ft_det",    "Fortune > Detune",      0.f, 1.f, 1.f, 0.f, 0.f, Fm::Pct) \
    F (FtWidth,  "ft_width",  "Fortune > Width",       0.f, 1.f, 1.f, 0.f, 0.f, Fm::Pct) \
    F (FtScr,    "ft_scr",    "Fortune > Scream",      0.f, 1.f, 1.f, 0.f, 0.f, Fm::Pct) \
    F (FtRMix,   "ft_rmix",   "Fortune > Reverb Mix",  0.f, 1.f, 1.f, 0.f, 0.f, Fm::Pct) \
    F (FtRSize,  "ft_rsize",  "Fortune > Reverb Size", 0.f, 1.f, 1.f, 0.f, 0.f, Fm::Pct) \
    B (EqOn,      "eq_on",      "EQ On",            true) \
    F (EqLow,     "eq_low",     "EQ Low",           -12.f, 12.f, 0.f, 0.f, 0.f, Fm::Db) \
    F (EqLowMid,  "eq_lowmid",  "EQ Low Mid",       -12.f, 12.f, 0.f, 0.f, 0.f, Fm::Db) \
    F (EqHighMid, "eq_highmid", "EQ High Mid",      -12.f, 12.f, 0.f, 0.f, 0.f, Fm::Db) \
    F (EqHigh,    "eq_high",    "EQ High",          -12.f, 12.f, 0.f, 0.f, 0.f, Fm::Db) \
    B (DistOn,    "dist_on",    "Distortion On",    false) \
    F (DistDrive, "dist_drive", "Distortion Drive", 0.f, 36.f, 9.f, 0.f, 0.f, Fm::Db) \
    F (DistTone,  "dist_tone",  "Distortion Tone",  -1.f, 1.f, 0.f, 0.f, 0.f, Fm::Bip) \
    F (DistMix,   "dist_mix",   "Distortion Mix",   0.f, 1.f, 0.5f, 0.f, 0.f, Fm::Pct) \
    F (DistOut,   "dist_out",   "Distortion Output", -12.f, 12.f, 0.f, 0.f, 0.f, Fm::Db) \
    B (ChoOn,     "cho_on",     "Chorus On",        false) \
    F (ChoRate,   "cho_rate",   "Chorus Rate",      0.05f, 8.f, 0.6f, 1.f, 0.f, Fm::LfoHz) \
    F (ChoDepth,  "cho_depth",  "Chorus Depth",     0.f, 1.f, 0.4f, 0.f, 0.f, Fm::Pct) \
    F (ChoWidth,  "cho_width",  "Chorus Width",     0.f, 1.f, 0.8f, 0.f, 0.f, Fm::Pct) \
    F (ChoMix,    "cho_mix",    "Chorus Mix",       0.f, 1.f, 0.3f, 0.f, 0.f, Fm::Pct) \
    B (DlyOn,     "dly_on",     "Delay On",         true) \
    C (DlyDiv,    "dly_div",    "Delay Time",       Choices::syncDivs(), 7) \
    F (DlyFb,     "dly_fb",     "Delay Feedback",   0.f, 0.95f, 0.35f, 0.f, 0.f, Fm::Pct) \
    B (DlyPP,     "dly_pp",     "Delay Ping Pong",  true) \
    F (DlyFilter, "dly_filter", "Delay Filter",     500.f, 16000.f, 5000.f, 3000.f, 0.f, Fm::Hz) \
    F (DlyMix,    "dly_mix",    "Delay Mix",        0.f, 1.f, 0.14f, 0.f, 0.f, Fm::Pct) \
    B (RvbOn,     "rvb_on",     "Reverb On",        true) \
    F (RvbPre,    "rvb_pre",    "Reverb Pre-Delay", 0.f, 250.f, 15.f, 50.f, 0.f, Fm::Ms) \
    F (RvbSize,   "rvb_size",   "Reverb Size",      0.f, 1.f, 0.55f, 0.f, 0.f, Fm::Pct) \
    F (RvbDecay,  "rvb_decay",  "Reverb Decay",     0.2f, 12.f, 2.2f, 2.5f, 0.f, Fm::Sec) \
    F (RvbDamp,   "rvb_damp",   "Reverb Damping",   1000.f, 16000.f, 7000.f, 6000.f, 0.f, Fm::Hz) \
    F (RvbWidth,  "rvb_width",  "Reverb Width",     0.f, 1.f, 1.f, 0.f, 0.f, Fm::Pct) \
    F (RvbMix,    "rvb_mix",    "Reverb Mix",       0.f, 1.f, 0.16f, 0.f, 0.f, Fm::Pct) \
    F (StWidth,   "st_width",   "Stereo Width",     0.f, 2.f, 1.f, 0.f, 0.f, Fm::Pct) \
    F (StBalance, "st_balance", "Stereo Balance",   -1.f, 1.f, 0.f, 0.f, 0.f, Fm::Pan) \
    F (StMonoBass,"st_monobass","Mono Bass",        0.f, 400.f, 120.f, 120.f, 0.f, Fm::Hz) \
    B (LimOn,     "lim_on",     "Limiter On",       true) \
    F (LimInput,  "lim_input",  "Limiter Input",    0.f, 18.f, 0.f, 0.f, 0.f, Fm::Db) \
    F (LimCeiling,"lim_ceiling","Limiter Ceiling",  -12.f, 0.f, -0.3f, 0.f, 0.f, Fm::Db) \
    F (LimRelease,"lim_release","Limiter Release",  10.f, 500.f, 80.f, 100.f, 0.f, Fm::Ms) \
    C (PlayMode,  "play_mode",  "Play Mode",        Choices::playModes, 0) \
    F (PolyVoices,"poly_voices","Polyphony",        1.f, 16.f, 12.f, 0.f, 1.f, Fm::Int) \
    F (Glide,     "glide",      "Glide",            0.f, 2000.f, 0.f, 200.f, 0.f, Fm::Ms) \
    F (BendRange, "bend_range", "Pitch Bend Range", 0.f, 24.f, 2.f, 0.f, 1.f, Fm::Semi) \
    F (VelSens,   "vel_sens",   "Velocity",         0.f, 1.f, 0.4f, 0.f, 0.f, Fm::Pct) \
    F (Transpose, "transpose",  "Transpose",        -24.f, 24.f, 0.f, 0.f, 1.f, Fm::Semi) \
    F (Master,    "master",     "Master Volume",    -36.f, 6.f, 0.f, 0.f, 0.f, Fm::Db) \
    C (DistOs,    "dist_os",    "Distortion Quality", Choices::screamOs, 1) \
    C (FxOrder,   "fx_order",   "FX Order",         Choices::fxOrders, 0)

    namespace P
    {
#define TUNE_ENUM_F(e, ...) e,
#define TUNE_ENUM_C(e, ...) e,
#define TUNE_ENUM_B(e, ...) e,
        enum Id : int { TUNE_PARAMS (TUNE_ENUM_F, TUNE_ENUM_C, TUNE_ENUM_B) count };
#undef TUNE_ENUM_F
#undef TUNE_ENUM_C
#undef TUNE_ENUM_B
    }

    inline const std::array<const char*, P::count>& paramIds()
    {
#define TUNE_ID_F(e, id, ...) id,
#define TUNE_ID_C(e, id, ...) id,
#define TUNE_ID_B(e, id, ...) id,
        static const std::array<const char*, P::count> ids { TUNE_PARAMS (TUNE_ID_F, TUNE_ID_C, TUNE_ID_B) };
#undef TUNE_ID_F
#undef TUNE_ID_C
#undef TUNE_ID_B
        return ids;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();   // Params.cpp
    juce::String formatValue (Fm, float);
}
