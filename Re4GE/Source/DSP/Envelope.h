#pragma once
// RE4GE - the envelope and the pitch curve as pure functions of time (the engine plays them, the GUI draws them)
//   ENVELOPE: DELAY (DUCK level before it) -> INITIAL -> SWELL ms + CURVE -> HOLD -> DECAY to SUSTAIN (DECAY CURVE) -> GATE end with FADE.
//   TIMING SYNC = delay / hold / decay / gate in % of a step, MS = in ms. PUMP / SWELL / TIGHT / ROLL / PULSE are settings of it.
//   PITCH: STABLE / DROP / RISE / BOUNCE (AMOUNT, TIME), in semitones over the note.
#include "ReParams.h"
#include <cmath>

namespace re
{
    // t ms from the step start, stepMs = length of one step, gateEnd in ms
    struct Times { double delay, hold, decay, gateEnd; };
    inline Times times (const Params& p, double stepMs)
    {
        Times t;
        if (p.sync) { t.delay = p.delayPct * 0.01 * stepMs; t.hold = p.holdPct * 0.01 * stepMs; t.decay = p.decayPct * 0.01 * stepMs; t.gateEnd = p.gatePct * 0.01 * stepMs; }
        else        { t.delay = p.delayMs; t.hold = p.holdMs; t.decay = p.decayMs; t.gateEnd = p.gateMs; }
        if (! p.gate) t.gateEnd = juce::jmin (t.gateEnd, stepMs - 0.5);
        return t;
    }
    inline double swellShape (double x, float curve)
    {
        x = juce::jlimit (0.0, 1.0, x);
        if (curve > 0.001f) return 1.0 - std::pow (1.0 - x, 1.0 + 4.0 * curve);      // fast, then landing (sidechain release)
        if (curve < -0.001f) return std::pow (x, 1.0 - 4.0 * curve);                  // slow start, late swell
        return x;
    }
    // level 0..1 before the gate fade
    inline double envAt (const Params& p, const Times& tm, double t)
    {
        if (t < tm.delay) return p.duck;
        const double u = t - tm.delay;
        if (u < p.swellMs) return p.initial + (1.0 - p.initial) * swellShape (u / juce::jmax (0.5f, p.swellMs), p.curve);
        const double v = u - p.swellMs - tm.hold;
        if (v < 0.0 || tm.decay < 0.5) return 1.0;
        const double d = juce::jlimit (0.0, 1.0, v / tm.decay);
        return p.sustain + (1.0 - p.sustain) * std::pow (1.0 - d, (double) p.decayCurve);
    }
    inline double gateAt (const Params& p, const Times& tm, double t)       // PATTERN: cosine fade that ends exactly on gateEnd
    {
        const double f = juce::jlimit (0.0, 1.0, (tm.gateEnd - t) / juce::jmax (0.5f, p.fadeMs));
        return 0.5 - 0.5 * std::cos (f * juce::MathConstants<double>::pi);
    }
    inline double pitchAt (const Params& p, const Times& tm, double t)      // semitones over the note
    {
        const double T = juce::jmax (1.f, p.pitchMs);
        double st = 0.0;
        if (p.pitchMode == Drop || p.pitchMode == Bounce) st += p.pitchSt * std::exp (-3.0 * juce::jmax (0.0, t - tm.delay) / T);
        if (p.pitchMode == Rise || p.pitchMode == Bounce)
        {
            const double startAt = p.gate ? tm.delay : tm.gateEnd - T;
            const double r = juce::jlimit (0.0, 1.0, (t - startAt) / T);
            st += (p.pitchMode == Bounce ? 0.5 : 1.0) * p.pitchSt * r * r;
        }
        if (p.extraDropSt != 0.f) st += p.extraDropSt * std::exp (-3.0 * juce::jmax (0.0, t - tm.delay) / T);          // PITCH macro
        if (p.extraRiseSt != 0.f)                                                                                         // REVERSE macro: a rise into the kick
        {
            const double R = juce::jmax (60.0, T), startAt = p.gate ? tm.delay : tm.gateEnd - R;
            const double r = juce::jlimit (0.0, 1.0, (t - startAt) / R);
            st += p.extraRiseSt * r * r;
        }
        return st;
    }
}
