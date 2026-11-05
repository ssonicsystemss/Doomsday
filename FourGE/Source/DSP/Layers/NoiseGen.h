#pragma once
// =====================================================================
//  4GE MODULE 04 - NOISE GENERATOR (A / B). Eight real sources, each its own spectrum:
//   WHITE, PINK (Kellet filter), BROWN (leaky integrator), BLUE (+3 dB / oct: first difference), VIOLET (+6 dB / oct: second
//   difference), METALLIC (white through 4 resonators: FREQ / Q / AMOUNT / CHARACTER spreads their ratios),
//   DIGITAL (sample-rate reduction + bit depth + quantisation curve + GRAIN drop-outs), CUSTOM (white through 8 bands shaped
//   by a drawn spectrum profile).  Chain: source -> COLOR tilt -> [FILTER LP / HP / BP / NOTCH + DRIVE] (MIX) -> envelope.
//   Stereo WIDTH = a second independent stream on the right. Seeded: FIXED / PER TRIGGER / FREE RUN (+ RANDOM AMOUNT).
//   A modulation envelope (ADSR or drawn, module 02 style) moves level / cutoff / resonance / drive / color / width / grain.
// =====================================================================
#include "../Osc/Noise.h"
#include "../Env/MultiEnv.h"
#include "DoomsdayDSP.h"

namespace fge::dsp
{
    enum NoiseKind { NkWhite = 0, NkPink, NkBrown, NkBlue, NkViolet, NkMetallic, NkDigital, NkCustom };
    enum FilterType { FtLow = 0, FtHigh, FtBand, FtNotch };
    enum NoiseEnvSrc { NeOwn = 0, NePunch, NeBody, NeTail, NeSub, NeDrawn };
    enum RandomMode { RmFixed = 0, RmPerTrigger, RmFree };

    struct NoiseSettings
    {
        bool on = false;
        int kind = NkWhite, filter = FtLow, envSrc = NeOwn, randMode = RmPerTrigger;
        float gain = 0.25f, color = 0.f, cutoff = 20000.f, reso = 0.f, drive = 0.f, mix = 1.f, width = 0.f;
        float attackMs = 0.5f, decayMs = 60.f, lengthMs = 150.f;
        bool track = false;
        float resHz = 3000.f, resQ = 0.5f, resAmt = 0.8f, metal = 0.5f;
        float bits = 6.f, srr = 0.4f, quant = 0.f, grain = 0.f;
        uint32_t seed = 11; float randAmt = 1.f;
        std::array<float, 4> route {};           // into the part inputs (before their chain)
        float toTexture = 1.f;                    // straight to the TEXTURE bus
        bool modOn = false, modDrawn = false; float modA = 0.f, modD = 40.f;
        float mLevel = 0.f, mCut = 0.f, mRes = 0.f, mDrive = 0.f, mColor = 0.f, mWidth = 0.f, mGrain = 0.f;
        const env::Shape* ampShape = nullptr;     // DRAWN envelope
        const env::Shape* modShape = nullptr;     // DRAWN modulation envelope
        const env::Shape* profile = nullptr;      // CUSTOM spectrum
        // cooked once per block: resonator / band coefficients and gains
        std::array<doomsday::dsp::SvfCoefs, 4> resC {};
        std::array<doomsday::dsp::SvfCoefs, 8> bandC {};
        std::array<float, 8> bandG {};
        void cookFilters (double rate)
        {
            static constexpr float ratio[4] { 1.f, 1.47f, 2.09f, 2.83f };
            for (int k = 0; k < 4; ++k)
                resC[(size_t) k] = doomsday::dsp::svfCoefs (resHz * (1.f + (ratio[k] - 1.f) * (0.3f + 1.4f * metal)), 1.f / (0.6f + resQ * resQ * 40.f), rate);
            for (int k = 0; k < 8; ++k)
            {
                bandC[(size_t) k] = doomsday::dsp::svfCoefs (60.f * std::pow (233.f, (float) k / 7.f), 1.4f, rate);
                bandG[(size_t) k] = profile != nullptr ? std::pow (10.f, (profile->eval ((float) k / 7.f) * 36.f - 24.f) / 20.f) : 1.f;
            }
        }
    };

    // one noise stream (the left or the right channel)
    struct NoiseStream
    {
        void reset (uint32_t seed)
        {
            base.reset (seed); alt.reset (seed ^ 0x5bd1e995u); w.reset (seed * 2654435761u + 7u);
            prev1 = prev2 = 0.f; hold = 0.f; holdPh = 1.f; grainOn = 1.f; grainLeft = 0;
            for (auto& r : res) r = {}; for (auto& b : band) b = {};
            lp = 0.f; tone = {}; drv = 0.f;
        }
        // the raw source (no colour / filter); 'r' = mix of the fixed stream and the per-trigger one (RANDOM AMOUNT)
        inline float source (const NoiseSettings& s, float inv, float rate, float randAmt, NoiseStream* other)
        {
            juce::ignoreUnused (other);
            auto white = [&] { const float a = base.tick (NoiseWhite, 0.f, inv); if (randAmt >= 0.999f) return a;
                               const float b = alt.tick (NoiseWhite, 0.f, inv); return a * std::sqrt (randAmt) + b * std::sqrt (1.f - randAmt); };
            switch (s.kind)
            {
                case NkPink:   return base.tick (NoisePink, 0.f, inv);
                case NkBrown:  return base.tick (NoiseBrown, 0.f, inv);
                case NkBlue:   { const float x = white(); const float y = (x - prev1) * 0.7f; prev1 = x; return y; }
                case NkViolet: { const float x = white(); const float y = (x - 2.f * prev1 + prev2) * 0.42f; prev2 = prev1; prev1 = x; return y; }
                case NkMetallic:
                {
                    const float x = white();
                    float y = 0.f;
                    for (int k = 0; k < 4; ++k) y += res[(size_t) k].bp (x, s.resC[(size_t) k]) * (1.f - 0.18f * (float) k);
                    return x * (1.f - s.resAmt) + y * s.resAmt * (1.5f + s.resQ * 3.f);
                }
                case NkDigital:
                {
                    holdPh += 1.f - 0.985f * s.srr;                       // sample-rate reduction (hold)
                    if (holdPh >= 1.f) { holdPh -= 1.f; hold = white(); }
                    const float steps = std::exp2 (juce::jlimit (1.f, 16.f, s.bits)) * 0.5f;
                    float q = hold;
                    if (s.quant > 0.f) q = (q < 0.f ? -1.f : 1.f) * std::pow (std::abs (q), 1.f + 2.f * s.quant);   // QUANTISATION: steps crowd towards 0
                    return std::round (q * steps) / steps;
                }
                case NkCustom:
                {
                    const float x = white();
                    float y = 0.f;                                        // 8 bands, 60 Hz .. 14 kHz, gains from the drawn profile
                    for (int k = 0; k < 8; ++k) y += band[(size_t) k].bp (x, s.bandC[(size_t) k]) * s.bandG[(size_t) k];
                    return y * 0.9f;
                }
                default: return white();
            }
        }
        NoiseEngine base, alt;
        Noise w;
        float prev1 = 0.f, prev2 = 0.f, hold = 0.f, holdPh = 1.f, grainOn = 1.f; int grainLeft = 0;
        std::array<doomsday::dsp::Svf, 4> res {};
        std::array<doomsday::dsp::Svf, 8> band {};
        float lp = 0.f; doomsday::dsp::Svf tone; float drv = 0.f;
    };

    class NoiseVoice
    {
    public:
        // seed per mode: FIXED = the same each hit, PER TRIGGER = a new one per Note On, FREE RUN = from the engine's clock
        void start (const NoiseSettings& s, uint32_t hitCount, uint64_t clockSamples)
        {
            uint32_t seed = s.seed * 2246822519u + 1u;
            uint32_t alt = seed;
            if (s.randMode == RmPerTrigger) alt = seed ^ (hitCount * 0x9e3779b9u + 0x7f4a7c15u);
            if (s.randMode == RmFree) alt = seed ^ (uint32_t) (clockSamples * 2654435761ull);
            // the stream that changes per hit is 'alt'; RANDOM AMOUNT blends the fixed one with it
            l.reset (s.randMode == RmFixed ? seed : alt); r.reset ((s.randMode == RmFixed ? seed : alt) ^ 0xa5a5a5a5u);
            l.alt.reset (seed); r.alt.reset (seed ^ 0xa5a5a5a5u);
            l.base.reset (s.randMode == RmFixed ? seed : alt);
            g.reset (seed ^ 0x13579bdu);
            grainGate = 1.f; grainLeft = 0; modSeg = 0; ampSeg = 0;
        }

        // one sample: returns L / R; partAmp = the amp envelopes of the parts (PUNCH / BODY / TAIL / SUB AMP sources)
        inline void tick (const NoiseSettings& s, float ms, float inv, float rate, float noteRatio, const std::array<float, 4>& partAmp, float& outL, float& outR)
        {
            outL = outR = 0.f;
            // ---- envelope
            float env;
            switch (s.envSrc)
            {
                case NePunch: env = partAmp[0]; break;
                case NeBody:  env = partAmp[1]; break;
                case NeTail:  env = partAmp[2]; break;
                case NeSub:   env = partAmp[3]; break;
                case NeDrawn: env = ms >= s.lengthMs || s.ampShape == nullptr ? 0.f : juce::jlimit (0.f, 1.f, s.ampShape->eval (ms / juce::jmax (1.f, s.lengthMs), ampSeg)); break;
                default:
                {
                    const float a = juce::jmax (0.05f, s.attackMs);
                    env = ms < a ? ms / a : std::exp (-(ms - a) / juce::jmax (1.f, s.decayMs));
                    break;
                }
            }
            if (s.envSrc == NeOwn || s.envSrc == NeDrawn) env *= juce::jlimit (0.f, 1.f, (s.lengthMs - ms) / 3.f);       // ends with a 3 ms fade
            if (env <= 1.0e-6f && ms > s.lengthMs) return;
            // ---- modulation envelope (module 02 style: ADSR or drawn)
            float m = 0.f;
            if (s.modOn)
            {
                if (s.modDrawn && s.modShape != nullptr) m = juce::jlimit (0.f, 1.f, s.modShape->eval (ms / juce::jmax (1.f, s.modA + s.modD), modSeg));
                else { const float a = juce::jmax (0.05f, s.modA); m = ms < a ? ms / a : std::exp (-(ms - a) / juce::jmax (1.f, s.modD)); }
            }
            const float color = juce::jlimit (-1.f, 1.f, s.color + s.mColor * m);
            const float cut = juce::jlimit (20.f, 20000.f, s.cutoff * (s.track ? noteRatio : 1.f) * std::exp2 (s.mCut * 6.f * m));
            const float reso = juce::jlimit (0.f, 1.f, s.reso + s.mRes * m);
            const float drive = juce::jlimit (0.f, 1.f, s.drive + s.mDrive * m);
            const float width = juce::jlimit (0.f, 1.f, s.width + s.mWidth * m);
            const float grain = juce::jlimit (0.f, 1.f, s.grain + s.mGrain * m);
            const float level = s.gain * std::pow (10.f, s.mLevel * 24.f * m / 20.f);
            if ((ctl++ & 15) == 0) coefs = doomsday::dsp::svfCoefs (cut, 1.f / (0.7f + reso * reso * 14.f), rate);
            // ---- GRAIN: random short drop-outs (2 .. 6 ms), deterministic
            if (grain > 0.f)
            {
                if (grainLeft-- <= 0) { grainLeft = (int) ((0.002f + 0.004f * g01()) * rate); grainGate = g01() < grain * 0.85f ? 0.f : 1.f; }
            }
            else grainGate = 1.f;
            auto chain = [&] (NoiseStream& st, float randAmt) -> float
            {
                float x = st.source (s, inv, rate, randAmt, nullptr);
                // COLOR: tilt around ~800 Hz (dark .. bright)
                const float k = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 800.f / rate);
                st.lp += k * (x - st.lp);
                const float hi = x - st.lp;
                x = st.lp * (1.f - color) + hi * (1.f + color);
                // FILTER + DRIVE, blended by MIX
                float y = x;
                const bool open = s.filter == FtLow && cut >= 19999.f && reso < 0.01f;
                if (! open)
                {
                    float l, b; st.tone.tick (x, coefs, l, b);
                    switch (s.filter)
                    {
                        case FtHigh:  y = x - coefs.k * b - l; break;
                        case FtBand:  y = b * (1.f + reso * 2.f); break;
                        case FtNotch: y = x - coefs.k * b; break;
                        default:      y = l; break;
                    }
                }
                if (drive > 0.f) { const float gd = 1.f + drive * 24.f; y = std::tanh (y * gd) / std::tanh (gd) * (1.f + drive); }
                return (x + (y - x) * s.mix) * grainGate;
            };
            const float ra = s.randMode == RmFixed ? 1.f : s.randAmt;
            const float a = chain (l, ra);
            const float b = width > 0.001f ? chain (r, ra) : a;
            outL = a * env * level;
            outR = (a + (b - a) * width) * env * level;
            if (! std::isfinite (outL) || ! std::isfinite (outR)) { outL = outR = 0.f; }
        }
    private:
        float g01() { return (float) (g.tick() * 0.5f + 0.5f); }
        NoiseStream l, r;
        Noise g;
        float grainGate = 1.f; int grainLeft = 0;
        doomsday::dsp::SvfCoefs coefs;
        uint32_t ctl = 0;
        int modSeg = 0, ampSeg = 0;
    };
}
