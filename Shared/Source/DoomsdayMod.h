#pragma once
// =====================================================================
//  DOOMSDAY shared: modulation (namespace doomsday::mod)
//
//   Shape       drawn curve: up to 32 dots {time, value 0..1, bend}; time axis is free (per-mille of a length,
//               ms, or one LFO cycle 0..1000). bend bends the segment AFTER a dot (-1 = fast start, +1 = slow start,
//               0 = straight; old "ms:v" strings stay straight). eval() is O(1) when time only moves forward.
//               toString / fromString to store it in the plugin state.  (Forge of Wrath's kick::Shape is this.)
//   Lfo         SINE / TRIANGLE / SAW UP / SAW DOWN / SQUARE / S&H / SMOOTH RANDOM / DRAWN (a Shape as one cycle),
//               free rate in Hz or synced to the host (1/64 .. 8 bars, straight / dotted / triplet), phase offset,
//               syncTo (ppq) locks the phase to the song position, retrigger() for "restart on every note".
//               Output -1..1 (bipolar) or 0..1 (unipolar).
//               CHAOS = Lorenz attractor (never repeats, smooth – "alive" screeches)
//   Performer   16-step curve sequencer (each step = a small curve: hold / ramps / exp / sine / pulse / off, with height),
//               two sequences A / B with MORPH, synced to the song (Massive's Performer)
//   RandomPerNote  one random value per note-on (Massive's Random Track), optional glide
//   KeyScale    key + scale (major / minor / harmonic minor / phrygian / dorian / ...), quantize a MIDI note into it
//   Arp         arpeggiator with step sequencer: UP / DOWN / UP-DOWN / RANDOM / AS PLAYED, octaves, per-step gate /
//               velocity / transpose / on, rate in beats, emits note events with sample offsets
//   Smoother    one-pole parameter smoothing (ms)
//   Matrix<S, D, N>  N slots: source -> destination x amount (+ optional via-source); sum per destination,
//               computed once per block or per sample; destinations read with dest (d)
//   syncDivisions / syncBeats (i)   table of host-sync note values for a Choice parameter
//
//  Real-time safe (no allocation).  Tested in Shared/Tests.
// =====================================================================
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

namespace doomsday::mod
{
    // ------------------------------------------------------------------ Shape
    struct Shape
    {
        static constexpr int maxPoints = 32;
        struct Pt { float ms = 0.f, v = 1.f, bend = 0.f; };
        std::array<Pt, maxPoints> pts {};
        int n = 0;

        static Shape make (std::initializer_list<Pt> list)
        {
            Shape s; for (auto& p : list) if (s.n < maxPoints) s.pts[(size_t) s.n++] = p;
            s.tidy(); return s;
        }
        float endMs() const { return n > 0 ? pts[(size_t) n - 1].ms : 0.f; }
        static inline float bent (float t, float bend)
        {
            if (std::abs (bend) < 1.0e-3f) return t;
            const float k = bend * 6.f;                         // +-1 -> exponent-like curve
            return (std::exp (k * t) - 1.f) / (std::exp (k) - 1.f);
        }
        // cursor makes forward evaluation O(1)
        inline float eval (float ms, int& seg) const
        {
            if (n == 0) return 1.f;
            if (ms <= pts[0].ms) { seg = 0; return pts[0].v; }
            if (seg >= n || (seg > 0 && ms < pts[(size_t) seg].ms)) seg = 0;
            while (seg + 1 < n && ms > pts[(size_t) seg + 1].ms) ++seg;
            if (seg + 1 >= n) return pts[(size_t) n - 1].v;
            const auto& a = pts[(size_t) seg]; const auto& b = pts[(size_t) seg + 1];
            const float span = b.ms - a.ms;
            if (span <= 0.f) return b.v;
            return a.v + (b.v - a.v) * bent ((ms - a.ms) / span, a.bend);
        }
        float at (float ms) const { int s = 0; return eval (ms, s); }
        void tidy()
        {
            for (int i = 0; i < n; ++i)
            {
                auto& p = pts[(size_t) i];
                p.ms = juce::jmax (0.f, p.ms); p.v = juce::jlimit (0.f, 1.f, p.v); p.bend = juce::jlimit (-1.f, 1.f, p.bend);
            }
            std::stable_sort (pts.begin(), pts.begin() + n, [] (const Pt& a, const Pt& b) { return a.ms < b.ms; });
            if (n > 0) pts[0].ms = 0.f;          // the first point is pinned to the start
        }
        bool insert (float ms, float v)
        {
            if (n >= maxPoints) return false;
            pts[(size_t) n++] = { ms, v, 0.f }; tidy(); return true;
        }
        void remove (int i) { if (i <= 0 || i >= n || n <= 2) return; for (int k = i; k < n - 1; ++k) pts[(size_t) k] = pts[(size_t) k + 1]; --n; }
        juce::String toString() const
        {
            juce::StringArray s;
            for (int i = 0; i < n; ++i)
            {
                const auto& p = pts[(size_t) i];
                juce::String t = juce::String (p.ms, 2) + ":" + juce::String (p.v, 4);
                if (std::abs (p.bend) > 1.0e-4f) t << ":" << juce::String (p.bend, 3);
                s.add (t);
            }
            return s.joinIntoString (";");
        }
        static bool fromString (const juce::String& text, Shape& out)
        {
            Shape s;
            for (auto& tok : juce::StringArray::fromTokens (text, ";", ""))
            {
                auto parts = juce::StringArray::fromTokens (tok, ":", "");
                if (parts.size() >= 2 && s.n < maxPoints)
                    s.pts[(size_t) s.n++] = { parts[0].getFloatValue(), parts[1].getFloatValue(), parts.size() > 2 ? parts[2].getFloatValue() : 0.f };
            }
            if (s.n < 2) return false;
            s.tidy(); out = s; return true;
        }
        bool operator== (const Shape& o) const
        {
            if (n != o.n) return false;
            for (int i = 0; i < n; ++i)
            {
                const auto& a = pts[(size_t) i]; const auto& b = o.pts[(size_t) i];
                if (a.ms != b.ms || a.v != b.v || a.bend != b.bend) return false;
            }
            return true;
        }
    };

    // ------------------------------------------------------------------ host sync values
    struct SyncDiv { const char* name; double beats; };
    inline const std::array<SyncDiv, 19>& syncDivisions()
    {
        static const std::array<SyncDiv, 19> d { {
            { "1/64", 0.0625 }, { "1/32", 0.125 }, { "1/16T", 1.0 / 6.0 }, { "1/16", 0.25 }, { "1/16D", 0.375 },
            { "1/8T", 1.0 / 3.0 }, { "1/8", 0.5 }, { "1/8D", 0.75 }, { "1/4T", 2.0 / 3.0 }, { "1/4", 1.0 }, { "1/4D", 1.5 },
            { "1/2T", 4.0 / 3.0 }, { "1/2", 2.0 }, { "1/2D", 3.0 }, { "1 BAR", 4.0 }, { "2 BARS", 8.0 }, { "4 BARS", 16.0 },
            { "8 BARS", 32.0 }, { "16 BARS", 64.0 } } };
        return d;
    }
    inline double syncBeats (int i) { return syncDivisions()[(size_t) juce::jlimit (0, (int) syncDivisions().size() - 1, i)].beats; }
    inline juce::StringArray syncNames() { juce::StringArray s; for (auto& d : syncDivisions()) s.add (d.name); return s; }

    // ------------------------------------------------------------------ LFO
    class Lfo
    {
    public:
        enum Wave { Sine = 0, Triangle, SawUp, SawDown, Square, SampleHold, SmoothRandom, Drawn, Chaos, numWaves };
        static juce::StringArray waveNames() { return { "SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "S&H", "SMOOTH", "DRAWN", "CHAOS" }; }

        void prepare (double sampleRate) { sr = sampleRate; }
        void setWave (int w) { wave = juce::jlimit (0, (int) numWaves - 1, w); }
        void setRateHz (double hz) { inc = juce::jmax (0.0, hz) / sr; }
        // synced: cycle = beats at bpm
        void setSynced (double beats, double bpm) { inc = (bpm / 60.0) / juce::jmax (1.0e-3, beats) / sr; cycleBeats = beats; }
        void setPhaseOffset (float off01) { offset = off01; }
        void setShape (const Shape* s) { drawn = s; }
        void setSeed (uint32_t s) { rng = s ? s : 1u; }
        // lock to the song position (call at block start with the host ppq when the transport plays)
        void syncTo (double ppq) { phase = std::fmod (ppq / juce::jmax (1.0e-3, cycleBeats), 1.0); if (phase < 0.0) phase += 1.0; }
        void retrigger() { phase = 0.0; newCycle(); }
        double getPhase() const { return phase; }

        // bipolar -1..1
        inline float next()
        {
            if (wave == Chaos) return chaosStep (1);
            const float y = valueAt ((float) std::fmod (phase + offset, 1.0));
            phase += inc;
            if (phase >= 1.0) { phase -= std::floor (phase); newCycle(); }
            return y;
        }
        inline float nextUnipolar() { return 0.5f * (next() + 1.f); }
        // advance a block, return the value at the block start (for per-block modulation)
        inline float advance (int samples)
        {
            if (wave == Chaos) return chaosStep (samples);
            const float y = valueAt ((float) std::fmod (phase + offset, 1.0));
            phase += inc * samples;
            if (phase >= 1.0) { phase -= std::floor (phase); newCycle(); }
            return y;
        }
    private:
        inline float valueAt (float p) const
        {
            switch (wave)
            {
                case Sine:         return std::sin (juce::MathConstants<float>::twoPi * p);
                case Triangle:     return 1.f - 4.f * std::abs (p - 0.5f) ;
                case SawUp:        return 2.f * p - 1.f;
                case SawDown:      return 1.f - 2.f * p;
                case Square:       return p < 0.5f ? 1.f : -1.f;
                case SampleHold:   return held;
                case SmoothRandom: { const float t = p * p * (3.f - 2.f * p); return prevRnd + (held - prevRnd) * t; }
                default:           return drawn != nullptr ? drawn->at (p * 1000.f) * 2.f - 1.f : 0.f;
            }
        }
        // Lorenz attractor, speed follows the rate (one "lap" ~ one cycle)
        inline float chaosStep (int samples)
        {
            const double dt = juce::jlimit (0.0, 0.02, inc * 1.2 * samples);
            const double dx = 10.0 * (ly - lx), dy = lx * (28.0 - lz) - ly, dz = lx * ly - (8.0 / 3.0) * lz;
            lx += dx * dt; ly += dy * dt; lz += dz * dt;
            return (float) juce::jlimit (-1.0, 1.0, lx / 20.0);
        }
        void newCycle()
        {
            prevRnd = held;
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            held = (float) (rng & 0xffffff) / 8388607.5f - 1.f;
        }
        double sr = 44100.0, phase = 0.0, inc = 0.0, cycleBeats = 1.0;
        float offset = 0.f, held = 0.f, prevRnd = 0.f;
        int wave = Sine;
        uint32_t rng = 0x2545f491u;
        const Shape* drawn = nullptr;
        double lx = 0.1, ly = 0.0, lz = 0.0;
    };

    // ------------------------------------------------------------------ Performer
    class Performer
    {
    public:
        enum Curve { Hold = 0, RampUp, RampDown, Triangle, ExpDown, ExpUp, SineBump, Pulse, Off, numCurves };
        static juce::StringArray curveNames() { return { "HOLD", "RAMP UP", "RAMP DOWN", "TRIANGLE", "EXP DOWN", "EXP UP", "SINE", "PULSE", "OFF" }; }
        struct Step { int curve = Hold; float height = 1.f; };
        static constexpr int maxSteps = 16;
        void setStep (int seq, int i, int curve, float height)
        {
            if (! juce::isPositiveAndBelow (i, maxSteps)) return;
            seqs[(size_t) (seq & 1)][(size_t) i] = { juce::jlimit (0, (int) numCurves - 1, curve), juce::jlimit (0.f, 1.f, height) };
        }
        void setLength (int n) { length = juce::jlimit (1, maxSteps, n); }
        void setMorph (float m01) { morph = juce::jlimit (0.f, 1.f, m01); }
        // value 0..1 at a song position in beats (one step = stepBeats)
        float at (double ppq, double stepBeats) const
        {
            const double pos = ppq / juce::jmax (1.0e-4, stepBeats);
            const double wrapped = pos - std::floor (pos / length) * length;
            const int i = juce::jlimit (0, length - 1, (int) wrapped);
            const float t = (float) (wrapped - i);
            const float a = curveValue (seqs[0][(size_t) i], t), b = curveValue (seqs[1][(size_t) i], t);
            return a + morph * (b - a);
        }
        static float curveValue (const Step& st, float t)
        {
            float v;
            switch (st.curve)
            {
                case RampUp:   v = t; break;
                case RampDown: v = 1.f - t; break;
                case Triangle: v = 1.f - std::abs (2.f * t - 1.f); break;
                case ExpDown:  v = std::exp (-5.f * t); break;
                case ExpUp:    v = std::exp (-5.f * (1.f - t)); break;
                case SineBump: v = std::sin (juce::MathConstants<float>::pi * t); break;
                case Pulse:    v = t < 0.5f ? 1.f : 0.f; break;
                case Off:      v = 0.f; break;
                default:       v = 1.f; break;
            }
            return v * st.height;
        }
    private:
        std::array<std::array<Step, maxSteps>, 2> seqs {};
        int length = 16;
        float morph = 0.f;
    };

    // ------------------------------------------------------------------ RandomPerNote
    struct RandomPerNote
    {
        uint32_t rng = 0x9E3779B9u;
        float target = 0.f, value = 0.f, glide = 1.f;
        void prepare (double rate, float glideMs) { glide = glideMs <= 0.f ? 1.f : 1.f - std::exp (-1.f / (glideMs * 0.001f * (float) rate)); }
        void setSeed (uint32_t s) { rng = s ? s : 1u; }
        // call on every note-on; returns the new target (-1..1)
        float trigger() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; target = (float) (rng & 0xffffff) / 8388607.5f - 1.f; return target; }
        inline float next() { value += glide * (target - value); return value; }
    };

    // ------------------------------------------------------------------ KeyScale
    struct KeyScale
    {
        enum Scale { Chromatic = 0, Major, Minor, HarmonicMinor, Phrygian, Dorian, MinorPentatonic, numScales };
        static juce::StringArray scaleNames() { return { "CHROMATIC", "MAJOR", "MINOR", "HARMONIC MINOR", "PHRYGIAN", "DORIAN", "MINOR PENTA" }; }
        static juce::StringArray keyNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
        int root = 9, scale = Minor;      // A minor
        bool contains (int note) const
        {
            static const int masks[numScales] = { 0xFFF, 0xAB5, 0x5AD, 0x9AD, 0x5AB, 0x6AD, 0x4A9 };   // bit i = semitone i above the root
            const int pc = ((note - root) % 12 + 12) % 12;
            return (masks[juce::jlimit (0, (int) numScales - 1, scale)] >> pc) & 1;
        }
        // nearest note of the scale (ties go down)
        int quantize (int note) const
        {
            for (int d = 0; d < 12; ++d)
            {
                if (contains (note - d)) return note - d;
                if (contains (note + d)) return note + d;
            }
            return note;
        }
    };

    // ------------------------------------------------------------------ Arp
    class Arp
    {
    public:
        enum Mode { Up = 0, Down, UpDown, Random, AsPlayed, numModes };
        static juce::StringArray modeNames() { return { "UP", "DOWN", "UP-DOWN", "RANDOM", "AS PLAYED" }; }
        struct StepCfg { bool on = true; float gate = 0.5f, velocity = 1.f; int transpose = 0; };
        static constexpr int maxHeld = 16, maxSteps = 16;

        void noteOn (int note, float vel)
        {
            if (count >= maxHeld) return;
            for (int i = 0; i < count; ++i) if (held[(size_t) i] == note) return;
            held[(size_t) count] = note; order[(size_t) count] = serial++; velocity[(size_t) count] = vel; ++count;
        }
        void noteOff (int note)
        {
            for (int i = 0; i < count; ++i)
                if (held[(size_t) i] == note)
                {
                    for (int k = i; k < count - 1; ++k) { held[(size_t) k] = held[(size_t) k + 1]; order[(size_t) k] = order[(size_t) k + 1]; velocity[(size_t) k] = velocity[(size_t) k + 1]; }
                    --count; return;
                }
        }
        void allOff() { count = 0; }
        void setMode (int m) { mode = juce::jlimit (0, (int) numModes - 1, m); }
        void setOctaves (int o) { octaves = juce::jlimit (1, 4, o); }
        void setRateBeats (double b) { rate = juce::jmax (1.0 / 64.0, b); }
        void setSteps (int n) { steps = juce::jlimit (1, maxSteps, n); }
        void setStep (int i, StepCfg c) { if (juce::isPositiveAndBelow (i, maxSteps)) cfg[(size_t) i] = c; }

        // emit (sampleOffset, note, velocity, isOn) for one block starting at ppq, bpm, sample rate, n samples
        template <typename Emit>
        void process (double ppq, double bpm, double sr, int n, Emit&& emit)
        {
            const double beatsPerSample = bpm / 60.0 / sr;
            for (int i = 0; i < n; ++i)
            {
                const double p = ppq + i * beatsPerSample;
                if (playing >= 0 && p >= offAt) { emit (i, playing, 0.f, false); playing = -1; }
                const long long stepIdx = (long long) std::floor (p / rate + 1.0e-9);
                if (stepIdx != lastStep)
                {
                    lastStep = stepIdx;
                    if (playing >= 0) { emit (i, playing, 0.f, false); playing = -1; }
                    if (count == 0) continue;
                    const auto& c = cfg[(size_t) (stepIdx % steps)];
                    const int note = pick (stepIdx);
                    if (c.on && note >= 0)
                    {
                        playing = juce::jlimit (0, 127, note + c.transpose);
                        offAt = (double) (stepIdx) * rate + rate * juce::jlimit (0.05, 1.0, (double) c.gate);
                        emit (i, playing, juce::jlimit (0.f, 1.f, c.velocity * lastVel), true);
                    }
                }
            }
        }
    private:
        int pick (long long stepIdx)
        {
            std::array<int, maxHeld> sorted {}; std::array<float, maxHeld> v {};
            int idx[maxHeld];
            for (int i = 0; i < count; ++i) idx[i] = i;
            if (mode == AsPlayed) std::sort (idx, idx + count, [this] (int a, int b) { return order[(size_t) a] < order[(size_t) b]; });
            else std::sort (idx, idx + count, [this] (int a, int b) { return held[(size_t) a] < held[(size_t) b]; });
            for (int i = 0; i < count; ++i) { sorted[(size_t) i] = held[(size_t) idx[i]]; v[(size_t) i] = velocity[(size_t) idx[i]]; }
            const int total = count * octaves;
            int k;
            switch (mode)
            {
                case Down:   k = total - 1 - (int) (stepIdx % total); break;
                case UpDown: { const int span = juce::jmax (1, 2 * total - 2); const int q = (int) (stepIdx % span); k = q < total ? q : span - q; break; }
                case Random: rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; k = (int) (rng % (uint32_t) total); break;
                default:     k = (int) (stepIdx % total); break;
            }
            lastVel = v[(size_t) (k % count)];
            return sorted[(size_t) (k % count)] + 12 * (k / count);
        }
        std::array<int, maxHeld> held {}; std::array<long long, maxHeld> order {}; std::array<float, maxHeld> velocity {};
        std::array<StepCfg, maxSteps> cfg {};
        int count = 0, mode = Up, octaves = 1, steps = 16, playing = -1;
        long long serial = 0, lastStep = -1;
        double rate = 0.25, offAt = 0.0;
        float lastVel = 1.f;
        uint32_t rng = 0x1234567u;
    };

    // ------------------------------------------------------------------ Smoother
    struct Smoother
    {
        float y = 0.f, c = 1.f;
        void prepare (double rate, float ms) { c = 1.f - std::exp (-1.f / (juce::jmax (0.01f, ms) * 0.001f * (float) rate)); }
        void reset (float v) { y = v; }
        inline float next (float target) { y += c * (target - y); return y; }
    };

    // ------------------------------------------------------------------ Matrix
    // sources: values you set every block / sample (LFOs -1..1, envelopes 0..1, velocity, mod wheel...)
    // slots: src -> dst * amount (* via source, if via >= 0) ; dest (d) = sum of all slots into d
    template <int NumSources, int NumDests, int NumSlots = 8>
    class Matrix
    {
    public:
        struct Slot { int src = -1, dst = -1, via = -1; float amount = 0.f; };
        void setSlot (int i, int src, int dst, float amount, int via = -1)
        {
            if (! juce::isPositiveAndBelow (i, NumSlots)) return;
            slots[(size_t) i] = { juce::jlimit (-1, NumSources - 1, src), juce::jlimit (-1, NumDests - 1, dst), juce::jlimit (-1, NumSources - 1, via), amount };
        }
        void clearSlot (int i) { if (juce::isPositiveAndBelow (i, NumSlots)) slots[(size_t) i] = {}; }
        inline void setSource (int s, float v) { if (juce::isPositiveAndBelow (s, NumSources)) src[(size_t) s] = v; }
        inline void compute()
        {
            dst.fill (0.f);
            for (auto& sl : slots)
            {
                if (sl.src < 0 || sl.dst < 0 || sl.amount == 0.f) continue;
                float v = src[(size_t) sl.src] * sl.amount;
                if (sl.via >= 0) v *= src[(size_t) sl.via];
                dst[(size_t) sl.dst] += v;
            }
        }
        inline float dest (int d) const { return juce::isPositiveAndBelow (d, NumDests) ? dst[(size_t) d] : 0.f; }
        const Slot& slot (int i) const { return slots[(size_t) juce::jlimit (0, NumSlots - 1, i)]; }
    private:
        std::array<Slot, NumSlots> slots {};
        std::array<float, NumSources> src {};
        std::array<float, NumDests> dst {};
    };
}
