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
    setCurrentProgram (0);
}

void GeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // FL may call this while an audio callback is still running (Replace, sample-rate change, loading a project):
    // processBlock outputs silence instead of rendering into voices that are being re-prepared (sonic-toolbox bugs/fl-replace-crash.md)
    const SpinLock::ScopedLockType l (prepareLock);
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
    if (pendingAudition.exchange (false)) { engine.cookOnly (f); engine.trigger (auditionNote, 1.f); }
    engine.process (buffer, midi, f);
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
}
bool GeProcessor::loadUserPresetFile (const File& f)
{
    if (! doomsday::PresetIO::load (*this, apvts, f)) return false;
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
