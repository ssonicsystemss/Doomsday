#pragma once
#include <juce_core/juce_core.h>

namespace ParamIDs
{
    constexpr int numBands = 3;
    // per band: "b1_" (LOW), "b2_" (MID), "b3_" (HIGH) + key
    inline juce::String band (int b, const char* key) { return "b" + juce::String (b + 1) + "_" + key; }
    inline constexpr const char* on = "on";
    inline constexpr const char* solo = "solo";
    inline constexpr const char* pre = "pre";
    inline constexpr const char* thresh = "thresh";
    inline constexpr const char* ratio = "ratio";
    inline constexpr const char* up = "up";
    inline constexpr const char* knee = "knee";
    inline constexpr const char* attack = "att";
    inline constexpr const char* release = "rel";
    inline constexpr const char* sat = "sat";
    inline constexpr const char* width = "width";
    inline constexpr const char* post = "post";
    // global
    inline constexpr const char* xoLow = "xo_lo";
    inline constexpr const char* xoHigh = "xo_hi";
    inline constexpr const char* input = "input";
    inline constexpr const char* tilt = "tilt";
    inline constexpr const char* monoBass = "mono_bass";
    inline constexpr const char* clip = "clip";
    inline constexpr const char* limit = "limit";
    inline constexpr const char* ceiling = "ceiling";
    inline constexpr const char* limRel = "lim_rel";
    inline constexpr const char* match = "match";
    inline constexpr const char* target = "target";
}

namespace Choices
{
    inline const juce::StringArray bandNames { "LOW", "MID", "HIGH" };
    inline const juce::StringArray targets { "OFF", "-14", "-9", "-7", "-5" };   // OFF / streaming / loud / hardstyle / raw
    inline const float targetLufs[] { 0.f, -14.f, -9.f, -7.f, -5.f };
}
