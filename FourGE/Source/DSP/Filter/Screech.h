#pragma once
// =====================================================================
//  4GE module 06 - SCREECH ENGINE (pure DSP). One screech per kick, made only from synthesis:
//   4 oscillators (SINE / SAW / SQUARE / TRIANGLE / PULSE band-limited, OSC TABLE = a module 03 wavetable; detune, FM + RING
//   from another oscillator, pan) -> the MODE's own algorithm -> 2 screech filters (module 06 filter algorithms) -> level / width.
//   MODES (each a different signal path):
//     TONAL     oscillators -> modulated filters
//     METALLIC  oscillators excite 6 inharmonic modes (METAL = how inharmonic)
//     FORMANT   oscillators -> 3 vowel band passes (VOWEL -> VOWEL END morph)
//     FM SCREECH  2-operator FM per oscillator (RATIO, INDEX, operator FEEDBACK)
//     NOISE     coloured noise -> two tuned comb resonators
//     FEEDBACK  oscillators -> a saturated delay loop tuned to the pitch (or FEEDBACK DELAY), filter + STABILITY guard
//     HYBRID    FM -> FORMANT -> FEEDBACK in series
//     CUSTOM    all six paths in parallel, own levels
//   Own AMP envelope (A / HOLD / D / S / R), MOD envelope (-> pitch, FM, cutoff, resonance, feedback, formant, drive, detune),
//   PITCH envelope (START -> END, end drop), and TIMING (start mode: manual / body end / + offset / tail / kick / host sync,
//   length / tail / auto, fades, pre-trigger, quantize).
// =====================================================================
#include "Filters.h"
#include "../Osc/Wavetable.h"

namespace fge::dsp::scr
{
    // the screech register: RELATIVE pitch = the played note this many times up (4 = two octaves: G1 49 Hz -> 196 Hz)
    // (owner 2026-11-05: three octaves up was too high)
    constexpr float registerMul = 4.f, registerHz = 49.f * registerMul;
    enum Mode { Tonal = 0, Metallic, Formant, Fm, Noise, Feedback, Hybrid, Custom };
    enum Wave { WSine = 0, WSaw, WSquare, WTri, WPulse, WTable };
    enum Start { SManual = 0, SBodyEnd, SBodyOffset, STail, SKick, SHost };
    enum Out { OTail = 0, OTexture, OMaster, OPunch, OBody, OSub };

    struct OscS { bool on = false; int wave = WSaw, fmSrc = 0, ringSrc = 0; float level = 0.8f, pitch = 0.f, fine = 0.f, phase01 = 0.f, detune = 0.f, pw = 0.3f, fm = 0.f, ring = 0.f, pan = 0.f; };
    struct FltS { bool on = false; int type = m06::FBp; float cut = 3000.f, res = 0.5f, drive = 0.f, key = 1.f, env = 0.f, mix = 1.f; };
    struct Settings
    {
        bool on = false; int mode = Tonal, output = OTail, pitchMode = 0; float level = 0.5f, width = 0.5f, baseHz = registerHz;
        float fmRatio = 2.f, fmIndex = 0.5f, fmFb = 0.2f, metal = 0.5f, metalQ = 0.6f, noiseCol = 0.f, noiseQ = 0.85f;
        float vowel = 0.f, vowelEnd = 2.f, fmtWidth = 0.5f, fmtReso = 0.6f; bool vowelAuto = false;
        float fbAmt = 0.6f, fbTone = 0.5f, fbDelay = 0.f, fbDrive = 0.5f, fbMix = 1.f, fbStab = 0.5f; int fbFilter = 2;
        std::array<float, 6> custom { 0.5f, 0.f, 0.f, 0.5f, 0.f, 0.f };
        float a = 2.f, hold = 0.f, d = 300.f, s = 0.6f, r = 60.f; int curve = 1;
        float modA = 0.f, modD = 200.f; int modCurve = 1;
        float mPitch = 0.f, mFm = 0.f, mCut = 0.f, mRes = 0.f, mFb = 0.f, mFmt = 0.f, mDrive = 0.f, mDetune = 0.f;
        float pStart = 12.f, pEnd = 0.f, pAmt = 0.f, pA = 0.f, pD = 150.f, pR = 0.f; int pCurve = 1;
        int startMode = SManual, quant = 0; float start = 180.f, offset = 0.f, length = 450.f, fadeIn = 5.f, fadeOut = 60.f;
        bool preTrig = false, syncTail = false, autoLen = false, followBody = true, freeTail = false;
        std::array<OscS, 4> osc; std::array<FltS, 2> flt;
        std::array<m06::FiltSettings, 2> fs;                     // the screech filters as module 06 filter settings (cooked)
    };
    // where / how long the screech plays in this kick (resolved at the hit from the real part times)
    struct Timing { float startMs = 180.f, endMs = 630.f, runFromMs = 180.f; };
    inline Timing resolve (const Settings& s, float bodyEndMs, float bodyLenMs, float tailStartMs, float tailLenMs, float kickLenMs, double bpm)
    {
        Timing t;
        const float bodyEnd = s.followBody ? bodyEndMs : bodyLenMs;
        float st;
        switch (s.startMode)
        {
            case SBodyEnd:    st = bodyEnd; break;
            case SBodyOffset: st = bodyEnd + s.offset; break;
            case STail:       st = tailStartMs + s.offset; break;
            case SKick:       st = s.offset; break;
            case SHost:       { const float step = (float) (60000.0 / juce::jlimit (30.0, 400.0, bpm > 0.0 ? bpm : 150.0) / 8.0); st = std::round (s.start / step) * step; break; }
            default:          st = s.start; break;
        }
        const float q = s.quant == 1 ? 1.f : s.quant == 2 ? 5.f : s.quant == 3 ? 10.f : 0.f;
        if (q > 0.f) st = std::round (st / q) * q;
        st = juce::jmax (0.f, st);
        float len = s.autoLen ? kickLenMs - st : s.syncTail ? tailLenMs : s.length;
        t.startMs = st;
        t.endMs = st + juce::jmax (1.f, len);
        if (! s.freeTail) t.endMs = juce::jmin (t.endMs, kickLenMs);            // never longer than the kick (unless INDEPENDENT)
        t.runFromMs = s.preTrig ? juce::jmax (0.f, st - 50.f) : st;              // PRE-TRIGGER: the engine starts 50 ms earlier, silent
        return t;
    }

    inline float blep (float t, float dt) { if (t < dt) { t /= dt; return t + t - t * t - 1.f; } if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; } return 0.f; }

    // ------------------------------------------------------------------ one screech
    class Voice
    {
    public:
        bool active = false;
        void start (const Settings& s, const Timing& t, uint32_t seed)
        {
            active = true; tm = t; fade = 1.f; fadeStep = 0.f; tick = 0; clock = 0.0; amp = 0.f;
            for (int i = 0; i < 4; ++i) { for (auto& p : ph[(size_t) i]) p = s.osc[(size_t) i].phase01; mph[(size_t) i] = 0.f; fmPrev[(size_t) i] = 0.f; last[(size_t) i] = 0.f; }
            for (auto& f : flt) f.reset();
            for (auto& c : ch) c = {};
            rng[0] = seed ^ 0x1234567u; rng[1] = seed ^ 0x7654321u;
            fbScale = 1.f; energy = 0.f;
        }
        void fadeOut (float ms, double rate) { if (active) fadeStep = 1.f / juce::jmax (1.f, ms * 0.001f * (float) rate); }
        // baseHz = the screech's base pitch now (its own clock runs from its hit - a fading old screech keeps its time)
        inline void process (const Settings& s, const OscTables& tb, float baseHz, double rate, float noteHz, float& outL, float& outR, float& oscMono)
        {
            outL = outR = oscMono = 0.f;
            if (! active) return;
            const float ms = (float) clock;
            clock += 1000.0 / rate;
            if (ms < tm.runFromMs) return;
            const float endAll = tm.endMs + s.r;
            if (ms > endAll) { active = false; return; }
            const float lt = ms - tm.startMs;                                     // local time (negative = pre-trigger)
            const float len = tm.endMs - tm.startMs;
            const float inv = (float) (1.0 / rate);
            // ---- envelopes (control rate)
            if ((tick++ & 15) == 0)
            {
                modV = lt <= 0.f ? 0.f : lt < s.modA ? m06::rise (lt / juce::jmax (0.01f, s.modA), s.modCurve) : m06::fall ((lt - s.modA) / juce::jmax (1.f, s.modD), s.modCurve);
                // pitch: END -> START over the attack, back to END over the decay, an end drop over the last RELEASE ms
                float pe = lt < s.pA ? m06::rise (lt / juce::jmax (0.01f, s.pA), s.pCurve) : m06::fall ((lt - s.pA) / juce::jmax (1.f, s.pD), s.pCurve);
                float semis = s.pAmt * (s.pEnd + (s.pStart - s.pEnd) * pe);
                if (s.pR > 0.f && lt > len - s.pR) semis -= 12.f * s.pAmt * juce::jlimit (0.f, 1.f, (lt - (len - s.pR)) / s.pR);
                semis += s.mPitch * modV;
                hz = juce::jlimit (10.f, (float) rate * 0.2f, baseHz * std::exp2 (semis / 12.f));
                // amplitude: A / HOLD / D / S until the end, then R; fades in / out
                float a;
                if (lt < 0.f) a = 0.f;
                else if (lt < s.a) a = m06::rise (lt / juce::jmax (0.01f, s.a), s.curve);
                else if (lt < s.a + s.hold) a = 1.f;
                else a = s.s + (1.f - s.s) * m06::fall ((lt - s.a - s.hold) / juce::jmax (1.f, s.d), s.curve);
                if (lt > len) a *= m06::fall ((lt - len) / juce::jmax (0.5f, s.r), s.curve);
                if (s.fadeIn > 0.f) a *= juce::jlimit (0.f, 1.f, lt / s.fadeIn);
                if (s.fadeOut > 0.f) a *= juce::jlimit (0.f, 1.f, (len - lt) / s.fadeOut + (lt > len ? 1.f : 0.f));
                ampTarget = a;
                vowelNow = juce::jlimit (0.f, 4.f, (s.vowelAuto ? s.vowel + (s.vowelEnd - s.vowel) * juce::jlimit (0.f, 1.f, lt / juce::jmax (1.f, len)) : s.vowel) + s.mFmt * modV * 4.f);
                updatePaths (s, rate);
                cx.ms = juce::jmax (0.f, lt); cx.lengthMs = juce::jmax (1.f, len); cx.noteHz = hz / registerMul; cx.relMs = -1.f;   // (KEY TRACK follows the screech pitch)
            }
            amp += 0.02f * (ampTarget - amp);
            // ---- the oscillators (stereo by pan x width)
            float oL = 0.f, oR = 0.f;
            const bool needOsc = s.mode != Noise && s.mode != Fm && s.mode != Hybrid;
            if (needOsc || (s.mode == Custom)) oscBank (s, tb, inv, oL, oR);
            oscMono = 0.5f * (oL + oR);
            float yL = 0.f, yR = 0.f;
            auto add = [&] (float g, float l, float r) { yL += g * l; yR += g * r; };
            switch (s.mode)
            {
                case Tonal:    add (1.f, oL, oR); break;
                case Metallic: add (1.f, metallic (0, oL), metallic (1, oR)); break;
                case Formant:  add (1.f, formant (0, oL), formant (1, oR)); break;
                case Fm:       { float l, r; fm (s, inv, l, r); add (1.f, l, r); oscMono = 0.5f * (l + r); break; }
                case Noise:    add (1.f, noise (s, 0), noise (s, 1)); break;
                case Feedback: add (1.f, feedback (s, 0, oL), feedback (s, 1, oR)); break;
                case Hybrid:   { float l, r; fm (s, inv, l, r); oscMono = 0.5f * (l + r); add (1.f, feedback (s, 0, formant (0, l)), feedback (s, 1, formant (1, r))); break; }
                default:
                {
                    const auto& c = s.custom;
                    if (c[0] > 0.f) add (c[0], oL, oR);
                    if (c[1] > 0.f) add (c[1], metallic (0, oL), metallic (1, oR));
                    if (c[2] > 0.f) add (c[2], formant (0, oL), formant (1, oR));
                    if (c[3] > 0.f) { float l, r; fm (s, inv, l, r); add (c[3], l, r); }
                    if (c[4] > 0.f) add (c[4], noise (s, 0), noise (s, 1));
                    if (c[5] > 0.f) add (c[5], feedback (s, 0, oL), feedback (s, 1, oR));
                    break;
                }
            }
            // ---- the two screech filters (module 06 algorithms, moved by the mod envelope)
            for (int k = 0; k < 2; ++k)
            {
                if (! s.flt[(size_t) k].on) continue;
                auto& st = flt[(size_t) k];
                if ((ftick[(size_t) k]++ & 15) == 0) st.update (s.fs[(size_t) k], cx, rate);
                st.process (s.fs[(size_t) k], yL, yR);
            }
            static constexpr float modeGain[8] { 8.f, 22.f, 24.f, 7.f, 32.f, 14.f, 11.f, 14.f };   // (measured: the modes about equally loud, level 0 dB ~ the tail)
            const float g = amp * s.level * fade * modeGain[juce::jlimit (0, 7, s.mode)];
            outL = soft4 (yL) * g; outR = soft4 (yR) * g;
            if (fadeStep > 0.f) { fade -= fadeStep; if (fade <= 0.f) { fade = 0.f; active = false; } }
        }
    private:
        static inline float soft4 (float x) { return 4.f * std::tanh (x * 0.25f); }
        inline float noiseSample (int c) { auto& r = rng[(size_t) c]; r ^= r << 13; r ^= r >> 17; r ^= r << 5; return (float) (r >> 8) * (1.f / 8388608.f) - 1.f; }

        void oscBank (const Settings& s, const OscTables& tb, float inv, float& L, float& R)
        {
            std::array<float, 4> outNow {};
            for (int i = 0; i < 4; ++i)
            {
                const auto& o = s.osc[(size_t) i];
                if (! o.on) continue;
                const float det = juce::jlimit (0.f, 1.f, o.detune + s.mDetune * modV);
                const float f0 = hz * std::exp2 ((o.pitch + o.fine * 0.01f) / 12.f);
                const int nv = det > 0.001f ? 3 : 1;
                float v = 0.f;
                const float pmod = o.fmSrc > 0 ? o.fm * juce::jlimit (0.f, 2.f, 1.f + s.mFm * modV) * 2.f * last[(size_t) o.fmSrc - 1] : 0.f;   // FM from another oscillator
                for (int k = 0; k < nv; ++k)
                {
                    const float cents = nv == 1 ? 0.f : (float) (k - 1) * det * 40.f;
                    const float dt = juce::jmin (0.45f, f0 * std::exp2 (cents / 1200.f) * inv);
                    auto& p = ph[(size_t) i][(size_t) k];
                    p += dt; p -= std::floor (p);
                    float pp = p + pmod; pp -= std::floor (pp);
                    float w;
                    switch (o.wave)
                    {
                        case WSine:   w = std::sin (juce::MathConstants<float>::twoPi * pp); break;
                        case WSquare: w = (pp < 0.5f ? 1.f : -1.f) + blep (pp, dt) - blep (std::fmod (pp + 0.5f, 1.f), dt); break;
                        case WTri:    w = 4.f * std::abs (pp - 0.5f) - 1.f; break;
                        case WPulse:  { const float q = juce::jlimit (0.05f, 0.95f, o.pw); w = (pp < q ? 1.f : -1.f) + blep (pp, dt) - blep (std::fmod (pp + 1.f - q, 1.f), dt); break; }
                        case WTable:  w = tb.t[(size_t) i] != nullptr ? tb.t[(size_t) i]->read (pp, 0.f, 0.f, Table::mipFor (dt)) : std::sin (juce::MathConstants<float>::twoPi * pp); break;
                        default:      w = 2.f * pp - 1.f - blep (pp, dt); break;
                    }
                    v += w;
                }
                v /= std::sqrt ((float) nv);
                if (o.ringSrc > 0) v *= 1.f - o.ring + o.ring * last[(size_t) o.ringSrc - 1];
                outNow[(size_t) i] = v;
                const float pan = juce::jlimit (-1.f, 1.f, o.pan * (0.3f + 0.7f * s.width));
                L += v * o.level * (1.f - juce::jmax (0.f, pan)); R += v * o.level * (1.f + juce::jmin (0.f, pan));
            }
            last = outNow;
        }
        // METALLIC: 6 inharmonic ringing modes on the pitch
        void updatePaths (const Settings& s, double rate)
        {
            const float decay = 20.f + 780.f * s.metalQ;
            for (int k = 0; k < 6; ++k)
            {
                const float ratio = std::pow ((float) (k + 1), 1.f + 0.7f * s.metal);
                for (auto& c : ch) c.modes[(size_t) k].set (juce::jmin (hz * ratio, (float) rate * 0.45f), decay / (1.f + 0.3f * (float) k), rate);
                modeAmp[(size_t) k] = hz * ratio < (float) rate * 0.45f ? 1.f / (1.f + 0.5f * (float) k) : 0.f;
            }
            const auto v = m06::vowelAt (vowelNow);
            const float spread = 0.7f + 0.6f * s.fmtWidth;
            const float sc = juce::jlimit (0.5f, 3.f, hz / registerHz);
            for (int k = 0; k < 3; ++k)
            {
                const float f = v[(size_t) k] * std::sqrt (sc) * (k == 0 ? 1.f : spread);
                fmtC[(size_t) k] = doomsday::dsp::svfCoefs (juce::jlimit (60.f, (float) rate * 0.45f, f), 1.f / (3.f + s.fmtReso * 25.f), rate);
            }
            const float fbHz = s.fbDelay > 0.01f ? 1000.f / s.fbDelay : hz;
            fbD = juce::jlimit (2.f, (float) m06::combLen - 4.f, (float) rate / juce::jmax (12.f, fbHz));
            fbC = doomsday::dsp::svfCoefs (500.f * std::exp2 (s.fbTone * 4.6f), 0.9f, rate);
            noiseD[0] = juce::jlimit (2.f, (float) m06::combLen - 4.f, (float) rate / hz);
            noiseD[1] = juce::jlimit (2.f, (float) m06::combLen - 4.f, (float) rate / (hz * 1.5f));
        }
        float metallic (int c, float x)
        {
            auto& h = ch[(size_t) c];
            float y = 0.3f * x;
            for (int k = 0; k < 6; ++k) y += h.modes[(size_t) k].tick (x) * modeAmp[(size_t) k] * 2.f;
            return y;
        }
        float formant (int c, float x)
        {
            auto& h = ch[(size_t) c];
            const float g[3] { 1.f, 0.7f, 0.45f };
            float y = 0.f;
            for (int k = 0; k < 3; ++k) { float l, b; h.fmt[(size_t) k].tick (x, fmtC[(size_t) k], l, b); y += b * fmtC[(size_t) k].k * g[k] * 1.6f; }
            return y;
        }
        // FM SCREECH: a 2-operator pair per oscillator (modulator at RATIO with its own feedback), sine operators
        void fm (const Settings& s, float inv, float& L, float& R)
        {
            L = R = 0.f;
            const float idx = juce::jlimit (0.f, 1.f, s.fmIndex + s.mFm * modV) * 8.f;
            for (int i = 0; i < 4; ++i)
            {
                const auto& o = s.osc[(size_t) i];
                if (! o.on) continue;
                const float f0 = hz * std::exp2 ((o.pitch + o.fine * 0.01f) / 12.f);
                auto& pc = ph[(size_t) i][0]; auto& pm = mph[(size_t) i];
                pc += juce::jmin (0.45f, f0 * inv); pc -= std::floor (pc);
                pm += juce::jmin (0.45f, f0 * s.fmRatio * inv); pm -= std::floor (pm);
                const float m = std::sin (juce::MathConstants<float>::twoPi * pm + s.fmFb * 3.f * fmPrev[(size_t) i]);
                fmPrev[(size_t) i] = m;
                const float wdt = f0 * s.width * 0.004f * inv;                  // a few cents of stereo
                const float vL = std::sin (juce::MathConstants<float>::twoPi * (pc + wdt) + idx * m), vR = std::sin (juce::MathConstants<float>::twoPi * (pc - wdt) + idx * m);
                const float pan = o.pan * s.width;
                L += vL * o.level * (1.f - juce::jmax (0.f, pan)); R += vR * o.level * (1.f + juce::jmin (0.f, pan));
            }
        }
        // NOISE SCREECH: coloured noise through two combs tuned to the pitch and a fifth above
        float noise (const Settings& s, int c)
        {
            auto& h = ch[(size_t) c];
            float n = noiseSample (c);
            h.col += 0.15f * (n - h.col);
            n = s.noiseCol < 0.f ? n + (h.col - n) * -s.noiseCol : n + (n - h.col) * s.noiseCol;
            const float fb = 0.5f + 0.495f * s.noiseQ;
            float y = 0.f;
            for (int k = 0; k < 2; ++k)
            {
                auto& buf = k == 0 ? h.c1 : h.c2; int& wi = k == 0 ? h.w1 : h.w2;
                const float rp = (float) wi - noiseD[(size_t) k]; const float rpos = rp < 0.f ? rp + (float) m06::combLen : rp;
                const int i0 = (int) rpos; const float fr = rpos - (float) i0;
                const float d = buf[(size_t) (i0 % m06::combLen)] + fr * (buf[(size_t) ((i0 + 1) % m06::combLen)] - buf[(size_t) (i0 % m06::combLen)]);
                const float w = m06::soft (n * 0.3f + fb * d);
                buf[(size_t) wi] = w; wi = (wi + 1) % m06::combLen;
                y += w * (k == 0 ? 1.f : 0.6f);
            }
            return y * (1.f - fb * 0.8f) * 3.f;
        }
        // FEEDBACK: delay loop (pitch or FEEDBACK DELAY) + filter + drive; STABILITY = how hard the guard holds it
        float feedback (const Settings& s, int c, float x)
        {
            auto& h = ch[(size_t) c];
            const float rp = (float) h.fw - fbD; const float rpos = rp < 0.f ? rp + (float) m06::combLen : rp;
            const int i0 = (int) rpos; const float fr = rpos - (float) i0;
            float d = h.fbuf[(size_t) (i0 % m06::combLen)] + fr * (h.fbuf[(size_t) ((i0 + 1) % m06::combLen)] - h.fbuf[(size_t) (i0 % m06::combLen)]);
            float l, b; h.fsv.tick (d, fbC, l, b);
            d = s.fbFilter == 0 ? l : s.fbFilter == 1 ? d - fbC.k * b - l : b * fbC.k;
            const float fbk = juce::jlimit (0.f, 0.95f, s.fbAmt + s.mFb * modV) * fbScale;
            const float gd = 1.f + 6.f * s.fbDrive;
            float w = std::tanh ((x + fbk * d) * gd) / std::sqrt (gd);
            // guard: a soft ceiling (lower with STABILITY), energy watch lowers the feedback, DC / NaN safe
            const float ceil = 1.5f - s.fbStab;
            w = ceil * std::tanh (w / ceil);
            h.dcx += 0.0005f * (w - h.dcx); w -= h.dcx;
            if (! std::isfinite (w)) { w = 0.f; h.fbuf.fill (0.f); }
            h.fbuf[(size_t) h.fw] = w; h.fw = (h.fw + 1) % m06::combLen;
            energy += 0.0005f * (w * w - energy);
            if (energy > 0.6f + (1.f - s.fbStab)) fbScale = juce::jmax (0.2f, fbScale - 0.002f); else fbScale = juce::jmin (1.f, fbScale + 0.0001f);
            return x + (w - x) * s.fbMix;
        }

        struct Ch
        {
            std::array<m06::Mode, 6> modes {}; std::array<doomsday::dsp::Svf, 3> fmt {};
            std::array<float, m06::combLen> c1 {}, c2 {}, fbuf {}; int w1 = 0, w2 = 0, fw = 0; float col = 0.f, dcx = 0.f; doomsday::dsp::Svf fsv;
        };
        Timing tm;
        std::array<std::array<float, 3>, 4> ph {}; std::array<float, 4> mph {}, fmPrev {}, last {};
        std::array<Ch, 2> ch {};
        std::array<m06::FiltState, 2> flt; std::array<int, 2> ftick {};
        std::array<float, 6> modeAmp {}; std::array<doomsday::dsp::SvfCoefs, 3> fmtC {}; doomsday::dsp::SvfCoefs fbC;
        float fbD = 100.f; std::array<float, 2> noiseD { 100.f, 66.f };
        std::array<uint32_t, 2> rng {};
        m06::Ctx cx;
        float hz = registerHz, amp = 0.f, ampTarget = 0.f, modV = 0.f, vowelNow = 0.f, fade = 1.f, fadeStep = 0.f, fbScale = 1.f, energy = 0.f;
        uint32_t tick = 0; double clock = 0.0;
    };

    // two voices: a new kick fades the old screech out in 5 ms (no click), the new one starts clean
    struct Engine
    {
        std::array<std::unique_ptr<Voice>, 2> v { std::make_unique<Voice>(), std::make_unique<Voice>() };
        int cur = 0;
        void trigger (const Settings& s, const Timing& t, uint32_t seed, double rate)
        {
            v[(size_t) cur]->fadeOut (5.f, rate);
            cur ^= 1;
            v[(size_t) cur]->start (s, t, seed);
        }
        void reset() { for (auto& x : v) x->active = false; }
    };
}
