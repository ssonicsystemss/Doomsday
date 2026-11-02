#include "PluginProcessor.h"
#include "GUI/PluginEditor.h"
#include "Parameters/Layout.h"
#include "Parameters/ParamBridge.h"
#include "Presets/FactoryPresets.h"
#include "Presets/SampleFile.h"

using namespace juce;
using namespace doomsday;
namespace mod = doomsday::mod;

// ------------------------------------------------------------ factory presets + templates
const StringArray& ReProcessor::templateNames() { return re4ge::templateNames(); }
void ReProcessor::applyTemplate (int t)
{
    if (! isPositiveAndBelow (t, (int) re4ge::templates().size())) return;
    for (auto& [id, v] : re4ge::templates()[(size_t) t]) set (id.toRawUTF8(), v);
    if (t != 4 && get (ParamIDs::step) > 1.5f) set (ParamIDs::step, 0.f);        // PULSE set 1/16; the others go back to 1/4
}
void ReProcessor::set (const char* id, float v)
{
    if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); }
}

int ReProcessor::getNumPrograms() { return (int) re4ge::factoryPresets().size(); }
const String ReProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? re4ge::factoryPresets()[(size_t) i].name : String(); }
void ReProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    presetName = re4ge::factoryPresets()[(size_t) i].name;
    PresetIO::apply (*this, apvts, re4ge::factoryPresets()[(size_t) i].v);
    for (int k = 0; k < numShapes; ++k)
    {
        const String txt = k < 4 ? re4ge::factoryPresets()[(size_t) i].shapes[(size_t) k] : String();
        setShape (k, txt.isNotEmpty() ? [&] { mod::Shape sh = re4ge::defaultLaneShape (k); mod::Shape::fromString (txt, sh); return sh; }() : re4ge::defaultLaneShape (k), false);
    }
}

// v0.1 / v0.2 files: old IDs -> v0.3
static void translateLegacy (XmlElement& xml)
{
    for (auto* c : xml.getChildWithTagNameIterator ("PARAM"))
        c->setAttribute ("id", String (ParamIDs::legacy (c->getStringAttribute ("id").toRawUTF8())));
}
bool ReProcessor::loadUserPresetFile (const File& f)
{
    auto xml = XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    translateLegacy (*xml);
    PresetValues v;
    for (auto* c : xml->getChildWithTagNameIterator ("PARAM")) v.push_back ({ c->getStringAttribute ("id"), (float) c->getDoubleAttribute ("value") });
    PresetIO::apply (*this, apvts, v);
    for (int k = 0; k < numShapes; ++k)
    {
        mod::Shape sh = re4ge::defaultLaneShape (k);
        mod::Shape::fromString (xml->getStringAttribute (shapeKey (k)), sh);
        setShape (k, sh, false);
    }
    restoreSample (xml->getStringAttribute ("samplePath"), xml->getStringAttribute ("sampleData"), xml->getDoubleAttribute ("sampleRate"), xml->getStringAttribute ("sampleName"));
    presetName = f.getFileNameWithoutExtension();
    return true;
}
bool ReProcessor::saveUserPresetAs (const String& name)
{
    String clean;
    if (! io.save (apvts, name, clean)) return false;
    presetName = clean;
    return true;
}
void ReProcessor::getStateInformation (MemoryBlock& d) { PresetIO::getState (apvts, d, currentProgram, presetName); }
void ReProcessor::setStateInformation (const void* data, int size)
{
    MemoryBlock fixed (data, (size_t) size);
    if (auto xml = getXmlFromBinary (data, size)) { translateLegacy (*xml); fixed.reset(); copyXmlToBinary (*xml, fixed); }
    if (PresetIO::setState (*this, apvts, fixed.getData(), (int) fixed.getSize(), currentProgram, presetName)) { applyShapesFromState(); restoreSample (apvts.state.getProperty ("samplePath").toString(), apvts.state.getProperty ("sampleData").toString(), (double) apvts.state.getProperty ("sampleRate", 44100.0), apvts.state.getProperty ("sampleName").toString()); morphRebuild(); }
}

void ReProcessor::randomizeSound() { randomizeWith (re4ge::Randomizer::read (apvts.state)); }
float ReProcessor::randomizeWith (const re4ge::Randomizer::Settings& st)
{
    if (randomizer == nullptr) randomizer = std::make_unique<re4ge::Randomizer> (*this);
    lastRandomDb = randomizer->randomize (st, rng);
    if (st.amount > 0.f) presetName = "Random #" + String (1000 + rng.nextInt (9000));
    return lastRandomDb;
}

re4ge::ExportJob ReProcessor::makeExportJob (int note, int beats) const
{
    re4ge::ExportJob j;
    j.params = gather();
    if (auto* s = const_cast<ReProcessor*> (this)->getEngine().currentSample()) j.sample = std::make_shared<re::Sample> (*s);
    j.bpm = jlimit (60.0, 300.0, (double) const_cast<ReProcessor*> (this)->getEngine().tempoNow.load());
    j.rate = getSampleRate() > 8000.0 ? getSampleRate() : 48000.0;
    j.note = jlimit (12, 96, note); j.beats = jlimit (1, 16, beats);
    j.name = presetName;
    return j;
}

AudioProcessor::BusesProperties ReProcessor::busesFor (bool fx)
{
    if (! fx) return BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true);
    return BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                            .withInput ("Sidechain", AudioChannelSet::stereo(), false)
                                            .withOutput ("Output", AudioChannelSet::stereo(), true);
}
ReProcessor::ReProcessor (bool fx)
    : AudioProcessor (busesFor (fx)), isFx (fx),
      apvts (*this, &undo, "RE4GE", re4ge::createLayout (fx))
{
    for (int i = 0; i < numShapes; ++i) shapes[(size_t) i] = lastShapes[(size_t) i] = re4ge::defaultLaneShape (i);
    apvts.state.addListener (this);
    gather();                     // records the parameter pointers once (message thread)
    morphOnPtr = apvts.getRawParameterValue (ParamIDs::morphOn); morphAmtPtr = apvts.getRawParameterValue (ParamIDs::morph);
    morphRebuild(); morphRebuild();          // both buffers sized (no allocation later on the audio thread)
    setCurrentProgram (0);
}

void ReProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const SpinLock::ScopedLockType l (prepareLock);
    if (isFx)
    {
        detector.prepare (sampleRate);
        dry.setSize (2, jmax (16, samplesPerBlock));
        ring.assign ((size_t) (sampleRate * 2.0), 0.f); ringPos = 0; lastHitPos = -1; silentFor = 0;
        if (auto* cur = engine.currentSample(); cur == &liveSmp[0] || cur == &liveSmp[1]) engine.setSampleRaw (nullptr);   // no stale LIVE kick
        for (auto& s : liveSmp) { s.data.clear(); s.data.reserve ((size_t) (sampleRate * 2.0) + 16); s.rate = sampleRate; s.name = "LIVE"; }
        fxMidi.ensureSize (2048);
    }
    engine.setParams (gather());
    engine.prepare (sampleRate, samplesPerBlock);
}

re::Params ReProcessor::gather() const
{
    re4ge::MorphView mv; const re4ge::MorphView* mvp = nullptr;
    if (morphOnPtr != nullptr && morphOnPtr->load() > 0.5f && morphValid.load())
    {
        const auto& d = morphBuf[(size_t) morphIdx.load()];
        if (d.a.size() == paramCache.ptrs.size()) { mv = { d.a.data(), d.b.data(), d.skip.data(), jlimit (0.f, 1.f, morphAmtPtr->load()) }; mvp = &mv; }
    }
    auto p = re4ge::gatherParams (apvts, paramCache, mvp);
    {
        const SpinLock::ScopedTryLockType l (shapeLock);
        if (l.isLocked()) lastShapes = shapes;          // busy (the GUI is writing): the last copy
    }
    for (int i = 0; i < 4; ++i) p.lane[(size_t) i].shape = lastShapes[(size_t) i];
    p.driveCurve = { lastShapes[4], lastShapes[5] };
    p.curveSerial = shapeSerial.load();
    return p;
}

// ------------------------------------------------------------ MORPH A / B
ValueTree ReProcessor::morphSlot (int slot) const
{
    auto m = apvts.state.getChildWithName ("MORPH");
    return m.isValid() ? m.getChildWithName (slot == 1 ? "B" : "A") : ValueTree();
}
void ReProcessor::morphStore (int slot)
{
    auto m = apvts.state.getOrCreateChildWithName ("MORPH", nullptr);
    auto s = m.getOrCreateChildWithName (slot == 1 ? "B" : "A", nullptr);
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
            if (! r->paramID.startsWith ("morph_")) s.setProperty (r->paramID, r->getValue(), nullptr);
}
void ReProcessor::morphRebuild()
{
    const int w = 1 - morphIdx.load();
    auto& d = morphBuf[(size_t) w];
    const size_t n = paramCache.prm.size();
    d.a.resize (n); d.b.resize (n); d.skip.resize (n);
    const auto A = morphSlot (0), B = morphSlot (1);
    for (size_t i = 0; i < n; ++i)
    {
        auto* r = paramCache.prm[i];
        const Identifier id (r->paramID);
        const float cur = r->getValue();
        d.a[i] = A.hasProperty (id) ? (float) A.getProperty (id) : cur;
        d.b[i] = B.hasProperty (id) ? (float) B.getProperty (id) : cur;
        d.skip[i] = r->paramID.startsWith ("quality") || r->paramID.startsWith ("fx_") ? 1                          // never: oversampling, FX detection
                  : dynamic_cast<AudioParameterChoice*> (r) != nullptr || dynamic_cast<AudioParameterBool*> (r) != nullptr ? 2 : 0;   // switches: at 50 %
    }
    morphIdx.store (w);
    morphValid.store (A.isValid() && B.isValid());
}
void ReProcessor::morphSelect (int slot)
{
    slot = jlimit (0, 1, slot);
    morphStore (morphActive());                                     // keep what was edited
    if (morphHas (slot) && slot != morphActive())
    {
        // one UNDO step: the tree first gets the values as they are now (APVTS copies them on a timer), then every change goes
        // into the tree with the undo manager at once (LESSONS: APVTS + UNDO)
        for (auto* prm : getParameters())
            if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
            {
                auto node = apvts.state.getChildWithProperty ("id", r->paramID);
                if (node.isValid()) node.setProperty ("value", r->convertFrom0to1 (r->getValue()), nullptr);
            }
        undo.beginNewTransaction ("A / B");
        const auto s = morphSlot (slot);
        for (auto* prm : getParameters())
            if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
                if (s.hasProperty (r->paramID))
                {
                    r->beginChangeGesture(); r->setValueNotifyingHost ((float) s.getProperty (r->paramID)); r->endChangeGesture();
                    auto node = apvts.state.getChildWithProperty ("id", r->paramID);
                    if (node.isValid()) node.setProperty ("value", r->convertFrom0to1 (r->getValue()), &undo);
                }
    }
    else if (! morphHas (slot)) morphStore (slot);                  // an empty slot takes the current sound
    apvts.state.setProperty ("morphEdit", slot, nullptr);
    morphRebuild();
}
void ReProcessor::morphCopy()
{
    const int a = morphActive();
    morphStore (a);
    morphStore (1 - a);
    morphRebuild();
}
void ReProcessor::morphSync()
{
    if (! morphHas (morphActive())) return;
    const auto s = morphSlot (morphActive());
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
            if (! r->paramID.startsWith ("morph_") && (! s.hasProperty (r->paramID) || std::abs ((float) s.getProperty (r->paramID) - r->getValue()) > 1.0e-6f))
            {
                morphStore (morphActive());
                morphRebuild();
                return;
            }
}

// ------------------------------------------------------------ SHAPE DESIGNER curves
const char* ReProcessor::shapeKey (int lane) { static const char* k[numShapes] { "shapeAmp", "shapePitch", "shapeHarm", "shapePhase", "shapeDrive1", "shapeDrive2" }; return k[jlimit (0, numShapes - 1, lane)]; }
mod::Shape ReProcessor::getShape (int lane) const { const SpinLock::ScopedLockType l (shapeLock); return shapes[(size_t) jlimit (0, numShapes - 1, lane)]; }
void ReProcessor::setShape (int lane, const mod::Shape& s, bool undoable)
{
    apvts.state.setProperty (shapeKey (lane), s.toString(), undoable ? &undo : nullptr);     // -> valueTreePropertyChanged
}
void ReProcessor::valueTreePropertyChanged (ValueTree&, const Identifier& id)
{
    for (int i = 0; i < numShapes; ++i)
        if (id.toString() == shapeKey (i))
        {
            mod::Shape s = re4ge::defaultLaneShape (i);
            mod::Shape::fromString (apvts.state.getProperty (id).toString(), s);
            { const SpinLock::ScopedLockType l (shapeLock); shapes[(size_t) i] = s; }
            shapeSerial.fetch_add (1);
        }
}
void ReProcessor::applyShapesFromState()
{
    for (int i = 0; i < numShapes; ++i)
    {
        mod::Shape s = re4ge::defaultLaneShape (i);
        mod::Shape::fromString (apvts.state.getProperty (shapeKey (i)).toString(), s);
        { const SpinLock::ScopedLockType l (shapeLock); shapes[(size_t) i] = s; }
    }
    shapeSerial.fetch_add (1);
}
void ReProcessor::resetShape (int lane) { setShape (lane, re4ge::defaultLaneShape (lane)); }
void ReProcessor::shapeFromKnobs (int lane)
{
    if (lane != re::LaneAmp) { setShape (lane, re4ge::defaultLaneShape (lane)); return; }
    // the knob envelope of one step, sampled at 24 points (the drawing starts where the knobs were)
    const auto p = gather();
    const double stepMs = p.stepBeats * 60000.0 / jmax (20.f, engine.tempoNow.load());
    const auto tm = re::times (p, stepMs);
    const double ref = p.gate ? tm.gateEnd : stepMs;
    mod::Shape s;
    for (int k = 0; k < 24; ++k)
    {
        const double t = ref * k / 23.0;
        const double v = re::envAt (p, tm, t) * (p.gate ? 1.0 : re::gateAt (p, tm, t));
        s.pts[(size_t) s.n++] = { (float) (1000.0 * k / 23.0), (float) jlimit (0.0, 1.0, v), 0.f };
    }
    s.tidy();
    setShape (lane, s);
}



void ReProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    if (! isFx) buffer.clear();                              // (the FX build needs its input)
    const SpinLock::ScopedTryLockType prepared (prepareLock);
    if (! prepared.isLocked()) { buffer.clear(); return; }
    engine.setParams (gather());
    double bpmNow = 150.0, ppq = 0.0; bool ppqOk = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bp = pos->getBpm()) bpmNow = *bp;
            if (auto q = pos->getPpqPosition()) { ppq = *q; ppqOk = pos->getIsPlaying(); }
        }
    engine.setTempo (bpmNow);
    engine.setHostPpq (ppq, ppqOk);
    engine.setNonRealtime (isNonRealtime());
    if (pendingAudition.exchange (false)) engine.audition (auditionNote);
    if (isFx) { processFx (buffer, buffer.getNumSamples()); return; }
    engine.process (buffer, midi);
}

// RE4GE FX: the kicks are found in the audio; each one starts the bass (a note on FIXED NOTE). LIVE = the SAMPLE source becomes
// the previous kick (captured from the input, ring buffer), so its tail plays backwards before the next kick. The first kick after
// silence has no previous one -> no bass there. Dry = the kick passes through untouched (sample-exact).
void ReProcessor::processFx (AudioBuffer<float>& buffer, int n)
{
    using namespace ParamIDs;
    const int nIn = jmin (2, getTotalNumInputChannels());
    if (dry.getNumSamples() < n) return;                                   // (the host broke its promise on the block size)
    auto mainIn = getBusBuffer (buffer, true, 0);
    for (int ch = 0; ch < 2; ++ch) dry.copyFrom (ch, 0, mainIn, jmin (ch, jmax (0, mainIn.getNumChannels() - 1)), 0, n);
    const bool useSc = get (fxDetect) > 0.5f && getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled();
    auto det = useSc ? getBusBuffer (buffer, true, 1) : mainIn;
    const float inG = std::pow (10.f, get (fxInput) / 20.f);
    detector.set (get (fxThreshold), get (fxSens), get (fxHoldoff));
    const bool live = get (fxLive) > 0.5f;
    const int note = jlimit (0, 127, roundToInt (get (fixedNote)));
    fxMidi.clear();
    float lv = 0.f;
    const int ringN = (int) ring.size();
    for (int i = 0; i < n; ++i)
    {
        float x = 0.f;
        for (int ch = 0; ch < det.getNumChannels(); ++ch) x += det.getSample (ch, i);
        x *= inG / (float) jmax (1, det.getNumChannels());
        lv = jmax (lv, std::abs (x));
        float mono = 0.f; for (int ch = 0; ch < jmax (1, nIn); ++ch) mono += dry.getSample (ch, i);
        ring[(size_t) ringPos] = mono * 0.5f * inG;
        if (detector.process (x))
        {
            if (live && lastHitPos >= 0)
            {
                // the previous kick: from its hit to this one (max 2 s) -> the LIVE sample (pre-allocated, no allocation here)
                auto& s = liveSmp[(size_t) liveIdx];
                const int len = jmin (ringN - 1, (ringPos - lastHitPos + ringN) % ringN);
                s.data.resize ((size_t) len);
                for (int k = 0; k < len; ++k) s.data[(size_t) k] = ring[(size_t) ((lastHitPos + k) % ringN)];
                engine.setSampleRaw (&s);
                liveIdx = 1 - liveIdx;
            }
            const bool first = lastHitPos < 0;
            silentFor = 0;
            lastHitPos = ringPos;
            if (! (live && first)) fxMidi.addEvent (MidiMessage::noteOn (1, note, 1.f), i);     // LIVE: the first kick after silence has no bass
            fxHits.fetch_add (1);
        }
        ringPos = (ringPos + 1) % ringN;
        if (lastHitPos >= 0 && ++silentFor > ringN) { lastHitPos = -1; silentFor = 0; }      // 2 s without a kick = silence
    }
    fxInLevel.store (jmax (fxInLevel.load() * 0.9f, lv));
    buffer.clear();
    AudioBuffer<float> out (buffer.getArrayOfWritePointers(), 2, n);
    engine.process (out, fxMidi);
    const float mix = get (fxMix);
    const bool dryOn = get (fxDry) > 0.5f;
    for (int ch = 0; ch < 2; ++ch)
    {
        out.applyGain (ch, 0, n, mix);
        if (dryOn) out.addFrom (ch, 0, dry, ch, 0, n);
    }
}

// message thread: the file -> engine (reading the WAV is Presets/SampleFile)
// OWN SOUND: the sample is also stored INSIDE the state (16-bit mono, base64), so a project / preset plays even when the file is gone
void ReProcessor::useSample (std::unique_ptr<re::Sample> s, const String& path)
{
    if (s != nullptr)
    {
        MemoryBlock mb (s->data.size() * 2);
        auto* d = static_cast<int16_t*> (mb.getData());
        for (size_t i = 0; i < s->data.size(); ++i) d[i] = (int16_t) jlimit (-32767, 32767, roundToInt (s->data[i] * 32767.f));
        apvts.state.setProperty ("sampleData", mb.toBase64Encoding(), nullptr);
        apvts.state.setProperty ("sampleRate", s->rate, nullptr);
        apvts.state.setProperty ("sampleName", s->name, nullptr);
        sections = re::analyseKick (s->data, s->rate);
    }
    else { for (auto* k : { "sampleData", "sampleRate", "sampleName" }) apvts.state.removeProperty (k, nullptr); sections = {}; }
    engine.setSample (std::move (s));
    apvts.state.setProperty ("samplePath", path, nullptr);
    sampleSerial.fetch_add (1);
}
bool ReProcessor::loadSample (const File& f)
{
    if (! f.existsAsFile()) { if (f.getFullPathName().isEmpty()) clearSample(); return false; }
    auto s = re4ge::readSampleFile (f);
    if (s == nullptr) return false;
    embeddedOnly = false;
    useSample (std::move (s), f.getFullPathName());
    return true;
}
// a project / preset: the file if it is there, else the copy inside
bool ReProcessor::restoreSample (const String& path, const String& embedded, double rate, const String& name)
{
    if (path.isNotEmpty() && File (path).existsAsFile() && loadSample (File (path))) return true;
    if (embedded.isEmpty()) { clearSample(); return false; }
    MemoryBlock mb; if (! mb.fromBase64Encoding (embedded)) { clearSample(); return false; }
    auto s = std::make_unique<re::Sample>();
    s->rate = rate > 1000.0 ? rate : 44100.0; s->name = name;
    const auto* d = static_cast<const int16_t*> (mb.getData());
    s->data.resize (mb.getSize() / 2);
    for (size_t i = 0; i < s->data.size(); ++i) s->data[i] = d[i] / 32767.f;
    embeddedOnly = true;
    useSample (std::move (s), path);
    return true;
}
void ReProcessor::clearSample() { embeddedOnly = false; useSample (nullptr, {}); }
void ReProcessor::detectKick (int use)
{
    const auto& k = sections;
    if (! k.ok) return;
    const double from = use == 0 ? k.bodyEnd : use == 1 ? k.punchEnd : use == 2 ? k.transientEnd : 0.0;
    set (ParamIDs::smpStart, (float) jmin (500.0, from));
    set (ParamIDs::smpEnd, (float) jlimit (10.0, 4000.0, k.tailEnd));
    if (k.rootNote > 0) set (ParamIDs::smpRoot, (float) k.rootNote);
    set (ParamIDs::smpReverse, 1.f);
}
String ReProcessor::sampleName() const { auto* s = engine.currentSample(); return s != nullptr ? s->name : String(); }

AudioProcessorEditor* ReProcessor::createEditor() { return new ReEditor (*this); }
#ifndef RE4GE_FX
 #define RE4GE_FX 0
#endif
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReProcessor (RE4GE_FX != 0); }
