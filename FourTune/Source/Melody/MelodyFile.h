#pragma once
// 4TUNE - melody <-> MIDI file (files only: no DSP, no GUI). Used by DRAG MIDI (spec 24) and the tests.
#include <juce_audio_basics/juce_audio_basics.h>
#include "MelodyGen.h"

namespace tune::melody
{
    // write the notes as a type-1 MIDI file: tempo + one track, 960 ticks per quarter (pitch, length, timing, velocity kept)
    inline bool writeMidi (const std::vector<Note>& notes, double bpm, const juce::File& f, const juce::String& trackName = "4TUNE melody")
    {
        constexpr int tpq = 960;
        juce::MidiMessageSequence tempo, track;
        tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / juce::jmax (20.0, bpm))), 0.0);
        tempo.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
        track.addEvent (juce::MidiMessage::textMetaEvent (3, trackName), 0.0);
        double end = 0;
        for (auto& n : notes)
        {
            const double on = std::round (n.start * tpq), off = std::round ((n.start + n.len) * tpq);
            track.addEvent (juce::MidiMessage::noteOn (1, juce::jlimit (0, 127, n.pitch), (juce::uint8) juce::jlimit (1, 127, n.vel)), on);
            track.addEvent (juce::MidiMessage::noteOff (1, juce::jlimit (0, 127, n.pitch)), juce::jmax (on + 1.0, off - 1.0));
            end = juce::jmax (end, off);
        }
        track.sort(); track.updateMatchedPairs();
        tempo.addEvent (juce::MidiMessage::endOfTrack(), end); track.addEvent (juce::MidiMessage::endOfTrack(), end);
        juce::MidiFile mf; mf.setTicksPerQuarterNote (tpq);
        mf.addTrack (tempo); mf.addTrack (track);
        f.deleteFile();
        juce::FileOutputStream os (f);
        return os.openedOk() && mf.writeTo (os, 1);
    }

    // read the notes of a MIDI file (beats), all tracks merged
    inline std::vector<Note> readMidi (const juce::File& f)
    {
        std::vector<Note> out;
        juce::FileInputStream in (f); juce::MidiFile mf;
        if (! in.openedOk() || ! mf.readFrom (in)) return out;
        const double tpq = (double) juce::jmax (1, (int) mf.getTimeFormat());
        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            juce::MidiMessageSequence seq (*mf.getTrack (t)); seq.updateMatchedPairs();
            for (auto* e : seq)
                if (e->message.isNoteOn() && e->noteOffObject != nullptr)
                {
                    Note n; n.start = e->message.getTimeStamp() / tpq;
                    n.len = (e->noteOffObject->message.getTimeStamp() - e->message.getTimeStamp()) / tpq;
                    n.pitch = e->message.getNoteNumber(); n.vel = e->message.getVelocity();
                    out.push_back (n);
                }
        }
        std::sort (out.begin(), out.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
        return out;
    }

    // DRAG MIDI: a fresh file in the temp folder named after the melody (FL takes the file name as the pattern name)
    inline juce::File exportForDrag (const std::vector<Note>& notes, double bpm, const juce::String& name)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("4TUNE");
        dir.createDirectory();
        auto f = dir.getChildFile (juce::File::createLegalFileName (name) + ".mid");
        writeMidi (notes, bpm, f, name);
        return f;
    }
}
