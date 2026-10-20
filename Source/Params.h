#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// ---------------------------------------------------------------------
//  Parameter IDs
//  OSC 1 keeps the v1.0 IDs ("wave", "cutoff", ...) so projects saved
//  with v1.0 load with the same sound. OSC 2 uses the "o2_" prefix.
// ---------------------------------------------------------------------
namespace ParamIDs
{
    constexpr int numOscs = 2;

    // per-oscillator bases ---------------------------------------------
    // oscillator
    inline constexpr const char* on       = "on";        // new in v2
    inline constexpr const char* level    = "level";     // new in v2
    inline constexpr const char* oct      = "oct";       // new in v2
    inline constexpr const char* semi     = "semi";      // new in v2
    inline constexpr const char* fine     = "fine";      // new in v2
    inline constexpr const char* wave     = "wave";
    inline constexpr const char* unison   = "unison";
    inline constexpr const char* detune   = "detune";
    inline constexpr const char* width    = "width";
    inline constexpr const char* fm       = "fm";
    inline constexpr const char* fmRatio  = "fmratio";
    inline constexpr const char* penv     = "penv";
    inline constexpr const char* pdecay   = "pdecay";
    // distortion tab
    inline constexpr const char* distOn   = "dist_on";   // new in v2
    inline constexpr const char* dtype    = "dtype";
    inline constexpr const char* drive    = "drive";
    inline constexpr const char* bite     = "bite";
    inline constexpr const char* dmix     = "dmix";
    // filter tab
    inline constexpr const char* filtOn   = "filt_on";   // new in v2
    inline constexpr const char* ftype    = "ftype";
    inline constexpr const char* cutoff   = "cutoff";
    inline constexpr const char* reso     = "reso";
    inline constexpr const char* fenv     = "fenv";
    inline constexpr const char* fdecay   = "fdecay";
    inline constexpr const char* keytrack = "keytrack";
    // lfo tab
    inline constexpr const char* lfoOn    = "lfo_on";    // new in v2
    inline constexpr const char* lshape   = "lshape";
    inline constexpr const char* lsync    = "lsync";
    inline constexpr const char* lrate    = "lrate";
    inline constexpr const char* lfoDiv   = "lfoDiv";
    inline constexpr const char* lcut     = "lcut";
    inline constexpr const char* lpitch   = "lpitch";

    // v3 (all neutral by default, so older projects sound the same)
    inline constexpr const char* noise    = "noise";     // DIST tab: noise before the distortion
    inline constexpr const char* lamp     = "lamp";      // LFO tab: LFO > AMP
    inline constexpr const char* formOn   = "form_on";   // FORMANT tab
    inline constexpr const char* formHz   = "form_hz";
    inline constexpr const char* formGain = "form_gain";
    inline constexpr const char* formQ    = "form_q";
    inline constexpr const char* formEnv  = "form_env";
    inline constexpr const char* formDec  = "form_dec";
    inline constexpr const char* formKey  = "form_key";
    inline constexpr const char* formLfo  = "form_lfo";

    inline bool isNewInV3 (const juce::String& base)
    {
        return base == noise || base == lamp || base.startsWith ("form_");
    }

    inline bool isNewInV2 (const juce::String& base)
    {
        return base == on || base == level || base == oct || base == semi || base == fine
            || base == distOn || base == filtOn || base == lfoOn || isNewInV3 (base);
    }

    // Full ID of a per-oscillator parameter (osc = 0 or 1)
    inline juce::String osc (int o, const juce::String& base)
    {
        if (o == 0) return isNewInV2 (base) ? "o1_" + base : base;
        return "o2_" + base;
    }

    // global ----------------------------------------------------------
    inline constexpr const char* glide    = "glide";
    inline constexpr const char* bend     = "bend";
    inline constexpr const char* legato   = "legato";
    inline constexpr const char* scream   = "scream";
    inline constexpr const char* attack   = "attack";
    inline constexpr const char* decay    = "decay";
    inline constexpr const char* sustain  = "sustain";
    inline constexpr const char* release  = "release";
    inline constexpr const char* dlyOn    = "dly_on";    // new in v2
    inline constexpr const char* dtime    = "dtime";
    inline constexpr const char* dfb      = "dfb";
    inline constexpr const char* dfx      = "dfx";
    inline constexpr const char* pingpong = "pingpong";
    inline constexpr const char* revOn    = "rev_on";    // new in v2
    inline constexpr const char* rsize    = "rsize";
    inline constexpr const char* rdamp    = "rdamp";
    inline constexpr const char* rmix     = "rmix";
    inline constexpr const char* volume   = "volume";
    // v3 globals
    inline constexpr const char* sweep    = "sweep";
    inline constexpr const char* sweepTime= "sweep_time";
    inline constexpr const char* ott      = "ott";
    inline constexpr const char* spread   = "spread";
}

namespace ParamChoices
{
    inline const juce::StringArray fmRatios   { "0.5", "1", "1.5", "2", "3", "4", "5", "7" };
    inline const float fmRatioValues[]        { 0.5f, 1.f, 1.5f, 2.f, 3.f, 4.f, 5.f, 7.f };

    inline const juce::StringArray filterTypes { "LP 12", "LP 24", "BAND", "HIGH" };
    inline const juce::StringArray distTypes   { "HARD", "TUBE", "FOLD", "SCREAM" };
    inline const juce::StringArray lfoShapes   { "SINE", "TRI", "SAW", "SQUARE", "S&H" };

    inline const juce::StringArray divisions   { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                                                 "1/4T", "1/4", "1/4D", "1/2", "1/1", "2/1" };
    inline const float divisionBeats[]         { 0.125f, 1.f / 6.f, 0.25f, 0.375f, 1.f / 3.f, 0.5f, 0.75f,
                                                 2.f / 3.f, 1.f, 1.5f, 2.f, 4.f, 8.f };
}
