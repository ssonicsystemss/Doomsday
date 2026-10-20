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
#include <array>
#include <cmath>

namespace doomsday::kick
{
    struct Shape
    {
        static constexpr int maxPoints = 24;
        struct Pt { float ms = 0.f, v = 1.f; };
        std::array<Pt, maxPoints> pts {};
        int n = 0;

        static Shape make (std::initializer_list<Pt> list)
        {
            Shape s; for (auto& p : list) if (s.n < maxPoints) s.pts[(size_t) s.n++] = p;
            s.tidy(); return s;
        }
        float endMs() const { return n > 0 ? pts[(size_t) n - 1].ms : 0.f; }
        // cursor makes forward evaluation O(1)
        inline float eval (float ms, int& seg) const
        {
            if (n == 0) return 1.f;
            if (ms <= pts[0].ms) return pts[0].v;
            while (seg + 1 < n && ms > pts[(size_t) seg + 1].ms) ++seg;
            if (seg + 1 >= n) return pts[(size_t) n - 1].v;
            const auto& a = pts[(size_t) seg]; const auto& b = pts[(size_t) seg + 1];
            const float span = b.ms - a.ms;
            return span <= 0.f ? b.v : a.v + (b.v - a.v) * (ms - a.ms) / span;
        }
        float at (float ms) const { int s = 0; return eval (ms, s); }
        void tidy()
        {
            for (int i = 0; i < n; ++i) { pts[(size_t) i].ms = juce::jmax (0.f, pts[(size_t) i].ms); pts[(size_t) i].v = juce::jlimit (0.f, 1.f, pts[(size_t) i].v); }
            std::stable_sort (pts.begin(), pts.begin() + n, [] (const Pt& a, const Pt& b) { return a.ms < b.ms; });
            if (n > 0) pts[0].ms = 0.f;          // the first point is pinned to the hit
        }
        bool insert (float ms, float v)
        {
            if (n >= maxPoints) return false;
            pts[(size_t) n++] = { ms, v }; tidy(); return true;
        }
        void remove (int i) { if (i <= 0 || i >= n || n <= 2) return; for (int k = i; k < n - 1; ++k) pts[(size_t) k] = pts[(size_t) k + 1]; --n; }
        juce::String toString() const
        {
            juce::StringArray s;
            for (int i = 0; i < n; ++i) s.add (juce::String (pts[(size_t) i].ms, 2) + ":" + juce::String (pts[(size_t) i].v, 4));
            return s.joinIntoString (";");
        }
        static bool fromString (const juce::String& text, Shape& out)
        {
            Shape s;
            for (auto& tok : juce::StringArray::fromTokens (text, ";", ""))
                if (tok.containsChar (':') && s.n < maxPoints)
                    s.pts[(size_t) s.n++] = { tok.upToFirstOccurrenceOf (":", false, false).getFloatValue(), tok.fromFirstOccurrenceOf (":", false, false).getFloatValue() };
            if (s.n < 2) return false;
            s.tidy(); out = s; return true;
        }
        bool operator== (const Shape& o) const
        {
            if (n != o.n) return false;
            for (int i = 0; i < n; ++i) if (pts[(size_t) i].ms != o.pts[(size_t) i].ms || pts[(size_t) i].v != o.pts[(size_t) i].v) return false;
            return true;
        }
    };

    struct KickParams
    {
        // tail
        float tailDb = 0.f, startRatio = 3.5f, dropMs = 60.f, curve = 4.f, wave = 0.f, tuneSemis = 0.f;
        // distortion
        int   distType = 1;                         // 0 SOFT, 1 TUBE, 2 HARD, 3 FOLD, 4 RAW
        float driveDb = 18.f, tone = 0.f, lowCutHz = 25.f, highCutHz = 12000.f;
        float screamHz = 900.f, scream = 0.f, crunch = 0.f, crunchDecayMs = 120.f;
        float bodyDb = 0.f, grit = 0.f;              // post low shelf (fundamental), noisy texture riding on the tail
        // punch + click
        bool  punchOn = true;
        float punchDb = -6.f, punchStartHz = 400.f, punchEndHz = 90.f, punchDropMs = 12.f, punchDecayMs = 40.f;
        float clickDb = -14.f, clickHz = 5000.f, clickMs = 4.f;
        // envelopes over LENGTH (time axis in per-mille: 0 = hit, 1000 = LENGTH)
        float lengthMs = 410.f;
        Shape amp = Shape::make ({ { 0.f, 1.f }, { 800.f, 0.92f }, { 950.f, 0.55f }, { 1000.f, 0.f } });
        Shape drive = Shape::make ({ { 0.f, 1.f }, { 1000.f, 1.f } });
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

    class KickVoice
    {
    public:
        void prepare (double oversampledRate) { rate = oversampledRate; inv = (float) (1.0 / rate); }
        void trigger (float noteHz, float velocity = 1.f)
        {
            active = true; t = 0.0; phase = 0.f; pphase = 0.f; vel = velocity; baseHz = noteHz;
            ampSeg = driveSeg = 0; fade = 1.f; fadeStep = 0.f;
            pre = {}; lc = {}; hc1 = {}; hc2 = {}; sc = {}; crHp = {}; dcx = dcy = 0.f; ck = {}; ck2 = {};
            noise = 0x12345678u; gnoise = 0x9e3779b9u; grHp = {}; bodyLp = {};
        }
        void release (float ms) { fadeStep = 1.f / juce::jmax (1.f, ms * 0.001f * (float) rate); }
        bool isActive() const { return active; }
        float timeMs() const { return (float) (t * 1000.0); }

        // one sample at the oversampled rate (mono)
        inline float render (const KickParams& p)
        {
            if (! active) return 0.f;
            const float ms = (float) (t * 1000.0);
            const float toPm = 1000.f / juce::jmax (10.f, p.lengthMs);
            const float endMs = p.amp.endMs() / toPm;
            if (ms > endMs + 1.f && ms > p.punchDecayMs * 4.f + 10.f) { active = false; return 0.f; }
            const float two = juce::MathConstants<float>::twoPi;
            // --- tail oscillator: pitch from START x note down to the note
            const float endHz = baseHz * std::pow (2.f, p.tuneSemis / 12.f);
            const float k = (float) t * 1000.f / juce::jmax (1.f, p.dropMs);
            const float hz = endHz * (1.f + (p.startRatio - 1.f) * std::exp (-k * p.curve));
            phase += hz * inv; if (phase >= 1.f) phase -= 1.f;
            const float s = std::sin (two * phase);
            const float tri = 1.f - 4.f * std::abs (phase - 0.5f);
            float saw = 2.f * phase - 1.f;
            saw -= doomsday::dsp::polyBlep (phase, hz * inv);
            const float w = p.wave;
            float x = w < 0.5f ? s + (tri - s) * (w * 2.f) : tri + (saw - tri) * ((w - 0.5f) * 2.f);
            // --- pre tone (tilt around ~400 Hz)
            const float lp = pre.lp (x, onePole (400.f));
            x = lp * (1.f - 0.6f * p.tone) + (x - lp) * (1.f + 2.f * p.tone);
            // the drawn DRIVE envelope = how dirty the kick is over time (drive, scream and grit follow it)
            const float dEnv = p.drive.eval (ms * toPm, driveSeg);
            const float gritNow = p.grit * dEnv;
            // --- GRIT noise (white, high-passed); a pinch before the distortion makes the edges jitter
            float gn = 0.f;
            if (gritNow > 0.001f)
            {
                gnoise ^= gnoise << 13; gnoise ^= gnoise >> 17; gnoise ^= gnoise << 5;
                gn = grHp.hp ((float) (int32_t) gnoise * (1.f / 2147483648.f), onePole (900.f));
                x += gn * gritNow * 0.012f;
            }
            // --- distortion with the drawn DRIVE envelope
            const float g = std::pow (10.f, (p.driveDb * dEnv) / 20.f);
            float y = shape (x * g, p.distType);
            // --- post filters
            y = lc.hp (y, onePole (p.lowCutHz));
            const auto hcC = doomsday::dsp::svfCoefs (p.highCutHz, 1.f / 0.707f, rate);
            y = hc2.lp (hc1.lp (y, hcC), hcC);
            // --- BODY: low shelf after the distortion (brings the fundamental back up / down)
            if (std::abs (p.bodyDb) > 0.05f)
                y += (std::pow (10.f, p.bodyDb / 20.f) - 1.f) * bodyLp.lp (y, doomsday::dsp::svfCoefs (140.f, 1.f / 0.707f, rate));
            // --- SCREAM: resonant mid peak (rawstyle 'voice')
            if (p.scream > 0.001f)
            {
                const auto sC = doomsday::dsp::svfCoefs (p.screamHz, 1.f / 5.f, rate);
                const float bp = sc.bp (y, sC);
                y = std::tanh (y + p.scream * dEnv * 3.f * bp);
            }
            // --- CRUNCH: decaying mid / high fizz on top
            if (p.crunch > 0.001f)
            {
                const float hpv = crHp.hp (y, onePole (350.f));
                const float ce = std::exp (-ms / juce::jmax (5.f, p.crunchDecayMs));
                y += p.crunch * ce * std::tanh (hpv * 6.f) * 0.6f;
            }
            // --- GRIT texture riding on the waveform (louder where the tail is louder)
            if (gritNow > 0.001f)
                y += gn * gritNow * 0.55f * (0.2f + std::abs (y));
            // dc blocker
            const float dc = y - dcx + 0.9995f * dcy; dcx = y; dcy = dc; y = dc;
            const float a = p.amp.eval (ms * toPm, ampSeg);
            float out = y * a * std::pow (10.f, p.tailDb / 20.f);
            // --- PUNCH: clean, fast pitch drop
            if (p.punchOn)
            {
                const float ph = p.punchEndHz + (p.punchStartHz - p.punchEndHz) * std::exp (-ms / juce::jmax (0.5f, p.punchDropMs) * 3.f);
                pphase += ph * inv; if (pphase >= 1.f) pphase -= 1.f;
                const float pe = std::exp (-ms / juce::jmax (1.f, p.punchDecayMs) * 2.3f) * juce::jmin (1.f, ms / 0.3f + 0.2f);
                out += std::sin (two * pphase) * pe * std::pow (10.f, p.punchDb / 20.f);
            }
            // --- CLICK: filtered noise burst
            if (p.clickDb > -59.f && ms < p.clickMs * 3.f)
            {
                noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
                const float nz = (float) (int32_t) noise * (1.f / 2147483648.f);
                const auto cC = doomsday::dsp::svfCoefs (p.clickHz, 1.f / 1.2f, rate);
                const float ce = std::exp (-ms / juce::jmax (0.3f, p.clickMs) * 2.3f);
                out += ck.bp (nz, cC) * ce * std::pow (10.f, p.clickDb / 20.f) * 2.f;
            }
            out *= vel;
            if (fadeStep > 0.f) { fade -= fadeStep; if (fade <= 0.f) { fade = 0.f; active = false; } out *= fade; }
            t += inv;
            return out;
        }
    private:
        struct OnePole { float z = 0.f; inline float lp (float x, float c) { z += c * (x - z); return z; } inline float hp (float x, float c) { z += c * (x - z); return x - z; } };
        inline float onePole (float hz) const { return 1.f - std::exp (-juce::MathConstants<float>::twoPi * juce::jlimit (5.f, (float) rate * 0.45f, hz) * inv); }
        double rate = 192000.0, t = 0.0;
        float inv = 1.f / 192000.f, phase = 0.f, pphase = 0.f, vel = 1.f, baseHz = 49.f, fade = 1.f, fadeStep = 0.f, dcx = 0.f, dcy = 0.f;
        bool active = false;
        int ampSeg = 0, driveSeg = 0;
        OnePole pre, lc, crHp, grHp;
        doomsday::dsp::Svf hc1, hc2, sc, ck, ck2, bodyLp;
        uint32_t noise = 0x12345678u, gnoise = 0x9e3779b9u;
    };

    // master clipper: drive into a blend of hard and soft clipping, then a fixed ceiling
    inline float masterClip (float x, float gain, float softness, float ceiling = 0.98f)
    {
        const float y = x * gain;
        const float hard = juce::jlimit (-1.f, 1.f, y), soft = std::tanh (y);
        return (hard + softness * (soft - hard)) * ceiling;
    }
}
