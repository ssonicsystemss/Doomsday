#pragma once
// =====================================================================
//  4GE - TOK (owner 2026-11-16): its own layer BEFORE the PUNCH ENGINE, the "tok" - shorter and more percussive than the punch,
//  longer than the click. Not a "small punch", not a macro; the CLICK stays a separate layer (WOOD TOK + a hard CLICK + RAW PUNCH).
//
//  ONE TOK CORE, six SOURCES (owner: "all types use the same TOK CORE, the types only give another source"):
//    TOK CORE = PITCH MODE + PITCH -> PITCH ENVELOPE (START -> fast drop -> END, DROP TIME) -> SOURCE -> TONE
//               -> AMP ENVELOPE (ATTACK = the peak, DECAY = the fall, LENGTH) -> DRIVE (saturation) -> LEVEL
//    PITCH MODE  FIXED (G3 + PITCH, never the MIDI note - the default) / FOLLOW MIDI (the played note in the tok register) /
//                RELATIVE (the played note in the tok register + PITCH)
//    SOURCES     SINE    a clean sine: the most neutral tok
//                TRI     a clean triangle: more harmonics, still soft
//                SQUARE  a square through a BAND whose TONE goes hollow / wooden <-> full / harsh
//                PUP     a saw on the pitch envelope (START +12 / +24 -> fast drop -> END 0 / -12) = the "pup!"
//                909     a hybrid: a SINE BODY on the pitch envelope (the "tok") + a band-passed NOISE CLICK only at its start
//                        (the "tik"), on its own 2 ms envelope - not a sine with noise mixed in at a fixed ratio
//                WOOD    an impulse -> BAND-PASS -> RESONANCE (two ringing modes 1 : 2.76, a struck bar) on the tok pitch (PITCH moves
//                        the resonance) -> the short core decay -> the core DRIVE
//  DSP only; reset() per kick, no allocation.
// =====================================================================
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "../Tone/Biquad.h"

namespace fge::dsp::front
{
    struct TokSettings
    {
        bool on = false;
        int type = 0;                 // SINE TRI SQUARE PUP 909 WOOD
        int pitchMode = 0;            // FIXED FOLLOW-MIDI RELATIVE
        float gain = 0.f;             // linear
        float tuneSt = 0.f, lengthMs = 45.f, peakMs = 4.f, fade = 0.5f, tone = 0.5f;
        float penvStartSt = 12.f, penvEndSt = 0.f, penvMs = 5.f, drive = 0.2f;
    };
    constexpr float tokBaseHz = 196.f;   // G3 = PITCH 0 (FIXED)

    // the tok's base pitch: FIXED = G3 + PITCH; FOLLOW MIDI = the note moved by octaves into the tok register (around G3);
    // RELATIVE = that + PITCH
    inline float tokPitchHz (const TokSettings& s, float noteHz)
    {
        if (s.pitchMode == 0 || noteHz <= 0.f) return tokBaseHz * std::exp2 (s.tuneSt / 12.f);
        float f = noteHz;
        while (f < tokBaseHz * 0.7071f) f *= 2.f;
        while (f >= tokBaseHz * 1.4142f) f *= 0.5f;
        return s.pitchMode == 2 ? f * std::exp2 (s.tuneSt / 12.f) : f;
    }

    class TokUnit
    {
    public:
        void prepare (double rate) { fs = rate; reset(); }
        void reset() { ph = 0.0; seed = 0x7a3b11c5u; for (auto* b : { &lp, &bhp, &blp, &m1, &m2, &nbp, &wbp }) b->clear(); blk = 0; }

        float process (const TokSettings& s, float ms, float noteHz)
        {
            if (! s.on || s.gain <= 0.f || ms > s.lengthMs + 3.f) return 0.f;
            // ---- the core: pitch + pitch envelope
            const float base = tokPitchHz (s, noteHz);
            const float st = s.penvEndSt + (s.penvStartSt - s.penvEndSt) * std::exp (-ms / std::max (0.3f, s.penvMs));
            const float hz = std::min (base * std::exp2 (st / 12.f), (float) fs * 0.4f);
            if (blk-- <= 0)
            {
                blk = 8;                                                                   // (the filters follow the pitch envelope)
                lp.lp (fs, std::clamp (400.0 * std::pow (2.0, s.tone * 5.5), 200.0, fs * 0.45), 0.7071);           // core TONE
                // SQUARE: the band - TONE 0 = a narrow band round the 2nd harmonic (hollow / wooden), 1 = wide open (full / harsh)
                bhp.hp (fs, std::clamp ((double) hz * (0.9 - 0.4 * s.tone), 20.0, fs * 0.4), 0.7071);
                blp.lp (fs, std::clamp ((double) hz * (2.5 + 60.0 * s.tone * s.tone), 60.0, fs * 0.45), 1.6 - 0.9 * s.tone);
                // WOOD: band-pass on the strike, then the resonant modes (TONE = brighter strike, a touch less ring)
                wbp.bp (fs, std::clamp ((double) hz * (1.5 + 2.0 * s.tone), 60.0, fs * 0.4), 1.2);
                m1.bp (fs, std::clamp ((double) hz, 40.0, fs * 0.4), 22.0 - 8.0 * s.tone);
                m2.bp (fs, std::clamp ((double) hz * 2.76, 40.0, fs * 0.4), 16.0 - 6.0 * s.tone);
                nbp.bp (fs, std::clamp (2500.0 + 5000.0 * s.tone, 500.0, fs * 0.4), 1.4);  // 909: the noise click's band
            }
            // ---- the amp envelope (ATTACK = peak, DECAY = fall, LENGTH)
            const float len = std::max (5.f, s.lengthMs), pk = std::clamp (s.peakMs, 0.3f, len * 0.5f);
            float env = ms < pk ? std::sin (1.5707963f * ms / pk) : std::pow (std::max (0.f, 1.f - (ms - pk) / (len - pk)), 0.6f + 3.f * (1.f - s.fade));
            if (ms > len) env = 0.f;
            // ---- the source
            float x = 0.f, post = 0.f;                                                     // post = outside the amp envelope (909's tik)
            switch (s.type)
            {
                case 0: x = (float) std::sin (6.283185307 * step (hz)); break;                                   // SINE
                case 1: x = (float) (4.0 * std::abs (step (hz) - 0.5) - 1.0); break;                              // TRI
                case 2: x = 2.2f * blp.tick (bhp.tick (step (hz) < 0.5 ? 1.f : -1.f)); break;                     // SQUARE -> BAND
                case 3: x = (float) (2.0 * step (hz) - 1.0); break;                                               // PUP: a saw on the pitch env
                case 4:                                                                                           // 909: SINE BODY + NOISE CLICK
                    x = (float) std::sin (6.283185307 * step (hz * (1.0 + 1.2 * std::exp (-ms / 1.2))));   // its own extra fast bend on top
                    x = std::tanh (1.8f * x) / std::tanh (1.8f);                                             // + a rounder, fatter body
                    post = 0.9f * nbp.tick (noise()) * std::exp (-ms / 0.8f) * std::min (1.f, ms / 0.1f);
                    break;
                default:                                                                                          // WOOD: impulse -> BP -> RESONANCE
                {
                    const float exc = ms < 0.6f ? wbp.tick (noise()) * 6.f : wbp.tick (0.f);
                    x = 18.f * m1.tick (exc) + 11.f * m2.tick (exc);                                    // (a resonator is quiet: +15 dB to sit with the other sources)
                    env = ms > len ? 0.f : std::pow (std::max (0.f, 1.f - ms / len), 0.6f + 3.f * (1.f - s.fade));   // (struck: no attack ramp)
                    break;
                }
            }
            float y = lp.tick (x * env + post);
            const float g = 1.f + 14.f * s.drive;                                          // DRIVE (the core saturation)
            y = std::tanh (y * g) / std::tanh (g);
            return s.gain * y;
        }
    private:
        double step (double f) { ph += f / fs; if (ph >= 1.0) ph -= 1.0; return ph; }
        float noise() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.f / 2147483648.f); }
        double fs = 48000.0, ph = 0.0;
        uint32_t seed = 0x7a3b11c5u;
        int blk = 0;
        bq::Biquad lp, bhp, blp, m1, m2, nbp, wbp;
    };
}
