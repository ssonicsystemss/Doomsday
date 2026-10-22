#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayParams.h"
#include "DoomsdayUI.h"
#include "GeEngine.h"
#include "Params.h"

class GeProcessor : public juce::AudioProcessor, public doomsday::ui::PresetHost
{
public:
    GeProcessor();
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

    // SAMPLE layer (message thread)
    bool loadSample (const juce::File&);
    void clearSample();
    juce::String sampleName() const;
    int getSampleSerial() const { return sampleSerial.load(); }
    static const char* shapeKey (int which);

    // drawn envelopes (0 = AMP, 1 = DRIVE, 2 = PITCH), shared with the audio thread
    ge::Shape getShape (int which) const;
    void setShape (int which, const ge::Shape&);
    int getShapeSerial() const { return shapeSerial.load(); }

    GeEngine& getEngine() { return engine; }
    GeParams gather() const;
    GeParams gatherForGui() const;      // the GUI waits for the envelopes instead of reading the audio thread's copies
    const doomsday::PresetIO io { "4GE", "4ge" };
    int auditionNote = 31;          // G1 in FL (49 Hz)

    // SOLO / MUTE per layer (ge::Layer bits) - GUI only, not saved with the preset
    std::atomic<int> muteMask { 0 }, soloMask { 0 };
    int effectiveMute() const { const int s = soloMask.load(); return s != 0 ? (ge::LAll & ~s) : muteMask.load(); }

private:
    float get (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void applyShapesFromState();
    GeEngine engine;
    int currentProgram = 0;
    juce::String presetName { "INIT" };
    mutable juce::SpinLock shapeLock;
    juce::SpinLock prepareLock;            // prepareToPlay holds it; processBlock only try-locks (silence while FL re-prepares)
    std::array<ge::Shape, 3> shapes;
    mutable ge::Shape lastAmp, lastDrive, lastPitch;      // audio-thread copies when the lock is busy
    std::atomic<int> shapeSerial { 0 }, sampleSerial { 0 };
    std::atomic<bool> pendingAudition { false };
    juce::Random rng;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeProcessor)
};
