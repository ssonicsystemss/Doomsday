#pragma once
// =====================================================================
//  DOOMSDAY shared: tempo-locked REVERSE BASS between kicks (namespace doomsday::rbass)
//  Taken from Damian's dkd / rutherford engine (ssonicsystemss/dkd, 2026-10-22: Source/RutherfordEngine.h), as a block.
//
//  The classic hardstyle reverse bass without a sample: in every step (1/4, 1/8 or 1/16 of the beat) the bass swells
//  from IN to OUT (fractions of the step) with amplitude x^CURVE, the pitch rises by RISE semitones in the last 40 %
//  of the swell (a reversed kick tail), then it is silent until the next kick. Short fades (1 / 1.5 ms) at both ends.
//  Saw (PolyBLEP) + sine on the bass note. Drive / tone / level are left to the plugin (doomsday::drive, filters).
//
//     rbass::Params p;  p.stepBeats = 1.0 (1/4) / 0.5 / 0.25;  p.in = 0.38; p.out = 0.95; p.curve = 2.6; p.riseSt = 12;
//     rb.prepare (sampleRate);  rb.setNoteHz (hz);
//     per sample: y = rb.next (p, clock.now(), clock.tempo(), keyHeld)        (clock = host::BeatClock)
//
//  swell (p, phaseInStep, stepSeconds, riseU) is the same function the GUI uses to draw the lane.
//  Kick tail that follows the tempo (dkd): tailLenMs (tail0to1, beatMs) / tailDecayMs (...)
//  Real-time safe.
// =====================================================================
#include <juce_core/juce_core.h>
#include <cmath>

namespace doomsday::rbass
{
    struct Params
    {
        double stepBeats = 1.0;      // 1 = 1/4, 0.5 = 1/8, 0.25 = 1/16
        float in = 0.38f;            // swell start, fraction of the step
        float out = 0.95f;           // swell end, fraction of the step (the rest = the gap before the next kick)
        float curve = 2.6f;          // amplitude = x^curve (higher = later, harder swell)
        float riseSt = 12.f;         // pitch rise at the end of the swell
        float saw = 0.45f, sine = 0.7f;
    };

    // amplitude 0..1 at s (0..1 of a step lasting stepSec); riseU = 0..1 how far into the pitch rise
    inline float swell (const Params& p, double s, double stepSec, float& riseU)
    {
        riseU = 0.f;
        if (p.out <= p.in + 0.01f || s < p.in || s >= p.out) return 0.f;
        const double r = (s - p.in) / (p.out - p.in);
        const double fade = juce::jmin (1.0, (p.out - s) * stepSec / 0.0015) * juce::jmin (1.0, (s - p.in) * stepSec / 0.001);
        const double rr = juce::jmax (0.0, (r - 0.6) / 0.4);
        riseU = (float) (rr * rr);
        return (float) (std::pow (r, (double) p.curve) * fade);
    }

    // a kick tail measured in beats (it follows the tempo): tail = 0..1 of a beat
    inline double tailLenMs (float tail, double beatMs)   { return tail * beatMs * 0.72; }
    inline double tailDecayMs (float tail, double beatMs) { return juce::jmax (3.0, tail * beatMs * 0.07); }

    class ReverseBass
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            ampCoef = (float) (1.0 - std::exp (-1.0 / (0.0005 * sr)));     // 0.5 ms smoothing of the swell
            reset();
        }
        void reset() { amp = 0.f; phase = 0.0; }
        void setNoteHz (double hz) { noteHz = juce::jmax (10.0, hz); }
        bool sounding() const { return amp > 1.0e-5f; }

        // beat = the clock position now (beats), held = a key is down (otherwise the swell dies out)
        float next (const Params& p, double beat, double bpm, bool held)
        {
            float target = 0.f, riseU = 0.f;
            if (held)
            {
                const double q = beat / p.stepBeats;
                target = swell (p, q - std::floor (q), p.stepBeats * 60.0 / bpm, riseU);
            }
            amp += (target - amp) * ampCoef;
            if (amp <= 1.0e-5f) return 0.f;
            const double f = noteHz * std::pow (2.0, p.riseSt * riseU / 12.0), dt = f / sr;
            phase += dt;
            if (phase >= 1.0) phase -= 1.0;
            const double saw = 2.0 * phase - 1.0 - blep (phase, dt);
            return (float) ((p.saw * saw + p.sine * std::sin (phase * juce::MathConstants<double>::twoPi)) * amp);
        }

    private:
        static double blep (double t, double dt)
        {
            if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
            if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
            return 0.0;
        }
        double sr = 48000.0, noteHz = 43.65, phase = 0.0;
        float amp = 0.f, ampCoef = 0.04f;
    };
}
