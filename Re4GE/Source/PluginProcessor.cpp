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
    loadSample (File (xml->getStringAttribute ("samplePath")));
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
    if (PresetIO::setState (*this, apvts, fixed.getData(), (int) fixed.getSize(), currentProgram, presetName)) { applyShapesFromState(); loadSample (File (apvts.state.getProperty ("samplePath").toString())); }
}

void ReProcessor::randomizeSound()
{
    using namespace ParamIDs;
    auto between = [this] (float a, float b) { return a + rng.nextFloat() * (b - a); };
    auto logB = [this] (float a, float b) { return a * std::pow (b / a, rng.nextFloat()); };
    applyTemplate (rng.nextInt (5));
    const bool tail = rng.nextFloat() < 0.5f;
    set (source, tail ? 1.f : 0.f);
    if (tail)
    {
        set (tailStart, logB (2.5f, 6.f)); set (tailDrop, logB (25.f, 180.f)); set (tailCurve, between (2.f, 5.f)); set (tailDist, (float) rng.nextInt (5));
        set (tailDrive, between (20.f, 44.f)); set (tailTone, between (-0.2f, 0.9f)); set (tailScream, rng.nextFloat() < 0.6f ? between (0.2f, 0.8f) : 0.f); set (tailScreamHz, logB (500.f, 1800.f));
    }
    else
    {
        set (warp, (float) rng.nextInt (5)); set (warpAmt, between (0.2f, 0.8f)); set (warpEnv, between (0.f, 1.f)); set (blend, between (0.f, 0.8f)); set (width, between (0.2f, 0.6f));
    }
    set (distType, (float) rng.nextInt (6)); set (ParamIDs::drive, between (6.f, 20.f)); set (driveEnv, between (0.f, 0.8f)); set (bias, rng.nextFloat() < 0.3f ? between (-0.5f, 0.5f) : 0.f);
    set (highCut, logB (1500.f, 16000.f)); set (bellHz, logB (1200.f, 2500.f)); set (bellGain, between (0.f, 8.f));   // the owner likes presence ~2 kHz (OWNER - 1 / 2)
    set (subHz, between (70.f, 120.f)); set (subLevel, 0.f); set (lowCut, 20.f);       // the low end stays (owner's rule)
    presetName = "Random #" + String (1000 + rng.nextInt (9000));
}

ReProcessor::ReProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "RE4GE", re4ge::createLayout())
{
    for (int i = 0; i < numShapes; ++i) shapes[(size_t) i] = lastShapes[(size_t) i] = re4ge::defaultLaneShape (i);
    apvts.state.addListener (this);
    setCurrentProgram (0);
}

void ReProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const SpinLock::ScopedLockType l (prepareLock);
    engine.setParams (gather());
    engine.prepare (sampleRate, samplesPerBlock);
}

re::Params ReProcessor::gather() const
{
    auto p = re4ge::gatherParams (apvts);
    {
        const SpinLock::ScopedTryLockType l (shapeLock);
        if (l.isLocked()) lastShapes = shapes;          // busy (the GUI is writing): the last copy
    }
    for (int i = 0; i < 4; ++i) p.lane[(size_t) i].shape = lastShapes[(size_t) i];
    p.driveCurve = { lastShapes[4], lastShapes[5] };
    p.curveSerial = shapeSerial.load();
    return p;
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
    buffer.clear();
    const SpinLock::ScopedTryLockType prepared (prepareLock);
    if (! prepared.isLocked()) return;
    engine.setParams (gather());
    double bpmNow = 150.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bp = pos->getBpm()) bpmNow = *bp;
    engine.setTempo (bpmNow);
    engine.setNonRealtime (isNonRealtime());
    if (pendingAudition.exchange (false)) engine.audition (auditionNote);
    engine.process (buffer, midi);
}

// message thread: the file -> engine (reading the WAV is Presets/SampleFile)
bool ReProcessor::loadSample (const File& f)
{
    if (! f.existsAsFile()) { if (f.getFullPathName().isEmpty()) clearSample(); return false; }
    auto s = re4ge::readSampleFile (f);
    if (s == nullptr) return false;
    engine.setSample (std::move (s));
    apvts.state.setProperty ("samplePath", f.getFullPathName(), nullptr);
    sampleSerial.fetch_add (1);
    return true;
}
void ReProcessor::clearSample()
{
    engine.setSample (nullptr);
    apvts.state.setProperty ("samplePath", String(), nullptr);
    sampleSerial.fetch_add (1);
}
String ReProcessor::sampleName() const { auto* s = engine.currentSample(); return s != nullptr ? s->name : String(); }

AudioProcessorEditor* ReProcessor::createEditor() { return new ReEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReProcessor(); }
