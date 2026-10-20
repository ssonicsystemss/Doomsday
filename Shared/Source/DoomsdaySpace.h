#pragma once
// =====================================================================
//  DOOMSDAY shared SPACE blocks (header only, from Abyss of Echoes)
//    space::Ducker        wet ducking from the dry signal (wet gets out of the way while you play)
//    space::EchoDelay     stereo / ping-pong / wide delay: tempo sync or ms, feedback filters, drive, tape wobble
//    space::Shifter       granular octave-up pitch shifter (for shimmer)
//    space::FdnReverb     8-line feedback-delay-network reverb: pre-delay, diffusion, size, decay (RT60),
//                         damping, low cut, modulation, shimmer, freeze, width
//    space::MonoBass      sides below a frequency removed (bass in mono)
//  All memory is allocated in prepare(); process() is real-time safe.
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <array>
#include <vector>

namespace doomsday::space
{
    // ------------------------------------------------------------ small helpers
    struct OnePole
    {
        float z = 0.f;
        inline float lp (float x, float c) { z += c * (x - z); return z; }
        inline float hp (float x, float c) { z += c * (x - z); return x - z; }
    };
    inline float onePoleCoef (float hz, double sr) { return 1.f - std::exp (-juce::MathConstants<float>::twoPi * juce::jlimit (5.f, (float) sr * 0.45f, hz) / (float) sr); }

    struct Line
    {
        std::vector<float> buf;
        int w = 0, size = 1;
        void prepare (int n) { size = juce::jmax (4, n); buf.assign ((size_t) size, 0.f); w = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.f); w = 0; }
        inline void push (float x) { buf[(size_t) w] = x; if (++w >= size) w = 0; }
        // delay in samples (>= 1), cubic interpolation
        inline float read (float d) const
        {
            d = juce::jlimit (1.f, (float) size - 3.f, d);
            float rp = (float) w - d;
            while (rp < 0.f) rp += (float) size;
            const int i1 = (int) rp; const float f = rp - (float) i1;
            const int i0 = (i1 - 1 + size) % size, i2 = (i1 + 1) % size, i3 = (i1 + 2) % size;
            const float y0 = buf[(size_t) i0], y1 = buf[(size_t) i1], y2 = buf[(size_t) i2], y3 = buf[(size_t) i3];
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }
        inline float readInt (int d) const { int rp = w - d; while (rp < 0) rp += size; return buf[(size_t) rp]; }
    };

    struct Allpass
    {
        Line line; int len = 1;
        void prepare (int n) { len = juce::jmax (1, n); line.prepare (n + 2); }
        void clear() { line.clear(); }
        inline float process (float x, float g)
        {
            const float d = line.readInt (len);
            const float y = -g * x + d;
            line.push (x + g * y);
            return y;
        }
    };

    // ------------------------------------------------------------ ducking
    struct Ducker
    {
        float env = 0.f, att = 0.f, rel = 0.f;
        void prepare (double sr) { att = 1.f - std::exp (-1.f / (0.004f * (float) sr)); rel = 1.f - std::exp (-1.f / (0.16f * (float) sr)); env = 0.f; }
        // feed the dry level, get the wet gain for 'amount' 0..1
        inline float gain (float dryL, float dryR, float amount)
        {
            const float pk = juce::jmax (std::abs (dryL), std::abs (dryR));
            env += (pk > env ? att : rel) * (pk - env);
            if (amount <= 0.f) return 1.f;
            return 1.f / (1.f + amount * 10.f * env);
        }
    };

    // ------------------------------------------------------------ delay
    struct EchoParams
    {
        int   mode = 1;              // 0 STEREO, 1 PING-PONG, 2 WIDE (right side 1.5x time)
        float timeMs = 300.f;        // already resolved from sync / ms
        float feedback = 0.4f, lowCutHz = 200.f, highCutHz = 7000.f, mod = 0.2f, drive = 0.f;
    };

    class EchoDelay
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            const int n = (int) (sr * 5.2) + 8;
            l.prepare (n); r.prepare (n);
            curL = curR = (float) (0.3 * sr);
            reset();
        }
        void reset() { l.clear(); r.clear(); hpL = hpR = lpL = lpR = {}; ph = 0.f; }

        // in: dry stereo, out: wet stereo
        inline void process (float inL, float inR, float& outL, float& outR, const EchoParams& p)
        {
            const float maxD = (float) (sr * 5.0);
            const float tL = juce::jlimit (4.f, maxD, p.timeMs * 0.001f * (float) sr);
            const float tR = p.mode == 2 ? juce::jlimit (4.f, maxD, tL * 1.5f) : tL;
            // smooth time changes (a short tape-like glide instead of clicks)
            curL += glide * (tL - curL); curR += glide * (tR - curR);
            ph += 0.55f / (float) sr; if (ph >= 1.f) ph -= 1.f;
            const float wob = p.mod * (0.0025f * (float) sr) * (0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * ph));
            float yL = l.read (curL + wob), yR = r.read (curR + wob * 0.8f);

            const float hpc = onePoleCoef (p.lowCutHz, sr), lpc = onePoleCoef (p.highCutHz, sr);
            yL = lpL.lp (hpL.hp (yL, hpc), lpc);
            yR = lpR.lp (hpR.hp (yR, hpc), lpc);
            if (p.drive > 0.001f)
            {
                const float k = 1.f + p.drive * 4.f, norm = 1.f / std::tanh (k);
                yL = std::tanh (yL * k) * norm; yR = std::tanh (yR * k) * norm;
            }
            const float fb = juce::jlimit (0.f, 0.97f, p.feedback);
            if (p.mode == 1)
            {
                // ping-pong: mono in on the left, then bounce left <-> right
                l.push (0.5f * (inL + inR) + yR * fb);
                r.push (yL * fb);
            }
            else
            {
                l.push (inL + yL * fb);
                r.push (inR + yR * fb);
            }
            outL = yL; outR = yR;
        }
    private:
        double sr = 44100.0;
        Line l, r;
        OnePole hpL, hpR, lpL, lpR;
        float curL = 1000.f, curR = 1000.f, ph = 0.f;
        static constexpr float glide = 0.0004f;
    };

    // ------------------------------------------------------------ octave-up granular shifter (shimmer)
    class Shifter
    {
    public:
        void prepare (double sr)
        {
            grain = juce::jmax (256, (int) (sr * 0.06));
            line.prepare (grain * 2 + 8);
            reset();
        }
        void reset() { line.clear(); phase = 0.f; }
        inline float process (float x)
        {
            line.push (x);
            // two read heads moving at 2x speed through a window of 'grain' samples, 180 degrees apart
            phase += 1.f / (float) grain; if (phase >= 1.f) phase -= 1.f;
            float y = 0.f;
            for (int k = 0; k < 2; ++k)
            {
                float p = phase + 0.5f * (float) k; if (p >= 1.f) p -= 1.f;
                const float d = (float) grain * (1.f - p) + 2.f;        // delay shrinks -> pitch up one octave
                const float win = std::sin (juce::MathConstants<float>::pi * p);
                y += line.read (d) * win * win;
            }
            return y;
        }
    private:
        Line line; int grain = 2048; float phase = 0.f;
    };

    // ------------------------------------------------------------ FDN reverb
    struct ReverbParams
    {
        float preDelayMs = 20.f, size = 0.6f, decayS = 2.5f, dampHz = 7000.f, lowCutHz = 150.f;
        float mod = 0.4f, shimmer = 0.f, width = 1.f, diffusion = 0.75f;
        bool  freeze = false;
    };

    class FdnReverb
    {
    public:
        static constexpr int N = 8;
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            const float k = (float) (sr / 44100.0);
            preL.prepare ((int) (sr * 0.32) + 8); preR.prepare ((int) (sr * 0.32) + 8);
            static const int apLen[4] = { 142, 107, 379, 277 };
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 4; ++i)
                    diff[c][(size_t) i].prepare ((int) ((float) apLen[i] * k * (c == 0 ? 1.f : 1.13f)));
            for (int i = 0; i < N; ++i) lines[(size_t) i].prepare ((int) (sr * 0.22) + 64);   // room for size 2.0 + mod
            shL.prepare (sr); shR.prepare (sr);
            reset();
        }
        void reset()
        {
            preL.clear(); preR.clear();
            for (auto& c : diff) for (auto& a : c) a.clear();
            for (auto& l : lines) l.clear();
            for (auto& d : damp) d = {};
            for (auto& h : lowc) h = {};
            shL.reset(); shR.reset();
            shFbL = shFbR = 0.f;
            for (int i = 0; i < N; ++i) lfo[(size_t) i] = (float) i / (float) N;
        }

        // in: stereo send, out: stereo reverb (100% wet)
        inline void process (float inL, float inR, float& outL, float& outR, const ReverbParams& p)
        {
            // base line lengths in ms (mutually prime-ish), scaled by SIZE (0..1 -> 0.35x .. 1.9x)
            static const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
            static const float lfoHz[N]  = { 0.31f, 0.43f, 0.57f, 0.67f, 0.79f, 0.89f, 0.97f, 1.07f };
            const float scale = 0.35f + 1.55f * p.size;
            const float inGain = p.freeze ? 0.f : 1.f;

            // pre-delay + shimmer feedback + input diffusion
            preL.push (inL * inGain + shFbL);
            preR.push (inR * inGain + shFbR);
            const float pd = juce::jmax (1.f, p.preDelayMs * 0.001f * (float) sr);
            float xL = preL.read (pd), xR = preR.read (pd);
            const float g = 0.35f + 0.35f * p.diffusion;
            for (auto& a : diff[0]) xL = a.process (xL, g);
            for (auto& a : diff[1]) xR = a.process (xR, g);

            // read the lines (modulated)
            std::array<float, N> y {};
            const float modDepth = p.mod * 0.0018f * (float) sr;
            for (int i = 0; i < N; ++i)
            {
                lfo[(size_t) i] += lfoHz[i] / (float) sr; if (lfo[(size_t) i] >= 1.f) lfo[(size_t) i] -= 1.f;
                const float len = baseMs[i] * 0.001f * (float) sr * scale;
                const float d = len + modDepth * (1.f + std::sin (juce::MathConstants<float>::twoPi * lfo[(size_t) i]));
                y[(size_t) i] = lines[(size_t) i].read (d);
            }
            // output taps (two decorrelated sums), before the feedback matrix
            float oL = 0.f, oR = 0.f;
            static const float sgnL[N] = { 1, -1, 1, 1, -1, 1, -1, 1 }, sgnR[N] = { 1, 1, -1, 1, 1, -1, -1, -1 };
            for (int i = 0; i < N; ++i) { oL += sgnL[i] * y[(size_t) i]; oR += sgnR[i] * y[(size_t) i]; }
            oL *= 0.35f; oR *= 0.35f;

            // Hadamard mixing (fast Walsh-Hadamard, normalised)
            for (int h = 1; h < N; h <<= 1)
                for (int i = 0; i < N; i += h << 1)
                    for (int j = i; j < i + h; ++j)
                    {
                        const float a = y[(size_t) j], b = y[(size_t) (j + h)];
                        y[(size_t) j] = a + b; y[(size_t) (j + h)] = a - b;
                    }
            const float nrm = 0.35355339f;   // 1 / sqrt (8)

            const float dc = p.freeze ? 1.f : onePoleCoef (p.dampHz, sr);
            const float lc = onePoleCoef (p.freeze ? 5.f : p.lowCutHz, sr);
            for (int i = 0; i < N; ++i)
            {
                const float lenS = baseMs[i] * 0.001f * scale;
                const float gi = p.freeze ? 1.f : std::pow (10.f, -3.f * lenS / juce::jmax (0.1f, p.decayS));
                float v = y[(size_t) i] * nrm * gi;
                v = damp[(size_t) i].lp (v, dc);
                v = lowc[(size_t) i].hp (v, lc);
                // input: left into even lines, right into odd lines
                v += (i & 1) ? xR : xL;
                lines[(size_t) i].push (v);
            }

            // shimmer: octave-up copy of the tail fed back into the input
            if (p.shimmer > 0.001f)
            {
                shFbL = shL.process (oL) * p.shimmer * 0.55f;
                shFbR = shR.process (oR) * p.shimmer * 0.55f;
            }
            else shFbL = shFbR = 0.f;

            // width (M/S)
            const float m = 0.5f * (oL + oR), s = 0.5f * (oL - oR) * p.width;
            outL = m + s; outR = m - s;
        }

        // RT60 the engine actually uses for a decay setting (for displays)
        static float effectiveDecay (const ReverbParams& p) { return p.freeze ? 99.f : p.decayS; }

    private:
        double sr = 44100.0;
        Line preL, preR;
        std::array<std::array<Allpass, 4>, 2> diff;
        std::array<Line, N> lines;
        std::array<OnePole, N> damp, lowc;
        std::array<float, N> lfo {};
        Shifter shL, shR;
        float shFbL = 0.f, shFbR = 0.f;
    };

    // ------------------------------------------------------------ bass in mono
    struct MonoBass
    {
        float a1 = 0.f, a2 = 0.f, b1 = 0.f, b2 = 0.f;
        // removes the side signal below 'hz' (two state-variable high-passes, 24 dB/oct); hz < 20 = off
        inline void process (float& l, float& r, float hz, double sr)
        {
            if (hz < 20.f) return;
            const float c = 2.f * std::sin (juce::MathConstants<float>::pi * juce::jmin (hz, (float) sr * 0.2f) / (float) sr);
            const float side = 0.5f * (l - r), mid = 0.5f * (l + r);
            a1 += c * a2; const float h1 = side - a1 - 1.414f * a2; a2 += c * h1;
            b1 += c * b2; const float h2 = h1 - b1 - 1.414f * b2; b2 += c * h2;
            l = mid + h2; r = mid - h2;
        }
    };
}
