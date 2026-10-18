#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// ------------------------------------------------------------ formatting
namespace
{
    String fmtHz (float v, int)   { return v >= 1000.f ? String (v / 1000.f, 2) + " kHz" : String ((int) std::round (v)) + " Hz"; }
    String fmtMs (float v, int)   { return v >= 1000.f ? String (v / 1000.f, 2) + " s" : String (v, std::abs (v) < 10.f ? 1 : 0) + " ms"; }
    String fmtOff (float v, int)  { return (v > 0.05f ? "+" : "") + String (v, std::abs (v) < 10.f ? 1 : 0) + " ms"; }
    String fmtDb (float v, int)   { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }
    String fmtPct (float v, int)  { return String ((int) std::round (v * 100.f)) + "%"; }
    String fmtSemi (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " st"; }
    String fmtTilt (float v, int) { return std::abs (v) < 0.005f ? String ("FLAT") : (v > 0.f ? "BRIGHT " : "DARK ") + String ((int) std::round (std::abs (v) * 100.f)); }
    float  parseNum (const String& t) { return t.retainCharacters ("0123456789.-+").getFloatValue(); }
    float  parseHz (const String& t)  { auto v = parseNum (t); return t.containsIgnoreCase ("k") ? v * 1000.f : v; }
    float  parseMs (const String& t)  { auto v = parseNum (t); return (t.trim().endsWithIgnoreCase ("s") && ! t.containsIgnoreCase ("ms")) ? v * 1000.f : v; }
    float  parsePct (const String& t) { return parseNum (t) / 100.f; }
    float  parseTilt (const String& t){ auto v = parseNum (t) / 100.f; return t.containsIgnoreCase ("dark") ? -std::abs (v) : v; }

    NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    int versionOf (const String& id)
    {
        return (id.endsWith ("offset") || id.endsWith ("_rev") || id.endsWith ("_forge") || id.startsWith ("f_")) ? 2 : 1;
    }
    std::unique_ptr<AudioParameterFloat> fparam (const String& id, const String& name, NormalisableRange<float> range, float def,
                                                 String (*fmt) (float, int), float (*parse) (const String&))
    {
        auto attrs = AudioParameterFloatAttributes().withStringFromValueFunction (fmt).withValueFromStringFunction (parse);
        return std::make_unique<AudioParameterFloat> (ParameterID { id, versionOf (id) }, name, range, def, attrs);
    }
    std::unique_ptr<AudioParameterBool> bparam (const String& id, const String& name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, versionOf (id) }, name, def);
    }

    struct LayerDefaults { const char* name; float level, envMs, hp, lp; bool forge; };
    const LayerDefaults layerDefaults[NumSampleLayers] {
        { "Punch",  0.f,  150.f, 20.f,  20000.f, false },
        { "Crunch", -6.f, 220.f, 250.f, 12000.f, true },
        { "Kick",   0.f,  800.f, 20.f,  20000.f, true },
    };
}

Envelope AncientProcessor::defaultEnvelope (int layer)
{
    return Envelope::make (layerDefaults[jlimit (0, 2, layer)].envMs);
}

AudioProcessorValueTreeState::ParameterLayout AncientProcessor::createLayout()
{
    using namespace ParamIDs;
    AudioProcessorValueTreeState::ParameterLayout l;

    for (int i = 0; i < NumSampleLayers; ++i)
    {
        const auto& d = layerDefaults[i];
        const String n (d.name);
        l.add (bparam (id (i, on), n + " On", true));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id (i, src), 1 }, n + " Sample", Library::choicesFor (i), 0));
        l.add (fparam (id (i, level),  n + " Level", { -36.f, 6.f }, d.level, fmtDb, parseNum));
        l.add (fparam (id (i, pitch),  n + " Pitch", { -24.f, 24.f, 0.01f }, 0.f, fmtSemi, parseNum));
        // wide range so the screen never blocks a drag; symmetric skew keeps the knob fine around 0
        l.add (fparam (id (i, offset), n + " Offset", NormalisableRange<float> (-2000.f, 2000.f, 0.1f, 0.4f, true), 0.f, fmtOff, parseMs));
        l.add (fparam (id (i, hp),     n + " Low Cut", skewed (20.f, 2000.f, 250.f), d.hp, fmtHz, parseHz));
        l.add (fparam (id (i, lp),     n + " High Cut", skewed (200.f, 20000.f, 3000.f), d.lp, fmtHz, parseHz));
        l.add (bparam (id (i, rev),    n + " Reverse", false));
        l.add (bparam (id (i, forge),  n + " to Forge", d.forge));
    }
    l.add (bparam (punchKey, "Punch Key Track", false));

    // FORGE
    l.add (bparam (fOn, "Forge On", true));
    l.add (fparam (fDrive,  "Forge Drive", { 0.f, 30.f }, 18.f, fmtDb, parseNum));
    l.add (fparam (fFocus,  "Forge Focus", skewed (150.f, 3000.f, 600.f), 520.f, fmtHz, parseHz));
    l.add (fparam (fGrit,   "Forge Grit", { 0.f, 1.f }, 0.2f, fmtPct, parsePct));
    l.add (fparam (fCrunch, "Forge Crunch", { 0.f, 1.f }, 0.55f, fmtPct, parsePct));
    l.add (fparam (fTone,   "Forge Tone", { -1.f, 1.f }, 0.25f, fmtTilt, parseTilt));
    l.add (fparam (fOtt,    "Forge OTT", { 0.f, 1.f }, 0.3f, fmtPct, parsePct));
    l.add (fparam (fSmooth, "Forge Smooth", { 0.f, 1.f }, 0.3f, fmtPct, parsePct));
    l.add (fparam (fMix,    "Forge Mix", { 0.f, 1.f }, 1.f, fmtPct, parsePct));

    // MASTER
    l.add (std::make_unique<AudioParameterInt> (ParameterID { root, 1 }, "Root Note", 24, 96, 60, AudioParameterIntAttributes()
              .withStringFromValueFunction ([] (int v, int) { return MidiMessage::getMidiNoteName (v, true, true, 5); })
              .withValueFromStringFunction ([] (const String& t)
              {
                  static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                  const auto up = t.trim().toUpperCase();
                  for (int n = 11; n >= 0; --n)
                      if (up.startsWith (names[n]))
                          return jlimit (24, 96, n + 12 * up.substring ((int) strlen (names[n])).getIntValue());
                  return t.getIntValue();
              })));
    l.add (fparam (preGain,  "Pre-Gain", { -12.f, 24.f }, 0.f, fmtDb, parseNum));
    l.add (bparam (clipOn,   "Clipper On", true));
    l.add (fparam (clipCeil, "Clipper Ceiling", { -12.f, 0.f }, -0.5f, fmtDb, parseNum));
    l.add (fparam (clipKnee, "Clipper Softness", { 0.f, 1.f }, 0.25f, fmtPct, parsePct));
    l.add (bparam (limOn,    "Limiter On", true));
    l.add (fparam (limCeil,  "Limiter Ceiling", { -12.f, 0.f }, -0.3f, fmtDb, parseNum));
    l.add (fparam (limRel,   "Limiter Release", skewed (5.f, 500.f, 60.f), 60.f, fmtMs, parseMs));
    l.add (fparam (volume,   "Volume", { -36.f, 6.f }, -3.f, fmtDb, parseNum));
    return l;
}

// ------------------------------------------------------------ factory presets
namespace
{
    struct Preset
    {
        const char* name;
        std::vector<std::pair<String, float>> values;
        std::array<float, NumSampleLayers> envMs;      // <= 0: layer default
    };

    const std::vector<Preset>& presets()
    {
        using namespace ParamIDs;
        static const std::vector<Preset> p {
            { "Ancient Raw", {}, { 0.f, 0.f, 0.f } },
            { "Temple Euphoric", { { id (Kick, src), 3 }, { id (Crunch, src), 3 }, { id (Crunch, level), -12 }, { id (Punch, src), 1 },
                                   { fDrive, 8 }, { fFocus, 420 }, { fGrit, 0.1f }, { fCrunch, 0.25f }, { fTone, 0.05f }, { fOtt, 0.25f },
                                   { fSmooth, 0.6f }, { clipKnee, 0.5f } },
                                 { 0.f, 0.f, 650.f } },
            { "Pharaoh Uptempo", { { id (Kick, src), 0 }, { id (Kick, pitch), 5 }, { id (Crunch, src), 1 }, { id (Crunch, level), -3 },
                                   { id (Punch, src), 5 }, { fDrive, 18 }, { fFocus, 700 }, { fGrit, 0.25f }, { fCrunch, 0.6f },
                                   { fTone, 0.25f }, { fOtt, 0.4f }, { fSmooth, 0.4f }, { preGain, 4 }, { clipKnee, 0.1f } },
                                 { 0.f, 0.f, 380.f } },
            { "Babylon Hardcore", { { id (Kick, src), 2 }, { id (Crunch, src), 2 }, { fDrive, 15 }, { fFocus, 450 }, { fGrit, 0.3f },
                                    { fCrunch, 0.5f }, { fTone, 0.1f }, { fOtt, 0.35f }, { fSmooth, 0.5f }, { preGain, 3 } },
                                  { 0.f, 0.f, 1100.f } },
            { "Colossus Frenchcore", { { id (Kick, src), 1 }, { id (Kick, pitch), 2 }, { id (Crunch, src), 5 }, { id (Crunch, level), -4 },
                                       { id (Punch, src), 2 }, { fDrive, 20 }, { fFocus, 800 }, { fGrit, 0.25f }, { fCrunch, 0.65f },
                                       { fTone, 0.3f }, { fOtt, 0.45f }, { fSmooth, 0.45f }, { preGain, 5 }, { clipKnee, 0.f } },
                                     { 0.f, 0.f, 1400.f } },
            { "Monolith Rawstyle", { { id (Kick, src), 5 }, { id (Crunch, src), 4 }, { id (Crunch, pitch), 3 }, { id (Crunch, offset), 4 },
                                     { id (Punch, src), 6 }, { fDrive, 17 }, { fFocus, 500 }, { fGrit, 0.35f }, { fCrunch, 0.55f },
                                     { fTone, 0.2f }, { fOtt, 0.4f }, { fSmooth, 0.45f }, { preGain, 3 } },
                                   { 0.f, 0.f, 900.f } },
            { "Reverse Ritual", { { id (Kick, src), 3 }, { id (Kick, rev), 1 }, { id (Punch, on), 0 }, { id (Crunch, on), 0 },
                                  { fDrive, 12 }, { fFocus, 500 }, { fGrit, 0.3f }, { fCrunch, 0.4f }, { fTone, 0.15f }, { fOtt, 0.35f } },
                                { 0.f, 0.f, 0.f } },
            { "Clean Tail", { { id (Punch, src), 0 }, { id (Crunch, on), 0 }, { id (Kick, src), 3 }, { fDrive, 6 }, { fGrit, 0.f },
                              { fCrunch, 0.2f }, { fOtt, 0.2f }, { fSmooth, 0.6f }, { fMix, 0.6f } },
                            { 0.f, 0.f, 0.f } },
            { "Punch Only", { { id (Crunch, on), 0 }, { id (Kick, on), 0 }, { id (Punch, src), 0 } }, { 0.f, 0.f, 0.f } },
        };
        return p;
    }
}

int AncientProcessor::getNumPrograms() { return (int) presets().size(); }

const String AncientProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

void AncientProcessor::setCurrentProgram (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentProgram = index;
    const auto& pr = presets()[(size_t) index];
    currentPresetName = pr.name;

    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& [pid, v] : pr.values)
        if (auto* rp = apvts.getParameter (pid))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));

    for (int l = 0; l < NumSampleLayers; ++l)
        setEnvelope (l, pr.envMs[(size_t) l] > 0.f ? Envelope::make (pr.envMs[(size_t) l]) : defaultEnvelope (l));
}

// ------------------------------------------------------------ construction
AncientProcessor::AncientProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ANCIENT", createLayout())
{
    formats.registerBasicFormats();
    SampleLibrary::get();   // build the library now, not on the audio thread

    using namespace ParamIDs;
    auto g = [this] (const String& pid) { auto* r = apvts.getRawParameterValue (pid); jassert (r != nullptr); return r; };
    for (int i = 0; i < NumSampleLayers; ++i)
    {
        const auto u = (size_t) i;
        ptr.on[u] = g (id (i, on));         ptr.src[u] = g (id (i, src));
        ptr.level[u] = g (id (i, level));   ptr.pitch[u] = g (id (i, pitch));
        ptr.offset[u] = g (id (i, offset)); ptr.hp[u] = g (id (i, hp));
        ptr.lp[u] = g (id (i, lp));         ptr.rev[u] = g (id (i, rev));
        ptr.forge[u] = g (id (i, forge));
        std::atomic_store (&envelopes[u], EnvPtr (std::make_shared<Envelope> (defaultEnvelope (i))));
    }
    ptr.punchKey = g (punchKey);
    ptr.fOn = g (fOn); ptr.fDrive = g (fDrive); ptr.fFocus = g (fFocus); ptr.fGrit = g (fGrit);
    ptr.fCrunch = g (fCrunch); ptr.fTone = g (fTone); ptr.fOtt = g (fOtt);
    ptr.fSmooth = g (fSmooth); ptr.fMix = g (fMix);
    ptr.root = g (root); ptr.preGain = g (preGain); ptr.clipOn = g (clipOn); ptr.clipCeil = g (clipCeil);
    ptr.clipKnee = g (clipKnee); ptr.limOn = g (limOn); ptr.limCeil = g (limCeil); ptr.limRel = g (limRel);
    ptr.volume = g (volume);

    for (int l = 0; l < NumSampleLayers; ++l)
        apvts.state.setProperty ("env" + String (l), defaultEnvelope (l).toString(), nullptr);
}

bool AncientProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void AncientProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (gatherParams());
    engine.prepare (sampleRate, samplesPerBlock);
}

SamplePtr AncientProcessor::getLayerSample (int layer) const
{
    const int s = (int) std::round (ptr.src[(size_t) layer]->load());
    if (s >= Library::userIndex)
        return std::atomic_load (&userSample[(size_t) layer]);
    return SampleLibrary::get().sample (layer, s);
}

KickParams AncientProcessor::gatherParams()
{
    auto db = [] (std::atomic<float>* a) { return Decibels::decibelsToGain (a->load(), -60.f); };
    auto on = [] (std::atomic<float>* a) { return a->load() > 0.5f; };

    KickParams k;
    for (int i = 0; i < NumSampleLayers; ++i)
    {
        auto& L = k.layer[(size_t) i];
        const auto u = (size_t) i;
        L.on = on (ptr.on[u]);
        L.gain = db (ptr.level[u]);
        L.pitch = ptr.pitch[u]->load();
        L.offsetMs = ptr.offset[u]->load();
        L.hpHz = ptr.hp[u]->load();
        L.lpHz = ptr.lp[u]->load();
        L.reverse = on (ptr.rev[u]);
        L.forge = on (ptr.forge[u]);
        L.keyTrack = i == Punch ? on (ptr.punchKey) : true;      // crunch & kick always follow the note
        L.sample = getLayerSample (i);
        L.env = getEnvelope (i);
    }
    k.forge.on = on (ptr.fOn);
    k.forge.driveDb = ptr.fDrive->load();
    k.forge.focusHz = ptr.fFocus->load();
    k.forge.grit = ptr.fGrit->load();
    k.forge.crunch = ptr.fCrunch->load();
    k.forge.tone = ptr.fTone->load();
    k.forge.ott = ptr.fOtt->load();
    k.forge.smooth = ptr.fSmooth->load();
    k.forge.mix = ptr.fMix->load();


    k.root = (int) std::round (ptr.root->load());
    k.preGain = Decibels::decibelsToGain (ptr.preGain->load());
    k.clipOn = on (ptr.clipOn);
    k.clipCeil = Decibels::decibelsToGain (ptr.clipCeil->load());
    k.clipKnee = ptr.clipKnee->load();
    k.limOn = on (ptr.limOn);
    k.limCeilDb = ptr.limCeil->load();
    k.limRelMs = ptr.limRel->load();
    k.volume = db (ptr.volume);
    return k;
}

void AncientProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
    if (buffer.getNumChannels() < 2) { buffer.clear(); return; }

    engine.setParams (gatherParams());

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    if (testTrigger.exchange (false))
        engine.trigger ((int) std::round (ptr.root->load()));

    int pos = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isNoteOn()) continue;
        const int at = jlimit (0, n, meta.samplePosition);
        if (at > pos) { engine.process (L + pos, R + pos, at - pos); pos = at; }
        engine.trigger (msg.getNoteNumber());
    }
    if (pos < n) engine.process (L + pos, R + pos, n - pos);
}

// ------------------------------------------------------------ envelopes
void AncientProcessor::setEnvelope (int layer, Envelope e)
{
    if (! isPositiveAndBelow (layer, (int) NumSampleLayers)) return;
    e.sortAndClamp();
    if (e.points.size() < 2) e = defaultEnvelope (layer);
    keepAlive.push_back (std::atomic_load (&envelopes[(size_t) layer]));
    std::atomic_store (&envelopes[(size_t) layer], EnvPtr (std::make_shared<Envelope> (e)));
    apvts.state.setProperty ("env" + String (layer), e.toString(), nullptr);
    collectGarbage();
}

void AncientProcessor::restoreEnvelopesFromState()
{
    for (int l = 0; l < NumSampleLayers; ++l)
    {
        Envelope e;
        if (! Envelope::fromString (apvts.state.getProperty ("env" + String (l)).toString(), e))
            e = defaultEnvelope (l);
        setEnvelope (l, e);
    }
}

// ------------------------------------------------------------ user samples
bool AncientProcessor::isSupportedAudioFile (const String& path)
{
    return File (path).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
}

bool AncientProcessor::loadUserSample (int layer, const File& file)
{
    if (! isPositiveAndBelow (layer, (int) NumSampleLayers) || ! file.existsAsFile()) return false;

    std::unique_ptr<AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;

    const int len = (int) std::min<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * 20.0));
    const int chans = jlimit (1, 2, (int) reader->numChannels);
    AudioBuffer<float> tmp (chans, len);
    reader->read (&tmp, 0, len, 0, true, chans > 1);

    auto s = std::make_shared<SampleData>();
    s->data.resize ((size_t) len);
    for (int i = 0; i < len; ++i)
    {
        float v = 0.f;
        for (int c = 0; c < chans; ++c) v += tmp.getSample (c, i);
        s->data[(size_t) i] = v / (float) chans;
    }
    s->sampleRate = reader->sampleRate;
    s->name = file.getFileNameWithoutExtension();
    s->isUser = true;
    s->path = file.getFullPathName();

    keepAlive.push_back (std::atomic_load (&userSample[(size_t) layer]));
    std::atomic_store (&userSample[(size_t) layer], SamplePtr (s));
    apvts.state.setProperty ("user" + String (layer), s->path, nullptr);

    if (auto* p = apvts.getParameter (ParamIDs::id (layer, ParamIDs::src)))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) Library::userIndex));

    // a new sample gets an envelope that covers its whole length (minus a short fade)
    const float lenMs = (float) ((double) len / reader->sampleRate * 1000.0);
    setEnvelope (layer, Envelope::make (lenMs, jmin (30.f, lenMs * 0.1f)));

    collectGarbage();
    return true;
}

String AncientProcessor::getUserSamplePath (int layer) const
{
    return apvts.state.getProperty ("user" + String (layer)).toString();
}

void AncientProcessor::collectGarbage()
{
    keepAlive.erase (std::remove_if (keepAlive.begin(), keepAlive.end(),
                                     [] (const std::shared_ptr<const void>& s) { return s == nullptr || s.use_count() <= 1; }),
                     keepAlive.end());
}

void AncientProcessor::restoreUserSamplesFromState()
{
    for (int l = 0; l < NumSampleLayers; ++l)
    {
        const auto path = getUserSamplePath (l);
        auto current = std::atomic_load (&userSample[(size_t) l]);
        if (path.isEmpty())
        {
            if (current != nullptr) { keepAlive.push_back (current); std::atomic_store (&userSample[(size_t) l], SamplePtr()); }
            continue;
        }
        if (current != nullptr && current->path == path) continue;

        // load without touching the layer's source selection or its saved envelope
        auto* srcParam = apvts.getParameter (ParamIDs::id (l, ParamIDs::src));
        const float before = srcParam != nullptr ? srcParam->getValue() : 0.f;
        const auto envBefore = apvts.state.getProperty ("env" + String (l)).toString();
        if (! loadUserSample (l, File (path)))
        {
            keepAlive.push_back (current);
            std::atomic_store (&userSample[(size_t) l], SamplePtr());
            apvts.state.setProperty ("user" + String (l), path, nullptr);   // keep it so the GUI can say what is missing
        }
        Envelope e;
        if (Envelope::fromString (envBefore, e)) setEnvelope (l, e);
        if (srcParam != nullptr) srcParam->setValueNotifyingHost (before);
    }
    collectGarbage();
}

// ------------------------------------------------------------ user presets
File AncientProcessor::getUserPresetFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Ancient Drums").getChildFile ("Presets");
}

Array<File> AncientProcessor::getUserPresets() const
{
    auto files = getUserPresetFolder().findChildFiles (File::findFiles, false, "*.adp");
    files.sort();
    return files;
}

bool AncientProcessor::saveUserPreset (const String& name)
{
    const auto clean = File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto folder = getUserPresetFolder();
    if (! folder.createDirectory()) return false;

    std::unique_ptr<XmlElement> xml (apvts.copyState().createXml());
    if (xml == nullptr) return false;
    for (auto* a : { "program", "presetName", "selLayer", "zoomMs" }) xml->removeAttribute (a);
    if (! xml->writeTo (folder.getChildFile (clean + ".adp"))) return false;
    currentPresetName = clean;
    return true;
}

void AncientProcessor::applyValues (const XmlElement& xml)
{
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto* child : xml.getChildWithTagNameIterator ("PARAM"))
        if (auto* rp = apvts.getParameter (child->getStringAttribute ("id")))
            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child->getDoubleAttribute ("value")));
}

bool AncientProcessor::loadUserPreset (const File& file)
{
    auto xml = XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    applyValues (*xml);
    for (int l = 0; l < NumSampleLayers; ++l)
    {
        apvts.state.setProperty ("user" + String (l), xml->getStringAttribute ("user" + String (l)), nullptr);
        apvts.state.setProperty ("env" + String (l), xml->getStringAttribute ("env" + String (l)), nullptr);
    }
    restoreUserSamplesFromState();
    restoreEnvelopesFromState();
    currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

// ------------------------------------------------------------ randomize
void AncientProcessor::randomize()
{
    using namespace ParamIDs;
    auto& r = rng;
    auto set = [this] (const String& pid, float v)
    {
        if (auto* p = apvts.getParameter (pid))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    };
    auto between = [&r] (float lo, float hi) { return lo + r.nextFloat() * (hi - lo); };
    auto logBetween = [&r] (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); };
    auto chance = [&r] (float p) { return r.nextFloat() < p ? 1.f : 0.f; };

    for (int l = 0; l < NumSampleLayers; ++l)
    {
        // a layer playing your own sample keeps it; library layers get a new sample
        if ((int) std::round (ptr.src[(size_t) l]->load()) != Library::userIndex)
            set (id (l, src), (float) r.nextInt (Library::perLayer));
        set (id (l, rev), 0.f);
    }
    set (id (Punch, on), chance (0.85f));
    set (id (Punch, level), between (-8.f, 0.f));
    set (id (Punch, pitch), between (-5.f, 7.f));
    set (id (Punch, offset), 0.f);
    set (id (Punch, hp), logBetween (20.f, 200.f));
    set (id (Punch, lp), logBetween (6000.f, 20000.f));
    set (id (Punch, forge), chance (0.15f));
    set (punchKey, chance (0.3f));

    set (id (Crunch, on), chance (0.85f));
    set (id (Crunch, level), between (-14.f, -2.f));
    set (id (Crunch, pitch), between (-7.f, 7.f));
    set (id (Crunch, offset), between (0.f, 12.f));
    set (id (Crunch, hp), logBetween (150.f, 700.f));
    set (id (Crunch, lp), logBetween (5000.f, 18000.f));
    set (id (Crunch, forge), 1.f);

    set (id (Kick, on), 1.f);
    set (id (Kick, level), between (-3.f, 0.f));
    set (id (Kick, pitch), between (-3.f, 3.f));
    set (id (Kick, offset), between (0.f, 6.f));
    set (id (Kick, hp), 20.f);
    set (id (Kick, lp), logBetween (6000.f, 20000.f));
    set (id (Kick, forge), 1.f);

    set (fOn, 1.f);
    set (fDrive, between (8.f, 20.f));
    set (fFocus, logBetween (350.f, 900.f));
    set (fGrit, between (0.05f, 0.4f));
    set (fCrunch, between (0.25f, 0.65f));
    set (fTone, between (-0.1f, 0.3f));
    set (fOtt, between (0.2f, 0.5f));
    set (fSmooth, between (0.3f, 0.7f));
    set (fMix, chance (0.7f) > 0.f ? 1.f : between (0.6f, 0.9f));


    set (preGain, between (0.f, 8.f));
    set (clipOn, 1.f);
    set (clipKnee, between (0.f, 0.6f));
    set (limOn, 1.f);
    // root note and volume are left alone on purpose

    setEnvelope (Punch, Envelope::make (logBetween (80.f, 220.f)));
    setEnvelope (Crunch, Envelope::make (logBetween (120.f, 350.f)));
    setEnvelope (Kick, Envelope::make (logBetween (400.f, 1300.f)));

    currentPresetName = "Random #" + String (1000 + r.nextInt (9000));
}

// ------------------------------------------------------------ state
AudioProcessorEditor* AncientProcessor::createEditor() { return new AncientEditor (*this); }

void AncientProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("version", 2, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void AncientProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = ValueTree::fromXml (*xml);
            currentProgram = vt.getProperty ("program", 0);
            currentPresetName = vt.getProperty ("presetName", getProgramName (currentProgram)).toString();

            // parameters the saved state doesn't know (older versions) get their defaults
            for (auto* prm : getParameters())
                if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
                    if (! vt.getChildWithProperty ("id", rp->paramID).isValid())
                    {
                        ValueTree child ("PARAM");
                        child.setProperty ("id", rp->paramID, nullptr);
                        child.setProperty ("value", rp->convertFrom0to1 (rp->getDefaultValue()), nullptr);
                        vt.appendChild (child, nullptr);
                    }

            apvts.replaceState (vt);
            restoreUserSamplesFromState();
            restoreEnvelopesFromState();
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AncientProcessor(); }
