#pragma once
#include "PluginProcessor.h"
#include "DoomsdayUI.h"

// =====================================================================
//  MIRROR OF SOULS - black + ice blue
// =====================================================================
namespace MirrorTheme
{
    const juce::Colour ice { 0xff7fe3ff }, iceDeep { 0xff0e4a66 }, iceHot { 0xffe0f9ff };
    doomsday::ui::Theme make();
}

// the big screen: spectrum before / after, EQ curve, draggable band points
class EqDisplay : public juce::Component
{
public:
    EqDisplay (MirrorProcessor&, std::function<void (int)> onSelect);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = {}; hoverBand = -1; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void tick();
    int selected = 2;
private:
    float xOf (double hz) const;
    double hzOf (float x) const;
    float eqY (float db) const;
    float dbOfY (float y) const;
    float anY (float dbfs) const;
    juce::Point<float> nodePos (int band) const;
    int bandAt (juce::Point<float>) const;
    juce::RangedAudioParameter* prm (int band, const char* key) const;
    float val (int band, const char* key) const;
    void setVal (int band, const char* key, float v);

    MirrorProcessor& proc;
    std::function<void (int)> onSelect;
    juce::Rectangle<float> plot;
    doomsday::dsp::Spectrum pre, post;
    std::vector<float> scratch;
    juce::Point<float> hover;
    int hoverBand = -1, dragBand = -1;
};

// the 8 band buttons
class BandChips : public juce::Component
{
public:
    BandChips (MirrorProcessor&, std::function<void (int)> onSelect);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    int selected = 2;
private:
    int chipAt (juce::Point<int>) const;
    MirrorProcessor& proc;
    std::function<void (int)> onSelect;
};

// controls of one band (only the selected band's set is visible)
class BandControls : public juce::Component
{
public:
    BandControls (MirrorProcessor&, int band);
    void resized() override;
    void refresh();
private:
    MirrorProcessor& proc;
    const int band;
    std::unique_ptr<doomsday::ui::Switch> power, dyn;
    std::unique_ptr<doomsday::ui::Choice> type, slope, mode;
    std::unique_ptr<doomsday::ui::Knob> freq, gain, q, thresh;
};

class Canvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1400, H = 860;
    explicit Canvas (MirrorProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void select (int);
    void renderBackground();
    MirrorProcessor& proc;
    doomsday::ui::StageLookAndFeel lnf;
    juce::Image background;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> panels;
    EqDisplay display;
    BandChips chips;
    std::array<std::unique_ptr<BandControls>, ParamIDs::numBands> bands;
    doomsday::ui::PresetBar presets;
    std::unique_ptr<doomsday::ui::Knob> amount, output, monoBass;
    std::unique_ptr<doomsday::ui::Choice> analyzer;
    int shown = -1;
};

class MirrorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MirrorEditor (MirrorProcessor&);
    void resized() override { doomsday::ui::scaleCanvas (*this, canvas, Canvas::W, Canvas::H); }
private:
    Canvas canvas;
};
