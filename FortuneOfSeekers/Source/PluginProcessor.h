#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "FortuneEngine.h"
#include "Params.h"

class FortuneProcessor : public juce::AudioProcessor
{
public:
    FortuneProcessor();
    ~FortuneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Fortune of Seekers"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // presets (message thread)
    static juce::File getUserPresetFolder();
    juce::Array<juce::File> getUserPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File&);
    void randomize();
    juce::String getPresetName() const { return currentPresetName; }

    // visuals
    FortuneEngine::PathFeed& getPath (int part) { return engine.path[(size_t) part]; }
    float takePeak() { return engine.outPeak.exchange (0.f); }

    int getActiveVoices() const { return engine.activeVoices(); }

    // audition from the GUI / tests
    void auditionChord (bool on) { audition.store (on ? 1 : 2); }

private:
    EngineParams gatherParams (double bpm, double ppq) const;
    void applyValues (const juce::XmlElement&);

    using A = std::atomic<float>*;
    struct OscPtrs { A on {}, wave {}, voices {}, oct {}, semi {}, vol {}, phase {}, detune {}, stereo {}, pan {}, scream {}; };
    struct PartPtrs
    {
        std::array<OscPtrs, 2> osc;
        A on {}, ftype {}, cutoff {}, reso {}, fdrive {}, fenv {}, fdecay {}, att {}, dec {}, sus {}, rel {}, level {};
        A distOn {}, distType {}, distDrive {}, distMix {}, choOn {}, choRate {}, choDepth {}, choMix {};
        A dlyOn {}, dlyDiv {}, dlyFb {}, dlyMix {}, dlyPP {}, revOn {}, revSize {}, revDamp {}, revMix {};
        A pumpOn {}, pumpDiv {}, pumpDepth {}, pumpShape {};
    };
    struct Ptrs { std::array<PartPtrs, 2> part; A mono {}, glide {}, fortune {}, volume {}; } ptr;

    FortuneEngine engine;
    std::atomic<int> audition { 0 };
    bool auditionHeld = false;
    int currentProgram = 0;
    juce::String currentPresetName { "Seeker's Anthem" };
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FortuneProcessor)
};
