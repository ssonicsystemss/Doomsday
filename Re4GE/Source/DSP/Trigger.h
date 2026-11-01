#pragma once
// RE4GE - TRIGGER: where we are in the note, per sample
//   PATTERN : a note starts BEATS beats of steps (1/4, 1/8, 1/16) on the host tempo, MASK picks the steps, OFFSET shifts them
//   GATE    : a note is one bass hit, note-off starts the FADE
// It only tells the time, the envelope target, the pitch offset and the gate; the engine reacts to a new step.
#include "Envelope.h"

namespace re
{
    class Trigger
    {
    public:
        struct Tick { double target = 0.0, st = 0.0, gate = 0.0; bool newStep = false; };

        void noteOn (int note, bool audition) { held = note; noteOffAt = -1.0; auditionOff = audition; tNote = 0.0; step = -1; active = true; }
        void noteOff (int note) { if (note == held) noteOffAt = tNote; }
        void stop() { active = false; }
        bool isActive() const { return active; }
        double timeMs() const { return tNote; }

        // per chunk: the step grid from the (latched) settings
        void cook (const Params& p, double stepMs_)
        {
            stepMs = stepMs_;
            tm = times (p, stepMs);
            stepsPerBeat = juce::jmax (1, (int) std::round (1.0 / p.stepBeats));
            totalSteps = stepsPerBeat * juce::jmax (1, p.beats);
            offMs = p.offsetPct * 0.01 * stepMs;
        }
        const Times& stepTimes() const { return tm; }

        // one sample (call only while active); advance() afterwards moves the clock
        inline Tick tick (const Params& p)
        {
            Tick k;
            if (p.gate)
            {
                if (step < 0) { step = 0; k.newStep = true; }
                double g = 1.0;
                if (noteOffAt >= 0.0) g = 0.5 + 0.5 * std::cos (juce::jmin (1.0, (tNote - noteOffAt) / juce::jmax (0.5f, p.fadeMs)) * juce::MathConstants<double>::pi);
                if (auditionOff && tNote > 400.0 && noteOffAt < 0.0) noteOffAt = tNote;
                k.target = envAt (p, tm, tNote) * g;
                k.st = pitchAt (p, tm, tNote);
                k.gate = g;
                if (noteOffAt >= 0.0 && tNote - noteOffAt > p.fadeMs + 1.0) active = false;
            }
            else
            {
                const double tt = tNote - offMs;
                const int s = tt < 0.0 ? -1 : (int) std::floor (tt / stepMs);
                if (s >= totalSteps) active = false;
                else if (s >= 0)
                {
                    if (s != step) { step = s; k.newStep = true; }
                    const double t = tt - s * stepMs;
                    if ((p.mask >> (s % stepsPerBeat)) & 1) { k.gate = gateAt (p, tm, t); k.target = envAt (p, tm, t) * k.gate; k.st = pitchAt (p, tm, t); }
                }
            }
            return k;
        }
        void advance (double ms) { tNote += ms; }

        // PATTERN: ms from now until the next step starts sounding (DELAY of the next played step); 0 = sounding now / GATE
        double msUntilAudible (const Params& p) const
        {
            if (p.gate || p.duck > 0.f) return 0.0;
            const double tt = tNote - offMs;
            const int s = tt < 0.0 ? 0 : (int) std::floor (tt / stepMs) + (tt - std::floor (tt / stepMs) * stepMs < tm.delay ? 0 : 1);
            return juce::jmax (0.0, offMs + s * stepMs + tm.delay - tNote);
        }

    private:
        Times tm {};
        double tNote = 0.0, noteOffAt = -1.0, stepMs = 400.0, offMs = 0.0;
        int held = -1, step = -1, stepsPerBeat = 1, totalSteps = 1;
        bool active = false, auditionOff = false;
    };
}
