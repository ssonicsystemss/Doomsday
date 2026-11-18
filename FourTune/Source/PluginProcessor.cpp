#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DoomsdayHost.h"

using namespace juce;
using namespace tune;

// ------------------------------------------------------------ factory presets (category = the "CAT - " prefix, spec 27)
namespace
{
    struct Preset { const char* name; doomsday::PresetValues values; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> list {
            // PLUCKS (owner priority) - INIT is already a euphoric pluck
            { "PLUCKS - Euphoric Pluck", {} },
            { "PLUCKS - Bright Arp", { { "ae_d", 300 }, { "ae_s", 0 }, { "ae_r", 180 }, { "fe_d", 170 }, { "fe_s", 0.1f }, { "flt_env", 4.5f }, { "flt_cut", 900 },
                                      { "o3_voices", 5 }, { "o3_detune", 20 }, { "dly_div", 7 }, { "dly_mix", 0.2f }, { "rvb_mix", 0.18f }, { "scr_amt", 0.08f } } },
            { "PLUCKS - Dotted Keys", { { "o1_wave", 3 }, { "o1_voices", 1 }, { "o2_wave", 0 }, { "o2_oct", 1 }, { "o2_level", 0.3f }, { "flt_type", 0 }, { "flt_env", 3 },
                                       { "ae_d", 460 }, { "ae_s", 0.15f }, { "fe_d", 300 }, { "o3_level", 0.45f }, { "dly_div", 7 }, { "dly_mix", 0.22f } } },
            { "PLUCKS - Supersaw Pluck", { { "o1_voices", 7 }, { "o1_detune", 22 }, { "o3_voices", 11 }, { "o3_detune", 42 }, { "o4_on", 1 }, { "o4_level", 0.3f },
                                          { "ae_d", 520 }, { "ae_s", 0.2f }, { "fe_d", 220 }, { "flt_env", 4 }, { "flt_cut", 1100 }, { "scr_amt", 0.18f },
                                          { "rvb_mix", 0.22f }, { "rvb_size", 0.7f } } },
            { "PLUCKS - Soft Pluck", { { "o1_wave", 2 }, { "o2_wave", 1 }, { "o2_oct", 0 }, { "o2_level", 0.4f }, { "flt_type", 0 }, { "flt_cut", 700 }, { "flt_res", 0.08f },
                                      { "flt_env", 3 }, { "fe_d", 380 }, { "ae_d", 800 }, { "ae_s", 0.1f }, { "scr_amt", 0 }, { "rvb_mix", 0.25f } } },
            // EUPHORIC / LEADS
            { "EUPHORIC - Euphoric Lead", { { "ae_a", 2 }, { "ae_d", 1200 }, { "ae_s", 0.85f }, { "ae_r", 320 }, { "fe_d", 600 }, { "fe_s", 0.5f }, { "flt_env", 1.5f },
                                           { "flt_cut", 2500 }, { "o1_voices", 5 }, { "o1_detune", 15 }, { "o2_wave", 0 }, { "o2_voices", 3 }, { "o2_detune", 10 }, { "o2_level", 0.4f },
                                           { "o3_voices", 9 }, { "o3_detune", 40 }, { "o4_on", 1 }, { "o4_voices", 7 }, { "o4_detune", 25 }, { "o4_level", 0.3f },
                                           { "scr_amt", 0.25f }, { "rvb_mix", 0.22f }, { "rvb_size", 0.7f }, { "dly_mix", 0.12f }, { "fortune", 0.3f } } },
            { "LEADS - Classic Hardstyle", { { "o1_voices", 1 }, { "o2_wave", 4 }, { "o2_wt", 0.4f }, { "o2_oct", 0 }, { "o2_level", 0.5f }, { "o3_voices", 5 }, { "o3_detune", 18 },
                                            { "scr_amt", 0.35f }, { "flt_cut", 3500 }, { "flt_env", 1 }, { "ae_d", 900 }, { "ae_s", 0.85f }, { "ae_r", 250 } } },
            { "RAW - Rawphoric Lead", { { "o1_wave", 3 }, { "o1_voices", 3 }, { "o1_detune", 10 }, { "o3_voices", 9 }, { "o3_detune", 35 }, { "scr_amt", 0.55f }, { "scr_bite", 0.7f },
                                       { "flt_cut", 3000 }, { "flt_res", 0.3f }, { "flt_env", 2 }, { "ae_d", 1000 }, { "ae_s", 0.9f }, { "play_mode", 2 }, { "glide", 40 },
                                       { "rvb_mix", 0.18f } } },
            // SUPERSAWS / FESTIVAL
            { "SUPERSAWS - Festival Supersaw", { { "o1_voices", 7 }, { "o1_detune", 25 }, { "o2_wave", 0 }, { "o2_voices", 7 }, { "o2_detune", 20 }, { "o2_level", 0.5f },
                                                { "o3_voices", 16 }, { "o3_detune", 55 }, { "o3_umix", 0.9f }, { "o4_on", 1 }, { "o4_voices", 9 }, { "o4_detune", 40 }, { "o4_level", 0.35f },
                                                { "flt_cut", 6000 }, { "flt_res", 0.1f }, { "flt_env", 1 }, { "ae_a", 3 }, { "ae_d", 1500 }, { "ae_s", 0.9f }, { "ae_r", 400 },
                                                { "scr_amt", 0.3f }, { "cho_on", 1 }, { "rvb_mix", 0.25f }, { "rvb_size", 0.8f }, { "fortune", 0.5f } } },
            { "FESTIVAL - Fortune Anthem", { { "o1_voices", 7 }, { "o1_detune", 25 }, { "o2_wave", 0 }, { "o2_voices", 7 }, { "o2_detune", 20 }, { "o2_level", 0.5f },
                                            { "o3_voices", 16 }, { "o3_detune", 50 }, { "o4_on", 1 }, { "o4_voices", 9 }, { "o4_detune", 35 }, { "o4_level", 0.35f },
                                            { "flt_cut", 4500 }, { "flt_env", 1.5f }, { "ae_d", 1500 }, { "ae_s", 0.9f }, { "ae_r", 450 }, { "scr_amt", 0.35f },
                                            { "rvb_mix", 0.22f }, { "rvb_size", 0.75f }, { "fortune", 0.85f } } },
            // CLASSIC / KEYS / PADS / BASS / EXPERIMENTAL
            { "CLASSIC - Hoover-ish Saw", { { "o1_voices", 5 }, { "o1_detune", 35 }, { "o1_warp", 0.35f }, { "o2_wave", 4 }, { "o2_oct", -1 }, { "o2_level", 0.5f }, { "o3_on", 0 },
                                           { "flt_cut", 4000 }, { "flt_env", 0.5f }, { "ae_s", 0.9f }, { "ae_d", 800 }, { "glide", 80 }, { "play_mode", 2 }, { "scr_amt", 0.3f } } },
            { "KEYS - Piano Pluck", { { "o1_wave", 2 }, { "o1_voices", 1 }, { "o2_wave", 1 }, { "o2_oct", 1 }, { "o2_level", 0.35f }, { "o3_level", 0.3f }, { "o3_voices", 3 },
                                     { "o3_detune", 8 }, { "ae_d", 900 }, { "ae_s", 0.1f }, { "fe_d", 350 }, { "flt_env", 2.5f }, { "flt_cut", 900 }, { "scr_amt", 0 },
                                     { "rvb_mix", 0.2f } } },
            { "PADS - Sky Pad", { { "ae_a", 400 }, { "ae_d", 2000 }, { "ae_s", 0.8f }, { "ae_r", 1500 }, { "flt_cut", 2200 }, { "flt_env", 0.5f }, { "fe_a", 300 }, { "fe_d", 1500 },
                                 { "o3_voices", 16 }, { "o3_detune", 45 }, { "o1_voices", 5 }, { "o1_detune", 20 }, { "cho_on", 1 }, { "rvb_mix", 0.4f }, { "rvb_size", 0.9f },
                                 { "rvb_decay", 5 }, { "scr_amt", 0 }, { "dly_mix", 0.08f } } },
            { "BASS - Mono Saw Bass", { { "play_mode", 1 }, { "o1_voices", 1 }, { "o1_oct", -1 }, { "o2_oct", -2 }, { "o2_level", 0.5f }, { "o3_on", 0 }, { "flt_cut", 600 },
                                       { "flt_env", 2.5f }, { "fe_d", 200 }, { "fe_s", 0.2f }, { "ae_s", 0.9f }, { "rvb_on", 0 }, { "dly_on", 0 }, { "scr_amt", 0.15f } } },
            { "EXPERIMENTAL - Warp Pluck", { { "o1_wave", 5 }, { "o1_wt", 0.5f }, { "o1_warp", 0.6f }, { "o3_wave", 5 }, { "o3_wt", 0.8f }, { "mm3_src", 4 }, { "mm3_dst", 7 },
                                            { "mm3_amt", 0.5f }, { "lfo1_sync", 1 }, { "lfo1_div", 6 }, { "ae_d", 600 }, { "ae_s", 0.1f } } },
            { "INIT", { { "o1_voices", 1 }, { "o2_on", 0 }, { "o3_on", 0 }, { "o4_on", 0 }, { "flt_cut", 20000 }, { "flt_env", 0 }, { "flt_res", 0 }, { "flt_drive", 0 },
                       { "ae_d", 1000 }, { "ae_s", 1 }, { "scr_amt", 0 }, { "dly_on", 0 }, { "rvb_on", 0 }, { "mm1_src", 0 }, { "mm2_src", 0 } } },
        };
        return list;
    }
}

int TuneProcessor::getNumPrograms() { return (int) presets().size(); }
const String TuneProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? String (presets()[(size_t) i].name) : String(); }
void TuneProcessor::setCurrentProgram (int i)
{
    if (! isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    presetName = presets()[(size_t) i].name;
    doomsday::PresetIO::apply (*this, apvts, presets()[(size_t) i].values);
    undo.clearUndoHistory();
}
void TuneProcessor::setStateInformation (const void* data, int size)
{
    // a broken project must not inject NaN / inf: clean the XML, then load it
    if (auto xml = getXmlFromBinary (data, size))
    {
        doomsday::host::sanitizeState (*xml);
        MemoryBlock clean; copyXmlToBinary (*xml, clean);
        doomsday::PresetIO::setState (*this, apvts, clean.getData(), (int) clean.getSize(), currentProgram, presetName);
    }
}
bool TuneProcessor::loadUserPresetFile (const File& f)
{
    const auto keepMelody = melodyTree().createCopy();          // a sound preset never replaces the melody
    if (! doomsday::PresetIO::load (*this, apvts, f)) return false;
    { auto t = melodyTree(); t.copyPropertiesAndChildrenFrom (keepMelody, nullptr); }
    presetName = f.getFileNameWithoutExtension(); currentProgram = -1;
    return true;
}
bool TuneProcessor::saveUserPresetAs (const String& name) { return saveUserPreset (name, lastAuthor()); }
static File authorFile() { return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Doomsday").getChildFile ("4TUNE author.txt"); }
String TuneProcessor::lastAuthor() { return authorFile().loadFileAsString().trim(); }
bool TuneProcessor::saveUserPreset (const String& name, const String& author)
{
    String clean;
    if (! io.save (apvts, name, clean)) return false;
    lib::Library::setAuthorAndStrip (io.folder().getChildFile (clean + "." + io.extension), author.trim());
    if (author.trim().isNotEmpty()) { authorFile().getParentDirectory().createDirectory(); authorFile().replaceWithText (author.trim()); }
    presetName = clean; currentProgram = -1;
    return true;
}
void TuneProcessor::envelopeMode (bool lead)
{
    undo.beginNewTransaction (lead ? "Lead envelope" : "Pluck envelope");
    const float pl[] { 0.5f, 0.f, 380.f, 0.12f, 180.f, 0.5f }, ld[] { 4.f, 0.f, 1200.f, 0.85f, 380.f, 0.3f };
    for (int i = 0; i < 6; ++i) setValue ((P::Id) (P::AeA + i), lead ? ld[i] : pl[i]);
}

// ------------------------------------------------------------ processor
TuneProcessor::TuneProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "FOURTUNE", createLayout())
{
    const auto& ids = paramIds();
    for (int i = 0; i < P::count; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (ids[(size_t) i]);
        params[(size_t) i] = apvts.getParameter (ids[(size_t) i]);
        jassert (raw[(size_t) i] != nullptr);
    }
    setCurrentProgram (0);
    melodyTree();                                   // the MELODY child exists from the start
    if (melodyNotes().empty()) generateMelody (false);
    undo.clearUndoHistory();
    apvts.state.addListener (this);
    publishMelody();
}

void TuneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
    engine.setSettings (gather (150.0));
    engine.snapSmoothing();
    setLatencySamples (engine.latencySamples());
    firstBlock = true;
}

tune::dsp::Settings TuneProcessor::gather (double bpm) const
{
    auto v = [this] (int id) { return raw[(size_t) id]->load(); };
    auto on = [&v] (int id) { return v (id) > 0.5f; };
    auto ix = [&v] (int id) { return (int) std::round (v (id)); };
    auto db = [&v] (int id) { return v (id) <= -47.9f ? 0.f : Decibels::decibelsToGain (v (id)); };
    tune::dsp::Settings s;
    constexpr int oscStride = P::O2On - P::O1On;
    for (int o = 0; o < 4; ++o)
    {
        const int b = P::O1On + o * oscStride;
        auto& x = s.osc[(size_t) o];
        x.on = on (b + (P::O1On - P::O1On)); x.wave = ix (b + (P::O1Wave - P::O1On));
        x.wt = v (b + (P::O1Wt - P::O1On)); x.warp = v (b + (P::O1Warp - P::O1On));
        x.semis = 12.f * v (b + (P::O1Oct - P::O1On)) + v (b + (P::O1Semi - P::O1On)) + v (b + (P::O1Fine - P::O1On)) * 0.01f;
        x.level = v (b + (P::O1Level - P::O1On)); x.pan = v (b + (P::O1Pan - P::O1On)); x.width = v (b + (P::O1Width - P::O1On));
        x.phase = v (b + (P::O1Phase - P::O1On)); x.phRand = v (b + (P::O1PhRnd - P::O1On)); x.retrig = on (b + (P::O1Retrig - P::O1On));
        x.voices = ix (b + (P::O1Voices - P::O1On)); x.detune = v (b + (P::O1Detune - P::O1On));
        x.uspread = v (b + (P::O1USpread - P::O1On)); x.umix = v (b + (P::O1UMix - P::O1On));
    }
    s.gaGain = db (P::GaVol); s.gaPan = v (P::GaPan); s.gaWidth = v (P::GaWidth);
    s.gbGain = db (P::GbVol); s.gbPan = v (P::GbPan); s.gbWidth = v (P::GbWidth);
    s.fltType = ix (P::FltType); s.cut = v (P::FltCut); s.res = v (P::FltRes); s.fltDriveDb = v (P::FltDrive); s.key = v (P::FltKey);
    s.envOct = v (P::FltEnv); s.route = ix (P::FltRoute);
    auto env = [&v] (int a) { tune::dsp::EnvSet e; e.a = v (a); e.h = v (a + 1); e.d = v (a + 2); e.s = v (a + 3); e.r = v (a + 4); e.curve = v (a + 5); return e; };
    s.fe = env (P::FeA); s.ae = env (P::AeA); s.me = env (P::MeA);
    constexpr int lfoStride = P::L2Shape - P::L1Shape;
    for (int l = 0; l < 2; ++l)
    {
        const int b = P::L1Shape + l * lfoStride;
        auto& x = s.lfo[(size_t) l];
        x.shape = ix (b);
        const bool sync = on (b + (P::L1Sync - P::L1Shape));
        x.hz = sync ? (float) (bpm / 60.0 / Choices::syncBeats (ix (b + (P::L1Div - P::L1Shape)))) : v (b + (P::L1Rate - P::L1Shape));
        x.depth = v (b + (P::L1Depth - P::L1Shape)); x.phase = v (b + (P::L1Phase - P::L1Shape));
        x.fadeMs = v (b + (P::L1Fade - P::L1Shape)); x.retrig = on (b + (P::L1Retrig - P::L1Shape));
    }
    constexpr int slotStride = P::M2Src - P::M1Src;
    for (int k = 0; k < tune::dsp::numSlots; ++k)
    {
        const int b = P::M1Src + k * slotStride;
        s.slots[(size_t) k] = { ix (b), ix (b + 1), v (b + 2) };
    }
    s.scr = v (P::ScrAmt); s.scrDriveDb = v (P::ScrDrive); s.scrSat = v (P::ScrSat); s.scrTone = v (P::ScrTone); s.scrBite = v (P::ScrBite);
    s.scrBody = v (P::ScrBody); s.scrMix = v (P::ScrMix); s.scrOutDb = v (P::ScrOut); s.scrOs = ix (P::ScrOs);
    s.fortune = v (P::Fortune); s.ftCut = v (P::FtCut); s.ftDet = v (P::FtDet); s.ftWidth = v (P::FtWidth); s.ftScr = v (P::FtScr);
    s.ftRMix = v (P::FtRMix); s.ftRSize = v (P::FtRSize);
    s.eqOn = on (P::EqOn); s.eqLow = v (P::EqLow); s.eqLowMid = v (P::EqLowMid); s.eqHighMid = v (P::EqHighMid); s.eqHigh = v (P::EqHigh);
    s.distOn = on (P::DistOn); s.distDriveDb = v (P::DistDrive); s.distTone = v (P::DistTone); s.distMix = v (P::DistMix); s.distOutDb = v (P::DistOut);
    s.choOn = on (P::ChoOn); s.choRate = v (P::ChoRate); s.choDepth = v (P::ChoDepth); s.choWidth = v (P::ChoWidth); s.choMix = v (P::ChoMix);
    s.dlyOn = on (P::DlyOn); s.dlyBeats = Choices::syncBeats (ix (P::DlyDiv)); s.dlyFb = v (P::DlyFb); s.dlyPP = on (P::DlyPP);
    s.dlyFilter = v (P::DlyFilter); s.dlyMix = v (P::DlyMix);
    s.rvbOn = on (P::RvbOn); s.rvbPre = v (P::RvbPre); s.rvbSize = v (P::RvbSize); s.rvbDecay = v (P::RvbDecay); s.rvbDamp = v (P::RvbDamp);
    s.rvbWidth = v (P::RvbWidth); s.rvbMix = v (P::RvbMix);
    s.stWidth = v (P::StWidth); s.stBalance = v (P::StBalance); s.stMonoBass = v (P::StMonoBass);
    s.limOn = on (P::LimOn); s.limInDb = v (P::LimInput); s.limCeil = v (P::LimCeiling); s.limRel = v (P::LimRelease);
    s.playMode = ix (P::PlayMode); s.polyVoices = ix (P::PolyVoices); s.glideMs = v (P::Glide); s.bendRange = v (P::BendRange);
    s.velSens = v (P::VelSens); s.transpose = v (P::Transpose); s.master = Decibels::decibelsToGain (v (P::Master), -60.f);
    s.bpm = bpm;
    return s;
}

void TuneProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, buffer.getNumSamples());
    if (buffer.getNumChannels() < 2) { buffer.clear(); return; }
    double bpm, ppq;
    doomsday::PresetIO::hostTime (getPlayHead(), bpm, ppq);

    const int cmd = auditionCmd.exchange (0);      // PLAY: an A minor chord (the euphoric test chord)
    if (cmd == 1 && ! auditionHeld) { for (int n : { 57, 60, 64, 69 }) midi.addEvent (MidiMessage::noteOn (1, n, (uint8) 110), 0); auditionHeld = true; }
    if (cmd == 2 && auditionHeld)   { for (int n : { 57, 60, 64, 69 }) midi.addEvent (MidiMessage::noteOff (1, n), 0); auditionHeld = false; }

    // MELODY PREVIEW: on the host grid when FL plays, on its own clock when it is stopped
    {
        bool playing = false;
        if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) playing = pos->getIsPlaying();
        lastPpq.store (playing ? ppq : -1.0); lastBpm.store (bpm);
        melodyPlayer.process (midi, buffer.getNumSamples(), getSampleRate() > 0 ? getSampleRate() : 48000.0, bpm, playing ? ppq : -1.0, melodyPreview.load());
    }
    engine.setSettings (gather (bpm));
    if (firstBlock) { engine.snapSmoothing(); firstBlock = false; }
    engine.smooth (buffer.getNumSamples());
    const int lat = engine.latencySamples();
    if (lat != getLatencySamples()) setLatencySamples (lat);

    int pos = 0;
    const int n = buffer.getNumSamples();
    float pk = 0.f;
    auto renderTo = [&] (int end)
    {
        if (end > pos) { engine.render (buffer, pos, end - pos); pk = jmax (pk, engine.scopeLevel()); pos = end; }
    };
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        renderTo (jlimit (0, n, meta.samplePosition));
        if (m.isNoteOn())                     engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())               engine.noteOff (m.getNoteNumber());
        else if (m.isPitchWheel())            engine.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.f);
        else if (m.isController() && m.getControllerNumber() == 1) engine.setModWheel ((float) m.getControllerValue() / 127.f);
        else if (m.isChannelPressure())       engine.setAftertouch ((float) m.getChannelPressureValue() / 127.f);
        else if (m.isAftertouch())            engine.setAftertouch ((float) m.getAfterTouchValue() / 127.f);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    renderTo (n);
    peak.store (pk);
}

// ------------------------------------------------------------ RANDOM / SMART RANDOM (spec 28)
void TuneProcessor::setValue (P::Id id, float realValue)
{
    if (auto* p = params[(size_t) id]) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (realValue)); p->endChangeGesture(); }
}

void TuneProcessor::fullRandom()
{
    // every sound-design parameter anywhere in its range (not the output / performance / limiter)
    for (int i = 0; i < P::count; ++i)
    {
        if (i >= P::LimOn) break;                                    // limiter, play mode, master stay
        if (i == P::Fortune || (i >= P::FtCut && i <= P::FtRSize)) continue;
        if (auto* p = params[(size_t) i]) { p->beginChangeGesture(); p->setValueNotifyingHost (rng.nextFloat()); p->endChangeGesture(); }
    }
    presetName = "Random #" + String (1000 + rng.nextInt (9000)); currentProgram = -1;
}

void TuneProcessor::smartRandom()
{
    auto between = [this] (float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };
    auto chance = [this] (float p) { return rng.nextFloat() < p; };
    const bool pluck = chance (0.55f);                                 // owner priority: plucks + euphoric leads / supersaws
    for (int o = 0; o < 4; ++o)
    {
        const int b = P::O1On + o * (P::O2On - P::O1On);
        auto id = [b] (int rel) { return (P::Id) (b + rel - P::O1On); };
        const bool on = o == 0 || (o == 2 ? chance (0.85f) : chance (0.5f));
        setValue (id (P::O1On), on ? 1.f : 0.f);
        const int wave = chance (0.65f) ? 0 : rng.nextInt (6);
        setValue (id (P::O1Wave), (float) wave);
        setValue (id (P::O1Wt), between (0.f, 1.f));
        setValue (id (P::O1Warp), chance (0.25f) ? between (0.f, 0.5f) : 0.f);
        setValue (id (P::O1Oct), (float) (o == 1 ? -1 + rng.nextInt (2) : o == 3 ? rng.nextInt (2) : 0));
        setValue (id (P::O1Semi), chance (0.1f) ? 7.f : 0.f);
        setValue (id (P::O1Fine), between (-6.f, 6.f));
        setValue (id (P::O1Level), between (o < 2 ? 0.5f : 0.3f, o == 0 ? 0.9f : 0.7f));
        const int voices = o < 2 ? 1 + rng.nextInt (7) : 5 + rng.nextInt (12);
        setValue (id (P::O1Voices), (float) voices);
        setValue (id (P::O1Detune), o < 2 ? between (5.f, 22.f) : between (20.f, 50.f));      // never extreme (spec 28)
        setValue (id (P::O1USpread), o < 2 ? between (0.2f, 0.6f) : between (0.7f, 1.f));
        setValue (id (P::O1UMix), between (0.6f, 1.f));
        setValue (id (P::O1PhRnd), o < 2 ? between (0.f, 0.3f) : 1.f);
    }
    setValue (P::FltType, chance (0.75f) ? 1.f : 0.f);
    setValue (P::FltCut, pluck ? between (600.f, 1600.f) : between (1800.f, 7000.f));
    setValue (P::FltRes, between (0.05f, 0.35f));
    setValue (P::FltDrive, between (0.f, 8.f));
    setValue (P::FltEnv, pluck ? between (2.5f, 5.f) : between (0.f, 2.f));
    setValue (P::FeD, pluck ? between (120.f, 400.f) : between (300.f, 1200.f));
    setValue (P::FeS, between (0.f, 0.4f));
    setValue (P::AeA, pluck ? between (0.f, 3.f) : between (0.f, 20.f));
    setValue (P::AeD, pluck ? between (250.f, 800.f) : between (600.f, 2000.f));
    setValue (P::AeS, pluck ? between (0.f, 0.3f) : between (0.7f, 1.f));
    setValue (P::AeR, between (120.f, 500.f));
    setValue (P::ScrAmt, chance (0.3f) ? 0.f : between (0.05f, 0.5f));
    setValue (P::ScrBite, between (0.2f, 0.7f));
    setValue (P::RvbMix, between (0.1f, 0.28f));                       // never drowning (spec 28)
    setValue (P::RvbSize, between (0.4f, 0.85f));
    setValue (P::DlyMix, between (0.f, 0.22f));
    setValue (P::ChoOn, chance (0.3f) ? 1.f : 0.f);
    setValue (P::GbVol, between (-6.f, 0.f));
    presetName = String (pluck ? "Smart Pluck #" : "Smart Lead #") + String (1000 + rng.nextInt (9000)); currentProgram = -1;
}

// ------------------------------------------------------------ MELODY (state child MELODY: generator settings + notes N)
namespace
{
    const Identifier idMel ("MELODY"), idNote ("N"), idStart ("s"), idLen ("l"), idPitch ("p"), idVel ("v");
}
ValueTree TuneProcessor::melodyTree() { return apvts.state.getOrCreateChildWithName (idMel, nullptr); }

melody::Settings TuneProcessor::melodySettings()
{
    auto t = melodyTree();
    melody::Settings m;
    m.key = t.getProperty ("key", m.key); m.scale = t.getProperty ("scale", m.scale);
    m.customMask = (uint16_t) (int) t.getProperty ("mask", (int) m.customMask);
    m.bars = t.getProperty ("bars", m.bars); m.mode = t.getProperty ("mode", m.mode);
    m.density = (float) t.getProperty ("density", m.density); m.complexity = (float) t.getProperty ("complexity", m.complexity);
    m.repetition = (float) t.getProperty ("repetition", m.repetition); m.movement = (float) t.getProperty ("movement", m.movement);
    m.rhythm = (float) t.getProperty ("rhythm", m.rhythm);
    m.octLow = t.getProperty ("octLow", m.octLow); m.octHigh = t.getProperty ("octHigh", m.octHigh);
    m.rootBounce = (bool) t.getProperty ("bounce", m.rootBounce); m.seed = (uint32_t) (int) t.getProperty ("seed", (int) m.seed);
    return m;
}
void TuneProcessor::setMelodySetting (const Identifier& id, const var& v) { melodyTree().setProperty (id, v, nullptr); }

std::vector<melody::Note> TuneProcessor::melodyNotes()
{
    std::vector<melody::Note> v;
    for (auto c : melodyTree())
        if (c.hasType (idNote))
            v.push_back ({ (double) c.getProperty (idStart), (double) c.getProperty (idLen), (int) c.getProperty (idPitch), (int) c.getProperty (idVel) });
    std::sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.start < b.start || (a.start == b.start && a.pitch < b.pitch); });
    return v;
}
void TuneProcessor::setMelodyNotes (const std::vector<melody::Note>& v, bool newTransaction, const String& name)
{
    if (newTransaction) undo.beginNewTransaction (name);
    auto t = melodyTree();
    for (int i = t.getNumChildren(); --i >= 0;) if (t.getChild (i).hasType (idNote)) t.removeChild (i, &undo);
    for (auto& n : v)
    {
        ValueTree c (idNote);
        c.setProperty (idStart, jmax (0.0, n.start), nullptr); c.setProperty (idLen, jmax (1.0 / 64.0, n.len), nullptr);
        c.setProperty (idPitch, jlimit (0, 127, n.pitch), nullptr); c.setProperty (idVel, jlimit (1, 127, n.vel), nullptr);
        t.appendChild (c, &undo);
    }
}
void TuneProcessor::generateMelody (bool newSeed)
{
    if (newSeed) setMelodySetting ("seed", (int) (1 + rng.nextInt (999999)));
    melody::Generator g;
    setMelodyNotes (g.generate (melodySettings()), true, "Generate melody");
    melodyTree().setProperty ("length", 4.0 * melodySettings().bars, nullptr);
}
double TuneProcessor::melodyLengthBeats()
{
    double len = (double) melodyTree().getProperty ("length", 16.0);
    for (auto& n : melodyNotes()) len = jmax (len, std::ceil ((n.start + n.len) / 4.0 - 1e-6) * 4.0);
    return jmax (4.0, len);
}
void TuneProcessor::publishMelody() { melodyPlayer.publish (melodyNotes(), melodyLengthBeats()); ++melodyVersion; }

AudioProcessorEditor* TuneProcessor::createEditor() { return new TuneEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TuneProcessor(); }
