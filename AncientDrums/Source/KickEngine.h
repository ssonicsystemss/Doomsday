#pragma once
// =====================================================================
//  ANCIENT DRUMS v2 - kick engine
//
//  PUNCH / CRUNCH / KICK sample layers (offset, envelope, reverse, pitch, filters)
//     layers with FORGE on  ->  FORGE  --+
//     layers with FORGE off ------------+--> PRE-GAIN -> CLIP -> LIMIT -> VOLUME
//
//  FORGE (the hardstyle tail chain), v2.1 - tuned to stay musical:
//     FOCUS pre-emphasis -> DRIVE (asym tube, 4x OS, fizz low-pass, de-emphasis)
//     -> GRIT (gated, band-limited short reverb fed into the next stage)
//     -> CRUNCH (mid presence + band-limited soft-knee clip, 4x OS)
//     -> TONE (tilt EQ) -> OTT (3-band, gentle) -> SMOOTH (de-harsh) -> MIX (parallel)
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
    float driveDb = 18.f, focusHz = 520.f, grit = 0.2f, crunch = 0.55f, tone = 0.25f, ott = 0.3f, smooth = 0.3f, mix = 1.f;
};

struct KickParams
{
    std::array<LayerParams, NumSampleLayers> layer;
    ForgeParams forge;
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
        for (auto* b : { &dryBuf, &forgeBuf, &tmpBuf, &tmpIn })
            b->assign ((size_t) maxBlockSize, 0.f);
        mixDelay.assign (64, 0.f);
        gritReverb.setSampleRate (sr);

        auto makeOS = [this] { auto o = std::make_unique<juce::dsp::Oversampling<float>> (
                                   1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
                               o->initProcessing ((size_t) maxBlockSize); return o; };
        clipOS = makeOS();
        forgeOS1 = makeOS();
        forgeOS2 = makeOS();
        osLatency = (int) std::round (forgeOS1->getLatencyInSamples());

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
        gritReverb.reset();
        std::fill (mixDelay.begin(), mixDelay.end(), 0.f);
        mixWrite = 0;
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
            lv.audibleFrom = juce::jmax (0.0, -lv.offset);   // negative offset trims the start
            lv.reverse = lp.reverse;
            lv.forge = lp.forge;
            lv.active = true;
        }

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
        double rate = 1.0, offset = 0.0, audibleFrom = 0.0;
        int n = 0;
        size_t envSeg = 0;
        bool active = false, reverse = false, forge = true;
        Svf hp, lp;
    };
    struct Voice
    {
        bool active = false;
        float steal = 1.f, stealStep = 0.f;
        std::array<LayerVoice, NumSampleLayers> layer;
    };

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
        const double a = t - lv.audibleFrom;                   // time since the layer became audible
        const float ms = (float) (a * 1000.0 / sr);           // envelope time: 0 = where the layer starts sounding
        float g = 1.f;
        if (lv.env != nullptr)
        {
            if (lv.env->endsSilent() && ms >= lv.env->endMs()) return false;
            g = lv.env->eval (ms, lv.envSeg);
        }
        if (a < 24.0) g *= (float) (a / 24.0);                 // 0.5 ms anti-click where the layer starts
        out = hermite (d, lv.reverse ? len - pos : pos) * g;
        return true;
    }

    // ------------------------------------------------------------ FORGE
    struct ForgeState
    {
        Svf focus, deEmph, fizz1, fizz2;               // drive stage (oversampled)
        Svf presence, preLp, post;                     // crunch stage (oversampled)
        Svf toneLo, toneHi, smoothLp1, smoothLp2, smoothDip;
        float dc1x = 0.f, dc1y = 0.f, dc2x = 0.f, dc2y = 0.f;
        float gate = 0.f, gritHp = 0.f, gritLp = 0.f;
        float envIn = 0.f, envOut = 0.f, match = 1.f;
        float bandLo = 0.f, bandHiLp = 0.f;
        std::array<float, 3> bandEnv {};
    } fz;

    static inline float tube (float x)          // asymmetric: even + odd harmonics
    {
        return std::tanh (x + 0.25f) - 0.2449187f;
    }

    void runForge (float* buf, int len)
    {
        const auto& f = p.forge;
        const double osr = sr * 4.0;

        // dry copy for the parallel MIX, delayed by the oversampling latency so the blend doesn't comb
        const bool parallel = f.mix < 0.999f;
        // level follower of the input: the FORGE output is matched back to it at the end (no loudness jump)
        {
            const float att = 1.f - std::exp (-1.f / (0.001f * (float) sr));
            const float rel = 1.f - std::exp (-1.f / (0.12f * (float) sr));
            for (int i = 0; i < len; ++i)
            {
                const float a = std::abs (buf[i]);
                fz.envIn += (a > fz.envIn ? att : rel) * (a - fz.envIn);
                tmpIn[(size_t) i] = fz.envIn;
            }
        }
        mixLatency = osLatency * (f.crunch > 0.001f ? 2 : 1);
        if (parallel) std::copy (buf, buf + len, dryBuf.begin());

        // ---- stage 1: FOCUS pre-emphasis -> DRIVE -> fizz low-pass -> de-emphasis (oversampled)
        {
            float cF[4];  coeffs (cF, f.focusHz, 0.8f, osr);
            float cZ[4];  coeffs (cZ, 12500.f, 0.6f, osr);
            const float pre  = juce::Decibels::decibelsToGain (7.f);
            const float post = juce::Decibels::decibelsToGain (-3.5f);
            const float drive = juce::Decibels::decibelsToGain (f.driveDb + 2.f);
            const float norm = 1.f / (0.55f + 0.45f * std::tanh (drive * 0.5f));
            float* chans[] = { buf };
            juce::dsp::AudioBlock<float> block (chans, 1, (size_t) len);
            auto os = forgeOS1->processSamplesUp (block);
            float* o = os.getChannelPointer (0);
            for (size_t i = 0; i < os.getNumSamples(); ++i)
            {
                float y = tube (fz.focus.peak (o[i], cF, pre) * drive) * norm;
                y = fz.fizz1.lp (y, cZ);                         // the drive's fizz never reaches the next stages
                o[i] = fz.deEmph.peak (y, cF, post);
            }
            forgeOS1->processSamplesDown (block);
        }
        for (int i = 0; i < len; ++i)       // DC blocker (asymmetric curve adds DC)
        {
            const float x = buf[i];
            fz.dc1y = x - fz.dc1x + 0.9995f * fz.dc1y; fz.dc1x = x;
            buf[i] = fz.dc1y;
        }

        // ---- GRIT: short reverb, band-limited (150 Hz..3.5 kHz) and gated by the kick, pushed into CRUNCH
        if (f.grit > 0.001f)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.38f; rp.damping = 0.55f; rp.wetLevel = 1.f; rp.dryLevel = 0.f; rp.width = 0.f;
            gritReverb.setParameters (rp);
            std::copy (buf, buf + len, tmpBuf.begin());
            gritReverb.processMono (tmpBuf.data(), len);
            const float att = 1.f - std::exp (-1.f / (0.0005f * (float) sr));
            const float rel = 1.f - std::exp (-1.f / (0.040f * (float) sr));
            const float aHp = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 150.f / (float) sr);
            const float aLp = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 3500.f / (float) sr);
            for (int i = 0; i < len; ++i)
            {
                float r = tmpBuf[(size_t) i];
                fz.gritHp += aHp * (r - fz.gritHp); r -= fz.gritHp;
                fz.gritLp += aLp * (r - fz.gritLp); r = fz.gritLp;
                const float a = std::abs (buf[i]);
                fz.gate += (a > fz.gate ? att : rel) * (a - fz.gate);
                buf[i] += f.grit * 1.3f * r * juce::jlimit (0.f, 1.f, fz.gate * 3.f);
            }
        }

        // ---- stage 2: CRUNCH (mid presence, band-limited, soft-knee clip, oversampled)
        if (f.crunch > 0.001f)
        {
            float cP[4]; coeffs (cP, 2400.f, 0.6f, osr);
            float cB[4]; coeffs (cB, 7000.f, 0.6f, osr);
            float cL[4]; coeffs (cL, 11000.f, 0.6f, osr);
            const float pres = juce::Decibels::decibelsToGain (12.f * f.crunch);
            const float gain = juce::Decibels::decibelsToGain (24.f * f.crunch);
            const float mix = juce::jlimit (0.f, 1.f, f.crunch * 1.5f);
            const float makeup = 0.85f;
            float* chans[] = { buf };
            juce::dsp::AudioBlock<float> block (chans, 1, (size_t) len);
            auto os = forgeOS2->processSamplesUp (block);
            float* o = os.getChannelPointer (0);
            for (size_t i = 0; i < os.getNumSamples(); ++i)
            {
                const float x = o[i];
                const float in = fz.preLp.lp (fz.presence.peak (x, cP, pres), cB) * gain;
                const float y = softClip (in, 1.f, 0.35f) * makeup;
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
            const float gLo = juce::Decibels::decibelsToGain (-4.f * f.tone);
            const float gHi = juce::Decibels::decibelsToGain (4.5f * f.tone);
            for (int i = 0; i < len; ++i)
                buf[i] = fz.toneHi.highShelf (fz.toneLo.lowShelf (buf[i], cLo, gLo), cHi, gHi);
        }

        // ---- OTT: 3 bands pulled towards a target level - gentle upward gain so tails don't turn to noise
        if (f.ott > 0.001f)
        {
            const float aLo = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 120.f / (float) sr);
            const float aHi = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 2500.f / (float) sr);
            const float att = 1.f - std::exp (-1.f / (0.002f * (float) sr));
            const float rel = 1.f - std::exp (-1.f / (0.08f * (float) sr));
            const float targets[3] = { 0.30f, 0.20f, 0.12f };
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
                    const float ratio = targets[b] / juce::jmax (e, 0.01f);
                    const float g = juce::jlimit (0.3f, 4.f, std::pow (ratio, 0.6f));
                    y += bands[b] * g;
                }
                buf[i] = x + f.ott * (y - x);
            }
        }

        // ---- SMOOTH: takes the edge off - 12 dB/oct low-pass sliding 20k -> 5k + a dip at the ear's harsh spot
        if (f.smooth > 0.001f)
        {
            const float hz = 20000.f * std::pow (0.3f, f.smooth);
            float cS[4]; coeffs (cS, hz, 0.6f, sr);
            float cD[4]; coeffs (cD, 3300.f, 1.1f, sr);
            const float dip = juce::Decibels::decibelsToGain (-4.f * f.smooth);
            for (int i = 0; i < len; ++i)
                buf[i] = fz.smoothDip.peak (fz.smoothLp2.lp (fz.smoothLp1.lp (buf[i], cS), cS), cD, dip);
        }

        // ---- LEVEL MATCH: FORGE changes the colour, not the loudness - the master clipper isn't slammed
        {
            const float att = 1.f - std::exp (-1.f / (0.001f * (float) sr));
            const float rel = 1.f - std::exp (-1.f / (0.12f * (float) sr));
            const float gs  = 1.f - std::exp (-1.f / (0.01f * (float) sr));
            for (int i = 0; i < len; ++i)
            {
                const float a = std::abs (buf[i]);
                fz.envOut += (a > fz.envOut ? att : rel) * (a - fz.envOut);
                const float target = fz.envOut > 1.0e-5f ? juce::jlimit (0.2f, 2.f, 1.1f * tmpIn[(size_t) i] / fz.envOut) : 1.f;
                fz.match += gs * (target - fz.match);
                buf[i] *= fz.match;
            }
        }

        // ---- MIX: parallel blend with the clean layers
        if (parallel)
        {
            const int lat = juce::jlimit (0, (int) mixDelay.size() - 1, mixLatency);
            const int sz = (int) mixDelay.size();
            for (int i = 0; i < len; ++i)
            {
                mixDelay[(size_t) mixWrite] = dryBuf[(size_t) i];
                int rp = mixWrite - lat; if (rp < 0) rp += sz;
                const float d = mixDelay[(size_t) rp];
                if (++mixWrite >= sz) mixWrite = 0;
                buf[i] = d + f.mix * (buf[i] - d);
            }
        }
        else
        {
            for (int i = 0; i < len; ++i)             // keep the delay line warm for a click-free switch
            {
                mixDelay[(size_t) mixWrite] = 0.f;
                if (++mixWrite >= (int) mixDelay.size()) mixWrite = 0;
            }
        }
    }

    // ------------------------------------------------------------ render
    void renderSegment (float* outL, float* outR, int len)
    {
        float hpC[NumSampleLayers][4], lpC[NumSampleLayers][4];
        bool useHp[NumSampleLayers], useLp[NumSampleLayers];
        for (int l = 0; l < NumSampleLayers; ++l)
        {
            useHp[l] = p.layer[(size_t) l].hpHz > 21.f;
            useLp[l] = p.layer[(size_t) l].lpHz < 19900.f;
            coeffs (hpC[l], p.layer[(size_t) l].hpHz, 0.707f, sr);
            coeffs (lpC[l], p.layer[(size_t) l].lpHz, 0.707f, sr);
        }
        bool anyForge = false;

        // ---------------- layers -> dry / forge buses
        for (int i = 0; i < len; ++i)
        {
            float dry = 0.f, frg = 0.f;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                float vd = 0.f, vf = 0.f;
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

                float st = 1.f;
                if (v.stealStep > 0.f)
                {
                    v.steal -= v.stealStep;
                    if (v.steal <= 0.f) { v.active = false; continue; }
                    st = v.steal;
                }
                if (! any) v.active = false;
                dry += vd * st; frg += vf * st;
            }
            outL[i] = dry;
            forgeBuf[(size_t) i] = frg;
            anyForge |= frg != 0.f;
        }

        // ---------------- FORGE (always run while its tails may still ring)
        if (p.forge.on)
        {
            if (anyForge) forgeIdle = 0; else forgeIdle += len;
            if (forgeIdle < (int) sr)       // keep processing ~1 s after the last input for reverb/OTT tails
                runForge (forgeBuf.data(), len);
            else
                std::fill (forgeBuf.begin(), forgeBuf.begin() + len, 0.f);
        }

        // ---------------- master
        sPre.setTargetValue (p.preGain);
        sVol.setTargetValue (p.volume);
        for (int i = 0; i < len; ++i)
            outL[i] = (outL[i] + forgeBuf[(size_t) i]) * sPre.getNextValue();

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

    std::vector<float> dryBuf, forgeBuf, tmpBuf, tmpIn, mixDelay;
    int forgeIdle = 1 << 30, mixWrite = 0, mixLatency = 0, osLatency = 0;
    juce::Reverb gritReverb;

    std::unique_ptr<juce::dsp::Oversampling<float>> clipOS, forgeOS1, forgeOS2;
    juce::dsp::Limiter<float> limiter;
    juce::SmoothedValue<float> sPre, sVol;
    int hitWrite = 0;
};
