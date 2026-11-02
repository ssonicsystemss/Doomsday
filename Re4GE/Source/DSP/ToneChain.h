#pragma once
// RE4GE - TONE: PRE FILTER -> DRIVE (4GE stage: type, BIAS, MIX, auto gain; DRIVE ENV = dirtier at the peak) -> DC 5 Hz
//         -> LOW CUT (off by default: the owner's rule, the low end stays) -> BELL (+ the macros' BODY 220 Hz / BITE 2 kHz bells)
//         -> STAGE 2 (v0.4, 0 dB = off) -> HIGH CUT (4-pole)
//         v0.7: each stage can be a DISTORTION LAB curve instead (Shared lab:: with ADAA): SATURATION / DIODE / WAVEFOLD /
//         RECTIFIER / BITCRUSH / CUSTOM (drawn), HARDNESS, TONE (tilt, PRE or POST), OUTPUT; AUTO GAIN keeps the level.
//         The PRE FILTER cutoff can move with the envelope (ENV, octaves) and the note (KEY): the engine passes its coefficients.
// One ToneChain per chain (centre / left / right); the coefficients are made once per chunk (ToneCoefs).
#include "ReParams.h"
#include "DoomsdayLab.h"

namespace re
{
    struct ToneCoefs
    {
        doomsday::dsp::SvfCoefs pre, lc, hc1, hc2;
        doomsday::dsp::BiquadCoefs bell, body, bite;
        float dcR = 0.999f, g2 = 1.f;
        std::array<float, 2> out { 1.f, 1.f };
        std::array<const doomsday::lab::CustomTable*, 2> table { nullptr, nullptr };
        bool dynPre = false;
        void cook (const Params& p, double osr)
        {
            using namespace doomsday::dsp;
            pre = svfCoefs (p.preHz, ge::resoToK (p.preReso), osr);
            lc = svfCoefs (juce::jmax (20.f, p.lowCutHz), 1.f / 0.707f, osr);
            hc1 = svfCoefs (p.highCutHz, 1.f / 0.54f, osr); hc2 = svfCoefs (p.highCutHz, 1.f / 1.31f, osr);
            bell = BiquadCoefs::make (BiquadCoefs::Bell, osr, p.bellHz, juce::jmax (0.2f, p.bellQ), p.bellDb);
            body = BiquadCoefs::make (BiquadCoefs::Bell, osr, 220.f, 0.8f, p.bodyDb);
            bite = BiquadCoefs::make (BiquadCoefs::Bell, osr, 2000.f, 1.1f, p.biteDb);
            g2 = std::pow (10.f, p.dist2Db / 20.f);
            for (int s = 0; s < 2; ++s) out[(size_t) s] = std::pow (10.f, p.lab[(size_t) s].outDb / 20.f);
            dynPre = p.preType != 0 && (std::abs (p.preEnvOct) > 0.001f || p.preKey > 0.001f);
            dcR = (float) std::exp (-juce::MathConstants<double>::twoPi * 5.0 / osr);       // rate-independent (LESSONS: Forge's 0.9995)
        }
    };

    struct ToneChain
    {
        doomsday::dsp::Svf pre, hc1, hc2, lcF;
        doomsday::dsp::Biquad bell, body, bite;
        float dcx = 0.f, dcy = 0.f, dc2x = 0.f, dc2y = 0.f;
        std::array<doomsday::lab::Shaper, 2> shaper;
        std::array<doomsday::lab::Tilt, 2> tilt;
        float peak[2] { 0.f, 0.f };                       // the stages' input level (|x * drive|), for the live dot in the GUI
        void clear() { *this = ToneChain(); }
        void setTilt (const Params& p, double osr) { for (int s = 0; s < 2; ++s) tilt[(size_t) s].set (p.lab[(size_t) s].tone, osr); }
        // one stage: CLASSIC (4GE) or a LAB curve
        inline float stageN (int s, float x, float g, int classicType, float bias, float mix, const Params& p, const ToneCoefs& c)
        {
            const auto& L = p.lab[(size_t) s];
            peak[s] = juce::jmax (peak[s], std::abs (x * g + bias));
            if (L.algo < 0) return ge::stage (x, g, classicType, bias, mix, p.autoGain) * c.out[(size_t) s];
            float in = x;
            if (! L.tonePost && std::abs (L.tone) > 0.01f) in = tilt[(size_t) s].process (in);
            const auto* tab = c.table[(size_t) s];
            float y = shaper[(size_t) s].process (in * g + bias, L.algo, L.hard, tab);
            if (std::abs (bias) > 1.0e-4f) y -= (float) doomsday::lab::curve (bias, L.algo, L.hard, tab);
            if (p.autoGain)
            {
                const float lg = std::log2 (juce::jmax (1.f, g));
                y *= (1.f / juce::jmin (1.f, juce::jmax (0.05f, g))) * (1.f - 0.29f * juce::jlimit (0.f, 1.f, lg / 4.f));
            }
            if (L.tonePost && std::abs (L.tone) > 0.01f) y = tilt[(size_t) s].process (y);
            y = mix >= 0.999f ? y : x + (y - x) * mix;
            return y * c.out[(size_t) s];
        }
        // g = drive gain (linear) for this sample
        inline float process (float x, float g, const Params& p, const ToneCoefs& c, const doomsday::dsp::SvfCoefs& preC)
        {
            x = ge::filt (pre, x, preC, p.preType);
            if (! p.stagesOff) x = stageN (0, x, g, p.distType, p.bias, p.mix, p, c);
            const float dc = x - dcx + c.dcR * dcy; dcx = x; dcy = dc; x = dc;
            if (p.lowCutHz > 20.5f) x = lcF.hp (x, c.lc);
            if (std::abs (p.bellDb) > 0.05f) x = bell.process (x, c.bell);
            if (p.bodyDb > 0.05f) x = body.process (x, c.body);
            if (p.biteDb > 0.05f) x = bite.process (x, c.bite);
            if (p.dist2Db > 0.05f && ! p.stagesOff)
            {
                x = stageN (1, x, c.g2, p.dist2Type, p.dist2Bias, p.dist2Mix, p, c);
                if (std::abs (p.dist2Bias) > 1.0e-4f) { const float d = x - dc2x + c.dcR * dc2y; dc2x = x; dc2y = d; x = d; }
            }
            x = hc2.lp (hc1.lp (x, c.hc1), c.hc2);
            return x;
        }
    };
}
