// Offline checks for RE4GE (reverse bass)
//   RenderTest <outdir>       tests, preset renders (with a short kick on top, for listening), GUI snapshots
#include "../Source/PluginProcessor.h"
#include "../Source/GUI/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>

using namespace juce;
static double sr = 48000.0;
static const int block = 512;
static bool ok = true;
static void check (bool c, const char* what, const String& extra = {}) { printf ("%-58s %s %s\n", what, c ? "OK" : "FAIL", extra.toRawUTF8()); ok &= c; }
static void set (ReProcessor& p, const String& id, float v) { if (auto* a = p.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); }

// a fake host with a tempo
struct Host : AudioPlayHead
{
    double bpm = 150.0;
    Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setBpm (bpm); i.setIsPlaying (true); return i; }
};

// notes: (start ms, length ms, note); renders `seconds`
struct Note { double at, len; int note; float vel = 1.f; };
// bufferSize < 0 = random sizes 1..1500 (seeded)
static AudioBuffer<float> play (ReProcessor& p, Host& h, std::vector<Note> notes, double seconds, int bufferSize = block)
{
    p.setPlayHead (&h);
    p.prepareToPlay (sr, bufferSize > 0 ? bufferSize : 1500);
    AudioBuffer<float> out (2, (int) (sr * seconds)); out.clear();
    Random rnd (77);
    for (int pos = 0, n = 0; pos < out.getNumSamples(); pos += n)
    {
        n = jmin (bufferSize > 0 ? bufferSize : 1 + rnd.nextInt (1500), out.getNumSamples() - pos);
        AudioBuffer<float> b (2, n); b.clear(); MidiBuffer m;
        for (auto& nt : notes)
        {
            const int on = (int) (nt.at * 0.001 * sr), off = (int) ((nt.at + nt.len) * 0.001 * sr);
            if (on >= pos && on < pos + n) m.addEvent (MidiMessage::noteOn (1, nt.note, nt.vel), on - pos);
            if (off >= pos && off < pos + n) m.addEvent (MidiMessage::noteOff (1, nt.note), off - pos);
        }
        p.processBlock (b, m);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, n);
    }
    return out;
}
static std::vector<Note> everyBeat (int beats, double bpm, int note = 31) { std::vector<Note> v; for (int i = 0; i < beats; ++i) v.push_back ({ i * 60000.0 / bpm, 100.0, note }); return v; }
static double rms (const AudioBuffer<float>& b, double fromMs, double toMs)
{
    const int a = jmax (0, (int) (fromMs * 0.001 * sr)), e = jmin (b.getNumSamples(), (int) (toMs * 0.001 * sr));
    double acc = 0; for (int i = a; i < e; ++i) acc += (double) b.getSample (0, i) * b.getSample (0, i);
    return std::sqrt (acc / jmax (1, e - a));
}
static int byName (ReProcessor& p, const char* start) { for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).startsWith (start)) return i; return 0; }
static double diff (const AudioBuffer<float>& a, const AudioBuffer<float>& b)
{
    double d = 0; for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < jmin (a.getNumSamples(), b.getNumSamples()); ++i) d = jmax (d, (double) std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
    return d;
}
static bool finite (const AudioBuffer<float>& b) { for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (0, i))) return false; return true; }
static void writeWav (const File& f, const AudioBuffer<float>& b)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}
// strongest frequency 20..300 Hz in a window (DFT scan)
static double domHz (const AudioBuffer<float>& b, double fromMs, double lenMs)
{
    double hz = 0, best = 0;
    const int a0 = (int) (fromMs * 0.001 * sr), n = (int) (lenMs * 0.001 * sr);
    for (double f = 20.0; f <= 300.0; f += 0.5)
    {
        double re = 0, im = 0;
        for (int i = 0; i < n; ++i) { const double w = 0.5 - 0.5 * std::cos (6.283185307 * i / n), v = b.getSample (0, a0 + i) * w;
                                     re += v * std::cos (6.283185307 * f * i / sr); im += v * std::sin (6.283185307 * f * i / sr); }
        if (re * re + im * im > best) { best = re * re + im * im; hz = f; }
    }
    return hz;
}
// a short kick on every beat (4GE's voice, default settings, 170 ms) so the owner hears the bass in context
static void addKicks (AudioBuffer<float>& b, double bpm)
{
    ge::Params k; k.lengthMs = 170.f; k.punchIntoDist = true; k.headFixed = true; k.headMs = 110.f;
    k.cook (sr);
    ge::Voice v; v.prepare (sr);
    const int beat = (int) (60.0 / bpm * sr);
    for (int start = 0; start < b.getNumSamples(); start += beat)
    {
        v.trigger (49.f, 1.f, k);
        for (int i = start; i < jmin (b.getNumSamples(), start + beat); ++i) { const float y = ge::masterClip (v.render (k), 1.6f, 0.4f) * 0.8f; for (int ch = 0; ch < 2; ++ch) b.addSample (ch, i, y); }
    }
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir = File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "re4ge_out");
    outDir.createDirectory();
    using namespace ParamIDs;
    ReProcessor p; Host host;

    // 1) every preset: clean, audible, not over 0 dBFS; render with kicks for listening
    {
        int bad = 0; String why;
        AudioBuffer<float> all (2, (int) (sr * 1.6) * p.getNumPrograms()); all.clear();
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i); host.bpm = 150.0;
            const bool gate = p.getProgramName (i).startsWith ("GATE");
            auto notes = gate ? std::vector<Note> { { 200.0, 300.0, 31 }, { 600.0, 150.0, 34 }, { 800.0, 400.0, 29 } } : everyBeat (4, 150.0);
            auto b = play (p, host, notes, 1.6);
            const float pk = b.getMagnitude (0, 0, b.getNumSamples());
            printf ("   %-34s peak %.2f  rms %.1f dB\n", p.getProgramName (i).toRawUTF8(), pk, 20.0 * std::log10 (rms (b, 0, 1600) + 1e-9));
            if (! finite (b) || pk > 1.0f || pk < 0.05f) { ++bad; why << p.getProgramName (i) << " (" << String (pk, 2) << ") "; }
            writeWav (outDir.getChildFile ("solo_" + String (i).paddedLeft ('0', 2) + ".wav"), b);       // the bass alone (for measuring)
            if (! gate) addKicks (b, 150.0);
            writeWav (outDir.getChildFile (String (i).paddedLeft ('0', 2) + "_" + File::createLegalFileName (p.getProgramName (i)) + ".wav"), b);
            for (int ch = 0; ch < 2; ++ch) all.copyFrom (ch, i * (int) (sr * 1.6), b, ch, 0, b.getNumSamples());
        }
        writeWav (outDir.getChildFile ("re4ge_presets_with_kick.wav"), all);
        check (bad == 0, "all presets clean and audible", String (p.getNumPrograms()) + " presets " + why);
    }
    // 2) PATTERN timing (PUMP, 1/4): silent until DELAY, sound in the middle, silent before the next kick - at 150 and 172 BPM
    for (double bpm : { 150.0, 172.0 })
    {
        p.setCurrentProgram (0); host.bpm = bpm;
        auto b = play (p, host, everyBeat (4, bpm), 4 * 60.0 / bpm);
        const double beatMs = 60000.0 / bpm, delay = 0.36 * beatMs, gate = 0.98 * beatMs;
        // onset: the first sample above -40 dB of the bass's peak in beat 2
        const float pk = b.getMagnitude (0, (int) (beatMs * 0.001 * sr), (int) (beatMs * 0.001 * sr));
        int on = -1, off = -1;
        for (int i = (int) (beatMs * 0.001 * sr); i < (int) (2 * beatMs * 0.001 * sr); ++i)
            if (std::abs (b.getSample (0, i)) > pk * 0.01f) { if (on < 0) on = i; off = i; }
        const double onMs = on * 1000.0 / sr - beatMs, offMs = off * 1000.0 / sr - beatMs;
        check (std::abs (onMs - delay) < 3.0 && offMs < gate + 1.0 && offMs > gate - 30.0, ("PATTERN timing at " + String (bpm, 0) + " BPM: starts on DELAY, gone before the kick").toRawUTF8(),
               "(start " + String (onMs, 1) + " / " + String (delay, 1) + " ms, end " + String (offMs, 1) + " / " + String (gate, 1) + " ms)");
    }
    // 3) TAIL backwards: on the note in the swell, rising at the end (the tail's pitch drop played backwards)
    {
        p.setCurrentProgram (byName (p, "PUMP - QL Tail")); host.bpm = 150.0;
        set (p, subHz, 20.f); set (p, highCut, 3000.f); set (p, drive, 0.f); set (p, uniVoices, 1.f);
        auto b = play (p, host, everyBeat (2, 150.0), 0.8);
        const double hz = domHz (b, 190.0, 80.0), hzLate = domHz (b, 360.0, 30.0);
        check (hz > 45.0 && hz < 65.0 && hzLate > hz * 1.3, "TAIL: on the note, then rises before the kick", String (hz, 1) + " -> " + String (hzLate, 1) + " Hz");
    }
    // 4) SUB: the clean sine adds low end, the low end is not cut by default
    {
        p.setCurrentProgram (0); host.bpm = 150.0;
        auto low = [&] (const AudioBuffer<float>& b) { return domHz (b, 200.0, 120.0); };
        auto b = play (p, host, everyBeat (2, 150.0), 0.8);
        auto* lc = p.apvts.getParameter (lowCut);
        check (lc != nullptr && lc->getValue() < 0.001f && std::abs (low (b) - 49.0) < 4.0, "the low end stays: LOW CUT off by default, the note is the strongest",
               String (low (b), 1) + " Hz");
    }
    // 5) GATE mode: a 300 ms note sounds, silence after note-off + FADE
    {
        for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).startsWith ("GATE")) p.setCurrentProgram (i);
        auto b = play (p, host, { { 100.0, 300.0, 31 } }, 0.8);
        const double during = rms (b, 200.0, 380.0), after = rms (b, 460.0, 780.0);
        check (during > 0.05 && after < during * 0.001, "GATE: plays while the note is held, silent after FADE", "(" + String (during, 3) + " / " + String (after, 6) + ")");
    }
    // 6) MASK: offbeats only on 1/16 (steps 2 + 4)
    {
        for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).startsWith ("PULSE")) p.setCurrentProgram (i);
        host.bpm = 150.0;
        auto b = play (p, host, everyBeat (2, 150.0), 0.8);
        const double s1 = rms (b, 400.0, 500.0), s2 = rms (b, 500.0, 600.0), s3 = rms (b, 600.0, 700.0), s4 = rms (b, 700.0, 800.0);
        check (s2 > 0.02 && s4 > 0.02 && s1 < s2 * 0.01 && s3 < s4 * 0.01, "MASK: 1/16 steps 2 + 4 play, 1 + 3 are silent",
               String (s1, 4) + " / " + String (s2, 3) + " / " + String (s3, 4) + " / " + String (s4, 3));
    }
    // 7) the same bass at 44.1 / 48 / 96 kHz
    {
        double lv[3]; int k = 0;
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            sr = rate; p.setCurrentProgram (byName (p, "PUMP - QL Tail")); host.bpm = 150.0;
            lv[k++] = 20.0 * std::log10 (rms (play (p, host, everyBeat (2, 150.0), 0.8), 400.0, 800.0) + 1e-9);
        }
        sr = 48000.0;
        check (std::abs (lv[0] - lv[2]) < 1.0 && std::abs (lv[1] - lv[2]) < 1.0, "44.1 / 48 / 96 kHz: the same level", String (lv[0], 1) + " / " + String (lv[1], 1) + " / " + String (lv[2], 1) + " dB");
    }
    // 8) bit-identical renders (phase reset per step, the tail rendered the same way)
    {
        p.setCurrentProgram (byName (p, "SWELL - Synth Bend"));
        auto a = play (p, host, everyBeat (2, 150.0), 0.8), b = play (p, host, everyBeat (2, 150.0), 0.8);
        double d = 0; for (int i = 0; i < a.getNumSamples(); ++i) d += std::abs (a.getSample (0, i) - b.getSample (0, i));
        check (d == 0.0, "two renders are bit-identical", String (d, 8));
    }
    // 9) PHASE WARP: every mode clean and different from OFF; every source / pitch mode clean
    {
        int bad = 0; String why;
        auto render = [&] () { return play (p, host, everyBeat (2, 150.0), 0.8); };
        p.setCurrentProgram (byName (p, "SWELL - Synth Bend")); set (p, warp, 0.f);
        const auto off = render();
        for (int w = 1; w <= 4; ++w)
        {
            set (p, warp, (float) w); set (p, warpAmt, 0.8f);
            auto b = render();
            double d = 0; for (int i = 0; i < b.getNumSamples(); ++i) d += std::abs (b.getSample (0, i) - off.getSample (0, i));
            if (! finite (b) || b.getMagnitude (0, 0, b.getNumSamples()) > 1.f || d < 1.0) { ++bad; why << "warp " << w << " "; }
        }
        for (int pm = 0; pm < 4; ++pm) for (int src = 0; src < 2; ++src)
        {
            p.setCurrentProgram (0); set (p, source, (float) src); set (p, pitchMode, (float) pm); set (p, pitchAmt, 12.f);
            auto b = render();
            if (! finite (b) || b.getMagnitude (0, 0, b.getNumSamples()) > 1.f || rms (b, 0, 800) < 0.01) { ++bad; why << "pitch " << pm << "/" << src << " "; }
        }
        for (int t = 0; t < 5; ++t) { p.setCurrentProgram (byName (p, "SWELL - Synth Bend")); p.applyTemplate (t); auto b = render(); if (! finite (b) || rms (b, 0, 800) < 0.01) { ++bad; why << "template " << t << " "; } }
        check (bad == 0, "warp modes, pitch modes, sources, templates: clean", why);
    }
    // 10) CPU
    {
        p.setCurrentProgram (byName (p, "PUMP - QL Tail")); host.bpm = 150.0;
        auto t0 = std::chrono::high_resolution_clock::now();
        play (p, host, everyBeat (25, 150.0), 10.0);
        const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        check (secs / 10.0 < 0.15, "CPU (10 s at 150 BPM, TAIL source)", String (secs / 10.0 * 100.0, 1) + " %");
    }
    // 12) the result never depends on the host's buffer size (internal 32-sample grid): 1, 7, 64, 333, 4096 and random sizes
    {
        double worst = 0; String why;
        for (const char* pre : { "PUMP - QL Tail", "SWELL - Synth Bend", "GATE", "PULSE" })
        {
            p.setCurrentProgram (byName (p, pre)); host.bpm = 150.0;
            const bool gate = String (pre) == "GATE";
            auto notes = gate ? std::vector<Note> { { 30.0, 300.0, 31 }, { 500.0, 200.0, 34 } } : everyBeat (2, 150.0);
            const auto ref = play (p, host, notes, 0.8, 512);
            for (int bs : { 1, 7, 64, 333, 4096, -1 })
            {
                const double d = diff (ref, play (p, host, notes, 0.8, bs));
                if (d > worst) { worst = d; why = String (pre) + " @ " + String (bs); }
            }
        }
        // (block 1: the oversampler's float rounding differs by ~1e-8 = -160 dB; everything else is bit-identical)
        check (worst < 1.0e-6, "buffer size 1 / 7 / 64 / 333 / 4096 / random: the same samples", "max diff " + String (worst, 9) + (worst > 0 ? " (" + why + ")" : String()));
    }
    // 13) parameter IDs are frozen since v0.3: the layout must match Tests/param_ids.txt exactly (add new IDs at the end, never rename)
    {
        StringArray now; for (auto* prm : p.getParameters()) if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm)) now.add (rp->paramID);
        const File list = File (__FILE__).getSiblingFile ("param_ids.txt");
        if (SystemStats::getEnvironmentVariable ("RE4GE_WRITE_IDS", {}).isNotEmpty()) list.replaceWithText (now.joinIntoString ("\n") + "\n");
        StringArray frozen; frozen.addLines (list.loadFileAsString()); frozen.removeEmptyStrings();
        String missing; for (auto& id : frozen) if (! now.contains (id)) missing << id << " ";
        String extra; for (auto& id : now) if (! frozen.contains (id)) extra << id << " ";
        check (frozen.size() > 0 && missing.isEmpty() && extra.isEmpty(), "parameter IDs = Tests/param_ids.txt (frozen)",
               String (now.size()) + " IDs" + (missing.isNotEmpty() ? " MISSING: " + missing : String()) + (extra.isNotEmpty() ? " NOT IN LIST: " + extra : String()));
        int legacyOk = 0; for (auto* old : { "drive", "high_cut", "trigger", "width", "tail_highcut", "stereo_width" }) legacyOk += now.contains (ParamIDs::legacy (old)) ? 1 : 0;
        check (legacyOk == 6, "old v0.2 IDs map to v0.3 IDs (old user presets load)");
    }
    // 14b) KICK TAIL: knobs work live (PATTERN, one note for 4 beats: a TAIL DRIVE change after beat 1 is heard from beat 2 on),
    //      and no CPU spike on the note (the tail renders over the silent DELAY, not in one block)
    {
        p.setCurrentProgram (byName (p, "PUMP - QL Tail")); host.bpm = 150.0; set (p, beats, 2.f);       // BEATS 4
        p.setPlayHead (&host); p.prepareToPlay (sr, block);
        const int beatS = (int) (0.4 * sr);
        AudioBuffer<float> out (2, beatS * 4); out.clear();
        double worstMs = 0.0;
        for (int pos = 0; pos < out.getNumSamples(); pos += block)
        {
            const int n = jmin (block, out.getNumSamples() - pos);
            if (pos >= beatS && pos - block < beatS) set (p, tailDrive, 6.f);
            AudioBuffer<float> b (2, n); b.clear(); MidiBuffer m; if (pos == 0) m.addEvent (MidiMessage::noteOn (1, 31, 1.f), 0);
            const auto t0 = std::chrono::high_resolution_clock::now();
            p.processBlock (b, m);
            worstMs = jmax (worstMs, std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - t0).count());
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, n);
        }
        // beat 1 vs beat 3: different tails (drive 29.5 -> 6 dB)
        double d = 0; for (int i = 0; i < beatS; ++i) d += std::abs (out.getSample (0, i) - out.getSample (0, i + 2 * beatS));
        const double blockMs = 1000.0 * block / sr;
        check (d > 1.0, "KICK TAIL knobs work live (a change mid-note is heard on the next step)", "diff " + String (d, 2));
        check (worstMs < blockMs * 0.25, "KICK TAIL: no CPU spike on the note (worst block)", String (worstMs, 2) + " ms of " + String (blockMs, 1) + " ms");
    }
    // 14) every knob changes the sound: min vs max, in at least one of the contexts where it matters
    {
        // a short reversed-kick-like WAV for the SAMPLE source
        const File wav = outDir.getChildFile ("test_sample.wav");
        {
            AudioBuffer<float> k (2, (int) (sr * 0.6)); double ph = 0;
            for (int i = 0; i < k.getNumSamples(); ++i) { const double t = i / sr; ph += (50.0 + 250.0 * std::exp (-t * 25.0)) / sr; const float v = (float) (std::tanh (3.0 * std::sin (6.2831853 * ph)) * std::exp (-t * 3.0)) * 0.7f; k.setSample (0, i, v); k.setSample (1, i, v); }
            writeWav (wav, k);
        }
        struct Ctx { const char* preset; std::vector<std::pair<const char*, float>> extra; bool sample = false; bool gate = false; };
        const std::vector<Ctx> ctxs {
            { "PUMP - QL Tail", {} },
            { "SWELL - Synth Bend", {} },
            { "OWNER - 1", {} },
            { "INIT", { { source, 0.f }, { warp, 0.f } } },
            { "INIT", { { source, 2.f } }, true },
            { "INIT", { { timing, 1.f } } },
            { "INIT", { { timing, 1.f }, { sustain, 0.f }, { decayMs, 150.f }, { holdMs, 50.f } } },
            { "INIT", { { keyTrack, 0.f }, { pitchMode, 3.f } } },
            { "PULSE", {} },
            { "GATE", {}, false, true },
            { "GATE", { { source, 1.f } }, false, true } };
        String dead; int tested = 0;
        for (auto* prm : p.getParameters())
        {
            auto* rp = dynamic_cast<RangedAudioParameter*> (prm); if (rp == nullptr) continue;
            ++tested;
            bool moves = false;
            for (auto& cx : ctxs)
            {
                p.setCurrentProgram (byName (p, cx.preset)); host.bpm = 150.0;
                for (auto& [id, v] : cx.extra) set (p, id, v);
                if (cx.sample) p.loadSample (wav); else p.clearSample();
                auto notes = cx.gate ? std::vector<Note> { { 30.0, 500.0, 31, 0.6f } } : std::vector<Note> { { 0.0, 100.0, 31, 0.6f } };
                rp->setValueNotifyingHost (0.f); const auto a = play (p, host, notes, 0.8);
                rp->setValueNotifyingHost (1.f); const auto b = play (p, host, notes, 0.8);
                if (diff (a, b) > 1.0e-4) { moves = true; break; }
            }
            if (! moves) dead << rp->paramID << " ";
        }
        p.clearSample();
        check (dead.isEmpty(), "every parameter changes the sound (min vs max)", String (tested) + " params" + (dead.isNotEmpty() ? " DEAD: " + dead : String()));
    }
    // 15) GUI: editor stress + snapshots of the three pages
    {
        p.setCurrentProgram (byName (p, "PUMP - QL Tail"));
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (Canvas::W, Canvas::H);
        Random r (3);
        for (int it = 0; it < 150; ++it)
        {
            auto* prm = p.getParameters()[r.nextInt (p.getParameters().size())];
            prm->setValueNotifyingHost (r.nextFloat());
            AudioBuffer<float> b (2, 512); b.clear(); MidiBuffer m; if (it % 9 == 0) m.addEvent (MidiMessage::noteOn (1, 31, 1.f), 0);
            p.processBlock (b, m);
            dynamic_cast<ReEditor&> (*ed).getCanvas().tickForTest();
            if (it % 10 == 0) ed->createComponentSnapshot (ed->getLocalBounds(), true, 0.5f);
        }
        p.setCurrentProgram (byName (p, "PUMP - QL Tail"));
        play (p, host, everyBeat (1, 150.0), 0.3);
        for (int pg : { 0, 1, 2 })
        {
            dynamic_cast<ReEditor&> (*ed).getCanvas().showPageForTest (pg);
            dynamic_cast<ReEditor&> (*ed).getCanvas().tickForTest();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.f);
            const char* names[] { "gui_source.png", "gui_shape.png", "gui_tone.png" };
            PNGImageFormat png; FileOutputStream fo (outDir.getChildFile (names[pg])); fo.setPosition (0); fo.truncate(); png.writeImageToStream (img, fo);
        }
        p.setPlayHead (nullptr);
    }
    printf ("%s\n", ok ? "ALL OK" : "SOME FAILED");
    return ok ? 0 : 1;
}
