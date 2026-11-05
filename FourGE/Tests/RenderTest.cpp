// 4GE offline checks (rebuild, module 01 Core Kick Engine - its acceptance list 14.1-14.12 + the registry / GUI)
//   RenderTest <outdir>                                   tests, renders of the factory presets, GUI snapshot
//   RenderTest <outdir> --render <settings.txt> <out.wav> one kick: lines "note N", "seconds S", "<param id> <value>"
#include "../Source/PluginProcessor.h"
#include "../Source/GUI/PluginEditor.h"
#include "../Source/Presets/LayerTools.h"
#include "../Source/Presets/DistTools.h"
#include "../Source/Presets/M06Tools.h"
#include "../Source/Presets/M07Tools.h"
#include "../Source/Presets/M08Tools.h"
#include "../Source/GUI/DistScope.h"
#include "../Source/Presets/FactoryPresets.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <set>

using namespace juce;
using namespace fge;
static bool ok = true;
static void check (bool c, const char* what, const String& extra = {}) { printf ("%-62s %s %s\n", what, c ? "OK" : "FAIL", extra.toRawUTF8()); fflush (stdout); ok &= c; }
static double sr = 44100.0;

static void writeWav (const File& f, const AudioBuffer<float>& b, double rate = 44100.0)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), rate, (unsigned) b.getNumChannels(), 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}
static float peak (const AudioBuffer<float>& b, int a = 0, int n = -1)
{
    if (n < 0) n = b.getNumSamples() - a;
    float p = 0.f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch) for (int i = a; i < a + n && i < b.getNumSamples(); ++i) p = jmax (p, std::abs (b.getSample (ch, i)));
    return p;
}
static double rms (const AudioBuffer<float>& b, int a = 0, int n = -1, int ch = 0)
{
    if (n < 0) n = b.getNumSamples() - a;
    double s = 0; int c = 0;
    for (int i = a; i < a + n && i < b.getNumSamples(); ++i) { s += (double) b.getSample (ch, i) * b.getSample (ch, i); ++c; }
    return c > 0 ? std::sqrt (s / c) : 0.0;
}
static double diff (const AudioBuffer<float>& x, const AudioBuffer<float>& y)
{
    double s = 0;
    const int n = jmin (x.getNumSamples(), y.getNumSamples());
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i) { const double d = x.getSample (ch, i) - y.getSample (ch, i); s += d * d; }
    return std::sqrt (s / (2.0 * n));
}
// fundamental by autocorrelation in a window (rate-aware)
static double pitchAt (const AudioBuffer<float>& b, double rate, double fromS, double lenS)
{
    const int a = (int) (fromS * rate), n = (int) (lenS * rate);
    const float* x = b.getReadPointer (0);
    const int lo = (int) (rate / 1000), hi = (int) (rate / 25);
    if (a + n + hi >= b.getNumSamples()) return 0.0;
    std::vector<double> ac ((size_t) hi + 2, 0.0);
    for (int lag = lo - 1; lag <= hi + 1; ++lag) { double s = 0; for (int i = 0; i < n; ++i) s += x[a + i] * x[a + i + lag]; ac[(size_t) lag] = s; }
    double best = 0; int bestLag = 0;
    for (int lag = lo; lag <= hi; ++lag) if (ac[(size_t) lag] > best) { best = ac[(size_t) lag]; bestLag = lag; }
    for (int lag = lo; lag < bestLag; ++lag)
        if (ac[(size_t) lag] > 0.9 * best && ac[(size_t) lag] >= ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1]) { bestLag = lag; break; }
    if (bestLag <= lo) return 0.0;
    // parabolic refinement
    const double y0 = ac[(size_t) bestLag - 1], y1 = ac[(size_t) bestLag], y2 = ac[(size_t) bestLag + 1];
    const double d = (y0 - y2) / (2.0 * (y0 - 2.0 * y1 + y2));
    return rate / (bestLag + (std::isfinite (d) ? d : 0.0));
}
static double noteHz (int n) { return 440.0 * std::pow (2.0, (n - 69) / 12.0); }
// render through the processor (MIDI path)
static AudioBuffer<float> hitProc (GeProcessor& p, std::vector<std::pair<int, MidiMessage>> events, double seconds, double rate = 44100.0, int block = 512)
{
    p.prepareToPlay (rate, block);
    const int total = (int) (rate * seconds);
    AudioBuffer<float> out (2, total); out.clear();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = jmin (block, total - pos);
        AudioBuffer<float> b (2, n); b.clear();
        MidiBuffer m;
        for (auto& [at, msg] : events) if (at >= pos && at < pos + n) m.addEvent (msg, at - pos);
        p.processBlock (b, m);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, n);
    }
    return out;
}
static void setP (GeProcessor& p, const char* id, float v) { if (auto* a = p.apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (v)); }
static int idOf (const String& s) { for (int i = 0; i < P::count; ++i) if (s == def (i).id) return i; return -1; }

static int renderMode (const File& settings, const File& outFile)
{
    ParamFrame f = ParamFrame::defaults();
    int note = 31; double seconds = 1.0;
    StringArray lines; lines.addLines (settings.loadFileAsString());
    for (auto& l : lines)
    {
        auto t = StringArray::fromTokens (l.trim(), " \t", "");
        if (t.size() < 2) continue;
        if (t[0] == "note") note = t[1].getIntValue();
        else if (t[0] == "seconds") seconds = t[1].getDoubleValue();
        else if (int id = idOf (t[0]); id >= 0) f[id] = (float) t[1].getDoubleValue();
        else printf ("unknown parameter %s\n", t[0].toRawUTF8());
    }
    writeWav (outFile, renderKick (f, note, sr, seconds));
    return 0;
}

// module 05: all 16 distortion stages (4 x 16x oversampling, parallel, multiband, pre / post, modulation) - CPU test + --bench
static void cpuStress (GeProcessor& p)
{
    for (int st = 0; st < 16; ++st)
    {
        const String o = "dist" + String (st + 1) + "_";
        setP (p, (o + "on").toRawUTF8(), 1.f); setP (p, (o + "location").toRawUTF8(), (float) (st / 4 == 0 ? 0 : st / 4 == 1 ? 3 : st / 4 == 2 ? 2 : 1));
        setP (p, (o + "type").toRawUTF8(), (float) (st % 10)); setP (p, (o + "drive").toRawUTF8(), 0.6f);
        if (st < 4) setP (p, (o + "oversampling").toRawUTF8(), 2.f);
        if (st / 4 == 2) setP (p, (o + "path").toRawUTF8(), (float) (st % 3 + 1));
        if (st / 4 == 3) setP (p, (o + "band").toRawUTF8(), (float) (st % 3 + 1));
        setP (p, (o + "pre_on").toRawUTF8(), 1.f); setP (p, (o + "post_on").toRawUTF8(), 1.f); setP (p, (o + "mod_drive").toRawUTF8(), 0.3f);
    }
}
static double cpuTime (GeProcessor& pp, double seconds)
{
    pp.prepareToPlay (sr, 512);
    const auto t0 = Time::getMillisecondCounterHiRes();
    for (int blk = 0; blk < (int) (seconds * sr / 512); ++blk)
    {
        AudioBuffer<float> bb (2, 512); bb.clear(); MidiBuffer m;
        if (blk % 40 == 0) m.addEvent (MidiMessage::noteOn (1, 31, 1.f), 0);
        pp.processBlock (bb, m);
    }
    return (Time::getMillisecondCounterHiRes() - t0) / (1000.0 * seconds);      // seconds of CPU per second of audio
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory().getChildFile ("4ge_out"));
    outDir.createDirectory();
    if (argc > 2 && String (argv[2]) == "--bench") { GeProcessor p; cpuStress (p); printf ("%.1f %% of one core\n", 100.0 * cpuTime (p, argc > 3 ? String (argv[3]).getDoubleValue() : 2.0)); return 0; }
    if (argc > 4 && String (argv[2]) == "--render") return renderMode (File::getCurrentWorkingDirectory().getChildFile (argv[3]), File::getCurrentWorkingDirectory().getChildFile (argv[4]));

    const ParamFrame base = ParamFrame::defaults();
    const int G1 = 31;

    // ---------------------------------------------------------------- 0. registry / frozen IDs
    {
        const File ids = File (__FILE__).getSiblingFile ("param_ids.txt");
        StringArray frozen; frozen.addLines (ids.loadFileAsString()); frozen.removeEmptyStrings();
        bool same = true;
        for (int i = 0; i < frozen.size(); ++i) same &= i < P::count && frozen[i] == def (i).id;
        StringArray all; for (int i = 0; i < P::count; ++i) all.add (def (i).id);
        std::set<std::string> uniq; for (auto& s : all) uniq.insert (s.toStdString());
        check (same && (int) uniq.size() == P::count, "REGISTRY: frozen IDs unchanged, only appended, unique",
               String (frozen.size()) + " frozen / " + String (P::count) + " now");
        if (frozen.size() < P::count) ids.replaceWithText (all.joinIntoString ("\n") + "\n");      // append the new ones
        GeProcessor p;
        check ((int) p.getParameters().size() == P::count, "REGISTRY: the APVTS has every parameter, in registry order",
               String (p.getParameters().size()));
    }

    // ---------------------------------------------------------------- 14.1 a complete kick from synthesis only
    const auto kick = renderKick (base, G1, sr, 1.0);
    {
        const float pk = peak (kick);
        int last = kick.getNumSamples() - 1; while (last > 0 && std::abs (kick.getSample (0, last)) < 1.0e-3f) --last;
        const double lenMs = last * 1000.0 / sr;
        check (pk > 0.5f && pk < 1.4f && lenMs > 400.0 && lenMs < 460.0, "14.1 INIT = a complete kick from synthesis (no sample)",
               "peak " + String (pk, 2) + ", " + String (lenMs, 0) + " ms");
        writeWav (outDir.getChildFile ("00_INIT.wav"), kick);
    }
    // ---------------------------------------------------------------- 14.2 the four parts are independent (full = sum of the solos)
    {
        AudioBuffer<float> sum (2, kick.getNumSamples()); sum.clear();
        String e;
        bool each = true;
        for (int p = 0; p < 4; ++p)
        {
            auto s = renderKick (base, G1, sr, 1.0, p);
            each &= rms (s) > 1.0e-3;
            e << String (Decibels::gainToDecibels ((float) rms (s)), 1) << " ";
            for (int ch = 0; ch < 2; ++ch) sum.addFrom (ch, 0, s, ch, 0, s.getNumSamples());
        }
        const double d = diff (sum, kick) / jmax (1.0e-9, rms (kick));
        check (each && d < 1.0e-3, "14.2 PUNCH / BODY / TAIL / SUB each sound alone, full = their sum", "rms dB " + e + " rel diff " + String (d, 6));
    }
    // ---------------------------------------------------------------- 14.3 SOLO / MUTE per part
    {
        bool good = true; String e;
        const int mute[4] { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute };
        const int solo[4] { P::mixPunchSolo, P::mixBodySolo, P::mixTailSolo, P::mixSubSolo };
        for (int p = 0; p < 4; ++p)
        {
            ParamFrame m = base; m[mute[p]] = 1.f;
            auto muted = renderKick (m, G1, sr, 1.0);
            auto alone = renderKick (base, G1, sr, 1.0, p);
            AudioBuffer<float> rest (kick); for (int ch = 0; ch < 2; ++ch) rest.addFrom (ch, 0, alone, ch, 0, alone.getNumSamples(), -1.f);
            const double dm = diff (muted, rest) / jmax (1.0e-9, rms (kick));
            ParamFrame s = base; s[solo[p]] = 1.f;
            const double ds = diff (renderKick (s, G1, sr, 1.0), alone) / jmax (1.0e-9, rms (alone));
            good &= dm < 1.0e-3 && ds < 1.0e-3;
            e << String (dm, 5) << "/" << String (ds, 5) << " ";
        }
        ParamFrame all = base; for (int p = 0; p < 4; ++p) all[mute[p]] = 1.f;
        good &= peak (renderKick (all, G1, sr, 0.5)) < 1.0e-6f;
        check (good, "14.3 MUTE removes exactly that part, SOLO plays only it, all muted = silence", e);
    }
    // ---------------------------------------------------------------- 14.4 LENGTH does not stretch the PUNCH
    {
        ParamFrame a = base, b = base; a[P::coreLength] = 250.f; b[P::coreLength] = 1500.f;
        const double dp = diff (renderKick (a, G1, sr, 2.0, 0), renderKick (b, G1, sr, 2.0, 0));
        auto ta = renderKick (a, G1, sr, 2.0, 2), tb = renderKick (b, G1, sr, 2.0, 2);
        auto end = [] (const AudioBuffer<float>& x) { int l = x.getNumSamples() - 1; while (l > 0 && std::abs (x.getSample (0, l)) < 1.0e-3f) --l; return l * 1000.0 / sr; };
        ParamFrame off = b; off[P::coreAutoLen] = 0.f;
        const double tOff = end (renderKick (off, G1, sr, 2.0, 2));
        check (dp < 1.0e-6 && end (tb) > 3.0 * end (ta) && std::abs (tOff - 420.0) < 15.0,
               "14.4 LENGTH: punch untouched, tail follows (AUTO), AUTO OFF keeps own length",
               "punch diff " + String (dp, 8) + ", tail " + String (end (ta), 0) + " -> " + String (end (tb), 0) + " ms, AUTO OFF " + String (tOff, 0) + " ms");
    }
    // ---------------------------------------------------------------- 14.5 every part has a working envelope
    {
        bool good = true; String e;
        const int dec[4] { P::punchDecay, P::bodyDecay, P::tailDecay, P::subDecay };
        const int att[4] { P::punchAttack, P::bodyAttack, P::tailAttack, P::subAttack };
        for (int p = 0; p < 4; ++p)
        {
            ParamFrame s = base, l = base;
            s[P::punchSustain] = s[P::bodySustain] = s[P::tailSustain] = s[P::subSustain] = 0.05f;
            s[P::punchLen] = 150.f; s[P::coreAutoLen] = 0.f; s[P::coreLength] = 2000.f;
            l = s;
            s[dec[p]] = def (dec[p]).min + 4.f; l[dec[p]] = def (dec[p]).max * 0.3f;
            auto xs = renderKick (s, G1, sr, 1.0, p), xl = renderKick (l, G1, sr, 1.0, p);
            const int w = (int) ((p == 0 ? 0.012 : 0.06) * sr);
            const double rS = rms (xs, w, w), rL = rms (xl, w, w);
            ParamFrame a = base; a[att[p]] = def (att[p]).max; a[P::punchLen] = 150.f;
            auto xa = renderKick (a, G1, sr, 1.0, p), x0 = renderKick (base, G1, sr, 1.0, p);
            const int w2 = (int) (0.0015 * sr);
            const double aSlow = rms (xa, 0, w2), aFast = rms (x0, 0, w2);
            good &= rL > rS * 1.5 && aFast > aSlow * 1.5;
            e << String (rL / jmax (1.0e-9, rS), 1) << "x/" << String (aFast / jmax (1.0e-9, aSlow), 1) << "x ";
        }
        check (good, "14.5 DECAY and ATTACK change every part's envelope", e);
    }

    // ---------------------------------------------------------------- 14.6 the pitch engine really moves the pitch (and lands on the note)
    {
        ParamFrame f = base; f[P::tailChar] = 0.f; f[P::tailDrive] = 0.f; f[P::tailTone] = 20000.f; f[P::tailReso] = 0.f;
        f[P::coreLength] = 900.f; f[P::peTime] = 120.f;
        auto x = renderKick (f, G1, sr, 1.2, 2);
        const double early = pitchAt (x, sr, 0.004, 0.012), late = pitchAt (x, sr, 0.45, 0.12);
        auto xa = renderKick (f, 33, sr, 1.2, 2);
        const double lateA = pitchAt (xa, sr, 0.45, 0.12);
        ParamFrame st = f; st[P::peMode] = 5.f;
        const double stable = pitchAt (renderKick (st, G1, sr, 1.2, 2), sr, 0.004, 0.03);
        ParamFrame rise = f; rise[P::peMode] = 4.f; rise[P::peStart] = 98.f; rise[P::peTime] = 300.f;
        auto xr = renderKick (rise, G1, sr, 1.2, 2);
        const double r0 = pitchAt (xr, sr, 0.01, 0.06), r1 = pitchAt (xr, sr, 0.45, 0.1);
        check (early > 120.0 && std::abs (late - noteHz (G1)) < 1.0 && std::abs (lateA - noteHz (33)) < 1.2 && std::abs (stable - noteHz (G1)) < 1.5 && r1 > r0 * 1.6,
               "14.6 PITCH: drops START -> note (G1 / A1), STABLE stays, RISE rises",
               String (early, 0) + " -> " + String (late, 1) + " Hz (G1 " + String (noteHz (G1), 1) + "), A1 " + String (lateA, 1)
               + ", stable " + String (stable, 1) + ", rise " + String (r0, 0) + " -> " + String (r1, 0));
        // the modes are different curves
        String e; bool differ = true;
        std::vector<AudioBuffer<float>> m;
        for (int i = 0; i < 6; ++i) { ParamFrame q = f; q[P::peMode] = (float) i; m.push_back (renderKick (q, G1, sr, 0.5, 2)); }
        for (int i = 0; i < 6; ++i) for (int j = i + 1; j < 6; ++j) { const double d = diff (m[(size_t) i], m[(size_t) j]); differ &= d > 1.0e-3; }
        check (differ, "14.6 the 6 pitch modes are 6 different movements");
        // tracking off: fixed Hz, the note does not matter
        ParamFrame fx = f; fx[P::coreKeyTrack] = 0.f; fx[P::peEnd] = 55.f;
        const double fixedLate = pitchAt (renderKick (fx, 40, sr, 1.2, 2), sr, 0.45, 0.12);
        check (std::abs (fixedLate - 55.0) < 1.2, "PITCH TRACKING off = END PITCH in Hz on any note", String (fixedLate, 1) + " Hz");
        // the sub sits on the note
        const double subP = pitchAt (renderKick (base, 33, sr, 1.0, 3), sr, 0.1, 0.12);
        check (std::abs (subP - noteHz (33)) < 1.0, "SUB on the played note (tracking)", String (subP, 1) + " Hz");
    }
    // ---------------------------------------------------------------- tail CHARACTER: 7 different algorithms
    {
        std::vector<std::vector<double>> prof;
        for (int c = 0; c < 7; ++c)
        {
            ParamFrame f = base; f[P::tailChar] = (float) c; f[P::tailDrive] = 0.7f;
            auto x = renderKick (f, G1, sr, 0.5, 2);
            // harmonic profile 1..12 at 0.15-0.35 s (Goertzel on the note)
            std::vector<double> h;
            const int a = (int) (0.15 * sr), n = (int) (0.2 * sr);
            for (int k = 1; k <= 12; ++k)
            {
                const double w = 2.0 * MathConstants<double>::pi * noteHz (G1) * k / sr;
                double re = 0, im = 0;
                for (int i = 0; i < n; ++i) { re += x.getSample (0, a + i) * std::cos (w * i); im += x.getSample (0, a + i) * std::sin (w * i); }
                h.push_back (10.0 * std::log10 (re * re + im * im + 1.0e-12));
            }
            const double h0 = h[0];
            for (auto& v : h) v -= h0;
            prof.push_back (h);
        }
        double minD = 1e9; String pair;
        for (size_t i = 0; i < prof.size(); ++i) for (size_t j = i + 1; j < prof.size(); ++j)
        {
            double d = 0; for (size_t k = 1; k < 12; ++k) d += std::abs (prof[i][k] - prof[j][k]);
            if (d / 11.0 < minD) { minD = d / 11.0; pair = Choices::tailCharacters()[(int) i] + "/" + Choices::tailCharacters()[(int) j]; }
        }
        check (minD > 1.5, "TAIL CHARACTER: 7 modes have 7 different harmonic profiles", "closest " + pair + " " + String (minD, 1) + " dB / harmonic");
    }
    // ---------------------------------------------------------------- 14.7 MIDI Note On triggers, Note Off does not cut
    {
        GeProcessor p;
        auto silent = hitProc (p, {}, 0.3);
        auto hit = hitProc (p, { { 100, MidiMessage::noteOn (1, G1, 1.f) } }, 0.6);
        auto off = hitProc (p, { { 100, MidiMessage::noteOn (1, G1, 1.f) }, { 2300, MidiMessage::noteOff (1, G1) } }, 0.6);
        check (peak (silent) == 0.f && peak (hit) > 0.2f && diff (hit, off) < 1.0e-7, "14.7 Note On plays the kick, Note Off never cuts it",
               "peak " + String (peak (hit), 2) + ", off diff " + String (diff (hit, off), 9));
        // velocity sensitivity
        setP (p, "core_velocity", 1.f);
        auto soft = hitProc (p, { { 0, MidiMessage::noteOn (1, G1, 0.5f) } }, 0.4);
        auto hard = hitProc (p, { { 0, MidiMessage::noteOn (1, G1, 1.f) } }, 0.4);
        check (std::abs (peak (soft) / peak (hard) - 0.5f) < 0.05f, "VELOCITY sensitivity scales the kick", String (peak (soft) / peak (hard), 3));
    }
    // ---------------------------------------------------------------- 14.8 overlapping triggers: CHOKE and POLY, no errors, no runaway
    {
        GeProcessor p;
        std::vector<std::pair<int, MidiMessage>> ev;
        for (int i = 0; i < 12; ++i) ev.push_back ({ i * 1500 + (i % 3) * 7, MidiMessage::noteOn (1, G1 + (i % 3) * 2, 1.f) });
        auto choke = hitProc (p, ev, 1.2);
        setP (p, "core_voice_mode", 1.f);
        auto poly = hitProc (p, ev, 1.2);
        bool finite = true;
        for (auto* b : { &choke, &poly }) for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < b->getNumSamples(); ++i) finite &= std::isfinite (b->getSample (ch, i));
        // the choke fade is smooth: no sample jump bigger than the kick's own steepest slope x 1.5
        double maxStep = 0, maxStepSingle = 0;
        for (int i = 1; i < choke.getNumSamples(); ++i) maxStep = jmax (maxStep, (double) std::abs (choke.getSample (0, i) - choke.getSample (0, i - 1)));
        for (int i = 1; i < kick.getNumSamples(); ++i) maxStepSingle = jmax (maxStepSingle, (double) std::abs (kick.getSample (0, i) - kick.getSample (0, i - 1)));
        check (finite && peak (choke) < 3.f && peak (poly) < 8.f && peak (poly) > peak (choke) * 1.05f && maxStep < maxStepSingle * 1.6,
               "14.8 12 fast overlapping kicks: CHOKE / POLY stable, choke without jumps",
               "peaks " + String (peak (choke), 2) + " / " + String (peak (poly), 2) + ", step " + String (maxStep / maxStepSingle, 2) + "x");
    }
    // ---------------------------------------------------------------- 14.9 sample rates
    {
        String e; bool good = true;
        double ref = 0; float refPk = 0.f;
        for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        {
            ParamFrame f = base;
            auto x = renderKick (f, G1, rate, 0.8);
            const double pl = pitchAt (renderKick (f, G1, rate, 0.8, 2), rate, 0.25, 0.1);
            const float pk = peak (x);
            if (ref == 0) { ref = pl; refPk = pk; }
            good &= std::abs (pl - ref) < 0.5 && std::abs (Decibels::gainToDecibels (pk / refPk)) < 0.6f;
            e << String (rate / 1000.0, 1) << "k: " << String (pl, 1) << " Hz " << String (Decibels::gainToDecibels (pk), 1) << " dB  ";
        }
        check (good, "14.9 44.1 / 48 / 88.2 / 96 kHz: same pitch, same level", e);
    }
    // ---------------------------------------------------------------- 14.10 no clicks: start, end of the kick, end of each part
    {
        // the slope right at the very end (last 3 ms before silence) must be tiny vs the peak
        int last = kick.getNumSamples() - 1; while (last > 0 && std::abs (kick.getSample (0, last)) < 1.0e-5f) --last;
        double endStep = 0;
        for (int i = last - (int) (0.003 * sr); i <= last; ++i) endStep = jmax (endStep, (double) std::abs (kick.getSample (0, i)));
        double startJump = std::abs (kick.getSample (0, 0)) + std::abs (kick.getSample (1, 0));
        // short parts with abrupt settings: attack 0, release 0
        ParamFrame f = base; f[P::punchRelease] = 0.f; f[P::bodyRelease] = 0.f; f[P::tailRelease] = 0.f; f[P::subRelease] = 0.f;
        f[P::punchAttack] = 0.f; f[P::tailAttack] = 0.f; f[P::coreAutoLen] = 0.f;
        auto x = renderKick (f, G1, sr, 1.0);
        // energy above 8 kHz in a 5 ms window around each part's end vs the same window 20 ms earlier
        bool good = true; String e;
        for (int p = 0; p < 4; ++p)
        {
            const float endMs = p == 0 ? f[P::punchLen] : p == 1 ? f[P::bodyLen] : p == 2 ? f[P::tailLen] : f[P::subLen];
            auto s = renderKick (f, G1, sr, 1.0, p);
            const int at = (int) (endMs * 0.001 * sr);
            // 4th-order high-pass at 3 kHz: what a click adds
            doomsday::dsp::Svf h1, h2;
            const auto c3 = doomsday::dsp::svfCoefs (3000.f, 1.414f, sr);
            double hfEnd = 0, hfBefore = 0;
            for (int i = 0; i < at + 400 && i < s.getNumSamples(); ++i)
            {
                const float y = h2.hp (h1.hp (s.getSample (0, i), c3), c3);
                if (i > at - 200) hfEnd = jmax (hfEnd, (double) std::abs (y));
                else if (i > at - 1400 && i < at - 600) hfBefore = jmax (hfBefore, (double) std::abs (y));
            }
            // a click = more highs at the end than the part had just before (or, for a clean part, highs at all)
            const double rel = hfEnd / jmax (1.0e-9, (double) peak (s));
            good &= hfEnd < hfBefore * 1.2 + 0.003 * peak (s);
            e << String (Decibels::gainToDecibels ((float) rel), 0) << " dB ";
        }
        check (endStep < 0.01 * peak (kick) && startJump < 0.05 && good, "14.10 no clicks at the start, the end, the end of each part (release 0)",
               "end " + String (endStep, 5) + ", start " + String (startJump, 4) + ", part ends " + e);
    }
    // ---------------------------------------------------------------- 14.11 every visible parameter changes the sound
    {
        StringArray dead;
        int tested = 0;
        const bool quick = std::getenv ("FGE_QUICK") != nullptr;          // (developer runs: skip this long check)
        const StringArray only = StringArray::fromTokens (String (std::getenv ("FGE_ONLY") != nullptr ? std::getenv ("FGE_ONLY") : ""), ",", "");   // (developer runs: these prefixes only)
        for (int id = quick ? P::count : 0; id < P::count; ++id)
        {
            const auto& d = def (id);
            if (! only.isEmpty() && [&] { for (auto& o : only) if (String (d.id).startsWith (o)) return false; return true; }()) continue;
            ParamFrame a = base, b = base;
            // context: parameters that only work in a mode get that mode (the GUI greys them out otherwise)
            if (id == P::tailPw) { a[P::tailSrc] = b[P::tailSrc] = 4.f; }
            if (id == P::subPitch) { a[P::coreKeyTrack] = b[P::coreKeyTrack] = 0.f; }
            if (id == P::bodyReso || id == P::tailReso) { a[id - 1] = b[id - 1] = 1500.f; }
            if (id == P::bodyMix || id == P::punchMix) { a[id - 1] = b[id - 1] = 0.8f; }
            if (id == P::tailMix) { a[P::tailDrive] = b[P::tailDrive] = 0.8f; }
            if (id == P::corePhaseRst) { /* needs two kicks: checked below */ continue; }
            if (id == P::coreVoice || id == P::coreVelocity) continue;                 // MIDI behaviour, checked above
            if (id == P::coreAutoLen) { a[P::coreLength] = b[P::coreLength] = 1200.f; }
            if (id == P::peEnd || id == P::subPitch) { a[P::coreKeyTrack] = b[P::coreKeyTrack] = 0.f; }
            // module 02 contexts: an envelope slot's settings work when the slot is ON; FOLLOW settings when the link is FOLLOW;
            // the sub's own pitch envelope when it has a DEPTH; SYNC / LOCAL BPM with the global SYNC on (tempo != 150)
            const String sid (d.id);
            auto slotBase = [&] (const String& pre) { for (int k = 0; k < P::count; ++k) if (String (def (k).id) == pre + "_on") return k; return -1; };
            for (auto* pre : { "punch_filter_env", "body_filter_env", "tail_filter_env", "punch_drive_env", "body_drive_env", "tail_drive_env",
                               "sub_drive_env", "body_reso_env", "tail_reso_env" })
                if (sid.startsWith (pre) && ! sid.endsWith ("_on")) { const int on = slotBase (pre); a[on] = b[on] = 1.f; a[on + 1] = b[on + 1] = 0.f;
                                                                        if (sid.contains ("filter")) { a[id - 0] = a[id]; } }
            if (sid.contains ("drive_env")) { a[P::tailChar] = b[P::tailChar] = 1.f; a[P::punchDrive] = b[P::punchDrive] = a[P::bodyDrive] = b[P::bodyDrive] = a[P::tailDrive] = b[P::tailDrive] = a[P::subDrive] = b[P::subDrive] = 0.3f; }
            if (sid.contains ("filter_env") || sid.contains ("reso_env")) { a[P::punchTone] = b[P::punchTone] = a[P::bodyTone] = b[P::bodyTone] = a[P::tailTone] = b[P::tailTone] = 2000.f; a[P::bodyReso] = b[P::bodyReso] = a[P::tailReso] = b[P::tailReso] = 0.4f;
                                                                            a[P::bodySrc] = b[P::bodySrc] = 2.f; a[P::bodyLevel] = b[P::bodyLevel] = 0.f; }
            if (sid.endsWith ("_env_mode") && ! sid.contains ("amp")) {}                       // DRAWN vs ADSR differ with the default shapes
            if (sid.endsWith ("_follow_depth") || sid.endsWith ("_follow_offset") || sid.endsWith ("_follow_time") || sid.endsWith ("_link_source"))
            {
                const String part = sid.upToFirstOccurrenceOf ("_", false, false);
                const int lk = idOf (part + "_link");
                a[lk] = b[lk] = 1.f;                                                                 // FOLLOW
                const int srcId = idOf (part + "_link_source");
                if (! sid.endsWith ("_link_source")) a[srcId] = b[srcId] = part == "tail" ? 0.f : 2.f;
                a[P::subPDepth] = b[P::subPDepth] = -12.f;                                            // the sub moves too (a source with a movement)
            }
            if (sid == "tail_link") { a[P::tailLinkSrc] = b[P::tailLinkSrc] = 0.f; }
            if (sid.startsWith ("sub_penv") || sid == "sub_link") { a[P::subPDepth] = b[P::subPDepth] = -12.f; if (sid == "sub_penv_depth") { a[id] = 0.f; } }
            if (sid == "sub_link") { a[P::subLinkSrc] = b[P::subLinkSrc] = 1.f; }
            if (sid.endsWith ("_sync") || sid == "env_local_bpm")
            {
                a[P::envSync] = b[P::envSync] = 1.f; a[P::envLocalBpm] = b[P::envLocalBpm] = 100.f;
                if (sid.endsWith ("_sync") && sid != "env_sync") { const String pre = sid.dropLastCharacters (5); const int on = idOf (pre + "_on"); if (on >= 0) a[on] = b[on] = 1.f; }
                if (sid == "env_sync") { for (auto* q : { "tail_amp_env_sync" }) a[idOf (q)] = b[idOf (q)] = 1.f; }
                if (sid == "env_local_bpm") { a[idOf ("tail_amp_env_sync")] = b[idOf ("tail_amp_env_sync")] = 1.f; }
            }
            if (sid.startsWith ("tail_st2_") && sid != "tail_st2_type") { a[P::tailSt2Type] = b[P::tailSt2Type] = 3.f; }
            if (sid == "tail_mid_hz" || sid == "tail_mid_q") { a[P::tailMidGain] = b[P::tailMidGain] = 9.f; }
            if (sid == "tail_hold_level") { a[P::tailPOffset] = b[P::tailPOffset] = 100.f; a[id] = -12.f; }
            if (sid == "master_clip_soft") { a[P::clipDrive] = b[P::clipDrive] = 12.f; }
            // module 03 contexts: the oscillator is ON (routed to the tail by default) and in the mode the control belongs to
            if (sid.startsWith ("osc") || sid.startsWith ("fm_"))
            {
                auto set2 = [&] (const String& pid, float v) { const int k = idOf (pid); if (k >= 0 && k != id) { a[k] = v; b[k] = v; } };
                const String n = sid.substring (3, 4);          // osc N / FM modulator N
                const String o = "osc" + n + "_";
                const String other = "osc" + String (n == "1" ? 2 : 1) + "_";
                set2 (o + "on", 1.f);
                const String k = sid.fromFirstOccurrenceOf ("_", false, false);
                if (k == "noise") set2 (o + "wave", 5.f);
                if (k == "solo") set2 (other + "on", 1.f);
                if (k == "freq") set2 (o + "track", 4.f);
                if (k == "pw") set2 (o + "wave", 4.f);
                if (k == "phase_random") set2 (o + "phase_mode", 1.f);
                if (k == "phase_source") { set2 (o + "phase_mode", 3.f); set2 ("osc2_on", 1.f); set2 ("osc2_phase", 90.f); set2 ("osc3_on", 1.f); set2 ("osc3_phase", 200.f); }
                if (k == "detune" || k == "spread" || k == "blend" || k == "unison_mode" || k == "unison_phase" || k == "seed") set2 (o + "unison", 5.f);
                if (k == "fm_env") set2 (o + "fm_feedback", 0.6f);
                if (k == "mod_decay") { set2 (o + "fm_feedback", 0.6f); set2 (o + "fm_env", 1.f); }
                if (k.startsWith ("ring_") && k != "ring_source") set2 (o + "ring_source", 5.f);
                if (k == "ring_source") set2 (other + "on", 1.f);
                if (k.startsWith ("sync_")) { set2 (other + "on", 1.f); set2 (other + "wave", 0.f); if (k != "sync_source") set2 (o + "sync_source", (float) (n == "1" ? 2 : 1)); }
                if (k.startsWith ("wt_")) { set2 (o + "wave", 7.f); if (k == "wt_start" || k == "wt_end" || k == "wt_scan") set2 (o + "wt_mode", 1.f);
                                            if (k == "wt_morph") set2 (o + "wt_position", 0.4f); }
                if (k.startsWith ("h_")) set2 (o + "wave", 8.f);
                if (sid.startsWith ("fm_")) { const String c = "osc" + sid.substring (8, 9) + "_"; set2 (c + "on", 1.f); set2 (o + "to_tail", -60.f); set2 (o + "wave", 0.f); }
                a[P::tailChar] = b[P::tailChar] = 0.f;                              // clean tail: the oscillator itself is heard
            }
            // module 04 contexts
            {
                auto set2 = [&] (const String& pid, float v) { const int k = idOf (pid); if (k >= 0 && k != id) { a[k] = v; b[k] = v; } };
                if (sid.startsWith ("noise"))
                {
                    const String o = sid.substring (0, 7);                          // "noiseA_" / "noiseB_"
                    const String k = sid.substring (7);
                    set2 (o + "on", 1.f);
                    if (k.startsWith ("res_") || k == "metal") set2 (o + "type", 5.f);
                    if (k == "bits" || k == "sr_reduce" || k == "quant") set2 (o + "type", 6.f);
                    if (k == "resonance" || k == "mod_cutoff" || k == "mod_resonance") set2 (o + "cutoff", 2000.f);
                    if (k == "mix") set2 (o + "drive", 0.8f);
                    if (k.startsWith ("mod_")) { set2 (o + "mod_on", 1.f); if (k == "mod_on" || k == "mod_mode" || k == "mod_attack" || k == "mod_decay") set2 (o + "mod_level", 1.f); }
                    if (k == "track") continue;                                      // tracking is identity at G1: checked on another note below
                }
                if (sid.startsWith ("tr_") || sid.startsWith ("shaper_"))
                {
                    set2 ("tr_on", 1.f);
                    if (sid == "tr_sync") set2 ("tail_penv_offset", 60.f);
                    if (sid == "tr_attack_time" || sid == "tr_decay_time") set2 ("tr_attack_level", 12.f);
                    if (sid.startsWith ("shaper_")) { set2 ("shaper_on", 1.f); if (sid != "shaper_attack") set2 ("shaper_attack", 0.8f); }
                }
                if (sid.startsWith ("layer") && sid != "layer_solo_mode")
                {
                    const String o = sid.upToFirstOccurrenceOf ("_", true, false);  // "layerN_"
                    const String k = sid.fromFirstOccurrenceOf ("_", false, false);
                    const String other = o == "layer1_" ? "layer2_" : "layer1_";
                    set2 (o + "on", 1.f);
                    if (k == "filter_type" || k == "cutoff" || k == "resonance") set2 (o + "filter_on", 1.f);
                    if (k == "sustain" || k == "release") set2 (o + "env_mode", 0.f);
                    if (k == "solo") set2 (other + "on", 1.f);
                    if (k.startsWith ("send_")) set2 (o + "route_mode", 1.f);
                    if (k == "send_pre") { set2 (o + "send_texture", 0.f); }
                    if (k.startsWith ("link_"))
                    {
                        if (k != "link_source") set2 (o + "link_source", (float) (o == "layer1_" ? 2 : 1));
                        else set2 (o + "link_level", 1.f);
                        set2 (other + "level", -20.f); set2 (other + "pitch", 12.f); set2 (other + "length", 100.f); set2 (other + "env_mode", 0.f);
                    }
                }
                if (sid.startsWith ("group_"))
                {
                    const String g = sid.fromFirstOccurrenceOf ("_", false, false).upToFirstOccurrenceOf ("_", false, false);
                    const int gi = Choices::layerGroups().indexOf (g.toUpperCase());
                    set2 ("layer1_on", 1.f); set2 ("layer1_group", (float) gi);
                    if (sid.endsWith ("_width")) set2 ("layer1_width", 0.8f);
                    if (sid.endsWith ("_solo")) { set2 ("layer2_on", 1.f); set2 ("layer2_group", (float) (gi == 4 ? 5 : 4)); }
                }
                if (sid == "bus_texture_level") set2 ("layer1_on", 1.f);
                if (sid == "bus_custom_level") { set2 ("layer1_on", 1.f); set2 ("layer1_group", 5.f); }
                if (sid == "layer_solo_mode") { set2 ("layer1_on", 1.f); set2 ("layer1_solo", 1.f); set2 ("layer1_filter_on", 1.f); set2 ("layer1_drive", 0.6f); }
            }
            // module 05 contexts: the stage is ON (GLOBAL, drive up) and set to the type / mode the control belongs to
            {
                auto set2 = [&] (const String& pid, float v) { const int k = idOf (pid); if (k >= 0 && k != id) { a[k] = v; b[k] = v; } };
                if (sid.startsWith ("dist") && ! sid.startsWith ("dist_"))
                {
                    const String o = sid.upToFirstOccurrenceOf ("_", true, false);      // "distN_"
                    const String k = sid.fromFirstOccurrenceOf ("_", false, false);
                    const String other = o == "dist1_" ? "dist2_" : "dist1_";
                    set2 (o + "on", 1.f); set2 (o + "drive", 0.7f);
                    auto type = [&] (int t) { set2 (o + "type", (float) t); };
                    if (k == "ceiling") type (1);
                    if (k == "even" || k == "odd") type (2);
                    if (k == "fold_stages") { type (4); set2 (o + "drive", 1.f); set2 (o + "shape", 1.f); }
                    if (k == "bits" || k == "sr_reduce" || k == "dither") { type (5); if (k == "dither") set2 (o + "bits", 4.f); }
                    if (k == "rect_mode") type (6);
                    if (k == "neg_drive" || k == "neg_threshold") type (7);
                    if (k == "curve_interp") type (10);
                    if (k == "morph_type") set2 (o + "morph", 0.5f);
                    if (k == "path") { set2 (other + "on", 1.f); set2 (other + "type", 4.f); set2 (other + "drive", 0.8f); set2 (other + "path", 2.f); }   // the other in PARALLEL B
                    if (k == "order") { type (1); set2 (other + "on", 1.f); set2 (other + "type", 4.f); set2 (other + "drive", 0.8f); set2 (other + "order", 8.f); }
                    if (k.startsWith ("pre_") && k != "pre_on") set2 (o + "pre_on", 1.f);
                    if (k == "pre_resonance" || k == "pre_drive" || k == "pre_slope") set2 (o + "pre_cutoff", 1500.f);
                    if (k.startsWith ("post_") && k != "post_on") set2 (o + "post_on", 1.f);
                    if (k == "post_resonance" || k == "post_drive" || k == "post_slope" || k == "post_mix") set2 (o + "post_cutoff", 1500.f);
                    if (k == "mod_source" || k == "mod_attack" || k == "mod_decay") set2 (o + "mod_drive", 0.8f);
                    if (k == "mod_cutoff") { set2 (o + "post_on", 1.f); set2 (o + "post_cutoff", 1500.f); }
                    if (k == "mod_fold") type (4);
                    if (k == "mod_mix") set2 (o + "mix", 0.5f);
                    if (k == "oversampling") { type (1); set2 (o + "drive", 1.f); }
                }
                if (sid == "dist_xover_low" || sid == "dist_xover_high" || sid == "dist_band_solo")
                { set2 ("dist1_on", 1.f); set2 ("dist1_drive", 1.f); set2 ("dist1_band", sid == "dist_xover_high" ? 3.f : 1.f);
                  if (sid == "dist_band_solo") { set2 ("dist2_on", 1.f); set2 ("dist2_band", 3.f); } }
                if (sid == "dist_global_position" || sid == "dist_offline_quality") { set2 ("dist1_on", 1.f); set2 ("dist1_type", 1.f); set2 ("dist1_drive", 1.f); }
            }
            // module 06 contexts: the filter / resonator / screech is ON and in the mode the control belongs to
            {
                auto set2 = [&] (const String& pid, float v) { const int k = idOf (pid); if (k >= 0 && k != id) { a[k] = v; b[k] = v; } };
                const bool isFlt = sid.startsWith ("flt") && sid.containsChar ('_') && sid.substring (3, 4).containsOnly ("12345678");
                const bool isRes = sid.startsWith ("res") && sid.substring (3, 4).containsOnly ("12") && sid.containsChar ('_');
                if (isFlt || isRes)
                {
                    const String o = sid.upToFirstOccurrenceOf ("_", true, false);
                    const String k = sid.fromFirstOccurrenceOf ("_", false, false);
                    const String other = isFlt ? (o == "flt1_" ? "flt2_" : "flt1_") : (o == "res1_" ? "res2_" : "res1_");
                    set2 (o + "on", 1.f);
                    if (k == "position") { set2 ("dist1_on", 1.f); set2 ("dist1_location", 3.f); set2 ("dist1_drive", 0.9f); set2 ("dist1_type", 1.f); }
                    if (isFlt)
                    {
                        if (k == "svf_mode") set2 (o + "type", 6.f);
                        if (k == "vowel") set2 (o + "type", 8.f);
                        if (k == "key_track") continue;                               // identity on G1: checked on another note below
                        if (k.startsWith ("env_") && k != "env_amount" && k != "env_to_reso" && k != "env_to_drive") set2 (o + "env_amount", 0.8f);
                        if (k == "env_release") set2 (o + "env_sustain", 0.6f);
                        if (k == "env_sync") set2 ("core_length", 900.f);
                        if (k == "order") { set2 (o + "drive", 0.8f); set2 (other + "on", 1.f); set2 (other + "type", 1.f); set2 (other + "drive", 0.8f); set2 (other + "order", 4.f); }
                        if (k == "path") { set2 (other + "on", 1.f); set2 (other + "type", 1.f); set2 (other + "path", 2.f); }
                    }
                    else
                    {
                        if (k == "pitch_track") continue;                             // identity on G1: checked below
                        if (k.startsWith ("penv_") && k != "penv_amount") set2 (o + "penv_amount", 0.8f);
                        if (k == "penv_release") set2 (o + "penv_sustain", 0.5f);
                    }
                }
                if (sid == "filter_xover_low" || sid == "filter_xover_high") { set2 ("flt1_on", 1.f); set2 ("flt1_band", sid == "filter_xover_high" ? 3.f : 1.f); set2 ("flt1_type", 1.f); set2 ("flt1_cutoff", 20000.f); }
                const bool isScr = sid.startsWith ("scr_");
                if (isScr)
                {
                    set2 ("scr_on", 1.f);
                    const String k = sid.substring (4);
                    auto mode = [&] (int m) { set2 ("scr_mode", (float) m); };
                    if (k.startsWith ("fm_") || k == "mod_fm") mode (3);
                    if (k.startsWith ("metal")) mode (1);
                    if (k.startsWith ("noise_")) mode (4);
                    if (k.startsWith ("vowel") || k.startsWith ("formant_") || k == "mod_formant") { mode (2); if (k == "vowel_end") set2 ("scr_vowel_auto", 1.f); }
                    if (k.startsWith ("fb_") || k == "mod_feedback") mode (5);
                    if (k.startsWith ("mix_")) mode (7);
                    if (k == "base_hz") set2 ("scr_pitch_mode", 2.f);
                    if (k == "mod_attack" || k == "mod_decay" || k == "mod_curve") set2 ("scr_mod_pitch", 12.f);
                    if (k.startsWith ("pitch_") && k != "pitch_amount" && k != "pitch_mode") set2 ("scr_pitch_amount", 1.f);
                    if (k == "offset") set2 ("scr_start_mode", 2.f);
                    if (k == "sync_tail" || k == "auto_length") set2 ("scr_length", 50.f);
                    if (k == "release") set2 ("scr_length", 80.f);                       // (INSIDE the tail: the release must end before the tail does)
                    if (k == "quantize") set2 ("scr_start", 183.3f);
                    if (k == "follow_body") { set2 ("scr_start_mode", 1.f); set2 ("body_amp_env_delay", 40.f); }
                    if (k == "width") set2 ("scr_osc1_pan", 0.7f);
                    if (k.startsWith ("osc"))
                    {
                        const String o = k.upToFirstOccurrenceOf ("_", true, false), kk = k.fromFirstOccurrenceOf ("_", false, false);
                        const String other = o == "osc1_" ? "osc2_" : "osc1_";
                        set2 ("scr_" + o + "on", 1.f);
                        if (kk == "pw") set2 ("scr_" + o + "wave", 4.f);
                        if (kk == "fm_amount" || kk == "fm_source") { set2 ("scr_" + other + "on", 1.f); if (kk == "fm_amount") set2 ("scr_" + o + "fm_source", o == "osc1_" ? 2.f : 1.f); else set2 ("scr_" + o + "fm_amount", 0.8f); }
                        if (kk == "ring" || kk == "ring_source") { set2 ("scr_" + other + "on", 1.f); if (kk == "ring") set2 ("scr_" + o + "ring_source", o == "osc1_" ? 2.f : 1.f); else set2 ("scr_" + o + "ring", 0.8f); }
                    }
                    if (k.startsWith ("f1_") || k.startsWith ("f2_"))
                    {
                        set2 ("scr_" + k.substring (0, 3) + "on", 1.f);
                        if (k.endsWith ("key_track")) { set2 ("scr_pitch_mode", 2.f); set2 ("scr_base_hz", 2000.f); }
                    }
                }
            }
            // module 07 contexts: the processor is ON and doing something the control can change
            {
                auto set2 = [&] (const String& pid, float v) { const int k = idOf (pid); if (k >= 0 && k != id) { a[k] = v; b[k] = v; } };
                auto isN = [&] (const char* pre) { const String p (pre); return sid.startsWith (p) && sid.length() > p.length() && CharacterFunctions::isDigit (sid[p.length()]); };
                if (isN ("eq"))
                {
                    const String o = sid.upToFirstOccurrenceOf ("_", true, false), k = sid.fromFirstOccurrenceOf ("_", false, false);
                    set2 (o + "on", 1.f); set2 (o + "gain", 9.f); set2 (o + "freq", 900.f);
                    if (k == "slope") { set2 (o + "type", 3.f); set2 (o + "freq", 300.f); }
                    if (k == "position") { set2 ("dist1_on", 1.f); set2 ("dist1_type", 1.f); set2 ("dist1_drive", 0.8f); }
                }
                if (isN ("deq"))
                {
                    const String o = sid.upToFirstOccurrenceOf ("_", true, false), k = sid.fromFirstOccurrenceOf ("_", false, false);
                    set2 (o + "on", 1.f); set2 (o + "threshold", -40.f); set2 (o + "range", 12.f); set2 (o + "freq", 900.f);
                    if (k == "window") set2 (o + "detector", 1.f);
                    if (k == "position") { set2 ("dist1_on", 1.f); set2 ("dist1_type", 1.f); set2 ("dist1_drive", 0.8f); }
                    if (k == "threshold") set2 (o + "ratio", 8.f);
                }
                if (sid.startsWith ("mb"))
                {
                    set2 ("mb_on", 1.f); set2 ("tail_width_from", 80.f); set2 ("tail_width", 1.f);
                    const String k = sid.fromFirstOccurrenceOf ("_", false, false);
                    const String o = sid.upToFirstOccurrenceOf ("_", true, false);
                    if (o != "mb_")
                    {
                        if (k == "threshold" || k == "ratio" || k == "attack" || k == "release" || k == "knee" || k == "makeup" || k == "mix")
                        { set2 (o + "dynamics", 1.f); set2 (o + "threshold", -40.f); }
                        if (k == "tr_attack" || k == "tr_sustain") set2 (o + "dynamics", 5.f);
                        if (k == "solo") set2 ("mb1_gain", -6.f);
                    }
                    else
                    {
                        if (k == "listen") set2 ("mb2_gain", -12.f);
                        if (k == "xover1") set2 ("mb1_gain", -12.f);
                        if (k == "xover2") set2 ("mb2_gain", -12.f);
                        if (k == "xover3") set2 ("mb3_gain", -12.f);
                        if (k == "crossover") set2 ("mb2_gain", -12.f);
                        if (k == "position") { set2 ("mb2_gain", -12.f); set2 ("mb2_dynamics", 1.f); set2 ("mb2_threshold", -40.f); set2 ("dist1_on", 1.f); set2 ("dist1_type", 1.f); set2 ("dist1_drive", 0.8f); }
                        if (k == "location" || k == "on") set2 ("mb2_gain", -12.f);
                    }
                }
                if (sid.startsWith ("sp_") || sid == "tone_order")
                {
                    set2 ("sp_on", 1.f);
                    const String k = sid.substring (3);
                    if (k.startsWith ("tilt") || k == "on" || k == "location" || k == "position") set2 ("sp_tilt", 0.6f);
                    if (k.startsWith ("tilt_")) set2 ("sp_tilt_intensity", k == "tilt_intensity" ? 0.5f : 1.f);
                    if (k.startsWith ("harm")) { set2 ("sp_harm_on", 1.f); set2 ("sp_harm_drive", 0.6f); }
                    if (k.startsWith ("exc")) { set2 ("sp_exc_on", 1.f); set2 ("sp_exc_amount", 0.8f); }
                    if (k.startsWith ("comp")) { set2 ("sp_comp_on", 1.f); set2 ("sp_comp_threshold", 0.f); set2 ("sp_comp_reduction", 18.f); set2 ("sp_comp_sensitivity", 1.f); }
                    if (k.startsWith ("freeze")) { set2 ("sp_freeze", 1.f); set2 ("sp_freeze_mix", 1.f); set2 ("sp_freeze_capture", 60.f); }
                    if (k == "position") { set2 ("dist1_on", 1.f); set2 ("dist1_type", 1.f); set2 ("dist1_drive", 0.8f); }
                    if (sid == "tone_order") { set2 ("sp_harm_on", 1.f); set2 ("sp_harm_drive", 0.8f); set2 ("eq1_on", 1.f); set2 ("eq1_gain", 12.f); set2 ("eq1_freq", 300.f);
                                               set2 ("mb_on", 1.f); set2 ("mb2_dynamics", 1.f); set2 ("mb2_threshold", -40.f); }
                }
                if (sid.startsWith ("stw_"))
                {
                    set2 ("stw_on", 1.f); set2 ("tail_width_from", 80.f); set2 ("tail_width", 1.f);
                    const String k = sid.substring (4);
                    if (k == "on" || k == "location" || k == "mix" || k == "mono_listen") set2 ("stw_width", 2.f);
                    if (k == "xover_low" || k == "xover_high") { set2 ("stw_low_width", 0.f); set2 ("stw_mid_width", 1.f); set2 ("stw_high_width", 2.f); set2 ("stw_mono_below", 20.f); }
                    if (k.endsWith ("_width")) set2 ("stw_mono_below", 20.f);
                }
            }
            // module 08 contexts: the processor is ON and working on the kick
            {
                auto set2 = [&] (const String& pid, float v) { const int k = idOf (pid); if (k >= 0 && k != id) { a[k] = v; b[k] = v; } };
                auto isN = [&] (const char* pre) { const String p (pre); return sid.startsWith (p) && sid.length() > p.length() && CharacterFunctions::isDigit (sid[p.length()]); };
                auto distOn = [&] { set2 ("dist1_on", 1.f); set2 ("dist1_type", 1.f); set2 ("dist1_drive", 0.8f); };
                if (isN ("cmp"))
                {
                    const String o = sid.upToFirstOccurrenceOf ("_", true, false), k = sid.fromFirstOccurrenceOf ("_", false, false);
                    set2 (o + "on", 1.f); set2 (o + "threshold", k == "knee" ? -12.f : -40.f); set2 (o + "ratio", 8.f);
                    if (k == "dry_level" || k == "wet_level") set2 (o + "type", 6.f);
                    if (k == "lookahead") set2 (o + "location", 15.f);
                    if (k == "sc_gain" || k == "sc_mix") set2 (o + "sc_source", 1.f);
                    if (k == "position") distOn();
                }
                if (isN ("ts"))
                {
                    const String o = sid.upToFirstOccurrenceOf ("_", true, false), k = sid.fromFirstOccurrenceOf ("_", false, false);
                    set2 (o + "on", 1.f); set2 (o + "attack", 0.8f); set2 (o + "sustain", 0.5f);
                    const int dl = (int) def (idOf (o + "location")).def;                      // its default place (PUNCH .. TEXTURE)
                    if (k == "position") { distOn(); set2 ("dist1_location", (float) dl); }
                    if (k == "sustain") set2 (o + "mode", 1.f);
                    if (dl == 5 || k == "location") { set2 ("layer1_on", 1.f); set2 ("layer1_group", 4.f); }   // something on the TEXTURE bus
                }
                if (isN ("clp"))
                {
                    const String o = sid.upToFirstOccurrenceOf ("_", true, false), k = sid.fromFirstOccurrenceOf ("_", false, false);
                    set2 (o + "on", 1.f);
                    if (k == "position") distOn();
                }
                if (isN ("lim"))
                {
                    const String o = sid.upToFirstOccurrenceOf ("_", true, false), k = sid.fromFirstOccurrenceOf ("_", false, false);
                    set2 (o + "on", 1.f); set2 (o + "input", 6.f);
                    if (k == "oversampling" || k == "true_peak") { set2 (o + "true_peak", 1.f); set2 (o + "location", 15.f); }
                    if (k == "stereo_link") { set2 ("mix_body_pan", -1.f); set2 ("mix_tail_pan", 1.f); set2 (o + "input", 12.f); }   // (L and R really differ)
                    if (k == "position") { set2 (o + "location", 0.f); distOn(); }
                    if (k == "lookahead") set2 (o + "location", 15.f);
                }
                if (sid.startsWith ("crest_")) { set2 ("crest_on", 1.f); if (sid == "crest_position") distOn(); }
                if (sid == "dyn_auto_gain" || sid == "dyn_level_match" || sid == "dyn_order")
                { set2 ("cmp1_on", 1.f); set2 ("cmp1_threshold", -40.f); set2 ("cmp1_ratio", 8.f); set2 ("ts1_on", 1.f); set2 ("ts1_attack", 0.8f); set2 ("ts1_location", 0.f);
                  if (sid == "dyn_order") { set2 ("clp1_on", 1.f); set2 ("clp1_drive", 0.8f); } }
                if (sid == "mb_link") { set2 ("mb_on", 1.f); for (auto* bnd : { "mb1_", "mb2_", "mb3_", "mb4_" }) { set2 (String (bnd) + "dynamics", 1.f); set2 (String (bnd) + "threshold", -40.f); set2 (String (bnd) + "ratio", 10.f); } }
            }
            if (sid.endsWith ("_trigger")) continue;                                                // MIDI behaviour (note-off / legato): checked below
            if (d.kind == Kind::Choice)
            {
                bool any = false;
                for (int c = 0; c < d.choices().size() && ! any; ++c) { if (c == (int) d.def) continue; b[id] = (float) c; any |= diff (renderKick (a, G1, sr, 0.6), renderKick (b, G1, sr, 0.6)) > 1.0e-4; }
                ++tested; if (! any) dead.add (d.id);
                continue;
            }
            a[id] = d.min; b[id] = d.kind == Kind::Bool ? 1.f - d.def : d.max;
            if (d.unit == Unit::Deg) b[id] = 90.f;                                   // 0 and 360 degrees are the same phase
            if (d.kind == Kind::Bool) a[id] = d.def;
            ++tested;
            if (diff (renderKick (a, G1, sr, 0.6), renderKick (b, G1, sr, 0.6)) < 1.0e-4) dead.add (d.id);
        }
        // PHASE RESET: two kicks in a row differ when it is off
        {
            GeProcessor p; setP (p, "core_voice_mode", 1.f);
            std::vector<std::pair<int, MidiMessage>> ev { { 0, MidiMessage::noteOn (1, G1, 1.f) }, { 7777, MidiMessage::noteOn (1, G1, 1.f) } };
            auto on = hitProc (p, ev, 0.5);
            setP (p, "core_phase_reset", 0.f);
            auto off = hitProc (p, ev, 0.5);
            if (diff (on, off) < 1.0e-4) dead.add ("core_phase_reset");
        }
        check (dead.isEmpty(), "14.11 every parameter changes the sound (min vs max / every choice)",
               String (tested) + " tested" + (dead.isEmpty() ? String() : ", DEAD: " + dead.joinIntoString (" ")));
    }
    // ---------------------------------------------------------------- 14.12 the state round trip restores everything
    {
        GeProcessor a;
        Random rnd (7);
        for (auto* prm : a.getParameters()) prm->setValueNotifyingHost (rnd.nextFloat());
        MemoryBlock mb; a.getStateInformation (mb);
        GeProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        int bad = 0;
        for (int i = 0; i < P::count; ++i)
        {
            auto* pa = dynamic_cast<RangedAudioParameter*> (a.getParameters()[i]);
            auto* pb = dynamic_cast<RangedAudioParameter*> (b.getParameters()[i]);
            if (std::abs (pa->convertFrom0to1 (pa->getValue()) - pb->convertFrom0to1 (pb->getValue())) > 1.0e-3f * jmax (1.f, std::abs (pa->convertFrom0to1 (pa->getValue())))) ++bad;
        }
        check (bad == 0, "14.12 save / load of the state restores every parameter", String (bad) + " differ");
    }
    // ---------------------------------------------------------------- MODULE02 section 14: acceptance tests of the envelope engine
    {
        ParamFrame f = base; f[P::tailChar] = 0.f; f[P::tailDrive] = 0.f; f[P::tailTone] = 20000.f; f[P::tailReso] = 0.f; f[P::coreLength] = 900.f;
        f[P::tailUniVoices] = 1.f; f[P::peTime] = 200.f; f[P::peStart] = 400.f;
        // 2. every interpolation type is a different movement in the DSP (TAIL PITCH = CUSTOM, one segment, 8 curves)
        {
            ParamFrame q = f; q[P::peMode] = 8.f;
            std::vector<AudioBuffer<float>> outs;
            for (int c = 0; c < env::NumCurves; ++c)
            {
                env::Bank b;
                auto sh = env::Shape::make ({ { 0.f, 1.f }, { 1.f, 0.f } });
                sh.p[0].curve = (uint8_t) c; sh.p[0].h1x = 0.1f; sh.p[0].h1y = 0.9f; sh.p[0].h2x = 0.2f; sh.p[0].h2y = 1.f;
                b.s[env::PitchTail] = sh;
                outs.push_back (renderKick (q, G1, sr, 0.4, 2, 1.f, &b));
            }
            double minD = 1.0e9; String pair;
            for (int i = 0; i < (int) outs.size(); ++i) for (int j = i + 1; j < (int) outs.size(); ++j)
            { const double d = diff (outs[(size_t) i], outs[(size_t) j]); if (d < minD) { minD = d; pair = String (env::curveName (i)) + "/" + env::curveName (j); } }
            check (minD > 1.0e-3, "M02 14.2 all 8 interpolation types act in the DSP", "closest " + pair + " " + String (minD, 4));
        }
        // 3. every part has its own pitch envelope: PUNCH CUSTOM changes only the punch
        {
            env::Bank b; b.s[env::PitchPunch] = env::Shape::make ({ { 0.f, 1.f }, { 0.8f, 1.f }, { 1.f, 0.f } });
            ParamFrame q = f; q[P::punchPMode] = 8.f; q[P::punchLevel] = 0.f;
            const double dp = diff (renderKick (q, G1, sr, 0.4, 0, 1.f, &b), renderKick (f, G1, sr, 0.4, 0));
            const double dt = diff (renderKick (q, G1, sr, 0.4, 2, 1.f, &b), renderKick (f, G1, sr, 0.4, 2));
            check (dp > 1.0e-3 && dt < 1.0e-7, "M02 14.3 each part has its own pitch envelope", "punch " + String (dp, 4) + " tail " + String (dt, 8));
        }
        // 4. FOLLOW: the sub copies the tail's pitch movement; INDEPENDENT (no depth) stays on the note whatever the tail does
        {
            ParamFrame a2 = f; a2[P::subLink] = 1.f; a2[P::subLinkSrc] = 2.f; a2[P::subFDepth] = 0.5f;
            ParamFrame b2 = a2; b2[P::peStart] = 200.f;
            const double follow = diff (renderKick (a2, G1, sr, 0.4, 3), renderKick (b2, G1, sr, 0.4, 3));
            ParamFrame c2 = a2; c2[P::subLink] = 0.f;
            ParamFrame d2 = c2; d2[P::peStart] = 200.f;
            const double indep = diff (renderKick (c2, G1, sr, 0.4, 3), renderKick (d2, G1, sr, 0.4, 3));
            check (follow > 1.0e-3 && indep < 1.0e-6, "M02 14.4 FOLLOW copies the movement, INDEPENDENT does not", "follow " + String (follow, 4) + " indep " + String (indep, 7));
        }
        // 5. LINK loops are cut: punch follows body, body follows punch -> finite, both back to INDEPENDENT
        {
            ParamFrame q = f; q[P::punchLink] = 1.f; q[P::punchLinkSrc] = 1.f; q[P::bodyLink] = 1.f; q[P::bodyLinkSrc] = 0.f;
            auto x = renderKick (q, G1, sr, 0.5);
            ParamFrame r = q; r[P::punchLink] = 0.f;                      // the loop is cut at the first part: punch independent, body still follows it
            const bool finite = std::isfinite (rms (x)) && peak (x) < 4.f;
            check (finite && diff (x, renderKick (r, G1, sr, 0.5)) < 1.0e-6, "M02 14.5 a LINK loop is cut (no feedback, no endless recursion)");
        }
        // 6. TIME SCALE stretches the curve without changing its shape: pitch at 2t (scale 2) == pitch at t (scale 1)
        {
            ParamFrame q = f; q[P::peTime] = 300.f; q[P::punchLevel] = -60.f; q[P::bodyLevel] = -60.f;
            ParamFrame q2 = q; q2[P::tailPScale] = 2.f;
            const auto x1 = renderKick (q, G1, sr, 1.0, 2), x2 = renderKick (q2, G1, sr, 1.0, 2);
            double worst = 0.0;
            for (double t : { 0.04, 0.08, 0.14 }) worst = jmax (worst, std::abs (std::log2 (pitchAt (x2, sr, 2.0 * t, 0.024) / pitchAt (x1, sr, t, 0.012))) * 12.0);
            check (worst < 1.0, "M02 14.6 TIME SCALE stretches without changing the shape", "worst " + String (worst, 2) + " st");
        }
        // 7. host BPM: a SYNCed envelope is twice as long at 75 BPM as at 150
        {
            ParamFrame q = f; q[P::envSync] = 1.f; q[P::tailAmpSync] = 1.f; q[P::tailAmpMode] = 1.f; q[P::coreLength] = 2000.f;
            q[P::tailLen] = 300.f; q[P::coreAutoLen] = 0.f;
            env::Bank b; b.s[env::AmpTail] = env::Shape::make ({ { 0.f, 1.f }, { 0.9f, 1.f }, { 1.f, 0.f } }, env::Linear);
            auto endOf = [] (const AudioBuffer<float>& x) { int last = 0; const float th = 0.1f * peak (x); for (int i = 0; i < x.getNumSamples(); ++i) if (std::abs (x.getSample (0, i)) > th) last = i; return last / sr * 1000.0; };
            const double e150 = endOf (renderKick (q, G1, sr, 1.5, 2, 1.f, &b, 150.0)), e75 = endOf (renderKick (q, G1, sr, 1.5, 2, 1.f, &b, 75.0));
            check (std::abs (e75 / jmax (1.0, e150) - 2.0) < 0.1, "M02 14.7 SYNCed envelopes follow the host BPM", String (e150, 0) + " ms @150, " + String (e75, 0) + " ms @75");
        }
        // 13. realtime (processor, MIDI) == offline (renderKick) with the same drawn shapes
        {
            GeProcessor p2; setP (p2, "tail_amp_env_mode", 1.f); setP (p2, "pe_mode", 8.f);
            p2.setShape (env::PitchTail, env::Shape::make ({ { 0.f, 1.f }, { 0.3f, 0.9f }, { 0.35f, 0.f }, { 1.f, 0.f } }));
            p2.setShape (env::AmpTail, env::Shape::make ({ { 0.f, 1.f }, { 0.5f, 0.6f }, { 1.f, 0.f } }));
            const auto rt = hitProc (p2, { { 0, MidiMessage::noteOn (1, 31, (uint8) 127) } }, 0.6);
            const auto off = renderKick (p2.frame(), 31, sr, 0.6, -1, 1.f, &p2.shapes());
            const int lat = 0;                                              // both paths carry the same oversampling latency
            double d = 0.0, e = 0.0;
            for (int i = 0; i + lat < rt.getNumSamples(); ++i) { const double u = rt.getSample (0, i + lat) - off.getSample (0, i); d += u * u; e += (double) off.getSample (0, i) * off.getSample (0, i); }
            check (std::sqrt (d / jmax (1.0e-12, e)) < 1.0e-4, "M02 14.13 realtime and offline export give the same kick", "rel " + String (std::sqrt (d / jmax (1.0e-12, e)), 7));
        }
        // 12 + 14. parameter moves while kicks overlap (POLY, RETRIGGER, drawn envelopes): finite, no jumps
        {
            GeProcessor p3; setP (p3, "core_voice_mode", 1.f); setP (p3, "tail_amp_env_trigger", 1.f); setP (p3, "tail_filter_env_on", 1.f);
            p3.prepareToPlay (sr, 256);
            float maxStep = 0.f, last = 0.f; bool finite = true;
            for (int blk = 0; blk < 300; ++blk)
            {
                AudioBuffer<float> b (2, 256); b.clear(); MidiBuffer m;
                if (blk % 20 == 0) m.addEvent (MidiMessage::noteOn (1, 31, (uint8) 110), 17);
                if (blk % 20 == 9) m.addEvent (MidiMessage::noteOff (1, 31), 3);
                setP (p3, "tail_filter_env_amount", (float) std::sin (blk * 0.37) * 0.8f);
                setP (p3, "tail_tone", 2000.f + 6000.f * (float) (blk % 7) / 6.f);
                p3.processBlock (b, m);
                for (int i = 0; i < 256; ++i) { const float v = b.getSample (0, i); finite &= std::isfinite (v); maxStep = jmax (maxStep, std::abs (v - last)); last = v; }
            }
            check (finite && maxStep < 1.2f, "M02 14.12 / 14.14 moving parameters on overlapping kicks: finite, no jumps", "max step " + String (maxStep, 3));
        }
    }
    // ---------------------------------------------------------------- MODULE03 section 17: the oscillator engine
    {
        // a clean host: the tail plays a pure sine on the note (G1), the oscillator under test comes in on FIXED Hz
        ParamFrame h = base;
        h[P::tailSrc] = 0.f; h[P::tailChar] = 0.f; h[P::tailDrive] = 0.f; h[P::tailTone] = 20000.f; h[P::tailReso] = 0.f;
        h[P::tailUniVoices] = 1.f; h[P::tailSt2Type] = 0.f; h[P::clipDrive] = 0.f; h[P::coreAutoLen] = 0.f; h[P::coreLength] = 1200.f;
        h[P::tailLen] = 1100.f; h[P::tailSustain] = 1.f; h[P::tailDecay] = 2000.f; h[P::punchLevel] = -60.f; h[P::bodyLevel] = -60.f; h[P::subLevel] = -60.f;
        auto oscOn = [] (ParamFrame& f, int n, int wave, float hz)
        {
            const int s = (n - 1) * (P::osc2On - P::osc1On);
            f[P::osc1On + s] = 1.f; f[P::osc1Wave + s] = (float) wave; f[P::osc1Track + s] = 4.f; f[P::osc1Freq + s] = hz; f[P::osc1Level + s] = 0.f;
        };
        auto at = [] (int n, int first) { return first + (n - 1) * (P::osc2On - P::osc1On); };
        // amplitude of one frequency in the steady part (Goertzel, 0.3 .. 0.6 s)
        auto amp = [] (const AudioBuffer<float>& x, double hz)
        {
            const int a = (int) (0.3 * sr), n = (int) (0.3 * sr);
            const double w = MathConstants<double>::twoPi * hz / sr, c = 2.0 * std::cos (w);
            double s1 = 0.0, s2 = 0.0;
            for (int i = 0; i < n; ++i) { const double win = 0.5 - 0.5 * std::cos (MathConstants<double>::twoPi * i / (n - 1));
                                          const double s0 = x.getSample (0, a + i) * win + c * s1 - s2; s2 = s1; s1 = s0; }
            return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (n * 0.25);
        };
        auto dbv = [] (double v) { return 20.0 * std::log10 (v + 1.0e-9); };
        // 1. every waveform sounds and is its own waveform
        {
            std::vector<AudioBuffer<float>> w;
            for (int k = 0; k < 9; ++k) { ParamFrame f = h; oscOn (f, 1, k, 330.f); f[P::osc1WtPos] = 0.5f; f[P::osc1Pw] = 0.3f; w.push_back (renderKick (f, G1, sr, 0.7)); }
            double minD = 1.0e9; String pr;
            for (int i = 0; i < 9; ++i) for (int j = i + 1; j < 9; ++j) { const double d = diff (w[(size_t) i], w[(size_t) j]); if (d < minD) { minD = d; pr = String (i) + "/" + String (j); } }
            ParamFrame p1 = h; oscOn (p1, 1, 4, 330.f); p1[P::osc1Pw] = 0.1f;
            ParamFrame p2 = p1; p2[P::osc1Pw] = 0.5f;
            const auto x1 = renderKick (p1, G1, sr, 0.7), x2 = renderKick (p2, G1, sr, 0.7);
            const double even1 = amp (x1, 660.0), even2 = amp (x2, 660.0);           // a 50 % pulse has (almost) no even harmonics
            check (minD > 1.0e-3 && even1 > 4.0 * even2, "M03 17.1 all 9 waveforms differ, PULSE WIDTH changes the spectrum",
                   "closest " + pr + " " + String (minD, 4) + ", 2nd harmonic at PW 10 % / 50 %: " + String (dbv (even1), 1) + " / " + String (dbv (even2), 1) + " dB");
        }
        // 2 + 3. four independent oscillators (own phase each): all four alone, and their sum is the sum
        {
            ParamFrame f = h; float hzs[4] { 330.f, 420.f, 515.f, 610.f };
            for (int n = 1; n <= 4; ++n) oscOn (f, n, 0, hzs[n - 1]);
            const auto x = renderKick (f, G1, sr, 0.7);
            bool all = true; String e;
            for (int n = 1; n <= 4; ++n) { const double v = amp (x, hzs[n - 1]); all &= v > 0.02; e << String (dbv (v), 1) << " "; }
            check (all, "M03 17.2 four independent oscillators sound together", e + "dB");
        }
        // 4 + 5. UNISON: independent detuned voices (energy beside the centre), the level stays under control
        {
            ParamFrame f1 = h; oscOn (f1, 1, 0, 400.f);
            ParamFrame f5 = f1; f5[P::osc1Uni] = 7.f; f5[P::osc1Detune] = 1.f; f5[P::osc1Blend] = 1.f;
            const auto x1 = renderKick (f1, G1, sr, 0.7), x5 = renderKick (f5, G1, sr, 0.7);
            const double side1 = amp (x1, 400.0 * std::exp2 (100.0 / 1200.0)), side5 = amp (x5, 400.0 * std::exp2 (100.0 / 1200.0));
            const double lv = dbv (rms (x5, (int) (0.3 * sr), (int) (0.3 * sr))) - dbv (rms (x1, (int) (0.3 * sr), (int) (0.3 * sr)));
            ParamFrame f0 = f5; f0[P::osc1Detune] = 0.f; f0[P::osc1UniPh] = 0.f; f0[P::osc1Spread] = 0.f;              // no detune + same phase = one voice
            const double same = diff (renderKick (f0, G1, sr, 0.7), x1);
            check (side5 > 10.0 * side1 && std::abs (lv) < 6.0 && same < 1.0e-3, "M03 17.4 / 17.5 UNISON: real detuned voices, level kept",
                   "+100 ct: " + String (dbv (side1), 1) + " -> " + String (dbv (side5), 1) + " dB, level " + String (lv, 1) + " dB");
        }
        // 6. WAVETABLE: a WAV becomes frames, the position scans through them
        {
            std::vector<float> wav (2048);
            for (int i = 0; i < 1024; ++i) wav[(size_t) i] = std::sin (MathConstants<float>::twoPi * (float) i / 1024.f);
            for (int i = 0; i < 1024; ++i) wav[(size_t) (1024 + i)] = i < 512 ? 1.f : -1.f;
            fge::dsp::OscData d; d.osc[0].userFrames = fge::dsp::tables::framesFromAudio (wav.data(), 2048, 1024);
            ParamFrame f = h; oscOn (f, 1, 7, 330.f); f[P::osc1WtTable] = 5.f;
            ParamFrame f2 = f; f2[P::osc1WtPos] = 1.f;
            const auto x0 = renderKick (f, G1, sr, 0.7, -1, 1.f, nullptr, 0.0, -1.0, nullptr, &d);
            const auto x1 = renderKick (f2, G1, sr, 0.7, -1, 1.f, nullptr, 0.0, -1.0, nullptr, &d);
            ParamFrame fs = f; fs[P::osc1WtMode] = 1.f; fs[P::osc1WtRate] = 600.f;
            const auto xs = renderKick (fs, G1, sr, 0.7, -1, 1.f, nullptr, 0.0, -1.0, nullptr, &d);
            const double h3a = amp (x0, 990.0), h3b = amp (x1, 990.0);
            check (d.osc[0].userFrames.size() == 2 && h3b > 10.0 * h3a && diff (xs, x0) > 1.0e-3, "M03 17.6 WAVETABLE: imported frames, position + scan move the timbre",
                   "3rd harmonic sine frame " + String (dbv (h3a), 1) + " dB, square frame " + String (dbv (h3b), 1) + " dB");
        }
        // 7. HARMONIC engine: ODD 0 removes the odd overtones, EVEN 0 the even ones
        {
            ParamFrame f = h; oscOn (f, 1, 8, 330.f);
            ParamFrame fo = f; fo[P::osc1HOdd] = 0.f;
            ParamFrame fe = f; fe[P::osc1HEven] = 0.f;
            const auto xo = renderKick (fo, G1, sr, 0.7), xe = renderKick (fe, G1, sr, 0.7);
            check (amp (xo, 990.0) < 0.05 * amp (xo, 660.0) && amp (xe, 660.0) < 0.05 * amp (xe, 990.0), "M03 17.7 HARMONIC engine shapes the real spectrum",
                   "odd 0: h3 / h2 " + String (dbv (amp (xo, 990.0)) - dbv (amp (xo, 660.0)), 1) + " dB, even 0: h2 / h3 " + String (dbv (amp (xe, 660.0)) - dbv (amp (xe, 990.0)), 1) + " dB");
        }
        // 8. FM = frequency modulation: sidebands at carrier +- modulator, bounded level
        {
            ParamFrame f = h; oscOn (f, 1, 0, 330.f); oscOn (f, 2, 0, 110.f); f[P::osc2ToTail] = -60.f;
            ParamFrame g = f; g[P::fm2to1] = 0.5f;
            const auto x0 = renderKick (f, G1, sr, 0.7), x1 = renderKick (g, G1, sr, 0.7);
            const double sb0 = amp (x0, 440.0), sb1 = amp (x1, 440.0);
            check (sb1 > 20.0 * sb0 && peak (x1) < 2.f * peak (x0) + 0.5f, "M03 17.8 FM: sidebands appear (frequency modulation), level bounded",
                   "330 + 110 Hz sideband " + String (dbv (sb0), 1) + " -> " + String (dbv (sb1), 1) + " dB");
        }
        // 9. RING = multiplication: 330 x 110 -> 220 + 440, the 330 itself disappears
        {
            ParamFrame f = h; oscOn (f, 1, 0, 330.f); f[P::osc1RingSrc] = 5.f; f[P::osc1RingHz] = 110.f;
            const auto x = renderKick (f, G1, sr, 0.7);
            const double c = amp (x, 330.0), lo = amp (x, 220.0), hi = amp (x, 440.0);
            check (lo > 8.0 * c && hi > 8.0 * c, "M03 17.9 RING MOD multiplies: sum + difference, no carrier",
                   "220 / 330 / 440 Hz: " + String (dbv (lo), 1) + " / " + String (dbv (c), 1) + " / " + String (dbv (hi), 1) + " dB");
        }
        // 10. HARD SYNC: the slave repeats at the master's period (its harmonics, not its own pitch)
        {
            ParamFrame f = h; oscOn (f, 1, 2, 300.f); oscOn (f, 2, 0, 300.f); f[P::osc2ToTail] = -60.f;
            f[P::osc1SyncSrc] = 2.f; f[P::osc1SyncAmt] = 0.37f;                  // slave at 300 x 3.59 Hz, reset every 300 Hz cycle
            const auto x = renderKick (f, G1, sr, 0.7);
            const double onGrid = amp (x, 1200.0), offGrid = amp (x, 300.0 * 3.59);
            ParamFrame nf = f; nf[P::osc1SyncSrc] = 0.f; nf[P::osc1Freq] = 300.f * 3.59f;      // the same slave, free
            const auto y = renderKick (nf, G1, sr, 0.7);
            check (onGrid > 4.0 * offGrid && amp (y, 300.0 * 3.59) > 4.0 * amp (y, 1200.0), "M03 17.10 HARD SYNC locks the slave to the master's period",
                   "1200 / 1077 Hz synced " + String (dbv (onGrid), 1) + " / " + String (dbv (offGrid), 1) + " dB");
        }
        // 11. ROUTING: an oscillator sent only to BODY changes only the body
        {
            ParamFrame f = base; f[P::osc1On] = 1.f; f[P::osc1ToTail] = -60.f; f[P::osc1ToBody] = 0.f; f[P::bodyLevel] = -6.f;
            const double db = diff (renderKick (f, G1, sr, 0.5, 1), renderKick (base, G1, sr, 0.5, 1));
            const double dt = diff (renderKick (f, G1, sr, 0.5, 2), renderKick (base, G1, sr, 0.5, 2));
            check (db > 1.0e-3 && dt < 1.0e-6, "M03 17.11 ROUTING: oscillator -> BODY only", "body " + String (db, 4) + " tail " + String (dt, 7));
        }
        // 13. extreme settings: 4 oscillators, 16 voices, full FM + feedback, sync, ring -> finite, bounded
        {
            ParamFrame f = h;
            for (int n = 1; n <= 4; ++n) { oscOn (f, n, n % 3 == 0 ? 2 : 3, 60.f * (float) n); f[at (n, P::osc1Uni)] = 16.f; f[at (n, P::osc1Detune)] = 1.f; f[at (n, P::osc1FmFb)] = 1.f; f[at (n, P::osc1Level)] = -12.f; }
            for (int id2 : { P::fm1to2, P::fm2to3, P::fm3to4, P::fm4to1, P::fm2to1 }) f[id2] = 1.f;
            f[P::osc3SyncSrc] = 1.f; f[P::osc4RingSrc] = 2.f;
            const auto x = renderKick (f, G1, sr, 0.6);
            ParamFrame plain = f;                                                    // the same oscillators without unison / FM / sync / ring
            for (int n = 1; n <= 4; ++n) { plain[at (n, P::osc1Uni)] = 1.f; plain[at (n, P::osc1FmFb)] = 0.f; }
            for (int id2 : { P::fm1to2, P::fm2to3, P::fm3to4, P::fm4to1, P::fm2to1 }) plain[id2] = 0.f;
            plain[P::osc3SyncSrc] = 0.f; plain[P::osc4RingSrc] = 0.f;
            const auto y = renderKick (plain, G1, sr, 0.6);
            check (std::isfinite (rms (x)) && rms (x) < 1.6 * rms (y), "M03 17.13 extreme unison / FM / sync / ring: finite, no level runaway",
                   "rms " + String (dbv (rms (x)) - dbv (rms (y)), 1) + " dB vs plain");
        }
    }
    // ---------------------------------------------------------------- MODULE04 section 19: noise, transient, layers
    {
        // a silent kick: only module 04 is heard (all four parts muted in the mixer)
        ParamFrame q = base;
        for (int id2 : { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute }) q[id2] = 1.f;
        q[P::clipDrive] = 0.f; q[P::coreAutoLen] = 0.f; q[P::coreLength] = 1200.f;
        auto band = [] (const AudioBuffer<float>& x, double lo, double hi, double a, double len)
        {
            // energy in a band (FFT of one window)
            const int order = 12, n = 1 << order, start = (int) (a * sr);
            juce::dsp::FFT fft (order);
            std::vector<float> buf ((size_t) n * 2, 0.f);
            const int m = jmin (n, (int) (len * sr));
            for (int i = 0; i < m && start + i < x.getNumSamples(); ++i) buf[(size_t) i] = x.getSample (0, start + i) * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) m));
            fft.performFrequencyOnlyForwardTransform (buf.data());
            double e = 0.0;
            for (int k = 1; k < n / 2; ++k) { const double f = k * sr / n; if (f >= lo && f < hi) e += (double) buf[(size_t) k] * buf[(size_t) k]; }
            return 10.0 * std::log10 (e + 1.0e-12);
        };
        auto noiseOnly = [&] (int kind) { ParamFrame f = q; f[P::noiseNAOn] = 1.f; f[P::noiseNAType] = (float) kind; f[P::noiseNALevel] = 0.f; f[P::noiseNALength] = 400.f; f[P::noiseNADecay] = 2000.f; return f; };
        // 1 + 2. eight sources, each its own spectrum: tilt order BROWN < PINK < WHITE < BLUE < VIOLET
        {
            std::vector<double> tilt; std::vector<AudioBuffer<float>> outs; String e;
            for (int k = 0; k < 8; ++k)
            {
                auto x = renderKick (noiseOnly (k), G1, sr, 0.4);
                tilt.push_back (band (x, 4000, 16000, 0.05, 0.08) - band (x, 100, 400, 0.05, 0.08));
                outs.push_back (x);
                e << Choices::noiseKinds()[k].substring (0, 3) << " " << String (tilt.back(), 0) << "  ";
            }
            double minD = 1.0e9; for (int i = 0; i < 8; ++i) for (int j = i + 1; j < 8; ++j) minD = jmin (minD, diff (outs[(size_t) i], outs[(size_t) j]));
            const bool order = tilt[2] < tilt[1] && tilt[1] < tilt[0] && tilt[0] < tilt[3] && tilt[3] < tilt[4];
            check (order && minD > 1.0e-3, "M04 19.1 / 19.2 eight noise sources with their own spectra", e + "(hi - lo dB)");
        }
        // 3. modulation: the mod envelope opens the cutoff at the start, the end stays dark
        {
            ParamFrame f = noiseOnly (0); f[P::noiseNACutoff] = 500.f; f[P::noiseNAModOn] = 1.f; f[P::noiseNAModCut] = 1.f; f[P::noiseNAModD] = 30.f;
            auto x = renderKick (f, G1, sr, 0.4);
            const double early = band (x, 3000, 12000, 0.0, 0.02) - band (x, 100, 500, 0.0, 0.02), late = band (x, 3000, 12000, 0.25, 0.05) - band (x, 100, 500, 0.25, 0.05);
            check (early > late + 12.0, "M04 19.3 noise modulation (mod envelope -> cutoff)", "bright early " + String (early, 1) + " vs late " + String (late, 1) + " dB");
        }
        // 4. seed: FIXED = the same every hit, PER TRIGGER = new each hit, the same seed = the same export
        {
            GeProcessor p; setP (p, "noiseA_on", 1.f); for (auto* m : { "mix_punch_mute", "mix_body_mute", "mix_tail_mute", "mix_sub_mute" }) setP (p, m, 1.f);
            setP (p, "noiseA_random_mode", 0.f);
            std::vector<std::pair<int, MidiMessage>> ev { { 0, MidiMessage::noteOn (1, G1, 1.f) }, { 20000, MidiMessage::noteOn (1, G1, 1.f) } };
            auto x = hitProc (p, ev, 1.0);
            auto seg = [] (const AudioBuffer<float>& b, int a) { AudioBuffer<float> o (2, 4000); for (int c = 0; c < 2; ++c) o.copyFrom (c, 0, b, c, a, 4000); return o; };
            const int lat = p.getLatencySamples();
            const double fixedD = diff (seg (x, lat + 100), seg (x, 20000 + lat + 100));
            setP (p, "noiseA_random_mode", 1.f);
            auto y = hitProc (p, ev, 1.0);
            const double perD = diff (seg (y, lat + 100), seg (y, 20000 + lat + 100));
            const double same = diff (renderKick (noiseOnly (0), G1, sr, 0.3), renderKick (noiseOnly (0), G1, sr, 0.3));
            check (fixedD < 1.0e-6 && perD > 1.0e-3 && same == 0.0, "M04 19.4 SEED: FIXED repeats, PER TRIGGER changes, deterministic export",
                   "fixed " + String (fixedD, 7) + " per trigger " + String (perD, 4));
        }
        // 5. the transient engine makes its own attack signal (alone in a silent kick), POSITION moves it
        {
            ParamFrame f = q; f[P::trOn] = 1.f; f[P::trClickLvl] = 0.f; f[P::trOutput] = 4.f;      // (the PUNCH bus is muted here)
            ParamFrame g2 = f; g2[P::trPosition] = 100.f;
            auto x = renderKick (f, G1, sr, 0.4), y = renderKick (g2, G1, sr, 0.4);
            const float early = peak (x, 0, (int) (0.02 * sr)), late = peak (y, (int) (0.1 * sr), (int) (0.03 * sr)), lateEarly = peak (y, 0, (int) (0.02 * sr));
            check (early > 0.1f && late > 0.1f && lateEarly < 1.0e-4f, "M04 19.5 TRANSIENT: its own click, POSITION moves it", "peak " + String (early, 2));
        }
        // 6. the shaper follows the real envelope: ATTACK + raises the onset vs the body, SUSTAIN + the other way
        {
            ParamFrame f = base; f[P::shOn] = 1.f; f[P::clipDrive] = 0.f;
            auto ratio = [&] (const ParamFrame& pf) { auto x = renderKick (pf, G1, sr, 0.5); return 20.0 * std::log10 (peak (x, 0, (int) (0.006 * sr)) / (rms (x, (int) (0.15 * sr), (int) (0.1 * sr)) + 1.0e-9)); };
            ParamFrame atk = f; atk[P::shAttack] = 1.f;
            ParamFrame sus = f; sus[P::shSustain] = 1.f;
            const double r0 = ratio (f), ra = ratio (atk), rs = ratio (sus);
            check (ra > r0 + 2.0 && rs < r0 - 1.0, "M04 19.6 TRANSIENT SHAPER reacts to the real envelope", "onset / body " + String (r0, 1) + " -> attack+ " + String (ra, 1) + ", sustain+ " + String (rs, 1) + " dB");
        }
        // 7 + 8. eight independent layers: own pitch + own envelope each
        {
            ParamFrame f = q; const int ls = P::layer2On - P::layer1On;
            for (int i = 0; i < 8; ++i) { f[P::layer1On + i * ls] = 1.f; f[P::layer1Pitch + i * ls] = 24.f + 3.f * (float) i; f[P::layer1Len + i * ls] = 100.f + 60.f * (float) i;
                                          f[P::layer1EnvMode + i * ls] = 0.f; f[P::layer1Sus + i * ls] = 1.f; f[P::layer1Level + i * ls] = -12.f; }
            auto x = renderKick (f, G1, sr, 0.8);
            bool each = true; String e;
            for (int i = 0; i < 8; ++i)
            {
                ParamFrame g2 = f; g2[P::layer1Mute + i * ls] = 1.f;
                const double d = diff (x, renderKick (g2, G1, sr, 0.8));
                each &= d > 1.0e-4; e << String (d, 4) << " ";
            }
            // the shortest layer is gone after its LENGTH + release, the longest still plays
            ParamFrame only0 = f, only7 = f;
            for (int i = 0; i < 8; ++i) { only0[P::layer1On + i * ls] = i == 0 ? 1.f : 0.f; only7[P::layer1On + i * ls] = i == 7 ? 1.f : 0.f; }
            const auto a0 = renderKick (only0, G1, sr, 0.8), a7 = renderKick (only7, G1, sr, 0.8);
            const bool env = rms (a0, (int) (0.2 * sr), (int) (0.1 * sr)) < 1.0e-5 && rms (a7, (int) (0.4 * sr), (int) (0.1 * sr)) > 1.0e-3;
            check (each && env, "M04 19.7 / 19.8 eight independent layers, own pitch + envelope each", e);
        }
        // 9 + 10. groups / routing: a BODY layer plays on the body bus (body fader), a MULTI send to MASTER skips the clip
        {
            ParamFrame f = base; f[P::layer1On] = 1.f; f[P::layer1Group] = 1.f; f[P::layer1Level] = 0.f;
            ParamFrame fBodyOff = f; fBodyOff[P::mixBodyLvl] = -60.f;
            ParamFrame plainBodyOff = base; plainBodyOff[P::mixBodyLvl] = -60.f;
            const bool onBody = diff (renderKick (fBodyOff, G1, sr, 0.4), renderKick (plainBodyOff, G1, sr, 0.4)) < 1.0e-6 && diff (renderKick (f, G1, sr, 0.4), renderKick (base, G1, sr, 0.4)) > 1.0e-3;
            ParamFrame g2 = base; g2[P::layer1On] = 1.f; g2[P::layer1Route] = 1.f; g2[P::layer1SMaster] = 0.f; g2[P::layer1Level] = 12.f;
            for (int id2 : { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute }) g2[id2] = 1.f;
            g2[P::clipDrive] = 24.f;
            const float pkMaster = peak (renderKick (g2, G1, sr, 0.4));
            ParamFrame g3 = g2; g3[P::layer1SMaster] = -60.f; g3[P::layer1STex] = 0.f;
            const float pkTex = peak (renderKick (g3, G1, sr, 0.4));
            ParamFrame g4 = f; g4[P::grpBodyLevel] = -60.f;                   // the group level reaches its layer
            const bool grp = diff (renderKick (g4, G1, sr, 0.4), renderKick (base, G1, sr, 0.4)) < 1.0e-6;
            check (onBody && pkMaster > pkTex * 1.5f && grp, "M04 19.9 / 19.10 groups + routing (BODY bus, MASTER send after the clip, group level)",
                   "peak via MASTER " + String (pkMaster, 2) + " vs via TEXTURE (clipped) " + String (pkTex, 2));
        }
        // 11. SOLO / MUTE: one soloed layer silences the others; un-solo gives the full mix back
        {
            ParamFrame f = q; f[P::layer1On] = 1.f; f[P::layer2On] = 1.f; f[P::layer2Pitch] = 19.f;
            ParamFrame s1 = f; s1[P::layer1Solo] = 1.f;
            ParamFrame only1 = f; only1[P::layer2On] = 0.f;
            const bool ok = diff (renderKick (s1, G1, sr, 0.4), renderKick (only1, G1, sr, 0.4)) < 1.0e-6;
            ParamFrame m1 = f; m1[P::layer1Mute] = 1.f;
            ParamFrame only2 = f; only2[P::layer1On] = 0.f;
            check (ok && diff (renderKick (m1, G1, sr, 0.4), renderKick (only2, G1, sr, 0.4)) < 1.0e-6, "M04 19.11 layer SOLO / MUTE");
        }
        // 16. switching a layer on / off during a kick does not click
        {
            GeProcessor p; setP (p, "layer1_on", 1.f); setP (p, "layer1_length", 2000.f); setP (p, "layer1_env_mode", 0.f); setP (p, "layer1_sustain", 1.f); setP (p, "layer1_level", 0.f);
            for (auto* m : { "mix_punch_mute", "mix_body_mute", "mix_tail_mute", "mix_sub_mute" }) setP (p, m, 1.f);
            p.prepareToPlay (sr, 256);
            float maxStep = 0.f, last = 0.f, pk = 0.f;
            for (int blk = 0; blk < 120; ++blk)
            {
                AudioBuffer<float> bb (2, 256); bb.clear(); MidiBuffer m;
                if (blk == 0) m.addEvent (MidiMessage::noteOn (1, G1, 1.f), 0);
                if (blk == 40) setP (p, "layer1_mute", 1.f);
                if (blk == 70) setP (p, "layer1_mute", 0.f);
                p.processBlock (bb, m);
                for (int i = 0; i < 256; ++i) { const float v = bb.getSample (0, i); if (blk > 4) maxStep = jmax (maxStep, std::abs (v - last)); last = v; pk = jmax (pk, std::abs (v)); }
            }
            check (maxStep < 0.12f * pk, "M04 19.16 muting / unmuting a layer during a kick does not click", "max step " + String (maxStep / jmax (1.0e-6f, pk), 3) + " of peak");
        }
        // 17 + 18. everything on at once: finite; the processor (realtime) = renderKick (export)
        {
            GeProcessor p; const int ls = P::layer2On - P::layer1On;
            for (int i = 0; i < 8; ++i) { setP (p, def (P::layer1On + i * ls).id, 1.f); setP (p, def (P::layer1Src + i * ls).id, (float) (i + 3)); }
            setP (p, "osc1_on", 1.f); setP (p, "noiseA_on", 1.f); setP (p, "noiseB_on", 1.f); setP (p, "noiseB_type", 5.f); setP (p, "tr_on", 1.f); setP (p, "shaper_on", 1.f);
            setP (p, "shaper_attack", 0.5f); setP (p, "layer3_route_mode", 1.f); setP (p, "layer3_send_master", -6.f);
            p.refreshTables();
            const auto tb = p.currentTables();
            fge::dsp::OscTables ot; for (int i = 0; i < 8; ++i) ot.t[(size_t) i] = tb[(size_t) i].get();
            const auto rt = hitProc (p, { { 0, MidiMessage::noteOn (1, G1, (uint8) 127) } }, 0.6);
            const auto off = renderKick (p.frame(), G1, sr, 0.6, -1, 1.f, &p.shapes(), 0.0, -1.0, &ot);
            const bool finite = std::isfinite (rms (rt)) && peak (rt) < 16.f;
            check (finite && diff (rt, off) < 1.0e-6, "M04 19.17 / 19.18 all layers + noise + transient: stable, realtime = export",
                   "peak " + String (peak (rt), 2) + ", rt vs export " + String (diff (rt, off), 8));
        }
        // noise PITCH TRACKING (identity on G1 - checked on another note)
        {
            ParamFrame f = noiseOnly (0); f[P::noiseNACutoff] = 1000.f; f[P::noiseNAFilter] = 2.f; f[P::noiseNAReso] = 0.7f;
            ParamFrame t = f; t[P::noiseNATrack] = 1.f;
            check (diff (renderKick (f, 43, sr, 0.3), renderKick (t, 43, sr, 0.3)) > 1.0e-3, "M04 noise PITCH TRACKING moves the filter with the note");
        }
    }
    // ---------------------------------------------------------------- module 05: distortion & waveshaper engine (MODULE05.md 19)
    {
        namespace dd = fge::dsp::dist;
        const int ds = P::dist2On - P::dist1On;
        auto S = [&] (int st, int first) { return first + st * ds; };
        ParamFrame q = base; q[P::clipDrive] = 0.f;
        auto stage = [&] (ParamFrame& f, int st, int loc, int type, float drive)
        { f[S (st, P::dist1On)] = 1.f; f[S (st, P::dist1Loc)] = (float) loc; f[S (st, P::dist1Type)] = (float) type; f[S (st, P::dist1Drive)] = drive; f[S (st, P::dist1Order)] = (float) (st + 1); };
        auto band = [] (const AudioBuffer<float>& x, double lo, double hi, double a, double len, double rate = 44100.0)
        {
            const int order = 13, n = 1 << order, start = (int) (a * rate);
            juce::dsp::FFT fft (order);
            std::vector<float> buf ((size_t) n * 2, 0.f);
            const int m = jmin (n, (int) (len * rate));
            for (int i = 0; i < m && start + i < x.getNumSamples(); ++i) buf[(size_t) i] = x.getSample (0, start + i) * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) m));
            fft.performFrequencyOnlyForwardTransform (buf.data());
            double e = 0.0;
            for (int k = 1; k < n / 2; ++k) { const double f = k * rate / n; if (f >= lo && f < hi) e += (double) buf[(size_t) k] * buf[(size_t) k]; }
            return 10.0 * std::log10 (e + 1.0e-12);
        };
        auto relDiff = [&] (const AudioBuffer<float>& a, const AudioBuffer<float>& b, double from, double len)
        {
            const int i0 = (int) (from * sr), n = (int) (len * sr);
            double e = 0, r = 0;
            for (int i = i0; i < i0 + n && i < a.getNumSamples(); ++i) { const double d = a.getSample (0, i) - b.getSample (0, i); e += d * d; r += (double) b.getSample (0, i) * b.getSample (0, i); }
            return std::sqrt (e / (r + 1.0e-12));
        };
        const auto dry = renderKick (q, G1, sr, 0.5);
        // 1. eleven types, each its own curve (not one tanh with names)
        {
            std::vector<AudioBuffer<float>> o; String e;
            for (int t = 0; t < dd::NumTypes; ++t) { ParamFrame f = q; stage (f, 0, dd::LocGlobal, t, 0.7f); o.push_back (renderKick (f, G1, sr, 0.5)); }
            double minD = 1.0e9; int ma = 0, mb = 0;
            for (int i = 0; i < dd::NumTypes; ++i) for (int j = i + 1; j < dd::NumTypes; ++j) { const double d = diff (o[(size_t) i], o[(size_t) j]); if (d < minD) { minD = d; ma = i; mb = j; } }
            double minDry = 1.0e9; for (auto& x : o) minDry = jmin (minDry, diff (x, dry));
            check (minD > 1.0e-3 && minDry > 1.0e-3, "M05 19.1 11 distortion types, 11 different results",
                   "closest " + Choices::distTypes()[ma] + " / " + Choices::distTypes()[mb] + " " + String (minD, 4));
        }
        // 2. CUSTOM waveshaper: the drawn curve is the real curve (table = the points), two curves = two sounds
        {
            dd::ShapeParams sp; sp.type = dd::Custom; sp.interp = 0; const auto cv = dd::CurveData::preset (2); sp.curve = &cv;
            dd::Table t; t.build (sp, 1.f);
            const bool pts = std::abs (t.eval (0.25) - 0.8f) < 0.01f && std::abs (t.eval (-0.3) + 0.4f) < 0.01f && std::abs (t.eval (1.0) - 1.f) < 0.01f;
            ParamFrame f = q; stage (f, 0, dd::LocGlobal, dd::Custom, 0.3f);
            dd::DistData a, b; a.curve[0] = dd::CurveData::preset (1); b.curve[0] = dd::CurveData::preset (3); b.curveVersion[0] = 1;
            const double d = diff (renderKick (f, G1, sr, 0.5, -1, 1.f, nullptr, 0.0, -1.0, nullptr, nullptr, &a), renderKick (f, G1, sr, 0.5, -1, 1.f, nullptr, 0.0, -1.0, nullptr, nullptr, &b));
            check (pts && d > 1.0e-3, "M05 19.2 CUSTOM waveshaper: own curves, the points are the real curve", "two curves diff " + String (d, 4));
        }
        // 3 + 4. four+ independent stages; the order changes the signal
        {
            ParamFrame f = q; stage (f, 0, dd::LocTail, dd::Hard, 0.6f); stage (f, 1, dd::LocTail, dd::Wavefold, 0.6f); stage (f, 2, dd::LocTail, dd::Tube, 0.5f); stage (f, 3, dd::LocTail, dd::Digital, 0.5f);
            const auto all = renderKick (f, G1, sr, 0.5);
            bool each = true; String e;
            for (int st = 0; st < 4; ++st) { ParamFrame g = f; g[S (st, P::dist1On)] = 0.f; const double d = diff (all, renderKick (g, G1, sr, 0.5)); each &= d > 1.0e-4; e << String (d, 4) << " "; }
            ParamFrame sw = f; sw[S (0, P::dist1Order)] = 2.f; sw[S (1, P::dist1Order)] = 1.f;
            const double ord = diff (all, renderKick (sw, G1, sr, 0.5));
            check (each && ord > 1.0e-3, "M05 19.3 / 19.4 four independent stages, the ORDER changes the sound", "each " + e + "swap 1<->2 " + String (ord, 4));
        }
        // 5. serial vs parallel (and parallel is not just MIX)
        {
            ParamFrame ser = q; stage (ser, 0, dd::LocGlobal, dd::Wavefold, 0.7f); stage (ser, 1, dd::LocGlobal, dd::Hard, 0.7f);
            ParamFrame par = ser; par[S (0, P::dist1Path)] = 1.f; par[S (1, P::dist1Path)] = 2.f;
            ParamFrame mix = q; stage (mix, 0, dd::LocGlobal, dd::Wavefold, 0.7f); mix[S (0, P::dist1Mix)] = 0.5f;
            const auto a = renderKick (ser, G1, sr, 0.5), b = renderKick (par, G1, sr, 0.5), c = renderKick (mix, G1, sr, 0.5);
            check (diff (a, b) > 1.0e-3 && diff (b, c) > 1.0e-3, "M05 19.5 SERIAL and PARALLEL routing (parallel != MIX)", "serial vs parallel " + String (diff (a, b), 4));
        }
        // 6. multiband: the bands sum flat (nothing distorted), one band distorted leaves the others
        {
            // the magnitude response of the split + sum (an impulse through the chain at the engine rate) = flat
            ParamFrame f = q; for (int k = 0; k < 3; ++k) { stage (f, k, dd::LocGlobal, dd::Hard, 1.f); f[S (k, P::dist1Band)] = (float) (k + 1); f[S (k, P::dist1Mix)] = 0.f; }
            double worst = 0.0;
            {
                const double rate = 44100.0 * 4.0;
                env::Bank eb; auto bank = std::make_unique<dd::DistBank>(); auto tb = std::make_unique<dd::DistTables>(); tb->allocate();
                dd::cookDist (f, nullptr, eb, *bank, *tb, rate, false);
                auto cs = std::make_unique<dd::ChainState>(); cs->reset();
                const int order = 15, n = 1 << order; juce::dsp::FFT fft (order); std::vector<float> buf ((size_t) n * 2, 0.f);
                for (int i = 0; i < n; ++i) buf[(size_t) i] = cs->process (*bank, dd::LocGlobal, i == 0 ? 1.f : 0.f, dd::ModCtx {}, rate);
                fft.performFrequencyOnlyForwardTransform (buf.data());
                for (int k = 1; k < n / 2; ++k) { const double fr = k * rate / n; if (fr > 20.0 && fr < 20000.0) worst = jmax (worst, std::abs (20.0 * std::log10 (buf[(size_t) k] + 1.0e-12))); }
            }
            const auto flat = renderKick (f, G1, sr, 0.5);
            ParamFrame hi = f; hi[S (2, P::dist1Mix)] = 1.f; hi[S (2, P::dist1Auto)] = 0.f;     // the HIGH band distorted, LOW + MID clean
            const auto h = renderKick (hi, G1, sr, 0.5);
            const double lowMove = std::abs (band (h, 40, 150, 0.0, 0.18) - band (flat, 40, 150, 0.0, 0.18)), highMove = std::abs (band (h, 4000, 12000, 0.0, 0.18) - band (flat, 4000, 12000, 0.0, 0.18));
            check (worst < 0.1 && lowMove < 1.0 && highMove > 1.0, "M05 19.6 MULTIBAND: LR4 bands sum flat, a HIGH-only stage leaves the low end",
                   "sum dev " + String (worst, 2) + " dB, low " + String (lowMove, 2) + " dB, high " + String (highMove, 1) + " dB");
        }
        // 7. PRE and POST filter are separate (pre = what gets distorted, post = what comes out)
        {
            ParamFrame pre = q; stage (pre, 0, dd::LocGlobal, dd::Hard, 1.f); pre[S (0, P::dist1Auto)] = 0.f;
            ParamFrame post = pre;
            pre[S (0, P::dist1PreOn)] = 1.f; pre[S (0, P::dist1PreType)] = 0.f; pre[S (0, P::dist1PreCut)] = 300.f;
            post[S (0, P::dist1PostOn)] = 1.f; post[S (0, P::dist1PostType)] = 0.f; post[S (0, P::dist1PostCut)] = 300.f;
            const auto a = renderKick (pre, G1, sr, 0.5), b = renderKick (post, G1, sr, 0.5);
            const double hfPre = band (a, 3000, 10000, 0.0, 0.18), hfPost = band (b, 3000, 10000, 0.0, 0.18);
            check (diff (a, b) > 1.0e-3 && hfPre > hfPost + 6.0, "M05 19.7 PRE and POST filters work independently", "HF 3-10k: pre LP " + String (hfPre, 1) + " vs post LP " + String (hfPost, 1) + " dB");
        }
        // 8. dynamic distortion follows its envelope (drive up at the start, gone later)
        {
            ParamFrame f = q; stage (f, 0, dd::LocGlobal, dd::Hard, 0.f);
            ParamFrame m = f; m[S (0, P::dist1MDrive)] = 1.f; m[S (0, P::dist1ModA)] = 0.f; m[S (0, P::dist1ModD)] = 30.f;
            const auto a = renderKick (f, G1, sr, 0.5), b = renderKick (m, G1, sr, 0.5);
            const double early = relDiff (b, a, 0.0, 0.03), late = relDiff (b, a, 0.3, 0.1);
            check (early > 0.05 && early > late * 8.0, "M05 19.8 DYNAMIC distortion follows the envelope", "change early " + String (early, 3) + " late " + String (late, 4));
        }
        // 9. per layer / per group
        {
            ParamFrame f = q; for (int id : { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute }) f[id] = 1.f;
            f[P::layer1On] = 1.f; f[P::layer2On] = 1.f; f[P::layer2Pitch] = 19.f;
            ParamFrame d = f; stage (d, 0, dd::LocLayer1, dd::Hard, 1.f);
            ParamFrame only2 = f; only2[P::layer1On] = 0.f; ParamFrame only2d = d; only2d[P::layer1On] = 0.f;
            const bool layerOk = diff (renderKick (f, G1, sr, 0.4), renderKick (d, G1, sr, 0.4)) > 1.0e-3 && diff (renderKick (only2, G1, sr, 0.4), renderKick (only2d, G1, sr, 0.4)) < 1.0e-7;
            ParamFrame g = q; stage (g, 0, dd::LocBody, dd::Hard, 1.f);
            ParamFrame gm = g; gm[P::mixBodyMute] = 1.f; ParamFrame qm = q; qm[P::mixBodyMute] = 1.f;
            const bool groupOk = diff (renderKick (g, G1, sr, 0.4), dry) > 1.0e-3 && diff (renderKick (gm, G1, sr, 0.4), renderKick (qm, G1, sr, 0.4)) < 1.0e-7;
            check (layerOk && groupOk, "M05 19.9 distortion per LAYER and per GROUP (only there)");
        }
        // 10. oversampling really lowers the aliasing (a hard clip of a 7.3 kHz sine at the engine rate)
        {
            const double rate = 44100.0 * 4.0;
            auto alias = [&] (int os)
            {
                ParamFrame f = q; stage (f, 0, dd::LocGlobal, dd::Hard, 1.f); f[S (0, P::dist1Os)] = (float) os; f[S (0, P::dist1Auto)] = 0.f;
                env::Bank eb; auto bank = std::make_unique<dd::DistBank>(); auto tb = std::make_unique<dd::DistTables>(); tb->allocate();
                dd::cookDist (f, nullptr, eb, *bank, *tb, rate, false);
                auto cs = std::make_unique<dd::ChainState>(); cs->reset();
                AudioBuffer<float> out (1, 1 << 15);
                for (int i = 0; i < out.getNumSamples(); ++i) out.setSample (0, i, cs->process (*bank, dd::LocGlobal, 0.5f * (float) std::sin (2.0 * MathConstants<double>::pi * 7300.0 * i / rate), dd::ModCtx {}, rate));
                const int order = 14, n = 1 << order; juce::dsp::FFT fft (order); std::vector<float> buf ((size_t) n * 2, 0.f);
                for (int i = 0; i < n; ++i) buf[(size_t) i] = out.getSample (0, 8000 + i) * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) n));
                fft.performFrequencyOnlyForwardTransform (buf.data());
                double e = 0.0;
                for (int k = 1; k < n / 2; ++k)
                {
                    const double fr = k * rate / n; if (fr < 30.0 || fr > 20000.0) continue;
                    bool harm = false; for (int h = 1; h * 7300.0 < 21000.0; ++h) harm |= std::abs (fr - h * 7300.0) < 6.0 * rate / n;
                    if (! harm) e += (double) buf[(size_t) k] * buf[(size_t) k];
                }
                return 10.0 * std::log10 (e + 1.0e-20);
            };
            const double a0 = alias (0), a2 = alias (2);
            check (a2 < a0 - 6.0, "M05 19.10 OVERSAMPLING lowers the aliasing (0..20 kHz, non-harmonic)", "4x " + String (a0, 1) + " dB -> 16x " + String (a2, 1) + " dB");
        }
        // 11 + 12. the visualizer is real: the transfer curve = the measured stage, the spectrum = the real kick's
        {
            ParamFrame f = q; stage (f, 0, dd::LocGlobal, dd::Soft, 0.5f); f[S (0, P::dist1Auto)] = 0.f; f[S (0, P::dist1Bias)] = 0.f;
            fge::gui::DistScope::Request rq; rq.f = f; rq.stage = 0;
            const auto r = fge::gui::DistScope::analyse (rq);
            const double rate = 44100.0 * 4.0;
            env::Bank eb; auto bank = std::make_unique<dd::DistBank>(); auto tb = std::make_unique<dd::DistTables>(); tb->allocate();
            dd::cookDist (f, nullptr, eb, *bank, *tb, rate, false);
            auto cs = std::make_unique<dd::ChainState>(); cs->reset();
            auto curveAt = [&] (float x) { const float pos = (x + 1.5f) / 3.f * (float) (r.curveX.size() - 1); const int i = jlimit (0, (int) r.curveX.size() - 2, (int) pos); const float fr = pos - (float) i;
                                            return r.curveY[(size_t) i] + fr * (r.curveY[(size_t) i + 1] - r.curveY[(size_t) i]); };
            float worst = 0.f, pk = 0.f, xPrev = 0.f;
            for (int i = 0; i < 40000; ++i)
            {
                const float x = 1.4f * (float) std::sin (2.0 * MathConstants<double>::pi * 1000.0 * i / rate);
                const float y = cs->process (*bank, dd::LocGlobal, x, dd::ModCtx {}, rate);
                if (i > 20000) { worst = jmax (worst, std::abs (y - curveAt (0.5f * (x + xPrev)))); pk = jmax (pk, std::abs (y)); }
                xPrev = x;
            }
            ParamFrame g = f; g[S (0, P::dist1Drive)] = 0.9f; rq.f = g;
            const auto r2 = fge::gui::DistScope::analyse (rq);
            const bool moves = std::abs (r2.curveY[155] - r.curveY[155]) > 0.02f;           // (x = 0.05: drive moves the curve)
            // spectrum: a hard clip adds highs to the 'after' spectrum; MIX 0 = the same spectrum
            ParamFrame h = q; stage (h, 0, dd::LocGlobal, dd::Hard, 1.f); rq.f = h;
            const auto s1 = fge::gui::DistScope::analyse (rq);
            auto hf = [] (const std::vector<float>& sp) { double e = 0; int n = 0; for (int k = 0; k < fge::gui::DistScope::bins; ++k) if (fge::gui::DistScope::binHz (k) > 4000.f) { e += sp[(size_t) k]; ++n; } return e / jmax (1, n); };
            ParamFrame h0 = h; h0[S (0, P::dist1Mix)] = 0.f; rq.f = h0;
            const auto s0 = fge::gui::DistScope::analyse (rq);
            double same = 0; for (int k = 0; k < fge::gui::DistScope::bins; ++k) same = jmax (same, (double) std::abs (s0.specA[(size_t) k] - s0.specB[(size_t) k]));
            check (worst < 0.03f * pk && moves && hf (s1.specA) > hf (s1.specB) + 3.0 && same < 0.1,
                   "M05 19.11 / 19.12 TRANSFER CURVE = the measured stage, SPECTRUM = the real kick",
                   "curve error " + String (worst / jmax (1.0e-6f, pk), 4) + ", HF after - before " + String (hf (s1.specA) - hf (s1.specB), 1) + " dB, mix 0 " + String (same, 3) + " dB");
        }
        // 13. MORPH moves smoothly (11 steps: no step much bigger than the others)
        {
            ParamFrame f = q; stage (f, 0, dd::LocGlobal, dd::Soft, 0.7f); f[S (0, P::dist1MorphT)] = (float) dd::Wavefold;
            std::vector<AudioBuffer<float>> o;
            for (int k = 0; k <= 10; ++k) { ParamFrame g = f; g[S (0, P::dist1Morph)] = (float) k / 10.f; o.push_back (renderKick (g, G1, sr, 0.4)); }
            double mx = 0, sum = 0, mn = 1.0e9;
            for (int k = 0; k < 10; ++k) { const double d = diff (o[(size_t) k], o[(size_t) k + 1]); mx = jmax (mx, d); mn = jmin (mn, d); sum += d; }
            check (mn > 1.0e-5 && mx < 2.5 * sum / 10.0, "M05 19.13 MORPH is continuous (both algorithms run, the outputs blend)", "max step / mean " + String (mx / (sum / 10.0), 2));
        }
        // 14. presets / files keep the whole configuration (+ every factory preset makes a sound)
        {
            GeProcessor p;
            distt::apply (p, distt::presets()[12]);                                // DIGITAL CHAOS (multiband)
            dd::CurveData cv = dd::CurveData::preset (3); p.setDistCurve (5, cv);
            setP (p, "dist6_on", 1.f); setP (p, "dist6_type", (float) dd::Custom); setP (p, "dist6_location", 1.f); setP (p, "dist_xover_low", 333.f);
            const auto before = p.frame(); const auto curveBefore = p.distData().curve[5].toString();
            auto x = distt::save (p, distt::ScFull, 0, 0);
            distt::resetAll (p);
            const bool reset = p.apvts.getRawParameterValue ("dist1_on")->load() < 0.5f;
            distt::load (p, *x, 0, 0);
            bool same = p.distData().curve[5].toString() == curveBefore;
            for (int i = 0; i < P::count; ++i) if (String (def (i).id).startsWith ("dist")) same &= std::abs (p.frame()[i] - before[i]) < 1.0e-4f;
            // a chain file into another location
            auto ch = distt::save (p, distt::ScChain, 0, dd::LocTail);
            distt::load (p, *ch, 0, dd::LocPunch);
            const bool chain = distt::chain (p, dd::LocPunch).size() == 3;
            bool allSound = true; String e;
            for (auto& pr : distt::presets())
            {
                GeProcessor pp; distt::apply (pp, pr);
                ParamFrame pf = pp.frame(), ref = base; pf[P::layer1On] = ref[P::layer1On] = 1.f; pf[P::layer1Group] = ref[P::layer1Group] = 4.f;   // something on TEXTURE too
                const auto k = renderKick (pf, G1, sr, 0.4, -1, 1.f, nullptr, 0.0, -1.0, nullptr, nullptr, &pp.distData());
                const bool good = std::isfinite (rms (k)) && peak (k) < 4.f && diff (k, renderKick (ref, G1, sr, 0.4)) > 1.0e-4;
                if (! good) e << pr.name << " ";
                allSound &= good;
            }
            check (reset && same && chain && allSound, "M05 19.14 presets / files restore the full configuration (stage, chain, multiband, parallel, full)", e);
        }
        // 15. no uncontrolled level: every type at full drive with AUTO GAIN stays near the input level
        {
            const double r0 = rms (dry); double worst = 0; String e;
            for (int t = 0; t < dd::NumTypes; ++t)
            {
                ParamFrame f = q; stage (f, 0, dd::LocGlobal, t, 1.f);
                const auto x = renderKick (f, G1, sr, 0.5);
                const double dB = 20.0 * std::log10 (rms (x) / r0);
                worst = jmax (worst, std::abs (dB)); e << String (dB, 1) << " ";
            }
            check (worst < 6.0, "M05 19.15 AUTO GAIN: no level jumps (every type at full drive within 6 dB)", e + "dB");
        }
        // 16. switching the type / moving DRIVE during a kick does not click
        {
            auto run = [&] (bool sw)
            {
                GeProcessor p; setP (p, "dist1_on", 1.f); setP (p, "dist1_location", 3.f); setP (p, "dist1_drive", 0.7f);
                p.prepareToPlay (sr, 256);
                float maxStep = 0.f, last = 0.f, pk = 0.f;
                for (int blk = 0; blk < 60; ++blk)
                {
                    AudioBuffer<float> bb (2, 256); bb.clear(); MidiBuffer m;
                    if (blk == 0) m.addEvent (MidiMessage::noteOn (1, G1, 1.f), 0);
                    if (sw && blk > 4 && blk % 4 == 0) { setP (p, "dist1_type", (float) ((blk / 4) % dd::NumTypes)); setP (p, "dist1_drive", (blk / 4) % 2 ? 0.2f : 0.9f); }
                    p.processBlock (bb, m);
                    for (int i = 0; i < 256; ++i) { const float v = bb.getSample (0, i); if (blk > 4) maxStep = jmax (maxStep, std::abs (v - last)); last = v; pk = jmax (pk, std::abs (v)); }
                }
                return maxStep / jmax (1.0e-6f, pk);
            };
            float worstType = 0.f;
            for (int t = 0; t < dd::NumTypes; ++t)
            {
                GeProcessor p; setP (p, "dist1_on", 1.f); setP (p, "dist1_location", 3.f); setP (p, "dist1_type", (float) t); setP (p, "dist1_drive", 0.9f);
                auto x = hitProc (p, { { 0, MidiMessage::noteOn (1, G1, 1.f) } }, 0.35);
                float ms = 0.f, pk = peak (x); for (int i = 2000; i < x.getNumSamples(); ++i) ms = jmax (ms, std::abs (x.getSample (0, i) - x.getSample (0, i - 1)));
                worstType = jmax (worstType, ms / jmax (1.0e-6f, pk));
            }
            const float s = run (true);
            check (s < worstType * 1.3f + 0.02f, "M05 19.16 switching TYPE / DRIVE during a kick does not click", "max step " + String (s, 3) + " vs steady types " + String (worstType, 3));
        }
        // 17. CPU with all 16 stages (4 x 16x oversampling, parallel, multiband, layer)
        {
            GeProcessor p; cpuStress (p);
            GeProcessor plain;
            GeProcessor typical; distt::apply (typical, distt::presets()[5]);    // DENSE DISTORTION (3 tail stages) + a 8x global hard clip
            setP (typical, "dist4_on", 1.f); setP (typical, "dist4_type", 1.f); setP (typical, "dist4_oversampling", 1.f);
            const double base0 = cpuTime (plain, 2.0), typ = cpuTime (typical, 2.0), full = cpuTime (p, 2.0);
                        // (measured against the plain kick on the same machine: cloud machines differ in speed from run to run)
            check (full < 16.0 * base0 && typ < 4.0 * base0, "M05 19.17 CPU: 16 stages (4 at 16x) bounded, a usual chain light",
                   "no dist " + String (100.0 * base0, 1) + " %, 4 stages " + String (100.0 * typ, 1) + " %, 16 stages " + String (100.0 * full, 1) + " % of one core");
        }
        // the owner's listening file: the MODERN factory kick, one bar (4 kicks at 150 BPM, cut on the beat, 6 ms fade) per distortion preset
        {
            const char* names[] { "(dry)", "MODERN HARD CLIP", "CLEAN AGGRESSION", "DENSE DISTORTION", "RAW ASYMMETRIC", "BRUTAL FOLD", "HARD DIGITAL", "CRUSHED TAIL", "CLASSIC SATURATION", "METALLIC FOLD" };
            const int beat = (int) (0.4 * sr), fade = (int) (0.006 * sr);
            AudioBuffer<float> demo (2, beat * 4 * (int) std::size (names)); demo.clear();
            String order;
            for (int k = 0; k < (int) std::size (names); ++k)
            {
                GeProcessor pp; pp.setCurrentProgram (1);
                for (auto& pr : distt::presets()) if (String (pr.name) == names[k]) distt::apply (pp, pr);
                auto x = renderKick (pp.frame(), G1, sr, 0.4, -1, 1.f, &pp.shapes(), 0.0, -1.0, nullptr, nullptr, &pp.distData());
                for (int i = 0; i < fade; ++i) for (int c = 0; c < 2; ++c) x.setSample (c, beat - fade + i, x.getSample (c, beat - fade + i) * (1.f - (float) i / (float) fade));
                for (int b = 0; b < 4; ++b) for (int c = 0; c < 2; ++c) demo.copyFrom (c, (k * 4 + b) * beat, x, c, 0, beat);
                order << String (k * 1.6, 1) << " s " << names[k] << "\n";
            }
            demo.applyGain (0.5f / jmax (1.0e-6f, peak (demo)) * 1.4f);
            writeWav (outDir.getChildFile ("4ge_m05_distortion_presets.wav"), demo);
            outDir.getChildFile ("4ge_m05_distortion_presets.txt").replaceWithText (order);
        }
        // 18. realtime = export (stages everywhere, a custom curve, a layer stage, multiband, parallel)
        {
            GeProcessor p;
            distt::apply (p, distt::presets()[13]);                                // COMPLEX HARMONICS (parallel)
            setP (p, "dist4_on", 1.f); setP (p, "dist4_type", (float) dd::Custom); setP (p, "dist4_location", 0.f); setP (p, "dist4_band", 2.f);
            p.setDistCurve (3, dd::CurveData::preset (2));
            setP (p, "layer1_on", 1.f); setP (p, "dist5_on", 1.f); setP (p, "dist5_location", (float) dd::LocLayer1); setP (p, "dist5_type", (float) dd::Foldback); setP (p, "dist5_oversampling", 1.f);
            setP (p, "dist5_mod_source", 3.f); setP (p, "dist5_mod_drive", 0.5f);
            const auto rt = hitProc (p, { { 0, MidiMessage::noteOn (1, G1, (uint8) 127) } }, 0.5);
            const auto off = renderKick (p.frame(), G1, sr, 0.5, -1, 1.f, &p.shapes(), 0.0, -1.0, nullptr, nullptr, &p.distData());
            const bool finite = std::isfinite (rms (rt)) && peak (rt) < 8.f;
            check (finite && diff (rt, off) < 1.0e-6, "M05 19.18 realtime = export (+ stable)", "rt vs export " + String (diff (rt, off), 8));
        }
    }
    // ---------------------------------------------------------------- module 06: filter, resonance & screech engine (MODULE06.md 13)
    {
        namespace m6 = fge::dsp::m06;
        const int fs = P::flt2On - P::flt1On;
        auto F = [&] (int i, int first) { return first + i * fs; };
        ParamFrame q = base; q[P::clipDrive] = 0.f;
        const double rate = 44100.0 * 4.0;
        // the magnitude response (dB per log bin, 30 Hz .. 20 kHz) of one filter setting: an impulse through the real filter code
        auto response = [&] (const ParamFrame& f, int idx, float vel = 1.f)
        {
            env::Bank eb; auto bank = std::make_unique<m6::Bank>(); m6::cookBank (f, eb, *bank, rate);
            auto st = std::make_unique<m6::State>(); st->reset();
            const int order = 15, n = 1 << order; juce::dsp::FFT fft (order); std::vector<float> buf ((size_t) n * 2, 0.f);
            m6::Ctx cx; cx.vel = vel; cx.ms = 1.0e6f; cx.lengthMs = 1.0e7f;
            for (int i = 0; i < n; ++i) { float l = i == 0 ? 1.f : 0.f, r = l; st->process (*bank, bank->f[(size_t) idx].loc, bank->f[(size_t) idx].pos, l, r, cx, rate); buf[(size_t) i] = l; }
            fft.performFrequencyOnlyForwardTransform (buf.data());
            std::vector<double> db;
            for (int k = 0; k < 120; ++k) { const double hz = 30.0 * std::pow (20000.0 / 30.0, k / 119.0); db.push_back (20.0 * std::log10 (buf[(size_t) jlimit (1, n / 2 - 1, (int) (hz * n / rate))] + 1.0e-9)); }
            return db;
        };
        auto filt = [&] (int type, float cut, float res)
        { ParamFrame f = q; f[P::flt1On] = 1.f; f[P::flt1Type] = (float) type; f[P::flt1Cut] = cut; f[P::flt1Res] = res; return f; };
        auto binOf = [] (double hz) { return jlimit (0, 119, (int) std::lround (119.0 * std::log (hz / 30.0) / std::log (20000.0 / 30.0))); };
        // 1. cutoff: a real, smooth change of colour (the response moves with it)
        {
            const auto a = response (filt (0, 500.f, 0.f), 0), b = response (filt (0, 5000.f, 0.f), 0);
            const double at2k = b[(size_t) binOf (2000)] - a[(size_t) binOf (2000)];
            // automating the cutoff during a kick does not click
            GeProcessor p; setP (p, "flt1_on", 1.f); setP (p, "flt1_location", 0.f); p.prepareToPlay (sr, 256);
            float maxStep = 0.f, last = 0.f, pk = 0.f;
            for (int blk = 0; blk < 60; ++blk)
            {
                AudioBuffer<float> bb (2, 256); bb.clear(); MidiBuffer m;
                if (blk == 0) m.addEvent (MidiMessage::noteOn (1, G1, 1.f), 0);
                if (blk > 4) setP (p, "flt1_cutoff", blk % 2 ? 300.f : 12000.f);
                p.processBlock (bb, m);
                for (int i = 0; i < 256; ++i) { const float v = bb.getSample (0, i); if (blk > 4) maxStep = jmax (maxStep, std::abs (v - last)); last = v; pk = jmax (pk, std::abs (v)); }
            }
            GeProcessor p0; setP (p0, "flt1_on", 1.f); setP (p0, "flt1_location", 0.f); setP (p0, "flt1_cutoff", 12000.f);
            auto x = hitProc (p0, { { 0, MidiMessage::noteOn (1, G1, 1.f) } }, 0.35);
            float base0 = 0.f; for (int i = 2000; i < x.getNumSamples(); ++i) base0 = jmax (base0, std::abs (x.getSample (0, i) - x.getSample (0, i - 1)));
            check (at2k > 15.0 && maxStep < base0 * 1.5f + 0.02f * pk, "M06 cutoff: a real change of colour, automation does not click",
                   "2 kHz: " + String (at2k, 1) + " dB louder at 5 kHz cutoff, step " + String (maxStep, 3) + " vs " + String (base0, 3));
        }
        // 2. resonance really rises at the cutoff
        {
            const auto a = response (filt (0, 2000.f, 0.f), 0), b = response (filt (0, 2000.f, 0.9f), 0);
            const double rise = b[(size_t) binOf (2000)] - a[(size_t) binOf (2000)];
            check (rise > 10.0, "M06 resonance: a real peak at the cutoff", "+" + String (rise, 1) + " dB");
        }
        // 3. ten filter types, ten different responses
        {
            std::vector<std::vector<double>> r;
            for (int t = 0; t < m6::NumFTypes; ++t) { auto f = filt (t, 1500.f, 0.5f); f[P::flt1Vowel] = 1.f; f[P::flt1SvfMode] = 0.3f; r.push_back (response (f, 0)); }
            double minD = 1.0e9; int ma = 0, mb = 0;
            for (int i = 0; i < m6::NumFTypes; ++i) for (int j = i + 1; j < m6::NumFTypes; ++j)
            {
                double d = 0; for (size_t k = 0; k < r[(size_t) i].size(); ++k) d = jmax (d, std::abs (r[(size_t) i][k] - r[(size_t) j][k]));
                if (d < minD) { minD = d; ma = i; mb = j; }
            }
            // (ALL PASS: flat magnitude by design - its difference is the phase; with MIX 50 % it makes notches)
            check (minD > 3.0, "M06 10 filter types with their own response", "closest " + Choices::m06FilterTypes()[ma] + " / " + Choices::m06FilterTypes()[mb] + " " + String (minD, 1) + " dB");
        }
        // 4. extreme Q: stable (noise through every type at full resonance and drive)
        {
            bool stable = true; String e;
            for (int t = 0; t < m6::NumFTypes; ++t)
            {
                auto f = filt (t, 3000.f, 1.f); f[P::flt1Drive] = 1.f;
                env::Bank eb; auto bank = std::make_unique<m6::Bank>(); m6::cookBank (f, eb, *bank, rate);
                auto st = std::make_unique<m6::State>(); st->reset();
                Random rnd (3); float pk = 0.f; bool fin = true; m6::Ctx cx;
                for (int i = 0; i < 200000; ++i) { float l = rnd.nextFloat() * 2.f - 1.f, r = l; st->process (*bank, 3, 0, l, r, cx, rate); fin &= std::isfinite (l); pk = jmax (pk, std::abs (l)); }
                if (! fin || pk > 30.f) { stable = false; e << Choices::m06FilterTypes()[t] << " " << String (pk, 1) << " "; }
            }
            check (stable, "M06 extreme resonance + drive: every type stays stable", e);
        }
        // the screech alone (on TEXTURE, the kick muted) for the next checks
        auto screechOnly = [&] (ParamFrame f, double secs = 0.9)
        {
            f[P::scrOn] = 1.f; f[P::scrOutput] = 1.f; for (int id : { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute }) f[id] = 1.f;
            f[P::coreLength] = 1500.f; f[P::coreAutoLen] = 0.f;
            return renderKick (f, G1, sr, secs);
        };
        auto centroid = [&] (const AudioBuffer<float>& x, double a, double len)
        {
            const int order = 12, n = 1 << order, st = (int) (a * sr); juce::dsp::FFT fft (order); std::vector<float> buf ((size_t) n * 2, 0.f);
            const int m = jmin (n, (int) (len * sr));
            for (int i = 0; i < m && st + i < x.getNumSamples(); ++i) buf[(size_t) i] = x.getSample (0, st + i) * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) m));
            fft.performFrequencyOnlyForwardTransform (buf.data());
            double s1 = 0, s0 = 0; for (int k = 1; k < n / 2; ++k) { s1 += k * sr / n * buf[(size_t) k]; s0 += buf[(size_t) k]; }
            return s1 / jmax (1.0e-9, s0);
        };
        // 5. eight screech modes, eight different sounds (no sample: synthesis only)
        {
            std::vector<AudioBuffer<float>> o; String e;
            for (int m = 0; m < 8; ++m) { ParamFrame f = q; f[P::scrMode] = (float) m; o.push_back (screechOnly (f)); e << roundToInt (20.0 * std::log10 (rms (o.back(), (int) (0.2 * sr), (int) (0.3 * sr)) + 1.0e-9)) << " "; }
            double minD = 1.0e9; for (int i = 0; i < 8; ++i) for (int j = i + 1; j < 8; ++j) minD = jmin (minD, diff (o[(size_t) i], o[(size_t) j]));
            bool loud = true; for (auto& x : o) loud &= rms (x, (int) (0.2 * sr), (int) (0.3 * sr)) > 0.003;
            check (minD > 1.0e-3 && loud, "M06 SCREECH: 8 synthesis modes, all sound, all different", "rms dB " + e);
        }
        // 6. FM SCREECH: the index really modulates (brighter, more partials)
        {
            ParamFrame a = q; a[P::scrMode] = 3.f; a[P::scrFmIndex] = 0.f; a[P::sf1On] = 0.f;
            ParamFrame b = a; b[P::scrFmIndex] = 1.f;
            const double c0 = centroid (screechOnly (a), 0.25, 0.1), c1 = centroid (screechOnly (b), 0.25, 0.1);
            check (c1 > c0 * 1.5, "M06 FM SCREECH: audible frequency modulation", "centroid " + String (c0, 0) + " -> " + String (c1, 0) + " Hz");
        }
        // 7. FORMANT MORPH is continuous (vowel 0..4 in 16 steps: no jump)
        {
            std::vector<std::vector<double>> r;
            for (int k = 0; k <= 16; ++k) { auto f = filt (m6::FFormant, 1000.f, 0.5f); f[P::flt1Vowel] = 4.f * (float) k / 16.f; r.push_back (response (f, 0)); }
            double mx = 0, sum = 0;
            for (int k = 0; k < 16; ++k) { double d = 0; for (size_t i = 0; i < r[0].size(); ++i) d += std::abs (r[(size_t) k][i] - r[(size_t) k + 1][i]); mx = jmax (mx, d); sum += d; }
            check (mx < 3.0 * sum / 16.0 && sum > 1.0, "M06 FORMANT MORPH: smooth A -> E -> I -> O -> U", "max step / mean " + String (mx / (sum / 16.0), 2));
        }
        // 8. FEEDBACK: aggressive but never runs away (95 %, full drive, stability 0)
        {
            ParamFrame f = q; f[P::scrMode] = 5.f; f[P::scrFbAmt] = 0.95f; f[P::scrFbDrive] = 1.f; f[P::scrFbStab] = 0.f; f[P::scrLevel] = 0.f; f[P::scrLength] = 1200.f;
            const auto x = screechOnly (f, 1.4);
            double mean = 0; for (int i = 0; i < x.getNumSamples(); ++i) mean += x.getSample (0, i); mean /= x.getNumSamples();
            ParamFrame g = f; g[P::scrFbAmt] = 0.f;
            const bool differs = diff (x, screechOnly (g, 1.4)) > 1.0e-3;
            check (std::isfinite (rms (x)) && peak (x) < 6.f && std::abs (mean) < 0.01 && differs, "M06 FEEDBACK at 95 %: no runaway, no DC",
                   "peak " + String (peak (x), 2) + ", DC " + String (mean, 4));
        }
        // 9. PITCH ENVELOPE: the screech falls from START to END
        {
            ParamFrame f = q; f[P::scrMode] = 0.f; f[P::sf1On] = 0.f; f[P::so1Wave] = 0.f; f[P::scrPitchMode] = 2.f; f[P::scrBaseHz] = 200.f;
            f[P::scrPAmt] = 1.f; f[P::scrPStart] = 24.f; f[P::scrPEnd] = 0.f; f[P::scrPD] = 150.f; f[P::scrStart] = 100.f;
            const auto x = screechOnly (f);
            const double early = pitchAt (x, sr, 0.11, 0.02), late = pitchAt (x, sr, 0.45, 0.05);
            check (early > late * 2.0 && std::abs (late - 200.0) < 10.0, "M06 PITCH ENVELOPE: START -> END", String (early, 0) + " Hz -> " + String (late, 0) + " Hz");
        }
        // 10. SCREECH TIMING: BODY END follows the real body (200 / 400 / 700 ms), OFFSET, TAIL does not move it, MANUAL keeps START
        {
            auto onset = [&] (const AudioBuffer<float>& x) { for (int i = 0; i < x.getNumSamples(); ++i) if (std::abs (x.getSample (0, i)) > 1.0e-4f) return 1000.0 * i / sr; return -1.0; };
            auto withBody = [&] (float bodyMs, int mode, float offset, float tailLen = 568.f, float start = 180.f)
            {
                ParamFrame f = q; f[P::bodyLen] = bodyMs; f[P::scrStartMode] = (float) mode; f[P::scrOffset] = offset; f[P::tailLen] = tailLen; f[P::scrStart] = start;
                f[P::scrFadeIn] = 0.f; f[P::scrAtk] = 0.f;
                return onset (screechOnly (f, 1.2));
            };
            const double b2 = withBody (200.f, 1, 0.f), b4 = withBody (400.f, 1, 0.f), b7 = withBody (700.f, 1, 0.f), off = withBody (400.f, 2, 100.f);
            const double tail = withBody (400.f, 1, 0.f, 1200.f), man = withBody (700.f, 0, 0.f, 568.f, 250.f);
            const bool ok6 = std::abs (b2 - 200) < 2 && std::abs (b4 - 400) < 2 && std::abs (b7 - 700) < 2 && std::abs (off - 500) < 2 && std::abs (tail - b4) < 0.5 && std::abs (man - 250) < 2;
            check (ok6, "M06 SCREECH START MODE: BODY END / + OFFSET / TAIL / MANUAL",
                   String (b2, 1) + " " + String (b4, 1) + " " + String (b7, 1) + " ms, +100: " + String (off, 1) + ", tail 1200: " + String (tail, 1) + ", manual 250: " + String (man, 1));
        }
        // 11. routing: a filter on LAYER 1 only there; on BODY only the body; BEFORE / AFTER the distortion differ
        {
            ParamFrame f = q; for (int id : { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute }) f[id] = 1.f;
            f[P::layer1On] = 1.f; f[P::layer2On] = 1.f; f[P::layer2Pitch] = 19.f; f[P::layer1Src] = 1.f; f[P::layer2Src] = 1.f;
            ParamFrame d = f; d[P::flt1On] = 1.f; d[P::flt1Loc] = (float) m6::LLayer1; d[P::flt1Cut] = 300.f;
            ParamFrame o2 = f; o2[P::layer1On] = 0.f; ParamFrame o2d = d; o2d[P::layer1On] = 0.f;
            const bool layer = diff (renderKick (f, G1, sr, 0.4), renderKick (d, G1, sr, 0.4)) > 1.0e-3 && diff (renderKick (o2, G1, sr, 0.4), renderKick (o2d, G1, sr, 0.4)) < 1.0e-7;
            ParamFrame g = q; g[P::flt1On] = 1.f; g[P::flt1Loc] = (float) m6::LBody; g[P::flt1Cut] = 200.f;
            ParamFrame gm = g; gm[P::mixBodyMute] = 1.f; ParamFrame qm = q; qm[P::mixBodyMute] = 1.f;
            const bool group = diff (renderKick (g, G1, sr, 0.4), renderKick (q, G1, sr, 0.4)) > 1.0e-3 && diff (renderKick (gm, G1, sr, 0.4), renderKick (qm, G1, sr, 0.4)) < 1.0e-7;
            ParamFrame pre = q; pre[P::flt1On] = 1.f; pre[P::flt1Cut] = 400.f; pre[P::dist1On] = 1.f; pre[P::dist1Loc] = 3.f; pre[P::dist1Type] = 1.f; pre[P::dist1Drive] = 1.f;
            ParamFrame post = pre; post[P::flt1Pos] = 1.f;
            ParamFrame preNo = pre, postNo = post; preNo[P::dist1On] = postNo[P::dist1On] = 0.f;
            const bool dist = diff (renderKick (pre, G1, sr, 0.4), renderKick (post, G1, sr, 0.4)) > 1.0e-3 && diff (renderKick (preNo, G1, sr, 0.4), renderKick (postNo, G1, sr, 0.4)) < 1.0e-6;
            check (layer && group && dist, "M06 routing: per LAYER, per GROUP, BEFORE / AFTER the module 05 distortion",
                   String (layer ? "" : "layer! ") + (group ? "" : "group! ") + (dist ? "" : "dist!"));
        }
        // 12. presets / files restore everything; RANDOMIZE never touches a LOCKED engine; A / B
        {
            GeProcessor p;
            m06t::apply (p, m06t::presets()[4]);                                  // AGGRESSIVE METAL
            const auto before = p.frame();
            auto x = m06t::moduleToXml (p);
            m06t::reset (p, m06t::moduleIds());
            m06t::load (p, *x, 0, 0);
            bool same = true; for (auto& id : m06t::moduleIds()) same &= std::abs (p.frame()[idOf (id)] - before[idOf (id)]) < 1.0e-4f;
            std::array<bool, 3> lock { true, false, true };
            const auto pre = p.frame();
            m06t::randomize (p, lock, 7, 1.f);
            bool lockOk = true, moved = false;
            for (auto& id : m06t::engineIds (m06t::EFilter)) lockOk &= p.frame()[idOf (id)] == pre[idOf (id)];
            for (auto& id : m06t::engineIds (m06t::EScreech)) lockOk &= p.frame()[idOf (id)] == pre[idOf (id)];
            for (auto& id : m06t::engineIds (m06t::EResonance)) moved |= p.frame()[idOf (id)] != pre[idOf (id)];
            p.undo.undo();
            bool undone = true; for (auto& id : m06t::moduleIds()) undone &= std::abs (p.frame()[idOf (id)] - pre[idOf (id)]) < 1.0e-4f;
            m06t::Compare ab; ab.toggle (p); setP (p, "res1_freq", 5000.f); ab.toggle (p);
            const bool abOk = std::abs (p.apvts.getRawParameterValue ("res1_freq")->load() - pre[P::res1Freq]) < 0.5f;
            bool allSound = true; String e;
            for (auto& pr : m06t::presets())
            {
                GeProcessor pp; m06t::apply (pp, pr);
                const auto k = renderKick (pp.frame(), G1, sr, 0.6);
                const bool good = std::isfinite (rms (k)) && peak (k) < 4.f && diff (k, renderKick (base, G1, sr, 0.6)) > 1.0e-4;
                if (! good) e << pr.name << " "; allSound &= good;
            }
            check (same && lockOk && moved && undone && abOk && allSound, "M06 presets / files recall, RANDOMIZE respects LOCK (+ undo), A / B",
                   String (same ? "" : "recall! ") + (lockOk ? "" : "lock! ") + (moved ? "" : "no move! ") + (undone ? "" : "undo! ") + (abOk ? "" : "ab! ") + e);
        }
        // the owner's listening file: the MODERN kick, one bar (4 kicks at 150 BPM, 6 ms fade) per module 06 preset, then each screech mode
        {
            const int beat = (int) (0.4 * sr), fade = (int) (0.006 * sr);
            const int nPre = (int) m06t::presets().size(), total = 1 + nPre + 8;
            AudioBuffer<float> demo (2, beat * 4 * total); demo.clear();
            String order;
            for (int k = 0; k < total; ++k)
            {
                GeProcessor pp; pp.setCurrentProgram (1);
                String name = "(dry)";
                if (k >= 1 && k <= nPre) { m06t::apply (pp, m06t::presets()[(size_t) k - 1]); name = m06t::presets()[(size_t) k - 1].name; }
                if (k > nPre) { const int m = k - nPre - 1; setP (pp, "scr_on", 1.f); setP (pp, "scr_mode", (float) m); setP (pp, "scr_start_mode", 1.f);
                                name = "SCREECH " + Choices::scrModes()[m]; }
                auto x = renderKick (pp.frame(), G1, sr, 0.4, -1, 1.f, &pp.shapes());
                for (int i = 0; i < fade; ++i) for (int c = 0; c < 2; ++c) x.setSample (c, beat - fade + i, x.getSample (c, beat - fade + i) * (1.f - (float) i / (float) fade));
                for (int b = 0; b < 4; ++b) for (int c = 0; c < 2; ++c) demo.copyFrom (c, (k * 4 + b) * beat, x, c, 0, beat);
                order << String (k * 1.6, 1) << " s " << name << "\n";
            }
            demo.applyGain (0.7f / jmax (1.0e-6f, peak (demo)));
            writeWav (outDir.getChildFile ("4ge_m06_screech.wav"), demo);
            outDir.getChildFile ("4ge_m06_screech.txt").replaceWithText (order);
        }
        // 13. key / pitch tracking (identity on G1: checked on another note) + velocity
        {
            ParamFrame f = q; f[P::flt1On] = 1.f; f[P::flt1Cut] = 800.f; f[P::flt1Res] = 0.6f;
            ParamFrame t = f; t[P::flt1Key] = 2.f;
            ParamFrame r = q; r[P::res1On] = 1.f; ParamFrame rt = r; rt[P::res1Track] = 2.f;
            check (diff (renderKick (f, 43, sr, 0.3), renderKick (t, 43, sr, 0.3)) > 1.0e-3 && diff (renderKick (r, 43, sr, 0.3), renderKick (rt, 43, sr, 0.3)) > 1.0e-3,
                   "M06 KEY TRACK / PITCH TRACK follow the note");
        }
        // 14. offline render = realtime (filters everywhere, resonators, screech HYBRID with a layer filter) + CPU with everything on
        {
            GeProcessor p;
            m06t::apply (p, m06t::presets()[6]);                                  // FORMANT MACHINE
            setP (p, "res1_on", 1.f); setP (p, "res1_source", 6.f); setP (p, "res1_location", 0.f);
            setP (p, "layer1_on", 1.f); setP (p, "flt2_on", 1.f); setP (p, "flt2_location", 6.f); setP (p, "flt2_type", 7.f);
            setP (p, "flt3_on", 1.f); setP (p, "flt3_location", 14.f); setP (p, "flt3_type", 4.f);
            const auto rt = hitProc (p, { { 0, MidiMessage::noteOn (1, G1, (uint8) 127) } }, 0.6);
            const auto off = renderKick (p.frame(), G1, sr, 0.6, -1, 1.f, &p.shapes());
            GeProcessor full;
            for (int i = 0; i < 8; ++i) { setP (full, ("flt" + String (i + 1) + "_on").toRawUTF8(), 1.f); setP (full, ("flt" + String (i + 1) + "_type").toRawUTF8(), (float) (i + 2)); }
            setP (full, "res1_on", 1.f); setP (full, "res2_on", 1.f); setP (full, "res2_mode", 2.f); setP (full, "scr_on", 1.f); setP (full, "scr_mode", 7.f);
            for (auto* k : { "scr_mix_metal", "scr_mix_formant", "scr_mix_noise", "scr_mix_feedback" }) setP (full, k, 0.5f);
            for (int o = 2; o <= 4; ++o) setP (full, ("scr_osc" + String (o) + "_on").toRawUTF8(), 1.f);
            GeProcessor plain;
            const double c0 = cpuTime (plain, 2.0), c1 = cpuTime (full, 2.0);
            check (diff (rt, off) < 1.0e-6 && std::isfinite (rms (rt)) && c1 < 0.6, "M06 realtime = export, CPU with 8 filters + 2 resonators + full screech",
                   "rt vs export " + String (diff (rt, off), 8) + ", CPU " + String (100.0 * c0, 1) + " % -> " + String (100.0 * c1, 1) + " %");
        }
    }
    // ---------------------------------------------------------------- module 07: EQ, multiband & spectral shaping (MODULE07.md 13)
    {
        namespace tn = fge::dsp::tone;
        const double rate = 44100.0 * 4.0;
        ParamFrame q = base; q[P::clipDrive] = 0.f;
        const int es = P::eq2On - P::eq1On;
        // run the real module 07 state on a test signal at the engine rate (GLOBAL, AFTER)
        auto runTone = [&] (const ParamFrame& f, const std::function<void (int, float&, float&)>& gen, int n, int loc = 0, int pos = 1)
        {
            auto bank = std::make_unique<tn::Bank>(); tn::cookBank (f, *bank, rate);
            auto st = std::make_unique<tn::State>();
            AudioBuffer<float> out (2, n);
            for (int i = 0; i < n; ++i)
            {
                float l, r; gen (i, l, r);
                fge::dsp::m06::Ctx cx; cx.ms = (float) (i * 1000.0 / rate);
                st->process (*bank, loc, pos, l, r, cx, rate);
                out.setSample (0, i, l); out.setSample (1, i, r);
            }
            return out;
        };
        auto sine = [&] (double hz, float a) { return [=] (int i, float& l, float& r) { l = r = a * (float) std::sin (2.0 * MathConstants<double>::pi * hz * i / rate); }; };
        auto levelAt = [&] (const AudioBuffer<float>& x, double hz, int ch = 0, int from = 0)
        {
            const int order = 15, n = 1 << order; juce::dsp::FFT fft (order); std::vector<float> buf ((size_t) n * 2, 0.f);
            for (int i = 0; i < n && from + i < x.getNumSamples(); ++i) buf[(size_t) i] = x.getSample (ch, from + i) * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) n));
            fft.performFrequencyOnlyForwardTransform (buf.data());
            const int k = (int) std::round (hz * n / rate); float m = 0.f; for (int j = k - 3; j <= k + 3; ++j) m = jmax (m, buf[(size_t) jlimit (0, n / 2 - 1, j)]);
            return (double) Decibels::gainToDecibels (m * 4.f / (float) n, -160.f);
        };
        auto impulseDb = [&] (const ParamFrame& f, std::initializer_list<double> hzs, int loc = 0)
        {
            auto y = runTone (f, [] (int i, float& l, float& r) { l = r = i == 0 ? 1.f : 0.f; }, 1 << 16, loc);
            const int order = 16, n = 1 << order; juce::dsp::FFT fft (order); std::vector<float> buf ((size_t) n * 2, 0.f);
            for (int i = 0; i < n; ++i) buf[(size_t) i] = y.getSample (0, i);
            fft.performFrequencyOnlyForwardTransform (buf.data());
            std::vector<double> o; for (double hz : hzs) o.push_back (Decibels::gainToDecibels (buf[(size_t) jlimit (1, n / 2 - 1, (int) std::round (hz * n / rate))], -160.f));
            return o;
        };
        // 1 + 2. 16 independent bands, each type its own response
        {
            ParamFrame f = q;
            for (int i = 0; i < 16; ++i) { f[P::eq1On + i * es] = 1.f; f[P::eq1Freq + i * es] = 40.f * std::pow (2.f, (float) i * 0.58f); f[P::eq1Gain + i * es] = (i % 2 ? 6.f : -6.f); f[P::eq1Q + i * es] = 2.f; }
            const auto all = renderKick (f, G1, sr, 0.4);
            bool each = true;
            for (int i = 0; i < 16; ++i) { ParamFrame g = f; g[P::eq1On + i * es] = 0.f; each &= diff (all, renderKick (g, G1, sr, 0.4)) > 1.0e-4; }
            // types: bell +12 at 1 kHz = +12; low cut 24 dB at 200: -24 dB at 50 Hz; high shelf +6 at 4k: +6 at 16k; notch at 1k: deep; tilt +12: lows down, highs up
            auto one = [&] (int type, float hz, float gain, float qv, int slope) { ParamFrame g = q; g[P::eq1On] = 1.f; g[P::eq1Type] = (float) type; g[P::eq1Freq] = hz; g[P::eq1Gain] = gain; g[P::eq1Q] = qv; g[P::eq1Slope] = (float) slope; return g; };
            const auto bell = impulseDb (one (tn::Bell, 1000, 12, 1, 1), { 1000, 100 });
            const auto cut = impulseDb (one (tn::LowCut, 200, 0, 0.707f, 2), { 50, 2000 });
            const auto shelf = impulseDb (one (tn::HighShelf, 4000, 6, 0.707f, 1), { 16000, 200 });
            const auto notch = impulseDb (one (tn::Notch, 1000, 0, 2, 1), { 1000, 3000 });
            const auto tilt = impulseDb (one (tn::Tilt, 1000, 12, 1, 1), { 40, 15000 });
            const bool types = std::abs (bell[0] - 12) < 0.3 && std::abs (bell[1]) < 0.5 && cut[0] < -22 && std::abs (cut[1]) < 0.3 && std::abs (shelf[0] - 6) < 0.5
                            && std::abs (shelf[1]) < 0.3 && notch[0] < -30 && tilt[0] < -4.5 && tilt[1] > 4.5;
            check (each && types, "M07 16 EQ bands independent, every type its real response",
                   "bell " + String (bell[0], 1) + " cut@50 " + String (cut[0], 1) + " shelf " + String (shelf[0], 1) + " notch " + String (notch[0], 1) + " tilt " + String (tilt[0], 1) + " / " + String (tilt[1], 1));
        }
        // 3. dynamic EQ reacts to the real level: loud 2 kHz is cut, quiet 2 kHz is not
        {
            ParamFrame f = q; f[P::deq1On] = 1.f; f[P::deq1Freq] = 2000.f; f[P::deq1Q] = 1.f; f[P::deq1Thr] = -24.f; f[P::deq1Range] = 12.f; f[P::deq1Ratio] = 20.f; f[P::deq1Knee] = 0.f;
            const double refL = levelAt (runTone (q, sine (2000, 0.5f), 60000), 2000, 0, 20000), refQ = levelAt (runTone (q, sine (2000, 0.01f), 60000), 2000, 0, 20000);
            const double loud = levelAt (runTone (f, sine (2000, 0.5f), 60000), 2000, 0, 20000) - refL;
            const double quiet = levelAt (runTone (f, sine (2000, 0.01f), 60000), 2000, 0, 20000) - refQ;
            ParamFrame up = f; up[P::deq1Mode] = 2.f;                              // UPWARD: quiet is lifted
            const double qUp = levelAt (runTone (up, sine (2000, 0.01f), 60000), 2000, 0, 20000) - refQ;
            check (loud < -9 && std::abs (quiet) < 1 && qUp > 9, "M07 DYNAMIC EQ follows the real level (cut loud / lift quiet)",
                   "loud " + String (loud, 1) + " dB, quiet " + String (quiet, 1) + " dB, upward quiet " + String (qUp, 1) + " dB");
        }
        // 4 + 5. multiband: the bands sum flat (LR 12 / 24 / 48), SOLO plays one band
        {
            String e; double worst = 0;
            for (int xt : { 0, 1, 2 })
            {
                ParamFrame f = q; f[P::mbOn] = 1.f; f[P::mbXType] = (float) xt;
                for (double v : impulseDb (f, { 30, 80, 150, 250, 600, 2500, 5000, 12000 })) worst = jmax (worst, std::abs (v));
            }
            ParamFrame s = q; s[P::mbOn] = 1.f; s[P::mb2Solo] = 1.f;
            const auto so = impulseDb (s, { 140, 6000, 30 });
            check (worst < 0.1 && so[0] > -2.5 && so[1] < -30 && so[2] < -12, "M07 MULTIBAND: 4 bands sum flat (LR12/24/48), SOLO isolates a band",
                   "sum dev " + String (worst, 3) + " dB, solo LOW: 140 Hz " + String (so[0], 1) + ", 6 kHz " + String (so[1], 1) + " dB");
        }
        // 6. multiband dynamics on a real envelope: a loud low band is compressed, the transient mode lifts the attack
        {
            ParamFrame f = q; f[P::mbOn] = 1.f; f[P::mb2Dyn] = (float) tn::MdComp; f[P::mb2Thr] = -30.f; f[P::mb2Ratio] = 10.f;
            const double c = levelAt (runTone (f, sine (150, 0.5f), 60000), 150, 0, 20000) - Decibels::gainToDecibels (0.5f);
            ParamFrame t = q; t[P::mbOn] = 1.f; t[P::mb4Dyn] = (float) tn::MdTransient; t[P::mb4TrA] = 12.f;
            const auto plain = renderKick (q, G1, sr, 0.3), tr = renderKick ([&] { ParamFrame g = q; g[P::mbOn] = 1.f; g[P::mb4Dyn] = (float) tn::MdTransient; g[P::mb4TrA] = 12.f; return g; }(), G1, sr, 0.3);
            const float atk0 = peak (plain, 0, (int) (0.01 * sr)), atk1 = peak (tr, 0, (int) (0.01 * sr));
            check (c < -10 && atk1 > atk0 * 1.15f, "M07 MULTIBAND DYNAMICS: compressor on the real band level, TRANSIENT lifts the attack",
                   "comp " + String (c, 1) + " dB, attack peak " + String (atk0, 2) + " -> " + String (atk1, 2));
        }
        // 7. harmonic shaper makes real harmonics (even -> 2nd, odd -> 3rd)
        {
            ParamFrame f = q; f[P::spOn] = 1.f; f[P::spHOn] = 1.f; f[P::spHMix] = 1.f; f[P::spHDrive] = 0.6f; f[P::spHLow] = 1.f; f[P::spHHigh] = 1.f;
            ParamFrame ev = f; ev[P::spHEven] = 1.f; ev[P::spHOdd] = 0.f; ParamFrame od = f; od[P::spHEven] = 0.f; od[P::spHOdd] = 1.f;
            const auto a = runTone (ev, sine (300, 0.5f), 60000), b = runTone (od, sine (300, 0.5f), 60000);
            const double e2 = levelAt (a, 600, 0, 20000) - levelAt (a, 900, 0, 20000), o3 = levelAt (b, 900, 0, 20000) - levelAt (b, 600, 0, 20000);
            check (e2 > 15 && o3 > 15, "M07 HARMONIC SHAPER: EVEN -> 2nd harmonic, ODD -> 3rd", "even: h2-h3 " + String (e2, 1) + " dB, odd: h3-h2 " + String (o3, 1) + " dB");
        }
        // 8. exciter: new content only in its range (a 3 kHz sine gets 6 / 9 kHz; 0 excitation = nothing)
        {
            ParamFrame f = q; f[P::spOn] = 1.f; f[P::spXOn] = 1.f; f[P::spXAmt] = 1.f; f[P::spXLo] = 2000.f; f[P::spXHi] = 16000.f; f[P::spXHarm] = 2.f;
            const auto a = runTone (f, sine (3000, 0.3f), 60000);
            ParamFrame z = f; z[P::spXAmt] = 0.f;
            const auto b = runTone (z, sine (3000, 0.3f), 60000);
            const double newC = jmax (levelAt (a, 6000, 0, 20000), levelAt (a, 9000, 0, 20000)) - levelAt (a, 3000, 0, 20000);
            const double none = jmax (levelAt (b, 6000, 0, 20000), levelAt (b, 9000, 0, 20000)) - levelAt (b, 3000, 0, 20000);
            check (newC > -40 && none < -90, "M07 EXCITER generates new harmonics (not a shelf)", "new " + String (newC, 1) + " dB, off " + String (none, 1) + " dB");
        }
        // 9. spectral compression pulls down what sticks out (loud 1 kHz vs soft 4 kHz)
        {
            ParamFrame f = q; f[P::spOn] = 1.f; f[P::spCOn] = 1.f; f[P::spCThr] = 3.f; f[P::spCRed] = 18.f; f[P::spCSens] = 1.f; f[P::spCLo] = 200.f; f[P::spCHi] = 12000.f;
            auto two = [&] (int i, float& l, float& r) { l = r = 0.5f * (float) std::sin (2.0 * MathConstants<double>::pi * 1000.0 * i / rate) + 0.05f * (float) std::sin (2.0 * MathConstants<double>::pi * 4000.0 * i / rate); };
            const auto y = runTone (f, two, 80000);
            const double before = Decibels::gainToDecibels (0.5f) - Decibels::gainToDecibels (0.05f);
            const double after = levelAt (y, 1000, 0, 40000) - levelAt (y, 4000, 0, 40000);
            check (after < before - 3.0, "M07 SPECTRAL COMPRESSION: the loud peak comes down, the rest stays", "1k vs 4k " + String (before, 1) + " -> " + String (after, 1) + " dB");
        }
        // 10. mid / side: a MID band leaves a pure side signal alone, a SIDE band changes it
        {
            ParamFrame f = q; f[P::eq1On] = 1.f; f[P::eq1Freq] = 1000.f; f[P::eq1Gain] = 12.f; f[P::eq1Stereo] = (float) tn::EsMid;
            auto side = [&] (int i, float& l, float& r) { l = 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * 1000.0 * i / rate); r = -l; };
            const double m = levelAt (runTone (f, side, 60000), 1000, 0, 20000);
            ParamFrame g = f; g[P::eq1Stereo] = (float) tn::EsSide;
            const double sd = levelAt (runTone (g, side, 60000), 1000, 0, 20000);
            const double ref = levelAt (runTone (q, side, 60000), 1000, 0, 20000);
            check (std::abs (m - ref) < 0.3 && sd - ref > 11, "M07 MID / SIDE: MID band ignores the side, SIDE band changes it", "mid " + String (m - ref, 2) + " dB, side +" + String (sd - ref, 1) + " dB");
        }
        // 11. mono compatibility: MONO BELOW takes the side out of the lows, WIDTH 200 % never widens the sub
        {
            ParamFrame f = q; f[P::stwOn] = 1.f; f[P::stwMono] = 150.f; f[P::stwWidth] = 2.f;
            auto sig = [&] (int i, float& l, float& r)
            {
                const float s60 = 0.3f * (float) std::sin (2.0 * MathConstants<double>::pi * 60.0 * i / rate), s3k = 0.1f * (float) std::sin (2.0 * MathConstants<double>::pi * 3000.0 * i / rate);
                l = s60 + s3k; r = -s60 - s3k;                                        // both only in the side
            };
            auto y = runTone (f, sig, 80000);
            AudioBuffer<float> sideOut (1, y.getNumSamples());
            for (int i = 0; i < y.getNumSamples(); ++i) sideOut.setSample (0, i, 0.5f * (y.getSample (0, i) - y.getSample (1, i)));
            const double lo = levelAt (sideOut, 60, 0, 30000) - Decibels::gainToDecibels (0.3f), hi = levelAt (sideOut, 3000, 0, 30000) - Decibels::gainToDecibels (0.1f);
            check (lo < -20 && hi > 5, "M07 MONO COMPATIBILITY: no side below MONO BELOW, WIDTH only widens above", "side 60 Hz " + String (lo, 1) + " dB, side 3 kHz " + String (hi, 1) + " dB");
        }
        // 12. spectral freeze: after the capture the frozen texture plays on (decaying), before it the signal is untouched
        {
            ParamFrame f = q; f[P::spOn] = 1.f; f[P::spFOn] = 1.f; f[P::spFMix] = 1.f; f[P::spFCap] = 80.f; f[P::spFDecay] = 2000.f; f[P::spCLo] = 100.f; f[P::spCHi] = 10000.f;
            const auto dry = renderKick (q, G1, sr, 0.6), frz = renderKick (f, G1, sr, 0.6);
            AudioBuffer<float> a (2, (int) (0.07 * sr)), b (2, (int) (0.07 * sr));
            for (int c = 0; c < 2; ++c) { a.copyFrom (c, 0, dry, c, 0, a.getNumSamples()); b.copyFrom (c, 0, frz, c, 0, b.getNumSamples()); }
            const double early = diff (a, b);
            const double late = rms (frz, (int) (0.3 * sr), (int) (0.2 * sr));
            check (early < 1.0e-3 && late > 1.0e-3 && diff (dry, frz) > 1.0e-2, "M07 SPECTRAL FREEZE: captures at the set time and holds the texture", "after capture rms " + String (late, 4));
        }
        // 13. per layer / per group / before - after the distortion
        {
            ParamFrame f = q; for (int id : { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute }) f[id] = 1.f;
            f[P::layer1On] = 1.f; f[P::layer2On] = 1.f; f[P::layer2Pitch] = 19.f;
            ParamFrame d = f; d[P::eq1On] = 1.f; d[P::eq1Loc] = (float) fge::dsp::m06::LLayer1; d[P::eq1Gain] = 12.f; d[P::eq1Freq] = 300.f;
            ParamFrame only2 = f; only2[P::layer1On] = 0.f; ParamFrame only2d = d; only2d[P::layer1On] = 0.f;
            const bool layerOk = diff (renderKick (f, G1, sr, 0.4), renderKick (d, G1, sr, 0.4)) > 1.0e-3 && diff (renderKick (only2, G1, sr, 0.4), renderKick (only2d, G1, sr, 0.4)) < 1.0e-7;
            ParamFrame g = q; g[P::eq1On] = 1.f; g[P::eq1Loc] = 2.f; g[P::eq1Gain] = 12.f; g[P::eq1Freq] = 500.f;
            ParamFrame gm = g; gm[P::mixBodyMute] = 1.f; ParamFrame qm = q; qm[P::mixBodyMute] = 1.f;
            const bool groupOk = diff (renderKick (g, G1, sr, 0.4), renderKick (q, G1, sr, 0.4)) > 1.0e-3 && diff (renderKick (gm, G1, sr, 0.4), renderKick (qm, G1, sr, 0.4)) < 1.0e-7;
            ParamFrame pre = q; pre[P::dist1On] = 1.f; pre[P::dist1Type] = 1.f; pre[P::dist1Drive] = 0.9f; pre[P::eq1On] = 1.f; pre[P::eq1Gain] = 12.f; pre[P::eq1Freq] = 800.f; pre[P::eq1Pos] = 0.f;
            ParamFrame post = pre; post[P::eq1Pos] = 1.f;
            const bool posOk = diff (renderKick (pre, G1, sr, 0.4), renderKick (post, G1, sr, 0.4)) > 1.0e-3;
            check (layerOk && groupOk && posOk, "M07 routing: per LAYER, per GROUP, BEFORE / AFTER the distortion");
        }
        // 14. automation: sweeping a band's gain / frequency during a kick does not click
        {
            auto run = [&] (bool sw)
            {
                GeProcessor p; setP (p, "eq1_on", 1.f); setP (p, "eq1_freq", 500.f); setP (p, "eq1_gain", 0.f);
                p.prepareToPlay (sr, 128);
                float maxStep = 0.f, last = 0.f, pk = 0.f;
                for (int blk = 0; blk < 120; ++blk)
                {
                    AudioBuffer<float> bb (2, 128); bb.clear(); MidiBuffer m;
                    if (blk == 0) m.addEvent (MidiMessage::noteOn (1, G1, 1.f), 0);
                    if (sw) { setP (p, "eq1_gain", 12.f * (float) std::sin (blk * 0.4)); setP (p, "eq1_freq", 300.f + 3000.f * (float) (blk % 10) / 10.f); }
                    p.processBlock (bb, m);
                    for (int i = 0; i < 128; ++i) { const float v = bb.getSample (0, i); if (blk > 4) maxStep = jmax (maxStep, std::abs (v - last)); last = v; pk = jmax (pk, std::abs (v)); }
                }
                return maxStep / jmax (1.0e-6f, pk);
            };
            const float a = run (false), b = run (true);
            check (b < a * 1.6f + 0.02f, "M07 automation of gain / frequency is smooth (no clicks)", "max step " + String (b, 3) + " vs steady " + String (a, 3));
        }
        // 15 + 16. presets / files / randomize with LOCK + realtime = export + CPU with everything on
        {
            GeProcessor p;
            m07t::apply (p, m07t::presets()[2]);
            const auto before = p.frame();
            auto x = m07t::moduleToXml (p);
            m06t::reset (p, m07t::moduleIds());
            m07t::load (p, *x, 0);
            bool same = true; for (int i = P::eq1On; i < P::count; ++i) same &= std::abs (p.frame()[i] - before[i]) < 1.0e-4f;
            std::array<bool, m07t::NumEngines> lk {}; lk[m07t::EEq] = true;
            const float g0 = p.apvts.getRawParameterValue ("eq2_gain")->load(), t0 = p.apvts.getRawParameterValue ("deq1_threshold")->load();
            m07t::randomize (p, lk, 7, 1.f);
            const bool lockOk = p.apvts.getRawParameterValue ("eq2_gain")->load() == g0 && p.apvts.getRawParameterValue ("deq1_threshold")->load() != t0;
            p.undo.undo();
            const bool undone = p.apvts.getRawParameterValue ("deq1_threshold")->load() == t0;
            bool allSound = true; String e;
            for (auto& pr : m07t::presets())
            {
                GeProcessor pp; m07t::apply (pp, pr);
                const auto k = renderKick (pp.frame(), G1, sr, 0.4);
                const bool good = std::isfinite (rms (k)) && peak (k) < 6.f && diff (k, renderKick (base, G1, sr, 0.4)) > 1.0e-4;
                if (! good) e << pr.name << " ";
                allSound &= good;
            }
            // everything on: realtime = export
            GeProcessor full;
            for (int i = 0; i < 16; ++i) { setP (full, ("eq" + String (i + 1) + "_on").toRawUTF8(), 1.f); setP (full, ("eq" + String (i + 1) + "_gain").toRawUTF8(), i % 2 ? 2.f : -2.f); }
            for (int i = 0; i < 4; ++i) setP (full, ("deq" + String (i + 1) + "_on").toRawUTF8(), 1.f);
            for (auto* id : { "mb_on", "sp_on", "sp_harm_on", "sp_exc_on", "sp_comp_on", "sp_freeze", "stw_on" }) setP (full, id, 1.f);
            setP (full, "mb2_dynamics", 1.f); setP (full, "mb4_dynamics", 5.f); setP (full, "sp_tilt", 0.3f);
            const auto rt = hitProc (full, { { 0, MidiMessage::noteOn (1, G1, (uint8) 127) } }, 0.5);
            const auto off = renderKick (full.frame(), G1, sr, 0.5, -1, 1.f, &full.shapes(), 0.0, -1.0, nullptr, nullptr, &full.distData());
            GeProcessor plain; const double c0 = cpuTime (plain, 1.0), c1 = cpuTime (full, 1.0);
            check (same && lockOk && undone && allSound && diff (rt, off) < 1.0e-6 && std::isfinite (rms (rt)) && c1 < 12.0 * c0,
                   "M07 presets / files recall, RANDOMIZE + LOCK (+ undo), realtime = export, CPU with everything on",
                   e + "rt vs export " + String (diff (rt, off), 8) + ", CPU " + String (100.0 * c0, 1) + " % -> " + String (100.0 * c1, 1) + " %"
                   + (same ? "" : " recall!") + (lockOk ? "" : " lock!") + (undone ? "" : " undo!"));
        }
        // the owner's listening file: the MODERN kick, one bar per module 07 preset
        {
            const int beat = (int) (0.4 * sr), fade = (int) (0.006 * sr);
            const auto& pl = m07t::presets();
            AudioBuffer<float> demo (2, beat * 4 * (int) (pl.size() + 1)); demo.clear();
            String order;
            for (int k = 0; k <= (int) pl.size(); ++k)
            {
                GeProcessor pp; pp.setCurrentProgram (1);
                if (k > 0) for (auto& [id, v] : pl[(size_t) k - 1].v) setP (pp, id, v);
                auto x = renderKick (pp.frame(), G1, sr, 0.4, -1, 1.f, &pp.shapes(), 0.0, -1.0, nullptr, nullptr, &pp.distData());
                for (int i = 0; i < fade; ++i) for (int c = 0; c < 2; ++c) x.setSample (c, beat - fade + i, x.getSample (c, beat - fade + i) * (1.f - (float) i / (float) fade));
                for (int b = 0; b < 4; ++b) for (int c = 0; c < 2; ++c) demo.copyFrom (c, (k * 4 + b) * beat, x, c, 0, beat);
                order << String (k * 1.6, 1) << " s " << (k == 0 ? String ("(dry)") : String (pl[(size_t) k - 1].name)) << "\n";
            }
            demo.applyGain (0.7f / jmax (1.0e-6f, peak (demo)));
            writeWav (outDir.getChildFile ("4ge_m07_eq_presets.wav"), demo);
            outDir.getChildFile ("4ge_m07_eq_presets.txt").replaceWithText (order);
        }
    }
    // ---------------------------------------------------------------- module 08: dynamics, compressor, transient & limiter (MODULE08.md 14)
    {
        namespace dy = fge::dsp::dyn;
        const double rate = 44100.0 * 4.0;
        ParamFrame q = base; q[P::clipDrive] = 0.f;
        auto runDyn = [&] (const ParamFrame& f, const std::function<void (int, float&, float&)>& gen, int n, int loc = 0, int pos = 1, const float* sc = nullptr)
        {
            auto bank = std::make_unique<dy::Bank>(); dy::cookBank (f, nullptr, *bank, rate, rate);
            auto st = std::make_unique<dy::State>();
            AudioBuffer<float> out (2, n);
            for (int i = 0; i < n; ++i)
            {
                float l, r; gen (i, l, r);
                fge::dsp::m06::Ctx cx; cx.ms = (float) (i * 1000.0 / rate);
                st->process (*bank, loc, pos, l, r, cx, sc, rate);
                out.setSample (0, i, l); out.setSample (1, i, r);
            }
            return out;
        };
        auto sine = [&] (double hz, float a) { return [=] (int i, float& l, float& r) { l = r = a * (float) std::sin (2.0 * MathConstants<double>::pi * hz * i / rate); }; };
        auto pkDb = [] (const AudioBuffer<float>& x, int from, int n) { float m = 0; for (int i = from; i < from + n && i < x.getNumSamples(); ++i) m = jmax (m, std::abs (x.getSample (0, i))); return (double) Decibels::gainToDecibels (m, -160.f); };
        auto cmp = [&] (int type, float thr, float ratio, float atk = 0.05f, float rel = 50.f) { ParamFrame f = q; f[P::cmp1On] = 1.f; f[P::cmp1Type] = (float) type; f[P::cmp1Thr] = thr; f[P::cmp1Ratio] = ratio; f[P::cmp1Knee] = 0.f; f[P::cmp1Atk] = atk; f[P::cmp1Rel] = rel; return f; };
        {
            float e1 = 0.f, e2 = 0.f;
            for (int k = 0; k < 20000; ++k) { const float gv = std::pow (10.f, -6.f + 7.f * (float) k / 20000.f); e1 = jmax (e1, std::abs (dy::fastDb (gv) - Decibels::gainToDecibels (gv, -200.f)));
                                             const float d = -100.f + 130.f * (float) k / 20000.f; e2 = jmax (e2, std::abs (dy::fromDb (d) / Decibels::decibelsToGain (d, -200.f) - 1.f)); }
            check (e1 < 0.005f && e2 < 1.0e-4f, "M08 fast dB / gain math is exact enough", "max error " + String (e1, 5) + " dB, " + String (e2 * 100.f, 4) + " %");
        }
        // 1. ratio: -6 dBFS through thr -30: ratio 2 -> -18, ratio 10 -> -27.6
        {
            const double r2 = pkDb (runDyn (cmp (dy::VCA, -30, 2), sine (1000, 0.5f), 60000), 40000, 10000), r10 = pkDb (runDyn (cmp (dy::VCA, -30, 10), sine (1000, 0.5f), 60000), 40000, 10000);
            check (std::abs (r2 + 18.0) < 1.0 && std::abs (r10 + 27.6) < 1.0, "M08 COMPRESSOR ratio is the real ratio", "2:1 " + String (r2, 1) + " dB (-18), 10:1 " + String (r10, 1) + " dB (-27.6)");
        }
        // 2. attack / release are measurable (time to half the reduction)
        {
            auto burst = [&] (int i, float& l, float& r) { const float a = i < 20000 ? 0.01f : i < 60000 ? 0.5f : 0.01f; l = r = a * (float) std::sin (2.0 * MathConstants<double>::pi * 1000.0 * i / rate); };
            auto halfTime = [&] (float atk, float rel, bool release)
            {
                const auto y = runDyn (cmp (dy::VCA, -30, 10, atk, rel), burst, 300000);
                // envelope of the output, in windows of 1 ms
                const int w = (int) (0.001 * rate); std::vector<float> e;
                for (int i = 0; i + w < y.getNumSamples(); i += w) { float m = 0; for (int k = 0; k < w; ++k) m = jmax (m, std::abs (y.getSample (0, i + k))); e.push_back (m); }
                const int s0 = (release ? 60000 : 20000) / w;
                const float startV = e[(size_t) s0 + 1], endV = e[(size_t) jmin ((int) e.size() - 1, s0 + (release ? 700 : 35))];
                for (int k = s0 + 1; k < (int) e.size(); ++k) if (std::abs (e[(size_t) k] - endV) < 0.5f * std::abs (startV - endV)) return (k - s0) * 1.0;
                return 1.0e9;
            };
            const double a1 = halfTime (1.f, 50.f, false), a20 = halfTime (20.f, 50.f, false), r20 = halfTime (1.f, 20.f, true), r200 = halfTime (1.f, 200.f, true);
            check (a20 > a1 * 3 && r200 > r20 * 3, "M08 ATTACK / RELEASE change the real reaction", "attack 1 / 20 ms: " + String (a1, 0) + " / " + String (a20, 0) + " ms, release 20 / 200 ms: " + String (r20, 0) + " / " + String (r200, 0) + " ms");
        }
        // 3. eight compressor types, eight behaviours (on the kick)
        {
            std::vector<AudioBuffer<float>> o;
            for (int t = 0; t < 8; ++t) { ParamFrame f = cmp (t, -30, 6, 5.f, 120.f); f[P::cmp1Knee] = 6.f; f[P::cmp1Dry] = -6.f; f[P::cmp1ScSrc] = 1.f; o.push_back (renderKick (f, G1, sr, 0.5)); }
            double minD = 1.0e9; int ma = 0, mb = 0;
            for (int i = 0; i < 8; ++i) for (int j = i + 1; j < 8; ++j) { const double d = diff (o[(size_t) i], o[(size_t) j]); if (d < minD) { minD = d; ma = i; mb = j; } }
            check (minD > 1.0e-3, "M08 8 compressor types behave differently", "closest " + Choices::cmpTypes()[ma] + " / " + Choices::cmpTypes()[mb] + " " + String (minD, 4));
        }
        // 4. sidechain: a TAIL compressor keyed by the PUNCH ducks the tail only while the punch plays
        {
            ParamFrame self = cmp (dy::VCA, -30, 10, 0.1f, 30.f); self[P::cmp1Loc] = 3.f;
            ParamFrame key = self; key[P::cmp1ScSrc] = 1.f; key[P::cmp1ScMix] = 1.f;
            ParamFrame solo = q; solo[P::mixTailSolo] = 1.f; ParamFrame selfS = self; selfS[P::mixTailSolo] = 1.f; ParamFrame keyS = key; keyS[P::mixTailSolo] = 1.f;
            const auto plain = renderKick (solo, G1, sr, 0.5), a = renderKick (selfS, G1, sr, 0.5), k = renderKick (keyS, G1, sr, 0.5);
            const double early = rms (k, 0, (int) (0.012 * sr)) / (rms (plain, 0, (int) (0.012 * sr)) + 1e-12), late = rms (k, (int) (0.25 * sr), (int) (0.1 * sr)) / (rms (plain, (int) (0.25 * sr), (int) (0.1 * sr)) + 1e-12);
            check (diff (a, k) > 1.0e-3 && early < 0.7 && late > 0.85, "M08 SIDECHAIN: the detector hears the chosen source (PUNCH ducks the TAIL)",
                   "tail level during punch " + String (early, 2) + "x, later " + String (late, 2) + "x");
        }
        // 5. parallel: dry + wet at 0 dB do not double the level
        {
            ParamFrame f = cmp (dy::Parallel, 0, 1); f[P::cmp1Dry] = 0.f; f[P::cmp1Wet] = 0.f;
            const double lv = pkDb (runDyn (f, sine (1000, 0.5f), 40000), 20000, 10000) - Decibels::gainToDecibels (0.5f);
            ParamFrame c = cmp (dy::Parallel, -30, 10); c[P::cmp1Dry] = 0.f; c[P::cmp1Wet] = 0.f;
            const double lc = pkDb (runDyn (c, sine (1000, 0.5f), 40000), 20000, 10000) - Decibels::gainToDecibels (0.5f);
            check (std::abs (lv) < 0.1 && lc < 0.f && lc > -12.f, "M08 PARALLEL compression mixes without doubling the level", "uncompressed " + String (lv, 2) + " dB, compressed blend " + String (lc, 1) + " dB");
        }
        // 6. multiband band link (module 07 engine): linked bands share the detector
        {
            ParamFrame f = q; f[P::mbOn] = 1.f;
            for (int k = 0; k < 4; ++k) { f[P::mb1Dyn + k * (P::mb2Gain - P::mb1Gain)] = 1.f; f[P::mb1Thr + k * (P::mb2Gain - P::mb1Gain)] = -30.f; f[P::mb1Ratio + k * (P::mb2Gain - P::mb1Gain)] = 10.f; }
            auto two = [&] (int i, float& l, float& r) { l = r = 0.5f * (float) std::sin (2.0 * MathConstants<double>::pi * 50.0 * i / rate) + 0.01f * (float) std::sin (2.0 * MathConstants<double>::pi * 6000.0 * i / rate); };
            auto hi = [&] (const AudioBuffer<float>& y) { AudioBuffer<float> h (1, 40000); fge::dsp::tone::Sec a, b; const auto c = fge::dsp::tone::coef (fge::dsp::tone::SHp, 3000, 0.7071, 0, rate);
                                                          for (int i = 0; i < 40000; ++i) h.setSample (0, i, (float) b.tick (c, a.tick (c, y.getSample (0, 40000 + i)))); return rms (h, 10000, 30000); };
            auto runTone = [&] (const ParamFrame& pf)
            {
                auto bank = std::make_unique<fge::dsp::tone::Bank>(); fge::dsp::tone::cookBank (pf, *bank, rate);
                auto st = std::make_unique<fge::dsp::tone::State>(); AudioBuffer<float> o (2, 80000);
                for (int i = 0; i < 80000; ++i) { float l, r; two (i, l, r); fge::dsp::m06::Ctx cx; cx.ms = (float) (i * 1000.0 / rate); st->process (*bank, 0, 1, l, r, cx, rate); o.setSample (0, i, l); o.setSample (1, i, r); }
                return o;
            };
            const auto free = runTone (f);
            ParamFrame g = f; g[P::mbLink] = 4.f;
            const auto linked = runTone (g);
            const double d = 20.0 * std::log10 (hi (linked) / (hi (free) + 1e-12));
            check (d < -6.0, "M08 MULTIBAND band link: a loud sub pulls the linked high band down too", "high band " + String (d, 1) + " dB with ALL BANDS linked");
        }
        // 7. transient shaper: attack and sustain separately (onset / body ratio)
        {
            auto ratio = [&] (const ParamFrame& pf) { auto x = renderKick (pf, G1, sr, 0.5); return 20.0 * std::log10 (peak (x, 0, (int) (0.006 * sr)) / (rms (x, (int) (0.15 * sr), (int) (0.1 * sr)) + 1.0e-9)); };
            ParamFrame f = q; f[P::ts1On] = 1.f; f[P::ts1Loc] = 0.f; f[P::ts1Mode] = 1.f;
            ParamFrame atk = f; atk[P::ts1Atk] = 1.f; atk[P::ts1Mode] = 0.f;
            ParamFrame sus = f; sus[P::ts1Sus] = 1.f;
            const double r0 = ratio (q), ra = ratio (atk), rs = ratio (sus);
            check (ra > r0 + 2.0 && rs < r0 - 2.0, "M08 TRANSIENT SHAPER: ATTACK and SUSTAIN separately (real envelope)", "onset / body " + String (r0, 1) + " -> attack+ " + String (ra, 1) + ", sustain+ " + String (rs, 1) + " dB");
        }
        // 8. clipper: every type keeps the ceiling, 7 different shapes
        {
            std::vector<AudioBuffer<float>> o; bool ceil = true; String e;
            for (int t = 0; t < 7; ++t)
            {
                ParamFrame f = q; f[P::clp1On] = 1.f; f[P::clp1Type] = (float) t; f[P::clp1Drive] = 1.f; f[P::clp1Ceil] = -6.f; f[P::clp1Sym] = 0.3f;
                auto x = renderKick (f, G1, sr, 0.4);
                o.push_back (x);
                // the clipper's own output = the kick at its GLOBAL place; check the curve directly too
                fge::dsp::dyn::ClpSettings cs; cs.type = t; cs.drive = 1.f; cs.ceil = Decibels::decibelsToGain (-6.f); cs.sym = 0.3f; static const fge::dsp::dist::CurveData cv = fge::dsp::dist::CurveData::preset (1); cs.curve = &cv;
                float mx = 0; for (int k = -400; k <= 400; ++k) mx = jmax (mx, std::abs (fge::dsp::dyn::clipCurve (cs, (float) k / 100.f)));
                ceil &= mx <= cs.ceil * 1.0001f; e << String (Decibels::gainToDecibels (mx), 1) << " ";
            }
            double minD = 1.0e9; for (int i = 0; i < 7; ++i) for (int j = i + 1; j < 7; ++j) minD = jmin (minD, diff (o[(size_t) i], o[(size_t) j]));
            check (ceil && minD > 1.0e-3, "M08 CLIPPER: 7 algorithms, the ceiling holds", "max out (dB, ceiling -6): " + e);
        }
        // 9. clipper oversampling lowers the aliasing (hard clip of a 7.3 kHz sine)
        {
            auto alias = [&] (int os)
            {
                ParamFrame f = q; f[P::clp1On] = 1.f; f[P::clp1Type] = 1.f; f[P::clp1Drive] = 1.f; f[P::clp1Os] = (float) os; f[P::clp1Soft] = 0.f;
                const auto out = runDyn (f, sine (7300, 0.5f), 1 << 15);
                const int order = 14, n = 1 << order; juce::dsp::FFT fft (order); std::vector<float> buf ((size_t) n * 2, 0.f);
                for (int i = 0; i < n; ++i) buf[(size_t) i] = out.getSample (0, 8000 + i) * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) n));
                fft.performFrequencyOnlyForwardTransform (buf.data());
                double e = 0.0;
                for (int k = 1; k < n / 2; ++k) { const double fr = k * rate / n; if (fr < 30.0 || fr > 20000.0) continue; bool h = false; for (int m = 1; m * 7300.0 < 21000.0; ++m) h |= std::abs (fr - m * 7300.0) < 6.0 * rate / n; if (! h) e += (double) buf[(size_t) k] * buf[(size_t) k]; }
                return 10.0 * std::log10 (e + 1.0e-20);
            };
            const double a0 = alias (0), a2 = alias (2);
            check (a2 < a0 - 6.0, "M08 CLIPPER oversampling lowers the aliasing", "engine 4x " + String (a0, 1) + " dB -> 16x " + String (a2, 1) + " dB");
        }
        // 10 + 11. limiter: the ceiling is never passed (every mode, +18 dB in), TRUE PEAK holds between the samples too
        {
            bool held = true; String e;
            for (int m = 0; m < 5; ++m)
            {
                GeProcessor p; setP (p, "lim1_on", 1.f); setP (p, "lim1_input", 18.f); setP (p, "lim1_mode", (float) m); setP (p, "lim1_ceiling", -1.f);
                const auto y = hitProc (p, { { 0, MidiMessage::noteOn (1, G1, 1.f) } }, 0.6);
                const float pk = peak (y);
                held &= pk <= Decibels::decibelsToGain (-1.f) * 1.0001f; e << String (Decibels::gainToDecibels (pk), 2) << " ";
            }
            auto tpOf = [&] (const AudioBuffer<float>& y) { fge::dsp::dyn::TruePeak t; float m = 0; for (int i = 0; i < y.getNumSamples(); ++i) m = jmax (m, t.push (y.getSample (0, i), 8)); return Decibels::gainToDecibels (m); };
            GeProcessor a; setP (a, "lim1_on", 1.f); setP (a, "lim1_input", 18.f); setP (a, "lim1_ceiling", -1.f); setP (a, "lim1_true_peak", 1.f); setP (a, "lim1_oversampling", 3.f);
            const float tp = tpOf (hitProc (a, { { 0, MidiMessage::noteOn (1, G1, 1.f) } }, 0.6));
            check (held && tp < -0.8f, "M08 LIMITER: ceiling never passed (5 modes), TRUE PEAK between the samples", "peaks " + e + "dB, true peak " + String (tp, 2) + " dB (ceiling -1)");
        }
        // 12. crest factor: the measurement is right (a sine = 3.0 dB) and the control moves the kick toward the target
        {
            auto crestOf = [] (const AudioBuffer<float>& x, int from, int n) { double pk = 0, ms = 0; for (int i = from; i < from + n; ++i) { const double v = x.getSample (0, i); pk = jmax (pk, std::abs (v)); ms += v * v; } return 20.0 * std::log10 (pk / std::sqrt (ms / n + 1e-20)); };
            AudioBuffer<float> sn (1, 44100); for (int i = 0; i < 44100; ++i) sn.setSample (0, i, (float) std::sin (2.0 * MathConstants<double>::pi * 100.0 * i / 44100.0));
            const double cs = crestOf (sn, 0, 44100);
            const auto k0 = renderKick (q, G1, sr, 0.4);
            ParamFrame f = q; f[P::crOn] = 1.f; f[P::crTarget] = 3.f; f[P::crSens] = 1.f;
            const auto k1 = renderKick (f, G1, sr, 0.4);
            const double c0 = crestOf (k0, 0, (int) (0.4 * sr)), c1 = crestOf (k1, 0, (int) (0.4 * sr));
            check (std::abs (cs - 3.01) < 0.1 && c1 < c0 - 0.5, "M08 CREST FACTOR: real measurement, control moves it to the target", "sine " + String (cs, 2) + " dB, kick " + String (c0, 1) + " -> " + String (c1, 1) + " dB (target 3)");
        }
        // 13 + 18. AUTO GAIN keeps the level (slowly: the crest stays), LEVEL MATCH plays the dry kick at the processed level
        {
            ParamFrame c = cmp (dy::VCA, -24, 4, 1.f, 100.f);
            ParamFrame ag = c; ag[P::dynAuto] = 2.f;
            const auto k0 = renderKick (q, G1, sr, 0.5), kc = renderKick (c, G1, sr, 0.5), ka = renderKick (ag, G1, sr, 0.5);
            const double dropC = 20.0 * std::log10 (rms (kc) / rms (k0)), dropA = 20.0 * std::log10 (rms (ka) / rms (k0));
            auto crestOf = [] (const AudioBuffer<float>& x) { double pk = 0, ms = 0; for (int i = 0; i < x.getNumSamples(); ++i) { const double v = x.getSample (0, i); pk = jmax (pk, std::abs (v)); ms += v * v; } return 20.0 * std::log10 (pk / std::sqrt (ms / x.getNumSamples() + 1e-20)); };
            ParamFrame lm = c; lm[P::dynMatch] = 1.f;
            const auto kl = renderKick (lm, G1, sr, 0.5);
            const double dropL = 20.0 * std::log10 (rms (kl) / rms (k0));
            check (dropC < -3.0 && std::abs (dropA) < std::abs (dropC) * 0.5 && std::abs (crestOf (ka) - crestOf (kc)) < 3.0 && std::abs (dropL - dropC) < 3.0 && diff (kl, kc) > 1.0e-3,
                   "M08 AUTO GAIN (windowed, no per-sample normalising) + LEVEL MATCH",
                   "comp " + String (dropC, 1) + " dB, with auto gain " + String (dropA, 1) + " dB, level match " + String (dropL, 1) + " dB");
        }
        // 14. group processing: a BODY compressor changes only the body
        {
            ParamFrame g = cmp (dy::VCA, -40, 10); g[P::cmp1Loc] = 2.f;
            ParamFrame gm = g; gm[P::mixBodyMute] = 1.f; ParamFrame qm = q; qm[P::mixBodyMute] = 1.f;
            check (diff (renderKick (g, G1, sr, 0.4), renderKick (q, G1, sr, 0.4)) > 1.0e-3 && diff (renderKick (gm, G1, sr, 0.4), renderKick (qm, G1, sr, 0.4)) < 1.0e-7,
                   "M08 GROUP processing: a BODY compressor changes only the body");
        }
        // 15. automation of threshold / ratio / ceiling during a kick does not click
        {
            auto run = [&] (bool sw)
            {
                GeProcessor p; setP (p, "cmp1_on", 1.f); setP (p, "cmp1_threshold", -20.f); setP (p, "clp1_on", 1.f);
                p.prepareToPlay (sr, 128);
                float maxStep = 0.f, last = 0.f, pk = 0.f;
                for (int blk = 0; blk < 120; ++blk)
                {
                    AudioBuffer<float> bb (2, 128); bb.clear(); MidiBuffer m;
                    if (blk == 0) m.addEvent (MidiMessage::noteOn (1, G1, 1.f), 0);
                    if (sw) { setP (p, "cmp1_threshold", -30.f + 20.f * (float) std::sin (blk * 0.5)); setP (p, "cmp1_ratio", 2.f + (float) (blk % 7)); setP (p, "clp1_ceiling", -6.f + 5.f * (float) std::sin (blk * 0.3)); }
                    p.processBlock (bb, m);
                    for (int i = 0; i < 128; ++i) { const float v = bb.getSample (0, i); if (blk > 4) maxStep = jmax (maxStep, std::abs (v - last)); last = v; pk = jmax (pk, std::abs (v)); }
                }
                return maxStep / jmax (1.0e-6f, pk);
            };
            const float a = run (false), b = run (true);
            check (b < a * 1.6f + 0.02f, "M08 automation of threshold / ratio / ceiling is smooth", "max step " + String (b, 3) + " vs steady " + String (a, 3));
        }
        // 16 + 17. realtime = export with a MASTER lookahead (the export is on the beat, the host gets the latency) + CPU + presets
        {
            GeProcessor p; m08t::apply (p, m08t::presets()[2]);                       // TIGHT IMPACT (+ master limiter with lookahead)
            setP (p, "cmp2_on", 1.f); setP (p, "cmp2_location", 15.f); setP (p, "cmp2_lookahead", 3.f);
            p.prepareToPlay (sr, 512);
            const auto rt = hitProc (p, { { 0, MidiMessage::noteOn (1, G1, (uint8) 127) } }, 0.8);
            const int look = p.getLatencySamples() - p.getEngine().latencySamples() + p.getEngine().dynLookahead();   // (the processor reported it)
            const auto off = renderKick (p.frame(), G1, sr, 0.6, -1, 1.f, &p.shapes(), 0.0, -1.0, nullptr, nullptr, &p.distData());
            AudioBuffer<float> rs (2, off.getNumSamples());
            for (int c = 0; c < 2; ++c) rs.copyFrom (c, 0, rt, c, look, off.getNumSamples());
            const bool latOk = p.getLatencySamples() == p.getEngine().latencySamples() && look > 100;
            bool allSound = true; String e;
            for (auto& pr : m08t::presets())
            {
                GeProcessor pp; m08t::apply (pp, pr);
                const auto k = renderKick (pp.frame(), G1, sr, 0.4);
                const bool good = std::isfinite (rms (k)) && peak (k) < 4.f && diff (k, renderKick (base, G1, sr, 0.4)) > 1.0e-4;
                if (! good) e << pr.name << " ";
                allSound &= good;
            }
            auto x = m08t::moduleToXml (p); const auto before = p.frame();
            m06t::reset (p, m08t::moduleIds()); m08t::load (p, *x, 0, 0);
            bool same = true; for (int i = P::cmp1On; i < P::count; ++i) same &= std::abs (p.frame()[i] - before[i]) < 1.0e-4f;
            GeProcessor full;
            for (int i = 1; i <= 4; ++i) { setP (full, ("cmp" + String (i) + "_on").toRawUTF8(), 1.f); setP (full, ("cmp" + String (i) + "_location").toRawUTF8(), (float) (i == 4 ? 15 : i)); }
            for (int i = 1; i <= 5; ++i) { setP (full, ("ts" + String (i) + "_on").toRawUTF8(), 1.f); setP (full, ("ts" + String (i) + "_attack").toRawUTF8(), 0.5f); }
            for (auto* id : { "clp1_on", "clp2_on", "lim1_on", "lim2_on", "crest_on" }) setP (full, id, 1.f);
            setP (full, "clp1_oversampling", 2.f); setP (full, "lim1_true_peak", 1.f); setP (full, "dyn_auto_gain", 3.f);
            GeProcessor plain; const double c0 = cpuTime (plain, 1.0), c1 = cpuTime (full, 1.0);
            check (diff (rs, off) < 1.0e-6 && latOk && allSound && same && c1 < 8.0 * c0, "M08 realtime = export (lookahead reported to the host), presets / files, CPU",
                   e + "rt vs export " + String (diff (rs, off), 8) + ", latency " + String (p.getLatencySamples()) + " samples, CPU " + String (100.0 * c0, 1) + " % -> " + String (100.0 * c1, 1) + " %");
        }
        // the owner's listening file: the MODERN kick, one bar per module 08 preset (each at the same RMS: hear the shape, not the level)
        {
            const int beat = (int) (0.4 * sr), fade = (int) (0.006 * sr);
            const auto& pl = m08t::presets();
            AudioBuffer<float> demo (2, beat * 4 * (int) (pl.size() + 1)); demo.clear();
            String order;
            for (int k = 0; k <= (int) pl.size(); ++k)
            {
                GeProcessor pp; pp.setCurrentProgram (1);
                if (k > 0) for (auto& [id, v] : pl[(size_t) k - 1].v) setP (pp, id, v);
                auto x = renderKick (pp.frame(), G1, sr, 0.4, -1, 1.f, &pp.shapes(), 0.0, -1.0, nullptr, nullptr, &pp.distData());
                x.applyGain (0.2f / (float) jmax (1.0e-6, rms (x)));
                for (int i = 0; i < fade; ++i) for (int c = 0; c < 2; ++c) x.setSample (c, beat - fade + i, x.getSample (c, beat - fade + i) * (1.f - (float) i / (float) fade));
                for (int b = 0; b < 4; ++b) for (int c = 0; c < 2; ++c) demo.copyFrom (c, (k * 4 + b) * beat, x, c, 0, beat);
                order << String (k * 1.6, 1) << " s " << (k == 0 ? String ("(dry)") : String (pl[(size_t) k - 1].name)) << "\n";
            }
            demo.applyGain (0.95f / jmax (1.0e-6f, peak (demo)));
            writeWav (outDir.getChildFile ("4ge_m08_dynamics_presets.wav"), demo);
            outDir.getChildFile ("4ge_m08_dynamics_presets.txt").replaceWithText (order);
        }
    }
    // ---------------------------------------------------------------- module 03 wiring: oscillator data in the state (OSCS), presets, realtime = export
    {
        GeProcessor p; setP (p, "osc1_on", 1.f); setP (p, "osc1_wave", 6.f); setP (p, "osc2_on", 1.f); setP (p, "osc2_wave", 8.f);
        const auto drawn = env::Shape::make ({ { 0.f, 1.f }, { 0.3f, 1.f }, { 0.35f, -1.f }, { 1.f, -0.6f } }, env::Linear);
        p.setOscCustom (0, drawn);
        auto amps = p.oscData().osc[1]; for (int hh = 0; hh < 64; ++hh) amps.hAmp[(size_t) hh] = hh % 3 == 0 ? 1.f : 0.f;
        p.setOscProperty (1, "hAmp", fge::dsp::oscstate::harmonicsText (amps, 0));
        p.refreshTables();
        const auto tb = p.currentTables();
        fge::dsp::OscTables ot; for (int i = 0; i < 8; ++i) ot.t[(size_t) i] = tb[(size_t) i].get();
        const auto rt = hitProc (p, { { 0, MidiMessage::noteOn (1, 31, (uint8) 127) } }, 0.6);
        const auto off = renderKick (p.frame(), 31, sr, 0.6, -1, 1.f, &p.shapes(), 0.0, -1.0, &ot);
        const auto plain = renderKick (p.frame(), 31, sr, 0.6, -1, 1.f, &p.shapes());                     // default data
        const bool same = diff (rt, off) < 1.0e-6 * jmax (1.0, rms (off)) + 1.0e-7, matters = diff (off, plain) > 1.0e-3;
        MemoryBlock mb; p.getStateInformation (mb);
        GeProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize()); q.refreshTables();
        const bool saved = q.oscData().osc[0].custom.toString() == drawn.toString() && q.oscData().osc[1].hAmp[1] == 0.f
                           && diff (hitProc (q, { { 0, MidiMessage::noteOn (1, 31, (uint8) 127) } }, 0.6), rt) < 1.0e-6;
        p.undo.undo();
        const bool undone = p.oscData().osc[1].hAmp[1] > 0.f;
        p.setCurrentProgram (0);
        const bool reset = p.oscData().osc[0].custom.toString() == fge::dsp::OscData::defaultCustom().toString();
        check (same && matters && saved && undone && reset, "M03 17.14 / 17.15 oscillator data: saved, undo, realtime = export, factory reset",
               String (same ? "" : "rt!=export ") + (matters ? "" : "no-effect ") + (saved ? "" : "not-saved ") + (undone ? "" : "no-undo ") + (reset ? "" : "no-reset"));
    }
    // ---------------------------------------------------------------- module 02 wiring: DRAWN shapes in the state (ENVS), undo, presets, audio
    {
        auto hit = [] (GeProcessor& p) { return hitProc (p, { { 0, MidiMessage::noteOn (1, 31, (uint8) 127) } }, 0.6); };
        GeProcessor a; setP (a, "tail_amp_env_mode", 1.f);                 // TAIL AMP = DRAWN
        const auto before = hit (a);
        const auto cut = env::Shape::make ({ { 0.f, 1.f }, { 0.08f, 1.f }, { 0.1f, 0.f }, { 1.f, 0.f } });
        a.setShape (env::AmpTail, cut);
        const auto after = hit (a);
        const bool audio = diff (before, after) > 0.05 * rms (before);
        MemoryBlock mb; a.getStateInformation (mb);
        GeProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool saved = b.shape (env::AmpTail).toString() == cut.toString() && diff (hit (b), after) < 1.0e-6;
        a.undo.undo();
        const bool undone = a.shape (env::AmpTail).toString() == env::defaultShape (env::AmpTail).toString();
        a.undo.redo();
        a.setCurrentProgram (1);
        const bool reset = a.shape (env::AmpTail).toString() == env::defaultShape (env::AmpTail).toString();
        // a user preset file carries the shapes too
        b.setShape (env::PitchTail, env::Shape::make ({ { 0.f, 1.f }, { 0.5f, 1.f }, { 0.55f, 0.f }, { 1.f, 0.f } }));
        std::unique_ptr<XmlElement> xml (b.apvts.copyState().createXml());
        auto tmp = File::createTempFile (".4gp"); xml->writeTo (tmp);
        GeProcessor c; c.loadUserPresetFile (tmp); tmp.deleteFile();
        const bool preset = c.shape (env::PitchTail).toString() == b.shape (env::PitchTail).toString() && c.shape (env::AmpTail).toString() == cut.toString();
        check (audio && saved && undone && reset && preset, "M02 drawn shapes: change the sound, saved in project + preset, undo, factory reset",
               String (audio ? "" : "no-audio ") + (saved ? "" : "not-saved ") + (undone ? "" : "no-undo ") + (reset ? "" : "no-reset ") + (preset ? "" : "no-preset"));
    }
    // ---------------------------------------------------------------- factory presets: all play, sane levels
    {
        GeProcessor p;
        String e; bool good = true;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            auto x = renderKick (p.frame(), G1, sr, 1.5);
            const float pk = peak (x);
            good &= pk > 0.15f && pk < 4.f;
            writeWav (outDir.getChildFile (String (i).paddedLeft ('0', 2) + "_" + p.getProgramName (i) + ".wav"), x);
            e << String (Decibels::gainToDecibels (pk), 0) << " ";
        }
        check (good, "PRESETS: every factory preset plays at a sane level", e);
    }
    // ---------------------------------------------------------------- GUI: snapshot, the waveform shows the real kick
    {
        GeProcessor p;
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        auto* ge = dynamic_cast<GeEditor*> (ed.get());
        ed->setSize (GeCanvas::W, GeCanvas::H);
        ge->canvas.waveView().update (p.frame(), p.auditionNote);       // (the editor timer does this in the plugin)
        for (int i = 0; i < 300 && ! ge->canvas.waveView().hasRender(); ++i) Thread::sleep (20);
        const auto pk = ge->canvas.waveView().lastPeaks();
        check (ge->canvas.waveView().hasRender() && std::abs (pk[0] - peak (kick) * 1.f) < 0.25f * peak (kick),
               "GUI: the central waveform is the rendered kick", "peak " + String (pk[0], 2) + " vs " + String (peak (kick), 2));
        // a disabled control: SUB PITCH with tracking on
        auto* sp = ge->canvas.findControl ("sub_pitch");
        check (sp != nullptr && ! sp->isEnabled(), "GUI: SUB PITCH greyed out while PITCH TRACKING is on");
        for (int part : { 2, 0 })
        {
            ge->canvas.selectPart (part);
            auto img = ge->canvas.createComponentSnapshot (ge->canvas.getLocalBounds());
            PNGImageFormat png;
            FileOutputStream os (outDir.getChildFile (part == 2 ? "gui.png" : "gui_punch.png"));
            if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (img, os); }
        }
        // ENVELOPES page (module 02): drawing on TAIL PITCH switches it to CUSTOM, plays, and undoes in one step
        {
            auto& env = ge->canvas.envelopes();
            ge->canvas.showPage (1);
            env.select (2, gui::EnvPage::Pitch);
            const auto before = renderKick (p.frame(), G1, sr, 0.6, -1, 1.f, &p.shapes());
            const auto drawn = env::Shape::make ({ { 0.f, 1.f }, { 0.4f, 1.f }, { 0.45f, 0.f }, { 1.f, 0.f } });
            env.commitShape (drawn);
            const bool custom = roundToInt (p.apvts.getRawParameterValue ("pe_mode")->load()) == 8 && p.shape (env::PitchTail).toString() == drawn.toString();
            const auto after = renderKick (p.frame(), G1, sr, 0.6, -1, 1.f, &p.shapes());
            const bool plays = diff (before, after) > 0.05 * rms (before);
            p.undo.undo();
            const bool undone = roundToInt (p.apvts.getRawParameterValue ("pe_mode")->load()) != 8
                                && p.shape (env::PitchTail).toString() == env::defaultShape (env::PitchTail).toString();
            // 64 points through the editor, never more; the time order is kept
            env.select (1, gui::EnvPage::Amp);
            auto& ed2 = env.editor();
            for (int i = 0; i < 80; ++i) ed2.addPoint ((float) (i % 70) / 70.f + 0.003f, 0.5f + 0.4f * std::sin ((float) i));
            const auto& sh = p.shape (env::AmpBody);
            bool ordered = true;
            for (int i = 1; i < sh.n; ++i) ordered &= sh.p[(size_t) i].t >= sh.p[(size_t) i - 1].t;
            const bool drawnMode = roundToInt (p.apvts.getRawParameterValue ("body_amp_env_mode")->load()) == 1;
            // every view has its real parameters on the page
            bool wired = true;
            for (auto [pp, vv, id] : std::initializer_list<std::tuple<int, int, const char*>> { { 2, 0, "tail_penv_offset" }, { 0, 1, "punch_amp_env_delay" },
                     { 2, 2, "tail_filter_env_attack" }, { 3, 3, "sub_drive_env_amount" }, { 1, 4, "body_reso_env_decay" } })
            { env.select (pp, vv); wired &= env.findControl (id) != nullptr; }
            check (custom && plays && undone && sh.n == env::maxPoints && ordered && drawnMode && wired,
                   "M02 GUI: drawing switches to CUSTOM / DRAWN, plays, one undo, 64 points max, every view wired",
                   String (sh.n) + " points" + (custom ? "" : " no-custom") + (plays ? "" : " silent") + (undone ? "" : " no-undo")
                   + (ordered ? "" : " order") + (drawnMode ? "" : " not-drawn") + (wired ? "" : " not-wired"));
            env.select (2, gui::EnvPage::Pitch);
            env.commitShape (env::Shape::make ({ { 0.f, 1.f }, { 0.25f, 1.f }, { 0.3f, 0.2f }, { 0.6f, 0.05f }, { 1.f, 0.f } }));
            env.select (2, gui::EnvPage::Pitch);
            auto img = ge->canvas.createComponentSnapshot (ge->canvas.getLocalBounds());
            PNGImageFormat png;
            FileOutputStream os (outDir.getChildFile ("gui_envelopes.png"));
            if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (img, os); }
            ge->canvas.showPage (0);
        }
        // OSCILLATORS page (module 03): every tab wired, the scope shows the REAL spectrum
        {
            ge->canvas.showPage (2);
            auto& op = ge->canvas.oscillators();
            setP (p, "osc1_on", 1.f); setP (p, "osc1_wave", 8.f); setP (p, "osc1_h_even", 0.f);
            p.refreshTables();
            op.selectOsc (0);
            bool wired = true; String missing;
            const char* need[8] { "osc1_noise", "osc1_wt_position", "osc1_h_odd", "osc1_detune", "fm_2_to_1", "osc1_ring_mix", "osc1_phase_mode", "osc1_sync_amount" };
            for (int t = 0; t < 8; ++t) { op.selectTab (t); if (op.findControl (need[t]) == nullptr) { wired = false; missing << need[t] << " "; } }
            op.selectTab (2);
            op.scope().update (p.frame(), p.currentTables(), 0, false, true);
            for (int i = 0; i < 300 && ! op.scope().hasRender(); ++i) Thread::sleep (20);
            Thread::sleep (200);
            const auto sp = op.scope().lastSpectrum();
            const float oddMinusEven = sp[2] - sp[1];                         // harmonic 3 vs harmonic 2
            check (wired && op.scope().hasRender() && oddMinusEven > 30.f, "M03 GUI: 8 tabs wired, the scope spectrum is the real signal (EVEN 0 -> no h2)",
                   "h3 - h2 = " + String (oddMinusEven, 1) + " dB " + missing);
            auto img = ge->canvas.createComponentSnapshot (ge->canvas.getLocalBounds());
            PNGImageFormat png;
            FileOutputStream os (outDir.getChildFile ("gui_oscillators.png"));
            if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (img, os); }
            op.selectTab (0);
            auto img2 = ge->canvas.createComponentSnapshot (ge->canvas.getLocalBounds());
            FileOutputStream os2 (outDir.getChildFile ("gui_oscillators_wave.png"));
            if (os2.openedOk()) { os2.setPosition (0); os2.truncate(); png.writeImageToStream (img2, os2); }
            ge->canvas.showPage (0);
            setP (p, "osc1_on", 0.f);
        }
        // LAYERS page (module 04): randomizer respects LOCK, phase align only on APPLY, the displays are real, sets restore
        {
            ge->canvas.showPage (3);
            auto& lp = ge->canvas.layersPage();
            setP (p, "layer1_on", 1.f); setP (p, "layer2_on", 1.f); setP (p, "layer2_phase", 90.f);
            p.undo.beginNewTransaction();
            layers::setLocked (p, 1, true);
            const float l2Before = p.apvts.getRawParameterValue ("layer2_pitch")->load(), l1Before = p.apvts.getRawParameterValue ("layer1_pitch")->load();
            layers::RandSettings rs; rs.mode = layers::RPitch; rs.intensity = 1.f; rs.seed = 5;
            layers::randomize (p, rs);
            const bool lockOk = p.apvts.getRawParameterValue ("layer2_pitch")->load() == l2Before && p.apvts.getRawParameterValue ("layer1_pitch")->load() != l1Before;
            p.undo.undo();
            const bool undoOk = p.apvts.getRawParameterValue ("layer1_pitch")->load() == l1Before;
            // phase align: analysis changes nothing; the best phase of layer 2 against layer 1 (same sine) is ~0 degrees
            const auto before = p.frame();
            p.refreshTables();
            const auto tb2 = p.currentTables(); fge::dsp::OscTables ot2; for (int i = 0; i < 8; ++i) ot2.t[(size_t) i] = tb2[(size_t) i].get();
            const auto r = fge::dsp::analysePhase (p.frame(), 0, 1, 0.f, 60.f, 360.f, 31, &p.shapes(), &ot2);
            const auto after = p.frame();
            bool untouched = true; for (int i = 0; i < P::count; ++i) untouched &= before[i] == after[i];
            const float err = std::abs (std::remainder (r.bestDeg, 360.f));
            // the displays show the real signals (layer 1 on = waveform, layer 3 off = silent)
            lp.preview().update (p.frame(), p.shapes(), p.shapesVersion() + 1000, p.currentTables());
            std::vector<fge::gui::KickPreview::Wave> w;
            for (int i = 0; i < 400; ++i) { w = lp.preview().waves(); if (w.size() >= 12) break; Thread::sleep (20); }
            const bool real = w.size() >= 12 && ! w[0].silent && w[2].silent;
            // a layer set file restores the full configuration
            auto x = layers::setToXml (p);
            setP (p, "layer1_level", -30.f); setP (p, "group_texture_level", -10.f);
            layers::xmlToSet (p, *x);
            const bool restored = std::abs (p.apvts.getRawParameterValue ("layer1_level")->load() + 6.f) < 0.01f && std::abs (p.apvts.getRawParameterValue ("group_texture_level")->load()) < 0.01f;
            check (lockOk && undoOk && untouched && err < 12.f && r.bestCorr > 0.9f && real && restored,
                   "M04 19.12-19.15 randomizer LOCK + undo, phase align only on APPLY, real displays, sets restore",
                   "best " + String (r.bestDeg, 0) + " deg corr " + String (r.bestCorr, 2) + (lockOk ? "" : " lock!") + (undoOk ? "" : " undo!") + (untouched ? "" : " changed!")
                   + (real ? "" : " display!") + (restored ? "" : " set!"));
            PNGImageFormat png;
            for (int v = 0; v < 3; ++v)
            {
                lp.showView (v);
                auto img = ge->canvas.createComponentSnapshot (ge->canvas.getLocalBounds());
                FileOutputStream os (outDir.getChildFile ("gui_layers" + String (v) + ".png"));
                if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (img, os); }
            }
            setP (p, "layer1_on", 0.f); setP (p, "layer2_on", 0.f);
            ge->canvas.showPage (0);
        }
        // DISTORTION page (module 05): only the controls the type uses, the chain list, the real visualizer
        {
            ge->canvas.showPage (4);
            auto& dp = ge->canvas.distPage();
            p.undo.beginNewTransaction();
            const int st = distt::addStage (p, fge::dsp::dist::LocTail, fge::dsp::dist::Hard);
            dp.selectLocation (fge::dsp::dist::LocTail); dp.selectStage (st);
            const String o = "dist" + String (st + 1) + "_";
            auto vis = dp.visibleIds();
            const bool hard = vis.contains (o + "ceiling") && ! vis.contains (o + "even") && ! vis.contains (o + "bits");
            setP (p, (o + "type").toRawUTF8(), 2.f); dp.selectStage (st);
            vis = dp.visibleIds();
            const bool tube = vis.contains (o + "even") && vis.contains (o + "odd") && ! vis.contains (o + "ceiling");
            setP (p, (o + "drive").toRawUTF8(), 0.9f);
            dp.requestScope();
            fge::gui::DistScope::Result r;
            for (int i = 0; i < 400; ++i) { Thread::sleep (20); r = dp.scopeResult(); if (r.valid && r.stage == st) break; }
            const bool scopeOk = r.valid && r.stage == st && r.curveX.size() == (size_t) fge::gui::DistScope::columns && ! r.specA.empty();
            p.undo.undo();
            const bool undone = p.apvts.getRawParameterValue ((o + "on").toRawUTF8())->load() < 0.5f;
            distt::apply (p, distt::presets()[6]);                                  // RAW ASYMMETRIC (tail) for the snapshot
            dp.selectLocation (fge::dsp::dist::LocTail);
            for (int i = 0; i < 100; ++i) { dp.requestScope(); Thread::sleep (20); if (dp.scopeResult().valid && dp.scopeResult().stage == dp.stage()) break; }
            dp.adoptScope();
            check (hard && tube && scopeOk && undone, "M05 GUI: per-type controls, chain, real visualizer, ADD = one undo step",
                   String (hard ? "" : "hard! ") + (tube ? "" : "tube! ") + (scopeOk ? "" : "scope! ") + (undone ? "" : "undo!"));
            PNGImageFormat png;
            auto img = ge->canvas.createComponentSnapshot (ge->canvas.getLocalBounds());
            FileOutputStream os (outDir.getChildFile ("gui_distortion.png"));
            if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (img, os); }
            distt::resetAll (p);
            ge->canvas.showPage (0);
        }
        // SCREECH page (module 06): only the controls the type / mode uses, real displays
        {
            ge->canvas.showPage (5);
            auto& sp = ge->canvas.screechPage();
            setP (p, "flt1_on", 1.f); setP (p, "flt1_type", 0.f);
            sp.showTab (0); sp.selectFilter (0);
            auto vis = sp.visibleIds();
            const bool lp = vis.contains ("flt1_slope") && ! vis.contains ("flt1_vowel") && ! vis.contains ("flt1_svf_mode");
            setP (p, "flt1_type", 8.f); sp.selectFilter (0); vis = sp.visibleIds();
            const bool fmt = vis.contains ("flt1_vowel") && ! vis.contains ("flt1_slope");
            setP (p, "scr_on", 1.f); setP (p, "scr_mode", 3.f); sp.showTab (2); sp.showScreechView (1); vis = sp.visibleIds();
            const bool fm = vis.contains ("scr_fm_index") && ! vis.contains ("scr_metal") && ! vis.contains ("scr_fb_amount");
            sp.requestScope();
            fge::gui::M06Scope::Result r;
            for (int i = 0; i < 400; ++i) { Thread::sleep (20); r = sp.scopeResult(); if (r.valid && ! r.scrHi.empty()) break; }
            const bool real = r.valid && r.fltStart.size() == (size_t) fge::gui::M06Scope::bins && r.scrPeak > 1.0e-4f;
            check (lp && fmt && fm && real, "M06 GUI: per-type / per-mode controls, real displays",
                   String (lp ? "" : "lp! ") + (fmt ? "" : "formant! ") + (fm ? "" : "fm! ") + (real ? "" : "display!"));
            m06t::apply (p, m06t::presets()[2]);                                    // WIDE SCREECH for the snapshots
            PNGImageFormat png;
            for (int t = 0; t < 3; ++t)
            {
                sp.showTab (t); if (t == 2) sp.showScreechView (0);
                sp.requestScope();
                for (int i = 0; i < 200; ++i) { Thread::sleep (20); if (sp.scopeResult().valid) break; }
                Thread::sleep (300); sp.adoptScope();
                auto img = ge->canvas.createComponentSnapshot (ge->canvas.getLocalBounds());
                FileOutputStream os (outDir.getChildFile ("gui_screech" + String (t) + ".png"));
                if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (img, os); }
            }
            m06t::reset (p, m06t::moduleIds());
            ge->canvas.showPage (0);
        }
        // EQ page (module 07): only the controls the type uses, the response is the real one, the analyzer runs
        {
            ge->canvas.showPage (6);
            auto& ep = ge->canvas.eqPage();
            setP (p, "eq1_on", 1.f); setP (p, "eq1_type", 0.f); setP (p, "eq1_freq", 1000.f); setP (p, "eq1_gain", 12.f); setP (p, "eq1_q", 1.f);
            ep.showTab (0); ep.selectBand (0);
            auto vis = ep.visibleIds();
            const bool bell = vis.contains ("eq1_gain") && vis.contains ("eq1_q") && ! vis.contains ("eq1_slope");
            setP (p, "eq1_type", 3.f); ep.selectBand (0); vis = ep.visibleIds();
            const bool cut = vis.contains ("eq1_slope") && ! vis.contains ("eq1_gain");
            setP (p, "eq1_type", 0.f); ep.selectBand (0);
            ep.requestScope();
            fge::gui::EqScope::Result r;
            float at1k = -100.f;
            for (int i = 0; i < 600; ++i)                                         // (an older request may answer first: wait for this one)
            {
                Thread::sleep (20); r = ep.scopeResult();
                if (! r.valid || r.band != 0) continue;
                at1k = -100.f; for (int k = 0; k < fge::gui::EqScope::bins; ++k) if (std::abs (fge::gui::EqScope::binHz (k) - 1000.f) < 30.f) at1k = jmax (at1k, r.resp[(size_t) k]);
                if (std::abs (at1k - 12.f) < 0.5f) break;
                if (i % 50 == 49) ep.requestScope();
            }
            const bool real = r.valid && std::abs (at1k - 12.f) < 0.5f && ! r.post.empty() && r.domHz > 20.f;
            check (bell && cut && real, "M07 GUI: per-type controls, the curve is the real response (+12 dB bell), analyzer PRE / POST",
                   String (bell ? "" : "bell! ") + (cut ? "" : "cut! ") + (real ? "" : "display (" + String (at1k, 1) + ")!"));
            m07t::apply (p, m07t::presets()[2]);                                    // MODERN BALANCE for the snapshots
            PNGImageFormat png;
            for (int t = 0; t < 5; ++t)
            {
                ep.showTab (t); ep.requestScope();
                for (int i = 0; i < 200; ++i) { Thread::sleep (20); if (ep.scopeResult().valid) break; }
                Thread::sleep (300); ep.adoptScope();
                auto img = ge->canvas.createComponentSnapshot (ge->canvas.getLocalBounds());
                FileOutputStream os (outDir.getChildFile ("gui_eq" + String (t) + ".png"));
                if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (img, os); }
            }
            m06t::reset (p, m07t::moduleIds());
            ge->canvas.showPage (0);
        }
        // DYN page (module 08): per-type enabling, the curve is the real one (cmpGainDb), the probe history shows the gain reduction
        {
            ge->canvas.showPage (7);
            auto& dp = ge->canvas.dynPage();
            dp.showTab (0); dp.select (0);
            setP (p, "cmp1_on", 1.f); setP (p, "cmp1_location", 0.f); setP (p, "cmp1_threshold", -30.f); setP (p, "cmp1_ratio", 4.f); setP (p, "cmp1_knee", 0.f);
            dp.select (0);
            const bool lookOff = ! dp.isControlEnabled ("cmp1_lookahead") && ! dp.isControlEnabled ("cmp1_dry_level");
            setP (p, "cmp1_location", 15.f); setP (p, "cmp1_type", 6.f); dp.select (0);
            Thread::sleep (100); dp.select (0);
            const bool lookOn = dp.isControlEnabled ("cmp1_lookahead") && dp.isControlEnabled ("cmp1_dry_level") && ! dp.isControlEnabled ("cmp1_sc_source");
            setP (p, "cmp1_location", 0.f); setP (p, "cmp1_type", 0.f); dp.select (0);
            fge::gui::DynScope::Result r; bool curveOk = false, histOk = false;
            dp.requestScope();
            for (int i = 0; i < 600 && ! (curveOk && histOk); ++i)
            {
                Thread::sleep (20); r = dp.scopeResult();
                if (! r.valid || r.kind != 0 || r.index != 0) continue;
                // at 0 dB in: -30 + 30 / 4 = -22.5 dB out
                curveOk = ! r.curveY.empty() && std::abs (r.curveY.back() + 22.5f) < 0.3f;
                histOk = r.gr.size() == (size_t) fge::gui::DynScope::histN && r.maxGr < -3.f;
                if (i % 50 == 49) dp.requestScope();
            }
            dp.showTab (4); const bool limTab = dp.visibleIds().contains ("lim1_ceiling") && ! dp.visibleIds().contains ("cmp1_ratio");
            check (lookOff && lookOn && curveOk && histOk && limTab, "M08 GUI: per-type / per-place controls, the real curve, the gain-reduction history",
                   String (lookOff ? "" : "lookahead off! ") + (lookOn ? "" : "lookahead on! ") + (curveOk ? "" : "curve (" + String (r.curveY.empty() ? 0.f : r.curveY.back(), 1) + ")! ")
                   + (histOk ? "" : "history (" + String (r.maxGr, 1) + ")! ") + (limTab ? "" : "tab!"));
            m08t::apply (p, m08t::presets()[2]);                                    // TIGHT IMPACT for the snapshots
            setP (p, "clp1_type", 6.f);
            PNGImageFormat png;
            for (int t = 0; t < fge::gui::DynPage::NumTabs; ++t)
            {
                dp.showTab (t); dp.select (0); dp.requestScope();
                for (int i = 0; i < 300; ++i) { Thread::sleep (20); auto q = dp.scopeResult(); if (q.valid && q.kind == jmax (0, dp.kind())) break; }
                Thread::sleep (200); dp.adoptScope();
                auto img = ge->canvas.createComponentSnapshot (ge->canvas.getLocalBounds());
                FileOutputStream os (outDir.getChildFile ("gui_dyn" + String (t) + ".png"));
                if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (img, os); }
            }
            m06t::reset (p, m08t::moduleIds());
            ge->canvas.showPage (0);
        }
        auto wav = ge->canvas.waveView().writeWav (outDir);
        check (wav.existsAsFile() && wav.getSize() > 20000, "EXPORT: the waveform writes the kick as a WAV (drag into FL)", wav.getFileName());
    }
    printf ("%s\n", ok ? "ALL OK" : "SOMETHING FAILED");
    return ok ? 0 : 1;
}
