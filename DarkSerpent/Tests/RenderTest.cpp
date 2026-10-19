// Offline test for Dark Serpent: presets, modes, bend, talk, rage, energy, state, randomize, CPU, GUI, demo + reference renders.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;
static const double sr = 48000.0;
static const int block = 512;
static bool ok = true;

static void check (bool c, const char* what, const String& extra = {}) { printf ("%-50s %s %s\n", what, c ? "OK" : "FAIL", extra.toRawUTF8()); ok &= c; }
static void set (SerpentProcessor& p, const char* id, float v) { if (auto* r = p.apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (v)); }
struct Note { double beat, len; int note; };

static AudioBuffer<float> render (SerpentProcessor& p, const std::vector<Note>& notes, double beats)
{
    const double spb = sr * 60.0 / 150.0;
    const int total = (int) (beats * spb);
    AudioBuffer<float> out (2, total);
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
static void silence (SerpentProcessor& p, double s = 2.0)
{
    MidiBuffer m; m.addEvent (MidiMessage::allNotesOff (1), 0);
    for (int i = 0; i < (int) (s * sr) / block; ++i) { AudioBuffer<float> b (2, block); b.clear(); p.processBlock (b, m); m.clear(); }
}
static float peakOf (const AudioBuffer<float>& b) { return jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples())); }
static bool nan (const AudioBuffer<float>& b) { for (int c = 0; c < 2; ++c) for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) return true; return false; }
// spectral centroid of the mono sum over [a, b)
static double centroid (const AudioBuffer<float>& x, int a, int b)
{
    const int n = 8192; double num = 0, den = 0;
    for (int s = a; s + n <= b; s += n)
    {
        juce::dsp::FFT fft (13);
        std::vector<float> d ((size_t) n * 2, 0.f);
        for (int i = 0; i < n; ++i) d[(size_t) i] = 0.5f * (x.getSample (0, s + i) + x.getSample (1, s + i)) * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) n));
        fft.performFrequencyOnlyForwardTransform (d.data());
        for (int k = 1; k < n / 2; ++k) { const double f = k * sr / n, m = (double) d[(size_t) k] * d[(size_t) k]; num += f * m; den += m; }
    }
    return den > 0 ? num / den : 0;
}
// pitch (Hz) by autocorrelation on a short window starting at 'at'
static double pitchAt (const AudioBuffer<float>& x, int at, int win = 2048)
{
    double best = 0; int lag = 0;
    for (int l = (int) (sr / 1200); l < (int) (sr / 60); ++l)
    {
        double acc = 0;
        for (int i = 0; i < win; ++i) acc += (double) x.getSample (0, at + i) * x.getSample (0, at + i + l);
        if (acc > best) { best = acc; lag = l; }
    }
    return lag > 0 ? sr / lag : 0;
}
static void writeWav (const AudioBuffer<float>& b, const File& f)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static std::vector<Note> leadLine()
{
    const int line[] = { 64, 67, 71, 72, 71, 67, 64, 62, 64, 67, 69, 67, 64, 62, 60, 62 };
    std::vector<Note> v;
    for (int i = 0; i < 16; ++i) v.push_back ({ i * 0.5, i % 4 == 3 ? 0.5 : 0.45, line[i] });   // some legato overlaps
    return v;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File out = argc > 1 ? File (argv[1]) : File::getCurrentWorkingDirectory();
    using namespace ParamIDs;
    SerpentProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);
    const double spb = sr * 60.0 / 150.0;

    // logo animation frames: RenderTest <dir> logo <frames>
    if (argc > 2 && String (argv[2]) == "logo")
    {
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (Canvas::W, Canvas::H);
        const int frames = argc > 3 ? String (argv[3]).getIntValue() : 1;
        std::function<SmokeLogo* (Component&)> find = [&] (Component& c) -> SmokeLogo*
        {
            if (auto* l = dynamic_cast<SmokeLogo*> (&c)) return l;
            for (auto* ch : c.getChildren()) if (auto* l = find (*ch)) return l;
            return nullptr;
        };
        auto* logo = find (*ed);
        for (int i = 0; i < frames; ++i)
        {
            logo->tick (0.25f + 0.2f * (float) std::sin (i * 0.3));
            auto img = ed->createComponentSnapshot ({ 0, 0, Canvas::W, 210 }, true, 1.0f);
            auto f = out.getChildFile (String::formatted ("logo_%04d.png", i)); f.deleteFile();
            FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        }
        return 0;
    }
    // reference-style render: held C (FL "C5" = MIDI 60), 1.7 s, default preset, no delay / reverb
    if (argc > 2 && String (argv[2]) == "ref")
    {
        for (int i = 3; i < argc; ++i) { auto kv = StringArray::fromTokens (argv[i], "=", ""); if (kv.size() == 2) set (proc, kv[0].toRawUTF8(), kv[1].getFloatValue()); }
        set (proc, dlyOn, 0); set (proc, revOn, 0);
        silence (proc, 0.5);
        auto o = render (proc, { { 0.0, 4.0, 60 } }, 4.25);
        writeWav (o, out.getChildFile ("serpent_ref.wav"));
        return 0;
    }

    // ---- every preset plays the lead line
    const int seg = 10;
    AudioBuffer<float> demo (2, (int) (spb * seg * proc.getNumPrograms())); demo.clear();
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        silence (proc, 0.5);
        const bool poly = (int) std::round (proc.apvts.getRawParameterValue (mode)->load()) == 2;
        auto notes = leadLine();
        if (poly) { auto extra = notes; for (auto& n : extra) { n.note -= 12; notes.push_back (n); } }
        auto o = render (proc, notes, seg);
        const float pk = peakOf (o);
        const bool pass = ! nan (o) && pk > 0.05f && pk <= 1.f;
        ok &= pass;
        printf ("%-18s peak %.3f  centroid %5.0f Hz  %s\n", proc.getProgramName (i).toRawUTF8(), pk, centroid (o, 0, (int) (spb * 8)), pass ? "OK" : "FAIL");
        for (int c = 0; c < 2; ++c) demo.copyFrom (c, (int) (i * seg * spb), o, c, 0, o.getNumSamples());
    }
    writeWav (demo, out.getChildFile ("serpent_demo.wav"));

    // ---- play modes
    proc.setCurrentProgram (0);
    silence (proc);
    render (proc, { { 0.0, 2.0, 60 }, { 0.5, 2.0, 64 } }, 1.0);
    check (proc.getEngine().activeVoices() == 1, "LEGATO: overlapping notes = 1 voice");
    set (proc, mode, 2); silence (proc);
    render (proc, { { 0.0, 2.0, 60 }, { 0.0, 2.0, 72 }, { 0.2, 2.0, 67 }, { 0.4, 2.0, 79 } }, 1.0);
    check (proc.getEngine().activeVoices() == 3, "POLY: capped at 3 voices", String (proc.getEngine().activeVoices()));
    set (proc, mode, 0);

    // ---- BEND: starts 5 semitones low, lands on the note
    {
        proc.setCurrentProgram (proc.getNumPrograms() - 1);   // Init: plain saws
        set (proc, coreVoices, 1); set (proc, coreOct, 0); set (proc, glide, 0); set (proc, bendAmt, -5); set (proc, bendTime, 150);
        silence (proc);
        auto o = render (proc, { { 0.0, 1.0, 57 } }, 1.0);
        const double early = pitchAt (o, 200, 900), late = pitchAt (o, (int) (0.3 * sr));
        const double st = 12.0 * std::log2 (early / late);
        check (st < -1.5 && std::abs (late - 220.0) < 4.0, "BEND rises into the note", "(" + String (st, 1) + " st in the first 20 ms, " + String (late, 1) + " Hz later)");
    }

    // ---- TALK: vowel A vs U changes the colour
    {
        proc.setCurrentProgram (0);
        set (proc, moveOn, 0); set (proc, talk, 1.f); set (proc, vowel, 0.f); silence (proc);
        const double cA = centroid (render (proc, { { 0.0, 2.0, 60 } }, 2.0), 4800, (int) (1.5 * spb));
        set (proc, vowel, 1.f); silence (proc);
        const double cU = centroid (render (proc, { { 0.0, 2.0, 60 } }, 2.0), 4800, (int) (1.5 * spb));
        check (cA > cU * 1.3, "TALK: vowel A brighter than U", "(" + String (cA, 0) + " vs " + String (cU, 0) + " Hz)");
    }

    // ---- ENERGY brightens and widens
    {
        proc.setCurrentProgram (0); set (proc, moveOn, 0); silence (proc);
        const double c0 = centroid (render (proc, { { 0.0, 2.0, 60 } }, 2.0), 4800, (int) (1.5 * spb));
        set (proc, energy, 1.f); silence (proc);
        auto o = render (proc, { { 0.0, 2.0, 60 } }, 2.0);
        const double c1 = centroid (o, 4800, (int) (1.5 * spb));
        check (c1 > c0 * 1.15 && ! nan (o) && peakOf (o) <= 1.f, "ENERGY brightens", "(" + String (c0, 0) + " -> " + String (c1, 0) + " Hz)");
    }

    // ---- MOVE makes the brightness move
    {
        proc.setCurrentProgram (0); set (proc, revOn, 0); set (proc, dlyOn, 0); silence (proc);
        auto o = render (proc, { { 0.0, 4.0, 60 } }, 4.0);
        double lo = 1e9, hi = 0;
        for (int s = (int) (0.3 * sr); s + 4096 < (int) (1.5 * sr); s += 2400) { const double c = centroid (o, s, s + 8192); lo = jmin (lo, c); hi = jmax (hi, c); }
        check (hi > lo * 1.3, "MOVE sweeps the brightness", "(" + String (lo, 0) + " .. " + String (hi, 0) + " Hz)");
    }

    // ---- CPU (3-voice poly, 16-voice core)
    {
        proc.setCurrentProgram (7); set (proc, coreVoices, 16); silence (proc);
        std::vector<Note> n; for (int r = 0; r < 4; ++r) for (int k : { 48, 60, 72 }) n.push_back ({ r * 4.0, 3.8, k });
        const auto t0 = Time::getMillisecondCounterHiRes();
        auto o = render (proc, n, 16.0);
        const double ms = Time::getMillisecondCounterHiRes() - t0, audio = o.getNumSamples() / sr * 1000.0;
        printf ("CPU: %.1f s of 3 voices x 16-voice core in %.0f ms (%.1f%% of real time)\n", audio / 1000.0, ms, ms / audio * 100.0);
        check (! nan (o), "heavy render clean");
    }

    // ---- state + presets + randomize
    {
        proc.setCurrentProgram (2); set (proc, vowel, 0.6f);
        MemoryBlock st; proc.getStateInformation (st);
        SerpentProcessor p2; p2.setStateInformation (st.getData(), (int) st.getSize());
        check (std::abs (p2.apvts.getRawParameterValue (vowel)->load() - 0.6f) < 0.01f && p2.currentPresetName() == "Hiss of Chaos", "state round trip");
        set (proc, cutoff, 3333.f);
        const bool saved = proc.saveUserPresetAs ("__test_dsp");
        proc.setCurrentProgram (0);
        const bool back = proc.loadUserPresetFile (proc.io.folder().getChildFile ("__test_dsp.dsp"));
        check (saved && back && std::abs (proc.apvts.getRawParameterValue (cutoff)->load() - 3333.f) < 1.f, "user preset save/load");
        proc.io.folder().getChildFile ("__test_dsp.dsp").deleteFile();
    }
    for (int i = 0; i < 30; ++i)
    {
        proc.randomizeSound(); silence (proc, 0.3);
        auto o = render (proc, leadLine(), 4.0);
        if (nan (o) || peakOf (o) < 0.02f || peakOf (o) > 1.f) { printf ("randomize #%d FAIL %.3f\n", i, peakOf (o)); ok = false; }
    }
    check (true, "randomize x30");

    // ---- GUI snapshot while a note is ringing
    {
        proc.setCurrentProgram (0); silence (proc, 0.3);
        render (proc, { { 0.0, 4.0, 60 } }, 1.0);
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (Canvas::W, Canvas::H);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = out.getChildFile ("serpent_gui.png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
    }
    // ---- Shared blocks taken over from Fortune of Seekers
    {
        namespace sd = doomsday::dsp;
        struct V { bool active = false, held = false, sustained = false; int note = -1; juce::uint64 age = 0; float kill = 1.f, killStep = 0.f; };
        std::array<V, 6> vs;
        sd::VoiceManager<V, 6> vm (vs);
        auto start = [&] (int n) { bool same; auto& v = vm.noteOn (n, 4, 0.01f, same); v.active = true; v.held = true; v.note = n; return same; };
        for (int n : { 60, 64, 67, 71 }) start (n);
        start (74);                                      // over the limit: the oldest (60) gets a fade
        int fading = 0; for (auto& v : vs) fading += v.killStep > 0.f ? 1 : 0;
        const bool same = start (64);
        auto rel = [] (V& v) { v.active = false; };
        vm.setPedal (true, rel); vm.noteOff (67, rel);
        bool sustained = false; for (auto& v : vs) sustained |= v.note == 67 && v.active && v.sustained;
        vm.setPedal (false, rel);
        bool released = true; for (auto& v : vs) released &= ! (v.note == 67 && v.active);
        check (fading == 1 && same && sustained && released, "Shared VoiceManager: steal, same-note, pedal");

        sd::NoteStack ns; ns.push (60); ns.push (64); ns.push (67); ns.remove (67);
        check (ns.top() == 64 && ns.count == 2, "Shared NoteStack");

        sd::Screamer sc; sc.set (0.7f);
        double fund = 0, all = 0;
        for (int i = 0; i < 48000; ++i)
        {
            const double ph = MathConstants<double>::twoPi * 220.0 * i / 48000.0;
            const double y = sc.process (0.4f * (float) std::sin (ph));
            fund += y * std::sin (ph); all += y * y;
        }
        const double harm = 1.0 - 2.0 * fund * fund / 48000.0 / all;
        check (harm > 0.1, "Shared Screamer adds harmonics", String (harm * 100.0, 1) + "%");

        // UI: FxTabs + FlowLane render
        doomsday::ui::StageLookAndFeel lnf (SerpentTheme::make());
        Component host; host.setLookAndFeel (&lnf); host.setSize (760, 260);
        doomsday::ui::FxTabs tabs (proc.apvts, 0);
        std::vector<std::unique_ptr<Component>> c1, c2;
        c1.push_back (std::make_unique<doomsday::ui::Knob> (proc.apvts, ParamIDs::dlyFb, "FEEDBACK"));
        c1.push_back (std::make_unique<doomsday::ui::Knob> (proc.apvts, ParamIDs::dlyMix, "MIX"));
        c2.push_back (std::make_unique<doomsday::ui::Knob> (proc.apvts, ParamIDs::revSize, "SIZE"));
        tabs.addTab ("DELAY", ParamIDs::dlyOn, std::move (c1), std::make_unique<doomsday::ui::Choice> (proc.apvts, ParamIDs::dlyDiv, Choices::delayDivs, 0, true));
        tabs.addTab ("REVERB", ParamIDs::revOn, std::move (c2));
        doomsday::ui::FlowLane lane (proc.getEngine().feed, 0, "OUT");
        host.addAndMakeVisible (tabs); host.addAndMakeVisible (lane);
        tabs.setBounds (0, 0, 560, 200); lane.setBounds (580, 0, 160, 250);
        proc.setCurrentProgram (0); render (proc, { { 0.0, 2.0, 60 } }, 1.5);
        tabs.refresh(); lane.tick();
        auto img = host.createComponentSnapshot (host.getLocalBounds(), true, 1.f);
        auto f = out.getChildFile ("shared_widgets.png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        check (img.isValid(), "Shared FxTabs + FlowLane render");
        host.removeAllChildren(); host.setLookAndFeel (nullptr);
    }

    printf (ok ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
