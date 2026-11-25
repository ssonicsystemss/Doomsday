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
#include "Core/ShapedPunch.h"
#include "Core/PvcPunch.h"
#include "Tone/PvcTail.h"
#include "Tone/ZaagTail.h"
#include "Tone/HardcoreKick.h"
#include "Analysis/LayerEnergy.h"
#include "Core/ZaagPunch.h"
#include "Core/ClassicPunch.h"
#include "Core/RawPunch.h"
#include "Tone/TailRise.h"
#include "Front/Crack.h"
#include "Tone/RawTail.h"
#include "Tone/ExpTail.h"
#include "Front/Click.h"
#include "Front/Tok.h"
#include "Front/SubPunch.h"
#include "Front/FrontProfiles.h"
#include "Env/KickEnvelopes.h"
#include "Osc/OscTables.h"
#include "Layers/LayerCook.h"
#include "Dist/DistCook.h"
#include "Tone/ToneCook.h"
#include "Dyn/DynCook.h"
#include "Spatial/SpatialCook.h"
#include "Filter/M06Cook.h"
#include "Mod/Mod.h"
#include "DoomsdayKick.h"
#include "../Parameters/ParamFrame.h"
#include "Tone/Room.h"
#include "Tone/TailColor.h"
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
        bool startFixed = false;                  // the TOK (pitch START) at a fixed pitch (owner 2026-11-09)
        float humanPitch = 1.f;                   // KICK VARIATION - HUMANIZE: this hit's pitch factor (set per hit by the engine)
        bool tokMode = false, tokFixed = true;    // PUNCH / TOK -> TAIL: the punch is the tok (its own note), the tail starts after it
        float tokHz = 400.f, tailStartMs = 0.f, tokOverlapMs = 4.f;
        dsp::room::Settings room;
        dsp::shaped::Settings shaped;             // SHAPED PUNCH (owner 2026-11-14): the rendered tonal layer + click + verb, read by the voices
        const float* shL = nullptr; const float* shR = nullptr; int shLen = 0; double shRate = 48000.0;
        dsp::pvctail::Settings pvcTail;           // TAIL ENGINE PVC (owner 2026-11-14, the KLOENK tail)
        dsp::raw::Settings raw;
        dsp::exper::Settings exp;                 // TAIL TYPE EXPERIMENTAL (owner 2026-11-16, TAIL_RAW_EXP.md)                   // TAIL TYPE RAW (owner 2026-11-16, TAIL_RAW_EXP.md)
        dsp::zaag::Settings zaag;
        dsp::hardcore::Settings hc;                // TAIL TYPE HARDCORE (owner 2026-11-24)                 // TAIL TYPE ZAAG (owner 2026-11-14 / 15)
        dsp::front::ClickSettings click; dsp::front::TokSettings tok; dsp::front::SubPunchSettings spk; float zgSend = 0.f;
        bool crackSolo = false, crunchSolo = false;                  // SOLO CRACK: only the crack (diagnostics)
        dsp::front::CrackSettings crack;         // RAW PUNCH EXTENSION (owner 2026-11-21): CRACK + SNAP + BODY COUPLING, mixed after the master clip   // the FRONT (owner 2026-11-16)
        dsp::classicpunch::Settings cpunch;      // PUNCH TYPE CLASSIC (EARLY REWORK, owner 2026-11-16)
        dsp::rawpunch::Settings rpunch;
        dsp::rise::Settings rise;                // TAIL RISE (owner 2026-11-21): the end of the tail grows, BPM-grid synced          // PUNCH TYPE RAW: RAW CLASSIC / RAW 2015-2017 CRACKING (owner 2026-11-21)
        dsp::zaagpunch::Settings zpunch;          // PUNCH TYPE ZAAG (ARCHITECTURE.md rule 0: the punch lives in the PUNCH ENGINE)
        dsp::color::Settings color;               // TAIL COLOR after the tail's distortion (owner 2026-11-12)                 // the TAIL room before its distortion (EARLY, owner 2026-11-12)
        int tokNoteMode = 0;                      // 0 FIXED / 1 FOLLOW MIDI / 2 OFFSET (the played note + tokOffsetMul)
        float tokOffsetMul = 2.f, tokEndMs = 0.f, tokDecayDb = 0.f, tokBendSt = 0.f, tokGlideMs = 10.f;   // (owner 2026-11-10)
        float startHz = 220.f, endHz0 = 49.f, startAmountSt = 0.f, tailPAmt = 1.f;
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
        dsp::dyn::Bank dyn;                       // module 08: compressors / transient shapers / clippers / limiters / crest per place
        dsp::spatial::Bank spc;                   // module 11: the stereo unit of each group + the master
        uint32_t seedSalt = 0;                    // module 12: the render seed (0 = the preset's own randomness)
        bool masterMute = false, masterBypass = false, outMono = false, safetyClip = false, safetyLim = false;   // module 12: MASTER OUTPUT
        bool noteCut = false, noteCutFx = false;  // KICK END: the kick (and, with NOTE + FX, the effect tails) ends with the MIDI note
        dsp::scr::Settings scr;                   // module 06: the screech
        bool lbAny = false;
        float lbEndMs = 0.f;                      // when the last layer / noise / click ends       // TOK LEVEL: tail gain while the pitch HOLDs (after the distortion, so the drop hits harder)
    };

    // ------------------------------------------------------------------ pitch: semitones of one part at a time (links resolved)
    struct PitchEval
    {
        const KickConfig* c = nullptr;
        std::array<int, 8> cur {};                // segment cursors (own curve, followed curve)
        float tailScale = 1.f;                    // per voice: the tail depth so START stays fixed (TOK FIXED)
        float own (int part, float ms, int curIdx)
        {
            const auto& p = c->pitch[(size_t) part];
            if (! p.on) return 0.f;
            if (part == dsp::PTail && tailScale != 1.f) { const float s = tailScale; tailScale = 1.f; const float r = own (part, ms, curIdx) * s; tailScale = s; return r; }
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
            srcAmp = {}; srcPitch = 1.f; srcFilt = 0.f;
            active = true; t = 0.0; shSubPh = 0.f; age = order; vel = velocityGain; fade = 1.f; fadeStep = 0.f; held = true; ctl = 0; relAtMs = -1.f; noteNum = note;
            noteHz = 440.f * std::exp2 ((float) (note - 69) / 12.f) * cfg.humanPitch;
            const uint32_t seed = 0x9e3779b9u * (uint32_t) (note + 1) + cfg.seedSalt * 0x632BE5ABu;   // module 12: SEED (0 = as always)
            punch.start (cfg.ks.part[PPunch], phases01[PPunch], seed ^ 0x1234u);
            body.start  (cfg.ks.part[PBody],  phases01[PBody],  seed ^ 0x5678u);
            const float tph = cfg.tokMode ? phases01[PPunch] : phases01[PTail];   // TOK: the tail runs in phase with the tok (no cancellation in the crossfade)
            tail.start  (cfg.ks.part[PTail],  tph,  seed ^ 0x9abcu, 0);
            tailL.start (cfg.ks.part[PTail],  tph,  seed ^ 0x9abcu, 1);
            tailR.start (cfg.ks.part[PTail],  tph,  seed ^ 0x9abcu, 2);
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
                pe.c = &cfg; pe.tailScale = tailScaleFor (cfg);
                const float endEff = (cfg.keyTrack ? noteHz : cfg.endHz) * cfg.fineMul;
                const float subBase = (cfg.keyTrack ? noteHz : cfg.subHz) * cfg.fineMul;
                const float fTail = endEff * std::exp2 (pe.semis (dsp::PTail, 0.f) / 12.f);
                const std::array<float, 4> hz { cfg.tokMode ? tokHzFor (cfg) : fTail * std::exp2 (pe.semis (dsp::PPunch, 0.f) / 12.f), fTail * std::exp2 (pe.semis (dsp::PBody, 0.f) / 12.f),
                                                fTail, subBase * std::exp2 (pe.semis (dsp::PSub, 0.f) / 12.f) };
                const std::array<float, 4> starts { cfg.amp[dsp::PPunch].delayMs, cfg.amp[dsp::PBody].delayMs, cfg.amp[dsp::PTail].delayMs + cfg.holdMs, cfg.amp[dsp::PSub].delayMs };
                lbv.start (cfg.lb, cfg.osc, hitCount, (uint64_t) (absTimeSec * cfg.ks.rate), hz, starts);
                pe = {};
            }
            if (cfg.osc.any)
            {
                pe.c = &cfg; pe.tailScale = tailScaleFor (cfg);
                const float endEff = (cfg.keyTrack ? noteHz : cfg.endHz) * cfg.fineMul;
                const float subBase = (cfg.keyTrack ? noteHz : cfg.subHz) * cfg.fineMul;
                const float fTail = endEff * std::exp2 (pe.semis (dsp::PTail, 0.f) / 12.f);
                const std::array<float, 4> hz { cfg.tokMode ? tokHzFor (cfg) : fTail * std::exp2 (pe.semis (dsp::PPunch, 0.f) / 12.f), fTail * std::exp2 (pe.semis (dsp::PBody, 0.f) / 12.f),
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
        int note() const { return noteNum; }
        float elapsedMs() const { return (float) (t * 1000.0); }
        uint64_t order() const { return age; }
        double samplePos (int slot) const { return lbv.samplePos (slot); }
        void phases (float* out) const { out[0] = punch.phase(); out[1] = body.phase(); out[2] = tail.phase(); out[3] = sub.phase(); }
        // PUNCH / TOK: the tok core holds one note (FIXED = the same on every MIDI note, FOLLOW = moves with it) - no glide
        float tokHzFor (const KickConfig& cfg) const
        {
            const float endEff = (cfg.keyTrack ? noteHz : cfg.endHz) * cfg.fineMul;
            if (cfg.tokNoteMode == 2) return endEff * cfg.tokOffsetMul;
            return cfg.tokFixed ? cfg.tokHz * cfg.fineMul : cfg.tokHz * endEff / juce::jmax (1.f, cfg.endHz);
        }
        // the tok's own pitch envelope: BEND semitones away at the hit, gliding to its note
        static float tokBendAt (const KickConfig& cfg, float ms)
        {
            if (std::abs (cfg.tokBendSt) < 0.01f) return 1.f;
            return std::exp2 (cfg.tokBendSt * std::exp (-3.f * ms / juce::jmax (0.5f, cfg.tokGlideMs)) / 12.f);
        }
        // TOK FIXED: the tail's pitch depth for THIS note, so the hit (START) is the same pitch on every note and the end is the note
        float tailScaleFor (const KickConfig& cfg) const
        {
            const float d = cfg.pitch[dsp::PTail].depthSt;
            if (! cfg.startFixed || ! cfg.keyTrack || std::abs (d) < 0.01f) return 1.f;
            const float v = (12.f * std::log2 (juce::jmax (1.f, cfg.startHz) / juce::jmax (1.f, noteHz)) + cfg.startAmountSt) * cfg.tailPAmt;
            return v / d;
        }

        // adds this voice into the four part buses (oversampled rate)
        const dsp::OscTables* tables = nullptr;
        const float* scrInL = nullptr; const float* scrInR = nullptr;          // the screech into this voice's tail (the newest voice only)
        void render (const KickConfig& cfg, float* const* bus, int n)
        {
            using namespace dsp;
            if (! active) return;
            const auto& ks = cfg.ks;
            pe.c = &cfg; pe.tailScale = tailScaleFor (cfg);
            const float endEff = (cfg.keyTrack ? noteHz : cfg.endHz) * cfg.fineMul;
            const float subBase = (cfg.keyTrack ? noteHz : cfg.subHz) * cfg.fineMul;
            const float tokHz = tokHzFor (cfg);
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
                const float tokNow = cfg.tokMode ? tokHz * tokBendAt (cfg, ms) : 0.f;
                // TOK: before the tail starts it runs at the tok's pitch (same note, same phase) - the hand-over has no pitch / phase step
                const float fTail = juce::jlimit (5.f, nyq, (cfg.tokMode && ms < cfg.tailStartMs ? tokNow : endEff * std::exp2 (pe.semis (PTail, ms) / 12.f))
                                                            * (cfg.rise.on ? dsp::rise::pitchFactor (cfg.rise, ms) : 1.f));   // TAIL RISE: PITCH layer
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
                    const std::array<float, 4> hz { juce::jlimit (5.f, nyq, cfg.tokMode ? tokNow : fTail * std::exp2 (pe.semis (PPunch, ms) / 12.f)),
                                                    juce::jlimit (5.f, nyq, fTail * std::exp2 (pe.semis (PBody, ms) / 12.f)), fTail,
                                                    juce::jlimit (5.f, nyq, subBase * std::exp2 (pe.semis (PSub, ms) / 12.f)) };
                    oscs.tick (cfg.osc, tables != nullptr ? *tables : noTables, hz, ms, ks.inv, ext, extL, extR);
                }
                // ---- the amplitude envelopes of the parts (once per sample: the generators and the noise envelopes use them)
                std::array<float, 4> amp {};
                for (int p = 0; p < 4; ++p) amp[(size_t) p] = ms < cfg.partEnd[(size_t) p] ? ampOf (cfg, p, ms) : 0.f;
                if ((ctl & 15) == 1)                                              // module 09: what the modulation sources read
                {
                    srcAmp = amp;
                    const float depth = cfg.pitch[PTail].depthSt;
                    srcPitch = std::abs (depth) > 0.01f ? juce::jlimit (0.f, 1.f, pe.semis (PTail, ms) / depth) : 0.f;
                }
                // ---- module 04: noise into the part inputs, transient / layers into the 7 buses
                if (cfg.lbAny)
                {
                    const std::array<float, 4> hz { juce::jlimit (5.f, nyq, cfg.tokMode ? tokNow : fTail * std::exp2 (pe.semis (PPunch, ms) / 12.f)),
                                                    juce::jlimit (5.f, nyq, fTail * std::exp2 (pe.semis (PBody, ms) / 12.f)), fTail,
                                                    juce::jlimit (5.f, nyq, subBase * std::exp2 (pe.semis (PSub, ms) / 12.f)) };
                    float bl[dsp::NumBus] {}, br[dsp::NumBus] {};
                    dsp::dist::ModCtx mc;
                    mc.sinceHitMs = ms; mc.velocity = vel;
                    const float depth = cfg.pitch[PTail].depthSt;
                    mc.pitchNorm = std::abs (depth) > 0.01f ? juce::jlimit (0.f, 1.f, pe.semis (PTail, ms) / depth) : 0.f;
                    dsp::m06::Ctx m6c; m6c.ms = ms; m6c.vel = vel; m6c.noteHz = noteHz; m6c.lengthMs = ks.lengthMs; m6c.relMs = relAtMs;
                    lbv.tick (cfg.lb, cfg.osc, tables != nullptr ? *tables : noTables, hz, amp, noteHz / 49.f, ms, ks.inv, (float) ks.rate,
                              ext, extL, extR, bl, br, &cfg.dist, &mc, cfg.m06.any ? &cfg.m06 : nullptr, &m6c, cfg.tone.any ? &cfg.tone : nullptr, cfg.dyn.any ? &cfg.dyn : nullptr);
                    for (int k = 0; k < dsp::NumBus; ++k) { bus[6 + 2 * k][i] += bl[k] * gate; bus[7 + 2 * k][i] += br[k] * gate; }
                }
                // PUNCH / TOK -> BODY + TAIL: an equal-power crossfade over OVERLAP (cos out / sin in)
                float gTok = 1.f, gIn = 1.f;
                if (cfg.tokMode)
                {
                    // the tok fades out over the last OVERLAP ms of its length, body + tail fade in over OVERLAP from their start
                    // (AFTER TOK: the same window = an equal-power crossfade; INDEPENDENT: wherever TAIL START puts them)
                    const float ov = cfg.tokOverlapMs;
                    const float xo = ov > 0.01f ? juce::jlimit (0.f, 1.f, (ms - (cfg.tokEndMs - ov)) / ov) : (ms >= cfg.tokEndMs ? 1.f : 0.f);
                    const float xi = ov > 0.01f ? juce::jlimit (0.f, 1.f, (ms - cfg.tailStartMs) / ov) : (ms >= cfg.tailStartMs ? 1.f : 0.f);
                    gTok = std::cos (xo * 1.5707963f); gIn = std::sin (xi * 1.5707963f);
                    if (cfg.tokDecayDb < -0.01f)                                       // TOK DECAY: a strong start that eases off towards the hand-over
                        gTok *= std::exp2 (cfg.tokDecayDb * 0.1660964f * juce::jlimit (0.f, 1.f, ms / juce::jmax (1.f, cfg.tokEndMs)));
                }
                if (cfg.shaped.on && ms < cfg.partEnd[PPunch])                         // SHAPED PUNCH: the rendered layer (stereo) + its SUB on the note
                {
                    const double pos = (double) ms * 0.001 * cfg.shRate; const int i0 = (int) pos;
                    float sl = 0.f, sr = 0.f;
                    if (cfg.shL != nullptr && i0 + 1 < cfg.shLen)
                    {
                        const float fr = (float) (pos - i0);
                        sl = cfg.shL[i0] + (cfg.shL[i0 + 1] - cfg.shL[i0]) * fr; sr = cfg.shR[i0] + (cfg.shR[i0 + 1] - cfg.shR[i0]) * fr;
                    }
                    if (! cfg.shaped.tonalOnly && cfg.shaped.sub > 0.f)
                    {
                        const float d = cfg.shaped.subDelayMs, en = juce::jmax (d + 30.f, cfg.shaped.subDecayMs), hold = d + 15.f + 0.7f * (en - d - 15.f);
                        const float e = ms < d ? 0.2f * ms / juce::jmax (0.5f, d) : ms < d + 15.f ? 0.2f + 0.8f * (ms - d) / 15.f
                                      : ms < hold ? 1.f - 0.2f * (ms - d - 15.f) / juce::jmax (1.f, hold - d - 15.f) : juce::jmax (0.f, 0.8f * (1.f - (ms - hold) / juce::jmax (1.f, en - hold)));
                        const float up = std::exp2 (cfg.shaped.subStartSt / 12.f) - 1.f;      // SUB PUNCH: its own pitch envelope
                        const float hz = noteHz * cfg.fineMul * (1.f + up * std::exp (-ms / juce::jmax (1.f, cfg.shaped.subDropMs)));
                        const float wv = cfg.shaped.subWave == 1 ? 0.6366198f * std::asin (std::sin (shSubPh)) : std::sin (shSubPh);
                        const float sv = wv * e * cfg.shaped.sub;
                        shSubPh += 6.2831853f * hz * (float) dt; if (shSubPh > 6.2831853f) shSubPh -= 6.2831853f;
                        sl += sv; sr += sv;
                    }
                    bus[6][i] += sl * gate; bus[7][i] += sr * gate;
                }
                if (! cfg.shaped.on && ms < cfg.partEnd[PPunch])
                {
                    const float f = juce::jlimit (5.f, nyq, cfg.tokMode ? tokNow : fTail * std::exp2 (pe.semis (PPunch, ms) / 12.f));
                    bus[PPunch][i] += punch.tick (ks.part[PPunch], ks, ms, f, amp[PPunch], mod[PPunch], ext[PPunch]) * gate * gTok;
                }
                if (ms < cfg.partEnd[PBody])
                {
                    const float f = juce::jlimit (5.f, nyq, fTail * std::exp2 (pe.semis (PBody, ms) / 12.f));
                    bus[PBody][i] += body.tick (ks.part[PBody], ks, f, amp[PBody], mod[PBody], ext[PBody]) * gate * gIn;
                }
                if (ms < cfg.partEnd[PTail])
                {
                    float a = amp[PTail] * gate * gIn;
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
                    if (fs.on) { const float fv = slotValue (fs, stFlt[(size_t) p], ms); if (p == PTail) srcFilt = fv;
                                 hz = juce::jlimit (20.f, 20000.f, hz * std::exp2 (fs.amount * 6.f * fv)); }
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
    public:
        // module 09: the newest kick's envelopes as modulation sources (updated every 16 samples while it plays)
        std::array<float, 4> srcAmp {};
        float srcPitch = 0.f, srcFilt = 0.f;
        float noteNumberKey() const { return 12.f * std::log2 (noteHz / 440.f) + 69.f; }
    private:
        dsp::OscBankVoice oscs;
        dsp::LayerBankVoice lbv;
        static inline const dsp::OscTables noTables {};
        dsp::PunchGenerator punch; dsp::BodyGenerator body; dsp::TailGenerator tail, tailL, tailR; dsp::SubGenerator sub;
        std::array<dsp::EnvRunState, 4> stAmp {}, stFlt {}, stDrv {}, stRes {};
        std::array<dsp::ModNow, 4> mod {};
        PitchEval pe;
        double t = 0.0;
        float noteHz = 49.f, vel = 1.f, fade = 1.f, fadeStep = 0.f, relAtMs = -1.f, shSubPh = 0.f;
        bool active = false, held = false;
        int noteNum = -1;
        uint32_t ctl = 0;
        uint64_t age = 0;
    };

    // ------------------------------------------------------------------ the engine
    // module 14: the kick LENGTH in ms - the LENGTH knob, or LENGTH SYNC (a part of the beat at the tempo)
    inline float kickLengthMs (const ParamFrame& f, double bpm)
    {
        static const float beats[5] { 0.f, 0.25f, 0.5f, 1.f, 2.f };
        const int s = f.idx (P::lenSync);
        if (s <= 0) return f[P::coreLength];
        return juce::jlimit (60.f, 4000.f, (float) (beats[juce::jlimit (0, 4, s)] * 60000.0 / juce::jlimit (30.0, 400.0, bpm > 0.0 ? bpm : 150.0)));
    }
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
            m06St->reset(); scrEng.reset(); toneSt->reset(); dynSt->reset();
            limRel = (float) (1.0 - std::exp (-1.0 / (0.08 * sampleRate))); muteStep = (float) (1.0 / (0.005 * sampleRate)); limG = 1.f;
            muteG = 1.f;
            tailRoom->prepare (sampleRate * osFactor); tokRoom->prepare (sampleRate * osFactor); tailColor.reset();
            shapedR->prepare (sampleRate); pvcR->prepare (sampleRate); pvcU.prepare (sampleRate * osFactor); zaagU.prepare (sampleRate * osFactor); hcU.prepare (sampleRate * osFactor); rawU.prepare (sampleRate * osFactor); expU.prepare (sampleRate * osFactor); zpU.prepare (sampleRate * osFactor); cpU.prepare (sampleRate * osFactor); rpU.prepare (sampleRate * osFactor); riseU.prepare (sampleRate * osFactor);
            clkU.prepare (sampleRate * osFactor); tokU.prepare (sampleRate * osFactor); spkU.prepare (sampleRate * osFactor); crU.prepare (sampleRate * osFactor);
            sendBuf.assign ((size_t) (maxBlockSize * osFactor), 0.f);
            atkBuf.assign ((size_t) (maxBlockSize * osFactor), 0.f);
            crkBuf.assign ((size_t) (maxBlockSize * osFactor), 0.f);
            for (auto& u : *spcUnits)
            {
                u.reset();
                u.setRate (sampleRate * osFactor);
            }
            scrOsc.assign ((size_t) (maxBlockSize * osFactor), 0.f);
            scrInBufL.assign ((size_t) (maxBlockSize * osFactor), 0.f); scrInBufR.assign ((size_t) (maxBlockSize * osFactor), 0.f);
            sinceHitMs = 1.0e9f; sinceHitOs = (int64_t) 1 << 40;
            wl1 = wl2 = wh1 = wh2 = wr1 = wr2 = {}; clipped = false;
            stereo.setSize (2, maxBlockSize);
            for (auto& v : voices) v = KickVoice();
            for (auto& g : gainNow) g = { 0.f, 0.f };
            masterNow = 0.f; first = true; dcL = {}; dcR = {};
            for (auto& p : freePhase) p = 0.f;
            modEng.reset (sampleRate); sinceTick = 0; folEnv = folMs = folOut = 0.f; ccVal = {}; modFresh = true; heldNotes = 0;
        }
        int latencySamples() const { return (int) std::lround (os->getLatencyInSamples()) + cfg.dyn.masterLook; }
        // the drawn shapes (module 02) - set before process() each block; nullptr = the defaults
        void setBank (const env::Bank* b) { bank = b; }
        // the oscillator tables (module 03: CUSTOM / WAVETABLE / HARMONIC) - set before process() each block; nullptr = none
        void setTables (const dsp::OscTables* t) { tables = t; }
        // host tempo (module 02 SYNC); <= 0 = not known (the LOCAL BPM is used)
        void setHostBpm (double b) { hostBpm = b; }

        // module 09: the modulation matrix (nullptr / no active slot = no modulation) and the host position (LFO / sequencer sync)
        void setModData (const dsp::mod::Data* d)
        {
            if (d != modData || (d != nullptr && d->version != modVersion)) { modData = d; modVersion = d != nullptr ? d->version : 0; if (d != nullptr) modEng.scan (*d); }
        }
        // module 12: the render SEED (0 = the preset's own randomness; the same seed = the same kick) and the export source
        void setSeed (uint32_t s) { cfg.seedSalt = s; modEng.salt = s; }
        uint32_t seed() const { return cfg.seedSalt; }
        void setExportKeep (uint32_t core, uint32_t buses) { keepCore = core; keepBus = buses; }
        std::atomic<bool> safetyActive { false };                            // (GUI: the safety limiter is reducing)
        using SamplePtrs = std::array<std::atomic<const dsp::SampleData*>, dsp::numSampleSlots>;
        void setSamples (const SamplePtrs* p) { samplePtrs = p; }           // module 10 (the owner keeps every published sample alive)
        void setHostPos (double ppq, bool playing, int barBeats = 4) { hostPpq = ppq; hostPlaying = playing; hostBar = juce::jmax (1, barBeats); }
        const dsp::mod::Engine& modEngine() const { return modEng; }
        const ParamFrame& finalFrame() const { return frame; }
        // module 09 monitors for the GUI (SOURCE / DESTINATION MONITOR, lock-free; updated once per block)
        std::array<std::atomic<float>, dsp::mod::NumSrc> srcMon {};
        std::array<std::atomic<float>, dsp::mod::numSlots> slotFinal {};          // the FINAL normalised value of each slot's target
        std::atomic<int> seqStepMon { 0 };
        // module 10: the playback position of each sample slot in the newest kick (0..1 of the sample, -1 = silent) for the editor
        std::array<std::atomic<float>, dsp::numSampleSlots> smpPosMon {};
        bool modFullCook = false;
        uint32_t modExtraCook = 0;                      // (tests: re-cook these sections at every tick too)                        // (tests: re-cook everything at every control tick)

        void process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const ParamFrame& f)
        {
            if (modData != nullptr && modData->active > 0) { processMod (out, midi, f); publishSamplePos(); return; }
            modFresh = true;
            cook (f);
            const int n = out.getNumSamples();
            int pos = 0;
            for (const auto meta : midi)
            {
                const int at = juce::jlimit (0, n, meta.samplePosition);
                if (at > pos) { render (out, pos, at - pos); pos = at; }
                const auto m = meta.getMessage();
                if (m.isNoteOn()) { noteOnKey (m.getNoteNumber()); trigger (m.getNoteNumber(), m.getFloatVelocity()); }
                else if (m.isNoteOff()) release (m.getNoteNumber());
                // ONE SHOT: Note Off never cuts the kick (module 01, 11 / module 02, 11) - only RETRIGGER envelopes release
            }
            if (pos < n) render (out, pos, n - pos);
            publishSamplePos();
        }
        void publishSamplePos()
        {
            if (samplePtrs == nullptr) return;
            const KickVoice* newest = nullptr;
            for (const auto& v : voices) if (v.isActive() && (newest == nullptr || v.order() > newest->order())) newest = &v;
            for (int k = 0; k < dsp::numSampleSlots; ++k)
            {
                const auto* d = (*samplePtrs)[(size_t) k].load (std::memory_order_acquire);
                const double p = newest != nullptr && d != nullptr && d->n > 0 ? newest->samplePos (k) : -1.0;
                smpPosMon[(size_t) k].store (p < 0.0 ? -1.f : (float) (p / (double) d->n), std::memory_order_relaxed);
            }
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
            float velGain = 1.f - cfg.ks.velSens * (1.f - juce::jlimit (0.f, 1.f, velocity));
            // KICK VARIATION - HUMANIZE (owner 2026-11-20): a small, seeded difference per hit (pitch +-12 cents, level +-0.8 dB at 100 %)
            cfg.humanPitch = 1.f;
            if (const float hz = frame[P::kvHumanize]; hz > 0.f)
            {
                auto rnd = [] (uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return (float) (x & 0xffffff) / 8388607.5f - 1.f; };
                const uint32_t h = (uint32_t) counter * 0x9E3779B1u + cfg.seedSalt * 0x85EBCA77u + 0x27d4eb2fu;
                cfg.humanPitch = std::exp2 (hz * 12.f * rnd (h) / 1200.f);
                velGain *= juce::Decibels::decibelsToGain (hz * 0.8f * rnd (h ^ 0x5bd1e995u));
            }
            slot->trigger (note, velGain, cfg, ph, ++counter, legatoMs, clockSec, (uint32_t) counter + cfg.seedSalt * 0x9E3779B1u);
            shaper.hit();
            tailRoom->clear(); tokRoom->clear(); pvcU.reset(); zaagU.reset(); hcU.reset(); rawU.reset(); expU.resetFor (cfg.exp); expU.seedFor (note, cfg.exp.fixedSeed, (uint32_t) counter); zpU.reset(); cpU.reset(); rpU.reset(); riseU.reset(); clkU.reset(); tokU.reset(); spkU.reset(); crU.reset(); sinceHitMs = 0.f; sinceHitOs = 0; lastVelocity = juce::jlimit (0.f, 1.f, velocity);
            lastNoteHz = 440.f * std::exp2 ((float) (note - 69) / 12.f); relMs = -1.f;
            scrGate = 1.f; scrCut = false; outGate = 1.f; outCut = false;
            if (cfg.scr.on)                                                   // module 06: where the screech sits in this kick
            {
                const auto& ks = cfg.ks;
                const auto t = dsp::scr::resolve (cfg.scr, cfg.partEnd[dsp::PBody], ks.part[dsp::PBody].env.lengthMs, cfg.amp[dsp::PTail].delayMs,
                                                  ks.part[dsp::PTail].env.lengthMs, ks.lengthMs, tempo());
                scrEng.trigger (cfg.scr, t, (0x51f15eedu ^ (uint32_t) counter) + cfg.seedSalt * 0x632BE5ABu, cfg.ks.rate);
                scrTiming = t;
            }
            hits.fetch_add (1);
        }
        // NOTE-OFF (owner 2026-11-16 "the tail sometimes vanishes by itself"): a note-off of the PREVIOUS kick that arrives after the next
        // note-on (overlapping notes in the piano roll) used to release EVERY voice - the new kick's envelopes / KICK END too. Now a key
        // counts its note-ons: a note is released only when its last key goes up, and only the voices of that note.
        void noteOnKey (int note)
        {
            if (note < 0 || note >= 128) return;
            bool sounding = false; for (auto& v : voices) if (v.isHeld() && v.note() == note) sounding = true;
            if (! sounding) keyDown[(size_t) note] = 0;                      // (a lost note-off never leaves a key stuck)
            ++keyDown[(size_t) note];
        }
        void release (int note)
        {
            if (note >= 0 && note < 128)
            {
                if (keyDown[(size_t) note] > 1) { --keyDown[(size_t) note]; return; }   // another note-on of this key is still held
                keyDown[(size_t) note] = 0;
            }
            KickVoice* newest = nullptr; bool any = false;
            for (auto& v : voices) if (v.isActive() && (newest == nullptr || v.order() > newest->order())) newest = &v;
            for (auto& v : voices)
                if (v.isHeld() && (v.note() == note || note < 0))
                {
                    any = true; v.noteOff (cfg);
                    if (cfg.noteCut) v.choke (5.f, cfg.ks.rate);                 // NOTE END CUTS: the kick stops with the note
                }
            if (! any || newest == nullptr || (newest->note() != note && note >= 0)) return;   // an older kick's note: the current kick keeps playing
            if (cfg.noteCut) scrCut = true;
            if (cfg.noteCutFx) outCut = true;                                // NOTE + FX: the whole output fades too
            if (relMs < 0.f) relMs = sinceHitMs;
            modEng.onNoteOff();
        }
        std::array<int, 128> keyDown {};
        void stopAll() { for (auto& v : voices) v.choke (3.f, cfg.ks.rate); keyDown.fill (0); }
        void cookOnly (const ParamFrame& f) { cook (f); }

        // meters (peak since the GUI last read them): PUNCH / BODY / TAIL / SUB and the output
        std::array<std::atomic<float>, dsp::NumParts> partPeak {};
        std::atomic<float> outPeak { 0.f };
        std::atomic<int> hits { 0 };
        const KickConfig& config() const { return cfg; }
        double tempo() const { return hostBpm > 0.0 ? hostBpm : (double) frame[P::envLocalBpm]; }

    private:
        // ---------------------------------------------------------- module 09: the block in control ticks of 32 samples (+ a tick at every kick)
        void processMod (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const ParamFrame& base)
        {
            const int n = out.getNumSamples();
            // the control ticks run on their own grid (every 32 samples, restarted at each kick) - not on the host blocks,
            // so the same kick sounds the same at any position / block size (realtime = export). A changed BASE re-cooks at once.
            bool full = modFresh || std::memcmp (base.v.data(), lastBase.v.data(), sizeof (float) * (size_t) P::count) != 0;
            if (full) lastBase = base;
            double ppq = hostPpq;
            auto tick = [&] (int adv)
            {
                dsp::mod::Ctx cx = modCtx();
                cx.ppq = ppq; cx.playing = hostPlaying; cx.barBeats = hostBar;
                if (modFresh) { modEng.setMatrixFrame (base); modFresh = false; }
                const uint32_t moved = modEng.tick (*modData, base, fm, cx, adv);
                cook (fm, full || modFullCook ? dsp::mod::GAll : (moved | modExtraCook));
                full = false;
            };
            auto it = midi.begin();
            int pos = 0;
            const double beatsPerSample = tempo() / 60.0 / sr;
            while (pos < n || it != midi.end())
            {
                bool kicked = false;
                while (it != midi.end() && (*it).samplePosition <= pos)
                {
                    const auto m = (*it).getMessage();
                    if (m.isNoteOn()) { noteOnKey (m.getNoteNumber()); modEng.onNoteOn (heldNotes == 0); ++heldNotes; lastKey = m.getNoteNumber(); lastVel = m.getFloatVelocity(); kicked = true; pendingNotes[(size_t) juce::jmin (15, pendingN++)] = { m.getNoteNumber(), m.getFloatVelocity() }; }
                    else if (m.isNoteOff()) { heldNotes = juce::jmax (0, heldNotes - 1); release (m.getNoteNumber()); }
                    else if (m.isController())
                        for (int c = 0; c < 4; ++c)
                            if (m.getControllerNumber() == (int) std::lround (base[P::cc1Num + c * (P::cc2Num - P::cc1Num)])) ccVal[(size_t) c] = (float) m.getControllerValue() / 127.f;
                    ++it;
                }
                if (kicked || full || sinceTick >= dsp::mod::ctlN) { tick (sinceTick); sinceTick = 0; }
                for (int k = 0; k < juce::jmin (16, pendingN); ++k) trigger (pendingNotes[(size_t) k].first, pendingNotes[(size_t) k].second);
                pendingN = 0;
                if (pos >= n) break;
                int end = juce::jmin (n, pos + (dsp::mod::ctlN - sinceTick));
                if (it != midi.end()) end = juce::jmin (end, juce::jmax (pos + 1, (*it).samplePosition));
                render (out, pos, end - pos);
                sinceTick += end - pos;
                ppq += (end - pos) * beatsPerSample;
                pos = end;
            }
            hostPpq = ppq;
            for (int k = 0; k < dsp::mod::NumSrc; ++k) srcMon[(size_t) k].store (modEng.source (k), std::memory_order_relaxed);
            for (int k = 0; k < modData->active; ++k) slotFinal[(size_t) k].store (modEng.finalNorm (modData->slot[(size_t) k].dst), std::memory_order_relaxed);
            seqStepMon.store (modEng.seqStep(), std::memory_order_relaxed);
        }
        dsp::mod::Ctx modCtx() const
        {
            dsp::mod::Ctx cx;
            cx.bpm = tempo(); cx.velocity = lastVel; cx.follower = folOut; cx.cc = ccVal; cx.key = juce::jlimit (-1.f, 1.f, (float) (lastKey - 60) / 24.f);
            const KickVoice* newest = nullptr;
            for (auto& v : voices) if (v.isActive() && (newest == nullptr || v.order() > newest->order())) newest = &v;
            if (newest != nullptr)
            {
                float k = 0.f;
                for (int p = 0; p < 4; ++p) { cx.amp[(size_t) p] = newest->srcAmp[(size_t) p]; k = juce::jmax (k, newest->srcAmp[(size_t) p]); }
                cx.amp[4] = k; cx.pitchEnv = newest->srcPitch; cx.filtEnv = newest->srcFilt;
            }
            return cx;
        }
        // AUDIO FOLLOWER: the envelope of one part bus (oversampled rate), 0..1 above its threshold over its range
        void follow (float* const* b, int on)
        {
            const auto& f = frame;
            const double r = cfg.ks.rate;
            const float aC = 1.f - std::exp (-1.f / (float) (f[P::folAttack] * 0.001 * r)), rC = 1.f - std::exp (-1.f / (float) (f[P::folRelease] * 0.001 * r));
            const float g = std::pow (10.f, f[P::folGain] / 20.f);
            const int src = f.idx (P::folSource), det = f.idx (P::folDetect);
            for (int i = 0; i < on; ++i)
            {
                float x;
                switch (src)
                {
                    case 1: x = b[0][i]; break;
                    case 2: x = b[1][i]; break;
                    case 3: x = b[2][i] + 0.5f * (b[4][i] + b[5][i]); break;
                    case 4: x = b[3][i]; break;
                    case 5: x = 0.5f * (b[6 + 2 * dsp::BusTexture][i] + b[7 + 2 * dsp::BusTexture][i]); break;
                    default: x = b[0][i] + b[1][i] + b[2][i] + b[3][i] + 0.5f * (b[4][i] + b[5][i]); break;
                }
                x = std::abs (x) * g;
                folMs += (x * x - folMs) * rC;                                    // (RMS: a mean-square window = the release time)
                const float lvl = det == 0 ? x : det == 1 ? std::sqrt (folMs) * 1.4142f : 0.5f * (x + std::sqrt (folMs) * 1.4142f);
                folEnv += (lvl - folEnv) * (lvl > folEnv ? aC : rC);
            }
            const float db = folEnv > 1.0e-6f ? 20.f * std::log10 (folEnv) : -120.f;
            float v = juce::jlimit (0.f, 1.f, (db - f[P::folThresh]) / juce::jmax (1.f, f[P::folRange]));
            if (v < f[P::folSens]) v = 0.f;                                       // the GATE: silence stays 0
            const float sm = f[P::folSmooth];
            folOut = sm < 0.5f ? v : folOut + (1.f - std::exp (-(float) on / (float) (sm * 0.001 * r))) * (v - folOut);
            if (! std::isfinite (folOut)) folOut = folEnv = folMs = 0.f;
        }
        float folEnv = 0.f, folMs = 0.f, folOut = 0.f;
        std::array<float, 4> ccVal {};
        dsp::mod::Engine modEng;
        const SamplePtrs* samplePtrs = nullptr;
        const dsp::mod::Data* modData = nullptr;
        uint32_t modVersion = 0;
        ParamFrame fm = ParamFrame::defaults(), lastBase = ParamFrame::defaults();
        int sinceTick = 0, heldNotes = 0, lastKey = 60, hostBar = 4, pendingN = 0;
        float lastVel = 1.f;
        double hostPpq = 0.0;
        bool hostPlaying = false, modFresh = true;
        std::array<std::pair<int, float>, 16> pendingNotes {};

        // ---------------------------------------------------------- per block: parameters -> cooked settings
        // the cooked sections (module 09 re-cooks only the sections a modulation moved; GCore also re-cooks oscillators + layers)
        void cookSubs (const ParamFrame& f, uint32_t mask, const env::Bank& b)
        {
            if (mask & dsp::mod::GOsc) cookOscillators (f);
            if (mask & dsp::mod::GLayers) cookLayers (f, b);
            if (mask & dsp::mod::GDist) dsp::dist::cookDist (f, distData, b, cfg.dist, distTables, cfg.ks.rate, offline);
            if (mask & dsp::mod::GM06) { dsp::m06::cookBank (f, b, cfg.m06, cfg.ks.rate); dsp::scr::cook (f, cfg.scr); }
            if (mask & dsp::mod::GTone) { dsp::tone::cookBank (f, cfg.tone, cfg.ks.rate); dsp::spatial::cookBank (f, cfg.spc, cfg.ks.rate); }
            if (mask & dsp::mod::GDyn) dsp::dyn::cookBank (f, distData, cfg.dyn, cfg.ks.rate, sr);
        }
        void cook (const ParamFrame& f, uint32_t mask = dsp::mod::GAll)
        {
            using namespace dsp;
            static const env::Bank defaultBank;
            if ((mask & mod::GCore) == 0) { frame = f; cookSubs (f, mask, bank != nullptr ? *bank : defaultBank); return; }
            frame = f;
            static const env::Bank defaults;
            const env::Bank& b = bank != nullptr ? *bank : defaults;
            auto& ks = cfg.ks;
            const double rate = sr * osFactor;
            ks.rate = rate; ks.inv = (float) (1.0 / rate);
            ks.dcR = (float) std::exp (-juce::MathConstants<double>::twoPi * 5.0 / rate);
            ks.dcTail = (float) std::exp (-juce::MathConstants<double>::twoPi * 10.0 / rate);
            ks.avgC = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 8.0 / rate));
            ks.lengthMs = kickLengthMs (f, tempo());                          // module 14: the LENGTH knob or LENGTH SYNC
            cfg.zaag.lengthMs = f.on (P::zgLenFollow) ? ks.lengthMs : juce::jmin (ks.lengthMs, f[P::zgLength]);   // ZAAG LENGTH: follows the kick (never ends between kicks)
            ks.phaseReset = f.on (P::corePhaseRst);
            ks.velSens = f[P::coreVelocity];
            cfg.keyTrack = f.on (P::coreKeyTrack);
            // PUNCH / TOK: the tail starts its drop FROM THE TOK NOTE (no pitch jump at the hand-over, owner 2026-11-10 "słychać przejście")
            const bool tokM = f.idx (P::punchMode) == 1;
            cfg.startFixed = tokM ? f.idx (P::punchNoteMode) == 0 : f.idx (P::peStartMode) == 1;
            cfg.startHz = tokM ? (f.idx (P::punchNoteMode) == 2 ? f[P::peEnd] * std::exp2 (f[P::punchNoteOffset] / 12.f) : f[P::punchPitch]) : f[P::peStart]; cfg.startAmountSt = f[P::peAmount]; cfg.tailPAmt = f[P::tailPAmount];
            cfg.endHz = f[P::peEnd]; cfg.fineMul = std::exp2 (f[P::peFine] / 1200.f); cfg.subHz = f[P::subPitch];
            // global time (module 02, 9): TIME SCALE, OFFSET; SYNC = times designed at 150 BPM follow the host tempo
            const float gScale = f[P::envTimeScale], gOffset = f[P::envTimeOffset];
            const float syncF = f.on (P::envSync) ? (float) (150.0 / juce::jlimit (30.0, 400.0, tempo())) : 1.f;

            // AUTO LENGTH: the punch keeps its short attack, the body follows a little, tail and sub follow fully
            const float sc = f.on (P::coreAutoLen) ? ks.lengthMs / 450.f : 1.f;
            // module 14: how much each part follows (default: punch 0, body 0.5 = square root, tail / sub 1)
            auto follow = [sc] (float k) { return k <= 0.f ? 1.f : k >= 1.f ? sc : k == 0.5f ? std::sqrt (sc) : std::pow (sc, k); };
            const float scPart[4] { follow (f[P::lenFollowPunch]), follow (f[P::lenFollowBody]), follow (f[P::lenFollowTail]), follow (f[P::lenFollowSub]) };
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
            pu.trLevel = f[P::punchTrLevel]; pu.trLenMs = f[P::punchTrLength];
            pu.trTone = svfCoefs (f[P::punchTrTone], 1.2f, rate);
            cfg.tokMode = f.idx (P::punchMode) == 1; cfg.tokFixed = f.idx (P::punchNoteMode) == 0; cfg.tokHz = f[P::punchPitch];
            cfg.tokNoteMode = f.idx (P::punchNoteMode); cfg.tokOffsetMul = std::exp2 (f[P::punchNoteOffset] / 12.f);
            cfg.tokDecayDb = f[P::punchTokDecay]; cfg.tokBendSt = f[P::punchTokBend]; cfg.tokGlideMs = f[P::punchGlide];
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
            cfg.room.mix = f[P::tailRoom]; cfg.room.on = cfg.room.mix > 0.001f;
            cfg.room.t60Ms = f[P::tailRoomDecay]; cfg.room.tone = f[P::tailRoomTone];
            cfg.color.gainDb = f[P::tailColorGain]; cfg.color.on = std::abs (cfg.color.gainDb) > 0.05f;
            cfg.color.freq = f[P::tailColorFreq]; cfg.color.q = f[P::tailColorQ]; cfg.color.track = f.idx (P::tailColorTrack) == 1; cfg.color.harm = f[P::tailColorHarm];
            cfg.room.delayMs = f[P::tailRoomDelay]; cfg.room.onTok = f.idx (P::tailRoomInput) == 1;
            cfg.wide = tx.uniVoices > 1 && cfg.width > 0.001f;
            cfg.widthSplit = svfCoefs (f[P::tailWidthFrom], 1.414f, rate);
            cfg.clipG = db (f[P::clipDrive]); cfg.clipSoft = f[P::clipSoft];
            cfg.holdGain = db (f[P::tailHoldLvl]);
            cookSubs (f, mask | mod::GOsc | mod::GLayers, b);

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
            cfg.pitch[PTail].depthSt = (12.f * std::log2 (juce::jmax (1.f, cfg.startHz) / juce::jmax (1.f, f[P::peEnd])) + f[P::peAmount]) * f[P::tailPAmount];
            {
                PitchEval tmp; tmp.c = &cfg;
                const float tail0 = f[P::peEnd] * std::exp2 (tmp.semis (PTail, 0.f) / 12.f);
                cfg.pitch[PPunch].depthSt = 12.f * std::log2 (juce::jmax (1.f, f[P::punchPitch]) / juce::jmax (1.f, tail0)) * f[P::punchPAmount];
                cfg.pitch[PBody].depthSt  = 12.f * std::log2 (juce::jmax (1.f, f[P::bodyPitch]) / juce::jmax (1.f, tail0)) * f[P::bodyPAmount];
            }
            cfg.pitch[PSub].depthSt = f[P::subPDepth] * f[P::subPAmount];
            // PUNCH / TOK -> BODY + TAIL: body and tail (amplitude, filter, drive, resonance and pitch clocks) start when the tok ends
            // (minus the OVERLAP crossfade) - an explicit time relation, not the tail's pitch HOLD (owner 2026-11-09)
            cfg.tailStartMs = 0.f;
            if (cfg.tokMode)
            {
                const float ov = f[P::punchOverlap];
                // TAIL START MODE: AFTER TOK = when the tok ends (minus the crossfade); INDEPENDENT = at TAIL START (may overlap the tok)
                const float t0 = f.idx (P::tailStartMode) == 1 ? f[P::tailStart] * gScale + gOffset
                                                              : juce::jmax (0.f, cfg.partEnd[PPunch] - ov);
                cfg.tailStartMs = t0; cfg.tokOverlapMs = ov; cfg.tokEndMs = cfg.partEnd[PPunch];
                for (int p : { (int) PBody, (int) PTail })                       // BODY and TAIL: two parts, each its own envelopes / character
                {
                    auto& a = cfg.amp[(size_t) p];
                    a.delayMs += t0;                                                 // (the hand-over itself = the equal-power crossfade in render)
                    cfg.partEnd[(size_t) p] += t0;
                    for (auto* e : { &cfg.filter[(size_t) p], &cfg.drive[(size_t) p], &cfg.reso[(size_t) p] }) e->delayMs += t0;
                    cfg.pitch[(size_t) p].offsetMs += t0;
                }
            }
            cfg.holdMs = cfg.pitch[PTail].on ? cfg.pitch[PTail].offsetMs : 0.f;
            // SHAPED PUNCH (owner 2026-11-14): a broadband source -> mid boost -> distortion -> tape stop -> shaper; rendered once per change
            {
                auto& sh = cfg.shaped;
                const int pm = f.idx (P::punchMode);                                  // 2 EUPHORIC (shaped), 3 PVC (owner 2026-11-14)
                sh.on = pm == 2 || pm == 3;
                if (sh.on)
                {
                    sh.source = f.idx (P::pshSource); sh.transposeSt = f[P::pshTranspose]; sh.midHz = f[P::pshMidFreq]; sh.midDb = f[P::pshMidGain];
                    sh.driveDb = f[P::pshDrive]; sh.asym = f[P::pshAsym]; sh.tape = f[P::pshTape]; sh.tapeStart = f[P::pshTapeStart];
                    sh.spikeMs = f[P::pshSpike]; sh.body = f[P::pshBody]; sh.bodyMs = f[P::pshBodyMs]; sh.endMs = f[P::pshEnd];
                    sh.lowCutHz = f[P::pshLowCut]; sh.click = f[P::pshClick]; sh.verb = f[P::pshVerb];
                    sh.sub = f[P::pshSub]; sh.subDelayMs = f[P::pshSubDelay]; sh.tonalOnly = f.on (P::pshTonalOnly);
                    sh.tonal = f[P::pshTonal]; sh.clickMs = f[P::pshClickLen]; sh.clickHz = f[P::pshClickTone];
                    sh.subWave = f.idx (P::pshSubWave); sh.subStartSt = f[P::pshSubStart]; sh.subDropMs = f[P::pshSubDrop]; sh.subDecayMs = f[P::pshSubDecay];
                    float lenMs = 0.f;
                    if (pm == 3)
                    {
                        dsp::pvc::Settings ps;
                        ps.source = f.idx (P::pvcSource); ps.pitchHz = f[P::pvcPitch]; ps.resHz = f[P::pvcResFreq]; ps.resQ = f[P::pvcResQ];
                        ps.resAmount = f[P::pvcResAmount]; ps.material = f.idx (P::pvcMaterial); ps.mix = f[P::pvcMix]; ps.ringRatio = f[P::pvcRingRatio];
                        ps.comp = f[P::pvcComp]; ps.driveDb = f[P::pvcDrive]; ps.sat = f[P::pvcSat]; ps.asym = f[P::pvcAsym];
                        ps.lowMidDb = f[P::pvcLowMid]; ps.midDb = f[P::pvcMid]; ps.highMidDb = f[P::pvcHighMid];
                        ps.attackMs = f[P::pvcAttack]; ps.lengthMs = f[P::pvcLength]; ps.decayMs = f[P::pvcDecay];
                        ps.model = f.idx (P::pvcModel); ps.tubeHz = f[P::pvcTubeHz]; ps.comb = f[P::pvcTubeComb]; ps.jumpSt = f[P::pvcJump];
                        ps.dropSt = f[P::pvcDrop]; ps.dropMs = f[P::pvcDropTime]; ps.phaser = f[P::pvcPhaser]; ps.phMode = f.idx (P::pvcPhMode);
                        ps.phRate = f[P::pvcPhRate]; ps.phFb = f[P::pvcPhFb]; ps.brightHz = f[P::pvcBright];
                        ps.tonal = sh.tonal; ps.click = sh.click; ps.clickMs = sh.clickMs; ps.clickHz = sh.clickHz; ps.verb = sh.verb; ps.tonalOnly = sh.tonalOnly;
                        pvcR->render (ps);
                        cfg.shL = pvcR->left(); cfg.shR = pvcR->right(); cfg.shLen = pvcR->length(); lenMs = pvcR->lengthMs();
                    }
                    else
                    {
                        shapedR->render (sh);
                        cfg.shL = shapedR->left(); cfg.shR = shapedR->right(); cfg.shLen = shapedR->length(); lenMs = shapedR->lengthMs();
                    }
                    cfg.shRate = sr;
                    cfg.partEnd[PPunch] = juce::jmax (lenMs, sh.tonalOnly || sh.sub <= 0.f ? 0.f : juce::jmax (sh.subDelayMs + 30.f, sh.subDecayMs));
                }
            }

            {                                                                       // TAIL ENGINE PVC (owner 2026-11-14)
                auto& pt = cfg.pvcTail;
                pt.on = f.idx (P::tailEngine) == 1;
                {                                                                   // TAIL TYPE RAW (owner 2026-11-16)
                    auto& r = cfg.raw;
                    r.on = f.idx (P::tailEngine) == 3;
                    r.osc1 = f.idx (P::rwOsc1); r.osc2 = f.idx (P::rwOsc2); r.noise = f.idx (P::rwNoise);
                    r.osc2Level = f[P::rwOsc2Lvl]; r.osc2St = f[P::rwOsc2St]; r.noiseLevel = f[P::rwNoiseLvl];
                    r.startSt = f[P::rwStart]; r.dropMs = f[P::rwDrop]; r.curve = f[P::rwCurve]; r.rootSt = f[P::rwRoot];
                    r.preOn = f.on (P::rwPreOn); r.d1On = f.on (P::rwD1On); r.shapeOn = f.on (P::rwShapeOn); r.d2On = f.on (P::rwD2On);
                    r.bpOn = f.on (P::rwBpOn); r.d3On = f.on (P::rwD3On); r.postOn = f.on (P::rwPostOn); r.clipOn = f.on (P::rwClipOn);
                    r.lowCut = f[P::rwLowCut]; r.preMidDb = f[P::rwPreMid];
                    r.d1Type = f.idx (P::rwD1Type); r.d2Type = f.idx (P::rwD2Type); r.d3Type = f.idx (P::rwD3Type);
                    r.d1Db = f[P::rwD1Db]; r.d2Db = f[P::rwD2Db]; r.d3Db = f[P::rwD3Db];
                    r.growlHz = f[P::rwGrowlHz]; r.growlDb = f[P::rwGrowlDb]; r.growlQ = f[P::rwGrowlQ];
                    r.postLowCut = f[P::rwPostLowCut]; r.postHighDb = f[P::rwPostHigh]; r.clipHard = f[P::rwClipHard];
                    r.fltShape = f.idx (P::rwFltShape); r.sync = f.idx (P::rwSync);
                    r.fltStart = f[P::rwFltStart]; r.fltEnd = f[P::rwFltEnd]; r.fltMid = f[P::rwFltMid]; r.fltReso = f[P::rwFltReso];
                    r.fltSpeed = f[P::rwFltSpeed]; r.fltTimeMs = f[P::rwFltTime]; r.fltCurve = f[P::rwFltCurve]; r.fltMix = f[P::rwFltMix];
                    r.bodyEndMs = f[P::bodyLen];
                    r.growl = f[P::rwGrowl]; r.crunch = f[P::rwCrunch];
                    r.mDrive = f[P::tlDrive]; r.mTone = f[P::tlTone]; r.mMove = f[P::tlMove]; r.mReso = f[P::tlReso];
                    r.mDensity = f[P::tlDensity]; r.mSpace = f[P::tlSpace]; r.mChaos = f[P::tlChaos]; r.mWidth = f[P::tlWidth];
                    r.verb = f[P::rwVerb]; r.verbSize = f[P::rwVerbSize]; r.verbDecay = f[P::rwVerbDecay]; r.verbDamp = f[P::rwVerbDamp];
                    r.verbPre = f[P::rwVerbPre]; r.verbLowCut = f[P::rwVerbLowCut];
                    r.sub = f[P::rwSub]; r.subHz = f[P::rwSubHz]; r.width = f[P::rwWidth];
                    r.lengthMs = ks.lengthMs; r.attackMs = f[P::rwAttack]; r.body = f[P::rwBody]; r.decay = f[P::rwDecay]; r.fade = f[P::rwFade];
                }
                {                                                                   // TAIL TYPE EXPERIMENTAL (owner 2026-11-16)
                    auto& e = cfg.exp;
                    e.on = f.idx (P::tailEngine) == 4;
                    e.osc = f.idx (P::exOsc); e.wtPos = f[P::exWtPos]; e.oscLevel = f[P::exOscLvl];
                    e.fmLevel = f[P::exFmLvl]; e.fmAmount = f[P::exFmAmt]; e.fmRatio = f[P::exFmRatio];
                    e.noise = f.idx (P::exNoise); e.noiseLevel = f[P::exNoiseLvl]; e.ringRatio = f[P::exRingRatio];
                    e.pitchCurve = f.idx (P::exPCurve); e.startSt = f[P::exStart]; e.endSt = f[P::exEnd]; e.midSt = f[P::exMid]; e.range = f[P::exRange];
                    e.pitchMs = f[P::exPTime]; e.pitchCurveAmt = f[P::exPShape]; e.pmRate = f[P::exPmRate]; e.pmDepth = f[P::exPmDepth]; e.rootSt = f[P::exRoot];
                    { auto& q = e.slot[0]; q.type = f.idx (P::exS1Type); q.mix = f[P::exS1Mix]; q.amount = f[P::exS1Amt]; q.param = f[P::exS1Par]; q.modSrc = f.idx (P::exS1Mod); q.modDepth = f[P::exS1Depth]; }
                    { auto& q = e.slot[1]; q.type = f.idx (P::exS2Type); q.mix = f[P::exS2Mix]; q.amount = f[P::exS2Amt]; q.param = f[P::exS2Par]; q.modSrc = f.idx (P::exS2Mod); q.modDepth = f[P::exS2Depth]; }
                    { auto& q = e.slot[2]; q.type = f.idx (P::exS3Type); q.mix = f[P::exS3Mix]; q.amount = f[P::exS3Amt]; q.param = f[P::exS3Par]; q.modSrc = f.idx (P::exS3Mod); q.modDepth = f[P::exS3Depth]; }
                    { auto& q = e.slot[3]; q.type = f.idx (P::exS4Type); q.mix = f[P::exS4Mix]; q.amount = f[P::exS4Amt]; q.param = f[P::exS4Par]; q.modSrc = f.idx (P::exS4Mod); q.modDepth = f[P::exS4Depth]; }
                    { auto& q = e.slot[4]; q.type = f.idx (P::exS5Type); q.mix = f[P::exS5Mix]; q.amount = f[P::exS5Amt]; q.param = f[P::exS5Par]; q.modSrc = f.idx (P::exS5Mod); q.modDepth = f[P::exS5Depth]; }
                    { auto& q = e.slot[5]; q.type = f.idx (P::exS6Type); q.mix = f[P::exS6Mix]; q.amount = f[P::exS6Amt]; q.param = f[P::exS6Par]; q.modSrc = f.idx (P::exS6Mod); q.modDepth = f[P::exS6Depth]; }
                    { auto& q = e.slot[6]; q.type = f.idx (P::exS7Type); q.mix = f[P::exS7Mix]; q.amount = f[P::exS7Amt]; q.param = f[P::exS7Par]; q.modSrc = f.idx (P::exS7Mod); q.modDepth = f[P::exS7Depth]; }
                    { auto& q = e.slot[7]; q.type = f.idx (P::exS8Type); q.mix = f[P::exS8Mix]; q.amount = f[P::exS8Amt]; q.param = f[P::exS8Par]; q.modSrc = f.idx (P::exS8Mod); q.modDepth = f[P::exS8Depth]; }
                    e.lfo1Rate = f[P::exLfo1]; e.lfo2Rate = f[P::exLfo2]; e.rndRate = f[P::exRndRate];
                    e.curve0 = f[P::exCurve0]; e.curve1 = f[P::exCurve1]; e.curve2 = f[P::exCurve2]; e.fixedSeed = f.idx (P::exSeed) == 0;
                    e.fmMod = f[P::exFmMod]; e.fsMod = f[P::exFsMod];
                    e.mDrive = f[P::tlDrive]; e.mTone = f[P::tlTone]; e.mMove = f[P::tlMove]; e.mReso = f[P::tlReso];
                    e.mDensity = f[P::tlDensity]; e.mSpace = f[P::tlSpace]; e.mChaos = f[P::tlChaos]; e.mWidth = f[P::tlWidth];
                    e.sub = f[P::exSub]; e.subHz = f[P::exSubHz]; e.width = f[P::exWidth]; e.drive = f[P::exDrive];
                    e.lengthMs = ks.lengthMs; e.attackMs = f[P::exAttack]; e.body = f[P::exBody]; e.decay = f[P::exDecay]; e.fade = f[P::exFade];
                }
                {                                                                   // TAIL TYPE HARDCORE (owner spec 2026-11-24): GABBER / MILLENNIUM / HARDCORE / UPTEMPO = profiles
                    auto& h = cfg.hc;
                    h.on = f.idx (P::tailEngine) == 5;
                    h.source = f.idx (P::hcSrc); h.srcMix = f[P::hcSrcMix];
                    h.dropSt = f[P::hcDrop]; h.timeMs = f[P::hcTime]; h.curve = f.idx (P::hcCurve); h.customCurve = f[P::hcCurveAmt];
                    h.keyMode = f.idx (P::hcKeyMode); h.fixedNote = f[P::hcRootNote]; h.endSt = f[P::hcEnd]; h.fineCt = f[P::hcFine];
                    h.punchMs = f[P::hcPunchLen]; h.bodyMs = f[P::hcBodyLen]; h.punchDb = f[P::hcPunchDb]; h.bodyDb = f[P::hcBodyDb]; h.tailDb = f[P::hcTailDb];
                    h.tailDecay = f[P::hcTailDecay]; h.attackMs = f[P::hcAttack];
                    h.crOn = f.on (P::hcCrOn); h.crSrc = f.idx (P::hcCrSrc); h.crStartMs = f[P::hcCrStart]; h.crLenMs = f[P::hcCrLen];
                    h.crPitchSt = f[P::hcCrPitch]; h.crPitchMode = f.idx (P::hcCrPMode); h.crPitchAmt = f[P::hcCrPAmt]; h.crPitchMs = f[P::hcCrPTime];
                    h.crMix = f[P::hcCrMix]; h.crDrive = f[P::hcCrDrive]; h.crAsym = f[P::hcCrAsym];
                    h.txType = f.idx (P::hcTxType); h.txMix = f[P::hcTxMix]; h.txLenMs = f[P::hcTxLen];
                    h.eqLowDb = f[P::hcEqLowDb]; h.eqLowHz = f[P::hcEqLowHz]; h.eqLmDb = f[P::hcEqLmDb]; h.eqLmHz = f[P::hcEqLmHz]; h.eqMidDb = f[P::hcEqMidDb]; h.eqMidHz = f[P::hcEqMidHz];
                    h.eqHmDb = f[P::hcEqHmDb]; h.eqHmHz = f[P::hcEqHmHz]; h.eqHighDb = f[P::hcEqHighDb]; h.eqHighHz = f[P::hcEqHighHz];
                    h.d1Db = f[P::hcD1]; h.betweenHz = f[P::hcBtwHz]; h.betweenDb = f[P::hcBtwDb]; h.d2Db = f[P::hcD2]; h.asym = f[P::hcAsym]; h.bias = f[P::hcBias]; h.asymPosDb = f[P::hcAsymPos]; h.asymNegDb = f[P::hcAsymNeg];
                    h.holdMs = f[P::hcHold]; h.dropXSt = f[P::hcDropX]; h.msDip = f[P::hcMsDip]; h.msDipAtMs = f[P::hcMsDipAt]; h.msDipLenMs = f[P::hcMsDipLen]; h.msClick = f[P::hcMsClick]; h.msOwn = f.on (P::hcMsOwn); h.clip2Db = f[P::hcClip2]; h.lpHz = f[P::hcLp]; h.fuzzBias = f[P::hcFuzz]; h.harmDb = f[P::hcHarmDb]; h.harmN = f.idx (P::hcHarmN); h.compAmt = f[P::hcComp]; h.compAtkMs = f[P::hcCompAtk]; h.compRelMs = f[P::hcCompRel]; h.clickLenMs = f[P::hcClickLen]; h.clickNoise = f[P::hcClickNoise]; h.clickBal = f[P::hcClickBal]; h.preDecay = f[P::hcPreDecay];
                    h.clipMode = f.idx (P::hcClipMode); h.clipDb = f[P::hcClip]; h.resample = f[P::hcResample]; h.stages = f.idx (P::hcStages) + 1;
                    h.postLowCut = f[P::hcPostLowCut]; h.postBodyDb = f[P::hcPostBody]; h.postMidDb = f[P::hcPostMid];
                    h.postCrunchDb = f[P::hcPostCrunch]; h.postCrunchHz = f[P::hcPostCrunchHz]; h.postHighDb = f[P::hcPostHigh];
                    h.msMix = f[P::hcMsMix]; h.msTrack = f.idx (P::hcMsTrack); h.msHz = f[P::hcMsHz]; h.msHarm = f[P::hcMsHarm]; h.msWave = f.idx (P::hcMsWave);
                    h.msSweepSt = f[P::hcMsSweep]; h.msSweepMs = f[P::hcMsSweepMs]; h.msStartMs = f[P::hcMsStart]; h.msFadeMs = f[P::hcMsFade];
                    h.msMotion = f[P::hcMsMotion]; h.msRate = f[P::hcMsRate]; h.outClipDb = f[P::hcOutClip];
                    // LENGTH: the KICK LENGTH, or capped by the BPM GRID (x FILL) so the kick never blocks the next one (uptempo)
                    h.lengthMs = ks.lengthMs;
                    if (f.idx (P::hcLenMode) == 1)
                    {
                        static constexpr double notes[] { 0.25, 0.5, 1.0, 2.0 };       // 1/16 .. 1/2 in beats
                        const double gridMs = 60000.0 / juce::jlimit (30.0, 400.0, tempo()) * notes[juce::jlimit (0, 3, f.idx (P::hcGrid))];
                        h.lengthMs = juce::jmin (ks.lengthMs, (float) (gridMs * f[P::hcFill]));
                    }
                    // BURST MODE (MIDI / automation, never random): SHORT / PITCHED / HEAVY, or by the note velocity
                    int burst = f.idx (P::hcBurst);
                    if (burst == 4) burst = lastVelocity < 0.4f ? 0 : lastVelocity < 0.6f ? 1 : lastVelocity < 0.8f ? 2 : 3;
                    if (burst == 1) { h.lengthMs *= 0.5f; h.bodyMs *= 0.6f; h.crLenMs *= 0.6f; }
                    else if (burst == 2) { h.endSt += 5.f; h.dropSt += 6.f; }
                    else if (burst == 3) { h.d2Db += 6.f; h.bodyDb += 3.f; h.asym = juce::jmin (1.f, h.asym + 0.2f); }
                }
                auto& z = cfg.zaag;                                                 // TAIL ENGINE ZAAG
                z.on = f.idx (P::tailEngine) == 2;
                z.variant = f.idx (P::zgVariant);
                z.highSaw = f[P::zgSawbLevel]; z.startSt = f[P::zgStart]; z.dropMs = f[P::zgDrop]; z.curve = f[P::zgCurve]; z.rootSt = f[P::zgRoot];
                z.lowCtrl = f[P::zgLowCtrl]; z.fundDb = f[P::zgS1Fund]; z.midHz = f[P::zgS1MidHz]; z.midDb = f[P::zgS1Mid]; z.lowCut = f[P::zgS1LowCut];
                z.d1Db = f[P::zgS1Drive]; z.d1Asym = f[P::zgS1Asym]; z.d1Hard = f[P::zgS1Curve]; z.d1Mix = f[P::zgS1Mix];
                z.shelfDb = f[P::zgEqmShelf]; z.motionDepth = f[P::zgEqmDepth]; z.motionTime = f[P::zgEqmTime]; z.d2Db = f[P::zgS2Drive];
                z.swDb = f[P::zgPeak]; z.swQ = f[P::zgPeakQ]; z.swStart = f[P::zgSwStart]; z.swPlateau = f[P::zgSwPlateau]; z.swEnd = f[P::zgSwEnd];
                z.swHold = f[P::zgSwHold]; z.swReopen = f[P::zgSwReopen];
                z.rsnMode = f.idx (P::zgRsnMode); z.rsnPos = f.idx (P::zgRsnPos); z.rsnFreq = f[P::zgRsnFreq]; z.rsnReso = f[P::zgRsnReso];
                z.rsnWidth = f[P::zgRsnWidth]; z.rsnMix = f[P::zgRsnMix];
                z.d3Type = f.idx (P::zgS3Type); z.d3Db = f[P::zgDrive]; z.d3Preamp = f[P::zgS3Preamp]; z.d3Asym = f[P::zgS3Asym]; z.d3Mix = f[P::zgS3Mix];
                z.distEnv = f[P::zgDistEnv]; z.dirt = f[P::zgDirt]; z.dirtHz = f[P::zgDirtTone];
                z.subPush = f[P::zgSquash]; z.lmCut = f[P::zgLmCut]; z.highEnh = f[P::zgFinalTone]; z.press = f[P::zgPress]; z.knee = f[P::zgKnee];
                z.sideCut = f[P::zgSideCut]; z.sideBoostHz = f[P::zgSideBoostHz]; z.sideBoost = f[P::zgSideBoost]; z.width = f[P::zgWidth]; z.hverb = f[P::zgHverb];
                z.subSrc = f.idx (P::zgSubSrc); z.sub = f[P::zgSub]; z.subFreq = f[P::zgSubFreq]; z.subDecay = f[P::zgSubDecay];
                z.zaag = f[P::zgZaag]; z.drive = f[P::zgGrit]; z.resonance = f[P::zgResonance]; z.movement = f[P::zgMovement]; z.bass = f[P::zgBass];
                z.stereo = f[P::zgStereo]; z.crush = f[P::zgCrush]; z.character = f[P::zgCharacter];
                z.attackMs = f[P::zgAttack]; z.body = f[P::zgBody]; z.decay = f[P::zgDecay]; z.fade = f[P::zgFade];
                {   // FRONT OF THE KICK (owner 2026-11-16): TOK, CLICK, SUB PUNCH - shared modules; the PUNCH TYPE profile changes how they are used
                    const auto& pf = dsp::front::profileFor (f.idx (P::punchMode));
                    auto dbGain = [] (float db) { return db <= -59.9f ? 0.f : std::pow (10.f, db / 20.f); };
                    auto& tk = cfg.tok; // AUTO / ON / OFF; AUTO click / sub punch only if the PUNCH TYPE wants the GLOBAL layers (EUPHORIC / PVC / ZAAG have their own)
                    auto layerOn = [&] (int mode, int autoOn, bool profileWants = true) { const int m = f.idx (mode); return m == 1 || (m == 0 && f[autoOn] > 0.5f && profileWants); };   // AUTO / ON / OFF
                    tk.gain = dbGain (f[P::tkLevel]); tk.on = tk.gain > 0.f && layerOn (P::tkMode, P::tkAuto); tk.type = f.idx (P::tkType); tk.tuneSt = f[P::tkTune];
                    tk.lengthMs = f[P::tkLength]; tk.peakMs = f[P::tkPeak]; tk.fade = f[P::tkFade]; tk.tone = f[P::tkTone];
                    tk.pitchMode = f.idx (P::tkPitchMode); tk.penvStartSt = f[P::tkPenvStart]; tk.penvEndSt = f[P::tkPenvEnd]; tk.penvMs = f[P::tkPenvTime]; tk.drive = f[P::tkDrive];
                    auto& ck = cfg.click; const float cdb = f[P::clkLevel];
                    ck.gain = cdb <= -59.9f ? 0.f : dbGain (cdb + pf.clickDb); ck.on = ck.gain > 0.f && layerOn (P::clkMode, P::clkAuto, pf.globalInAuto); ck.type = f.idx (P::clkType); ck.pitchHz = f[P::clkPitch];
                    ck.lengthMs = f[P::clkLength] * pf.clickLenMul; ck.decay = f[P::clkDecay];
                    ck.tone = juce::jlimit (0.f, 1.f, f[P::clkTone] + pf.clickTone); ck.drive = juce::jlimit (0.f, 1.f, f[P::clkDrive] + pf.clickDrive); ck.mix = f[P::clkMix];
                    auto& sp = cfg.spk; const float sdb = f[P::spkLevel];
                    sp.gain = sdb <= -59.9f ? 0.f : dbGain (sdb + pf.subDb); sp.on = sp.gain > 0.f && layerOn (P::spkMode, P::spkAuto, pf.subInAuto); sp.startHz = f[P::spkStart]; sp.endHz = f[P::spkEnd];
                    sp.delayMs = f[P::spkDelay]; sp.lengthMs = f[P::spkLength] * pf.subLenMul;
                    sp.drive = juce::jlimit (0.f, 1.f, f[P::spkDrive] + pf.subDrive); sp.tone = juce::jlimit (0.f, 1.f, f[P::spkTone] + pf.subTone);
                    cfg.zgSend = f.idx (P::tailEngine) == 2 ? f[P::zgSend] : 0.f;      // (only into a ZAAG tail)
                    auto& cr = cfg.crack;                                             // RAW PUNCH EXTENSION: CRACK + SNAP + BODY COUPLING (only with PUNCH TYPE RAW)
                    cr.on = f.on (P::ckOn) && f.idx (P::punchMode) == 6 && f[P::ckLevel] > -59.9f;
                    cr.gain = dbGain (f[P::ckLevel]); cr.cracking = f.idx (P::rpType) == 1;
                    cr.snap = f[P::ckSnap]; cr.crack = f[P::ckCrack]; cr.density = f[P::ckDensity]; cr.edge = f[P::ckEdge]; cr.coupling = f[P::ckCoupling];
                    cr.snapLenMs = f[P::ckSnapLen]; cr.snapTone = f[P::ckSnapTone]; cr.snapBody = f[P::ckSnapBody]; cr.snapEdge = f[P::ckSnapEdge];
                    cr.pitchMode = f.idx (P::ckPitchMode); cr.pitchSt = f[P::ckPitch]; cr.fixedHz = f[P::ckToneHz]; cr.pitchMod = f[P::ckPitchMod];
                    cr.srcPunch = f[P::ckSrcPunch]; cr.srcTrans = f[P::ckSrcTrans]; cr.srcNoise = f[P::ckSrcNoise];
                    cr.attackMs = f[P::ckAttack]; cr.decayMs = f[P::ckDecay]; cr.sustain = f[P::ckSustain]; cr.releaseMs = f[P::ckRelease]; cr.lengthMs = f[P::ckLength];
                    cr.driveDb = f[P::ckDrive]; cr.shape = f.idx (P::ckShape); cr.clip = f[P::ckClip]; cr.lowDb = f[P::ckLow]; cr.midDb = f[P::ckMid]; cr.highDb = f[P::ckHigh];
                    cr.bodyStartMs = f[P::bodyAmpDelay] + f[P::bodyAttack];
                    cr.model = f.idx (P::ckModel); cr.source = f.idx (P::ckSource); cr.ws = f.idx (P::ckWs);          // TRANSIENT CRACK (owner REWORK)
                    cr.raw = f[P::ckRaw]; cr.position = f[P::ckPosition]; cr.width = f[P::ckWidth]; cr.pitchOff = f[P::ckPitchOff]; cr.pitchEnv = f[P::ckPitchEnv];
                    cr.asym = f[P::ckAsym]; cr.bpHz = f[P::ckBpHz]; cr.bpWidth = f[P::ckBpWidth]; cr.bpRes = f[P::ckBpRes]; cr.cbody = f[P::ckBody];
                    cr.punchAmt = f[P::punchLevel] <= -59.9f ? 0.f : juce::jlimit (0.f, 1.25f, dbGain (f[P::punchLevel]));   // CRACK follows the PUNCH
                    cr.listen = f.idx (P::ckListen);
                    cfg.crackSolo = cr.on && cr.model == 0 && cr.listen != 0;                          // SOLO SOURCE / DRY / WET / REVERB / FINAL: only the crack
                    cr.rvOn = f.on (P::ckRvOn); cr.rvRoute = f.idx (P::ckRvRoute); cr.rvMix = f[P::ckRvMix]; cr.rvSize = f[P::ckRvSize];   // CRACK REVERB
                    cr.rvDecayMs = f[P::ckRvDecay]; cr.rvPreMs = f[P::ckRvPre]; cr.rvEarly = f[P::ckRvEarly]; cr.rvLate = f[P::ckRvLate];
                    cr.rvDiffusion = f[P::ckRvDiff]; cr.rvDamping = f[P::ckRvDamp]; cr.rvDrive = f[P::ckRvDrive]; cr.rvDriveType = f.idx (P::ckRvDriveType);
                    cr.rvGate = f.on (P::ckRvGate); cr.rvHpHz = f[P::ckRvHp]; cr.rvLpHz = f[P::ckRvLp]; cr.rvRes = f[P::ckRvRes];
                    cr.hiGainDb = f[P::ckHiGain]; cr.hiHz = f[P::ckHiFreq];
                    cr.move.sweep = f[P::ckMvSweep]; cr.move.flange = f[P::ckMvFlange]; cr.move.phaser = f[P::ckMvPhaser];     // CRACK MOVEMENT
                    cr.move.pitchSt = f[P::ckMvPitch]; cr.move.rateHz = f[P::ckMvRate]; cr.move.timeMs = f[P::ckMvTime];
                }
                {                                                                   // PUNCH TYPE CLASSIC (EARLY REWORK, owner 2026-11-16)
                    auto& q = cfg.cpunch;
                    q.on = f.idx (P::punchMode) == 5;
                    q.shape = f[P::cpShape]; q.transLevel = f[P::cpTrans]; q.transMs = f[P::cpTransMs]; q.transHz = f[P::cpTransHz];
                    q.resLevel = f[P::cpRes]; q.resHz = f[P::cpResHz]; q.resQ = f[P::cpResQ]; q.noise = f[P::cpNoise]; q.noiseHz = f[P::cpNoiseHz];
                    q.startHz = f[P::cpStart]; q.hitHz = f[P::cpHit]; q.dropMs = f[P::cpDrop]; q.glideMs = f[P::cpGlide]; q.curve = f[P::cpCurve];
                    q.endSt = f[P::cpEndSt]; q.follow = true;
                    q.lowCut = f[P::cpLowCut]; q.lowMidHz = f[P::cpLmHz]; q.lowMidDb = f[P::cpLmDb]; q.midHz = f[P::cpMidHz]; q.midDb = f[P::cpMidDb]; q.highCut = f[P::cpHighCut];
                    q.satDb = f[P::cpSat]; q.asym = f[P::cpAsym]; q.satDecayMs = f[P::cpSatDecay]; q.wsType = f.idx (P::cpWsType); q.wsDb = f[P::cpWsDb]; q.transCtrl = f[P::cpTransCtrl];
                    q.attackMs = f[P::cpAttack]; q.holdMs = f[P::cpHold]; q.decayMs = f[P::cpDecay]; q.decayCurve = f[P::cpDecayCurve];
                }
                {                                                                   // PUNCH TYPE RAW (owner 2026-11-21, RAW_PUNCH.md): two architectures, own knobs each
                    auto& q = cfg.rpunch;
                    q.on = f.idx (P::punchMode) == 6; q.type = f.idx (P::rpType); q.kickMs = cfg.ks.lengthMs;
                    q.crunchOn = f.on (P::cnOn); q.crunch = f[P::cnCrunch]; q.grit = f[P::cnGrit];   // DYNAMIC DISTORTED CRUNCH
                    q.crunchTone = f[P::cnTone]; q.bodyFollow = f[P::cnFollow];
                    q.crunchOffsetMs = f[P::cnOffset]; q.crunchTube = f[P::cnTube];
                    q.cnSrc = f.idx (P::cnSrc); q.cnSrcMix = f[P::cnSrcMix]; q.cnPitchOff = f[P::cnPitchOff]; q.cnPhaseDeg = f[P::cnPhase];   // RAW CRUNCH spec
                    q.cnAttackMs = f[P::cnAttack]; q.cnDecayMs = f[P::cnDecay]; q.cnLowCut = f[P::cnLowCut]; q.cnBodyLimit = f[P::cnBodyLimit];
                    q.cnMidBoost = f[P::cnMidBoost]; q.cnHighCut = f[P::cnHighCut]; q.cnPreEmph = f[P::cnPreEmph]; q.cnWs = f.idx (P::cnWs);
                    q.cnClip = f[P::cnClip]; q.cnDistMix = f[P::cnDistMix];
                    cfg.crunchSolo = f.on (P::cnSolo) && q.crunchOn && f.idx (P::punchMode) == 6;
                    q.bodyStartMs = f[P::bodyAmpDelay]; q.bodyEndMs = f[P::bodyAmpDelay] + f[P::bodyLen];
                    if (q.type == 0)
                    {
                        q.snap = f[P::rcSnap]; q.crack = f[P::rcCrack]; q.crackDecayMs = f[P::rcCrackDecay]; q.coupling = f[P::rcCoupling];
                        q.startHz = f[P::rcStart]; q.dropMs = f[P::rcDrop]; q.endSt = f[P::rcEnd]; q.decayMs = f[P::rcDecay]; q.driveDb = f[P::rcDrive];
                        q.density = f[P::rcDensity]; q.crackHz = f[P::rcCrackHz];
                    }
                    else
                    {
                        q.snap = f[P::rkSnap]; q.crack = f[P::rkCrack]; q.crackDecayMs = f[P::rkCrackDecay]; q.coupling = f[P::rkCoupling];
                        q.startHz = f[P::rkStart]; q.dropMs = f[P::rkDrop]; q.endSt = f[P::rkEnd]; q.decayMs = f[P::rkDecay]; q.driveDb = f[P::rkDrive];
                        q.density = f[P::rkDensity]; q.edge = f[P::rkEdge]; q.tone = f[P::rkTone]; q.dynamic = f[P::rkDynamic];
                    }
                }
                {                                                                   // TAIL RISE (owner 2026-11-21): the window from the BPM GRID (or the kick length)
                    auto& q = cfg.rise;
                    q.on = f.on (P::rsOn); q.kickMs = cfg.ks.lengthMs;
                    static const double beats[5] { 0.25, 0.5, 1.0, 2.0, 4.0 };
                    const double gridMs = 60000.0 / juce::jlimit (30.0, 400.0, tempo()) * beats[juce::jlimit (0, 4, f.idx (P::rsGrid))];
                    const float span = f.idx (P::rsSync) == 0 ? (float) gridMs : cfg.ks.lengthMs;
                    q.startMs = span * f[P::rsStart]; q.endMs = span;
                    q.curve = f.idx (P::rsCurve); q.release = f.idx (P::rsRelease); q.mode = f.idx (P::rsMode); q.amount = f[P::rsAmount];
                    q.gain = f[P::rsGain]; q.pitch = f[P::rsPitch]; q.drive = f[P::rsDrive]; q.harm = f[P::rsHarm]; q.filter = f[P::rsFilter]; q.noise = f[P::rsNoise];
                    q.pitchRangeSt = f[P::rsPitchRange];
                }
                auto& zp = cfg.zpunch;                                              // PUNCH TYPE ZAAG
                zp.on = f.idx (P::punchMode) == 4;
                zp.click = f[P::zgClick]; zp.piep = f[P::zgPiep]; zp.piepHz = f[P::zgPiepHz]; zp.piepMs = f[P::zgPiepMs];
                zp.trask = f[P::zgTrask]; zp.traskDelay = f[P::zgTraskDelay]; zp.traskMs = f[P::zgTraskMs]; zp.traskTone = f[P::zgTraskTone];
                zp.pup = f[P::zgPup]; zp.pupHz = f[P::zgPupHz]; zp.pupMs = f[P::zgPupMs]; zp.pupOct = f[P::zgPupOct]; zp.pupDrive = f[P::zgPupDrive];
                zp.thump = f[P::zgThump]; zp.thumpHz = f[P::zgThumpHz]; zp.thumpMs = f[P::zgThumpMs]; zp.thumpDriveDb = f[P::zgThumpDrive];
                zp.verb = f[P::zgVerb]; zp.verbDecayMs = f[P::zgVerbDecay]; zp.verbSize = f[P::zgVerbSize]; zp.sendMs = f[P::zgVerbSend]; zp.preMs = f[P::zgVerbPre];
                zp.gateMs = f[P::zgVerbGate]; zp.dampHz = f[P::zgVerbDamp]; zp.verbLowCut = f[P::zgVerbLowCut];
                pt.source = f.idx (P::ptSource); pt.startSt = f[P::ptStart]; pt.holdMs = f[P::ptHold]; pt.dropMs = f[P::ptDropTime];
                pt.rootSt = f[P::ptRoot]; pt.slideSt = f[P::ptSlide];
                pt.resHz = f[P::ptResFreq]; pt.resQ = f[P::ptResQ]; pt.resAmt = f[P::ptResAmount];
                pt.revSize = f[P::ptRevSize]; pt.revDecayMs = f[P::ptRevDecay]; pt.preMs = f[P::ptRevPre]; pt.highCut = f[P::ptRevCut]; pt.wet = f[P::ptRevWet];
                pt.comp = f[P::gtComp]; pt.drive1Db = f[P::ptDrive1]; pt.clip = f[P::ptClip]; pt.drive2Db = f[P::ptDrive2]; pt.asym = f[P::ptAsym];
                pt.gateMode = f.idx (P::gtGate); pt.curve = f.idx (P::ptGateCurve); pt.div = f.idx (P::gtGateDiv);
                pt.gateMs = f[P::gtGateLen]; pt.depth = f[P::gtGateDepth]; pt.movement = f[P::ptMovement]; pt.bpm = (float) tempo();
                pt.filterHz = f[P::gtFilter]; pt.reso = f[P::gtReso]; pt.moveHz = f[P::gtMoveRate]; pt.moveDepth = f[P::gtMoveDepth];
                pt.low = f[P::ptLow]; pt.lowMid = f[P::ptLowMid]; pt.mid = f[P::ptMid]; pt.high = f[P::ptHigh];
                pt.monoBelow = f[P::ptMonoBelow]; pt.width = f[P::ptWidth]; pt.side = f[P::ptSide];
                pt.delayMs = f[P::ptDelay]; pt.attackMs = f[P::ptAttack]; pt.lengthMs = f[P::ptLength]; pt.fadeMs = f[P::ptFade]; pt.growDb = f[P::ptGrow];
            }

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
            cfg.masterMute = f.on (P::masterMute); cfg.masterBypass = f.on (P::masterBypass); cfg.outMono = f.idx (P::masterOutMode) == 1;   // module 12
            cfg.safetyClip = f.on (P::masterSafeClip); cfg.safetyLim = f.on (P::masterSafeLim);
            cfg.noteCut = f.idx (P::coreNoteCut) >= 1; cfg.noteCutFx = f.idx (P::coreNoteCut) == 2;
            if (first) { gainNow = gainTarget; masterNow = masterTarget; muteG = cfg.masterMute ? 0.f : 1.f; first = false; }
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
            for (int k = 0; k < dsp::numSampleSlots; ++k)                      // module 10: the audio of the sample slots (lock-free pointers)
                lb.samples[(size_t) k] = samplePtrs != nullptr ? (*samplePtrs)[(size_t) k].load (std::memory_order_acquire) : nullptr;
            cfg.lbAny = lb.anyLayer || lb.anyNoise || lb.click.on;
            float end = 0.f;
            for (const auto& n : lb.noise) if (n.on) end = juce::jmax (end, n.envSrc == NeOwn || n.envSrc == NeDrawn ? n.lengthMs + 5.f : cfg.ks.lengthMs);
            if (lb.click.on) end = juce::jmax (end, lb.click.positionMs + 300.f + lb.click.lenMs + 5.f);
            for (const auto& l : lb.layer)
            {
                if (! l.on) continue;
                float len = l.lenMs;
                if (l.smp.slot >= 0 && l.smp.mode == SmOneShot)                     // a one-shot sample ends with its region
                {
                    const auto* d = lb.samples[(size_t) l.smp.slot];
                    len = d == nullptr ? 0.f : juce::jmin (len, (float) ((double) std::abs (l.smp.end - l.smp.start) * (double) d->n / d->rate * 1000.0
                                                                          / juce::jmax (0.01, (double) l.smp.rate * l.pitchMul * (l.smp.track ? 0.25 : 1.0))) + 10.f);   // (tracking: assume the longest)
                }
                end = juce::jmax (end, l.offsetMs + l.delay + len + (l.envMode == LeAdsr ? l.rel : 0.f) + 5.f);
            }
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
            if (layerTap != nullptr) { layerTap->rate = cfg.ks.rate; layerTap->begin (on); }                 // LAYER ENERGY (offline renders only)
            for (int c = 0; c < numBusCh; ++c) juce::FloatVectorOperations::clear (bus.getWritePointer (c), on);
            float* b[numBusCh];
            for (int c = 0; c < numBusCh; ++c) b[c] = bus.getWritePointer (c);
            curBus = b;
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
            if (modData != nullptr && modEng.uses (dsp::mod::SFollower)) follow (b, on);   // module 09: the AUDIO FOLLOWER (the raw part buses)
            if (! inside) scrAny = renderScreech (b, on);
            if (keepCore != 0xF || keepBus != 0x7F)                           // module 12: export of one group / layer
            {
                for (int c = 0; c < 4; ++c) if (! (keepCore >> c & 1u)) juce::FloatVectorOperations::clear (b[c], on);
                if (! (keepCore >> 2 & 1u)) { juce::FloatVectorOperations::clear (b[4], on); juce::FloatVectorOperations::clear (b[5], on); }
                for (int k = 0; k < 7; ++k) if (! (keepBus >> k & 1u)) { juce::FloatVectorOperations::clear (b[6 + 2 * k], on); juce::FloatVectorOperations::clear (b[7 + 2 * k], on); }
            }
            const bool busAny = cfg.lbAny || scrAny;
            const bool m6 = cfg.m06.any, m7 = cfg.tone.any, m8 = cfg.dyn.any;
            clockSec += (double) n / sr;

            stereo.clear (0, 0, n); stereo.clear (1, 0, n);
            juce::dsp::AudioBlock<float> blk (stereo.getArrayOfWritePointers(), 2, 0, (size_t) n);
            auto up = os->processSamplesUp (blk);
            float* L = up.getChannelPointer (0);
            float* R = up.getChannelPointer (1);
            const float inv = 1.f / (float) on;
            std::array<float, dsp::NumParts> pk {};
            // order: PUNCH, BODY, TAIL (-> MASTER CLIP on these three), then the SUB around the clip
            const bool atkCrunch = cfg.rpunch.on && cfg.rpunch.crunchOn && cfg.rpunch.crunch > 0.f;   // DISTORTED CRUNCH on the attack bus
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
                            if (m8 && cfg.dyn.at (dsp::m06::LTexture, 0)) dynSt->process (cfg.dyn, dsp::m06::LTexture, 0, tl, tr, m6Ctx (i), scOf (b, i), cfg.ks.rate);
                            if (distOn[dsp::dist::LocTexture])                             // TEXTURE group distortion
                            {
                                const auto mc = modCtx (i);
                                tl = distChain[dsp::dist::LocTexture * 2].process (cfg.dist, dsp::dist::LocTexture, tl, mc, cfg.ks.rate);
                                tr = distChain[dsp::dist::LocTexture * 2 + 1].process (cfg.dist, dsp::dist::LocTexture, tr, mc, cfg.ks.rate);
                            }
                            if (m6 && cfg.m06.at (dsp::m06::LTexture, 1)) m06St->process (cfg.m06, dsp::m06::LTexture, 1, tl, tr, m6Ctx (i), cfg.ks.rate);
                            if (m7 && cfg.tone.at (dsp::m06::LTexture, 1)) toneSt->process (cfg.tone, dsp::m06::LTexture, 1, tl, tr, m6Ctx (i), cfg.ks.rate);
                            if (m8 && cfg.dyn.at (dsp::m06::LTexture, 1)) dynSt->process (cfg.dyn, dsp::m06::LTexture, 1, tl, tr, m6Ctx (i), scOf (b, i), cfg.ks.rate);
                            float cl = b[6 + 2 * dsp::BusCustom][i] * cg, cr = b[7 + 2 * dsp::BusCustom][i] * cg;
                            if (cfg.spc.t[dsp::spatial::TTexture].on) (*spcUnits)[dsp::spatial::TTexture].process (cfg.spc.t[dsp::spatial::TTexture], tl, tr);   // module 11
                            if (cfg.spc.t[dsp::spatial::TCustom].on) (*spcUnits)[dsp::spatial::TCustom].process (cfg.spc.t[dsp::spatial::TCustom], cl, cr);
                            L[i] += tl + cl;
                            R[i] += tr + cr;
                        }
                    if (cfg.dist.globalPos == 0) globalChain (L, R, on);                                  // GLOBAL, before the clip (sub clean)
                    shaper.process (cfg.lb.shaper, L, R, on);
                }
                if (c == 3 && cfg.clipG > 1.0001f && ! cfg.masterBypass)
                {
                    for (int i = 0; i < on; ++i) { L[i] = doomsday::kick::masterClip (L[i], cfg.clipG, cfg.clipSoft); R[i] = doomsday::kick::masterClip (R[i], cfg.clipG, cfg.clipSoft); }
                }
                if (c == 3 && (cfg.crack.on || atkCrunch))                            // the RAW ATTACK BUS: after the master clip (the kick's DRIVE never touches the crack / crunch)
                    for (int i = 0; i < on; ++i) { L[i] += atkBuf[(size_t) i]; R[i] += atkBuf[(size_t) i]; atkBuf[(size_t) i] = 0.f; }
                const auto g0 = gainNow[(size_t) c], g1 = gainTarget[(size_t) c];
                const float* x = b[c];
                const bool wideTail = c == 2 && cfg.wide;
                const bool roomHere = cfg.room.on && ((c == 2 && ! cfg.zaag.on && ! cfg.pvcTail.on && ! cfg.raw.on && ! cfg.exp.on && ! cfg.hc.on) || (c == 0 && cfg.room.onTok && ! cfg.zpunch.on && ! cfg.cpunch.on && ! cfg.rpunch.on));   // (the classic room: not on the engine parts)
                auto* room = c == 0 ? tokRoom.get() : tailRoom.get();
                if (roomHere) room->cook (cfg.room);
                const bool colorHere = c == 2 && cfg.color.on && ! cfg.pvcTail.on && ! cfg.zaag.on && ! cfg.raw.on && ! cfg.exp.on && ! cfg.hc.on;
                const bool pvcHere = (c == 2 && (cfg.pvcTail.on || cfg.zaag.on || cfg.raw.on || cfg.exp.on || cfg.hc.on)) || (c == 0 && (cfg.zpunch.on || cfg.cpunch.on || cfg.rpunch.on));   // an ENGINE replaces the part's source (rule 0: one engine per part)
                const float noteNow = lastNoteHz * cfg.fineMul;
                const bool keepThis = (keepCore >> c & 1u) != 0;                  // (export of one part: FRONT / ATTACK BUS / engines belong to their part)
                auto noteTail = [&] (int i) { return c == 2 && cfg.rise.on ? noteNow * dsp::rise::pitchFactor (cfg.rise, msAt (i)) : noteNow; };   // TAIL RISE: PITCH on the tail engines
                if (colorHere) tailColor.cook (cfg.color, cfg.keyTrack ? lastNoteHz * cfg.fineMul : cfg.endHz * cfg.fineMul, cfg.ks.rate);
                float p = 0.f;
                for (int i = 0; i < on; ++i)
                {
                    const float u = (float) (i + 1) * inv;
                    const float gl = g0.first + (g1.first - g0.first) * u, gr = g0.second + (g1.second - g0.second) * u;
                    float yl, yr;
                    if (pvcHere && c == 0 && cfg.cpunch.on) { yl = yr = cpU.process (cfg.cpunch, msAt (i), noteNow); }   // PUNCH TYPE CLASSIC (EARLY)
                    else if (pvcHere && c == 0 && cfg.rpunch.on) { yl = yr = rpU.process (cfg.rpunch, msAt (i), noteNow); }   // PUNCH TYPE RAW
                    else if (pvcHere && c == 0) zpU.process (cfg.zpunch, msAt (i), noteNow, yl, yr);                  // PUNCH TYPE ZAAG
                    else if (pvcHere && cfg.hc.on) { yl = yr = hcU.process (cfg.hc, msAt (i), noteTail (i)); if (layerTap != nullptr) layerTap->add (dsp::energy::Crunch, i, hcU.lastCrunch()); }       // TAIL TYPE HARDCORE
                    else if (pvcHere && cfg.exp.on) expU.process (cfg.exp, msAt (i), noteTail (i), yl, yr);              // TAIL TYPE EXPERIMENTAL
                    else if (pvcHere && cfg.raw.on) rawU.process (cfg.raw, msAt (i), noteTail (i), yl, yr);              // TAIL TYPE RAW
                    else if (pvcHere && cfg.zaag.on) zaagU.process (cfg.zaag, msAt (i), noteTail (i), yl, yr, cfg.zgSend > 0.f ? sendBuf[(size_t) i] : 0.f);   // TAIL TYPE ZAAG (+ ZAAG SEND)
                    else if (pvcHere) pvcU.process (cfg.pvcTail, b[1][i], msAt (i), noteTail (i), yl, yr);   // (the room after it = the reverb after the cut)
                    else if (wideTail)
                    {
                        // lows of the plain centre + mids / highs of the detuned sides as mid / side (LR4: sums flat)
                        const float low = wl2.lp (wl1.lp (x[i], cfg.widthSplit), cfg.widthSplit);
                        const float hl = wh2.hp (wh1.hp (b[4][i], cfg.widthSplit), cfg.widthSplit), hr = wr2.hp (wr1.hp (b[5][i], cfg.widthSplit), cfg.widthSplit);
                        const float m = 0.5f * (hl + hr), sd = 0.5f * (hl - hr) * cfg.width;
                        yl = low + m + sd; yr = low + m - sd;
                    }
                    else { yl = x[i]; yr = x[i]; }
                    if (pvcHere && ! keepThis) yl = yr = 0.f;                         // module 12 export of one part: the ENGINE of another part stays silent too
                    if (! pvcHere || c == 0) { yl += b[6 + 2 * c][i]; yr += b[7 + 2 * c][i]; }  // module 04: layers on this part's bus (the PUNCH BUS with every punch type)
                    float front = 0.f;                                                // the FRONT: TOK (before the punch), CLICK + SUB PUNCH -> the PUNCH BUS
                    if (c == 0)                                                       // (own levels: not under the PUNCH fader / mute, not through the punch group's FX)
                    {
                        const float ms = msAt (i);
                        const float tk = tokU.process (cfg.tok, ms, noteNow);
                        if (layerTap != nullptr) layerTap->add (dsp::energy::Tok, i, tk);
                        if (cfg.zgSend > 0.f)                                         // ZAAG SEND: TOK + PUNCH (not the CLICK) into the ZAAG cascade
                        {
                            sendBuf[(size_t) i] = cfg.zgSend * (0.5f * (yl * gl + yr * gr) + tk);
                            yl *= 1.f - cfg.zgSend; yr *= 1.f - cfg.zgSend; front = tk * (1.f - cfg.zgSend);
                        }
                        else front = tk;
                        front += clkU.process (cfg.click, ms) + spkU.process (cfg.spk, ms);
                        if (cfg.crack.on || atkCrunch)                                // RAW PUNCH: CRACK + SNAP + DISTORTED CRUNCH -> the attack bus (after the master clip, below)
                        {
                            float a = cfg.crack.on ? crU.process (cfg.crack, ms, 0.5f * (yl + yr), rpU.lastHz(), b[1][i], noteNow) : 0.f;
                            crkBuf[(size_t) i] = a;
                            if (atkCrunch && layerTap != nullptr) layerTap->add (dsp::energy::Crunch, i, rpU.lastCrunch());
                            if (atkCrunch) { a += rpU.lastCrunch(); if (cfg.crunchSolo) crkBuf[(size_t) i] = rpU.lastCrunch(); }
                            if (cfg.noteCut && relMs >= 0.f) a *= juce::jlimit (0.f, 1.f, 1.f - (ms - relMs) / 5.f);
                            atkBuf[(size_t) i] = keepThis ? a : 0.f;
                        }
                        if (cfg.noteCut && relMs >= 0.f) front *= juce::jlimit (0.f, 1.f, 1.f - (ms - relMs) / 5.f);
                        if (! keepThis) front = 0.f;
                    }
                    if (pvcHere && cfg.noteCut && relMs >= 0.f)                                   // KICK END = NOTE: the engines stop with the note too (5 ms fade)
                    {
                        const float cg = juce::jlimit (0.f, 1.f, 1.f - (msAt (i) - relMs) / 5.f);
                        yl *= cg; yr *= cg;
                    }
                    if (c == 2 && cfg.rise.on) riseU.process (cfg.rise, msAt (i), yl, yr);                                                    // TAIL RISE: the tail's end grows (its own layers)
                    if (roomHere) room->process (cfg.room, yl, yr, juce::jlimit (0.f, 1.f, (cfg.ks.lengthMs - msAt (i)) / 40.f));               // TAIL ROOM: before everything that distorts the tail
                    if (m6 && cfg.m06.at (c + 1, 0)) m06St->process (cfg.m06, c + 1, 0, yl, yr, m6Ctx (i), cfg.ks.rate);   // module 06 BEFORE
                    if (m7 && cfg.tone.at (c + 1, 0)) toneSt->process (cfg.tone, c + 1, 0, yl, yr, m6Ctx (i), cfg.ks.rate); // module 07 BEFORE
                    if (m8 && cfg.dyn.at (c + 1, 0)) dynSt->process (cfg.dyn, c + 1, 0, yl, yr, m6Ctx (i), scOf (b, i), cfg.ks.rate); // module 08 BEFORE
                    if (distOn[(size_t) c + 1])                                       // module 05: the GROUP distortion (pre fader)
                    {
                        const auto mc = modCtx (i);
                        yl = distChain[(size_t) (c + 1) * 2].process (cfg.dist, c + 1, yl, mc, cfg.ks.rate);
                        yr = distChain[(size_t) (c + 1) * 2 + 1].process (cfg.dist, c + 1, yr, mc, cfg.ks.rate);
                    }
                    if (colorHere) tailColor.process (yl, yr);                        // TAIL COLOR: after the tail's distortion
                    if (m6 && cfg.m06.at (c + 1, 1)) m06St->process (cfg.m06, c + 1, 1, yl, yr, m6Ctx (i), cfg.ks.rate);   // module 06 AFTER
                    if (m7 && cfg.tone.at (c + 1, 1)) toneSt->process (cfg.tone, c + 1, 1, yl, yr, m6Ctx (i), cfg.ks.rate); // module 07 AFTER
                    if (m8 && cfg.dyn.at (c + 1, 1)) dynSt->process (cfg.dyn, c + 1, 1, yl, yr, m6Ctx (i), scOf (b, i), cfg.ks.rate); // module 08 AFTER
                    if (cfg.spc.t[(size_t) c].on) (*spcUnits)[(size_t) c].process (cfg.spc.t[(size_t) c], yl, yr);                   // module 11: the group's stereo
                    if (! std::isfinite (yl) || ! std::isfinite (yr)) { yl = yr = 0.f; blewUp = true; }   // a filter / state that blew up: silence the
                                                                                                                  //  sample now, reset the processing after the block (never stuck)
                    if (layerTap != nullptr) { static constexpr int lay[4] { dsp::energy::Punch, dsp::energy::Body, dsp::energy::Tail, dsp::energy::Sub }; layerTap->add (lay[c], i, 0.5f * (yl * gl + yr * gr)); }
                    yl = yl * gl + front; yr = yr * gr + front;
                    L[i] = (c == 0 ? 0.f : L[i]) + yl;
                    R[i] = (c == 0 ? 0.f : R[i]) + yr;
                    p = juce::jmax (p, std::abs (yl), std::abs (yr));
                }
                pk[(size_t) c] = p;
                gainNow[(size_t) c] = g1;
            }
            if (blewUp)                                                       // SAFETY (owner 2026-11-16 "the kick vanished, only the click stayed"): a NaN / inf
            {                                                                 // in a group's processing would stay forever - clear every processing state
                blewUp = false; blowUps.fetch_add (1);
                for (auto& ch : distChain) ch.reset();
                m06St->reset(); toneSt->reset(); dynSt->reset(); tailColor.reset(); tailRoom->clear(); tokRoom->clear();
                for (auto& u : *spcUnits) u.reset();
                for (int k = 0; k < on; ++k) { if (! std::isfinite (L[k])) L[k] = 0.f; if (! std::isfinite (R[k])) R[k] = 0.f; }
            }
            if (cfg.dist.globalPos == 1) globalChain (L, R, on);                                       // GLOBAL, after the clip (with the sub)
            if (busAny)                                                      // the MASTER bus: after the clip
                for (int i = 0; i < on; ++i) { L[i] += b[6 + 2 * dsp::BusMaster][i]; R[i] += b[7 + 2 * dsp::BusMaster][i]; }
            if (cfg.spc.t[dsp::spatial::TMaster].on && ! cfg.masterBypass)    // module 11: MASTER STEREO (before the output dynamics)
                for (int i = 0; i < on; ++i) (*spcUnits)[dsp::spatial::TMaster].process (cfg.spc.t[dsp::spatial::TMaster], L[i], R[i]);
            if (cfg.crackSolo || cfg.crunchSolo)                              // SOLO CRACK / SOLO CRUNCH: the crack alone (no TOK / PUNCH / BODY / TAIL / SUB / crunch)
                for (int i = 0; i < on; ++i) { L[i] = R[i] = crkBuf[(size_t) i]; crkBuf[(size_t) i] = 0.f; }
            sinceHitOs += on; sinceHitMs = msAt (0);          // (a sample counter: the same at any block size)
            os->processSamplesDown (blk);
            const float m0 = masterNow, m1 = masterTarget, invN = 1.f / (float) n;
            const float r = (float) std::exp (-juce::MathConstants<double>::twoPi * 5.0 / sr);
            float opk = 0.f;
            const float* sl = stereo.getReadPointer (0);
            const float* sr2 = stereo.getReadPointer (1);
            for (int i = 0; i < n; ++i)
            {
                const float m = m0 + (m1 - m0) * (float) (i + 1) * invN;
                float yl = dcL.tick (sl[i], r) * m, yr = dcR.tick (sr2[i], r) * m;
                if (cfg.dyn.any && cfg.dyn.at (dsp::dyn::LMaster, 1) && ! cfg.masterBypass)   // module 08 at MASTER OUTPUT (host rate, lookahead)
                {
                    dsp::m06::Ctx cx = m6Ctx (0); cx.ms = msAt (i * osFactor);
                    dynSt->process (cfg.dyn, dsp::dyn::LMaster, 1, yl, yr, cx, nullptr, sr);
                }
                // module 12: OUTPUT MODE, the optional SAFETY CLIP / LIMITER (off by default, never switched on by themselves), MUTE
                if (cfg.outMono) yl = yr = 0.5f * (yl + yr);
                if (! cfg.masterBypass)
                {
                    if (cfg.safetyClip) { yl = safeClip (yl); yr = safeClip (yr); }
                    if (cfg.safetyLim)
                    {
                        const float pk = juce::jmax (std::abs (yl), std::abs (yr));
                        const float want = pk > 0.977f ? 0.977f / pk : 1.f;             // -0.2 dBFS, instant attack (no lookahead), 80 ms release
                        limG = want < limG ? want : limG + limRel * (1.f - limG);
                        yl *= limG; yr *= limG;
                        if (limG < 0.999f) safetyActive.store (true, std::memory_order_relaxed);
                    }
                    else limG = 1.f;
                }
                const float muteT = cfg.masterMute ? 0.f : 1.f;
                muteG = muteG < muteT ? juce::jmin (muteT, muteG + muteStep) : juce::jmax (muteT, muteG - muteStep);
                if (muteG < 1.f) { yl *= muteG; yr *= muteG; }
                if (outCut) { outGate = juce::jmax (0.f, outGate - muteStep); yl *= outGate; yr *= outGate; }   // NOTE + FX (5 ms)
                out.setSample (0, start + i, yl);
                if (out.getNumChannels() > 1) out.setSample (1, start + i, yr);
                opk = juce::jmax (opk, std::abs (yl), std::abs (yr));
            }
            masterNow = m1;
            for (int c = 0; c < dsp::NumParts; ++c) partPeak[(size_t) c].store (juce::jmax (partPeak[(size_t) c].load(), pk[(size_t) c]));
            outPeak.store (juce::jmax (outPeak.load(), opk));
            if (cfg.dyn.any) publishMeters();
        }
    public:
        // module 08: the live meters (engine-level processors) for the GUI - lock free
        struct LiveMeter { std::atomic<float> inPk { 0 }, outPk { 0 }, inRms { 0 }, outRms { 0 }, gr { 0 }, grMax { 0 }, grAvg { 0 }; std::atomic<bool> clip { false }; };
        std::array<LiveMeter, 14> m8Meters;                                     // comp 0-3, transient 4-8, clipper 9-10, limiter 11-12, crest 13
        void resetMeterMax() { for (auto& m : m8Meters) { m.grMax.store (0.f); m.clip.store (false); } }
    private:
        void publishMeters()
        {
            auto pub = [&] (int k, dsp::dyn::Meter& m)
            {
                auto& o = m8Meters[(size_t) k];
                o.inPk.store (juce::jmax (o.inPk.load() * 0.8f, m.inPk)); o.outPk.store (juce::jmax (o.outPk.load() * 0.8f, m.outPk));
                o.inRms.store (std::sqrt (m.inMs)); o.outRms.store (std::sqrt (m.outMs));
                o.gr.store (m.gr); o.grMax.store (juce::jmin (o.grMax.load(), m.grMax));
                if (m.grN > 0) o.grAvg.store (o.grAvg.load() + 0.1f * (m.grSum / (float) m.grN - o.grAvg.load()));
                if (m.clip) o.clip.store (true);
                m.inPk = m.outPk = 0.f; m.grSum = 0.f; m.grN = 0; m.grMax = 0.f; m.clip = false;
            };
            auto& d = *dynSt;
            for (int i = 0; i < 4; ++i) pub (i, d.cmp[(size_t) i].meter);
            for (int i = 0; i < 5; ++i) pub (4 + i, d.ts[(size_t) i].meter);
            for (int i = 0; i < 2; ++i) pub (9 + i, d.clp[(size_t) i].meter);
            for (int i = 0; i < 2; ++i) pub (11 + i, d.lim[(size_t) i].meter);
            pub (13, d.crest.meter);
        }

        // module 05: what moves the stages' modulation at engine level (since the last hit, the tail pitch envelope, the velocity)
        dsp::dist::ModCtx modCtx (int i)
        {
            dsp::dist::ModCtx m;
            m.sinceHitMs = msAt (i);
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
            c.ms = msAt (i); c.vel = lastVelocity; c.noteHz = lastNoteHz; c.lengthMs = cfg.ks.lengthMs;
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
            if (cfg.dyn.any && cfg.dyn.at (dsp::m06::LGlobal, 0)) for (int i = 0; i < on; ++i) dynSt->process (cfg.dyn, dsp::m06::LGlobal, 0, L[i], R[i], m6Ctx (i), scOf (curBus, i), cfg.ks.rate);
            if (distOn[dsp::dist::LocGlobal]) globalDist (L, R, on);
            if (z) for (int i = 0; i < on; ++i) m06St->process (cfg.m06, dsp::m06::LGlobal, 1, L[i], R[i], m6Ctx (i), cfg.ks.rate);
            if (tz) for (int i = 0; i < on; ++i) toneSt->process (cfg.tone, dsp::m06::LGlobal, 1, L[i], R[i], m6Ctx (i), cfg.ks.rate);
            if (cfg.dyn.any && cfg.dyn.at (dsp::m06::LGlobal, 1)) for (int i = 0; i < on; ++i) dynSt->process (cfg.dyn, dsp::m06::LGlobal, 1, L[i], R[i], m6Ctx (i), scOf (curBus, i), cfg.ks.rate);
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
                const float ms = msAt (i);
                if (cfg.scr.pitchMode == 1 && (i & 15) == 0) semis = pe.semis (dsp::PTail, ms);
                // RELATIVE: the played note two octaves up (the screech register); MIDI TRACKED: + the kick's pitch engine; ABSOLUTE: BASE HZ
                const float base = cfg.scr.pitchMode == 2 ? cfg.scr.baseHz : lastNoteHz * dsp::scr::registerMul * (cfg.scr.pitchMode == 1 ? std::exp2 (semis / 12.f) : 1.f);
                float l = 0.f, r = 0.f, o = 0.f;
                for (auto& v : scrEng.v) { float vl, vr, vo; v->process (cfg.scr, tb, base, cfg.ks.rate, lastNoteHz, vl, vr, vo); l += vl; r += vr; o += vo; }
                if (scrCut) { scrGate = juce::jmax (0.f, scrGate - (float) (1000.0 / (5.0 * cfg.ks.rate))); l *= scrGate; r *= scrGate; o *= scrGate; }
                scrOsc[(size_t) i] = o;
                if (cfg.m06.any && cfg.m06.at (dsp::m06::LScreech, 0)) m06St->process (cfg.m06, dsp::m06::LScreech, 0, l, r, m6Ctx (i), cfg.ks.rate);
                if (cfg.tone.any && cfg.tone.at (dsp::m06::LScreech, 0)) toneSt->process (cfg.tone, dsp::m06::LScreech, 0, l, r, m6Ctx (i), cfg.ks.rate);
                if (cfg.dyn.any && cfg.dyn.at (dsp::m06::LScreech, 0)) dynSt->process (cfg.dyn, dsp::m06::LScreech, 0, l, r, m6Ctx (i), scOf (b, i), cfg.ks.rate);
                if (layerTap != nullptr) layerTap->add (dsp::energy::Scream, i, 0.5f * (l + r));
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
        int64_t sinceHitOs = (int64_t) 1 << 40;
        float msAt (int i) const { return sinceHitOs >= ((int64_t) 1 << 39) ? 1.0e9f : (float) ((double) (sinceHitOs + i) * 1000.0 / cfg.ks.rate); }
        bool blewUp = false; std::atomic<int> blowUps { 0 };           // (SAFETY: how many times a state was rescued - tests / diagnostics)
        float sinceHitMs = 1.0e9f, pitchNormNow = 0.f, lastVelocity = 1.f, lastNoteHz = 49.f, relMs = -1.f;
        std::unique_ptr<dsp::m06::State> m06St = std::make_unique<dsp::m06::State>();   // module 06 at engine level (not the layers)
        std::unique_ptr<dsp::tone::State> toneSt = std::make_unique<dsp::tone::State>(); // module 07 at engine level (not the layers)
        std::unique_ptr<dsp::dyn::State> dynSt = std::make_unique<dsp::dyn::State>();    // module 08 at engine level (not the layers)
        std::unique_ptr<dsp::room::Room> tailRoom = std::make_unique<dsp::room::Room>();
        dsp::color::TailColor tailColor;
        std::unique_ptr<dsp::room::Room> tokRoom = std::make_unique<dsp::room::Room>();     // TOK + TAIL: the same room on the punch bus
        std::unique_ptr<dsp::shaped::Renderer> shapedR = std::make_unique<dsp::shaped::Renderer>();
        std::unique_ptr<dsp::pvc::Renderer> pvcR = std::make_unique<dsp::pvc::Renderer>();          // PVC punch (owner 2026-11-14)
        dsp::pvctail::Unit pvcU;
        dsp::zaag::Unit zaagU; dsp::hardcore::Unit hcU; dsp::raw::Unit rawU; dsp::exper::Unit expU;
        dsp::zaagpunch::Unit zpU;
        dsp::classicpunch::Unit cpU;
        dsp::rawpunch::Unit rpU;
        dsp::rise::Unit riseU;
        dsp::front::ClickUnit clkU; dsp::front::TokUnit tokU; dsp::front::SubPunchUnit spkU; dsp::front::CrackUnit crU;
        std::vector<float> atkBuf, crkBuf;                                          // RAW CRACK ENGINE: the RAW ATTACK BUS of this block
        std::vector<float> sendBuf;                                         // ZAAG SEND: TOK + PUNCH of this block -> the ZAAG cascade
        std::unique_ptr<std::array<dsp::spatial::Unit, dsp::spatial::NumTargets>> spcUnits = std::make_unique<std::array<dsp::spatial::Unit, dsp::spatial::NumTargets>>();   // module 11
        float scBuf[6] {};
        float* const* curBus = nullptr;
        // module 08 sidechain sources this sample: [1..4] the raw parts (+ their layers), [5] the texture bus
        const float* scOf (float* const* b, int i)
        {
            if (b == nullptr) return nullptr;
            for (int c = 0; c < 4; ++c) scBuf[c + 1] = b[c][i] + 0.5f * (b[6 + 2 * c][i] + b[7 + 2 * c][i]);
            scBuf[5] = 0.5f * (b[6 + 2 * dsp::BusTexture][i] + b[7 + 2 * dsp::BusTexture][i]);
            return scBuf;
        }
    public:
        dsp::dyn::State& dynState() { return *dynSt; }                       // (GUI analysis: the probe / meters)
        int dynLookahead() const { return cfg.dyn.masterLook; }
    private:
        dsp::scr::Engine scrEng;
        dsp::energy::LayerTap* layerTap = nullptr;                             // LAYER ENERGY: offline renders only (never the plugin)
    public:
        void setLayerTap (dsp::energy::LayerTap* t) { layerTap = t; }
        double tapRate() const { return cfg.ks.rate; }
    private:
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
        float limG = 1.f, limRel = 0.001f, muteG = 1.f, muteStep = 0.01f;   // module 12
        float scrGate = 1.f, outGate = 1.f; bool scrCut = false, outCut = false;   // KICK END: the screech / the whole output (NOTE + FX)
        static float safeClip (float x) { const float a = std::abs (x); if (a <= 0.9f) return x; return std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f), x); }
        // module 12: what an export of one group / layer keeps (the engine renders everything, the rest is silenced at the buses)
        uint32_t keepCore = 0xF, keepBus = 0x7F;
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
                                                const dsp::dist::DistData* distData = nullptr, dsp::dyn::Probe* probe = nullptr,
                                                const dsp::mod::Data* modData = nullptr, const Engine::SamplePtrs* samples = nullptr,
                                                dsp::energy::LayerTap* layerTap = nullptr)
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
        engine->setModData (modData);
        engine->setLayerTap (layerTap);                  // LAYER ENERGY (offline)
        engine->setSamples (samples);                    // module 10: the sample slots                    // module 09: the same modulation as the plugin (not playing: the engine clock at 'bpm')
        engine->cookOnly (f);
        engine->dynState().probe = probe;                      // (GUI analysis: one module 08 processor's gain history)
        const int look = engine->dynLookahead();               // module 08: the export starts on the beat (the lookahead is taken out)
        juce::AudioBuffer<float> out (2, total), tmp (2, block);
        out.clear();
        for (int pos = 0; pos < total + look; pos += block)
        {
            const int n = juce::jmin (block, total + look - pos);
            tmp.setSize (2, n, false, false, true); tmp.clear();
            juce::MidiBuffer m;
            if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, velocity), 0);
            if (offAt >= pos && offAt < pos + n) m.addEvent (juce::MidiMessage::noteOff (1, note), offAt - pos);
            engine->process (tmp, m, f);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i) { const int o = pos + i - look; if (o >= 0 && o < total) out.setSample (ch, o, tmp.getSample (ch, i)); }
        }
        return out;
    }
}
