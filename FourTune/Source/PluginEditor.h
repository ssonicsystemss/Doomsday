#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "DoomsdayFlat.h"
#include "DoomsdayUI.h"
#include "PianoRoll.h"

// 4TUNE GUI (no DSP here): SYNTH / MOD / FX / MELODY pages, the signal flow OSCILLATORS -> FILTER -> SCREAM -> FX -> OUTPUT
// always visible, FORTUNE big and central (spec 29)
namespace tuneui
{
    // LIQUID GLASS in violet + lime (owner 2026-11-18: "styl liquid glass ale kolory fiolet i limonka jak w FoS")
    inline juce::Colour groupA()  { return juce::Colour (0xffb6f23e); }    // PART A lime (Fortune of Seekers identity)
    inline juce::Colour groupB()  { return juce::Colour (0xffa66bff); }    // PART B violet
    inline juce::Colour fortune() { return juce::Colour (0xffd8ff4a); }    // FORTUNE: the brightest lime
    inline juce::Colour filter()  { return juce::Colour (0xffc7a6ff); }    // light lilac
    inline juce::Colour scream()  { return juce::Colour (0xffff4fd8); }    // hot magenta-violet
    inline juce::Colour fx()      { return juce::Colour (0xff7ef0a8); }    // mint-lime
    inline juce::Colour neutral() { return juce::Colour (0xffc9c3d6); }
    // glass: translucent violet-tinted panel, hairline edge, highlight along the top
    void glassCard (juce::Graphics&, doomsday::flat::FlatLookAndFeel&, juce::Rectangle<int>, const juce::String& title, juce::Colour dot);
    void glassBackground (juce::Graphics&, juce::Rectangle<float>);

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
        juce::TextButton pluckEnv { "PLUCK" }, leadEnv { "LEAD" };
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

    class ValueKnob;
    class MidiDragSource;
    // MELODY (spec 21-24): generator settings, GENERATE, PREVIEW, the piano roll, edit tools, DRAG MIDI into FL
    class MelodyPage : public Page, private juce::Timer
    {
    public:
        explicit MelodyPage (TuneProcessor&);
        ~MelodyPage() override;
        void resized() override;
        void paintOverChildren (juce::Graphics&) override;
        PianoRoll& roll() { return *pianoRoll; }
        void syncFromState();
    private:
        void timerCallback() override;
        juce::ComboBox key, scale, mode, bars, range, shift, gridBox;
        std::vector<std::unique_ptr<ValueKnob>> genKnobs;
        juce::TextButton generate { "GENERATE" }, newSeed { "NEW IDEA" }, bounce { "ROOT BOUNCE" }, preview { "PREVIEW" };
        std::array<juce::TextButton, 12> custom;
        std::vector<std::unique_ptr<juce::TextButton>> tools;
        std::unique_ptr<PianoRoll> pianoRoll;
        std::unique_ptr<MidiDragSource> dragSrc;
    };

    // PRESETS (spec 27): categories, favourites, search, author, LOAD / FAVOURITE / RENAME / DELETE / DUPLICATE / SAVE AS
    class PresetPage : public Page, public juce::ListBoxModel
    {
    public:
        explicit PresetPage (TuneProcessor&);
        void resized() override;
        void paintOverChildren (juce::Graphics&) override;
        void visibilityChanged() override { if (isVisible()) rebuild(); }
        int getNumRows() override { return (int) rows.size(); }
        void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
        void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { loadSelected(); }
        void selectedRowsChanged (int) override { repaint(); }
        void rebuild();
        std::function<void()> onLoaded;
        tune::lib::Library& library() { return lib; }
    private:
        const tune::lib::Entry* selected() const;
        void loadSelected();
        tune::lib::Library lib;
        juce::String category { "ALL" };
        std::vector<int> rows;
        std::vector<std::unique_ptr<juce::TextButton>> catButtons;
        juce::TextEditor search, saveName, author;
        juce::ComboBox saveCat;
        juce::ListBox list { "presets", this };
        juce::TextButton load { "LOAD" }, fav { "FAVOURITE" }, rename { "RENAME" }, del { "DELETE" }, dup { "DUPLICATE" }, save { "SAVE AS" };
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
    std::vector<tuneui::Page*> pages() { return { &synth, &mod, &fxp, &melody, &presets }; }
    void refreshNow() { timerCallback(); }
private:
    void timerCallback() override;
    TuneProcessor& proc;
    doomsday::flat::FlatLookAndFeel lnf;
    doomsday::ui::StageLookAndFeel stageLnf { makeTheme() };
    static doomsday::ui::Theme makeTheme();
    doomsday::ui::PresetBar presetBar;
    doomsday::flat::TabStrip tabs { { "SYNTH", "MOD", "FX", "MELODY", "PRESETS" }, tuneui::fortune() };
    juce::TextButton fullRandom { "FULL RANDOM" };
    tuneui::FlowBar flow;
    tuneui::SynthPage synth;
    tuneui::ModPage mod;
    tuneui::FxPage fxp;
    tuneui::MelodyPage melody;
    tuneui::PresetPage presets;
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
