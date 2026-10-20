#pragma once
// =====================================================================
//  DOOMSDAY shared: analog-style filter (namespace doomsday::filter)
//
//   Ladder   4-stage zero-delay-feedback ladder (Moog-style) with saturation inside the loop, like the
//            "analog" filters of Sylenth1 / Diva: warm, rounds off when pushed, resonance up to self-oscillation.
//            Modes (mixed from the 4 stages, Oberheim style): LP24, LP12, BP12, HP12, HP24.
//            drive: input gain into the loop saturation (0 dB = clean-ish, +24 dB = hot). Resonance 0..1
//            (1 = self-oscillates: a sine at the cutoff). Bass loss at high resonance compensated (bassComp 0..1).
//            One channel per instance (use two for stereo). setCutoff can be called every sample (cheap: one tan
//            per call, or use setCutoffFast with a cached tan via `prewarp`).
//   prewarp (hz, rate)  the g coefficient, if you want to compute it once per block
//
//  Real-time safe (no allocation).  Tested in Shared/Tests (cutoff, slopes, self-oscillation, stability, drive).
// =====================================================================
#include <juce_core/juce_core.h>
#include <cmath>
#include <array>

namespace doomsday::filter
{
    enum Mode { LP24 = 0, LP12, BP12, HP12, HP24, numModes };
    inline juce::StringArray modeNames() { return { "LP 24", "LP 12", "BP 12", "HP 12", "HP 24" }; }

    inline float prewarp (float hz, double rate)
    {
        const float f = juce::jlimit (10.f, (float) rate * 0.45f, hz);
        return std::tan (juce::MathConstants<float>::pi * f / (float) rate);
    }

    class Ladder
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; setCutoff (1000.f); reset(); }
        void reset() { s = {}; }
        void setCutoff (float hz) { setG (prewarp (hz, sr)); }
        void setG (float gIn) { g = gIn; G = g / (1.f + g); }
        void setResonance (float r01) { k = 4.f * juce::jlimit (0.f, 1.05f, r01); }
        void setDrive (float db) { drive = std::pow (10.f, db / 20.f); invDrive = 1.f / std::pow (drive, 0.8f); }   // partial makeup: hotter, not much louder
        void setMode (int m) { mode = juce::jlimit (0, (int) numModes - 1, m); }
        void setBassComp (float c) { bassComp = juce::jlimit (0.f, 1.f, c); }

        inline float process (float x)
        {
            // linear ZDF prediction of the loop output, then the saturated input stage
            const float inv = 1.f / (1.f + g);
            const float S = (G * G * G * s[0] + G * G * s[1] + G * s[2] + s[3]) * inv;
            const float G4 = G * G * G * G;
            const float in = x * drive * (1.f + k * 0.25f * bassComp);
            float u = (in - k * S) / (1.f + k * G4);
            u = 2.f * std::tanh (0.5f * u);                          // the warm part: the loop saturates (soft, with headroom), never explodes
            // four one-pole TPT stages, each slightly saturated (analog-style rounding of loud signals)
            float y[4];
            float v = u;
            for (int i = 0; i < 4; ++i)
            {
                const float w = (v - s[(size_t) i]) * G;
                y[i] = w + s[(size_t) i];
                s[(size_t) i] = y[i] + w;
                v = i < 3 ? sat (y[i]) : y[i];
            }
            float out;
            switch (mode)
            {
                case LP12: out = y[1]; break;
                case BP12: out = 2.f * (y[0] - y[1]); break;
                case HP12: out = u - 2.f * y[0] + y[1]; break;
                case HP24: out = u - 4.f * y[0] + 6.f * y[1] - 4.f * y[2] + y[3]; break;
                default:   out = y[3]; break;
            }
            return out * invDrive;
        }
    private:
        static inline float sat (float x) { return x * (27.f + x * x) / (27.f + 9.f * x * x); }   // gentle tanh-like, cheap
        double sr = 44100.0;
        float g = 0.07f, G = 0.065f, k = 0.f, drive = 1.f, invDrive = 1.f, bassComp = 0.5f;
        int mode = LP24;
        std::array<float, 4> s {};
    };
}
