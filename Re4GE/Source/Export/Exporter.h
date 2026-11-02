#pragma once
// RE4GE - EXPORT: the current bass rendered offline (EXPORT quality, the FL grid from bar 1) and written as a 24-bit WAV to
// Documents / RE4GE / Export. Runs on a background thread (never the audio thread); the job carries COPIES of the settings and
// the sample, so the plugin can change while it renders. DSP = re::Engine, files = this component, GUI = ExportPanel.
#include <juce_audio_formats/juce_audio_formats.h>
#include "../DSP/Engine.h"

namespace re4ge
{
    struct ExportJob
    {
        re::Params params;
        std::shared_ptr<re::Sample> sample;     // a copy (may be null)
        double bpm = 150.0, rate = 48000.0;
        int note = 31, beats = 4;
        juce::String name { "RE4GE" };
    };

    struct Exporter
    {
        static juce::File folder() { return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("RE4GE").getChildFile ("Export"); }
        // the bass, one note per beat (PATTERN) or a held note per beat (GATE); silent tail trimmed to the length
        static juce::AudioBuffer<float> render (const ExportJob& j)
        {
            auto eng = std::make_unique<re::Engine>();
            eng->setNonRealtime (true);
            eng->setSampleRaw (j.sample.get());
            eng->setParams (j.params);
            eng->setTempo (j.bpm);
            const int block = 512;
            eng->prepare (j.rate, block);
            const double beatS = 60.0 / j.bpm;
            const int total = (int) std::ceil (j.beats * beatS * j.rate);
            juce::AudioBuffer<float> out (2, total); out.clear();
            juce::AudioBuffer<float> b (2, block);
            for (int pos = 0; pos < total; pos += block)
            {
                const int n = juce::jmin (block, total - pos);
                b.setSize (2, n, false, false, true); b.clear();
                juce::MidiBuffer m;
                for (int k = 0; k < j.beats; ++k)
                {
                    const int on = (int) std::llround (k * beatS * j.rate), off = (int) std::llround ((k + (j.params.gate ? 0.92 : 0.25)) * beatS * j.rate);
                    if (on >= pos && on < pos + n) m.addEvent (juce::MidiMessage::noteOn (1, j.note, 1.f), on - pos);
                    if (off >= pos && off < pos + n) m.addEvent (juce::MidiMessage::noteOff (1, j.note), off - pos);
                }
                eng->setHostPpq (pos / j.rate / beatS, true);
                eng->setParams (j.params);
                eng->process (b, m);
                for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, n);
            }
            return out;
        }
        // a free file name: "<name> G1 150bpm.wav", "... (2).wav"
        static juce::File target (const ExportJob& j)
        {
            static const char* names[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            const juce::String base = juce::File::createLegalFileName (j.name.trim().isEmpty() ? juce::String ("RE4GE") : j.name.trim())
                                      + " " + names[j.note % 12] + juce::String (j.note / 12 - 1) + " " + juce::String (juce::roundToInt (j.bpm)) + "bpm";
            auto f = folder().getChildFile (base + ".wav");
            for (int i = 2; f.existsAsFile(); ++i) f = folder().getChildFile (base + " (" + juce::String (i) + ").wav");
            return f;
        }
        static bool write (const juce::AudioBuffer<float>& b, double rate, const juce::File& f)
        {
            if (! f.getParentDirectory().createDirectory()) return false;
            juce::WavAudioFormat wav;
            auto stream = std::make_unique<juce::FileOutputStream> (f);
            if (! stream->openedOk()) return false;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (stream.get(), rate, 2, 24, {}, 0));
            if (w == nullptr) return false;
            stream.release();
            return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
        // the whole job (background thread): returns the file, or an empty File on failure
        static juce::File run (const ExportJob& j)
        {
            const auto b = render (j);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (ch, i))) return {};
            const auto f = target (j);
            return write (b, j.rate, f) ? f : juce::File();
        }
    };
}
