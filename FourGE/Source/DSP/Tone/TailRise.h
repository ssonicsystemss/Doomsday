#pragma once
// =====================================================================
//  4GE - TAIL RISE (owner 2026-11-21, FourGE/TAIL_RISE.md): the end of the TAIL grows (energy / level / aggression / brightness / pitch)
//  instead of only fading. Synced to the BPM GRID (owner's decision 2026-11-21: no MIDI note-length sync - the schedule is known at
//  NOTE ON, the same in playback and in the FL render; NOTE OFF stays fully independent) or to the TAIL (one-shot).
//    window   BPM:  start = GRID x START, end = GRID       TAIL: start = KICK LENGTH x START, end = KICK LENGTH
//    curve    LINEAR / EXPONENTIAL / S-CURVE / FAST;   release FADE (30 ms) / HARD STOP (3 ms, no click) / TAIL FOLLOW (holds to the end)
//    layers   one RISE envelope drives SEPARATE layers, each with its own amount (the MODE sets them, HYBRID = your own mix):
//             GAIN (peak-controlled), PITCH (the tail's pitch - in the generators, not a shifter), DRIVE, HARMONIC (exciter on the highs),
//             FILTER (a high shelf opening), NOISE (a texture following the tail's level)
//  Never extends the kick (the kick length gate stays). DSP only: the tail bus, no allocation, coefficients every 32 samples.
// =====================================================================
#include <cmath>
#include <algorithm>
#include <cstdint>
#include "Biquad.h"

namespace fge::dsp::rise
{
    enum Mode { Power = 0, Raw, Pitch, Harmonic, Hybrid };
    enum Curve { Linear = 0, Exponential, SCurve, Fast };
    enum Release { Fade = 0, HardStop, TailFollow };
    struct Settings
    {
        bool on = false;
        float startMs = 300.f, endMs = 400.f, kickMs = 400.f;      // the window (cooked from SYNC / GRID / START by the engine)
        int curve = Linear, release = Fade, mode = Power;
        float amount = 0.5f;
        float gain = 0.f, pitch = 0.f, drive = 0.f, harm = 0.f, filter = 0.f, noise = 0.f;   // HYBRID: the user's own layer amounts
        float pitchRangeSt = 7.f;
    };
    // the layer amounts of a MODE (x AMOUNT)
    struct Layers { float gain, pitch, drive, harm, filter, noise; };
    inline Layers layersOf (const Settings& s)
    {
        Layers l {};
        switch (s.mode)
        {
            case Power:    l = { 1.f, 0.f, 0.f, 0.f, 0.2f, 0.f }; break;
            case Raw:      l = { 0.3f, 0.f, 1.f, 0.5f, 0.f, 0.3f }; break;
            case Pitch:    l = { 0.2f, 1.f, 0.f, 0.f, 0.f, 0.f }; break;
            case Harmonic: l = { 0.f, 0.f, 0.f, 1.f, 0.6f, 0.f }; break;
            default:       l = { s.gain, s.pitch, s.drive, s.harm, s.filter, s.noise }; break;
        }
        const float a = std::clamp (s.amount, 0.f, 1.f);
        return { l.gain * a, l.pitch * a, l.drive * a, l.harm * a, l.filter * a, l.noise * a };
    }
    // the RISE envelope 0..1 at a time (a pure function of the time: known at NOTE ON)
    inline float envelope (const Settings& s, float ms)
    {
        if (! s.on || ms < s.startMs) return 0.f;
        const float len = std::max (1.f, s.endMs - s.startMs);
        if (ms <= s.endMs)
        {
            const float u = (ms - s.startMs) / len;
            switch (s.curve)
            {
                case Exponential: return u * u * u;
                case SCurve:      return u * u * (3.f - 2.f * u);
                case Fast:        return 1.f - (1.f - u) * (1.f - u) * (1.f - u);
                default:          return u;
            }
        }
        const float over = ms - s.endMs;
        if (s.release == TailFollow) return 1.f;
        const float rel = s.release == HardStop ? 3.f : 30.f;
        return std::max (0.f, 1.f - over / rel);
    }
    // the tail's pitch factor (the PITCH layer moves the tail generators themselves)
    inline float pitchFactor (const Settings& s, float ms)
    {
        if (! s.on) return 1.f;
        const auto l = layersOf (s);
        return l.pitch > 0.f ? std::exp2 (l.pitch * s.pitchRangeSt * envelope (s, ms) / 12.f) : 1.f;
    }

    class Unit
    {
    public:
        void prepare (double rate) { fs = rate; reset(); prepareFilters(); }
        void reset() { blk = 0; peak = 0.f; follow = 0.f; seed = 0x2545f491u; for (auto* b : { &hpL, &hpR, &shL, &shR, &nbp }) b->clear(); }
        // the tail bus, in place
        void process (const Settings& s, float ms, float& l, float& r)
        {
            if (! s.on) return;
            const float e = envelope (s, ms);
            const float a = std::max (std::abs (l), std::abs (r));
            if (e <= 0.f) { peak = std::max (peak * 0.9995f, a); return; }     // (the tail's own peak before the rise: the GAIN ceiling)
            if (blk-- <= 0) { blk = 32; const auto ly = layersOf (s); lay = ly; shL.shelf (fs, 3000.0, 12.0 * ly.filter * e, true); shR = shL; }
            const float ex = e;
            // DRIVE: aggression, not loudness (partly compensated)
            if (lay.drive > 0.f)
            {
                const float g = 1.f + 8.f * lay.drive * ex;
                const float comp = 1.f / std::sqrt (g);
                l = std::tanh (l * g) * comp * 1.2f; r = std::tanh (r * g) * comp * 1.2f;
            }
            // HARMONIC: an exciter on the highs (asymmetric shaper on a high-passed copy)
            if (lay.harm > 0.f)
            {
                const float hl = hpL.tick (l), hr = hpR.tick (r);
                auto sh = [] (float x) { return x >= 0.f ? std::tanh (4.f * x) : 0.7f * std::tanh (3.f * x); };
                l += lay.harm * ex * 0.8f * sh (hl * 3.f); r += lay.harm * ex * 0.8f * sh (hr * 3.f);
            }
            // FILTER: the high shelf opens
            if (lay.filter > 0.f) { l = shL.tick (l); r = shR.tick (r); }
            // NOISE: a texture that follows the tail's level
            if (lay.noise > 0.f)
            {
                follow += (a - follow) * 0.01f * (float) (176400.0 / fs);
                const float nz = nbp.tick (noise()) * follow * lay.noise * ex * 1.5f;
                l += nz; r += nz;
            }
            // GAIN: up to +12 dB, peak-controlled (a soft knee at 1.4 x the tail's own peak before the rise)
            if (lay.gain > 0.f)
            {
                const float g = std::pow (10.f, 12.f * lay.gain * ex / 20.f);
                const float ceil = std::max (0.05f, peak * 1.4f);
                auto knee = [ceil] (float x) { const float ax = std::abs (x); return ax <= 0.7f * ceil ? x : std::copysign (0.7f * ceil + 0.3f * ceil * std::tanh ((ax - 0.7f * ceil) / (0.3f * ceil)), x); };
                l = knee (l * g); r = knee (r * g);
            }
        }
    private:
        float noise() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (float) (int32_t) seed * (1.f / 2147483648.f); }
        double fs = 48000.0;
        int blk = 0;
        float peak = 0.f, follow = 0.f;
        uint32_t seed = 0x2545f491u;
        Layers lay {};
        bq::Biquad hpL, hpR, shL, shR, nbp;
    public:
        void prepareFilters() { hpL.hp (fs, 1500.0, 0.7071); hpR.hp (fs, 1500.0, 0.7071); nbp.bp (fs, 4000.0, 0.8); }
    };
}
