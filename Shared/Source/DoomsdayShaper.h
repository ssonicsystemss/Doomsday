#pragma once
// =====================================================================
//  DOOMSDAY shared: sidechain / volume shaping and time effects (namespace doomsday::shaper)
//  (ideas from Kickstart 2, ShaperBox 3, Gross Beat – see Shared/RESEARCH.md)
//
//   OnsetDetector   AUDIO TRIGGER: finds hits in a sidechain (e.g. the kick): input low-pass (focus on the kick),
//                   fast vs slow envelope, threshold in dBFS, minimum gap between hits; process () = true on a hit
//   presetCurve (i) 12 ready ducking / trimming curves (Kickstart style): 0..9 duck, 10..11 trim the tail
//   VolumeShaper    plays a drawn curve (mod::Shape over 0..1000) as gain: synced to the song (one cycle = beats) or
//                   started by a trigger (one-shot, length in ms); DEPTH per band (LOW / MID / HIGH via an LR4 crossover)
//                   so you can duck only the bass under the kick
//   TapeStop        slows the sound down to a stop (pitch falls) over a time, with a curve; release = back to normal
//   Stutter         grabs a slice (length in samples / beats) when engaged and repeats it, short crossfades (no clicks)
//   Reverser        plays the input backwards in chunks (reverse delay), crossfaded
//
//  All buffers are allocated in prepare (); processing is real-time safe.  Tested in Shared/Tests.
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>
#include <cmath>
#include "DoomsdayMod.h"
#include "DoomsdayDynamics.h"

namespace doomsday::shaper
{
    // ------------------------------------------------------------------ OnsetDetector
    class OnsetDetector
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            setFocusHz (180.f);
            fa = 1.f - std::exp (-1.f / (0.0005f * (float) sr));
            fr = 1.f - std::exp (-1.f / (0.030f * (float) sr));
            sa = 1.f - std::exp (-1.f / (0.030f * (float) sr));
            reset();
        }
        void reset() { lp1 = lp2 = 0.f; fast = slow = 0.f; since = 1 << 30; }
        // low-pass on the detector input (180 Hz = kick, 20000 = full range)
        void setFocusHz (float hz) { c = 1.f - std::exp (-juce::MathConstants<float>::twoPi * juce::jlimit (20.f, 20000.f, hz) / (float) sr); }
        void setThresholdDb (float db) { thr = std::pow (10.f, db / 20.f); }
        void setMinGapMs (float ms) { gap = (int) (ms * 0.001 * sr); }
        void setSensitivity (float s01) { ratio = 1.2f + 3.f * (1.f - juce::jlimit (0.f, 1.f, s01)); }
        inline bool process (float x)
        {
            lp1 += c * (x - lp1); lp2 += c * (lp1 - lp2);
            const float m = std::abs (lp2) * 1.4f;
            fast += (m > fast ? fa : fr) * (m - fast);
            slow += sa * (fast - slow);
            ++since;
            if (since > gap && fast > thr && fast > slow * ratio) { since = 0; return true; }
            return false;
        }
    private:
        double sr = 44100.0;
        float c = 0.03f, lp1 = 0.f, lp2 = 0.f, fast = 0.f, slow = 0.f, fa = 0.f, fr = 0.f, sa = 0.f, thr = 0.1f, ratio = 2.f;
        int gap = 2000, since = 1 << 30;
    };

    // ------------------------------------------------------------------ preset curves (0 = full duck at the start)
    inline juce::StringArray presetNames()
    {
        return { "DUCK FAST", "DUCK MEDIUM", "DUCK SLOW", "DUCK LONG", "DUCK HARD", "DUCK SOFT", "DUCK HOLD", "DUCK SCOOP",
                 "DUCK HALF", "DUCK PUMP", "TRIM SHORT", "TRIM LONG" };
    }
    inline doomsday::mod::Shape presetCurve (int i)
    {
        using S = doomsday::mod::Shape;
        switch (juce::jlimit (0, 11, i))
        {
            case 0:  return S::make ({ { 0, 0 }, { 120, 1, 0 }, { 1000, 1 } });
            case 1:  return S::make ({ { 0, 0, 0.5f }, { 300, 1 }, { 1000, 1 } });
            case 2:  return S::make ({ { 0, 0, 0.6f }, { 550, 1 }, { 1000, 1 } });
            case 3:  return S::make ({ { 0, 0, 0.4f }, { 850, 1 }, { 1000, 1 } });
            case 4:  return S::make ({ { 0, 0 }, { 200, 0, 0.7f }, { 400, 1 }, { 1000, 1 } });
            case 5:  return S::make ({ { 0, 0.4f, 0.5f }, { 350, 1 }, { 1000, 1 } });
            case 6:  return S::make ({ { 0, 0 }, { 250, 0 }, { 320, 1 }, { 1000, 1 } });
            case 7:  return S::make ({ { 0, 0, -0.6f }, { 450, 1 }, { 1000, 1 } });
            case 8:  return S::make ({ { 0, 0.5f, 0.5f }, { 300, 1 }, { 1000, 1 } });
            case 9:  return S::make ({ { 0, 0, 0.8f }, { 700, 1, 0 }, { 1000, 0.9f } });
            case 10: return S::make ({ { 0, 1 }, { 400, 1, -0.5f }, { 600, 0 }, { 1000, 0 } });
            default: return S::make ({ { 0, 1 }, { 700, 1, -0.5f }, { 900, 0 }, { 1000, 0 } });
        }
    }

    // ------------------------------------------------------------------ VolumeShaper
    class VolumeShaper
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            xo.prepare (sr);
            for (auto& s : smooth) { s.prepare (sr, 1.f); s.reset (1.f); }
        }
        void reset() { xo.reset(); oneShotPos = -1.0; }
        void setCurve (const doomsday::mod::Shape& s) { curve = s; }
        void setDepth (float lowD, float midD, float highD) { depth = { juce::jlimit (0.f, 1.f, lowD), juce::jlimit (0.f, 1.f, midD), juce::jlimit (0.f, 1.f, highD) }; }
        void setDepthAll (float d) { setDepth (d, d, d); }
        void setCrossovers (float lowHz, float highHz) { xo.set (lowHz, highHz); }
        // synced: one curve cycle = beats; call trigger () for one-shot mode instead
        void setSynced (bool s, double beats) { synced = s; cycleBeats = juce::jmax (1.0 / 64.0, beats); }
        void setOneShotMs (float ms) { oneShotLen = juce::jmax (1.0, ms * 0.001 * sr); }
        void trigger() { oneShotPos = 0.0; }
        // curve value 0..1 at the current position
        float curveNow (double ppq) const
        {
            if (synced) { const double ph = ppq / cycleBeats; return curve.at ((float) ((ph - std::floor (ph)) * 1000.0)); }
            if (oneShotPos < 0.0) return 1.f;
            return curve.at ((float) juce::jmin (1000.0, oneShotPos / oneShotLen * 1000.0));
        }
        // stereo in place; ppq = song position of the first sample, bpm for the synced mode
        void process (float* L, float* R, int n, double ppq, double bpm)
        {
            const double beatsPerSample = bpm / 60.0 / sr;
            const bool split = depth[0] != depth[1] || depth[1] != depth[2];
            for (int i = 0; i < n; ++i)
            {
                const float c = curveNow (ppq + i * beatsPerSample);
                if (! synced && oneShotPos >= 0.0) { oneShotPos += 1.0; if (oneShotPos > oneShotLen) oneShotPos = -1.0; }
                if (! split)
                {
                    const float g = smooth[0].next (1.f - depth[0] * (1.f - c));
                    L[i] *= g; R[i] *= g;
                    continue;
                }
                float lo, mi, hi, lo2, mi2, hi2;
                xo.split (0, L[i], lo, mi, hi); xo.split (1, R[i], lo2, mi2, hi2);
                const float g0 = smooth[0].next (1.f - depth[0] * (1.f - c)), g1 = smooth[1].next (1.f - depth[1] * (1.f - c)), g2 = smooth[2].next (1.f - depth[2] * (1.f - c));
                L[i] = lo * g0 + mi * g1 + hi * g2;
                R[i] = lo2 * g0 + mi2 * g1 + hi2 * g2;
            }
        }
    private:
        double sr = 44100.0, cycleBeats = 1.0, oneShotPos = -1.0, oneShotLen = 20000.0;
        bool synced = true;
        doomsday::mod::Shape curve = presetCurve (1);
        std::array<float, 3> depth { 1.f, 1.f, 1.f };
        std::array<doomsday::mod::Smoother, 3> smooth;
        doomsday::dyn::Crossover3 xo;
    };

    // ------------------------------------------------------------------ circular stereo buffer
    struct Ring
    {
        std::array<std::vector<float>, 2> b;
        int size = 1, w = 0;
        void prepare (int samples) { size = juce::jmax (8, samples); for (auto& c : b) c.assign ((size_t) size, 0.f); w = 0; }
        void clear() { for (auto& c : b) std::fill (c.begin(), c.end(), 0.f); }
        inline void push (float l, float r) { b[0][(size_t) w] = l; b[1][(size_t) w] = r; if (++w >= size) w = 0; }
        // linear interpolated read `back` samples behind the write head
        inline float read (int ch, double back) const
        {
            double pos = (double) w - back;
            while (pos < 0.0) pos += size;
            const int i0 = (int) pos % size, i1 = (i0 + 1) % size;
            const float f = (float) (pos - std::floor (pos));
            return b[(size_t) ch][(size_t) i0] + f * (b[(size_t) ch][(size_t) i1] - b[(size_t) ch][(size_t) i0]);
        }
    };

    // ------------------------------------------------------------------ TapeStop
    class TapeStop
    {
    public:
        void prepare (double sampleRate, float maxSeconds = 4.f) { sr = sampleRate; ring.prepare ((int) (sr * maxSeconds) + 8); reset(); }
        void reset() { ring.clear(); speed = 1.0; lag = 0.0; active = false; t = 0.0; }
        // curve: 0 = linear slow-down, 1 = fast at the end (like a turntable), -1 = fast at the start
        void engage (float stopMs, float curve) { active = true; t = 0.0; len = juce::jmax (1.0, stopMs * 0.001 * sr); bend = curve; }
        void release() { active = false; }
        bool isStopped() const { return active && speed <= 0.0; }
        inline void process (float& l, float& r)
        {
            ring.push (l, r);
            if (! active)
            {
                // catch up smoothly with the live signal
                lag = juce::jmax (0.0, lag - 0.25 * juce::jmax (1.0, lag * 0.002));
                speed = 1.0;
                if (lag < 0.5) { lag = 0.0; return; }
            }
            else
            {
                t += 1.0;
                const double x = juce::jmin (1.0, t / len);
                const double k = std::abs (bend) < 1.0e-3 ? x : (std::exp (bend * 3.0 * x) - 1.0) / (std::exp (bend * 3.0) - 1.0);
                speed = juce::jmax (0.0, 1.0 - k);
                lag = juce::jmin ((double) ring.size - 4.0, lag + (1.0 - speed));
            }
            const float fade = active && speed <= 0.02 ? (float) (speed / 0.02) : 1.f;
            l = ring.read (0, lag + 1.0) * fade; r = ring.read (1, lag + 1.0) * fade;
        }
    private:
        Ring ring;
        double sr = 44100.0, speed = 1.0, lag = 0.0, t = 0.0, len = 1.0;
        float bend = 0.f;
        bool active = false;
    };

    // ------------------------------------------------------------------ Stutter
    class Stutter
    {
    public:
        void prepare (double sampleRate, float maxSeconds = 2.f)
        {
            sr = sampleRate;
            ring.prepare ((int) (sr * maxSeconds) + 8);
            for (auto& c : slice) c.assign ((size_t) ring.size, 0.f);
            xf = (int) (0.003 * sr); reset();
        }
        void reset() { ring.clear(); active = false; pos = 0; }
        // repeat the last `samples` (e.g. 1/16 note = sr * 60 / bpm / 4); copies the slice (no allocation)
        void engage (int samples)
        {
            len = juce::jlimit (xf * 2 + 2, ring.size - 4, samples);
            for (int i = 0; i < len; ++i)
                for (int ch = 0; ch < 2; ++ch) slice[(size_t) ch][(size_t) i] = ring.read (ch, (double) (len - i));
            active = true; pos = 0;
        }
        void release() { active = false; }
        inline void process (float& l, float& r)
        {
            ring.push (l, r);
            if (! active) return;
            float g = 1.f;
            if (pos < xf) g = (float) pos / (float) xf;
            else if (pos > len - xf) g = (float) (len - pos) / (float) xf;
            l = slice[0][(size_t) pos] * g; r = slice[1][(size_t) pos] * g;
            if (++pos >= len) pos = 0;
        }
    private:
        Ring ring;
        std::array<std::vector<float>, 2> slice;
        double sr = 44100.0;
        int len = 1000, pos = 0, xf = 128;
        bool active = false;
    };

    // ------------------------------------------------------------------ Reverser
    class Reverser
    {
    public:
        void prepare (double sampleRate, float maxChunkSeconds = 2.f) { sr = sampleRate; ring.prepare ((int) (sr * maxChunkSeconds * 2.0) + 8); xf = (int) (0.004 * sr); reset(); }
        void reset() { ring.clear(); pos = 0; }
        void setChunk (int samples) { chunk = juce::jlimit (xf * 2 + 2, ring.size / 2 - 4, samples); }
        // output = the previous chunk backwards (latency = one chunk)
        inline void process (float& l, float& r)
        {
            ring.push (l, r);
            // at position pos inside the current chunk, play the sample that came (pos) samples before the chunk start, backwards
            const double back = 2.0 * pos + 2.0;
            float g = 1.f;
            if (pos < xf) g = (float) pos / (float) xf;
            else if (pos > chunk - xf) g = (float) (chunk - pos) / (float) xf;
            l = ring.read (0, back) * g; r = ring.read (1, back) * g;
            if (++pos >= chunk) pos = 0;
        }
        int latencySamples() const { return chunk; }
    private:
        Ring ring;
        double sr = 44100.0;
        int chunk = 22050, pos = 0, xf = 176;
    };
}
