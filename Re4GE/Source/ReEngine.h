#pragma once
// =====================================================================
//  RE4GE - reverse bass instrument (split out of 4GE, owner 2026-10-23; architecture: Shared/RESEARCH.md "Reverse bass")
//
//  TRIGGER  PATTERN : a note starts BEATS beats of steps (1/4, 1/8, 1/16) on the host tempo, MASK picks the steps,
//                     OFFSET shifts them (write the kick rhythm, the bass plays between the kicks)
//           GATE    : a note is one bass hit, note-off starts the FADE (a bass line in the piano roll)
//  SOURCE   SYNTH   : SAW <-> PULSE (BLEND, WIDTH) + SINE, PHASE WARP (BEND / SQUEEZE / SYNC / MIRROR, AMOUNT, ENV), phase reset per step
//           TAIL    : a kick tail (4GE's GeKick voice, no punch / click) rendered on the note and played BACKWARDS so its start
//                     (the highest pitch, the dirtiest part) lands on the end of the gate - the classic reverse kick bass
//           SAMPLE  : the user's WAV, backwards (ends on the gate) or forwards, ROOT + key tracking
//  PITCH    STABLE / DROP / RISE / BOUNCE (AMOUNT, TIME) + TUNE / FINE, KEY TRACK (off = always on NOTE)
//  ENVELOPE one shape for every style: DELAY (DUCK level before it) -> INITIAL -> SWELL ms + CURVE -> HOLD -> DECAY to SUSTAIN
//           (DECAY CURVE) -> GATE end with FADE. TIMING SYNC = delay / hold / decay / gate in % of a step, MS = in ms.
//           PUMP / SWELL / TIGHT / ROLL / PULSE are settings of it (templates).
//  TONE     envelope -> PRE FILTER -> DRIVE (4GE stage: type, BIAS, MIX, auto gain; DRIVE ENV = dirtier at the peak)
//           -> LOW CUT / HIGH CUT / BELL -> SUB (clean sine below SUB Hz, LR4, never cut) -> CLIP -> LEVEL
//  Rendered 4x oversampled (the oversampler and every buffer are made once, in the constructor). Mono.
//  The owner's rule: the low end is NOT cut by default (LOW CUT off, SUB on).
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "GeKick.h"       // (Shared) ge::stage / ge::filt / ge::Voice: the same distortion, filters and tail as 4GE
#include <atomic>

namespace re
{
    enum Source { SrcSynth = 0, SrcTail = 1, SrcSample = 2 };
    enum PitchMode { Stable = 0, Drop = 1, Rise = 2, Bounce = 3 };
    enum Warp { WarpOff = 0, Bend = 1, Squeeze = 2, Sync = 3, Mirror = 4 };

    struct Params
    {
        // trigger / timing
        bool gate = false, sync = true;
        double stepBeats = 1.0;                 // 1 = 1/4, 0.5 = 1/8, 0.25 = 1/16
        int mask = 15, beats = 1;               // MASK bit i = step i of each beat plays
        float offsetPct = 0.f;                  // shifts the steps (0..50 % of a step)
        // envelope
        float delayPct = 36.f, holdPct = 100.f, decayPct = 0.f, gatePct = 98.f;
        float delayMs = 120.f, holdMs = 400.f, decayMs = 0.f, gateMs = 380.f;
        float duck = 0.f, initial = 0.f, swellMs = 60.f, curve = 0.6f, sustain = 1.f, decayCurve = 1.5f, fadeMs = 25.f;
        // source
        int source = SrcTail;
        float blend = 0.3f, width = 0.5f, sineDb = -6.f;
        int warp = WarpOff; float warpAmt = 0.f, warpEnv = 0.f;
        ge::Params tail;                        // the kick tail (only the tail / distortion part is used)
        float tailLenMs = 300.f;                // GATE mode: how much tail is played backwards
        bool sampleReverse = true; int root = 31; float sampleStartMs = 0.f;
        // pitch
        int pitchMode = Stable; float pitchSt = 12.f, pitchMs = 60.f, tuneSt = 0.f, fineCt = 0.f;
        bool keyTrack = true; int fixedNote = 29;
        // tone
        int preType = 0; float preHz = 800.f, preReso = 0.2f;
        int distType = 0; float driveDb = 10.f, driveEnv = 0.f, bias = 0.f, mix = 1.f;
        float lowCutHz = 20.f, highCutHz = 900.f, bellHz = 700.f, bellDb = 0.f, bellQ = 1.f;
        float subHz = 90.f, subDb = 0.f;        // SUB: below subHz the bass is a clean sine (0 = off)
        float clip = 0.2f, levelDb = -3.f, velocitySens = 0.f;
    };

    // ---- the envelope (also drawn by the GUI): t ms from the step start, stepMs = length of one step, gateEnd in ms
    struct Times { double delay, hold, decay, gateEnd; };
    inline Times times (const Params& p, double stepMs)
    {
        Times t;
        if (p.sync) { t.delay = p.delayPct * 0.01 * stepMs; t.hold = p.holdPct * 0.01 * stepMs; t.decay = p.decayPct * 0.01 * stepMs; t.gateEnd = p.gatePct * 0.01 * stepMs; }
        else        { t.delay = p.delayMs; t.hold = p.holdMs; t.decay = p.decayMs; t.gateEnd = p.gateMs; }
        if (! p.gate) t.gateEnd = juce::jmin (t.gateEnd, stepMs - 0.5);
        return t;
    }
    inline double swellShape (double x, float curve)
    {
        x = juce::jlimit (0.0, 1.0, x);
        if (curve > 0.001f) return 1.0 - std::pow (1.0 - x, 1.0 + 4.0 * curve);      // fast, then landing (sidechain release)
        if (curve < -0.001f) return std::pow (x, 1.0 - 4.0 * curve);                  // slow start, late swell
        return x;
    }
    // level 0..1 before the gate fade
    inline double envAt (const Params& p, const Times& tm, double t)
    {
        if (t < tm.delay) return p.duck;
        const double u = t - tm.delay;
        if (u < p.swellMs) return p.initial + (1.0 - p.initial) * swellShape (u / juce::jmax (0.5f, p.swellMs), p.curve);
        const double v = u - p.swellMs - tm.hold;
        if (v < 0.0 || tm.decay < 0.5) return 1.0;
        const double d = juce::jlimit (0.0, 1.0, v / tm.decay);
        return p.sustain + (1.0 - p.sustain) * std::pow (1.0 - d, (double) p.decayCurve);
    }
    inline double gateAt (const Params& p, const Times& tm, double t)       // PATTERN: cosine fade that ends exactly on gateEnd
    {
        const double f = juce::jlimit (0.0, 1.0, (tm.gateEnd - t) / juce::jmax (0.5f, p.fadeMs));
        return 0.5 - 0.5 * std::cos (f * juce::MathConstants<double>::pi);
    }
    inline double pitchAt (const Params& p, const Times& tm, double t)      // semitones over the note
    {
        const double T = juce::jmax (1.f, p.pitchMs);
        double st = 0.0;
        if (p.pitchMode == Drop || p.pitchMode == Bounce) st += p.pitchSt * std::exp (-3.0 * juce::jmax (0.0, t - tm.delay) / T);
        if (p.pitchMode == Rise || p.pitchMode == Bounce)
        {
            const double startAt = p.gate ? tm.delay : tm.gateEnd - T;
            const double r = juce::jlimit (0.0, 1.0, (t - startAt) / T);
            st += (p.pitchMode == Bounce ? 0.5 : 1.0) * p.pitchSt * r * r;
        }
        return st;
    }
    // PHASE WARP: bends the phase of the oscillator (0..1) -> other harmonics than drive, the 'springy' reverse bass
    inline double warp (double ph, int mode, double a)
    {
        a = juce::jlimit (0.0, 0.98, a);
        switch (mode)
        {
            case Bend:      { const double d = 0.5 * (1.0 - a); return ph < d ? 0.5 * ph / d : 0.5 + 0.5 * (ph - d) / (1.0 - d); }
            case Squeeze:   { const double half = ph < 0.5 ? 0.0 : 0.5, q = (ph - half) * 2.0, d = 1.0 - a * 0.9; return half + 0.5 * juce::jmin (1.0, q / d); }
            case Sync:      { const double r = 1.0 + a * 7.0, x = ph * r; return x - std::floor (x); }
            case Mirror:    { const double m = ph < 0.5 ? ph : 1.0 - ph; return ph + a * (m - ph); }
            default:        return ph;
        }
    }

    struct Sample { std::vector<float> data; double rate = 44100.0; juce::String name; };

    class Engine
    {
    public:
        static constexpr int osLog2 = 2, osFactor = 1 << osLog2;
        Engine() : os (std::make_unique<juce::dsp::Oversampling<float>> (1, osLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false))
        {
            tailBuf.assign ((size_t) (192000 * 2.1), 0.f);
            gui.assign ((size_t) (192000 * 2.1), 0.f);
        }
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate; osr = sr * osFactor;
            maxBlockSize = juce::jmax (16, maxBlock);
            os->reset(); os->initProcessing ((size_t) maxBlockSize);
            mono.setSize (1, maxBlockSize);
            tailVoice.prepare (sr);
            tailCap = juce::jmin ((int) tailBuf.size(), (int) (sr * 2.0));
            guiCap = juce::jmin ((int) gui.size(), (int) (sr * 2.0));
            stop();
        }
        void setParams (const Params& np) { p = np; }
        void setTempo (double b) { bpm = b > 20.0 && std::isfinite (b) ? juce::jlimit (20.0, 400.0, b) : 150.0; tempoNow.store ((float) bpm); }
        void stop() { active = false; amp = 0.f; postGate = 0.f; pre = {}; hc1 = {}; hc2 = {}; lcF = {}; bell.reset(); sY1 = {}; sY2 = {}; sS1 = {}; sS2 = {}; dcx = dcy = 0.f; }

        void setSample (std::unique_ptr<Sample> s) { Sample* raw = s.get(); if (s != nullptr) keep.push_back (std::move (s)); sample.store (raw); }
        const Sample* currentSample() const { return sample.load(); }

        void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
        {
            const int n = buffer.getNumSamples();
            int pos = 0;
            for (const auto meta : midi)
            {
                const int at = juce::jlimit (0, n, meta.samplePosition);
                if (at > pos) { render (buffer, pos, at - pos); pos = at; }
                const auto m = meta.getMessage();
                if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity());
                else if (m.isNoteOff() && m.getNoteNumber() == heldNote) noteOffAt = tNote;
            }
            if (pos < n) render (buffer, pos, n - pos);
        }
        void audition (int note) { noteOn (note, 1.f); auditionOff = true; }

        std::atomic<float> tempoNow { 150.f }, outPeak { 0.f };
        std::atomic<int> hitSerial { 0 }, guiLength { 0 };
        std::vector<float> gui;                 // the output since the last note (GUI playhead / meter)

    private:
        void noteOn (int note, float velocity)
        {
            heldNote = note; noteOffAt = -1.0; auditionOff = false;
            const int playNote = p.keyTrack ? note : p.fixedNote;
            hz = 440.0 * std::pow (2.0, (playNote - 69 + p.tuneSt + p.fineCt * 0.01) / 12.0);
            vel = 1.f - p.velocitySens * (1.f - velocity);
            tNote = 0.0; step = -1; active = true;
            stepMs = p.stepBeats * 60000.0 / bpm;
            if (p.source == SrcTail) renderTail (playNote);
            playing = sample.load();
            guiWrite = 0; hitSerial.fetch_add (1);
        }
        // the kick tail on this note, long enough for one gate (PATTERN) or TAIL LENGTH (GATE)
        void renderTail (int playNote)
        {
            ge::Params t = p.tail;
            t.punchOn = false; t.clickDb = -60.f; t.headFixed = false; t.texDb = -60.f; t.mute = 0; t.duck = 0.f; t.tailAttackMs = 0.f;
            t.busType = 0; t.cleanSubHz = 0.f; t.room = 0.f;
            t.amp = ge::Shape::make ({ { 0.f, 1.f }, { 1000.f, 1.f } });
            t.drive = ge::Shape::make ({ { 0.f, 1.f }, { 1000.f, 1.f } });
            const Times tm = times (p, stepMs);
            const double lenMs = p.gate ? p.tailLenMs : juce::jmax (20.0, tm.gateEnd) + 10.0;
            tailLen = juce::jlimit (2, tailCap, (int) (lenMs * 0.001 * sr));
            t.lengthMs = (float) (tailLen * 1000.0 / sr) + 20.f;
            t.cook (sr);
            tailVoice.trigger ((float) (440.0 * std::pow (2.0, (playNote - 69 + p.tuneSt + p.fineCt * 0.01) / 12.0)), 1.f, t);
            for (int i = 0; i < tailLen; ++i) tailBuf[(size_t) i] = tailVoice.render (t);
        }
        void render (juce::AudioBuffer<float>& buffer, int start, int len)
        {
            while (len > 0) { const int n = juce::jmin (len, maxBlockSize); renderChunk (buffer, start, n); start += n; len -= n; }
        }
        // the start of a new step (or of the note in GATE mode): phases reset, the backwards read starts
        void startStep (const Times& tm)
        {
            phase = 0.0; subPh = 0.0;
            if (p.source == SrcTail) readPos = p.gate ? (double) tailLen - 2.0 : tm.gateEnd * 0.001 * sr;   // reaches the tail's start on the gate end
            else if (p.source == SrcSample && playing != nullptr)
            {
                const double sp = smpSpeed();
                readPos = p.sampleReverse ? (p.gate ? p.tailLenMs : tm.gateEnd) * 0.001 * playing->rate * sp + p.sampleStartMs * 0.001 * playing->rate
                                          : p.sampleStartMs * 0.001 * playing->rate;
            }
        }
        double smpSpeed() const { return p.keyTrack ? std::pow (2.0, ((hz > 0.0 ? 12.0 * std::log2 (hz / 440.0) + 69.0 : 69.0) - p.root) / 12.0) : 1.0; }
        // one source sample at the oversampled rate; e = envelope (warp ENV), st = pitch offset
        inline float source (double e, double st)
        {
            const double ratio = std::pow (2.0, st / 12.0);
            if (p.source == SrcSynth)
            {
                const double f = hz * ratio, dt = f / osr;
                phase += dt; if (phase >= 1.0) phase -= 1.0;
                const double a = p.warpAmt * (1.0 - p.warpEnv + p.warpEnv * e);
                const double w = p.warp != WarpOff ? warp (phase, p.warp, a) : phase;
                double saw, pulse;
                if (p.warp == WarpOff)
                {
                    saw = 2.0 * phase - 1.0 - doomsday::dsp::polyBlep ((float) phase, (float) dt);
                    const double pw = juce::jlimit (0.05f, 0.95f, p.width);
                    pulse = (phase < pw ? 1.0 : -1.0) + doomsday::dsp::polyBlep ((float) phase, (float) dt)
                          - doomsday::dsp::polyBlep (doomsday::dsp::wrap01 ((float) (phase + 1.0 - pw)), (float) dt) - (2.0 * pw - 1.0);
                }
                else
                {
                    // warped: the shapes come from a cosine of the bent phase (smooth, no hard edges to alias)
                    saw = std::sin (juce::MathConstants<double>::twoPi * w);
                    pulse = std::tanh (3.0 * std::sin (juce::MathConstants<double>::twoPi * w + (p.width - 0.5) * 2.0));
                    if (p.warp == Sync) { const double win = 1.0 - phase; saw *= win; pulse *= win; }
                }
                const double sine = std::sin (juce::MathConstants<double>::twoPi * w);
                return (float) ((saw + (pulse - saw) * p.blend) * 0.7 + sine * sineG);
            }
            const float* d = nullptr; int len = 0; double inc = 0.0;
            if (p.source == SrcTail) { d = tailBuf.data(); len = tailLen; inc = sr / osr * ratio; }
            else if (playing != nullptr) { d = playing->data.data(); len = (int) playing->data.size(); inc = playing->rate / osr * ratio * smpSpeed(); }
            if (d == nullptr || len < 2) return 0.f;
            const bool back = p.source == SrcTail || p.sampleReverse;
            if (readPos < 0.0 || readPos >= len - 1) { readPos += back ? -inc : inc; return 0.f; }
            const int i = (int) readPos; const float fr = (float) (readPos - i);
            const float y = d[i] + fr * (d[i + 1] - d[i]);
            readPos += back ? -inc : inc;
            return y;
        }
        void renderChunk (juce::AudioBuffer<float>& buffer, int start, int n)
        {
            using namespace doomsday::dsp;
            mono.clear (0, 0, n);
            juce::dsp::AudioBlock<float> blk (mono.getArrayOfWritePointers(), 1, 0, (size_t) n);
            auto up = os->processSamplesUp (blk);
            float* u = up.getChannelPointer (0);
            // coefficients once per chunk
            const auto preC = svfCoefs (p.preHz, ge::resoToK (p.preReso), osr);
            const auto lcC = svfCoefs (juce::jmax (20.f, p.lowCutHz), 1.f / 0.707f, osr);
            const auto hc1C = svfCoefs (p.highCutHz, 1.f / 0.54f, osr), hc2C = svfCoefs (p.highCutHz, 1.f / 1.31f, osr);
            const auto subC = svfCoefs (juce::jmax (20.f, p.subHz), 1.f / 0.707f, osr);
            const auto bellC = BiquadCoefs::make (BiquadCoefs::Bell, osr, p.bellHz, juce::jmax (0.2f, p.bellQ), p.bellDb);
            const float subG = p.subDb <= -59.f ? 0.f : std::pow (10.f, p.subDb / 20.f);
            sineG = p.sineDb <= -59.f ? 0.0 : std::pow (10.0, p.sineDb / 20.0);
            const float levelG = std::pow (10.f, p.levelDb / 20.f);
            const float dcR = (float) std::exp (-juce::MathConstants<double>::twoPi * 5.0 / osr);
            const float ampC = (float) (1.0 - std::exp (-1.0 / (0.0005 * osr)));        // 0.5 ms smoothing: no clicks between steps
            const double msPerSample = 1000.0 / osr;
            stepMs = p.stepBeats * 60000.0 / bpm;
            const Times tm = times (p, stepMs);
            const int stepsPerBeat = juce::jmax (1, (int) std::round (1.0 / p.stepBeats));
            const int totalSteps = stepsPerBeat * juce::jmax (1, p.beats);
            const double offMs = p.offsetPct * 0.01 * stepMs;
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                float y = 0.f;
                if (active)
                {
                    double t, target = 0.0, st = 0.0, gateNow = 0.0;
                    if (p.gate)
                    {
                        t = tNote;
                        if (step < 0) { step = 0; startStep (tm); }
                        double g = 1.0;
                        if (noteOffAt >= 0.0) g = 0.5 + 0.5 * std::cos (juce::jmin (1.0, (tNote - noteOffAt) / juce::jmax (0.5f, p.fadeMs)) * juce::MathConstants<double>::pi);
                        if (auditionOff && tNote > 400.0 && noteOffAt < 0.0) noteOffAt = tNote;
                        target = envAt (p, tm, t) * g;
                        st = pitchAt (p, tm, t);
                        gateNow = g;
                        if (noteOffAt >= 0.0 && tNote - noteOffAt > p.fadeMs + 1.0) active = false;
                    }
                    else
                    {
                        const double tt = tNote - offMs;
                        const int s = tt < 0.0 ? -1 : (int) std::floor (tt / stepMs);
                        if (s >= totalSteps) active = false;
                        else if (s >= 0)
                        {
                            if (s != step) { step = s; startStep (tm); }
                            t = tt - s * stepMs;
                            if ((p.mask >> (s % stepsPerBeat)) & 1) { gateNow = gateAt (p, tm, t); target = envAt (p, tm, t) * gateNow; st = pitchAt (p, tm, t); }
                        }
                    }
                    amp += ((float) target - amp) * ampC;
                    float x = source (amp, st) * amp;
                    // TONE: pre filter -> drive (dirtier at the peak with DRIVE ENV) -> filters
                    x = ge::filt (pre, x, preC, p.preType);
                    const float g = std::pow (10.f, p.driveDb * (1.f - p.driveEnv + p.driveEnv * amp) / 20.f);
                    x = ge::stage (x, g, p.distType, p.bias, p.mix, true);
                    const float dc = x - dcx + dcR * dcy; dcx = x; dcy = dc; x = dc;
                    if (p.lowCutHz > 20.5f) x = lcF.hp (x, lcC);
                    x = hc2.lp (hc1.lp (x, hc1C), hc2C);
                    if (std::abs (p.bellDb) > 0.05f) x = bell.process (x, bellC);
                    // SUB: a clean sine on the note (same envelope + pitch) replaces everything below SUB Hz
                    if (p.subHz > 20.f)
                    {
                        subPh += hz * std::pow (2.0, st / 12.0) / osr; if (subPh >= 1.0) subPh -= 1.0;
                        const float sine = (float) std::sin (subPh * juce::MathConstants<double>::twoPi) * amp;
                        x = sY2.hp (sY1.hp (x, subC), subC) + sS2.lp (sS1.lp (sine, subC), subC) * subG;
                    }
                    if (p.clip > 0.001f) x += p.clip * (std::tanh (x * (1.f + 4.f * p.clip)) / (1.f + 0.5f * p.clip) - x);
                    // the gate once more AFTER the filters: their ringing (the sub crossover delays the lows by a few ms) never reaches the next kick
                    postGate += ((float) gateNow - postGate) * ampC;
                    y = x * levelG * vel * postGate;
                    const float ay = std::abs (y);                                    // safety: a soft knee from -1 dB, never over 0 dBFS
                    if (ay > 0.89f) y = std::copysign (0.89f + 0.1f * std::tanh ((ay - 0.89f) / 0.1f), y);
                    tNote += msPerSample;
                }
                else { amp = 0.f; postGate = 0.f; }
                u[i] = y;
            }
            os->processSamplesDown (blk);
            const float* m = mono.getReadPointer (0);
            float pk = 0.f;
            for (int i = 0; i < n; ++i)
            {
                const float y = m[i];
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.setSample (ch, start + i, y);
                pk = juce::jmax (pk, std::abs (y));
                if (guiWrite < guiCap) gui[(size_t) guiWrite++] = y;
            }
            guiLength.store (guiWrite);
            outPeak.store (juce::jmax (outPeak.load(), pk));
        }

        Params p;
        double sr = 44100.0, osr = 176400.0, bpm = 150.0, stepMs = 400.0, hz = 49.0, tNote = 0.0, noteOffAt = -1.0, phase = 0.0, subPh = 0.0, readPos = 0.0, sineG = 0.5;
        int maxBlockSize = 512, step = -1, heldNote = -1, tailLen = 0, tailCap = 0, guiCap = 0, guiWrite = 0;
        bool active = false, auditionOff = false;
        float vel = 1.f, amp = 0.f, postGate = 0.f, dcx = 0.f, dcy = 0.f;
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        juce::AudioBuffer<float> mono;
        std::vector<float> tailBuf;
        ge::Voice tailVoice;
        doomsday::dsp::Svf pre, hc1, hc2, lcF, sY1, sY2, sS1, sS2;
        doomsday::dsp::Biquad bell;
        std::atomic<Sample*> sample { nullptr };
        const Sample* playing = nullptr;
        std::vector<std::unique_ptr<Sample>> keep;
    };
}
