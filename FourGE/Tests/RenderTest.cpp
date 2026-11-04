// 4GE offline checks (rebuild, module 01 Core Kick Engine - its acceptance list 14.1-14.12 + the registry / GUI)
//   RenderTest <outdir>                                   tests, renders of the factory presets, GUI snapshot
//   RenderTest <outdir> --render <settings.txt> <out.wav> one kick: lines "note N", "seconds S", "<param id> <value>"
#include "../Source/PluginProcessor.h"
#include "../Source/GUI/PluginEditor.h"
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

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory().getChildFile ("4ge_out"));
    outDir.createDirectory();
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
        for (int id = 0; id < P::count; ++id)
        {
            const auto& d = def (id);
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
            if (sid.endsWith ("_trigger")) continue;                                                // MIDI behaviour (note-off / legato): checked below
            if (d.kind == Kind::Choice)
            {
                bool any = false;
                for (int c = 0; c < d.choices().size(); ++c) { if (c == (int) d.def) continue; b[id] = (float) c; any |= diff (renderKick (a, G1, sr, 0.6), renderKick (b, G1, sr, 0.6)) > 1.0e-4; }
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
        auto wav = ge->canvas.waveView().writeWav (outDir);
        check (wav.existsAsFile() && wav.getSize() > 20000, "EXPORT: the waveform writes the kick as a WAV (drag into FL)", wav.getFileName());
    }
    printf ("%s\n", ok ? "ALL OK" : "SOMETHING FAILED");
    return ok ? 0 : 1;
}
