#pragma once
// =====================================================================
//  4GE module 10 - the SAMPLE POOL (file / state management only: never the audio thread, never GUI code)
//   - the 8 slots' audio lives in the plugin state (child SAMPLES): name, original path, the WORKING COPY embedded as a WAV
//     (base64; up to 20 s) - the user's file is never written; a missing file with no embedded copy = MISSING + RELINK, never
//     silently replaced;
//   - decoding (WAV / AIFF / FLAC / OGG / MP3), resampling (windowed sinc) to the engine rate and the tools (NORMALIZE, DC OFFSET,
//     TRIM SILENCE) run on a background thread; the result is published to the audio thread by an atomic pointer swap; a replaced
//     sample stays alive >= 1 s of audio (a playing voice crossfades away from it), then the message thread frees it;
//   - preview of a file (the browser) without assigning it to a slot.
// =====================================================================
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include "../DSP/Sample/Sample.h"
#include <mutex>
#include <memory>

namespace fge::smp
{
    using dsp::SampleData;
    enum Status { SEmpty = 0, SLoading, SReady, SMissing, SError };
    inline const char* statusText (int s) { static const char* t[5] { "EMPTY", "LOADING", "READY", "MISSING - RELINK", "ERROR" }; return t[juce::jlimit (0, 4, s)]; }
    static constexpr const char* tag = "SAMPLES";
    static constexpr double maxEmbedSec = 20.0;

    struct SlotInfo
    {
        int status = SEmpty;
        juce::String name, path, message;
        int channels = 0;
        double sourceRate = 0.0;
        int64_t sourceLen = 0;
    };

    inline juce::AudioFormatManager& formats()
    {
        struct F { juce::AudioFormatManager m; F() { m.registerBasicFormats(); } };
        static F f;
        return f.m;
    }
    // decode any supported file / stream -> the working copy (mono / stereo; more channels -> the first two + a message)
    inline bool decode (std::unique_ptr<juce::AudioFormatReader> rd, juce::AudioBuffer<float>& out, double& rate, juce::String& msg)
    {
        if (rd == nullptr) { msg = "unsupported or damaged file"; return false; }
        if (rd->lengthInSamples <= 4) { msg = "empty file"; return false; }
        if (rd->lengthInSamples > (int64_t) (rd->sampleRate * 60.0)) { msg = "longer than 60 s"; return false; }
        const int ch = (int) juce::jmin (2u, rd->numChannels);
        if (rd->numChannels > 2) msg = juce::String ((int) rd->numChannels) + " channels: using the first two";
        out.setSize (ch, (int) rd->lengthInSamples);
        rd->read (&out, 0, (int) rd->lengthInSamples, 0, true, ch > 1);
        rate = rd->sampleRate;
        return true;
    }
    inline juce::String toBase64Wav (const juce::AudioBuffer<float>& b, double rate)
    {
        juce::WavAudioFormat wav;
        // (written through a block we own: JUCE writers own their stream)
        juce::MemoryBlock mb;
        {
            auto* stream = new juce::MemoryOutputStream (mb, false);
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (stream, rate, (unsigned) b.getNumChannels(), 24, {}, 0));
            if (w == nullptr) { delete stream; return {}; }
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
        return mb.toBase64Encoding();
    }
    inline bool fromBase64Wav (const juce::String& s, juce::AudioBuffer<float>& out, double& rate)
    {
        juce::MemoryBlock mb;
        if (s.isEmpty() || ! mb.fromBase64Encoding (s)) return false;
        juce::WavAudioFormat wav;
        juce::String msg;
        return decode (std::unique_ptr<juce::AudioFormatReader> (wav.createReaderFor (new juce::MemoryInputStream (mb, true), true)), out, rate, msg);
    }
    // the engine copy: resampled (windowed sinc) to the engine rate
    inline std::shared_ptr<SampleData> prepare (const juce::AudioBuffer<float>& b, double fromRate, double toRate, uint32_t serial)
    {
        auto d = std::make_shared<SampleData>();
        d->channels = b.getNumChannels();
        d->l = dsp::resampleSinc (b.getReadPointer (0), b.getNumSamples(), fromRate, toRate);
        if (d->channels > 1) d->r = dsp::resampleSinc (b.getReadPointer (1), b.getNumSamples(), fromRate, toRate);
        d->n = (int64_t) d->l.size(); d->rate = toRate; d->serial = serial;
        if (d->channels > 1 && d->r.size() != d->l.size()) d->r.resize (d->l.size());
        return d;
    }
    // ---- destructive tools on the WORKING COPY (never the original file)
    enum Tool { TNormalize = 0, TDcOffset, TTrimSilence };
    inline void applyTool (juce::AudioBuffer<float>& b, int tool)
    {
        const int n = b.getNumSamples(), ch = b.getNumChannels();
        if (tool == TNormalize) { const float pk = b.getMagnitude (0, n); if (pk > 1.0e-6f) b.applyGain (0.98f / pk); }
        if (tool == TDcOffset)
            for (int c = 0; c < ch; ++c) { double m = 0.0; auto* p = b.getWritePointer (c); for (int i = 0; i < n; ++i) m += p[i]; m /= juce::jmax (1, n); for (int i = 0; i < n; ++i) p[i] -= (float) m; }
        if (tool == TTrimSilence)
        {
            const float thr = juce::jmax (1.0e-4f, b.getMagnitude (0, n) * 0.003f);         // -50 dB below the peak
            int a = 0, e = n - 1;
            auto loud = [&] (int i) { for (int c = 0; c < ch; ++c) if (std::abs (b.getSample (c, i)) > thr) return true; return false; };
            while (a < n - 1 && ! loud (a)) ++a;
            while (e > a && ! loud (e)) --e;
            if (e - a > 16 && (a > 0 || e < n - 1))
            {
                juce::AudioBuffer<float> t (ch, e - a + 1);
                for (int c = 0; c < ch; ++c) t.copyFrom (c, 0, b, c, a, e - a + 1);
                const int f = juce::jmin (64, t.getNumSamples() / 4);                    // tiny fades at the cut points
                for (int c = 0; c < ch; ++c) { t.applyGainRamp (c, 0, f, 0.f, 1.f); t.applyGainRamp (c, t.getNumSamples() - f, f, 1.f, 0.f); }
                b = std::move (t);
            }
        }
    }

    // the editor's picture of a slot (made on the background thread): min / max per bin of the mono sum + detected transients (0..1)
    struct Overview
    {
        static constexpr int bins = 4096;
        std::vector<float> mn, mx, transients;
        int64_t sourceLen = 0;
    };
    inline std::shared_ptr<Overview> makeOverview (const juce::AudioBuffer<float>& b, double rate)
    {
        auto o = std::make_shared<Overview>();
        const int n = b.getNumSamples(), ch = b.getNumChannels();
        o->sourceLen = n; o->mn.assign (Overview::bins, 0.f); o->mx.assign (Overview::bins, 0.f);
        if (n <= 0 || ch <= 0) return o;
        auto mono = [&] (int i) { float s = 0.f; for (int c = 0; c < ch; ++c) s += b.getSample (c, i); return s / (float) ch; };
        for (int k = 0; k < Overview::bins; ++k)
        {
            const int a = (int) ((int64_t) k * n / Overview::bins), e = juce::jmax (a + 1, (int) ((int64_t) (k + 1) * n / Overview::bins));
            float lo = 1.f, hi = -1.f; for (int i = a; i < e && i < n; ++i) { const float v = mono (i); lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); }
            o->mn[(size_t) k] = lo > hi ? 0.f : lo; o->mx[(size_t) k] = lo > hi ? 0.f : hi;
        }
        // TRANSIENTS: jumps of the 2 ms energy (> 9 dB above the last 30 ms, -50 dB under the peak at most, 40 ms apart)
        const int hop = juce::jmax (4, (int) (0.002 * rate));
        std::vector<float> e;
        for (int a = 0; a + hop <= n; a += hop) { double q = 0; for (int i = a; i < a + hop; ++i) { const float v = mono (i); q += v * v; } e.push_back ((float) (q / hop)); }
        const float peakE = e.empty() ? 0.f : *std::max_element (e.begin(), e.end());
        int last = -(1 << 30);
        for (size_t i = 1; i < e.size(); ++i)
        {
            float past = 0.f; int c = 0;
            for (int j = (int) i - 1; j >= 0 && j >= (int) i - 15; --j) { past += e[(size_t) j]; ++c; }
            past /= (float) juce::jmax (1, c);
            const int at = (int) i * hop;
            if (e[i] > peakE * 1.0e-5f && e[i] > past * 8.f && at - last > (int) (0.04 * rate)) { o->transients.push_back ((float) at / (float) n); last = at; }
        }
        return o;
    }

    class Pool : private juce::Thread
    {
    public:
        using Ptrs = std::array<std::atomic<const SampleData*>, dsp::numSampleSlots>;
        Pool() : juce::Thread ("4GE samples") { for (auto& p : ptrs) p.store (nullptr); startThread (juce::Thread::Priority::low); }
        ~Pool() override { signalThreadShouldExit(); wake.signal(); stopThread (4000); for (auto& p : ptrs) p.store (nullptr); }

        Ptrs ptrs;                                                       // what the audio thread reads
        std::atomic<int64_t> audioClock { 0 };                           // audio samples processed (the processor adds every block)
        std::function<void (int slot)> onChange;                         // (message thread) a slot finished loading / changed status

        void setTargetRate (double r)
        {
            if (std::abs (r - targetRate.load()) < 1.0) return;
            targetRate.store (r);
            std::lock_guard<std::mutex> l (lock);
            for (int k = 0; k < dsp::numSampleSlots; ++k) if (have[(size_t) k]) jobs[(size_t) k] = true;      // re-prepare at the new rate
            wake.signal();
        }
        // the state changed (message thread): decode what is new
        void syncFromState (const juce::ValueTree& t)
        {
            for (int k = 0; k < dsp::numSampleSlots; ++k)
            {
                const juce::String key = "s" + juce::String (k + 1);
                const juce::String data = t.isValid() ? t.getProperty (key + "data").toString() : juce::String();
                const juce::String path = t.isValid() ? t.getProperty (key + "path").toString() : juce::String();
                const juce::String name = t.isValid() ? t.getProperty (key + "name").toString() : juce::String();
                const juce::String sig = juce::String (data.hashCode64()) + "|" + path;
                if (sig == sigs[(size_t) k] && name == info (k).name) continue;
                sigs[(size_t) k] = sig;
                std::lock_guard<std::mutex> l (lock);
                auto& in = infos[(size_t) k];
                in.name = name; in.path = path; in.message = {};
                ++gen[(size_t) k];                                                // (a job still running for the old content is dropped)
                if (data.isEmpty() && path.isEmpty()) { in = {}; publish (k, nullptr); have[(size_t) k] = false; working[(size_t) k] = {}; jobs[(size_t) k] = decodeNeeded[(size_t) k] = false; continue; }
                in.status = SLoading;
                srcData[(size_t) k] = data; srcPath[(size_t) k] = path; decodeNeeded[(size_t) k] = true; jobs[(size_t) k] = true;
                wake.signal();
            }
        }
        SlotInfo info (int k) const { std::lock_guard<std::mutex> l (lock); return infos[(size_t) juce::jlimit (0, dsp::numSampleSlots - 1, k)]; }
        std::shared_ptr<const SampleData> data (int k) const { std::lock_guard<std::mutex> l (lock); return live[(size_t) juce::jlimit (0, dsp::numSampleSlots - 1, k)]; }
        std::shared_ptr<const Overview> overview (int k) const { std::lock_guard<std::mutex> l (lock); return views[(size_t) juce::jlimit (0, dsp::numSampleSlots - 1, k)]; }
        uint32_t version() const { return changes.load(); }               // (GUI: something of a slot changed)
        bool busy() const { std::lock_guard<std::mutex> l (lock); for (bool j : jobs) if (j) return true; return working_ > 0; }
        void waitIdle (int timeoutMs = 20000) const { const auto t0 = juce::Time::getMillisecondCounter(); while (busy() && juce::Time::getMillisecondCounter() - t0 < (juce::uint32) timeoutMs) juce::Thread::sleep (2); }
        // the working copy of a slot (tools / embedding) - a copy, message thread
        bool workingCopy (int k, juce::AudioBuffer<float>& b, double& rate) const
        {
            std::lock_guard<std::mutex> l (lock);
            if (! have[(size_t) k]) return false;
            b.makeCopyOf (working[(size_t) k]); rate = workRate[(size_t) k]; return true;
        }
        // free what no voice can still be playing (message thread, a timer)
        void collectGarbage()
        {
            std::lock_guard<std::mutex> l (lock);
            const int64_t now = audioClock.load();
            graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(), [now] (const auto& g) { return now - g.second > (int64_t) 96000; }), graveyard.end());
            if (now == lastCollect && graveyard.size() > 64) graveyard.erase (graveyard.begin(), graveyard.begin() + 32);   // (only while no audio runs: bounded memory)
            lastCollect = now;
        }
        // ---- PREVIEW (the browser): a file at the host rate, mixed into the output by the processor
        void preview (const juce::File& f, double hostRate)
        {
            std::lock_guard<std::mutex> l (lock);
            previewFile = f; previewRate = hostRate; previewJob = true; wake.signal();
        }
        // REGION PREVIEW: START..END of a slot (REVERSE as set) at the host rate, without triggering a kick
        void previewRegion (int k, float start, float end, bool reverse, double hostRate)
        {
            std::lock_guard<std::mutex> l (lock);
            previewSlot = k; previewA = juce::jmin (start, end); previewB = juce::jmax (start, end); previewRev = reverse; previewRate = hostRate; previewJob = true; wake.signal();
        }
        // module 12: listen to a rendered buffer (export PREVIEW) - any thread except the audio thread
        void previewAudio (const juce::AudioBuffer<float>& b, double fromRate, double hostRate)
        {
            auto d = prepare (b, fromRate, hostRate, 0u);
            std::lock_guard<std::mutex> l (lock);
            previewPos.store (-1);
            if (previewLive != nullptr) graveyard.push_back ({ previewLive, audioClock.load() });
            previewLive = d; previewPtr.store (d.get()); previewPos.store (0);
        }
        void stopPreview() { previewPos.store (-1); }
        void mixPreview (juce::AudioBuffer<float>& out)                  // audio thread: lock-free
        {
            const auto* p = previewPtr.load (std::memory_order_acquire);
            int64_t pos = previewPos.load();
            if (p == nullptr || pos < 0) return;
            const int n = out.getNumSamples();
            for (int i = 0; i < n && pos < p->n; ++i, ++pos)
                for (int c = 0; c < out.getNumChannels(); ++c) out.addSample (c, i, 0.7f * p->at (c, pos));
            previewPos.store (pos < p->n ? pos : -1);
        }
        bool previewing() const { return previewPos.load() >= 0; }

    private:
        void publish (int k, std::shared_ptr<SampleData> d)              // (lock held)
        {
            if (live[(size_t) k] != nullptr) graveyard.push_back ({ live[(size_t) k], audioClock.load() });
            live[(size_t) k] = d;
            ptrs[(size_t) k].store (d.get(), std::memory_order_release);
            if (d == nullptr) views[(size_t) k] = nullptr;
            changes.fetch_add (1);
        }
        void run() override
        {
            while (! threadShouldExit())
            {
                wake.wait (200);
                for (int k = 0; k < dsp::numSampleSlots && ! threadShouldExit(); ++k)
                {
                    juce::String data, path; bool dec; uint32_t g;
                    {
                        std::lock_guard<std::mutex> l (lock);
                        if (! jobs[(size_t) k]) continue;
                        jobs[(size_t) k] = false; data = srcData[(size_t) k]; path = srcPath[(size_t) k]; dec = decodeNeeded[(size_t) k]; decodeNeeded[(size_t) k] = false;
                        g = gen[(size_t) k]; ++working_;
                    }
                    juce::AudioBuffer<float> b; double rate = 44100.0; juce::String msg; bool ok = true;
                    if (dec)
                    {
                        if (data.isNotEmpty()) ok = fromBase64Wav (data, b, rate);
                        else
                        {
                            const juce::File f (path);
                            if (! f.existsAsFile()) ok = false, msg = "file not found";
                            else ok = decode (std::unique_ptr<juce::AudioFormatReader> (formats().createReaderFor (f)), b, rate, msg);
                        }
                    }
                    else { std::lock_guard<std::mutex> l (lock); b.makeCopyOf (working[(size_t) k]); rate = workRate[(size_t) k]; }
                    std::shared_ptr<SampleData> d; std::shared_ptr<Overview> ov;
                    if (ok) { d = prepare (b, rate, targetRate.load(), ++serial); ov = makeOverview (b, rate); }
                    {
                        std::lock_guard<std::mutex> l (lock);
                        --working_;
                        auto& in = infos[(size_t) k];
                        if (g != gen[(size_t) k]) continue;                                    // superseded while working
                        if (ok)
                        {
                            working[(size_t) k] = std::move (b); workRate[(size_t) k] = rate; have[(size_t) k] = true;
                            in.status = SReady; in.channels = working[(size_t) k].getNumChannels(); in.sourceRate = rate; in.sourceLen = working[(size_t) k].getNumSamples();
                            if (msg.isNotEmpty()) in.message = msg;
                            publish (k, d); views[(size_t) k] = ov;
                        }
                        else
                        {
                            in.status = path.isNotEmpty() && data.isEmpty() && ! juce::File (path).existsAsFile() ? SMissing : SError; in.message = msg;
                            have[(size_t) k] = false; publish (k, nullptr);
                        }
                    }
                    if (onChange) juce::MessageManager::callAsync ([this, k, alive = alivePtr] { if (*alive && onChange) onChange (k); });
                }
                // preview
                juce::File pf; double pr = 44100.0; bool pj; int ps; float pa, pb; bool prv;
                { std::lock_guard<std::mutex> l (lock); pj = previewJob; previewJob = false; pf = previewFile; pr = previewRate; ps = previewSlot; previewSlot = -1; pa = previewA; pb = previewB; prv = previewRev; }
                if (pj)
                {
                    juce::AudioBuffer<float> b; double rate = 44100.0; juce::String msg; bool got = false;
                    if (ps >= 0)
                    {
                        juce::AudioBuffer<float> w;
                        { std::lock_guard<std::mutex> l (lock); if (have[(size_t) ps]) { w.makeCopyOf (working[(size_t) ps]); rate = workRate[(size_t) ps]; got = true; } }
                        if (got)
                        {
                            const int n = w.getNumSamples(), a = juce::jlimit (0, n - 1, (int) (pa * (float) n)), e = juce::jlimit (a + 1, n, (int) (pb * (float) n));
                            b.setSize (w.getNumChannels(), e - a);
                            for (int c = 0; c < w.getNumChannels(); ++c) b.copyFrom (c, 0, w, c, a, e - a);
                            if (prv) b.reverse (0, b.getNumSamples());
                            const int f = juce::jmin (64, b.getNumSamples() / 4);
                            for (int c = 0; c < b.getNumChannels(); ++c) { b.applyGainRamp (c, 0, f, 0.f, 1.f); b.applyGainRamp (c, b.getNumSamples() - f, f, 1.f, 0.f); }
                        }
                    }
                    else got = decode (std::unique_ptr<juce::AudioFormatReader> (formats().createReaderFor (pf)), b, rate, msg);
                    if (got)
                    {
                        auto d = prepare (b, rate, pr, ++serial);
                        std::lock_guard<std::mutex> l (lock);
                        previewPos.store (-1);
                        if (previewLive != nullptr) graveyard.push_back ({ previewLive, audioClock.load() });
                        previewLive = d; previewPtr.store (d.get()); previewPos.store (0);
                    }
                }
            }
            *alivePtr = false;
        }
        mutable std::mutex lock;
        juce::WaitableEvent wake;
        std::atomic<double> targetRate { 44100.0 * 4.0 };
        std::array<SlotInfo, dsp::numSampleSlots> infos {};
        std::array<juce::String, dsp::numSampleSlots> sigs, srcData, srcPath;
        std::array<bool, dsp::numSampleSlots> jobs {}, decodeNeeded {}, have {};
        std::array<uint32_t, dsp::numSampleSlots> gen {};
        std::array<juce::AudioBuffer<float>, dsp::numSampleSlots> working;
        std::array<double, dsp::numSampleSlots> workRate {};
        std::array<std::shared_ptr<SampleData>, dsp::numSampleSlots> live;
        std::vector<std::pair<std::shared_ptr<SampleData>, int64_t>> graveyard;
        int working_ = 0;
        int64_t lastCollect = -1;
        uint32_t serial = 0;
        juce::File previewFile; double previewRate = 44100.0; bool previewJob = false;
        int previewSlot = -1; float previewA = 0.f, previewB = 1.f; bool previewRev = false;
        std::array<std::shared_ptr<Overview>, dsp::numSampleSlots> views;
        std::atomic<uint32_t> changes { 0 };
        std::shared_ptr<SampleData> previewLive;
        std::atomic<const SampleData*> previewPtr { nullptr };
        std::atomic<int64_t> previewPos { -1 };
        std::shared_ptr<bool> alivePtr = std::make_shared<bool> (true);
    };
}
