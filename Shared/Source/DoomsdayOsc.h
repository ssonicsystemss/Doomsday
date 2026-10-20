#pragma once
// =====================================================================
//  DOOMSDAY shared: oscillator ideas from Massive / Spire / Sytrus / Diva (namespace doomsday::osc)
//
//   IntensityOsc   one oscillator, one INTENSITY knob whose meaning depends on the MODE (Massive / Spire style):
//                    SPECTRUM  saw <-> square base, INTENSITY = how many harmonics (keytracked low-pass: dull .. bright)
//                    BEND      phase distortion (Casio CZ style): INTENSITY bends the read-out -> from sine to sharp, buzzy
//                    FORMANT   a formant ("vowel") that stays put while the pitch moves: INTENSITY = formant 200 Hz .. 4 kHz
//   ModOsc         one sine modulator that follows the pitch (ratio) + helpers: ring (), phaseOffset () for PM / FM,
//                  cutoffMul () for FILTER FM (growl)
//   FmOps<N>       N sine operators (Sytrus style): ratio, level, feedback, NxN modulation matrix, which ones are heard
//   ChordStack     unison <-> chord morph (Massive X): per-voice semitone offsets for UNISON / OCTAVES / FIFTH / POWER /
//                  MINOR / MAJOR / SUS4 / MINOR 7, `amount` 0 = all on the root, 1 = full chord
//   Slop           analog "slop" (Diva trimmers): small fixed per-voice offsets of pitch, cutoff and envelope times
//                  + an optional slow drift
//
//  Real-time safe (no allocation).  Tested in Shared/Tests.
// =====================================================================
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include "DoomsdayDSP.h"

namespace doomsday::osc
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    // ------------------------------------------------------------------ IntensityOsc
    class IntensityOsc
    {
    public:
        enum Mode { Spectrum = 0, Bend, Formant, numModes };
        static juce::StringArray modeNames() { return { "SPECTRUM", "BEND", "FORMANT" }; }
        void prepare (double sampleRate) { sr = sampleRate; inv = 1.f / (float) sr; reset(); }
        void reset (float startPhase = 0.f) { ph = startPhase; lp = {}; lp2 = {}; }
        void setMode (int m) { mode = juce::jlimit (0, (int) numModes - 1, m); }
        void setIntensity (float i01) { intensity = juce::jlimit (0.f, 1.f, i01); }
        void setShape (float sawToSquare) { shape = juce::jlimit (0.f, 1.f, sawToSquare); }

        inline float next (float hz)
        {
            const float dt = juce::jlimit (0.f, 0.49f, hz * inv);
            ph += dt; if (ph >= 1.f) ph -= 1.f;
            switch (mode)
            {
                case Bend:
                {
                    // CZ-style phase distortion of a sine: the knee moves from the middle (pure sine) to the start (sharp)
                    const float knee = 0.5f - 0.48f * intensity;
                    const float p = ph < knee ? 0.5f * ph / knee : 0.5f + 0.5f * (ph - knee) / (1.f - knee);
                    return std::sin (twoPi * p);
                }
                case Formant:
                {
                    // a sine at the formant frequency, restarted every period and windowed (no sync clicks)
                    const float formantHz = 200.f * std::pow (20.f, intensity);
                    const float ratio = juce::jmax (1.f, formantHz / juce::jmax (1.f, hz));
                    const float w = 0.5f - 0.5f * std::cos (twoPi * ph);
                    return std::sin (twoPi * ph * ratio) * w * 1.6f;
                }
                default:
                {
                    float saw = 2.f * ph - 1.f - doomsday::dsp::polyBlep (ph, dt);
                    float sq = (ph < 0.5f ? 1.f : -1.f) + doomsday::dsp::polyBlep (ph, dt) - doomsday::dsp::polyBlep (doomsday::dsp::wrap01 (ph + 0.5f), dt);
                    const float x = saw + shape * (sq - saw);
                    // keytracked 24 dB low-pass: from ~1 harmonic (almost a sine) to ~180
                    const float cut = juce::jmin ((float) sr * 0.45f, hz * std::pow (2.f, -0.5f + intensity * 8.f));
                    if (std::abs (cut - lastCut) > 1.f) { coefs = doomsday::dsp::svfCoefs (cut, 1.f / 0.707f, sr); lastCut = cut; }
                    return lp2.lp (lp.lp (x, coefs), coefs) * (1.f + 0.6f * (1.f - intensity));
                }
            }
        }
        float phase() const { return ph; }
    private:
        double sr = 44100.0;
        float inv = 1.f / 44100.f, ph = 0.f, intensity = 0.5f, shape = 0.f, lastCut = -1.f;
        int mode = Spectrum;
        doomsday::dsp::Svf lp, lp2;
        doomsday::dsp::SvfCoefs coefs;
    };

    // ------------------------------------------------------------------ ModOsc
    class ModOsc
    {
    public:
        void prepare (double sampleRate) { inv = 1.f / (float) sampleRate; ph = 0.f; }
        void reset() { ph = 0.f; }
        // ratio: modulator pitch relative to the note (1 = same, 0.5 = octave down, 1.414 = tritone ...)
        inline float next (float hz, float ratio) { ph += hz * ratio * inv; ph -= std::floor (ph); return std::sin (twoPi * ph); }
        static inline float ring (float x, float m, float amount) { return x + amount * (x * m - x); }
        // phase offset in cycles to add to a carrier's phase (PM = FM sound, no pitch drift)
        static inline float phaseOffset (float m, float amount) { return amount * 0.5f * m; }
        // cutoff multiplier for FILTER FM: +-amount octaves
        static inline float cutoffMul (float m, float amountOct) { return std::pow (2.f, m * amountOct); }
    private:
        float inv = 1.f / 44100.f, ph = 0.f;
    };

    // ------------------------------------------------------------------ FmOps
    template <int N = 4>
    class FmOps
    {
    public:
        void prepare (double sampleRate) { inv = 1.f / (float) sampleRate; reset(); }
        void reset() { ph.fill (0.f); out.fill (0.f); }
        void setOp (int i, float ratio, float level, float feedback, bool audible)
        {
            if (! juce::isPositiveAndBelow (i, N)) return;
            ratios[(size_t) i] = ratio; levels[(size_t) i] = level; fb[(size_t) i] = feedback; heard[(size_t) i] = audible;
        }
        // amount with which operator `from` modulates the phase of operator `to`
        void setMod (int from, int to, float amount) { if (juce::isPositiveAndBelow (from, N) && juce::isPositiveAndBelow (to, N)) matrix[(size_t) from][(size_t) to] = amount; }
        // per-operator envelope / level multipliers can be applied by the caller through `levelMul`
        inline float next (float hz, const float* levelMul = nullptr)
        {
            std::array<float, N> now {};
            float sum = 0.f;
            for (int i = 0; i < N; ++i)
            {
                float pm = fb[(size_t) i] * out[(size_t) i];
                for (int j = 0; j < N; ++j) pm += matrix[(size_t) j][(size_t) i] * out[(size_t) j];
                ph[(size_t) i] += hz * ratios[(size_t) i] * inv; ph[(size_t) i] -= std::floor (ph[(size_t) i]);
                const float lv = levels[(size_t) i] * (levelMul != nullptr ? levelMul[i] : 1.f);
                now[(size_t) i] = std::sin (twoPi * (ph[(size_t) i] + 0.5f * pm)) * lv;
                if (heard[(size_t) i]) sum += now[(size_t) i];
            }
            out = now;
            return sum;
        }
    private:
        float inv = 1.f / 44100.f;
        std::array<float, N> ph {}, out {}, ratios {}, levels {}, fb {};
        std::array<bool, N> heard {};
        std::array<std::array<float, N>, N> matrix {};
    public:
        FmOps() { ratios.fill (1.f); levels.fill (1.f); heard[0] = true; }
    };

    // ------------------------------------------------------------------ ChordStack
    struct ChordStack
    {
        enum Chord { Unison = 0, Octaves, Fifth, Power, Minor, Major, Sus4, Minor7, numChords };
        static juce::StringArray chordNames() { return { "UNISON", "OCTAVES", "FIFTH", "POWER", "MINOR", "MAJOR", "SUS4", "MINOR 7" }; }
        // semitone offset of voice v (0..n-1) for the chord, scaled by amount (0 = unison, 1 = chord)
        static float semitones (int chord, int v, int n, float amount)
        {
            static const int tones[numChords][4] = { { 0, 0, 0, 0 }, { 0, 12, -12, 24 }, { 0, 7, 12, 19 }, { 0, 7, 12, -12 },
                                                      { 0, 3, 7, 12 }, { 0, 4, 7, 12 }, { 0, 5, 7, 12 }, { 0, 3, 7, 10 } };
            if (n <= 1) return 0.f;
            const int c = juce::jlimit (0, (int) numChords - 1, chord);
            const int k = v % 4, oct = v / 4;
            return (float) (tones[c][k] + (c == Unison ? 0 : 12 * oct)) * juce::jlimit (0.f, 1.f, amount);
        }
        static float ratio (int chord, int v, int n, float amount) { return std::pow (2.f, semitones (chord, v, n, amount) / 12.f); }
    };

    // ------------------------------------------------------------------ Slop
    class Slop
    {
    public:
        struct Voice { float pitchCents = 0.f, cutoffMul = 1.f, timeMul = 1.f; };
        static constexpr int maxVoices = 32;
        void prepare (double sampleRate, uint32_t seed = 0x51ED270Bu)
        {
            rate = sampleRate;
            uint32_t r = seed ? seed : 1u;
            auto rnd = [&r] { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return (float) (r & 0xffffff) / 8388607.5f - 1.f; };
            for (auto& b : base) b = { rnd(), rnd(), rnd() };
            drift.fill (0.f);
        }
        // amount 0..1: up to +-12 cents, +-20 % cutoff, +-15 % envelope times
        Voice forVoice (int v, float amount) const
        {
            const auto& b = base[(size_t) (v % maxVoices)];
            const float a = juce::jlimit (0.f, 1.f, amount);
            return { a * (12.f * b[0] + 6.f * drift[(size_t) (v % maxVoices)]), std::pow (2.f, a * 0.26f * b[1]), 1.f + a * 0.15f * b[2] };
        }
        // slow random walk of the pitch (call once per block)
        void advance (int samples)
        {
            const float k = (float) samples / (float) rate;
            for (size_t i = 0; i < drift.size(); ++i)
            {
                r ^= r << 13; r ^= r >> 17; r ^= r << 5;
                const float step = ((float) (r & 0xffff) / 32767.5f - 1.f) * k * 2.f;
                drift[i] = juce::jlimit (-1.f, 1.f, drift[i] * (1.f - 0.3f * k) + step);
            }
        }
    private:
        double rate = 44100.0;
        std::array<std::array<float, 3>, maxVoices> base {};
        std::array<float, maxVoices> drift {};
        uint32_t r = 0xC0FFEE11u;
    };
}
