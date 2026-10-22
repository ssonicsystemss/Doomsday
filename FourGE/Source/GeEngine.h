#pragma once
// =====================================================================
//  4GE - hardstyle kick synthesizer (successor of Forge of Wrath)
//   MIDI note -> kick voice (GeKick.h: PUNCH / TAIL (osc A + B) / TEXTURE / SUB / CLICK -> BUS, ROOM,
//   drawn AMP / DRIVE / PITCH envelopes), rendered 4x oversampled
//   + SAMPLE layer (the user's own punch / kick, played at its own pitch)
//   -> OTT -> master CLIP (the clean sub passes around it) -> OUTPUT
//   A new hit fades the previous one out in 3 ms (two voices).
//   v0.2: the reverse bass moved out into its own plugin, RE4GE (owner, 2026-10-23).
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "GeKick.h"
#include <atomic>

struct GeParams
{
    ge::Params k;
    float ott = 0.f, clipDriveDb = 6.f, clipSoft = 0.3f, velocitySens = 0.f, outDb = 0.f;
    float sampleDb = 0.f, samplePitch = 0.f, sampleStartMs = 0.f;
};

// one loaded sample (mono), never freed while the plugin lives (old ones are kept, see GeEngine::setSample)
struct GeSample
{
    std::vector<float> data;
    double rate = 44100.0;
    juce::String name;
};

class GeEngine
{
public:
    static constexpr int osLog2 = 2, osFactor = 1 << osLog2;
    // the oversampler is made ONCE: re-creating it in prepare() can free it under a running audio callback
    // (FL "Replace" / re-prepare - lesson from dkd, Shared/LESSONS.md); the GUI buffer too (sized for up to 192 kHz)
    GeEngine() : os (std::make_unique<juce::dsp::Oversampling<float>> (1, osLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false))
    {
        hitBuf.assign ((size_t) (192000 * 2.5), 0.f);
    }
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        maxBlockSize = juce::jmax (16, maxBlock);
        os->reset();
        os->initProcessing ((size_t) maxBlockSize);
        mono.setSize (1, maxBlockSize);
        for (auto& v : voices) v.prepare (sr * osFactor);
        ott.prepare (sr);
        hitCap = juce::jmin ((int) hitBuf.size(), (int) (sr * 2.5));
        p.k.cook (sr * osFactor);
    }
    void setParams (const GeParams& np)
    {
        p = np;
        p.k.cook (sr * osFactor);
        for (auto& v : voices) v.setRoom (p.k.roomSize);
    }
    void reset() { for (auto& v : voices) v.release (0.1f); if (os) os->reset(); ott.reset(); smpPos = -1.0; }

    // message thread: new sample (nullptr = none). The audio thread picks it up on the next hit.
    void setSample (std::unique_ptr<GeSample> s)
    {
        GeSample* raw = s.get();
        if (s != nullptr) keep.push_back (std::move (s));
        sample.store (raw);
    }
    const GeSample* currentSample() const { return sample.load(); }

    void trigger (int note, float velocity)
    {
        for (auto& v : voices) if (v.isActive()) v.release (3.f);
        auto& v = voices[(size_t) next]; next = (next + 1) % (int) voices.size();
        vel = 1.f - p.velocitySens * (1.f - velocity);
        v.trigger (440.f * std::pow (2.f, (float) (note - 69) / 12.f), vel, p.k);
        playing = sample.load();
        if (playing != nullptr && ! playing->data.empty()) smpPos = p.sampleStartMs * 0.001 * playing->rate;
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
        float* m = mono.getWritePointer (0);
        float pk = 0.f;
        for (int i = 0; i < n; ++i)
        {
            float x = m[i];
            if (smpPos >= 0.0) { const float sv = sampleTick() * smpG; if (! (p.k.mute & ge::LSample)) x += sv; }
            if (p.ott > 0.001f) { float r = x; ott.process (x, r, p.ott); x = 0.5f * (x + r) * (1.f + 2.f * p.ott); }
            float y;
            if (p.k.cleanSubHz > 20.f)
            {
                // CLEAN SUB: the clipper only gets the highs, the clean sub passes around it (a soft knee near 0 dBFS keeps it safe)
                const float low = subLp2.lp (subLp1.lp (x, subC), subC), high = subHp2.hp (subHp1.hp (x, subC), subC);   // LR4 split
                y = ge::masterClip (high, g, p.clipSoft) + low * g * 0.98f;
                const float a = std::abs (y);
                if (a > 0.9f) y = std::copysign (0.9f + 0.085f * std::tanh ((a - 0.9f) / 0.085f), y);
                y *= outG;
            }
            else y = ge::masterClip (x, g, p.clipSoft) * outG;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.setSample (ch, start + i, y);
            pk = juce::jmax (pk, std::abs (y));
            if (hitWrite < hitCap) hitBuf[(size_t) hitWrite++] = y;
        }
        hitLength.store (hitWrite);
        outPeak.store (juce::jmax (outPeak.load(), pk));
    }

    double sr = 44100.0, smpPos = -1.0;
    int hitCap = 0, maxBlockSize = 512, next = 0, hitWrite = 0;
    float vel = 1.f;
    GeParams p;
    std::array<ge::Voice, 2> voices;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::AudioBuffer<float> mono;
    doomsday::dsp::Ott3 ott;
    doomsday::dsp::Svf subLp1, subLp2, subHp1, subHp2;
    std::atomic<GeSample*> sample { nullptr };
    const GeSample* playing = nullptr;
    std::vector<std::unique_ptr<GeSample>> keep;
};
