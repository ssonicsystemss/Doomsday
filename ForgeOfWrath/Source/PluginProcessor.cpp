#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using namespace doomsday;
namespace fm = doomsday::fmt;
using doomsday::kick::Shape;

namespace
{
    String fmtX (float v, int) { return "x" + String (v, 2); }
    String fmtNum (float v, int) { return String (v, 1); }
}

AudioProcessorValueTreeState::ParameterLayout ForgeProcessor::createLayout()
{
    using namespace ParamIDs;
    using make::f; using make::b; using make::c; using make::skew;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (f (tailLevel, "Tail Level", { -24.f, 6.f }, 0.f, fm::db, fm::num));
    l.add (f (tailStart, "Tail Start Pitch", skew (1.f, 12.f, 3.f), 3.5f, fmtX, fm::num));
    l.add (f (tailDrop, "Tail Pitch Drop", skew (2.f, 400.f, 60.f), 60.f, fm::ms, fm::msIn));
    l.add (f (tailCurve, "Tail Drop Curve", { 0.5f, 10.f }, 4.f, fmtNum, fm::num));
    l.add (f (tailWave, "Tail Wave (Sine > Tri > Saw)", { 0.f, 1.f }, 0.f));
    l.add (f (tune, "Tune", { -12.f, 12.f }, 0.f, fm::semi, fm::num));
    l.add (c (distType, "Distortion Type", Choices::distTypes, 1));
    l.add (f (drive, "Drive", { 0.f, 48.f }, 18.f, fm::db, fm::num));
    l.add (f (tone, "Tone", { -1.f, 1.f }, 0.f, fm::bip, fm::pctIn));
    l.add (f (lowCut, "Low Cut", skew (10.f, 300.f, 40.f), 25.f, fm::hz, fm::hzIn));
    l.add (f (highCut, "High Cut", skew (500.f, 20000.f, 5000.f), 12000.f, fm::hz, fm::hzIn));
    l.add (f (scream, "Scream", { 0.f, 1.f }, 0.f));
    l.add (f (screamHz, "Scream Freq", skew (150.f, 5000.f, 900.f), 900.f, fm::hz, fm::hzIn));
    l.add (f (crunch, "Crunch", { 0.f, 1.f }, 0.f));
    l.add (f (crunchDec, "Crunch Decay", skew (5.f, 600.f, 120.f), 120.f, fm::ms, fm::msIn));
    l.add (f (body, "Body", { -12.f, 12.f }, 0.f, fm::db, fm::num));
    l.add (f (grit, "Grit", { 0.f, 1.f }, 0.f));
    l.add (b (punchOn, "Punch On", true));
    l.add (f (punchLevel, "Punch Level", { -36.f, 6.f }, -6.f, fm::db, fm::num));
    l.add (f (punchStart, "Punch Start", skew (50.f, 2000.f, 400.f), 400.f, fm::hz, fm::hzIn));
    l.add (f (punchEnd, "Punch End", skew (30.f, 300.f, 90.f), 90.f, fm::hz, fm::hzIn));
    l.add (f (punchDrop, "Punch Drop", skew (0.5f, 80.f, 12.f), 12.f, fm::ms, fm::msIn));
    l.add (f (punchDecay, "Punch Decay", skew (3.f, 300.f, 40.f), 40.f, fm::ms, fm::msIn));
    l.add (f (clickLevel, "Click Level", { -60.f, 0.f }, -14.f, fm::db, fm::num));
    l.add (f (clickHz, "Click Tone", skew (500.f, 16000.f, 4000.f), 5000.f, fm::hz, fm::hzIn));
    l.add (f (clickLen, "Click Length", skew (0.3f, 30.f, 4.f), 4.f, fm::ms, fm::msIn));
    l.add (f (length, "Length", skew (80.f, 2000.f, 420.f), 410.f, fm::ms, fm::msIn));
    l.add (f (clipDrive, "Clip Drive", { 0.f, 24.f }, 6.f, fm::db, fm::num));
    l.add (f (clipSoft, "Clip Softness", { 0.f, 1.f }, 0.3f));
    l.add (f (velocity, "Velocity Sensitivity", { 0.f, 1.f }, 0.f));
    l.add (f (output, "Output", { -24.f, 24.f }, 0.f, fm::db, fm::num));
    return l;
}

// ------------------------------------------------------------ factory presets
namespace
{
    struct KickPreset { String name; PresetValues v; String amp, drive; };
    std::vector<KickPreset> buildPresets();
    const std::vector<KickPreset>& presets() { static const std::vector<KickPreset> p = buildPresets(); return p; }
}
#include "FactoryPresets.inc"

int ForgeProcessor::getNumPrograms() { return (int) presets().size(); }
const String ForgeProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : String(); }
void ForgeProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    const auto& pr = presets()[(size_t) i];
    presetName = pr.name;
    PresetIO::apply (*this, apvts, pr.v);
    Shape a, d;
    if (! Shape::fromString (pr.amp, a)) a = kick::KickParams().amp;
    if (! Shape::fromString (pr.drive, d)) d = kick::KickParams().drive;
    setShape (0, a); setShape (1, d);
}

Shape ForgeProcessor::getShape (int which) const { const SpinLock::ScopedLockType l (shapeLock); return shapes[(size_t) which]; }
void ForgeProcessor::setShape (int which, const Shape& s)
{
    { const SpinLock::ScopedLockType l (shapeLock); shapes[(size_t) which] = s; }
    apvts.state.setProperty (which == 0 ? "ampEnv" : "driveEnv", s.toString(), nullptr);
    shapeSerial.fetch_add (1);
}
void ForgeProcessor::applyShapesFromState()
{
    Shape a = kick::KickParams().amp, d = kick::KickParams().drive;
    Shape::fromString (apvts.state.getProperty ("ampEnv").toString(), a);
    Shape::fromString (apvts.state.getProperty ("driveEnv").toString(), d);
    setShape (0, a); setShape (1, d);
}

bool ForgeProcessor::loadUserPresetFile (const File& f)
{
    auto xml = XmlDocument::parse (f);
    if (xml == nullptr || ! PresetIO::load (*this, apvts, f)) return false;
    Shape a = kick::KickParams().amp, d = kick::KickParams().drive;
    Shape::fromString (xml->getStringAttribute ("ampEnv"), a);
    Shape::fromString (xml->getStringAttribute ("driveEnv"), d);
    setShape (0, a); setShape (1, d);
    presetName = f.getFileNameWithoutExtension();
    return true;
}
bool ForgeProcessor::saveUserPresetAs (const String& name)
{
    String clean;
    if (! io.save (apvts, name, clean)) return false;     // the envelopes are properties of the state -> saved with it
    presetName = clean;
    return true;
}
void ForgeProcessor::getStateInformation (MemoryBlock& d) { PresetIO::getState (apvts, d, currentProgram, presetName); }
void ForgeProcessor::setStateInformation (const void* data, int size)
{
    if (PresetIO::setState (*this, apvts, data, size, currentProgram, presetName)) applyShapesFromState();
}

void ForgeProcessor::randomizeSound()
{
    using namespace ParamIDs;
    auto set = [this] (const char* id, float v) { if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); } };
    auto between = [this] (float a, float b) { return a + rng.nextFloat() * (b - a); };
    auto logB = [this] (float a, float b) { return a * std::pow (b / a, rng.nextFloat()); };
    set (tailStart, logB (2.f, 6.f)); set (tailDrop, logB (25.f, 160.f)); set (tailCurve, between (2.f, 7.f)); set (tailWave, rng.nextFloat() < 0.6f ? 0.f : between (0.f, 0.6f));
    set (distType, (float) rng.nextInt (5)); set (drive, between (10.f, 40.f)); set (tone, between (-0.5f, 0.8f));
    set (lowCut, logB (20.f, 60.f)); set (highCut, logB (3000.f, 16000.f));
    set (scream, rng.nextFloat() < 0.4f ? between (0.2f, 0.8f) : 0.f); set (screamHz, logB (400.f, 2500.f));
    set (crunch, rng.nextFloat() < 0.5f ? between (0.2f, 0.9f) : 0.f); set (crunchDec, logB (40.f, 300.f));
    set (body, between (-2.f, 6.f)); set (grit, rng.nextFloat() < 0.5f ? between (0.05f, 0.5f) : 0.f);
    set (punchLevel, between (-14.f, -2.f)); set (punchStart, logB (200.f, 900.f)); set (punchEnd, logB (60.f, 150.f)); set (punchDrop, logB (4.f, 30.f)); set (punchDecay, logB (15.f, 80.f));
    set (clickLevel, between (-30.f, -8.f)); set (clickHz, logB (2000.f, 10000.f));
    set (length, logB (300.f, 600.f)); set (clipDrive, between (3.f, 12.f));
    // drive envelope: flat, fading or a reverse-bass swell
    const int kind = rng.nextInt (3);
    Shape d = kind == 0 ? Shape::make ({ { 0, 1 }, { 1000, 1 } })
            : kind == 1 ? Shape::make ({ { 0, 1 }, { 400, 0.6f }, { 1000, 0.3f } })
                        : Shape::make ({ { 0, 1 }, { 350, 0.25f }, { 700, 0.9f }, { 1000, 1 } });
    setShape (1, d);
    presetName = "Random #" + String (1000 + rng.nextInt (9000));
}

ForgeProcessor::ForgeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FORGE", createLayout())
{
    setCurrentProgram (0);
}

void ForgeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (gather());
    engine.prepare (sampleRate, samplesPerBlock);
}

ForgeParams ForgeProcessor::gather() const
{
    using namespace ParamIDs;
    ForgeParams e;
    auto& k = e.k;
    k.tailDb = get (tailLevel); k.startRatio = get (tailStart); k.dropMs = get (tailDrop); k.curve = get (tailCurve); k.wave = get (tailWave); k.tuneSemis = get (tune);
    k.distType = jlimit (0, 4, (int) std::round (get (distType))); k.driveDb = get (drive); k.tone = get (tone); k.lowCutHz = get (lowCut); k.highCutHz = get (highCut);
    k.scream = get (scream); k.screamHz = get (screamHz); k.crunch = get (crunch); k.crunchDecayMs = get (crunchDec);
    k.bodyDb = get (body); k.grit = get (grit);
    k.punchOn = get (punchOn) > 0.5f; k.punchDb = get (punchLevel); k.punchStartHz = get (punchStart); k.punchEndHz = get (punchEnd);
    k.punchDropMs = get (punchDrop); k.punchDecayMs = get (punchDecay);
    k.clickDb = get (clickLevel); k.clickHz = get (clickHz); k.clickMs = get (clickLen);
    k.lengthMs = get (length);
    { const SpinLock::ScopedTryLockType l (shapeLock); if (l.isLocked()) { k.amp = shapes[0]; k.drive = shapes[1]; } else { k.amp = lastAmp; k.drive = lastDrive; } }
    e.clipDriveDb = get (clipDrive); e.clipSoft = get (clipSoft); e.velocitySens = get (velocity); e.outDb = get (output);
    return e;
}

void ForgeProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();
    auto prm = gather();
    lastAmp = prm.k.amp; lastDrive = prm.k.drive;
    engine.setParams (prm);
    if (pendingAudition.exchange (false)) engine.audition (true, auditionNote);
    engine.process (buffer, midi);
}

AudioProcessorEditor* ForgeProcessor::createEditor() { return new ForgeEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ForgeProcessor(); }
