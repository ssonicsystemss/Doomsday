#pragma once
// RE4GE - REVERSE SHAPE DESIGNER (signature feature 1): drawn curves on one time line, per step
//   AMP       the level (DEPTH = how much it replaces the knob envelope: 0 = knobs, 1 = only the drawing)
//   PITCH     semitones: 50 % = on the note, top = +RANGE, bottom = -RANGE (DEPTH = RANGE 0..24 st)
//   HARMONIC  how much the bass 'opens': scales the DRIVE and moves the PRE FILTER down by up to 4 octaves (DEPTH = how much)
//   PHASE     scales PHASE WARP and the OSC 2-4 PM (DEPTH = how much)
// Each lane: ON, LENGTH (% of the step), ANCHOR (START = from the step start, END = it ends on the gate end, the reverse way),
// ONE SHOT / LOOP. The curves are doomsday::mod::Shape (Shared), time 0..1000 = the lane length; stored in the state by the processor.
#include "ReParams.h"
#include "Envelope.h"

namespace re
{
    enum Lane { LaneAmp = 0, LanePitch = 1, LaneHarm = 2, LanePhase = 3 };

    struct LaneValues { double amp = 1.0, st = 0.0, harm = 1.0, phase = 1.0; };

    class ShapeLanes
    {
    public:
        void restart() { cur = {}; }
        static bool any (const Params& p) { for (auto& l : p.lane) if (l.on) return true; return false; }
        // t = ms in the step, gateEnd = where the step's sound ends, refMs = the step (PATTERN) or the gate (GATE mode)
        static inline double laneX (const Params::LaneSet& l, double t, double gateEnd, double refMs)
        {
            const double L = juce::jmax (1.0, l.lenPct * 0.01 * refMs);
            const double start = l.anchor == 1 ? gateEnd - L : 0.0;
            double x = (t - start) / L;
            if (l.loop) x -= std::floor (x);           // LOOP repeats the curve over the whole step (also before an END-anchored lane)
            return juce::jlimit (0.0, 1.0, x);
        }
        inline LaneValues eval (const Params& p, double t, double gateEnd, double refMs)
        {
            LaneValues v;
            for (int i = 0; i < 4; ++i)
            {
                const auto& l = p.lane[(size_t) i];
                if (! l.on) continue;
                const double y = l.shape.eval ((float) (1000.0 * laneX (l, t, gateEnd, refMs)), cur[(size_t) i]);
                switch (i)
                {
                    case LaneAmp:   v.amp = y; break;
                    case LanePitch: v.st = (y - 0.5) * 2.0 * l.depth; break;
                    case LaneHarm:  v.harm = 1.0 - l.depth + l.depth * y; break;
                    default:        v.phase = 1.0 - l.depth + l.depth * y; break;
                }
            }
            return v;
        }
    private:
        std::array<int, 4> cur {};
    };
}
