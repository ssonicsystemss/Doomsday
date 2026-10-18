#pragma once
// =====================================================================
//  ANCIENT DRUMS v2 - kick engine
//
//  PUNCH / CRUNCH / KICK sample layers (offset, envelope, reverse, pitch, filters)
//     layers with FORGE on  ->  FORGE  --+
//     layers with FORGE off ------------+--> + SUB -> RUMBLE -> PRE-GAIN -> CLIP -> LIMIT -> VOLUME
//
//  FORGE (the hardstyle tail chain):
//     FOCUS pre-EQ (mid boost) -> DRIVE (asym tube, 4x OS)
//     -> GRIT (gated short reverb fed into the next stage)
//     -> CRUNCH (presence boost + hard clip, 4x OS)
//     -> TONE (tilt EQ) -> OTT (3-band up/down compression)
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "SampleLibrary.h"
#include "Envelope.h"
#include <atomic>

struct LayerParams
{
    bool  on = true;
    float gain = 1.f, pitch = 0.f, offsetMs = 0.f, hpHz = 20.f, lpHz = 20000.f;
    bool  keyTrack = true, reverse = false, forge = true;
    SamplePtr sample;
    EnvPtr env;
};

struct ForgeParams
{
    bool  on = true;
    float driveDb = 18.f, focusHz = 450.f, grit = 0.35f, crunch = 0.45f, tone = 0.2f, ott = 0.4f;
};

struct KickParams
{
    std::array<LayerParams, NumSampleLayers> layer;
    ForgeParams forge;
    // sub
    bool  subOn = false; int subWave = 0; int subOct = 0;
    float subGain = 0.5f, subAttackMs = 2.f, subLengthMs = 500.f, subToneHz = 400.f, subDrive = 0.f;
    // rumble
    bool  rumOn = false;
    float rumGain = 0.5f, rumDelayMs = 60.f, rumDecay = 0.6f, rumDrive = 0.5f, rumLpHz = 120.f, rumDuck = 0.7f;
    // master
    int   root = 60;
    float preGain = 1.f; bool clipOn = true; float clipCeil = 0.95f, clipKnee = 0.2f;
    bool  limOn = true; float limCeilDb = -0.3f, limRelMs = 60.f; float volume = 1.f;
};

class KickEngine
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        maxBlockSize = juce::jmax (32, maxBlock);
        for (auto* b : { &dryBuf, &forgeBuf, &busBuf, &rumBuf, &tmpBuf })
            b->assign ((size_t) maxBlockSize, 0.f);

        rumDelay.assign ((size_t) (sr * 0.5) + 8, 0.f);
        rumWrite = 0;
        rumReverb.setSampleRate (sr);
        gritReverb.setSampleRate (sr);

        auto makeOS = [this] { auto o = std::make_unique<juce::dsp::Oversampling<float>> (
                                   1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
                               o->initProcessing ((size_t) maxBlockSize); return o; };
        clipOS = makeOS();
        forgeOS1 = makeOS();
        forgeOS2 = makeOS();

        limiter.prepare ({ sr, (juce::uint32) maxBlockSize, 1 });

        hitBuffer.assign ((size_t) (sr * 2.0), 0.f);
        hitLength.store (0);

        sPre.reset (sr, 0.02);  sPre.setCurrentAndTargetValue (p.preGain);
        sVol.reset (sr, 0.02);  sVol.setCurrentAndTargetValue (p.volume);
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v = {};
        std::fill (rumDelay.begin(), rumDelay.end(), 0.f);
        rumReverb.reset(); gritReverb.reset();
        rumLp1 = {}; rumLp2 = {}; rumHp = 0.f; duckEnv = 0.f; rumWindow = 0.f; rumCountdown = 0;
        fz = {};
        for (auto* o : { clipOS.get(), forgeOS1.get(), forgeOS2.get() }) if (o) o->reset();
        limiter.reset();
    }

    void setParams (const KickParams& np) { p = np; }

    void trigger (int note)
    {
        for (auto& v : voices)
            if (v.active) v.stealStep = 1.f / (float) (0.004 * sr);

        auto& v = voices[(size_t) nextVoice];
        nextVoice = (nextVoice + 1) % (int) voices.size();
        v = {};
        v.active = true;

        const float noteSemis = (float) (note - p.root);
        for (int l = 0; l < NumSampleLayers; ++l)
        {
            const auto& lp = p.layer[(size_t) l];
            auto& lv = v.layer[(size_t) l];
            if (! lp.on || lp.sample == nullptr || lp.sample->data.size() < 4) continue;
            const float semis = lp.pitch + (lp.keyTrack ? noteSemis : 0.f);
            lv.sample = lp.sample;
            lv.env = lp.env;
            lv.rate = lp.sample->sampleRate / sr * std::pow (2.0, semis / 12.0);
            lv.offset = lp.offsetMs * 0.001 * sr;
            lv.reverse = lp.reverse;
            lv.forge = lp.forge;
            lv.active = true;
        }

        if (p.subOn)
        {
            auto& s = v.sub;
            const double hz = Library::tailHz * std::pow (2.0, (noteSemis + 12.f * (float) p.subOct) / 12.0);
            s.inc = hz / sr;
            s.len = juce::jmax (16, (int) (p.subLengthMs * 0.001 * sr));
            s.att = juce::jlimit (1, s.len / 2, (int) (p.subAttackMs * 0.001 * sr));
            s.fade = juce::jlimit (8, s.len, (int) (0.35 * s.len));
            s.active = true;
        }

        rumCountdown = juce::jmax (1, (int) (p.rumDelayMs * 0.001 * sr));
        hitWrite = 0;
        hitLength.store (0);
        hitSerial.fetch_add (1);
    }

    void process (float* outL, float* outR, int numSamples)
    {
        int pos = 0;
        while (pos < numSamples)
        {
            const int n = juce::jmin (maxBlockSize, numSamples - pos);
            renderSegment (outL + pos, outR + pos, n);
            pos += n;
        }
    }

    // last hit capture for the GUI (playhead + meters)
    std::vector<float> hitBuffer;
    std::atomic<int> hitLength { 0 };
    std::atomic<int> hitSerial { 0 };

    // ------------------------------------------------------------ shared DSP helpers (also used offline)
    static inline float hermite (const std::vector<float>& d, double pos)
    {
        const int sz = (int) d.size();
        const int i = juce::jlimit (0, sz - 1, (int) pos);
        const float f = (float) (pos - (double) i);
        const float xm1 = d[(size_t) juce::jmax (0, i - 1)];
        const float x0  = d[(size_t) i];
        const float x1  = d[(size_t) juce::jmin (sz - 1, i + 1)];
        const float x2  = d[(size_t) juce::jmin (sz - 1, i + 2)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    // ------------------------------------------------------------ filters
    struct Svf
    {
        float ic1 = 0.f, ic2 = 0.f;
        inline void tick (float v0, const float* c, float& l, float& b)
        {
            const float v3 = v0 - ic2;
            const float v1 = c[0] * ic1 + c[1] * v3;
            const float v2 = ic2 + c[1] * ic1 + c[2] * v3;
            ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
            l = v2; b = v1;
        }
        inline float lp (float v0, const float* c) { float l, b; tick (v0, c, l, b); return l; }
        inline float hp (float v0, const float* c) { float l, b; tick (v0, c, l, b); return v0 - c[3] * b - l; }
        // peaking / shelving built on the SVF outputs (gain g = linear)
        inline float peak (float v0, const float* c, float g) { float l, b; tick (v0, c, l, b); return v0 + (g - 1.f) * c[3] * b; }
        inline float lowShelf (float v0, const float* c, float g) { float l, b; tick (v0, c, l, b); return v0 + (g - 1.f) * l; }
        inline float highShelf (float v0, const float* c, float g) { float l, b; tick (v0, c, l, b); return v0 + (g - 1.f) * (v0 - c[3] * b - l); }
    };
    static void coeffs (float* c, float hz, float q, double rate)
    {
        const float g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (10.f, (float) (rate * 0.45), hz) / (float) rate);
        const float k = 1.f / q;
        c[0] = 1.f / (1.f + g * (g + k)); c[1] = g * c[0]; c[2] = g * c[1]; c[3] = k;
    }

    // ------------------------------------------------------------ voices
    struct LayerVoice
    {
        SamplePtr sample;
        EnvPtr env;
        double rate = 1.0, offset = 0.0;
        int n = 0;
        size_t envSeg = 0;
        bool active = false, reverse = false, forge = true;
        Svf hp, lp;
    };
    struct SubVoice
    {
        double phase = 0.0, inc = 0.0;
        int n = 0, len = 0, att = 1, fade = 1;
        bool active = false;
        Svf lp;
    };
    struct Voice
    {
        bool active = false;
        float steal = 1.f, stealStep = 0.f;
        std::array<LayerVoice, NumSampleLayers> layer;
        SubVoice sub;
    };

    static inline float polyBlep (double t, double dt)
    {
        if (t < dt)       { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.f;
    }
    static inline float fadeCurve (int n, int len, int fade)
    {
        if (n < len - fade) return 1.f;
        if (n >= len) return 0.f;
        const float x = (float) (n - (len - fade)) / (float) fade;
        return 0.5f * (1.f + std::cos (juce::MathConstants<float>::pi * x));
    }
    static inline float softClip (float x, float ceil, float knee)
    {
        const float a = std::abs (x);
        const float t = ceil * (1.f - knee);
        if (a <= t) return x;
        const float s = x < 0.f ? -1.f : 1.f;
        if (knee <= 0.0001f) return s * juce::jmin (a, ceil);
        return s * (t + (ceil - t) * std::tanh ((a - t) / (ceil - t)));
    }

    // one layer sample at the current voice time; returns false when the layer is finished
    inline bool layerSample (LayerVoice& lv, float& out)
    {
        const double t = (double) lv.n++ - lv.offset;         // output samples since the layer's start
        if (t < 0.0) { out = 0.f; return true; }              // still waiting (positive offset)
        const auto& d = lv.sample->data;
        const double pos = t * lv.rate;
        const double len = (double) d.size() - 1.0;
        if (pos >= len) return false;
        const float ms = (float) (t * 1000.0 / sr);
        float g = 1.f;
        if (lv.env != nullptr)
        {
            if (lv.env->endsSilent() && ms >= lv.env->endMs()) return false;
            g = lv.env->eval (ms, lv.envSeg);
        }
        if (t < 24.0) g *= (float) (t / 24.0);                 // 0.5 ms anti-click at the layer start
        out = hermite (d, lv.reverse ? len - pos : pos) * g;
        return true;
    }

    // ------------------------------------------------------------ FORGE
    struct ForgeState
    {
        Svf focus, presence, toneLo, toneHi;
        float dc1x = 0.f, dc1y = 0.f, dc2x = 0.f, dc2y = 0.f;
        float gate = 0.f;
        float bandLo = 0.f, bandHiLp = 0.f;
        std::array<float, 3> bandEnv {};
        Svf post;
    } fz;

    static inline float tube (float x)          // asymmetric: even + odd harmonics
    {
        return std::tanh (x + 0.25f) - 0.2449187f;
    }
    static inline float crunchShape (float x)   // hard knee with a little fold-back grit
    {
        const float h = juce::jlimit (-1.f, 1.f, x);
        const float f = std::sin (juce::jlimit (-4.f, 4.f, x) * 0.9f);
        return 0.75f * h + 0.25f * f;
    }

    void runForge (float* buf, int len)
    {
        const auto& f = p.forge;
        const double osr = sr * 4.0;

        // ---- stage 1: FOCUS pre-EQ + DRIVE (oversampled)
        {
            float cF[4]; coeffs (cF, f.focusHz, 0.9f, osr);
            const float focusG = juce::Decibels::decibelsToGain (10.f);
            const float drive = juce::Decibels::decibelsToGain (f.driveDb + 4.f);
            float* chans[] = { buf };
            juce::dsp::AudioBlock<float> block (chans, 1, (size_t) len);
            auto os = forgeOS1->processSamplesUp (block);
            float* o = os.getChannelPointer (0);
            const float norm = 1.f / (0.6f + 0.4f * std::tanh (drive * 0.5f));
            for (size_t i = 0; i < os.getNumSamples(); ++i)
                o[i] = tube (fz.focus.peak (o[i], cF, focusG) * drive) * norm;
            forgeOS1->processSamplesDown (block);
        }
        // DC blocker (asymmetric curve adds DC)
        for (int i = 0; i < len; ++i)
        {
            const float x = buf[i];
            fz.dc1y = x - fz.dc1x + 0.9995f * fz.dc1y; fz.dc1x = x;
            buf[i] = fz.dc1y;
        }

        // ---- GRIT: short reverb, gated by the kick itself, pushed into the crunch stage
        if (f.grit > 0.001f)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.42f; rp.damping = 0.25f; rp.wetLevel = 1.f; rp.dryLevel = 0.f; rp.width = 0.f;
            gritReverb.setParameters (rp);
            std::copy (buf, buf + len, tmpBuf.begin());
            gritReverb.processMono (tmpBuf.data(), len);
            const float att = 1.f - std::exp (-1.f / (0.0005f * (float) sr));
            const float rel = 1.f - std::exp (-1.f / (0.045f * (float) sr));
            for (int i = 0; i < len; ++i)
            {
                const float a = std::abs (buf[i]);
                fz.gate += (a > fz.gate ? att : rel) * (a - fz.gate);
                const float g = juce::jlimit (0.f, 1.f, fz.gate * 3.f);
                buf[i] += f.grit * 2.5f * tmpBuf[(size_t) i] * g;
            }
        }

        // ---- stage 2: CRUNCH (presence boost into a hard clipper, oversampled)
        if (f.crunch > 0.001f)
        {
            float cP[4]; coeffs (cP, 2200.f, 0.7f, osr);
            float cL[4]; coeffs (cL, 13000.f, 0.6f, osr);
            const float pres = juce::Decibels::decibelsToGain (9.f * f.crunch);
            const float gain = juce::Decibels::decibelsToGain (26.f * f.crunch);
            const float mix = juce::jlimit (0.f, 1.f, f.crunch * 1.6f);
            float* chans[] = { buf };
            juce::dsp::AudioBlock<float> block (chans, 1, (size_t) len);
            auto os = forgeOS2->processSamplesUp (block);
            float* o = os.getChannelPointer (0);
            for (size_t i = 0; i < os.getNumSamples(); ++i)
            {
                const float x = o[i];
                const float y = crunchShape (fz.presence.highShelf (x, cP, pres) * gain) * 0.8f;
                o[i] = fz.post.lp (x + mix * (y - x), cL);
            }
            forgeOS2->processSamplesDown (block);
            for (int i = 0; i < len; ++i)
            {
                const float x = buf[i];
                fz.dc2y = x - fz.dc2x + 0.9995f * fz.dc2y; fz.dc2x = x;
                buf[i] = fz.dc2y;
            }
        }

        // ---- TONE: tilt around ~700 Hz
        if (std::abs (f.tone) > 0.01f)
        {
            float cLo[4]; coeffs (cLo, 280.f, 0.7f, sr);
            float cHi[4]; coeffs (cHi, 1800.f, 0.7f, sr);
            const float gLo = juce::Decibels::decibelsToGain (-6.f * f.tone);
            const float gHi = juce::Decibels::decibelsToGain (7.f * f.tone);
            for (int i = 0; i < len; ++i)
                buf[i] = fz.toneHi.highShelf (fz.toneLo.lowShelf (buf[i], cLo, gLo), cHi, gHi);
        }

        // ---- OTT: 3 bands pulled towards a target level (downward above, upward below)
        if (f.ott > 0.001f)
        {
            const float aLo = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 120.f / (float) sr);
            const float aHi = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 2500.f / (float) sr);
            const float att = 1.f - std::exp (-1.f / (0.002f * (float) sr));
            const float rel = 1.f - std::exp (-1.f / (0.06f * (float) sr));
            const float targets[3] = { 0.30f, 0.22f, 0.12f };
            for (int i = 0; i < len; ++i)
            {
                const float x = buf[i];
                fz.bandLo += aLo * (x - fz.bandLo);
                const float lo = fz.bandLo;
                const float rest = x - lo;
                fz.bandHiLp += aHi * (rest - fz.bandHiLp);
                const float mid = fz.bandHiLp;
                const float hi = rest - mid;
                const float bands[3] = { lo, mid, hi };
                float y = 0.f;
                for (int b = 0; b < 3; ++b)
                {
                    const float a = std::abs (bands[b]);
                    auto& e = fz.bandEnv[(size_t) b];
                    e += (a > e ? att : rel) * (a - e);
                    const float ratio = targets[b] / juce::jmax (e, 0.003f);
                    const float g = juce::jlimit (0.25f, 8.f, std::pow (ratio, 0.75f));
                    y += bands[b] * g;
                }
                buf[i] = x + f.ott * (y - x);
            }
        }
    }

    // ------------------------------------------------------------ render
    void renderSegment (float* outL, float* outR, int len)
    {
        float hpC[NumSampleLayers][4], lpC[NumSampleLayers][4], subC[4];
        bool useHp[NumSampleLayers], useLp[NumSampleLayers];
        for (int l = 0; l < NumSampleLayers; ++l)
        {
            useHp[l] = p.layer[(size_t) l].hpHz > 21.f;
            useLp[l] = p.layer[(size_t) l].lpHz < 19900.f;
            coeffs (hpC[l], p.layer[(size_t) l].hpHz, 0.707f, sr);
            coeffs (lpC[l], p.layer[(size_t) l].lpHz, 0.707f, sr);
        }
        coeffs (subC, p.subToneHz, 0.707f, sr);
        const float subG = 1.f + p.subDrive * 12.f;
        const float subNorm = 1.f / std::tanh (subG);
        bool anyForge = false;

        // ---------------- layers -> dry / forge buses, sub -> sub bus (in busBuf)
        for (int i = 0; i < len; ++i)
        {
            float dry = 0.f, frg = 0.f, sub = 0.f;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                float vd = 0.f, vf = 0.f, vs = 0.f;
                bool any = false;

                for (int l = 0; l < NumSampleLayers; ++l)
                {
                    auto& lv = v.layer[(size_t) l];
                    if (! lv.active) continue;
                    float x;
                    if (! layerSample (lv, x)) { lv.active = false; continue; }
                    any = true;
                    if (useHp[l]) x = lv.hp.hp (x, hpC[l]);
                    if (useLp[l]) x = lv.lp.lp (x, lpC[l]);
                    const auto& lp = p.layer[(size_t) l];
                    x *= lp.on ? lp.gain : 0.f;
                    if (lv.forge && p.forge.on) vf += x; else vd += x;
                }

                auto& s = v.sub;
                if (s.active)
                {
                    if (s.n >= s.len) s.active = false;
                    else
                    {
                        any = true;
                        float x;
                        switch (p.subWave)
                        {
                            case 0:  x = std::sin ((float) (s.phase * juce::MathConstants<double>::twoPi)); break;
                            case 1:  x = (float) (2.0 * s.phase - 1.0) - polyBlep (s.phase, s.inc); break;
                            default:
                                x = s.phase < 0.5 ? 1.f : -1.f;
                                x += polyBlep (s.phase, s.inc) - polyBlep (std::fmod (s.phase + 0.5, 1.0), s.inc);
                        }
                        s.phase += s.inc; if (s.phase >= 1.0) s.phase -= 1.0;
                        if (p.subWave != 0) x = s.lp.lp (x, subC);
                        if (p.subDrive > 0.001f) x = std::tanh (x * subG) * subNorm;
                        const float att = s.n < s.att ? (float) s.n / (float) s.att : 1.f;
                        x *= att * fadeCurve (s.n, s.len, s.fade);
                        ++s.n;
                        vs += p.subOn ? x * p.subGain : 0.f;
                    }
                }

                float st = 1.f;
                if (v.stealStep > 0.f)
                {
                    v.steal -= v.stealStep;
                    if (v.steal <= 0.f) { v.active = false; continue; }
                    st = v.steal;
                }
                if (! any) v.active = false;
                dry += vd * st; frg += vf * st; sub += vs * st;
            }
            dryBuf[(size_t) i] = dry;
            forgeBuf[(size_t) i] = frg;
            busBuf[(size_t) i] = sub;
            anyForge |= frg != 0.f;
        }

        // ---------------- FORGE (always run while its tails may still ring)
        if (p.forge.on)
        {
            if (anyForge) forgeIdle = 0; else forgeIdle += len;
            if (forgeIdle < (int) sr)       // keep processing ~1 s after the last input for reverb/OTT tails
                runForge (forgeBuf.data(), len);
        }
        for (int i = 0; i < len; ++i)
            busBuf[(size_t) i] += dryBuf[(size_t) i] + forgeBuf[(size_t) i];

        // ---------------- rumble
        const bool rumble = p.rumOn;
        if (rumble)
        {
            const int dSamp = juce::jlimit (1, (int) rumDelay.size() - 2, (int) (p.rumDelayMs * 0.001 * sr));
            for (int i = 0; i < len; ++i)
            {
                int rp = rumWrite - dSamp; if (rp < 0) rp += (int) rumDelay.size();
                rumBuf[(size_t) i] = rumDelay[(size_t) rp];
                rumDelay[(size_t) rumWrite] = busBuf[(size_t) i];
                if (++rumWrite >= (int) rumDelay.size()) rumWrite = 0;
            }
            juce::Reverb::Parameters rpm;
            rpm.roomSize = 0.3f + 0.68f * p.rumDecay;
            rpm.damping = 0.45f; rpm.wetLevel = 1.f; rpm.dryLevel = 0.f; rpm.width = 0.f;
            rumReverb.setParameters (rpm);
            rumReverb.processMono (rumBuf.data(), len);

            float lpCo[4]; coeffs (lpCo, p.rumLpHz, 0.6f, sr);
            const float g = 1.f + p.rumDrive * 20.f;
            const float norm = 1.f / std::tanh (juce::jmin (g, 4.f));
            const float hpC1 = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 30.f / (float) sr);
            const float duckRel = std::exp (-1.f / (0.09f * (float) sr));
            const double winSec = 0.15 + 1.05 * p.rumDecay;
            const float winCoef = (float) std::exp (std::log (0.001) / (winSec * sr));
            for (int i = 0; i < len; ++i)
            {
                float x = rumBuf[(size_t) i];
                rumHp += hpC1 * (x - rumHp); x -= rumHp;
                x = std::tanh (x * g) * norm;
                x = rumLp1.lp (x, lpCo);
                x = rumLp2.lp (x, lpCo);
                const float a = std::abs (busBuf[(size_t) i]);
                duckEnv = a > duckEnv ? a : duckEnv * duckRel;
                const float duck = 1.f - p.rumDuck * juce::jmin (1.f, duckEnv * 2.5f);
                if (rumCountdown > 0 && --rumCountdown == 0) rumWindow = 1.f;
                const float win = rumWindow;
                rumWindow *= winCoef;
                rumBuf[(size_t) i] = x * duck * p.rumGain * win;
            }
        }
        else
        {
            rumWindow = 0.f; rumCountdown = 0;
            if (rumWasOn) { rumReverb.reset(); rumLp1 = {}; rumLp2 = {}; rumHp = 0.f; }
            for (int i = 0; i < len; ++i)
            {
                rumDelay[(size_t) rumWrite] = busBuf[(size_t) i];
                if (++rumWrite >= (int) rumDelay.size()) rumWrite = 0;
            }
        }
        rumWasOn = rumble;

        // ---------------- master
        sPre.setTargetValue (p.preGain);
        sVol.setTargetValue (p.volume);
        for (int i = 0; i < len; ++i)
            outL[i] = (busBuf[(size_t) i] + (rumble ? rumBuf[(size_t) i] : 0.f)) * sPre.getNextValue();

        if (p.clipOn)
        {
            float* chans[] = { outL };
            juce::dsp::AudioBlock<float> block (chans, 1, (size_t) len);
            auto os = clipOS->processSamplesUp (block);
            float* o = os.getChannelPointer (0);
            for (size_t i = 0; i < os.getNumSamples(); ++i)
                o[i] = softClip (o[i], p.clipCeil, p.clipKnee);
            clipOS->processSamplesDown (block);
        }
        if (p.limOn)
        {
            limiter.setThreshold (p.limCeilDb);
            limiter.setRelease (p.limRelMs);
            float* chans[] = { outL };
            juce::dsp::AudioBlock<float> block (chans, 1, (size_t) len);
            limiter.process (juce::dsp::ProcessContextReplacing<float> (block));
        }
        for (int i = 0; i < len; ++i)
        {
            const float y = juce::jlimit (-1.5f, 1.5f, outL[i] * sVol.getNextValue());
            outL[i] = y;
            outR[i] = y;
            if (hitWrite < (int) hitBuffer.size()) hitBuffer[(size_t) hitWrite++] = y;
        }
        hitLength.store (hitWrite);
    }

    KickParams p;
    double sr = 44100.0;
    int maxBlockSize = 512;
    std::array<Voice, 3> voices;
    int nextVoice = 0;

    std::vector<float> dryBuf, forgeBuf, busBuf, rumBuf, tmpBuf;
    std::vector<float> rumDelay;
    int rumWrite = 0, forgeIdle = 1 << 30;
    juce::Reverb rumReverb, gritReverb;
    Svf rumLp1, rumLp2;
    float rumHp = 0.f, duckEnv = 0.f, rumWindow = 0.f;
    int rumCountdown = 0;
    bool rumWasOn = false;

    std::unique_ptr<juce::dsp::Oversampling<float>> clipOS, forgeOS1, forgeOS2;
    juce::dsp::Limiter<float> limiter;
    juce::SmoothedValue<float> sPre, sVol;
    int hitWrite = 0;
};
