#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayParams.h"
#include "DoomsdayUI.h"
#include "Params.h"
#include "TuneEngine.h"
#include "Melody/MelodyGen.h"
#include "Melody/MelodyPlayer.h"
#include "Presets/TuneLibrary.h"
#include <atomic>

// 4TUNE - hardstyle melody synthesizer (see ../SPEC.md, ../PLAN.md). Files / presets here, DSP in TuneEngine.h, GUI in PluginEditor.
class TuneProcessor : public juce::AudioProcessor, public doomsday::ui::PresetHost, private juce::ValueTree::Listener
{
public:
    TuneProcessor();
    ~TuneProcessor() override { apvts.state.removeListener (this); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override { panicPending.store (true); }
    void reset() override { panicPending.store (true); }        // the host stopped / restarted the audio: no note survives
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "4TUNE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentProgram); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& d) override { doomsday::PresetIO::getState (apvts, d, currentProgram, presetName); }
    void setStateInformation (const void* data, int size) override;

    juce::UndoManager undo { 300000, 50 };      // declared before apvts
    juce::AudioProcessorValueTreeState apvts;

    // PresetHost
    int numFactoryPresets() override { return getNumPrograms(); }
    juce::String factoryPresetName (int i) override { return getProgramName (i); }
    void loadFactoryPreset (int i) override { setCurrentProgram (i); }
    juce::Array<juce::File> userPresetFiles() override { return io.list(); }
    bool loadUserPresetFile (const juce::File& f) override;
    bool saveUserPresetAs (const juce::String& name) override;
    void randomizeSound() override { smartRandom(); }
    juce::String currentPresetName() override { return presetName; }
    juce::String presetFolderText() override { return "Documents / 4TUNE / Presets"; }
    juce::File userPresetFolder() override { return io.folder(); }
    juce::UndoManager* undoManager() override { return &undo; }
    void audition (bool on) override { auditionCmd.store (on ? 1 : 2); }

    // PRESET LIBRARY (spec 27): the favourites file is the preset bar's, the author is remembered
    tune::lib::Library makeLibrary() { juce::StringArray f; for (int i = 0; i < getNumPrograms(); ++i) f.add (getProgramName (i)); return { f, io.folder(), io.extension, favouritesFile() }; }
    juce::File favouritesFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Doomsday")
                 .getChildFile (juce::File::createLegalFileName (presetFolderText()) + " favourites.txt");
    }
    juce::String lastAuthor();
    bool saveUserPreset (const juce::String& name, const juce::String& author);
    // AMP ENVELOPE quick shapes (spec 7): PLUCK = short decay, low sustain; LEAD = longer, high sustain (1 undo)
    void envelopeMode (bool lead);

    // RANDOM (spec 28): FULL = every sound-design parameter, SMART = musical ranges + safe relationships
    void fullRandom();
    void smartRandom();

    // tests / GUI
    tune::dsp::Settings gather (double bpm) const;
    float param (tune::P::Id id) const { return raw[(size_t) id]->load(); }
    juce::RangedAudioParameter* paramObj (tune::P::Id id) const { return params[(size_t) id]; }
    float outputPeak() const { return peak.load(); }
    float fortuneNow() const { return param (tune::P::Fortune); }
    const doomsday::PresetIO io { "4TUNE", "4tune" };
    // offline: the engine with the current settings (RenderTest / previews)
    tune::dsp::Engine& getEngine() { return engine; }

    // ---- MELODY (spec 21-24): settings + notes live in the state child MELODY (saved with the project, Ctrl+Z works)
    juce::ValueTree melodyTree();
    tune::melody::Settings melodySettings();
    void setMelodySetting (const juce::Identifier& id, const juce::var& v);           // no undo (generator controls)
    std::vector<tune::melody::Note> melodyNotes();
    void setMelodyNotes (const std::vector<tune::melody::Note>&, bool newTransaction, const juce::String& name = "Edit melody");
    void generateMelody (bool newSeed);                                                 // 1 click = 1 undo
    double melodyLengthBeats();
    std::atomic<bool> melodyPreview { false };
    std::atomic<bool> editorOpen { false };               // PREVIEW and PLAY only sound while the window is open
    double previewPosition() const { return melodyPlayer.positionInLoop (lastPpq.load()); }
    bool previewPlaying() const { return melodyPlayer.playing(); }
    double hostBpm() const { return lastBpm.load(); }
    std::atomic<int> melodyVersion { 0 };                 // GUI polls it to refresh the piano roll

private:
    void setValue (tune::P::Id id, float realValue);
    std::array<std::atomic<float>*, tune::P::count> raw {};
    std::array<juce::RangedAudioParameter*, tune::P::count> params {};
    tune::dsp::Engine engine;
    std::atomic<int> auditionCmd { 0 };
    bool auditionHeld = false, firstBlock = true;
    std::atomic<bool> panicPending { false };
    int auditionSamples = 0;                              // PLAY holds the chord at most a few seconds
    std::atomic<float> peak { 0.f };
    int currentProgram = 0;
    juce::String presetName { "INIT" };
    juce::Random rng;
    tune::melody::Player melodyPlayer;
    std::atomic<double> lastPpq { -1.0 }, lastBpm { 150.0 };
    void publishMelody();
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { publishMelody(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { publishMelody(); }
    void valueTreePropertyChanged (juce::ValueTree& t, const juce::Identifier&) override { if (t.hasType ("MELODY") || t.hasType ("N")) publishMelody(); }
    void valueTreeRedirected (juce::ValueTree&) override { publishMelody(); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneProcessor)
};
