#pragma once
// =====================================================================
//  FORGE OF WRATH - hardstyle kick synthesizer
//   MIDI note -> kick voice (Shared DoomsdayKick.h: TAIL + DIST + PUNCH + CLICK, drawn AMP / DRIVE envelopes)
//   rendered 4x oversampled -> master CLIP (drive into hard / soft clip) -> OUTPUT
//   A new hit fades the previous one out in 3 ms (two voices).
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "DoomsdayKick.h"
#include <atomic>

struct ForgeParams
{
    doomsday::kick::KickParams k;
    float clipDriveDb = 6.f, clipSoft = 0.3f, velocitySens = 0.f, outDb = 0.f;
};

class ForgeEngine
{
public:
    static constexpr int osLog2 = 2, osFactor = 1 << osLog2;
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        maxBlockSize = juce::jmax (16, maxBlock);
        os = std::make_unique<juce::dsp::Oversampling<float>> (1, osLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        os->initProcessing ((size_t) maxBlockSize);
        mono.setSize (1, maxBlockSize);
        for (auto& v : voices) v.prepare (sr * osFactor);
        hitBuf.assign ((size_t) (sr * 2.0), 0.f);
    }
    void setParams (const ForgeParams& np) { p = np; }
    void reset() { for (auto& v : voices) v = {}; for (auto& v : voices) v.prepare (sr * osFactor); if (os) os->reset(); }

    void trigger (int note, float velocity)
    {
        for (auto& v : voices) if (v.isActive()) v.release (3.f);
        auto& v = voices[(size_t) next]; next = (next + 1) % (int) voices.size();
        v.prepare (sr * osFactor);
        const float vel = 1.f - p.velocitySens * (1.f - velocity);
        v.trigger (440.f * std::pow (2.f, (float) (note - 69) / 12.f), vel);
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
        const float* m = mono.getReadPointer (0);
        float pk = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float y = doomsday::kick::masterClip (m[i], g, p.clipSoft) * outG;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.setSample (ch, start + i, y);
            pk = juce::jmax (pk, std::abs (y));
            if (hitWrite < (int) hitBuf.size()) hitBuf[(size_t) hitWrite++] = y;
        }
        hitLength.store (hitWrite);
        outPeak.store (juce::jmax (outPeak.load(), pk));
    }

    double sr = 44100.0;
    int maxBlockSize = 512, next = 0, hitWrite = 0;
    ForgeParams p;
    std::array<doomsday::kick::KickVoice, 2> voices;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::AudioBuffer<float> mono;
};
