#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayParams.h"
#include "DoomsdayUI.h"
#include "ForgeEngine.h"
#include "Params.h"

class ForgeProcessor : public juce::AudioProcessor, public doomsday::ui::PresetHost
{
public:
    ForgeProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Forge of Wrath"; }
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
    juce::String presetFolderText() override { return "Documents / Forge of Wrath / Presets"; }
    void audition (bool on) override { if (on) pendingAudition.store (true); }

    // drawn envelopes (0 = AMP, 1 = DRIVE), shared with the audio thread
    doomsday::kick::Shape getShape (int which) const;
    void setShape (int which, const doomsday::kick::Shape&);
    int getShapeSerial() const { return shapeSerial.load(); }

    ForgeEngine& getEngine() { return engine; }
    ForgeParams gather() const;
    const doomsday::PresetIO io { "Forge of Wrath", "fow" };
    int auditionNote = 31;          // G1 in FL (49 Hz)

private:
    float get (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void applyShapesFromState();
    ForgeEngine engine;
    int currentProgram = 0;
    juce::String presetName { "Forge Default" };
    mutable juce::SpinLock shapeLock;
    std::array<doomsday::kick::Shape, 2> shapes;
    mutable doomsday::kick::Shape lastAmp, lastDrive;      // audio-thread copies when the lock is busy
    std::atomic<int> shapeSerial { 0 };
    std::atomic<bool> pendingAudition { false };
    juce::Random rng;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForgeProcessor)
};
