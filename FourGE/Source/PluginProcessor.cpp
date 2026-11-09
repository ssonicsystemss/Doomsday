#include "PluginProcessor.h"
#include <set>
#include "Presets/VariantRandomizer.h"
#include "GUI/PluginEditor.h"
#include "Presets/FactoryPresets.h"

using namespace juce;
using namespace fge;

AudioProcessorValueTreeState::ParameterLayout GeProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    for (int i = 0; i < P::count; ++i)
    {
        const auto& d = def (i);
        switch (d.kind)
        {
            case Kind::Float:
            {
                NormalisableRange<float> r (d.min, d.max);
                if (d.centre > d.min && d.centre < d.max) r.setSkewForCentre (d.centre);
                auto attr = AudioParameterFloatAttributes()
                                .withStringFromValueFunction ([i] (float v, int) { return formatValue (def (i), v); })
                                .withValueFromStringFunction ([i] (const String& t) { return parseValue (def (i), t); });
                l.add (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name, r, d.def, attr));
                break;
            }
            case Kind::Bool:   l.add (std::make_unique<AudioParameterBool> (ParameterID { d.id, 1 }, d.name, d.def > 0.5f)); break;
            case Kind::Choice: l.add (std::make_unique<AudioParameterChoice> (ParameterID { d.id, 1 }, d.name, d.choices(), (int) d.def)); break;
        }
    }
    return l;
}

GeProcessor::GeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "FOURGE", createLayout())
{
    for (int i = 0; i < P::count; ++i) pointers.ptr[(size_t) i] = apvts.getRawParameterValue (def (i).id);
    engine.setSamples (&samplePool.ptrs);                                // module 10
    for (auto* prm : getParameters()) prm->addListener (this);          // module 13: gestures = "touched by the user"
    apvts.state.addListener (this);
    setCurrentProgram (0);
    rebuildShapes();
    rebuildOscData();
    rebuildDistData();
    for (auto& m : macroPending) m.store (-1.f);
    rebuildModData();
    refreshTables();
    startTimerHz (20);
}

void GeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // FL may call this while an audio callback is still running (Replace, sample-rate change, loading a project):
    // processBlock outputs silence instead of rendering into voices that are being re-prepared (sonic-toolbox bugs/fl-replace-crash.md)
    const SpinLock::ScopedLockType l (prepareLock);
    refreshTables();
    engine.prepare (sampleRate, samplesPerBlock);
    samplePool.setTargetRate (sampleRate * Engine::osFactor);
    meter.prepare (sampleRate);
    seedWanted.store (presetSeed());
    engine.cookOnly (frame());
    setLatencySamples (engine.latencySamples());
}

void GeProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();
    const SpinLock::ScopedTryLockType prepared (prepareLock);
    if (! prepared.isLocked()) return;
    ParamFrame f = ParamFrame::defaults();
    pointers.gather (f);
    double bpm = 0.0;                                      // no tempo from the host = LOCAL BPM (MODULE02 GLOBAL SYNC)
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto hb = pos->getBpm()) bpm = *hb;
    engine.setHostBpm (bpm);
    lastHostBpm.store (bpm);
    engine.setBank (&bankX.front());
    engine.setTables (&tablesX.front());
    engine.setDistData (&distX.front());
    engine.setOffline (isNonRealtime());
    engine.setModData (&modX.front());
    {
        double ppq = 0.0; bool playing = false; int bar = 4;                 // module 09: LFO / sequencer HOST SYNC and ON BAR
        if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq = *q;
            if (auto ts = pos->getTimeSignature()) bar = jmax (1, ts->numerator * 4 / jmax (1, ts->denominator));
        }
        engine.setHostPos (ppq, playing, bar);
    }
    if (pendingAudition.exchange (false)) { engine.cookOnly (f); engine.trigger (auditionNote, 1.f); }
    for (const auto meta : midi)                                       // module 09: MIDI LEARN + CC -> macros (applied on the message thread)
    {
        const auto m = meta.getMessage();
        if (! m.isController()) continue;
        if (learnTarget.load() >= 0) learnedCc.store (m.getControllerNumber());
        for (int i = 0; i < 12; ++i) if (macroCc[(size_t) i].load() == m.getControllerNumber()) macroPending[(size_t) i].store ((float) m.getControllerValue() / 127.f);
    }
    if (const auto sd = seedWanted.load (std::memory_order_relaxed); sd != engine.seed()) engine.setSeed (sd);   // module 12: the preset SEED
    engine.process (buffer, midi, f);
    meter.push (buffer.getReadPointer (0), buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : nullptr, buffer.getNumSamples());   // module 12: MASTER METERS
    samplePool.audioClock.fetch_add (buffer.getNumSamples());
    samplePool.mixPreview (buffer);                                     // the browser's PREVIEW (not a slot)
    // module 11: the stereo analyzer sees the real output; MONO AUDITION only changes what you hear (never the preset)
    if (buffer.getNumChannels() > 1)
    {
        analyzer.push (buffer.getReadPointer (0), buffer.getReadPointer (1), buffer.getNumSamples(), getSampleRate() > 0 ? getSampleRate() : 44100.0);
        if (monoAudition.load (std::memory_order_relaxed))
            for (int i = 0; i < buffer.getNumSamples(); ++i) { const float m = 0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)); buffer.setSample (0, i, m); buffer.setSample (1, i, m); }
    }
    // module 08: a LOOKAHEAD at MASTER OUTPUT delays the output - the host is told (it compensates)
    if (const int lat = engine.latencySamples(); lat != getLatencySamples()) setLatencySamples (lat);
}

// ------------------------------------------------------------ DRAWN shapes (state child ENVS)
ValueTree GeProcessor::envTree (bool create)
{
    auto t = apvts.state.getChildWithName (envsTag);
    if (! t.isValid() && create) { t = ValueTree (envsTag); apvts.state.appendChild (t, nullptr); }
    return t;
}
void GeProcessor::setShape (int slot, const env::Shape& s, bool undoable, bool newUndoStep)
{
    if (! isPositiveAndBelow (slot, (int) env::NumSlots)) return;
    if (undoable && newUndoStep) undo.beginNewTransaction ("Envelope");
    envTree (true).setProperty (env::slotKey (slot), s.toString(), undoable ? &undo : nullptr);
}
void GeProcessor::resetShapes (bool undoable)
{
    auto t = envTree (false);
    if (t.isValid()) t.removeAllProperties (undoable ? &undo : nullptr);
    rebuildShapes();
}
void GeProcessor::setOscProperty (int osc, const String& key, const String& text, bool undoable, bool newUndoStep)
{
    if (! isPositiveAndBelow (osc, fge::dsp::numOsc)) return;
    auto root = apvts.state.getChildWithName (oscsTag);
    if (! root.isValid()) { root = ValueTree (oscsTag); apvts.state.appendChild (root, nullptr); }
    const Identifier id ("osc" + String (osc + 1));
    auto t = root.getChildWithName (id);
    if (! t.isValid()) { t = ValueTree (id); root.appendChild (t, nullptr); }
    if (undoable && newUndoStep) undo.beginNewTransaction ("Oscillator");
    t.setProperty (key, text, undoable ? &undo : nullptr);
}
void GeProcessor::setDistCurve (int stage, const fge::dsp::dist::CurveData& c, bool newUndoStep)
{
    if (! isPositiveAndBelow (stage, fge::dsp::dist::numStages)) return;
    auto t = apvts.state.getChildWithName (distTag);
    if (! t.isValid()) { t = ValueTree (distTag); apvts.state.appendChild (t, nullptr); }
    if (newUndoStep) undo.beginNewTransaction ("Waveshaper curve");
    t.setProperty ("curve" + String (stage + 1), c.toString(), &undo);
}
void GeProcessor::setClipCurve (int clipper, const fge::dsp::dist::CurveData& c, bool newUndoStep)
{
    if (! isPositiveAndBelow (clipper, 2)) return;
    auto t = apvts.state.getChildWithName (distTag);
    if (! t.isValid()) { t = ValueTree (distTag); apvts.state.appendChild (t, nullptr); }
    if (newUndoStep) undo.beginNewTransaction ("Clip curve");
    auto v = c; v.sanitize();                                                        // (a broken / non-monotonic curve is repaired)
    t.setProperty ("clip" + String (clipper + 1), v.toString(), &undo);
}
void GeProcessor::rebuildDistData()
{
    const auto t = apvts.state.getChildWithName (distTag);
    fge::dsp::dist::DistData d;
    for (int i = 0; i < fge::dsp::dist::numStages; ++i)
    {
        fge::dsp::dist::CurveData c;
        if (t.isValid() && fge::dsp::dist::CurveData::fromString (t.getProperty ("curve" + String (i + 1)).toString(), c)) d.curve[(size_t) i] = c;
        else d.curve[(size_t) i] = fge::dsp::dist::CurveData::preset (1);           // a gentle S-curve to start from
        d.curveVersion[(size_t) i] = (int) distVersion;
    }
    for (int i = 0; i < 2; ++i)
    {
        fge::dsp::dist::CurveData c;
        if (t.isValid() && fge::dsp::dist::CurveData::fromString (t.getProperty ("clip" + String (i + 1)).toString(), c)) d.clip[(size_t) i] = c;
    }
    distNow = d; ++distVersion;
    distX.back() = d; distX.publish();
}
void GeProcessor::rebuildOscData()
{
    const auto root = apvts.state.getChildWithName (oscsTag);
    fge::dsp::OscData d;
    for (int i = 0; i < fge::dsp::numOsc; ++i) d.osc[(size_t) i] = fge::dsp::oscstate::fromTree (root.getChildWithName (Identifier ("osc" + String (i + 1))));
    {
        const SpinLock::ScopedLockType l (tableLock);
        oscDataNow = std::move (d);
        ++oscVersion;
    }
    refreshTables();
}
void GeProcessor::refreshTables()
{
    const auto f = frame();
    std::array<fge::dsp::TablePtr, fge::dsp::maxOsc> next;
    bool changed = false;
    {
        const SpinLock::ScopedLockType l (tableLock);
        next = tablesNow;
        for (int i = 0; i < fge::dsp::numOsc; ++i)
        {
            const bool on = f.on (P::osc1On + i * (P::osc2On - P::osc1On));
            const auto key = on ? fge::dsp::osctables::keyOf (f, i, oscVersion) : String ("off");
            if (key == tableKeys[(size_t) i]) continue;
            tableKeys[(size_t) i] = key;
            next[(size_t) i] = on ? fge::dsp::osctables::build (f, oscDataNow, i) : nullptr;
            changed = true;
        }
        if (! changed) return;
        retired.push_back (tablesNow);                       // the audio thread may hold the old pointers for a block or two
        if (retired.size() > 4) retired.erase (retired.begin());
        tablesNow = next;
    }
    auto& b = tablesX.back();
    for (int i = 0; i < fge::dsp::maxOsc; ++i) b.t[(size_t) i] = next[(size_t) i].get();
    tablesX.publish();
}

void GeProcessor::rebuildShapes()
{
    const SpinLock::ScopedLockType l (shapeWriteLock);
    const auto t = apvts.state.getChildWithName (envsTag);
    env::Bank b;
    for (int i = 0; i < env::NumSlots; ++i)
    {
        const auto txt = t.isValid() ? t.getProperty (env::slotKey (i)).toString() : String();
        if (txt.isEmpty() || ! env::Shape::fromString (txt, b.s[(size_t) i], slotMin (i), 1.f)) b.s[(size_t) i] = env::defaultShape (i);
    }
    current = b;
    bankX.back() = b;
    bankX.publish();
    ++version;
}

// ------------------------------------------------------------ MODULATION MATRIX (module 09, state child MODM)
ValueTree GeProcessor::modTree()
{
    auto t = apvts.state.getChildWithName (modTag);
    if (t.isValid()) return t;
    t = ValueTree (modTag);                                // first edit: write out what plays now (the default mapping)
    for (int i = 0; i < fge::dsp::mod::numSlots; ++i) if (! modNow.slot[(size_t) i].empty()) t.setProperty ("s" + String (i + 1), modNow.slot[(size_t) i].toText(), nullptr);
    t.setProperty ("steps", fge::modstate::stepsToText (modNow.step), nullptr);
    apvts.state.appendChild (t, &undo);
    return t;
}
void GeProcessor::setModSlot (int slot, const fge::modstate::SlotDesc& d, bool newUndoStep)
{
    if (! isPositiveAndBelow (slot, fge::dsp::mod::numSlots)) return;
    if (newUndoStep) undo.beginNewTransaction ("Modulation");
    auto t = modTree();
    const Identifier k ("s" + String (slot + 1));
    if (d.empty()) t.removeProperty (k, &undo); else t.setProperty (k, d.toText(), &undo);
}
void GeProcessor::setModSteps (const std::array<fge::dsp::mod::Step, fge::dsp::mod::maxSteps>& st, bool newUndoStep)
{
    if (newUndoStep) undo.beginNewTransaction ("Step sequencer");
    modTree().setProperty ("steps", fge::modstate::stepsToText (st), &undo);
}
void GeProcessor::resetModMatrix (bool undoable)
{
    if (undoable) undo.beginNewTransaction ("Reset modulation");
    auto t = apvts.state.getChildWithName (modTag);
    if (t.isValid()) apvts.state.removeChild (t, undoable ? &undo : nullptr);
    rebuildModData();
}
void GeProcessor::clearModMatrix()
{
    undo.beginNewTransaction ("Clear modulation");
    auto t = modTree();
    for (int i = 0; i < fge::dsp::mod::numSlots; ++i) t.removeProperty ("s" + String (i + 1), &undo);
}
void GeProcessor::setModProperty (const String& key, const String& text, bool newUndoStep)
{
    if (newUndoStep) undo.beginNewTransaction ("Modulation");
    modTree().setProperty (key, text, &undo);
}
void GeProcessor::storeMorph (bool toB)
{
    const auto f = frame();
    setModProperty (toB ? "morphB" : "morphA", fge::modstate::floatsToCsv (&f.v[(size_t) P::macro_impact], 12));
}
void GeProcessor::serviceMidi()
{
    const int t = learnTarget.load(), cc = learnedCc.load();
    if (t >= 0 && cc >= 0)
    {
        learnTarget.store (-1); learnedCc.store (-1);
        if (t < 4) { undo.beginNewTransaction ("MIDI learn"); setParamInUndoStep ("cc" + String (t + 1) + "_number", (float) cc); }
        else if (t >= 100 && t < 112)
        {
            auto map = modNow.ccMacro;
            for (auto& v : map) if ((int) v == cc) v = -1.f;                  // one CC drives one macro
            map[(size_t) (t - 100)] = (float) cc;
            setModProperty ("ccMacro", fge::modstate::floatsToCsv (map.data(), 12));
        }
    }
    static const char* ids[12] { "macro_impact", "macro_body", "macro_tail", "macro_pitch", "macro_grit", "macro_scream", "macro_weight", "macro_chaos",
                                 "macro_user1", "macro_user2", "macro_user3", "macro_user4" };
    for (int i = 0; i < 12; ++i)
    {
        const float v = macroPending[(size_t) i].exchange (-1.f);
        if (v >= 0.f) if (auto* a = apvts.getParameter (ids[i])) a->setValueNotifyingHost (a->convertTo0to1 (v));   // one value model for GUI / host / DSP
    }
}
void GeProcessor::rebuildModData()
{
    modNow = fge::modstate::fromTree (apvts.state.getChildWithName (modTag));
    ++modVer;
    fge::modstate::resolve (modNow, modResolved, modUnknown, modVer);
    for (int i = 0; i < 12; ++i) macroCc[(size_t) i].store ((int) modNow.ccMacro[(size_t) i]);
    modX.back() = modResolved;
    modX.publish();
}

// ------------------------------------------------------------ SAMPLE SLOTS (module 10)
ValueTree GeProcessor::sampleTree()
{
    auto t = apvts.state.getChildWithName (sampleTag);
    if (! t.isValid()) { t = ValueTree (sampleTag); apvts.state.appendChild (t, &undo); }
    return t;
}
bool GeProcessor::importSampleNow (int slot, const File& f, String* error)
{
    if (! isPositiveAndBelow (slot, fge::dsp::numSampleSlots)) return false;
    AudioBuffer<float> b; double rate = 44100.0; String msg;
    if (! fge::smp::decode (std::unique_ptr<AudioFormatReader> (fge::smp::formats().createReaderFor (f)), b, rate, msg)) { if (error) *error = msg; return false; }
    undo.beginNewTransaction ("Load sample");
    auto t = sampleTree();
    const String k = "s" + String (slot + 1);
    t.setProperty (k + "name", f.getFileNameWithoutExtension(), &undo);
    t.setProperty (k + "path", f.getFullPathName(), &undo);
    t.setProperty (k + "data", b.getNumSamples() <= rate * fge::smp::maxEmbedSec ? fge::smp::toBase64Wav (b, rate) : String(), &undo);
    if (error) *error = msg;
    return true;
}
void GeProcessor::importSample (int slot, const File& f)
{
    std::weak_ptr<bool> life = alive;
    Thread::launch ([this, life, slot, f]
    {
        AudioBuffer<float> b; double rate = 44100.0; String msg;
        const bool ok = fge::smp::decode (std::unique_ptr<AudioFormatReader> (fge::smp::formats().createReaderFor (f)), b, rate, msg);
        const String data = ok && b.getNumSamples() <= rate * fge::smp::maxEmbedSec ? fge::smp::toBase64Wav (b, rate) : String();
        MessageManager::callAsync ([this, life, slot, f, ok, msg, data]
        {
            if (life.expired()) return;
            if (ok)
            {
                undo.beginNewTransaction ("Load sample");
                auto t = sampleTree(); const String k = "s" + String (slot + 1);
                t.setProperty (k + "name", f.getFileNameWithoutExtension(), &undo); t.setProperty (k + "path", f.getFullPathName(), &undo); t.setProperty (k + "data", data, &undo);
            }
            if (onSampleImported) onSampleImported (slot, ok ? msg : (msg.isEmpty() ? String ("cannot read the file") : msg));
        });
    });
}
void GeProcessor::clearSample (int slot)
{
    undo.beginNewTransaction ("Clear sample");
    auto t = sampleTree(); const String k = "s" + String (slot + 1);
    for (auto* p : { "name", "path", "data" }) t.removeProperty (k + p, &undo);
}
bool GeProcessor::applySampleTool (int slot, int tool)
{
    AudioBuffer<float> b; double rate;
    if (! samplePool.workingCopy (slot, b, rate)) return false;
    fge::smp::applyTool (b, tool);
    undo.beginNewTransaction ("Sample tool");
    sampleTree().setProperty ("s" + String (slot + 1) + "data", fge::smp::toBase64Wav (b, rate), &undo);   // the working copy (the file stays untouched)
    return true;
}
int GeProcessor::relinkSamples (const File& folder)
{
    int n = 0;
    for (int k = 0; k < fge::dsp::numSampleSlots; ++k)
    {
        const auto in = samplePool.info (k);
        if (in.status != fge::smp::SMissing) continue;
        const String want = File (in.path).getFileName();
        Array<File> hits;
        for (const auto& e : RangedDirectoryIterator (folder, true, want.isNotEmpty() ? want : in.name + ".*", File::findFiles))
        { hits.add (e.getFile()); break; }
        if (hits.isEmpty()) continue;
        if (importSampleNow (k, hits[0])) ++n;
    }
    return n;
}
int GeProcessor::collectSamples (const File& folder)
{
    folder.createDirectory();
    int n = 0;
    undo.beginNewTransaction ("Collect samples");
    auto t = sampleTree();
    for (int k = 0; k < fge::dsp::numSampleSlots; ++k)
    {
        const File src (t.getProperty ("s" + String (k + 1) + "path").toString());
        if (! src.existsAsFile() || src.isAChildOf (folder)) continue;
        const File dst = folder.getChildFile (src.getFileName()).getNonexistentSibling (false);
        if (src.copyFileTo (dst)) { t.setProperty ("s" + String (k + 1) + "path", dst.getFullPathName(), &undo); ++n; }
    }
    return n;
}

// ------------------------------------------------------------ presets
int GeProcessor::getNumPrograms() { return (int) factoryPresets().size(); }
const String GeProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? factoryPresets()[(size_t) i].name : String(); }
void GeProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    presetName = factoryPresets()[(size_t) i].name;
    doomsday::PresetIO::apply (*this, apvts, factoryPresets()[(size_t) i].values);
    resetShapes (false);
    auto oscs = apvts.state.getChildWithName (oscsTag);
    if (oscs.isValid()) { apvts.state.removeChild (oscs, nullptr); rebuildOscData(); }
    auto dd = apvts.state.getChildWithName (distTag);
    if (dd.isValid()) { apvts.state.removeChild (dd, nullptr); rebuildDistData(); }
    apvts.state.removeProperty ("presetSeed", nullptr);                            // module 12: a factory preset plays with seed 0
    forgetTouched();                                                                // module 13: a preset is a fresh start
    {
        const String n = factoryPresets()[(size_t) jlimit (0, (int) factoryPresets().size() - 1, i)].name;
        const int st = n.startsWith ("EARLY") ? 0 : n.startsWith ("MODERN") ? 1 : n.startsWith ("RAW") ? 2 : -1;
        if (st >= 0) apvts.state.setProperty ("style", st, nullptr); else apvts.state.removeProperty ("style", nullptr);
    }
    seedWanted.store (0);
    auto sm = apvts.state.getChildWithName (sampleTag);                           // module 10: a factory preset has no samples
    if (sm.isValid()) apvts.state.removeChild (sm, nullptr);
    auto mm = apvts.state.getChildWithName (modTag);                              // module 09: the default macro mapping
    if (mm.isValid()) apvts.state.removeChild (mm, nullptr);
    rebuildModData();
    if (const auto& ex = factoryPresets()[(size_t) i].extra; ex.isNotEmpty())    // the preset's drawn envelopes / curves
        if (auto xml = XmlDocument::parse ("<X>" + ex + "</X>"))
        {
            if (auto* e = xml->getChildByName (envsTag))
            {
                auto t = envTree (true); t.removeAllProperties (nullptr);
                for (int k = 0; k < e->getNumAttributes(); ++k) t.setProperty (e->getAttributeName (k), e->getAttributeValue (k), nullptr);
                rebuildShapes();
            }
            if (auto* e = xml->getChildByName (distTag)) { apvts.state.appendChild (ValueTree::fromXml (*e), nullptr); rebuildDistData(); }
        }
}
bool GeProcessor::loadUserPresetFile (const File& f)
{
    if (! doomsday::PresetIO::load (*this, apvts, f)) return false;
    {
        auto t = envTree (true);                       // the drawn shapes saved with the preset (or the defaults)
        t.removeAllProperties (nullptr);
        if (auto xml = XmlDocument::parse (f))
            if (auto* e = xml->getChildByName (envsTag))
                for (int i = 0; i < e->getNumAttributes(); ++i) t.setProperty (e->getAttributeName (i), e->getAttributeValue (i), nullptr);
        rebuildShapes();
        // the oscillator data (drawn / imported cycles, harmonic profiles, user wavetables)
        auto old = apvts.state.getChildWithName (oscsTag);
        if (old.isValid()) apvts.state.removeChild (old, nullptr);
        if (auto xml = XmlDocument::parse (f))
            if (auto* e = xml->getChildByName (oscsTag)) apvts.state.appendChild (ValueTree::fromXml (*e), nullptr);
        rebuildOscData();
        auto oldD = apvts.state.getChildWithName (distTag);
        if (oldD.isValid()) apvts.state.removeChild (oldD, nullptr);
        if (auto xml = XmlDocument::parse (f))
            if (auto* e = xml->getChildByName (distTag)) apvts.state.appendChild (ValueTree::fromXml (*e), nullptr);
        rebuildDistData();
        auto oldM = apvts.state.getChildWithName (modTag);                        // module 09: the preset's matrix (none = default mapping)
        if (oldM.isValid()) apvts.state.removeChild (oldM, nullptr);
        auto oldS = apvts.state.getChildWithName (sampleTag);                    // module 10: the preset's samples
        if (oldS.isValid()) apvts.state.removeChild (oldS, nullptr);
        if (auto xml = XmlDocument::parse (f))
        {
            if (auto* e = xml->getChildByName (modTag)) apvts.state.appendChild (ValueTree::fromXml (*e), nullptr);
            if (auto* e = xml->getChildByName (sampleTag)) apvts.state.appendChild (ValueTree::fromXml (*e), nullptr);
        }
        rebuildModData();
    }
    if (auto xml = XmlDocument::parse (f))                                          // module 12 / 13: the preset's SEED and STYLE
    {
        if (xml->hasAttribute ("presetSeed")) apvts.state.setProperty ("presetSeed", xml->getStringAttribute ("presetSeed").getLargeIntValue(), nullptr);
        else apvts.state.removeProperty ("presetSeed", nullptr);
        if (xml->hasAttribute ("style")) apvts.state.setProperty ("style", xml->getIntAttribute ("style"), nullptr);
        else apvts.state.removeProperty ("style", nullptr);
        seedWanted.store (presetSeed());
    }
    forgetTouched();                                                                // a preset is a fresh start
    presetName = f.getFileNameWithoutExtension();
    return true;
}
bool GeProcessor::saveUserPresetAs (const String& name)
{
    String clean;
    if (! io.save (apvts, name, clean)) return false;
    presetName = clean;
    return true;
}
void GeProcessor::storeTouched()
{
    StringArray ids; for (int i = 0; i < P::count; ++i) if (touched[(size_t) i].load()) ids.add (def (i).id);
    if (ids.isEmpty()) apvts.state.removeProperty ("touched", nullptr); else apvts.state.setProperty ("touched", ids.joinIntoString (","), nullptr);
}
void GeProcessor::restoreTouched()
{
    forgetTouched();
    StringArray ids; ids.addTokens (apvts.state.getProperty ("touched").toString(), ",", {});
    for (auto& id : ids) for (int i = 0; i < P::count; ++i) if (id == def (i).id) { touched[(size_t) i].store (true); break; }
}
void GeProcessor::getStateInformation (MemoryBlock& d) { storeTouched(); doomsday::PresetIO::getState (apvts, d, currentProgram, presetName); }
void GeProcessor::setStateInformation (const void* data, int size) { doomsday::PresetIO::setState (*this, apvts, data, size, currentProgram, presetName); restoreTouched(); }

// RANDOM (simple, module 01): musical ranges for the sound-design parameters only - never the mixer / master / mute / solo.
// The full randomizer (modes / styles / LOCK / seed) is module 09.
void GeProcessor::randomizeSound() { fge::variant::randomize (*this); }      // module 09: the VARIANT RANDOMIZER (style / scope / locks / seed)

AudioProcessorEditor* GeProcessor::createEditor() { return new GeEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GeProcessor(); }

// ------------------------------------------------------------ module 12: SEED + EXPORT (message thread; the renders run on the exporter thread)
void GeProcessor::setPresetSeed (uint32_t sd)
{
    undo.beginNewTransaction ("Seed");
    apvts.state.setProperty ("presetSeed", (juce::int64) sd, &undo);
    seedWanted.store (sd);
}
void GeProcessor::setExportSettings (const fge::xp::Settings& s)
{
    auto t = apvts.state.getChildWithName ("EXPORT");
    if (! t.isValid()) { t = ValueTree ("EXPORT"); apvts.state.appendChild (t, nullptr); }
    s.toTree (t);                                                       // (export settings are not part of UNDO)
}
fge::dsp::xport::Snapshot GeProcessor::exportSnapshot() const
{
    fge::dsp::xport::Snapshot s;
    s.f = frame();
    *s.shapes = shapes();
    *s.dist = distData();
    *s.osc = oscData();
    if (modResolved.active > 0) s.mod = std::make_shared<fge::dsp::mod::Data> (modResolved);
    for (int k = 0; k < fge::dsp::numSampleSlots; ++k) s.samples[(size_t) k] = samplePool.data (k);
    s.presetName = presetName;
    s.presetSeed = presetSeed();
    if (const int st = currentStyle(); st >= 0 && st < 4) s.style = StringArray { "EARLY", "MODERN", "RAW", "EXPERIMENTAL" }[st];
    return s;
}
fge::dsp::xport::Snapshot GeProcessor::exportSnapshotOfProgram (int program) const
{
    auto tmp = std::make_unique<GeProcessor>();
    tmp->setCurrentProgram (program);
    auto s = tmp->exportSnapshot();
    s.mod = tmp->modResolved.active > 0 ? std::make_shared<fge::dsp::mod::Data> (tmp->modResolved) : nullptr;
    return s;
}
uint32_t GeProcessor::seedFor (const fge::xp::Settings& s, uint32_t preset) const
{
    switch (s.seedMode)
    {
        case fge::xp::SeedFixed: return 0u;
        case fge::xp::SeedNew: return (uint32_t) Random::getSystemRandom().nextInt (0x7fffffff) | 1u;
        case fge::xp::SeedCustom: return s.customSeed;
        default: return preset;
    }
}
std::vector<fge::xp::Job> GeProcessor::makeExportJobs (const fge::xp::Settings& s, bool batch, String* error) const
{
    using namespace fge::xp;
    std::vector<Job> jobs;
    const double rate = rateOf (s.rate, getSampleRate());
    const File root = s.folderFile();
    std::vector<Snapshot> snaps;
    if (batch && s.batchPresets == 1) for (int p = 0; p < (int) factoryPresets().size(); ++p) { snaps.push_back (exportSnapshotOfProgram (p)); }
    else snaps.push_back (exportSnapshot());
    std::vector<int> sources;
    if (batch) { for (int i = 0; i < 7; ++i) if (s.batchSources >> i & 1u) sources.push_back (i); }
    else sources.push_back (s.source);
    if (sources.empty()) { if (error) *error = "choose at least one source"; return jobs; }
    std::set<String> used;
    int kickNo = 0;
    for (auto& snap : snaps)
    {
        ++kickNo;
        for (int v = 1; v <= (batch ? s.batchVariants : 1); ++v)
        {
            const uint32_t seed = batch && v > 1 ? seedFor (s, snap.presetSeed) + (uint32_t) (v - 1) * 7919u : seedFor (s, snap.presetSeed);
            for (int src : sources)
            {
                Job j; j.snap = snap; j.set = s; j.presetName = snap.presetName;
                const auto [source, index] = sourceOf (src);
                j.spec.source = source; j.spec.index = index; j.spec.note = auditionNote; j.spec.rate = rate;
                j.spec.bpm = lastHostBpm.load() > 0.0 ? lastHostBpm.load() : (double) snap.f[P::envLocalBpm];
                j.spec.lengthMode = s.lengthMode; j.spec.fixedMs = s.fixedMs; j.spec.preRollMs = s.preRollMs; j.spec.postRollMs = s.postRollMs;
                j.spec.silenceDb = s.silenceDb; j.spec.minSilenceMs = s.minSilenceMs; j.spec.maxMs = s.maxMs; j.spec.trim = s.trim; j.spec.seed = seed;
                j.sourceLabel = sourceNames()[src];
                NameParts np; np.preset = snap.presetName; np.style = snap.style; np.group = src == 0 ? String ("FULL") : sourceNames()[src].upToFirstOccurrenceOf (" GROUP", false, false);
                np.note = auditionNote; np.bpm = j.spec.bpm; np.variant = v; np.seed = seed;
                File dir = root;
                if (batch)   // 4GE_EXPORT / <STYLE> / KICK_nn (_Vv) / KICK_<GROUP>.wav
                    dir = root.getChildFile (File::createLegalFileName (snap.style.isNotEmpty() ? snap.style : styleOf (snap.presetName)))
                              .getChildFile ("KICK_" + String (kickNo).paddedLeft ('0', 2) + (s.batchVariants > 1 ? "_V" + String (v).paddedLeft ('0', 2) : String()));
                for (int ver = 1; ver < 1000; ++ver)                         // a free VERSION (no silent overwrite)
                {
                    np.version = ver;
                    const String name = batch ? "KICK_" + np.group.replaceCharacter (' ', '_') : makeName (s.nameTemplate, np, s.replaceSharp);
                    const File f = dir.getChildFile (name + extOf (s.format));
                    if (s.overwrite || (! f.exists() && ! used.count (f.getFullPathName()))) { j.target = f; break; }
                    if (batch) { j.target = f; break; }                      // (batch: an existing file is reported, never replaced without OVERWRITE)
                }
                used.insert (j.target.getFullPathName());
                jobs.push_back (std::move (j));
            }
        }
    }
    return jobs;
}
