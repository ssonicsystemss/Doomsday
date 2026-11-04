#pragma once
// =====================================================================
//  4GE MODULE 01 - the four independent generators: PunchGenerator, BodyGenerator, TailGenerator, SubGenerator.
//  Each one: its own source, amplitude envelope, pitch (glides into the kick pitch / follows it / sits on the note),
//  tone filter, drive, dry / processed MIX, phase. They only meet in the CoreMixer (Engine.h).
//  Settings come cooked per block (KickSettings); a generator never reads parameters itself.
// =====================================================================
#include "Blocks.h"
#include <array>

namespace fge::dsp
{
    using doomsday::dsp::Svf;
    using doomsday::dsp::SvfCoefs;

    enum Part { PPunch = 0, PBody, PTail, PSub, NumParts };

    struct PartSettings
    {
        int source = 0;
        EnvTimes env;
        float pitchHz = 100.f;      // start pitch (punch / body), sub pitch (tracking off)
        float glideMs = 10.f;
        float drive = 0.f, mix = 1.f, phase01 = 0.f, pw = 0.5f;
        int character = 0;          // tail
        SvfCoefs tone;              // the tone low-pass (+ resonance for body / tail)
        bool toneOpen = true;       // tone at the top of the range = no filter
        float driveG = 1.f, driveNorm = 1.f;
        float endMs = 0.f;          // the part is silent after this (its envelope length)
    };

    // the part's tone / drive right now (per control block: the filter / drive / resonance envelopes move them)
    struct ModNow { SvfCoefs tone; bool toneOpen = true; float drive = 0.f, driveG = 1.f, driveNorm = 1.f; };

    struct PitchSettings { int mode = 0; float startHz = 220.f, endHz = 49.f, timeMs = 80.f, curve = 0.3f, amountSt = 0.f, fineMul = 1.f; bool keyTrack = true; };

    struct KickSettings
    {
        double rate = 176400.0; float inv = 1.f / 176400.f;
        PitchSettings pitch;
        std::array<PartSettings, NumParts> part;
        float lengthMs = 450.f;     // the global LENGTH: caps the kick (soft fade at the end)
        bool phaseReset = true;
        float dcR = 0.9998f, dcTail = 0.9996f, avgC = 0.001f;
        SvfCoefs scream;            // TAIL CHARACTER SCREAM band
        float velSens = 0.f;
    };

    // normalized drive: more harmonics, the same peak level (y = tanh (g x) / tanh (g))
    inline float drive (float x, float g, float norm) { return g <= 1.0001f ? x : std::tanh (g * x) * norm; }

    // ------------------------------------------------------------------ PUNCH
    class PunchGenerator
    {
    public:
        void start (const PartSettings& s, float phase01, uint32_t seed) { osc.reset (phase01); nz.reset (seed); tone = {}; click = {}; }
        float phase() const { return osc.ph; }
        // hz = this part's pitch now, amp = its amplitude envelope now, m = its tone / drive now
        inline float tick (const PartSettings& s, const KickSettings& ks, float ms, float hz, float amp, const ModNow& m)
        {
            float raw;
            switch (s.source)
            {
                case 4:  raw = nz.tick() * 0.8f; break;                                                     // NOISE
                case 5:                                                                                    // CLICK
                {
                    const float burst = nz.tick() * std::exp (-ms / 0.6f) * 1.6f;
                    raw = burst + 0.7f * osc.tick (Sine, hz, ks.inv);
                    raw = click.bp (raw, s.tone) * 1.4f + raw * 0.3f;
                    break;
                }
                default: raw = osc.tick (s.source, hz, ks.inv); break;                                      // SINE .. SQUARE
            }
            float y = m.toneOpen ? raw : tone.lp (raw, m.tone);
            y = drive (y, m.driveG, m.driveNorm);
            const float out = raw + (y - raw) * s.mix;
            return out * amp;
        }
    private:
        Osc osc; Noise nz; Svf tone, click;
    };

    // ------------------------------------------------------------------ BODY
    class BodyGenerator
    {
    public:
        void start (const PartSettings&, float phase01, uint32_t seed) { osc.reset (phase01); nz.reset (seed); tone = {}; }
        float phase() const { return osc.ph; }
        inline float tick (const PartSettings& s, const KickSettings& ks, float hz, float amp, const ModNow& m)
        {
            const float raw = s.source == 4 ? nz.tick() * 0.8f : osc.tick (s.source, hz, ks.inv);
            float y = m.toneOpen ? raw : tone.lp (raw, m.tone);
            y = drive (y, m.driveG, m.driveNorm);
            const float out = raw + (y - raw) * s.mix;
            return out * amp;
        }
    private:
        Osc osc; Noise nz; Svf tone;
    };

    // ------------------------------------------------------------------ TAIL
    //  CHARACTER: a different saturation algorithm each (not a renamed preset):
    //   0 CLEAN    nearly clean (soft limit only)          4 METALLIC  ring modulation by an inharmonic carrier + fold
    //   1 TONAL    asymmetric tube (even harmonics)         5 BUZZ      hard square-ish clip (odd harmonics)
    //   2 DIRTY    offset asymmetric hard clip (even harmonics)  6 CRUSHED   saturation -> bit reduction + sample-rate hold
    //   3 SCREAM   saturation + a resonant band fed back
    class TailGenerator
    {
    public:
        void start (const PartSettings&, float phase01, uint32_t seed) { osc.reset (phase01); ring.reset (0.f); nz.reset (seed); tone = {}; sc = {}; dc = {}; hold = 0.f; holdN = 0; avg = 0.f; }
        float phase() const { return osc.ph; }
        inline float tick (const PartSettings& s, const KickSettings& ks, float hz, float amp, const ModNow& m)
        {
            const float raw = s.source == 5 ? nz.tick() * 0.8f : osc.tick (s.source, hz, ks.inv, s.pw);
            const float d = m.drive;
            float y;
            switch (s.character)
            {
                case 0:  y = drive (raw, 1.f + d, 1.f / std::tanh (1.f + d)); break;
                case 1:
                {
                    const float g = m.driveG, b = 0.35f;
                    y = (std::tanh (g * raw + b) - std::tanh (b)) / juce::jmax (0.05f, std::tanh (g + b) - std::tanh (b));
                    break;
                }
                case 2:
                {
                    // asymmetric hard clip: the negative half is clipped lower and harder -> strong even harmonics even
                    // when the wave is driven square; its own average is tracked and taken out (no DC thump after the kick)
                    const float x = m.driveG * (raw + 0.3f);                 // the offset moves the zero crossings: unequal halves
                    const float t = x > 0.f ? std::tanh (x) : -0.6f * std::tanh (-1.7f * x);
                    avg += ks.avgC * (t - avg);
                    y = juce::jlimit (-1.f, 1.f, (t - avg) * 1.15f);
                    break;
                }
                case 3:
                {
                    const float t = std::tanh (m.driveG * raw);
                    y = std::tanh (t + (1.5f + 2.5f * d) * sc.bp (t, ks.scream));          // (the band follows the base drive)
                    break;
                }
                case 4:
                {
                    const float t = std::tanh (m.driveG * raw);
                    const float c = ring.tick (Sine, hz * 3.17f, ks.inv);
                    const float r = t + (t * c - t) * (0.35f + 0.5f * d);
                    y = std::sin (juce::jlimit (-4.f, 4.f, r * (1.f + 1.5f * d)) * 1.5707963f);
                    break;
                }
                case 5:  y = juce::jlimit (-1.f, 1.f, m.driveG * raw + 0.15f * d) * 0.85f; break;
                default:
                {
                    if (holdN <= 0)
                    {
                        const float t = std::tanh (m.driveG * raw);
                        const float q = std::exp2 (10.f - 8.f * d);           // 10 -> 2 bits
                        hold = std::round (t * q) / q;
                        holdN = 1 + (int) (d * d * 48.f);                      // sample-rate reduction (aliasing grit)
                    }
                    --holdN;
                    y = hold;
                    break;
                }
            }
            if (! m.toneOpen) y = tone.lp (y, m.tone);
            y = dc.tick (y, ks.dcTail);
            const float out = raw + (y - raw) * s.mix;
            return out * amp;
        }
    private:
        Osc osc, ring; Noise nz; Svf tone, sc; DcBlock dc;
        float hold = 0.f, avg = 0.f; int holdN = 0;
    };

    // ------------------------------------------------------------------ SUB (clean, mono, on the note)
    class SubGenerator
    {
    public:
        void start (const PartSettings&, float phase01, uint32_t) { osc.reset (phase01); }
        float phase() const { return osc.ph; }
        inline float tick (const PartSettings& s, const KickSettings& ks, float hz, float amp, const ModNow& m)
        {
            const float raw = osc.tick (s.source == 1 ? Triangle : Sine, hz, ks.inv);
            return drive (raw, m.driveG, m.driveNorm) * amp;
        }
    private:
        Osc osc;
    };
}
