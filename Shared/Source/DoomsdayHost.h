#pragma once
// =====================================================================
//  DOOMSDAY shared: talking to the host (namespace doomsday::host)
//  Taken from Damian's dkd (ssonicsystemss/dkd, 2026-10-22: src/DKD_kod_zrodlowy/dkd/Source/ParamCache.h,
//  RutherfordEngine.h, PluginProcessor.cpp), rewritten for our Shared/.
//
//   ParamCache        every parameter's std::atomic<float>* resolved once; get ("id") on the audio thread without
//                     APVTS's String-keyed lookup (that one builds a juce::String = can allocate on every call)
//   BeatClock         the beat position for tempo-locked things (kick on every beat, swells, MODs):
//                     locked to the host ppq while FL plays, a free clock at the host tempo while stopped that starts
//                     with the first key; a loop restart / seek / transport start never swallows the beat it lands on
//   sanitizeState()   drops PARAM entries with NaN / inf from a loaded state (a broken project must not inject NaN)
//
//  Real-time safe (no allocation, no locks).  Tested in Shared/Tests.
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>

namespace doomsday::host
{
    // ------------------------------------------------------------------ ParamCache
    class ParamCache
    {
    public:
        explicit ParamCache (juce::AudioProcessorValueTreeState& state)
        {
            const auto& all = state.processor.getParameters();
            entries.reset (new Entry[(size_t) juce::jmax (1, all.size())]);
            for (auto* prm : all)
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                {
                    auto& e = entries[(size_t) count++];
                    e.id = r->paramID;
                    e.value = state.getRawParameterValue (e.id);
                    e.param = r;
                }
        }

        // the pointer of the id string is remembered per entry (fast path), strcmp otherwise; never allocates
        std::atomic<float>* find (const char* id) const noexcept
        {
            for (int i = 0; i < count; ++i)
                if (entries[(size_t) i].alias.load (std::memory_order_relaxed) == id) return entries[(size_t) i].value;
            for (int i = 0; i < count; ++i)
                if (std::strcmp (entries[(size_t) i].id.toRawUTF8(), id) == 0)
                {
                    entries[(size_t) i].alias.store (id, std::memory_order_relaxed);
                    return entries[(size_t) i].value;
                }
            jassertfalse;   // unknown parameter ID
            return &missing;
        }
        float get (const char* id) const noexcept { return find (id)->load (std::memory_order_relaxed); }
        juce::RangedAudioParameter* param (const char* id) const noexcept
        {
            for (int i = 0; i < count; ++i) if (std::strcmp (entries[(size_t) i].id.toRawUTF8(), id) == 0) return entries[(size_t) i].param;
            return nullptr;
        }

    private:
        struct Entry
        {
            juce::String id;
            std::atomic<float>* value = nullptr;
            juce::RangedAudioParameter* param = nullptr;
            mutable std::atomic<const char*> alias { nullptr };
        };
        std::unique_ptr<Entry[]> entries;
        int count = 0;
        mutable std::atomic<float> missing { 0.f };
        JUCE_DECLARE_NON_COPYABLE (ParamCache)
    };

    // ------------------------------------------------------------------ BeatClock
    //  per block:  beginBlock (playing, ppq, bpm, sampleRate)
    //  key press:  if (firstKey && ! clock.isHostPlaying()) clock.startFree();
    //  per sample: if (clock.crossed (stepBeats)) fire(); ... clock.advance();
    class BeatClock
    {
    public:
        void reset() { clock = 0.0; prev = -1.0; playing = false; }

        void beginBlock (bool hostPlaying, double ppq, double bpmIn, double sampleRate)
        {
            bpm = bpmIn > 20.0 && std::isfinite (bpmIn) ? bpmIn : 150.0;
            inc = bpm / 60.0 / juce::jmax (1.0, sampleRate);
            const bool was = playing;
            playing = hostPlaying && std::isfinite (ppq);
            if (playing)
            {
                // a jump (loop restart, seek, transport start) must not swallow the beat it lands on
                if (! was || std::abs (ppq - clock) > 0.05)
                    prev = ppq - std::floor (ppq) < 0.02 ? std::floor (ppq) - 1.0 : ppq;
                clock = ppq;   // locked to the host every block
            }
        }
        // the first key while the transport is stopped: the free clock starts here, on a beat
        void startFree() { clock = 0.0; prev = -1.0e-9; }

        // true when a step boundary (every `stepBeats` beats) lies between the last sample and this one
        bool crossed (double stepBeats) const
        {
            return std::floor (clock / stepBeats) > std::floor (prev / stepBeats);
        }
        void advance() { prev = clock; clock += inc; }
        void advance (int samples) { prev = clock + inc * (samples - 1); clock += inc * samples; }

        double now() const { return clock; }                                       // beats (quarter notes)
        double phase (double stepBeats) const { const double q = clock / stepBeats; return q - std::floor (q); }
        double beatsPerSample() const { return inc; }
        double tempo() const { return bpm; }
        bool isHostPlaying() const { return playing; }

    private:
        double clock = 0.0, prev = -1.0, inc = 0.0, bpm = 150.0;
        bool playing = false;
    };

    // ------------------------------------------------------------------ state guard
    // call on the XML from getXmlFromBinary before replaceState / reading values
    inline juce::XmlElement& sanitizeState (juce::XmlElement& xml)
    {
        for (int i = xml.getNumChildElements(); --i >= 0;)
            if (auto* c = xml.getChildElement (i))
                if (c->hasAttribute ("value") && ! std::isfinite (c->getDoubleAttribute ("value")))
                    xml.removeChildElement (c, true);
        return xml;
    }
}
