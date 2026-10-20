#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using namespace doomsday;
namespace fm = doomsday::fmt;

namespace
{
    String fmtRatio (float v, int) { return v >= 19.9f ? String ("INF : 1") : String (v, v < 10.f ? 1 : 0) + " : 1"; }
    String fmtUp (float v, int) { return v <= 1.01f ? String ("OFF") : String (v, 1) + " : 1"; }
    String fmtWidth (float v, int) { return String ((int) std::round (v * 100.f)) + "%"; }
    String fmtMono (float v, int) { return v < 20.f ? String ("OFF") : fm::hz (v, 0); }
    String fmtDbTp (float v, int) { return String (v, 1) + " dBTP"; }
    String fmtTilt (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }
}

AudioProcessorValueTreeState::ParameterLayout TitanProcessor::createLayout()
{
    using namespace ParamIDs;
    using make::f; using make::b; using make::c; using make::skew;
    AudioProcessorValueTreeState::ParameterLayout l;
    for (int i = 0; i < numBands; ++i)
    {
        const String n = Choices::bandNames[i] + " ";
        l.add (b (band (i, on), n + "On", true));
        l.add (b (band (i, solo), n + "Solo", false));
        l.add (f (band (i, pre), n + "Pre Gain", { -12.f, 12.f }, 0.f, fm::db, fm::num));
        l.add (f (band (i, thresh), n + "Threshold", { -60.f, 0.f }, -18.f, fm::db, fm::num));
        l.add (f (band (i, ratio), n + "Ratio", skew (1.f, 20.f, 4.f), 2.f, fmtRatio, fm::num));
        l.add (f (band (i, up), n + "Upward Ratio", skew (1.f, 6.f, 2.f), 1.f, fmtUp, fm::num));
        l.add (f (band (i, knee), n + "Knee", { 0.f, 18.f }, 6.f, fm::db, fm::num));
        l.add (f (band (i, attack), n + "Attack", skew (0.1f, 200.f, 15.f), i == 0 ? 20.f : 10.f, fm::ms, fm::msIn));
        l.add (f (band (i, release), n + "Release", skew (5.f, 1000.f, 150.f), i == 0 ? 150.f : 100.f, fm::ms, fm::msIn));
        l.add (f (band (i, sat), n + "Saturation", { 0.f, 1.f }, 0.f));
        l.add (f (band (i, width), n + "Width", { 0.f, 2.f }, 1.f, fmtWidth, fm::pctIn));
        l.add (f (band (i, post), n + "Post Gain", { -12.f, 12.f }, 0.f, fm::db, fm::num));
    }
    l.add (f (xoLow, "Crossover Low", skew (40.f, 800.f, 160.f), 150.f, fm::hz, fm::hzIn));
    l.add (f (xoHigh, "Crossover High", skew (1000.f, 12000.f, 3500.f), 3500.f, fm::hz, fm::hzIn));
    l.add (f (input, "Input", { -12.f, 12.f }, 0.f, fm::db, fm::num));
    l.add (f (tilt, "Tilt", { -6.f, 6.f }, 0.f, fmtTilt, fm::num));
    l.add (f (monoBass, "Mono Bass", skew (0.f, 500.f, 150.f), 0.f, fmtMono, fm::hzIn));
    l.add (f (clip, "Clip", { 0.f, 1.f }, 0.f));
    l.add (f (limit, "Limit (Drive)", { 0.f, 18.f }, 0.f, fm::db, fm::num));
    l.add (f (ceiling, "Ceiling", { -3.f, 0.f }, -0.3f, fmtDbTp, fm::num));
    l.add (f (limRel, "Limiter Release", skew (10.f, 1000.f, 150.f), 200.f, fm::ms, fm::msIn));
    l.add (b (match, "Match Loudness", false));
    l.add (c (target, "Loudness Target", Choices::targets, 3));
    return l;
}

// ------------------------------------------------------------ factory presets
namespace
{
    struct PB { String name; PresetValues v; };
    // one band: thresh, ratio, up, knee, att, rel, sat, width, pre, post
    struct Bd { float th = -18.f, ra = 2.f, up = 1.f, knee = 6.f, att = 10.f, rel = 100.f, sat = 0.f, width = 1.f, pre = 0.f, post = 0.f; bool on = true; };
    struct Ms { float limit = 0.f, clip = 0.f, ceiling = -0.3f, rel = 200.f, tilt = 0.f, mono = 0.f, xoLo = 150.f, xoHi = 3500.f, input = 0.f; int target = 3; };

    PB make (const char* name, Bd lo, Bd mid, Bd hi, Ms m)
    {
        using namespace ParamIDs;
        PB p { name, {} };
        auto put = [&p] (const String& id, float v) { p.v.push_back ({ id, v }); };
        const Bd* bs[3] = { &lo, &mid, &hi };
        for (int i = 0; i < 3; ++i)
        {
            const Bd& b = *bs[i];
            put (band (i, on), b.on ? 1.f : 0.f); put (band (i, thresh), b.th); put (band (i, ratio), b.ra); put (band (i, up), b.up);
            put (band (i, knee), b.knee); put (band (i, attack), b.att); put (band (i, release), b.rel); put (band (i, sat), b.sat);
            put (band (i, width), b.width); put (band (i, pre), b.pre); put (band (i, post), b.post);
        }
        put (limit, m.limit); put (clip, m.clip); put (ceiling, m.ceiling); put (limRel, m.rel); put (tilt, m.tilt); put (monoBass, m.mono);
        put (xoLow, m.xoLo); put (xoHigh, m.xoHi); put (input, m.input); put (target, (float) m.target);
        return p;
    }

    const std::vector<PB>& presets()
    {
        static const std::vector<PB> list = []
        {
            std::vector<PB> L;
            auto add = [&L] (const char* n, Bd lo, Bd mid, Bd hi, Ms m) { L.push_back (make (n, lo, mid, hi, m)); };
            Bd off; off.on = false;
            // name                              LOW                                              MID                                               HIGH                                             MASTER
            add ("Default Master",               { -18, 2.f, 1.f, 6, 20, 150 },                   { -18, 2.f, 1.f, 6, 10, 100 },                    { -18, 2.f, 1.f, 6, 5, 80 },                     { 2.0f, 0.0f, -0.3f, 200 });
            add ("HARDSTYLE - Club Loud",        { -16, 3.f, 1.f, 6, 25, 120, 0.1f, 0.f },        { -20, 2.5f, 1.3f, 6, 8, 90, 0.15f, 1.05f },      { -22, 2.f, 1.5f, 6, 4, 70, 0.1f, 1.15f },       { 5.0f, 0.3f, -0.3f, 170, 0.5f, 120, 140, 3800, 0, 3 });
            add ("RAWSTYLE - Brutal",            { -14, 4.f, 1.f, 6, 20, 100, 0.15f, 0.f },       { -22, 3.f, 1.5f, 6, 5, 70, 0.35f, 1.f },         { -24, 2.5f, 1.8f, 6, 3, 60, 0.2f, 1.1f },       { 7.0f, 0.5f, -0.3f, 140, 0.f, 140, 150, 3500, 0, 4 });
            add ("EUPHORIC - Wide & Clear",      { -16, 2.5f, 1.f, 6, 25, 140, 0.f, 0.f },        { -20, 2.f, 1.3f, 8, 10, 110, 0.05f, 1.12f },     { -24, 2.f, 1.8f, 8, 5, 90, 0.05f, 1.3f },       { 4.0f, 0.2f, -0.3f, 200, 0.8f, 150, 150, 4500, 0, 3 });
            add ("UPTEMPO - Smash",              { -12, 5.f, 1.f, 4, 12, 70, 0.25f, 0.f },        { -20, 4.f, 1.6f, 4, 3, 50, 0.3f, 1.f },          { -22, 3.f, 1.8f, 4, 2, 40, 0.2f, 1.1f },        { 8.0f, 0.6f, -0.3f, 120, 0.3f, 150, 160, 3500, 0, 4 });
            add ("FRENCHCORE - Dense Wall",      { -14, 4.f, 1.2f, 6, 15, 90, 0.2f, 0.f },        { -22, 3.5f, 2.f, 6, 5, 60, 0.25f, 1.05f },       { -24, 3.f, 2.f, 6, 3, 50, 0.15f, 1.1f },        { 7.0f, 0.45f, -0.3f, 130, 0.f, 140, 150, 3200, 0, 4 });
            add ("HARDCORE - Punch Keeper",      { -14, 3.f, 1.f, 8, 35, 160, 0.1f, 0.f },        { -18, 2.5f, 1.2f, 8, 15, 110, 0.15f, 1.f },      { -20, 2.f, 1.3f, 8, 8, 80, 0.1f, 1.1f },        { 6.0f, 0.4f, -0.3f, 150, 0.f, 130, 150, 3500, 0, 4 });
            add ("STREAMING - Gentle -1 dBTP",    { -18, 2.f, 1.f, 8, 25, 150 },                   { -20, 1.8f, 1.f, 8, 12, 120 },                   { -22, 1.8f, 1.f, 8, 8, 100 },                   { 1.5f, 0.0f, -1.f, 220, 0.f, 0, 150, 3500, 0, 1 });
            add ("OTT - Hyper Dense",            { -30, 6.f, 3.f, 6, 5, 60, 0.1f, 1.f },          { -30, 6.f, 3.f, 6, 3, 50, 0.1f, 1.f },           { -30, 6.f, 3.f, 6, 2, 40, 0.1f, 1.f },          { 3.0f, 0.2f, -0.3f, 150, 0.f, 0, 120, 2500, -4.f, 0 });
            add ("WARM - Tape Glue",             { -20, 2.f, 1.f, 10, 30, 200, 0.3f, 0.9f },      { -20, 1.8f, 1.f, 10, 20, 160, 0.3f, 1.f },       { -20, 1.6f, 1.f, 10, 15, 140, 0.2f, 1.f },      { 3.0f, 0.2f, -0.3f, 200, -1.f, 100, 150, 4000, 0, 2 });
            add ("BRIGHT - Air Lift",            { -18, 2.f, 1.f, 8, 25, 150, 0.f, 0.9f },        { -20, 2.f, 1.2f, 8, 10, 110, 0.f, 1.05f },       { -26, 2.f, 2.2f, 8, 4, 80, 0.1f, 1.25f },       { 3.5f, 0.2f, -0.3f, 180, 1.5f, 120, 150, 5000, 0, 2 });
            add ("WIDE - Stereo Master",         { -18, 2.f, 1.f, 8, 25, 150, 0.f, 0.f },         { -20, 2.f, 1.1f, 8, 10, 110, 0.f, 1.25f },       { -22, 2.f, 1.3f, 8, 5, 90, 0.f, 1.45f },        { 3.5f, 0.2f, -0.3f, 180, 0.f, 160, 160, 4000, 0, 2 });
            add ("BUS - Kick Punch",             { -16, 4.f, 1.f, 6, 30, 90, 0.15f, 0.f },        { -18, 3.f, 1.f, 6, 15, 80, 0.2f, 0.5f },         { -20, 2.f, 1.f, 6, 5, 60, 0.1f, 0.8f },         { 3.0f, 0.4f, -0.3f, 120, 0.f, 200, 120, 3000, 0, 0 });
            add ("BUS - Lead Glue",              { -24, 2.f, 1.f, 8, 20, 120, 0.f, 1.f, -2.f },   { -20, 2.5f, 1.3f, 8, 8, 100, 0.15f, 1.1f },      { -22, 2.f, 1.5f, 8, 4, 80, 0.1f, 1.2f },        { 2.5f, 0.2f, -0.3f, 150, 0.3f, 150, 200, 4000, 0, 0 });
            add ("BUS - Vocal Smooth",           { -22, 2.f, 1.f, 10, 20, 150, 0.f, 1.f, -2.f },  { -22, 2.5f, 1.2f, 10, 10, 120, 0.05f, 1.f },     { -26, 3.f, 1.f, 10, 2, 60, 0.f, 1.1f },         { 2.0f, 0.0f, -0.5f, 180, 0.f, 0, 250, 6000, 0, 0 });
            add ("BUS - Drum Crunch",            { -18, 3.f, 1.f, 6, 20, 80, 0.3f, 0.f },         { -22, 4.f, 1.5f, 6, 5, 60, 0.4f, 1.f },          { -24, 3.f, 1.5f, 6, 2, 40, 0.3f, 1.f },         { 3.5f, 0.5f, -0.3f, 120, 0.f, 150, 150, 4000, 0, 0 });
            add ("LIMITER ONLY - Clean",         off, off, off,                                                                                                                     { 4.0f, 0.0f, -0.3f, 200, 0.f, 0, 150, 3500, 0, 3 });
            add ("LIMITER ONLY - Clip + Limit",  off, off, off,                                                                                                                     { 6.0f, 0.7f, -0.3f, 160, 0.f, 0, 150, 3500, 0, 3 });
            return L;
        }();
        return list;
    }
}

int TitanProcessor::getNumPrograms() { return (int) presets().size(); }
const String TitanProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : String(); }
void TitanProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    presetName = presets()[(size_t) i].name;
    PresetIO::apply (*this, apvts, presets()[(size_t) i].v);
}
bool TitanProcessor::loadUserPresetFile (const File& f)
{
    if (! PresetIO::load (*this, apvts, f)) return false;
    presetName = f.getFileNameWithoutExtension();
    return true;
}
bool TitanProcessor::saveUserPresetAs (const String& name)
{
    String clean;
    if (! io.save (apvts, name, clean)) return false;
    presetName = clean;
    return true;
}

TitanProcessor::TitanProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true).withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "TITANS", createLayout())
{
    setCurrentProgram (0);
}

void TitanProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (gather());
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.latencySamples());
}

TitanParams TitanProcessor::gather() const
{
    using namespace ParamIDs;
    TitanParams e;
    for (int i = 0; i < numBands; ++i)
    {
        auto& B = e.band[(size_t) i];
        B.on = get (band (i, on)) > 0.5f; B.solo = get (band (i, solo)) > 0.5f;
        B.preDb = get (band (i, pre)); B.postDb = get (band (i, post)); B.sat = get (band (i, sat)); B.width = get (band (i, width));
        B.comp.threshDb = get (band (i, thresh)); B.comp.ratio = get (band (i, ratio)); B.comp.upRatio = get (band (i, up));
        B.comp.kneeDb = get (band (i, knee)); B.comp.attackMs = get (band (i, attack)); B.comp.releaseMs = get (band (i, release));
    }
    e.xoLow = get (xoLow); e.xoHigh = get (xoHigh); e.inputDb = get (input); e.tiltDb = get (tilt); e.monoBassHz = get (monoBass);
    e.clip = get (clip); e.limitDb = get (limit); e.ceilingDb = get (ceiling); e.limReleaseMs = get (limRel); e.match = get (match) > 0.5f;
    return e;
}

void TitanProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
    engine.setParams (gather());
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
}

AudioProcessorEditor* TitanProcessor::createEditor() { return new TitanEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TitanProcessor(); }
