// Offline render test: plays a hardstyle screech riff through every preset,
// writes a WAV, checks for NaN / silence / clipping and snapshots the GUI.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir = argc > 1 ? File (argv[1]) : File::getCurrentWorkingDirectory();

    const double sr = 48000.0;
    const int block = 512;
    ProphecyProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    // held-note renders for sound matching:
    //   RenderTest <dir> held <note> [prog=N] [name=file] [paramId=value ...]
    //   no prog -> every factory preset to held_<index>.wav; 1.6 s note + 0.4 s tail
    if (argc > 3 && String (argv[2]) == "held")
    {
        const int note = String (argv[3]).getIntValue();
        int only = -2; String name;
        StringPairArray sets;
        for (int i = 4; i < argc; ++i)
        {
            auto kv = StringArray::fromTokens (argv[i], "=", "");
            if (kv.size() != 2) continue;
            if (kv[0] == "prog") only = kv[1].getIntValue();
            else if (kv[0] == "name") name = kv[1];
            else sets.set (kv[0], kv[1]);
        }
        auto renderOne = [&] (int prog, const File& f)
        {
            if (prog >= 0) proc.setCurrentProgram (prog);
            else for (auto* prm : proc.getParameters())          // prog=-1: plain defaults
                if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm)) rp->setValueNotifyingHost (rp->getDefaultValue());
            for (auto& k : sets.getAllKeys())
                if (auto* prm = proc.apvts.getParameter (k)) prm->setValueNotifyingHost (prm->convertTo0to1 (sets[k].getFloatValue()));
            { AudioBuffer<float> b (2, block); MidiBuffer m; m.addEvent (MidiMessage::allNotesOff (1), 0);
              for (int i = 0; i < 200; ++i) { b.clear(); proc.processBlock (b, m); m.clear(); } }
            const double holdS = sets.containsKey ("hold") ? sets["hold"].getDoubleValue() : 1.6;
            const int total = (int) (sr * (holdS + 0.4)), offAt = (int) (sr * holdS);
            AudioBuffer<float> out (2, total); out.clear();
            for (int pos = 0; pos < total; pos += block)
            {
                AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer midi;
                if (pos == 0) midi.addEvent (MidiMessage::noteOn (1, note, (uint8) 110), 0);
                if (offAt >= pos && offAt < pos + block) midi.addEvent (MidiMessage::noteOff (1, note), offAt - pos);
                proc.processBlock (buf, midi);
                for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, jmin (block, total - pos));
            }
            f.deleteFile();
            WavAudioFormat wav;
            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
            if (w) w->writeFromAudioSampleBuffer (out, 0, total);
        };
        if (only >= -1) renderOne (only, outDir.getChildFile ((name.isEmpty() ? "held_" + String (only) : name) + ".wav"));
        else for (int prog = 0; prog < proc.getNumPrograms(); ++prog)
            renderOne (prog, outDir.getChildFile (String::formatted ("held_%02d_", prog) + proc.getProgramName (prog).replaceCharacters (" '", "__") + ".wav"));
        return 0;
    }

    // riff @150 BPM: 1/8 notes, legato slides (beat = 19200 samples)
    const int beat = (int) (sr * 60.0 / 150.0);
    struct N { int startEighth, lenEighth, note; };
    const std::vector<N> riff { { 0, 2, 62 }, { 2, 1, 65 }, { 3, 2, 69 }, { 4, 1, 67 }, { 5, 3, 74 } };
    const int barSamples = beat * 4;

    AudioBuffer<float> all (2, barSamples * 2 * proc.getNumPrograms());
    all.clear();
    bool ok = true;

    for (int prog = 0; prog < proc.getNumPrograms(); ++prog)
    {
        proc.setCurrentProgram (prog);
        double sumSq = 0.0; float peak = 0.f; bool nan = false;

        for (int pos = 0; pos < barSamples * 2; pos += block)
        {
            AudioBuffer<float> buf (2, block);
            buf.clear();
            MidiBuffer midi;
            for (auto& n : riff)
            {
                const int on = n.startEighth * beat / 2, off = (n.startEighth + n.lenEighth) * beat / 2 + beat / 16;
                if (on >= pos && on < pos + block)   midi.addEvent (MidiMessage::noteOn (1, n.note, (uint8) 110), on - pos);
                if (off >= pos && off < pos + block) midi.addEvent (MidiMessage::noteOff (1, n.note), off - pos);
            }
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* d = buf.getReadPointer (ch);
                for (int i = 0; i < block; ++i)
                {
                    if (! std::isfinite (d[i])) nan = true;
                    peak = jmax (peak, std::abs (d[i]));
                    sumSq += d[i] * d[i];
                }
                const int dst = prog * barSamples * 2 + pos;
                all.copyFrom (ch, dst, buf, ch, 0, jmin (block, all.getNumSamples() - dst));
            }
        }
        const double rms = std::sqrt (sumSq / (barSamples * 4.0));
        const bool pass = ! nan && peak > 0.05f && peak <= 1.0f;
        ok &= pass;
        printf ("%-20s peak %.3f  rms %.3f (%.1f dBFS)  %s\n", proc.getProgramName (prog).toRawUTF8(), peak, rms,
                Decibels::gainToDecibels ((float) rms), pass ? "OK" : (nan ? "NAN!" : "FAIL"));
    }

    auto setP = [&proc] (const String& id, float v)
    {
        if (auto* prm = proc.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    auto renderRiff = [&] (int bars) -> float   // returns peak, -1 on NaN
    {
        float pk = 0.f;
        for (int pos = 0; pos < barSamples * bars; pos += block)
        {
            AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer midi;
            for (auto& n : riff)
            {
                const int on = n.startEighth * beat / 2, off = (n.startEighth + n.lenEighth) * beat / 2 + beat / 16;
                if (on >= pos && on < pos + block)   midi.addEvent (MidiMessage::noteOn (1, n.note, (uint8) 110), on - pos);
                if (off >= pos && off < pos + block) midi.addEvent (MidiMessage::noteOff (1, n.note), off - pos);
            }
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (! std::isfinite (v)) return -1.f;
                    pk = jmax (pk, std::abs (v));
                }
        }
        // release everything and let it ring out
        for (int b = 0; b < 400; ++b)
        {
            AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer m;
            if (b == 0) m.addEvent (MidiMessage::allNotesOff (1), 0);
            proc.processBlock (buf, m);
        }
        return pk;
    };

    // ---- embedded presets carry exactly the values from their .pos files
    {
        int idx = -1;
        for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == "Devil's Scream") idx = i;
        proc.setCurrentProgram (jmax (0, idx));
        auto v = [&proc] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
        const bool same = idx >= 0 && std::abs (v ("cutoff") - 2248.398f) < 0.5f && std::abs (v ("penv") + 20.9f) < 0.05f
                          && v ("o2_on") < 0.5f && v ("dly_on") < 0.5f && std::abs (v ("o2_bite") - 799.8f) < 0.5f;
        printf ("embedded preset values (Devil's Scream): %s\n", same ? "OK" : "FAIL");
        ok &= same;
    }

    // ---- states saved by v1.0 (no OSC2 / FX switches) load with OSC2 off and all FX on
    {
        proc.setCurrentProgram (0);
        MemoryBlock mb; proc.getStateInformation (mb);
        auto xml = AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
        for (int i = xml->getNumChildElements(); --i >= 0;)
        {
            const auto id = xml->getChildElement (i)->getStringAttribute ("id");
            if (id.startsWith ("o1_") || id.startsWith ("o2_") || id == "dly_on" || id == "rev_on")
                xml->removeChildElement (xml->getChildElement (i), true);
        }
        MemoryBlock old; AudioProcessor::copyXmlToBinary (*xml, old);
        setP ("o2_on", 1); setP ("o1_dist_on", 0); setP ("rev_on", 0); setP ("o1_on", 0);
        proc.setStateInformation (old.getData(), (int) old.getSize());
        auto v = [&proc] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
        const bool compat = v ("o2_on") < 0.5f && v ("o1_on") > 0.5f && v ("o1_dist_on") > 0.5f && v ("rev_on") > 0.5f;
        printf ("v1 project state -> osc1 on, osc2 off, fx on: %s\n", compat ? "OK" : "FAIL");
        ok &= compat;
    }

    // ---- OSC2 alone, both off, FX switches
    {
        proc.setCurrentProgram (0);
        setP ("o1_on", 0); setP ("o2_on", 1);
        const float pk2 = renderRiff (1);
        printf ("osc2 only -> peak %.3f %s\n", pk2, pk2 > 0.05f ? "OK" : "FAIL");
        ok &= pk2 > 0.05f;

        setP ("o2_on", 0); setP ("dly_on", 0); setP ("rev_on", 0);
        const float pk0 = renderRiff (1);
        printf ("both oscillators off -> peak %.5f %s\n", pk0, pk0 >= 0.f && pk0 < 1e-3f ? "OK" : "FAIL");
        ok &= pk0 >= 0.f && pk0 < 1e-3f;   // -60 dB: allows the last reverb tail of the previous test

        proc.setCurrentProgram (0);
        const char* sw[] = { "o1_dist_on", "o1_filt_on", "o1_lfo_on", "dly_on", "rev_on" };
        for (auto* id : sw)
        {
            setP (id, 0);
            const float pk = renderRiff (1);
            printf ("%-12s off -> peak %.3f %s\n", id, pk, pk > 0.02f ? "OK" : "FAIL");
            ok &= pk > 0.02f;
            setP (id, 1);
        }
    }

    // state round-trip
    MemoryBlock state;
    proc.setCurrentProgram (1);   // Zaagtand '04 (glide 255.8 ms)
    proc.getStateInformation (state);
    proc.setCurrentProgram (0);
    proc.setStateInformation (state.getData(), (int) state.getSize());
    const bool stateOk = std::abs (proc.apvts.getRawParameterValue ("glide")->load() - 255.795f) < 0.5f;
    printf ("state round-trip: %s\n", stateOk ? "OK" : "FAIL");
    ok &= stateOk;

    // randomize + user preset save/load round-trip
    for (int i = 0; i < 20; ++i)
    {
        proc.randomize();
        AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer midi;
        midi.addEvent (MidiMessage::noteOn (1, 60, (uint8) 110), 0);
        float pk = 0.f; bool bad = false;
        for (int b = 0; b < 60; ++b)
        {
            buf.clear(); proc.processBlock (buf, midi); midi.clear();
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < block; ++s) { auto v = buf.getSample (ch, s); bad |= ! std::isfinite (v); pk = jmax (pk, std::abs (v)); }
        }
        MidiBuffer off; off.addEvent (MidiMessage::allNotesOff (1), 0); buf.clear(); proc.processBlock (buf, off);
        if (bad || pk < 0.02f) { printf ("randomize #%d FAIL (peak %.3f)\n", i, pk); ok = false; }
    }
    printf ("randomize x20: done\n");
    {
        const float cut = proc.apvts.getRawParameterValue ("cutoff")->load();
        const bool saved = proc.saveUserPreset ("__test_preset");
        auto f = ProphecyProcessor::getUserPresetFolder().getChildFile ("__test_preset.pos");
        proc.setCurrentProgram (0);
        const bool loaded = proc.loadUserPreset (f);
        const bool same = std::abs (proc.apvts.getRawParameterValue ("cutoff")->load() - cut) < 1.f
                          && proc.getPresetName() == "__test_preset";
        printf ("user preset save/load: %s\n", (saved && loaded && same) ? "OK" : "FAIL");
        ok &= saved && loaded && same;
        f.deleteFile();
    }

    // write wav
    {
        auto f = outDir.getChildFile ("prophecy_demo.wav");
        f.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
    }

    // GUI snapshot
    proc.setCurrentProgram (3);   // Twin Prophecy (both oscillators)
    for (int b = 0; b < 40; ++b)
    {
        AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer midi;
        if (b == 0) midi.addEvent (MidiMessage::noteOn (1, 62, (uint8) 110), 0);
        proc.processBlock (buf, midi);
    }
    {
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (1360, 800);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile ("prophecy_gui.png");
        f.deleteFile();
        FileOutputStream os (f);
        PNGImageFormat().writeImageToStream (img, os);

        // second shot: FILTER tab on OSC 1, LFO tab on OSC 2
        std::function<void (Component&, int&)> walk = [&walk] (Component& c, int& n)
        {
            if (auto* t = dynamic_cast<FxTabs*> (&c)) t->selectTab (n++ == 0 ? 1 : 2);
            for (auto* ch : c.getChildren()) walk (*ch, n);
        };
        int n = 0; walk (*ed, n);
        auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f2 = outDir.getChildFile ("prophecy_gui_tabs.png");
        f2.deleteFile();
        FileOutputStream os2 (f2);
        PNGImageFormat().writeImageToStream (img2, os2);
    }

    printf (ok ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
