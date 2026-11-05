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
#include "EnvPage.h"
#include "OscPage.h"
#include "LayerPage.h"
#include "DistPage.h"
#include "ScreechPage.h"
#include "EqPage.h"
#include "DynPage.h"
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
    void showPage (int);                                             // 0 = MAIN, 1 = ENVELOPES (module 02), 2 = OSCILLATORS (module 03)
    fge::gui::OscPage& oscillators() { return *oscPage; }
    fge::gui::LayerPage& layersPage() { return *layerPage; }
    fge::gui::DistPage& distPage() { return *distortionPage; }
    fge::gui::ScreechPage& screechPage() { return *scrPage; }
    fge::gui::EqPage& eqPage() { return *eqPg; }
    fge::gui::DynPage& dynPage() { return *dynPg; }
    int page() const { return pageIdx; }
    fge::gui::EnvPage& envelopes() { return *envPage; }
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
    void paintMain (juce::Graphics&, const std::function<void (juce::Rectangle<int>, const juce::String&, juce::Colour)>& panel);
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
    int pageIdx = 0;
    std::unique_ptr<fge::gui::EnvPage> envPage;
    std::unique_ptr<fge::gui::OscPage> oscPage;
    std::unique_ptr<fge::gui::LayerPage> layerPage;
    std::unique_ptr<fge::gui::DistPage> distortionPage;
    std::unique_ptr<fge::gui::ScreechPage> scrPage;
    std::unique_ptr<fge::gui::EqPage> eqPg;
    std::unique_ptr<fge::gui::DynPage> dynPg;
    std::array<juce::TextButton, 8> pageBtn;
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
