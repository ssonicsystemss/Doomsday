#pragma once
// 4GE - Hardstyle Kick Designer (rebuild from scratch, 2026-11: module 01 Core Kick Engine on the Parameter Registry)
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayParams.h"
#include "DoomsdayUI.h"
#include "Parameters/ParamRegistry.h"
#include "Parameters/ParamFrame.h"
#include "DSP/Engine.h"
#include "Presets/ModState.h"
#include "Presets/SamplePool.h"
#include "Presets/Export.h"
#include "DSP/Meter/MasterMeter.h"

namespace fge { inline constexpr const char* versionText = "0.5"; }      // keep in step with CMakeLists.txt

class GeProcessor : public juce::AudioProcessor, public doomsday::ui::PresetHost, private juce::ValueTree::Listener, private juce::Timer,
                    private juce::AudioProcessorParameter::Listener
{
public:
    GeProcessor();
    ~GeProcessor() override { stopTimer(); apvts.state.removeListener (this); }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "4GE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& d) override;
    void setStateInformation (const void* data, int size) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::UndoManager undo { 300000, 50 };      // Ctrl+Z (declared before apvts)
    juce::AudioProcessorValueTreeState apvts;

    // PresetHost
    int numFactoryPresets() override { return getNumPrograms(); }
    juce::String factoryPresetName (int i) override { return getProgramName (i); }
    void loadFactoryPreset (int i) override { setCurrentProgram (i); }
    juce::Array<juce::File> userPresetFiles() override { return io.list(); }
    bool loadUserPresetFile (const juce::File& f) override;
    bool saveUserPresetAs (const juce::String& name) override;
    void randomizeSound() override;
    juce::String currentPresetName() override { return presetName; }
    juce::String presetFolderText() override { return "Documents / 4GE / Presets"; }
    juce::File userPresetFolder() override { return io.folder(); }
    juce::UndoManager* undoManager() override { return &undo; }
    void audition (bool on) override { if (on) pendingAudition.store (true); }

    // the current parameter values (any thread: atomics only)
    fge::ParamFrame frame() const { fge::ParamFrame f = fge::ParamFrame::defaults(); pointers.gather (f); return f; }
    fge::Engine& getEngine() { return engine; }
    const doomsday::PresetIO io { "4GE", "4gp" };

    // ---- DRAWN shapes (module 02): pitch CUSTOM curves + DRAWN envelopes, stored in the state child ENVS
    //      (saved with the project and user presets, undoable). Message thread only; the audio thread reads a lock-free copy.
    static constexpr const char* envsTag = "ENVS";
    const fge::env::Shape& shape (int slot) const { return current.s[(size_t) juce::jlimit (0, (int) fge::env::NumSlots - 1, slot)]; }
    void setShape (int slot, const fge::env::Shape&, bool undoable = true, bool newUndoStep = true);
    void resetShapes (bool undoable = true);
    const fge::env::Bank& shapes() const { return current; }
    uint32_t shapesVersion() const { return version; }
    // a parameter change that joins the current undo step (the APVTS otherwise records it later, from its timer)
    // a new undo step for a group of parameter changes: first let the APVTS write every pending parameter value into the tree (its timer
    // can wait up to 500 ms) - into the PREVIOUS step - so a late flush never becomes the newest step and Ctrl+Z takes this whole group back
    void beginUndoStep (const juce::String& name) { (void) apvts.copyState(); undo.beginNewTransaction (name); }
    void setParamInUndoStep (const juce::String& id, float plainValue)
    {
        auto child = apvts.state.getChildWithProperty ("id", id);
        if (child.isValid())
        {
            // the APVTS writes parameter values into the tree only on its timer: bring the tree up to date first (no undo),
            // so the undo step records "current -> new" and the change is never swallowed as "no change"
            if (auto* a = apvts.getRawParameterValue (id)) child.setProperty ("value", a->load(), nullptr);
            child.setProperty ("value", plainValue, &undo);
            // the value the parameter really holds now (range / float round trip): written into the tree in the SAME step - otherwise the
            // APVTS timer writes it later as a second undo step and one Ctrl+Z only takes that tiny correction back
            if (auto* a = apvts.getRawParameterValue (id)) if ((float) child["value"] != a->load()) child.setProperty ("value", a->load(), &undo);
        }
        else if (auto* a = apvts.getParameter (id)) a->setValueNotifyingHost (a->convertTo0to1 (plainValue));
    }
    // ---- OSCILLATOR data (module 03): drawn CUSTOM cycle, imported cycle, harmonic profile, USER wavetable frames.
    //      Stored in the state child OSCS (undoable, saved with projects / presets). Tables are built here, off the audio thread.
    static constexpr const char* oscsTag = "OSCS";
    const fge::dsp::OscData& oscData() const { return oscDataNow; }
    uint32_t oscDataVersion() const { return oscVersion; }
    void setOscProperty (int osc, const juce::String& key, const juce::String& text, bool undoable = true, bool newUndoStep = true);
    void setOscCustom (int osc, const fge::env::Shape& s) { setOscProperty (osc, "custom", s.toString()); }
    // the tables the engine plays now (shared copies for the waveform renderer)
    std::array<fge::dsp::TablePtr, fge::dsp::maxOsc> currentTables() const { const juce::SpinLock::ScopedLockType l (tableLock); return tablesNow; }
    void refreshTables();                                    // message thread (timer) / state load; cheap when nothing changed
    // ---- DISTORTION data (module 05): the CUSTOM waveshaper curve of each stage, state child DIST (undoable)
    static constexpr const char* distTag = "DIST";
    const fge::dsp::dist::DistData& distData() const { return distNow; }
    uint32_t distDataVersion() const { return distVersion; }
    void setDistCurve (int stage, const fge::dsp::dist::CurveData&, bool newUndoStep = true);
    void setClipCurve (int clipper, const fge::dsp::dist::CurveData&, bool newUndoStep = true);   // module 08 (state child DIST, "clip1" / "clip2")
    // ---- MODULATION MATRIX (module 09): 64 slots + the sequencer steps, state child MODM (undoable; none = the default macro mapping)
    static constexpr const char* modTag = fge::modstate::tag;
    const fge::modstate::Desc& modDesc() const { return modNow; }
    uint32_t modVersion() const { return modVer; }
    int modUnknownTargets() const { return modUnknown; }               // slots whose target is missing / not modulatable (OFF)
    const fge::dsp::mod::Data& modAudioCopy() const { return modResolved; }   // (message thread: offline renders / previews)
    void setModSlot (int slot, const fge::modstate::SlotDesc&, bool newUndoStep = true);
    void setModSteps (const std::array<fge::dsp::mod::Step, fge::dsp::mod::maxSteps>&, bool newUndoStep = true);
    void resetModMatrix (bool undoable = true);                         // back to the default macro mapping
    void clearModMatrix();                                              // every slot empty (one undo step)
    void setModProperty (const juce::String& key, const juce::String& text, bool newUndoStep = true);   // lfo1..8 shapes, morphA / B, ccMacro
    void setLfoShape (int lfo, const fge::dsp::mod::LfoShape& sh, bool newUndoStep = true) { setModProperty ("lfo" + juce::String (lfo + 1), fge::modstate::shapeToText (sh), newUndoStep); }
    void storeMorph (bool toB);                                         // MACRO MORPH: the 12 macro values now -> set A / B
    // MIDI LEARN (module 09 8): the next CC sets CC source n (0..3) or macro i (100 + i, 0..11); -1 = off
    void startMidiLearn (int target) { learnTarget.store (target); learnedCc.store (-1); }
    int midiLearnTarget() const { return learnTarget.load(); }
    void serviceMidi();                                                 // message thread (timer): learned CC + CC -> macro values
    // ---- SAMPLE SLOTS (module 10): the audio in the state child SAMPLES (working copy embedded, the user's file never written)
    static constexpr const char* sampleTag = fge::smp::tag;
    fge::smp::Pool& samples() { return samplePool; }
    bool importSampleNow (int slot, const juce::File&, juce::String* error = nullptr);   // decode + embed on this thread (tests / tools)
    void importSample (int slot, const juce::File&);                    // GUI: a background thread, then one undo step on the message thread
    void clearSample (int slot);
    bool applySampleTool (int slot, int tool);                          // NORMALIZE / DC OFFSET / TRIM SILENCE on the working copy (undoable)
    int collectSamples (const juce::File& folder);                      // COLLECT SAMPLES: copy the used files there, point the slots to them
    fge::smp::SlotInfo sampleInfo (int slot) const { return samplePool.info (slot); }
    int relinkSamples (const juce::File& folder);
    // module 11: the output's stereo analyzer (GUI reads it) and MONO AUDITION (a listening switch - not saved, not a parameter)
    fge::dsp::spatial::Analyzer analyzer;
    std::atomic<bool> monoAudition { false };
    // module 12: MASTER METERS (peak / true peak / RMS / LUFS / DC), the render SEED, EXPORT (settings in the state child EXPORT)
    fge::dsp::meter::LiveMeter meter;
    uint32_t presetSeed() const { return (uint32_t) (juce::int64) apvts.state.getProperty ("presetSeed", 0); }
    void setPresetSeed (uint32_t s);                                     // (undoable; the live engine plays with it)
    fge::xp::Settings exportSettings() const { return fge::xp::Settings::fromTree (apvts.state.getChildWithName ("EXPORT")); }
    void setExportSettings (const fge::xp::Settings&);
    fge::dsp::xport::Snapshot exportSnapshot() const;                    // the current sound (message thread)
    fge::dsp::xport::Snapshot exportSnapshotOfProgram (int program) const; // a factory preset, without touching the plugin
    std::vector<fge::xp::Job> makeExportJobs (const fge::xp::Settings&, bool batch, juce::String* error = nullptr) const;
    uint32_t seedFor (const fge::xp::Settings&, uint32_t preset) const;
    // module 13: the STYLE (state property "style", -1 = none) and the parameters the user moved himself ("touched": a knob gesture
    // or host automation; presets / styles / randomize / macros never mark). Saved with the project, cleared by loading a preset.
    // module 15: the preset name, the A / B slots (the whole state, switched without losing either), the RECENT hook
    void setPresetName (const juce::String& n) { presetName = n; }
    void abSwitch (int slot);                                          // 0 = A, 1 = B (an empty slot starts as a copy)
    void abCopyToOther();                                             // A -> B (or B -> A)
    int abActive() const { return abCur; }
    bool abHas (int slot) const { return ab[(size_t) (slot & 1)].getSize() > 0; }
    std::function<void (const juce::String& key)> onPresetLoaded;     // (the browser keeps RECENTLY USED)
    double hostBpm() const { const double b = lastHostBpm.load(); return b > 0.0 ? b : 150.0; }   // (the last tempo the host gave, 150 before)
    int currentStyle() const { return (int) apvts.state.getProperty ("style", -1); }
    bool isTouched (int i) const { return juce::isPositiveAndBelow (i, (int) fge::P::count) && touched[(size_t) i].load (std::memory_order_relaxed); }
    int touchedCount() const { int n = 0; for (int i = 0; i < fge::P::count; ++i) n += touched[(size_t) i].load() ? 1 : 0; return n; }
    void forgetTouched() { for (auto& t : touched) t.store (false); }
    void markTouched (int i) { if (juce::isPositiveAndBelow (i, (int) fge::P::count)) touched[(size_t) i].store (true); }
    fge::xp::History exportHistory;
    fge::xp::Exporter exporter { exportHistory };                       // RELINK: find the MISSING slots' files (by name) in a folder
    float samplePlayPos (int slot) const { return engine.smpPosMon[(size_t) juce::jlimit (0, fge::dsp::numSampleSlots - 1, slot)].load (std::memory_order_relaxed); }
    std::function<void (int slot, const juce::String& error)> onSampleImported;   // (GUI)
    static float slotMin (int slot) { return fge::env::isBipolar (slot) ? -1.f : 0.f; }
    int auditionNote = 31;          // G1 in FL (49 Hz)

private:
    juce::ValueTree envTree (bool create);
    void rebuildShapes();
    void valueTreePropertyChanged (juce::ValueTree& t, const juce::Identifier&) override { onTreeChange (t); }
    void valueTreeChildAdded (juce::ValueTree& t, juce::ValueTree& c) override
    { if (c.hasType (modTag)) rebuildModData(); else if (c.hasType (sampleTag)) samplePool.syncFromState (c); else onTreeChange (t); }
    void valueTreeChildRemoved (juce::ValueTree& t, juce::ValueTree& c, int) override
    { if (c.hasType (modTag)) rebuildModData(); else if (c.hasType (sampleTag)) samplePool.syncFromState ({}); else onTreeChange (t); }
    void valueTreeRedirected (juce::ValueTree&) override { rebuildShapes(); rebuildOscData(); rebuildDistData(); rebuildModData(); samplePool.syncFromState (apvts.state.getChildWithName (sampleTag)); }
    void onTreeChange (juce::ValueTree& t)
    {
        if (t.getType() == juce::Identifier ("PARAM")) return;            // parameter values: the tables follow on the timer
        if (t.getType() == juce::Identifier (oscsTag) || t.getParent().getType() == juce::Identifier (oscsTag)) rebuildOscData();
        else if (t.getType() == juce::Identifier (distTag)) rebuildDistData();
        else if (t.getType() == juce::Identifier (modTag)) rebuildModData();
        else if (t.getType() == juce::Identifier (sampleTag)) samplePool.syncFromState (t);
        else rebuildShapes();
    }
    void timerCallback() override { refreshTables(); serviceMidi(); samplePool.collectGarbage(); seedWanted.store (presetSeed()); }
    std::atomic<uint32_t> seedWanted { 0 };
    std::atomic<double> lastHostBpm { 0.0 };
    std::array<juce::MemoryBlock, 2> ab;
    int abCur = 0;
    std::array<std::atomic<bool>, fge::P::count> touched {};
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int index, bool starting) override { if (starting) markTouched (index); }
    void storeTouched();
    void restoreTouched();
    void rebuildOscData();
    void rebuildDistData();
    void rebuildModData();
    juce::ValueTree modTree();                                          // (creates it from the current data when missing)
    fge::modstate::Desc modNow = fge::modstate::defaults();
    fge::dsp::mod::Data modResolved;
    uint32_t modVer = 1;
    int modUnknown = 0;
    fge::dsp::Exchange<fge::dsp::mod::Data> modX;
    std::atomic<int> learnTarget { -1 }, learnedCc { -1 };
    fge::smp::Pool samplePool;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
    juce::ValueTree sampleTree();
    std::array<std::atomic<int>, 12> macroCc {};
    std::array<std::atomic<float>, 12> macroPending {};               // < 0 = nothing new from MIDI
    fge::dsp::dist::DistData distNow;
    uint32_t distVersion = 1;
    fge::dsp::Exchange<fge::dsp::dist::DistData> distX;
    fge::dsp::OscData oscDataNow;
    uint32_t oscVersion = 1;
    std::array<fge::dsp::TablePtr, fge::dsp::maxOsc> tablesNow;
    std::array<juce::String, fge::dsp::maxOsc> tableKeys;
    std::vector<std::array<fge::dsp::TablePtr, fge::dsp::maxOsc>> retired;   // tables the audio thread may still read
    fge::dsp::Exchange<fge::dsp::OscTables> tablesX;
    mutable juce::SpinLock tableLock;
    fge::env::BankExchange bankX;
    fge::env::Bank current;                 // message-thread copy (GUI, waveform)
    uint32_t version = 1;
    juce::SpinLock shapeWriteLock;          // writers only (the host may set the state from another thread)
    fge::Engine engine;
    fge::ParamPointers pointers;
    int currentProgram = 0;
    juce::String presetName { "INIT" };
    juce::SpinLock prepareLock;            // prepareToPlay holds it; processBlock only try-locks (silence while FL re-prepares)
    std::atomic<bool> pendingAudition { false };
    juce::Random rng;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeProcessor)
};
