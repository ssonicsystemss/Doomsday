// Offline renderer for the 5CR34M 4LPH4 engine prototype.
//   AlphaRender <out.wav> note=<midi> dur=<s> [param=value ...]
//   AlphaRender <out.wav> pattern=<file> bpm=<150> [param=value ...]   (file: one note per line "startBeat lenBeats midi")
//   AlphaRender <out.wav> params=<file> ...                              (file: param=value lines, then overrides)
#include "../Source/AlphaEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <functional>
using namespace juce;

static std::map<String, std::function<void (alpha::Params&, float)>> setters()
{
    std::map<String, std::function<void (alpha::Params&, float)>> m;
#define F(name) m[#name] = [] (alpha::Params& p, float v) { p.name = v; };
#define I(name) m[#name] = [] (alpha::Params& p, float v) { p.name = (int) std::round (v); };
    F(scoopSt) F(scoopMs) F(scoopCurve) F(glideMs) F(tuneSt) F(coreLevel) F(coreShape) F(hashLevel) F(hashDecayMs) F(hashSustain) I(voices) F(detuneCents)
    F(carrierSaw) F(ratio) F(index) F(chaos) F(chaosHz) F(grit) F(hashHp) F(drive1Db) I(drive1Curve) F(combMix) F(combFb)
    F(combRatio) F(formHz) F(formGainDb) F(formQ) F(drive2Db) I(drive2Curve) F(toneHz) F(lowCut) F(attackMs)
    F(decayMs) F(sustain) F(releaseMs) F(outDb)
#undef F
#undef I
    return m;
}

int main (int argc, char** argv)
{
    if (argc < 3) { printf ("usage: AlphaRender out.wav note=67 dur=1.6 [param=value ...]\n"); return 1; }
    const double sr = 48000.0; const int block = 512;
    alpha::Params p;
    auto set = setters();
    int note = 67; double dur = 1.6, bpm = 150.0; String pattern;
    StringArray args;
    for (int i = 2; i < argc; ++i) args.add (argv[i]);
    for (int i = 0; i < args.size(); ++i)
        if (args[i].startsWith ("params="))
        {
            StringArray lines; lines.addLines (File (args[i].fromFirstOccurrenceOf ("=", false, false)).loadFileAsString());
            for (auto& l : lines) if (l.contains ("=")) args.insert (i + 1, l.trim());
        }
    for (auto& a : args)
    {
        const String k = a.upToFirstOccurrenceOf ("=", false, false), v = a.fromFirstOccurrenceOf ("=", false, false);
        if (k == "note") note = v.getIntValue();
        else if (k == "dur") dur = v.getDoubleValue();
        else if (k == "bpm") bpm = v.getDoubleValue();
        else if (k == "pattern") pattern = v;
        else if (k == "params") {}
        else if (set.count (k)) set[k] (p, v.getFloatValue());
        else { printf ("unknown param %s\n", k.toRawUTF8()); return 1; }
    }
    struct E { int pos; bool on; int note; };
    std::vector<E> ev;
    int total;
    if (pattern.isNotEmpty())
    {
        const double beat = sr * 60.0 / bpm;
        StringArray lines; lines.addLines (File (pattern).loadFileAsString());
        double end = 0;
        for (auto& l : lines)
        {
            auto t = StringArray::fromTokens (l, " \t", ""); t.removeEmptyStrings();
            if (t.size() < 3 || t[0].startsWith ("#")) continue;
            const double s = t[0].getDoubleValue(), len = t[1].getDoubleValue();
            ev.push_back ({ (int) (s * beat), true, t[2].getIntValue() });
            ev.push_back ({ (int) ((s + len) * beat), false, t[2].getIntValue() });
            end = jmax (end, s + len);
        }
        total = (int) (end * beat + sr * 0.6);
    }
    else
    {
        ev.push_back ({ 0, true, note });
        ev.push_back ({ (int) (dur * sr), false, note });
        total = (int) ((dur + 0.4) * sr);
    }
    std::sort (ev.begin(), ev.end(), [] (const E& a, const E& b) { return a.pos < b.pos || (a.pos == b.pos && ! a.on && b.on); });

    alpha::Engine eng;
    eng.setParams (p);
    eng.prepare (sr, block);
    AudioBuffer<float> out (2, total); out.clear();
    size_t ei = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        AudioBuffer<float> buf (2, block); buf.clear(); MidiBuffer midi;
        while (ei < ev.size() && ev[ei].pos < pos + block)
        {
            const auto& e = ev[ei++];
            midi.addEvent (e.on ? MidiMessage::noteOn (1, e.note, (uint8) 110) : MidiMessage::noteOff (1, e.note), jmax (0, e.pos - pos));
        }
        eng.process (buf, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, jmin (block, total - pos));
    }
    const float pk = out.getMagnitude (0, total);
    File f (argv[1]); f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (out, 0, total);
    printf ("peak %.3f%s\n", pk, (std::isfinite (pk) ? "" : " NAN!"));
    return std::isfinite (pk) ? 0 : 2;
}
