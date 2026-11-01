#pragma once
// RE4GE - SUB and STEREO
//   SUB    : a clean sine on the note replaces everything below SUB Hz (LR4: HP on the bass + LP on the sine, sums flat).
//            Never cut, never distorted - the 'pierdolnięcie'. TUNE's whole octaves are left out (TUNE +12 still has its sub on the note).
//   STEREO : the lows of the centre chain stay mono; above SPLIT the detuned left / right chains go in as mid / side scaled by WIDTH.
#include "ReParams.h"

namespace re
{
    class Sub
    {
    public:
        void clear() { ph = 0.0; y1 = {}; y2 = {}; s1 = {}; s2 = {}; }
        void resetPhase() { ph = 0.0; }
        void cook (const Params& p, double osr_) { osr = osr_; c = doomsday::dsp::svfCoefs (juce::jmax (20.f, p.subHz), 1.f / 0.707f, osr); g = p.subDb <= -59.f ? 0.f : std::pow (10.f, p.subDb / 20.f); on = p.subHz > 20.f; }
        // hz = the sub's note, ratio = the pitch movement, amp = the envelope
        inline float process (float centre, double hz, double ratio, float amp)
        {
            if (! on) return centre;
            ph += hz * ratio / osr; if (ph >= 1.0) ph -= 1.0;
            const float sine = (float) std::sin (ph * juce::MathConstants<double>::twoPi) * amp;
            return y2.hp (y1.hp (centre, c), c) + s2.lp (s1.lp (sine, c), c) * g;
        }
        // the note of the sub: TUNE's whole octaves left out
        static double noteHz (int playNote, const Params& p)
        {
            return 440.0 * std::pow (2.0, (playNote - 69 + (p.tuneSt - 12.0 * std::round (p.tuneSt / 12.0)) + p.fineCt * 0.01) / 12.0);
        }
    private:
        doomsday::dsp::Svf y1, y2, s1, s2;
        doomsday::dsp::SvfCoefs c;
        double ph = 0.0, osr = 176400.0;
        float g = 1.f;
        bool on = true;
    };

    class Stereo
    {
    public:
        static bool wide (const Params& p) { return p.uniVoices > 1 && p.stereoWidth > 0.001f; }
        void clear() { c1 = {}; c2 = {}; for (auto& f : side) f = {}; }
        void cook (const Params& p, double osr) { c = doomsday::dsp::svfCoefs (juce::jlimit (60.f, 2000.f, p.splitHz), 1.f / 0.707f, osr); width = p.stereoWidth; }
        inline void process (float centre, float left, float right, float& yl, float& yr)
        {
            const float low = c2.lp (c1.lp (centre, c), c);
            const float hl = side[1].hp (side[0].hp (left, c), c), hr = side[3].hp (side[2].hp (right, c), c);
            const float m = 0.5f * (hl + hr), sdv = 0.5f * (hl - hr) * width;
            yl = low + m + sdv; yr = low + m - sdv;
        }
    private:
        doomsday::dsp::Svf c1, c2;
        std::array<doomsday::dsp::Svf, 4> side;
        doomsday::dsp::SvfCoefs c;
        float width = 0.4f;
    };

    // OUTPUT: CLIP (soft) -> LEVEL * velocity * the post gate -> a safety knee from -1.5 dB (never over 0 dBFS after down-sampling)
    inline float outputStage (float x, const Params& p, float gain)
    {
        if (p.clip > 0.001f) x += p.clip * (std::tanh (x * (1.f + 4.f * p.clip)) / (1.f + 0.5f * p.clip) - x);
        x *= gain;
        const float ax = std::abs (x);
        if (ax > 0.84f) x = std::copysign (0.84f + 0.1f * std::tanh ((ax - 0.84f) / 0.1f), x);
        return x;
    }
}
