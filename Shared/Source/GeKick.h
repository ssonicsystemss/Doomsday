#pragma once
// =====================================================================
//  GeKick.h (Shared) - the 4GE kick voice (successor of Forge of Wrath's DoomsdayKick.h; Forge is frozen, this one grows).
//  Used by 4GE (the kick) and RE4GE (its tail as the reverse bass source, ge::stage / ge::filt in its tone chain)
//  Built after the owner's architecture document (Shared/RESEARCH.md "Architektura hardstyle kicka", 2026-10-23):
//
//   PUNCH   : SINE / TRI / SAW / NOISE / RESO source, Hz START -> END with AMOUNT (depth of the move), DROP, ATTACK, PHASE,
//             DECAY or TOK (spike -> shelf -> cut), its own FILTER (LP / HP / BP / NOTCH) -> DRIVE with its own type,
//             or INTO DIST (own pass through the tail's STAGE 1)
//   TAIL    : OSC A (sine -> tri -> saw) + OSC B (saw / pulse / tri) with separate levels, PHASE, ATTACK,
//             pitch START (x note or fixed Hz) -> note (CURVE / HOLD / DRAWN, FIXED HEAD), DUCK under the punch
//             -> TONE (tilt) -> PRE FILTER (type, cutoff, reso, envelope sweep) -> STAGE 1 (type, drive, BIAS, MIX)
//             -> MID EQ -> STAGE 2 (type, drive, BIAS, MIX) -> LOW / HIGH CUT -> BODY -> SCREAM / CRUNCH / GRIT -> AMP
//             (AUTO GAIN: the stages keep their level when the drive goes up)
//   TEXTURE : WHITE / PINK noise, PULSE / SAW on the tail pitch -> band / reso filter -> drive -> envelope -> high-pass
//   SUB     : with CLEAN SUB the clean sine below the split is its own layer with its own LEVEL and skips the BUS
//   CLICK   : filtered noise burst
//   BUS     : PUNCH + TAIL + TEXTURE + CLICK -> BUS DIST (type, drive, mix) -> + SUB
//   ROOM    : small room before or after the distortion, with its own LOW CUT
//  Layers can be muted (solo / mute in the GUI, not saved).
//  Real-time safe: no allocation in render (the room is sized in prepare()).
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include "DoomsdayDSP.h"
#include "DoomsdayMod.h"
#include "DoomsdayKick.h"      // Shape, KickRoom, shape(), masterClip() - shared, unchanged
#include <array>
#include <cmath>

namespace ge
{
    using Shape = doomsday::mod::Shape;
    using doomsday::kick::masterClip;

    enum Layer { LPunch = 1, LTail = 2, LTexture = 4, LSub = 8, LClick = 16, LSample = 32, LAll = 63 };

    // waveshapers: 0 SOFT, 1 TUBE, 2 HARD, 3 FOLD, 4 RAW, 5 CRUSH (stepped)
    inline float shape (float x, int type)
    {
        if (type == 5) { const float q = std::round (juce::jlimit (-1.2f, 1.2f, x) * 6.f) / 6.f; return juce::jlimit (-1.f, 1.f, q); }
        return doomsday::kick::shape (x, type);
    }
    // one distortion stage: BIAS shifts the curve (even harmonics, the rest is removed again), AUTO keeps the level
    inline float stage (float x, float g, int type, float bias, float mix, bool autoGain)
    {
        float y = shape (x * g + bias, type) - (std::abs (bias) > 1.0e-4f ? shape (bias, type) : 0.f);
        if (autoGain)
        {
            const float lg = std::log2 (juce::jmax (1.f, g));
            y *= (1.f / juce::jmin (1.f, juce::jmax (0.05f, g))) * (1.f - 0.29f * juce::jlimit (0.f, 1.f, lg / 4.f));
        }
        return mix >= 0.999f ? y : x + (y - x) * mix;
    }

    // filter types: 0 OFF, 1 LP, 2 HP, 3 BP, 4 NOTCH
    inline float filt (doomsday::dsp::Svf& f, float x, const doomsday::dsp::SvfCoefs& c, int type)
    {
        if (type <= 0) return x;
        float l, b; f.tick (x, c, l, b);
        switch (type)
        {
            case 1:  return l;
            case 2:  return x - c.k * b - l;
            case 3:  return b * c.k;          // unity peak
            default: return x - c.k * b;
        }
    }
    inline float resoToK (float r) { return 1.f / (0.6f + juce::jlimit (0.f, 1.f, r) * r * 11.4f); }     // 0..1 -> Q 0.6 .. 12

    struct Params
    {
        // ---- TAIL
        float tailDb = 0.f, startRatio = 3.5f, dropMs = 60.f, curve = 4.f, wave = 0.f, tuneSemis = 0.f, holdMs = 0.f;
        bool  startFixed = false; float startHz = 250.f;       // START as a fixed Hz on every note (the drop is longer on low notes)
        float oscADb = 0.f, oscBDb = -60.f, oscBWidth = 0.5f; int oscBType = 0;   // 0 SAW, 1 PULSE, 2 TRI
        float tailPhase = 0.f, tailAttackMs = 0.f, duck = 0.f;
        // UNISON (2026-10-28, "drętwe" vs 5CR34M 4LPH4): extra detuned copies of OSC A that drift slowly. The distortion after them
        // turns the beating into a dense, moving spectrum. side 0 = centre (the engine keeps it plain: the low band comes from it),
        // 1 / 2 = left / right copies with their own detune pattern and phases (the stereo width of the mids / highs).
        int   uniVoices = 1; float uniDetuneCt = 0.f, uniMix = 0.6f, driftCt = 0.f; int side = 0;
        bool  headFixed = false; float headMs = 110.f, headHz = 49.f, headGlideMs = 8.f;
        bool  pitchDrawn = false;
        // pre filter (before STAGE 1)
        int   preType = 0; float preHz = 800.f, preReso = 0.2f, preEnv = 0.f, preDecayMs = 80.f;   // ENV: octaves x 4 at the hit, decaying
        // distortion
        int   distType = 1; float driveDb = 18.f, tone = 0.f, bias1 = 0.f, mix1 = 1.f;
        float midHz = 800.f, midDb = 0.f, midQ = 1.f;
        int   dist2Type = 0; float drive2Db = 6.f, bias2 = 0.f, mix2 = 1.f;
        bool  autoGain = false;
        float lowCutHz = 25.f, highCutHz = 12000.f;
        float screamHz = 900.f, scream = 0.f, crunch = 0.f, crunchDecayMs = 120.f, bodyDb = 0.f, grit = 0.f;
        float cleanSubHz = 0.f, subDb = 0.f;
        float chaos = 0.f, chaosMs = 60.f, chaosHz = 2500.f;
        float room = 0.f, roomSize = 0.5f, roomLowCutHz = 20.f; bool roomIntoDist = true;
        // ---- PUNCH
        bool  punchOn = true; int punchType = 0;              // 0 SINE, 1 TRI, 2 SAW, 3 NOISE, 4 RESO
        float punchDb = -6.f, punchStartHz = 400.f, punchEndHz = 90.f, punchAmount = 1.f, punchDropMs = 12.f, punchDecayMs = 40.f;
        float punchAttackMs = 0.f, punchPhase = 0.f, punchLenMs = 0.f;
        int   punchFiltType = 0; float punchFiltHz = 1000.f, punchFiltReso = 0.2f;
        int   punchDistType = 0; float punchDriveDb = 0.f;
        bool  punchIntoDist = false;
        // ---- TEXTURE
        int   texSource = 0;                                   // 0 WHITE, 1 PINK, 2 PULSE, 3 SAW
        float texDb = -60.f, texHz = 3000.f, texReso = 0.3f, texDecayMs = 60.f, texDriveDb = 0.f, texSemis = 12.f, texHpHz = 200.f;
        // ---- CLICK
        float clickDb = -14.f, clickHz = 5000.f, clickMs = 4.f;
        // ---- BUS
        int   busType = 0; float busDriveDb = 6.f, busMix = 1.f;     // 0 OFF, 1.. = SOFT .. CRUSH
        // ---- envelopes over LENGTH (per-mille)
        float lengthMs = 410.f;
        Shape amp = Shape::make ({ { 0.f, 1.f }, { 800.f, 0.92f }, { 950.f, 0.55f }, { 1000.f, 0.f } });
        Shape drive = Shape::make ({ { 0.f, 1.f }, { 1000.f, 1.f } });
        Shape pitch = Shape::make ({ { 0.f, 0.55f }, { 40.f, 0.3f }, { 150.f, 0.08f }, { 400.f, 0.f }, { 1000.f, 0.f } });
        int   mute = 0;                                        // Layer bits that are silent

        struct Cooked
        {
            double rate = 0.0;
            doomsday::dsp::SvfCoefs hc, body, scream, chaos, click, sub, pre, pf, tex, rlc;
            doomsday::dsp::BiquadCoefs mid;
            float preC = 0.f, lcC = 0.f, crC = 0.f, grC = 0.f, texHpC = 0.f;
            float tailG = 1.f, bodyG = 0.f, punchG = 1.f, punchDrv = 1.f, clickG = 0.f, chaosNorm = 0.f, drive2G = 1.f, oscAG = 1.f, oscBG = 0.f;
            float texG = 0.f, texDrv = 1.f, subG = 1.f, busG = 1.f, pinkNorm = 1.f, dcR = 0.9998f;
        };
        mutable Cooked ck;
        void cook (double rate) const
        {
            using namespace doomsday::dsp;
            auto one = [rate] (float hz) { return 1.f - std::exp (-juce::MathConstants<float>::twoPi * juce::jlimit (5.f, (float) rate * 0.45f, hz) / (float) rate); };
            auto db = [] (float d) { return std::pow (10.f, d / 20.f); };
            ck.rate = rate;
            ck.hc = svfCoefs (highCutHz, 1.f / 0.707f, rate);
            ck.body = svfCoefs (140.f, 1.f / 0.707f, rate);
            ck.scream = svfCoefs (screamHz, 1.f / 5.f, rate);
            ck.chaos = svfCoefs (chaosHz, 1.f / 0.8f, rate);
            ck.click = svfCoefs (clickHz, 1.f / 1.2f, rate);
            ck.sub = svfCoefs (juce::jmax (20.f, cleanSubHz), 1.f / 0.707f, rate);
            ck.pre = svfCoefs (preHz, resoToK (preReso), rate);
            ck.pf = svfCoefs (punchFiltHz, resoToK (punchFiltReso), rate);
            ck.tex = svfCoefs (texHz, resoToK (texReso), rate);
            ck.rlc = svfCoefs (juce::jmax (20.f, roomLowCutHz), 1.f / 0.707f, rate);
            ck.mid = BiquadCoefs::make (BiquadCoefs::Bell, rate, midHz, juce::jmax (0.2f, midQ), midDb);
            ck.preC = one (400.f); ck.lcC = one (lowCutHz); ck.crC = one (350.f); ck.grC = one (900.f); ck.texHpC = one (texHpHz);
            ck.tailG = db (tailDb); ck.bodyG = db (bodyDb) - 1.f;
            ck.punchG = db (punchDb); ck.punchDrv = db (punchDriveDb);
            ck.clickG = db (clickDb) * 2.f;
            ck.chaosNorm = std::sqrt ((float) rate * 0.5f / juce::jmax (100.f, chaosHz)) * 1.5f;
            ck.drive2G = db (drive2Db);
            ck.oscAG = oscADb <= -59.f ? 0.f : db (oscADb);
            ck.oscBG = oscBDb <= -59.f ? 0.f : db (oscBDb);
            ck.texG = texDb <= -59.f ? 0.f : db (texDb);
            ck.texDrv = db (texDriveDb);
            ck.subG = subDb <= -59.f ? 0.f : db (subDb);
            ck.busG = db (busDriveDb);
            ck.pinkNorm = 0.11f;
            ck.dcR = (float) std::exp (-juce::MathConstants<double>::twoPi * 5.0 / rate);     // DC blocker at 5 Hz on every sample rate
            // (Forge used a fixed 0.9995: 14 Hz at 44.1 kHz x4 but 30 Hz at 96 kHz x4 - the 96 kHz kick lost 1 dB of its fundamental)
        }
    };

    class Voice
    {
    public:
        void prepare (double oversampledRate) { rate = oversampledRate; inv = (float) (1.0 / rate); room.prepare (rate); }
        void trigger (float noteHz, float velocity, const Params& p)
        {
            active = true; t = 0.0; vel = velocity; baseHz = noteHz;
            phase = phaseB = juce::jlimit (0.f, 1.f, p.tailPhase / 360.f); if (phase >= 1.f) phase = phaseB = 0.f;
            {
                uCount = 0;
                uint32_t h = 0x51ed270bu * (uint32_t) (p.side + 1);                  // fixed per side: every hit is the same
                for (size_t i = 0; i < uph.size(); ++i)
                {
                    h ^= h << 13; h ^= h >> 17; h ^= h << 5; uph[i] = (float) (h & 0xffff) / 65536.f;
                    h ^= h << 13; h ^= h >> 17; h ^= h << 5; driftHz[i] = 0.4f + 1.9f * (float) (h & 0xffff) / 65536.f;
                    h ^= h << 13; h ^= h >> 17; h ^= h << 5; driftPh[i] = (float) (h & 0xffff) / 65536.f;
                }
            }
            pphase = juce::jlimit (0.f, 0.999f, p.punchPhase / 360.f); tphase = 0.f;
            ampSeg = driveSeg = pitchSeg = 0; fade = 1.f; fadeStep = 0.f;
            pre = {}; lc = {}; hc1 = {}; hc2 = {}; sc = {}; crHp = {}; dcx = dcy = 0.f; ck = {};
            noise = 0x12345678u; gnoise = 0x9e3779b9u; cnoise = 0x7f4a7c15u; pnoise = 0x2545f491u; tnoise = 0x68e31da4u;
            grHp = {}; bodyLp = {}; ch1 = {}; ch2 = {}; prf = {}; pf = {}; pres = {}; pnz = {}; tf = {}; thp1 = {}; thp2 = {};
            midF.reset(); room.clear(); subY1 = {}; subY2 = {}; subS1 = {}; subS2 = {}; phc1 = {}; phc2 = {}; psc = {}; plc = {};
            rl1 = {}; rl2 = {}; pink = {}; bdcx = bdcy = 0.f;
        }
        void release (float ms) { fadeStep = 1.f / juce::jmax (1.f, ms * 0.001f * (float) rate); }
        bool isActive() const { return active; }
        void setRoom (float size) { room.set (size); }

        inline float render (const Params& p)
        {
            if (! active) return 0.f;
            using namespace doomsday::dsp;
            const auto& c = p.ck;
            const float ms = (float) (t * 1000.0);
            const float toPm = 1000.f / juce::jmax (10.f, p.lengthMs);
            const float endMs = p.amp.endMs() / toPm;
            const bool roomAfter = p.room > 0.001f && ! p.roomIntoDist;
            const float tailEnd = endMs + (roomAfter ? room.tailMs() : 0.f);
            const float punchEnd = p.punchLenMs > 0.5f ? p.punchLenMs + 6.f : p.punchDecayMs * 4.f + 10.f + p.punchAttackMs;
            const float texEnd = c.texG > 0.f ? p.texDecayMs * 6.f : 0.f;
            if (ms > tailEnd + 1.f && ms > punchEnd && ms > texEnd && ms > p.clickMs * 3.f) { active = false; return 0.f; }
            const float two = juce::MathConstants<float>::twoPi;
            const int mute = p.mute;

            // ---------------- tail pitch
            float endHz = baseHz * std::pow (2.f, p.tuneSemis / 12.f);
            if (p.headFixed)
            {
                const float u = juce::jlimit (0.f, 1.f, (ms - p.headMs) / juce::jmax (0.5f, p.headGlideMs));
                endHz = p.headHz * std::pow (endHz / p.headHz, u * u * (3.f - 2.f * u));
            }
            float hz;
            if (p.pitchDrawn) hz = endHz * std::pow (2.f, p.pitch.eval (ms * toPm, pitchSeg) * 4.f);
            else
            {
                const float e = std::exp (-juce::jmax (0.f, ms - p.holdMs) / juce::jmax (1.f, p.dropMs) * p.curve);
                hz = p.startFixed ? endHz + (p.startHz - endHz) * e : endHz * (1.f + (p.startRatio - 1.f) * e);
            }
            hz = juce::jlimit (5.f, (float) rate * 0.2f, hz);
            const bool inTail = ms <= endMs + 1.f;

            // ---------------- PUNCH
            float punch = 0.f, punchGate = 1.f, pe = 0.f;
            if (p.punchOn && ms < punchEnd)
            {
                const float mv = std::exp (-ms / juce::jmax (0.5f, p.punchDropMs) * 3.f) * p.punchAmount;
                const float ph = juce::jlimit (10.f, (float) rate * 0.2f, p.punchEndHz + (p.punchStartHz - p.punchEndHz) * mv);
                const float dt = ph * inv;
                pphase += dt; if (pphase >= 1.f) pphase -= 1.f;
                const float att = p.punchAttackMs > 0.02f ? juce::jmin (1.f, ms / p.punchAttackMs) : juce::jmin (1.f, ms / 0.3f + 0.2f);
                const float tm = juce::jmax (0.f, ms - p.punchAttackMs);
                if (p.punchLenMs > 0.5f)
                {
                    pe = 0.55f + 0.45f * std::exp (-tm / juce::jmax (1.f, p.punchDecayMs) * 2.3f);
                    pe *= att * juce::jlimit (0.f, 1.f, (p.punchLenMs + 5.f - ms) / 5.f);
                }
                else pe = std::exp (-tm / juce::jmax (1.f, p.punchDecayMs) * 2.3f) * att;
                float ps;
                switch (p.punchType)
                {
                    case 1:  ps = 1.f - 4.f * std::abs (pphase - 0.5f); break;
                    case 2:  ps = 2.f * pphase - 1.f - polyBlep (pphase, dt); break;
                    case 3:
                    {
                        pnoise ^= pnoise << 13; pnoise ^= pnoise >> 17; pnoise ^= pnoise << 5;
                        const float wn = (float) (int32_t) pnoise * (1.f / 2147483648.f);
                        ps = pnz.bp (wn, svfCoefs (ph * 2.f, 1.f / 1.5f, rate)) * 2.2f;       // noise around the punch pitch
                        break;
                    }
                    case 4:
                    {
                        const float saw = 2.f * pphase - 1.f - polyBlep (pphase, dt);
                        ps = pres.lp (saw, svfCoefs (ph * 3.f, 1.f / 9.f, rate)) * 0.45f;      // a ringing peak riding the pitch
                        break;
                    }
                    default: ps = std::sin (two * pphase);
                }
                ps = filt (pf, ps, c.pf, p.punchFiltType);
                if (p.punchDriveDb > 0.05f) ps = shape (ps * c.punchDrv, p.punchDistType) / juce::jmax (0.3f, std::abs (shape (c.punchDrv, p.punchDistType)));
                punch = ps * pe * c.punchG;
                punchGate = p.punchLenMs > 0.5f ? juce::jlimit (0.f, 1.f, (p.punchLenMs + 5.f - ms) / 5.f) : 1.f;
            }

            // ---------------- TAIL
            const float dEnv = p.drive.eval (ms * toPm, driveSeg);
            float y = 0.f, subOut = 0.f;
            if (inTail || roomAfter)
            {
                const float dt = hz * inv;
                phase += dt; if (phase >= 1.f) phase -= 1.f;
                phaseB += dt; if (phaseB >= 1.f) phaseB -= 1.f;
                const float s = std::sin (two * phase);
                float x = 0.f;
                if (inTail)
                {
                    const float tri = 1.f - 4.f * std::abs (phase - 0.5f);
                    const float saw = 2.f * phase - 1.f - polyBlep (phase, dt);
                    const float w = p.wave;
                    x = (w < 0.5f ? s + (tri - s) * (w * 2.f) : tri + (saw - tri) * ((w - 0.5f) * 2.f)) * c.oscAG;
                    if (c.oscBG > 0.f)
                    {
                        float b;
                        if (p.oscBType == 1)
                        {
                            const float pw = juce::jlimit (0.05f, 0.95f, p.oscBWidth);
                            b = (phaseB < pw ? 1.f : -1.f) + polyBlep (phaseB, dt) - polyBlep (wrap01 (phaseB + 1.f - pw), dt);
                            b -= 2.f * pw - 1.f;                       // no DC
                        }
                        else if (p.oscBType == 2) b = 1.f - 4.f * std::abs (phaseB - 0.5f);
                        else b = 2.f * phaseB - 1.f - polyBlep (phaseB, dt);
                        x += b * c.oscBG;
                    }
                    if (p.uniVoices > 1 && p.side != 0 && p.uniDetuneCt + p.driftCt > 0.01f)
                    {
                        const int nu = juce::jmin (6, p.uniVoices - 1);
                        float xu = 0.f;
                        const bool refresh = (uCount++ & 31) == 0;          // detune + drift move slowly: new ratios every 32 samples
                        const float w = p.wave;
                        for (int i = 0; i < nu; ++i)
                        {
                            if (refresh)
                            {
                                // spread -1..+1, a different pattern per side; slow drift (0.4 - 2.3 Hz) per voice
                                const float spread = nu == 1 ? (p.side == 1 ? -1.f : 1.f) : (2.f * (float) i / (float) (nu - 1) - 1.f) * (p.side == 1 ? 1.f : -0.83f);
                                const float cents = spread * p.uniDetuneCt + p.driftCt * std::sin (two * (driftHz[(size_t) i] * (float) t + driftPh[(size_t) i]));
                                uRatio[(size_t) i] = std::exp2 (cents / 1200.f);
                            }
                            const float udt = dt * uRatio[(size_t) i];
                            float& up = uph[(size_t) i];
                            up += udt; if (up >= 1.f) up -= 1.f;
                            const float utri = 1.f - 4.f * std::abs (up - 0.5f);
                            if (w < 0.5f) { const float us = fastSin01 (up); xu += us + (utri - us) * (w * 2.f); }
                            else          { const float usaw = 2.f * up - 1.f - polyBlep (up, udt); xu += utri + (usaw - utri) * ((w - 0.5f) * 2.f); }
                        }
                        x += xu * c.oscAG * p.uniMix / std::sqrt ((float) nu);
                    }
                    if (p.tailAttackMs > 0.02f) x *= juce::jmin (1.f, ms / p.tailAttackMs);
                }
                // TONE (tilt ~400 Hz)
                const float lp = pre.lp (x, c.preC);
                x = lp * (1.f - 0.6f * p.tone) + (x - lp) * (1.f + 2.f * p.tone);
                // PRE FILTER (+ sweep)
                if (p.preType > 0)
                {
                    if (std::abs (p.preEnv) > 0.001f)
                    {
                        const float e = std::exp (-ms / juce::jmax (1.f, p.preDecayMs));
                        x = filt (prf, x, svfCoefs (p.preHz * std::exp2 (p.preEnv * 4.f * e), resoToK (p.preReso), rate), p.preType);
                    }
                    else x = filt (prf, x, c.pre, p.preType);
                }
                const float gritNow = p.grit * dEnv;
                if (p.chaos > 0.001f && ms < p.chaosMs * 6.f)
                {
                    cnoise ^= cnoise << 13; cnoise ^= cnoise >> 17; cnoise ^= cnoise << 5;
                    const float wn = (float) (int32_t) cnoise * (1.f / 2147483648.f);
                    const float nz = ch2.lp (ch1.lp (wn, c.chaos), c.chaos);
                    x += nz * c.chaosNorm * std::exp (-ms / juce::jmax (2.f, p.chaosMs)) * juce::jmin (1.f, ms / 0.5f) * p.chaos;
                }
                if (p.room > 0.001f && p.roomIntoDist) x += rl2.hp (rl1.hp (room.process (x), c.rlc), c.rlc) * p.room * 2.5f;
                float gn = 0.f;
                if (gritNow > 0.001f)
                {
                    gnoise ^= gnoise << 13; gnoise ^= gnoise >> 17; gnoise ^= gnoise << 5;
                    gn = grHp.hp ((float) (int32_t) gnoise * (1.f / 2147483648.f), c.grC);
                    x += gn * gritNow * 0.012f;
                }
                // STAGE 1 -> MID EQ -> STAGE 2
                y = stage (x, std::pow (10.f, (p.driveDb * dEnv) / 20.f), p.distType, p.bias1, p.mix1, p.autoGain);
                if (std::abs (p.midDb) > 0.05f) y = midF.process (y, c.mid);
                if (p.dist2Type > 0) y = stage (y, c.drive2G * (0.4f + 0.6f * dEnv), p.dist2Type - 1, p.bias2, p.mix2, p.autoGain);
                y = lc.hp (y, c.lcC);
                y = hc2.lp (hc1.lp (y, c.hc), c.hc);
                if (std::abs (p.bodyDb) > 0.05f) y += c.bodyG * bodyLp.lp (y, c.body);
                if (p.cleanSubHz > 20.f)
                {
                    y = subY2.hp (subY1.hp (y, c.sub), c.sub);
                    subOut = subS2.lp (subS1.lp (inTail ? s : 0.f, c.sub), c.sub) * 1.1f;
                }
                if (p.scream > 0.001f) y = std::tanh (y + p.scream * dEnv * 3.f * sc.bp (y, c.scream));
                if (p.crunch > 0.001f) y += p.crunch * std::exp (-ms / juce::jmax (5.f, p.crunchDecayMs)) * std::tanh (crHp.hp (y, c.crC) * 6.f) * 0.6f;
                if (gritNow > 0.001f) y += gn * gritNow * 0.55f * (0.2f + std::abs (y));
                const float dc = y - dcx + c.dcR * dcy; dcx = y; dcy = dc; y = dc;
                float a = (inTail ? p.amp.eval (ms * toPm, ampSeg) : 0.f) * c.tailG;
                if (p.duck > 0.001f && pe > 0.f) a *= 1.f - p.duck * juce::jmin (1.f, pe);      // the tok gets room, the tail comes in under it
                y *= a; subOut *= a;
                if (roomAfter) y += rl2.hp (rl1.hp (room.process (y), c.rlc), c.rlc) * p.room * 1.6f;
                if (mute & LTail) y = 0.f;
                subOut *= (mute & LSub) ? 0.f : c.subG;
            }

            // ---------------- TEXTURE
            float tex = 0.f;
            if (c.texG > 0.f && ms < texEnd && ! (mute & LTexture))
            {
                float src;
                if (p.texSource >= 2)
                {
                    const float th = juce::jlimit (10.f, (float) rate * 0.2f, hz * std::exp2 (p.texSemis / 12.f)), dt = th * inv;
                    tphase += dt; if (tphase >= 1.f) tphase -= 1.f;
                    src = p.texSource == 2 ? (tphase < 0.5f ? 1.f : -1.f) + polyBlep (tphase, dt) - polyBlep (wrap01 (tphase + 0.5f), dt)
                                           : 2.f * tphase - 1.f - polyBlep (tphase, dt);
                }
                else
                {
                    tnoise ^= tnoise << 13; tnoise ^= tnoise >> 17; tnoise ^= tnoise << 5;
                    const float wn = (float) (int32_t) tnoise * (1.f / 2147483648.f);
                    if (p.texSource == 1)
                    {
                        // pink (Paul Kellet, economy)
                        pink[0] = 0.99765f * pink[0] + wn * 0.0990460f;
                        pink[1] = 0.96300f * pink[1] + wn * 0.2965164f;
                        pink[2] = 0.57000f * pink[2] + wn * 1.0526913f;
                        src = (pink[0] + pink[1] + pink[2] + wn * 0.1848f) * 0.25f;
                    }
                    else src = wn;
                }
                float v = tf.bp (src, c.tex) * c.tex.k * 1.4f;
                if (p.texDriveDb > 0.05f) v = std::tanh (v * c.texDrv) / std::tanh (juce::jmax (1.f, c.texDrv) * 0.5f);
                v = thp2.hp (thp1.hp (v, c.texHpC), c.texHpC);              // never touches the low end
                tex = v * std::exp (-ms / juce::jmax (1.f, p.texDecayMs)) * juce::jmin (1.f, ms / 0.5f) * c.texG;
            }

            // ---------------- sum: punch / tail / texture / click -> BUS -> + SUB
            float out = y + tex;
            if (! (mute & LPunch))
            {
                if (p.punchIntoDist && punch != 0.f)
                {
                    float pd = shape (punch / juce::jmax (1.0e-6f, c.punchG) * std::pow (10.f, p.driveDb / 20.f), p.distType);
                    pd = plc.hp (pd, c.lcC);
                    pd = phc2.lp (phc1.lp (pd, c.hc), c.hc);
                    if (p.scream > 0.001f) pd = std::tanh (pd + p.scream * 3.f * psc.bp (pd, c.scream));
                    out += pd * c.punchG * punchGate;
                }
                else out += punch;
            }
            if (p.clickDb > -59.f && ms < p.clickMs * 3.f && ! (mute & LClick))
            {
                noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
                const float nz = (float) (int32_t) noise * (1.f / 2147483648.f);
                out += ck.bp (nz, c.click) * std::exp (-ms / juce::jmax (0.3f, p.clickMs) * 2.3f) * c.clickG;
            }
            if (p.busType > 0)
            {
                float b = stage (out, c.busG, p.busType - 1, 0.f, p.busMix, true);
                const float dc = b - bdcx + c.dcR * bdcy; bdcx = b; bdcy = dc;
                out = dc;
            }
            out += subOut;
            out *= vel;
            if (fadeStep > 0.f) { fade -= fadeStep; if (fade <= 0.f) { fade = 0.f; active = false; } out *= fade; }
            t += inv;
            return out;
        }
    private:
        struct OnePole { float z = 0.f; inline float lp (float x, float c) { z += c * (x - z); return z; } inline float hp (float x, float c) { z += c * (x - z); return x - z; } };
        double rate = 192000.0, t = 0.0;
        float inv = 1.f / 192000.f, phase = 0.f, phaseB = 0.f, pphase = 0.f, tphase = 0.f, vel = 1.f, baseHz = 49.f, fade = 1.f, fadeStep = 0.f;
        float dcx = 0.f, dcy = 0.f, bdcx = 0.f, bdcy = 0.f;
        bool active = false;
        int ampSeg = 0, driveSeg = 0, pitchSeg = 0;
        OnePole pre, lc, crHp, grHp, plc, thp1, thp2;
        doomsday::dsp::Svf hc1, hc2, sc, ck, bodyLp, ch1, ch2, phc1, phc2, psc, prf, pf, pres, pnz, tf, rl1, rl2;
        doomsday::dsp::Biquad midF;
        doomsday::dsp::Svf subY1, subY2, subS1, subS2;
        doomsday::kick::KickRoom room;
        std::array<float, 3> pink {};
        std::array<float, 6> uph {}, driftHz {}, driftPh {}, uRatio { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
        uint32_t uCount = 0;
        uint32_t noise = 0x12345678u, gnoise = 0x9e3779b9u, cnoise = 0x7f4a7c15u, pnoise = 0x2545f491u, tnoise = 0x68e31da4u;
    };
}
