// Offline test for Fortune of Seekers: presets, waves, polyphony, mono, parts, scream, pump,
// state + presets round trips, randomize, CPU, GUI snapshot, demo wav.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;

static const double sr = 48000.0;
static const int block = 512;
static bool ok = true;

static void check (bool cond, const char* what, const String& extra = {})
{
    printf ("%-50s %s %s\n", what, cond ? "OK" : "FAIL", extra.toRawUTF8());
    ok &= cond;
}
static void set (FortuneProcessor& p, const String& id, float v)
{
    if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

struct Note { double beat, len; int note; };

// render a list of notes (beats at 150 BPM) + tail; returns the audio
static AudioBuffer<float> render (FortuneProcessor& p, const std::vector<Note>& notes, double beats)
{
    const double spb = sr * 60.0 / 150.0;
    const int total = (int) (beats * spb);
    AudioBuffer<float> out (2, total);
    out.clear();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = jmin (block, total - pos);
        AudioBuffer<float> buf (2, n); buf.clear();
        MidiBuffer midi;
        for (auto& e : notes)
        {
            const int on = (int) (e.beat * spb), off = (int) ((e.beat + e.len) * spb);
            if (on >= pos && on < pos + n) midi.addEvent (MidiMessage::noteOn (1, e.note, (uint8) 110), on - pos);
            if (off >= pos && off < pos + n) midi.addEvent (MidiMessage::noteOff (1, e.note), off - pos);
        }
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
    }
    return out;
}

static void silence (FortuneProcessor& p, double seconds = 4.0)
{
    MidiBuffer m; m.addEvent (MidiMessage::allNotesOff (1), 0);
    for (int i = 0; i < (int) (seconds * sr) / block; ++i) { AudioBuffer<float> b (2, block); b.clear(); p.processBlock (b, m); m.clear(); }
}

static float peakOf (const AudioBuffer<float>& b) { return jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples())); }
static bool hasNaN (const AudioBuffer<float>& b)
{
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (ch, i))) return true;
    return false;
}
// harmonic share: 1 - (power at the fundamental) / (total power), over whole cycles
static double harmonicShare (const AudioBuffer<float>& b, int from, double f0)
{
    const int cycles = (int) ((b.getNumSamples() - from) * f0 / sr * 0.9);
    const int len = (int) (cycles * sr / f0);
    double re = 0.0, im = 0.0, all = 0.0;
    for (int i = 0; i < len; ++i)
    {
        const double x = b.getSample (0, from + i), ph = MathConstants<double>::twoPi * f0 * i / sr;
        re += x * std::cos (ph); im += x * std::sin (ph); all += x * x;
    }
    const double fund = 2.0 * (re * re + im * im) / len;
    return all > 0.0 ? jmax (0.0, 1.0 - fund / all) : 0.0;
}

// A minor euphoric progression: Am - F - C - G (2 beats each) and a lead line
static std::vector<Note> chords (double at)
{
    const int prog[4][4] = { { 57, 60, 64, 69 }, { 53, 57, 60, 65 }, { 48, 55, 60, 64 }, { 55, 59, 62, 67 } };
    std::vector<Note> v;
    for (int c = 0; c < 4; ++c) for (int n : prog[c]) v.push_back ({ at + c * 2.0, 1.9, n + 12 });
    return v;
}
static std::vector<Note> lead (double at)
{
    const int line[] = { 69, 72, 76, 81, 79, 76, 72, 74, 76, 74, 72, 71, 72, 76, 79, 83 };
    std::vector<Note> v;
    for (int i = 0; i < 16; ++i) v.push_back ({ at + i * 0.5, 0.45, line[i] });
    return v;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir = argc > 1 ? File (argv[1]) : File::getCurrentWorkingDirectory();
    using namespace ParamIDs;

    FortuneProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    // ---- every factory preset plays chords or a lead line
    const double spb = sr * 60.0 / 150.0;
    const int segBeats = 10;
    AudioBuffer<float> demo (2, (int) (spb * segBeats * proc.getNumPrograms()));
    demo.clear();
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        silence (proc, 0.5);
        const bool isMono = proc.apvts.getRawParameterValue (mono)->load() > 0.5f;
        auto out = render (proc, isMono ? lead (0.0) : chords (0.0), segBeats);
        const float pk = peakOf (out);
        const bool pass = ! hasNaN (out) && pk > 0.05f && pk <= 1.0f;
        ok &= pass;
        printf ("%-20s %s peak %.3f  %s\n", proc.getProgramName (i).toRawUTF8(), isMono ? "lead  " : "chords", pk, pass ? "OK" : "FAIL");
        for (int ch = 0; ch < 2; ++ch) demo.copyFrom (ch, (int) (i * segBeats * spb), out, ch, 0, out.getNumSamples());
    }

    // ---- each wave on its own
    for (int w = 0; w < 6; ++w)
    {
        proc.setCurrentProgram (proc.getNumPrograms() - 1);       // Init
        set (proc, part (1, partOn), 0);
        set (proc, osc (0, 0, wave), (float) w);
        silence (proc, 0.3);
        auto out = render (proc, { { 0.0, 1.0, 60 } }, 1.5);
        check (! hasNaN (out) && peakOf (out) > 0.05f && peakOf (out) < 1.f, ("wave " + Choices::waves[w]).toRawUTF8(), String (peakOf (out), 3));
    }

    // ---- polyphony + mono
    {
        proc.setCurrentProgram (proc.getNumPrograms() - 1);
        silence (proc, 0.3);
        std::vector<Note> six;
        for (int n : { 48, 52, 55, 60, 64, 67 }) six.push_back ({ 0.0, 2.0, n });
        render (proc, six, 1.0);
        check (proc.getActiveVoices() == 6, "poly: 6-note chord = 6 voices", String (proc.getActiveVoices()));
        silence (proc, 1.0);
        std::vector<Note> many;
        for (int i = 0; i < 20; ++i) many.push_back ({ 0.0, 2.0, 40 + i * 2 });
        auto out = render (proc, many, 1.0);
        check (proc.getActiveVoices() <= FortuneEngine::numSlots && ! hasNaN (out), "poly: 20 notes are capped and stay clean",
               String (proc.getActiveVoices()) + " voices");
        silence (proc, 1.0);
        set (proc, mono, 1);
        render (proc, { { 0.0, 2.0, 60 }, { 0.5, 2.0, 64 }, { 1.0, 2.0, 67 } }, 1.5);
        check (proc.getActiveVoices() == 1, "mono: overlapping notes = 1 voice", String (proc.getActiveVoices()));
        set (proc, mono, 0);
    }

    // ---- both parts off -> silence
    {
        proc.setCurrentProgram (0);
        set (proc, part (0, partOn), 0);
        set (proc, part (1, partOn), 0);
        silence (proc, 4.0);
        auto out = render (proc, chords (0.0), 4.0);
        check (peakOf (out) < 1.0e-4f, "both parts off -> silence", String (peakOf (out), 6));
    }

    // ---- SCREAM adds harmonics (sine osc)
    {
        proc.setCurrentProgram (proc.getNumPrograms() - 1);
        set (proc, part (1, partOn), 0);
        set (proc, osc (0, 0, wave), 0);
        set (proc, osc (0, 0, voices), 1);
        set (proc, part (0, revOn), 0);
        silence (proc, 0.3);
        auto clean = render (proc, { { 0.0, 1.0, 57 } }, 1.0);
        set (proc, osc (0, 0, scream), 0.7f);
        silence (proc, 0.3);
        auto scr = render (proc, { { 0.0, 1.0, 57 } }, 1.0);
        const double h0 = harmonicShare (clean, 4800, 220.0), h1 = harmonicShare (scr, 4800, 220.0);
        check (h1 > h0 * 20.0 && h1 > 0.1, "SCREAM adds harmonics", "(harmonics " + String (h0 * 100.0, 3) + "% -> " + String (h1 * 100.0, 1) + "%)");
    }

    // ---- PUMP ducks on the beat
    {
        proc.setCurrentProgram (proc.getNumPrograms() - 1);
        set (proc, part (1, partOn), 0);
        set (proc, part (0, revOn), 0);
        set (proc, part (0, pumpOn), 1);
        set (proc, part (0, pumpDepth), 1.f);
        silence (proc, 0.3);
        auto out = render (proc, { { 0.0, 4.0, 60 } }, 4.0);
        float lo = 1e9f, hi = 0.f;
        for (int w = (int) (sr * 0.2); w + 960 < out.getNumSamples() - (int) (sr * 0.3); w += 960)
        {
            const float r = out.getRMSLevel (0, w, 960);
            lo = jmin (lo, r); hi = jmax (hi, r);
        }
        check (hi > lo * 6.f, "PUMP modulates the level", "(rms " + String (lo, 4) + " .. " + String (hi, 4) + ")");
    }

    // ---- part B alone, part A alone
    {
        proc.setCurrentProgram (0);
        set (proc, part (0, partOn), 0);
        silence (proc, 2.0);
        const float pb = peakOf (render (proc, chords (0.0), 3.0));
        proc.setCurrentProgram (0);
        set (proc, part (1, partOn), 0);
        silence (proc, 2.0);
        const float pa = peakOf (render (proc, chords (0.0), 3.0));
        check (pa > 0.05f && pb > 0.02f, "each part plays on its own", "(A " + String (pa, 3) + ", B " + String (pb, 3) + ")");
    }

    // ---- CPU: 8-note chords on the heaviest preset (9-voice unisons in both parts)
    {
        proc.setCurrentProgram (5);   // Euphoric Horizon
        silence (proc, 0.3);
        std::vector<Note> big;
        for (int rep = 0; rep < 4; ++rep) for (int n : { 45, 52, 57, 60, 64, 69, 72, 76 }) big.push_back ({ rep * 4.0, 3.8, n });
        const auto t0 = Time::getMillisecondCounterHiRes();
        auto out = render (proc, big, 16.0);
        const double ms = Time::getMillisecondCounterHiRes() - t0;
        const double audioMs = out.getNumSamples() / sr * 1000.0;
        printf ("CPU: %.1f s of 8-voice chords in %.0f ms (%.1f%% of real time, 1 core)\n", audioMs / 1000.0, ms, ms / audioMs * 100.0);
        check (! hasNaN (out), "heavy chords render clean");
    }

    // ---- state round trip
    {
        proc.setCurrentProgram (2);
        set (proc, osc (1, 1, semi), 7.f);
        set (proc, fortune, 0.4f);
        MemoryBlock state;
        proc.getStateInformation (state);
        FortuneProcessor p2;
        p2.setStateInformation (state.getData(), (int) state.getSize());
        check (std::abs (p2.apvts.getRawParameterValue (osc (1, 1, semi))->load() - 7.f) < 0.01f
               && std::abs (p2.apvts.getRawParameterValue (fortune)->load() - 0.4f) < 0.01f
               && p2.getPresetName() == proc.getPresetName(), "state round trip");
    }

    // ---- user preset save / load
    {
        proc.setCurrentProgram (1);
        set (proc, part (0, cutoff), 1234.f);
        const bool saved = proc.saveUserPreset ("__test_fos");
        proc.setCurrentProgram (0);
        const auto f = FortuneProcessor::getUserPresetFolder().getChildFile ("__test_fos.fos");
        const bool back = proc.loadUserPreset (f);
        check (saved && back && std::abs (proc.apvts.getRawParameterValue (part (0, cutoff))->load() - 1234.f) < 1.f, "user preset save/load");
        f.deleteFile();
    }

    // ---- randomize
    for (int i = 0; i < 30; ++i)
    {
        proc.randomize();
        silence (proc, 0.2);
        auto out = render (proc, chords (0.0), 3.0);
        const float pk = peakOf (out);
        if (hasNaN (out) || pk < 0.02f || pk > 1.0f) { printf ("randomize #%d FAIL peak %.3f\n", i, pk); ok = false; }
    }
    check (true, "randomize x30");

    // ---- demo wav
    {
        auto f = outDir.getChildFile ("fortune_demo.wav");
        f.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (demo, 0, demo.getNumSamples());
    }

    // ---- GUI snapshot with both paths moving
    {
        proc.setCurrentProgram (0);
        silence (proc, 0.5);
        auto both = chords (0.0);
        render (proc, both, 3.3);
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (Canvas::W, Canvas::H);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile ("fortune_gui.png");
        f.deleteFile();
        FileOutputStream os (f);
        PNGImageFormat().writeImageToStream (img, os);
    }

    printf (ok ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
