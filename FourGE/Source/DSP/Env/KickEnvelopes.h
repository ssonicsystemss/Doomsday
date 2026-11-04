#pragma once
// =====================================================================
//  4GE MODULE 02 - the envelopes of one kick, cooked per block, evaluated per voice:
//   * PITCH per part (PUNCH / BODY / TAIL / SUB): an analytic mode (CLASSIC .. HARDSTYLE KICK) or the drawn CUSTOM curve,
//     DEPTH (semitones) x AMOUNT, TIME x TIME SCALE (x global scale x tempo sync), OFFSET, and the LINK system
//     (INDEPENDENT / FOLLOW: depth + offset + time / LINKED: the source's shape with own depth and time). Cycles are cut in cook().
//       tail  = END (note) x 2^(semis_tail / 12)        punch, body = tail pitch x 2^(semis / 12)        sub = note x 2^(semis_sub / 12)
//   * ENVELOPE SLOTS: AMP (all parts), FILTER (punch / body / tail), DRIVE (all), RESONANCE (body / tail):
//     ADSR or DRAWN, DELAY, CURVE, AMOUNT, POLARITY, TRIGGER (ONE SHOT / RETRIGGER / LEGATO), SYNC.
//  Pure DSP (no parameters, no GUI): the engine fills these structs from a ParamFrame + the envelope bank.
// =====================================================================
#include "MultiEnv.h"
#include "../Core/Blocks.h"

namespace fge::dsp
{
    enum LinkMode { Independent = 0, Follow, Linked };
    enum Trigger { OneShot = 0, Retrigger, Legato };
    constexpr int customMode = 8;

    struct PitchPart
    {
        bool on = true;
        int mode = 0;               // 0..7 analytic, 8 CUSTOM
        float curve = 0.3f;
        float depthSt = 0.f;        // semitones at E = 1 (already x AMOUNT)
        float timeMs = 80.f;        // the shape's time span (already x TIME SCALE x global scale x sync)
        float offsetMs = 0.f;
        int link = Independent, src = 2;
        float fDepth = 1.f, fOffsetSt = 0.f, fTime = 1.f;
        const env::Shape* custom = nullptr;
    };

    struct EnvSlot
    {
        bool on = false;
        bool drawn = false;
        EnvTimes adsr;              // ADSR mode: attack / decay / sustain / release, lengthMs = the part's length
        float decayK = 5.f;         // CURVE -> how fast the ADSR decay falls
        float delayMs = 0.f;
        float amount = 0.f;         // AMP: 0..1 depth; FILTER: -1..1 (x 6 octaves); DRIVE / RESO: -1..1
        bool bipolar = false;
        int trigger = OneShot;
        float releaseMs = 30.f;     // RETRIGGER: fade after the note-off
        float tension = 0.f;        // DRAWN: CURVE bends every segment a little
        const env::Shape* shape = nullptr;
    };

    // ADSR inside a length, with the decay curvature as a parameter (module 01's kickEnv generalised)
    inline float adsrEnv (const EnvTimes& e, float ms, float k)
    {
        if (ms >= e.lengthMs || ms < 0.f) return 0.f;
        const float att = juce::jmax (0.15f, e.attackMs);
        float v;
        if (ms < att) { const float x = ms / att; v = x * (2.f - x); }
        else
        {
            const float x = (ms - att) / juce::jmax (0.5f, e.decayMs);
            const float d = x >= 1.f ? 0.f : (std::exp (-k * x) - std::exp (-k)) / (1.f - std::exp (-k));
            v = e.sustain + (1.f - e.sustain) * d;
        }
        const float rel = juce::jmax (3.f, e.releaseMs);
        const float relStart = e.lengthMs - rel;
        if (ms > relStart) v *= 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * juce::jlimit (0.f, 1.f, (ms - relStart) / rel));
        return v;
    }

    // per voice: the clocks, cursors and note-off state of every envelope
    struct EnvRunState
    {
        int cursor = 0;
        bool released = false;
        float relAtMs = 0.f, relFrom = 0.f, last = 0.f;
        float clockOffsetMs = 0.f;   // LEGATO: continues the previous kick's envelope time
    };

    // value of one slot (0..1, or -1..1 bipolar) at the voice time ms
    inline float slotValue (const EnvSlot& s, EnvRunState& st, float ms)
    {
        const float t = ms + st.clockOffsetMs - s.delayMs;
        float v;
        if (t < 0.f) v = 0.f;
        else if (s.drawn && s.shape != nullptr)
        {
            const float len = juce::jmax (1.f, s.adsr.lengthMs);
            v = t >= len ? 0.f : juce::jlimit (0.f, 1.f, s.shape->eval (t / len, st.cursor));
            // a drawn amp shape ends with a 3 ms fade (no click when the last point is not 0)
            const float fadeIn = juce::jmin (1.f, t / 0.15f), fadeOut = juce::jlimit (0.f, 1.f, (len - t) / 3.f);
            v *= fadeIn * fadeOut;
        }
        else v = adsrEnv (s.adsr, t, s.decayK);
        if (st.released)
        {
            const float r = juce::jmax (3.f, s.releaseMs);
            const float x = juce::jlimit (0.f, 1.f, (ms - st.relAtMs) / r);
            v = st.relFrom * (0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * x));
        }
        st.last = v;
        return s.bipolar ? 2.f * v - 1.f : v;
    }
}
