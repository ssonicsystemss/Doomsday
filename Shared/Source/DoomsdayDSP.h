#pragma once
// =====================================================================
//  DOOMSDAY shared DSP blocks (header only)
//    dsp::polyBlep / wrap01 / fastSin01 / fastTanh / timeCoef / softClip
//    dsp::Env            ADSR (linear attack, exponential decay / release)
//    dsp::Svf, svfCoefs  TPT state-variable filter (+ LP24 / BAND / HIGH helper)
//    dsp::UnisonBank     polyBLEP unison oscillator (up to 16 voices, detune / stereo / pan)
//    dsp::FxChain        stereo CHORUS -> DELAY -> REVERB -> PUMP (tempo synced)
//    dsp::LevelFeed      min/max-per-chunk feed for GUI visualisations
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>
#include <vector>
#include <atomic>

namespace doomsday::dsp
{
    // ------------------------------------------------------------ helpers
    inline float polyBlep (float t, float dt)
    {
        if (t < dt)       { t /= dt;            return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }
    inline float wrap01 (float x) { return x - std::floor (x); }
    inline float fastSin01 (float ph)                       // sin(2 pi ph), ~0.1 % error
    {
        const float x = ph * 2.f - 1.f;
        float y = 4.f * x * (1.f - std::abs (x));
        y = 0.225f * (y * std::abs (y) - y) + y;
        return -y;
    }
    inline float fastTanh (float x)
    {
        x = juce::jlimit (-3.f, 3.f, x);
        const float x2 = x * x;
        return x * (27.f + x2) / (27.f + 9.f * x2);
    }
    inline float timeCoef (float ms, double rate)           // exp decay to -60 dB in ms
    {
        const double s = juce::jmax (0.0005, (double) ms * 0.001);
        return (float) std::exp (std::log (0.001) / (s * rate));
    }
    inline float softClip (float x, float knee = 0.85f)
    {
        const float a = std::abs (x);
        if (a < knee) return x;
        const float s = x > 0.f ? 1.f : -1.f;
        return s * (knee + (1.f - knee) * std::tanh ((a - knee) / (1.f - knee)));
    }
    inline float onePoleCoef (float hz, double rate) { return 1.f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) rate); }

    // ------------------------------------------------------------ ADSR
    struct Env
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
        float value = 0.f;
        struct Rates { float attInc = 1.f, decC = 0.f, sus = 1.f, relC = 0.f; };
        static Rates rates (float aMs, float dMs, float sus, float rMs, double rate)
        {
            return { (float) (1.0 / (juce::jmax (0.3, (double) aMs) * 0.001 * rate)), timeCoef (dMs, rate), sus, timeCoef (rMs, rate) };
        }
        void trigger (bool fromZero) { if (fromZero) value = 0.f; stage = Attack; }
        void release() { if (stage != Idle) stage = Release; }
        bool idle() const { return stage == Idle; }
        inline float next (const Rates& r)
        {
            switch (stage)
            {
                case Attack:  value += r.attInc; if (value >= 1.f) { value = 1.f; stage = Decay; } break;
                case Decay:   value = r.sus + (value - r.sus) * r.decC;
                              if (std::abs (value - r.sus) < 1.0e-4f) { value = r.sus; stage = Sustain; } break;
                case Sustain: value = r.sus; break;
                case Release: value *= r.relC; if (value < 1.0e-5f) { value = 0.f; stage = Idle; } break;
                case Idle:    value = 0.f; break;
            }
            return value;
        }
    };

    // ------------------------------------------------------------ TPT SVF
    struct SvfCoefs { float a1 = 1.f, a2 = 0.f, a3 = 0.f, k = 2.f; };
    inline SvfCoefs svfCoefs (float hz, float k, double rate)
    {
        const float g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (10.f, (float) (rate * 0.45), hz) / (float) rate);
        SvfCoefs c; c.k = k;
        c.a1 = 1.f / (1.f + g * (g + k)); c.a2 = g * c.a1; c.a3 = g * c.a2;
        return c;
    }
    struct Svf
    {
        float ic1 = 0.f, ic2 = 0.f;
        inline void tick (float v0, const SvfCoefs& c, float& lp, float& bp)
        {
            const float v3 = v0 - ic2;
            const float v1 = c.a1 * ic1 + c.a2 * v3;
            const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
            ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
            bp = v1; lp = v2;
        }
        inline float lp (float x, const SvfCoefs& c) { float l, b; tick (x, c, l, b); return l; }
        inline float bp (float x, const SvfCoefs& c) { float l, b; tick (x, c, l, b); return b; }
        inline float hp (float x, const SvfCoefs& c) { float l, b; tick (x, c, l, b); return x - c.k * b - l; }
        inline float peak (float x, const SvfCoefs& c, float gain) { float l, b; tick (x, c, l, b); return x + (gain - 1.f) * c.k * b; }
    };
    // filter type: 0 LP12, 1 LP24, 2 BAND, 3 HIGH
    struct MultiFilter
    {
        Svf a, b;
        inline float process (float x, int type, const SvfCoefs& c)
        {
            float lp, bp;
            a.tick (x, c, lp, bp);
            switch (type)
            {
                case 0:  return lp;
                case 1:  return b.lp (lp, c);
                case 2:  return bp * (0.5f + c.k * 0.5f);
                default: return x - c.k * bp - lp;
            }
        }
    };

    // ------------------------------------------------------------ unison oscillator bank
    // waves: 0 SINE, 1 TRI, 2 SAW, 3 SQUARE, 4 PULSE 25 %, 5 HYBRID (saw + square)
    struct UnisonBank
    {
        static constexpr int maxVoices = 16;
        struct Table
        {
            int n = 1, wave = 2;
            std::array<float, maxVoices> ratio {}, gl {}, gr {};
            // n voices spread over +-cents, stereo 0..1, pan -1..1, gain linear
            void setup (int voices, int waveType, float cents, float stereo, float pan, float gain)
            {
                n = juce::jlimit (1, maxVoices, voices);
                wave = waveType;
                const float norm = 1.f / std::sqrt ((float) n);
                const float pa = (juce::jlimit (-1.f, 1.f, pan) + 1.f) * juce::MathConstants<float>::pi * 0.25f;
                const float panL = std::cos (pa) * 1.41421f, panR = std::sin (pa) * 1.41421f;
                for (int u = 0; u < n; ++u)
                {
                    const float off = n == 1 ? 0.f : -1.f + 2.f * (float) u / (float) (n - 1);
                    // slightly denser in the middle, like the classic supersaw
                    const float shaped = off * (0.55f + 0.45f * std::abs (off));
                    ratio[(size_t) u] = std::pow (2.f, shaped * cents / 1200.f);
                    // alternate sides so neighbouring detunes land left / right (even the inner voices:
                    // full STEREO makes left and right genuinely different, like a wide supersaw)
                    const float side = n == 1 ? 0.f : (u % 2 == 0 ? 1.f : -1.f) * (0.85f + 0.15f * std::abs (off));
                    const float sp = juce::jlimit (-1.f, 1.f, side * stereo);
                    gl[(size_t) u] = std::sqrt (0.5f * (1.f - sp)) * panL * norm * gain;
                    gr[(size_t) u] = std::sqrt (0.5f * (1.f + sp)) * panR * norm * gain;
                }
            }
        };
        std::array<float, maxVoices> ph {};

        void resetPhases (juce::Random& rng, float startPhase = -1.f)   // < 0: random (free running)
        {
            for (int u = 0; u < maxVoices; ++u)
                ph[(size_t) u] = startPhase < 0.f ? rng.nextFloat() : wrap01 (startPhase + (u == 0 ? 0.f : (float) u * 0.618034f));
        }

        static inline float wave (int w, float t, float dt)
        {
            switch (w)
            {
                case 0:  return fastSin01 (t);
                case 1:  return 4.f * std::abs (t - 0.5f) - 1.f;
                case 2:  return 2.f * t - 1.f - polyBlep (t, dt);
                case 3:  return (t < 0.5f ? 1.f : -1.f) + polyBlep (t, dt) - polyBlep (wrap01 (t + 0.5f), dt);
                case 4:  return (t < 0.25f ? 1.f : -1.f) + polyBlep (t, dt) - polyBlep (wrap01 (t + 0.75f), dt) + 0.5f;
                default:
                {
                    const float saw = 2.f * t - 1.f - polyBlep (t, dt);
                    const float sq = (t < 0.5f ? 1.f : -1.f) + polyBlep (t, dt) - polyBlep (wrap01 (t + 0.5f), dt);
                    return 0.6f * saw + 0.4f * sq;
                }
            }
        }

        // adds one stereo sample; hz = base frequency, invRate = 1 / sample rate
        inline void render (const Table& T, float hz, float invRate, float& l, float& r)
        {
            for (int u = 0; u < T.n; ++u)
            {
                const float dt = juce::jmin (0.45f, hz * T.ratio[(size_t) u] * invRate);
                float& p = ph[(size_t) u];
                p += dt; if (p >= 1.f) p -= 1.f;
                const float s = wave (T.wave, p, dt);
                l += s * T.gl[(size_t) u];
                r += s * T.gr[(size_t) u];
            }
        }
    };

    // ------------------------------------------------------------ visual feed
    template <int Size = 1024>
    struct LevelFeed
    {
        std::array<float, Size> lo {}, hi {};
        std::atomic<int> write { 0 };
        int chunk = 176, pos = 0;
        float cLo = 1.f, cHi = -1.f;
        void prepare (double sr, double msPerRow = 4.0) { chunk = juce::jmax (16, (int) (sr * msPerRow * 0.001)); pos = 0; }
        void push (const float* L, const float* R, int n)
        {
            int w = write.load (std::memory_order_relaxed);
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (L[i] + R[i]);
                cLo = juce::jmin (cLo, m); cHi = juce::jmax (cHi, m);
                if (++pos >= chunk)
                {
                    lo[(size_t) w] = cLo; hi[(size_t) w] = cHi;
                    w = (w + 1) & (Size - 1);
                    pos = 0; cLo = 1.f; cHi = -1.f;
                }
            }
            write.store (w, std::memory_order_release);
        }
    };

    // ------------------------------------------------------------ FX chain: CHORUS -> DELAY -> REVERB -> PUMP
    struct FxParams
    {
        bool  choOn = false;  float choRate = 0.5f, choDepth = 0.5f, choMix = 0.35f;
        bool  dlyOn = false;  float dlyBeats = 0.75f, dlyFb = 0.35f, dlyMix = 0.18f; bool dlyPP = true;
        bool  revOn = true;   float revSize = 0.75f, revDamp = 0.4f, revMix = 0.25f;
        bool  pumpOn = false; float pumpBeats = 1.f, pumpDepth = 0.6f, pumpShape = 0.5f;
    };

    class FxChain
    {
    public:
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate;
            dlyL.assign ((size_t) (sr * 4.5) + 8, 0.f); dlyR.assign (dlyL.size(), 0.f);
            choL.assign ((size_t) (sr * 0.05) + 8, 0.f); choR.assign (choL.size(), 0.f);
            revL.assign ((size_t) juce::jmax (32, maxBlock), 0.f); revR.assign (revL.size(), 0.f);
            reverb.setSampleRate (sr);
            sDly.reset (sr, 0.08); sDly.setCurrentAndTargetValue ((float) (sr * 0.375));
            reset();
        }
        void reset()
        {
            std::fill (dlyL.begin(), dlyL.end(), 0.f); std::fill (dlyR.begin(), dlyR.end(), 0.f);
            std::fill (choL.begin(), choL.end(), 0.f); std::fill (choR.begin(), choR.end(), 0.f);
            reverb.reset();
            dlyW = choW = 0; fbLp = {}; fbHp = {}; revHp = {};
            choPh = 0.f; pumpG = 1.f; freeBeat = 0.0;
        }
        // call once per block before process(); bpm from the host, extraRev adds to the reverb mix (e.g. a macro)
        void setup (const FxParams& fp, double bpm, float extraRevSize = 0.f)
        {
            p = fp;
            tempo = juce::jlimit (30.0, 400.0, bpm);
            sDly.setTargetValue ((float) juce::jlimit (16.0, (double) dlyL.size() - 8.0, p.dlyBeats * 60.0 / tempo * sr));
            juce::Reverb::Parameters rp;
            rp.roomSize = juce::jlimit (0.f, 1.f, p.revSize + extraRevSize);
            rp.damping = p.revDamp; rp.wetLevel = 0.33f; rp.dryLevel = 0.f; rp.width = 1.f; rp.freezeMode = 0.f;
            reverb.setParameters (rp);
        }
        // ppqAtStart < 0: host not playing (PUMP free-runs); offset = samples since the block start
        void process (float* L, float* R, int len, double ppqAtStart, int offset, float extraRevMix = 0.f)
        {
            const float twoPi = juce::MathConstants<float>::twoPi;
            // CHORUS
            {
                const int sz = (int) choL.size();
                const float inc = p.choRate / (float) sr;
                const float base = 0.007f * (float) sr, depth = 0.0045f * (float) sr * p.choDepth;
                const float mix = p.choOn ? p.choMix : 0.f;
                for (int i = 0; i < len; ++i)
                {
                    choL[(size_t) choW] = L[i]; choR[(size_t) choW] = R[i];
                    if (mix > 0.f)
                    {
                        choPh += inc; if (choPh >= 1.f) choPh -= 1.f;
                        const float dl = base + depth * (0.5f + 0.5f * fastSin01 (choPh));
                        const float dr = base + depth * (0.5f + 0.5f * fastSin01 (wrap01 (choPh + 0.25f)));
                        auto tap = [&] (const std::vector<float>& line, float d)
                        {
                            float rp = (float) choW - d; if (rp < 0.f) rp += (float) sz;
                            const int i0 = (int) rp, i1 = (i0 + 1) % sz;
                            const float fr = rp - (float) i0;
                            return line[(size_t) i0] + fr * (line[(size_t) i1] - line[(size_t) i0]);
                        };
                        const float wl = tap (choR, dl), wr = tap (choL, dr);
                        L[i] = L[i] * (1.f - 0.5f * mix) + wl * mix * 0.7f;
                        R[i] = R[i] * (1.f - 0.5f * mix) + wr * mix * 0.7f;
                    }
                    if (++choW >= sz) choW = 0;
                }
            }
            // DELAY
            {
                const int sz = (int) dlyL.size();
                const float lpC = onePoleCoef (6000.f, sr), hpC = onePoleCoef (200.f, sr);
                const float fb = p.dlyOn ? p.dlyFb : 0.f, mix = p.dlyOn ? p.dlyMix : 0.f;
                for (int i = 0; i < len; ++i)
                {
                    const float t = sDly.getNextValue();
                    float rp = (float) dlyW - t; if (rp < 0.f) rp += (float) sz;
                    const int i0 = (int) rp, i1 = (i0 + 1) % sz;
                    const float fr = rp - (float) i0;
                    float dl = dlyL[(size_t) i0] + fr * (dlyL[(size_t) i1] - dlyL[(size_t) i0]);
                    float dr = dlyR[(size_t) i0] + fr * (dlyR[(size_t) i1] - dlyR[(size_t) i0]);
                    fbLp[0] += lpC * (dl - fbLp[0]); dl = fbLp[0];
                    fbLp[1] += lpC * (dr - fbLp[1]); dr = fbLp[1];
                    fbHp[0] += hpC * (dl - fbHp[0]); dl -= fbHp[0];
                    fbHp[1] += hpC * (dr - fbHp[1]); dr -= fbHp[1];
                    const float inL = p.dlyOn ? L[i] : 0.f, inR = p.dlyOn ? R[i] : 0.f;
                    if (p.dlyPP) { dlyL[(size_t) dlyW] = 0.5f * (inL + inR) + dr * fb; dlyR[(size_t) dlyW] = dl * fb; }
                    else         { dlyL[(size_t) dlyW] = inL + dl * fb;                dlyR[(size_t) dlyW] = inR + dr * fb; }
                    if (++dlyW >= sz) dlyW = 0;
                    L[i] += dl * mix; R[i] += dr * mix;
                }
            }
            // REVERB
            {
                const float hpC = onePoleCoef (220.f, sr);
                for (int i = 0; i < len; ++i)
                {
                    revHp[0] += hpC * (L[i] - revHp[0]); revHp[1] += hpC * (R[i] - revHp[1]);
                    revL[(size_t) i] = p.revOn ? L[i] - revHp[0] : 0.f;
                    revR[(size_t) i] = p.revOn ? R[i] - revHp[1] : 0.f;
                }
                reverb.processStereo (revL.data(), revR.data(), len);
                const float mix = p.revOn ? juce::jlimit (0.f, 1.f, p.revMix + extraRevMix) : 0.f;
                const float dry = 1.f - 0.3f * mix;
                for (int i = 0; i < len; ++i) { L[i] = L[i] * dry + revL[(size_t) i] * mix; R[i] = R[i] * dry + revR[(size_t) i] * mix; }
            }
            // PUMP
            {
                const double bps = tempo / 60.0 / sr;
                const float k = 1.f + 7.f * p.pumpShape;
                const float sm = 1.f - std::exp (-1.f / (0.0015f * (float) sr));
                for (int i = 0; i < len; ++i)
                {
                    double beat;
                    if (ppqAtStart >= 0.0) beat = ppqAtStart + (double) (offset + i) * bps;
                    else { beat = freeBeat; freeBeat += bps; }
                    float target = 1.f;
                    if (p.pumpOn)
                    {
                        const double ph = beat / p.pumpBeats;
                        target = 1.f - p.pumpDepth * std::pow (1.f - (float) (ph - std::floor (ph)), k);
                    }
                    pumpG += sm * (target - pumpG);
                    L[i] *= pumpG; R[i] *= pumpG;
                }
            }
            juce::ignoreUnused (twoPi);
        }

    private:
        FxParams p;
        double sr = 44100.0, tempo = 150.0, freeBeat = 0.0;
        std::vector<float> dlyL, dlyR, choL, choR, revL, revR;
        int dlyW = 0, choW = 0;
        std::array<float, 2> fbLp {}, fbHp {}, revHp {};
        float choPh = 0.f, pumpG = 1.f;
        juce::SmoothedValue<float> sDly;
        juce::Reverb reverb;
    };
}
