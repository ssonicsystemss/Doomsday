#pragma once
// =====================================================================
//  RE4GE - the engine: puts the DSP components together (no APVTS, no GUI, no files - those live in Parameters/, GUI/, Presets/)
//
//  TRIGGER (Trigger.h) -> OSC 1: SYNTH (Oscillator.h) / KICK TAIL backwards (KickTail.h) / SAMPLE (readBuf) + OSC 2-4 (OscBank.h)
//  (MACROS (Macros.h) move the settings on top of the knobs, once per grid point)
//  -> x ENVELOPE (Envelope.h) -> TONE (ToneChain.h) per chain -> SUB on the centre (LowEnd.h) -> STEREO (LowEnd.h) -> OUTPUT
//  Three chains: centre (0), left (1), right (2); left / right only with UNISON + WIDTH.
//  Rendered 4x oversampled (the oversampler and every buffer are made once, in the constructor).
//  Internal grid of 32 samples: the settings are taken and the coefficients made only on it, so the result never depends
//  on the host's buffer size (test: buffers 1 ... 4096 and random sizes give the same samples).
//  The owner's rule: the low end is NOT cut by default (LOW CUT off, SUB on).
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "ReParams.h"
#include "Envelope.h"
#include "Trigger.h"
#include "Oscillator.h"
#include "KickTail.h"
#include "ToneChain.h"
#include "LowEnd.h"
#include "OscBank.h"
#include "Macros.h"
#include "ShapeLanes.h"
#include <atomic>

namespace re
{
    class Engine
    {
    public:
        static constexpr int grid = 32;
        // QUALITY: 2x / 4x / 8x / 16x oversampling, every factor made once here (switching never allocates);
        // EXPORT uses its own factor when the host renders offline
        Engine()
        {
            for (int i = 0; i < 4; ++i)
            {
                osArr[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (2, (size_t) (i + 1), juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
                osArr[(size_t) i]->initProcessing ((size_t) grid);
            }
            gui.assign ((size_t) (192000 * 2.1), 0.f);
        }
        void setNonRealtime (bool b) { offline = b; }
        void prepare (double sampleRate, int /*maxBlock*/)
        {
            sr = sampleRate;
            osIdx = juce::jlimit (0, 3, offline ? pending.qualityExport : pending.qualityRt);
            osr = sr * (1 << (osIdx + 1));
            for (auto& o : osArr) o->reset();
            work.setSize (2, grid);
            tail.prepare (sr);
            guiCap = juce::jmin ((int) gui.size(), (int) (sr * 2.0));
            clock = 0;
            builtCurves = -1;
            latch();
            stop();
        }
        // the settings for the next grid point (the audio thread, once per host block)
        void setParams (const Params& np) { pending = np; }
        void setTempo (double b) { bpmPending = b > 20.0 && std::isfinite (b) ? juce::jlimit (20.0, 400.0, b) : 150.0; tempoNow.store ((float) bpmPending); }
        void stop() { trig.stop(); amp = 0.f; postGate = 0.f; for (auto& c : ch) c.clear(); sub.clear(); stereo.clear(); }

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
                if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity(), false);
                else if (m.isNoteOff()) trig.noteOff (m.getNoteNumber());
            }
            if (pos < n) render (buffer, pos, n - pos);
        }
        void audition (int note) { noteOn (note, 1.f, true); }

        std::atomic<float> tempoNow { 150.f }, outPeak { 0.f };
        std::array<std::atomic<float>, 2> drivePeak {};      // the stages' input level for the live dot (the GUI takes and resets it)
        std::atomic<int> hitSerial { 0 }, guiLength { 0 };
        std::vector<float> gui;                 // the output since the last note (GUI playhead / meter)

    private:
        // ---- per grid point: the settings (+ the oversampling factor and the drawn distortion curves)
        void latch()
        {
            p = applyMacros (pending);
            const int want = juce::jlimit (0, 3, offline ? p.qualityExport : p.qualityRt);
            if (want != osIdx)
            {
                osIdx = want; osr = sr * (1 << (osIdx + 1));
                osArr[(size_t) osIdx]->reset();
                stop();                                         // a short silence: every filter changes its rate
            }
            const int curveKey = p.curveSerial * 4 + (p.lab[0].symmetric ? 1 : 0) + (p.lab[1].symmetric ? 2 : 0);
            if (curveKey != builtCurves)
            {
                builtCurves = curveKey;
                for (int s = 0; s < 2; ++s) customTab[(size_t) s].build (p.driveCurve[(size_t) s], p.lab[(size_t) s].symmetric);
                for (auto& c : ch) for (auto& sh : c.tone.shaper) sh.customChanged();
            }
            cook();
        }
        void cook()
        {
            bpm = bpmPending;
            stepMs = p.stepBeats * 60000.0 / bpm;
            trig.cook (p, stepMs);
            tone.cook (p, osr);
            for (int s = 0; s < 2; ++s) tone.table[(size_t) s] = &customTab[(size_t) s];
            for (auto& c : ch) c.tone.setTilt (p, osr);
            sub.cook (p, osr);
            stereo.cook (p, osr);
            for (int sd = 0; sd < 3; ++sd) ch[(size_t) sd].smear.cook (p, osr, sd);
            clickG = p.clickDb <= -59.f ? 0.f : std::pow (10.f, p.clickDb / 20.f);
            clickC = doomsday::dsp::svfCoefs (p.clickHz, 1.f / 0.707f, osr);
            clickDecay = (float) std::exp (-1.0 / (juce::jmax (0.5, (double) p.clickMs) * 0.001 * osr / 3.0));
            glideC = p.glideMs < 0.5f ? 1.0 : 1.0 - std::exp (-1.0 / (p.glideMs * 0.001 * osr / 3.0));
            sineG = p.sineDb <= -59.f ? 0.0 : std::pow (10.0, p.sineDb / 20.0);
            osc1G = p.osc1Db <= -59.f ? 0.f : std::pow (10.f, p.osc1Db / 20.f);
            OscBank::cook (p, osr, oscG, oscMult, oscNoise);
            oscOn = OscBank::any (p);
            lanesOn = ShapeLanes::any (p);
            tone.dynPre = tone.dynPre || (p.preType != 0 && p.lane[LaneHarm].on);
            levelG = std::pow (10.f, p.levelDb / 20.f);
            ampC = (float) (1.0 - std::exp (-1.0 / (0.0005 * osr)));        // 0.5 ms smoothing: no clicks between steps
        }
        void noteOn (int note, float velocity, bool isAudition)
        {
            if (clock % grid == 0) latch();       // a note on a grid point already gets this block's settings
            trig.noteOn (note, isAudition);
            const double prevHz = hz * glideR;
            const bool legato = trig.isActive() && amp > 1.0e-4f;
            playNote = p.keyTrack ? note : p.fixedNote;
            hz = 440.0 * std::pow (2.0, (playNote - 69 + p.tuneSt + p.fineCt * 0.01) / 12.0);
            subHzNote = Sub::noteHz (playNote, p);
            glideR = (legato && p.glideMs >= 0.5f && hz > 0.0) ? prevHz / hz : 1.0;     // GLIDE: slide from the last note's pitch
            vel = 1.f - p.velocitySens * (1.f - velocity);
            uCount = 0; preCount = 0; lastT = -1.0; clickRnd = 0x2545f491u;
            if (p.source == SrcTail) { beginTail(); if (p.gate) tail.finish(); }
            playing = sample.load();
            guiWrite = 0; hitSerial.fetch_add (1);
        }
        // the kick tail on this note, long enough for one gate (PATTERN) or TAIL LENGTH (GATE)
        double tailLenMs() const { return p.gate ? p.tailLenMs : juce::jmax (20.0, trig.stepTimes().gateEnd) + 10.0; }
        void beginTail() { tail.begin (p, hz, tailLenMs()); }

        void render (juce::AudioBuffer<float>& buffer, int start, int len)
        {
            while (len > 0)
            {
                if (clock % grid == 0) latch();
                const int n = juce::jmin (len, grid - (int) (clock % grid));
                renderChunk (buffer, start, n);
                start += n; len -= n; clock += (uint64_t) n;
            }
        }
        // one chain = source -> tone; three of them: centre (0), left (1), right (2)
        struct Chain
        {
            double phase = 0.0, readPos = 0.0;
            std::array<double, 6> uph {}, upos {};
            ToneChain tone;
            OscBank::State ob;
            Smear smear;
            void clear() { phase = 0.0; readPos = 0.0; uph = {}; upos = {}; tone.clear(); ob = {}; smear.clear(); }
        };
        // the start of a new step (or of the note in GATE mode): phases reset, the backwards read starts
        void startStep()
        {
            const Times& tm = trig.stepTimes();
            if (p.source == SrcTail && ! p.gate) { beginTail(); tail.pace (trig.msUntilAudible (p) * 0.001 * sr, grid); }   // live knobs: re-render if they changed
            const bool lock = ! p.phaseFree;              // PHASE RESET (lock): every step starts the same; FREE: the phases run on
            if (lock) sub.resetPhase();
            for (int sd = 0; sd < 3; ++sd)
            {
                auto& c = ch[(size_t) sd];
                if (lock) { c.phase = 0.0; uniSeed (c.uph, sd); OscBank::reset (p, c.ob, sd); }
                if (p.source == SrcTail) c.readPos = p.gate ? (double) tail.length() - 2.0 : tm.gateEnd * 0.001 * sr;   // reaches the tail's start on the gate end
                else if (p.source == SrcSample && playing != nullptr)
                {
                    const double sp = smpSpeed();
                    c.readPos = p.sampleReverse ? (p.gate ? p.tailLenMs : tm.gateEnd) * 0.001 * playing->rate * sp + p.sampleStartMs * 0.001 * playing->rate
                                                : p.sampleStartMs * 0.001 * playing->rate;
                }
                for (auto& u : c.upos) u = c.readPos;
            }
        }
        double smpSpeed() const { return p.keyTrack ? std::pow (2.0, ((hz > 0.0 ? 12.0 * std::log2 (hz / 440.0) + 69.0 : 69.0) - p.root) / 12.0) : 1.0; }

        // one source sample of chain sd at the oversampled rate; e = envelope (warp ENV), ratio = pitch movement
        inline float source (int sd, double e, double ratio, bool refresh)
        {
            float y = osc1G > 0.f ? osc1G * sourceMain (sd, e, ratio, refresh) : (sourceMain (sd, e, ratio, refresh), 0.f);
            if (oscOn)
            {
                const double det = sd == 0 ? 1.0 : std::exp2 (0.7 * uniCents (p, 0, 1, sd, trig.timeMs()) / 1200.0);
                y += OscBank::render (p, ch[(size_t) sd].ob, hz * ratio, osr, det, oscG, oscMult, oscNoise, lv.phase);
            }
            return y;
        }
        // OSC 1 (the read heads / phases move on even when it is silent)
        inline float sourceMain (int sd, double e, double ratio, bool refresh)
        {
            auto& c = ch[(size_t) sd];
            const int nu = sd > 0 && p.uniVoices > 1 ? juce::jmin (6, p.uniVoices - 1) : 0;
            if (refresh) for (int i = 0; i < nu; ++i) uRatio[(size_t) sd][(size_t) i] = std::exp2 (uniCents (p, i, nu, sd, trig.timeMs()) / 1200.0);
            if (p.source == SrcSynth)
            {
                const double dt = hz * ratio / osr;
                c.phase += dt; if (c.phase >= 1.0) c.phase -= 1.0;
                const double a = p.warpAmt * (1.0 - p.warpEnv + p.warpEnv * e) * lv.phase;
                double y = synthWave (p, c.phase, dt, a, sineG);
                if (nu > 0)
                {
                    double yu = 0.0;
                    for (int i = 0; i < nu; ++i)
                    {
                        const double udt = dt * uRatio[(size_t) sd][(size_t) i];
                        double& up = c.uph[(size_t) i];
                        up += udt; if (up >= 1.0) up -= 1.0;
                        yu += synthWave (p, up, udt, a, sineG);
                    }
                    y += yu * p.uniMix / std::sqrt ((double) nu);
                }
                return (float) y;
            }
            const float* d = nullptr; int len = 0; double inc = 0.0;
            if (p.source == SrcTail) { d = tail.data(); len = tail.length(); inc = sr / osr * ratio; }       // (len 0 while the first tail renders: silent, but the read moves on)
            else if (playing != nullptr) { d = playing->data.data(); len = (int) playing->data.size(); inc = playing->rate / osr * ratio * smpSpeed(); }
            if (d == nullptr) return 0.f;
            const double dir = p.source == SrcTail || p.sampleReverse ? -1.0 : 1.0;
            float y = readBuf (d, len, c.readPos);
            c.readPos += dir * inc;
            if (nu > 0)
            {
                float yu = 0.f;
                for (int i = 0; i < nu; ++i)
                {
                    double& up = c.upos[(size_t) i];
                    yu += readBuf (d, len, up);
                    up += dir * inc * uRatio[(size_t) sd][(size_t) i];
                }
                y += yu * p.uniMix / std::sqrt ((float) nu);
            }
            return y;
        }

        void renderChunk (juce::AudioBuffer<float>& buffer, int start, int n)
        {
            // the kick tail renders in the background of the silent part (no CPU spike); finished at once if it is needed now
            if (tail.busy()) tail.pace (trig.isActive() ? trig.msUntilAudible (p) * 0.001 * sr : 0.0, grid);

            work.clear (0, 0, n); work.clear (1, 0, n);
            juce::dsp::AudioBlock<float> blk (work.getArrayOfWritePointers(), 2, 0, (size_t) n);
            auto& os = *osArr[(size_t) osIdx];
            auto up = os.processSamplesUp (blk);
            float* uL = up.getChannelPointer (0);
            float* uR = up.getChannelPointer (1);
            const double msPerSample = 1000.0 / osr;
            const bool w = Stereo::wide (p);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                float yl = 0.f, yr = 0.f;
                if (trig.isActive())
                {
                    auto k = trig.tick (p);
                    if (k.newStep) { startStep(); lanes.restart(); }
                    // REVERSE SHAPE DESIGNER: the drawn lanes on the step's time line
                    if (lanesOn && k.plays)
                    {
                        const double gateEnd = trig.stepTimes().gateEnd, ref = p.gate ? gateEnd : stepMs;
                        lv = lanes.eval (p, k.t, gateEnd, ref);
                        const auto& la = p.lane[LaneAmp];
                        if (la.on) k.target = (k.target * (1.0 - la.depth) + lv.amp * la.depth * k.gate);
                        k.st += lv.st;
                    }
                    else lv = {};
                    double gateNow = k.gate;
                    if (k.target < 1.0e-6 && p.duck <= 0.f) gateNow = 0.0;       // silent part of the step: the filters' ringing is gated too
                    amp += ((float) k.target - amp) * ampC;
                    const bool refresh = (uCount++ & 31) == 0;
                    glideR += (1.0 - glideR) * glideC;
                    const double ratio = std::pow (2.0, k.st / 12.0) * glideR;
                    const float g = std::pow (10.f, p.driveDb * (1.f - p.driveEnv + p.driveEnv * amp) * (float) lv.harm / 20.f);
                    if (tone.dynPre && (preCount++ & 15) == 0)       // PRE FILTER ENV / KEY: the cutoff follows the envelope and the note
                        preNow = doomsday::dsp::svfCoefs ((float) (p.preHz * std::exp2 (p.preEnvOct * (amp - 1.f) - 4.0 * (1.0 - lv.harm)) * std::pow (hz / 65.406, (double) p.preKey)),
                                                          ge::resoToK (p.preReso), osr);
                    const auto& preC = tone.dynPre ? preNow : tone.pre;
                    float out[3] = { 0.f, 0.f, 0.f };
                    for (int sd = 0; sd < (w ? 3 : 1); ++sd)
                        out[sd] = ch[(size_t) sd].smear.process (ch[(size_t) sd].tone.process (source (sd, amp, ratio, refresh) * amp, g, p, tone, preC));
                    float centre = sub.process (out[0], subHzNote, ratio, amp);
                    // CLICK: a short noise tick with its own HP, outside the envelope and the drive, at the START of the bass or its PEAK
                    if (clickG > 0.f && k.plays)
                    {
                        const double mark = trig.stepTimes().delay + (p.clickAt == 1 ? p.swellMs : 0.0);
                        if (k.newStep) lastT = -1.0;
                        if (lastT < mark && k.t >= mark) clickEnv = 1.f;
                        lastT = k.t;
                    }
                    if (clickEnv > 1.0e-4f)
                    {
                        clickRnd ^= clickRnd << 13; clickRnd ^= clickRnd >> 17; clickRnd ^= clickRnd << 5;
                        const float nz = (float) (clickRnd & 0xffffff) / 8388608.f - 1.f;
                        centre += clickHp2.hp (clickHp1.hp (nz, clickC), clickC) * clickEnv * clickG;
                        clickEnv *= clickDecay;
                    }
                    if (w) stereo.process (centre, out[1], out[2], yl, yr);
                    else yl = yr = centre;
                    // the gate once more AFTER the filters: their ringing (the sub crossover delays the lows by a few ms) never reaches the next kick
                    postGate += ((float) gateNow - postGate) * ampC;
                    const float gain = levelG * vel * postGate;
                    yl = outputStage (yl, p, gain); yr = outputStage (yr, p, gain);
                    trig.advance (msPerSample);
                }
                else { amp = 0.f; postGate = 0.f; }
                uL[i] = yl; uR[i] = yr;
            }
            os.processSamplesDown (blk);
            for (int s = 0; s < 2; ++s)
            {
                float pk2 = 0.f;
                for (auto& c : ch) { pk2 = juce::jmax (pk2, c.tone.peak[s]); c.tone.peak[s] = 0.f; }
                drivePeak[(size_t) s].store (juce::jmax (drivePeak[(size_t) s].load(), pk2));
            }
            const float* l = work.getReadPointer (0);
            const float* r = work.getReadPointer (1);
            float pk = 0.f;
            for (int i = 0; i < n; ++i)
            {
                buffer.setSample (0, start + i, l[i]);
                if (buffer.getNumChannels() > 1) buffer.setSample (1, start + i, r[i]);
                pk = juce::jmax (pk, std::abs (l[i]), std::abs (r[i]));
                if (guiWrite < guiCap) gui[(size_t) guiWrite++] = 0.5f * (l[i] + r[i]);
            }
            guiLength.store (guiWrite);
            outPeak.store (juce::jmax (outPeak.load(), pk));
        }

        Params p, pending;
        double sr = 44100.0, osr = 176400.0, bpm = 150.0, bpmPending = 150.0, stepMs = 400.0, hz = 49.0, subHzNote = 49.0, sineG = 0.5;
        uint64_t clock = 0;
        Trigger trig;
        KickTail tail;
        ToneCoefs tone;
        Sub sub;
        Stereo stereo;
        std::array<Chain, 3> ch;
        std::array<std::array<double, 6>, 3> uRatio {};
        uint32_t uCount = 0, preCount = 0;
        doomsday::dsp::SvfCoefs preNow;
        float osc1G = 1.f;
        std::array<float, 3> oscG {}, oscNoise {};
        std::array<double, 3> oscMult {};
        bool oscOn = false, lanesOn = false;
        ShapeLanes lanes;
        LaneValues lv;
        double glideR = 1.0, glideC = 1.0, lastT = -1.0;
        float clickG = 0.f, clickEnv = 0.f, clickDecay = 0.9f;
        uint32_t clickRnd = 0x2545f491u;
        doomsday::dsp::Svf clickHp1, clickHp2;
        doomsday::dsp::SvfCoefs clickC;
        int playNote = 31, guiCap = 0, guiWrite = 0;
        float vel = 1.f, amp = 0.f, postGate = 0.f, levelG = 1.f, ampC = 0.01f;
        std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 4> osArr;
        int osIdx = 1, builtCurves = -1;
        bool offline = false;
        std::array<doomsday::lab::CustomTable, 2> customTab;
        juce::AudioBuffer<float> work;
        std::atomic<Sample*> sample { nullptr };
        const Sample* playing = nullptr;
        std::vector<std::unique_ptr<Sample>> keep;
    };
}
