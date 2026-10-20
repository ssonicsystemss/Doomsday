#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayParams.h"
#include "DoomsdayUI.h"
#include "EchoEngine.h"
#include "Params.h"
#include <optional>

class AbyssProcessor : public juce::AudioProcessor, public doomsday::ui::PresetHost
{
public:
    AbyssProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Abyss of Echoes"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& d) override { doomsday::PresetIO::getState (apvts, d, currentProgram, presetName); }
    void setStateInformation (const void* data, int size) override { doomsday::PresetIO::setState (*this, apvts, data, size, currentProgram, presetName); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // PresetHost
    int numFactoryPresets() override { return getNumPrograms(); }
    juce::String factoryPresetName (int i) override { return getProgramName (i); }
    void loadFactoryPreset (int i) override { setCurrentProgram (i); }
    juce::Array<juce::File> userPresetFiles() override { return io.list(); }
    bool loadUserPresetFile (const juce::File& f) override;
    bool saveUserPresetAs (const juce::String& name) override;
    void randomizeSound() override {}
    juce::String currentPresetName() override { return presetName; }
    juce::String presetFolderText() override { return "Documents / Abyss of Echoes / Presets"; }

    EchoEngine& getEngine() { return engine; }
    std::atomic<double> bpm { 150.0 };
    EchoParams gather() const;
    const doomsday::PresetIO io { "Abyss of Echoes", "aoe" };

private:
    float get (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    EchoEngine engine;
    int currentProgram = 0;
    juce::String presetName { "Default Space" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbyssProcessor)
};
