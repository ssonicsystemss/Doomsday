#pragma once
// 4GE - Hardstyle Kick Designer (rebuild from scratch, 2026-11: module 01 Core Kick Engine on the Parameter Registry)
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayParams.h"
#include "DoomsdayUI.h"
#include "Parameters/ParamRegistry.h"
#include "Parameters/ParamFrame.h"
#include "DSP/Engine.h"

namespace fge { inline constexpr const char* versionText = "0.5"; }      // keep in step with CMakeLists.txt

class GeProcessor : public juce::AudioProcessor, public doomsday::ui::PresetHost, private juce::ValueTree::Listener
{
public:
    GeProcessor();
    ~GeProcessor() override { apvts.state.removeListener (this); }
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
    void setShape (int slot, const fge::env::Shape&, bool undoable = true);
    void resetShapes (bool undoable = true);
    const fge::env::Bank& shapes() const { return current; }
    uint32_t shapesVersion() const { return version; }
    static float slotMin (int slot) { return fge::env::isPitch (slot) ? -1.f : 0.f; }
    int auditionNote = 31;          // G1 in FL (49 Hz)

private:
    juce::ValueTree envTree (bool create);
    void rebuildShapes();
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { rebuildShapes(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { rebuildShapes(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { rebuildShapes(); }
    void valueTreeRedirected (juce::ValueTree&) override { rebuildShapes(); }
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
