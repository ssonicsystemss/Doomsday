#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "KickEngine.h"
#include "Params.h"

class AncientProcessor : public juce::AudioProcessor
{
public:
    AncientProcessor();
    ~AncientProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Ancient Drums"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // ---- samples (message thread)
    bool loadUserSample (int layer, const juce::File& file);
    SamplePtr getLayerSample (int layer) const;          // what the layer plays right now (may be null)
    juce::String getUserSamplePath (int layer) const;
    static bool isSupportedAudioFile (const juce::String& path);

    // ---- volume envelopes (message thread writes, audio thread reads)
    EnvPtr getEnvelope (int layer) const { return std::atomic_load (&envelopes[(size_t) layer]); }
    void setEnvelope (int layer, Envelope e);
    static Envelope defaultEnvelope (int layer);

    // ---- presets (message thread)
    static juce::File getUserPresetFolder();
    juce::Array<juce::File> getUserPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File&);
    void randomize();
    juce::String getPresetName() const { return currentPresetName; }

    // ---- audition from the GUI
    void triggerTest() { testTrigger.store (true); }

    // ---- last hit for the display
    const std::vector<float>& getHitBuffer() const { return engine.hitBuffer; }
    int getHitLength() const { return engine.hitLength.load(); }
    int getHitSerial() const { return engine.hitSerial.load(); }

    // ---- editor UI state that is saved with the project
    int getSelectedLayer() const { return (int) apvts.state.getProperty ("selLayer", (int) Kick); }
    void setSelectedLayer (int l) { apvts.state.setProperty ("selLayer", juce::jlimit (0, 2, l), nullptr); }
    float getZoomMs() const { return (float) apvts.state.getProperty ("zoomMs", 1000.f); }
    void setZoomMs (float ms) { apvts.state.setProperty ("zoomMs", ms, nullptr); }

private:
    KickParams gatherParams();
    void applyValues (const juce::XmlElement&);
    void restoreUserSamplesFromState();
    void restoreEnvelopesFromState();
    void collectGarbage();

    struct Ptrs
    {
        using A = std::atomic<float>*;
        std::array<A, NumSampleLayers> on {}, src {}, level {}, pitch {}, offset {}, hp {}, lp {}, rev {}, forge {};
        A punchKey {}, fOn {}, fDrive {}, fFocus {}, fGrit {}, fCrunch {}, fTone {}, fOtt {}, fSmooth {}, fMix {};
        A root {}, preGain {}, clipOn {}, clipCeil {}, clipKnee {}, limOn {}, limCeil {}, limRel {}, volume {};
    } ptr;

    KickEngine engine;
    juce::AudioFormatManager formats;

    std::array<SamplePtr, NumSampleLayers> userSample;   // atomic access via std::atomic_load/store
    std::array<EnvPtr, NumSampleLayers> envelopes;       // atomic access via std::atomic_load/store
    std::vector<std::shared_ptr<const void>> keepAlive;  // old objects freed on the message thread only
    std::atomic<bool> testTrigger { false };

    int currentProgram = 0;
    juce::String currentPresetName { "Ancient Raw" };
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AncientProcessor)
};
