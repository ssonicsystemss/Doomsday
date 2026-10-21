#pragma once
// =====================================================================
//  5CR34M 4LPH4 - screech engine (mono / legato voice)
//
//  What the owner's references showed (ScreamAlpha/PLAN.md): a strong, almost pure core tone + a dense, noisy,
//  never-steady "hash" above it, and every note thrown in with a pitch scoop. So the voice is:
//
//    pitch = note + glide + SCOOP (every note, also legato) + bend
//      |
//      +--> CORE   : sine <-> triangle <-> square at the note           (the tone you hum)
//      +--> HASH   : unison carriers, phase-modulated by a modulator at RATIO (index FM)
//                    + CHAOS: band-limited noise in the phase -> the highs never sit still
//                    -> high-pass -> DRIVE 1 -> COMB (tuned to the note, feedback) -> FORMANT peak
//      core + hash -> DRIVE 2 -> TONE (low-pass) -> AMP envelope
//
//  Everything above runs 4x oversampled. Real-time safe after prepare().
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <memory>
#include <cmath>
#include "../../Shared/Source/DoomsdayDSP.h"
#include "../../Shared/Source/DoomsdayDrive.h"

namespace alpha
{
struct Params
{
    // note gesture
    float scoopSt = 7.f, scoopMs = 70.f, scoopCurve = 0.5f;   // start this many semitones away, fall into the note (curve 0 = linear, 1 = fast)
    float glideMs = 25.f;
    float tuneSt = 0.f;                                        // fine tune of the whole voice (semitones)
    // core
    float coreLevel = 0.8f, coreShape = 0.15f;                // shape: 0 sine, 0.5 triangle, 1 square
    // hash
    float hashLevel = 0.5f;
    float hashDecayMs = 400.f, hashSustain = 1.f;             // the hash is brightest when the note is thrown, then settles
    int   voices = 5;
    float detuneCents = 25.f;
    float carrierSaw = 0.3f;                                   // 0 sine carrier, 1 saw carrier
    float ratio = 1.0f, index = 3.f;                           // FM modulator ratio / depth (radians / 2pi units ~ index)
    float chaos = 0.4f, chaosHz = 12.f;                        // every unison voice drifts in pitch on its own (pitched, never noise)
    float grit = 0.f;                                          // a little fast noise in the phase (keep small: lots = vacuum cleaner)
    float hashHp = 700.f;                                      // high-pass before the drive (keeps the hash above the core)
    float drive1Db = 24.f; int drive1Curve = 1;                // HARD
    float combMix = 0.f, combFb = 0.6f, combRatio = 1.f;       // comb tuned to note * ratio
    float formHz = 1500.f, formGainDb = 6.f, formQ = 2.f;
    // output
    float drive2Db = 6.f; int drive2Curve = 0;                 // SOFT
    float toneHz = 9000.f;
    float lowCut = 150.f;                                      // final high-pass: light bottom
    float attackMs = 2.f, decayMs = 300.f, sustain = 0.9f, releaseMs = 80.f;
    float outDb = -6.f;
};

class Engine
{
public:
    static constexpr int osLog2 = 2, os = 1 << osLog2, maxVoices = 7;

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate; osr = sr * os; maxBlockSize = juce::jmax (16, maxBlock);
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (1, osLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampler->initProcessing ((size_t) maxBlockSize);
        comb.assign ((size_t) (osr / 20.0) + 8, 0.f);         // down to 20 Hz
        heldNotes.reserve (128);
        reset();
    }

    void reset()
    {
        heldNotes.clear(); gate = false; env = 0.f; stage = Idle;
        for (auto& p : carPh) p = 0.f;
        modPh = 0.f; corePh = 0.f; noiseLp = 0.f; noiseLp2 = 0.f;
        hp1 = {}; hp2 = {}; form = {}; tone1 = {}; tone2 = {}; outHp1 = {}; outHp2 = {};
        d1.reset(); d2.reset(); dc.reset();
        std::fill (comb.begin(), comb.end(), 0.f); combW = 0;
        if (oversampler) oversampler->reset();
    }

    void setParams (const Params& np) { p = np; }

    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
    {
        const int n = buffer.getNumSamples();
        int pos = 0;
        for (const auto meta : midi)
        {
            const int ev = juce::jlimit (0, n, meta.samplePosition);
            if (ev > pos) { render (buffer, pos, ev - pos); pos = ev; }
            handle (meta.getMessage());
        }
        if (pos < n) render (buffer, pos, n - pos);
    }

private:
    enum Stage { Idle, Attack, Decay, Sustain, Release };

    void handle (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), m.getNoteNumber()), heldNotes.end());
            heldNotes.push_back (m.getNoteNumber());
            noteOn (m.getNoteNumber());
        }
        else if (m.isNoteOff())
        {
            const bool wasTop = ! heldNotes.empty() && heldNotes.back() == m.getNoteNumber();
            heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), m.getNoteNumber()), heldNotes.end());
            if (heldNotes.empty()) { gate = false; stage = Release; }
            else if (wasTop) noteOn (heldNotes.back());
        }
        else if (m.isPitchWheel()) bend = 2.f * ((m.getPitchWheelValue() - 8192) / 8192.f);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { heldNotes.clear(); gate = false; stage = Release; }
    }

    void noteOn (int note)
    {
        const bool sounding = stage != Idle;
        target = (float) note;
        if (! sounding || p.glideMs < 1.f) cur = target;
        scoopPos = 0.f;                                   // the gesture fires on EVERY note, legato included
        hashEnv = 1.f;
        if (! gate || ! sounding) { stage = Attack; if (! sounding) env = 0.f; }
        gate = true;
    }

    static float coreWave (float ph, float dt, float shape)
    {
        using doomsday::dsp::polyBlep;
        const float s = std::sin (ph * juce::MathConstants<float>::twoPi);
        const float tri = 1.f - 4.f * std::abs (ph - 0.5f);
        float sq = ph < 0.5f ? 1.f : -1.f;
        sq += polyBlep (ph, dt) - polyBlep (doomsday::dsp::wrap01 (ph + 0.5f), dt);
        if (shape < 0.5f) { const float t = shape * 2.f; return s + t * (tri - s); }
        const float t = (shape - 0.5f) * 2.f; return tri + t * (sq - tri);
    }

    void render (juce::AudioBuffer<float>& buffer, int start, int len)
    {
        while (len > 0)
        {
            const int n = juce::jmin (len, maxBlockSize);
            renderChunk (buffer, start, n);
            start += n; len -= n;
        }
    }

    void renderChunk (juce::AudioBuffer<float>& buffer, int start, int len)
    {
        using namespace doomsday::dsp;
        float* outL = buffer.getWritePointer (0, start);
        // work in a mono block, upsample, synthesize, downsample
        juce::dsp::AudioBlock<float> block (&outL, 1, (size_t) len);
        block.clear();
        auto up = oversampler->processSamplesUp (block);
        float* x = up.getChannelPointer (0);
        const int N = (int) up.getNumSamples();

        // per-block coefficients
        const float inv = (float) (1.0 / osr);
        const float glideC = p.glideMs < 1.f ? 0.f : std::exp (-1.f / (p.glideMs * 0.001f / 3.f * (float) osr));
        const float scoopStep = 1.f / juce::jmax (1.f, p.scoopMs * 0.001f * (float) osr);
        const float noiseC = onePoleCoef (2500.f, osr);                     // GRIT: fast noise
        const float driftC = onePoleCoef (p.chaosHz, osr);                  // CHAOS: slow per-voice drift
        const auto hpC = svfCoefs (p.hashHp, 1.41f, osr);
        const auto formC = svfCoefs (p.formHz, 1.f / juce::jmax (0.3f, p.formQ), osr);
        const float formG = juce::Decibels::decibelsToGain (p.formGainDb);
        const auto toneC = svfCoefs (p.toneHz, 1.41f, osr);
        const auto lowC = svfCoefs (p.lowCut, 1.41f, osr);
        const float g1 = juce::Decibels::decibelsToGain (p.drive1Db), g2 = juce::Decibels::decibelsToGain (p.drive2Db);
        const float mk1 = doomsday::drive::makeup (p.drive1Curve, g1), mk2 = doomsday::drive::makeup (p.drive2Curve, g2);
        const int nv = juce::jlimit (1, maxVoices, p.voices);
        std::array<float, maxVoices> det {};
        for (int v = 0; v < nv; ++v)
        {
            const float off = nv == 1 ? 0.f : -1.f + 2.f * (float) v / (float) (nv - 1);
            det[(size_t) v] = std::pow (2.f, off * p.detuneCents / 1200.f);
        }
        const float vnorm = 1.f / std::sqrt ((float) nv);
        const float attInc = 1.f / juce::jmax (1.f, p.attackMs * 0.001f * (float) osr);
        const float decC = timeCoef (p.decayMs, osr), relC = timeCoef (p.releaseMs, osr);
        const float outG = juce::Decibels::decibelsToGain (p.outDb);
        const float hashC = timeCoef (p.hashDecayMs, osr);
        const float nyq = (float) osr * 0.45f;

        for (int i = 0; i < N; ++i)
        {
            // amp envelope
            switch (stage)
            {
                case Attack:  env += attInc; if (env >= 1.f) { env = 1.f; stage = Decay; } break;
                case Decay:   env = p.sustain + (env - p.sustain) * decC; if (std::abs (env - p.sustain) < 1e-4f) stage = Sustain; break;
                case Sustain: env = p.sustain; break;
                case Release: env *= relC; if (env < 1e-5f) { env = 0.f; stage = Idle; } break;
                case Idle:    env = 0.f; break;
            }
            if (stage == Idle) { x[i] = 0.f; continue; }

            // pitch: glide + scoop gesture
            cur = target + (cur - target) * glideC;
            float scoop = 0.f;
            if (scoopPos < 1.f)
            {
                scoopPos += scoopStep;
                const float t = juce::jmin (1.f, scoopPos);
                const float k = 1.f + 6.f * p.scoopCurve;                        // curve: how fast it lands
                scoop = p.scoopSt * std::pow (1.f - t, k);
            }
            const float semi = cur + scoop + bend + p.tuneSt;
            const float f0 = juce::jmin (440.f * std::pow (2.f, (semi - 69.f) / 12.f), nyq);
            const float dt = f0 * inv;

            // core
            corePh += dt; if (corePh >= 1.f) corePh -= 1.f;
            const float core = coreWave (corePh, dt, p.coreShape) * p.coreLevel;

            // hash: FM + chaos noise in the phase
            noiseLp += noiseC * (noise.next() - noiseLp);
            noiseLp2 += noiseC * (noiseLp - noiseLp2);
            modPh += dt * p.ratio; modPh -= std::floor (modPh);
            const float pm = p.index * std::sin (modPh * juce::MathConstants<float>::twoPi) * 0.15915494f
                           + p.grit * 6.f * noiseLp2;
            float hash = 0.f;
            for (int v = 0; v < nv; ++v)
            {
                float& ph = carPh[(size_t) v];
                float& d = drift[(size_t) v];
                d += driftC * (noise.next() * 2.5f - d);
                const float dtv = dt * det[(size_t) v] * (1.f + p.chaos * 0.035f * d);   // up to ~ +-60 cents
                ph += dtv; if (ph >= 1.f) ph -= 1.f;
                const float t = wrap01 (ph + pm);
                const float s = std::sin (t * juce::MathConstants<float>::twoPi);
                const float saw = 2.f * t - 1.f;
                hash += s + p.carrierSaw * (saw - s);
            }
            hash *= vnorm;
            hash = hp2.hp (hp1.hp (hash, hpC), hpC);
            hash = d1.process (hash * g1, p.drive1Curve) * mk1;

            // comb tuned to the note
            if (p.combMix > 0.f)
            {
                const float delay = juce::jlimit (2.f, (float) comb.size() - 3.f, (float) osr / juce::jmax (20.f, f0 * p.combRatio));
                float rp = (float) combW - delay; if (rp < 0.f) rp += (float) comb.size();
                const int i0 = (int) rp, i1 = (i0 + 1) % (int) comb.size();
                const float fr = rp - (float) i0;
                const float dly = comb[(size_t) i0] + fr * (comb[(size_t) i1] - comb[(size_t) i0]);
                comb[(size_t) combW] = juce::jlimit (-4.f, 4.f, hash + p.combFb * dly);
                if (++combW >= (int) comb.size()) combW = 0;
                hash += p.combMix * (dly - hash * 0.5f);
            }
            hash = form.peak (hash, formC, formG);

            hashEnv *= hashC;
            const float hashAmt = p.hashSustain + (1.f - p.hashSustain) * hashEnv;
            float y = core + hash * p.hashLevel * hashAmt;
            y = d2.process (y * g2, p.drive2Curve) * mk2;
            y = tone2.lp (tone1.lp (y, toneC), toneC);
            x[i] = y * env;
        }

        oversampler->processSamplesDown (block);
        for (int i = 0; i < len; ++i)
        {
            float y = outHp2.hp (outHp1.hp (outL[i], lowC), lowC);
            y = dc.process (y) * outG;
            outL[i] = y;
        }
        for (int ch = 1; ch < buffer.getNumChannels(); ++ch) buffer.copyFrom (ch, start, buffer, 0, start, len);
    }

    Params p;
    double sr = 48000.0, osr = 192000.0;
    int maxBlockSize = 512;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::vector<int> heldNotes;
    bool gate = false;
    Stage stage = Idle;
    float hashEnv = 0.f;
    float env = 0.f, cur = 60.f, target = 60.f, bend = 0.f, scoopPos = 1.f;
    std::array<float, maxVoices> carPh { 0.f, 0.37f, 0.71f, 0.13f, 0.89f, 0.52f, 0.26f };
    float modPh = 0.f, corePh = 0.f, noiseLp = 0.f, noiseLp2 = 0.f;
    std::array<float, maxVoices> drift {};
    doomsday::dsp::XorNoise noise;
    doomsday::dsp::Svf hp1, hp2, form, tone1, tone2, outHp1, outHp2;
    doomsday::drive::Adaa1 d1, d2;
    doomsday::drive::DcBlock dc;
    std::vector<float> comb;
    int combW = 0;
};
}
