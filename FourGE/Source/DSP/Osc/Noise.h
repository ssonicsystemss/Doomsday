#pragma once
// =====================================================================
//  4GE NOISE ENGINE (module 03, reused by module 04): WHITE / PINK / BROWN / METALLIC / DIGITAL, deterministic (seeded).
//  METALLIC = six band-limited squares at inharmonic ratios (the 808 cymbal recipe) around the oscillator frequency,
//  DIGITAL = a 15-bit LFSR clocked from the oscillator frequency (tonal, "8-bit" noise). Pure math, no allocation.
// =====================================================================
#include "../Core/Blocks.h"
#include <array>

namespace fge::dsp
{
    enum NoiseType { NoiseWhite = 0, NoisePink, NoiseBrown, NoiseMetallic, NoiseDigital, NumNoiseTypes };

    struct NoiseEngine
    {
        void reset (uint32_t seed)
        {
            white.reset (seed * 2654435761u + 1u);
            b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.f; brown = 0.f;
            for (size_t i = 0; i < met.size(); ++i) met[i].reset ((float) i * 0.137f);
            lfsr = 0x4001u ^ (seed & 0x3fffu); lfsr = lfsr == 0 ? 1u : lfsr; clockPh = 0.f; digital = 1.f;
        }
        inline float tick (int type, float hz, float inv)
        {
            switch (type)
            {
                case NoisePink:
                {
                    const float w = white.tick();               // Paul Kellet's refined filter
                    b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f; b2 = 0.96900f * b2 + w * 0.1538520f;
                    b3 = 0.86650f * b3 + w * 0.3104856f; b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
                    const float y = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f; b6 = w * 0.115926f;
                    return y * 0.2f;
                }
                case NoiseBrown:
                {
                    brown = juce::jlimit (-1.f, 1.f, 0.998f * brown + 0.06f * white.tick());
                    return brown * 1.6f;
                }
                case NoiseMetallic:
                {
                    static constexpr float r[6] { 1.f, 1.4471f, 1.617f, 1.9265f, 2.5028f, 2.6637f };
                    float s = 0.f;
                    const float base = juce::jlimit (40.f, 4000.f, hz) * 2.f;
                    for (size_t i = 0; i < 6; ++i) s += met[i].tick (Square, base * r[i], inv);
                    return s * 0.22f;
                }
                case NoiseDigital:
                {
                    clockPh += juce::jlimit (0.f, 1.f, hz * 16.f * inv);
                    while (clockPh >= 1.f)
                    {
                        clockPh -= 1.f;
                        const uint32_t bit = (lfsr ^ (lfsr >> 1)) & 1u;
                        lfsr = (lfsr >> 1) | (bit << 14);
                        digital = (lfsr & 1u) ? 0.8f : -0.8f;
                    }
                    return digital;
                }
                default: return white.tick() * 0.8f;
            }
        }
    private:
        Noise white;
        float b0 = 0.f, b1 = 0.f, b2 = 0.f, b3 = 0.f, b4 = 0.f, b5 = 0.f, b6 = 0.f, brown = 0.f;
        std::array<Osc, 6> met {};
        uint32_t lfsr = 0x4001u;
        float clockPh = 0.f, digital = 1.f;
    };
}
