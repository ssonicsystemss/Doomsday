#pragma once
// =====================================================================
//  4GE MODULE 04 - LAYER ENGINE: 8 independent layers (sized for 16), each its own signal path:
//   source (internal SINE / TRI / SAW / SQUARE, NOISE A / B, TRANSIENT, OSC 1..4 - each layer runs its OWN copy of the source)
//   -> pitch (the group's part pitch x PITCH / FINE) -> own envelope (ADSR / ONE SHOT / MULTIPOINT drawn, delay, amount,
//   time scale) -> START OFFSET / LENGTH -> phase / invert -> FILTER (LP / HP / BP / NOTCH) + DRIVE -> WIDTH (all-pass side,
//   mono sum kept) -> PAN -> fader (LEVEL x GROUP LEVEL) -> SINGLE: the group's output bus, or MULTI: sends (pre / post fader)
//   to the PUNCH / BODY / TAIL / SUB / TEXTURE / CUSTOM / MASTER buses. One path per layer, no hidden double sums.
// =====================================================================
#include "NoiseGen.h"
#include "Transient.h"
#include "../Osc/OscBank.h"
#include "../Dist/Distortion.h"
#include "../Filter/Filters.h"
#include "../Tone/Tone.h"

namespace fge::dsp
{
    constexpr int maxLayers = 16, numLayers = 8;
    enum Bus { BusPunch = 0, BusBody, BusTail, BusSub, BusTexture, BusCustom, BusMaster, NumBus };
    enum LayerSource { LsSine = 0, LsTriangle, LsSaw, LsSquare, LsNoiseA, LsNoiseB, LsTransient, LsOsc1, LsOsc2, LsOsc3, LsOsc4 };
    enum LayerGroup { LgPunch = 0, LgBody, LgTail, LgSub, LgTexture, LgCustom, NumGroups };
    enum LayerEnv { LeAdsr = 0, LeOneShot, LeMulti };

    struct LayerSettings
    {
        bool on = false, audible = false, sourceOnly = false;
        int source = LsSine, group = LgTexture, track = 2, envMode = LeOneShot, outBus = BusTexture;
        bool multi = false, preFader = false;
        float fader = 0.5f, groupGain = 1.f;              // LEVEL x GROUP LEVEL (sign = INVERT), the group part alone (pre-fader sends)
        float panL = 1.f, panR = 1.f, side = 0.f;          // pan (layer + group), stereo width (layer x group)
        float pitchMul = 1.f, lenMs = 300.f, offsetMs = 0.f, phase01 = 0.f;
        bool fltOn = false; int fltType = FtLow; doomsday::dsp::SvfCoefs flt; float drive = 0.f;
        float atk = 1.f, dec = 120.f, sus = 0.3f, rel = 30.f, delay = 0.f, amount = 1.f;
        const env::Shape* shape = nullptr;
        std::array<float, NumBus> send {};
    };
    struct LayerBankSettings
    {
        std::array<NoiseSettings, 2> noise {};
        ClickSettings click;
        std::array<NoiseSettings, 2> layerNoise {};       // the same sources for layers: unity gain, no own envelope
        ClickSettings layerClick;
        ShaperSettings shaper;
        std::array<LayerSettings, maxLayers> layer {};
        float texGain = 1.f, custGain = 1.f;
        bool anyLayer = false, anyNoise = false;
    };

    // ---- one layer inside one kick voice (all its DSP state is its own)
    struct LayerVoice
    {
        void start (const LayerSettings& s, const LayerBankSettings& bank, const OscBankSettings& osc, uint32_t seed, uint32_t hitCount,
                    uint64_t clock, const std::array<float, 4>& partHz)
        {
            ph = s.phase01; flt = {}; ap1 = ap2 = 0.f; ap1x = ap2x = 0.f; seg = 0; relFrom = 0.f;
            if (s.source == LsNoiseA || s.source == LsNoiseB) nv.start (bank.layerNoise[(size_t) (s.source - LsNoiseA)], hitCount + seed, clock);
            if (s.source == LsTransient) cv.start (seed * 7919u + hitCount, s.offsetMs);
            if (s.source >= LsOsc1) ov.start (osc, partHz, 0.0, hitCount);
        }
        // returns the processed MONO layer signal (before pan / width / fader); 0 when silent
        inline float tick (const LayerSettings& s, const LayerBankSettings& bank, const OscBankSettings& osc, const OscTables& tb,
                           const std::array<float, 4>& partHz, float ms, float inv, float rate)
        {
            const float t = ms - s.offsetMs;                                      // START OFFSET
            if (t < 0.f) return 0.f;
            const float tl = t - s.delay;                                         // envelope DELAY
            const float end = s.lenMs + (s.envMode == LeAdsr ? s.rel : 0.f);
            if (tl > end + 3.f) return 0.f;
            // ---- envelope
            float e = 0.f;
            if (tl >= 0.f)
            {
                if (s.envMode == LeMulti && s.shape != nullptr) e = tl < s.lenMs ? juce::jlimit (0.f, 1.f, s.shape->eval (tl / juce::jmax (1.f, s.lenMs), seg)) : 0.f;
                else
                {
                    const float a = juce::jmax (0.05f, s.atk);
                    if (tl < a) e = tl / a;
                    else if (s.envMode == LeOneShot) e = std::exp (-(tl - a) / juce::jmax (1.f, s.dec));
                    else { const float d = std::exp (-(tl - a) / juce::jmax (1.f, s.dec)); e = s.sus + (1.f - s.sus) * d; }
                    if (s.envMode == LeAdsr && tl >= s.lenMs) { if (relFrom <= 0.f) relFrom = e; e = relFrom * juce::jmax (0.f, 1.f - (tl - s.lenMs) / juce::jmax (1.f, s.rel)); }
                }
                if (s.envMode != LeAdsr) e *= juce::jlimit (0.f, 1.f, (s.lenMs - tl) / 3.f);      // ends at LENGTH with a 3 ms fade
                e = juce::jmin (1.f, tl / 0.15f) * e;
            }
            const float env = (1.f - s.amount) * (tl >= 0.f && tl < end ? 1.f : 0.f) + s.amount * e;
            // ---- source (own copy)
            const float hz = juce::jlimit (5.f, rate * 0.45f, partHz[(size_t) s.track] * s.pitchMul);
            float x = 0.f;
            switch (s.source)
            {
                case LsNoiseA: case LsNoiseB:
                {
                    float l, r; static const std::array<float, 4> amp { 1.f, 1.f, 1.f, 1.f };
                    nv.tick (bank.layerNoise[(size_t) (s.source - LsNoiseA)], t, inv, rate, s.pitchMul, amp, l, r);
                    x = 0.5f * (l + r);
                    break;
                }
                case LsTransient:
                {
                    float l, r; cv.tick (bank.layerClick, ms, inv, rate, s.pitchMul, l, r);
                    x = 0.5f * (l + r);
                    break;
                }
                case LsOsc1: case LsOsc2: case LsOsc3: case LsOsc4:
                {
                    const int o = s.source - LsOsc1;
                    x = ov.tickOne (osc.o[(size_t) o], tb, o, juce::jlimit (5.f, rate * 0.45f, partHz[(size_t) s.track] * osc.o[(size_t) o].mul * s.pitchMul), t, inv);
                    break;
                }
                default:
                {
                    const float dt = hz * inv;
                    const float p = ph; ph += dt; if (ph >= 1.f) ph -= 1.f;
                    switch (s.source)
                    {
                        case LsTriangle: x = 1.f - 4.f * std::abs (p - 0.5f); break;
                        case LsSaw:      x = 2.f * p - 1.f - polyBlep (p, dt); break;
                        case LsSquare:   { float q = p + 0.5f; if (q >= 1.f) q -= 1.f; x = ((p < 0.5f ? 1.f : -1.f) + polyBlep (p, dt) - polyBlep (q, dt)) * 0.8f; break; }
                        default:         x = std::sin (twoPi * p); break;
                    }
                }
            }
            x *= env;
            // ---- FILTER + DRIVE (SOLO SOURCE hears the layer before them)
            if (! s.sourceOnly)
            {
                if (s.fltOn)
                {
                    float l, b; flt.tick (x, s.flt, l, b);
                    switch (s.fltType)
                    {
                        case FtHigh:  x = x - s.flt.k * b - l; break;
                        case FtBand:  x = b; break;
                        case FtNotch: x = x - s.flt.k * b; break;
                        default:      x = l; break;
                    }
                }
                if (s.drive > 0.f) { const float g = 1.f + s.drive * 20.f; x = std::tanh (x * g) / std::tanh (g); }
            }
            return std::isfinite (x) ? x : 0.f;
        }
        // WIDTH: the side signal from two all-pass stages (mono sum unchanged)
        inline float side (float x)
        {
            const float c1 = 0.6f, c2 = -0.35f;
            const float y1 = -c1 * x + ap1x + c1 * ap1; ap1x = x; ap1 = y1;
            const float y2 = -c2 * y1 + ap2x + c2 * ap2; ap2x = y1; ap2 = y2;
            return y2 - x * 0.5f;
        }
        float ph = 0.f, ap1 = 0.f, ap2 = 0.f, ap1x = 0.f, ap2x = 0.f, relFrom = 0.f;
        int seg = 0;
        doomsday::dsp::Svf flt;
        NoiseVoice nv;
        ClickVoice cv;
        OscBankVoice ov;
    };

    // ---- everything of module 04 inside one kick voice
    class LayerBankVoice
    {
    public:
        void start (const LayerBankSettings& b, const OscBankSettings& osc, uint32_t hitCount, uint64_t clock, const std::array<float, 4>& partHz,
                    const std::array<float, 4>& partStartMs)
        {
            m06st->reset(); toneSt->reset();
            for (int k = 0; k < 2; ++k) if (b.noise[(size_t) k].on) noise[(size_t) k].start (b.noise[(size_t) k], hitCount, clock);
            if (b.click.on) click.start (0x2468aceu + hitCount * 31u, b.click.positionMs + (b.click.sync > 0 ? partStartMs[(size_t) b.click.sync - 1] : 0.f));
            for (int i = 0; i < numLayers; ++i)
            {
                started[(size_t) i] = b.layer[(size_t) i].on;
                (*chains)[(size_t) i].reset();

                fadeG[(size_t) i] = b.layer[(size_t) i].on && b.layer[(size_t) i].audible ? 1.f : 0.f;
                if (started[(size_t) i]) layer[(size_t) i].start (b.layer[(size_t) i], b, osc, (uint32_t) (i + 1) * 977u, hitCount, clock, partHz);
            }
        }
        // adds the noise generators into the part inputs (ext / tail L R), and everything else into the 7 stereo buses
        inline void tick (const LayerBankSettings& b, const OscBankSettings& osc, const OscTables& tb, const std::array<float, 4>& partHz,
                          const std::array<float, 4>& partAmp, float noteRatio, float ms, float inv, float rate,
                          std::array<float, 4>& ext, float& extL, float& extR, float* busL, float* busR,
                          const dist::DistBank* dist = nullptr, const dist::ModCtx* mod = nullptr,
                          const m06::Bank* m6 = nullptr, const m06::Ctx* m6c = nullptr, const tone::Bank* m7 = nullptr)
        {
            for (int k = 0; k < 2; ++k)
            {
                const auto& ns = b.noise[(size_t) k];
                if (! ns.on) continue;
                float l, r; noise[(size_t) k].tick (ns, ms, inv, rate, noteRatio, partAmp, l, r);
                const float m = 0.5f * (l + r);
                for (int p = 0; p < 4; ++p) ext[(size_t) p] += m * ns.route[(size_t) p];
                extL += l * ns.route[2]; extR += r * ns.route[2];
                busL[BusTexture] += l * ns.toTexture; busR[BusTexture] += r * ns.toTexture;
            }
            if (b.click.on)
            {
                float l, r; click.tick (b.click, ms, inv, rate, 1.f, l, r);
                busL[b.click.output] += l; busR[b.click.output] += r;
            }
            const float step = inv * 1000.f / 3.f;                                   // ON / OFF / MUTE / SOLO fade in 3 ms (no clicks)
            for (int i = 0; i < numLayers; ++i)
            {
                const auto& s = b.layer[(size_t) i];
                auto& g = fadeG[(size_t) i];
                const float target = s.on && s.audible ? 1.f : 0.f;
                g = target > g ? juce::jmin (target, g + step) : juce::jmax (target, g - step);
                if (! started[(size_t) i] || g <= 0.f) continue;                     // a layer switched on during a kick waits for the next hit
                auto& v = layer[(size_t) i];
                float x = v.tick (s, b, osc, tb, partHz, ms, inv, rate) * g;
                // module 06 (BEFORE) -> module 05: the LAYER distortion -> module 06 (AFTER), all before pan / width / fader
                const int ml = m06::LLayer1 + i;
                const bool m6on = m6 != nullptr && m6c != nullptr && m6->atLoc (ml);
                const bool m7on = m7 != nullptr && m6c != nullptr && m7->atLoc (ml);
                auto m6run = [&] (int pos)
                {
                    if (m6on && m6->at (ml, pos)) { float l = x, r = x; m06State().process (*m6, ml, pos, l, r, *m6c, (double) rate); x = 0.5f * (l + r); }
                    if (m7on && m7->at (ml, pos)) { float l = x, r = x; toneSt->process (*m7, ml, pos, l, r, *m6c, (double) rate); x = 0.5f * (l + r); }   // module 07
                };
                m6run (0);
                if (dist != nullptr && dist->count[(size_t) (dist::LocLayer1 + i)] > 0 && mod != nullptr)
                    x = (*chains)[(size_t) i].process (*dist, dist::LocLayer1 + i, x, *mod, rate);
                m6run (1);
                if (x == 0.f) continue;
                const float sd = s.side > 0.001f ? v.side (x) * s.side : 0.f;
                const float l = (x + sd) * s.panL, r = (x - sd) * s.panR;
                if (! s.multi) { busL[s.outBus] += l * s.fader; busR[s.outBus] += r * s.fader; continue; }
                const float sg = s.preFader ? s.groupGain : s.fader;
                for (int bus = 0; bus < NumBus; ++bus)
                    if (s.send[(size_t) bus] > 0.f) { busL[bus] += l * sg * s.send[(size_t) bus]; busR[bus] += r * sg * s.send[(size_t) bus]; }
            }
        }
        // the processed signal of one layer (tests / GUI previews)
        LayerVoice& layerVoice (int i) { return layer[(size_t) i]; }
    private:
        std::array<NoiseVoice, 2> noise;
        ClickVoice click;
        std::array<LayerVoice, numLayers> layer;
        std::array<float, numLayers> fadeG {};
        std::unique_ptr<std::array<dist::ChainState, numLayers>> chains = std::make_unique<std::array<dist::ChainState, numLayers>>();
        std::unique_ptr<m06::State> m06st = std::make_unique<m06::State>();       // module 06: filters / resonators placed on a layer
        std::unique_ptr<tone::State> toneSt = std::make_unique<tone::State>();    // module 07: EQ / multiband / spectral placed on a layer
    public:
        m06::State& m06State() { return *m06st; }
    private:
        std::array<bool, numLayers> started {};
    };
}
