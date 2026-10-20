#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using namespace doomsday;
namespace fm = doomsday::fmt;

AudioProcessorValueTreeState::ParameterLayout MirrorProcessor::createLayout()
{
    using namespace ParamIDs;
    using make::f; using make::b; using make::c; using make::skew;
    AudioProcessorValueTreeState::ParameterLayout l;
    for (int i = 0; i < numBands; ++i)
    {
        const String n = "Band " + String (i + 1) + " ";
        l.add (b (band (i, on), n + "On", false));
        l.add (c (band (i, type), n + "Type", Choices::types, Choices::defaultType[i]));
        l.add (c (band (i, slope), n + "Slope", Choices::slopes, 1));
        l.add (f (band (i, freq), n + "Freq", skew (20.f, 20000.f, 640.f), Choices::defaultFreq[i], fm::hz, fm::hzIn));
        l.add (f (band (i, gain), n + "Gain", gainRange(), 0.f, fm::db, fm::num));
        l.add (f (band (i, q), n + "Q", skew (0.1f, 18.f, 1.f), 0.9f, [] (float v, int) { return String (v, 2); }, fm::num));
        l.add (c (band (i, mode), n + "Mode", Choices::modes, 0));
        l.add (b (band (i, dyn), n + "Dynamic", false));
        l.add (f (band (i, thresh), n + "Threshold", { -60.f, 0.f }, -24.f, fm::db, fm::num));
    }
    l.add (f (amount, "Amount", { 0.f, 1.5f }, 1.f));
    l.add (f (output, "Output", { -24.f, 12.f }, 0.f, fm::db, fm::num));
    l.add (f (monoBass, "Mono Bass", skew (0.f, 500.f, 150.f), 0.f, [] (float v, int) { return v < 20.f ? String ("OFF") : fm::hz (v, 0); }, fm::hzIn));
    l.add (c (analyzer, "Analyzer", Choices::analyzers, 2));
    return l;
}

// ------------------------------------------------------------ factory presets
namespace
{
    using namespace Choices;
    struct PB
    {
        String name; PresetValues v;
        PB& bd (int i, int t, float hz, float gn = 0.f, float qq = 0.9f, int sl = 1, int md = 0, bool dy = false, float thr = -24.f)
        {
            using namespace ParamIDs;
            v.push_back ({ band (i, on), 1.f }); v.push_back ({ band (i, type), (float) t }); v.push_back ({ band (i, freq), hz });
            v.push_back ({ band (i, gain), gn }); v.push_back ({ band (i, q), qq }); v.push_back ({ band (i, slope), (float) sl });
            v.push_back ({ band (i, mode), (float) md }); v.push_back ({ band (i, dyn), dy ? 1.f : 0.f }); v.push_back ({ band (i, thresh), thr });
            return *this;
        }
        PB& g (const char* id, float x) { v.push_back ({ id, x }); return *this; }
    };
    enum { ST = 0, MID = 1, SIDE = 2 };

    const std::vector<PB>& presets()
    {
        static const std::vector<PB> list = []
        {
            std::vector<PB> L;
            auto add = [&L] (const char* n) -> PB& { L.push_back (PB { n, {} }); return L.back(); };
            add ("Flat");
            // KICK
            add ("KICK - Punch Focus").bd (0, LowCut, 25).bd (2, Bell, 55, 3.f, 1.f).bd (3, Bell, 300, -3.f, 1.2f).bd (5, Bell, 3500, 2.5f, 1.f);
            add ("KICK - Clean Tail").bd (0, LowCut, 30).bd (2, Bell, 200, -4.f, 1.f).bd (3, Bell, 600, -2.f, 1.4f).bd (6, HighShelf, 8000, -2.f, 0.7f);
            add ("KICK - Raw Crunch").bd (0, LowCut, 28).bd (3, Bell, 400, -3.f, 1.2f).bd (4, Bell, 1200, 3.f, 1.f).bd (5, Bell, 4000, 2.f, 1.2f);
            add ("KICK - Sub Weight").bd (0, LowCut, 22, 0, 0.7f, 2).bd (1, LowShelf, 60, 3.f, 0.7f).bd (3, Bell, 250, -2.f, 1.f).bd (7, HighCut, 14000);
            // HIHAT
            add ("HIHAT - Crisp").bd (0, LowCut, 400).bd (5, Bell, 3000, -2.f, 1.5f).bd (6, HighShelf, 10000, 3.f, 0.7f);
            add ("HIHAT - Soft").bd (0, LowCut, 600).bd (5, Bell, 7000, -3.f, 1.2f).bd (7, HighCut, 13000);
            add ("HIHAT - Air").bd (0, LowCut, 700, 0, 0.7f, 2).bd (3, Bell, 1000, -2.f, 1.f).bd (6, HighShelf, 12000, 4.5f, 0.7f);
            add ("HIHAT - Tame Harsh").bd (0, LowCut, 500).bd (5, Bell, 5000, -5.f, 3.f, 1, ST, true, -30.f);
            // SNARE / CLAP
            add ("SNARE - Body").bd (0, LowCut, 120).bd (2, Bell, 220, 3.f, 1.2f).bd (3, Bell, 800, -2.f, 1.4f).bd (5, Bell, 5000, 2.f, 1.f);
            add ("SNARE - Crack").bd (0, LowCut, 150).bd (3, Bell, 450, -3.f, 1.4f).bd (4, Bell, 2500, 4.f, 1.f).bd (6, HighShelf, 9000, 2.f, 0.7f);
            add ("CLAP - Wide").bd (0, LowCut, 200).bd (3, Bell, 700, -2.f, 1.2f).bd (6, HighShelf, 6000, 3.f, 0.7f, 1, SIDE);
            add ("SNARE - Box Fix").bd (0, LowCut, 100).bd (3, Bell, 500, -5.f, 2.f, 1, ST, true, -28.f);
            // BASS
            add ("BASS - Clean Low").bd (0, LowCut, 30).bd (2, Bell, 250, -3.f, 1.f).bd (7, HighCut, 8000).g (ParamIDs::monoBass, 150);
            add ("BASS - Growl").bd (0, LowCut, 35).bd (2, Bell, 300, -2.f, 1.2f).bd (4, Bell, 800, 3.f, 1.f).bd (5, Bell, 2000, 2.f, 1.2f);
            add ("BASS - Sub Only").bd (2, Bell, 55, 2.f, 1.f).bd (7, HighCut, 150, 0, 0.7f, 1).g (ParamIDs::monoBass, 200);
            add ("BASS - Kick Space").bd (0, LowCut, 35).bd (1, Bell, 50, -3.f, 1.2f).bd (3, Bell, 180, 1.5f, 1.f).g (ParamIDs::monoBass, 120);
            // LEAD / SCREECH
            add ("LEAD - Presence").bd (0, LowCut, 200).bd (2, Bell, 400, -2.f, 1.f).bd (4, Bell, 3000, 3.f, 1.f).bd (6, HighShelf, 10000, 2.f, 0.7f);
            add ("SCREECH - Tame").bd (0, LowCut, 150).bd (4, Bell, 3500, -6.f, 4.f, 1, ST, true, -30.f).bd (5, Bell, 6000, -4.f, 3.f, 1, ST, true, -32.f);
            add ("LEAD - Wide Air").bd (0, LowCut, 180).bd (4, Bell, 2500, -2.f, 1.f, 1, MID).bd (6, HighShelf, 7000, 4.f, 0.7f, 1, SIDE);
            add ("LEAD - Mid Push").bd (0, LowCut, 150).bd (4, Bell, 1500, 3.5f, 0.9f);
            // MELODY / MIDS
            add ("MELODY - Clear Mids").bd (0, LowCut, 250).bd (2, Bell, 450, -3.f, 1.f).bd (4, Bell, 2000, 2.f, 1.f);
            add ("PLUCK - Snap").bd (0, LowCut, 300).bd (5, Bell, 4000, 3.f, 1.2f).bd (6, HighShelf, 11000, 2.f, 0.7f);
            add ("PAD - Smooth").bd (0, LowCut, 200).bd (5, Bell, 3000, -2.f, 1.f).bd (6, HighShelf, 6000, 2.f, 0.7f, 1, SIDE).bd (7, HighCut, 9000, 0, 0.7f, 0);
            add ("CHORDS - Vocal Room").bd (0, LowCut, 200).bd (4, Bell, 2500, -3.f, 1.f, 1, MID).bd (6, HighShelf, 9000, 1.5f, 0.7f, 1, SIDE);
            return L;
        }();
        return list;
    }
}

int MirrorProcessor::getNumPrograms() { return (int) presets().size(); }
const String MirrorProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : String(); }
void MirrorProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    presetName = presets()[(size_t) i].name;
    PresetIO::apply (*this, apvts, presets()[(size_t) i].v);
}
bool MirrorProcessor::loadUserPresetFile (const File& f)
{
    if (! PresetIO::load (*this, apvts, f)) return false;
    presetName = f.getFileNameWithoutExtension();
    return true;
}
bool MirrorProcessor::saveUserPresetAs (const String& name)
{
    String clean;
    if (! io.save (apvts, name, clean)) return false;
    presetName = clean;
    return true;
}

MirrorProcessor::MirrorProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true).withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MIRROR", createLayout())
{
    setCurrentProgram (0);
}

void MirrorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (gather());
    engine.prepare (sampleRate, samplesPerBlock);
}

EqParams MirrorProcessor::gather() const
{
    using namespace ParamIDs;
    EqParams e;
    for (int i = 0; i < numBands; ++i)
    {
        auto& B = e.band[(size_t) i];
        B.on = get (band (i, on)) > 0.5f;
        B.type = jlimit (0, 5, (int) std::round (get (band (i, type))));
        B.slope = jlimit (0, 3, (int) std::round (get (band (i, slope))));
        B.freq = get (band (i, freq)); B.gainDb = get (band (i, gain)); B.q = get (band (i, q));
        B.mode = jlimit (0, 2, (int) std::round (get (band (i, mode))));
        B.dyn = get (band (i, dyn)) > 0.5f; B.threshDb = get (band (i, thresh));
    }
    e.amount = get (amount); e.outDb = get (output); e.monoBassHz = get (monoBass);
    return e;
}

void MirrorProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
    engine.setParams (gather());
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
}

AudioProcessorEditor* MirrorProcessor::createEditor() { return new MirrorEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MirrorProcessor(); }
