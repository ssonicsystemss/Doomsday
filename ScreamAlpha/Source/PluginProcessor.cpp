#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// ------------------------------------------------------------ parameters (ID = engine field name; never rename)
namespace
{
    String fmtHz (float v, int)  { return v >= 1000.f ? String (v / 1000.f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; }
    String fmtMs (float v, int)  { return v >= 1000.f ? String (v / 1000.f, 2) + " s" : (v < 10.f ? String (v, 1) : String (roundToInt (v))) + " ms"; }
    String fmtDb (float v, int)  { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }
    String fmtPct (float v, int) { return String (roundToInt (v * 100.f)) + "%"; }
    String fmtSt (float v, int)  { return (v > 0.05f ? "+" : "") + String (v, 1) + " st"; }
    String fmtCt (float v, int)  { return String (roundToInt (v)) + " ct"; }
    String fmtX (float v, int)   { return String (v, 2) + "x"; }
    String fmtNum (float v, int) { return String (v, 2); }
    float parse (const String& t) { auto v = t.retainCharacters ("0123456789.-+").getFloatValue(); return t.containsIgnoreCase ("k") ? v * 1000.f : v; }
    float parsePct (const String& t) { return parse (t) / 100.f; }

    struct Def { const char* id; const char* name; float lo, hi, def, centre; String (*fmt) (float, int); bool pct = false; };
    const Def floats[] {
        { "scoopSt", "Scoop", -24.f, 24.f, 7.f, 0.f, fmtSt },
        { "scoopMs", "Scoop Time", 5.f, 2000.f, 70.f, 150.f, fmtMs },
        { "scoopCurve", "Scoop Curve", 0.f, 1.f, 0.5f, 0.f, fmtPct, true },
        { "glideMs", "Glide", 0.f, 1000.f, 25.f, 100.f, fmtMs },
        { "tuneSt", "Tune", -12.f, 12.f, 0.f, 0.f, fmtSt },
        { "coreLevel", "Core Level", 0.f, 1.5f, 0.8f, 0.f, fmtPct, true },
        { "coreShape", "Core Shape", 0.f, 1.f, 0.15f, 0.f, fmtPct, true },
        { "hashLevel", "Hash Level", 0.f, 2.f, 0.5f, 0.f, fmtPct, true },
        { "hashDecayMs", "Hash Decay", 10.f, 5000.f, 400.f, 400.f, fmtMs },
        { "hashSustain", "Hash Sustain", 0.f, 1.f, 1.f, 0.f, fmtPct, true },
        { "detuneCents", "Detune", 0.f, 100.f, 25.f, 0.f, fmtCt },
        { "carrierSaw", "Saw", 0.f, 1.f, 0.3f, 0.f, fmtPct, true },
        { "ratio", "FM Ratio", 0.25f, 8.f, 1.f, 1.5f, fmtX },
        { "index", "FM Amount", 0.f, 12.f, 3.f, 0.f, fmtNum },
        { "chaos", "Chaos", 0.f, 1.f, 0.4f, 0.f, fmtPct, true },
        { "chaosHz", "Chaos Speed", 1.f, 100.f, 12.f, 12.f, fmtHz },
        { "grit", "Grit", 0.f, 0.3f, 0.f, 0.f, fmtPct, true },
        { "hashHp", "Hash Low Cut", 20.f, 5000.f, 700.f, 500.f, fmtHz },
        { "drive1Db", "Drive", 0.f, 48.f, 24.f, 0.f, fmtDb },
        { "combMix", "Comb", 0.f, 1.f, 0.f, 0.f, fmtPct, true },
        { "combFb", "Comb Feedback", -0.95f, 0.95f, 0.6f, 0.f, fmtNum },
        { "combRatio", "Comb Tune", 0.25f, 4.f, 1.f, 1.f, fmtX },
        { "formHz", "Formant", 200.f, 8000.f, 1500.f, 1200.f, fmtHz },
        { "formGainDb", "Formant Gain", -12.f, 24.f, 6.f, 0.f, fmtDb },
        { "formQ", "Formant Q", 0.3f, 12.f, 2.f, 2.f, fmtNum },
        { "drive2Db", "Drive 2", 0.f, 36.f, 6.f, 0.f, fmtDb },
        { "toneHz", "Tone", 500.f, 20000.f, 9000.f, 5000.f, fmtHz },
        { "lowCut", "Low Cut", 20.f, 1000.f, 150.f, 150.f, fmtHz },
        { "attackMs", "Attack", 0.5f, 2000.f, 2.f, 50.f, fmtMs },
        { "decayMs", "Decay", 5.f, 5000.f, 300.f, 400.f, fmtMs },
        { "sustain", "Sustain", 0.f, 1.f, 0.9f, 0.f, fmtPct, true },
        { "releaseMs", "Release", 5.f, 5000.f, 80.f, 300.f, fmtMs },
        { "chorus", "Chorus", 0.f, 1.f, 0.f, 0.f, fmtPct, true },
        { "outDb", "Volume", -36.f, 12.f, -6.f, 0.f, fmtDb },
        // FX (after the voice): OTT -> delay -> reverb
        { "ott", "OTT", 0.f, 1.f, 0.f, 0.f, fmtPct, true },
        { "dlyMix", "Delay Mix", 0.f, 1.f, 0.f, 0.f, fmtPct, true },
        { "dlyFb", "Delay Feedback", 0.f, 0.9f, 0.35f, 0.f, fmtPct, true },
        { "revMix", "Reverb Mix", 0.f, 1.f, 0.f, 0.f, fmtPct, true },
        { "revSize", "Reverb Size", 0.f, 1.f, 0.7f, 0.f, fmtPct, true },
        { "revDamp", "Reverb Damp", 0.f, 1.f, 0.45f, 0.f, fmtPct, true },
    };
    const StringArray curves { "SOFT", "HARD", "TUBE", "FOLD", "CUBIC" };
    const StringArray dlyNames { "1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2" };
    const float dlyBeats[] { 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f };

    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "INIT", {} },
    { "HOOVER - Classic Juno", { { "scoopSt", -5.0f }, { "scoopMs", 220.0f }, { "scoopCurve", 0.35f }, { "glideMs", 110.0f }, { "coreLevel", 0.55f }, { "coreShape", 1.0f }, { "coreOct", -1.0f }, { "hashLevel", 0.9f }, { "hashDecayMs", 600.0f }, { "hashSustain", 0.8f }, { "voices", 7.0f }, { "detuneCents", 38.0f }, { "carrierSaw", 1.0f }, { "ratio", 0.5f }, { "index", 0.35f }, { "chaos", 0.3f }, { "chaosHz", 5.0f }, { "grit", 0.0f }, { "hashHp", 90.0f }, { "drive1Db", 9.0f }, { "drive1Curve", 0.0f }, { "combMix", 0.0f }, { "formHz", 900.0f }, { "formGainDb", 3.0f }, { "formQ", 1.0f }, { "drive2Db", 6.0f }, { "drive2Curve", 0.0f }, { "toneHz", 7500.0f }, { "lowCut", 50.0f }, { "attackMs", 3.0f }, { "decayMs", 600.0f }, { "sustain", 0.85f }, { "releaseMs", 120.0f }, { "chorus", 0.85f }, { "outDb", -6.0f }, { "ott", 0.25f }, { "dlyMix", 0.18f }, { "dlyDiv", 2.f }, { "dlyFb", 0.35f }, { "revMix", 0.3f }, { "revSize", 0.75f }, { "revDamp", 0.45f } } },
    { "HOOVER - Rave Dive", { { "scoopSt", 12.0f }, { "scoopMs", 420.0f }, { "scoopCurve", 0.2f }, { "glideMs", 110.0f }, { "coreLevel", 0.55f }, { "coreShape", 1.0f }, { "coreOct", -1.0f }, { "hashLevel", 0.9f }, { "hashDecayMs", 600.0f }, { "hashSustain", 0.8f }, { "voices", 7.0f }, { "detuneCents", 48.0f }, { "carrierSaw", 1.0f }, { "ratio", 0.5f }, { "index", 0.35f }, { "chaos", 0.3f }, { "chaosHz", 5.0f }, { "grit", 0.0f }, { "hashHp", 90.0f }, { "drive1Db", 16.0f }, { "drive1Curve", 0.0f }, { "combMix", 0.0f }, { "formHz", 1300.0f }, { "formGainDb", 7.0f }, { "formQ", 1.5f }, { "drive2Db", 6.0f }, { "drive2Curve", 0.0f }, { "toneHz", 7500.0f }, { "lowCut", 50.0f }, { "attackMs", 3.0f }, { "decayMs", 600.0f }, { "sustain", 0.85f }, { "releaseMs", 120.0f }, { "chorus", 0.7f }, { "outDb", -6.0f }, { "ott", 0.3f }, { "dlyMix", 0.22f }, { "dlyDiv", 3.f }, { "dlyFb", 0.4f }, { "revMix", 0.35f }, { "revSize", 0.8f }, { "revDamp", 0.4f } } },
    { "HOOVER - Hardstyle Scream", { { "scoopSt", 5.0f }, { "scoopMs", 70.0f }, { "scoopCurve", 0.5f }, { "glideMs", 40.0f }, { "coreLevel", 0.55f }, { "coreShape", 0.7f }, { "coreOct", 0.0f }, { "hashLevel", 0.9f }, { "hashDecayMs", 600.0f }, { "hashSustain", 0.8f }, { "voices", 7.0f }, { "detuneCents", 38.0f }, { "carrierSaw", 1.0f }, { "ratio", 2.0f }, { "index", 2.2f }, { "chaos", 0.3f }, { "chaosHz", 5.0f }, { "grit", 0.0f }, { "hashHp", 250.0f }, { "drive1Db", 24.0f }, { "drive1Curve", 1.0f }, { "combMix", 0.35f }, { "formHz", 1200.0f }, { "formGainDb", 10.0f }, { "formQ", 2.0f }, { "drive2Db", 6.0f }, { "drive2Curve", 0.0f }, { "toneHz", 7500.0f }, { "lowCut", 150.0f }, { "attackMs", 3.0f }, { "decayMs", 600.0f }, { "sustain", 0.85f }, { "releaseMs", 120.0f }, { "chorus", 0.5f }, { "outDb", -6.0f }, { "combFb", 0.55f }, { "combRatio", 1.0f }, { "ott", 0.45f }, { "dlyMix", 0.15f }, { "dlyDiv", 2.f }, { "dlyFb", 0.3f }, { "revMix", 0.25f }, { "revSize", 0.65f }, { "revDamp", 0.5f } } },
        };
        return p;
    }
}

AudioProcessorValueTreeState::ParameterLayout AlphaProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& d : floats)
    {
        NormalisableRange<float> r (d.lo, d.hi);
        if (d.centre > d.lo && d.centre < d.hi) r.setSkewForCentre (d.centre);
        auto attrs = AudioParameterFloatAttributes().withStringFromValueFunction (d.fmt)
                         .withValueFromStringFunction (d.pct ? parsePct : parse);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name, r, d.def, attrs));
    }
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "voices", 1 }, "Voices", 1, 7, 5));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "coreOct", 1 }, "Core Octave", StringArray { "-1 OCT", "0", "+1 OCT" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "drive1Curve", 1 }, "Drive Curve", curves, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "drive2Curve", 1 }, "Drive 2 Curve", curves, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "dlyDiv", 1 }, "Delay Time", dlyNames, 2));
    return l;
}

AlphaProcessor::AlphaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "SCREAMALPHA", createLayout())
{
    setCurrentProgram (1);      // start on the classic hoover
}

alpha::Params AlphaProcessor::gather() const
{
    alpha::Params p;
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    p.scoopSt = v ("scoopSt"); p.scoopMs = v ("scoopMs"); p.scoopCurve = v ("scoopCurve"); p.glideMs = v ("glideMs"); p.tuneSt = v ("tuneSt");
    p.coreLevel = v ("coreLevel"); p.coreShape = v ("coreShape"); p.coreOct = roundToInt (v ("coreOct")) - 1;
    p.hashLevel = v ("hashLevel"); p.hashDecayMs = v ("hashDecayMs"); p.hashSustain = v ("hashSustain");
    p.voices = roundToInt (v ("voices")); p.detuneCents = v ("detuneCents"); p.carrierSaw = v ("carrierSaw");
    p.ratio = v ("ratio"); p.index = v ("index"); p.chaos = v ("chaos"); p.chaosHz = v ("chaosHz"); p.grit = v ("grit");
    p.hashHp = v ("hashHp"); p.drive1Db = v ("drive1Db"); p.drive1Curve = roundToInt (v ("drive1Curve"));
    p.combMix = v ("combMix"); p.combFb = v ("combFb"); p.combRatio = v ("combRatio");
    p.formHz = v ("formHz"); p.formGainDb = v ("formGainDb"); p.formQ = v ("formQ");
    p.drive2Db = v ("drive2Db"); p.drive2Curve = roundToInt (v ("drive2Curve")); p.toneHz = v ("toneHz"); p.lowCut = v ("lowCut");
    p.attackMs = v ("attackMs"); p.decayMs = v ("decayMs"); p.sustain = v ("sustain"); p.releaseMs = v ("releaseMs");
    p.chorus = v ("chorus"); p.outDb = v ("outDb");
    return p;
}

int AlphaProcessor::getNumPrograms() { return (int) presets().size(); }
const String AlphaProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? String (presets()[(size_t) i].name) : String(); }

void AlphaProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    currentName = presets()[(size_t) i].name;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm)) rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, val] : presets()[(size_t) i].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (String (id) == "coreOct" ? val + 1.f : val));   // octave stored as -1 / 0 / +1
    undo.clearUndoHistory();
}

bool AlphaProcessor::isBusesLayoutSupported (const BusesLayout& l) const { return l.getMainOutputChannelSet() == AudioChannelSet::stereo(); }

void AlphaProcessor::prepareToPlay (double sr, int block)
{
    engine.setParams (gather());
    engine.prepare (sr, block);
    ott.prepare (sr);
    fx.prepare (sr, block);
}

void AlphaProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals nd;
    buffer.clear();
    engine.setParams (gather());
    engine.process (buffer, midi);

    // FX: OTT -> delay (tempo synced, ping-pong) -> reverb
    double bpm = 150.0;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) bpm = *b;
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    const float ottAmt = v ("ott");
    if (ottAmt > 0.001f) for (int i = 0; i < buffer.getNumSamples(); ++i) ott.process (L[i], R[i], ottAmt);
    doomsday::dsp::FxParams fp;
    fp.dlyMix = v ("dlyMix"); fp.dlyOn = fp.dlyMix > 0.001f; fp.dlyFb = v ("dlyFb"); fp.dlyPP = true;
    fp.dlyBeats = dlyBeats[jlimit (0, 5, roundToInt (v ("dlyDiv")))];
    fp.revMix = v ("revMix"); fp.revOn = fp.revMix > 0.001f; fp.revSize = v ("revSize"); fp.revDamp = v ("revDamp");
    fx.setup (fp, bpm);
    fx.process (L, R, buffer.getNumSamples(), -1.0, 0);
    int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < buffer.getNumSamples(); ++i) { scope[(size_t) w] = 0.5f * (L[i] + R[i]); w = (w + 1) & (scopeSize - 1); }
    scopeWrite.store (w, std::memory_order_release);
}

// ------------------------------------------------------------ RANDOM + user presets
float AlphaProcessor::measureLoudnessDb()
{
    auto prm = gather();
    constexpr double rate = 48000.0; constexpr int block = 512, total = (int) (rate * 0.7), skip = (int) (rate * 0.05);
    double sum = 0.0; long n = 0;
    for (int note : { 62, 74 })
    {
        auto eng = std::make_unique<alpha::Engine>();
        eng->setParams (prm); eng->prepare (rate, block);
        AudioBuffer<float> b (2, block);
        for (int pos = 0; pos < total; pos += block)
        {
            b.clear(); MidiBuffer m;
            if (pos == 0) m.addEvent (MidiMessage::noteOn (1, note, (uint8) 110), 0);
            eng->process (b, m);
            for (int i = 0; i < block; ++i)
                if (pos + i >= skip) { const float v = 0.5f * (b.getSample (0, i) + b.getSample (1, i)); sum += (double) v * v; ++n; }
        }
    }
    return n > 0 ? (float) (10.0 * std::log10 (sum / (double) n + 1e-12)) : -100.f;
}

void AlphaProcessor::randomize()
{
    auto& r = rng;
    auto set = [this] (const char* id, float v)
    { if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); } };
    auto lin = [&r] (float a, float b) { return a + r.nextFloat() * (b - a); };
    auto lg = [&r] (float a, float b) { return a * std::pow (b / a, r.nextFloat()); };
    auto pick = [&r] (std::initializer_list<float> o) { return *(o.begin() + r.nextInt ((int) o.size())); };
    auto chance = [&r] (float pr) { return r.nextFloat() < pr; };

    set ("scoopSt", chance (0.7f) ? lin (2.f, 14.f) : lin (-8.f, -3.f));
    set ("scoopMs", lg (40.f, 500.f)); set ("scoopCurve", lin (0.2f, 0.6f)); set ("glideMs", lg (10.f, 150.f)); set ("tuneSt", 0.f);
    set ("coreLevel", lin (0.3f, 1.f)); set ("coreShape", r.nextFloat());
    set ("coreOct", chance (0.25f) ? 0.f : (chance (0.12f) ? 2.f : 1.f));                       // choice index: 0 = -1 oct
    set ("hashLevel", lin (0.4f, 1.4f)); set ("hashDecayMs", lg (100.f, 2000.f)); set ("hashSustain", lin (0.3f, 1.f));
    set ("voices", (float) (3 + r.nextInt (5))); set ("detuneCents", lin (10.f, 60.f)); set ("carrierSaw", r.nextFloat());
    set ("ratio", pick ({ 0.5f, 1.f, 1.5f, 2.f, 3.f, 4.f })); set ("index", chance (0.3f) ? lin (0.f, 0.6f) : lg (0.6f, 5.f));
    set ("chaos", lin (0.f, 0.6f)); set ("chaosHz", lg (2.f, 30.f)); set ("grit", chance (0.3f) ? lin (0.f, 0.04f) : 0.f);
    set ("hashHp", lg (100.f, 1500.f));
    set ("drive1Db", lin (6.f, 32.f)); set ("drive1Curve", (float) r.nextInt (5));
    set ("combMix", chance (0.4f) ? lin (0.1f, 0.6f) : 0.f); set ("combFb", lin (-0.7f, 0.8f)); set ("combRatio", pick ({ 0.5f, 1.f, 2.f }));
    set ("formHz", lg (500.f, 3000.f)); set ("formGainDb", lin (0.f, 14.f)); set ("formQ", lg (0.7f, 5.f));
    set ("drive2Db", lin (0.f, 12.f)); set ("drive2Curve", pick ({ 0.f, 2.f })); set ("toneHz", lg (5000.f, 14000.f)); set ("lowCut", lg (60.f, 250.f));
    set ("attackMs", lin (0.5f, 5.f)); set ("decayMs", lg (150.f, 2000.f)); set ("sustain", lin (0.6f, 1.f)); set ("releaseMs", lg (40.f, 300.f));
    set ("chorus", chance (0.6f) ? lin (0.2f, 0.85f) : 0.f);
    set ("ott", lin (0.f, 0.6f)); set ("dlyMix", lin (0.f, 0.3f)); set ("dlyDiv", (float) r.nextInt (6)); set ("dlyFb", lin (0.2f, 0.5f));
    set ("revMix", lin (0.1f, 0.4f)); set ("revSize", lin (0.5f, 0.85f)); set ("revDamp", lin (0.3f, 0.6f));

    // level: every random sound about as loud as the presets (2 passes: the drives are not linear)
    set ("outDb", 0.f);
    for (int pass = 0; pass < 2; ++pass)
    {
        const float lvl = measureLoudnessDb();
        if (lvl < -90.f) break;
        set ("outDb", jlimit (-24.f, 12.f, apvts.getRawParameterValue ("outDb")->load() + (randomTargetDb - lvl)));
    }
    currentName = "Random #" + String (1000 + r.nextInt (9000));
    currentProgram = -1;
}

File AlphaProcessor::userFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("5CR34M 4LPH4").getChildFile ("Presets");
}

Array<File> AlphaProcessor::userPresets() const
{
    auto f = userFolder().findChildFiles (File::findFiles, false, "*.sapreset");
    f.sort();
    return f;
}

bool AlphaProcessor::saveUserPreset (const String& name)
{
    const auto clean = File::createLegalFileName (name.trim());
    if (clean.isEmpty() || ! userFolder().createDirectory()) return false;
    auto st = apvts.copyState();
    if (auto xml = st.createXml())
        if (xml->writeTo (userFolder().getChildFile (clean + ".sapreset"))) { currentName = clean; currentProgram = -1; return true; }
    return false;
}

bool AlphaProcessor::loadUserPreset (const File& f)
{
    auto xml = XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm)) rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto* c : xml->getChildWithTagNameIterator ("PARAM"))
        if (auto* rp = apvts.getParameter (c->getStringAttribute ("id")))
            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) c->getDoubleAttribute ("value")));
    currentName = f.getFileNameWithoutExtension(); currentProgram = -1;
    undo.clearUndoHistory();
    return true;
}

AudioProcessorEditor* AlphaProcessor::createEditor() { return new AlphaEditor (*this); }

void AlphaProcessor::getStateInformation (MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("program", currentProgram, nullptr);
    st.setProperty ("presetName", currentName, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void AlphaProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = ValueTree::fromXml (*xml);
            currentProgram = vt.getProperty ("program", 0);
            currentName = vt.getProperty ("presetName", "INIT").toString();
            apvts.replaceState (vt);
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AlphaProcessor(); }
