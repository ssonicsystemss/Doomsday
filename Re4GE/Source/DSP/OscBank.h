#pragma once
// RE4GE - OSC 2 / 3 / 4: extra layers on top of OSC 1 (the SYNTH / TAIL / SAMPLE source). Everything is summed BEFORE the tone chain,
// so the layers distort together (intermodulation = density, the owner's samples measure ~100x denser than a single clean layer).
//   WAVE  SINE / TRI / SAW / SQUARE / PULSE (polyBLEP) / NOISE (white -> one-pole LP: WIDTH = brightness)
//   OCT / SEMI / FINE on the played pitch (follows the pitch movement), PHASE = start phase on every step (PHASE RESET)
//   SYNC  hard sync to the note's phasor (an osc tuned up 'talks'), PM = phase modulation by the note's sine
//   Left / right chains: one detuned copy per side (cheap stereo); NOISE is decorrelated per side.
#include "ReParams.h"
#include "Oscillator.h"

namespace re
{
    class OscBank
    {
    public:
        struct State
        {
            std::array<double, 3> ph {};
            std::array<uint32_t, 3> rnd { 0x12345u, 0x6789au, 0xbcdefu };
            std::array<float, 3> lp {};
            double master = 0.0;
        };
        static bool any (const Params& p) { for (auto& o : p.osc) if (o.levelDb > -59.f) return true; return false; }
        static void cook (const Params& p, double osr, std::array<float, 3>& gain, std::array<double, 3>& mult, std::array<float, 3>& noiseC)
        {
            for (size_t i = 0; i < 3; ++i)
            {
                const auto& o = p.osc[i];
                gain[i] = o.levelDb <= -59.f ? 0.f : std::pow (10.f, o.levelDb / 20.f);
                mult[i] = std::exp2 (o.oct + (o.semi + o.fineCt * 0.01) / 12.0);
                const double fc = 300.0 * std::pow (60.0, (double) o.width);                 // NOISE: 300 Hz .. 18 kHz
                noiseC[i] = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * juce::jmin (fc, osr * 0.45) / osr));
            }
        }
        // the start of a step: phases to PHASE, master to 0 (also when FREE is off)
        static void reset (const Params& p, State& s, int sd)
        {
            s.master = 0.0;
            for (size_t i = 0; i < 3; ++i) { s.ph[i] = p.osc[i].phaseDeg / 360.0; s.rnd[i] = 0x9e3779b9u * (uint32_t) (i + 1) + 0x7f4a7c15u * (uint32_t) (sd + 1); s.lp[i] = 0.f; }
        }
        // one sample: hzNow = the played pitch incl. the pitch movement, detune = ratio for this side
        static inline float render (const Params& p, State& s, double hzNow, double osr, double detune,
                                    const std::array<float, 3>& gain, const std::array<double, 3>& mult, const std::array<float, 3>& noiseC,
                                    double pmScale = 1.0)
        {
            const double mdt = hzNow / osr;
            s.master += mdt;
            const bool wrapped = s.master >= 1.0;
            if (wrapped) s.master -= 1.0;
            const double msin = std::sin (juce::MathConstants<double>::twoPi * s.master);
            float y = 0.f;
            for (size_t i = 0; i < 3; ++i)
            {
                if (gain[i] <= 0.f) continue;
                const auto& o = p.osc[i];
                if (o.wave == WNoise)
                {
                    auto& r = s.rnd[i]; r ^= r << 13; r ^= r >> 17; r ^= r << 5;
                    const float w = (float) (r & 0xffffff) / 8388608.f - 1.f;
                    s.lp[i] += (w - s.lp[i]) * noiseC[i];
                    y += s.lp[i] * 1.7f * gain[i];
                    continue;
                }
                const double dt = mdt * mult[i] * detune;
                double& ph = s.ph[i];
                ph += dt; if (ph >= 1.0) ph -= std::floor (ph);
                if (o.sync && wrapped) ph = s.master * mult[i] * detune - std::floor (s.master * mult[i] * detune);   // restart with the note
                double q = ph;
                if (o.pm > 0.f) { q += o.pm * pmScale * 0.35 * msin; q -= std::floor (q); }
                double v;
                switch (o.wave)
                {
                    case WSine: v = std::sin (juce::MathConstants<double>::twoPi * q); break;
                    case WTri:  v = 1.0 - 4.0 * std::abs (q - 0.5); break;
                    case WSaw:  v = 2.0 * q - 1.0 - doomsday::dsp::polyBlep ((float) q, (float) dt); break;
                    default:
                    {
                        const double pw = o.wave == WSquare ? 0.5 : juce::jlimit (0.05, 0.95, (double) o.width);
                        v = (q < pw ? 1.0 : -1.0) + doomsday::dsp::polyBlep ((float) q, (float) dt)
                          - doomsday::dsp::polyBlep (doomsday::dsp::wrap01 ((float) (q + 1.0 - pw)), (float) dt) - (2.0 * pw - 1.0);
                    }
                }
                y += (float) v * 0.7f * gain[i];
            }
            return y;
        }
    };
}
