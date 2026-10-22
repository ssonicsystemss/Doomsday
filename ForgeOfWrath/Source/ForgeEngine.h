#pragma once
// =====================================================================
//  FORGE OF WRATH - hardstyle kick synthesizer
//   MIDI note -> kick voice (Shared DoomsdayKick.h: TAIL + STAGE 1 -> MID EQ -> STAGE 2 + ROOM + PUNCH + CLICK,
//   drawn AMP / DRIVE / PITCH envelopes), rendered 4x oversampled
//   + SAMPLE layer (the user's own punch / kick, played at its own pitch)
//   -> OTT -> + REVERSE BASS -> master CLIP -> OUTPUT
//   A new hit fades the previous one out in 3 ms (two voices).
//   REVERSE BASS (v2.1, from Damian's dkd - Shared DoomsdayReverseBass.h): every kick starts one beat of swells
//   (1/4, 1/8 or 1/16 steps) on the kick's note, locked to the host tempo: the bass rises from IN to OUT of each step,
//   pitches up at the end like a reversed kick tail and leaves a clean gap before the next kick.
//   SOURCE TAIL (default, the owner's wish): the bass IS the kick's own tail played backwards - every kick renders its tail
//   (no punch / click, flat level) on its note into a buffer, and each swell plays it reversed so it ENDS on the tail's
//   start (the pitch drop becomes the rise). SOURCE SYNTH: saw + sine with RISE (the dkd way).
//   -> its own DRIVE (ADAA, soft = tanh, no DC) -> TONE (24 dB low-pass) -> LEVEL.
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "DoomsdayKick.h"
#include "DoomsdayReverseBass.h"
#include "DoomsdayDrive.h"
#include <atomic>

struct ForgeParams
{
    doomsday::kick::KickParams k;
    float ott = 0.f, clipDriveDb = 6.f, clipSoft = 0.3f, velocitySens = 0.f, outDb = 0.f;
    float sampleDb = 0.f, samplePitch = 0.f, sampleStartMs = 0.f;
    // REVERSE BASS
    bool rbOn = false, rbTail = true;
    doomsday::rbass::Params rb;
    float rbTuneSt = 0.f, rbDriveDb = 14.f, rbToneHz = 1600.f, rbLevelDb = -4.f;
};

// one loaded sample (mono), never freed while the plugin lives (old ones are kept, see ForgeEngine::setSample)
struct ForgeSample
{
    std::vector<float> data;
    double rate = 44100.0;
    juce::String name;
};

class ForgeEngine
{
public:
    static constexpr int osLog2 = 2, osFactor = 1 << osLog2;
    // the oversampler is made ONCE: re-creating it in prepare() can free it under a running audio callback
    // (FL "Replace" / re-prepare - lesson from dkd, Shared/LESSONS.md)
    // big buffers too (sized for up to 192 kHz): nothing the audio thread or the GUI touches is re-allocated in prepare()
    ForgeEngine() : os (std::make_unique<juce::dsp::Oversampling<float>> (1, osLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false))
    {
        hitBuf.assign ((size_t) (192000 * 2.5), 0.f);
        rbBuf.assign ((size_t) (192000 * 2.1), 0.f);
    }
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        maxBlockSize = juce::jmax (16, maxBlock);
        os->reset();
        os->initProcessing ((size_t) maxBlockSize);
        rbass.prepare (sr); rbDrive.reset(); rbLp1 = {}; rbLp2 = {}; sinceKick = -1.0;
        rbLen = 0; rbVoice.prepare (sr);
        mono.setSize (1, maxBlockSize);
        for (auto& v : voices) v.prepare (sr * osFactor);
        ott.prepare (sr);
        hitCap = juce::jmin ((int) hitBuf.size(), (int) (sr * 2.5));
        p.k.cook (sr * osFactor);
    }
    void setParams (const ForgeParams& np)
    {
        p = np;
        p.k.cook (sr * osFactor);
        for (auto& v : voices) v.setRoom (p.k.roomSize);
    }
    void reset() { for (auto& v : voices) v.release (0.1f); if (os) os->reset(); ott.reset(); smpPos = -1.0; sinceKick = -1.0; rbass.reset(); }
    void setTempo (double bpmNow) { bpm = bpmNow > 20.0 && std::isfinite (bpmNow) ? juce::jlimit (20.0, 400.0, bpmNow) : 150.0; tempoNow.store ((float) bpm); }
    std::atomic<float> tempoNow { 150.f };      // for the GUI (REVERSE BASS lane)

    // message thread: new sample (nullptr = none). The audio thread picks it up on the next hit.
    void setSample (std::unique_ptr<ForgeSample> s)
    {
        ForgeSample* raw = s.get();
        if (s != nullptr) keep.push_back (std::move (s));
        sample.store (raw);
    }
    const ForgeSample* currentSample() const { return sample.load(); }

    void trigger (int note, float velocity)
    {
        for (auto& v : voices) if (v.isActive()) v.release (3.f);
        auto& v = voices[(size_t) next]; next = (next + 1) % (int) voices.size();
        vel = 1.f - p.velocitySens * (1.f - velocity);
        v.trigger (440.f * std::pow (2.f, (float) (note - 69) / 12.f), vel);
        // REVERSE BASS: one beat of swells starts with this kick, on its note
        sinceKick = 0.0;
        rbass.setNoteHz (440.0 * std::pow (2.0, ((double) note - 69.0 + p.k.tuneSemis + p.rbTuneSt) / 12.0));
        if (p.rbOn && p.rbTail) renderReverseTail (note);
        playing = sample.load();
        if (playing != nullptr && ! playing->data.empty())
        {
            smpPos = p.sampleStartMs * 0.001 * playing->rate;
            smpFade = 1.f;
        }
        hitWrite = 0; hitSerial.fetch_add (1);
    }

    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
    {
        const int n = buffer.getNumSamples();
        int pos = 0;
        for (const auto meta : midi)
        {
            const int at = juce::jlimit (0, n, meta.samplePosition);
            if (at > pos) { render (buffer, pos, at - pos); pos = at; }
            const auto m = meta.getMessage();
            if (m.isNoteOn()) trigger (m.getNoteNumber(), m.getFloatVelocity());
        }
        if (pos < n) render (buffer, pos, n - pos);
    }

    void audition (bool on, int note = 31) { if (on) trigger (note, 1.f); }

    // last hit for the GUI (playhead / meter)
    std::vector<float> hitBuf;
    std::atomic<int> hitLength { 0 }, hitSerial { 0 };
    std::atomic<float> outPeak { 0.f };

private:
    // REVERSE BASS, SOURCE TAIL: the tail of this kick (its note, its distortion), long enough for one swell
    void renderReverseTail (int note)
    {
        rbParams = p.k;
        rbParams.punchOn = false; rbParams.clickDb = -60.f; rbParams.headFixed = false;
        rbParams.amp = doomsday::kick::Shape::make ({ { 0.f, 1.f }, { 1000.f, 1.f } });
        const double swellSec = (p.rb.out - p.rb.in) * p.rb.stepBeats * 60.0 / bpm;
        rbLen = juce::jlimit (0, juce::jmin ((int) rbBuf.size(), (int) (sr * 2.1)), (int) (swellSec * sr) + 2);
        rbParams.lengthMs = (float) (swellSec * 1000.0) + 20.f;
        rbParams.cook (sr);
        rbVoice.setRoom (rbParams.roomSize);
        rbVoice.trigger (440.f * std::pow (2.f, ((float) note - 69.f + p.rbTuneSt) / 12.f), 1.f);
        for (int i = 0; i < rbLen; ++i) rbBuf[(size_t) i] = rbVoice.render (rbParams);
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
    inline float sampleTick()
    {
        if (playing == nullptr || smpPos < 0.0) return 0.f;
        const auto& d = playing->data;
        const int i = (int) smpPos;
        if (i + 1 >= (int) d.size()) { smpPos = -1.0; return 0.f; }
        const float fr = (float) (smpPos - i);
        const float y = d[(size_t) i] + fr * (d[(size_t) i + 1] - d[(size_t) i]);
        smpPos += playing->rate / sr * std::pow (2.0, (double) p.samplePitch / 12.0);
        return y;
    }
    void renderChunk (juce::AudioBuffer<float>& buffer, int start, int n)
    {
        mono.clear (0, 0, n);
        juce::dsp::AudioBlock<float> blk (mono.getArrayOfWritePointers(), 1, 0, (size_t) n);
        auto up = os->processSamplesUp (blk);
        float* u = up.getChannelPointer (0);
        bool any = false;
        for (auto& v : voices) any |= v.isActive();
        for (size_t i = 0; i < up.getNumSamples(); ++i)
        {
            float s = 0.f;
            if (any) for (auto& v : voices) s += v.render (p.k);
            u[i] = s;
        }
        os->processSamplesDown (blk);
        const float g = std::pow (10.f, p.clipDriveDb / 20.f), outG = std::pow (10.f, p.outDb / 20.f);
        const float smpG = std::pow (10.f, p.sampleDb / 20.f) * vel;
        const auto subC = doomsday::dsp::svfCoefs (juce::jmax (20.f, p.k.cleanSubHz), 1.f / 0.707f, sr);
        const bool rbActive = p.rbOn && (sinceKick >= 0.0 || (! p.rbTail && rbass.sounding()));
        const auto rbC1 = doomsday::dsp::svfCoefs (p.rbToneHz, 1.f / 0.54f, sr), rbC2 = doomsday::dsp::svfCoefs (p.rbToneHz, 1.f / 1.31f, sr);
        const float rbG = std::pow (10.f, p.rbDriveDb / 20.f), rbMk = doomsday::drive::makeup (doomsday::drive::Soft, rbG);
        const float rbOut = std::pow (10.f, p.rbLevelDb / 20.f);
        const double beatsPerSample = bpm / 60.0 / sr;
        float* m = mono.getWritePointer (0);
        float pk = 0.f;
        for (int i = 0; i < n; ++i)
        {
            float x = m[i];
            if (smpPos >= 0.0) x += sampleTick() * smpG;
            if (p.ott > 0.001f) { float r = x; ott.process (x, r, p.ott); x = 0.5f * (x + r) * (1.f + 2.f * p.ott); }
            if (rbActive)
            {
                const double beat = sinceKick >= 0.0 ? sinceKick * beatsPerSample : 1.0;
                float b;
                if (p.rbTail)
                {
                    // the kick's tail backwards: u = 0 at IN -> 1 at OUT reads the tail from its end back to its start
                    b = 0.f;
                    if (sinceKick >= 0.0 && beat < 1.0 && rbLen > 2)
                    {
                        const double q = beat / p.rb.stepBeats, s = q - std::floor (q);
                        float riseU = 0.f;
                        const float a = doomsday::rbass::swell (p.rb, s, p.rb.stepBeats * 60.0 / bpm, riseU);
                        if (a > 0.f)
                        {
                            const double u = (s - p.rb.in) / juce::jmax (0.01, (double) (p.rb.out - p.rb.in));
                            const double rp = (1.0 - u) * (double) (rbLen - 2);
                            const int i0 = juce::jlimit (0, rbLen - 2, (int) rp);
                            const float fr = (float) (rp - i0);
                            b = (rbBuf[(size_t) i0] + fr * (rbBuf[(size_t) i0 + 1] - rbBuf[(size_t) i0])) * a;
                        }
                    }
                }
                else b = rbass.next (p.rb, beat, bpm, sinceKick >= 0.0 && beat < 1.0);
                b = rbDrive.process (b * rbG, doomsday::drive::Soft) * rbMk;
                b = rbLp2.lp (rbLp1.lp (b, rbC1), rbC2);
                x += b * rbOut;
                if (sinceKick >= 0.0) { sinceKick += 1.0; if (beat >= 1.0) sinceKick = -1.0; }
            }
            float y;
            if (p.k.cleanSubHz > 20.f)
            {
                // CLEAN SUB: the clipper only gets the highs, the clean sub passes around it (a soft knee near 0 dBFS keeps it safe)
                const float low = subLp2.lp (subLp1.lp (x, subC), subC), high = subHp2.hp (subHp1.hp (x, subC), subC);   // LR4 split
                y = doomsday::kick::masterClip (high, g, p.clipSoft) + low * g * 0.98f;
                const float a = std::abs (y);
                if (a > 0.9f) y = std::copysign (0.9f + 0.085f * std::tanh ((a - 0.9f) / 0.085f), y);
                y *= outG;
            }
            else y = doomsday::kick::masterClip (x, g, p.clipSoft) * outG;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.setSample (ch, start + i, y);
            pk = juce::jmax (pk, std::abs (y));
            if (hitWrite < hitCap) hitBuf[(size_t) hitWrite++] = y;
        }
        hitLength.store (hitWrite);
        outPeak.store (juce::jmax (outPeak.load(), pk));
    }

    double sr = 44100.0, smpPos = -1.0, bpm = 150.0, sinceKick = -1.0;
    doomsday::rbass::ReverseBass rbass;
    doomsday::kick::KickVoice rbVoice;
    doomsday::kick::KickParams rbParams;
    std::vector<float> rbBuf;
    int rbLen = 0, hitCap = 0;
    doomsday::drive::Adaa1 rbDrive;
    doomsday::dsp::Svf rbLp1, rbLp2;
    int maxBlockSize = 512, next = 0, hitWrite = 0;
    float vel = 1.f, smpFade = 1.f;
    ForgeParams p;
    std::array<doomsday::kick::KickVoice, 2> voices;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::AudioBuffer<float> mono;
    doomsday::dsp::Ott3 ott;
    doomsday::dsp::Svf subLp1, subLp2, subHp1, subHp2;
    std::atomic<ForgeSample*> sample { nullptr };
    const ForgeSample* playing = nullptr;
    std::vector<std::unique_ptr<ForgeSample>> keep;
};
