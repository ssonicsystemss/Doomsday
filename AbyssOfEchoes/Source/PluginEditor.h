#pragma once
#include "PluginProcessor.h"
#include "DoomsdayUI.h"

// =====================================================================
//  ABYSS OF ECHOES - black + molten amber
// =====================================================================
namespace AbyssTheme
{
    const juce::Colour amber { 0xffffa53a }, amberDeep { 0xff6e3a08 }, amberHot { 0xffffe2b0 };
    doomsday::ui::Theme make();
}

// THE ABYSS: the echoes (delay taps, left above / right below) and the reverb tail on one timeline,
// with the live wet level glowing through it
class AbyssDisplay : public juce::Component
{
public:
    explicit AbyssDisplay (AbyssProcessor&);
    void paint (juce::Graphics&) override;
    void tick();
private:
    AbyssProcessor& proc;
    float dlyLevel = 0.f, revLevel = 0.f, phase = 0.f;
    juce::String lastSig;
};

// STEREO FIELD: goniometer (mid up, sides left / right) + correlation bar
class StereoField : public juce::Component
{
public:
    explicit StereoField (AbyssProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    AbyssProcessor& proc;
    std::vector<juce::Point<float>> pts;
    float corr = 1.f, lastWrite = -1.f;
};

class Canvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1400, H = 900;
    explicit Canvas (AbyssProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void renderBackground();
    doomsday::ui::Knob* knob (const char* id, const char* label, bool bipolar = false);
    void grid (juce::Rectangle<int> area, const std::vector<juce::Component*>& cells, int cols);

    AbyssProcessor& proc;
    doomsday::ui::StageLookAndFeel lnf;
    juce::Image background;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> panels;
    AbyssDisplay display;
    StereoField field;
    doomsday::ui::PresetBar presets;
    doomsday::ui::Meter meter;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::unique_ptr<doomsday::ui::Switch> dlyPower, revPower, syncChip, freezeChip, dryChip;
    std::unique_ptr<doomsday::ui::Choice> modeChoice;
    std::vector<juce::Component*> dlyCells, revCells, outCells;
    doomsday::ui::Knob* divKnob = nullptr;
    doomsday::ui::Knob* msKnob = nullptr;
};

class AbyssEditor : public juce::AudioProcessorEditor
{
public:
    explicit AbyssEditor (AbyssProcessor&);
    void resized() override { doomsday::ui::scaleCanvas (*this, canvas, Canvas::W, Canvas::H); }
private:
    Canvas canvas;
};
