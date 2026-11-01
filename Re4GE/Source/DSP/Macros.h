#pragma once
// RE4GE - the 8 macros on fixed targets (step 1; in step 4 they become sources of the MOD MATRIX).
// They work ON TOP of the knobs: 0 = exactly the modules as set. Every macro moves several things that belong together.
//   BODY      +6 dB bell at 220 Hz (the 150-400 Hz 'meat' the owner's samples have)
//   WEIGHT    SUB +6 dB and its crossover +40 Hz
//   REVERSE   the reverse-bass set: later, slower swell (CURVE -, SWELL x2.5), a rise of +7 st into the kick, harmonics opening late
//             (DRIVE ENV -> 100 %, PRE FILTER ENV +2 oct)
//   PITCH     a drop of +12 st at the start of every step (bounce)
//   GRIT      DRIVE +18 dB, STAGE 2 +12 dB
//   BITE      +8 dB bell at 2 kHz (the owner's presence) and HIGH CUT up to x4
//   MOVEMENT  UNISON detune +25 ct, drift +12 ct (unison on if it was off)
//   SPACE     WIDTH +0.6, MONO BELOW down to x0.6 (unison on if it was off)
#include "ReParams.h"

namespace re
{
    inline Params applyMacros (Params p)
    {
        const auto& m = p.macro;
        if (m[0] > 0.f) p.bodyDb = 6.f * m[0];
        if (m[1] > 0.f) { p.subDb = juce::jmin (6.f, (p.subDb <= -59.f ? -12.f : p.subDb) + 6.f * m[1]); p.subHz = juce::jmin (250.f, juce::jmax (p.subHz, 60.f) + 40.f * m[1]); }
        if (m[2] > 0.f)
        {
            p.curve = juce::jmax (-1.f, p.curve - 1.2f * m[2]);
            p.swellMs = juce::jmin (400.f, p.swellMs * (1.f + 1.5f * m[2]));
            p.extraRiseSt = 7.f * m[2];
            p.driveEnv += m[2] * (1.f - p.driveEnv);
            p.preEnvOct = juce::jlimit (-4.f, 4.f, p.preEnvOct + 2.f * m[2]);
        }
        if (m[3] > 0.f) p.extraDropSt = 12.f * m[3];
        if (m[4] > 0.f) { p.driveDb = juce::jmin (48.f, p.driveDb + 18.f * m[4]); p.dist2Db = juce::jmin (40.f, p.dist2Db + 12.f * m[4]); }
        if (m[5] > 0.f) { p.biteDb = 8.f * m[5]; p.highCutHz = juce::jmin (20000.f, p.highCutHz * (1.f + 3.f * m[5])); }
        if (m[6] > 0.f || m[7] > 0.f) p.uniVoices = juce::jmax (p.uniVoices, 3);
        if (m[6] > 0.f) { p.uniDetuneCt = juce::jmin (80.f, p.uniDetuneCt + 25.f * m[6]); p.driftCt = juce::jmin (40.f, p.driftCt + 12.f * m[6]); }
        if (m[7] > 0.f) { p.stereoWidth = juce::jmin (1.f, p.stereoWidth + 0.6f * m[7]); p.splitHz = juce::jmax (80.f, p.splitHz * (1.f - 0.4f * m[7])); }
        return p;
    }
}
