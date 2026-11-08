#pragma once
// =====================================================================
//  4GE module 09 - MODULATION ENGINE (pure DSP: no GUI, no files, no APVTS; real-time safe)
//   ONE engine for the whole plugin (MODULE09 1): sources -> normalise -> slot (polarity, curve, depth, offset, lag, smoothing)
//   -> combine (ADD / MULTIPLY / MAX / MIN / REPLACE) -> clamp -> the FINAL value of a registered parameter -> the DSP cook.
//   Domain: the parameter's NORMALISED range (0..1 with its skew) - Hz / dB / ms come only from the parameter itself.
//   The BASE value is never written: FINAL = f (BASE, modulations) is computed into a copy of the ParamFrame.
//   Sources: 8 MASTER MACROS + 4 USER MACROS (RULES 1 / 2: a macro is only a source), LFO 1-8, STEP SEQUENCER, RANDOM 1-2,
//   VELOCITY, KEY, PITCH ENVELOPE, AMP ENVELOPES (PUNCH / BODY / TAIL / SUB / KICK), FILTER ENVELOPE.
//   Control rate = every 32 host samples (+ at every kick trigger); LFO / sequencer clocks are sample exact over the block.
// =====================================================================
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include "../../Parameters/ParamFrame.h"

namespace fge::dsp::mod
{
    enum Src : int
    {
        SNone = 0,
        SMacro1 = 1,                    // 1..8 MASTER MACROS (IMPACT .. CHAOS), 9..12 USER 1..4
        SLfo1 = 13,                     // 13..20
        SSeq = 21, SRnd1 = 22, SRnd2 = 23,
        SVel = 24, SKey = 25, SPitchEnv = 26,
        SAmpPunch = 27, SAmpBody = 28, SAmpTail = 29, SAmpSub = 30, SAmpKick = 31,
        SFiltEnv = 32,
        NumSrc = 33
    };
    inline const juce::StringArray& sourceNames()
    {
        static const juce::StringArray a { "-", "IMPACT", "BODY", "TAIL", "PITCH", "GRIT", "SCREAM", "WEIGHT", "CHAOS", "USER 1", "USER 2", "USER 3", "USER 4",
                                           "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "LFO 7", "LFO 8", "STEP SEQ", "RANDOM 1", "RANDOM 2",
                                           "VELOCITY", "KEY", "PITCH ENV", "AMP PUNCH", "AMP BODY", "AMP TAIL", "AMP SUB", "AMP KICK", "FILTER ENV" };
        return a;
    }
    inline bool isMacro (int s) { return s >= SMacro1 && s < SLfo1; }
    inline bool isMovement (int s) { return (s >= SLfo1 && s <= SRnd2); }           // MOVEMENT scales these
    enum Pol { PBipolar = 0, PUniPos, PUniNeg };
    enum Curve { CLinear = 0, CExp, CLog, CSCurve, CInverted };
    enum Mode { MAdd = 0, MMultiply, MMax, MMin, MReplace };
    inline const juce::StringArray& polNames()   { static const juce::StringArray a { "BIPOLAR", "UNIPOLAR +", "UNIPOLAR -" }; return a; }
    inline const juce::StringArray& curveNames() { static const juce::StringArray a { "LINEAR", "EXPONENTIAL", "LOGARITHMIC", "S-CURVE", "INVERTED" }; return a; }
    inline const juce::StringArray& modeNames()  { static const juce::StringArray a { "ADD", "MULTIPLY", "MAX", "MIN", "REPLACE" }; return a; }
    inline const juce::StringArray& stepCurveNames() { static const juce::StringArray a { "STEP", "LINEAR", "RAMP UP", "RAMP DOWN", "SMOOTH" }; return a; }

    static constexpr int numSlots = 64, maxSteps = 64, numLfo = 8, numRnd = 2, ctlN = 32;

    // one matrix row as the audio thread sees it (the processor resolves the destination ID -> index; unknown = dst -1)
    struct Slot
    {
        bool on = false, mute = false, solo = false;
        int src = SNone, dst = -1, pol = PUniPos, curve = CLinear, mode = MAdd;
        float depth = 0.f, offset = 0.f, smoothMs = 0.f, lagMs = 0.f;
    };
    enum StepCurve { KStep = 0, KLinear, KRampUp, KRampDown, KSmooth };
    struct Step
    {
        float value = 0.f, prob = 1.f;
        bool tie = false, accent = false, mute = false;
        int ratchet = 1, curve = KStep;
    };
    struct Data
    {
        std::array<Slot, numSlots> slot {};
        std::array<Step, maxSteps> step {};
        int active = 0;                           // number of usable slots (on, a source, a destination)
        uint32_t version = 0;
    };

    // ---- which destinations are legal: a FLOAT parameter flagged FMod, never a latency / safety-critical switch
    inline bool modulatable (int id)
    {
        if (id < 0 || id >= P::count) return false;
        const auto& d = def (id);
        if (d.kind != Kind::Float || ! d.has (FMod)) return false;
        const juce::String s (d.id);
        return ! s.contains ("lookahead");        // (the lookahead changes the host latency)
    }
    // safety-critical levels (master, limiter ceilings): a modulation may only LOWER them (MODULE09 3.2 / 9.2)
    inline bool safetyDown (int id)
    {
        static const std::array<bool, P::count> t = [] { std::array<bool, P::count> x {};
            for (int i = 0; i < P::count; ++i) { const juce::String s (def (i).id); x[(size_t) i] = def (i).has (FSafety) || (s.startsWith ("lim") && s.endsWith ("_ceiling")); } return x; }();
        return id >= 0 && id < P::count && t[(size_t) id];
    }
    // the parameter's normalised range - the same as the APVTS layout (skew for the centre)
    struct Ranges
    {
        std::array<juce::NormalisableRange<float>, P::count> r;
        Ranges()
        {
            for (int i = 0; i < P::count; ++i)
            {
                const auto& d = def (i);
                juce::NormalisableRange<float> x (d.min, d.max);
                if (d.kind == Kind::Float && d.centre > d.min && d.centre < d.max) x.setSkewForCentre (d.centre);
                r[(size_t) i] = x;
            }
        }
        static const Ranges& get() { static const Ranges g; return g; }
    };

    // which cooked section of the engine a parameter belongs to (the engine re-cooks only those when a modulation moves)
    enum Group : uint32_t { GCore = 1, GOsc = 2, GLayers = 4, GDist = 8, GM06 = 16, GTone = 32, GDyn = 64, GMod = 128, GAll = 255 };
    inline uint32_t groupOf (int id)
    {
        static const std::array<uint32_t, P::count> table = []
        {
            std::array<uint32_t, P::count> t {};
            struct Pre { const char* p; uint32_t g; };
            static const Pre pre[] { { "mb_link", GTone }, { "macro_", GMod }, { "lfo", GMod }, { "seq_", GMod }, { "rnd", GMod }, { "mod_", GMod },
                                     { "punch", GCore }, { "body", GCore }, { "tail", GCore }, { "sub", GCore }, { "mix_", GCore }, { "core", GCore },
                                     { "pe_", GCore }, { "env_", GCore }, { "master", GCore }, { "osc", GOsc }, { "fm_", GOsc },
                                     { "layer", GLayers }, { "noise", GLayers }, { "tr_", GLayers }, { "group", GLayers }, { "bus", GLayers }, { "shaper", GLayers },
                                     { "dist", GDist }, { "flt", GM06 }, { "res", GM06 }, { "scr", GM06 }, { "filter", GM06 },
                                     { "eq", GTone }, { "deq", GTone }, { "mb", GTone }, { "sp_", GTone }, { "stw", GTone }, { "tone", GTone },
                                     { "cmp", GDyn }, { "ts", GDyn }, { "clp", GDyn }, { "lim", GDyn }, { "crest", GDyn }, { "dyn_", GDyn } };
            for (int i = 0; i < P::count; ++i)
            {
                const juce::String s (def (i).id);
                uint32_t g = GAll;
                for (auto& p : pre) if (s.startsWith (p.p)) { g = p.g; break; }
                t[(size_t) i] = g;
            }
            return t;
        }();
        return id >= 0 && id < P::count ? table[(size_t) id] : GAll;
    }

    // ---- small deterministic noise (seed + counter -> -1..1); the same everywhere (realtime = export)
    inline float hashNoise (uint32_t seed, uint32_t k)
    {
        uint32_t x = seed * 0x9E3779B1u ^ (k + 0x7F4A7C15u) * 0x85EBCA77u;
        x ^= x >> 15; x *= 0x2C1B3C6Du; x ^= x >> 12; x *= 0x297A2D39u; x ^= x >> 15;
        return (float) (x & 0xFFFFFFu) / (float) 0x7FFFFF - 1.f;
    }
    inline double divisionBeats (int d)
    {
        static constexpr double b[16] { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625, 3.0, 1.5, 0.75, 0.375, 4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.0 / 12.0 };
        return b[juce::jlimit (0, 15, d)];
    }
    enum Retrig { ROff = 0, ROnNote, ROnKick, ROnBar };
    enum LfoMode { LLoop = 0, LOneHold, LOneZero };
    enum RndMode { RFixed = 0, RPerTrigger, RFreeRun, ROneShot, RControlled };

    // what the engine knows this control tick (from the newest kick and the host)
    struct Ctx
    {
        float velocity = 1.f, key = 0.f, pitchEnv = 0.f, filtEnv = 0.f;
        std::array<float, 5> amp {};             // PUNCH, BODY, TAIL, SUB, KICK
        double bpm = 150.0, ppq = 0.0;            // ppq = the host position at the start of this tick (or the engine clock)
        bool playing = false;
        int barBeats = 4;
    };

    inline float shapeCurve (float x, int c)       // x in -1..1 (sign kept)
    {
        const float a = std::abs (x), s = x < 0.f ? -1.f : 1.f;
        switch (c)
        {
            case CExp:      return s * a * a * a;
            case CLog:      return s * (1.f - (1.f - a) * (1.f - a) * (1.f - a));
            case CSCurve:   return s * a * a * (3.f - 2.f * a);
            case CInverted: return -x;
            default:        return x;
        }
    }

    // =====================================================================
    class Engine
    {
    public:
        void reset (double hostRate)
        {
            rate = hostRate;
            for (auto& l : lfo) l = {};
            seq = {}; for (auto& r : rnd) r = {};
            for (auto& s : slotSt) s = {};
            for (auto& v : srcVal) v = 0.f;
            for (int t = 0; t < prevN; ++t) acc[(size_t) prevT[(size_t) t]] = {};
            touchN = prevN = 0;
            clockPpq = 0.0; lastPlaying = false; kicks = 0; runs = 1; notesHeld = 0; anyTick = false;
            pendingKick = pendingNote = false; lastBar = -1;
        }
        // ---- events (the engine calls these at the exact sample)
        void onNoteOn (bool firstOfPhrase) { pendingKick = true; if (firstOfPhrase) pendingNote = true; ++kicks; ++notesHeld; }
        void onNoteOff() { notesHeld = juce::jmax (0, notesHeld - 1); if (notesHeld == 0) released = true; }

        // ---- one control tick: advance the sources by 'n' host samples, compute FINAL into 'out'. Returns the groups that moved.
        uint32_t tick (const Data& d, const ParamFrame& base, ParamFrame& out, const Ctx& cx, int n)
        {
            const double dtSec = (double) n / rate;
            const ParamFrame& m = lastFinal;          // the module 09 settings themselves (may be modulated - one tick late, never a cycle)
            // ---- the clock (host ppq while playing, else the engine's own beat clock at the tempo)
            const double bpm = cx.bpm > 0.0 ? cx.bpm : 150.0;
            if (cx.playing) clockPpq = cx.ppq; else clockPpq += dtSec * bpm / 60.0;
            if (cx.playing && ! lastPlaying) ++runs;
            lastPlaying = cx.playing;
            const int bar = (int) std::floor (clockPpq / juce::jmax (1, cx.barBeats));
            const bool barEdge = lastBar >= 0 && bar != lastBar;
            lastBar = bar;
            const bool evKick = pendingKick, evNote = pendingNote, evRel = released;
            pendingKick = pendingNote = released = false;
            auto fires = [&] (int r) { return (r == ROnNote && evNote) || (r == ROnKick && evKick) || (r == ROnBar && barEdge); };
            const float movement = m[P::modMovement];

            // ---- LFOs
            for (int i = 0; i < numLfo; ++i)
            {
                const int o = i * (P::lfo2Wave - P::lfo1Wave);
                auto& L = lfo[(size_t) i];
                if (! used[(size_t) (SLfo1 + i)]) { srcVal[(size_t) (SLfo1 + i)] = 0.f; continue; }
                const int wave = m.idx (P::lfo1Wave + o), retr = m.idx (P::lfo1Retrig + o), mode = m.idx (P::lfo1Mode + o);
                const bool sync = m.on (P::lfo1Sync + o);
                const double hz = sync ? bpm / 60.0 / divisionBeats (m.idx (P::lfo1Div + o)) : (double) m[P::lfo1Rate + o];
                const bool locked = sync && retr == ROff && cx.playing;     // HOST SYNC, free: the phase follows the song position
                // the time since the last tick passed BEFORE this tick's events: advance first, then retrigger
                if (! locked)
                {
                    L.phase += hz * dtSec;
                    if (L.phase >= 1.0) { const auto w = (int64_t) std::floor (L.phase); L.cycles += w; L.phase -= (double) w; if (mode != LLoop) L.done = true; }
                }
                L.sinceMs += (float) (dtSec * 1000.0);
                if (L.relMs >= 0.f) L.relMs += (float) (dtSec * 1000.0);
                if (fires (retr)) { L.phase = 0.0; L.cycles = 0; L.sinceMs = 0.f; L.done = false; L.relMs = -1.f; }
                if (evRel) L.relMs = 0.f;
                double ph;
                if (locked) { const double x = clockPpq / divisionBeats (m.idx (P::lfo1Div + o)); L.cycles = (int64_t) std::floor (x); ph = x - std::floor (x); }
                else ph = L.phase;
                const double p = std::fmod (ph + m[P::lfo1Phase + o] / 360.0, 1.0);
                const float pw = m[P::lfo1Pw + o];
                const uint32_t seed = 0xA5u + (uint32_t) i * 977u;
                float v;
                switch (wave)
                {
                    case 1:  v = (float) (p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p); break;
                    case 2:  v = (float) (2.0 * p - 1.0); break;
                    case 3:  v = (float) (1.0 - 2.0 * p); break;
                    case 4:  v = p < pw ? 1.f : -1.f; break;
                    case 5:  v = p < pw * 0.25 ? 1.f : -1.f; break;
                    case 6:  v = hashNoise (seed, (uint32_t) L.cycles); break;
                    case 7:  { const float a = hashNoise (seed, (uint32_t) L.cycles), b = hashNoise (seed, (uint32_t) L.cycles + 1u);
                               const float u = 0.5f - 0.5f * std::cos ((float) p * juce::MathConstants<float>::pi); v = a + (b - a) * u; break; }
                    default: v = (float) std::sin (p * juce::MathConstants<double>::twoPi); break;
                }
                if (L.done) v = mode == LOneZero ? 0.f : L.endV; else L.endV = v;
                if (m.idx (P::lfo1Pol + o) == 1) v = 0.5f * (v + 1.f);
                // fade in after a retrigger, fade out after the note is released
                const float fi = m[P::lfo1FadeIn + o], fo = m[P::lfo1FadeOut + o];
                float g = fi > 0.5f ? juce::jlimit (0.f, 1.f, L.sinceMs / fi) : 1.f;
                if (L.relMs >= 0.f && fo > 0.5f) g *= juce::jlimit (0.f, 1.f, 1.f - L.relMs / fo);
                v *= g;
                // a wave change crossfades (5 ms) - never a jump
                if (wave != L.wave) { if (L.wave >= 0) { L.xfade = 1.f; L.from = L.out; } L.wave = wave; }
                if (L.xfade > 0.f) { v = L.from + (v - L.from) * (1.f - L.xfade); L.xfade = juce::jmax (0.f, L.xfade - (float) (dtSec * 1000.0 / 5.0)); }
                L.out = smooth (L.out, v, m[P::lfo1Smooth + o], dtSec, ! anyTick);
                srcVal[(size_t) (SLfo1 + i)] = L.out;
            }
            // ---- STEP SEQUENCER
            if (used[SSeq]) srcVal[SSeq] = seqTick (d, m, fires (m.idx (P::seqReset)), cx, dtSec, bpm);
            // ---- RANDOM 1-2
            for (int i = 0; i < numRnd; ++i)
            {
                const int o = i * (P::rnd2Mode - P::rnd1Mode);
                if (! used[(size_t) (SRnd1 + i)]) { srcVal[(size_t) (SRnd1 + i)] = 0.f; continue; }
                auto& R = rnd[(size_t) i];
                const uint32_t seed = (uint32_t) std::lround (m[P::rnd1Seed + o]) * 2654435761u + (uint32_t) i;
                float target = R.target;
                switch (m.idx (P::rnd1Mode + o))
                {
                    case RFixed:      target = hashNoise (seed, 0u); break;
                    case RPerTrigger: if (evKick || ! R.started) target = hashNoise (seed, (uint32_t) kicks); break;
                    case RFreeRun:    { R.clock += m[P::rnd1Rate + o] * dtSec; target = hashNoise (seed, (uint32_t) std::floor (R.clock)); break; }
                    case ROneShot:    target = hashNoise (seed, 1000u + (uint32_t) runs); break;
                    case RControlled: if (evKick) target = juce::jlimit (-1.f, 1.f, R.target + m[P::rnd1Dev + o] * hashNoise (seed, (uint32_t) kicks)); break;
                    default: break;
                }
                R.started = true; R.target = target;
                float v = target * m[P::rnd1Amount + o];
                if (m.idx (P::rnd1Pol + o) == 1) v = 0.5f * (v + 1.f);
                R.out = smooth (R.out, v, m[P::rnd1Smooth + o], dtSec, evKick && m[P::rnd1Smooth + o] < 0.5f);
                srcVal[(size_t) (SRnd1 + i)] = R.out;
            }
            // ---- the rest
            for (int i = 0; i < 12; ++i) srcVal[(size_t) (SMacro1 + i)] = juce::jlimit (0.f, 1.f, base[P::macro_impact + i]);
            srcVal[SVel] = cx.velocity; srcVal[SKey] = cx.key; srcVal[SPitchEnv] = cx.pitchEnv; srcVal[SFiltEnv] = cx.filtEnv;
            for (int i = 0; i < 5; ++i) srcVal[(size_t) (SAmpPunch + i)] = cx.amp[(size_t) i];

            // ---- the matrix
            out = base;
            for (int t = 0; t < prevN; ++t) acc[(size_t) prevT[(size_t) t]] = {};
            bool anySolo = false;
            for (int k = 0; k < d.active; ++k) anySolo |= d.slot[(size_t) k].solo;
            touchN = 0;
            const auto& R = Ranges::get();
            for (int k = 0; k < d.active; ++k)
            {
                const auto& s = d.slot[(size_t) k];
                auto& st = slotSt[(size_t) k];
                if (! s.on || s.dst < 0 || s.src <= SNone || s.src >= NumSrc || s.mute || (anySolo && ! s.solo)) { st = {}; continue; }
                // POLARITY: BIPOLAR = the source's own range (an LFO set to UNIPOLAR stays 0..1), UNIPOLAR + / - = folded into 0..1 / -1..0
                float x = srcVal[(size_t) s.src];
                if (s.pol != PBipolar && srcBipolar (s.src, m)) x = 0.5f * (x + 1.f);
                if (s.pol == PUniNeg) x = -x;
                x = shapeCurve (juce::jlimit (-1.f, 1.f, x), s.curve);
                float v = s.offset + s.depth * x;
                if (isMovement (s.src)) v *= movement;
                if (s.lagMs > 0.5f && st.init) { const float mx = (float) (dtSec * 1000.0 / s.lagMs); v = st.lag + juce::jlimit (-mx, mx, v - st.lag); }
                st.lag = v;
                st.v = smooth (st.v, v, s.smoothMs, dtSec, ! st.init);
                st.init = true;
                auto& a = acc[(size_t) s.dst];
                if (! a.any) { a = {}; a.any = true; touchT[(size_t) touchN++] = s.dst; }
                switch (s.mode)
                {
                    case MMultiply: a.mul *= juce::jmax (0.f, 1.f + st.v); break;
                    case MMax:      a.hasMax = true; a.mx = juce::jmax (a.mx, st.v); break;
                    case MMin:      a.hasMin = true; a.mn = juce::jmin (a.mn, st.v); break;
                    case MReplace:  a.hasRep = true; a.rep = st.v; break;
                    default:        a.add += st.v; break;
                }
            }
            uint32_t moved = 0;
            for (int t = 0; t < touchN; ++t)
            {
                const int id = touchT[(size_t) t];
                const auto& a = acc[(size_t) id];
                const auto& r = R.r[(size_t) id];
                const float b = r.convertTo0to1 (juce::jlimit (r.start, r.end, base[id]));
                float y = (b + a.add) * a.mul;
                if (a.hasMax) y = juce::jmax (y, b + a.mx);
                if (a.hasMin) y = juce::jmin (y, b + a.mn);
                if (a.hasRep) y = a.rep;
                y = juce::jlimit (0.f, 1.f, std::isfinite (y) ? y : b);
                if (safetyDown (id)) y = juce::jmin (y, b);                          // a safety level can only go DOWN
                const float v = y == b ? base[id] : r.convertFrom0to1 (y);            // (no modulation = the base value, bit-exact)
                out[id] = v;
                if (v != lastFinal[id] || ! anyTick) moved |= groupOf (id);
                final[(size_t) id] = y;
            }
            // destinations no longer modulated return to their base value (and must be re-cooked)
            for (int t = 0; t < prevN; ++t) if (! acc[(size_t) prevT[(size_t) t]].any) moved |= groupOf (prevT[(size_t) t]);
            prevT = touchT; prevN = touchN;
            lastFinal = out;
            anyTick = true;
            return moved;
        }
        // which sources the matrix uses (inactive sources are not computed); call when the data changes
        void scan (const Data& d)
        {
            used.fill (false);
            for (int k = 0; k < d.active; ++k) if (d.slot[(size_t) k].on) used[(size_t) juce::jlimit (0, NumSrc - 1, d.slot[(size_t) k].src)] = true;
        }
        float source (int s) const { return s >= 0 && s < NumSrc ? srcVal[(size_t) s] : 0.f; }
        float finalNorm (int id) const { return id >= 0 && id < P::count ? final[(size_t) id] : 0.f; }
        int seqStep() const { return seq.lastStep; }
        bool srcBipolar (int s, const ParamFrame& m) const
        {
            if (s >= SLfo1 && s < SLfo1 + numLfo) return m.idx (P::lfo1Pol + (s - SLfo1) * (P::lfo2Wave - P::lfo1Wave)) == 0;
            if (s == SSeq) return m.idx (P::seqPol) == 0;
            if (s == SRnd1 || s == SRnd2) return m.idx (P::rnd1Pol + (s - SRnd1) * (P::rnd2Mode - P::rnd1Mode)) == 0;
            return s == SKey || s == SFiltEnv;
        }
        void setMatrixFrame (const ParamFrame& f) { lastFinal = f; }     // (before the first tick: the module 09 settings)

    private:
        static float smooth (float y, float x, float ms, double dtSec, bool jump)
        {
            if (jump || ms < 0.5f) return x;
            const float k = 1.f - std::exp ((float) (-dtSec * 1000.0 / ms));
            return y + k * (x - y);
        }
        float seqTick (const Data& d, const ParamFrame& m, bool reset, const Ctx& cx, double dtSec, double bpm)
        {
            auto& S = seq;
            const int len = juce::jlimit (1, maxSteps, (int) std::lround (m[P::seqLength]));
            const bool sync = m.on (P::seqSync);
            const double stepBeats = divisionBeats (m.idx (P::seqDiv));
            const bool locked = sync && m.idx (P::seqReset) == ROff && cx.playing;
            if (sync) S.pos += dtSec * bpm / 60.0 / stepBeats; else S.pos += dtSec * (double) m[P::seqRate];
            if (reset) { S.pos = 0.0; S.anchor = clockPpq; }                         // (after the time that passed before the event)
            const double steps = locked ? clockPpq / stepBeats : S.pos;
            // swing: every second step starts later; humanize: a small fixed (seeded) shift per step
            int64_t k = (int64_t) std::floor (steps);
            double u = steps - (double) k;
            const double sw = m[P::seqSwing] * 0.5, hu = m[P::seqHuman] * 0.15 * hashNoise ((uint32_t) m[P::seqSeed] + 77u, (uint32_t) k);
            const double startAt = ((k & 1) ? sw : 0.0) + hu;
            if (u < startAt) { --k; u = u + 1.0 - (((k & 1) ? sw : 0.0)); }
            else u = (u - startAt) / juce::jmax (0.05, 1.0 - startAt);
            u = juce::jlimit (0.0, 1.0, u);
            auto stepAt = [&] (int64_t c) -> int
            {
                const int64_t L = len;
                switch (m.idx (P::seqDir))
                {
                    case 1:  return (int) (L - 1 - ((c % L) + L) % L);
                    case 2:  { if (L == 1) return 0; const int64_t per = 2 * L - 2, q = ((c % per) + per) % per; return (int) (q < L ? q : per - q); }
                    case 3:  return (int) ((uint32_t) (0.5f * (hashNoise ((uint32_t) m[P::seqSeed], (uint32_t) c) + 1.f) * (float) L) % (uint32_t) L);
                    case 4:  { const int64_t q = ((c % L) + L) % L, half = (L + 1) / 2; return (int) (q < half ? 2 * q : 2 * (q - half) + 1); }
                    default: return (int) (((c % L) + L) % L);
                }
            };
            auto plays = [&] (int64_t c, int idx) { const auto& st = d.step[(size_t) idx]; return st.prob >= 0.999f || 0.5f * (hashNoise ((uint32_t) m[P::seqSeed] + 31u, (uint32_t) c) + 1.f) < st.prob; };
            auto valueOf = [&] (int idx) { const auto& st = d.step[(size_t) idx]; if (st.mute) return 0.f;
                                           float v = st.value; if (st.accent) v += (v < 0.f ? -1.f : 1.f) * m[P::seqAccent]; return juce::jlimit (-1.f, 1.f, v); };
            const int idx = stepAt (k);
            float v;
            if (! plays (k, idx))
            {
                switch (m.idx (P::seqSkip))
                {
                    case 1:  v = S.held; break;                                    // HOLD the last played value
                    case 2:  { int64_t c = k + 1; int j = stepAt (c); for (int t = 0; t < len && ! plays (c, j); ++t) j = stepAt (++c); v = valueOf (j); break; }   // SKIP to the next playing step
                    default: v = 0.f; break;
                }
            }
            else
            {
                const auto& st = d.step[(size_t) idx];
                const float a = valueOf (idx), b = valueOf (stepAt (k + 1));
                const int rat = juce::jlimit (1, 8, st.ratchet);
                const double uu = st.tie ? u : u * rat - std::floor (u * rat);
                switch (st.tie ? KLinear : st.curve)
                {
                    case KLinear:   v = a + (b - a) * (float) u; break;
                    case KRampUp:   v = a * (float) uu; break;
                    case KRampDown: v = a * (float) (1.0 - uu); break;
                    case KSmooth:   v = a + (b - a) * (float) (0.5 - 0.5 * std::cos (u * juce::MathConstants<double>::pi)); break;
                    default:        v = (rat > 1 && uu > 0.5) ? 0.f : a; break;        // STEP (a ratchet = repeated hits with gaps)
                }
                S.held = a;
            }
            S.lastStep = idx;
            if (m.idx (P::seqPol) == 1) v = 0.5f * (v + 1.f);
            S.out = smooth (S.out, v, m[P::seqSmooth], dtSec, ! anyTick);
            return S.out;
        }

        struct LfoSt { double phase = 0.0; int64_t cycles = 0; float sinceMs = 1.0e6f, relMs = -1.f, out = 0.f, endV = 0.f, from = 0.f, xfade = 0.f; int wave = -1; bool done = false; };
        struct SeqSt { double pos = 0.0, anchor = 0.0; float out = 0.f, held = 0.f; int lastStep = 0; };
        struct RndSt { float target = 0.f, out = 0.f; double clock = 0.0; bool started = false; };
        struct SlotSt { float v = 0.f, lag = 0.f; bool init = false; };
        struct Acc { bool any = false, hasMax = false, hasMin = false, hasRep = false; float add = 0.f, mul = 1.f, mx = -10.f, mn = 10.f, rep = 0.f; };
        double rate = 44100.0, clockPpq = 0.0;
        bool lastPlaying = false, pendingKick = false, pendingNote = false, released = false, anyTick = false;
        int lastBar = -1, notesHeld = 0;
        uint32_t kicks = 0, runs = 1;
        std::array<LfoSt, numLfo> lfo {};
        SeqSt seq;
        std::array<RndSt, numRnd> rnd {};
        std::array<SlotSt, numSlots> slotSt {};
        std::array<float, NumSrc> srcVal {};
        std::array<bool, NumSrc> used {};
        std::array<Acc, P::count> acc {};
        std::array<float, P::count> final {};
        std::array<int, numSlots> touchT {}, prevT {};
        int touchN = 0, prevN = 0;
        ParamFrame lastFinal = ParamFrame::defaults();
    };
}
