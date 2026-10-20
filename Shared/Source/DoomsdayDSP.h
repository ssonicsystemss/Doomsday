#pragma once
// =====================================================================
//  DOOMSDAY shared DSP blocks (header only)
//    dsp::polyBlep / wrap01 / fastSin01 / fastTanh / timeCoef / softClip
//    dsp::Env            ADSR (linear attack, exponential decay / release)
//    dsp::Svf, svfCoefs  TPT state-variable filter (+ LP24 / BAND / HIGH helper)
//    dsp::UnisonBank     polyBLEP unison oscillator (up to 16 voices, detune / stereo / pan)
//    dsp::FxChain        stereo CHORUS -> DELAY -> REVERB -> PUMP (tempo synced)
//    dsp::LevelFeed      min/max-per-chunk feed for GUI visualisations
//    dsp::shape / Screamer / DcBlocker      waveshapers (SOFT HARD TUBE SCREAM), per-osc SCREAM
//    dsp::NoteStack / VoiceManager          mono legato note stack, polyphonic allocation + stealing + pedal
//    (from Mirror of Souls)  BiquadCoefs / Biquad / butterworthQs, AnalyzerTap, Spectrum
//    (from Prophecy of Scream v3)  XorNoise, PitchSweep, Formant, Ott3, StereoSpread
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>
#include <vector>
#include <atomic>
#include <complex>

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

    // ------------------------------------------------------------ waveshapers (from Fortune of Seekers)
    // 0 SOFT, 1 HARD, 2 TUBE (asymmetric: follow with a DcBlocker), 3 SCREAM (tanh + fold-back)
    inline float shape (float x, int type)
    {
        switch (type)
        {
            case 0:  return fastTanh (x);
            case 1:  return juce::jlimit (-0.9f, 0.9f, x) * 1.1f;
            case 2:  return (std::tanh (x + 0.3f) - 0.2913f) * 0.9f;
            default: return juce::jlimit (-1.f, 1.f, (std::tanh (x * 1.3f) + 0.35f * std::sin (x * 2.3f)) * 0.8f);
        }
    }
    // output gain that keeps the level roughly steady as the drive goes up
    inline float shapeMakeup (float driveGain, int type) { return 0.5f / juce::jmax (0.5f, std::abs (shape (driveGain * 0.5f, type))); }

    // per-oscillator SCREAM: hard drive + fold-back -> nasal, screechy harmonics (amount 0..1)
    struct Screamer
    {
        float gain = 1.f, fold = 0.f, norm = 1.f;
        void set (float amount)
        {
            const float a = juce::jlimit (0.f, 1.f, amount);
            gain = 1.f + a * 7.f;
            fold = a * 0.6f;
            norm = a > 0.001f ? 0.85f / (std::tanh (gain * 0.7f) + fold * 0.5f + 0.2f) : 1.f;
        }
        bool active() const { return fold > 0.f; }
        inline float process (float x) const { return (fastTanh (x * gain) + fold * std::sin (x * gain * 1.7f)) * norm; }
    };

    struct DcBlocker
    {
        float x1 = 0.f, y1 = 0.f, r = 0.9993f;
        inline float process (float x) { y1 = x - x1 + r * y1; x1 = x; return y1; }
    };

    // ------------------------------------------------------------ note handling (from Fortune of Seekers)
    // mono / legato: the held-note stack (last note priority)
    struct NoteStack
    {
        std::array<int, 32> notes {};
        int count = 0;
        void clear() { count = 0; }
        bool empty() const { return count == 0; }
        int top() const { return count > 0 ? notes[(size_t) count - 1] : -1; }
        void remove (int n)
        {
            int w = 0;
            for (int i = 0; i < count; ++i) if (notes[(size_t) i] != n) notes[(size_t) w++] = notes[(size_t) i];
            count = w;
        }
        void push (int n) { remove (n); if (count < (int) notes.size()) notes[(size_t) count++] = n; }
    };

    // polyphony: voice allocation with click-free stealing and the sustain pedal.
    // Voice needs: bool active, held, sustained; int note; juce::uint64 age; float kill, killStep.
    template <typename Voice, size_t Slots>
    struct VoiceManager
    {
        std::array<Voice, Slots>& voices;
        juce::uint64 ageCounter = 0;
        bool pedal = false;
        explicit VoiceManager (std::array<Voice, Slots>& v) : voices (v) {}

        int playing() const { int c = 0; for (auto& v : voices) c += (v.active && v.killStep == 0.f) ? 1 : 0; return c; }

        // returns the voice to (re)start for this note; sameNote = true when a ringing voice of that note is reused.
        // Voices over the limit get killStep = fadeStep (fade them out in the render loop, then reset them).
        Voice& noteOn (int note, int polyLimit, float fadeStep, bool& sameNote)
        {
            sameNote = false;
            for (auto& v : voices)
                if (v.active && v.note == note && v.killStep == 0.f) { sameNote = true; v.age = ++ageCounter; return v; }
            if (playing() >= polyLimit)
            {
                Voice* victim = nullptr;      // oldest released voice first, else the oldest
                for (auto& v : voices) if (v.active && v.killStep == 0.f && ! v.held && (victim == nullptr || v.age < victim->age)) victim = &v;
                if (victim == nullptr)
                    for (auto& v : voices) if (v.active && v.killStep == 0.f && (victim == nullptr || v.age < victim->age)) victim = &v;
                if (victim != nullptr) victim->killStep = fadeStep;
            }
            Voice* slot = nullptr;
            for (auto& v : voices) if (! v.active) { slot = &v; break; }
            if (slot == nullptr) for (auto& v : voices) if (slot == nullptr || v.age < slot->age) slot = &v;
            *slot = Voice {};
            slot->age = ++ageCounter;
            return *slot;
        }
        // release: callable that releases a voice's envelopes
        template <typename Release>
        void noteOff (int note, Release&& release)
        {
            for (auto& v : voices)
                if (v.active && v.held && v.note == note)
                {
                    v.held = false;
                    if (pedal) v.sustained = true; else release (v);
                }
        }
        template <typename Release>
        void setPedal (bool down, Release&& release)
        {
            pedal = down;
            if (! down) for (auto& v : voices) if (v.active && v.sustained) { v.sustained = false; release (v); }
        }
        template <typename Release>
        void allOff (Release&& release) { for (auto& v : voices) if (v.active) { v.held = false; v.sustained = false; release (v); } }
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

// =====================================================================
//  EQ + analyser blocks (from Mirror of Souls)
//    dsp::Biquad / BiquadCoefs     TDF2 biquad, RBJ designs (bell, shelves, notch, band, HP, LP) + magnitude()
//    dsp::AnalyzerTap              lock-free ring of mono samples (audio thread writes, GUI reads)
//    dsp::Spectrum                 FFT 4096 -> smoothed dB per frequency (message thread)
// =====================================================================
namespace doomsday::dsp
{
    struct BiquadCoefs
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        enum Type { Bell, LowShelf, HighShelf, Notch, BandPass, HighPass, LowPass };
        static BiquadCoefs make (Type t, double fs, double hz, double q, double gainDb = 0.0)
        {
            hz = juce::jlimit (5.0, fs * 0.49, hz);
            q = juce::jmax (0.05, q);
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w = juce::MathConstants<double>::twoPi * hz / fs, cw = std::cos (w), sw = std::sin (w);
            const double alpha = sw / (2.0 * q);
            double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
            switch (t)
            {
                case Bell:      b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
                case LowShelf:
                {
                    const double s = 2 * std::sqrt (A) * alpha;
                    b0 = A * ((A + 1) - (A - 1) * cw + s); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - s);
                    a0 = (A + 1) + (A - 1) * cw + s;       a1 = -2 * ((A - 1) + (A + 1) * cw);     a2 = (A + 1) + (A - 1) * cw - s;
                    break;
                }
                case HighShelf:
                {
                    const double s = 2 * std::sqrt (A) * alpha;
                    b0 = A * ((A + 1) + (A - 1) * cw + s); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - s);
                    a0 = (A + 1) - (A - 1) * cw + s;       a1 = 2 * ((A - 1) - (A + 1) * cw);       a2 = (A + 1) - (A - 1) * cw - s;
                    break;
                }
                case Notch:     b0 = 1; b1 = -2 * cw; b2 = 1; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
                case BandPass:  b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
                case HighPass:  b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
                case LowPass:   b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            }
            BiquadCoefs c; c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0;
            return c;
        }
        // |H| at hz (linear)
        double magnitude (double hz, double fs) const
        {
            const double w = juce::MathConstants<double>::twoPi * hz / fs;
            const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
            return std::abs ((b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2));
        }
    };

    struct Biquad
    {
        double z1 = 0, z2 = 0;
        inline float process (float xf, const BiquadCoefs& c)
        {
            const double x = xf;
            const double y = c.b0 * x + z1;
            z1 = c.b1 * x - c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            return (float) y;
        }
        void reset() { z1 = z2 = 0; }
    };

    // Butterworth stage Qs for 2, 4 and 8 pole cuts (12 / 24 / 48 dB per octave)
    inline std::vector<double> butterworthQs (int stages)
    {
        if (stages <= 1) return { 0.70710678 };
        if (stages == 2) return { 0.54119610, 1.30656296 };
        if (stages == 4) return { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
        std::vector<double> q;               // any order: Q of each 2nd-order section of a 2*stages Butterworth
        for (int k = 1; k <= stages; ++k) q.push_back (1.0 / (2.0 * std::sin ((2.0 * k - 1.0) * juce::MathConstants<double>::pi / (4.0 * stages))));
        std::sort (q.begin(), q.end());
        return q;
    }

    template <int Size = 8192>
    struct AnalyzerTap
    {
        std::array<float, Size> buf {};
        std::atomic<int> write { 0 };
        void push (const float* L, const float* R, int n)
        {
            int w = write.load (std::memory_order_relaxed);
            for (int i = 0; i < n; ++i) { buf[(size_t) w] = 0.5f * (L[i] + (R != nullptr ? R[i] : L[i])); w = (w + 1) & (Size - 1); }
            write.store (w, std::memory_order_release);
        }
        // copies the newest n samples (oldest first)
        void latest (float* dest, int n) const
        {
            const int w = write.load (std::memory_order_acquire);
            for (int i = 0; i < n; ++i) dest[i] = buf[(size_t) ((w - n + i + 4 * Size) & (Size - 1))];
        }
    };

    // message-thread FFT analyser: feed() the newest 4096 samples each frame, read dbAt(hz)
    class Spectrum
    {
    public:
        static constexpr int order = 12, size = 1 << order;
        Spectrum() : fft (order), window ((size_t) size), data ((size_t) size * 2), smooth ((size_t) size / 2, -120.f)
        {
            for (int i = 0; i < size; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (size - 1));
        }
        void setSampleRate (double s) { sr = s; }
        // fall: dB per frame the display may drop (rise is instant)
        void feed (const float* samples, float fallDb = 2.5f)
        {
            for (int i = 0; i < size; ++i) data[(size_t) i] = samples[i] * window[(size_t) i];
            std::fill (data.begin() + size, data.end(), 0.f);
            fft.performFrequencyOnlyForwardTransform (data.data());
            const float norm = 4.f / (float) size;     // Hann gain compensation, peak = 0 dB for a full-scale sine
            for (int k = 1; k < size / 2; ++k)
            {
                const float db = juce::Decibels::gainToDecibels (data[(size_t) k] * norm, -120.f);
                auto& s = smooth[(size_t) k];
                s = db > s ? db : juce::jmax (db, s - fallDb);
            }
        }
        // smoothed level at hz: interpolated between bins at the low end, power-averaged over ~1/12 octave above
        float dbAt (double hz) const
        {
            const double c = hz / (sr / size);
            const double half = c * 0.03;
            if (half < 1.0)
            {
                const int k = juce::jlimit (1, size / 2 - 2, (int) c);
                const float t = (float) juce::jlimit (0.0, 1.0, c - k);
                return smooth[(size_t) k] + t * (smooth[(size_t) k + 1] - smooth[(size_t) k]);
            }
            const int a = juce::jlimit (1, size / 2 - 1, (int) (c - half)), b = juce::jlimit (1, size / 2 - 1, (int) (c + half));
            double pw = 0.0;
            for (int k = a; k <= b; ++k) pw += std::pow (10.0, smooth[(size_t) k] / 10.0);
            return (float) (10.0 * std::log10 (pw / (b - a + 1) + 1e-12));
        }
    private:
        juce::dsp::FFT fft;
        std::vector<float> window, data, smooth;
        double sr = 48000.0;
    };
}


// =====================================================================
//  Screech tools (from Prophecy of Scream v3)
//    XorNoise      cheap white noise (-1..1), one per channel for wide, dense "dirt"
//    PitchSweep    dive / riser after a note start: 0 -> semis, eased out over a time
//    Formant       resonant bell on a stereo signal - the moving "voice" peak of a screech
//    Ott3          3-band up/down compressor (quiet bands up, loud bands down) - the dense hardstyle top
//    StereoSpread  wider stereo: more side + a decorrelated copy of the mid above ~220 Hz
//  All of them are neutral at amount 0. No allocation after prepare().
// =====================================================================
namespace doomsday::dsp
{
    struct XorNoise
    {
        uint32_t state = 0x9e3779b9u;
        inline float next()
        {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            return (float) (int32_t) state * (1.f / 2147483648.f);
        }
    };

    struct PitchSweep
    {
        float pos = 1.0e9f, step = 0.f, now = 0.f;
        void setTime (float ms, double rate) { step = (float) (1.0 / (juce::jmax (1.0, (double) ms) * 0.001 * rate)); }
        void start() { pos = 0.f; }
        void reset() { pos = 1.0e9f; }
        // call once per sample; returns the pitch offset in semitones
        inline float next (float semis)
        {
            const float x = juce::jmin (1.f, pos);
            now = semis * (1.f - (1.f - x) * (1.f - x));
            if (pos < 1.f) pos += step;
            return now;
        }
    };

    struct Formant
    {
        std::array<Svf, 2> f {};
        void reset() { f = {}; }
        // hz must already be limited to a safe range; gain is linear (1 = flat); amt 0..1 fades the stage in
        inline void process (float& l, float& r, float hz, float q, float gain, float amt, double rate)
        {
            const float g = std::tan (juce::MathConstants<float>::pi * hz * (float) (1.0 / rate));
            const float k = 1.f / juce::jmax (0.3f, q);
            SvfCoefs c; c.k = k;
            c.a1 = 1.f / (1.f + g * (g + k)); c.a2 = g * c.a1; c.a3 = g * c.a2;
            float lp, bp;
            f[0].tick (l, c, lp, bp); const float yl = l + (gain - 1.f) * k * bp;
            f[1].tick (r, c, lp, bp); const float yr = r + (gain - 1.f) * k * bp;
            const float makeup = 1.f / (1.f + (gain - 1.f) * 0.18f);
            l += amt * (yl * makeup - l);
            r += amt * (yr * makeup - r);
        }
    };

    class Ott3
    {
    public:
        void prepare (double sampleRate)
        {
            const float sr = (float) sampleRate;
            loC = 2.f * std::sin (juce::MathConstants<float>::pi * 180.f / sr);
            hiC = 2.f * std::sin (juce::MathConstants<float>::pi * 2200.f / sr);
            att = 1.f - std::exp (-1.f / (0.002f * sr));
            rel = 1.f - std::exp (-1.f / (0.09f * sr));
            reset();
        }
        void reset() { st = {}; }
        // amount 0..1 (0 = bypass, exact)
        inline void process (float& l, float& r, float amount)
        {
            if (amount <= 0.001f) return;
            auto& o = st;
            float bl[3], br[3];
            // Chamberlin state-variable low-pass (s1 = low, s2 = band), Q ~ 0.7
            auto lp2 = [] (float x, float c, float& s1, float& s2) { s1 += c * s2; const float hp = x - s1 - 1.414f * s2; s2 += c * hp; return s1; };
            const float loL = lp2 (l, loC, o.lo[0][0], o.lo[0][1]), loR = lp2 (r, loC, o.lo[1][0], o.lo[1][1]);
            const float hiL = l - lp2 (l, hiC, o.hi[0][0], o.hi[0][1]), hiR = r - lp2 (r, hiC, o.hi[1][0], o.hi[1][1]);
            bl[0] = loL; br[0] = loR; bl[2] = hiL; br[2] = hiR;
            bl[1] = l - loL - hiL; br[1] = r - loR - hiR;
            const float amt = amount * 0.75f;
            const float target[3] = { 0.10f, 0.16f, 0.09f };
            l = 0.f; r = 0.f;
            for (int b = 0; b < 3; ++b)
            {
                const float pk = juce::jmax (std::abs (bl[b]), std::abs (br[b]));
                o.env[b] += (pk > o.env[b] ? att : rel) * (pk - o.env[b]);
                const float gn = juce::jlimit (0.2f, 6.f, std::pow (target[b] / (o.env[b] + 1.0e-4f), amt));
                l += bl[b] * gn; r += br[b] * gn;
            }
        }
    private:
        struct State { float lo[2][2] {}, hi[2][2] {}, env[3] {}; } st;
        float loC = 0.f, hiC = 0.f, att = 0.f, rel = 0.f;
    };

    class StereoSpread
    {
    public:
        void prepare (double sampleRate)
        {
            for (size_t i = 0; i < ap.size(); ++i)
            {
                ap[i].buf.assign ((size_t) (sampleRate * 0.012) + 4, 0.f);
                ap[i].len = juce::jmax (8, (int) (sampleRate * (i == 0 ? 0.0041 : 0.0067)));
                ap[i].pos = 0;
            }
            hpC = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 220.f / (float) sampleRate);
            reset();
        }
        void reset() { for (auto& a : ap) { std::fill (a.buf.begin(), a.buf.end(), 0.f); a.pos = 0; } hp = 0.f; }
        // amount 0..1 (0 = bypass, exact); mono-compatible: L + R is unchanged
        inline void process (float& l, float& r, float amount)
        {
            if (amount <= 0.001f) return;
            const float mid = 0.5f * (l + r);
            float side = 0.5f * (l - r);
            float y2[2];
            for (size_t a = 0; a < ap.size(); ++a)
            {
                auto& A = ap[a];
                int rp = A.pos - A.len; if (rp < 0) rp += (int) A.buf.size();
                const float d = A.buf[(size_t) rp];
                const float y = -0.55f * mid + d;
                A.buf[(size_t) A.pos] = mid + 0.55f * y;
                if (++A.pos >= (int) A.buf.size()) A.pos = 0;
                y2[a] = y;
            }
            float extra = 0.5f * (y2[0] - y2[1]);
            hp += hpC * (extra - hp);
            extra -= hp;
            side = side * (1.f + 0.6f * amount) + extra * 1.4f * amount;
            l = mid + side; r = mid - side;
        }
    private:
        struct Allpass { std::vector<float> buf; int len = 8, pos = 0; };
        std::array<Allpass, 2> ap;
        float hpC = 0.f, hp = 0.f;
    };
}
