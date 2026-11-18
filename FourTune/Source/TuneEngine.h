#pragma once
// =====================================================================
//  4TUNE - SOUND ENGINE (DSP only: knows no GUI and no files)
//  OSC 1-4 (unison) -> GROUP A / B -> FILTER -> AMP ENV -> SCREAM -> EQ -> DIST -> CHORUS -> DELAY -> REVERB -> STEREO -> LIMITER
//  FORTUNE = a modulation layer on top of the knobs (FINAL = BASE + FORTUNE), never replaces them.
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "DoomsdayDSP.h"
#include "DoomsdayDrive.h"
#include "DoomsdayFilter.h"
#include "DoomsdaySpace.h"
#include "DoomsdayDynamics.h"
#include <array>
#include <cmath>

namespace tune::dsp
{
    namespace dd = doomsday::dsp;
    constexpr int maxPoly = 16, maxUni = 16, numOsc = 4, numSlots = 16, sub = 16;
    constexpr float voiceGain = 0.42f;

    // ------------------------------------------------------------------ settings (cooked once per block)
    struct EnvSet { float a = 1, h = 0, d = 300, s = 0.5f, r = 200, curve = 0.5f; };
    struct OscSet { bool on = false; int wave = 0; float wt = 0, warp = 0, semis = 0, level = 0, pan = 0, width = 1, phase = 0, phRand = 0;
                    bool retrig = true; int voices = 1; float detune = 0, uspread = 0, umix = 1; };
    struct LfoSet { int shape = 0; float hz = 2, depth = 1, phase = 0, fadeMs = 0; bool retrig = true; };
    struct Slot   { int src = 0, dst = 0; float amt = 0; };

    // mod sources / destinations (indices = the Choice lists in Params.h)
    enum Src { SNone, SAmpEnv, SFltEnv, SModEnv, SLfo1, SLfo2, SVel, SWheel, SAfter, SKey, SRand, SFortune, numSrc };
    enum Dst { DNone, DPitch, DO1Pitch, DO2Pitch, DO3Pitch, DO4Pitch, DFine, DWt, DWarp, DCutoff, DRes, DDetune, DSpread, DPan, DVolume,
               DO1Level, DO2Level, DO3Level, DO4Level, DGroupA, DGroupB, DFltDrive, DScream, DDelayMix, DReverbMix, numDst };

    struct Settings
    {
        std::array<OscSet, numOsc> osc;
        float gaGain = 1, gaPan = 0, gaWidth = 1, gbGain = 1, gbPan = 0, gbWidth = 1;
        int fltType = 1, route = 0; float cut = 1400, res = 0.2f, fltDriveDb = 3, key = 0.5f, envOct = 3;
        EnvSet fe, ae, me;
        std::array<LfoSet, 2> lfo;
        std::array<Slot, numSlots> slots;
        float scr = 0, scrDriveDb = 8, scrSat = 0.4f, scrTone = 0, scrBite = 0.4f, scrBody = 0.5f, scrMix = 1, scrOutDb = 0; int scrOs = 1;
        float fortune = 0, ftCut = 1, ftDet = 1, ftWidth = 1, ftScr = 1, ftRMix = 1, ftRSize = 1;
        bool eqOn = true; float eqLow = 0, eqLowMid = 0, eqHighMid = 0, eqHigh = 0;
        bool distOn = false; float distDriveDb = 9, distTone = 0, distMix = 0.5f, distOutDb = 0; int distOs = 1, fxOrder = 0;
        bool choOn = false; float choRate = 0.6f, choDepth = 0.4f, choWidth = 0.8f, choMix = 0.3f;
        bool dlyOn = true; double dlyBeats = 0.75; float dlyFb = 0.35f; bool dlyPP = true; float dlyFilter = 5000, dlyMix = 0.14f;
        bool rvbOn = true; float rvbPre = 15, rvbSize = 0.55f, rvbDecay = 2.2f, rvbDamp = 7000, rvbWidth = 1, rvbMix = 0.16f;
        float stWidth = 1, stBalance = 0, stMonoBass = 120;
        bool limOn = true; float limInDb = 0, limCeil = -0.3f, limRel = 80;
        int playMode = 0, polyVoices = 12; float glideMs = 0, bendRange = 2, velSens = 0.4f, transpose = 0, master = 0.63f;
        double bpm = 150.0;
    };

    // FORTUNE response (spec 13-19): one shaped curve drives every destination so it reads as one transformation
    struct FortuneMap
    {
        static float curve (float f) { f = juce::jlimit (0.f, 1.f, f); return f * f * (3.f - 2.f * f) * 0.6f + f * 0.4f; }   // slow start, steady middle, full at 1
        static float cutOct (const Settings& s)   { return 2.f * s.ftCut * std::pow (juce::jlimit (0.f, 1.f, s.fortune), 1.3f); }       // up to +2 oct
        static float detuneAdd (const Settings& s, int osc)                                                                             // cents added on top of the knob
        {
            const float f = curve (s.fortune) * s.ftDet;
            return f * (osc < 2 ? 8.f : 30.f);                       // group A stays focused, group B becomes the supersaw (spec 14)
        }
        static float detuneMul (const Settings& s, int osc) { return 1.f + curve (s.fortune) * s.ftDet * (osc < 2 ? 0.25f : 0.8f); }
        static float widthAdd (const Settings& s)  { return curve (s.fortune) * s.ftWidth * 0.5f; }
        // SCREAM (spec 15): little when the knob is at 0, a lot when it is already active, pushes it past the top at 100 %
        static float screamAdd (const Settings& s, float base)
        {
            const float f = std::pow (juce::jlimit (0.f, 1.f, s.fortune), 1.5f) * s.ftScr;
            return f * (0.10f + 0.60f * juce::jlimit (0.f, 1.f, base));
        }
        static float rvbMixAdd (const Settings& s)  { return 0.15f * s.ftRMix * curve (s.fortune); }    // +15 points
        static float rvbSizeAdd (const Settings& s) { return 0.10f * s.ftRSize * curve (s.fortune); }   // +10 points
    };

    // ------------------------------------------------------------------ AHDSR with curve
    struct Ahdsr
    {
        enum Stage { Idle, Attack, Hold, Decay, Sustain, Release };
        Stage stage = Idle; float t = 0, value = 0, from = 0;
        static inline float down (float t, float c)     // 1 -> 0
        {
            const float e = 1.f + 4.f * std::abs (c);
            return c >= 0.f ? std::pow (1.f - t, e) : 1.f - std::pow (t, e);
        }
        void trigger (bool fromZero) { from = fromZero ? 0.f : value; stage = Attack; t = 0; }
        void release() { if (stage != Idle) { from = value; stage = Release; t = 0; } }
        void kill() { stage = Idle; value = 0; }
        bool idle() const { return stage == Idle; }
        inline float next (const EnvSet& e, float invSrMs)   // invSrMs = 1000 / sr
        {
            switch (stage)
            {
                case Idle: value = 0; break;
                case Attack:
                    t += e.a > 0.05f ? invSrMs / e.a : 1.f;
                    if (t >= 1.f) { value = 1.f; stage = e.h > 0.05f ? Hold : Decay; t = 0; break; }
                    value = from + (1.f - from) * (1.f - down (t, e.curve));     // positive curve = fast rise
                    break;
                case Hold:
                    t += invSrMs / e.h; value = 1.f;
                    if (t >= 1.f) { stage = Decay; t = 0; }
                    break;
                case Decay:
                    t += invSrMs / juce::jmax (1.f, e.d);
                    if (t >= 1.f) { value = e.s; stage = Sustain; break; }
                    value = e.s + (1.f - e.s) * down (t, e.curve);
                    break;
                case Sustain: value = e.s; if (e.s <= 0.0001f) { stage = Idle; value = 0; } break;
                case Release:
                    t += invSrMs / juce::jmax (1.f, e.r);
                    if (t >= 1.f) { stage = Idle; value = 0; break; }
                    value = from * down (t, e.curve);
                    break;
            }
            return value;
        }
    };

    // ------------------------------------------------------------------ LFO (per voice + 2 free ones for the global destinations)
    struct LfoState
    {
        float ph = 0, held = 0, target = 0, smooth = 0, ageMs = 0; uint32_t seed = 12345;
        float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 8388608.f - 1.f; }
        void restart (float phase) { ph = phase; ageMs = 0; held = rnd(); target = rnd(); }
        // advance by dtMs, return -1..1 (depth and fade applied)
        float tick (const LfoSet& l, float dtMs)
        {
            const float prev = ph;
            ph += l.hz * dtMs * 0.001f;
            const bool wrapped = ph >= 1.f; if (wrapped) ph -= std::floor (ph);
            if (wrapped) { held = target; target = rnd(); }
            float v = 0;
            switch (l.shape)
            {
                case 0: v = dd::fastSin01 (ph); break;
                case 1: v = 1.f - 4.f * std::abs (ph - 0.5f); break;
                case 2: v = 1.f - 2.f * ph; break;                    // SAW = falling
                case 3: v = 2.f * ph - 1.f; break;                    // RAMP = rising
                case 4: v = ph < 0.5f ? 1.f : -1.f; break;
                case 5: v = held; break;                              // S&H
                default: v = held + (target - held) * (ph * ph * (3.f - 2.f * ph)); break;   // RANDOM (smooth)
            }
            juce::ignoreUnused (prev);
            ageMs += dtMs;
            const float fade = l.fadeMs > 1.f ? juce::jlimit (0.f, 1.f, ageMs / l.fadeMs) : 1.f;
            return v * l.depth * fade;
        }
    };

    // ------------------------------------------------------------------ oscillator wave (anti-aliased where it matters)
    struct Wave
    {
        static inline float pd (float p, float k) { return p < k ? 0.5f * p / k : 0.5f + 0.5f * (p - k) / (1.f - k); }
        static inline float pdInv (float q, float k) { return q < 0.5f ? 2.f * q * k : k + (q - 0.5f) * 2.f * (1.f - k); }
        static inline float saw (float p, float q, float dt) { return 2.f * q - 1.f - dd::polyBlep (p, dt); }
        static inline float square (float p, float q, float k, float dt)
        {
            return (q < 0.5f ? 1.f : -1.f) + dd::polyBlep (p, dt) - dd::polyBlep (dd::wrap01 (p - k + 1.f), dt);
        }
        static inline float sine (float q) { return dd::fastSin01 (q); }
        static inline float tri (float q) { return 4.f * std::abs (q - 0.5f) - 1.f; }
        // p = phase 0..1, dt = phase step, wave 0 SAW 1 SINE 2 TRI 3 SQUARE 4 PULSE 5 WAVETABLE, pos / warp 0..1
        static inline float get (int wave, float p, float dt, float pos, float warp)
        {
            const float k = 0.5f - 0.45f * warp;
            const float q = warp > 0.001f ? pd (p, k) : p;
            switch (wave)
            {
                case 0: return saw (p, q, dt);
                case 1: return sine (q);
                case 2: return tri (q);
                case 3: return square (p, q, k, dt);
                case 4:
                {
                    const float pw = 0.5f - 0.45f * pos;
                    const float edge = warp > 0.001f ? pdInv (pw, k) : pw;
                    return (q < pw ? 1.f : -1.f) + dd::polyBlep (p, dt) - dd::polyBlep (dd::wrap01 (p - edge + 1.f), dt) + (2.f * pw - 1.f) * -1.f;
                }
                default:
                {
                    const float x = juce::jlimit (0.f, 1.f, pos) * 3.f;
                    const int i = juce::jmin (2, (int) x); const float fr = x - (float) i;
                    auto one = [&] (int w) { return w == 0 ? sine (q) : w == 1 ? tri (q) : w == 2 ? saw (p, q, dt) : square (p, q, k, dt); };
                    return one (i) * (1.f - fr) + one (i + 1) * fr;
                }
            }
        }
    };

    // ------------------------------------------------------------------ one voice
    struct Voice
    {
        bool active = false, held = false; int note = 60; float vel = 1; uint64_t age = 0;
        float pitch = 60, target = 60, rnd = 0;
        Ahdsr ae, fe, me;
        std::array<LfoState, 2> lfo;
        std::array<std::array<float, maxUni>, numOsc> ph {};
        // control-rate tables
        std::array<std::array<float, maxUni>, numOsc> ratio {}, gl {}, gr {};
        std::array<int, numOsc> nUni {};
        std::array<float, numOsc> hz {}, wt {}, warp {}, oscMid {}, oscSide {};
        float gA = 1, gB = 1, panAL = 1, panAR = 1, panBL = 1, panBR = 1, wA = 1, wB = 1;
        float cutHz = 1000, res = 0.2f, driveDb = 0, volMul = 1, panL = 1, panR = 1;
        doomsday::filter::Ladder ladL, ladR;
        dd::Svf notchL, notchR; dd::SvfCoefs notchC;
        int fltMode = 0; bool notch = false;
        float lastCut = -1;
        std::array<float, numSrc> src {};
        std::array<std::array<float, 6>, numOsc> key {};       // last unison table inputs (recompute only on change)
    };

    // ------------------------------------------------------------------ SCREAM (spec 11)
    class Scream
    {
    public:
        void prepare (double rate, int maxBlock)
        {
            sr = rate; os.prepare (2, maxBlock);
            for (int c = 0; c < 2; ++c) { ag[(size_t) c].prepare (rate * 4.0); dc[(size_t) c].prepare (rate * 4.0); }
            reset();
        }
        void reset() { os.reset(); for (auto& a : ag) a.reset(); for (auto& d : dc) d.reset(); lowS = hiS = toneS = biteS = {}; }
        float latency() const { return os.latencySamples(); }
        // amount = SCREAM after FORTUNE / modulation (0..1.6)
        void process (juce::AudioBuffer<float>& b, int start, int n, const Settings& s, float amount)
        {
            if (amount < 0.002f || s.scrMix < 0.001f) { active = false; return; }
            active = true;
            os.setFactorIndex (juce::jlimit (0, 2, s.scrOs));
            const double rate = sr * os.ratio();
            const float driveDb = s.scrDriveDb * juce::jmin (1.f, amount) + 22.f * amount * amount;          // 0.1 -> ~1 dB, 0.5 -> ~10 dB, 1 -> ~30 dB
            const float g = juce::Decibels::decibelsToGain (juce::jmin (48.f, driveDb));
            const float fold = juce::jlimit (0.f, 1.f, (amount - 0.55f) / 0.9f);                              // wavefolding only in the upper range
            const float lowC = dd::onePoleCoef (180.f, rate), hiC = dd::onePoleCoef (2400.f, rate), exC = dd::onePoleCoef (3500.f, rate);
            const float toneC = dd::onePoleCoef (1500.f, rate);
            const float biteAmt = s.scrBite * (0.6f + amount), body = s.scrBody, sat = s.scrSat, tone = s.scrTone, mix = s.scrMix;
            const float out = juce::Decibels::decibelsToGain (s.scrOutDb);
            juce::dsp::AudioBlock<float> blk (b.getArrayOfWritePointers(), 2, (size_t) start, (size_t) n);
            os.process (blk, [&] (int c, float x)
            {
                const size_t ci = (size_t) c;
                // split: the lows only go through the shaper as much as BODY says (the rest stays clean)
                lowS[ci] += lowC * (x - lowS[ci]);
                const float low = lowS[ci], rest = x - low;
                // BITE: the 2-3 kHz edge pushed before the shaper
                biteS[ci] += hiC * (rest - biteS[ci]);
                const float hiPart = rest - biteS[ci];
                float in = (rest + hiPart * biteAmt * 1.6f + low * body) * g;
                // SATURATION: even harmonics (warm asymmetry) before the clipper
                in = in + sat * 0.35f * in * in / (1.f + std::abs (in));
                const float soft = dd::fastTanh (in);
                const float folded = std::sin (juce::jlimit (-8.f, 8.f, in) * 1.5707963f * (1.f + fold));
                float y = soft * (1.f - fold * 0.7f) + folded * fold * 0.7f;
                // HF excitation (bright screaming air), more with BITE
                hiS[ci] += exC * (y - hiS[ci]);
                const float air = y - hiS[ci];
                y += dd::fastTanh (air * 3.f) * 0.25f * biteAmt;
                y = dc[ci].process (y);
                // TONE tilt after the shaper
                toneS[ci] += toneC * (y - toneS[ci]);
                const float lowT = toneS[ci], highT = y - lowT;
                y = lowT * (1.f - 0.5f * juce::jmax (0.f, tone)) + highT * (1.f + 0.8f * tone);
                y += low * (1.f - body);
                y = ag[ci].process (x, y) * out;                   // gain compensation: SCREAM changes the character, not the loudness
                return x + mix * (y - x);
            });
        }
        bool active = false;
    private:
        double sr = 48000;
        doomsday::drive::OverSampler os;
        std::array<doomsday::drive::AutoGain, 2> ag;
        std::array<doomsday::drive::DcBlock, 2> dc;
        std::array<float, 2> lowS {}, hiS {}, toneS {}, biteS {};
    };

    // ------------------------------------------------------------------ small stereo chorus
    class Chorus
    {
    public:
        void prepare (double rate) { sr = rate; for (auto& d : buf) d.assign ((size_t) (rate * 0.06) + 8, 0.f); w = 0; ph = 0; }
        void reset() { for (auto& d : buf) std::fill (d.begin(), d.end(), 0.f); }
        inline void process (float& l, float& r, float rateHz, float depth, float width, float mix)
        {
            const int size = (int) buf[0].size();
            buf[0][(size_t) w] = l; buf[1][(size_t) w] = r;
            ph += rateHz / (float) sr; if (ph >= 1.f) ph -= 1.f;
            const float base = 0.012f * (float) sr, dep = depth * 0.006f * (float) sr;
            float o[2];
            for (int c = 0; c < 2; ++c)
            {
                const float m = dd::fastSin01 (dd::wrap01 (ph + (c == 0 ? 0.f : 0.25f * width)));
                const float d = base + dep * (0.5f + 0.5f * m);
                float rp = (float) w - d; while (rp < 0) rp += (float) size;
                const int i0 = (int) rp; const float fr = rp - (float) i0;
                const float a = buf[(size_t) c][(size_t) i0], b2 = buf[(size_t) c][(size_t) ((i0 + 1) % size)];
                o[c] = a + (b2 - a) * fr;
            }
            if (++w >= size) w = 0;
            const float wetL = o[0] * (0.5f + 0.5f * width) + o[1] * (0.5f - 0.5f * width);
            const float wetR = o[1] * (0.5f + 0.5f * width) + o[0] * (0.5f - 0.5f * width);
            l += mix * (wetL - l); r += mix * (wetR - r);
        }
    private:
        double sr = 48000; std::array<std::vector<float>, 2> buf; int w = 0; float ph = 0;
    };

    // ------------------------------------------------------------------ the engine
    class Engine
    {
    public:
        void prepare (double rate, int maxBlock)
        {
            sr = rate; invSrMs = (float) (1000.0 / rate);
            for (auto& v : voices) { v.ladL.prepare (rate); v.ladR.prepare (rate); v.active = false; }
            scream.prepare (rate, maxBlock);
            distOs.prepare (2, maxBlock);
            chorus.prepare (rate); echo.prepare (rate); reverb.prepare (rate);
            limiter.prepare (rate, 1.5f);
            for (auto& d : distAdaa) d = {};
            for (auto& d : distDc) d.prepare (rate);
            reset();
        }
        void reset()
        {
            for (auto& v : voices) { v.active = false; v.ae.kill(); v.fe.kill(); v.me.kill(); v.ladL.reset(); v.ladR.reset(); }
            scream.reset(); distOs.reset(); chorus.reset(); echo.reset(); reverb.reset(); limiter.reset();
            for (auto& e : eq) e = {};
            numHeld = 0; bend = 0; wheel = 0; after = 0;
        }
        int latencySamples() const { return (s.limOn ? limiter.latencySamples() : 0); }
        void setSettings (const Settings& st) { s = st; }
        const Settings& settings() const { return s; }

        // ---- MIDI
        void noteOn (int note, float velocity)
        {
            lastVel = velocity; lastKey = note;
            const bool mono = s.playMode != 0;
            if (mono)
            {
                pushHeld (note);
                auto& v = voices[0];
                const bool legato = s.playMode == 2 && v.active && v.held;
                startVoice (v, note, velocity, ! legato, legato || (s.playMode == 1 && v.active));
                return;
            }
            // POLY: a free voice, else steal the oldest (envelopes continue from their level: no click)
            const int limit = juce::jlimit (1, maxPoly, s.polyVoices);
            Voice* pick = nullptr;
            for (int i = 0; i < limit; ++i) if (! voices[(size_t) i].active) { pick = &voices[(size_t) i]; break; }
            if (pick == nullptr)
            {
                for (int i = 0; i < limit; ++i) if (! voices[(size_t) i].held && (pick == nullptr || voices[(size_t) i].age < pick->age)) pick = &voices[(size_t) i];
                if (pick == nullptr) for (int i = 0; i < limit; ++i) if (pick == nullptr || voices[(size_t) i].age < pick->age) pick = &voices[(size_t) i];
            }
            startVoice (*pick, note, velocity, true, s.glideMs > 0.5f && lastPitch >= 0.f);
        }
        void noteOff (int note)
        {
            if (s.playMode != 0)
            {
                popHeld (note);
                auto& v = voices[0];
                if (numHeld > 0)
                {
                    if (v.note != held[(size_t) numHeld - 1]) startVoice (v, held[(size_t) numHeld - 1], v.vel, s.playMode == 1, true);
                    return;
                }
                if (v.active && v.note == note) { v.held = false; v.ae.release(); v.fe.release(); v.me.release(); }
                return;
            }
            for (auto& v : voices)
                if (v.active && v.held && v.note == note) { v.held = false; v.ae.release(); v.fe.release(); v.me.release(); }
        }
        void allNotesOff() { for (auto& v : voices) if (v.active) { v.held = false; v.ae.release(); v.fe.release(); v.me.release(); } numHeld = 0; }
        void setPitchBend (float m11) { bend = m11; }        // -1..1
        void setModWheel (float v) { wheel = v; }
        void setAftertouch (float v) { after = v; }

        // ---- audio: render n samples starting at 'start' (adds nothing - writes)
        void render (juce::AudioBuffer<float>& b, int start, int n)
        {
            auto* L = b.getWritePointer (0) + start; auto* R = b.getWritePointer (1) + start;
            std::fill (L, L + n, 0.f); std::fill (R, R + n, 0.f);
            for (auto& v : voices) if (v.active) renderVoice (v, L, R, n);
            post (b, start, n);
        }

        float scopeLevel() const { return outPeak; }
        bool screamActive() const { return scream.active; }
        float globalMod (int dst) const { return gmod[(size_t) dst]; }

    private:
        // ---------------------------------------------------- voices
        void pushHeld (int note) { popHeld (note); if (numHeld < (int) held.size()) held[(size_t) numHeld++] = note; }
        void popHeld (int note)
        {
            for (int i = 0; i < numHeld; ++i)
                if (held[(size_t) i] == note) { for (int j = i; j < numHeld - 1; ++j) held[(size_t) j] = held[(size_t) j + 1]; --numHeld; return; }
        }

        void startVoice (Voice& v, int note, float vel, bool retrigEnvs, bool glide)
        {
            const bool wasActive = v.active;
            v.note = note; v.vel = vel; v.held = true; v.age = ++ageCounter;
            v.target = (float) note;
            if (! glide || s.glideMs < 0.5f) v.pitch = (float) note;
            else if (! wasActive || s.playMode == 0) v.pitch = lastPitch >= 0.f ? lastPitch : (float) note;
            lastPitch = (float) note;
            v.rnd = rng.nextFloat() * 2.f - 1.f;
            if (retrigEnvs || ! wasActive)
            {
                v.ae.trigger (! wasActive); v.fe.trigger (! wasActive); v.me.trigger (! wasActive);
                for (int l = 0; l < 2; ++l) if (s.lfo[(size_t) l].retrig || ! wasActive) v.lfo[(size_t) l].restart (s.lfo[(size_t) l].phase);
                for (int o = 0; o < numOsc; ++o)
                {
                    const auto& os = s.osc[(size_t) o];
                    if (! os.retrig && wasActive) continue;
                    for (int u = 0; u < maxUni; ++u)
                        v.ph[(size_t) o][(size_t) u] = os.retrig ? dd::wrap01 (os.phase + os.phRand * rng.nextFloat() + (u > 0 ? 0.07f * os.phRand * (float) u : 0.f))
                                                                 : rng.nextFloat();
                }
            }
            if (! wasActive) { v.ladL.reset(); v.ladR.reset(); v.notchL = {}; v.notchR = {}; v.lastCut = -1; for (auto& kk : v.key) kk[0] = -1.f; }
            v.active = true;
        }

        // mod sources of one voice (updated every 'sub' samples)
        void sources (Voice& v)
        {
            auto& x = v.src;
            x[SNone] = 0; x[SAmpEnv] = v.ae.value; x[SFltEnv] = v.fe.value; x[SModEnv] = v.me.value;
            const float dtMs = (float) sub * invSrMs;
            x[SLfo1] = v.lfo[0].tick (s.lfo[0], dtMs); x[SLfo2] = v.lfo[1].tick (s.lfo[1], dtMs);
            x[SVel] = v.vel; x[SWheel] = wheel; x[SAfter] = after; x[SKey] = ((float) v.note - 60.f) / 48.f; x[SRand] = v.rnd; x[SFortune] = s.fortune;
        }

        // control rate: modulation -> per-voice synthesis values
        void control (Voice& v)
        {
            sources (v);
            std::array<float, numDst> m {};
            for (auto& sl : s.slots) if (sl.src != SNone && sl.dst != DNone && sl.amt != 0.f) m[(size_t) sl.dst] += v.src[(size_t) sl.src] * sl.amt;

            // glide
            if (s.glideMs > 0.5f && v.pitch != v.target)
            {
                const float k = 1.f - std::exp (-(float) sub * invSrMs / juce::jmax (1.f, s.glideMs * 0.35f));
                v.pitch += (v.target - v.pitch) * k;
                if (std::abs (v.target - v.pitch) < 0.002f) v.pitch = v.target;
            }
            else v.pitch = v.target;

            const float basePitch = v.pitch + s.transpose + bend * s.bendRange + m[DPitch] * 24.f + m[DFine];
            const float wAdd = FortuneMap::widthAdd (s);
            for (int o = 0; o < numOsc; ++o)
            {
                const auto& os = s.osc[(size_t) o];
                if (! os.on) { v.nUni[(size_t) o] = 0; continue; }
                const float st = basePitch + os.semis + m[(size_t) (DO1Pitch + o)] * 24.f;
                v.hz[(size_t) o] = 440.f * std::exp2 ((st - 69.f) / 12.f);
                v.wt[(size_t) o] = juce::jlimit (0.f, 1.f, os.wt + m[DWt]);
                v.warp[(size_t) o] = juce::jlimit (0.f, 1.f, os.warp + m[DWarp]);
                const int n = juce::jlimit (1, maxUni, os.voices);
                v.nUni[(size_t) o] = n;
                const float cents = n > 1 ? juce::jlimit (0.f, 120.f, os.detune * FortuneMap::detuneMul (s, o) + FortuneMap::detuneAdd (s, o) + m[DDetune] * 50.f) : 0.f;
                const float spread = juce::jlimit (0.f, 1.f, os.uspread + m[DSpread] + (n > 1 ? wAdd : 0.f));
                const float level = juce::jlimit (0.f, 1.5f, os.level + m[(size_t) (DO1Level + o)]);
                const float norm = level / std::sqrt ((float) n);
                auto& kk = v.key[(size_t) o];
                if (kk[0] == (float) n && kk[1] == cents && kk[2] == spread && kk[3] == level && kk[4] == os.pan && kk[5] == os.umix) { v.oscSide[(size_t) o] = juce::jlimit (0.f, 2.f, os.width + wAdd * 0.5f); continue; }
                kk = { (float) n, cents, spread, level, os.pan, os.umix };
                const float pa = (juce::jlimit (-1.f, 1.f, os.pan) + 1.f) * juce::MathConstants<float>::pi * 0.25f;
                const float panL = std::cos (pa) * 1.41421f, panR = std::sin (pa) * 1.41421f;
                for (int u = 0; u < n; ++u)
                {
                    const float off = n == 1 ? 0.f : -1.f + 2.f * (float) u / (float) (n - 1);
                    const float shaped = off * (0.55f + 0.45f * std::abs (off));
                    v.ratio[(size_t) o][(size_t) u] = std::exp2 (shaped * cents / 1200.f);
                    const float side = n == 1 ? 0.f : (u % 2 == 0 ? 1.f : -1.f) * (0.85f + 0.15f * std::abs (off));
                    const float sp = juce::jlimit (-1.f, 1.f, side * spread);
                    const bool centre = n == 1 || u == (n - 1) / 2 || (n % 2 == 0 && u == n / 2);
                    const float lv = centre ? 1.f : os.umix;
                    v.gl[(size_t) o][(size_t) u] = std::sqrt (0.5f * (1.f - sp)) * panL * norm * lv;
                    v.gr[(size_t) o][(size_t) u] = std::sqrt (0.5f * (1.f + sp)) * panR * norm * lv;
                }
                v.oscSide[(size_t) o] = juce::jlimit (0.f, 2.f, os.width + wAdd * 0.5f);
            }
            // groups
            v.gA = s.gaGain * juce::jlimit (0.f, 2.f, 1.f + m[DGroupA]);
            v.gB = s.gbGain * juce::jlimit (0.f, 2.f, 1.f + m[DGroupB]);
            auto panGains = [] (float p, float& l, float& r) { const float a = (juce::jlimit (-1.f, 1.f, p) + 1.f) * 0.7853982f; l = std::cos (a) * 1.41421f; r = std::sin (a) * 1.41421f; };
            panGains (s.gaPan, v.panAL, v.panAR); panGains (s.gbPan, v.panBL, v.panBR);
            v.wA = juce::jlimit (0.f, 2.f, s.gaWidth + wAdd * 0.4f); v.wB = juce::jlimit (0.f, 2.f, s.gbWidth + wAdd);
            // filter
            const float keyOct = s.key * ((float) v.note + s.transpose - 60.f) / 12.f;
            const float oct = keyOct + s.envOct * v.fe.value + FortuneMap::cutOct (s) + m[DCutoff] * 6.f;
            v.cutHz = juce::jlimit (16.f, 20000.f, smCut * std::exp2 (oct));
            v.res = juce::jlimit (0.f, 1.f, smRes + m[DRes]);
            v.driveDb = juce::jlimit (0.f, 36.f, s.fltDriveDb + m[DFltDrive] * 24.f);
            // amp
            const float velGain = (1.f - s.velSens) + s.velSens * v.vel * v.vel;
            v.volMul = velGain * juce::jlimit (0.f, 2.f, 1.f + m[DVolume]);
            panGains (m[DPan], v.panL, v.panR);
            // filter coefficients (only when they moved)
            const int type = s.fltType;
            v.notch = type == 5;
            static const int ladderMode[] { doomsday::filter::LP12, doomsday::filter::LP24, doomsday::filter::HP12, doomsday::filter::HP24, doomsday::filter::BP12, doomsday::filter::LP24 };
            v.fltMode = ladderMode[juce::jlimit (0, 5, type)];
            if (v.notch) v.notchC = dd::svfCoefs (v.cutHz, 2.f - 1.9f * v.res, sr);
            else
            {
                v.ladL.setMode (v.fltMode); v.ladR.setMode (v.fltMode);
                if (std::abs (v.cutHz - v.lastCut) > 0.002f * v.cutHz) { const float g = doomsday::filter::prewarp (v.cutHz, sr); v.ladL.setG (g); v.ladR.setG (g); v.lastCut = v.cutHz; }
                v.ladL.setResonance (v.res * 0.98f); v.ladR.setResonance (v.res * 0.98f);
                v.ladL.setDrive (v.driveDb); v.ladR.setDrive (v.driveDb);
            }
        }

        void renderVoice (Voice& v, float* L, float* R, int n)
        {
            const float invSr = (float) (1.0 / sr);
            for (int i0 = 0; i0 < n; i0 += sub)
            {
                const int len = juce::jmin (sub, n - i0);
                control (v);
                // per-oscillator phase steps for this sub block
                for (int i = i0; i < i0 + len; ++i)
                {
                    float aL = 0, aR = 0, bL = 0, bR = 0;
                    for (int o = 0; o < numOsc; ++o)
                    {
                        const int nu = v.nUni[(size_t) o];
                        if (nu == 0) continue;
                        const int wave = s.osc[(size_t) o].wave;
                        const float hz = v.hz[(size_t) o], pos = v.wt[(size_t) o], warp = v.warp[(size_t) o];
                        float ol = 0, orr = 0;
                        auto& ph = v.ph[(size_t) o];
                        for (int u = 0; u < nu; ++u)
                        {
                            const float dt = juce::jmin (0.45f, hz * v.ratio[(size_t) o][(size_t) u] * invSr);
                            float& p = ph[(size_t) u];
                            p += dt; if (p >= 1.f) p -= 1.f;
                            const float x = Wave::get (wave, p, dt, pos, warp);
                            ol += x * v.gl[(size_t) o][(size_t) u]; orr += x * v.gr[(size_t) o][(size_t) u];
                        }
                        // oscillator stereo spread (M/S)
                        const float mid = 0.5f * (ol + orr), side = 0.5f * (ol - orr) * v.oscSide[(size_t) o];
                        ol = mid + side; orr = mid - side;
                        if (o < 2) { aL += ol; aR += orr; } else { bL += ol; bR += orr; }
                    }
                    // group mixers: volume, pan, width
                    auto group = [] (float& l, float& r, float g, float pl, float pr, float w)
                    {
                        const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * w;
                        l = (mid + side) * g * pl; r = (mid - side) * g * pr;
                    };
                    group (aL, aR, v.gA, v.panAL, v.panAR, v.wA);
                    group (bL, bR, v.gB, v.panBL, v.panBR, v.wB);
                    // filter route
                    float inL, inR, dryL = 0, dryR = 0;
                    if (s.route == 1)      { inL = aL; inR = aR; dryL = bL; dryR = bR; }
                    else if (s.route == 2) { inL = bL; inR = bR; dryL = aL; dryR = aR; }
                    else                   { inL = aL + bL; inR = aR + bR; }
                    float yL, yR;
                    if (v.notch)
                    {
                        float lp, bp;
                        v.notchL.tick (inL, v.notchC, lp, bp); yL = inL - v.notchC.k * bp;
                        v.notchR.tick (inR, v.notchC, lp, bp); yR = inR - v.notchC.k * bp;
                    }
                    else { yL = v.ladL.process (inL); yR = v.ladR.process (inR); }
                    yL += dryL; yR += dryR;
                    const float amp = v.ae.next (s.ae, invSrMs) * v.volMul * voiceGain;   // headroom: a 4-note chord ~ 0 dBFS before the limiter
                    v.fe.next (s.fe, invSrMs); v.me.next (s.me, invSrMs);
                    L[i] += yL * amp * v.panL; R[i] += yR * amp * v.panR;
                }
                if (v.ae.idle()) { v.active = false; return; }
            }
        }

        // ---------------------------------------------------- global part
        void globalModulation()
        {
            // the global destinations (SCREAM / DELAY / REVERB) follow the newest voice + two free-running LFOs
            const Voice* nv = nullptr;
            for (auto& v : voices) if (v.active && (nv == nullptr || v.age > nv->age)) nv = &v;
            std::array<float, numSrc> x {};
            if (nv != nullptr) x = nv->src;
            const float dtMs = blockMs;
            x[SLfo1] = glfo[0].tick (s.lfo[0], dtMs); x[SLfo2] = glfo[1].tick (s.lfo[1], dtMs);
            x[SWheel] = wheel; x[SAfter] = after; x[SFortune] = s.fortune;
            if (nv == nullptr) { x[SVel] = lastVel; x[SKey] = ((float) lastKey - 60.f) / 48.f; }
            gmod.fill (0.f);
            for (auto& sl : s.slots) if (sl.src != SNone && sl.dst >= DScream && sl.amt != 0.f) gmod[(size_t) sl.dst] += x[(size_t) sl.src] * sl.amt;
        }

        void post (juce::AudioBuffer<float>& b, int start, int n)
        {
            blockMs = (float) n * invSrMs;
            globalModulation();
            auto* L = b.getWritePointer (0) + start; auto* R = b.getWritePointer (1) + start;

            // SCREAM
            const float scrAmt = juce::jlimit (0.f, 1.6f, s.scr + FortuneMap::screamAdd (s, s.scr) + gmod[DScream]);
            scream.process (b, start, n, s, scrAmt);

            // EQ (100 Hz shelf, 400 Hz bell, 2.5 kHz bell, 8 kHz shelf)
            auto runEq = [&]
            {
                const bool eqActive = s.eqOn && (s.eqLow != 0.f || s.eqLowMid != 0.f || s.eqHighMid != 0.f || s.eqHigh != 0.f);
                if (eqActive)
                {
                    using BC = dd::BiquadCoefs;
                    const BC c[4] { BC::make (BC::LowShelf, sr, 100, 0.7, s.eqLow), BC::make (BC::Bell, sr, 400, 0.9, s.eqLowMid),
                                    BC::make (BC::Bell, sr, 2500, 0.9, s.eqHighMid), BC::make (BC::HighShelf, sr, 8000, 0.7, s.eqHigh) };
                    for (int i = 0; i < n; ++i)
                        for (int k = 0; k < 4; ++k) { L[i] = eq[(size_t) k][0].process (L[i], c[k]); R[i] = eq[(size_t) k][1].process (R[i], c[k]); }
                }
            };
            // DIST (ADAA tube curve, tone tilt, mix, output) - oversampled 1x / 2x / 4x (spec 26)
            auto runDist = [&]
            {
                if (! s.distOn) return;
                distOs.setFactorIndex (juce::jlimit (0, 2, s.distOs));
                const double rate = sr * distOs.ratio();
                const float g = juce::Decibels::decibelsToGain (s.distDriveDb), mk = 1.f / std::pow (g, 0.7f), out = juce::Decibels::decibelsToGain (s.distOutDb);
                const float tc = dd::onePoleCoef (1800.f, rate);
                juce::dsp::AudioBlock<float> blk (b.getArrayOfWritePointers(), 2, (size_t) start, (size_t) n);
                distOs.process (blk, [&] (int c, float x)
                {
                    const size_t ci = (size_t) c;
                    float y = distAdaa[ci].process (x * g, doomsday::drive::Tube) * mk;
                    y = distDc[ci].process (y);
                    distTone[ci] += tc * (y - distTone[ci]);
                    const float lo = distTone[ci], hi = y - lo;
                    y = lo * (1.f - 0.5f * juce::jmax (0.f, s.distTone)) + hi * (1.f + 0.8f * s.distTone);
                    return x + s.distMix * (y * out - x);
                });
            };
            // CHORUS
            auto runCho = [&]
            {
                if (s.choOn) for (int i = 0; i < n; ++i) chorus.process (L[i], R[i], s.choRate, s.choDepth, s.choWidth, s.choMix);
            };
            // DELAY
            auto runDly = [&]
            {
                const float dlyMix = juce::jlimit (0.f, 1.f, s.dlyMix + gmod[DDelayMix]);
                if (s.dlyOn && dlyMix > 0.0005f)
                {
                    doomsday::space::EchoParams ep;
                    ep.mode = s.dlyPP ? 1 : 0; ep.timeMs = (float) (s.dlyBeats * 60000.0 / juce::jmax (40.0, s.bpm));
                    ep.feedback = s.dlyFb; ep.lowCutHz = 180.f; ep.highCutHz = s.dlyFilter; ep.mod = 0.1f; ep.drive = 0.f;
                    for (int i = 0; i < n; ++i) { float wl, wr; echo.process (L[i], R[i], wl, wr, ep); L[i] += dlyMix * wl; R[i] += dlyMix * wr; }
                }
            };
            // REVERB (+ FORTUNE: mix +15, size +10 points)
            auto runRvb = [&]
            {
                const float rvbMix = juce::jlimit (0.f, 1.f, s.rvbMix + FortuneMap::rvbMixAdd (s) + gmod[DReverbMix]);
                if (s.rvbOn && rvbMix > 0.0005f)
                {
                    doomsday::space::ReverbParams rp;
                    rp.preDelayMs = s.rvbPre; rp.size = juce::jlimit (0.f, 1.f, s.rvbSize + FortuneMap::rvbSizeAdd (s)) * 1.6f + 0.2f;
                    rp.decayS = s.rvbDecay; rp.dampHz = s.rvbDamp; rp.lowCutHz = 180.f; rp.width = s.rvbWidth; rp.mod = 0.35f;
                    for (int i = 0; i < n; ++i) { float wl, wr; reverb.process (L[i], R[i], wl, wr, rp); L[i] = L[i] * (1.f - 0.35f * rvbMix) + rvbMix * wl; R[i] = R[i] * (1.f - 0.35f * rvbMix) + rvbMix * wr; }
                }
            };
            // FX ORDER (spec 20): 0 EQ DIST CHO DLY RVB, 1 DIST EQ CHO DLY RVB, 2 EQ DIST DLY RVB CHO, 3 EQ CHO DIST DLY RVB, 4 DIST EQ DLY CHO RVB
            static const int orders[5][5] { { 0, 1, 2, 3, 4 }, { 1, 0, 2, 3, 4 }, { 0, 1, 3, 4, 2 }, { 0, 2, 1, 3, 4 }, { 1, 0, 3, 2, 4 } };
            for (int k : orders[juce::jlimit (0, 4, s.fxOrder)])
                switch (k) { case 0: runEq(); break; case 1: runDist(); break; case 2: runCho(); break; case 3: runDly(); break; default: runRvb(); break; }
            // STEREO (width, balance, mono bass)
            {
                const float bl = std::sqrt (juce::jlimit (0.f, 2.f, 1.f - s.stBalance)), br = std::sqrt (juce::jlimit (0.f, 2.f, 1.f + s.stBalance));
                for (int i = 0; i < n; ++i)
                {
                    const float mid = 0.5f * (L[i] + R[i]), side = 0.5f * (L[i] - R[i]) * s.stWidth;
                    L[i] = (mid + side) * bl; R[i] = (mid - side) * br;
                    monoBass.process (L[i], R[i], s.stMonoBass, sr);
                }
            }
            // LIMITER + master
            float pk = 0;
            const float inG = juce::Decibels::decibelsToGain (s.limInDb);
            for (int i = 0; i < n; ++i)
            {
                float l = L[i], r = R[i];
                if (s.limOn) { l *= inG; r *= inG; limiter.process (l, r, s.limCeil, s.limRel); }
                l *= s.master; r *= s.master;
                if (! std::isfinite (l) || ! std::isfinite (r)) { l = r = 0.f; blewUp = true; }
                L[i] = l; R[i] = r;
                pk = juce::jmax (pk, std::abs (l), std::abs (r));
            }
            if (blewUp) { blewUp = false; for (auto& v : voices) { v.ladL.reset(); v.ladR.reset(); } scream.reset(); echo.reset(); reverb.reset(); chorus.reset(); }
            outPeak = pk;
        }

        Settings s;
        double sr = 48000; float invSrMs = 1000.f / 48000.f, blockMs = 1;
        std::array<Voice, maxPoly> voices;
        std::array<int, 32> held {}; int numHeld = 0;
        uint64_t ageCounter = 0;
        float lastPitch = -1.f, bend = 0, wheel = 0, after = 0, lastVel = 1; int lastKey = 60;
        float smCut = 1400, smRes = 0.2f;
        std::array<LfoState, 2> glfo;
        std::array<float, numDst> gmod {};
        Scream scream;
        std::array<std::array<dd::Biquad, 2>, 4> eq {};
        std::array<doomsday::drive::Adaa1, 2> distAdaa {};
        doomsday::drive::OverSampler distOs;
        std::array<doomsday::drive::DcBlock, 2> distDc {};
        std::array<float, 2> distTone {};
        Chorus chorus;
        doomsday::space::EchoDelay echo;
        doomsday::space::FdnReverb reverb;
        doomsday::space::MonoBass monoBass;
        doomsday::dyn::Limiter limiter;
        juce::Random rng { 4242 };
        float outPeak = 0; bool blewUp = false;
    public:
        // smoothing of the continuous filter knobs (called once per block before render)
        void smooth (int n)
        {
            const float k = 1.f - std::exp (-(float) n * invSrMs / 12.f);
            smCut = std::exp2 (std::log2 (smCut) + (std::log2 (juce::jmax (16.f, s.cut)) - std::log2 (smCut)) * k);
            smRes += (s.res - smRes) * k;
        }
        void snapSmoothing() { smCut = s.cut; smRes = s.res; }
    };
}
