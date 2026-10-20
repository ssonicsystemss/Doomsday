#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using namespace doomsday;
namespace fm = doomsday::fmt;

namespace
{
    String fmtSec (float v, int) { return v >= 19.9f ? String ("20 s") : (v < 1.f ? String ((int) std::round (v * 1000.f)) + " ms" : String (v, 2) + " s"); }
    float  secIn (const String& t) { auto v = fm::num (t); return t.containsIgnoreCase ("ms") ? v / 1000.f : v; }
    String fmtMono (float v, int) { return v < 20.f ? String ("OFF") : fm::hz (v, 0); }
    String fmtWidth (float v, int) { return String ((int) std::round (v * 100.f)) + "%"; }
}

AudioProcessorValueTreeState::ParameterLayout AbyssProcessor::createLayout()
{
    using namespace ParamIDs;
    using make::f; using make::b; using make::c; using make::skew;
    AudioProcessorValueTreeState::ParameterLayout l;
    // DELAY
    l.add (b (dlyOn, "Delay On", true));
    l.add (c (dlyMode, "Delay Mode", Choices::modes, 1));
    l.add (b (dlySync, "Delay Sync", true));
    l.add (c (dlyDiv, "Delay Time (Sync)", Choices::divisions, 6));
    l.add (f (dlyMs, "Delay Time (ms)", skew (1.f, 2000.f, 250.f), 300.f, fm::ms, fm::msIn));
    l.add (f (dlyFb, "Delay Feedback", { 0.f, 0.95f }, 0.35f));
    l.add (f (dlyLo, "Delay Low Cut", skew (20.f, 2000.f, 250.f), 250.f, fm::hz, fm::hzIn));
    l.add (f (dlyHi, "Delay High Cut", skew (1000.f, 20000.f, 6000.f), 7000.f, fm::hz, fm::hzIn));
    l.add (f (dlyMod, "Delay Wobble", { 0.f, 1.f }, 0.15f));
    l.add (f (dlyDrive, "Delay Drive", { 0.f, 1.f }, 0.f));
    l.add (f (dlyDuck, "Delay Ducking", { 0.f, 1.f }, 0.4f));
    l.add (f (dlyMix, "Delay Mix", { 0.f, 1.f }, 0.18f));
    // REVERB
    l.add (b (revOn, "Reverb On", true));
    l.add (f (revPre, "Pre-Delay", { 0.f, 250.f }, 20.f, fm::ms, fm::msIn));
    l.add (f (revSize, "Size", { 0.f, 1.f }, 0.6f));
    l.add (f (revDecay, "Decay", skew (0.2f, 20.f, 2.5f), 2.5f, fmtSec, secIn));
    l.add (f (revDamp, "Damping (High Cut)", skew (1000.f, 20000.f, 6000.f), 8000.f, fm::hz, fm::hzIn));
    l.add (f (revLo, "Reverb Low Cut", skew (20.f, 1000.f, 200.f), 200.f, fm::hz, fm::hzIn));
    l.add (f (revMod, "Reverb Motion", { 0.f, 1.f }, 0.4f));
    l.add (f (revShim, "Shimmer", { 0.f, 1.f }, 0.f));
    l.add (f (revDuck, "Reverb Ducking", { 0.f, 1.f }, 0.3f));
    l.add (f (revWidth, "Reverb Width", { 0.f, 1.5f }, 1.f, fmtWidth, fm::pctIn));
    l.add (f (revMix, "Reverb Mix", { 0.f, 1.f }, 0.22f));
    l.add (b (freeze, "Freeze", false));
    l.add (f (d2r, "Delay > Reverb", { 0.f, 1.f }, 0.2f));
    // STEREO / OUTPUT
    l.add (f (width, "Width", { 0.f, 2.f }, 1.f, fmtWidth, fm::pctIn));
    l.add (f (spread, "Spread", { 0.f, 1.f }, 0.f));
    l.add (f (monoBass, "Mono Bass", skew (0.f, 500.f, 150.f), 0.f, fmtMono, fm::hzIn));
    l.add (f (mix, "Mix", { 0.f, 1.f }, 1.f));
    l.add (b (dry, "Dry", true));
    l.add (f (output, "Output", { -24.f, 12.f }, 0.f, fm::db, fm::num));
    return l;
}

// ------------------------------------------------------------ factory presets
namespace
{
    struct PB { String name; PresetValues v; };
    // compact builders: delay / reverb / stereo
    struct Dly { int mode = 1; int div = 6; float ms = -1.f, fb = 0.35f, lo = 250.f, hi = 7000.f, mod = 0.15f, drive = 0.f, duck = 0.4f, mix = 0.18f; };
    struct Rev { float pre = 20.f, size = 0.6f, decay = 2.5f, damp = 8000.f, lo = 200.f, mod = 0.4f, shim = 0.f, duck = 0.3f, width = 1.f, mix = 0.22f; bool freeze = false; };
    struct St  { float width = 1.f, spread = 0.f, mono = 0.f, mix = 1.f, out = 0.f, d2r = 0.2f; };

    PB make (const char* name, const Dly* d, const Rev* r, St s = {})
    {
        using namespace ParamIDs;
        PB p { name, {} };
        auto put = [&p] (const char* id, float v) { p.v.push_back ({ id, v }); };
        put (dlyOn, d != nullptr ? 1.f : 0.f);
        if (d != nullptr)
        {
            put (dlyMode, (float) d->mode); put (dlySync, d->ms < 0.f ? 1.f : 0.f); put (dlyDiv, (float) d->div);
            if (d->ms >= 0.f) put (dlyMs, d->ms);
            put (dlyFb, d->fb); put (dlyLo, d->lo); put (dlyHi, d->hi); put (dlyMod, d->mod); put (dlyDrive, d->drive);
            put (dlyDuck, d->duck); put (dlyMix, d->mix);
        }
        put (revOn, r != nullptr ? 1.f : 0.f);
        if (r != nullptr)
        {
            put (revPre, r->pre); put (revSize, r->size); put (revDecay, r->decay); put (revDamp, r->damp); put (revLo, r->lo);
            put (revMod, r->mod); put (revShim, r->shim); put (revDuck, r->duck); put (revWidth, r->width); put (revMix, r->mix);
            put (freeze, r->freeze ? 1.f : 0.f);
        }
        put (width, s.width); put (spread, s.spread); put (monoBass, s.mono); put (mix, s.mix); put (output, s.out); put (d2r, s.d2r);
        return p;
    }
    enum Div { D32, D16T, D16, D16D, D8T, D8, D8D, D4T, D4, D4D, D2, D1 };
    enum Mode { STEREO, PING, WIDE };

    const std::vector<PB>& presets()
    {
        static const std::vector<PB> list = []
        {
            std::vector<PB> L;
            auto d = [] (Dly x) { return x; };
            auto r = [] (Rev x) { return x; };
            auto add = [&L] (const char* n, std::optional<Dly> dl, std::optional<Rev> rv, St s = {})
            { L.push_back (make (n, dl ? &*dl : nullptr, rv ? &*rv : nullptr, s)); };

            add ("Default Space", d ({ PING, D8D, -1, 0.35f, 250, 7000, 0.15f, 0, 0.3f, 0.2f }), r ({ 20, 0.6f, 2.5f, 8000, 200, 0.4f, 0, 0.2f, 1.f, 0.28f }));
            // LEADS / SCREECHES
            add ("LEAD - Euphoric Hall", d ({ PING, D8D, -1, 0.35f, 300, 6500, 0.2f, 0, 0.4f, 0.2f }),
                                         r ({ 25, 0.8f, 3.5f, 8000, 250, 0.5f, 0, 0.25f, 1.1f, 0.34f }), St { 1.f, 0.2f, 0, 1, 0, 0.3f });
            add ("LEAD - Drop Ducked", d ({ PING, D4, -1, 0.4f, 350, 6000, 0.15f, 0, 0.85f, 0.2f }),
                                       r ({ 15, 0.7f, 2.4f, 7500, 300, 0.4f, 0, 0.85f, 1.f, 0.3f }), St { 1.f, 0.f, 0, 1, 0, 0.2f });
            add ("LEAD - Screech Plate", std::nullopt, r ({ 8, 0.42f, 1.5f, 10000, 320, 0.3f, 0, 0.3f, 1.f, 0.3f }));
            add ("LEAD - Wide Doubler", d ({ WIDE, D8, 16, 0.f, 150, 12000, 0.35f, 0, 0, 0.32f }),
                                        r ({ 10, 0.35f, 0.9f, 9000, 300, 0.3f, 0, 0.3f, 1.2f, 0.1f }), St { 1.15f, 0.5f, 150, 1, -1.f, 0 });
            add ("RAW - Screech Ping", d ({ PING, D16, -1, 0.3f, 400, 7000, 0.1f, 0.2f, 0.5f, 0.2f }),
                                      r ({ 5, 0.35f, 1.1f, 9000, 350, 0.3f, 0, 0.5f, 1.f, 0.15f }));
            add ("UPTEMPO - Tight Space", d ({ STEREO, D16, -1, 0.2f, 400, 8000, 0.05f, 0, 0.5f, 0.1f }),
                                          r ({ 0, 0.28f, 0.7f, 9000, 350, 0.2f, 0, 0.5f, 1.f, 0.12f }));
            // MELODY / PADS
            add ("MELODY - Pluck Echo", d ({ PING, D8D, -1, 0.42f, 300, 6000, 0.2f, 0, 0.3f, 0.32f }),
                                        r ({ 20, 0.55f, 2.f, 7500, 250, 0.4f, 0, 0.2f, 1.f, 0.26f }), St { 1.f, 0.f, 0, 1, 0, 0.35f });
            add ("PAD - Endless Wash", std::nullopt, r ({ 40, 0.9f, 6.f, 6500, 200, 0.6f, 0, 0.f, 1.2f, 0.5f }), St { 1.1f, 0.3f, 0, 1, -1.f, 0 });
            add ("BREAKDOWN - Cathedral", d ({ PING, D2, -1, 0.5f, 300, 5000, 0.2f, 0, 0.3f, 0.15f }),
                                          r ({ 45, 1.f, 8.f, 6000, 220, 0.6f, 0, 0.2f, 1.2f, 0.42f }), St { 1.f, 0.2f, 0, 1, -1.5f, 0.5f });
            add ("BREAKDOWN - Shimmer Heaven", std::nullopt, r ({ 35, 0.9f, 7.f, 9000, 300, 0.5f, 0.55f, 0.2f, 1.2f, 0.42f }), St { 1.f, 0.2f, 0, 1, -1.5f, 0 });
            // VOCALS
            add ("VOCAL - Ping Throw", d ({ PING, D4D, -1, 0.45f, 450, 5000, 0.15f, 0, 0.7f, 0.3f }),
                                       r ({ 10, 0.45f, 1.4f, 9000, 300, 0.3f, 0, 0.6f, 1.f, 0.15f }), St { 1.f, 0.f, 0, 1, 0, 0.25f });
            add ("VOCAL - Slap Room", d ({ STEREO, D8, 110, 0.1f, 300, 7000, 0.05f, 0, 0.3f, 0.18f }),
                                      r ({ 5, 0.3f, 0.8f, 8000, 250, 0.2f, 0, 0.4f, 1.f, 0.14f }));
            // DRUMS
            add ("KICK - Short Room", std::nullopt, r ({ 0, 0.22f, 0.45f, 6000, 450, 0.1f, 0, 0.6f, 0.6f, 0.1f }), St { 1.f, 0.f, 150, 1, 0, 0 });
            add ("SNARE - Big Plate", std::nullopt, r ({ 5, 0.5f, 1.8f, 11000, 300, 0.3f, 0, 0.3f, 1.f, 0.3f }));
            add ("CLAP - Wide Hall", std::nullopt, r ({ 15, 0.7f, 2.2f, 9000, 350, 0.4f, 0, 0.2f, 1.3f, 0.32f }), St { 1.f, 0.5f, 0, 1, 0, 0 });
            // FX
            add ("FX - Dub Echo", d ({ STEREO, D4D, -1, 0.7f, 300, 3000, 0.4f, 0.5f, 0.3f, 0.35f }),
                                  r ({ 10, 0.5f, 1.6f, 6000, 250, 0.3f, 0, 0.3f, 1.f, 0.12f }), St { 1.f, 0.f, 0, 1, -1.f, 0.3f });
            add ("FX - Tape Wobble", d ({ PING, D8, -1, 0.5f, 250, 5000, 1.f, 0.3f, 0.3f, 0.3f }), std::nullopt);
            add ("FX - Endless Abyss", std::nullopt, r ({ 20, 1.f, 20.f, 7000, 200, 0.6f, 0.2f, 0.f, 1.2f, 0.45f }), St { 1.f, 0.2f, 0, 1, -2.f, 0 });
            // MIX BUS / STEREO TOOLS
            add ("MIX - Glue Room", std::nullopt, r ({ 0, 0.32f, 0.8f, 9000, 300, 0.2f, 0, 0.3f, 1.f, 0.07f }), St { 1.05f, 0.f, 120, 1, 0, 0 });
            add ("MIX - Stereo Widener", std::nullopt, std::nullopt, St { 1.35f, 0.45f, 150, 1, -0.5f, 0 });
            add ("MIX - Mono Bass Fix", std::nullopt, std::nullopt, St { 1.f, 0.f, 180, 1, 0, 0 });
            return L;
        }();
        return list;
    }
}

int AbyssProcessor::getNumPrograms() { return (int) presets().size(); }
const String AbyssProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : String(); }
void AbyssProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    presetName = presets()[(size_t) i].name;
    PresetIO::apply (*this, apvts, presets()[(size_t) i].v);
}
bool AbyssProcessor::loadUserPresetFile (const File& f)
{
    if (! PresetIO::load (*this, apvts, f)) return false;
    presetName = f.getFileNameWithoutExtension();
    return true;
}
bool AbyssProcessor::saveUserPresetAs (const String& name)
{
    String clean;
    if (! io.save (apvts, name, clean)) return false;
    presetName = clean;
    return true;
}

AbyssProcessor::AbyssProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true).withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ABYSS", createLayout())
{
    setCurrentProgram (0);
}

void AbyssProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (gather());
    engine.prepare (sampleRate, samplesPerBlock);
}

EchoParams AbyssProcessor::gather() const
{
    using namespace ParamIDs;
    EchoParams e;
    auto on = [this] (const char* id) { return get (id) > 0.5f; };
    e.dlyOn = on (dlyOn);
    e.dly.mode = jlimit (0, 2, (int) std::round (get (dlyMode)));
    const double beats = Choices::divisionBeats[jlimit (0, 11, (int) std::round (get (dlyDiv)))];
    e.dly.timeMs = on (dlySync) ? (float) (beats * 60000.0 / jlimit (40.0, 300.0, bpm.load())) : get (dlyMs);
    e.dly.feedback = get (dlyFb); e.dly.lowCutHz = get (dlyLo); e.dly.highCutHz = get (dlyHi);
    e.dly.mod = get (dlyMod); e.dly.drive = get (dlyDrive);
    e.dlyDuck = get (dlyDuck); e.dlyMix = get (dlyMix);
    e.revOn = on (revOn);
    e.rev.preDelayMs = get (revPre); e.rev.size = get (revSize); e.rev.decayS = get (revDecay); e.rev.dampHz = get (revDamp);
    e.rev.lowCutHz = get (revLo); e.rev.mod = get (revMod); e.rev.shimmer = get (revShim); e.rev.width = get (revWidth);
    e.rev.freeze = on (freeze);
    e.revDuck = get (revDuck); e.revMix = get (revMix); e.d2r = get (d2r);
    e.width = get (width); e.spread = get (spread); e.monoBassHz = get (monoBass); e.mix = get (mix); e.dry = on (dry); e.outDb = get (output);
    return e;
}

void AbyssProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm.store (*b);
    engine.setParams (gather());
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
}

AudioProcessorEditor* AbyssProcessor::createEditor() { return new AbyssEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AbyssProcessor(); }
