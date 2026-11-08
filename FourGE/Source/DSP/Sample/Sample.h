#pragma once
// =====================================================================
//  4GE module 10 - SAMPLE PLAYBACK (pure DSP: no files, no GUI, no allocation; the audio data arrives ready-made)
//   SampleData = one imported sample, already resampled (windowed sinc, on a background thread) to the engine's rate.
//   SamplePlayer = the playback of one slot inside one kick voice: region START..END, ONE SHOT / GATE / LOOP (crossfade) /
//   PING-PONG, REVERSE (non destructive), PLAYBACK RATE x pitch x key tracking (ROOT NOTE), FADE IN / OUT, phase offset,
//   4-point Hermite interpolation, a 3 ms crossfade when the sample of a slot is replaced while it plays (never a click).
// =====================================================================
#include <juce_core/juce_core.h>
#include <vector>
#include <array>
#include <cmath>

namespace fge::dsp
{
    constexpr int numSampleSlots = 8;
    struct SampleData
    {
        int channels = 1;
        int64_t n = 0;
        double rate = 44100.0;                    // the rate of l / r (the engine rate it was prepared for)
        std::vector<float> l, r;                  // r empty for mono
        uint32_t serial = 0;
        float at (int ch, int64_t i) const
        {
            if (i < 0 || i >= n) return 0.f;
            return (ch == 1 && channels > 1) ? r[(size_t) i] : l[(size_t) i];
        }
    };
    enum SampleMode { SmOneShot = 0, SmGate, SmLoop, SmPingPong };
    struct SampleSettings
    {
        int slot = -1;                            // -1 = not a sample layer
        float start = 0.f, end = 1.f, fadeInMs = 0.f, fadeOutMs = 3.f, rate = 1.f, loopStart = 0.25f, loopEnd = 0.75f, xfadeMs = 10.f;
        float rootHz = 49.f, phaseDeg = 0.f;
        bool reverse = false, track = false;
        int mode = SmOneShot;
    };

    class SamplePlayer
    {
    public:
        void start (const SampleSettings& s, const SampleData* d)
        {
            cur = d; old = nullptr; xf = 0.f; t = 0.0; done = (d == nullptr || d->n < 4); dir = s.reverse ? -1 : 1; relT = -1.0;
            if (done) return;
            region (s, *d);
            pos = s.reverse ? b - 1.0 : a;
            // PHASE OFFSET: a fraction of one period of the root note
            pos += (double) dir * (double) s.phaseDeg / 360.0 * d->rate / juce::jmax (1.0, (double) s.rootHz);
            // a start inside the audio (START > 0, REVERSE from END < 1, a phase offset) always gets >= 1 ms fade in (never a click)
            inside = s.reverse ? b < (double) d->n - 1.0 : a > 0.5;
            inside = inside || std::abs (s.phaseDeg) > 0.01f;
        }
        void release (double atMs) { if (relT < 0.0) relT = atMs; }
        // one output sample: mid (returned) + side (stereo samples), 'pitchMul' = the layer's PITCH / FINE, 'noteHz' the played note
        inline float tick (const SampleSettings& s, const SampleData* d, float pitchMul, float noteHz, double outRate, float& side)
        {
            side = 0.f;
            if (d != cur)                                                          // the slot got another sample: 3 ms crossfade
            {
                if (cur != nullptr && ! done && xf <= 0.f) { old = cur; oldPos = pos; oldDir = dir; xf = 1.f; }
                cur = d; if (cur == nullptr || cur->n < 4) { done = true; }
                else { region (s, *cur); pos = s.reverse ? b - 1.0 : a; dir = s.reverse ? -1 : 1; done = false; }
            }
            float l = 0.f, r = 0.f;
            const double ms = t * 1000.0 / outRate;
            t += 1.0;
            if (! done && cur != nullptr)
            {
                region (s, *cur);
                const double step = (double) s.rate * pitchMul * (s.track ? (double) noteHz / juce::jmax (1.0f, s.rootHz) : 1.0) * cur->rate / outRate;
                read (*cur, pos, l, r);
                // loop crossfade: near the loop end, blend in the audio from the loop start
                if ((s.mode == SmLoop) && la < lb)
                {
                    const double len = lb - la, xs = juce::jmin (len * 0.5, (double) s.xfadeMs * 0.001 * cur->rate);
                    const double toEnd = dir > 0 ? lb - pos : pos - la;
                    if (xs > 1.0 && toEnd < xs && toEnd >= 0.0)
                    {
                        float l2, r2; read (*cur, pos - (double) dir * len, l2, r2);
                        const float w = (float) (1.0 - toEnd / xs);                // 0 -> 1 toward the loop end (equal power)
                        const float g1 = std::cos (w * 1.5707963f), g2 = std::sin (w * 1.5707963f);
                        l = l * g1 + l2 * g2; r = r * g1 + r2 * g2;
                    }
                }
                // fades: FADE IN from the start of the playback; FADE OUT before the region end (ONE SHOT) or after the note (GATE)
                const float fin = juce::jmax (s.fadeInMs, inside ? 1.f : 0.f);
                float g = fin > 0.01f ? (float) juce::jmin (1.0, ms / fin) : 1.f;
                if (s.mode == SmOneShot && s.fadeOutMs > 0.01f)
                {
                    const double left = (dir > 0 ? b - pos : pos - a) / juce::jmax (1.0e-9, step) * 1000.0 / outRate;
                    g *= (float) juce::jlimit (0.0, 1.0, left / s.fadeOutMs);
                }
                if (s.mode == SmGate && relT >= 0.0)
                {
                    const double since = ms - relT;
                    g *= s.fadeOutMs > 0.01f ? (float) juce::jlimit (0.0, 1.0, 1.0 - since / s.fadeOutMs) : 0.f;
                    if (since >= s.fadeOutMs) done = true;
                }
                l *= g; r *= g;
                // advance
                pos += (double) dir * step;
                switch (s.mode)
                {
                    case SmLoop:
                        if (la < lb) { if (dir > 0 && pos >= lb) pos -= (lb - la); if (dir < 0 && pos < la) pos += (lb - la); }
                        else if (pos >= b || pos < a) done = true;
                        break;
                    case SmPingPong:
                        if (la < lb) { if (pos >= lb) { pos = 2.0 * lb - pos; dir = -1; } if (pos < la) { pos = 2.0 * la - pos; dir = 1; } }
                        else if (pos >= b || pos < a) done = true;
                        break;
                    default:
                        if (pos >= b || pos < a) done = true;
                        break;
                }
            }
            if (xf > 0.f && old != nullptr)                                        // the replaced sample fades out (3 ms)
            {
                float l2, r2; read (*old, oldPos, l2, r2); oldPos += (double) oldDir * old->rate / outRate;
                l = l * (1.f - xf) + l2 * xf; r = r * (1.f - xf) + r2 * xf;
                xf -= (float) (1000.0 / outRate / 3.0);
                if (xf <= 0.f) { xf = 0.f; old = nullptr; }
            }
            else if (xf > 0.f) xf = 0.f;
            if (! std::isfinite (l) || ! std::isfinite (r)) { l = r = 0.f; done = true; }
            side = 0.5f * (l - r);
            return 0.5f * (l + r);
        }
        bool finished() const { return done && xf <= 0.f; }
        double position() const { return pos; }
    private:
        void region (const SampleSettings& s, const SampleData& d)
        {
            const double n = (double) d.n;
            a = juce::jlimit (0.0, n - 2.0, (double) juce::jmin (s.start, s.end) * n);
            b = juce::jlimit (a + 2.0, n, (double) juce::jmax (s.start, s.end) * n);
            la = a + (double) juce::jmin (s.loopStart, s.loopEnd) * (b - a);
            lb = a + (double) juce::jmax (s.loopStart, s.loopEnd) * (b - a);
            if (lb - la < 16.0) lb = la;                                           // (a loop shorter than 16 samples = no loop)
        }
        static inline float herm (const SampleData& d, int ch, double p)
        {
            const int64_t i = (int64_t) std::floor (p);
            const float f = (float) (p - (double) i);
            const float y0 = d.at (ch, i - 1), y1 = d.at (ch, i), y2 = d.at (ch, i + 1), y3 = d.at (ch, i + 2);
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }
        static inline void read (const SampleData& d, double p, float& l, float& r)
        {
            l = herm (d, 0, p);
            r = d.channels > 1 ? herm (d, 1, p) : l;
        }
        const SampleData* cur = nullptr;
        const SampleData* old = nullptr;
        double pos = 0.0, oldPos = 0.0, t = 0.0, relT = -1.0, a = 0.0, b = 0.0, la = 0.0, lb = 0.0;
        int dir = 1, oldDir = 1;
        float xf = 0.f;
        bool done = true, inside = false;
    };

    // ---- import helpers (background thread only): windowed-sinc resampling (Kaiser, 64 taps, anti-aliased when going down)
    inline std::vector<float> resampleSinc (const float* in, int64_t n, double fromRate, double toRate)
    {
        if (n <= 0) return {};
        if (std::abs (fromRate - toRate) < 1.0e-6) return std::vector<float> (in, in + n);
        const double ratio = toRate / fromRate;
        const int64_t m = (int64_t) std::ceil ((double) n * ratio);
        std::vector<float> out ((size_t) m);
        const double cut = juce::jmin (1.0, ratio) * 0.97;                         // the cutoff below the lower Nyquist
        const int half = 32;
        const double beta = 8.6;
        auto bessel0 = [] (double x) { double s = 1.0, t = 1.0; for (int k = 1; k < 30; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; };
        const double i0b = bessel0 (beta);
        constexpr int tableN = 4096;                                              // the Kaiser window, tabulated once per call
        std::vector<double> win (tableN + 2);
        for (int k = 0; k <= tableN + 1; ++k) { const double w = juce::jmin (1.0, (double) k / tableN); win[(size_t) k] = bessel0 (beta * std::sqrt (juce::jmax (0.0, 1.0 - w * w))) / i0b; }
        const double scale = cut;                                                   // (the filter's DC gain)
        const int span = (int) std::ceil (half / cut);
        for (int64_t j = 0; j < m; ++j)
        {
            const double x = (double) j / ratio;
            const int64_t c = (int64_t) std::floor (x);
            double acc = 0.0;
            for (int64_t k = c - span + 1; k <= c + span; ++k)
            {
                if (k < 0 || k >= n) continue;
                const double d = (x - (double) k) * cut;
                const double w = d / (double) half;
                if (std::abs (w) >= 1.0) continue;
                const double sinc = std::abs (d) < 1.0e-9 ? 1.0 : std::sin (juce::MathConstants<double>::pi * d) / (juce::MathConstants<double>::pi * d);
                const double tp = std::abs (w) * tableN; const int ti = (int) tp; const double tf = tp - ti;
                acc += (double) in[k] * sinc * (win[(size_t) ti] + (win[(size_t) ti + 1] - win[(size_t) ti]) * tf);
            }
            out[(size_t) j] = (float) (acc * scale);
        }
        return out;
    }
}
