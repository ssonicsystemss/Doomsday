#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayParams.h"
#include "DoomsdayUI.h"
#include "DSP/Engine.h"
#include "DSP/KickAnalysis.h"
#include "DSP/HitDetector.h"
#include "Parameters/ParamIDs.h"
#include "Parameters/ParamBridge.h"

class ReProcessor : public juce::AudioProcessor, public doomsday::ui::PresetHost, private juce::ValueTree::Listener
{
public:
    // fx = the RE4GE FX build: an effect on the mixer, the kicks come in as audio (input or sidechain)
    explicit ReProcessor (bool fx = false);
    static BusesProperties busesFor (bool fx);
    const bool isFx;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
        return ! isFx || l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return isFx ? "RE4GE FX" : "RE4GE"; }
    bool acceptsMidi() const override { return ! isFx; }
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
    // KICK-TO-REVERSE (message thread): analyse the loaded WAV, and set the region / ROOT from it
    //   use: 0 = TAIL, 1 = BODY + TAIL, 2 = PUNCH .. END (everything after the click), 3 = ALL
    const re::KickSections& kickSections() const { return sections; }
    void detectKick (int use);
    std::atomic<int> fxHits { 0 };                      // RE4GE FX: kicks found so far (the GUI blinks)
    std::atomic<float> fxInLevel { 0.f };
    bool sampleIsEmbeddedOnly() const { return embeddedOnly; }        // the file is gone: the copy inside the project / preset plays
    int getSampleSerial() const { return sampleSerial.load(); }

    // SHAPE DESIGNER curves (message thread). They live in the state (saved with presets / projects, undoable);
    // a listener copies them for the audio thread under a spin lock (the audio thread only try-locks).
    // 0..3 = the SHAPE DESIGNER lanes, 4 / 5 = the CUSTOM distortion curves of stage 1 / 2
    static constexpr int numShapes = 6;
    static const char* shapeKey (int lane);
    doomsday::mod::Shape getShape (int lane) const;
    void setShape (int lane, const doomsday::mod::Shape&, bool undoable = true);
    void shapeFromKnobs (int lane);
    void resetShape (int lane);                       // AMP: the knob envelope drawn as a curve (a start for editing)
    int getShapeSerial() const { return shapeSerial.load(); }

    re::Engine& getEngine() { return engine; }
    re::Params gather() const;
    const doomsday::PresetIO io { "RE4GE", "re4ge" };
    int auditionNote = 31;          // G1 in FL (49 Hz)

private:
    float get (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void applyShapesFromState();
    mutable juce::SpinLock shapeLock;
    std::array<doomsday::mod::Shape, numShapes> shapes;
    mutable std::array<doomsday::mod::Shape, numShapes> lastShapes;
    std::atomic<int> shapeSerial { 0 };
    mutable re4ge::ParamCache paramCache;
    void set (const char* id, float v);
    re::Engine engine;
    int currentProgram = 0;
    juce::String presetName { "INIT" };
    juce::SpinLock prepareLock;            // prepareToPlay holds it; processBlock only try-locks (silence while FL re-prepares)
    std::atomic<int> sampleSerial { 0 };
    re::KickSections sections;
    // RE4GE FX
    void processFx (juce::AudioBuffer<float>&, int n);
    re::HitDetector detector;
    juce::AudioBuffer<float> dry;
    std::vector<float> ring;                       // the input (mono), for LIVE: the last kick
    int ringPos = 0, lastHitPos = -1, silentFor = 0;
    std::array<re::Sample, 2> liveSmp;
    int liveIdx = 0;
    juce::MidiBuffer fxMidi;
    bool embeddedOnly = false;
    void useSample (std::unique_ptr<re::Sample>, const juce::String& path);
    bool restoreSample (const juce::String& path, const juce::String& embedded, double rate, const juce::String& name);
    std::atomic<bool> pendingAudition { false };
    juce::Random rng;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReProcessor)
};
