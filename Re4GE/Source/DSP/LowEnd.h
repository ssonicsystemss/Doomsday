#pragma once
// RE4GE - SUB and STEREO
//   SUB    : a clean sine on the note replaces everything below SUB Hz (LR4: HP on the bass + LP on the sine, sums flat).
//            Never cut, never distorted - the 'pierdolnięcie'. TUNE's whole octaves are left out (TUNE +12 still has its sub on the note).
//   STEREO : the lows of the centre chain stay mono; above SPLIT the detuned left / right chains go in as mid / side scaled by WIDTH.
#include "ReParams.h"
#include <vector>

namespace re
{
    class Sub
    {
    public:
        void clear() { ph = 0.0; y1 = {}; y2 = {}; s1 = {}; s2 = {}; }
        void resetPhase() { ph = 0.0; }
        void cook (const Params& p, double osr_)
        {
            osr = osr_; c = doomsday::dsp::svfCoefs (juce::jmax (20.f, p.subHz), 1.f / 0.707f, osr); g = p.subDb <= -59.f ? 0.f : std::pow (10.f, p.subDb / 20.f); on = p.subHz > 20.f;
            mult = p.subOctDown ? 0.5 : 1.0; harm = p.subHarm; sat = p.subSat;
        }
        // hz = the sub's note, ratio = the pitch movement, amp = the envelope
        inline float process (float centre, double hz, double ratio, float amp)
        {
            if (! on) return centre;
            ph += hz * mult * ratio / osr; if (ph >= 1.0) ph -= 1.0;
            const double w = ph * juce::MathConstants<double>::twoPi;
            float sine = (float) std::sin (w);
            if (sat > 0.001f) sine = std::tanh (sine * (1.f + 5.f * sat)) / std::tanh (1.f + 5.f * sat);     // rounder / louder (default 0)
            float y = y2.hp (y1.hp (centre, c), c) + s2.lp (s1.lp (sine * amp, c), c) * g;
            if (harm > 0.001f) y += (float) (harm * (0.5 * std::sin (2.0 * w) + 0.3 * std::sin (3.0 * w))) * amp * g;   // clean 2nd / 3rd: the sub on small speakers
            return y;
        }
        // the note of the sub: TUNE's whole octaves left out
        static double noteHz (int playNote, const Params& p)
        {
            return 440.0 * std::pow (2.0, (playNote - 69 + (p.tuneSt - 12.0 * std::round (p.tuneSt / 12.0)) + p.fineCt * 0.01) / 12.0);
        }
    private:
        doomsday::dsp::Svf y1, y2, s1, s2;
        doomsday::dsp::SvfCoefs c;
        double ph = 0.0, osr = 176400.0, mult = 1.0;
        float harm = 0.f, sat = 0.f;
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

    // SMEAR: 4 allpasses (3-14 ms x SIZE) on the part above ~300 Hz, a different set per chain (wider). Fills the gaps between
    // the harmonics like the reverb inside reversed-kick samples, without washing out the low end. Buffers made once (192 kHz x 4).
    class Smear
    {
    public:
        Smear() { for (auto& d : line) d.assign (16384, 0.f); }
        void clear() { for (auto& d : line) std::fill (d.begin(), d.end(), 0.f); pos = 0; hp1 = {}; hp2 = {}; }
        void cook (const Params& p, double osr, int chainIdx)
        {
            amount = p.smear;
            static const float base[3][4] { { 3.1f, 5.3f, 8.9f, 13.7f }, { 3.7f, 6.1f, 9.7f, 12.9f }, { 2.9f, 4.7f, 7.9f, 14.3f } };
            for (int i = 0; i < 4; ++i) len[i] = juce::jlimit (8, 16383, (int) (base[chainIdx][i] * (0.35f + 0.65f * p.smearSize) * 0.001 * osr));
            hc = doomsday::dsp::svfCoefs (300.f, 1.f / 0.707f, osr);
        }
        inline float process (float x)
        {
            if (amount <= 0.001f) return x;
            float v = hp2.hp (hp1.hp (x, hc), hc);
            for (int i = 0; i < 4; ++i)
            {
                auto& d = line[(size_t) i];
                const float del = d[(size_t) ((pos - len[i]) & 16383)];
                const float in = v + 0.6f * del;
                d[(size_t) (pos & 16383)] = in;
                v = del - 0.6f * in;
            }
            pos = (pos + 1) & 16383;
            return x + amount * v;
        }
    private:
        std::array<std::vector<float>, 4> line;
        int len[4] { 100, 200, 300, 400 };
        int pos = 0;
        float amount = 0.f;
        doomsday::dsp::Svf hp1, hp2;
        doomsday::dsp::SvfCoefs hc;
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
