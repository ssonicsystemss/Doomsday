#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ScreechEngine.h"
#include "Params.h"
#include <array>
#include <atomic>

class ProphecyProcessor : public juce::AudioProcessor
{
public:
    ProphecyProcessor();
    ~ProphecyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Prophecy of Scream"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // presets (message thread)
    static juce::File getUserPresetFolder();
    juce::Array<juce::File> getUserPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    void randomize();
    // loudness (dB RMS) of the current sound for two held notes, measured offline with a private engine (message thread only)
    float measureLoudnessDb (bool atVolume) const;
    static constexpr float randomTargetDb = -11.f;   // RANDOMIZE sets VOLUME so every random sound lands here
    juce::String getPresetName() const { return currentPresetName; }

    // oscilloscope feed (written on audio thread, read on UI thread)
    static constexpr int scopeSize = 4096;
    std::array<float, scopeSize> scopeBuffer {};
    std::atomic<int> scopeWritePos { 0 };

private:
    EngineParams gatherParams (double bpm) const;
    void applyValues (const juce::XmlElement& xml);

    using A = std::atomic<float>*;
    struct OscPtrs
    {
        A on {}, level {}, oct {}, semi {}, fine {}, wave {}, unison {}, detune {}, width {}, fm {}, fmRatio {}, penv {}, pdecay {};
        A distOn {}, dtype {}, drive {}, bite {}, dmix {};
        A filtOn {}, ftype {}, cutoff {}, reso {}, fenv {}, fdecay {}, keytrack {};
        A lfoOn {}, lshape {}, lsync {}, lrate {}, lfoDiv {}, lcut {}, lpitch {};
        A noise {}, lamp {}, formOn {}, formHz {}, formGain {}, formQ {}, formEnv {}, formDec {}, formKey {}, formLfo {};
    };
    struct Ptrs
    {
        std::array<OscPtrs, ParamIDs::numOscs> osc;
        A glide {}, bend {}, legato {}, scream {}, attack {}, decay {}, sustain {}, release {};
        A dlyOn {}, dtime {}, dfb {}, dfx {}, pingpong {}, revOn {}, rsize {}, rdamp {}, rmix {}, volume {};
        A sweep {}, sweepTime {}, ott {}, spread {};
    } ptr;

    ScreechEngine engine;
    int currentProgram = 0;
    juce::String currentPresetName { "Prophecy Lead" };
    juce::Random randomGen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProphecyProcessor)
};
