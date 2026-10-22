#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayParams.h"
#include "DoomsdayUI.h"
#include "ReEngine.h"
#include "Params.h"

class ReProcessor : public juce::AudioProcessor, public doomsday::ui::PresetHost
{
public:
    ReProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "RE4GE"; }
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
    juce::String presetFolderText() override { return "Documents / RE4GE / Presets"; }
    juce::File userPresetFolder() override { return io.folder(); }
    juce::UndoManager* undoManager() override { return &undo; }
    void audition (bool on) override { if (on) pendingAudition.store (true); }

    // envelope templates: PUMP, SWELL, TIGHT, ROLL, PULSE (only the envelope / step knobs move)
    static const juce::StringArray& templateNames();
    void applyTemplate (int);

    // SAMPLE source (message thread)
    bool loadSample (const juce::File&);
    void clearSample();
    juce::String sampleName() const;
    int getSampleSerial() const { return sampleSerial.load(); }

    re::Engine& getEngine() { return engine; }
    re::Params gather() const;
    const doomsday::PresetIO io { "RE4GE", "re4ge" };
    int auditionNote = 31;          // G1 in FL (49 Hz)

private:
    float get (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void set (const char* id, float v);
    re::Engine engine;
    int currentProgram = 0;
    juce::String presetName { "INIT" };
    juce::SpinLock prepareLock;            // prepareToPlay holds it; processBlock only try-locks (silence while FL re-prepares)
    std::atomic<int> sampleSerial { 0 };
    std::atomic<bool> pendingAudition { false };
    juce::Random rng;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReProcessor)
};
