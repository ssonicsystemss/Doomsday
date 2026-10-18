#pragma once
// =====================================================================
//  ANCIENT DRUMS - built-in sample library
//  Every sample is synthesised once at start-up (deterministic seeds),
//  so the plugin ships as a single .vst3 without external files.
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include "Params.h"
#include <vector>
#include <memory>
#include <cmath>

struct SampleData
{
    std::vector<float> data;    // mono
    double sampleRate = 48000.0;
    juce::String name;
    bool isUser = false;
    juce::String path;          // user samples only
};
using SamplePtr = std::shared_ptr<const SampleData>;

class SampleLibrary
{
public:
    static const SampleLibrary& get()
    {
        static const SampleLibrary lib;
        return lib;
    }

    SamplePtr sample (int layer, int index) const
    {
        index = juce::jlimit (0, Library::perLayer - 1, index);
        return samples[(size_t) layer][(size_t) index];
    }

private:
    static constexpr double sr = 48000.0;
    static constexpr float twoPi = juce::MathConstants<float>::twoPi;

    SampleLibrary()
    {
        for (int i = 0; i < Library::perLayer; ++i)
        {
            samples[Punch].push_back (makePunch (i));
            samples[Crunch].push_back (makeCrunch (i));
            samples[Kick].push_back (makeKick (i));
        }
    }

    std::array<std::vector<SamplePtr>, NumSampleLayers> samples;

    // ------------------------------------------------------------ helpers
    struct Svf
    {
        float ic1 = 0.f, ic2 = 0.f, a1 = 0.f, a2 = 0.f, a3 = 0.f, k = 1.f;
        void set (float hz, float q)
        {
            const float g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (10.f, 23000.f, hz) / (float) sr);
            k = 1.f / q; a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        void tick (float v0, float& lp, float& bp, float& hp)
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
            lp = v2; bp = v1; hp = v0 - k * v1 - v2;
        }
    };

    static void normalise (std::vector<float>& d, float target = 0.92f)
    {
        // remove DC / sub-sonic offset left by asymmetric saturation (it only eats headroom)
        {
            const float c = std::exp (-juce::MathConstants<float>::twoPi * 18.f / (float) sr);
            float xPrev = 0.f, yPrev = 0.f;
            for (auto& v : d) { const float y = v - xPrev + c * yPrev; xPrev = v; yPrev = y; v = y; }
        }
        float pk = 0.f;
        for (auto v : d) pk = std::max (pk, std::abs (v));
        if (pk > 0.f) for (auto& v : d) v *= target / pk;
        // tiny fade-in/out to kill clicks at the edges
        const size_t fi = std::min<size_t> (16, d.size()), fo = std::min<size_t> (240, d.size());
        for (size_t i = 0; i < fi; ++i) d[i] *= (float) i / (float) fi;
        for (size_t i = 0; i < fo; ++i) d[d.size() - 1 - i] *= (float) i / (float) fo;
    }

    static SamplePtr finish (std::vector<float> d, const juce::String& name)
    {
        normalise (d);
        auto s = std::make_shared<SampleData>();
        s->data = std::move (d);
        s->sampleRate = sr;
        s->name = name;
        return s;
    }

    // ------------------------------------------------------------ PUNCH
    // short pitch-swept sine + click, saturated
    static SamplePtr makePunch (int i)
    {
        juce::Random r (1000 + i * 17);
        const float len    = 0.10f + 0.06f * r.nextFloat();
        const float fStart = 1500.f + 2600.f * r.nextFloat();
        const float fEnd   = 110.f + 110.f * r.nextFloat();
        const float tau    = 0.004f + 0.008f * r.nextFloat();
        const float decay  = 0.022f + 0.035f * r.nextFloat();
        const float drive  = 1.5f + 3.f * r.nextFloat();
        const float click  = 0.15f + 0.5f * r.nextFloat();
        const float tone   = 5000.f + 9000.f * r.nextFloat();

        const int n = (int) (len * sr);
        std::vector<float> d ((size_t) n);
        float ph = 0.f;
        Svf lpf; lpf.set (tone, 0.7f);
        Svf hpc; hpc.set (2500.f, 0.7f);
        for (int s = 0; s < n; ++s)
        {
            const float t = (float) s / (float) sr;
            const float f = fEnd + (fStart - fEnd) * std::exp (-t / tau);
            ph += f / (float) sr; ph -= std::floor (ph);
            float y = std::sin (ph * twoPi) * std::exp (-t / decay);

            float lp, bp, hp;
            const float nz = (r.nextFloat() * 2.f - 1.f) * std::exp (-t / 0.0018f);
            hpc.tick (nz, lp, bp, hp);
            y += hp * click;

            y = std::tanh (y * drive);
            lpf.tick (y, lp, bp, hp);
            d[(size_t) s] = lp;
        }
        return finish (std::move (d), Library::punchNames[i]);
    }

    // ------------------------------------------------------------ CRUNCH
    // swept sine + noise, hard distorted (different flavours), band-passed
    static SamplePtr makeCrunch (int i)
    {
        juce::Random r (2000 + i * 31);
        const float len    = 0.14f + 0.14f * r.nextFloat();
        const float fStart = 500.f + 900.f * r.nextFloat();
        const float fEnd   = 150.f + 120.f * r.nextFloat();
        const float tau    = 0.010f + 0.02f * r.nextFloat();
        const float noise  = 0.2f + 0.6f * r.nextFloat();
        const float drive  = 6.f + 18.f * r.nextFloat();
        const float centre = 1200.f + 2600.f * r.nextFloat();
        const float decay  = 0.05f + 0.09f * r.nextFloat();
        const int   mode   = i % 4;

        const int n = (int) (len * sr);
        std::vector<float> d ((size_t) n);
        float ph = 0.f, held = 0.f;
        int holdCount = 0;
        const int holdLen = 2 + r.nextInt (4);
        Svf bpf; bpf.set (centre, 0.9f);
        Svf hpf; hpf.set (350.f, 0.7f);
        for (int s = 0; s < n; ++s)
        {
            const float t = (float) s / (float) sr;
            const float f = fEnd + (fStart - fEnd) * std::exp (-t / tau);
            ph += f / (float) sr; ph -= std::floor (ph);
            float x = std::sin (ph * twoPi) + noise * (r.nextFloat() * 2.f - 1.f);
            x *= drive;
            float y;
            switch (mode)
            {
                case 0:  y = std::tanh (x); break;                                   // saturate
                case 1:  y = juce::jlimit (-1.f, 1.f, x); break;                     // hard clip
                case 2:  y = std::sin (x * 0.35f); break;                            // fold
                default: y = std::round (std::tanh (x) * 6.f) / 6.f; break;          // crush
            }
            if (mode == 3) { if (holdCount++ % holdLen == 0) held = y; y = held; }  // sample-rate reduce

            const float env = (1.f - std::exp (-t / 0.0008f)) * std::exp (-t / decay);
            float lp, bp, hp, lp2, bp2, hp2;
            bpf.tick (y, lp, bp, hp);
            hpf.tick (bp + 0.35f * y, lp2, bp2, hp2);
            d[(size_t) s] = hp2 * env;
        }
        return finish (std::move (d), Library::crunchNames[i]);
    }

    // ------------------------------------------------------------ KICK (full)
    // click + pitch-swept body landing exactly on the tail pitch (C2) + distorted tail
    static SamplePtr makeKick (int i)
    {
        juce::Random r (3000 + i * 13);
        const float len    = 0.55f + 0.35f * r.nextFloat();
        const float fStart = 280.f + 380.f * r.nextFloat();
        const float tau    = 0.012f + 0.022f * r.nextFloat();
        const float drive  = 2.5f + 9.f * r.nextFloat();
        const float asym   = (i % 2 == 0) ? 0.25f * r.nextFloat() : 0.f;
        const float hold   = 0.18f + 0.22f * r.nextFloat();
        const float rel    = 0.07f + 0.15f * r.nextFloat();
        const float tone   = 2500.f + 6000.f * r.nextFloat();
        const float click  = 0.2f + 0.6f * r.nextFloat();
        const float fTail  = Library::tailHz;

        const int n = (int) (len * sr);
        std::vector<float> d ((size_t) n);
        float ph = 0.f;
        Svf lpf; lpf.set (tone, 0.75f);
        Svf hpc; hpc.set (3000.f, 0.7f);
        for (int s = 0; s < n; ++s)
        {
            const float t = (float) s / (float) sr;
            const float f = fTail + (fStart - fTail) * std::exp (-t / tau);
            ph += f / (float) sr; ph -= std::floor (ph);
            float y = std::sin (ph * twoPi);

            float amp = t < hold ? 1.f : std::exp (-(t - hold) / rel);
            y *= amp;
            y = std::tanh ((y + asym * amp) * drive) - std::tanh (asym * amp * drive);

            float lp, bp, hp;
            const float nz = (r.nextFloat() * 2.f - 1.f) * std::exp (-t / 0.0015f);
            hpc.tick (nz, lp, bp, hp);
            y += hp * click;

            lpf.tick (y, lp, bp, hp);
            d[(size_t) s] = lp;
        }
        return finish (std::move (d), Library::kickNames[i]);
    }
};
