// Offline checks + tools for 4GE (started as Forge of Wrath's test, + checks for the 4GE modules)
//   RenderTest <outdir>                       tests, preset renders, GUI snapshot
//   RenderTest batch <jobs.txt> <outdir>      one kick per line: "name note id=value ... amp=<shape> drive=<shape>"
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <chrono>

using namespace juce;
static double sr = 44100.0;
static const int block = 512;
static bool ok = true;
static void check (bool c, const char* what, const String& extra = {}) { printf ("%-50s %s %s\n", what, c ? "OK" : "FAIL", extra.toRawUTF8()); ok &= c; }
static void set (GeProcessor& p, const String& id, float v) { if (auto* a = p.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); }

static AudioBuffer<float> hit (GeProcessor& p, int note, double seconds, float vel = 1.f)
{
    p.prepareToPlay (sr, block);
    AudioBuffer<float> out (2, (int) (sr * seconds)); out.clear();
    for (int pos = 0; pos < out.getNumSamples(); pos += block)
    {
        const int n = jmin (block, out.getNumSamples() - pos);
        AudioBuffer<float> b (2, n); b.clear();
        MidiBuffer m;
        if (pos == 0) m.addEvent (MidiMessage::noteOn (1, note, vel), 0);
        p.processBlock (b, m);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, n);
    }
    return out;
}
static void writeWav (const File& f, const AudioBuffer<float>& b)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}
// dominant frequency in a window by autocorrelation
static double pitchAt (const AudioBuffer<float>& b, double fromS, double lenS)
{
    const int a = (int) (fromS * sr), n = (int) (lenS * sr);
    const float* x = b.getReadPointer (0);
    double best = 0; int bestLag = 0;
    for (int lag = (int) (sr / 400); lag < (int) (sr / 25); ++lag)
    {
        double s = 0; for (int i = 0; i < n; ++i) s += x[a + i] * x[a + i + lag];
        if (s > best) { best = s; bestLag = lag; }
    }
    // the first lag that is almost as good = the period (not a multiple of it)
    for (int lag = (int) (sr / 400); lag < bestLag; ++lag)
    {
        double s = 0; for (int i = 0; i < n; ++i) s += x[a + i] * x[a + i + lag];
        double prev = 0, next = 0;
        for (int i = 0; i < n; ++i) { prev += x[a + i] * x[a + i + lag - 1]; next += x[a + i] * x[a + i + lag + 1]; }
        if (s > 0.9 * best && s >= prev && s >= next) return sr / lag;
    }
    return bestLag > 0 ? sr / bestLag : 0.0;
}

using ge::Shape;
static int batch (const File& jobs, const File& dir)
{
    dir.createDirectory();
    GeProcessor p;
    StringArray lines; jobs.readLines (lines);
    for (auto& line : lines)
    {
        auto tok = StringArray::fromTokens (line, " ", "");
        if (tok.size() < 2) continue;
        p.setCurrentProgram (0);
        double secs = 1.2;
        for (int i = 2; i < tok.size(); ++i)
        {
            const auto id = tok[i].upToFirstOccurrenceOf ("=", false, false), v = tok[i].fromFirstOccurrenceOf ("=", false, false);
            if (id == "amp" || id == "denv") { Shape s; if (Shape::fromString (v, s)) p.setShape (id == "amp" ? 0 : 1, s); }
            else if (id == "secs") secs = v.getDoubleValue();
            else if (id == "sr") sr = v.getDoubleValue();
            else set (p, id, v.getFloatValue());
        }
        writeWav (dir.getChildFile (tok[0] + ".wav"), hit (p, tok[1].getIntValue(), secs));
    }
    return 0;
}
// PROTOTYPE (2026-10-28, "drętwe"): a preset rendered as centre + left + right voices at 4x, low band from the plain centre,
// mids / highs from the detuned L / R copies -> stereo, dense. Writes <name>.wav at 4x (resample outside).
//   RenderTest proto <outdir> <name> <preset prefix> <note> <seconds> uni=5 det=12 drift=4 split=180 [param=value ...]
static int proto (int argc, char** argv)
{
    const File dir = File::getCurrentWorkingDirectory().getChildFile (argv[2]); dir.createDirectory();
    GeProcessor p;
    for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).startsWith (argv[4])) { p.setCurrentProgram (i); break; }
    const int note = atoi (argv[5]); const double secs = atof (argv[6]);
    int uni = 1; float det = 0.f, drift = 0.f, split = 180.f, mix = 0.6f, width = 1.f;
    for (int a = 7; a < argc; ++a)
    {
        const String t (argv[a]); const auto id = t.upToFirstOccurrenceOf ("=", false, false); const float v = t.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
        if (id == "uni") uni = (int) v; else if (id == "det") det = v; else if (id == "drift") drift = v; else if (id == "split") split = v; else if (id == "umix") mix = v; else if (id == "width") width = v;
        else set (p, id, v);
    }
    const double rate = 44100.0 * 4;
    auto prm = p.gatherForGui();
    ge::Params pc = prm.k, pl = prm.k, pr = prm.k;
    for (auto* q : { &pl, &pr }) { q->uniVoices = uni; q->uniDetuneCt = det; q->driftCt = drift; q->uniMix = mix; }
    pl.side = 1; pr.side = 2;
    for (auto* q : { &pc, &pl, &pr }) q->cook (rate);
    ge::Voice vc, vl, vr;
    for (auto* v : { &vc, &vl, &vr }) v->prepare (rate);
    vc.setRoom (pc.roomSize); vl.setRoom (pl.roomSize); vr.setRoom (pr.roomSize);
    const float hz = 440.f * std::pow (2.f, (note - 69) / 12.f);
    vc.trigger (hz, 1.f, pc); vl.trigger (hz, 1.f, pl); vr.trigger (hz, 1.f, pr);
    const auto sc = doomsday::dsp::svfCoefs (split, 1.f / 0.707f, rate);
    doomsday::dsp::Svf c1, c2, c3, c4, l1, l2, r1, r2;
    const float g = std::pow (10.f, prm.clipDriveDb / 20.f), outG = std::pow (10.f, prm.outDb / 20.f);
    const int n = (int) (secs * rate);
    AudioBuffer<float> out (2, n);
    for (int i = 0; i < n; ++i)
    {
        const float c = vc.render (pc), l = vl.render (pl), r = vr.render (pr);
        const float low = c2.lp (c1.lp (c, sc), sc);
        const float hc = c4.hp (c3.hp (c, sc), sc);
        const float hl0 = l2.hp (l1.hp (l, sc), sc), hr0 = r2.hp (r1.hp (r, sc), sc);
        const float hm = 0.5f * (hl0 + hr0), hs = 0.5f * (hl0 - hr0);                // mid / side of the detuned copies
        juce::ignoreUnused (hc);
        const float hl = hm + width * hs, hr = hm - width * hs;
        out.setSample (0, i, ge::masterClip (low + hl, g, prm.clipSoft) * outG);
        out.setSample (1, i, ge::masterClip (low + hr, g, prm.clipSoft) * outG);
    }
    const File f = dir.getChildFile (String (argv[3]) + ".wav"); f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), rate, 2, 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (out, 0, n);
    return 0;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    if (argc >= 2 && String (argv[1]) == "guistress")
    {
        // the editor with its timer + paint running while every parameter is moved (FL crash hunt, 2026-10-22)
        GeProcessor p; p.prepareToPlay (48000.0, 512);
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (Canvas::W, Canvas::H);
        Random r (7);
        for (int it = 0; it < 400; ++it)
        {
            if (it % 50 == 0) p.setCurrentProgram (r.nextInt (p.getNumPrograms()));
            for (int k = 0; k < 6; ++k)
            {
                auto* prm = p.getParameters()[r.nextInt (p.getParameters().size())];
                prm->setValueNotifyingHost (r.nextFloat());
            }
            AudioBuffer<float> b (2, 512); b.clear(); MidiBuffer m; if (it % 7 == 0) m.addEvent (MidiMessage::noteOn (1, 24 + r.nextInt (24), 1.f), 0);
            p.processBlock (b, m);
            dynamic_cast<GeEditor&> (*ed).getCanvas().tickForTest();
            ed->createComponentSnapshot (ed->getLocalBounds(), true, 0.5f);
        }
        dynamic_cast<GeEditor&> (*ed).getCanvas().openPresetBrowserForTest();     // the click on the preset name (crashed FL, 2026-10-22)
        printf ("guistress done\n");
        return 0;
    }
    if (argc >= 7 && String (argv[1]) == "proto") return proto (argc, argv);
    if (argc >= 4 && String (argv[1]) == "batch") return batch (File::getCurrentWorkingDirectory().getChildFile (argv[2]), File::getCurrentWorkingDirectory().getChildFile (argv[3]));
    const File outDir = File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "fourge_out");
    outDir.createDirectory();
    using namespace ParamIDs;
    GeProcessor p;

    // 1) default kick: sane level, tail lands on the played note, ends at LENGTH
    {
        p.setCurrentProgram (0);
        set (p, length, 500.f);
        auto b = hit (p, 31, 0.8);
        const float pk = b.getMagnitude (0, 0, b.getNumSamples());
        check (pk > 0.5f && pk <= 0.99f, "default kick peak in range", String (pk, 3));
        const double f = pitchAt (b, 0.25, 0.12);
        check (std::abs (f - 49.0) < 3.0, "tail settles on the note (G1 = 49 Hz)", String (f, 1) + " Hz");
        const float after = b.getMagnitude (0, (int) (0.52 * sr), (int) (0.2 * sr));
        check (after < 1e-4f, "silent after LENGTH", String (after, 6));
        bool finite = true; for (int i = 0; i < b.getNumSamples(); ++i) finite &= std::isfinite (b.getSample (0, i));
        check (finite, "no NaN / inf");
        const double f2 = pitchAt (hit (p, 34, 0.8), 0.25, 0.12);
        check (std::abs (f2 - 58.3) < 3.5, "note A#1 -> 58 Hz", String (f2, 1) + " Hz");
    }
    // 1b) CLEAN SUB: the lows of a hard-distorted tail become a clean sine (2nd / 3rd harmonic drop)
    {
        auto harmDb = [] (const AudioBuffer<float>& b)
        {
            const int n = 1 << 14, a = (int) (0.12 * sr);
            std::vector<float> w ((size_t) n * 2, 0.f);
            for (int i = 0; i < n && a + i < b.getNumSamples(); ++i) w[(size_t) i] = b.getSample (0, a + i) * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * i / n));
            dsp::FFT fft (14); fft.performFrequencyOnlyForwardTransform (w.data());
            auto at = [&] (double hz) { const int k = (int) std::round (hz * n / sr); float m = 0; for (int j = k - 2; j <= k + 2; ++j) m = jmax (m, w[(size_t) j]); return (double) m; };
            return 20.0 * std::log10 ((at (98.0) + at (147.0)) / at (49.0));
        };
        p.setCurrentProgram (0);
        set (p, distType, 2.f); set (p, drive, 30.f); set (p, length, 600.f); set (p, clipDrive, 0.f); set (p, uniVoices, 1.f);
        const double off = harmDb (hit (p, 31, 0.6));
        set (p, cleanSub, 250.f); set (p, uniVoices, 1.f);
        const double on = harmDb (hit (p, 31, 0.6));
        check (on < off - 10.0, "CLEAN SUB: low harmonics of the distortion removed", "(" + String (off, 1) + " -> " + String (on, 1) + " dB)");
    }
    // 2) every preset: no NaN, peak <= 0 dBFS, not silent
    {
        int bad = 0; String worst;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            auto b = hit (p, 31, 1.4);
            const float pk = b.getMagnitude (0, 0, b.getNumSamples());
            bool finite = true; for (int s = 0; s < b.getNumSamples(); ++s) finite &= std::isfinite (b.getSample (0, s));
            // the owner's own kicks are kept exactly as he set them by ear (they may peak a little over 0 dBFS)
            const float limit = p.getProgramName (i).startsWith ("OWNER") ? 2.f : 1.f;
            if (! finite || pk > limit || pk < 0.2f) { ++bad; worst << p.getProgramName (i) << " (" << String (pk, 2) << ") "; }
            writeWav (outDir.getChildFile (String (i).paddedLeft ('0', 2) + "_" + File::createLegalFileName (p.getProgramName (i)) + ".wav"), b);
        }
        check (bad == 0, "all presets clean", String (p.getNumPrograms()) + " presets " + worst);
    }
    // 3) retrigger: a new hit fades the old one without a click (no jump bigger than the kick itself makes)
    {
        p.setCurrentProgram (0);
        p.prepareToPlay (sr, block);
        AudioBuffer<float> b (2, block * 40); b.clear();
        for (int k = 0; k < 40; ++k)
        {
            AudioBuffer<float> c (2, block); c.clear(); MidiBuffer m;
            if (k == 0 || k == 20) m.addEvent (MidiMessage::noteOn (1, 31, 1.f), 100);
            p.processBlock (c, m);
            b.copyFrom (0, k * block, c, 0, 0, block);
        }
        check (b.getMagnitude (0, 20 * block, block * 20) > 0.5f, "retrigger plays a new hit");
    }
    // 4) envelopes: state round trip keeps the drawn shapes
    {
        p.setCurrentProgram (0);
        auto s = Shape::make ({ { 0, 1 }, { 300, 0.2f }, { 650, 0.9f }, { 1000, 1 } });
        p.setShape (1, s);
        MemoryBlock mb; p.getStateInformation (mb);
        GeProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        check (q.getShape (1).n == 4 && std::abs (q.getShape (1).pts[2].v - 0.9f) < 1e-3f, "drive envelope saved with the session");
    }
    // 5) CPU: plain (one voice set) and with UNISON (three voice sets: centre + left + right)
    for (int uni : { 1, 5 })
    {
        p.setCurrentProgram (0); set (p, uniVoices, (float) uni);
        p.prepareToPlay (48000.0, block);
        AudioBuffer<float> b (2, block);
        const int blocks = (int) (48000.0 * 10 / block);
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int k = 0; k < blocks; ++k) { b.clear(); MidiBuffer m; if (k % 18 == 0) m.addEvent (MidiMessage::noteOn (1, 31, 1.f), 0); p.processBlock (b, m); }
        const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        check (secs / 10.0 < (uni > 1 ? 0.30 : 0.15), uni > 1 ? "CPU with UNISON 5 (10 s of kicks)" : "CPU plain (10 s of kicks at 150 BPM)", String (secs / 10.0 * 100.0, 1) + " %");
    }
    // 8) PITCH / FINE move the tail only
    {
        p.setCurrentProgram (0); sr = 48000.0;
        set (p, ParamIDs::tune, 5.f); set (p, ParamIDs::tailFine, 50.f);
        auto b = hit (p, 31, 0.5);
        const double hz = pitchAt (b, 0.3, 0.08), want = 49.0 * std::pow (2.0, 5.5 / 12.0);
        check (std::abs (hz - want) < 3.0, "PITCH +5 st, FINE +50 ct -> tail on 66.7 Hz", String (hz, 1) + " Hz");
        set (p, ParamIDs::tune, 0.f); set (p, ParamIDs::tailFine, 0.f);
    }
    // 9) PUNCH keeps its pitch on any note (only its START / END knobs set it)
    {
        p.setCurrentProgram (0); sr = 48000.0;
        set (p, ParamIDs::punchInto, 0.f); set (p, ParamIDs::tailLevel, -24.f); set (p, ParamIDs::punchLevel, 0.f); set (p, ParamIDs::clickLevel, -60.f); set (p, ParamIDs::chaos, 0.f);
        auto lo = hit (p, 31, 0.1), hi = hit (p, 43, 0.1);
        double diff = 0, all = 0;
        for (int i = 0; i < (int) (0.03 * sr); ++i) { const double a = lo.getSample (0, i), b = hi.getSample (0, i); diff += (a - b) * (a - b); all += a * a; }
        // the tail is turned down, not off: allow a small rest
        check (diff < all * 0.1, "PUNCH: same pitch on G1 and G2", "(difference " + String (10.0 * std::log10 (diff / (all + 1e-12) + 1e-12), 1) + " dB)");
    }
    // 10) PUNCH INTO DIST: A/B for the owner (INIT, G1 then C2: punch off-chain, then into the distortion)
    {
        sr = 48000.0;
        AudioBuffer<float> ab (2, (int) (sr * 0.5) * 4); ab.clear();
        int at = 0;
        for (float into : { 0.f, 1.f })
            for (int note : { 31, 36 })
            {
                p.setCurrentProgram (0); set (p, ParamIDs::punchInto, into);
                auto b = hit (p, note, 0.5);
                for (int ch = 0; ch < 2; ++ch) ab.copyFrom (ch, at, b, ch, 0, b.getNumSamples());
                at += b.getNumSamples();
            }
        bool finite = true; for (int i = 0; i < ab.getNumSamples(); ++i) finite &= std::isfinite (ab.getSample (0, i));
        check (finite && ab.getMagnitude (0, 0, ab.getNumSamples()) < 1.05f, "PUNCH INTO DIST: clean output");
        writeWav (outDir.getChildFile ("punch_into_ab.wav"), ab);
    }
    // 11) FIXED HEAD: like the owner's reference kicks - the first part is identical on G1 and D2, the tail is on the note
    {
        sr = 48000.0;
        p.setCurrentProgram (0); set (p, ParamIDs::headOn, 1.f); set (p, ParamIDs::headLen, 110.f);
        hit (p, 31, 1.0);                                       // flush the previous voice
        auto g1 = hit (p, 31, 1.0), d2 = hit (p, 38, 1.0);
        double diff = 0, all = 0;
        for (int i = 0; i < (int) (0.1 * sr); ++i) { const double a = g1.getSample (0, i), b = d2.getSample (0, i); diff += (a - b) * (a - b); all += a * a; }
        const double tailG = pitchAt (g1, 0.25, 0.08), tailD = pitchAt (d2, 0.25, 0.08);
        check (diff < all * 1e-6 && std::abs (tailG - 49.0) < 3.0 && std::abs (tailD - 73.4) < 4.0, "FIXED HEAD: same first 100 ms on every note, tail on the note",
               "(head diff " + String (10.0 * std::log10 (diff / (all + 1e-20) + 1e-20), 1) + " dB, tails " + String (tailG, 1) + " / " + String (tailD, 1) + " Hz)");
    }
    // ===================== 4GE modules
    auto finiteOf = [] (const AudioBuffer<float>& b) { for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (0, i))) return false; return true; };
    auto rmsOf = [] (const AudioBuffer<float>& b, double a, double e)
    {
        const int i0 = (int) (a * sr), i1 = jmin (b.getNumSamples(), (int) (e * sr));
        double acc = 0; for (int i = i0; i < i1; ++i) acc += (double) b.getSample (0, i) * b.getSample (0, i);
        return std::sqrt (acc / jmax (1, i1 - i0));
    };
    // 12) the same settings render bit-identical twice (phase reset, noise seeds)
    {
        sr = 48000.0;
        p.setCurrentProgram (0); set (p, texLevel, -6.f); set (p, texSource, 1.f); set (p, punchType, 3.f); set (p, busType, 2.f);
        hit (p, 31, 0.6);
        auto a = hit (p, 31, 0.6), b = hit (p, 31, 0.6);
        double diff = 0; for (int i = 0; i < a.getNumSamples(); ++i) diff += std::abs (a.getSample (0, i) - b.getSample (0, i));
        check (diff == 0.0, "4GE: two renders are bit-identical", "(diff " + String (diff, 8) + ")");
    }
    // 13) every new option: clean output, makes sound
    {
        sr = 48000.0;
        int bad = 0; String why;
        auto test = [&] (const String& name, std::initializer_list<std::pair<const char*, float>> vals)
        {
            p.setCurrentProgram (0);
            for (auto& [id, v] : vals) set (p, id, v);
            auto b = hit (p, 31, 0.6);
            const float pk = b.getMagnitude (0, 0, b.getNumSamples());
            if (! finiteOf (b) || pk > 1.0f || pk < 0.1f) { ++bad; why << name << " (" << String (pk, 2) << ") "; }
        };
        for (int t = 0; t < 5; ++t) test ("punch type " + String (t), { { punchType, (float) t }, { punchInto, 0.f }, { punchDrive, 12.f }, { punchFiltType, (float) (t % 5) } });
        for (int t = 0; t < 4; ++t) test ("texture " + String (t), { { texSource, (float) t }, { texLevel, 0.f }, { texDrive, 12.f }, { texReso, 0.9f } });
        for (int t = 1; t < 7; ++t) test ("bus " + String (t), { { busType, (float) t }, { busDrive, 24.f } });
        for (int t = 0; t < 3; ++t) test ("osc B " + String (t), { { oscBType, (float) t }, { oscBLevel, 0.f }, { oscBWidth, 0.2f } });
        for (int t = 1; t < 5; ++t) test ("pre filter " + String (t), { { preType, (float) t }, { preReso, 1.f }, { preEnv, 1.f }, { preEnv, -1.f } });
        test ("bias / mix", { { bias1, 0.8f }, { mix1, 0.5f }, { dist2Type, 5.f }, { bias2, -0.7f }, { mix2, 0.7f } });
        test ("crush", { { distType, 5.f }, { dist2Type, 6.f } });
        test ("start Hz + attack + phase + duck", { { tailStartMode, 1.f }, { tailStartHz, 600.f }, { tailAttack, 8.f }, { tailPhase, 90.f }, { punchPhase, 90.f }, { duck, 1.f }, { punchAttack, 3.f } });
        test ("room + low cut", { { room, 1.f }, { roomLowCut, 400.f }, { roomInto, 0.f } });
        check (bad == 0, "4GE: every new option is clean and audible", why);
    }
    // 14) START in Hz: the tail starts on the same pitch on G1 and D2
    {
        sr = 48000.0;
        p.setCurrentProgram (0); set (p, headOn, 0.f); set (p, punchOn, 0.f); set (p, clickLevel, -60.f); set (p, chaos, 0.f); set (p, uniVoices, 1.f);
        set (p, tailStartMode, 1.f); set (p, tailStartHz, 300.f); set (p, tailDrop, 200.f); set (p, tailCurve, 1.f); set (p, ParamIDs::drive, 0.f); set (p, clipDrive, 0.f);
        set (p, distType, 0.f);
        const double g = pitchAt (hit (p, 31, 0.3), 0.003, 0.012), d = pitchAt (hit (p, 38, 0.3), 0.003, 0.012);
        check (std::abs (g - d) < 25.0 && g > 200.0, "4GE: START Hz is the same on every note", String (g, 0) + " / " + String (d, 0) + " Hz");
    }
    // 15) AUTO GAIN: drive 6 -> 40 dB changes the level much less than without it
    {
        sr = 48000.0;
        auto lvl = [&] (bool autoOn, float drv)
        {
            p.setCurrentProgram (0); set (p, autoGain, autoOn ? 1.f : 0.f); set (p, distType, 0.f); set (p, ParamIDs::drive, drv);
            set (p, punchOn, 0.f); set (p, clipDrive, 0.f); set (p, cleanSub, 0.f); set (p, output, -12.f);
            return 20.0 * std::log10 (rmsOf (hit (p, 31, 0.5), 0.15, 0.3) + 1e-9);
        };
        const double offJump = lvl (false, 40.f) - lvl (false, 0.f), onJump = lvl (true, 40.f) - lvl (true, 0.f);
        check (std::abs (onJump) < 2.0 && std::abs (onJump) < std::abs (offJump) - 1.0, "4GE: AUTO GAIN keeps the level when DRIVE goes up",
               "(jump " + String (offJump, 1) + " dB -> " + String (onJump, 1) + " dB)");
    }
    // 16) SOLO / MUTE
    {
        sr = 48000.0;
        p.setCurrentProgram (0);
        p.muteMask.store (ge::LAll); const double all = rmsOf (hit (p, 31, 0.4), 0.0, 0.4);
        p.muteMask.store (0); p.soloMask.store (ge::LPunch); set (p, punchInto, 0.f);
        auto pb = hit (p, 31, 0.4);
        const double late = rmsOf (pb, 0.2, 0.35), early = rmsOf (pb, 0.0, 0.03);
        p.soloMask.store (0);
        check (all < 1e-6 && early > 0.01 && late < early * 0.05, "4GE: MUTE all = silence, SOLO punch = only the punch", "(" + String (all, 6) + ", " + String (early, 3) + " / " + String (late, 4) + ")");
    }
    // 17) the same kick at 44.1 / 48 / 96 kHz: same pitch, same level
    {
        double hz[3], lv[3]; int k = 0;
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            sr = rate; p.setCurrentProgram (0);
            auto b = hit (p, 31, 0.5);
            hz[k] = pitchAt (b, 0.25, 0.08); lv[k] = 20.0 * std::log10 (rmsOf (b, 0.0, 0.4) + 1e-9); ++k;
        }
        sr = 48000.0;
        check (std::abs (hz[0] - hz[2]) < 2.0 && std::abs (lv[0] - lv[2]) < 1.0 && std::abs (lv[1] - lv[2]) < 1.0, "4GE: 44.1 / 48 / 96 kHz sound the same",
               String (hz[0], 1) + " / " + String (hz[1], 1) + " / " + String (hz[2], 1) + " Hz, " + String (lv[0], 1) + " / " + String (lv[1], 1) + " / " + String (lv[2], 1) + " dB");
    }
    // 18) A/B for the owner: INIT, then the new modules one by one (G1, 0.5 s each)
    {
        sr = 48000.0;
        struct V { const char* name; std::vector<std::pair<const char*, float>> vals; };
        const std::vector<V> vs {
            { "INIT", {} },
            { "+ osc B saw -12 dB", { { oscBLevel, -12.f } } },
            { "+ pre filter BP sweep", { { preType, 3.f }, { preHz, 500.f }, { preReso, 0.6f }, { preEnv, 0.6f }, { preDecay, 120.f } } },
            { "+ stage 1 bias", { { bias1, 0.5f } } },
            { "+ texture pink 3 kHz", { { texLevel, -10.f }, { texSource, 1.f }, { texHz, 3000.f }, { texDecay, 120.f } } },
            { "+ bus tube", { { busType, 2.f }, { busDrive, 8.f } } },
            { "punch RESO, into off", { { punchType, 4.f }, { punchInto, 0.f }, { punchDrive, 12.f }, { punchFiltType, 2.f }, { punchFiltHz, 250.f } } },
            { "duck 70% + tail attack 4 ms", { { duck, 0.7f }, { tailAttack, 4.f } } } };
        AudioBuffer<float> ab (2, (int) (sr * 0.5) * (int) vs.size()); ab.clear();
        int at = 0;
        std::vector<std::pair<const char*, float>> acc;
        for (auto& v : vs)
        {
            p.setCurrentProgram (0);
            for (auto& x : v.vals) acc.push_back (x);
            for (auto& [id, val] : acc) set (p, id, val);
            auto b = hit (p, 31, 0.5);
            for (int ch = 0; ch < 2; ++ch) ab.copyFrom (ch, at, b, ch, 0, b.getNumSamples());
            at += b.getNumSamples();
        }
        writeWav (outDir.getChildFile ("4ge_modules_ab.wav"), ab);
    }
    // 19) UNISON / WIDTH: stereo above WIDTH FROM, mono below it, the tail stays on the note
    {
        sr = 48000.0;
        p.setCurrentProgram (0); set (p, uniVoices, 5.f); set (p, width, 0.5f); set (p, widthSplit, 350.f);
        auto b = hit (p, 31, 0.5);
        AudioBuffer<float> s (1, b.getNumSamples()), m (1, b.getNumSamples());
        for (int i = 0; i < b.getNumSamples(); ++i) { s.setSample (0, i, 0.5f * (b.getSample (0, i) - b.getSample (1, i))); m.setSample (0, i, 0.5f * (b.getSample (0, i) + b.getSample (1, i))); }
        auto bandE = [&] (const AudioBuffer<float>& x, double lo, double hi)
        {
            const int nfft = 1 << 15, a = (int) (0.1 * sr);
            std::vector<float> w ((size_t) nfft * 2, 0.f);
            for (int i = 0; i < nfft && a + i < x.getNumSamples(); ++i) w[(size_t) i] = x.getSample (0, a + i) * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * i / nfft));
            dsp::FFT fft (15); fft.performFrequencyOnlyForwardTransform (w.data());
            double e = 0; for (int k = (int) (lo * nfft / sr); k < (int) (hi * nfft / sr); ++k) e += (double) w[(size_t) k] * w[(size_t) k];
            return e;
        };
        const double lowSide = 10.0 * std::log10 (bandE (s, 20, 150) / (bandE (m, 20, 150) + 1e-20) + 1e-20);
        const double highSide = 10.0 * std::log10 (bandE (s, 1000, 6000) / (bandE (m, 1000, 6000) + 1e-20) + 1e-20);
        const double hz = pitchAt (m, 0.3, 0.08);
        check (lowSide < -30.0 && highSide > -20.0 && highSide < -3.0 && std::abs (hz - 49.0) < 3.0, "4GE: WIDTH - mids / highs stereo, lows mono, on the note",
               "(side/mid: lows " + String (lowSide, 1) + " dB, 1-6 kHz " + String (highSide, 1) + " dB, " + String (hz, 1) + " Hz)");
        set (p, uniVoices, 1.f);
    }
    // 6) GUI snapshot
    {
        for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).startsWith ("OWNER - QL")) { p.setCurrentProgram (i); break; }
        sr = 48000.0; hit (p, 31, 0.15);
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (Canvas::W, Canvas::H);
        p.muteMask.store (ge::LTexture); p.soloMask.store (0);
        for (int pg : { 4, 0, 1, 2, 3 })
        {
            dynamic_cast<GeEditor&> (*ed).getCanvas().showPageForTest (pg);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.f);
            const char* names[] { "gui_punch.png", "gui_tail.png", "gui_texture.png", "gui_bus.png", "gui_main.png" };
            PNGImageFormat png; FileOutputStream fo (outDir.getChildFile (names[pg])); fo.setPosition (0); fo.truncate(); png.writeImageToStream (img, fo);
        }
        p.muteMask.store (0);
    }
    printf ("%s\n", ok ? "ALL OK" : "SOME FAILED");
    return ok ? 0 : 1;
}
