#pragma once
// =====================================================================
//  4GE ENGINE (module 01: Core Kick Engine)
//   MIDI NOTE ON -> KickVoice (pitch engine + PUNCH / BODY / TAIL / SUB generators, 4x oversampled)
//   -> CORE MIXER (level / pan / mute / solo / phase invert / peak meter per part) -> MASTER -> out
//  Voices: CHOKE (a new kick fades the previous one in 6 ms) or POLY (they overlap), 8 at most (the oldest is reused).
//  Real-time safe: the oversampler and every buffer are made in the constructor / prepare(); process() never allocates.
//  Pure DSP: knows no GUI, no files, no APVTS - it gets a ParamFrame per block.
// =====================================================================
#include <juce_dsp/juce_dsp.h>
#include "Core/Generators.h"
#include "../Parameters/ParamFrame.h"
#include <array>
#include <atomic>

namespace fge
{
    // ------------------------------------------------------------------ one kick
    class KickVoice
    {
    public:
        void trigger (int note, float velocityGain, const dsp::KickSettings& ks, const float* phases01, uint64_t order)
        {
            using namespace dsp;
            active = true; t = 0.0; age = order; vel = velocityGain; fade = 1.f; fadeStep = 0.f;
            noteHz = 440.f * std::exp2 ((float) (note - 69) / 12.f);
            const uint32_t seed = 0x9e3779b9u * (uint32_t) (note + 1);
            punch.start (ks.part[PPunch], phases01[PPunch], seed ^ 0x1234u);
            body.start  (ks.part[PBody],  phases01[PBody],  seed ^ 0x5678u);
            tail.start  (ks.part[PTail],  phases01[PTail],  seed ^ 0x9abcu);
            sub.start   (ks.part[PSub],   phases01[PSub],   seed);
        }
        void choke (float ms, double rate) { if (active) fadeStep = 1.f / juce::jmax (1.f, ms * 0.001f * (float) rate); }
        bool isActive() const { return active; }
        uint64_t order() const { return age; }
        void phases (float* out) const { out[0] = punch.phase(); out[1] = body.phase(); out[2] = tail.phase(); out[3] = sub.phase(); }

        // adds this voice into the four part buses (oversampled rate)
        void render (const dsp::KickSettings& ks, float* const* bus, int n)
        {
            using namespace dsp;
            if (! active) return;
            const auto& ps = ks.pitch;
            const float k = ps.keyTrack ? noteHz / juce::jmax (1.f, ps.endHz) : 1.f;
            const float endEff = (ps.keyTrack ? noteHz : ps.endHz) * ps.fineMul;
            const float startEff = ps.startHz * k * ps.fineMul;
            const float lnRatio = std::log (juce::jmax (1.f, startEff) / juce::jmax (1.f, endEff)) + ps.amountSt * 0.05776227f;  // ln2 / 12
            const float eng0 = endEff * std::exp (lnRatio * pitchShape (ps.mode, 0.f, ps.timeMs, ps.curve));
            const float subHz = ps.keyTrack ? endEff : ks.part[PSub].pitchHz * ps.fineMul;
            float total = 0.f;
            for (const auto& p : ks.part) total = juce::jmax (total, p.endMs);
            total = juce::jmin (total, ks.lengthMs);
            const float fadeLen = juce::jmin (15.f, ks.lengthMs * 0.1f);
            const double dt = 1.0 / ks.rate;
            for (int i = 0; i < n; ++i)
            {
                const float ms = (float) (t * 1000.0);
                if (ms > total + 0.5f) { active = false; break; }
                const float s = pitchShape (ps.mode, ms, ps.timeMs, ps.curve);
                const float hz = juce::jlimit (5.f, (float) ks.rate * 0.2f, endEff * std::exp (lnRatio * s));
                const float gate = juce::jlimit (0.f, 1.f, (ks.lengthMs - ms) / juce::jmax (0.5f, fadeLen)) * vel * fade;
                bus[PPunch][i] += punch.tick (ks.part[PPunch], ks, ms, hz, eng0, k) * gate;
                bus[PBody][i]  += body.tick  (ks.part[PBody],  ks, ms, hz, eng0, k) * gate;
                bus[PTail][i]  += tail.tick  (ks.part[PTail],  ks, ms, hz) * gate;
                bus[PSub][i]   += sub.tick   (ks.part[PSub],   ks, ms, subHz) * gate;
                if (fadeStep > 0.f) { fade -= fadeStep; if (fade <= 0.f) { fade = 0.f; active = false; break; } }
                t += dt;
            }
        }
    private:
        dsp::PunchGenerator punch; dsp::BodyGenerator body; dsp::TailGenerator tail; dsp::SubGenerator sub;
        double t = 0.0;
        float noteHz = 49.f, vel = 1.f, fade = 1.f, fadeStep = 0.f;
        bool active = false;
        uint64_t age = 0;
    };

    // ------------------------------------------------------------------ the engine
    class Engine
    {
    public:
        static constexpr int osLog2 = 2, osFactor = 1 << osLog2, maxVoices = 8;

        Engine() : os (std::make_unique<juce::dsp::Oversampling<float>> (2, osLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false)) {}

        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate;
            maxBlockSize = juce::jmax (16, maxBlock);
            os->reset();
            os->initProcessing ((size_t) maxBlockSize);
            bus.setSize (dsp::NumParts, maxBlockSize * osFactor);
            stereo.setSize (2, maxBlockSize);
            for (auto& v : voices) v = KickVoice();
            for (auto& g : gainNow) g = { 0.f, 0.f };
            masterNow = 0.f; first = true; dcL = {}; dcR = {};
            for (auto& p : freePhase) p = 0.f;
        }
        int latencySamples() const { return (int) std::lround (os->getLatencyInSamples()); }

        void process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const ParamFrame& f)
        {
            cook (f);
            const int n = out.getNumSamples();
            int pos = 0;
            for (const auto meta : midi)
            {
                const int at = juce::jlimit (0, n, meta.samplePosition);
                if (at > pos) { render (out, pos, at - pos); pos = at; }
                const auto m = meta.getMessage();
                if (m.isNoteOn()) trigger (m.getNoteNumber(), m.getFloatVelocity());
                // Note Off never cuts the kick: it rings out with its own length (module 01, 11)
            }
            if (pos < n) render (out, pos, n - pos);
        }

        void trigger (int note, float velocity)
        {
            const bool poly = frame.idx (P::coreVoice) == 1;
            float ph[dsp::NumParts];
            // PHASE RESET off: the new kick continues the phases of the newest kick (free-running oscillators)
            KickVoice* newest = nullptr;
            for (auto& v : voices) if (v.isActive() && (newest == nullptr || v.order() > newest->order())) newest = &v;
            if (! ks.phaseReset) { if (newest != nullptr) newest->phases (freePhase.data()); for (int i = 0; i < dsp::NumParts; ++i) ph[i] = freePhase[(size_t) i]; }
            else for (int i = 0; i < dsp::NumParts; ++i) ph[i] = ks.part[(size_t) i].phase01;
            if (! poly) for (auto& v : voices) v.choke (6.f, ks.rate);
            KickVoice* slot = nullptr;
            for (auto& v : voices) if (! v.isActive()) { slot = &v; break; }
            if (slot == nullptr) { slot = &voices[0]; for (auto& v : voices) if (v.order() < slot->order()) slot = &v; }
            const float velGain = 1.f - ks.velSens * (1.f - juce::jlimit (0.f, 1.f, velocity));
            slot->trigger (note, velGain, ks, ph, ++counter);
            hits.fetch_add (1);
        }
        void stopAll() { for (auto& v : voices) v.choke (3.f, ks.rate); }
        void cookOnly (const ParamFrame& f) { cook (f); }

        // meters (peak since the GUI last read them): PUNCH / BODY / TAIL / SUB and the output
        std::array<std::atomic<float>, dsp::NumParts> partPeak {};
        std::atomic<float> outPeak { 0.f };
        std::atomic<int> hits { 0 };
        const dsp::KickSettings& settings() const { return ks; }

    private:
        // ---------------------------------------------------------- per block: parameters -> cooked settings
        void cook (const ParamFrame& f)
        {
            using namespace dsp;
            frame = f;
            const double rate = sr * osFactor;
            ks.rate = rate; ks.inv = (float) (1.0 / rate);
            ks.dcR = (float) std::exp (-juce::MathConstants<double>::twoPi * 5.0 / rate);
            ks.dcTail = (float) std::exp (-juce::MathConstants<double>::twoPi * 10.0 / rate);
            ks.avgC = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 8.0 / rate));
            ks.lengthMs = f[P::coreLength];
            ks.phaseReset = f.on (P::corePhaseRst);
            ks.velSens = f[P::coreVelocity];
            auto& pc = ks.pitch;
            pc.mode = f.idx (P::peMode); pc.startHz = f[P::peStart]; pc.endHz = f[P::peEnd]; pc.timeMs = f[P::peTime];
            pc.curve = f[P::peCurve]; pc.amountSt = f[P::peAmount]; pc.fineMul = std::exp2 (f[P::peFine] / 1200.f); pc.keyTrack = f.on (P::coreKeyTrack);
            // AUTO LENGTH: the punch keeps its short attack, the body follows a little, tail and sub follow fully
            const float sc = f.on (P::coreAutoLen) ? f[P::coreLength] / 450.f : 1.f;
            const float scBody = std::sqrt (sc);
            auto env = [] (float a, float d, float s, float r, float len, float scale)
            {
                EnvTimes e; e.attackMs = a; e.decayMs = d * scale; e.sustain = s; e.releaseMs = r * scale; e.lengthMs = len * scale; return e;
            };
            auto db = [] (float v) { return std::pow (10.f, v / 20.f); };
            auto tone = [rate] (float hz, float reso) { return svfCoefs (hz, 1.f / (0.7f + reso * reso * 11.f), rate); };
            auto setDrive = [&db] (PartSettings& p, float d, float maxDb) { p.drive = d; p.driveG = db (d * maxDb); p.driveNorm = 1.f / std::tanh (juce::jmax (1.0e-3f, p.driveG)); };

            auto& pu = ks.part[PPunch];
            pu.source = f.idx (P::punchSrc); pu.pitchHz = f[P::punchPitch]; pu.glideMs = f[P::punchGlide];
            pu.env = env (f[P::punchAttack], f[P::punchDecay], f[P::punchSustain], f[P::punchRelease], f[P::punchLen], 1.f);
            pu.tone = svfCoefs (f[P::punchSrc] > 4.5f ? juce::jlimit (300.f, 16000.f, f[P::punchPitch] * 6.f) : f[P::punchTone], pu.source == 5 ? 1.f / 1.5f : 1.414f, rate);
            pu.toneOpen = pu.source != 5 && f[P::punchTone] > 19000.f;
            setDrive (pu, f[P::punchDrive], 30.f); pu.mix = f[P::punchMix]; pu.phase01 = f[P::punchPhase] / 360.f;

            auto& bo = ks.part[PBody];
            bo.source = f.idx (P::bodySrc); bo.pitchHz = f[P::bodyPitch]; bo.glideMs = f[P::bodyGlide];
            bo.env = env (f[P::bodyAttack], f[P::bodyDecay], f[P::bodySustain], f[P::bodyRelease], f[P::bodyLen], scBody);
            bo.tone = tone (f[P::bodyTone], f[P::bodyReso]); bo.toneOpen = f[P::bodyTone] > 19000.f && f[P::bodyReso] < 0.01f;
            setDrive (bo, f[P::bodyDrive], 30.f); bo.mix = f[P::bodyMix]; bo.phase01 = f[P::bodyPhase] / 360.f;

            auto& ta = ks.part[PTail];
            ta.source = f.idx (P::tailSrc); ta.character = f.idx (P::tailChar); ta.pw = f[P::tailPw];
            ta.env = env (f[P::tailAttack], f[P::tailDecay], f[P::tailSustain], f[P::tailRelease], f[P::tailLen], sc);
            ta.tone = tone (f[P::tailTone], f[P::tailReso]); ta.toneOpen = f[P::tailTone] > 19000.f && f[P::tailReso] < 0.01f;
            static constexpr float tailMaxDb[7] { 0.f, 36.f, 56.f, 48.f, 40.f, 36.f, 24.f };     // hardstyle tails need a LOT of gain (sharp edges = highs)
            setDrive (ta, f[P::tailDrive], tailMaxDb[juce::jlimit (0, 6, ta.character)]);
            if (ta.character == 5) ta.driveG = db (12.f + ta.drive * 36.f);
            ta.mix = f[P::tailMix]; ta.phase01 = f[P::tailPhase] / 360.f;
            ks.scream = svfCoefs (1200.f * (0.7f + 0.8f * ta.drive), 1.f / 6.f, rate);

            auto& su = ks.part[PSub];
            su.source = f.idx (P::subSrc); su.pitchHz = f[P::subPitch];
            su.env = env (f[P::subAttack], f[P::subDecay], f[P::subSustain], f[P::subRelease], f[P::subLen], sc);
            setDrive (su, f[P::subDrive], 18.f); su.phase01 = f[P::subPhase] / 360.f;
            for (auto& p : ks.part) p.endMs = p.env.lengthMs;

            // CORE MIXER: generator LEVEL x fader x mute / solo x polarity, constant-power pan
            const int lvl[4]  { P::punchLevel, P::bodyLevel, P::tailLevel, P::subLevel };
            const int fad[4]  { P::mixPunchLvl, P::mixBodyLvl, P::mixTailLvl, P::mixSubLvl };
            const int pan[4]  { P::mixPunchPan, P::mixBodyPan, P::mixTailPan, P::mixSubPan };
            const int mute[4] { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute };
            const int solo[4] { P::mixPunchSolo, P::mixBodySolo, P::mixTailSolo, P::mixSubSolo };
            const int inv[4]  { P::mixPunchInv, P::mixBodyInv, P::mixTailInv, P::mixSubInv };
            bool anySolo = false;
            for (int i = 0; i < 4; ++i) anySolo |= f.on (solo[i]);
            for (int i = 0; i < 4; ++i)
            {
                const bool silent = f.on (mute[i]) || (anySolo && ! f.on (solo[i]));
                const float gl = f[lvl[i]] <= -59.9f ? 0.f : db (f[lvl[i]]);
                const float gf = f[fad[i]] <= -59.9f ? 0.f : db (f[fad[i]]);
                const float g = silent ? 0.f : gl * gf * (f.on (inv[i]) ? -1.f : 1.f);
                const float a = (juce::jlimit (-1.f, 1.f, f[pan[i]]) + 1.f) * juce::MathConstants<float>::pi * 0.25f;
                gainTarget[(size_t) i] = { g * std::cos (a) * 1.41421356f, g * std::sin (a) * 1.41421356f };   // centre = unity
            }
            masterTarget = f[P::coreMaster] <= -59.9f ? 0.f : db (f[P::coreMaster]);
            if (first) { gainNow = gainTarget; masterNow = masterTarget; first = false; }
        }

        void render (juce::AudioBuffer<float>& out, int start, int len)
        {
            while (len > 0)
            {
                const int n = juce::jmin (len, maxBlockSize);
                renderChunk (out, start, n);
                start += n; len -= n;
            }
        }
        void renderChunk (juce::AudioBuffer<float>& out, int start, int n)
        {
            const int on = n * osFactor;
            for (int c = 0; c < dsp::NumParts; ++c) juce::FloatVectorOperations::clear (bus.getWritePointer (c), on);
            float* b[dsp::NumParts] { bus.getWritePointer (0), bus.getWritePointer (1), bus.getWritePointer (2), bus.getWritePointer (3) };
            for (auto& v : voices) v.render (ks, b, on);

            stereo.clear (0, 0, n); stereo.clear (1, 0, n);
            juce::dsp::AudioBlock<float> blk (stereo.getArrayOfWritePointers(), 2, 0, (size_t) n);
            auto up = os->processSamplesUp (blk);
            float* L = up.getChannelPointer (0);
            float* R = up.getChannelPointer (1);
            const float inv = 1.f / (float) on;
            std::array<float, dsp::NumParts> pk {};
            for (int c = 0; c < dsp::NumParts; ++c)
            {
                const auto g0 = gainNow[(size_t) c], g1 = gainTarget[(size_t) c];
                const float* x = b[c];
                float p = 0.f;
                for (int i = 0; i < on; ++i)
                {
                    const float u = (float) (i + 1) * inv;
                    const float gl = g0.first + (g1.first - g0.first) * u, gr = g0.second + (g1.second - g0.second) * u;
                    const float yl = x[i] * gl, yr = x[i] * gr;
                    L[i] = (c == 0 ? 0.f : L[i]) + yl;
                    R[i] = (c == 0 ? 0.f : R[i]) + yr;
                    p = juce::jmax (p, std::abs (yl), std::abs (yr));
                }
                pk[(size_t) c] = p;
                gainNow[(size_t) c] = g1;
            }
            os->processSamplesDown (blk);
            const float m0 = masterNow, m1 = masterTarget, invN = 1.f / (float) n;
            const float r = (float) std::exp (-juce::MathConstants<double>::twoPi * 5.0 / sr);
            float opk = 0.f;
            const float* sl = stereo.getReadPointer (0);
            const float* sr2 = stereo.getReadPointer (1);
            for (int i = 0; i < n; ++i)
            {
                const float m = m0 + (m1 - m0) * (float) (i + 1) * invN;
                const float yl = dcL.tick (sl[i], r) * m, yr = dcR.tick (sr2[i], r) * m;
                out.setSample (0, start + i, yl);
                if (out.getNumChannels() > 1) out.setSample (1, start + i, yr);
                opk = juce::jmax (opk, std::abs (yl), std::abs (yr));
            }
            masterNow = m1;
            for (int c = 0; c < dsp::NumParts; ++c) partPeak[(size_t) c].store (juce::jmax (partPeak[(size_t) c].load(), pk[(size_t) c]));
            outPeak.store (juce::jmax (outPeak.load(), opk));
        }

        double sr = 44100.0;
        int maxBlockSize = 512;
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        juce::AudioBuffer<float> bus, stereo;
        std::array<KickVoice, maxVoices> voices;
        dsp::KickSettings ks;
        ParamFrame frame = ParamFrame::defaults();
        std::array<std::pair<float, float>, dsp::NumParts> gainNow {}, gainTarget {};
        std::array<float, dsp::NumParts> freePhase {};
        float masterNow = 0.f, masterTarget = 1.f;
        bool first = true;
        dsp::DcBlock dcL, dcR;
        uint64_t counter = 0;
    };

    // ------------------------------------------------------------------ offline render (GUI waveform, tests, export)
    //  one kick at 'rate', 'seconds' long; soloPart = -1 the full kick, 0..3 = only that part (mixer settings kept)
    inline juce::AudioBuffer<float> renderKick (ParamFrame f, int note, double rate, double seconds, int soloPart = -1, float velocity = 1.f)
    {
        if (soloPart >= 0)
        {
            const int solo[4] { P::mixPunchSolo, P::mixBodySolo, P::mixTailSolo, P::mixSubSolo };
            const int mute[4] { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute };
            for (int i = 0; i < 4; ++i) { f[solo[i]] = i == soloPart ? 1.f : 0.f; f[mute[i]] = 0.f; }
        }
        const int block = 256, total = (int) (rate * seconds);
        auto engine = std::make_unique<Engine>();
        engine->prepare (rate, block);
        juce::AudioBuffer<float> out (2, total), tmp (2, block);
        out.clear();
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            tmp.setSize (2, n, false, false, true); tmp.clear();
            juce::MidiBuffer m;
            if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, velocity), 0);
            engine->process (tmp, m, f);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, tmp, ch, 0, n);
        }
        return out;
    }
}
