#pragma once
// =====================================================================
//  PROPHECY OF SCREAM v3  -  hardstyle screech engine
//
//  mono / legato voice (glide, bend, SWEEP, amp env shared by both layers)
//
//   OSC 1: unison saw<->square + FM (+ NOISE) -> bite HP -> DIST -> FILTER -> FORMANT --+
//                                                                                      +-> AMP
//   OSC 2: unison saw<->square + FM (+ NOISE) -> bite HP -> DIST -> FILTER -> FORMANT --+
//          (each layer has its own pitch env, filter env, formant env and LFO; LFO > AMP gates the layer)
//
//   everything above runs 4x oversampled
//   -> OTT (3-band up/down compression) -> WIDTH (stereo spread) -> DELAY -> REVERB -> VOLUME -> soft clip
//
//   v3 additions (NOISE, FORMANT, LFO > AMP, SWEEP, OTT, WIDTH) are all neutral at their defaults,
//   so v2 presets and projects sound exactly the same.
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include <array>
#include <memory>
#include "../Shared/Source/DoomsdayDSP.h"      // v3 blocks: XorNoise, PitchSweep, Formant, Ott3, StereoSpread

struct OscParams
{
    bool  on = true;
    float levelDb = 0.f;
    int   octave = 0;
    float semi = 0.f, fineCents = 0.f;
    float wave = 0.f, detuneCents = 18.f, width = 0.6f, fm = 0.25f, fmRatio = 1.f;
    int   unison = 3;
    float penvSemis = 12.f, pdecayMs = 120.f;
    // distortion
    bool  distOn = true;
    int   dtype = 3;
    float driveDb = 24.f, biteHz = 250.f, dmix = 1.f;
    // filter
    bool  filtOn = true;
    int   ftype = 1;
    float cutoff = 3500.f, reso = 0.45f, fenvOct = 2.5f, fdecayMs = 300.f, keytrack = 0.5f;
    // lfo
    bool  lfoOn = true;
    int   lshape = 0;
    float lfoHz = 4.f, lcutOct = 0.f, lpitchSemis = 0.f;
    float lampDepth = 0.f;                       // v3: LFO > AMP (gate / tremolo)
    // v3: noise mixed in before the distortion (the dense "dirt" under a screech)
    float noise = 0.f;
    // v3: formant - a resonant peak after the filter, with its own envelope
    bool  formOn = false;
    float formHz = 900.f, formGainDb = 12.f, formQ = 4.f, formEnvOct = 0.f, formDecayMs = 300.f, formKey = 0.f, formLfoOct = 0.f;
};

struct EngineParams
{
    std::array<OscParams, 2> osc;
    // voice
    float glideMs = 60.f;
    int   bendRange = 12;
    bool  legato = true;
    float scream = 0.3f;
    float sweepSemis = 0.f, sweepMs = 600.f;     // v3: pitch sweep after every new note (dives / risers)
    // amp
    float attackMs = 2.f, decayMs = 400.f, sustain = 1.f, releaseMs = 150.f;
    // fx
    bool  delayOn = true;
    float delayBeats = 0.75f, delayFb = 0.35f, delayMix = 0.15f;
    bool  pingpong = true;
    bool  reverbOn = true;
    float revSize = 0.75f, revDamp = 0.4f, revMix = 0.2f;
    // master
    float ott = 0.f, spread = 0.f;               // v3
    float volumeDb = -6.f;
    double bpm = 150.0;
};

class ScreechEngine
{
public:
    static constexpr int maxUnison = 7;
    static constexpr int osFactorLog2 = 2;          // 4x oversampling
    static constexpr int osFactor = 1 << osFactorLog2;

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        osr = sampleRate * osFactor;
        maxBlockSize = juce::jmax (16, maxBlock);

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            2, osFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampler->initProcessing ((size_t) maxBlockSize);

        for (size_t o = 0; o < layers.size(); ++o)
            layers[o].prepare (osr, p.osc[o]);

        sVolume.reset (sr, 0.03);    sVolume.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.volumeDb));
        sDelayMix.reset (sr, 0.03);  sDelayMix.setCurrentAndTargetValue (p.delayOn ? p.delayMix : 0.f);
        sRevMix.reset (sr, 0.03);    sRevMix.setCurrentAndTargetValue (p.reverbOn ? p.revMix : 0.f);
        sDelayTime.reset (sr, 0.08); sDelayTime.setCurrentAndTargetValue ((float) (sr * 0.25));

        delaySize = (int) (sr * 8.5) + 4;
        delayL.assign ((size_t) delaySize, 0.f);
        delayR.assign ((size_t) delaySize, 0.f);
        delayWrite = 0;

        ott.prepare (sr);
        spread.prepare (sr);

        revScratchL.assign ((size_t) maxBlockSize, 0.f);
        revScratchR.assign ((size_t) maxBlockSize, 0.f);
        reverb.setSampleRate (sr);
        reverb.reset();

        reset();
    }

    void reset()
    {
        heldNotes.clear();
        gate = false;
        amp.stage = Env::Idle; amp.value = 0.f;
        for (auto& l : layers) l.resetState();
        for (auto& v : fbLp) v = 0.f;
        for (auto& v : fbHp) v = 0.f;
        for (auto& v : revHp) v = 0.f;
        ott.reset();
        spread.reset();
        sweep.reset();
        std::fill (delayL.begin(), delayL.end(), 0.f);
        std::fill (delayR.begin(), delayR.end(), 0.f);
        reverb.reset();
        if (oversampler) oversampler->reset();
        pitchWheel = 0.f; modWheel = 0.f;
    }

    void setParams (const EngineParams& np) { p = np; }

    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
    {
        const int numSamples = buffer.getNumSamples();
        updateBlockTargets();

        int pos = 0;
        for (const auto meta : midi)
        {
            const int evPos = juce::jlimit (0, numSamples, meta.samplePosition);
            if (evPos > pos) { renderChunked (buffer, pos, evPos - pos); pos = evPos; }
            handleMidi (meta.getMessage());
        }
        if (pos < numSamples) renderChunked (buffer, pos, numSamples - pos);
    }

private:
    // ------------------------------------------------------------ helpers
    static inline float polyBlep (float t, float dt)
    {
        if (t < dt)       { t /= dt;            return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }
    static inline float wrap01 (float x) { return x - std::floor (x); }
    static inline float timeCoef (float ms, double rate)
    {
        const double s = juce::jmax (0.0005, (double) ms * 0.001);
        return (float) std::exp (std::log (0.001) / (s * rate));
    }
    static inline float distort (float x, int type)
    {
        switch (type)
        {
            case 0:  return juce::jlimit (-1.f, 1.f, x);                                  // HARD
            case 1:  return (std::tanh (x + 0.35f) - 0.33638f) * 0.75f;                   // TUBE (asym)
            case 2:  return std::sin (x * 1.5707963f);                                    // FOLD
            default:                                                                      // SCREAM
            {
                float y = std::tanh (x * 1.4f);
                y += 0.55f * (std::abs (y) - 0.5f);
                return juce::jlimit (-0.92f, 0.92f, y * 1.3f);
            }
        }
    }
    static inline float softClip (float x)
    {
        const float a = std::abs (x);
        if (a < 0.8f) return x;
        const float s = x > 0.f ? 1.f : -1.f;
        return s * (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f));
    }

    // ------------------------------------------------------------ envelope
    struct Env
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
        float value = 0.f, attackInc = 0.f, decCoef = 0.f, relCoef = 0.f, sustain = 1.f;

        void trigger (bool fromZero) { if (fromZero) value = 0.f; stage = Attack; }
        void release() { if (stage != Idle) stage = Release; }
        float next()
        {
            switch (stage)
            {
                case Attack:  value += attackInc; if (value >= 1.f) { value = 1.f; stage = Decay; } break;
                case Decay:   value = sustain + (value - sustain) * decCoef;
                              if (std::abs (value - sustain) < 1.0e-4f) { value = sustain; stage = Sustain; } break;
                case Sustain: value = sustain; break;
                case Release: value *= relCoef; if (value < 1.0e-5f) { value = 0.f; stage = Idle; } break;
                case Idle:    value = 0.f; break;
            }
            return value;
        }
    };

    // ------------------------------------------------------------ TPT state variable filter
    struct Svf
    {
        float ic1 = 0.f, ic2 = 0.f;
        inline void tick (float v0, float a1, float a2, float a3, float& lp, float& bp)
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.f * v1 - ic1;
            ic2 = 2.f * v2 - ic2;
            bp = v1; lp = v2;
        }
    };
    struct ChannelFilter { Svf a, b; };

    // ------------------------------------------------------------ one oscillator layer
    struct Layer
    {
        // runtime state
        std::array<float, maxUnison> carrierPhase { 0.f, 0.31f, 0.67f, 0.13f, 0.89f, 0.45f, 0.58f };
        std::array<float, maxUnison> modPhase {};
        std::array<float, maxUnison> uniRatio {}, uniL {}, uniR {};
        int uniCount = 1;
        float uniNorm = 1.f;
        float pEnv = 0.f, pEnvCoef = 0.f, fEnv = 0.f, fEnvCoef = 0.f;
        float lfoPhase = 0.f, shValue = 0.f;
        float formEnv = 0.f, formEnvCoef = 0.f;
        doomsday::dsp::XorNoise noiseGen;
        std::array<float, 2> biteState {};
        std::array<ChannelFilter, 2> filt {};
        doomsday::dsp::Formant formant;
        juce::Random rng;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> sCutoff, sBite;
        juce::SmoothedValue<float> sReso, sDrive, sDMix, sFm, sWave, sGain, sDistAmt, sFiltAmt, sLfoAmt, sNoise, sFormAmt, sFormGain;

        void prepare (double osr, const OscParams& op)
        {
            const double t = 0.015;
            sCutoff.reset (osr, t);  sCutoff.setCurrentAndTargetValue (op.cutoff);
            sBite.reset (osr, t);    sBite.setCurrentAndTargetValue (op.biteHz);
            sReso.reset (osr, t);    sReso.setCurrentAndTargetValue (op.reso);
            sDrive.reset (osr, t);   sDrive.setCurrentAndTargetValue (1.f);
            sDMix.reset (osr, t);    sDMix.setCurrentAndTargetValue (op.dmix);
            sFm.reset (osr, t);      sFm.setCurrentAndTargetValue (op.fm);
            sWave.reset (osr, t);    sWave.setCurrentAndTargetValue (op.wave);
            sGain.reset (osr, 0.02); sGain.setCurrentAndTargetValue (op.on ? juce::Decibels::decibelsToGain (op.levelDb) : 0.f);
            sDistAmt.reset (osr, 0.02); sDistAmt.setCurrentAndTargetValue (op.distOn ? 1.f : 0.f);
            sFiltAmt.reset (osr, 0.02); sFiltAmt.setCurrentAndTargetValue (op.filtOn ? 1.f : 0.f);
            sLfoAmt.reset (osr, 0.02);  sLfoAmt.setCurrentAndTargetValue (op.lfoOn ? 1.f : 0.f);
            sNoise.reset (osr, t);      sNoise.setCurrentAndTargetValue (op.noise);
            sFormAmt.reset (osr, 0.02); sFormAmt.setCurrentAndTargetValue (op.formOn ? 1.f : 0.f);
            sFormGain.reset (osr, t);   sFormGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (op.formGainDb));
        }

        void resetState()
        {
            pEnv = 0.f; fEnv = 0.f; formEnv = 0.f;
            for (auto& c : filt) c = {};
            formant.reset();
            for (auto& h : biteState) h = 0.f;
        }

        void noteStart()
        {
            fEnv = 1.f;
            pEnv = 1.f;
            formEnv = 1.f;
            lfoPhase = 0.f;
            shValue = rng.nextFloat() * 2.f - 1.f;
        }

        bool silent() const { return sGain.getCurrentValue() <= 0.f && sGain.getTargetValue() <= 0.f; }
    };

    // ------------------------------------------------------------ MIDI
    void handleMidi (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber();
            const bool wasHeld = ! heldNotes.empty();
            heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), n), heldNotes.end());
            heldNotes.push_back (n);
            startNote (n, ! (wasHeld && p.legato));
        }
        else if (m.isNoteOff())
        {
            const int n = m.getNoteNumber();
            const bool wasCurrent = ! heldNotes.empty() && heldNotes.back() == n;
            heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), n), heldNotes.end());
            if (heldNotes.empty())   { gate = false; amp.release(); }
            else if (wasCurrent)     startNote (heldNotes.back(), ! p.legato);
        }
        else if (m.isPitchWheel())
            pitchWheel = juce::jlimit (-1.f, 1.f, (m.getPitchWheelValue() - 8192) / 8192.f);
        else if (m.isController() && m.getControllerNumber() == 1)
            modWheel = m.getControllerValue() / 127.f;
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            heldNotes.clear(); gate = false; amp.release();
        }
    }

    void startNote (int note, bool retrigger)
    {
        const bool sounding = amp.stage != Env::Idle;
        targetSemi = (float) note;
        if (p.glideMs < 1.f || ! sounding) curSemi = targetSemi;

        if (retrigger || ! gate)
        {
            amp.trigger (! sounding);
            for (auto& l : layers) l.noteStart();
            sweep.start();
        }
        gate = true;
    }

    // ------------------------------------------------------------ per-block setup
    void updateBlockTargets()
    {
        const float scr = juce::jlimit (0.f, 1.f, p.scream + modWheel);

        for (size_t o = 0; o < layers.size(); ++o)
        {
            auto& L = layers[o];
            const auto& op = p.osc[o];
            L.sCutoff.setTargetValue (op.cutoff);
            L.sReso.setTargetValue (juce::jlimit (0.f, 1.f, op.reso + scr * 0.22f));
            L.sDrive.setTargetValue (juce::Decibels::decibelsToGain (op.driveDb + scr * 12.f));
            L.sDMix.setTargetValue (op.dmix);
            L.sFm.setTargetValue (op.fm + scr * 0.35f);
            L.sWave.setTargetValue (op.wave);
            L.sBite.setTargetValue (op.biteHz);
            L.sGain.setTargetValue (op.on ? juce::Decibels::decibelsToGain (op.levelDb, -60.f) : 0.f);
            L.sDistAmt.setTargetValue (op.distOn ? 1.f : 0.f);
            L.sFiltAmt.setTargetValue (op.filtOn ? 1.f : 0.f);
            L.sLfoAmt.setTargetValue (op.lfoOn ? 1.f : 0.f);
            L.sNoise.setTargetValue (op.noise);
            L.sFormAmt.setTargetValue (op.formOn ? 1.f : 0.f);
            L.sFormGain.setTargetValue (juce::Decibels::decibelsToGain (op.formGainDb));
            if (amp.stage == Env::Idle)
            {
                // nothing is sounding: switch instantly, no ramp to leak into the next note
                L.sGain.setCurrentAndTargetValue (L.sGain.getTargetValue());
                L.sDistAmt.setCurrentAndTargetValue (L.sDistAmt.getTargetValue());
                L.sFiltAmt.setCurrentAndTargetValue (L.sFiltAmt.getTargetValue());
                L.sLfoAmt.setCurrentAndTargetValue (L.sLfoAmt.getTargetValue());
                L.sFormAmt.setCurrentAndTargetValue (L.sFormAmt.getTargetValue());
            }

            L.fEnvCoef = timeCoef (op.fdecayMs, osr);
            L.pEnvCoef = timeCoef (op.pdecayMs, osr);
            L.formEnvCoef = timeCoef (op.formDecayMs, osr);

            const int n = juce::jlimit (1, maxUnison, op.unison);
            for (int v = 0; v < n; ++v)
            {
                const float off = n == 1 ? 0.f : -1.f + 2.f * (float) v / (float) (n - 1);
                L.uniRatio[(size_t) v] = std::pow (2.f, off * op.detuneCents / 1200.f);
                const float pan = off * op.width;
                L.uniL[(size_t) v] = std::sqrt (0.5f * (1.f - pan));
                L.uniR[(size_t) v] = std::sqrt (0.5f * (1.f + pan));
            }
            L.uniCount = n;
            L.uniNorm = 1.f / std::sqrt ((float) n) * 1.41f;
        }

        sVolume.setTargetValue (juce::Decibels::decibelsToGain (p.volumeDb));
        sDelayMix.setTargetValue (p.delayOn ? p.delayMix : 0.f);
        sRevMix.setTargetValue (p.reverbOn ? p.revMix : 0.f);

        const double bpm = juce::jlimit (20.0, 400.0, p.bpm);
        sDelayTime.setTargetValue ((float) juce::jlimit (16.0, (double) delaySize - 4.0, p.delayBeats * 60.0 / bpm * sr));

        amp.attackInc = (float) (1.0 / (juce::jmax (0.05, (double) p.attackMs) * 0.001 * osr));
        amp.decCoef   = timeCoef (p.decayMs, osr);
        amp.relCoef   = timeCoef (p.releaseMs, osr);
        amp.sustain   = p.sustain;

        sweep.setTime (p.sweepMs, osr);

        glideCoef = p.glideMs < 1.f ? 0.f : (float) std::exp (-1.0 / ((p.glideMs * 0.001 / 3.0) * osr));

        juce::Reverb::Parameters rp;
        rp.roomSize = p.revSize; rp.damping = p.revDamp;
        rp.wetLevel = 0.33f; rp.dryLevel = 0.f; rp.width = 1.f; rp.freezeMode = 0.f;
        reverb.setParameters (rp);
    }

    void renderChunked (juce::AudioBuffer<float>& buffer, int start, int len)
    {
        while (len > 0)
        {
            const int n = juce::jmin (len, maxBlockSize);
            renderSegment (buffer, start, n);
            start += n; len -= n;
        }
    }

    // one oversampled sample of one layer (stereo, before the amp env)
    inline void renderLayer (Layer& L, const OscParams& op, float invOsr, float nyq, float& outL, float& outR)
    {
        // modulators
        L.pEnv *= L.pEnvCoef;
        L.fEnv *= L.fEnvCoef;
        L.formEnv *= L.formEnvCoef;

        const float lfoInc = op.lfoHz * invOsr;
        L.lfoPhase += lfoInc;
        if (L.lfoPhase >= 1.f) { L.lfoPhase -= std::floor (L.lfoPhase); L.shValue = L.rng.nextFloat() * 2.f - 1.f; }
        float lfo;
        switch (op.lshape)
        {
            case 0:  lfo = std::sin (L.lfoPhase * juce::MathConstants<float>::twoPi); break;
            case 1:  lfo = 1.f - 4.f * std::abs (L.lfoPhase - 0.5f); break;
            case 2:  lfo = 1.f - 2.f * L.lfoPhase; break;
            case 3:  lfo = L.lfoPhase < 0.5f ? 1.f : -1.f; break;
            default: lfo = L.shValue; break;
        }
        lfo *= L.sLfoAmt.getNextValue();

        const float cutoffBase = L.sCutoff.getNextValue();
        const float reso  = L.sReso.getNextValue();
        const float drive = L.sDrive.getNextValue();
        const float distAmt = L.sDistAmt.getNextValue();
        const float dmix  = L.sDMix.getNextValue() * distAmt;
        const float fmIdx = L.sFm.getNextValue() * 6.f;
        const float wave  = L.sWave.getNextValue();
        const float bite  = L.sBite.getNextValue();
        const float gain  = L.sGain.getNextValue();
        const float filtAmt = L.sFiltAmt.getNextValue();

        const float tune = 12.f * (float) op.octave + op.semi + op.fineCents * 0.01f;
        const float semiNow = curSemi + tune + pitchWheel * (float) p.bendRange
                            + op.penvSemis * L.pEnv + op.lpitchSemis * lfo + sweepNow;
        const float baseHz = 440.f * std::pow (2.f, (semiNow - 69.f) / 12.f);

        // unison oscillators
        float l = 0.f, r = 0.f;
        for (int v = 0; v < L.uniCount; ++v)
        {
            const float hz = juce::jmin (baseHz * L.uniRatio[(size_t) v], nyq);
            const float dt = hz * invOsr;
            float& ph = L.carrierPhase[(size_t) v];
            float& mp = L.modPhase[(size_t) v];
            ph += dt; if (ph >= 1.f) ph -= 1.f;
            mp += dt * op.fmRatio; mp -= std::floor (mp);

            const float pm = fmIdx * std::sin (mp * juce::MathConstants<float>::twoPi) * 0.15915494f;
            const float t = wrap01 (ph + pm);
            const float dte = juce::jlimit (1.0e-5f, 0.5f, dt * (1.f + std::abs (fmIdx) * op.fmRatio));

            const float saw = 2.f * t - 1.f - polyBlep (t, dte);
            float sq = t < 0.5f ? 1.f : -1.f;
            sq += polyBlep (t, dte) - polyBlep (wrap01 (t + 0.5f), dte);

            const float o = saw + wave * (sq - saw);
            l += o * L.uniL[(size_t) v];
            r += o * L.uniR[(size_t) v];
        }
        l *= L.uniNorm * 0.5f;
        r *= L.uniNorm * 0.5f;

        // noise (independent per side -> wide, dense dirt once it hits the distortion)
        const float nz = L.sNoise.getNextValue();
        if (nz > 0.f)
        {
            l += nz * 1.3f * L.noiseGen.next();
            r += nz * 1.3f * L.noiseGen.next();
        }

        // bite (pre-distortion high-pass) + distortion
        if (distAmt > 0.f)
        {
            const float hpC = 1.f - std::exp (-juce::MathConstants<float>::twoPi * bite * invOsr);
            L.biteState[0] += hpC * (l - L.biteState[0]);
            L.biteState[1] += hpC * (r - L.biteState[1]);
            // the bite HP belongs to the distortion section: it fades in/out with the section
            const float bl = l - L.biteState[0] * distAmt;
            const float br = r - L.biteState[1] * distAmt;
            const float dl = distort (bl * drive, op.dtype);
            const float dr = distort (br * drive, op.dtype);
            l = bl + dmix * (dl - bl);
            r = br + dmix * (dr - br);
        }

        // filter
        if (filtAmt > 0.f)
        {
            const float modOct = op.fenvOct * L.fEnv + op.lcutOct * lfo + op.keytrack * (curSemi - 60.f) / 12.f;
            const float fc = juce::jlimit (20.f, nyq * 0.98f, cutoffBase * std::pow (2.f, modOct));
            const float g = std::tan (juce::MathConstants<float>::pi * fc * invOsr);
            const float k = 2.f - 1.96f * reso;
            const float a1 = 1.f / (1.f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;
            const float fl = filterSample (L.filt[0], op.ftype, l, a1, a2, a3, k);
            const float fr = filterSample (L.filt[1], op.ftype, r, a1, a2, a3, k);
            l += filtAmt * (fl - l);
            r += filtAmt * (fr - r);
        }

        // formant: a resonant bell after the filter (envelope + LFO + key tracking move it)
        const float formAmt = L.sFormAmt.getNextValue();
        const float formGain = L.sFormGain.getNextValue();
        if (formAmt > 0.f)
        {
            // KEY follows the pitch you actually hear (note + octave + pitch env + LFO + sweep + bend)
            const float modOct = op.formEnvOct * L.formEnv + op.formLfoOct * lfo + op.formKey * (semiNow - 60.f) / 12.f;
            const float fc = juce::jlimit (30.f, nyq * 0.9f, op.formHz * std::pow (2.f, modOct));
            L.formant.process (l, r, fc, op.formQ, formGain, formAmt, osr);
        }

        // LFO > AMP: gate / tremolo (square LFO = rhythmic chop)
        const float ampMod = 1.f - op.lampDepth * 0.5f * (1.f - lfo);

        outL += l * gain * ampMod;
        outR += r * gain * ampMod;
    }

    static inline float filterSample (ChannelFilter& f, int ftype, float x, float a1, float a2, float a3, float k)
    {
        float lp, bp;
        f.a.tick (x, a1, a2, a3, lp, bp);
        switch (ftype)
        {
            case 0: return lp;                                                            // LP 12
            case 1: { float lp2, bp2; f.b.tick (lp, a1, a2, a3, lp2, bp2); return lp2; } // LP 24
            case 2: return bp * (0.6f + k * 0.5f);                                        // BAND
            default: return x - k * bp - lp;                                              // HIGH
        }
    }

    // ------------------------------------------------------------ the actual sound
    void renderSegment (juce::AudioBuffer<float>& buffer, int start, int len)
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) start, (size_t) len);
        block.clear();

        auto os = oversampler->processSamplesUp (block);
        float* oL = os.getChannelPointer (0);
        float* oR = os.getChannelPointer (1);
        const int osLen = (int) os.getNumSamples();

        const float invOsr = (float) (1.0 / osr);
        const float nyq = (float) (osr * 0.45);

        for (int i = 0; i < osLen; ++i)
        {
            curSemi = targetSemi + (curSemi - targetSemi) * glideCoef;
            sweepNow = sweep.next (p.sweepSemis);          // SWEEP: eases out from the note to note + sweep semitones
            const float ampV = amp.next();

            if (ampV <= 0.f && amp.stage == Env::Idle)
            {
                // keep the layer smoothers moving so on/off ramps finish while silent
                for (auto& L : layers) { L.sGain.skip (1); L.sDistAmt.skip (1); L.sFiltAmt.skip (1); L.sLfoAmt.skip (1); }
                oL[i] = 0.f; oR[i] = 0.f;
                continue;
            }

            float l = 0.f, r = 0.f;
            for (size_t o = 0; o < layers.size(); ++o)
            {
                auto& L = layers[o];
                if (L.silent()) continue;
                renderLayer (L, p.osc[o], invOsr, nyq, l, r);
            }
            oL[i] = l * ampV;
            oR[i] = r * ampV;
        }

        oversampler->processSamplesDown (block);

        // --- base-rate FX: delay -> reverb -> master
        float* L = buffer.getWritePointer (0, start);
        float* R = buffer.getWritePointer (1, start);

        const float fbLpC = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 6500.f / (float) sr);
        const float fbHpC = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 180.f / (float) sr);
        const float rvHpC = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 220.f / (float) sr);
        const float fb = p.delayOn ? p.delayFb : 0.f;

        for (int i = 0; i < len; ++i)
        {
            float l = L[i], r = R[i];

            ott.process (l, r, p.ott);          // OTT: 3 bands, each pulled towards the same level
            spread.process (l, r, p.spread);    // WIDTH: more side + decorrelated mid above 220 Hz

            const float dTime = sDelayTime.getNextValue();
            const float dMix = sDelayMix.getNextValue();
            float rp = (float) delayWrite - dTime;
            if (rp < 0.f) rp += (float) delaySize;
            const int i0 = (int) rp;
            const int i1 = (i0 + 1) % delaySize;
            const float fr = rp - (float) i0;
            float dl = delayL[(size_t) i0] + fr * (delayL[(size_t) i1] - delayL[(size_t) i0]);
            float dr = delayR[(size_t) i0] + fr * (delayR[(size_t) i1] - delayR[(size_t) i0]);

            fbLp[0] += fbLpC * (dl - fbLp[0]); dl = fbLp[0];
            fbLp[1] += fbLpC * (dr - fbLp[1]); dr = fbLp[1];
            fbHp[0] += fbHpC * (dl - fbHp[0]); dl -= fbHp[0];
            fbHp[1] += fbHpC * (dr - fbHp[1]); dr -= fbHp[1];

            if (p.pingpong)
            {
                delayL[(size_t) delayWrite] = 0.5f * (l + r) + dr * fb;
                delayR[(size_t) delayWrite] = dl * fb;
            }
            else
            {
                delayL[(size_t) delayWrite] = l + dl * fb;
                delayR[(size_t) delayWrite] = r + dr * fb;
            }
            if (++delayWrite >= delaySize) delayWrite = 0;

            l += dl * dMix;
            r += dr * dMix;

            revHp[0] += rvHpC * (l - revHp[0]);
            revHp[1] += rvHpC * (r - revHp[1]);
            revScratchL[(size_t) i] = l - revHp[0];
            revScratchR[(size_t) i] = r - revHp[1];

            L[i] = l; R[i] = r;
        }

        reverb.processStereo (revScratchL.data(), revScratchR.data(), len);

        for (int i = 0; i < len; ++i)
        {
            const float rm = sRevMix.getNextValue();
            const float vol = sVolume.getNextValue();
            const float dryG = 1.f - 0.35f * rm;
            L[i] = softClip ((L[i] * dryG + revScratchL[(size_t) i] * rm) * vol);
            R[i] = softClip ((R[i] * dryG + revScratchR[(size_t) i] * rm) * vol);
        }
    }

    // ------------------------------------------------------------ state
    EngineParams p;
    double sr = 44100.0, osr = 176400.0;
    int maxBlockSize = 512;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    std::vector<int> heldNotes;
    bool gate = false;
    float curSemi = 60.f, targetSemi = 60.f, glideCoef = 0.f;
    float pitchWheel = 0.f, modWheel = 0.f;
    Env amp;

    std::array<Layer, 2> layers;

    juce::SmoothedValue<float> sVolume, sDelayMix, sRevMix, sDelayTime;
    std::vector<float> delayL, delayR;
    int delaySize = 1, delayWrite = 0;
    std::array<float, 2> fbLp {}, fbHp {}, revHp {};
    float sweepNow = 0.f;
    doomsday::dsp::PitchSweep sweep;
    doomsday::dsp::Ott3 ott;
    doomsday::dsp::StereoSpread spread;
    juce::Reverb reverb;
    std::vector<float> revScratchL, revScratchR;
};
