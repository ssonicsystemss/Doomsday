#include "PluginProcessor.h"
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
    apvts.state.addListener (this);
    setCurrentProgram (0);
    rebuildShapes();
    rebuildOscData();
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
    engine.setBank (&bankX.front());
    engine.setTables (&tablesX.front());
    if (pendingAudition.exchange (false)) { engine.cookOnly (f); engine.trigger (auditionNote, 1.f); }
    engine.process (buffer, midi, f);
}

// ------------------------------------------------------------ DRAWN shapes (state child ENVS)
ValueTree GeProcessor::envTree (bool create)
{
    auto t = apvts.state.getChildWithName (envsTag);
    if (! t.isValid() && create) { t = ValueTree (envsTag); apvts.state.appendChild (t, nullptr); }
    return t;
}
void GeProcessor::setShape (int slot, const env::Shape& s, bool undoable)
{
    if (! isPositiveAndBelow (slot, (int) env::NumSlots)) return;
    if (undoable) undo.beginNewTransaction ("Envelope");
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
    }
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
void GeProcessor::getStateInformation (MemoryBlock& d) { doomsday::PresetIO::getState (apvts, d, currentProgram, presetName); }
void GeProcessor::setStateInformation (const void* data, int size) { doomsday::PresetIO::setState (*this, apvts, data, size, currentProgram, presetName); }

// RANDOM (simple, module 01): musical ranges for the sound-design parameters only - never the mixer / master / mute / solo.
// The full randomizer (modes / styles / LOCK / seed) is module 09.
void GeProcessor::randomizeSound()
{
    undo.beginNewTransaction ("Random");
    auto setV = [this] (int id, float v)
    {
        if (auto* p = apvts.getParameter (def (id).id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    auto r = [this] (float a, float b) { return a + (b - a) * rng.nextFloat(); };
    setV (P::peMode, (float) rng.nextInt (4));
    setV (P::peStart, r (150.f, 420.f));
    setV (P::peTime, r (40.f, 220.f));
    setV (P::peCurve, r (0.1f, 0.6f));
    setV (P::tailChar, (float) (1 + rng.nextInt (6)));
    setV (P::tailSrc, (float) rng.nextInt (4));
    setV (P::tailDrive, r (0.35f, 0.9f));
    setV (P::tailTone, r (3000.f, 14000.f));
    setV (P::tailReso, r (0.f, 0.5f));
    setV (P::punchSrc, (float) rng.nextInt (6));
    setV (P::punchPitch, r (180.f, 600.f));
    setV (P::punchDrive, r (0.f, 0.7f));
    setV (P::bodyPitch, r (90.f, 260.f));
    setV (P::bodyLevel, r (-18.f, -6.f));
    setV (P::bodyDrive, r (0.f, 0.6f));
}

AudioProcessorEditor* GeProcessor::createEditor() { return new GeEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GeProcessor(); }
