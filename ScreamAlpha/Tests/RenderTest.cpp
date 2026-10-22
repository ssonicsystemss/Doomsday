// 5CR34M 4LPH4 offline test: every preset plays the 16th riff + a held note -> NaN / silence / clipping checks,
// one WAV with all presets, GUI snapshot.   RenderTest <outdir>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
using namespace juce;

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir = argc > 1 ? File (argv[1]) : File::getCurrentWorkingDirectory();
    const double sr = 48000.0; const int block = 512;
    AlphaProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    const int beat = (int) (sr * 60.0 / 150.0), six = beat / 4;
    struct N { int step, len, note; };
    const std::vector<N> riff { { 0, 1, 62 }, { 1, 1, 62 }, { 2, 1, 74 }, { 3, 1, 62 }, { 5, 1, 62 }, { 6, 1, 65 }, { 7, 1, 69 },
                                { 8, 2, 72 }, { 10, 1, 69 }, { 11, 1, 67 }, { 12, 1, 65 }, { 13, 1, 64 }, { 14, 2, 65 } };
    const int perPreset = beat * 4 * 2 + (int) sr * 2;      // 2 bars of riff + 1.6 s held note + tail
    AudioBuffer<float> all (2, perPreset * proc.getNumPrograms()); all.clear();
    bool ok = true;
    for (int prog = 0; prog < proc.getNumPrograms(); ++prog)
    {
        proc.setCurrentProgram (prog);
        { AudioBuffer<float> b (2, block); MidiBuffer m; m.addEvent (MidiMessage::allNotesOff (1), 0);
          for (int i = 0; i < 60; ++i) { b.clear(); proc.processBlock (b, m); m.clear(); } }
        struct E { int pos; bool on; int note; };
        std::vector<E> ev;
        for (int bar = 0; bar < 2; ++bar)
            for (auto& n : riff) { const int on = bar * beat * 4 + n.step * six; ev.push_back ({ on, true, n.note }); ev.push_back ({ on + n.len * six - 600, false, n.note }); }
        ev.push_back ({ beat * 8, true, 69 }); ev.push_back ({ beat * 8 + (int) (sr * 1.6), false, 69 });
        std::sort (ev.begin(), ev.end(), [] (const E& a, const E& b) { return a.pos < b.pos || (a.pos == b.pos && ! a.on && b.on); });
        size_t ei = 0; float peak = 0.f; double sum = 0.0; bool nan = false;
        for (int pos = 0; pos < perPreset; pos += block)
        {
            AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer midi;
            while (ei < ev.size() && ev[ei].pos < pos + block)
            { const auto& e = ev[ei++]; midi.addEvent (e.on ? MidiMessage::noteOn (1, e.note, (uint8) 110) : MidiMessage::noteOff (1, e.note), jmax (0, e.pos - pos)); }
            proc.processBlock (buf, midi);
            const int n = jmin (block, perPreset - pos);
            for (int ch = 0; ch < 2; ++ch)
            {
                all.copyFrom (ch, prog * perPreset + pos, buf, ch, 0, n);
                for (int i = 0; i < n; ++i) { const float v = buf.getSample (ch, i); nan |= ! std::isfinite (v); peak = jmax (peak, std::abs (v)); sum += v * v; }
            }
        }
        const float rms = (float) std::sqrt (sum / (2.0 * perPreset));
        const bool good = ! nan && peak > 0.02f && peak < 1.2f;
        printf ("%-28s peak %.3f  rms %.1f dB  %s\n", proc.getProgramName (prog).toRawUTF8(), peak, Decibels::gainToDecibels (rms), good ? "OK" : "FAIL");
        ok &= good;
    }
    {
        auto f = outDir.getChildFile ("scream_alpha_presets.wav"); f.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
    }
    {   // RANDOM: no NaN, every sound near the target level
        float lo = 100.f, hi = -100.f; bool fin = true;
        for (int i = 0; i < 12; ++i)
        {
            proc.randomize();
            const float db = proc.measureLoudnessDb();
            fin &= std::isfinite (db);
            lo = jmin (lo, db); hi = jmax (hi, db);
        }
        const bool good = fin && lo > AlphaProcessor::randomTargetDb - 4.f && hi < AlphaProcessor::randomTargetDb + 4.f;
        printf ("random x12 level %.1f .. %.1f dB (target %.1f): %s\n", lo, hi, AlphaProcessor::randomTargetDb, good ? "OK" : "FAIL");
        ok &= good;
    }
    auto setP = [&proc] (const char* id, float v) { if (auto* q = proc.apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    auto setN = [&proc] (const char* id, float v01) { if (auto* q = proc.apvts.getParameter (id)) q->setValueNotifyingHost (v01); };
    const float cutIdx = (float) alpha::mod::targetIndex ("cutoffHz");
    {   // MOD 1 -> CUTOFF: RISE over 1 bar, from 400 Hz up to 6.4 kHz -> the last quarter of the bar is clearly brighter than the first
        proc.setCurrentProgram (1);
        setP ("cutoffHz", 400.f); setP ("autoShape", 1); setP ("autoDiv", 6); setP ("revMix", 0.f); setP ("dlyMix", 0.f);
        setP ("m1Target", cutIdx); setP ("m1Pol", 0.f);
        setN ("m1Range", proc.apvts.getParameter ("cutoffHz")->convertTo0to1 (6400.f));
        AudioBuffer<float> rec (2, beat * 4); rec.clear();
        for (int pos = 0; pos < beat * 4; pos += block)
        {
            AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer m;
            if (pos == 0) m.addEvent (MidiMessage::noteOn (1, 62, (uint8) 110), 0);
            proc.processBlock (buf, m);
            rec.copyFrom (0, pos, buf, 0, 0, jmin (block, beat * 4 - pos));
        }
        auto hiShare = [&rec] (int a, int n)
        {
            double hi = 0, all = 0; float prev = 0.f;
            for (int i = a; i < a + n; ++i) { const float x = rec.getSample (0, i); const float d = x - prev; prev = x; hi += d * d; all += x * x; }
            return hi / (all + 1e-12);
        };
        const double early = hiShare (beat / 4, beat / 2), late = hiShare (beat * 3, beat / 2);
        const bool good = late > early * 2.0;
        printf ("MOD 1 -> cutoff (rise): brightness %.4f -> %.4f %s\n", early, late, good ? "OK" : "FAIL");
        ok &= good;
        proc.setCurrentProgram (0);
    }
    {   // old presets / projects: "Depth" (autoOct) becomes MOD 1 -> CUTOFF
        int owner6 = -1;
        for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == "SCREECH - Owner 6") owner6 = i;
        proc.setCurrentProgram (owner6);
        const bool good = proc.modTarget (0) == (int) cutIdx && std::abs (proc.apvts.getRawParameterValue ("autoOct")->load()) < 1e-4f;
        // and an old state with cutoff 2 kHz, wobble, depth -2 oct -> MOD 1 -> cutoff, "-", range end 500 Hz
        proc.setCurrentProgram (1);
        setP ("cutoffHz", 2000.f); setP ("autoShape", 4); setP ("autoOct", -2.f); setP ("m1Target", 0.f);
        proc.migrateLegacyAuto();
        const float endHz = proc.apvts.getParameter ("cutoffHz")->convertFrom0to1 (proc.apvts.getParameter ("m1Range")->getValue());
        const bool good2 = proc.modTarget (0) == (int) cutIdx && roundToInt (proc.apvts.getRawParameterValue ("m1Pol")->load()) == 1 && std::abs (endHz - 500.f) < 5.f;
        printf ("old cutoff automation -> MOD 1: %s (end %.0f Hz)\n", good && good2 ? "OK" : "FAIL", endHz);
        ok &= good && good2;
    }
    {   // MOD 2 CUSTOM on the formant + MOD 1 STEPS on the drive, both on one knob too: no NaN, sound stays sane
        proc.setCurrentProgram (11);
        proc.setCustom (1, doomsday::mod::Shape::make ({ { 0.f, 0.f }, { 250.f, 1.f, -0.6f }, { 500.f, 0.2f }, { 500.f, 0.9f }, { 1000.f, 0.f } }));
        setP ("m2Shape", (float) alpha::mod::Custom); setP ("m2Div", 4); setP ("m2Target", (float) alpha::mod::targetIndex ("formHz")); setP ("m2Pol", 1.f); setN ("m2Range", 0.1f);
        setP ("autoShape", (float) alpha::mod::Steps); setP ("autoDiv", 2); setP ("m1Target", (float) alpha::mod::targetIndex ("formHz")); setP ("m1Pol", 0.f); setN ("m1Range", 1.f);
        float peak = 0.f; bool nan = false; float lo = 2.f, hi = -1.f;
        for (int b = 0; b < 200; ++b)
        {
            AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer m;
            if (b % 25 == 0) m.addEvent (MidiMessage::noteOn (1, 62 + (b / 25) % 3 * 5, (uint8) 110), 7);
            if (b % 25 == 20) m.addEvent (MidiMessage::allNotesOff (1), 0);
            proc.processBlock (buf, m);
            for (int i = 0; i < block; ++i) { const float v = buf.getSample (0, i); nan |= ! std::isfinite (v); peak = jmax (peak, std::abs (v)); }
            lo = jmin (lo, proc.modKnob01 (1)); hi = jmax (hi, proc.modKnob01 (1));
        }
        const bool good = ! nan && peak > 0.02f && peak < 1.5f && hi - lo > 0.2f;
        printf ("MOD 2 custom + MOD 1 steps on one knob: peak %.3f, knob moves %.2f .. %.2f %s\n", peak, lo, hi, good ? "OK" : "FAIL");
        ok &= good;
        // the drawn shape survives the project state
        MemoryBlock mb; proc.getStateInformation (mb);
        const auto before = proc.getCustom (1);
        proc.setCurrentProgram (0);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool same = proc.getCustom (1) == before && roundToInt (proc.apvts.getRawParameterValue ("m2Shape")->load()) == alpha::mod::Custom;
        printf ("custom shape in the project: %s\n", same ? "OK" : "FAIL"); ok &= same;
    }
    {   // user preset save / load
        proc.setCurrentProgram (3);
        const float a = proc.apvts.getRawParameterValue ("formHz")->load();
        const bool saved = proc.saveUserPreset ("__alpha_test");
        proc.setCurrentProgram (1);
        auto f = AlphaProcessor::userFolder().getChildFile ("__alpha_test.sapreset");
        const bool loaded = proc.loadUserPreset (f);
        const bool same = std::abs (proc.apvts.getRawParameterValue ("formHz")->load() - a) < 0.5f;
        printf ("user preset save/load: %s\n", saved && loaded && same ? "OK" : "FAIL");
        ok &= saved && loaded && same;
        f.deleteFile();
    }
    {   // state round trip
        proc.setCurrentProgram (2);
        MemoryBlock mb; proc.getStateInformation (mb);
        const float a = proc.apvts.getRawParameterValue ("scoopSt")->load();
        proc.setCurrentProgram (0);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool same = std::abs (proc.apvts.getRawParameterValue ("scoopSt")->load() - a) < 1e-4f;
        printf ("state round-trip: %s\n", same ? "OK" : "FAIL"); ok &= same;
    }
    {
        proc.setCurrentProgram (1);
        setP ("cutoffHz", 1500.f); setP ("reso", 0.4f); setP ("fortune", 0.45f);
        proc.setCustom (0, doomsday::mod::Shape::make ({ { 0.f, 0.f }, { 125.f, 1.f, 0.5f }, { 375.f, 0.3f }, { 500.f, 0.8f, -0.5f }, { 750.f, 0.1f }, { 1000.f, 0.6f } }));
        setP ("autoShape", (float) alpha::mod::Custom); setP ("autoDiv", 6); setP ("m1Target", cutIdx); setN ("m1Range", 0.9f);
        setP ("m2Shape", (float) alpha::mod::Wobble); setP ("m2Target", (float) alpha::mod::targetIndex ("formHz")); setP ("m2Pol", 1.f); setN ("m2Range", 0.2f);
        for (int b = 0; b < 30; ++b) { AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer m; if (b == 0) m.addEvent (MidiMessage::noteOn (1, 62, (uint8) 110), 0); proc.processBlock (buf, m); }
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        auto& canvas = dynamic_cast<AlphaEditor&> (*ed).canvas;
        auto snap = [&] (const char* name)
        {
            canvas.refreshNow();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = outDir.getChildFile (name); f.deleteFile();
            FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        };
        snap ("scream_alpha_gui.png");
        canvas.beginAssign (1);
        snap ("scream_alpha_gui_assign.png");
        canvas.endAssign();
        canvas.modPanel().select (1);
        snap ("scream_alpha_gui_mod2.png");
    }
    printf (ok ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
