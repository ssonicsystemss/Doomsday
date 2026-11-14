#pragma once
// =====================================================================
//  4GE - PVC PUNCH (owner 2026-11-14, the KLOENK): a model of the PUNCH ENGINE next to EUPHORIC (ShapedPunch.h) - its own DSP:
//  SOURCE (saw at PITCH / noise / snare / hybrid) -> CLEAN || RESONANCE (the "tube") -> MATERIAL (ring mod / flanger / comb)
//  -> BLEND -> COMPRESSOR -> DRIVE (asymmetric) + SATURATION -> EQ (low-mid / mid / high-mid, AFTER the distortion) -> SHAPE.
//  Monotone: no pitch drop - the movement of a PVC kick lives in its GATED tail. + the shared CLICK (psh_click*).
//  Rendered once per change into buffers made in prepare() (like ShapedPunch), the voices read it. DSP only, no allocation after prepare().
// =====================================================================
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include "../Tone/Room.h"

namespace fge::dsp::pvc
{
    struct Settings
    {
        int source = 0; float pitchHz = 150.f;
        float resHz = 1200.f, resQ = 8.f, resAmount = 0.7f;
        int material = 1; float mix = 0.5f, ringRatio = 2.f;
        float comp = 0.6f, driveDb = 20.f, sat = 0.3f, asym = 0.4f;
        float lowMidDb = 0.f, midDb = 3.f, highMidDb = 0.f;
        float attackMs = 0.5f, lengthMs = 70.f, decayMs = 40.f;
        float tonal = 1.f, click = 0.5f, clickMs = 6.f, clickHz = 8000.f, verb = 0.12f;
        bool tonalOnly = false;
        // TUBE model (owner 2026-11-14, from his PVC samples): 2 tube tones + a pipe comb, jump at the start, drop at the end, phaser, brightness
        int model = 1; float tubeHz = 1120.f, comb = 0.3f, jumpSt = 6.f, dropSt = -12.f, dropMs = 100.f;
        float phaser = 0.5f; int phMode = 0; float phRate = 4.f, phFb = 0.7f, brightHz = 7500.f;
        float key() const
        {
            return (float) source * 1.3f + pitchHz * 0.13f + resHz * 0.011f + resQ * 3.1f + resAmount * 17.f + (float) material * 5.7f + mix * 19.f + ringRatio * 7.3f
                   + comp * 23.f + driveDb * 1.9f + sat * 29.f + asym * 31.f + lowMidDb * 2.3f + midDb * 3.7f + highMidDb * 4.1f
                   + attackMs * 11.f + lengthMs * 0.37f + decayMs * 0.53f + tonal * 47.f + click * 37.f + clickMs * 5.3f + clickHz * 0.0071f
                   + verb * 41.f + (tonalOnly ? 43.f : 0.f)
                   + (float) model * 53.f + tubeHz * 0.017f + comb * 59.f + jumpSt * 6.1f + dropSt * 6.7f + dropMs * 0.29f
                   + phaser * 61.f + (float) phMode * 67.f + phRate * 7.9f + phFb * 71.f + brightHz * 0.0013f;
        }
    };

    class Renderer
    {
    public:
        static constexpr float maxMs = 600.f;
        void prepare (double rate)
        {
            fs = rate;
            const size_t n = (size_t) (rate * maxMs * 0.001) + 16;
            outL.assign (n, 0.f); outR.assign (n, 0.f); a.assign (n, 0.f); b.assign (n, 0.f); c.assign (n, 0.f);
            room.prepare (rate);
            cooked = -1.f; len = 0;
        }
        void render (const Settings& s)
        {
            if (outL.empty()) return;
            const float k = s.key();
            if (k == cooked) return;
            cooked = k;
            const int n = (int) outL.size();
            const int L = std::min (n, (int) ((s.attackMs + s.lengthMs + s.decayMs + 5.f) * 0.001f * (float) fs));
            if (s.model == 1) tube (s, n, L);
            else {
            // ---- SOURCE (monotone)
            uint32_t seed = 0x2545F491u;
            auto rnd = [&seed] { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * 4.6566129e-10f; };
            double ph = 0.0, bp = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / (float) fs;
                const float saw = (float) (2.0 * ph - 1.0), noise = rnd();
                float x = 0.f;
                switch (s.source)
                {
                    case 1: x = noise; break;
                    case 2: x = noise * std::exp (-t / 0.12f) + 0.8f * (float) std::sin (bp) * std::exp (-t / 0.05f); break;
                    case 3: x = 0.6f * saw + 0.4f * noise; break;
                    default: x = saw; break;
                }
                a[(size_t) i] = x;
                ph += s.pitchHz / fs; if (ph >= 1.0) ph -= 1.0;
                bp += 6.283185307 * 190.0 / fs;
            }
            // ---- RESONANCE (the tube): a band-pass at RES FREQ
            for (int i = 0; i < n; ++i) b[(size_t) i] = a[(size_t) i];
            bandpass (b, s.resHz, s.resQ);
            for (int i = 0; i < n; ++i) b[(size_t) i] = b[(size_t) i] * s.resAmount + a[(size_t) i] * (1.f - s.resAmount) * 0.25f;
            // ---- MATERIAL
            if (s.material == 0)                                                        // RING MOD: x PITCH x RATIO
            {
                double rp = 0.0; const double dr = 6.283185307 * s.pitchHz * s.ringRatio / fs;
                for (int i = 0; i < n; ++i) { b[(size_t) i] *= (float) std::sin (rp); rp += dr; }
            }
            else                                                                         // FLANGER (moving) / COMB (fixed): delay = 1 / RES FREQ
            {
                const double d0 = fs / std::max (60.f, s.resHz);
                for (int i = 0; i < n; ++i)
                {
                    const double d = s.material == 1 ? d0 * (1.0 + 0.15 * std::sin (6.283185307 * 3.0 * i / fs)) : d0;
                    const double j = (double) i - d; const int j0 = (int) std::floor (j);
                    float dl = 0.f;
                    if (j0 >= 0 && j0 + 1 < i) { const float fr = (float) (j - j0); dl = c[(size_t) j0] * (1.f - fr) + c[(size_t) j0 + 1] * fr; }
                    c[(size_t) i] = b[(size_t) i] + 0.85f * dl;
                }
                for (int i = 0; i < n; ++i) b[(size_t) i] = c[(size_t) i] * 0.15f;
            }
            // ---- BLEND (the two at the same level)
            const float ra = rms (a, L), rb = rms (b, L), gb = rb > 1.0e-9f ? ra / rb : 0.f;
            for (int i = 0; i < n; ++i) a[(size_t) i] = a[(size_t) i] * (1.f - s.mix) + b[(size_t) i] * gb * s.mix;
            // ---- COMPRESSOR before the drive (an envelope follower, even level into the distortion)
            {
                const float co = std::exp (-1.f / (0.004f * (float) fs)); float e = 0.f, emax = 1.0e-4f;
                for (int i = 0; i < n; ++i) { e = co * e + (1.f - co) * std::abs (a[(size_t) i]); c[(size_t) i] = e; emax = std::max (emax, e); }
                for (int i = 0; i < n; ++i) a[(size_t) i] *= std::pow ((c[(size_t) i] + 1.0e-4f) / emax, -s.comp * 0.9f);
            }
            float pk = 1.0e-9f; for (int i = 0; i < L; ++i) pk = std::max (pk, std::abs (a[(size_t) i]));
            // ---- DRIVE (asymmetric) + SATURATION
            const float g = std::pow (10.f, s.driveDb / 20.f) / pk, neg = 1.f - 0.9f * s.asym;
            for (int i = 0; i < n; ++i)
            {
                const float x = a[(size_t) i] * g, y = x >= 0.f ? std::tanh (x) : std::tanh (x * neg);
                a[(size_t) i] = (1.f - s.sat) * y + s.sat * std::tanh (2.f * y);
            }
            }
            // ---- EQ after the distortion
            bell (a, 300.f, s.lowMidDb, 0.9f); bell (a, 1000.f, s.midDb, 0.9f); bell (a, 3000.f, s.highMidDb, 0.9f);
            // ---- SHAPE: attack, hold (PUNCH LENGTH), decay; then the lows out (the SUB PUNCH carries them)
            for (int i = 0; i < n; ++i)
            {
                const float ms = (float) i * 1000.f / (float) fs;
                float e;
                if (ms < s.attackMs) e = ms / std::max (0.05f, s.attackMs);
                else if (ms < s.attackMs + s.lengthMs) e = 1.f - 0.15f * (ms - s.attackMs) / std::max (1.f, s.lengthMs);
                else e = std::max (0.f, 0.85f * (1.f - (ms - s.attackMs - s.lengthMs) / std::max (1.f, s.decayMs)));
                a[(size_t) i] *= e;
            }
            highpass (a, 120.f);
            float pk = 1.0e-9f; for (int i = 0; i < n; ++i) pk = std::max (pk, std::abs (a[(size_t) i]));
            for (int i = 0; i < n; ++i) a[(size_t) i] *= s.tonal / pk;
            // ---- CLICK (shared with EUPHORIC: a short distorted laser)
            std::fill (b.begin(), b.end(), 0.f);
            if (! s.tonalOnly && s.click > 0.f)
            {
                const float cl = std::max (1.f, s.clickMs), f0 = std::max (600.f, s.clickHz);
                const int cn = std::min (n, (int) ((cl * 3.f + 5.f) * 0.001 * fs));
                double cp = 0.0; float cpk = 1.0e-9f;
                for (int i = 0; i < cn; ++i)
                {
                    const float ms = (float) i * 1000.f / (float) fs, f = f0 * std::pow (300.f / f0, std::min (1.f, ms / (cl * 0.66f)));
                    b[(size_t) i] = std::tanh (8.f * (float) std::sin (cp)) * std::exp (-ms / (cl / 3.f)); cp += 6.283185307 * f / fs;
                }
                highpass (b, std::min (2000.f, f0 * 0.25f));
                for (int i = 0; i < cn; ++i) cpk = std::max (cpk, std::abs (b[(size_t) i]));
                for (int i = 0; i < cn; ++i) b[(size_t) i] *= s.click / cpk;
            }
            // ---- a short stereo room on the punch (shared REVERB), then out
            fge::dsp::room::Settings rs; rs.on = true; rs.mix = 1.f; rs.t60Ms = 220.f; rs.tone = 7000.f; rs.delayMs = 0.f;
            room.clear(); room.cook (rs);
            float wpk = 1.0e-9f;
            for (int i = 0; i < n; ++i)
            {
                float l = a[(size_t) i], r = a[(size_t) i];
                room.process (rs, l, r);
                outL[(size_t) i] = l - a[(size_t) i]; outR[(size_t) i] = r - a[(size_t) i];
                wpk = std::max (wpk, std::max (std::abs (outL[(size_t) i]), std::abs (outR[(size_t) i])));
            }
            const float wg = s.verb / wpk;
            int last = 0;
            for (int i = 0; i < n; ++i)
            {
                const float m = a[(size_t) i] + b[(size_t) i];
                outL[(size_t) i] = m + outL[(size_t) i] * wg; outR[(size_t) i] = m + outR[(size_t) i] * wg;
                if (std::abs (outL[(size_t) i]) > 1.0e-4f || std::abs (outR[(size_t) i]) > 1.0e-4f) last = i;
            }
            len = last + 1;
        }
        int length() const { return len; }
        float lengthMs() const { return (float) len * 1000.f / (float) fs; }
        const float* left() const { return outL.data(); }
        const float* right() const { return outR.data(); }
    private:
        // TUBE: the source + phaser + drive + brightness into a (the shape / EQ / click / room are shared with KLOENK)
        void tube (const Settings& s, int n, int L)
        {
            const float endMs = s.attackMs + s.lengthMs;
            double p1 = 0.0, p2 = 0.0, ps = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const float ms = (float) i * 1000.f / (float) fs;
                float f = s.tubeHz * (1.f + (std::exp2 (s.jumpSt / 12.f) - 1.f) * std::exp (-ms / 6.f));       // the "ci" of "ciup"
                const float u = std::clamp ((ms - (endMs - s.dropMs)) / std::max (1.f, s.dropMs), 0.f, 1.f);
                f *= std::exp2 (s.dropSt * u * u / 12.f);                                                         // the fall at the end
                a[(size_t) i] = ((float) std::sin (p1) + 0.6f * (float) std::sin (p2)) / 1.6f;                   // 2 tube tones (1 : 1.59)
                b[(size_t) i] = (float) (2.0 * ps - 1.0);                                                         // the saw for the comb
                c[(size_t) i] = f;
                p1 += 6.283185307 * f / fs; p2 += 6.283185307 * f * 1.59 / fs; ps += f / fs; if (ps >= 1.0) ps -= 1.0;
            }
            // the pipe: a comb with NEGATIVE feedback at half the period (a closed pipe = odd harmonics), the delay follows the pitch
            for (int i = 0; i < n; ++i)
            {
                const double j = (double) i - fs / std::max (50.f, c[(size_t) i]) / 2.0; const int j0 = (int) std::floor (j);
                const float dl = j0 >= 0 ? b[(size_t) j0] : 0.f;               // (b already holds the comb output below j0 < i)
                b[(size_t) i] = b[(size_t) i] - 0.6f * dl;
            }
            highpass (b, 300.f); lowpass (b, 4000.f);
            float pa = 1.0e-9f, pb = 1.0e-9f;
            for (int i = 0; i < L; ++i) { pa = std::max (pa, std::abs (a[(size_t) i])); pb = std::max (pb, std::abs (b[(size_t) i])); }
            for (int i = 0; i < n; ++i) a[(size_t) i] = (1.f - s.comb) * a[(size_t) i] / pa + s.comb * b[(size_t) i] / pb;
            // PHASER: 6 all-pass stages + feedback; SWEEP = one pass high -> low over the punch, LFO = moving
            if (s.phaser > 0.f)
            {
                float z[6] {}, last = 0.f, ap = 0.f;
                for (int i = 0; i < n; ++i)
                {
                    if ((i & 15) == 0)
                    {
                        const float ms = (float) i * 1000.f / (float) fs;
                        const float pos = s.phMode == 0 ? std::clamp (1.f - ms / std::max (1.f, endMs), 0.f, 1.f)
                                                        : 0.5f + 0.5f * (float) std::sin (6.283185307 * s.phRate * i / fs);
                        const double fc = 800.0 * std::pow (6000.0 / 800.0, (double) pos), tn = std::tan (3.14159265 * fc / fs);
                        ap = (float) ((tn - 1.0) / (tn + 1.0));
                    }
                    float v = a[(size_t) i] + s.phFb * last;
                    for (auto& zz : z) { const float o = ap * v + zz; zz = v - ap * o; v = o; }
                    last = v;
                    a[(size_t) i] = (1.f - 0.5f * s.phaser) * a[(size_t) i] + 0.5f * s.phaser * v;
                }
            }
            float pk = 1.0e-9f; for (int i = 0; i < L; ++i) pk = std::max (pk, std::abs (a[(size_t) i]));
            const float g = std::pow (10.f, s.driveDb / 20.f) / pk;
            for (int i = 0; i < n; ++i) a[(size_t) i] = std::tanh (g * a[(size_t) i]);
            lowpass (a, s.brightHz); lowpass (a, s.brightHz);
        }
        void lowpass (std::vector<float>& x, float hz) const
        {
            const double w = 6.283185307 * std::min ((double) hz, fs * 0.45) / fs, al = std::sin (w) / (2.0 * 0.7071), c0 = std::cos (w);
            biquad (x, (1 - c0) / 2, 1 - c0, (1 - c0) / 2, 1 + al, -2 * c0, 1 - al);
        }
        static float rms (const std::vector<float>& x, int L)
        {
            double e = 0.0; for (int i = 0; i < L; ++i) e += (double) x[(size_t) i] * x[(size_t) i];
            return (float) std::sqrt (e / std::max (1, L));
        }
        void biquad (std::vector<float>& x, double b0, double b1, double b2, double a0, double a1, double a2) const
        {
            b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
            double z1 = 0, z2 = 0;
            for (auto& v : x) { const double y = b0 * v + z1; z1 = b1 * v - a1 * y + z2; z2 = b2 * v - a2 * y; v = (float) y; }
        }
        void bandpass (std::vector<float>& x, float hz, float q) const
        {
            const double w = 6.283185307 * std::min ((double) hz, fs * 0.45) / fs, al = std::sin (w) / (2.0 * std::max (0.3f, q)), c0 = std::cos (w);
            biquad (x, al, 0.0, -al, 1 + al, -2 * c0, 1 - al);
        }
        void highpass (std::vector<float>& x, float hz) const
        {
            const double w = 6.283185307 * std::min ((double) hz, fs * 0.45) / fs, al = std::sin (w) / (2.0 * 0.7071), c0 = std::cos (w);
            biquad (x, (1 + c0) / 2, -(1 + c0), (1 + c0) / 2, 1 + al, -2 * c0, 1 - al);
        }
        void bell (std::vector<float>& x, float hz, float db, float q) const
        {
            if (std::abs (db) < 0.01f) return;
            const double A = std::pow (10.0, db / 40.0), w = 6.283185307 * hz / fs, al = std::sin (w) / (2.0 * q), c0 = std::cos (w);
            biquad (x, 1 + al * A, -2 * c0, 1 - al * A, 1 + al / A, -2 * c0, 1 - al / A);
        }
        double fs = 48000.0;
        std::vector<float> outL, outR, a, b, c;
        fge::dsp::room::Room room;
        float cooked = -1.f;
        int len = 0;
    };
}
