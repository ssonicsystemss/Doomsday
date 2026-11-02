#pragma once
// RE4GE - TRIGGER: where we are in the note, per sample
//   PATTERN : a note starts BEATS beats of steps (1/4, 1/8, 1/16) on the host tempo, MASK picks the steps, OFFSET shifts them
//   GATE    : a note is one bass hit, note-off starts the FADE
//   SEQUENCER (v0.12, PATTERN only): the steps are counted on the host's grid (a note on beat 3 starts at step 8 of 1/16),
//             step a of the song uses SEQ step a % LENGTH: ON / LEVEL / PITCH; SWING moves every 2nd step later (its envelope is
//             squeezed into the rest of the step, so it still ends before the next one)
// It only tells the time, the envelope target, the pitch offset and the gate; the engine reacts to a new step.
#include "Envelope.h"

namespace re
{
    class Trigger
    {
    public:
        struct Tick { double target = 0.0, st = 0.0, gate = 0.0, t = -1.0; bool newStep = false, plays = false; };

        // base = the song step the note starts on (host grid; -1 = unknown: the steps run on from the last note)
        void noteOn (int note, bool audition, long long base = -1)
        {
            held = note; noteOffAt = -1.0; auditionOff = audition; tNote = 0.0; step = -1; active = true;
            baseStep = base >= 0 ? base : nextStep;
        }
        void noteOff (int note) { if (note == held) noteOffAt = tNote; }
        void stop() { active = false; }
        void resetSong() { nextStep = 0; seqNow.store (-1); }
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
                k.target = envAt (p, tm, tNote) * g; k.t = tNote; k.plays = true;
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
                    if (s != step) { step = s; k.newStep = true; nextStep = baseStep + s + 1; }
                    double t = tt - s * stepMs;
                    const long long a = baseStep + s;
                    bool on = ((p.mask >> (s % stepsPerBeat)) & 1) != 0;
                    float lvl = 1.f, pst = 0.f;
                    if (p.seqOn)
                    {
                        const auto& q = p.seq[(size_t) (a % juce::jmax (1, p.seqLen))];
                        on = q.on && q.vel > 0.001f; lvl = q.vel; pst = q.pitchSt;
                        seqNow.store ((int) (a % juce::jmax (1, p.seqLen)));
                        const double sw = swingMs (p);
                        if ((a & 1) && sw > 0.0) t = juce::jmax (0.0, t - sw) * stepMs / (stepMs - sw);
                    }
                    k.t = t;
                    if (on) { k.plays = true; k.gate = gateAt (p, tm, t); k.target = envAt (p, tm, t) * k.gate * lvl; k.st = pitchAt (p, tm, t) + pst; }
                }
            }
            return k;
        }
        void advance (double ms) { tNote += ms; }
        double swingMs (const Params& p) const { return p.seqOn ? juce::jlimit (0.0, 0.6, p.swing * 0.01) * stepMs * 0.5 : 0.0; }
        // the real time (ms in the step) when the current step's gate ends (later on a swung step)
        double gateEndNow (const Params& p) const
        {
            const double sw = ((baseStep + juce::jmax (0, step)) & 1) ? swingMs (p) : 0.0;
            return sw + tm.gateEnd * (stepMs - sw) / stepMs;
        }
        std::atomic<int> seqNow { -1 };         // the SEQ step playing (for the GUI)

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
        long long baseStep = 0, nextStep = 0;
        bool active = false, auditionOff = false;
    };
}
