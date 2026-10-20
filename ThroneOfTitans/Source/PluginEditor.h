#pragma once
#include "PluginProcessor.h"
#include "DoomsdayUI.h"

// =====================================================================
//  THRONE OF TITANS - black + platinum (bands: ember / platinum / steel blue)
// =====================================================================
namespace TitanTheme
{
    const juce::Colour plat { 0xffdfe4ec }, platDeep { 0xff3c4350 }, platHot { 0xffffffff };
    const juce::Colour band[3] { juce::Colour (0xffff7a4d), juce::Colour (0xffdfe4ec), juce::Colour (0xff66c2ff) };
    doomsday::ui::Theme make();
}

// spectrum before / after, the three bands, draggable crossovers, gain reduction per band
class TitanDisplay : public juce::Component
{
public:
    TitanDisplay (TitanProcessor&, std::function<void (int)> onSelect);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragXo = -1; }
    void tick();
    int selected = 1;
private:
    float xOf (double hz) const;
    double hzOf (float x) const;
    int xoAt (float x) const;
    TitanProcessor& proc;
    std::function<void (int)> onSelect;
    juce::Rectangle<float> plot;
    doomsday::dsp::Spectrum pre, post;
    std::vector<float> scratch;
    std::array<float, 3> gr {}, up {};
    int dragXo = -1, hoverXo = -1;
};

// transfer curve of the selected band with the live level
class CurveView : public juce::Component
{
public:
    explicit CurveView (TitanProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    int band = 1;
    float liveGr = 0.f;
private:
    TitanProcessor& proc;
};

// LUFS (momentary / short-term / integrated), true peak, target, in / out / limiter meters
class LoudnessPanel : public juce::Component
{
public:
    explicit LoudnessPanel (TitanProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    TitanProcessor& proc;
    std::unique_ptr<doomsday::ui::Switch> matchSw;
    std::unique_ptr<doomsday::ui::Choice> targetChoice;
    juce::TextButton resetBtn { "RESET" }, learnBtn { "LEARN" };
    void learnTick();
    float currentTarget() const;
    int learnPhase = -1, learnTicks = 0;          // -1 = idle
    double learnSum = 0.0; int learnCount = 0;
    juce::String learnMsg; int learnMsgTicks = 0;
    float limAvg = 0.f, clipAvg = 0.f;
    float m = -70.f, s = -70.f, i = -70.f, tp = -120.f, tpMax = -120.f, lim = 0.f, inPk = 0.f, outPk = 0.f, clip = 0.f, matchDb = 0.f, inS = -70.f;
};

class BandControls : public juce::Component
{
public:
    BandControls (TitanProcessor&, int band);
    void resized() override;
    void refresh();
    std::unique_ptr<doomsday::ui::Switch> onSw, soloSw;
private:
    TitanProcessor& proc;
    const int band;
    std::vector<std::unique_ptr<doomsday::ui::Knob>> knobs;
};

class Canvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1400, H = 900;
    explicit Canvas (TitanProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    void select (int);
    void renderBackground();
    juce::Rectangle<int> chipBounds (int b) const;
    TitanProcessor& proc;
    doomsday::ui::StageLookAndFeel lnf;
    juce::Image background;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> panels;
    TitanDisplay display;
    CurveView curve;
    LoudnessPanel loud;
    doomsday::ui::PresetBar presets;
    std::array<std::unique_ptr<BandControls>, 3> bands;
    std::unique_ptr<doomsday::ui::Knob> limitKnob;
    std::vector<std::unique_ptr<doomsday::ui::Knob>> masterKnobs;
    juce::Rectangle<int> chipArea;
    std::array<float, 3> chipGr {};
    int shown = -1;
};

class TitanEditor : public juce::AudioProcessorEditor
{
public:
    explicit TitanEditor (TitanProcessor&);
    void resized() override { doomsday::ui::scaleCanvas (*this, canvas, Canvas::W, Canvas::H); }
private:
    Canvas canvas;
};
