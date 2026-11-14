#pragma once
// =====================================================================
//  4GE - SHAPED PUNCH (owner 2026-11-14, "no kurwa mamy to"; the Eternize kick-punch framework, Shared/RESEARCH.md)
//  The punch's TONAL layer is NOT a sine: a BROADBAND source (a snare transposed up / white noise) -> a MID boost -> an
//  asymmetric distortion (the boosted band is driven into the ceiling = the character / crack) -> TAPE STOP (the punch slows
//  down and drops in pitch towards its end) -> VOLUME SHAPER (spike + body) -> LOW CUT; + a short CLICK; + a short stereo reverb.
//  The SUB of the punch (a mono sine on the note, entering a little later) is played by the voice (it follows the note).
//  The layer does not depend on the note: it is RENDERED ONCE when its settings change (into buffers made in prepare()),
//  the voices only read it. DSP only - no GUI, no files, no allocation after prepare().
// =====================================================================
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include "../Tone/Room.h"

namespace fge::dsp::shaped
{
    struct Settings
    {
        bool on = false;
        int source = 0;              // 0 SNARE, 1 NOISE, 2 HYBRID
        float transposeSt = 14.f, midHz = 1000.f, midDb = 12.f, driveDb = 24.f, asym = 0.5f;
        float tape = 0.5f, tapeStart = 0.35f;
        float spikeMs = 6.f, body = 0.55f, bodyMs = 70.f, endMs = 140.f;
        float lowCutHz = 250.f, click = 0.5f, verb = 0.12f;
        float sub = 0.9f, subDelayMs = 25.f;
        bool tonalOnly = false;
        float tonal = 1.f, clickMs = 6.f, clickHz = 8000.f;                 // BLEND + CLICK (owner 2026-11-14: the PUNCH ENGINE)
        int subWave = 0; float subStartSt = 14.f, subDropMs = 20.f, subDecayMs = 300.f;   // SUB PUNCH (played by the voice)
        float stretch = 1.f, fshiftHz = 0.f; int formant = 0; float formantSt = 0.f, formantMix = 0.6f;   // FX (Neroz 2026)
        float key() const
        {
            return (float) source * 1.3f + transposeSt * 7.1f + midHz * 0.011f + midDb * 3.3f + driveDb * 1.7f + asym * 19.f + tape * 23.f + tapeStart * 29.f
                   + spikeMs * 2.9f + body * 31.f + bodyMs * 0.37f + endMs * 0.13f + lowCutHz * 0.017f + click * 37.f + verb * 41.f + (tonalOnly ? 43.f : 0.f)
                   + tonal * 47.f + clickMs * 5.3f + clickHz * 0.0071f
                   + stretch * 53.f + fshiftHz * 0.059f + (float) formant * 61.f + formantSt * 6.7f + formantMix * 67.f;
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
            outL.assign (n, 0.f); outR.assign (n, 0.f); a.assign (n, 0.f); b.assign (n, 0.f);
            room.prepare (rate);
            cooked = -1.f; len = 0;
        }
        // (from the engine's cook, when the settings changed) render the layer into the buffers
        void render (const Settings& s)
        {
            if (outL.empty()) return;
            const float k = s.key();
            if (k == cooked) return;
            cooked = k;
            const int n = (int) outL.size();
            const float r = std::exp2 (s.transposeSt / 12.f);
            // ---- SOURCE: a synth snare (noise + a short 190 Hz body) or noise, "transposed" = shorter and higher
            uint32_t seed = 0x9E3779B9u;
            auto rnd = [&seed] { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * 4.6566129e-10f; };
            const float nDec = 0.12f / r, bDec = 0.05f / r, bHz = 190.f * r;
            double ph = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / (float) fs;
                float x = rnd() * std::exp (-t / nDec);
                if (s.source == 2) x *= 0.7f;                                    // HYBRID: the noise a little lower + the snare body
                if (s.source != 1) { x += 0.8f * (float) std::sin (ph) * std::exp (-t / bDec); ph += 6.283185307 * bHz / fs; }
                a[(size_t) i] = x;
            }
            hp1 (a, 150.f * std::min (r, 4.f));
            // ---- STRETCH: overlap-add of 20 ms grains, read slower than written (same pitch, longer, grainier)
            if (s.stretch > 1.01f)
            {
                std::fill (b.begin(), b.end(), 0.f);
                const int G = std::max (16, (int) (0.02 * fs)), hop = G / 4;
                const double inHop = (double) hop / s.stretch;
                double ip = 0.0;
                for (int op = 0; op + G < n; op += hop, ip += inHop)
                {
                    const int i0 = (int) ip; if (i0 + G >= n) break;
                    for (int j = 0; j < G; ++j)
                    {
                        const float w = 0.5f - 0.5f * std::cos (6.2831853f * (float) j / (float) G);
                        b[(size_t) (op + j)] += a[(size_t) (i0 + j)] * w * 0.6667f;
                    }
                }
                std::swap (a, b);
            }
            // ---- CHARACTER: mid boost -> asymmetric distortion
            bell (a, s.midHz, s.midDb, 1.f);
            float pk = 1.0e-9f; for (int i = 0; i < n; ++i) pk = std::max (pk, std::abs (a[(size_t) i]));
            const float g = std::pow (10.f, s.driveDb / 20.f) / pk, neg = 1.f - 0.9f * s.asym;
            for (int i = 0; i < n; ++i) { const float x = a[(size_t) i] * g; a[(size_t) i] = x >= 0.f ? std::tanh (x) : std::tanh (x * neg); }
            // ---- FREQUENCY SHIFT (single sideband: a 90-degree allpass pair, Niemitalo) - every frequency moves by the same Hz
            if (std::abs (s.fshiftHz) > 0.5f)
            {
                static constexpr double ca[4] { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
                static constexpr double cb[4] { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };
                double xa[4][2] {}, ya[4][2] {}, xb[4][2] {}, yb[4][2] {}, prevRe = 0.0, ph = 0.0;
                const double dph = 6.283185307 * s.fshiftHz / fs;
                for (int i = 0; i < n; ++i)
                {
                    double re = a[(size_t) i], im = a[(size_t) i];
                    for (int k = 0; k < 4; ++k)
                    {
                        const double c2 = ca[k] * ca[k], y = c2 * (re + ya[k][1]) - xa[k][1];
                        xa[k][1] = xa[k][0]; xa[k][0] = re; ya[k][1] = ya[k][0]; ya[k][0] = y; re = y;
                        const double d2 = cb[k] * cb[k], z = d2 * (im + yb[k][1]) - xb[k][1];
                        xb[k][1] = xb[k][0]; xb[k][0] = im; yb[k][1] = yb[k][0]; yb[k][0] = z; im = z;
                    }
                    const double rd = prevRe; prevRe = re;                      // path 1 one sample later
                    a[(size_t) i] = (float) (rd * std::cos (ph) - im * std::sin (ph));
                    ph += dph; if (ph > 6.283185307) ph -= 6.283185307; else if (ph < 0.0) ph += 6.283185307;
                }
            }
            // ---- FORMANT: three vowel resonances (F1 / F2 / F3) in parallel, mixed with the dry punch at the same level
            if (s.formant > 0)
            {
                static constexpr float F[5][3] { { 730.f, 1090.f, 2440.f }, { 530.f, 1840.f, 2480.f }, { 270.f, 2290.f, 3010.f },
                                                  { 570.f, 840.f, 2410.f }, { 300.f, 870.f, 2240.f } };
                static constexpr float Gn[3] { 1.f, 0.5f, 0.25f };
                const float sh = std::exp2 (s.formantSt / 12.f);
                std::fill (b.begin(), b.end(), 0.f);
                for (int k = 0; k < 3; ++k)
                {
                    const double w = 6.283185307 * std::min ((double) F[s.formant - 1][k] * sh, fs * 0.45) / fs, q = 6.0, al = std::sin (w) / (2.0 * q), c = std::cos (w);
                    const double b0 = al / (1 + al), b2 = -al / (1 + al), a1 = -2 * c / (1 + al), a2 = (1 - al) / (1 + al);
                    double z1 = 0, z2 = 0;
                    for (int i = 0; i < n; ++i)
                    {
                        const double x = a[(size_t) i], y = b0 * x + z1; z1 = -a1 * y + z2; z2 = b2 * x - a2 * y;
                        b[(size_t) i] += (float) y * Gn[k];
                    }
                }
                double ed = 0, ef = 0; for (int i = 0; i < n; ++i) { ed += (double) a[(size_t) i] * a[(size_t) i]; ef += (double) b[(size_t) i] * b[(size_t) i]; }
                const float g = ef > 1.0e-12 ? (float) std::sqrt (ed / ef) : 0.f;
                for (int i = 0; i < n; ++i) a[(size_t) i] = a[(size_t) i] * (1.f - s.formantMix) + b[(size_t) i] * g * s.formantMix;
            }
            // ---- TAPE STOP: the playback rate falls from 1 to (1 - depth) after START (part of the shaped length)
            const int L = std::min (n, (int) (s.endMs * 0.001 * fs) + 8);
            {
                double pos = 0.0; const double st = s.tapeStart * L;
                for (int i = 0; i < L; ++i)
                {
                    const int i0 = (int) pos; const float fr = (float) (pos - i0);
                    b[(size_t) i] = i0 + 1 < n ? a[(size_t) i0] + (a[(size_t) i0 + 1] - a[(size_t) i0]) * fr : 0.f;
                    const double u = pos < st ? 0.0 : std::min (1.0, (pos - st) / std::max (1.0, (double) L - st));
                    pos += 1.0 - s.tape * std::pow (u, 1.5);
                }
                for (int i = L; i < n; ++i) b[(size_t) i] = 0.f;
            }
            // ---- VOLUME SHAPER: 1 ms attack, the spike, the body, out at END
            const float sp = std::max (1.5f, s.spikeMs), bm = std::max (sp + 5.f, s.bodyMs), en = std::max (bm + 5.f, s.endMs);
            for (int i = 0; i < L; ++i)
            {
                const float ms = (float) i * 1000.f / (float) fs;
                float e;
                if (ms < 1.f) e = ms;
                else if (ms < sp) e = 1.f;
                else if (ms < sp + 4.f) e = 1.f + (s.body - 1.f) * (ms - sp) / 4.f;
                else if (ms < bm) e = s.body + (0.8f * s.body - s.body) * (ms - sp - 4.f) / std::max (1.f, bm - sp - 4.f);
                else e = 0.8f * s.body * std::max (0.f, 1.f - (ms - bm) / (en - bm));
                b[(size_t) i] *= e;
            }
            // ---- corrective EQ: the lows out (3rd order)
            hp1 (b, s.lowCutHz); hp2 (b, s.lowCutHz);
            pk = 1.0e-9f; for (int i = 0; i < n; ++i) pk = std::max (pk, std::abs (b[(size_t) i]));
            for (int i = 0; i < n; ++i) b[(size_t) i] *= s.tonal / pk;
            // ---- CLICK: a laser 8 kHz -> 300 Hz in 4 ms, distorted, ~6 ms, high-passed
            std::fill (a.begin(), a.end(), 0.f);
            if (! s.tonalOnly && s.click > 0.f)
            {
                double cp = 0.0; float cpk = 1.0e-9f;
                const float cl = std::max (1.f, s.clickMs), f0 = std::max (600.f, s.clickHz);
                const int cn = std::min (n, (int) ((cl * 3.f + 5.f) * 0.001 * fs));
                for (int i = 0; i < cn; ++i)
                {
                    const float ms = (float) i * 1000.f / (float) fs;
                    const float f = f0 * std::pow (300.f / f0, std::min (1.f, ms / (cl * 0.66f)));
                    a[(size_t) i] = std::tanh (8.f * (float) std::sin (cp)) * std::exp (-ms / (cl / 3.f)); cp += 6.283185307 * f / fs;
                }
                hp2 (a, std::min (2000.f, f0 * 0.25f));
                for (int i = 0; i < cn; ++i) cpk = std::max (cpk, std::abs (a[(size_t) i]));
                for (int i = 0; i < cn; ++i) a[(size_t) i] *= s.click / cpk;
            }
            // ---- mix + the short stereo reverb on the tonal layer
            fge::dsp::room::Settings rs; rs.on = true; rs.mix = 1.f; rs.t60Ms = 220.f; rs.tone = 7000.f; rs.delayMs = 0.f;
            room.clear(); room.cook (rs);
            float wpk = 1.0e-9f;
            for (int i = 0; i < n; ++i)
            {
                float l = b[(size_t) i], rr = b[(size_t) i];
                const float dl = l, dr = rr;
                room.process (rs, l, rr);
                outL[(size_t) i] = l - dl; outR[(size_t) i] = rr - dr;            // the wet only
                wpk = std::max (wpk, std::max (std::abs (outL[(size_t) i]), std::abs (outR[(size_t) i])));
            }
            const float wg = s.verb / wpk;
            int last = 0;
            for (int i = 0; i < n; ++i)
            {
                const float m = b[(size_t) i] + a[(size_t) i];
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
        void hp1 (std::vector<float>& x, float hz) const
        {
            const float c = std::exp (-6.2831853f * hz / (float) fs); float xp = 0.f, yp = 0.f;
            for (auto& v : x) { const float y = c * (yp + v - xp); xp = v; yp = y; v = y; }
        }
        void hp2 (std::vector<float>& x, float hz) const { biquad (x, hz, 0.f, 0.7071f, true); }
        void bell (std::vector<float>& x, float hz, float db, float q) const { biquad (x, hz, db, q, false); }
        void biquad (std::vector<float>& x, float hz, float db, float q, bool hp) const
        {
            const double w = 6.283185307 * std::min ((double) hz, fs * 0.45) / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w);
            double b0, b1, b2, a0, a1, a2;
            if (hp) { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; }
            else { const double A = std::pow (10.0, db / 40.0); b0 = 1 + al * A; b1 = -2 * c; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * c; a2 = 1 - al / A; }
            b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
            double z1 = 0, z2 = 0;
            for (auto& v : x) { const double y = b0 * v + z1; z1 = b1 * v - a1 * y + z2; z2 = b2 * v - a2 * y; v = (float) y; }
        }
        double fs = 48000.0;
        std::vector<float> outL, outR, a, b;
        fge::dsp::room::Room room;
        float cooked = -1.f;
        int len = 0;
    };
}
