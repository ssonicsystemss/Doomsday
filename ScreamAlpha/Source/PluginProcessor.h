#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "AlphaEngine.h"
#include "AlphaMod.h"
#include "../../Shared/Source/DoomsdayDSP.h"
#include <array>
#include <atomic>

// 5CR34M 4LPH4 - the new screech engine (see ../PLAN.md); MOD 1 / MOD 2 in AlphaMod.h
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
    // a parameter value that replaces the knob value (MOD 1 / MOD 2), real units
    struct Ov { const char* id = nullptr; float val = 0.f; };
    alpha::Params gather (const Ov* ov = nullptr, int numOv = 0) const;

    // ---- MOD 1 / MOD 2
    static constexpr int numMods = alpha::mod::numMods;
    doomsday::mod::Shape getCustom (int m) const;                 // the drawn CUSTOM shape (message thread)
    void setCustom (int m, const doomsday::mod::Shape&);
    int customSerial() const { return customSer.load(); }
    int modTarget (int m) const;                                  // 1-based target index, 0 = none
    juce::RangedAudioParameter* targetParam (int index) const;    // the knob parameter of a target index
    float modPhase (int m) const { return modPh[(size_t) m].load(); }     // 0..1 position in the cycle
    float modValue (int m) const { return modVal[(size_t) m].load(); }    // 0..1 shape value now
    float modKnob01 (int m) const { return modKnob[(size_t) m].load(); }  // where the assigned knob is moved to now (0..1), -1 = idle
    double modBeats (int m) const;                                // cycle length in beats
    // old projects / presets: cutoff automation with "Depth" (autoOct) -> MOD 1 -> CUTOFF
    void migrateLegacyAuto();

    static constexpr int scopeSize = 4096;
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };

private:
    float pv (const char* id, const Ov* ov, int numOv) const;
    void processChunk (juce::AudioBuffer<float>&, int start, int len, const juce::MidiBuffer&, double bpm, double beat);
    void resetCustomShapes();

    alpha::Engine engine;
    std::array<doomsday::mod::Shape, alpha::mod::numMods> custom, customAudio;
    mutable juce::SpinLock customLock;
    std::atomic<int> customSer { 0 };
    std::array<std::atomic<float>, alpha::mod::numMods> modPh {}, modVal {}, modKnob {};
    std::array<int, alpha::mod::numMods> modSeg {};
    std::vector<juce::RangedAudioParameter*> targets;             // index 0 = none
    double freeBeat = 0.0;
    static constexpr int modChunk = 32;
    doomsday::dsp::Ott3 ott;
    doomsday::dsp::FxChain fx;
    int currentProgram = 0;
    juce::String currentName { "INIT" };
    juce::Random rng;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlphaProcessor)
};
