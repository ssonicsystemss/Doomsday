#pragma once
// =====================================================================
//  4GE module 06 - FILTER ENGINE + RESONANCE ENGINE (pure DSP: no GUI, no files, no allocation after construction)
//   8 filter instances + 2 resonators, each placed on a LOCATION (GLOBAL / PUNCH / BODY / TAIL / SUB / TEXTURE / LAYER 1-8 / SCREECH)
//   BEFORE or AFTER that location's distortion (module 05). Filters of one place run in ORDER as SERIAL / PARALLEL A-B-C / SPLIT
//   (LR4 bands, sum flat) blocks; the resonators follow them.
//   Filter types - each its own algorithm: LP / HP / BP / NOTCH (TPT SVF cascades, 6-48 dB), LADDER (4 poles, tanh feedback),
//   DIODE LADDER (coupled asymmetric diode stages), STATE VARIABLE (continuous LP-BP-HP), COMB (fractional delay + loop),
//   FORMANT (3 band passes on a vowel table), ALL PASS (4 x 2nd order). Every filter: own ADSR, key / velocity tracking,
//   stereo / mono / mid / side processing, smoothed cutoff / resonance / drive / mix.
//   Resonators: CLEAN (one ringing mode), METALLIC (6 inharmonic modes), SCREECH (waveguide with saturation), RUBBER
//   (tension modulated mode), FORMANT (3 vowel modes), CUSTOM (8 harmonic modes, drawn amplitudes); excited by the input signal,
//   an internal SINE / SAW / SQUARE / NOISE / IMPULSE or the screech oscillators; pitch envelope; stabilised FEEDBACK.
// =====================================================================
#include "DoomsdayDSP.h"
#include "../Env/MultiEnv.h"
#include <array>
#include <cmath>

namespace fge::dsp::m06
{
    using doomsday::dsp::SvfCoefs;
    using doomsday::dsp::Svf;
    enum FType { FLp = 0, FHp, FBp, FNotch, FLadder, FDiode, FSvf, FComb, FFormant, FAllpass, NumFTypes };
    enum Loc { LGlobal = 0, LPunch, LBody, LTail, LSub, LTexture, LLayer1, LScreech = 14, NumLocs = 15 };
    enum RMode { RClean = 0, RMetallic, RScreech, RRubber, RFormant, RCustom };
    enum RSrc { RsSignal = 0, RsSine, RsSaw, RsSquare, RsNoise, RsImpulse, RsScreech };
    enum Stereo { StStereo = 0, StMono, StMid, StSide };
    constexpr int numFilters = 8, numRes = 2, combLen = 8192;
    inline bool slopeWorks (int t) { return t <= FNotch || t == FSvf; }

    // what moves the filters in time (per sample at the engine rate)
    struct Ctx { float ms = 1.0e9f, vel = 1.f, noteHz = 49.f, lengthMs = 450.f, relMs = -1.f; float scrOsc = 0.f; };

    // ------------------------------------------------------------------ ADSR (+ curve, invert, sync to the kick length)
    struct EnvSet { float a = 1.f, d = 150.f, s = 0.f, r = 100.f; int curve = 1; bool inv = false, sync = false; };
    inline float rise (float x, int c) { x = juce::jlimit (0.f, 1.f, x); return c == 0 ? x : c == 1 ? x * x * x : 1.f - (1.f - x) * (1.f - x) * (1.f - x); }
    inline float fall (float x, int c) { x = juce::jlimit (0.f, 1.f, x); return c == 0 ? 1.f - x : c == 1 ? (1.f - x) * (1.f - x) * (1.f - x) : 1.f - x * x * x; }
    inline float envValue (const EnvSet& e, const Ctx& c)
    {
        const float sc = e.sync ? juce::jlimit (0.1f, 10.f, c.lengthMs / 450.f) : 1.f;           // (times designed at the default 450 ms)
        const float a = e.a * sc, d = e.d * sc, r = e.r * sc;
        auto at = [&] (float ms)
        {
            if (ms < a) return rise (ms / juce::jmax (0.01f, a), e.curve);
            const float t = ms - a;
            if (t < d) return e.s + (1.f - e.s) * fall (t / juce::jmax (0.01f, d), e.curve);
            return e.s;
        };
        // RELEASE: from the note off, or so that it ends with the kick (one-shot kicks) - never before ATTACK + DECAY
        const float relAt = juce::jmax (a + d, c.relMs >= 0.f ? juce::jmin (c.relMs, c.lengthMs - r) : c.lengthMs - r);
        float v;
        if (c.ms < relAt) v = at (c.ms);
        else { const float v0 = at (relAt); v = v0 * fall ((c.ms - relAt) / juce::jmax (0.01f, r), e.curve); }
        return e.inv ? 1.f - v : v;
    }

    // ------------------------------------------------------------------ one filter instance, cooked
    struct FiltSettings
    {
        bool on = false; int loc = LTail, pos = 0, order = 1, path = 0, band = 0, type = FLp, slope = 1, stereo = StStereo;
        float cut = 2000.f, res = 0.2f, drive = 0.f, mix = 1.f, key = 0.f, env = 0.f, vel = 0.f, svfMode = 0.f, vowel = 0.f, envRes = 0.f, envDrv = 0.f;
        EnvSet e;
    };
    // vowel formants (Hz at a 1 kHz cutoff): A E I O U
    inline const std::array<std::array<float, 3>, 5>& vowels()
    {
        static const std::array<std::array<float, 3>, 5> v { { { 800.f, 1150.f, 2900.f }, { 400.f, 1600.f, 2700.f }, { 270.f, 2290.f, 3010.f },
                                                              { 450.f, 800.f, 2830.f }, { 325.f, 700.f, 2530.f } } };
        return v;
    }
    inline std::array<float, 3> vowelAt (float v)                    // 0..4, continuous (MORPH between the vowels in octaves)
    {
        v = juce::jlimit (0.f, 4.f, v);
        const int i = juce::jmin (3, (int) v); const float t = v - (float) i;
        std::array<float, 3> o {};
        for (int k = 0; k < 3; ++k) o[(size_t) k] = std::exp2 (std::log2 (vowels()[(size_t) i][(size_t) k]) * (1.f - t) + std::log2 (vowels()[(size_t) i + 1][(size_t) k]) * t);
        return o;
    }
    inline float soft (float x) { return x / (1.f + std::abs (x)); }
    inline float diode (float x) { return x > 0.f ? std::tanh (x) : 0.8f * std::tanh (1.25f * x); }

    // the running state of one filter on one channel
    struct FiltCh
    {
        std::array<Svf, 4> s {}; float op = 0.f; std::array<float, 4> z {};
        std::array<float, combLen> comb {}; int ci = 0; float combLp = 0.f;
        void reset() { s = {}; op = 0.f; z = {}; comb.fill (0.f); ci = 0; combLp = 0.f; }
    };
    // per filter instance: the smoothed / cooked coefficients (both channels share them)
    struct FiltState
    {
        std::array<FiltCh, 2> ch;
        float cutLog = -1.f, res = 0.f, drive = 0.f, mix = 1.f; int tick = 0;
        SvfCoefs c, cBw, cBw2; float g = 0.1f, G = 0.1f, opA = 0.1f, combD = 100.f, combFb = 0.f;
        std::array<SvfCoefs, 3> fmt; float fmtNorm = 1.f;
        void reset() { for (auto& x : ch) x.reset(); cutLog = -1.f; tick = 0; }

        // control rate (every 16 samples): the envelope / key / velocity move cutoff, resonance, drive
        void update (const FiltSettings& f, const Ctx& cx, double rate)
        {
            const float ev = (f.env != 0.f || f.envRes != 0.f || f.envDrv != 0.f) ? envValue (f.e, cx) : 0.f;
            float oct = f.env * ev * 6.f + f.vel * 3.f * (cx.vel - 0.5f);
            if (f.key != 0.f) oct += f.key * std::log2 (juce::jmax (5.f, cx.noteHz) / 49.f);
            const float target = std::log2 (juce::jlimit (20.f, 20000.f, f.cut)) + oct;
            const float rT = juce::jlimit (0.f, 1.f, f.res + f.envRes * ev), dT = juce::jlimit (0.f, 1.f, f.drive + f.envDrv * ev);
            const float k = 1.f - std::exp (-16.f / (0.004f * (float) rate));                       // ~4 ms
            if (cutLog < 0.f) { cutLog = target; res = rT; drive = dT; mix = f.mix; }
            cutLog += k * (target - cutLog); res += k * (rT - res); drive += k * (dT - drive); mix += k * (f.mix - mix);
            const float hz = juce::jlimit (20.f, (float) rate * 0.45f, std::exp2 (cutLog));
            const float q = 0.707f + res * res * 24.f;
            c = doomsday::dsp::svfCoefs (hz, 1.f / q, rate);
            cBw = doomsday::dsp::svfCoefs (hz, 1.414f, rate);
            cBw2 = doomsday::dsp::svfCoefs (hz, 0.765f, rate);
            g = std::tan (juce::MathConstants<float>::pi * hz / (float) rate); G = g / (1.f + g);
            opA = 1.f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) rate);
            combD = juce::jlimit (2.f, (float) combLen - 4.f, (float) rate / juce::jmax (22.f, hz));
            combFb = 0.985f * res;
            if (f.type == FFormant)
            {
                const auto v = vowelAt (f.vowel);
                const float sc = juce::jlimit (0.25f, 4.f, hz / 1000.f), fq = 4.f + res * 22.f;
                for (int i = 0; i < 3; ++i) fmt[(size_t) i] = doomsday::dsp::svfCoefs (juce::jlimit (30.f, (float) rate * 0.45f, v[(size_t) i] * sc), 1.f / fq, rate);
                fmtNorm = 1.f / (1.f + 0.6f + 0.4f);
            }
        }
        inline float run (const FiltSettings& f, FiltCh& h, float x)
        {
            const float dry = x;
            if (drive > 0.001f) { const float gd = 1.f + 8.f * drive; x = std::tanh (x * gd) / std::tanh (gd); }
            float y = x;
            switch (f.type)
            {
                case FLp: case FHp: case FBp: case FNotch: case FSvf:
                {
                    if (f.slope == 0 && (f.type == FLp || f.type == FHp))                      // 6 dB: one pole
                    { h.op += opA * (x - h.op); y = f.type == FLp ? h.op : x - h.op; break; }
                    const int n = f.slope <= 1 ? 1 : f.slope == 2 ? 2 : 4;
                    for (int k = 0; k < n; ++k)
                    {
                        const SvfCoefs& cc = k == n - 1 ? c : (n == 2 ? cBw : (k % 2 == 0 ? cBw : cBw2));
                        float l, b; h.s[(size_t) k].tick (y, cc, l, b);
                        const float hp = y - cc.k * b - l;
                        switch (f.type)
                        {
                            case FLp: y = l; break;
                            case FHp: y = hp; break;
                            case FBp: y = b * cc.k; break;                                      // (0 dB at the centre)
                            case FNotch: y = y - cc.k * b; break;
                            default:                                                             // STATE VARIABLE: LP -> BP -> HP
                            {
                                const float m = juce::jlimit (0.f, 1.f, f.svfMode);
                                y = m < 0.5f ? l * (1.f - 2.f * m) + b * cc.k * 2.f * m : b * cc.k * (2.f - 2.f * m) + hp * (2.f * m - 1.f);
                            }
                        }
                    }
                    break;
                }
                case FLadder:                                                                   // 4 one-pole stages, tanh feedback
                {
                    const float kk = 3.9f * res;
                    float u = x * (1.f + 0.5f * kk) - kk * std::tanh (h.z[3]);
                    for (auto& zi : h.z) { const float v = (u - zi) * G; const float lp = v + zi; zi = lp + v; u = lp; }
                    y = u;
                    break;
                }
                case FDiode:                                                                    // coupled diode stages (asymmetric, gritty)
                {
                    const float kk = 4.6f * res;
                    const float in = x * (1.f + 0.8f * kk) - kk * h.z[3];
                    auto& z = h.z;
                    const float a = diode (in);
                    z[0] += G * (a - diode (z[0]) + 0.3f * (diode (z[1]) - diode (z[0])));
                    z[1] += G * (diode (z[0]) - diode (z[1]) + 0.3f * (diode (z[2]) - diode (z[1])));
                    z[2] += G * (diode (z[1]) - diode (z[2]) + 0.3f * (diode (z[3]) - diode (z[2])));
                    z[3] += G * (diode (z[2]) - diode (z[3]));
                    for (auto& zz : z) zz = juce::jlimit (-4.f, 4.f, std::isfinite (zz) ? zz : 0.f);
                    y = z[3] * 1.3f;
                    break;
                }
                case FComb:                                                                     // fractional delay at 1 / cutoff, damped loop
                {
                    const float rp = (float) h.ci - combD;
                    const float rpos = rp < 0.f ? rp + (float) combLen : rp;
                    const int i0 = (int) rpos; const float fr = rpos - (float) i0;
                    const float d0 = h.comb[(size_t) (i0 % combLen)], d1 = h.comb[(size_t) ((i0 + 1) % combLen)];
                    const float dl = d0 + fr * (d1 - d0);
                    h.combLp += 0.6f * (dl - h.combLp);
                    const float w = soft (x + combFb * h.combLp);
                    h.comb[(size_t) h.ci] = w; h.ci = (h.ci + 1) % combLen;
                    y = w * (1.f - 0.6f * combFb);
                    break;
                }
                case FFormant:
                {
                    float sum = 0.f; const float gains[3] { 1.f, 0.6f, 0.4f };
                    for (int i = 0; i < 3; ++i) { float l, b; h.s[(size_t) i].tick (x, fmt[(size_t) i], l, b); sum += b * fmt[(size_t) i].k * gains[i]; }
                    y = sum * fmtNorm * 1.6f;
                    break;
                }
                case FAllpass:                                                                  // 4 x 2nd order all pass (phase; MIX = notches)
                {
                    for (int k = 0; k < 4; ++k) { float l, b; h.s[(size_t) k].tick (y, c, l, b); y = y - 2.f * c.k * b; }
                    break;
                }
                default: break;
            }
            const float r = dry + (y - dry) * mix;
            return std::isfinite (r) ? r : 0.f;
        }
        inline void process (const FiltSettings& f, float& l, float& r)
        {
            switch (f.stereo)
            {
                case StMono: { const float m = run (f, ch[0], 0.5f * (l + r)); l = r = m; break; }
                case StMid:  { const float m = run (f, ch[0], 0.5f * (l + r)), s = 0.5f * (l - r); l = m + s; r = m - s; break; }
                case StSide: { const float m = 0.5f * (l + r), s = run (f, ch[0], 0.5f * (l - r)); l = m + s; r = m - s; break; }
                default:     l = run (f, ch[0], l); r = run (f, ch[1], r); break;
            }
        }
    };

    // ------------------------------------------------------------------ resonator, cooked
    struct ResSettings
    {
        bool on = false; int loc = LTail, pos = 0, mode = RClean, src = RsSignal;
        float freq = 1800.f, q = 12.f, decay = 300.f, amount = 0.5f, damp = 0.3f, exc = 0.5f, fb = 0.f, track = 0.f, mix = 1.f;
        float pAmt = 0.f, pStart = 8000.f, pEnd = 1800.f; EnvSet pe;
        const env::Shape* custom = nullptr;
    };
    // a two-pole ringing mode (peak gain 1, ring time = DECAY)
    struct Mode
    {
        float a1 = 0.f, a2 = 0.f, b0 = 0.f, y1 = 0.f, y2 = 0.f;
        void set (float hz, float decayMs, double rate)
        {
            const float w = juce::MathConstants<float>::twoPi * juce::jlimit (20.f, (float) rate * 0.45f, hz) / (float) rate;
            const float rr = std::exp (-1.f / juce::jmax (1.f, decayMs * 0.001f * (float) rate / 6.9f));        // -60 dB after DECAY
            a1 = 2.f * rr * std::cos (w); a2 = -rr * rr;
            b0 = (1.f - rr) * std::sqrt (1.f + rr * rr - 2.f * rr * std::cos (2.f * w));
        }
        inline float tick (float x) { const float y = b0 * x + a1 * y1 + a2 * y2; y2 = y1; y1 = y; return y; }
    };
    struct ResCh
    {
        std::array<Mode, 8> m {}; Svf excBp, loopBp; float dampLp = 0.f; std::array<float, combLen> wg {}; int wi = 0; float wgLp = 0.f, last = 0.f, follow = 0.f;
        void reset() { m = {}; excBp = {}; loopBp = {}; wg.fill (0.f); wi = 0; wgLp = 0.f; last = 0.f; follow = 0.f; }
    };
    struct ResState
    {
        std::array<ResCh, 2> ch;
        int tick = 0; float hz = 1800.f, ph = 0.f, fbScale = 1.f, energy = 0.f; uint32_t rng = 0x2545f491u; bool impulseDone = true; float lastMs = 1.0e9f;
        SvfCoefs excC, loopC; float wgD = 100.f, wgG = 0.9f, wgDamp = 0.5f, dampA = 1.f;
        std::array<float, 8> amp {};
        int nModes = 1;
        void reset() { for (auto& c : ch) c.reset(); tick = 0; ph = 0.f; fbScale = 1.f; energy = 0.f; impulseDone = true; lastMs = 1.0e9f; }

        float baseHz (const ResSettings& s, const Ctx& cx) const
        {
            float f = s.freq;
            if (s.track != 0.f) f *= std::pow (juce::jmax (5.f, cx.noteHz) / 49.f, s.track);
            if (s.pAmt > 0.f)
            {
                const float e = envValue (s.pe, cx);                                       // 0 = END, 1 = START
                const float envHz = std::exp2 (std::log2 (s.pEnd) + e * (std::log2 (s.pStart) - std::log2 (s.pEnd)));
                f = std::exp2 (std::log2 (f) * (1.f - s.pAmt) + std::log2 (envHz) * s.pAmt);
            }
            return juce::jlimit (20.f, 20000.f, f);
        }
        void update (const ResSettings& s, const Ctx& cx, double rate)
        {
            hz = baseHz (s, cx);
            excC = doomsday::dsp::svfCoefs (hz, 1.f / juce::jmax (0.1f, s.q), rate);
            const float dampHz = std::exp2 (std::log2 (20000.f) * (1.f - s.damp) + std::log2 (juce::jmax (40.f, hz * 0.5f)) * s.damp);
            dampA = 1.f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (dampHz, (float) rate * 0.45f) / (float) rate);
            const float dec = s.decay;
            auto setAll = [&] (int n, const float* ratio, const float* a)
            {
                nModes = n;
                for (int k = 0; k < n; ++k)
                {
                    const float dk = dec * (1.f - s.damp * 0.9f * (float) k / (float) juce::jmax (1, n - 1));
                    float fk = hz * ratio[k];
                    if (s.mode == RRubber) fk *= 1.f + 0.6f * juce::jlimit (0.f, 1.f, ch[0].follow * 3.f);   // tension: pitch falls as it rings out
                    for (auto& c : ch) c.m[(size_t) k].set (juce::jmin (fk, (float) rate * 0.45f), dk, rate);
                    amp[(size_t) k] = fk < (float) rate * 0.45f ? a[k] : 0.f;
                }
            };
            switch (s.mode)
            {
                case RMetallic: { static const float r[6] { 1.f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f }, a[6] { 1.f, 0.7f, 0.5f, 0.35f, 0.25f, 0.18f }; setAll (6, r, a); break; }
                case RFormant:  { static const float r[3] { 1.f, 1.4375f, 3.625f }, a[3] { 1.f, 0.7f, 0.4f }; setAll (3, r, a); break; }
                case RCustom:
                {
                    float r[8], a[8];
                    for (int k = 0; k < 8; ++k) { r[k] = (float) (k + 1); int seg = 0; a[k] = s.custom != nullptr ? juce::jlimit (0.f, 1.f, s.custom->eval ((float) k / 7.f, seg)) : 1.f / (float) (k + 1); }
                    setAll (8, r, a); break;
                }
                case RScreech:
                {
                    wgD = juce::jlimit (2.f, (float) combLen - 4.f, (float) rate / hz);
                    wgG = std::pow (10.f, -3.f * (1.f / hz) / juce::jmax (0.001f, dec * 0.001f));        // loop gain from the ring time (T60)
                    wgDamp = 1.f - 0.85f * s.damp;
                    loopC = doomsday::dsp::svfCoefs (hz, 1.f / juce::jmax (0.5f, s.q * 0.25f), rate);
                    nModes = 0; break;
                }
                default: { static const float r[1] { 1.f }, a[1] { 1.f }; setAll (1, r, a); break; }
            }
        }
        inline float excite (const ResSettings& s, float in, const Ctx& cx, double rate)
        {
            const float env = std::exp (-cx.ms / juce::jmax (1.f, s.decay));
            switch (s.src)
            {
                case RsSine:   { ph += hz / (float) rate; ph -= std::floor (ph); return std::sin (juce::MathConstants<float>::twoPi * ph) * env; }
                case RsSaw:    { ph += 0.5f * hz / (float) rate; ph -= std::floor (ph); return (2.f * ph - 1.f) * env; }
                case RsSquare: { ph += 0.5f * hz / (float) rate; ph -= std::floor (ph); return (ph < 0.5f ? 1.f : -1.f) * env; }
                case RsNoise:  { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return ((float) (rng >> 8) * (1.f / 8388608.f) - 1.f) * env; }
                case RsImpulse:
                {
                    if (cx.ms < lastMs) impulseDone = false;                                  // a new kick
                    lastMs = cx.ms;
                    if (! impulseDone) { impulseDone = true; return 30.f; }
                    return 0.f;
                }
                case RsScreech: return cx.scrOsc;
                default: return in;
            }
        }
        inline float runCh (const ResSettings& s, ResCh& h, float e)
        {
            float x = e * (0.25f + 3.75f * s.exc);
            h.dampLp += dampA * (x - h.dampLp); x = h.dampLp;                               // DAMPING: highs taken out before it rings
            x = h.excBp.bp (x, excC) * excC.k * 2.f + x * 0.2f;                               // Q: how narrow the excitation is
            x += s.fb * fbScale * std::tanh (h.last);                                           // FEEDBACK (stabilised below)
            float y = 0.f;
            if (s.mode == RScreech)
            {
                const float rp = (float) h.wi - wgD; const float rpos = rp < 0.f ? rp + (float) combLen : rp;
                const int i0 = (int) rpos; const float fr = rpos - (float) i0;
                const float dl = h.wg[(size_t) (i0 % combLen)] + fr * (h.wg[(size_t) ((i0 + 1) % combLen)] - h.wg[(size_t) (i0 % combLen)]);
                h.wgLp += wgDamp * (dl - h.wgLp);
                const float loop = h.loopBp.bp (h.wgLp, loopC) * loopC.k;
                const float w = std::tanh (1.5f * (x + wgG * (0.6f * h.wgLp + 0.8f * loop)));    // saturation in the loop = screech
                h.wg[(size_t) h.wi] = w; h.wi = (h.wi + 1) % combLen;
                y = w;
            }
            else for (int k = 0; k < nModes; ++k) y += h.m[(size_t) k].tick (x) * amp[(size_t) k];
            if (! std::isfinite (y)) { h.reset(); y = 0.f; }
            h.last = y;
            h.follow += (std::abs (y) > h.follow ? 0.01f : 0.0005f) * (std::abs (y) - h.follow);
            return y;
        }
        inline void process (const ResSettings& s, float& l, float& r, const Ctx& cx, double rate)
        {
            const float dryL = l, dryR = r;
            const float eM = excite (s, 0.5f * (l + r), cx, rate);
            const bool sig = s.src == RsSignal;
            const float eL = sig ? l : eM, eR = sig ? r : eM;
            float yl = runCh (s, ch[0], eL), yr = runCh (s, ch[1], eR);
            // energy guard: the loop never runs away (FEEDBACK is lowered while it is too hot)
            energy += 0.001f * (yl * yl + yr * yr - energy);
            if (energy > 4.f) fbScale = juce::jmax (0.f, fbScale - 0.01f); else fbScale = juce::jmin (1.f, fbScale + 0.0002f);
            yl = 4.f * std::tanh (yl * 0.25f); yr = 4.f * std::tanh (yr * 0.25f);
            const float keep = sig ? 1.f - s.amount : 1.f;
            const float wl = dryL * keep + yl * s.amount, wr = dryR * keep + yr * s.amount;
            l = dryL + (wl - dryL) * s.mix; r = dryR + (wr - dryR) * s.mix;
        }
    };

    // ------------------------------------------------------------------ the cooked bank: settings + the order per place
    struct Bank
    {
        std::array<FiltSettings, numFilters> f {};
        std::array<ResSettings, numRes> r {};
        std::array<std::array<std::array<int, numFilters>, 2>, NumLocs> list {};   // [loc][pos] -> filter indices in ORDER
        std::array<std::array<int, 2>, NumLocs> count {};
        std::array<std::array<std::array<int, numRes>, 2>, NumLocs> rlist {};
        std::array<std::array<int, 2>, NumLocs> rcount {};
        SvfCoefs xLow, xHigh;
        bool any = false;
        bool at (int loc, int pos) const { return count[(size_t) loc][(size_t) pos] + rcount[(size_t) loc][(size_t) pos] > 0; }
        bool atLoc (int loc) const { return at (loc, 0) || at (loc, 1); }
    };

    // ------------------------------------------------------------------ the running state of all instances (one per engine / per voice)
    struct State
    {
        std::array<FiltState, numFilters> f;
        std::array<ResState, numRes> r;
        struct Split { Svf l1a, l1b, h1a, h1b, l2a, l2b, h2a, h2b, la, lb, ha, hb; };
        std::array<std::array<Split, 2>, NumLocs * 2> split {};                  // [loc * 2 + pos][channel]
        void reset() { for (auto& x : f) x.reset(); for (auto& x : r) x.reset(); split = {}; }

        // SPLIT: LR4 bands, the low band gets the high crossover's all pass (sum flat)
        static inline void bands (Split& s, const Bank& b, float x, float& low, float& mid, float& high)
        {
            low = s.l1b.lp (s.l1a.lp (x, b.xLow), b.xLow);
            const float rest = s.h1b.hp (s.h1a.hp (x, b.xLow), b.xLow);
            mid = s.l2b.lp (s.l2a.lp (rest, b.xHigh), b.xHigh);
            high = s.h2b.hp (s.h2a.hp (rest, b.xHigh), b.xHigh);
            low = s.lb.lp (s.la.lp (low, b.xHigh), b.xHigh) + s.hb.hp (s.ha.hp (low, b.xHigh), b.xHigh);
        }
        inline void runOne (const Bank& b, int i, float& l, float& r, const Ctx& cx, double rate)
        {
            auto& st = f[(size_t) i];
            if ((st.tick++ & 15) == 0) st.update (b.f[(size_t) i], cx, rate);
            st.process (b.f[(size_t) i], l, r);
        }
        inline void process (const Bank& b, int loc, int pos, float& l, float& r, const Ctx& cx, double rate)
        {
            const int n = b.count[(size_t) loc][(size_t) pos];
            const auto& L = b.list[(size_t) loc][(size_t) pos];
            int k = 0;
            while (k < n)
            {
                const auto& first = b.f[(size_t) L[(size_t) k]];
                if (first.band != 0)                                                  // ---- SPLIT block
                {
                    int e = k; while (e < n && b.f[(size_t) L[(size_t) e]].band != 0) ++e;
                    auto& sp = split[(size_t) (loc * 2 + pos)];
                    float bl[3], br[3];
                    bands (sp[0], b, l, bl[0], bl[1], bl[2]); bands (sp[1], b, r, br[0], br[1], br[2]);
                    for (int j = k; j < e; ++j) { const int fi = L[(size_t) j]; const int bd = b.f[(size_t) fi].band - 1; runOne (b, fi, bl[bd], br[bd], cx, rate); }
                    l = bl[0] + bl[1] + bl[2]; r = br[0] + br[1] + br[2];
                    k = e;
                }
                else if (first.path != 0)                                             // ---- PARALLEL block (level kept: the average)
                {
                    int e = k; while (e < n && b.f[(size_t) L[(size_t) e]].path != 0 && b.f[(size_t) L[(size_t) e]].band == 0) ++e;
                    float sl = 0.f, sr = 0.f; int nb = 0;
                    for (int p = 1; p <= 3; ++p)
                    {
                        float yl = l, yr = r; bool used = false;
                        for (int j = k; j < e; ++j) { const int fi = L[(size_t) j]; if (b.f[(size_t) fi].path == p) { runOne (b, fi, yl, yr, cx, rate); used = true; } }
                        if (used) { sl += yl; sr += yr; ++nb; }
                    }
                    if (nb > 0) { l = sl / (float) nb; r = sr / (float) nb; }
                    k = e;
                }
                else { runOne (b, L[(size_t) k], l, r, cx, rate); ++k; }             // ---- SERIAL
            }
            // the resonators of this place
            for (int j = 0; j < b.rcount[(size_t) loc][(size_t) pos]; ++j)
            {
                const int ri = b.rlist[(size_t) loc][(size_t) pos][(size_t) j];
                auto& rs = this->r[(size_t) ri];
                if ((rs.tick++ & 15) == 0) rs.update (b.r[(size_t) ri], cx, rate);
                rs.process (b.r[(size_t) ri], l, r, cx, rate);
            }
        }
    };
}
