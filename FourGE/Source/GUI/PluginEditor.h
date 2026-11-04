#pragma once
// =====================================================================
//  4GE editor - FIRST VERSION for module 01 (the full interface is module 17, designed from the real parameters).
//  Header (LENGTH / MASTER / AUTO LENGTH / PHASE RESET / PITCH TRACKING / VOICE, presets + PLAY), PITCH ENGINE row,
//  the central REAL waveform (drag = WAV), four part cards (LEVEL / PITCH / LENGTH / DRIVE / MUTE / SOLO),
//  the selected part's detail and the CORE MIXER (fader / pan / phase invert / peak meter per part).
//  GUI only: reads parameters through the APVTS, the waveform renders on its own thread.
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "DoomsdayUI.h"
#include "../PluginProcessor.h"
#include "WaveView.h"
#include "Palette.h"

class GeCanvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1280, H = 930;
    explicit GeCanvas (GeProcessor&);
    ~GeCanvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void selectPart (int);
    int selectedPart() const { return selected; }
    fge::gui::WaveView& waveView() { return wave; }
    juce::Component* findControl (const juce::String& paramId);     // tests
private:
    void timerCallback() override;
    using Knob = doomsday::flat::Knob;
    using Toggle = doomsday::flat::Toggle;
    using Seg = doomsday::flat::Segmented;
    Knob* knob (const char* id, const juce::String& label, juce::Colour accent, int d = 40, bool bipolar = false);
    Toggle* toggle (const char* id, const juce::String& text, Toggle::Style, juce::Colour accent);
    Seg* seg (const char* id, juce::StringArray labels, juce::Colour accent);
    juce::ComboBox* combo (const char* id);
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    std::vector<juce::Component*> masterRow;
    void layoutDetail();
public:
    void updateEnabled();
private:

    GeProcessor& proc;
    doomsday::flat::FlatLookAndFeel lnf;
    juce::TooltipWindow tips { this, 600 };
    doomsday::ui::StageLookAndFeel stageLnf { makeTheme() };      // the shared preset bar draws with the stage look
    static doomsday::ui::Theme makeTheme();
    doomsday::ui::PresetBar presetBar;
    fge::gui::WaveView wave;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::multimap<juce::String, juce::Component*> byId;

    // header / pitch engine
    std::vector<juce::Component*> header, pitchRow;
    // the four part cards and their details
    std::array<std::vector<juce::Component*>, 4> card, detail, mixer;
    std::array<juce::Rectangle<int>, 4> cardArea;
    juce::Rectangle<int> headerArea, pitchArea, waveArea, detailArea, mixerArea, meterArea;
    std::array<juce::Rectangle<int>, 4> mixMeterArea;
    int selected = 2;
    std::array<float, 4> meter {};
    float outMeter = 0.f;
};

class GeEditor : public juce::AudioProcessorEditor
{
public:
    explicit GeEditor (GeProcessor&);
    void resized() override;
    GeCanvas canvas;
};
