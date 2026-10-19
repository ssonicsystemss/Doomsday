#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using namespace doomsday;
namespace fm = doomsday::fmt;

AudioProcessorValueTreeState::ParameterLayout SerpentProcessor::createLayout()
{
    using namespace ParamIDs;
    using make::f; using make::b; using make::c; using make::i; using make::skew;
    AudioProcessorValueTreeState::ParameterLayout l;
    // CORE
    l.add (c (coreWave, "Core Wave", Choices::coreWaves, 0));
    l.add (i (coreVoices, "Core Voices", 1, 16, 12));
    l.add (f (coreDetune, "Core Detune", { 0.f, 1.f }, 0.75f));
    l.add (f (coreOct, "Core Octave Layer", { 0.f, 1.f }, 1.f));
    l.add (f (coreLevel, "Core Level", { -48.f, 6.f }, -5.f, fm::db, fm::num));
    // EDGE
    l.add (b (edgeOn, "Edge On", true));
    l.add (i (edgeVoices, "Edge Voices", 1, 16, 9));
    l.add (f (edgeDetune, "Edge Detune", { 0.f, 1.f }, 0.85f));
    l.add (c (edgeOct, "Edge Octave", Choices::edgeOcts, 0));
    l.add (f (edgeTone, "Edge Tone", skew (200.f, 6000.f, 1200.f), 3000.f, fm::hz, fm::hzIn));
    l.add (f (edgeLevel, "Edge Level", { -48.f, 6.f }, -3.f, fm::db, fm::num));
    // BODY
    l.add (b (bodyOn, "Body On", true));
    l.add (c (bodyWave, "Body Wave", Choices::bodyWaves, 0));
    l.add (i (bodyOct, "Body Octave", -1, 0, 0, true));
    l.add (f (bodyLevel, "Body Level", { -48.f, 6.f }, -14.f, fm::db, fm::num));
    // FILTER + TALK
    l.add (f (cutoff, "Cutoff", skew (60.f, 20000.f, 2000.f), 20000.f, fm::hz, fm::hzIn));
    l.add (f (reso, "Resonance", { 0.f, 1.f }, 0.15f));
    l.add (f (tone, "Tone", { -1.f, 1.f }, 0.6f, fm::bip, fm::pctIn));
    l.add (f (fenv, "Filter Env", { -4.f, 4.f }, 1.f, fm::oct, fm::num));
    l.add (f (fdecay, "Filter Decay", skew (5.f, 3000.f, 300.f), 400.f, fm::ms, fm::msIn));
    l.add (f (talk, "Talk", { 0.f, 1.f }, 0.15f));
    l.add (f (vowel, "Vowel", { 0.f, 1.f }, 0.f, [] (float v, int) { const int k = jlimit (0, 4, (int) std::round (v * 4.f));
                                                                      return String::charToString (Choices::vowels[k]) + " " + String ((int) std::round (v * 100.f)); },
              fm::pctIn));
    l.add (f (sweep, "Vowel Sweep", { -1.f, 1.f }, 0.f, fm::bip, fm::pctIn));
    l.add (f (sweepTime, "Sweep Time", skew (10.f, 2000.f, 250.f), 250.f, fm::ms, fm::msIn));
    l.add (f (talkKey, "Talk Key Track", { 0.f, 1.f }, 0.4f));
    // MOVE
    l.add (b (moveOn, "Move On", true));
    l.add (c (moveDiv, "Move Rate", Choices::moveDivs, 3));
    l.add (c (moveShape, "Move Shape", Choices::shapes, 0));
    l.add (f (moveCut, "Move > Cutoff", { -4.f, 4.f }, 1.f, fm::oct, fm::num));
    l.add (f (moveVowel, "Move > Vowel", { -1.f, 1.f }, 0.2f, fm::bip, fm::pctIn));
    l.add (f (moveAmp, "Move > Amp", { 0.f, 1.f }, 0.f));
    l.add (b (moveRetrig, "Move Retrigger", true));
    // PLAY
    l.add (c (mode, "Play Mode", Choices::modes, 0));
    l.add (f (glide, "Glide", skew (0.f, 1000.f, 120.f), 40.f, fm::ms, fm::msIn));
    l.add (f (bendAmt, "Bend", { -12.f, 12.f, 0.1f }, 0.f, fm::semi, fm::num));
    l.add (f (bendTime, "Bend Time", skew (5.f, 1000.f, 120.f), 80.f, fm::ms, fm::msIn));
    l.add (b (bendFirst, "Bend First Note Only", true));
    l.add (f (vibDepth, "Vibrato Depth", { 0.f, 100.f }, 8.f, fm::cents, fm::num));
    l.add (f (vibRate, "Vibrato Rate", skew (0.5f, 12.f, 5.f), 5.f, fm::lfoHz, fm::num));
    l.add (f (vibDelay, "Vibrato Delay", skew (0.f, 2000.f, 300.f), 250.f, fm::ms, fm::msIn));
    l.add (f (velocity, "Velocity", { 0.f, 1.f }, 0.3f));
    // RAGE
    l.add (b (rageOn, "Rage On", true));
    l.add (c (rageType, "Rage Type", Choices::rages, 0));
    l.add (f (rageDrive, "Rage Drive", { 0.f, 30.f }, 8.f, fm::db, fm::num));
    l.add (f (rageMix, "Rage Mix", { 0.f, 1.f }, 0.5f));
    l.add (f (ott, "OTT", { 0.f, 1.f }, 0.5f));
    // AMP
    l.add (f (attack, "Attack", skew (0.3f, 5000.f, 120.f), 3.f, fm::ms, fm::msIn));
    l.add (f (decay, "Decay", skew (5.f, 5000.f, 500.f), 600.f, fm::ms, fm::msIn));
    l.add (f (sustain, "Sustain", { 0.f, 1.f }, 1.f));
    l.add (f (release, "Release", skew (5.f, 8000.f, 500.f), 200.f, fm::ms, fm::msIn));
    // SPACE
    l.add (f (width, "Width", { 0.f, 1.f }, 1.f));
    l.add (f (monoBass, "Mono Bass", skew (0.f, 600.f, 150.f), 180.f, [] (float v, int) { return v < 20.f ? String ("OFF") : fm::hz (v, 0); }, fm::hzIn));
    // FX
    l.add (b (dlyOn, "Delay On", true));
    l.add (c (dlyDiv, "Delay Time", Choices::delayDivs, 3));
    l.add (f (dlyFb, "Delay Feedback", { 0.f, 0.9f }, 0.3f));
    l.add (f (dlyMix, "Delay Mix", { 0.f, 1.f }, 0.12f));
    l.add (b (revOn, "Reverb On", true));
    l.add (f (revSize, "Reverb Size", { 0.f, 1.f }, 0.7f));
    l.add (f (revMix, "Reverb Mix", { 0.f, 1.f }, 0.18f));
    l.add (b (pumpOn, "Pump On", false));
    l.add (c (pumpDiv, "Pump Rate", Choices::pumpDivs, 1));
    l.add (f (pumpDepth, "Pump Depth", { 0.f, 1.f }, 0.6f));
    // global
    l.add (f (energy, "Energy", { 0.f, 1.f }, 0.f));
    l.add (f (volume, "Volume", { -36.f, 6.f }, -6.f, fm::db, fm::num));
    return l;
}

// ------------------------------------------------------------ factory presets
namespace
{
    struct Preset { const char* name; PresetValues values; };
    const std::vector<Preset>& presets()
    {
        using namespace ParamIDs;
        static const std::vector<Preset> list {
            { "Venom Lead", {} },
            { "Raw Serpent", { { rageType, 1 }, { rageDrive, 12 }, { rageMix, 0.7f }, { talk, 0.55f }, { sweep, 0.5f }, { sweepTime, 180 },
                               { bendAmt, -5 }, { bendTime, 90 }, { moveDiv, 4 }, { moveCut, 1.4f }, { ott, 0.5f }, { coreOct, 0.6f },
                               { cutoff, 7000 }, { reso, 0.3f } } },
            { "Hiss of Chaos", { { rageType, 2 }, { rageDrive, 8 }, { rageMix, 0.5f }, { edgeOct, 1 }, { edgeLevel, -9 }, { vowel, 0.5f },
                                 { moveDiv, 6 }, { moveVowel, 0.8f }, { moveCut, 0.6f }, { talk, 0.6f } } },
            { "Cobra Choir", { { talk, 0.8f }, { vowel, 0.75f }, { sweep, -0.6f }, { sweepTime, 600 }, { moveDiv, 1 }, { moveVowel, 0.5f },
                               { moveCut, 0.4f }, { coreVoices, 16 }, { coreDetune, 0.7f }, { revSize, 0.9f }, { revMix, 0.35f },
                               { attack, 40 }, { release, 600 }, { rageDrive, 2 } } },
            { "Shedding Skin", { { moveDiv, 6 }, { moveShape, 3 }, { moveAmp, 0.85f }, { moveCut, 0.8f }, { talk, 0.45f }, { vowel, 0.25f },
                                 { rageDrive, 6 }, { ott, 0.45f }, { dlyMix, 0.2f } } },
            { "Coil Stab", { { decay, 260 }, { sustain, 0.15f }, { release, 180 }, { fenv, 2.5f }, { fdecay, 200 }, { cutoff, 4000 },
                             { talk, 0.5f }, { sweep, 0.6f }, { sweepTime, 120 }, { mode, 1 }, { dlyDiv, 3 }, { dlyMix, 0.25f }, { moveOn, 0 } } },
            { "Black Smoke", { { cutoff, 2600 }, { vowel, 1.f }, { talk, 0.55f }, { bodyWave, 2 }, { bodyOct, -1 }, { bodyLevel, -10 },
                               { edgeOn, 0 }, { rageType, 0 }, { rageDrive, 10 }, { rageMix, 0.6f }, { moveCut, 0.7f }, { moveDiv, 2 } } },
            { "Serpent Octaves", { { mode, 2 }, { glide, 0 }, { coreVoices, 7 }, { talk, 0.4f }, { vowel, 0.25f }, { rageDrive, 6 } } },
            { "Init", { { coreLevel, -10 }, { talk, 0.f }, { moveOn, 0 }, { rageOn, 0 }, { ott, 0.f }, { edgeOn, 0 }, { bodyOn, 0 }, { dlyOn, 0 }, { revOn, 0 },
                        { vibDepth, 0 } } },
        };
        return list;
    }
}

int SerpentProcessor::getNumPrograms() { return (int) presets().size(); }
const String SerpentProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? String (presets()[(size_t) i].name) : String(); }
void SerpentProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    presetName = presets()[(size_t) i].name;
    PresetIO::apply (*this, apvts, presets()[(size_t) i].values);
}

bool SerpentProcessor::loadUserPresetFile (const File& f)
{
    if (! PresetIO::load (*this, apvts, f)) return false;
    presetName = f.getFileNameWithoutExtension();
    return true;
}
bool SerpentProcessor::saveUserPresetAs (const String& name)
{
    String clean;
    if (! io.save (apvts, name, clean)) return false;
    presetName = clean;
    return true;
}

// ------------------------------------------------------------ engine
SerpentProcessor::SerpentProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SERPENT", createLayout())
{
    setCurrentProgram (0);
}

void SerpentProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (gather (150.0, -1.0));
    engine.prepare (sampleRate, samplesPerBlock);
}

SerpentParams SerpentProcessor::gather (double bpm, double ppq) const
{
    using namespace ParamIDs;
    auto on = [this] (const char* id) { return get (id) > 0.5f; };
    auto ix = [this] (const char* id) { return (int) std::round (get (id)); };
    auto gain = [this] (const char* id) { return get (id) <= -47.9f ? 0.f : Decibels::decibelsToGain (get (id)); };
    SerpentParams s;
    s.coreWave = Choices::coreWaveIds[jlimit (0, 3, ix (coreWave))];
    s.coreVoices = ix (coreVoices); s.coreDetune = get (coreDetune); s.coreOct = get (coreOct); s.coreGain = gain (coreLevel);
    s.edgeOn = on (edgeOn); s.edgeVoices = ix (edgeVoices); s.edgeDetune = get (edgeDetune); s.edgeOct = 1 + jlimit (0, 1, ix (edgeOct));
    s.edgeToneHz = get (edgeTone); s.edgeGain = gain (edgeLevel);
    s.bodyOn = on (bodyOn); s.bodyWave = Choices::bodyWaveIds[jlimit (0, 2, ix (bodyWave))]; s.bodyOct = ix (bodyOct); s.bodyGain = gain (bodyLevel);
    s.cutoff = get (cutoff); s.tone = get (tone); s.reso = get (reso); s.fenvOct = get (fenv); s.fdecayMs = get (fdecay);
    s.talk = get (talk); s.vowel = get (vowel); s.sweep = get (sweep); s.sweepMs = get (sweepTime); s.talkKey = get (talkKey);
    s.moveOn = on (moveOn); s.moveBeats = Choices::moveBeats[jlimit (0, Choices::moveDivs.size() - 1, ix (moveDiv))];
    s.moveShape = ix (moveShape); s.moveCut = get (moveCut); s.moveVowel = get (moveVowel); s.moveAmp = get (moveAmp); s.moveRetrig = on (moveRetrig);
    s.mode = jlimit (0, 2, ix (mode)); s.glideMs = get (glide); s.bendSt = get (bendAmt); s.bendMs = get (bendTime); s.bendFirst = on (bendFirst);
    s.vibCents = get (vibDepth); s.vibHz = get (vibRate); s.vibDelayMs = get (vibDelay); s.velAmt = get (velocity);
    s.rageOn = on (rageOn); s.rageType = jlimit (0, 2, ix (rageType)); s.rageDb = get (rageDrive); s.rageMix = get (rageMix); s.ott = get (ott);
    s.attackMs = get (attack); s.decayMs = get (decay); s.sustain = get (sustain); s.releaseMs = get (release);
    s.width = get (width); s.monoBassHz = get (monoBass);
    s.fx.dlyOn = on (dlyOn); s.fx.dlyBeats = Choices::delayBeats[jlimit (0, Choices::delayDivs.size() - 1, ix (dlyDiv))];
    s.fx.dlyFb = get (dlyFb); s.fx.dlyMix = get (dlyMix); s.fx.dlyPP = true;
    s.fx.revOn = on (revOn); s.fx.revSize = get (revSize); s.fx.revMix = get (revMix); s.fx.revDamp = 0.45f;
    s.fx.pumpOn = on (pumpOn); s.fx.pumpBeats = Choices::pumpBeats[jlimit (0, Choices::pumpDivs.size() - 1, ix (pumpDiv))];
    s.fx.pumpDepth = get (pumpDepth); s.fx.pumpShape = 0.5f;
    s.energy = get (energy);
    s.volume = Decibels::decibelsToGain (get (volume), -60.f);
    s.bpm = bpm; s.ppq = ppq;
    return s;
}

void SerpentProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, buffer.getNumSamples());
    if (buffer.getNumChannels() < 2) { buffer.clear(); return; }
    double bpm, ppq;
    PresetIO::hostTime (getPlayHead(), bpm, ppq);

    const int cmd = auditionCmd.exchange (0);       // PLAY: a C4 + C5 octave (POLY) or C4
    if (cmd == 1 && ! auditionHeld) { midi.addEvent (MidiMessage::noteOn (1, 60, (uint8) 110), 0); midi.addEvent (MidiMessage::noteOn (1, 72, (uint8) 110), 0); auditionHeld = true; }
    if (cmd == 2 && auditionHeld)   { midi.addEvent (MidiMessage::noteOff (1, 60), 0); midi.addEvent (MidiMessage::noteOff (1, 72), 0); auditionHeld = false; }

    engine.setParams (gather (bpm, ppq));
    AudioBuffer<float> stereo (buffer.getArrayOfWritePointers(), 2, buffer.getNumSamples());
    engine.process (stereo, midi);
}

// ------------------------------------------------------------ randomize
void SerpentProcessor::randomizeSound()
{
    using namespace ParamIDs;
    auto set = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); }
    };
    auto between = [this] (float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };
    auto chance = [this] (float p) { return rng.nextFloat() < p ? 1.f : 0.f; };
    set (coreWave, (float) (rng.nextFloat() < 0.6f ? 0 : rng.nextInt (4)));
    set (coreVoices, (float) (5 + rng.nextInt (12)));
    set (coreDetune, between (0.3f, 0.8f));
    set (coreOct, between (0.3f, 1.f));
    set (edgeOn, chance (0.7f)); set (edgeVoices, (float) (3 + rng.nextInt (10))); set (edgeOct, chance (0.3f));
    set (edgeTone, between (800.f, 3000.f)); set (edgeLevel, between (-18.f, -8.f));
    set (bodyOn, chance (0.6f)); set (bodyWave, (float) rng.nextInt (3)); set (bodyOct, -chance (0.4f)); set (bodyLevel, between (-20.f, -10.f));
    set (cutoff, 2500.f * std::pow (5.f, rng.nextFloat())); set (reso, between (0.f, 0.45f));
    set (fenv, between (0.f, 2.f)); set (fdecay, between (150.f, 700.f));
    set (talk, between (0.15f, 0.8f)); set (vowel, rng.nextFloat()); set (sweep, chance (0.5f) * between (-0.7f, 0.7f));
    set (sweepTime, between (80.f, 600.f));
    set (moveOn, chance (0.8f)); set (moveDiv, (float) (1 + rng.nextInt (6))); set (moveShape, (float) rng.nextInt (5));
    set (moveCut, between (0.f, 1.8f)); set (moveVowel, between (-0.6f, 0.6f)); set (moveAmp, chance (0.2f) * between (0.3f, 0.9f));
    set (bendAmt, chance (0.35f) * between (-7.f, 3.f)); set (vibDepth, between (0.f, 20.f));
    set (rageOn, chance (0.85f)); set (rageType, (float) rng.nextInt (3)); set (rageDrive, between (2.f, 14.f)); set (rageMix, between (0.2f, 0.8f));
    set (ott, between (0.1f, 0.6f)); set (width, between (0.6f, 1.f));
    presetName = "Random #" + String (1000 + rng.nextInt (9000));
}

AudioProcessorEditor* SerpentProcessor::createEditor() { return new SerpentEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SerpentProcessor(); }
