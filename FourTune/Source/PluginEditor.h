#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "DoomsdayFlat.h"
#include "DoomsdayUI.h"

// 4TUNE GUI (no DSP here): SYNTH / MOD / FX / MELODY pages, the signal flow OSCILLATORS -> FILTER -> SCREAM -> FX -> OUTPUT
// always visible, FORTUNE big and central (spec 29)
namespace tuneui
{
    inline juce::Colour groupA()  { return juce::Colour (0xff3ddc84); }    // PART A green (Fortune of Seekers identity)
    inline juce::Colour groupB()  { return juce::Colour (0xffa66bff); }    // PART B violet
    inline juce::Colour fortune() { return juce::Colour (0xffffc23d); }    // FORTUNE gold
    inline juce::Colour filter()  { return juce::Colour (0xff4ea1ff); }
    inline juce::Colour scream()  { return juce::Colour (0xffff5a36); }
    inline juce::Colour fx()      { return juce::Colour (0xff36c9d6); }
    inline juce::Colour neutral() { return juce::Colour (0xffb4bac3); }

    struct Card { juce::String title; juce::Colour dot; juce::Rectangle<int> area; };

    // one page: owns its controls, paints its cards
    class Page : public juce::Component
    {
    public:
        Page (TuneProcessor& p) : proc (p) {}
        void paint (juce::Graphics&) override;
        doomsday::flat::Knob* knob (const char* id, const char* label, juce::Colour c, bool bipolar = false, int d = 36);
        juce::ComboBox* combo (const char* id);
        doomsday::flat::Toggle* toggle (const char* id, const char* text, juce::Colour c, bool chip = true);
        std::vector<Card> cards;
        std::vector<doomsday::flat::Knob*> knobs;          // every knob of the page (tests: each one is wired)
    protected:
        TuneProcessor& proc;
        std::vector<std::unique_ptr<juce::Component>> owned;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    };

    class FortuneView;
    class SynthPage : public Page
    {
    public:
        explicit SynthPage (TuneProcessor&);
        ~SynthPage() override;
        void resized() override;
        void paintOverChildren (juce::Graphics&) override;
        void tick();
    private:
        struct OscRow { doomsday::flat::Toggle* on; juce::ComboBox* wave; doomsday::flat::Toggle* retrig; std::vector<juce::Component*> knobs; };
        std::array<OscRow, 4> osc;
        std::vector<juce::Component*> grpA, grpB, flt, fenv, aenv, perf, scr;
        juce::ComboBox *fltType = nullptr, *fltRoute = nullptr, *playMode = nullptr;
        doomsday::flat::Knob* fortuneKnob = nullptr, *screamKnob = nullptr;
        doomsday::flat::Segmented* scrOs = nullptr;
        std::unique_ptr<FortuneView> fview;
    };

    class ModPage : public Page
    {
    public:
        explicit ModPage (TuneProcessor&);
        void resized() override;
        void paintOverChildren (juce::Graphics&) override;
    private:
        struct LfoUi { juce::ComboBox* shape; juce::ComboBox* div; doomsday::flat::Toggle* sync; doomsday::flat::Toggle* retrig; std::vector<juce::Component*> knobs; };
        std::array<LfoUi, 2> lfo;
        std::vector<juce::Component*> menv, ftAmt;
        struct SlotUi { juce::ComboBox* src; juce::ComboBox* dst; doomsday::flat::Knob* amt; };
        std::array<SlotUi, 16> slots;
    };

    class FxPage : public Page
    {
    public:
        explicit FxPage (TuneProcessor&);
        void resized() override;
    private:
        struct Unit { doomsday::flat::Toggle* on = nullptr; juce::Component* extra = nullptr; juce::Component* extra2 = nullptr; std::vector<juce::Component*> knobs; };
        std::array<Unit, 8> units;
    };

    class MelodyPage : public Page
    {
    public:
        explicit MelodyPage (TuneProcessor& p) : Page (p) {}
        void paint (juce::Graphics&) override;
    };

    // OSCILLATORS -> FILTER -> SCREAM -> FX -> OUTPUT (click = open the page; lit = active)
    class FlowBar : public juce::Component
    {
    public:
        explicit FlowBar (TuneProcessor& p) : proc (p) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        std::function<void (int)> onPage;
        void tick() { repaint(); }
    private:
        juce::Rectangle<float> item (int i) const;
        TuneProcessor& proc;
    };
}

class TuneCanvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1440, H = 980;
    explicit TuneCanvas (TuneProcessor&);
    ~TuneCanvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void showPage (int);
    int page() const { return current; }
    std::vector<tuneui::Page*> pages() { return { &synth, &mod, &fxp, &melody }; }
    void refreshNow() { timerCallback(); }
private:
    void timerCallback() override;
    TuneProcessor& proc;
    doomsday::flat::FlatLookAndFeel lnf;
    doomsday::ui::StageLookAndFeel stageLnf { makeTheme() };
    static doomsday::ui::Theme makeTheme();
    doomsday::ui::PresetBar presetBar;
    doomsday::flat::TabStrip tabs { { "SYNTH", "MOD", "FX", "MELODY" }, tuneui::fortune() };
    juce::TextButton fullRandom { "FULL RANDOM" };
    tuneui::FlowBar flow;
    tuneui::SynthPage synth;
    tuneui::ModPage mod;
    tuneui::FxPage fxp;
    tuneui::MelodyPage melody;
    int current = 0;
    juce::TooltipWindow tips { this, 600 };
};

class TuneEditor : public juce::AudioProcessorEditor
{
public:
    explicit TuneEditor (TuneProcessor&);
    void resized() override;
    TuneCanvas canvas;
};
