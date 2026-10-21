#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "AlphaEngine.h"
#include "../../Shared/Source/DoomsdayDSP.h"
#include <array>
#include <atomic>

// 5CR34M 4LPH4 v0.1 - first playable build of the new screech engine (see ../PLAN.md)
class AlphaProcessor : public juce::AudioProcessor
{
public:
    AlphaProcessor();
    ~AlphaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "5CR34M 4LPH4"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentProgram); }
    int programIndex() const { return currentProgram; }      // -1 = user preset / random
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::UndoManager undo { 300000, 50 };
    juce::AudioProcessorValueTreeState apvts;

    juce::String presetName() const { return currentName; }

    // RANDOM: a new sound in sensible ranges, level matched (measured offline) - message thread
    void randomize();
    float measureLoudnessDb();                       // the current sound, two held notes, without the SPACE effects
    static constexpr float randomTargetDb = -14.f;
    // user presets: Documents/5CR34M 4LPH4/Presets/*.sapreset
    static juce::File userFolder();
    juce::Array<juce::File> userPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File&);
    alpha::Params gather() const;
    float autoNow() const { return engine.autoNow(); }
    double autoPhaseNow() const { return engine.autoPhaseNow(); }

    static constexpr int scopeSize = 4096;
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };

private:
    alpha::Engine engine;
    doomsday::dsp::Ott3 ott;
    doomsday::dsp::FxChain fx;
    int currentProgram = 0;
    juce::String currentName { "INIT" };
    juce::Random rng;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlphaProcessor)
};
