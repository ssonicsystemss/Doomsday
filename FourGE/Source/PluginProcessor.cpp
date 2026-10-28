#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;
using namespace doomsday;
namespace fm = doomsday::fmt;
using ge::Shape;

namespace
{
    String fmtX (float v, int) { return "x" + String (v, 2); }
    String fmtNum (float v, int) { return String (v, 1); }
    String offDb (float v, int) { return v <= -59.f ? String ("OFF") : String (v, 1) + " dB"; }
    String deg (float v, int) { return String (roundToInt (v)) + juce::String (CharPointer_UTF8 ("\xc2\xb0")); }
}

AudioProcessorValueTreeState::ParameterLayout GeProcessor::createLayout()
{
    using namespace ParamIDs;
    using make::f; using make::b; using make::c; using make::skew;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (f (tailLevel, "Tail Level", { -24.f, 6.f }, 0.f, fm::db, fm::num));
    l.add (f (tailStart, "Tail Start Pitch", skew (1.f, 12.f, 3.f), 3.5f, fmtX, fm::num));
    l.add (f (tailDrop, "Tail Pitch Drop", skew (2.f, 400.f, 60.f), 60.f, fm::ms, fm::msIn));
    l.add (f (tailHold, "Tail Pitch Hold", skew (0.f, 200.f, 30.f), 0.f, fm::ms, fm::msIn));
    l.add (f (tailCurve, "Tail Drop Curve", { 0.5f, 10.f }, 4.f, fmtNum, fm::num));
    l.add (f (tailWave, "Tail Wave (Sine > Tri > Saw)", { 0.f, 1.f }, 0.f));
    l.add (f (tune, "Tune", { -12.f, 12.f }, 0.f, fm::semi, fm::num));
    l.add (c (pitchMode, "Pitch Mode", Choices::pitchModes, 0));
    l.add (c (distType, "Distortion Type", Choices::distTypes, 1));
    l.add (f (midHz, "Mid EQ Freq", skew (80.f, 8000.f, 700.f), 800.f, fm::hz, fm::hzIn));
    l.add (f (midGain, "Mid EQ Gain", { -18.f, 18.f }, 0.f, fm::db, fm::num));
    l.add (f (midQ, "Mid EQ Q", skew (0.3f, 8.f, 1.2f), 1.f, fmtNum, fm::num));
    l.add (c (dist2Type, "Stage 2 Type", Choices::dist2Types, 0));
    l.add (f (drive2, "Stage 2 Drive", { 0.f, 36.f }, 6.f, fm::db, fm::num));
    l.add (f (room, "Room", { 0.f, 1.f }, 0.f));
    l.add (f (roomSize, "Room Size", { 0.f, 1.f }, 0.5f));
    l.add (b (roomInto, "Room Into Distortion", true));
    l.add (f (ott, "OTT", { 0.f, 1.f }, 0.f));
    l.add (f (smpLevel, "Sample Level", { -36.f, 6.f }, 0.f, fm::db, fm::num));
    l.add (f (smpPitch, "Sample Pitch", { -24.f, 24.f }, 0.f, fm::semi, fm::num));
    l.add (f (smpStart, "Sample Start", { 0.f, 200.f }, 0.f, fm::ms, fm::msIn));
    l.add (f (ParamIDs::drive, "Drive", { 0.f, 48.f }, 18.f, fm::db, fm::num));
    l.add (f (tone, "Tone", { -1.f, 1.f }, 0.f, fm::bip, fm::pctIn));
    l.add (f (lowCut, "Low Cut", skew (10.f, 300.f, 40.f), 25.f, fm::hz, fm::hzIn));
    l.add (f (highCut, "High Cut", skew (500.f, 20000.f, 5000.f), 12000.f, fm::hz, fm::hzIn));
    l.add (f (scream, "Scream", { 0.f, 1.f }, 0.f));
    l.add (f (screamHz, "Scream Freq", skew (150.f, 5000.f, 900.f), 900.f, fm::hz, fm::hzIn));
    l.add (f (crunch, "Crunch", { 0.f, 1.f }, 0.f));
    l.add (f (crunchDec, "Crunch Decay", skew (5.f, 600.f, 120.f), 120.f, fm::ms, fm::msIn));
    l.add (f (body, "Body", { -12.f, 12.f }, 0.f, fm::db, fm::num));
    l.add (f (grit, "Grit", { 0.f, 1.f }, 0.f));
    l.add (f (cleanSub, "Clean Sub", skew (0.f, 250.f, 80.f), 0.f, [] (float v, int) { return v < 20.f ? String ("OFF") : fm::hz (v, 0); }, fm::hzIn));
    l.add (b (punchOn, "Punch On", true));
    l.add (f (punchLevel, "Punch Level", { -36.f, 6.f }, -6.f, fm::db, fm::num));
    l.add (f (punchStart, "Punch Start", skew (50.f, 2000.f, 400.f), 400.f, fm::hz, fm::hzIn));
    l.add (f (punchEnd, "Punch End", skew (30.f, 1200.f, 120.f), 90.f, fm::hz, fm::hzIn));     // v2.1: up to 1.2 kHz (toks rise)
    l.add (f (punchDrop, "Punch Drop", skew (0.5f, 80.f, 12.f), 12.f, fm::ms, fm::msIn));
    l.add (f (punchDecay, "Punch Decay", skew (3.f, 300.f, 40.f), 40.f, fm::ms, fm::msIn));
    l.add (f (chaos, "Chaos", { 0.f, 1.f }, 0.f));
    l.add (f (chaosDec, "Chaos Decay", skew (3.f, 400.f, 60.f), 60.f, fm::ms, fm::msIn));
    l.add (f (chaosHz, "Chaos Tone", skew (150.f, 12000.f, 2500.f), 2500.f, fm::hz, fm::hzIn));
    l.add (f (punchDrive, "Punch Drive", { 0.f, 36.f }, 0.f, fm::db, fm::num));
    l.add (f (clickLevel, "Click Level", { -60.f, 0.f }, -14.f, fm::db, fm::num));
    l.add (f (clickHz, "Click Tone", skew (500.f, 16000.f, 4000.f), 5000.f, fm::hz, fm::hzIn));
    l.add (f (clickLen, "Click Length", skew (0.3f, 30.f, 4.f), 4.f, fm::ms, fm::msIn));
    l.add (f (length, "Length", skew (80.f, 2000.f, 420.f), 410.f, fm::ms, fm::msIn));
    l.add (f (clipDrive, "Clip Drive", { 0.f, 24.f }, 6.f, fm::db, fm::num));
    l.add (f (clipSoft, "Clip Softness", { 0.f, 1.f }, 0.3f));
    l.add (f (velocity, "Velocity Sensitivity", { 0.f, 1.f }, 0.f));
    l.add (f (output, "Output", { -24.f, 24.f }, 0.f, fm::db, fm::num));
    l.add (b (punchInto, "Punch Into Distortion", false));
    l.add (b (headOn, "Fixed Head", false));
    l.add (f (punchLen, "Punch Length", skew (0.f, 250.f, 80.f), 0.f, [] (float v, int) { return v < 0.5f ? String ("DECAY") : fm::ms (v, 0); }, fm::msIn));
    l.add (f (headLen, "Head Length", skew (20.f, 300.f, 110.f), 110.f, fm::ms, fm::msIn));
    l.add (f (headHz, "Head Pitch", skew (30.f, 120.f, 55.f), 49.f, [] (float v, int)
              {
                  static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                  const int n = roundToInt (69.0 + 12.0 * std::log2 (v / 440.0));
                  return String (v, 1) + " Hz (" + names[(n % 12 + 12) % 12] + String (n / 12 - 1) + ")";
              }, fm::hzIn));
    l.add (f (tailFine, "Tail Fine Tune", { -100.f, 100.f }, 0.f, [] (float v, int) { return (v > 0.5f ? "+" : "") + String (roundToInt (v)) + " ct"; }, fm::num));
    // ---- 4GE
    l.add (c (tailStartMode, "Tail Start Mode", Choices::startModes, 0));
    l.add (f (tailStartHz, "Tail Start Hz", skew (40.f, 1500.f, 250.f), 250.f, fm::hz, fm::hzIn));
    l.add (f (oscALevel, "Osc A Level", { -60.f, 6.f }, 0.f, offDb, fm::num));
    l.add (f (oscBLevel, "Osc B Level", { -60.f, 6.f }, -60.f, offDb, fm::num));
    l.add (c (oscBType, "Osc B Type", Choices::oscBTypes, 0));
    l.add (f (oscBWidth, "Osc B Pulse Width", { 0.05f, 0.95f }, 0.5f, fm::pct, fm::pctIn));
    l.add (f (tailPhase, "Tail Phase", { 0.f, 360.f }, 0.f, deg, fm::num));
    l.add (f (tailAttack, "Tail Attack", skew (0.f, 20.f, 3.f), 0.f, fm::ms, fm::msIn));
    l.add (f (duck, "Tail Duck Under Punch", { 0.f, 1.f }, 0.f, fm::pct, fm::pctIn));
    l.add (c (preType, "Pre Filter Type", Choices::filterTypes, 0));
    l.add (f (preHz, "Pre Filter Cutoff", skew (30.f, 16000.f, 900.f), 800.f, fm::hz, fm::hzIn));
    l.add (f (preReso, "Pre Filter Resonance", { 0.f, 1.f }, 0.2f, fm::pct, fm::pctIn));
    l.add (f (preEnv, "Pre Filter Envelope", { -1.f, 1.f }, 0.f, fm::bip, fm::pctIn));
    l.add (f (preDecay, "Pre Filter Env Decay", skew (3.f, 800.f, 80.f), 80.f, fm::ms, fm::msIn));
    l.add (f (bias1, "Stage 1 Bias", { -1.f, 1.f }, 0.f, fm::bip, fm::pctIn));
    l.add (f (mix1, "Stage 1 Mix", { 0.f, 1.f }, 1.f, fm::pct, fm::pctIn));
    l.add (f (bias2, "Stage 2 Bias", { -1.f, 1.f }, 0.f, fm::bip, fm::pctIn));
    l.add (f (mix2, "Stage 2 Mix", { 0.f, 1.f }, 1.f, fm::pct, fm::pctIn));
    l.add (b (autoGain, "Auto Gain", true));
    l.add (f (subLevel, "Sub Level", { -60.f, 6.f }, 0.f, offDb, fm::num));
    l.add (f (roomLowCut, "Room Low Cut", skew (20.f, 1000.f, 200.f), 180.f, fm::hz, fm::hzIn));
    l.add (c (punchType, "Punch Type", Choices::punchTypes, 0));
    l.add (f (punchAmount, "Punch Amount", { 0.f, 1.f }, 1.f, fm::pct, fm::pctIn));
    l.add (f (punchAttack, "Punch Attack", skew (0.f, 10.f, 2.f), 0.f, fm::ms, fm::msIn));
    l.add (f (punchPhase, "Punch Phase", { 0.f, 360.f }, 0.f, deg, fm::num));
    l.add (c (punchFiltType, "Punch Filter Type", Choices::filterTypes, 0));
    l.add (f (punchFiltHz, "Punch Filter Cutoff", skew (30.f, 16000.f, 900.f), 1000.f, fm::hz, fm::hzIn));
    l.add (f (punchFiltReso, "Punch Filter Resonance", { 0.f, 1.f }, 0.2f, fm::pct, fm::pctIn));
    l.add (c (punchDistType, "Punch Drive Type", Choices::distTypes, 0));
    l.add (c (texSource, "Texture Source", Choices::texSources, 0));
    l.add (f (texLevel, "Texture Level", { -60.f, 6.f }, -60.f, offDb, fm::num));
    l.add (f (texHz, "Texture Freq", skew (100.f, 16000.f, 2500.f), 3000.f, fm::hz, fm::hzIn));
    l.add (f (texReso, "Texture Resonance", { 0.f, 1.f }, 0.3f, fm::pct, fm::pctIn));
    l.add (f (texDecay, "Texture Decay", skew (3.f, 1000.f, 80.f), 60.f, fm::ms, fm::msIn));
    l.add (f (texDrive, "Texture Drive", { 0.f, 36.f }, 0.f, fm::db, fm::num));
    l.add (f (texSemis, "Texture Pitch", { -24.f, 36.f }, 12.f, fm::semi, fm::num));
    l.add (f (texHp, "Texture Low Cut", skew (40.f, 2000.f, 250.f), 200.f, fm::hz, fm::hzIn));
    l.add (c (busType, "Bus Distortion", Choices::dist2Types, 0));
    l.add (f (busDrive, "Bus Drive", { 0.f, 30.f }, 6.f, fm::db, fm::num));
    l.add (f (busMix, "Bus Mix", { 0.f, 1.f }, 1.f, fm::pct, fm::pctIn));
    l.add (f (uniVoices, "Unison Voices", { 1.f, 7.f, 1.f }, 1.f, [] (float v, int) { return v < 1.5f ? String ("OFF") : String (roundToInt (v)); }, fm::num));
    l.add (f (uniDetune, "Unison Detune", { 0.f, 60.f }, 18.f, fm::cents, fm::num));
    l.add (f (uniDrift, "Unison Drift", { 0.f, 30.f }, 6.f, fm::cents, fm::num));
    l.add (f (uniMix, "Unison Mix", { 0.f, 1.f }, 0.6f, fm::pct, fm::pctIn));
    l.add (f (width, "Width", { 0.f, 1.f }, 0.4f, fm::pct, fm::pctIn));
    l.add (f (widthSplit, "Width From", skew (80.f, 1500.f, 350.f), 350.f, fm::hz, fm::hzIn));
    return l;
}

// ------------------------------------------------------------ factory presets
namespace
{
    struct KickPreset { String name; PresetValues v; String amp, drive, pitch = {}; };
    std::vector<KickPreset> buildPresets();
    const std::vector<KickPreset>& presets() { static const std::vector<KickPreset> p = buildPresets(); return p; }
}
#include "FactoryPresets.inc"

int GeProcessor::getNumPrograms() { return (int) presets().size(); }
const String GeProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : String(); }
void GeProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    const auto& pr = presets()[(size_t) i];
    presetName = pr.name;
    PresetIO::apply (*this, apvts, pr.v);
    Shape a, d, pt;
    if (! Shape::fromString (pr.amp, a)) a = ge::Params().amp;
    if (! Shape::fromString (pr.drive, d)) d = ge::Params().drive;
    if (! Shape::fromString (pr.pitch, pt)) pt = ge::Params().pitch;
    setShape (0, a); setShape (1, d); setShape (2, pt);
}

Shape GeProcessor::getShape (int which) const { const SpinLock::ScopedLockType l (shapeLock); return shapes[(size_t) which]; }
void GeProcessor::setShape (int which, const Shape& s)
{
    { const SpinLock::ScopedLockType l (shapeLock); shapes[(size_t) which] = s; }
    apvts.state.setProperty (shapeKey (which), s.toString(), nullptr);
    shapeSerial.fetch_add (1);
}
void GeProcessor::applyShapesFromState()
{
    const ge::Params def;
    Shape sh[3] = { def.amp, def.drive, def.pitch };
    for (int i = 0; i < 3; ++i) { Shape::fromString (apvts.state.getProperty (shapeKey (i)).toString(), sh[i]); setShape (i, sh[i]); }
    loadSample (File (apvts.state.getProperty ("samplePath").toString()));
}

bool GeProcessor::loadUserPresetFile (const File& f)
{
    auto xml = XmlDocument::parse (f);
    if (xml == nullptr || ! PresetIO::load (*this, apvts, f)) return false;
    const ge::Params def;
    Shape sh[3] = { def.amp, def.drive, def.pitch };
    for (int i = 0; i < 3; ++i) { Shape::fromString (xml->getStringAttribute (shapeKey (i)), sh[i]); setShape (i, sh[i]); }
    loadSample (File (xml->getStringAttribute ("samplePath")));
    presetName = f.getFileNameWithoutExtension();
    return true;
}
bool GeProcessor::saveUserPresetAs (const String& name)
{
    String clean;
    if (! io.save (apvts, name, clean)) return false;     // the envelopes are properties of the state -> saved with it
    presetName = clean;
    return true;
}
void GeProcessor::getStateInformation (MemoryBlock& d) { PresetIO::getState (apvts, d, currentProgram, presetName); }
void GeProcessor::setStateInformation (const void* data, int size)
{
    if (PresetIO::setState (*this, apvts, data, size, currentProgram, presetName)) applyShapesFromState();
}

void GeProcessor::randomizeSound()
{
    using namespace ParamIDs;
    auto set = [this] (const char* id, float v) { if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); } };
    auto between = [this] (float a, float b) { return a + rng.nextFloat() * (b - a); };
    auto logB = [this] (float a, float b) { return a * std::pow (b / a, rng.nextFloat()); };
    set (tailStart, logB (2.f, 6.f)); set (tailDrop, logB (25.f, 160.f)); set (tailCurve, between (2.f, 7.f)); set (tailWave, rng.nextFloat() < 0.6f ? 0.f : between (0.f, 0.6f));
    set (distType, (float) rng.nextInt (5)); set (ParamIDs::drive, between (10.f, 40.f)); set (tone, between (-0.5f, 0.8f));
    set (lowCut, logB (20.f, 60.f)); set (highCut, logB (3000.f, 16000.f));
    set (scream, rng.nextFloat() < 0.4f ? between (0.2f, 0.8f) : 0.f); set (screamHz, logB (400.f, 2500.f));
    set (crunch, rng.nextFloat() < 0.5f ? between (0.2f, 0.9f) : 0.f); set (crunchDec, logB (40.f, 300.f));
    set (body, between (-2.f, 6.f)); set (grit, rng.nextFloat() < 0.5f ? between (0.05f, 0.5f) : 0.f);
    set (punchInto, rng.nextFloat() < 0.7f ? 1.f : 0.f);
    set (headOn, rng.nextFloat() < 0.7f ? 1.f : 0.f); set (headLen, between (80.f, 140.f));
    set (punchLevel, between (-14.f, -2.f)); set (punchStart, logB (200.f, 900.f)); set (punchEnd, logB (60.f, 150.f)); set (punchDrop, logB (4.f, 30.f)); set (punchDecay, logB (15.f, 80.f));
    set (chaos, rng.nextFloat() < 0.7f ? between (0.2f, 0.8f) : 0.f); set (chaosDec, logB (20.f, 150.f)); set (chaosHz, logB (800.f, 6000.f));
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

GeProcessor::GeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "FOURGE", createLayout())
{
    setCurrentProgram (0);
}

void GeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // FL may call this while an audio callback is still running (Replace, sample-rate change, loading a project):
    // processBlock outputs silence instead of rendering into voices that are being re-prepared (sonic-toolbox bugs/fl-replace-crash.md)
    const SpinLock::ScopedLockType l (prepareLock);
    engine.setParams (gather());
    engine.prepare (sampleRate, samplesPerBlock);
}

GeParams GeProcessor::gatherForGui() const
{
    auto e = gather();
    const SpinLock::ScopedLockType l (shapeLock);
    e.k.amp = shapes[0]; e.k.drive = shapes[1]; e.k.pitch = shapes[2];
    return e;
}

GeParams GeProcessor::gather() const
{
    using namespace ParamIDs;
    GeParams e;
    auto& k = e.k;
    k.tailDb = get (tailLevel); k.startRatio = get (tailStart); k.dropMs = get (tailDrop); k.curve = get (tailCurve); k.holdMs = get (tailHold); k.wave = get (tailWave); k.tuneSemis = get (tune) + get (tailFine) / 100.f;
    k.distType = jlimit (0, 5, (int) std::round (get (distType))); k.driveDb = get (ParamIDs::drive); k.tone = get (tone); k.lowCutHz = get (lowCut); k.highCutHz = get (highCut);
    k.scream = get (scream); k.screamHz = get (screamHz); k.crunch = get (crunch); k.crunchDecayMs = get (crunchDec);
    k.bodyDb = get (body); k.grit = get (grit); k.cleanSubHz = get (cleanSub);
    k.punchOn = get (punchOn) > 0.5f; k.punchDb = get (punchLevel); k.punchStartHz = get (punchStart); k.punchEndHz = get (punchEnd);
    k.punchDropMs = get (punchDrop); k.punchDecayMs = get (punchDecay);
    k.punchIntoDist = get (punchInto) > 0.5f; k.punchLenMs = get (punchLen);
    k.headFixed = get (headOn) > 0.5f; k.headMs = get (headLen); k.headHz = get (headHz);
    k.chaos = get (chaos); k.chaosMs = get (chaosDec); k.chaosHz = get (chaosHz);
    k.clickDb = get (clickLevel); k.clickHz = get (clickHz); k.clickMs = get (clickLen);
    k.lengthMs = get (length);
    { const SpinLock::ScopedTryLockType l (shapeLock); if (l.isLocked()) { k.amp = shapes[0]; k.drive = shapes[1]; k.pitch = shapes[2]; } else { k.amp = lastAmp; k.drive = lastDrive; k.pitch = lastPitch; } }
    k.pitchDrawn = get (pitchMode) > 0.5f;
    k.midHz = get (midHz); k.midDb = get (midGain); k.midQ = get (midQ);
    k.dist2Type = jlimit (0, 6, (int) std::round (get (dist2Type))); k.drive2Db = get (drive2);
    k.room = get (room); k.roomSize = get (roomSize); k.roomIntoDist = get (roomInto) > 0.5f;
    k.punchDriveDb = get (punchDrive);
    auto ch = [this] (const char* id, int hi) { return jlimit (0, hi, (int) std::round (get (id))); };
    k.startFixed = get (tailStartMode) > 0.5f; k.startHz = get (tailStartHz);
    k.oscADb = get (oscALevel); k.oscBDb = get (oscBLevel); k.oscBType = ch (oscBType, 2); k.oscBWidth = get (oscBWidth);
    k.tailPhase = get (tailPhase); k.tailAttackMs = get (tailAttack); k.duck = get (duck);
    k.preType = ch (preType, 4); k.preHz = get (preHz); k.preReso = get (preReso); k.preEnv = get (preEnv); k.preDecayMs = get (preDecay);
    k.bias1 = get (bias1); k.mix1 = get (mix1); k.bias2 = get (bias2); k.mix2 = get (mix2); k.autoGain = get (autoGain) > 0.5f;
    k.subDb = get (subLevel); k.roomLowCutHz = get (roomLowCut);
    k.punchType = ch (punchType, 4); k.punchAmount = get (punchAmount); k.punchAttackMs = get (punchAttack); k.punchPhase = get (punchPhase);
    k.punchFiltType = ch (punchFiltType, 4); k.punchFiltHz = get (punchFiltHz); k.punchFiltReso = get (punchFiltReso); k.punchDistType = ch (punchDistType, 5);
    k.texSource = ch (texSource, 3); k.texDb = get (texLevel); k.texHz = get (texHz); k.texReso = get (texReso); k.texDecayMs = get (texDecay);
    k.texDriveDb = get (texDrive); k.texSemis = get (texSemis); k.texHpHz = get (texHp);
    k.busType = ch (busType, 6); k.busDriveDb = get (busDrive); k.busMix = get (busMix);
    k.uniVoices = roundToInt (get (uniVoices)); k.uniDetuneCt = get (uniDetune); k.driftCt = get (uniDrift); k.uniMix = get (uniMix);
    e.width = get (width); e.splitHz = get (widthSplit);
    k.mute = effectiveMute();
    e.ott = get (ott); e.sampleDb = get (smpLevel); e.samplePitch = get (smpPitch); e.sampleStartMs = get (smpStart);
    e.clipDriveDb = get (clipDrive); e.clipSoft = get (clipSoft); e.velocitySens = get (velocity); e.outDb = get (output);
    return e;
}

void GeProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();
    const SpinLock::ScopedTryLockType prepared (prepareLock);
    if (! prepared.isLocked()) return;
    auto prm = gather();
    lastAmp = prm.k.amp; lastDrive = prm.k.drive; lastPitch = prm.k.pitch;
    engine.setParams (prm);
    if (pendingAudition.exchange (false)) engine.audition (true, auditionNote);
    engine.process (buffer, midi);
}

const char* GeProcessor::shapeKey (int which) { return which == 0 ? "ampEnv" : which == 1 ? "driveEnv" : "pitchEnv"; }

// message thread: read a WAV / AIFF / FLAC into a mono buffer (max 4 s) and hand it to the engine
bool GeProcessor::loadSample (const File& f)
{
    if (! f.existsAsFile()) { if (f.getFullPathName().isEmpty()) clearSample(); return false; }
    AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) return false;
    const int len = (int) std::min<long long> ((long long) r->lengthInSamples, (long long) (r->sampleRate * 4.0));
    const int nch = r->numChannels > 0 ? (int) r->numChannels : 1;
    AudioBuffer<float> b (nch, len);
    r->read (&b, 0, len, 0, true, true);
    auto s = std::make_unique<GeSample>();
    s->rate = r->sampleRate; s->name = f.getFileNameWithoutExtension();
    s->data.resize ((size_t) len);
    for (int i = 0; i < len; ++i)
    {
        float v = 0.f;
        for (int ch = 0; ch < b.getNumChannels(); ++ch) v += b.getSample (ch, i);
        s->data[(size_t) i] = v / (float) b.getNumChannels();
    }
    engine.setSample (std::move (s));
    apvts.state.setProperty ("samplePath", f.getFullPathName(), nullptr);
    sampleSerial.fetch_add (1);
    return true;
}
void GeProcessor::clearSample()
{
    engine.setSample (nullptr);
    apvts.state.setProperty ("samplePath", String(), nullptr);
    sampleSerial.fetch_add (1);
}
juce::String GeProcessor::sampleName() const
{
    auto* s = engine.currentSample();
    return s != nullptr ? s->name : String();
}

AudioProcessorEditor* GeProcessor::createEditor() { return new GeEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GeProcessor(); }
