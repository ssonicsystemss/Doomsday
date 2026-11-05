#pragma once
// =====================================================================
//  4GE ENGINE
//   module 01: MIDI NOTE ON -> KickVoice (PUNCH / BODY / TAIL / SUB generators, 4x oversampled)
//              -> CORE MIXER (level / pan / mute / solo / phase invert / peak meter per part) -> MASTER -> out
//   module 02: every part has its own PITCH envelope (analytic modes or the drawn CUSTOM curve + the LINK system) and
//              envelope slots AMP / FILTER / DRIVE / RESONANCE (ADSR or DRAWN, delay, curve, amount, polarity,
//              ONE SHOT / RETRIGGER / LEGATO, tempo SYNC), global TIME SCALE / OFFSET (Env/KickEnvelopes.h)
//  Voices: CHOKE (a new kick fades the previous one in 6 ms) or POLY (they overlap), 8 at most (the oldest is reused).
//  Real-time safe: the oversampler and every buffer are made in the constructor / prepare(); process() never allocates.
//  Pure DSP: knows no GUI, no files, no APVTS - it gets a ParamFrame + the envelope bank per block.
// =====================================================================
#include <juce_dsp/juce_dsp.h>
#include "Core/Generators.h"
#include "Env/KickEnvelopes.h"
#include "Osc/OscTables.h"
#include "Layers/LayerCook.h"
#include "Dist/DistCook.h"
#include "Tone/ToneCook.h"
#include "Filter/M06Cook.h"
#include "DoomsdayKick.h"
#include "../Parameters/ParamFrame.h"
#include <array>
#include <atomic>

namespace fge
{
    // everything of one kick that comes from the parameters (per block)
    struct KickConfig
    {
        dsp::KickSettings ks;
        std::array<dsp::PitchPart, 4> pitch;
        std::array<dsp::EnvSlot, 4> amp;          // PUNCH / BODY / TAIL / SUB
        std::array<dsp::EnvSlot, 4> filter;       // PUNCH / BODY / TAIL (SUB unused)
        std::array<dsp::EnvSlot, 4> drive;        // all four
        std::array<dsp::EnvSlot, 4> reso;         // BODY / TAIL
        std::array<dsp::ModNow, 4> modBase;       // tone / drive without envelopes
        std::array<float, 4> toneHz {}, resoBase {}, driveBase {}, driveMaxDb {};
        std::array<float, 4> partEnd {};          // ms after which the part is silent
        float endHz = 49.f, fineMul = 1.f, subHz = 49.f;
        bool keyTrack = true;
        bool wide = false;                        // TAIL UNISON + WIDTH: the left / right tail chains run too
        float width = 0.f;
        dsp::SvfCoefs widthSplit;                 // LR4 split: lows from the centre, mids / highs from left / right
        float clipG = 1.f, clipSoft = 0.5f;       // MASTER CLIP (the sub goes around it)
        float holdMs = 0.f, holdGain = 1.f;
        dsp::OscBankSettings osc;                 // module 03: the oscillators routed into the parts
        dsp::LayerBankSettings lb;                // module 04: noise A / B, transient, layers
        dsp::dist::DistBank dist;                 // module 05: the distortion stages and their order per location
        dsp::m06::Bank m06;                       // module 06: the filters / resonators and their order per place
        dsp::tone::Bank tone;                     // module 07: EQ / dynamic EQ / multiband / spectral / stereo per place
        dsp::scr::Settings scr;                   // module 06: the screech
        bool lbAny = false;
        float lbEndMs = 0.f;                      // when the last layer / noise / click ends       // TOK LEVEL: tail gain while the pitch HOLDs (after the distortion, so the drop hits harder)
    };

    // ------------------------------------------------------------------ pitch: semitones of one part at a time (links resolved)
    struct PitchEval
    {
        const KickConfig* c = nullptr;
        std::array<int, 8> cur {};                // segment cursors (own curve, followed curve)
        float own (int part, float ms, int curIdx)
        {
            const auto& p = c->pitch[(size_t) part];
            if (! p.on) return 0.f;
            const auto& shapeOf = p.link == dsp::Linked ? c->pitch[(size_t) p.src] : p;      // LINKED: the source's shape
            const float x = juce::jmax (0.f, ms - p.offsetMs) / juce::jmax (0.5f, p.timeMs);
            float e;
            if (shapeOf.mode == dsp::customMode && shapeOf.custom != nullptr) e = shapeOf.custom->eval (juce::jmin (1.f, x), cur[(size_t) curIdx]);
            else e = dsp::pitchShapeX (shapeOf.mode, x, shapeOf.curve);
            return p.depthSt * e;
        }
        float semis (int part, float ms, int depth = 0)
        {
            const auto& p = c->pitch[(size_t) part];
            if (p.link == dsp::Follow && depth < 4)
                return p.fDepth * semis (p.src, ms / juce::jmax (0.1f, p.fTime), depth + 1) + p.fOffsetSt;
            return own (part, ms, juce::jmin (7, part + depth * 4));
        }
    };

    // ------------------------------------------------------------------ one kick
    class KickVoice
    {
    public:
        void trigger (int note, float velocityGain, const KickConfig& cfg, const float* phases01, uint64_t order, float legatoMs,
                      double absTimeSec = 0.0, uint32_t hitCount = 0)
        {
            using namespace dsp;
            active = true; t = 0.0; age = order; vel = velocityGain; fade = 1.f; fadeStep = 0.f; held = true; ctl = 0; relAtMs = -1.f;
            noteHz = 440.f * std::exp2 ((float) (note - 69) / 12.f);
            const uint32_t seed = 0x9e3779b9u * (uint32_t) (note + 1);
            punch.start (cfg.ks.part[PPunch], phases01[PPunch], seed ^ 0x1234u);
            body.start  (cfg.ks.part[PBody],  phases01[PBody],  seed ^ 0x5678u);
            tail.start  (cfg.ks.part[PTail],  phases01[PTail],  seed ^ 0x9abcu, 0);
            tailL.start (cfg.ks.part[PTail],  phases01[PTail],  seed ^ 0x9abcu, 1);
            tailR.start (cfg.ks.part[PTail],  phases01[PTail],  seed ^ 0x9abcu, 2);
            sub.start   (cfg.ks.part[PSub],   phases01[PSub],   seed);
            for (auto* set : { &stAmp, &stFlt, &stDrv, &stRes })
                for (auto& s : *set) s = {};
            auto legato = [legatoMs] (const std::array<EnvSlot, 4>& slots, std::array<EnvRunState, 4>& st)
            {
                for (size_t i = 0; i < 4; ++i) if (slots[i].trigger == Legato) st[i].clockOffsetMs = legatoMs;
            };
            legato (cfg.amp, stAmp); legato (cfg.filter, stFlt); legato (cfg.drive, stDrv); legato (cfg.reso, stRes);
            pe = {};
            mod = cfg.modBase;
            if (cfg.lbAny)
            {
                pe.c = &cfg;
                const float endEff = (cfg.keyTrack ? noteHz : cfg.endHz) * cfg.fineMul;
                const float subBase = (cfg.keyTrack ? noteHz : cfg.subHz) * cfg.fineMul;
                const float fTail = endEff * std::exp2 (pe.semis (dsp::PTail, 0.f) / 12.f);
                const std::array<float, 4> hz { fTail * std::exp2 (pe.semis (dsp::PPunch, 0.f) / 12.f), fTail * std::exp2 (pe.semis (dsp::PBody, 0.f) / 12.f),
                                                fTail, subBase * std::exp2 (pe.semis (dsp::PSub, 0.f) / 12.f) };
                const std::array<float, 4> starts { cfg.amp[dsp::PPunch].delayMs, cfg.amp[dsp::PBody].delayMs, cfg.amp[dsp::PTail].delayMs + cfg.holdMs, cfg.amp[dsp::PSub].delayMs };
                lbv.start (cfg.lb, cfg.osc, hitCount, (uint64_t) (absTimeSec * cfg.ks.rate), hz, starts);
                pe = {};
            }
            if (cfg.osc.any)
            {
                pe.c = &cfg;
                const float endEff = (cfg.keyTrack ? noteHz : cfg.endHz) * cfg.fineMul;
                const float subBase = (cfg.keyTrack ? noteHz : cfg.subHz) * cfg.fineMul;
                const float fTail = endEff * std::exp2 (pe.semis (dsp::PTail, 0.f) / 12.f);
                const std::array<float, 4> hz { fTail * std::exp2 (pe.semis (dsp::PPunch, 0.f) / 12.f), fTail * std::exp2 (pe.semis (dsp::PBody, 0.f) / 12.f),
                                                fTail, subBase * std::exp2 (pe.semis (dsp::PSub, 0.f) / 12.f) };
                oscs.start (cfg.osc, hz, absTimeSec, hitCount);
                pe = {};
            }
        }
        void noteOff (const KickConfig& cfg)
        {
            if (! active || ! held) return;
            held = false;
            const float ms = (float) (t * 1000.0);
            auto rel = [ms] (const std::array<dsp::EnvSlot, 4>& slots, std::array<dsp::EnvRunState, 4>& st)
            {
                for (size_t i = 0; i < 4; ++i)
                    if (slots[i].trigger == dsp::Retrigger && ! st[i].released) { st[i].released = true; st[i].relAtMs = ms; st[i].relFrom = st[i].last; }
            };
            rel (cfg.amp, stAmp); rel (cfg.filter, stFlt); rel (cfg.drive, stDrv); rel (cfg.reso, stRes);
            relAtMs = ms;
        }
        void choke (float ms, double rate) { if (active) fadeStep = 1.f / juce::jmax (1.f, ms * 0.001f * (float) rate); }
        bool isActive() const { return active; }
        bool isHeld() const { return active && held; }
        float elapsedMs() const { return (float) (t * 1000.0); }
        uint64_t order() const { return age; }
        void phases (float* out) const { out[0] = punch.phase(); out[1] = body.phase(); out[2] = tail.phase(); out[3] = sub.phase(); }

        // adds this voice into the four part buses (oversampled rate)
        const dsp::OscTables* tables = nullptr;
        const float* scrInL = nullptr; const float* scrInR = nullptr;          // the screech into this voice's tail (the newest voice only)
        void render (const KickConfig& cfg, float* const* bus, int n)
        {
            using namespace dsp;
            if (! active) return;
            const auto& ks = cfg.ks;
            pe.c = &cfg;
            const float endEff = (cfg.keyTrack ? noteHz : cfg.endHz) * cfg.fineMul;
            const float subBase = (cfg.keyTrack ? noteHz : cfg.subHz) * cfg.fineMul;
            float total = 0.f;
            for (auto e : cfg.partEnd) total = juce::jmax (total, e);
            if (cfg.lbAny) total = juce::jmax (total, cfg.lbEndMs);
            total = juce::jmin (total, ks.lengthMs);
            const float fadeLen = juce::jmin (15.f, ks.lengthMs * 0.1f);
            const double dt = 1.0 / ks.rate;
            const float nyq = (float) ks.rate * 0.2f;
            for (int i = 0; i < n; ++i)
            {
                const float ms = (float) (t * 1000.0);
                if (ms > total + 0.5f) { active = false; break; }
                // ---- control rate (every 16 samples): the filter / drive / resonance envelopes move tone and drive
                if ((ctl++ & 15) == 0) updateMods (cfg, ms);
                // ---- pitch of every part
                const float fTail = juce::jlimit (5.f, nyq, endEff * std::exp2 (pe.semis (PTail, ms) / 12.f));
                const float gate = juce::jlimit (0.f, 1.f, (ks.lengthMs - ms) / juce::jmax (0.5f, fadeLen)) * vel * fade;
                // ---- module 03: the oscillators, pitched by the part they follow, feed the part inputs
                std::array<float, 4> ext {};
                float extL = 0.f, extR = 0.f;
                if (scrInL != nullptr)                                            // module 06: the screech INSIDE the tail (before its drive)
                {
                    const float l = scrInL[i], r = scrInR[i];
                    ext[PTail] += 0.5f * (l + r); extL += l; extR += r;
                }
                if (cfg.osc.any)
                {
                    const std::array<float, 4> hz { juce::jlimit (5.f, nyq, fTail * std::exp2 (pe.semis (PPunch, ms) / 12.f)),
                                                    juce::jlimit (5.f, nyq, fTail * std::exp2 (pe.semis (PBody, ms) / 12.f)), fTail,
                                                    juce::jlimit (5.f, nyq, subBase * std::exp2 (pe.semis (PSub, ms) / 12.f)) };
                    oscs.tick (cfg.osc, tables != nullptr ? *tables : noTables, hz, ms, ks.inv, ext, extL, extR);
                }
                // ---- the amplitude envelopes of the parts (once per sample: the generators and the noise envelopes use them)
                std::array<float, 4> amp {};
                for (int p = 0; p < 4; ++p) amp[(size_t) p] = ms < cfg.partEnd[(size_t) p] ? ampOf (cfg, p, ms) : 0.f;
                // ---- module 04: noise into the part inputs, transient / layers into the 7 buses
                if (cfg.lbAny)
                {
                    const std::array<float, 4> hz { juce::jlimit (5.f, nyq, fTail * std::exp2 (pe.semis (PPunch, ms) / 12.f)),
                                                    juce::jlimit (5.f, nyq, fTail * std::exp2 (pe.semis (PBody, ms) / 12.f)), fTail,
                                                    juce::jlimit (5.f, nyq, subBase * std::exp2 (pe.semis (PSub, ms) / 12.f)) };
                    float bl[dsp::NumBus] {}, br[dsp::NumBus] {};
                    dsp::dist::ModCtx mc;
                    mc.sinceHitMs = ms; mc.velocity = vel;
                    const float depth = cfg.pitch[PTail].depthSt;
                    mc.pitchNorm = std::abs (depth) > 0.01f ? juce::jlimit (0.f, 1.f, pe.semis (PTail, ms) / depth) : 0.f;
                    dsp::m06::Ctx m6c; m6c.ms = ms; m6c.vel = vel; m6c.noteHz = noteHz; m6c.lengthMs = ks.lengthMs; m6c.relMs = relAtMs;
                    lbv.tick (cfg.lb, cfg.osc, tables != nullptr ? *tables : noTables, hz, amp, noteHz / 49.f, ms, ks.inv, (float) ks.rate,
                              ext, extL, extR, bl, br, &cfg.dist, &mc, cfg.m06.any ? &cfg.m06 : nullptr, &m6c, cfg.tone.any ? &cfg.tone : nullptr);
                    for (int k = 0; k < dsp::NumBus; ++k) { bus[6 + 2 * k][i] += bl[k] * gate; bus[7 + 2 * k][i] += br[k] * gate; }
                }
                if (ms < cfg.partEnd[PPunch])
                {
                    const float f = juce::jlimit (5.f, nyq, fTail * std::exp2 (pe.semis (PPunch, ms) / 12.f));
                    bus[PPunch][i] += punch.tick (ks.part[PPunch], ks, ms, f, amp[PPunch], mod[PPunch], ext[PPunch]) * gate;
                }
                if (ms < cfg.partEnd[PBody])
                {
                    const float f = juce::jlimit (5.f, nyq, fTail * std::exp2 (pe.semis (PBody, ms) / 12.f));
                    bus[PBody][i] += body.tick (ks.part[PBody], ks, f, amp[PBody], mod[PBody], ext[PBody]) * gate;
                }
                if (ms < cfg.partEnd[PTail])
                {
                    float a = amp[PTail] * gate;
                    if (cfg.holdMs > 0.f && cfg.holdGain != 1.f)                       // TOK LEVEL, 3 ms into the drop
                        a *= cfg.holdGain + (1.f - cfg.holdGain) * juce::jlimit (0.f, 1.f, (ms - cfg.holdMs) / 3.f);
                    bus[PTail][i] += tail.tick (ks.part[PTail], ks, fTail, 1.f, mod[PTail], ext[PTail]) * a;
                    if (cfg.wide)
                    {
                        bus[4][i] += tailL.tick (ks.part[PTail], ks, fTail, 1.f, mod[PTail], extL) * a;
                        bus[5][i] += tailR.tick (ks.part[PTail], ks, fTail, 1.f, mod[PTail], extR) * a;
                    }
                }
                if (ms < cfg.partEnd[PSub])
                {
                    const float f = juce::jlimit (5.f, nyq, subBase * std::exp2 (pe.semis (PSub, ms) / 12.f));
                    bus[PSub][i] += sub.tick (ks.part[PSub], ks, f, amp[PSub], mod[PSub], ext[PSub]) * gate;
                }
                if (fadeStep > 0.f) { fade -= fadeStep; if (fade <= 0.f) { fade = 0.f; active = false; break; } }
                t += dt;
            }
        }
    private:
        inline float ampOf (const KickConfig& cfg, int p, float ms)
        {
            const auto& s = cfg.amp[(size_t) p];
            const float v = dsp::slotValue (s, stAmp[(size_t) p], ms);
            if (s.amount >= 0.999f) return v;
            return (1.f - s.amount) * (ms < s.adsr.lengthMs + s.delayMs ? 1.f : 0.f) + s.amount * v;
        }
        void updateMods (const KickConfig& cfg, float ms)
        {
            using namespace dsp;
            for (int p = 0; p < 4; ++p)
            {
                const auto& fs = cfg.filter[(size_t) p];
                const auto& ds = cfg.drive[(size_t) p];
                const auto& rs = cfg.reso[(size_t) p];
                if (! fs.on && ! ds.on && ! rs.on) { mod[(size_t) p] = cfg.modBase[(size_t) p]; continue; }
                auto m = cfg.modBase[(size_t) p];
                if (fs.on || rs.on)
                {
                    float hz = cfg.toneHz[(size_t) p], reso = cfg.resoBase[(size_t) p];
                    if (fs.on) hz = juce::jlimit (20.f, 20000.f, hz * std::exp2 (fs.amount * 6.f * slotValue (fs, stFlt[(size_t) p], ms)));
                    if (rs.on) reso = juce::jlimit (0.f, 1.f, reso + rs.amount * slotValue (rs, stRes[(size_t) p], ms));
                    m.tone = svfCoefs (hz, p == PPunch ? 1.414f : 1.f / (0.7f + reso * reso * 11.f), cfg.ks.rate);
                    m.toneOpen = false;
                }
                if (ds.on)
                {
                    const float dv = juce::jlimit (0.f, 1.f, cfg.driveBase[(size_t) p] + ds.amount * slotValue (ds, stDrv[(size_t) p], ms));
                    m.drive = dv;
                    m.driveG = std::pow (10.f, dv * cfg.driveMaxDb[(size_t) p] / 20.f);
                    if (p == PTail && cfg.ks.part[PTail].character == 5) m.driveG = std::pow (10.f, (12.f + dv * 36.f) / 20.f);
                    m.driveNorm = 1.f / std::tanh (juce::jmax (1.0e-3f, m.driveG));
                }
                mod[(size_t) p] = m;
            }
        }
        dsp::OscBankVoice oscs;
        dsp::LayerBankVoice lbv;
        static inline const dsp::OscTables noTables {};
        dsp::PunchGenerator punch; dsp::BodyGenerator body; dsp::TailGenerator tail, tailL, tailR; dsp::SubGenerator sub;
        std::array<dsp::EnvRunState, 4> stAmp {}, stFlt {}, stDrv {}, stRes {};
        std::array<dsp::ModNow, 4> mod {};
        PitchEval pe;
        double t = 0.0;
        float noteHz = 49.f, vel = 1.f, fade = 1.f, fadeStep = 0.f, relAtMs = -1.f;
        bool active = false, held = false;
        uint32_t ctl = 0;
        uint64_t age = 0;
    };

    // ------------------------------------------------------------------ the engine
    class Engine
    {
    public:
        static constexpr int osLog2 = 2, osFactor = 1 << osLog2, maxVoices = 8;
        static constexpr int numBusCh = dsp::NumParts + 2 + 2 * dsp::NumBus;   // parts, tail L / R, the 7 stereo layer buses

        Engine() : os (std::make_unique<juce::dsp::Oversampling<float>> (2, osLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false)) {}

        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate;
            maxBlockSize = juce::jmax (16, maxBlock);
            os->reset();
            os->initProcessing ((size_t) maxBlockSize);
            bus.setSize (numBusCh, maxBlockSize * osFactor);
            shaper.prepare (sampleRate * osFactor);
            distTables.allocate();
            for (auto& ch : distChain) ch.reset();
            m06St->reset(); scrEng.reset(); toneSt->reset();
            scrOsc.assign ((size_t) (maxBlockSize * osFactor), 0.f);
            scrInBufL.assign ((size_t) (maxBlockSize * osFactor), 0.f); scrInBufR.assign ((size_t) (maxBlockSize * osFactor), 0.f);
            sinceHitMs = 1.0e9f;
            wl1 = wl2 = wh1 = wh2 = wr1 = wr2 = {}; clipped = false;
            stereo.setSize (2, maxBlockSize);
            for (auto& v : voices) v = KickVoice();
            for (auto& g : gainNow) g = { 0.f, 0.f };
            masterNow = 0.f; first = true; dcL = {}; dcR = {};
            for (auto& p : freePhase) p = 0.f;
        }
        int latencySamples() const { return (int) std::lround (os->getLatencyInSamples()); }
        // the drawn shapes (module 02) - set before process() each block; nullptr = the defaults
        void setBank (const env::Bank* b) { bank = b; }
        // the oscillator tables (module 03: CUSTOM / WAVETABLE / HARMONIC) - set before process() each block; nullptr = none
        void setTables (const dsp::OscTables* t) { tables = t; }
        // host tempo (module 02 SYNC); <= 0 = not known (the LOCAL BPM is used)
        void setHostBpm (double b) { hostBpm = b; }

        void process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const ParamFrame& f)
        {
            cook (f);
            const int n = out.getNumSamples();
            int pos = 0;
            for (const auto meta : midi)
            {
                const int at = juce::jlimit (0, n, meta.samplePosition);
                if (at > pos) { render (out, pos, at - pos); pos = at; }
                const auto m = meta.getMessage();
                if (m.isNoteOn()) trigger (m.getNoteNumber(), m.getFloatVelocity());
                else if (m.isNoteOff()) release (m.getNoteNumber());
                // ONE SHOT: Note Off never cuts the kick (module 01, 11 / module 02, 11) - only RETRIGGER envelopes release
            }
            if (pos < n) render (out, pos, n - pos);
        }

        void trigger (int note, float velocity)
        {
            const bool poly = frame.idx (P::coreVoice) == 1;
            float ph[dsp::NumParts];
            KickVoice* newest = nullptr;
            for (auto& v : voices) if (v.isActive() && (newest == nullptr || v.order() > newest->order())) newest = &v;
            // PHASE RESET off: the new kick continues the phases of the newest kick (free-running oscillators)
            if (! cfg.ks.phaseReset) { if (newest != nullptr) newest->phases (freePhase.data()); for (int i = 0; i < dsp::NumParts; ++i) ph[i] = freePhase[(size_t) i]; }
            else for (int i = 0; i < dsp::NumParts; ++i) ph[i] = cfg.ks.part[(size_t) i].phase01;
            // LEGATO envelopes continue the time of a kick whose note is still held
            const float legatoMs = (newest != nullptr && newest->isHeld()) ? newest->elapsedMs() : 0.f;
            if (! poly) for (auto& v : voices) v.choke (6.f, cfg.ks.rate);
            KickVoice* slot = nullptr;
            for (auto& v : voices) if (! v.isActive()) { slot = &v; break; }
            if (slot == nullptr) { slot = &voices[0]; for (auto& v : voices) if (v.order() < slot->order()) slot = &v; }
            const float velGain = 1.f - cfg.ks.velSens * (1.f - juce::jlimit (0.f, 1.f, velocity));
            slot->trigger (note, velGain, cfg, ph, ++counter, legatoMs, clockSec, (uint32_t) counter);
            shaper.hit();
            sinceHitMs = 0.f; lastVelocity = juce::jlimit (0.f, 1.f, velocity);
            lastNoteHz = 440.f * std::exp2 ((float) (note - 69) / 12.f); relMs = -1.f;
            if (cfg.scr.on)                                                   // module 06: where the screech sits in this kick
            {
                const auto& ks = cfg.ks;
                const auto t = dsp::scr::resolve (cfg.scr, cfg.partEnd[dsp::PBody], ks.part[dsp::PBody].env.lengthMs, cfg.amp[dsp::PTail].delayMs,
                                                  ks.part[dsp::PTail].env.lengthMs, ks.lengthMs, tempo());
                scrEng.trigger (cfg.scr, t, 0x51f15eedu ^ (uint32_t) counter, cfg.ks.rate);
                scrTiming = t;
            }
            hits.fetch_add (1);
        }
        void release (int) { for (auto& v : voices) v.noteOff (cfg); if (relMs < 0.f) relMs = sinceHitMs; }
        void stopAll() { for (auto& v : voices) v.choke (3.f, cfg.ks.rate); }
        void cookOnly (const ParamFrame& f) { cook (f); }

        // meters (peak since the GUI last read them): PUNCH / BODY / TAIL / SUB and the output
        std::array<std::atomic<float>, dsp::NumParts> partPeak {};
        std::atomic<float> outPeak { 0.f };
        std::atomic<int> hits { 0 };
        const KickConfig& config() const { return cfg; }
        double tempo() const { return hostBpm > 0.0 ? hostBpm : (double) frame[P::envLocalBpm]; }

    private:
        // ---------------------------------------------------------- per block: parameters -> cooked settings
        void cook (const ParamFrame& f)
        {
            using namespace dsp;
            frame = f;
            static const env::Bank defaults;
            const env::Bank& b = bank != nullptr ? *bank : defaults;
            auto& ks = cfg.ks;
            const double rate = sr * osFactor;
            ks.rate = rate; ks.inv = (float) (1.0 / rate);
            ks.dcR = (float) std::exp (-juce::MathConstants<double>::twoPi * 5.0 / rate);
            ks.dcTail = (float) std::exp (-juce::MathConstants<double>::twoPi * 10.0 / rate);
            ks.avgC = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 8.0 / rate));
            ks.lengthMs = f[P::coreLength];
            ks.phaseReset = f.on (P::corePhaseRst);
            ks.velSens = f[P::coreVelocity];
            cfg.keyTrack = f.on (P::coreKeyTrack);
            cfg.endHz = f[P::peEnd]; cfg.fineMul = std::exp2 (f[P::peFine] / 1200.f); cfg.subHz = f[P::subPitch];
            // global time (module 02, 9): TIME SCALE, OFFSET; SYNC = times designed at 150 BPM follow the host tempo
            const float gScale = f[P::envTimeScale], gOffset = f[P::envTimeOffset];
            const float syncF = f.on (P::envSync) ? (float) (150.0 / juce::jlimit (30.0, 400.0, tempo())) : 1.f;

            // AUTO LENGTH: the punch keeps its short attack, the body follows a little, tail and sub follow fully
            const float sc = f.on (P::coreAutoLen) ? f[P::coreLength] / 450.f : 1.f;
            const float scPart[4] { 1.f, std::sqrt (sc), sc, sc };
            auto db = [] (float v) { return std::pow (10.f, v / 20.f); };
            auto tone = [rate] (float hz, float reso) { return svfCoefs (hz, 1.f / (0.7f + reso * reso * 11.f), rate); };
            auto setDrive = [&db] (ModNow& m, float d, float maxDb) { m.drive = d; m.driveG = db (d * maxDb); m.driveNorm = 1.f / std::tanh (juce::jmax (1.0e-3f, m.driveG)); };

            const int src[4] { P::punchSrc, P::bodySrc, P::tailSrc, P::subSrc };
            const int att[4] { P::punchAttack, P::bodyAttack, P::tailAttack, P::subAttack };
            const int dec[4] { P::punchDecay, P::bodyDecay, P::tailDecay, P::subDecay };
            const int sus[4] { P::punchSustain, P::bodySustain, P::tailSustain, P::subSustain };
            const int rel[4] { P::punchRelease, P::bodyRelease, P::tailRelease, P::subRelease };
            const int len[4] { P::punchLen, P::bodyLen, P::tailLen, P::subLen };
            const int drv[4] { P::punchDrive, P::bodyDrive, P::tailDrive, P::subDrive };
            const int phs[4] { P::punchPhase, P::bodyPhase, P::tailPhase, P::subPhase };
            for (int p = 0; p < 4; ++p)
            {
                auto& ps = ks.part[(size_t) p];
                ps.source = f.idx (src[p]);
                ps.env.attackMs = f[att[p]]; ps.env.decayMs = f[dec[p]] * scPart[p]; ps.env.sustain = f[sus[p]];
                ps.env.releaseMs = f[rel[p]] * scPart[p]; ps.env.lengthMs = f[len[p]] * scPart[p];
                ps.phase01 = f[phs[p]] / 360.f;
                ps.drive = f[drv[p]];
            }
            auto& pu = ks.part[PPunch];
            pu.pitchHz = f[P::punchPitch]; pu.glideMs = f[P::punchGlide]; pu.mix = f[P::punchMix];
            pu.tone = svfCoefs (pu.source == 5 ? juce::jlimit (300.f, 16000.f, f[P::punchPitch] * 6.f) : f[P::punchTone], pu.source == 5 ? 1.f / 1.5f : 1.414f, rate);
            auto& bo = ks.part[PBody]; bo.pitchHz = f[P::bodyPitch]; bo.glideMs = f[P::bodyGlide]; bo.mix = f[P::bodyMix];
            auto& ta = ks.part[PTail]; ta.character = f.idx (P::tailChar); ta.pw = f[P::tailPw]; ta.mix = f[P::tailMix];
            auto& su = ks.part[PSub]; su.pitchHz = f[P::subPitch];

            // base tone / drive per part (the envelopes move these)
            static constexpr float tailMaxDb[7] { 0.f, 36.f, 56.f, 48.f, 40.f, 36.f, 24.f };     // hardstyle tails need a LOT of gain (sharp edges = highs)
            cfg.toneHz = { pu.source == 5 ? juce::jlimit (300.f, 16000.f, f[P::punchPitch] * 6.f) : f[P::punchTone], f[P::bodyTone], f[P::tailTone], 20000.f };
            cfg.resoBase = { 0.f, f[P::bodyReso], f[P::tailReso], 0.f };
            cfg.driveBase = { f[P::punchDrive], f[P::bodyDrive], f[P::tailDrive], f[P::subDrive] };
            cfg.driveMaxDb = { 30.f, 30.f, tailMaxDb[juce::jlimit (0, 6, ta.character)], 18.f };
            auto& mb = cfg.modBase;
            mb[PPunch].tone = pu.tone; mb[PPunch].toneOpen = pu.source != 5 && f[P::punchTone] > 19000.f;
            mb[PBody].tone = tone (f[P::bodyTone], f[P::bodyReso]); mb[PBody].toneOpen = f[P::bodyTone] > 19000.f && f[P::bodyReso] < 0.01f;
            mb[PTail].tone = tone (f[P::tailTone], f[P::tailReso]); mb[PTail].toneOpen = f[P::tailTone] > 19000.f && f[P::tailReso] < 0.01f;
            mb[PSub].toneOpen = true;
            for (int p = 0; p < 4; ++p) setDrive (mb[(size_t) p], cfg.driveBase[(size_t) p], cfg.driveMaxDb[(size_t) p]);
            if (ta.character == 5) { mb[PTail].driveG = db (12.f + ta.drive * 36.f); mb[PTail].driveNorm = 1.f / std::tanh (mb[PTail].driveG); }
            ks.scream = svfCoefs (1200.f * (0.7f + 0.8f * ta.drive), 1.f / 6.f, rate);
            // ---- sound pass: tail unison / stage 2 / mid bell / cuts, width, master clip
            auto& tx = ks.tail;
            tx.uniVoices = juce::jlimit (1, 7, (int) std::lround (f[P::tailUniVoices]));
            tx.uniDetuneCt = f[P::tailUniDetune]; tx.uniDriftCt = f[P::tailUniDrift]; tx.uniMix = f[P::tailUniMix];
            tx.st2Type = f.idx (P::tailSt2Type); tx.st2G = db (f[P::tailSt2Drive]); tx.st2Bias = f[P::tailSt2Bias]; tx.st2Mix = f[P::tailSt2Mix];
            tx.midOn = std::abs (f[P::tailMidGain]) > 0.05f;
            tx.mid = doomsday::dsp::BiquadCoefs::make (doomsday::dsp::BiquadCoefs::Bell, rate, f[P::tailMidHz], juce::jmax (0.2f, f[P::tailMidQ]), f[P::tailMidGain]);
            tx.lowCutOn = f[P::tailLowCut] > 20.5f; tx.lowCut = svfCoefs (f[P::tailLowCut], 1.414f, rate);
            tx.highCutOn = f[P::tailHighCut] < 19500.f; tx.highCut = svfCoefs (f[P::tailHighCut], 1.414f, rate);
            cfg.width = f[P::tailWidth];
            cfg.wide = tx.uniVoices > 1 && cfg.width > 0.001f;
            cfg.widthSplit = svfCoefs (f[P::tailWidthFrom], 1.414f, rate);
            cfg.clipG = db (f[P::clipDrive]); cfg.clipSoft = f[P::clipSoft];
            cfg.holdGain = db (f[P::tailHoldLvl]);
            cookOscillators (f);
            cookLayers (f, b);
            dsp::dist::cookDist (f, distData, b, cfg.dist, distTables, cfg.ks.rate, offline);
            dsp::m06::cookBank (f, b, cfg.m06, cfg.ks.rate);
            dsp::tone::cookBank (f, cfg.tone, cfg.ks.rate);
            dsp::scr::cook (f, cfg.scr);

            // ---- envelope slots (module 02, 7)
            const int aMode[4] { P::punchAmpMode, P::bodyAmpMode, P::tailAmpMode, P::subAmpMode };
            for (int p = 0; p < 4; ++p)
            {
                auto& s = cfg.amp[(size_t) p];
                const int base = aMode[p];          // Mode, Delay, Curve, Amount, Trig, Sync
                const float tf = gScale * (f.on (base + 5) ? syncF : 1.f);
                s.on = true;
                s.drawn = f.idx (base) == 1;
                s.adsr = ks.part[(size_t) p].env;
                s.adsr.attackMs *= tf; s.adsr.decayMs *= tf; s.adsr.releaseMs *= tf; s.adsr.lengthMs *= tf;
                s.delayMs = (f[base + 1] + gOffset) * tf;
                s.decayK = 1.f + 8.f * f[base + 2];
                s.amount = f[base + 3];
                s.trigger = f.idx (base + 4);
                s.releaseMs = juce::jmax (3.f, s.adsr.releaseMs);
                s.bipolar = false;
                s.shape = &b.s[(size_t) (env::AmpPunch + p)];
                cfg.partEnd[(size_t) p] = s.adsr.lengthMs + s.delayMs;
            }
            auto modSlot = [&] (dsp::EnvSlot& s, int base, int part, int bankSlot)
            {
                // On, Mode, Attack, Decay, Sustain, Release, Delay, Curve, Amount, Polarity, Trig, Sync
                const float tf = gScale * (f.on (base + 11) ? syncF : 1.f);
                s.on = f.on (base);
                s.drawn = f.idx (base + 1) == 1;
                s.adsr.attackMs = f[base + 2] * tf; s.adsr.decayMs = f[base + 3] * tf; s.adsr.sustain = f[base + 4];
                s.adsr.releaseMs = f[base + 5] * tf; s.adsr.lengthMs = ks.part[(size_t) part].env.lengthMs * tf;
                s.delayMs = (f[base + 6] + gOffset) * tf;
                s.decayK = 1.f + 8.f * f[base + 7];
                s.amount = f[base + 8];
                s.bipolar = f.idx (base + 9) == 1;
                s.trigger = f.idx (base + 10);
                s.releaseMs = juce::jmax (3.f, f[base + 5] * tf);
                s.shape = &b.s[(size_t) bankSlot];
            };
            for (auto& s : cfg.filter) s.on = false;
            for (auto& s : cfg.reso) s.on = false;
            modSlot (cfg.filter[PPunch], P::punchFltOn, PPunch, env::FilterPunch);
            modSlot (cfg.filter[PBody],  P::bodyFltOn,  PBody,  env::FilterBody);
            modSlot (cfg.filter[PTail],  P::tailFltOn,  PTail,  env::FilterTail);
            modSlot (cfg.drive[PPunch],  P::punchDrvOn, PPunch, env::DrivePunch);
            modSlot (cfg.drive[PBody],   P::bodyDrvOn,  PBody,  env::DriveBody);
            modSlot (cfg.drive[PTail],   P::tailDrvOn,  PTail,  env::DriveTail);
            modSlot (cfg.drive[PSub],    P::subDrvOn,   PSub,   env::DriveSub);
            modSlot (cfg.reso[PBody],    P::bodyResOn,  PBody,  env::ResoBody);
            modSlot (cfg.reso[PTail],    P::tailResOn,  PTail,  env::ResoTail);
            if (pu.source == 5) cfg.filter[PPunch].on = false;      // CLICK tunes its own band

            // ---- pitch per part (module 02, 5 + 6)
            const int pOn[4] { P::punchPOn, P::bodyPOn, P::tailPOn, P::subPOn };   // POn, PAmount, PScale, POffset, Link, LinkSrc, FDepth, FOffset, FTime
            const int pMode[4] { P::punchPMode, P::bodyPMode, P::peMode, P::subPMode };
            const int pCurve[4] { P::punchPCurve, P::bodyPCurve, P::peCurve, P::subPCurve };
            const float pTime[4] { f[P::punchGlide] * 5.f, f[P::bodyGlide] * 5.f, f[P::peTime], f[P::subPTime] };
            for (int p = 0; p < 4; ++p)
            {
                auto& pp = cfg.pitch[(size_t) p];
                const int b0 = pOn[p];
                pp.on = f.on (b0);
                pp.mode = f.idx (pMode[p]); pp.curve = f[pCurve[p]];
                pp.timeMs = pTime[p] * f[b0 + 2] * gScale;
                pp.offsetMs = f[b0 + 3] + gOffset;
                pp.link = f.idx (b0 + 4); pp.src = f.idx (b0 + 5);
                pp.fDepth = f[b0 + 6]; pp.fOffsetSt = f[b0 + 7]; pp.fTime = f[b0 + 8];
                pp.custom = &b.s[(size_t) (env::PitchPunch + p)];
                pp.depthSt = 0.f;
                if (pp.src == p) pp.link = Independent;             // a part never follows itself
            }
            // the LINK graph must not loop (A follows B follows A): a part in a loop goes back to INDEPENDENT
            for (int p = 0; p < 4; ++p)
            {
                int q = p, steps = 0;
                bool loop = false;
                while (cfg.pitch[(size_t) q].link != Independent && steps < 5) { q = cfg.pitch[(size_t) q].src; ++steps; if (q == p) { loop = true; break; } }
                if (loop || steps >= 5) cfg.pitch[(size_t) p].link = Independent;
            }
            // depths: tail from START / END (+ AMOUNT), punch / body from their PITCH vs the tail at the hit, sub its own
            cfg.pitch[PTail].depthSt = (12.f * std::log2 (juce::jmax (1.f, f[P::peStart]) / juce::jmax (1.f, f[P::peEnd])) + f[P::peAmount]) * f[P::tailPAmount];
            {
                PitchEval tmp; tmp.c = &cfg;
                const float tail0 = f[P::peEnd] * std::exp2 (tmp.semis (PTail, 0.f) / 12.f);
                cfg.pitch[PPunch].depthSt = 12.f * std::log2 (juce::jmax (1.f, f[P::punchPitch]) / juce::jmax (1.f, tail0)) * f[P::punchPAmount];
                cfg.pitch[PBody].depthSt  = 12.f * std::log2 (juce::jmax (1.f, f[P::bodyPitch]) / juce::jmax (1.f, tail0)) * f[P::bodyPAmount];
            }
            cfg.pitch[PSub].depthSt = f[P::subPDepth] * f[P::subPAmount];
            cfg.holdMs = cfg.pitch[PTail].on ? cfg.pitch[PTail].offsetMs : 0.f;

            // CORE MIXER: generator LEVEL x fader x mute / solo x polarity, constant-power pan
            const int lvl[4]  { P::punchLevel, P::bodyLevel, P::tailLevel, P::subLevel };
            const int fad[4]  { P::mixPunchLvl, P::mixBodyLvl, P::mixTailLvl, P::mixSubLvl };
            const int pan[4]  { P::mixPunchPan, P::mixBodyPan, P::mixTailPan, P::mixSubPan };
            const int mute[4] { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute };
            const int solo[4] { P::mixPunchSolo, P::mixBodySolo, P::mixTailSolo, P::mixSubSolo };
            const int inv[4]  { P::mixPunchInv, P::mixBodyInv, P::mixTailInv, P::mixSubInv };
            bool anySolo = false;
            for (int i = 0; i < 4; ++i) anySolo |= f.on (solo[i]);
            for (int i = 0; i < 4; ++i)
            {
                const bool silent = f.on (mute[i]) || (anySolo && ! f.on (solo[i]));
                const float gl = f[lvl[i]] <= -59.9f ? 0.f : db (f[lvl[i]]);
                const float gf = f[fad[i]] <= -59.9f ? 0.f : db (f[fad[i]]);
                const float g = silent ? 0.f : gl * gf * (f.on (inv[i]) ? -1.f : 1.f);
                const float a = (juce::jlimit (-1.f, 1.f, f[pan[i]]) + 1.f) * juce::MathConstants<float>::pi * 0.25f;
                gainTarget[(size_t) i] = { g * std::cos (a) * 1.41421356f, g * std::sin (a) * 1.41421356f };   // centre = unity
            }
            masterTarget = f[P::coreMaster] <= -59.9f ? 0.f : db (f[P::coreMaster]);
            if (first) { gainNow = gainTarget; masterNow = masterTarget; first = false; }
        }

        // module 03: oscillator parameters -> cooked settings (shared with the GUI previews)
        void cookOscillators (const ParamFrame& f)
        {
            dsp::cookOscBank (f, cfg.osc);
            if (cfg.osc.tailStereo) cfg.wide = true;
        }
        // module 04: noise / transient / layers
        void cookLayers (const ParamFrame& f, const env::Bank& b)
        {
            using namespace dsp;
            auto& lb = cfg.lb;
            cookLayerBank (f, b, lb, cfg.ks.rate);
            cfg.lbAny = lb.anyLayer || lb.anyNoise || lb.click.on;
            float end = 0.f;
            for (const auto& n : lb.noise) if (n.on) end = juce::jmax (end, n.envSrc == NeOwn || n.envSrc == NeDrawn ? n.lengthMs + 5.f : cfg.ks.lengthMs);
            if (lb.click.on) end = juce::jmax (end, lb.click.positionMs + 300.f + lb.click.lenMs + 5.f);
            for (const auto& l : lb.layer) if (l.on) end = juce::jmax (end, l.offsetMs + l.delay + l.lenMs + (l.envMode == LeAdsr ? l.rel : 0.f) + 5.f);
            cfg.lbEndMs = end;
            for (const auto& n : lb.noise) if (n.on && n.route[2] > 0.f && n.width > 0.001f) cfg.wide = true;
        }

        void render (juce::AudioBuffer<float>& out, int start, int len)
        {
            while (len > 0)
            {
                const int n = juce::jmin (len, maxBlockSize);
                renderChunk (out, start, n);
                start += n; len -= n;
            }
        }
        void renderChunk (juce::AudioBuffer<float>& out, int start, int n)
        {
            const int on = n * osFactor;
            for (int c = 0; c < numBusCh; ++c) juce::FloatVectorOperations::clear (bus.getWritePointer (c), on);
            float* b[numBusCh];
            for (int c = 0; c < numBusCh; ++c) b[c] = bus.getWritePointer (c);
            const bool inside = cfg.scr.on && cfg.scr.output == dsp::scr::OInside;
            bool scrAny = false;
            KickVoice* newest = nullptr;
            if (inside)
            {
                std::fill (scrInBufL.begin(), scrInBufL.begin() + on, 0.f); std::fill (scrInBufR.begin(), scrInBufR.begin() + on, 0.f);
                float* tgt[2] { scrInBufL.data(), scrInBufR.data() };
                scrAny = renderScreech (b, on, tgt);
                for (auto& v : voices) if (v.isActive() && (newest == nullptr || v.order() > newest->order())) newest = &v;
            }
            for (auto& v : voices)
            {
                v.tables = tables;
                v.scrInL = &v == newest ? scrInBufL.data() : nullptr; v.scrInR = &v == newest ? scrInBufR.data() : nullptr;
                v.render (cfg, b, on);
                v.scrInL = v.scrInR = nullptr;
            }
            for (int l = 0; l < 6; ++l) distOn[(size_t) l] = cfg.dist.count[(size_t) l] > 0;
            if (! inside) scrAny = renderScreech (b, on);
            const bool busAny = cfg.lbAny || scrAny;
            const bool m6 = cfg.m06.any, m7 = cfg.tone.any;
            clockSec += (double) n / sr;

            stereo.clear (0, 0, n); stereo.clear (1, 0, n);
            juce::dsp::AudioBlock<float> blk (stereo.getArrayOfWritePointers(), 2, 0, (size_t) n);
            auto up = os->processSamplesUp (blk);
            float* L = up.getChannelPointer (0);
            float* R = up.getChannelPointer (1);
            const float inv = 1.f / (float) on;
            std::array<float, dsp::NumParts> pk {};
            // order: PUNCH, BODY, TAIL (-> MASTER CLIP on these three), then the SUB around the clip
            for (int c : { 0, 1, 2, 3 })
            {
                if (c == 3)
                {
                    // TEXTURE / CUSTOM buses join before the clip, then the attack gain + transient shaper on the whole kick
                    const float tg = cfg.lb.texGain, cg = cfg.lb.custGain;
                    if (busAny)
                        for (int i = 0; i < on; ++i)
                        {
                            float tl = b[6 + 2 * dsp::BusTexture][i] * tg, tr = b[7 + 2 * dsp::BusTexture][i] * tg;
                            if (m6 && cfg.m06.at (dsp::m06::LTexture, 0)) m06St->process (cfg.m06, dsp::m06::LTexture, 0, tl, tr, m6Ctx (i), cfg.ks.rate);
                            if (m7 && cfg.tone.at (dsp::m06::LTexture, 0)) toneSt->process (cfg.tone, dsp::m06::LTexture, 0, tl, tr, m6Ctx (i), cfg.ks.rate);
                            if (distOn[dsp::dist::LocTexture])                             // TEXTURE group distortion
                            {
                                const auto mc = modCtx (i);
                                tl = distChain[dsp::dist::LocTexture * 2].process (cfg.dist, dsp::dist::LocTexture, tl, mc, cfg.ks.rate);
                                tr = distChain[dsp::dist::LocTexture * 2 + 1].process (cfg.dist, dsp::dist::LocTexture, tr, mc, cfg.ks.rate);
                            }
                            if (m6 && cfg.m06.at (dsp::m06::LTexture, 1)) m06St->process (cfg.m06, dsp::m06::LTexture, 1, tl, tr, m6Ctx (i), cfg.ks.rate);
                            if (m7 && cfg.tone.at (dsp::m06::LTexture, 1)) toneSt->process (cfg.tone, dsp::m06::LTexture, 1, tl, tr, m6Ctx (i), cfg.ks.rate);
                            L[i] += tl + b[6 + 2 * dsp::BusCustom][i] * cg;
                            R[i] += tr + b[7 + 2 * dsp::BusCustom][i] * cg;
                        }
                    if (cfg.dist.globalPos == 0) globalChain (L, R, on);                                  // GLOBAL, before the clip (sub clean)
                    shaper.process (cfg.lb.shaper, L, R, on);
                }
                if (c == 3 && cfg.clipG > 1.0001f)
                {
                    for (int i = 0; i < on; ++i) { L[i] = doomsday::kick::masterClip (L[i], cfg.clipG, cfg.clipSoft); R[i] = doomsday::kick::masterClip (R[i], cfg.clipG, cfg.clipSoft); }
                }
                const auto g0 = gainNow[(size_t) c], g1 = gainTarget[(size_t) c];
                const float* x = b[c];
                const bool wideTail = c == 2 && cfg.wide;
                float p = 0.f;
                for (int i = 0; i < on; ++i)
                {
                    const float u = (float) (i + 1) * inv;
                    const float gl = g0.first + (g1.first - g0.first) * u, gr = g0.second + (g1.second - g0.second) * u;
                    float yl, yr;
                    if (wideTail)
                    {
                        // lows of the plain centre + mids / highs of the detuned sides as mid / side (LR4: sums flat)
                        const float low = wl2.lp (wl1.lp (x[i], cfg.widthSplit), cfg.widthSplit);
                        const float hl = wh2.hp (wh1.hp (b[4][i], cfg.widthSplit), cfg.widthSplit), hr = wr2.hp (wr1.hp (b[5][i], cfg.widthSplit), cfg.widthSplit);
                        const float m = 0.5f * (hl + hr), sd = 0.5f * (hl - hr) * cfg.width;
                        yl = low + m + sd; yr = low + m - sd;
                    }
                    else { yl = x[i]; yr = x[i]; }
                    yl += b[6 + 2 * c][i]; yr += b[7 + 2 * c][i];                    // module 04: layers on this part's bus
                    if (m6 && cfg.m06.at (c + 1, 0)) m06St->process (cfg.m06, c + 1, 0, yl, yr, m6Ctx (i), cfg.ks.rate);   // module 06 BEFORE
                    if (m7 && cfg.tone.at (c + 1, 0)) toneSt->process (cfg.tone, c + 1, 0, yl, yr, m6Ctx (i), cfg.ks.rate); // module 07 BEFORE
                    if (distOn[(size_t) c + 1])                                       // module 05: the GROUP distortion (pre fader)
                    {
                        const auto mc = modCtx (i);
                        yl = distChain[(size_t) (c + 1) * 2].process (cfg.dist, c + 1, yl, mc, cfg.ks.rate);
                        yr = distChain[(size_t) (c + 1) * 2 + 1].process (cfg.dist, c + 1, yr, mc, cfg.ks.rate);
                    }
                    if (m6 && cfg.m06.at (c + 1, 1)) m06St->process (cfg.m06, c + 1, 1, yl, yr, m6Ctx (i), cfg.ks.rate);   // module 06 AFTER
                    if (m7 && cfg.tone.at (c + 1, 1)) toneSt->process (cfg.tone, c + 1, 1, yl, yr, m6Ctx (i), cfg.ks.rate); // module 07 AFTER
                    yl *= gl; yr *= gr;
                    L[i] = (c == 0 ? 0.f : L[i]) + yl;
                    R[i] = (c == 0 ? 0.f : R[i]) + yr;
                    p = juce::jmax (p, std::abs (yl), std::abs (yr));
                }
                pk[(size_t) c] = p;
                gainNow[(size_t) c] = g1;
            }
            if (cfg.dist.globalPos == 1) globalChain (L, R, on);                                       // GLOBAL, after the clip (with the sub)
            if (busAny)                                                      // the MASTER bus: after the clip
                for (int i = 0; i < on; ++i) { L[i] += b[6 + 2 * dsp::BusMaster][i]; R[i] += b[7 + 2 * dsp::BusMaster][i]; }
            sinceHitMs += (float) on * 1000.f / (float) cfg.ks.rate;
            os->processSamplesDown (blk);
            const float m0 = masterNow, m1 = masterTarget, invN = 1.f / (float) n;
            const float r = (float) std::exp (-juce::MathConstants<double>::twoPi * 5.0 / sr);
            float opk = 0.f;
            const float* sl = stereo.getReadPointer (0);
            const float* sr2 = stereo.getReadPointer (1);
            for (int i = 0; i < n; ++i)
            {
                const float m = m0 + (m1 - m0) * (float) (i + 1) * invN;
                const float yl = dcL.tick (sl[i], r) * m, yr = dcR.tick (sr2[i], r) * m;
                out.setSample (0, start + i, yl);
                if (out.getNumChannels() > 1) out.setSample (1, start + i, yr);
                opk = juce::jmax (opk, std::abs (yl), std::abs (yr));
            }
            masterNow = m1;
            for (int c = 0; c < dsp::NumParts; ++c) partPeak[(size_t) c].store (juce::jmax (partPeak[(size_t) c].load(), pk[(size_t) c]));
            outPeak.store (juce::jmax (outPeak.load(), opk));
        }

        // module 05: what moves the stages' modulation at engine level (since the last hit, the tail pitch envelope, the velocity)
        dsp::dist::ModCtx modCtx (int i)
        {
            dsp::dist::ModCtx m;
            m.sinceHitMs = sinceHitMs + (float) i * 1000.f / (float) cfg.ks.rate;
            if ((i & 15) == 0)
            {
                PitchEval pe; pe.c = &cfg;
                const float depth = cfg.pitch[dsp::PTail].depthSt;
                pitchNormNow = std::abs (depth) > 0.01f ? juce::jlimit (0.f, 1.f, pe.semis (dsp::PTail, m.sinceHitMs) / depth) : 0.f;
            }
            m.pitchNorm = pitchNormNow; m.velocity = lastVelocity;
            return m;
        }
        // module 06: what moves the engine-level filters / resonators now
        dsp::m06::Ctx m6Ctx (int i) const
        {
            dsp::m06::Ctx c;
            c.ms = sinceHitMs + (float) i * 1000.f / (float) cfg.ks.rate; c.vel = lastVelocity; c.noteHz = lastNoteHz; c.lengthMs = cfg.ks.lengthMs;
            c.relMs = relMs; c.scrOsc = i < (int) scrOsc.size() ? scrOsc[(size_t) i] : 0.f;
            return c;
        }
        // GLOBAL: module 06 BEFORE -> the global distortion -> module 06 AFTER
        void globalChain (float* L, float* R, int on)
        {
            const bool a = cfg.m06.any && cfg.m06.at (dsp::m06::LGlobal, 0), z = cfg.m06.any && cfg.m06.at (dsp::m06::LGlobal, 1);
            const bool ta = cfg.tone.any && cfg.tone.at (dsp::m06::LGlobal, 0), tz = cfg.tone.any && cfg.tone.at (dsp::m06::LGlobal, 1);
            if (a) for (int i = 0; i < on; ++i) m06St->process (cfg.m06, dsp::m06::LGlobal, 0, L[i], R[i], m6Ctx (i), cfg.ks.rate);
            if (ta) for (int i = 0; i < on; ++i) toneSt->process (cfg.tone, dsp::m06::LGlobal, 0, L[i], R[i], m6Ctx (i), cfg.ks.rate);
            if (distOn[dsp::dist::LocGlobal]) globalDist (L, R, on);
            if (z) for (int i = 0; i < on; ++i) m06St->process (cfg.m06, dsp::m06::LGlobal, 1, L[i], R[i], m6Ctx (i), cfg.ks.rate);
            if (tz) for (int i = 0; i < on; ++i) toneSt->process (cfg.tone, dsp::m06::LGlobal, 1, L[i], R[i], m6Ctx (i), cfg.ks.rate);
        }
        // module 06: the screech (+ the filters / resonators placed on SCREECH) into its output bus; true = it sounds
        bool renderScreech (float* const* b, int on, float* const* inside = nullptr)
        {
            bool any = false;
            for (auto& v : scrEng.v) any |= v->active;
            std::fill (scrOsc.begin(), scrOsc.begin() + juce::jmin ((int) scrOsc.size(), on), 0.f);
            if (! any || ! cfg.scr.on) { if (! cfg.scr.on) scrEng.reset(); return false; }
            static const int busOf[7] { dsp::BusTail, dsp::BusTexture, dsp::BusMaster, dsp::BusPunch, dsp::BusBody, dsp::BusSub, dsp::BusTail };
            const int bus = busOf[juce::jlimit (0, 6, cfg.scr.output)];
            const dsp::OscTables& tb = tables != nullptr ? *tables : noOscTables;
            PitchEval pe; pe.c = &cfg;
            float semis = 0.f;
            for (int i = 0; i < on; ++i)
            {
                const float ms = sinceHitMs + (float) i * 1000.f / (float) cfg.ks.rate;
                if (cfg.scr.pitchMode == 1 && (i & 15) == 0) semis = pe.semis (dsp::PTail, ms);
                // RELATIVE: the played note two octaves up (the screech register); MIDI TRACKED: + the kick's pitch engine; ABSOLUTE: BASE HZ
                const float base = cfg.scr.pitchMode == 2 ? cfg.scr.baseHz : lastNoteHz * dsp::scr::registerMul * (cfg.scr.pitchMode == 1 ? std::exp2 (semis / 12.f) : 1.f);
                float l = 0.f, r = 0.f, o = 0.f;
                for (auto& v : scrEng.v) { float vl, vr, vo; v->process (cfg.scr, tb, base, cfg.ks.rate, lastNoteHz, vl, vr, vo); l += vl; r += vr; o += vo; }
                scrOsc[(size_t) i] = o;
                if (cfg.m06.any && cfg.m06.at (dsp::m06::LScreech, 0)) m06St->process (cfg.m06, dsp::m06::LScreech, 0, l, r, m6Ctx (i), cfg.ks.rate);
                if (cfg.tone.any && cfg.tone.at (dsp::m06::LScreech, 0)) toneSt->process (cfg.tone, dsp::m06::LScreech, 0, l, r, m6Ctx (i), cfg.ks.rate);
                if (inside != nullptr) { inside[0][i] = l; inside[1][i] = r; }
                else { b[6 + 2 * bus][i] += l; b[7 + 2 * bus][i] += r; }
            }
            return true;
        }
        void globalDist (float* L, float* R, int on)
        {
            for (int i = 0; i < on; ++i)
            {
                const auto mc = modCtx (i);
                L[i] = distChain[0].process (cfg.dist, dsp::dist::LocGlobal, L[i], mc, cfg.ks.rate);
                R[i] = distChain[1].process (cfg.dist, dsp::dist::LocGlobal, R[i], mc, cfg.ks.rate);
            }
        }
        std::unique_ptr<std::array<dsp::dist::ChainState, 12>> distChainsOwned = std::make_unique<std::array<dsp::dist::ChainState, 12>>();
        std::array<dsp::dist::ChainState, 12>& distChain = *distChainsOwned;     // GLOBAL, PUNCH, BODY, TAIL, SUB, TEXTURE x L / R
        dsp::dist::DistTables distTables;
        const dsp::dist::DistData* distData = nullptr;
        std::array<bool, 6> distOn {};
        float sinceHitMs = 1.0e9f, pitchNormNow = 0.f, lastVelocity = 1.f, lastNoteHz = 49.f, relMs = -1.f;
        std::unique_ptr<dsp::m06::State> m06St = std::make_unique<dsp::m06::State>();   // module 06 at engine level (not the layers)
        std::unique_ptr<dsp::tone::State> toneSt = std::make_unique<dsp::tone::State>(); // module 07 at engine level (not the layers)
        dsp::scr::Engine scrEng;
        dsp::scr::Timing scrTiming;
        std::vector<float> scrOsc;                                             // the screech oscillators per sample (resonator excitation)
        std::vector<float> scrInBufL, scrInBufR;                               // the screech INSIDE the tail (sized in prepare)
        static inline const dsp::OscTables noOscTables {};
    public:
        const dsp::scr::Timing& screechTiming() const { return scrTiming; }
    private:
        bool offline = false;
    public:
        void setDistData (const dsp::dist::DistData* d) { distData = d; }
        void setOffline (bool o) { offline = o; }
    private:

        double sr = 44100.0, hostBpm = 0.0;
        int maxBlockSize = 512;
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        juce::AudioBuffer<float> bus, stereo;
        std::array<KickVoice, maxVoices> voices;
        KickConfig cfg;
        const env::Bank* bank = nullptr;
        const dsp::OscTables* tables = nullptr;
        double clockSec = 0.0;                    // the engine's own clock (FREE RUN oscillator phases)
        dsp::AttackShaper shaper;                 // module 04: attack gain + transient shaper (kick before the clip)
        ParamFrame frame = ParamFrame::defaults();
        std::array<std::pair<float, float>, dsp::NumParts> gainNow {}, gainTarget {};
        std::array<float, dsp::NumParts> freePhase {};
        float masterNow = 0.f, masterTarget = 1.f;
        bool first = true;
        dsp::DcBlock dcL, dcR;
        doomsday::dsp::Svf wl1, wl2, wh1, wh2, wr1, wr2;
        bool clipped = false;
        uint64_t counter = 0;
    };

    // ------------------------------------------------------------------ offline render (GUI waveform, tests, export)
    //  one kick at 'rate', 'seconds' long; soloPart = -1 the full kick, 0..3 = only that part (mixer settings kept)
    inline juce::AudioBuffer<float> renderKick (ParamFrame f, int note, double rate, double seconds, int soloPart = -1, float velocity = 1.f,
                                                const env::Bank* bank = nullptr, double bpm = 0.0, double noteOffAt = -1.0,
                                                const dsp::OscTables* tables = nullptr, const dsp::OscData* oscData = nullptr,
                                                const dsp::dist::DistData* distData = nullptr)
    {
        // no tables given: build them here from the parameters (+ the given / default oscillator data) - offline only
        std::array<dsp::TablePtr, dsp::maxOsc> keep;
        dsp::OscTables own;
        if (tables == nullptr)
        {
            static const dsp::OscData defaults;
            const auto& d = oscData != nullptr ? *oscData : defaults;
            for (int i = 0; i < dsp::numOsc; ++i)
                if (f.on (P::osc1On + i * (P::osc2On - P::osc1On))) { keep[(size_t) i] = dsp::osctables::build (f, d, i); own.t[(size_t) i] = keep[(size_t) i].get(); }
            tables = &own;
        }
        if (soloPart >= 0)
        {
            const int solo[4] { P::mixPunchSolo, P::mixBodySolo, P::mixTailSolo, P::mixSubSolo };
            const int mute[4] { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute };
            for (int i = 0; i < 4; ++i) { f[solo[i]] = i == soloPart ? 1.f : 0.f; f[mute[i]] = 0.f; }
        }
        const int block = 256, total = (int) (rate * seconds);
        const int offAt = noteOffAt >= 0.0 ? (int) (noteOffAt * rate) : -1;
        auto engine = std::make_unique<Engine>();
        engine->prepare (rate, block);
        engine->setBank (bank);
        engine->setHostBpm (bpm);
        engine->setTables (tables);
        engine->setDistData (distData);
        engine->setOffline (true);                       // renderKick = the export path (OFFLINE QUALITY applies)
        juce::AudioBuffer<float> out (2, total), tmp (2, block);
        out.clear();
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            tmp.setSize (2, n, false, false, true); tmp.clear();
            juce::MidiBuffer m;
            if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, velocity), 0);
            if (offAt >= pos && offAt < pos + n) m.addEvent (juce::MidiMessage::noteOff (1, note), offAt - pos);
            engine->process (tmp, m, f);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, tmp, ch, 0, n);
        }
        return out;
    }
}
