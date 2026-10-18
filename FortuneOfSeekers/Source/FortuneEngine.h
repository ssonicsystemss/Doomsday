#pragma once
// =====================================================================
//  FORTUNE OF SEEKERS - hardstyle melody synth engine
//
//  polyphonic (12 voices, 16 slots for click-free stealing) or MONO legato
//
//  PART A:  OSC A1 + OSC A2 -> FILTER (env) -> AMP --> DIST --(2x OS)--> CHORUS -> DELAY -> REVERB -> PUMP -> LEVEL --+
//  PART B:  OSC B1 + OSC B2 -> FILTER (env) -> AMP --> DIST --(2x OS)--> CHORUS -> DELAY -> REVERB -> PUMP -> LEVEL --+--> VOLUME -> soft clip
//
//  every oscillator: wave, unison VOICES with DETUNE / STEREO spread, OCTAVE, SEMI, VOLUME, PHASE, PAN, SCREAM
//  FORTUNE macro: opens the filters, widens the detune, adds scream and space on both parts
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include <array>
#include <atomic>
#include <memory>

struct OscParams
{
    bool  on = true;
    int   wave = 2;               // SINE TRI SAW SQUARE PULSE NOISE
    int   voices = 5;
    int   octave = 0, semi = 0;
    float gain = 0.7f;            // linear
    float phase = 1.f;            // 0..1 start phase, >= 0.995 = free (random)
    float detune = 0.3f, stereo = 0.7f, pan = 0.f, scream = 0.f;
};

struct PartParams
{
    bool  on = true;
    std::array<OscParams, 2> osc;
    int   ftype = 1;
    float cutoff = 9000.f, reso = 0.15f, fdrive = 0.f, fenvOct = 0.f, fdecayMs = 300.f;
    float attackMs = 2.f, decayMs = 400.f, sustain = 1.f, releaseMs = 250.f;
    float level = 0.7f;
    bool  distOn = false; int distType = 0; float distDriveDb = 9.f, distMix = 1.f;
    bool  choOn = false;  float choRate = 0.5f, choDepth = 0.5f, choMix = 0.35f;
    bool  dlyOn = false;  float dlyBeats = 0.75f, dlyFb = 0.35f, dlyMix = 0.18f; bool dlyPP = true;
    bool  revOn = true;   float revSize = 0.75f, revDamp = 0.4f, revMix = 0.25f;
    bool  pumpOn = false; float pumpBeats = 1.f, pumpDepth = 0.6f, pumpShape = 0.5f;
};

struct EngineParams
{
    std::array<PartParams, 2> part;
    bool   mono = false;
    float  glideMs = 0.f;
    float  fortune = 0.f;
    float  volume = 0.7f;
    double bpm = 150.0;
    double ppq = -1.0;            // host position at block start (< 0: host not playing)
};

class FortuneEngine
{
public:
    static constexpr int numSlots = 16, polyLimit = 12, maxUnison = 9;
    static constexpr int pathSize = 1024;          // visual feed: min/max per ~4 ms

    // ---------------------------------------------------------------- visual feed (audio thread writes, GUI reads)
    struct PathFeed
    {
        std::array<float, pathSize> lo {}, hi {};
        std::atomic<int> write { 0 };
    };
    std::array<PathFeed, 2> path;
    std::atomic<float> outPeak { 0.f };

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        osr = sr * 2.0;
        maxBlockSize = juce::jmax (32, maxBlock);
        for (auto& f : fx)
        {
            f.os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
            f.os->initProcessing ((size_t) maxBlockSize);
            f.buf.setSize (2, maxBlockSize);
            f.dlyL.assign ((size_t) (sr * 4.5) + 8, 0.f);
            f.dlyR.assign (f.dlyL.size(), 0.f);
            f.choL.assign ((size_t) (sr * 0.05) + 8, 0.f);
            f.choR.assign (f.choL.size(), 0.f);
            f.revL.assign ((size_t) maxBlockSize, 0.f);
            f.revR.assign ((size_t) maxBlockSize, 0.f);
            f.reverb.setSampleRate (sr);
            f.sGain.reset (osr, 0.02);
            f.sDlyTime.reset (sr, 0.08);
            f.sDlyTime.setCurrentAndTargetValue ((float) (sr * 0.375));
        }
        sVolume.reset (sr, 0.03);
        sVolume.setCurrentAndTargetValue (p.volume);
        pathChunk = juce::jmax (16, (int) (sr * 0.004));
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v = {};
        heldMono.clear();
        for (auto& f : fx)
        {
            if (f.os) f.os->reset();
            std::fill (f.dlyL.begin(), f.dlyL.end(), 0.f); std::fill (f.dlyR.begin(), f.dlyR.end(), 0.f);
            std::fill (f.choL.begin(), f.choL.end(), 0.f); std::fill (f.choR.begin(), f.choR.end(), 0.f);
            f.reverb.reset();
            f.dlyWrite = f.choWrite = 0;
            f.fbLp = {}; f.fbHp = {}; f.revHp = {}; f.dcX = {}; f.dcY = {};
            f.choPhase = 0.f; f.pumpGain = 1.f; f.freePump = 0.0;
            f.chunkPos = 0; f.chunkLo = 1.f; f.chunkHi = -1.f;
        }
        pitchWheel = 0.f; pedal = false; lastNote = -1;
    }

    void setParams (const EngineParams& np) { p = np; }

    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
    {
        const int n = buffer.getNumSamples();
        blockSetup();
        int pos = 0;
        blockStartPpq = p.ppq;
        blockOffset = 0;
        for (const auto meta : midi)
        {
            const int at = juce::jlimit (0, n, meta.samplePosition);
            if (at > pos) { renderChunked (buffer, pos, at - pos); pos = at; }
            handleMidi (meta.getMessage());
        }
        if (pos < n) renderChunked (buffer, pos, n - pos);
    }

    int activeVoices() const
    {
        int c = 0;
        for (auto& v : voices) c += v.active ? 1 : 0;
        return c;
    }

private:
    // ================================================================ helpers
    static inline float polyBlep (float t, float dt)
    {
        if (t < dt)       { t /= dt;            return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }
    static inline float wrap01 (float x) { return x - std::floor (x); }
    static inline float fastSin01 (float ph)     // sine of a 0..1 phase, ~0.1 % error
    {
        float x = ph * 2.f - 1.f;                  // -1..1  (-> sin(pi * x) with a sign flip)
        float y = 4.f * x * (1.f - std::abs (x));
        y = 0.225f * (y * std::abs (y) - y) + y;
        return -y;
    }
    static inline float fastTanh (float x)
    {
        x = juce::jlimit (-3.f, 3.f, x);
        const float x2 = x * x;
        return x * (27.f + x2) / (27.f + 9.f * x2);
    }
    static inline float timeCoef (float ms, double rate)
    {
        const double s = juce::jmax (0.0005, (double) ms * 0.001);
        return (float) std::exp (std::log (0.001) / (s * rate));
    }
    static inline float softClip (float x)
    {
        const float a = std::abs (x);
        if (a < 0.85f) return x;
        const float s = x > 0.f ? 1.f : -1.f;
        return s * (0.85f + 0.15f * std::tanh ((a - 0.85f) / 0.15f));
    }
    static inline float distort (float x, int type)
    {
        switch (type)
        {
            case 0:  return fastTanh (x);                                               // SOFT
            case 1:  return juce::jlimit (-0.9f, 0.9f, x) * 1.1f;                       // HARD
            case 2:  return (std::tanh (x + 0.3f) - 0.2913f) * 0.9f;                   // TUBE (asym, DC blocked after)
            default:                                                                    // SCREAM
            {
                float y = std::tanh (x * 1.3f);
                y += 0.35f * std::sin (x * 2.3f);
                return juce::jlimit (-1.f, 1.f, y * 0.8f);
            }
        }
    }

    // ================================================================ envelope
    struct Env
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
        float value = 0.f;
        void trigger (bool fromZero) { if (fromZero) value = 0.f; stage = Attack; }
        void release() { if (stage != Idle) stage = Release; }
        inline float next (float attInc, float decC, float sus, float relC)
        {
            switch (stage)
            {
                case Attack:  value += attInc; if (value >= 1.f) { value = 1.f; stage = Decay; } break;
                case Decay:   value = sus + (value - sus) * decC;
                              if (std::abs (value - sus) < 1.0e-4f) { value = sus; stage = Sustain; } break;
                case Sustain: value = sus; break;
                case Release: value *= relC; if (value < 1.0e-5f) { value = 0.f; stage = Idle; } break;
                case Idle:    value = 0.f; break;
            }
            return value;
        }
    };

    // ================================================================ filter (TPT SVF, 2 stages for LP24)
    struct Svf
    {
        float ic1 = 0.f, ic2 = 0.f;
        inline void tick (float v0, float a1, float a2, float a3, float& lp, float& bp)
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.f * v1 - ic1;
            ic2 = 2.f * v2 - ic2;
            bp = v1; lp = v2;
        }
    };
    struct ChanFilter { Svf a, b; };
    struct FiltCoefs { float a1 = 1.f, a2 = 0.f, a3 = 0.f, k = 2.f; };

    static inline float filterSample (ChanFilter& f, int type, float x, const FiltCoefs& c)
    {
        float lp, bp;
        f.a.tick (x, c.a1, c.a2, c.a3, lp, bp);
        switch (type)
        {
            case 0:  return lp;
            case 1:  { float lp2, bp2; f.b.tick (lp, c.a1, c.a2, c.a3, lp2, bp2); return lp2; }
            case 2:  return bp * (0.5f + c.k * 0.5f);
            default: return x - c.k * bp - lp;
        }
    }

    // ================================================================ voices
    struct OscState
    {
        std::array<float, maxUnison> ph {};
    };
    struct PartVoice
    {
        Env amp;
        float fenv = 0.f, semi = 60.f;      // each part glides on its own copy of the same curve
        std::array<OscState, 2> osc;
        std::array<ChanFilter, 2> filt;
        FiltCoefs fc;
    };
    struct Voice
    {
        bool active = false, held = false, sustained = false;
        int note = -1;
        float vel = 1.f, curSemi = 60.f, targetSemi = 60.f;
        float kill = 1.f, killStep = 0.f;
        juce::uint64 age = 0;
        int counter = 0;
        std::array<PartVoice, 2> part;
    };

    // per-block oscillator tables
    struct OscTable
    {
        bool live = false;
        int n = 1, wave = 2;
        float tuneRatio = 1.f, screamGain = 1.f, screamFold = 0.f, screamNorm = 1.f;
        std::array<float, maxUnison> ratio {}, gl {}, gr {};
        float noiseL = 0.f, noiseR = 0.f, noiseMix = 0.f;   // noise: stereo decorrelation
    };
    struct PartTable
    {
        bool live = false;
        std::array<OscTable, 2> osc;
        float cutoff = 9000.f, k = 2.f, fdrive = 1.f, fdriveNorm = 1.f, fenvOct = 0.f, fcoef = 0.f;
        float attInc = 1.f, decC = 0.f, sus = 1.f, relC = 0.f;
        int ftype = 1;
    };

    // ================================================================ per-part FX state
    struct PartFx
    {
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        juce::AudioBuffer<float> buf;
        juce::SmoothedValue<float> sGain, sDlyTime;
        std::vector<float> dlyL, dlyR, choL, choR, revL, revR;
        int dlyWrite = 0, choWrite = 0;
        std::array<float, 2> fbLp {}, fbHp {}, revHp {}, dcX {}, dcY {};
        float choPhase = 0.f, pumpGain = 1.f;
        double freePump = 0.0;
        juce::Reverb reverb;
        int chunkPos = 0;
        float chunkLo = 1.f, chunkHi = -1.f;
    };

    // ================================================================ MIDI
    void handleMidi (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber();
            const float vel = m.getFloatVelocity();
            if (p.mono) monoNoteOn (n, vel); else polyNoteOn (n, vel);
        }
        else if (m.isNoteOff())
        {
            const int n = m.getNoteNumber();
            if (p.mono) monoNoteOff (n);
            else
                for (auto& v : voices)
                    if (v.active && v.held && v.note == n)
                    {
                        v.held = false;
                        if (pedal) v.sustained = true; else releaseVoice (v);
                    }
        }
        else if (m.isPitchWheel())
            pitchWheel = juce::jlimit (-1.f, 1.f, (m.getPitchWheelValue() - 8192) / 8192.f);
        else if (m.isSustainPedalOn())  pedal = true;
        else if (m.isSustainPedalOff())
        {
            pedal = false;
            for (auto& v : voices) if (v.active && v.sustained) { v.sustained = false; releaseVoice (v); }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            heldMono.clear();
            for (auto& v : voices) if (v.active) { v.held = false; v.sustained = false; releaseVoice (v); }
        }
    }

    void releaseVoice (Voice& v) { for (auto& pv : v.part) pv.amp.release(); }

    void startVoice (Voice& v, int note, float vel, bool fromZero, bool resetPhase)
    {
        const bool glide = p.glideMs >= 1.f && lastNote >= 0;
        v.active = true; v.held = true; v.sustained = false;
        v.note = note; v.vel = vel;
        v.targetSemi = (float) note;
        v.curSemi = glide ? (float) lastNote : (float) note;
        v.kill = 1.f; v.killStep = 0.f;
        v.age = ++ageCounter;
        v.counter = 0;
        for (int pi = 0; pi < 2; ++pi)
        {
            auto& pv = v.part[(size_t) pi];
            pv.amp.trigger (fromZero);
            pv.fenv = 1.f;
            pv.semi = v.curSemi;
            if (fromZero) for (auto& f : pv.filt) f = {};
            if (! resetPhase) continue;
            for (int o = 0; o < 2; ++o)
            {
                const auto& op = p.part[(size_t) pi].osc[(size_t) o];
                auto& st = pv.osc[(size_t) o];
                const bool free = op.phase >= 0.995f;
                for (int u = 0; u < maxUnison; ++u)
                {
                    const float spread = u == 0 ? 0.f : wrap01 ((float) u * 0.618034f + 0.11f);
                    st.ph[(size_t) u] = free ? rng.nextFloat() : wrap01 (op.phase + spread);
                }
            }
        }
        lastNote = note;
    }

    void polyNoteOn (int note, float vel)
    {
        // same note still ringing: retrigger it in place
        for (auto& v : voices)
            if (v.active && v.note == note && v.killStep == 0.f)
            {
                startVoice (v, note, vel, false, false);
                return;
            }
        int playing = 0;
        for (auto& v : voices) playing += (v.active && v.killStep == 0.f) ? 1 : 0;
        if (playing >= polyLimit)
        {
            // steal: oldest released voice first, else the oldest one - it fades out in 3 ms
            Voice* victim = nullptr;
            for (auto& v : voices)
                if (v.active && v.killStep == 0.f && ! v.held && (victim == nullptr || v.age < victim->age)) victim = &v;
            if (victim == nullptr)
                for (auto& v : voices)
                    if (v.active && v.killStep == 0.f && (victim == nullptr || v.age < victim->age)) victim = &v;
            if (victim != nullptr) victim->killStep = 1.f / (float) (0.003 * osr);
        }
        Voice* slot = nullptr;
        for (auto& v : voices) if (! v.active) { slot = &v; break; }
        if (slot == nullptr)
            for (auto& v : voices) if (slot == nullptr || v.age < slot->age) slot = &v;
        startVoice (*slot, note, vel, true, true);
    }

    void monoNoteOn (int note, float vel)
    {
        heldMono.erase (std::remove (heldMono.begin(), heldMono.end(), note), heldMono.end());
        heldMono.push_back (note);
        auto& v = voices[0];
        for (size_t i = 1; i < voices.size(); ++i) if (voices[i].active && voices[i].killStep == 0.f) voices[i].killStep = 1.f / (float) (0.003 * osr);
        const bool legato = v.active && heldMono.size() > 1 && v.part[0].amp.stage != Env::Release;
        if (legato)
        {
            v.targetSemi = (float) note; v.note = note; v.held = true;
            if (p.glideMs < 1.f) for (auto& pv : v.part) pv.semi = v.targetSemi;
            lastNote = note;
        }
        else
            startVoice (v, note, vel, ! v.active, ! v.active);
    }

    void monoNoteOff (int note)
    {
        const bool wasCurrent = ! heldMono.empty() && heldMono.back() == note;
        heldMono.erase (std::remove (heldMono.begin(), heldMono.end(), note), heldMono.end());
        auto& v = voices[0];
        if (heldMono.empty())
        {
            v.held = false;
            if (pedal) v.sustained = true; else releaseVoice (v);
        }
        else if (wasCurrent)
        {
            v.targetSemi = (float) heldMono.back(); v.note = heldMono.back();
            if (p.glideMs < 1.f) for (auto& pv : v.part) pv.semi = v.targetSemi;
            lastNote = v.note;
        }
    }

    // ================================================================ block setup
    void blockSetup()
    {
        const float fort = juce::jlimit (0.f, 1.f, p.fortune);
        for (int pi = 0; pi < 2; ++pi)
        {
            const auto& pp = p.part[(size_t) pi];
            auto& T = tables[(size_t) pi];
            T.live = pp.on;
            for (int o = 0; o < 2; ++o)
            {
                const auto& op = pp.osc[(size_t) o];
                auto& O = T.osc[(size_t) o];
                O.live = op.on && op.gain > 0.0005f;
                O.wave = op.wave;
                O.n = op.wave == 5 ? 1 : juce::jlimit (1, maxUnison, op.voices);
                O.tuneRatio = std::pow (2.f, (12.f * (float) op.octave + (float) op.semi) / 12.f);
                const float det = juce::jlimit (0.f, 1.f, op.detune + fort * 0.25f);
                const float cents = 70.f * std::pow (det, 1.6f);
                const float scr = juce::jlimit (0.f, 1.f, op.scream + fort * 0.25f * (op.scream > 0.f ? 1.f : 0.4f));
                O.screamGain = 1.f + scr * 7.f;
                O.screamFold = scr * 0.6f;
                O.screamNorm = scr > 0.001f ? 1.f / (std::tanh (O.screamGain * 0.7f) + O.screamFold * 0.5f + 0.2f) * 0.85f : 1.f;
                const float norm = 1.f / std::sqrt ((float) O.n);
                const float panL = std::cos ((juce::jlimit (-1.f, 1.f, op.pan) + 1.f) * juce::MathConstants<float>::pi * 0.25f) * 1.41421f;
                const float panR = std::sin ((juce::jlimit (-1.f, 1.f, op.pan) + 1.f) * juce::MathConstants<float>::pi * 0.25f) * 1.41421f;
                for (int u = 0; u < O.n; ++u)
                {
                    // spread: centre voice first, then alternating outwards (-1..1)
                    const float off = O.n == 1 ? 0.f : -1.f + 2.f * (float) u / (float) (O.n - 1);
                    O.ratio[(size_t) u] = std::pow (2.f, off * cents / 1200.f);
                    const float sp = off * op.stereo;
                    O.gl[(size_t) u] = std::sqrt (0.5f * (1.f - sp)) * panL * norm * op.gain;
                    O.gr[(size_t) u] = std::sqrt (0.5f * (1.f + sp)) * panR * norm * op.gain;
                }
                O.noiseL = panL * op.gain * 0.7f;
                O.noiseR = panR * op.gain * 0.7f;
                O.noiseMix = op.stereo;
            }
            T.ftype = pp.ftype;
            T.cutoff = juce::jlimit (20.f, 20000.f, pp.cutoff * std::pow (2.f, fort * 2.f));
            T.k = 2.f - 1.94f * juce::jlimit (0.f, 1.f, pp.reso);
            T.fdrive = 1.f + pp.fdrive * 5.f;
            T.fdriveNorm = pp.fdrive > 0.001f ? 1.f / std::tanh (T.fdrive) : 1.f;
            T.fenvOct = pp.fenvOct;
            T.fcoef = timeCoef (pp.fdecayMs, osr);
            T.attInc = (float) (1.0 / (juce::jmax (0.3, (double) pp.attackMs) * 0.001 * osr));
            T.decC = timeCoef (pp.decayMs, osr);
            T.sus = pp.sustain;
            T.relC = timeCoef (pp.releaseMs, osr);

            auto& F = fx[(size_t) pi];
            F.sGain.setTargetValue (pp.on ? pp.level : 0.f);
            const double bpm = juce::jlimit (30.0, 400.0, p.bpm);
            F.sDlyTime.setTargetValue ((float) juce::jlimit (16.0, (double) F.dlyL.size() - 8.0, pp.dlyBeats * 60.0 / bpm * sr));
            juce::Reverb::Parameters rp;
            rp.roomSize = juce::jlimit (0.f, 1.f, pp.revSize + fort * 0.1f);
            rp.damping = pp.revDamp; rp.wetLevel = 0.33f; rp.dryLevel = 0.f; rp.width = 1.f; rp.freezeMode = 0.f;
            F.reverb.setParameters (rp);
        }
        glideCoef = p.glideMs < 1.f ? 0.f : (float) std::exp (-1.0 / ((p.glideMs * 0.001 / 3.0) * osr));
        sVolume.setTargetValue (p.volume);
    }

    // ================================================================ rendering
    void renderChunked (juce::AudioBuffer<float>& buffer, int start, int len)
    {
        while (len > 0)
        {
            const int n = juce::jmin (len, maxBlockSize);
            renderSegment (buffer, start, n);
            start += n; len -= n;
        }
    }

    inline float oscSample (const OscTable& O, OscState& st, float hz, float invOsr, float& l, float& r)
    {
        float sl = 0.f, sr_ = 0.f;
        if (O.wave == 5)
        {
            const float a = rng.nextFloat() * 2.f - 1.f, b = rng.nextFloat() * 2.f - 1.f;
            sl = a; sr_ = a + O.noiseMix * (b - a);
            l += sl * O.noiseL; r += sr_ * O.noiseR;
            return 0.f;
        }
        for (int u = 0; u < O.n; ++u)
        {
            const float dt = juce::jmin (0.45f, hz * O.ratio[(size_t) u] * invOsr);
            float& ph = st.ph[(size_t) u];
            ph += dt; if (ph >= 1.f) ph -= 1.f;
            float s;
            switch (O.wave)
            {
                case 0:  s = fastSin01 (ph); break;
                case 1:  s = 4.f * std::abs (ph - 0.5f) - 1.f; break;
                case 2:  s = 2.f * ph - 1.f - polyBlep (ph, dt); break;
                case 3:  s = (ph < 0.5f ? 1.f : -1.f) + polyBlep (ph, dt) - polyBlep (wrap01 (ph + 0.5f), dt); break;
                default: s = (ph < 0.25f ? 1.f : -1.f) + polyBlep (ph, dt) - polyBlep (wrap01 (ph + 0.75f), dt) + 0.5f; break;
            }
            sl += s * O.gl[(size_t) u];
            sr_ += s * O.gr[(size_t) u];
        }
        if (O.screamFold > 0.f)
        {
            // SCREAM: hard drive + fold-back -> nasal, screechy harmonics (rendered 2x oversampled)
            sl = (fastTanh (sl * O.screamGain) + O.screamFold * std::sin (sl * O.screamGain * 1.7f)) * O.screamNorm;
            sr_ = (fastTanh (sr_ * O.screamGain) + O.screamFold * std::sin (sr_ * O.screamGain * 1.7f)) * O.screamNorm;
        }
        l += sl; r += sr_;
        return 0.f;
    }

    void renderSegment (juce::AudioBuffer<float>& buffer, int start, int len)
    {
        const float invOsr = (float) (1.0 / osr);
        const float bend = pitchWheel * 2.f;
        float* outL = buffer.getWritePointer (0, start);
        float* outR = buffer.getWritePointer (1, start);
        std::fill (outL, outL + len, 0.f);
        std::fill (outR, outR + len, 0.f);

        for (int pi = 0; pi < 2; ++pi)
        {
            auto& F = fx[(size_t) pi];
            auto& T = tables[(size_t) pi];
            const auto& pp = p.part[(size_t) pi];
            F.buf.clear (0, len);
            float* chans[] = { F.buf.getWritePointer (0), F.buf.getWritePointer (1) };
            juce::dsp::AudioBlock<float> block (chans, 2, (size_t) len);
            auto os = F.os->processSamplesUp (block);
            float* oL = os.getChannelPointer (0);
            float* oR = os.getChannelPointer (1);
            const int osLen = (int) os.getNumSamples();
            std::fill (oL, oL + osLen, 0.f);
            std::fill (oR, oR + osLen, 0.f);

            const bool partAudible = T.live || F.sGain.isSmoothing() || F.sGain.getCurrentValue() > 0.f;

            for (auto& v : voices)
            {
                if (! v.active) continue;
                auto& pv = v.part[(size_t) pi];
                for (int i = 0; i < osLen; ++i)
                {
                    const float semi = pv.semi = v.targetSemi + (pv.semi - v.targetSemi) * glideCoef;

                    const float a = pv.amp.next (T.attInc, T.decC, T.sus, T.relC);
                    pv.fenv *= T.fcoef;
                    if (! partAudible || a <= 0.f) continue;

                    const float hz = 440.f * std::exp2 ((semi + bend - 69.f) / 12.f);
                    float l = 0.f, r = 0.f;
                    for (int o = 0; o < 2; ++o)
                        if (T.osc[(size_t) o].live) oscSample (T.osc[(size_t) o], pv.osc[(size_t) o], hz * T.osc[(size_t) o].tuneRatio, invOsr, l, r);

                    if (((v.counter + i) & 7) == 0)
                    {
                        const float fc = juce::jlimit (20.f, (float) (osr * 0.45),
                                                       T.cutoff * std::exp2 (T.fenvOct * pv.fenv + 0.4f * (semi - 60.f) / 12.f));
                        const float g = std::tan (juce::MathConstants<float>::pi * fc * invOsr);
                        pv.fc.k = T.k;
                        pv.fc.a1 = 1.f / (1.f + g * (g + T.k));
                        pv.fc.a2 = g * pv.fc.a1;
                        pv.fc.a3 = g * pv.fc.a2;
                    }
                    if (T.fdrive > 1.001f) { l = fastTanh (l * T.fdrive) * T.fdriveNorm; r = fastTanh (r * T.fdrive) * T.fdriveNorm; }
                    l = filterSample (pv.filt[0], T.ftype, l, pv.fc);
                    r = filterSample (pv.filt[1], T.ftype, r, pv.fc);

                    float g = a * (0.55f + 0.45f * v.vel);
                    if (v.killStep > 0.f) g *= juce::jmax (0.f, v.kill - v.killStep * (float) i);
                    oL[i] += l * g;
                    oR[i] += r * g;
                }
            }
            if (pi == 1)
            {
                // voice bookkeeping once per block: steal fades + finished voices
                for (auto& v : voices)
                {
                    if (! v.active) continue;
                    v.counter += osLen;
                    if (v.killStep > 0.f) { v.kill -= v.killStep * (float) osLen; if (v.kill <= 0.f) { v = {}; continue; } }
                    const bool aIdle = v.part[0].amp.stage == Env::Idle, bIdle = v.part[1].amp.stage == Env::Idle;
                    if (aIdle && bIdle) v = {};
                }
            }

            // DIST (oversampled) + part gain
            const float drv = juce::Decibels::decibelsToGain (pp.distDriveDb);
            const float dmix = pp.distOn ? pp.distMix : 0.f;
            const float dnorm = 0.5f / juce::jmax (0.5f, std::abs (distort (drv * 0.5f, pp.distType)));   // keeps the level steady
            for (int i = 0; i < osLen; ++i)
            {
                const float gp = F.sGain.getNextValue();
                float l = oL[i], r = oR[i];
                if (dmix > 0.f)
                {
                    const float dl = distort (l * drv, pp.distType) * dnorm;
                    const float dr = distort (r * drv, pp.distType) * dnorm;
                    l += dmix * (dl - l); r += dmix * (dr - r);
                }
                oL[i] = l * gp; oR[i] = r * gp;
            }
            F.os->processSamplesDown (block);

            renderFx (F, pp, chans[0], chans[1], len, pi);
            for (int i = 0; i < len; ++i) { outL[i] += chans[0][i]; outR[i] += chans[1][i]; }
        }

        // master
        float peak = outPeak.load (std::memory_order_relaxed);
        for (int i = 0; i < len; ++i)
        {
            const float vol = sVolume.getNextValue();
            outL[i] = softClip (outL[i] * vol);
            outR[i] = softClip (outR[i] * vol);
            peak = juce::jmax (peak, std::abs (outL[i]), std::abs (outR[i]));
        }
        outPeak.store (peak, std::memory_order_relaxed);
        blockOffset += len;
    }

    void renderFx (PartFx& F, const PartParams& pp, float* L, float* R, int len, int pi)
    {
        const float twoPi = juce::MathConstants<float>::twoPi;

        // DC blocker after the (possibly asymmetric) distortion
        if (pp.distOn && pp.distType == 2)
            for (int i = 0; i < len; ++i)
            {
                float* c[] = { L, R };
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float x = c[ch][i];
                    F.dcY[(size_t) ch] = x - F.dcX[(size_t) ch] + 0.9993f * F.dcY[(size_t) ch];
                    F.dcX[(size_t) ch] = x;
                    c[ch][i] = F.dcY[(size_t) ch];
                }
            }

        // CHORUS: two modulated delay lines, quadrature LFO
        {
            const int sz = (int) F.choL.size();
            const float inc = pp.choRate / (float) sr;
            const float base = 0.007f * (float) sr, depth = 0.0045f * (float) sr * pp.choDepth;
            const float mix = pp.choOn ? pp.choMix : 0.f;
            for (int i = 0; i < len; ++i)
            {
                F.choL[(size_t) F.choWrite] = L[i];
                F.choR[(size_t) F.choWrite] = R[i];
                if (mix > 0.f)
                {
                    F.choPhase += inc; if (F.choPhase >= 1.f) F.choPhase -= 1.f;
                    const float dl = base + depth * (0.5f + 0.5f * fastSin01 (F.choPhase));
                    const float dr = base + depth * (0.5f + 0.5f * fastSin01 (wrap01 (F.choPhase + 0.25f)));
                    auto tap = [&] (const std::vector<float>& line, float d)
                    {
                        float rp = (float) F.choWrite - d; if (rp < 0.f) rp += (float) sz;
                        const int i0 = (int) rp, i1 = (i0 + 1) % sz;
                        const float fr = rp - (float) i0;
                        return line[(size_t) i0] + fr * (line[(size_t) i1] - line[(size_t) i0]);
                    };
                    const float wl = tap (F.choR, dl), wr = tap (F.choL, dr);     // cross-fed for width
                    L[i] = L[i] * (1.f - 0.5f * mix) + wl * mix * 0.7f;
                    R[i] = R[i] * (1.f - 0.5f * mix) + wr * mix * 0.7f;
                }
                if (++F.choWrite >= sz) F.choWrite = 0;
            }
        }

        // DELAY (tempo synced, filtered feedback, ping-pong)
        {
            const int sz = (int) F.dlyL.size();
            const float lpC = 1.f - std::exp (-twoPi * 6000.f / (float) sr);
            const float hpC = 1.f - std::exp (-twoPi * 200.f / (float) sr);
            const float fb = pp.dlyOn ? pp.dlyFb : 0.f;
            const float mix = pp.dlyOn ? pp.dlyMix : 0.f;
            for (int i = 0; i < len; ++i)
            {
                const float t = F.sDlyTime.getNextValue();
                float rp = (float) F.dlyWrite - t; if (rp < 0.f) rp += (float) sz;
                const int i0 = (int) rp, i1 = (i0 + 1) % sz;
                const float fr = rp - (float) i0;
                float dl = F.dlyL[(size_t) i0] + fr * (F.dlyL[(size_t) i1] - F.dlyL[(size_t) i0]);
                float dr = F.dlyR[(size_t) i0] + fr * (F.dlyR[(size_t) i1] - F.dlyR[(size_t) i0]);
                F.fbLp[0] += lpC * (dl - F.fbLp[0]); dl = F.fbLp[0];
                F.fbLp[1] += lpC * (dr - F.fbLp[1]); dr = F.fbLp[1];
                F.fbHp[0] += hpC * (dl - F.fbHp[0]); dl -= F.fbHp[0];
                F.fbHp[1] += hpC * (dr - F.fbHp[1]); dr -= F.fbHp[1];
                const float inL = pp.dlyOn ? L[i] : 0.f, inR = pp.dlyOn ? R[i] : 0.f;
                if (pp.dlyPP)
                {
                    F.dlyL[(size_t) F.dlyWrite] = 0.5f * (inL + inR) + dr * fb;
                    F.dlyR[(size_t) F.dlyWrite] = dl * fb;
                }
                else
                {
                    F.dlyL[(size_t) F.dlyWrite] = inL + dl * fb;
                    F.dlyR[(size_t) F.dlyWrite] = inR + dr * fb;
                }
                if (++F.dlyWrite >= sz) F.dlyWrite = 0;
                L[i] += dl * mix;
                R[i] += dr * mix;
            }
        }

        // REVERB
        {
            const float hpC = 1.f - std::exp (-twoPi * 220.f / (float) sr);
            for (int i = 0; i < len; ++i)
            {
                F.revHp[0] += hpC * (L[i] - F.revHp[0]);
                F.revHp[1] += hpC * (R[i] - F.revHp[1]);
                F.revL[(size_t) i] = pp.revOn ? L[i] - F.revHp[0] : 0.f;
                F.revR[(size_t) i] = pp.revOn ? R[i] - F.revHp[1] : 0.f;
            }
            F.reverb.processStereo (F.revL.data(), F.revR.data(), len);
            const float mix = pp.revOn ? juce::jlimit (0.f, 1.f, pp.revMix + p.fortune * 0.15f) : 0.f;
            const float dry = 1.f - 0.3f * mix;
            for (int i = 0; i < len; ++i)
            {
                L[i] = L[i] * dry + F.revL[(size_t) i] * mix;
                R[i] = R[i] * dry + F.revR[(size_t) i] * mix;
            }
        }

        // PUMP: tempo-synced ducking (follows the host's beat grid while playing)
        {
            const double beatsPerSample = juce::jlimit (30.0, 400.0, p.bpm) / 60.0 / sr;
            const float k = 1.f + 7.f * pp.pumpShape;
            const float smooth = 1.f - std::exp (-1.f / (0.0015f * (float) sr));
            for (int i = 0; i < len; ++i)
            {
                double beat;
                if (blockStartPpq >= 0.0) beat = blockStartPpq + (double) (blockOffset + i) * beatsPerSample;
                else { beat = F.freePump; F.freePump += beatsPerSample; }
                float target = 1.f;
                if (pp.pumpOn)
                {
                    const double ph = beat / pp.pumpBeats;
                    const float x = (float) (ph - std::floor (ph));
                    target = 1.f - pp.pumpDepth * std::pow (1.f - x, k);
                }
                F.pumpGain += smooth * (target - F.pumpGain);
                L[i] *= F.pumpGain; R[i] *= F.pumpGain;
            }
        }

        // visual feed
        auto& P = path[(size_t) pi];
        int w = P.write.load (std::memory_order_relaxed);
        for (int i = 0; i < len; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            F.chunkLo = juce::jmin (F.chunkLo, m);
            F.chunkHi = juce::jmax (F.chunkHi, m);
            if (++F.chunkPos >= pathChunk)
            {
                P.lo[(size_t) w] = F.chunkLo; P.hi[(size_t) w] = F.chunkHi;
                w = (w + 1) & (pathSize - 1);
                F.chunkPos = 0; F.chunkLo = 1.f; F.chunkHi = -1.f;
            }
        }
        P.write.store (w, std::memory_order_release);
    }

    // ================================================================ state
    EngineParams p;
    double sr = 44100.0, osr = 88200.0;
    int maxBlockSize = 512, pathChunk = 176;
    std::array<Voice, numSlots> voices;
    std::array<PartTable, 2> tables;
    std::array<PartFx, 2> fx;
    std::vector<int> heldMono;
    juce::uint64 ageCounter = 0;
    int lastNote = -1;
    float glideCoef = 0.f, pitchWheel = 0.f;
    bool pedal = false;
    double blockStartPpq = -1.0;
    int blockOffset = 0;
    juce::SmoothedValue<float> sVolume;
    juce::Random rng;
};
