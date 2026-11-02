#pragma once
// RE4GE FX - finds the kicks in an audio signal (the main input or the sidechain): a fast and a slow envelope;
// a hit = the fast one jumps over THRESHOLD and well over the slow one (SENSITIVITY), not sooner than HOLDOFF after the last hit.
// Pure DSP, real-time safe.
#include <cmath>
#include <juce_core/juce_core.h>

namespace re
{
    class HitDetector
    {
    public:
        void prepare (double rate) { sr = rate; aF = coef (0.3); rF = coef (25.0); aS = coef (30.0); rS = coef (250.0); reset(); }
        void reset() { fast = slow = 0.f; sinceHit = 1 << 30; armed = true; }
        void set (float thresholdDb, float sensitivity, float holdoffMs)
        {
            thr = std::pow (10.f, thresholdDb / 20.f);
            ratio = 1.3f + 3.0f * (1.f - juce::jlimit (0.f, 1.f, sensitivity));        // more sensitive = a smaller jump is enough
            hold = (int) (juce::jmax (5.f, holdoffMs) * 0.001 * sr);
        }
        // one sample of the detection signal; true = a kick starts here
        inline bool process (float x)
        {
            const float m = std::abs (x);
            fast += (m > fast ? aF : rF) * (m - fast);
            slow += (m > slow ? aS : rS) * (m - slow);
            ++sinceHit;
            const bool over = fast > thr && fast > slow * ratio;
            if (! over) { armed = true; return false; }                  // the jump is over (the slow envelope caught up): ready for the next
            if (armed && sinceHit >= hold) { armed = false; sinceHit = 0; return true; }
            return false;
        }
        float level() const { return fast; }
    private:
        float coef (double ms) const { return (float) (1.0 - std::exp (-1.0 / (ms * 0.001 * sr))); }
        double sr = 44100.0;
        float fast = 0.f, slow = 0.f, aF = 0.1f, rF = 0.01f, aS = 0.01f, rS = 0.001f, thr = 0.06f, ratio = 2.f;
        int hold = 5000, sinceHit = 1 << 30;
        bool armed = true;
    };
}
