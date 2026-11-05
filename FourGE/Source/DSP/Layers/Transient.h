#pragma once
// =====================================================================
//  4GE MODULE 04 - TRANSIENT ENGINE
//   ClickVoice: an extra attack signal per kick - SINE CLICK, NOISE CLICK, IMPULSE (one band-limited, peak-limited pulse),
//     SHORT PULSE, METALLIC CLICK, OSC SNAP (a saw diving 2 octaves), CUSTOM (the drawn transient). Own envelope (LENGTH),
//     PITCH, TONE, DRIVE, MIX, WIDTH, POSITION (0..250 ms after the kick / punch / body / tail start).
//   AttackShaper (engine level, on the kick before the master clip):
//     - ATTACK LEVEL / TIME / DECAY: a gain envelope from each hit, split LOW / HIGH (LOW / HIGH TRANSIENT, flat sum);
//     - TRANSIENT SHAPER: a real envelope detector (fast vs slow followers) -> ATTACK / SUSTAIN gain, SENSITIVITY, TIME, MIX.
// =====================================================================
#include "../Osc/Noise.h"
#include "../Env/MultiEnv.h"
#include "DoomsdayDSP.h"

namespace fge::dsp
{
    enum TrSource { TrSine = 0, TrNoise, TrImpulse, TrPulse, TrMetal, TrSnap, TrCustom };

    struct ClickSettings
    {
        bool on = false;
        int source = TrSine, output = 0;
        float gain = 0.5f, lenMs = 8.f, hz = 1800.f, tone = 0.6f, drive = 0.2f, mix = 1.f, width = 0.f;
        float positionMs = 0.f;                       // + the sync point (cooked per voice)
        int sync = 0;                                 // 0 kick, 1 punch, 2 body, 3 tail start
        const env::Shape* custom = nullptr;
    };

    class ClickVoice
    {
    public:
        void start (uint32_t seed, float startMs) { nz.reset (seed); ph = 0.f; for (auto& o : met) o.reset (0.f); tone = {}; bp = {}; at = startMs;
                                                    for (auto& d : dl) d = 0.f; di = 0; seg = 0; }
        float startMs() const { return at; }
        // pitchMul: a layer playing the transient re-pitches it
        inline void tick (const ClickSettings& s, float ms, float inv, float rate, float pitchMul, float& L, float& R)
        {
            L = R = 0.f;
            const float t = ms - at;
            const float len = juce::jmax (0.1f, s.lenMs);
            if (t < 0.f || t > len + 3.f) { push (0.f); return; }
            const float env = juce::jmin (1.f, t / 0.08f) * std::exp (-5.f * t / len) * juce::jlimit (0.f, 1.f, (len + 3.f - t) / 3.f);
            const float hz = juce::jlimit (20.f, rate * 0.45f, s.hz * pitchMul);
            float x;
            switch (s.source)
            {
                case TrNoise: x = bp.bp (nz.tick() * 2.f, doomsday::dsp::svfCoefs (hz, 0.9f, rate)) * 1.6f; break;
                case TrImpulse:                                           // one gaussian pulse ~ 1 / hz wide, peak exactly 1
                {
                    const float w = 1000.f / hz * 0.35f;                  // ms
                    const float u = (t - 2.f * w) / w;
                    x = std::exp (-0.5f * u * u);
                    return finish (s, x, t, len, rate, L, R, 1.f);
                }
                case TrPulse:                                             // one-and-a-half cycles of a band-limited square
                {
                    const float dt = hz * inv;
                    const float p = ph; ph += dt; if (ph >= 1.f) ph -= 1.f;
                    float q = p + 0.5f; if (q >= 1.f) q -= 1.f;
                    x = ((p < 0.5f ? 1.f : -1.f) + polyBlep (p, dt) - polyBlep (q, dt)) * (t * hz / 1000.f < 1.5f ? 1.f : 0.f);
                    return finish (s, x * 0.8f, t, len, rate, L, R, juce::jmin (1.f, t / 0.05f));
                }
                case TrMetal:
                {
                    static constexpr float r[6] { 1.f, 1.4471f, 1.617f, 1.9265f, 2.5028f, 2.6637f };
                    float y = 0.f; for (size_t i = 0; i < 6; ++i) y += met[i].tick (Square, hz * 0.5f * r[i], inv);
                    x = bp.bp (y, doomsday::dsp::svfCoefs (hz, 0.7f, rate)) * 0.9f;
                    break;
                }
                case TrSnap:                                              // a saw that dives two octaves over the click
                {
                    const float f = hz * std::exp2 (-2.f * juce::jmin (1.f, t / len));
                    const float dt = f * inv;
                    const float p = ph; ph += dt; if (ph >= 1.f) ph -= 1.f;
                    x = (2.f * p - 1.f - polyBlep (p, dt)) * 0.8f;
                    break;
                }
                case TrCustom:
                    x = s.custom != nullptr ? s.custom->eval (juce::jlimit (0.f, 1.f, t / len), seg) : 0.f;
                    return finish (s, x, t, len, rate, L, R, juce::jlimit (0.f, 1.f, (len + 3.f - t) / 3.f));
                default:                                                  // SINE CLICK with a tiny pitch drop
                {
                    const float f = hz * (1.f + 1.5f * std::exp (-t / 0.4f));
                    ph += f * inv; if (ph >= 1.f) ph -= 1.f;
                    x = std::sin (twoPi * ph);
                    break;
                }
            }
            finish (s, x, t, len, rate, L, R, env);
        }
    private:
        inline void finish (const ClickSettings& s, float x, float t, float len, float rate, float& L, float& R, float env)
        {
            juce::ignoreUnused (t, len);
            x *= env;
            float y = tone.lp (x, doomsday::dsp::svfCoefs (300.f * std::exp2 (s.tone * 6.f), 0.8f, rate));
            if (s.drive > 0.f) { const float gd = 1.f + s.drive * 16.f; y = std::tanh (y * gd) / std::tanh (gd); }
            const float out = (x + (y - x) * s.mix) * s.gain;
            // WIDTH: the right channel hears the click ~0.6 ms later (Haas), blended by WIDTH
            const float delayed = dl[(size_t) ((di + dl.size() - delayN (rate)) % dl.size())];
            push (out);
            L = out; R = out + (delayed - out) * s.width;
            if (! std::isfinite (L) || ! std::isfinite (R)) L = R = 0.f;
        }
        inline size_t delayN (float rate) const { return (size_t) juce::jlimit (1, (int) dl.size() - 1, (int) (0.0006f * rate)); }
        inline void push (float v) { dl[di] = v; di = (di + 1) % dl.size(); }
        Noise nz; float ph = 0.f; std::array<Osc, 6> met {};
        doomsday::dsp::Svf tone, bp;
        std::array<float, 512> dl {}; size_t di = 0;
        float at = 0.f; int seg = 0;
    };

    // ---- engine level: the attack gain envelope (from the last hit) + the transient shaper, on the kick before the clip
    struct ShaperSettings
    {
        float atkDb = 0.f, lowDb = 0.f, highDb = 0.f, atkMs = 2.f, decMs = 30.f;
        bool shaper = false; float attack = 0.f, sustain = 0.f, sens = 0.5f, timeMs = 50.f, mix = 1.f;
        bool active() const { return shaper || std::abs (atkDb) + std::abs (lowDb) + std::abs (highDb) > 0.01f; }
    };
    class AttackShaper
    {
    public:
        void prepare (double rate) { sr = (float) rate; reset(); }
        void reset() { lowL = lowR = {}; e1 = e2 = e3 = 0.f; gNow = 1.f; sinceHit = 1.0e9f; }
        void hit() { sinceHit = 0.f; }
        inline void process (const ShaperSettings& s, float* L, float* R, int n)
        {
            if (! s.active()) { sinceHit += (float) n / sr * 1000.f; return; }
            const auto split = doomsday::dsp::svfCoefs (220.f, 1.414f, sr);
            const float inv = 1000.f / sr;
            const float aFast = coef (0.3f), rFast = coef (juce::jmax (1.f, s.timeMs * 0.3f)), aSlow = coef (juce::jmax (1.f, s.timeMs * 0.5f)), rSlow = coef (s.timeMs);
            const float smooth = coef (0.5f);
            for (int i = 0; i < n; ++i)
            {
                // ATTACK LEVEL / TIME / DECAY from the last hit, split LOW / HIGH
                float gl = 1.f, gh = 1.f;
                if (std::abs (s.atkDb) + std::abs (s.lowDb) + std::abs (s.highDb) > 0.01f)
                {
                    const float t = sinceHit;
                    const float a = juce::jmax (0.01f, s.atkMs);
                    const float env = t < a ? t / a : std::exp (-(t - a) / juce::jmax (1.f, s.decMs));
                    gl = std::pow (10.f, (s.atkDb + s.lowDb) * env / 20.f);
                    gh = std::pow (10.f, (s.atkDb + s.highDb) * env / 20.f);
                }
                sinceHit += inv;
                const float lowl = lowL.lp (L[i], split), lowr = lowR.lp (R[i], split);
                float yl = lowl * gl + (L[i] - lowl) * gh, yr = lowr * gl + (R[i] - lowr) * gh;
                // the TRANSIENT SHAPER: real envelope followers on the signal
                if (s.shaper)
                {
                    const float x = 0.5f * (std::abs (yl) + std::abs (yr));
                    e1 += (x > e1 ? aFast : rFast) * (x - e1);      // fast attack, fast release
                    e2 += (x > e2 ? aFast : rSlow) * (x - e2);      // fast attack, slow release
                    e3 += (x > e3 ? aSlow : rSlow) * (x - e3);      // slow attack, slow release
                    const float d1 = 20.f * std::log10 ((e1 + 1.0e-6f) / (e3 + 1.0e-6f));     // > 0 at onsets
                    const float d2 = 20.f * std::log10 ((e2 + 1.0e-6f) / (e1 + 1.0e-6f));     // > 0 while it decays
                    const float k = 0.25f + 1.5f * s.sens;
                    // SUSTAIN acts where the detector sees no onset (steady / decaying parts) + while it decays
                    const float steady = 1.f - juce::jlimit (0.f, 1.f, d1 / 6.f);
                    const float gainDb = juce::jlimit (-24.f, 24.f, k * (s.attack * juce::jlimit (0.f, 24.f, d1)
                                                                       + s.sustain * (juce::jlimit (0.f, 24.f, d2) + 8.f * steady)));
                    gNow += smooth * (std::pow (10.f, gainDb / 20.f) - gNow);
                    yl = yl + (yl * gNow - yl) * s.mix; yr = yr + (yr * gNow - yr) * s.mix;
                }
                L[i] = std::isfinite (yl) ? yl : 0.f; R[i] = std::isfinite (yr) ? yr : 0.f;
            }
        }
    private:
        float coef (float ms) const { return 1.f - std::exp (-1.f / juce::jmax (1.f, ms * 0.001f * sr)); }
        float sr = 176400.f;
        doomsday::dsp::Svf lowL, lowR;
        float e1 = 0.f, e2 = 0.f, e3 = 0.f, gNow = 1.f, sinceHit = 1.0e9f;
    };
}
