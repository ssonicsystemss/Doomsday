// Offline test for Ancient Drums v2: presets, layers, offset, envelopes, reverse, FORGE,
// user samples, state + presets round trips, randomize, GUI snapshots.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;

static const double sr = 48000.0;
static const int block = 512;
static bool ok = true;

struct Stats { float peak = 0.f; double rms = 0.0; bool nan = false; int firstAbove = -1; int peakAt = 0; double highRatio = 0.0; };

static Stats renderHits (AncientProcessor& proc, AudioBuffer<float>* dest, int destOffset, int hits = 4, int note = 60)
{
    const int beat = (int) (sr * 60.0 / 150.0);
    const int total = beat * hits;
    Stats st; double sum = 0.0;
    std::vector<float> first;
    for (int pos = 0; pos < total; pos += block)
    {
        AudioBuffer<float> buf (2, block); buf.clear();
        MidiBuffer midi;
        for (int h = 0; h < hits; ++h)
        {
            const int at = h * beat;
            if (at >= pos && at < pos + block) midi.addEvent (MidiMessage::noteOn (1, note + (h == 3 ? 5 : 0), (uint8) 120), at - pos);
        }
        proc.processBlock (buf, midi);
        for (int i = 0; i < block; ++i)
        {
            const float v = buf.getSample (0, i);
            if (! std::isfinite (v)) st.nan = true;
            if (std::abs (v) > st.peak) { st.peak = std::abs (v); st.peakAt = pos + i; }
            if (st.firstAbove < 0 && std::abs (v) > 0.01f) st.firstAbove = pos + i;
            sum += v * v;
            if (pos + i < beat) first.push_back (v);
        }
        if (dest != nullptr && destOffset + pos + block <= dest->getNumSamples())
            for (int ch = 0; ch < 2; ++ch) dest->copyFrom (ch, destOffset + pos, buf, ch, 0, block);
    }
    st.rms = std::sqrt (sum / total);
    // share of energy above 800 Hz in the tail of the first hit (60..300 ms) via a one-pole split
    double hi = 0.0, all = 0.0; float lp = 0.f;
    const float a = 1.f - std::exp (-MathConstants<float>::twoPi * 800.f / (float) sr);
    for (size_t i = 0; i < first.size(); ++i)
    {
        lp += a * (first[i] - lp);
        if (i > (size_t) (0.06 * sr) && i < (size_t) (0.30 * sr)) { const float h = first[i] - lp; hi += h * h; all += first[i] * first[i]; }
    }
    st.highRatio = all > 0.0 ? hi / all : 0.0;
    return st;
}

static void set (AncientProcessor& p, const String& id, float v)
{
    if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}
static void check (bool cond, const char* what, const String& extra = {})
{
    printf ("%-52s %s %s\n", what, cond ? "OK" : "FAIL", extra.toRawUTF8());
    ok &= cond;
}
static void silence (AncientProcessor& p)   // let every tail finish
{
    MidiBuffer none;
    for (int b = 0; b < 300; ++b) { AudioBuffer<float> buf (2, block); buf.clear(); p.processBlock (buf, none); }
}
static void soloKick (AncientProcessor& p, bool forge)
{
    p.setCurrentProgram (0);
    set (p, ParamIDs::id (Punch, ParamIDs::on), 0);
    set (p, ParamIDs::id (Crunch, ParamIDs::on), 0);
    set (p, ParamIDs::fOn, forge ? 1.f : 0.f);
    silence (p);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir = argc > 1 ? File (argv[1]) : File::getCurrentWorkingDirectory();

    AncientProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    const int beat = (int) (sr * 60.0 / 150.0);

    // tuning mode: RenderTest <dir> tune <param=value>... -> renders "Ancient Raw" with overrides to tune.wav
    if (argc > 2 && String (argv[2]) == "tune")
    {
        proc.setCurrentProgram (0);
        for (int a = 3; a < argc; ++a)
        {
            const auto kv = StringArray::fromTokens (argv[a], "=", "");
            if (kv.size() == 2) set (proc, kv[0], kv[1].getFloatValue());
        }
        AudioBuffer<float> t (2, beat * 4); t.clear();
        renderHits (proc, &t, 0);
        auto f = outDir.getChildFile ("tune.wav"); f.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (t, 0, t.getNumSamples());
        return 0;
    }

    AudioBuffer<float> demo (2, beat * 4 * proc.getNumPrograms());
    demo.clear();

    for (int prog = 0; prog < proc.getNumPrograms(); ++prog)
    {
        proc.setCurrentProgram (prog);
        auto st = renderHits (proc, &demo, prog * beat * 4);
        const bool pass = ! st.nan && st.peak > 0.1f && st.peak <= 1.0f;
        ok &= pass;
        printf ("%-20s peak %.3f  rms %.3f  >800Hz %4.1f%%  %s\n", proc.getProgramName (prog).toRawUTF8(), st.peak, st.rms,
                st.highRatio * 100.0, pass ? "OK" : "FAIL");
    }

    // ---- every library sample on its own
    for (int l = 0; l < NumSampleLayers; ++l)
        for (int s = 0; s < Library::perLayer; ++s)
        {
            proc.setCurrentProgram (0);
            for (int o = 0; o < NumSampleLayers; ++o) set (proc, ParamIDs::id (o, ParamIDs::on), o == l);
            set (proc, ParamIDs::id (l, ParamIDs::src), (float) s);
            auto st = renderHits (proc, nullptr, 0, 1);
            if (st.nan || st.peak < 0.05f) { printf ("library layer %d sample %d FAIL (%.3f)\n", l, s, st.peak); ok = false; }
        }
    check (true, "library samples x24 render");

    // ---- all layers off -> silence
    proc.setCurrentProgram (0);
    for (int l = 0; l < NumSampleLayers; ++l) set (proc, ParamIDs::id (l, ParamIDs::on), 0);
    silence (proc);
    check (renderHits (proc, nullptr, 0, 2).peak < 0.001f, "all layers off -> silence");

    // ---- offset moves the onset
    soloKick (proc, false);
    const int onset0 = renderHits (proc, nullptr, 0, 1).firstAbove;
    set (proc, ParamIDs::id (Kick, ParamIDs::offset), 100.f);
    silence (proc);
    const int onset100 = renderHits (proc, nullptr, 0, 1).firstAbove;
    check (std::abs ((onset100 - onset0) - 4800) < 60, "kick offset +100 ms moves the onset by 4800 samples",
           "(" + String (onset100 - onset0) + ")");

    // ---- big offsets: nothing blocks a long drag
    soloKick (proc, false);
    set (proc, ParamIDs::id (Kick, ParamIDs::offset), 1500.f);
    silence (proc);
    {
        int onset = -1;
        MidiBuffer m; m.addEvent (MidiMessage::noteOn (1, 60, (uint8) 120), 0);
        for (int b = 0; b < (int) (2.0 * sr) / block && onset < 0; ++b)
        {
            AudioBuffer<float> buf (2, block); buf.clear(); proc.processBlock (buf, m); m.clear();
            for (int i = 0; i < block; ++i) if (std::abs (buf.getSample (0, i)) > 0.01f) { onset = b * block + i; break; }
        }
        check (std::abs ((onset - onset0) - 72000) < 60, "kick offset +1500 ms", "(" + String (onset - onset0) + ")");
    }
    set (proc, ParamIDs::id (Kick, ParamIDs::offset), -2000.f);
    check (std::abs (proc.apvts.getRawParameterValue (ParamIDs::id (Kick, ParamIDs::offset))->load() + 2000.f) < 0.01f, "offset reaches -2000 ms");

    // ---- negative offset: the envelope starts where the layer becomes audible
    soloKick (proc, false);
    set (proc, ParamIDs::id (Kick, ParamIDs::offset), -50.f);
    {
        Envelope e; e.points = { { 0.f, 1.f }, { 60.f, 1.f }, { 80.f, 0.f } };
        proc.setEnvelope (Kick, e);
    }
    silence (proc);
    {
        float early = 0.f, late = 0.f;
        MidiBuffer m; m.addEvent (MidiMessage::noteOn (1, 60, (uint8) 120), 0);
        for (int b = 0; b < 40; ++b)
        {
            AudioBuffer<float> buf (2, block); buf.clear(); proc.processBlock (buf, m); m.clear();
            const int t0 = b * block;
            if (t0 < (int) (0.05 * sr)) early = jmax (early, buf.getMagnitude (0, 0, block));
            if (t0 > (int) (0.09 * sr)) late = jmax (late, buf.getMagnitude (0, 0, block));
        }
        check (early > 0.05f && late < 0.01f, "negative offset: envelope runs from the audible start",
               "(early " + String (early, 3) + ", late " + String (late, 4) + ")");
    }
    {
        Envelope e; e.points = { { 35.f, 0.4f }, { 10.f, 1.f }, { 200.f, 0.f } };
        proc.setEnvelope (Kick, e);
        auto got = proc.getEnvelope (Kick);
        check (got->points.front().ms == 0.f && std::abs (got->points.front().gain - 1.f) < 0.001f, "first envelope point is pinned at 0 ms");
    }
    set (proc, ParamIDs::id (Kick, ParamIDs::offset), 0.f);

    // ---- envelope: a short one stops the kick, a zero one silences it
    soloKick (proc, false);
    Envelope shortEnv; shortEnv.points = { { 0.f, 1.f }, { 80.f, 1.f }, { 100.f, 0.f } };
    proc.setEnvelope (Kick, shortEnv);
    {
        float late = 0.f;
        MidiBuffer m; m.addEvent (MidiMessage::noteOn (1, 60, (uint8) 120), 0);
        for (int b = 0; b < 40; ++b)
        {
            AudioBuffer<float> buf (2, block); buf.clear(); proc.processBlock (buf, m); m.clear();
            if (b * block > (int) (0.12 * sr)) late = jmax (late, buf.getMagnitude (0, 0, block));
        }
        check (late < 0.01f, "envelope ending at 100 ms ends the kick", "(after 120 ms: " + String (late, 4) + ")");
    }
    Envelope zero; zero.points = { { 0.f, 0.f }, { 500.f, 0.f } };
    proc.setEnvelope (Kick, zero);
    silence (proc);
    check (renderHits (proc, nullptr, 0, 1).peak < 0.001f, "all-zero envelope silences the layer");

    // ---- reverse: the loudest part moves to the end
    soloKick (proc, false);
    const int peakFwd = renderHits (proc, nullptr, 0, 1).peakAt;
    set (proc, ParamIDs::id (Kick, ParamIDs::rev), 1);
    silence (proc);
    const int peakRev = renderHits (proc, nullptr, 0, 1).peakAt;
    check (peakRev > peakFwd + (int) (0.2 * sr), "reverse moves the peak towards the end",
           "(" + String (peakFwd) + " -> " + String (peakRev) + ")");

    // ---- FORGE adds the crunch: much more energy above 800 Hz
    soloKick (proc, false);
    const double hiOff = renderHits (proc, nullptr, 0, 1).highRatio;
    soloKick (proc, true);
    const double hiOn = renderHits (proc, nullptr, 0, 1).highRatio;
    check (hiOn > hiOff * 1.5 && hiOn > 0.03, "FORGE adds upper harmonics",
           "(>800 Hz: " + String (hiOff * 100.0, 2) + "% -> " + String (hiOn * 100.0, 2) + "%)");

    // ---- FORGE MIX at 0 sounds like FORGE off
    soloKick (proc, false);
    {
        AudioBuffer<float> a (2, beat); a.clear();
        renderHits (proc, &a, 0, 1);
        soloKick (proc, true);
        set (proc, ParamIDs::fMix, 0.f);
        silence (proc);
        AudioBuffer<float> b (2, beat); b.clear();
        renderHits (proc, &b, 0, 1);
        double best = 1e9; int bestLag = 0;
        for (int lag = 0; lag < 24; ++lag)     // the FORGE path carries the oversampling latency
        {
            double diff = 0.0, ref = 0.0;
            for (int i = 0; i + lag < beat; ++i) { const double d = a.getSample (0, i) - b.getSample (0, i + lag); diff += d * d; ref += (double) a.getSample (0, i) * a.getSample (0, i); }
            if (diff / ref < best) { best = diff / ref; bestLag = lag; }
        }
        check (best < 0.01, "FORGE mix 0% = clean signal", "(err " + String (best * 100.0, 3) + "%, latency " + String (bestLag) + ")");
    }

    // ---- forge tails die out
    {
        proc.setCurrentProgram (5);   // Monolith Rawstyle
        renderHits (proc, nullptr, 0, 1);
        MidiBuffer none; float tail = 0.f;
        for (int b = 0; b < (int) (sr * 2.0) / block; ++b) { AudioBuffer<float> buf (2, block); buf.clear(); proc.processBlock (buf, none); }
        for (int b = 0; b < 20; ++b) { AudioBuffer<float> buf (2, block); buf.clear(); proc.processBlock (buf, none); tail = jmax (tail, buf.getMagnitude (0, 0, block)); }
        check (tail < 0.002f, "tails silent 2.4 s after the hit", "(" + String (tail, 5) + ")");
    }

    // ---- user sample + state round trip (sample path + envelope)
    auto userWav = outDir.getChildFile ("user_crunch.wav");
    {
        AudioBuffer<float> b (1, 9600);
        Random r (3);
        for (int i = 0; i < 9600; ++i) b.setSample (0, i, std::tanh (8.f * std::sin ((float) i * 0.06f) + r.nextFloat() - 0.5f) * std::exp (-(float) i / 2500.f) * 0.8f);
        userWav.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (userWav), 44100.0, 1, 24, {}, 0));
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
    proc.setCurrentProgram (0);
    const bool loaded = proc.loadUserSample (Crunch, userWav);
    auto us = proc.getLayerSample (Crunch);
    check (loaded && us != nullptr && us->isUser && us->data.size() == 9600, "user sample load");
    check (std::abs (proc.getEnvelope (Crunch)->endMs() - 217.7f) < 1.f, "user sample gets an envelope over its length",
           "(" + String (proc.getEnvelope (Crunch)->endMs(), 1) + " ms)");
    Envelope custom; custom.points = { { 0.f, 0.3f }, { 40.f, 1.f }, { 150.f, 0.5f }, { 260.f, 0.f } };
    proc.setEnvelope (Kick, custom);
    set (proc, ParamIDs::id (Kick, ParamIDs::offset), 12.5f);
    MemoryBlock state;
    proc.getStateInformation (state);
    {
        AncientProcessor proc2;
        proc2.setPlayConfigDetails (0, 2, sr, block);
        proc2.prepareToPlay (sr, block);
        proc2.setStateInformation (state.getData(), (int) state.getSize());
        auto us2 = proc2.getLayerSample (Crunch);
        auto e2 = proc2.getEnvelope (Kick);
        check (us2 != nullptr && us2->isUser && us2->path == userWav.getFullPathName(), "state keeps the user sample");
        check (e2->points.size() == 4 && std::abs (e2->points[2].gain - 0.5f) < 0.001f, "state keeps the envelope");
        check (std::abs (proc2.apvts.getRawParameterValue (ParamIDs::id (Kick, ParamIDs::offset))->load() - 12.5f) < 0.01f, "state keeps the offset");
    }

    // ---- randomize
    for (int i = 0; i < 30; ++i)
    {
        proc.randomize();
        auto st = renderHits (proc, nullptr, 0, 1);
        if (st.nan || st.peak < 0.05f || st.peak > 1.0f) { printf ("randomize #%d FAIL peak %.3f\n", i, st.peak); ok = false; }
    }
    check (true, "randomize x30");

    // ---- user preset save / load keeps envelopes
    {
        proc.setEnvelope (Punch, custom);
        const bool saved = proc.saveUserPreset ("__test_kick");
        auto f = AncientProcessor::getUserPresetFolder().getChildFile ("__test_kick.adp");
        proc.setCurrentProgram (3);
        const bool back = proc.loadUserPreset (f);
        check (saved && back && proc.getEnvelope (Punch)->points.size() == 4, "user preset save/load with envelopes");
        f.deleteFile();
    }

    // ---- demo wav
    {
        auto f = outDir.getChildFile ("ancient_v2_demo.wav");
        f.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (demo, 0, demo.getNumSamples());
    }

    // ---- GUI snapshots
    proc.setCurrentProgram (5);
    proc.setSelectedLayer (Kick);
    proc.setZoomMs (1000.f);
    set (proc, ParamIDs::id (Crunch, ParamIDs::offset), 6.f);
    {
        Envelope e; e.points = { { 0.f, 1.f }, { 120.f, 0.85f }, { 600.f, 0.7f }, { 820.f, 0.f } };
        proc.setEnvelope (Kick, e);
    }
    {
        MidiBuffer m; m.addEvent (MidiMessage::noteOn (1, 60, (uint8) 120), 0);
        for (int b = 0; b < 30; ++b) { AudioBuffer<float> buf (2, block); buf.clear(); proc.processBlock (buf, m); m.clear(); }
    }
    auto snap = [&] (const char* name)
    {
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());   // a fresh editor shows the current selection
        ed->setSize (1360, 780);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile (name);
        f.deleteFile();
        FileOutputStream os (f);
        PNGImageFormat().writeImageToStream (img, os);
    };
    snap ("ancient_gui.png");
    proc.setSelectedLayer (Crunch);
    proc.setZoomMs (500.f);
    set (proc, ParamIDs::id (Kick, ParamIDs::rev), 1);
    snap ("ancient_gui_crunch.png");
    proc.setSelectedLayer (Kick);
    proc.setZoomMs (1000.f);
    set (proc, ParamIDs::id (Kick, ParamIDs::offset), 1450.f);
    snap ("ancient_gui_offscreen.png");

    printf (ok ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
