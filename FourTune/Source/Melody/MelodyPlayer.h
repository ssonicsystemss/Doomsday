#pragma once
// 4TUNE - PREVIEW: plays the melody into the synth on the host grid (FL playing) or on its own clock (FL stopped).
// Audio thread only reads a snapshot (spin-locked copy made on the message thread): no allocation, no locks that wait.
#include <juce_audio_basics/juce_audio_basics.h>
#include "MelodyGen.h"
#include <array>

namespace tune::melody
{
    struct Snapshot
    {
        static constexpr int maxNotes = 1024;
        std::array<Note, maxNotes> notes {};
        int count = 0;
        double length = 16.0;                         // loop length in beats (= the phrase)
    };

    class Player
    {
    public:
        // message thread: a new melody / loop length
        void publish (const std::vector<Note>& v, double lengthBeats)
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            pending.count = (int) juce::jmin ((size_t) Snapshot::maxNotes, v.size());
            for (int i = 0; i < pending.count; ++i) pending.notes[(size_t) i] = v[(size_t) i];
            pending.length = juce::jmax (1.0, lengthBeats);
            dirty = true;
        }

        // audio thread: add the notes of this block to 'midi'. hostPpq < 0 = the host is stopped (own clock)
        void process (juce::MidiBuffer& midi, int numSamples, double sr, double bpm, double hostPpq, bool on)
        {
            {
                const juce::SpinLock::ScopedTryLockType tl (lock);
                if (tl.isLocked() && dirty) { snap = pending; dirty = false; }
            }
            if (! on) { if (wasOn) allOff (midi, 0); wasOn = false; return; }
            const double beatsPerSample = bpm / 60.0 / sr;
            const bool hostRuns = hostPpq >= 0.0;
            if (! wasOn || hostRuns != lastHostRuns) { allOff (midi, 0); ownBeat = 0.0; lastEnd = -1.0; }
            wasOn = true; lastHostRuns = hostRuns;
            const double startBeat = hostRuns ? hostPpq : ownBeat;
            const double endBeat = startBeat + numSamples * beatsPerSample;
            if (hostRuns && lastEnd >= 0.0 && std::abs (startBeat - lastEnd) > 0.01) allOff (midi, 0);   // the host jumped
            lastEnd = endBeat;
            if (! hostRuns) ownBeat = endBeat;
            const double L = snap.length;
            // note-offs that fall into this block
            for (int i = 0; i < numActive; )
            {
                if (active[(size_t) i].offBeat < endBeat)
                {
                    const int pos = juce::jlimit (0, numSamples - 1, (int) ((active[(size_t) i].offBeat - startBeat) / beatsPerSample));
                    midi.addEvent (juce::MidiMessage::noteOff (1, active[(size_t) i].pitch), pos);
                    active[(size_t) i] = active[(size_t) --numActive];
                }
                else ++i;
            }
            // note-ons (the phrase loops every L beats)
            const double loopStart = std::floor (startBeat / L) * L;
            for (double base = loopStart; base < endBeat; base += L)
                for (int i = 0; i < snap.count; ++i)
                {
                    const auto& n = snap.notes[(size_t) i];
                    if (n.start >= L) continue;
                    const double at = base + n.start;
                    if (at < startBeat || at >= endBeat) continue;
                    const int pos = juce::jlimit (0, numSamples - 1, (int) ((at - startBeat) / beatsPerSample));
                    midi.addEvent (juce::MidiMessage::noteOn (1, n.pitch, (juce::uint8) juce::jlimit (1, 127, n.vel)), pos);
                    if (numActive < (int) active.size()) active[(size_t) numActive++] = { n.pitch, at + juce::jmin (n.len, L) };
                }
        }
        bool playing() const { return wasOn; }
        double positionInLoop (double hostPpq) const { const double b = hostPpq >= 0 ? hostPpq : ownBeat; return std::fmod (b, juce::jmax (1.0, snap.length)); }

    private:
        void allOff (juce::MidiBuffer& midi, int pos)
        {
            for (int i = 0; i < numActive; ++i) midi.addEvent (juce::MidiMessage::noteOff (1, active[(size_t) i].pitch), pos);
            numActive = 0;
        }
        struct Active { int pitch; double offBeat; };
        juce::SpinLock lock;
        Snapshot pending, snap;
        bool dirty = false, wasOn = false, lastHostRuns = false;
        double ownBeat = 0, lastEnd = -1;
        std::array<Active, 256> active {}; int numActive = 0;
    };
}
