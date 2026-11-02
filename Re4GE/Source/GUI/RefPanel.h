#pragma once
// RE4GE - SYSTEM -> REFERENCE page: drop / load a reverse bass you like (one beat of it is used, at the FL tempo), then the
// plugin's current sound is rendered (one beat, background thread) and both are compared: the waveforms on top of each other,
// the envelopes, the 6 bands and plain-word hints. GUI only: analysis = re4ge::RefAnalysis, render = re4ge::Exporter.
#include "../PluginProcessor.h"
#include "../Analysis/RefAnalysis.h"
#include "DoomsdayFlat.h"

class RefPanel : public juce::Component, public juce::FileDragAndDropTarget, public juce::SettableTooltipClient
{
public:
    explicit RefPanel (ReProcessor& p) : proc (p) {}
    ~RefPanel() override { alive->store (false); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragHover = false; repaint(); }
    void pageShown();                                 // analyses the plugin again (and loads the saved reference once)
    // tests: synchronous versions
    bool loadForTest (const juce::File& f) { return loadNow (f); }
    void analyseMineForTest();
    const re4ge::RefProfile& refProfile() const { return ref; }
    const re4ge::RefProfile& mineProfile() const { return mine; }
    juce::StringArray hintsNow() const { return re4ge::RefAnalysis::hints (ref, mine); }
private:
    struct Wave { std::vector<float> peaks; };        // |x| peaks of one beat in 400 columns, normalised
    static Wave wave (const float* x, int n);
    juce::Rectangle<float> waveArea() const, bandArea() const, hintArea() const, loadButton() const, mineButton() const;
    bool loadNow (const juce::File&);
    void loadAsync (const juce::File&);
    void analyseMine();
    int beatSamples (double rate) const { return (int) (rate * 60.0 / juce::jlimit (60.0, 300.0, (double) proc.getEngine().tempoNow.load())); }
    ReProcessor& proc;
    re4ge::RefProfile ref, mine;
    Wave refWave, mineWave;
    juce::String refName;
    bool busy = false, dragHover = false, triedSaved = false;
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};
