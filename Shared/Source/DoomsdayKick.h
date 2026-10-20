#pragma once
// =====================================================================
//  DOOMSDAY shared KICK synthesis (header only, from Forge of Wrath)
//    kick::Shape       drawable envelope (up to 24 points, time in per-mille of LENGTH 0..1000, value 0..1), text round trip
//    kick::KickParams  everything one hit needs
//    kick::KickVoice   one hardstyle kick, rendered at the (oversampled) rate:
//        TAIL  : oscillator (sine -> triangle -> saw) tuned to the note, pitch drop from START x note
//                -> pre TONE -> DIST (SOFT / TUBE / HARD / FOLD / RAW) with a drawable DRIVE envelope
//                -> LOW / HIGH CUT -> SCREAM (resonant mid peak) + CRUNCH (decaying mid fizz) -> AMP envelope
//        PUNCH : clean sine with its own fast pitch drop and decay
//        CLICK : short filtered noise transient
//  All real-time safe (no allocation in render).
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include "DoomsdayDSP.h"
#include "DoomsdayMod.h"
#include <array>
#include <cmath>

namespace doomsday::kick
{
    using Shape = doomsday::mod::Shape;      // drawn envelopes (see DoomsdayMod.h)

    struct KickParams
    {
        // tail
        float tailDb = 0.f, startRatio = 3.5f, dropMs = 60.f, curve = 4.f, wave = 0.f, tuneSemis = 0.f, holdMs = 0.f;   // hold: pitch stays at START before it drops
        bool  pitchDrawn = false;                   // true: the drawn PITCH envelope (0..1 = 0..+48 semitones over the note) replaces START / DROP / CURVE / HOLD
        // distortion chain: STAGE 1 -> MID EQ -> STAGE 2 -> filters
        int   distType = 1;                         // 0 SOFT, 1 TUBE, 2 HARD, 3 FOLD, 4 RAW
        float driveDb = 18.f, tone = 0.f, lowCutHz = 25.f, highCutHz = 12000.f;
        float midHz = 800.f, midDb = 0.f, midQ = 1.f;
        int   dist2Type = 0;                        // 0 OFF, 1.. = SOFT .. RAW
        float drive2Db = 6.f;
        float screamHz = 900.f, scream = 0.f, crunch = 0.f, crunchDecayMs = 120.f;
        float bodyDb = 0.f, grit = 0.f;              // post low shelf (fundamental), noisy texture riding on the tail
        float chaos = 0.f, chaosMs = 60.f, chaosHz = 2500.f;   // noise burst INTO the distortion at the start: the dense, messy modern punch
        float room = 0.f, roomSize = 0.5f;  bool roomIntoDist = true;   // small room: before the distortion (dense raw tail) or after (space)
        // punch + click
        bool  punchOn = true;
        float punchDb = -6.f, punchStartHz = 400.f, punchEndHz = 90.f, punchDropMs = 12.f, punchDecayMs = 40.f, punchDriveDb = 0.f;
        float clickDb = -14.f, clickHz = 5000.f, clickMs = 4.f;
        // envelopes over LENGTH (time axis in per-mille: 0 = hit, 1000 = LENGTH)
        float lengthMs = 410.f;
        Shape amp = Shape::make ({ { 0.f, 1.f }, { 800.f, 0.92f }, { 950.f, 0.55f }, { 1000.f, 0.f } });
        Shape drive = Shape::make ({ { 0.f, 1.f }, { 1000.f, 1.f } });
        Shape pitch = Shape::make ({ { 0.f, 0.55f }, { 40.f, 0.3f }, { 150.f, 0.08f }, { 400.f, 0.f }, { 1000.f, 0.f } });

        // coefficients that only change with the parameters: computed once per block, not per sample
        struct Cooked
        {
            double rate = 0.0;
            doomsday::dsp::SvfCoefs hc, body, scream, chaos, click;
            doomsday::dsp::BiquadCoefs mid;
            float preC = 0.f, lcC = 0.f, crC = 0.f, grC = 0.f, tailG = 1.f, bodyG = 0.f, punchG = 1.f, punchNorm = 1.f, clickG = 0.f, chaosNorm = 0.f, drive2G = 1.f;
        };
        mutable Cooked ck;
        void cook (double rate) const
        {
            auto one = [rate] (float hz) { return 1.f - std::exp (-juce::MathConstants<float>::twoPi * juce::jlimit (5.f, (float) rate * 0.45f, hz) / (float) rate); };
            ck.rate = rate;
            ck.hc = doomsday::dsp::svfCoefs (highCutHz, 1.f / 0.707f, rate);
            ck.body = doomsday::dsp::svfCoefs (140.f, 1.f / 0.707f, rate);
            ck.scream = doomsday::dsp::svfCoefs (screamHz, 1.f / 5.f, rate);
            ck.chaos = doomsday::dsp::svfCoefs (chaosHz, 1.f / 0.8f, rate);
            ck.click = doomsday::dsp::svfCoefs (clickHz, 1.f / 1.2f, rate);
            ck.mid = doomsday::dsp::BiquadCoefs::make (doomsday::dsp::BiquadCoefs::Bell, rate, midHz, juce::jmax (0.2f, midQ), midDb);
            ck.preC = one (400.f); ck.lcC = one (lowCutHz); ck.crC = one (350.f); ck.grC = one (900.f);
            ck.tailG = std::pow (10.f, tailDb / 20.f); ck.bodyG = std::pow (10.f, bodyDb / 20.f) - 1.f;
            ck.punchG = std::pow (10.f, punchDb / 20.f);
            const float pd = std::pow (10.f, punchDriveDb / 20.f);
            ck.punchNorm = punchDriveDb > 0.05f ? pd : 1.f;
            ck.clickG = std::pow (10.f, clickDb / 20.f) * 2.f;
            ck.chaosNorm = std::sqrt ((float) rate * 0.5f / juce::jmax (100.f, chaosHz)) * 1.5f;
            ck.drive2G = std::pow (10.f, drive2Db / 20.f);
        }
    };

    inline float shape (float x, int type)
    {
        switch (type)
        {
            case 0:  return std::tanh (x);                                                        // SOFT
            case 1:  return std::tanh (x + 0.3f) - 0.2913126f;                                     // TUBE (asymmetric, even harmonics)
            case 2:  return juce::jlimit (-1.f, 1.f, x);                                          // HARD
            case 3:  return std::sin (juce::jlimit (-8.f, 8.f, x) * 1.5707963f);                   // FOLD
            default: { const float y = x + 0.45f * std::abs (x); return juce::jlimit (-1.f, 1.f, y) * 0.9f; }   // RAW (rectified buzz)
        }
    }

    // small mono room (4 combs + 2 allpasses); buffers sized once in prepare()
    class KickRoom
    {
    public:
        void prepare (double rate)
        {
            sr = rate;
            for (size_t i = 0; i < 4; ++i) comb[i].assign ((size_t) (combMs[i] * 0.001 * 1.6 * sr) + 4, 0.f);
            for (size_t i = 0; i < 2; ++i) ap[i].assign ((size_t) (apMs[i] * 0.001 * sr) + 4, 0.f);
            clear();
        }
        void clear() { for (auto& c : comb) std::fill (c.begin(), c.end(), 0.f); for (auto& a : ap) std::fill (a.begin(), a.end(), 0.f); pos = {}; apos = {}; damp = {}; }
        void set (float size)
        {
            const float sc = 0.45f + juce::jlimit (0.f, 1.f, size) * 1.1f;
            for (size_t i = 0; i < 4; ++i) len[i] = juce::jlimit (8, (int) comb[i].size() - 1, (int) (combMs[i] * 0.001 * sc * sr));
            for (size_t i = 0; i < 2; ++i) alen[i] = (int) ap[i].size() - 2;
            fb = 0.62f + 0.3f * juce::jlimit (0.f, 1.f, size);
        }
        float tailMs() const { return 120.f + 900.f * (fb - 0.62f) / 0.3f; }
        inline float process (float x)
        {
            float y = 0.f;
            for (size_t i = 0; i < 4; ++i)
            {
                auto& b = comb[i]; int& p = pos[i];
                const float out = b[(size_t) p];
                damp[i] += 0.35f * (out - damp[i]);
                b[(size_t) p] = x + damp[i] * fb;
                if (++p >= len[i]) p = 0;
                y += out;
            }
            y *= 0.25f;
            for (size_t i = 0; i < 2; ++i)
            {
                auto& b = ap[i]; int& p = apos[i];
                const float d = b[(size_t) p], v = y + 0.6f * d;
                b[(size_t) p] = v; y = d - 0.6f * v;
                if (++p >= alen[i]) p = 0;
            }
            return y;
        }
    private:
        static constexpr float combMs[4] { 29.7f, 37.1f, 41.1f, 43.7f };
        static constexpr float apMs[2] { 5.0f, 1.7f };
        double sr = 192000.0;
        std::array<std::vector<float>, 4> comb;
        std::array<std::vector<float>, 2> ap;
        std::array<int, 4> pos {}, len { 8, 8, 8, 8 };
        std::array<int, 2> apos {}, alen { 8, 8 };
        std::array<float, 4> damp {};
        float fb = 0.75f;
    };

    class KickVoice
    {
    public:
        // allocates (room buffers): call outside the audio callback
        void prepare (double oversampledRate) { rate = oversampledRate; inv = (float) (1.0 / rate); room.prepare (rate); }
        void trigger (float noteHz, float velocity = 1.f)
        {
            active = true; t = 0.0; phase = 0.f; pphase = 0.f; vel = velocity; baseHz = noteHz;
            ampSeg = driveSeg = pitchSeg = 0; fade = 1.f; fadeStep = 0.f;
            pre = {}; lc = {}; hc1 = {}; hc2 = {}; sc = {}; crHp = {}; dcx = dcy = 0.f; ck = {}; ck2 = {};
            noise = 0x12345678u; gnoise = 0x9e3779b9u; cnoise = 0x7f4a7c15u; grHp = {}; bodyLp = {}; ch1 = {}; ch2 = {};
            midF.reset(); room.clear();
        }
        void release (float ms) { fadeStep = 1.f / juce::jmax (1.f, ms * 0.001f * (float) rate); }
        bool isActive() const { return active; }
        float timeMs() const { return (float) (t * 1000.0); }

        // one sample at the oversampled rate (mono); p.cook (rate) must have been called after the last parameter change
        inline float render (const KickParams& p)
        {
            if (! active) return 0.f;
            const auto& c = p.ck;
            const float ms = (float) (t * 1000.0);
            const float toPm = 1000.f / juce::jmax (10.f, p.lengthMs);
            const float endMs = p.amp.endMs() / toPm;
            const bool roomAfter = p.room > 0.001f && ! p.roomIntoDist;
            const float tailEnd = endMs + (roomAfter ? room.tailMs() : 0.f);
            if (ms > tailEnd + 1.f && ms > p.punchDecayMs * 4.f + 10.f) { active = false; return 0.f; }
            const float two = juce::MathConstants<float>::twoPi;
            // --- tail oscillator: pitch from START x note down to the note (or the drawn PITCH envelope)
            const float endHz = baseHz * std::pow (2.f, p.tuneSemis / 12.f);
            float hz;
            if (p.pitchDrawn) hz = endHz * std::pow (2.f, p.pitch.eval (ms * toPm, pitchSeg) * 4.f);
            else
            {
                const float k = juce::jmax (0.f, ms - p.holdMs) / juce::jmax (1.f, p.dropMs);
                hz = endHz * (1.f + (p.startRatio - 1.f) * std::exp (-k * p.curve));
            }
            hz = juce::jmin (hz, (float) rate * 0.2f);
            const bool inTail = ms <= endMs + 1.f;
            float y = 0.f;
            const float dEnv = p.drive.eval (ms * toPm, driveSeg);
            if (inTail || roomAfter)
            {
                phase += hz * inv; if (phase >= 1.f) phase -= 1.f;
                const float s = std::sin (two * phase);
                const float tri = 1.f - 4.f * std::abs (phase - 0.5f);
                float saw = 2.f * phase - 1.f;
                saw -= doomsday::dsp::polyBlep (phase, hz * inv);
                const float w = p.wave;
                float x = inTail ? (w < 0.5f ? s + (tri - s) * (w * 2.f) : tri + (saw - tri) * ((w - 0.5f) * 2.f)) : 0.f;
                // --- pre tone (tilt around ~400 Hz)
                const float lp = pre.lp (x, c.preC);
                x = lp * (1.f - 0.6f * p.tone) + (x - lp) * (1.f + 2.f * p.tone);
                const float gritNow = p.grit * dEnv;
                // --- CHAOS: filtered noise burst fed into the distortion -> dense, non-repeating impact
                if (p.chaos > 0.001f && ms < p.chaosMs * 6.f)
                {
                    cnoise ^= cnoise << 13; cnoise ^= cnoise >> 17; cnoise ^= cnoise << 5;
                    const float wn = (float) (int32_t) cnoise * (1.f / 2147483648.f);
                    const float nz = ch2.lp (ch1.lp (wn, c.chaos), c.chaos);
                    const float ce = std::exp (-ms / juce::jmax (2.f, p.chaosMs)) * juce::jmin (1.f, ms / 0.5f);
                    x += nz * c.chaosNorm * ce * p.chaos;
                }
                // --- ROOM into the distortion: the reflections get distorted with the tail (dense raw / psy tails)
                if (p.room > 0.001f && p.roomIntoDist) x += room.process (x) * p.room * 2.5f;
                // --- GRIT noise; a pinch before the distortion makes the edges jitter
                float gn = 0.f;
                if (gritNow > 0.001f)
                {
                    gnoise ^= gnoise << 13; gnoise ^= gnoise >> 17; gnoise ^= gnoise << 5;
                    gn = grHp.hp ((float) (int32_t) gnoise * (1.f / 2147483648.f), c.grC);
                    x += gn * gritNow * 0.012f;
                }
                // --- STAGE 1 with the drawn DRIVE envelope
                const float g = std::pow (10.f, (p.driveDb * dEnv) / 20.f);
                y = shape (x * g, p.distType);
                // --- MID EQ between the stages (shapes what STAGE 2 bites into: honk, body, bite)
                if (std::abs (p.midDb) > 0.05f) y = midF.process (y, c.mid);
                // --- STAGE 2 (rounds / thickens the tail)
                if (p.dist2Type > 0) y = shape (y * c.drive2G * (0.4f + 0.6f * dEnv), p.dist2Type - 1);
                // --- post filters
                y = lc.hp (y, c.lcC);
                y = hc2.lp (hc1.lp (y, c.hc), c.hc);
                if (std::abs (p.bodyDb) > 0.05f) y += c.bodyG * bodyLp.lp (y, c.body);
                // --- SCREAM: resonant mid peak (rawstyle 'voice')
                if (p.scream > 0.001f) y = std::tanh (y + p.scream * dEnv * 3.f * sc.bp (y, c.scream));
                // --- CRUNCH: decaying mid / high fizz on top
                if (p.crunch > 0.001f)
                {
                    const float hpv = crHp.hp (y, c.crC);
                    y += p.crunch * std::exp (-ms / juce::jmax (5.f, p.crunchDecayMs)) * std::tanh (hpv * 6.f) * 0.6f;
                }
                if (gritNow > 0.001f) y += gn * gritNow * 0.55f * (0.2f + std::abs (y));
                const float dc = y - dcx + 0.9995f * dcy; dcx = y; dcy = dc; y = dc;
                y *= (inTail ? p.amp.eval (ms * toPm, ampSeg) : 0.f) * c.tailG;
                // --- ROOM after: space around the tail (rings on after LENGTH)
                if (roomAfter) y += room.process (y) * p.room * 1.6f;
            }
            float out = y;
            // --- PUNCH: fast pitch drop, optionally driven
            if (p.punchOn && ms < p.punchDecayMs * 4.f + 10.f)
            {
                const float ph = p.punchEndHz + (p.punchStartHz - p.punchEndHz) * std::exp (-ms / juce::jmax (0.5f, p.punchDropMs) * 3.f);
                pphase += ph * inv; if (pphase >= 1.f) pphase -= 1.f;
                const float pe = std::exp (-ms / juce::jmax (1.f, p.punchDecayMs) * 2.3f) * juce::jmin (1.f, ms / 0.3f + 0.2f);
                float ps = std::sin (two * pphase);
                if (p.punchDriveDb > 0.05f) ps = std::tanh (ps * c.punchNorm) / std::tanh (c.punchNorm);
                out += ps * pe * c.punchG;
            }
            // --- CLICK: filtered noise burst
            if (p.clickDb > -59.f && ms < p.clickMs * 3.f)
            {
                noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
                const float nz = (float) (int32_t) noise * (1.f / 2147483648.f);
                out += ck.bp (nz, c.click) * std::exp (-ms / juce::jmax (0.3f, p.clickMs) * 2.3f) * c.clickG;
            }
            out *= vel;
            if (fadeStep > 0.f) { fade -= fadeStep; if (fade <= 0.f) { fade = 0.f; active = false; } out *= fade; }
            t += inv;
            return out;
        }
        void setRoom (float size) { room.set (size); }
    private:
        struct OnePole { float z = 0.f; inline float lp (float x, float c) { z += c * (x - z); return z; } inline float hp (float x, float c) { z += c * (x - z); return x - z; } };
        double rate = 192000.0, t = 0.0;
        float inv = 1.f / 192000.f, phase = 0.f, pphase = 0.f, vel = 1.f, baseHz = 49.f, fade = 1.f, fadeStep = 0.f, dcx = 0.f, dcy = 0.f;
        bool active = false;
        int ampSeg = 0, driveSeg = 0, pitchSeg = 0;
        OnePole pre, lc, crHp, grHp;
        doomsday::dsp::Svf hc1, hc2, sc, ck, ck2, bodyLp, ch1, ch2;
        doomsday::dsp::Biquad midF;
        KickRoom room;
        uint32_t noise = 0x12345678u, gnoise = 0x9e3779b9u, cnoise = 0x7f4a7c15u;
    };

    // master clipper: drive into a blend of hard and soft clipping, then a fixed ceiling
    inline float masterClip (float x, float gain, float softness, float ceiling = 0.98f)
    {
        const float y = x * gain;
        const float hard = juce::jlimit (-1.f, 1.f, y), soft = std::tanh (y);
        return (hard + softness * (soft - hard)) * ceiling;
    }
}
