#pragma once
#include "PluginProcessor.h"
#include "DoomsdayUI.h"

// =====================================================================
//  FORGE OF WRATH - black + magma
// =====================================================================
namespace ForgeTheme
{
    const juce::Colour magma { 0xffff5a1f }, magmaDeep { 0xff5c1604 }, magmaHot { 0xffffc46b }, ember { 0xffff2d1a }, heat { 0xffffd34d };
    doomsday::ui::Theme make();
}

// THE ANVIL: preview of the kick (waveform + pitch), the last hit's playhead and the drawn AMP / DRIVE envelopes
class AnvilDisplay : public juce::Component,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit AnvilDisplay (ForgeProcessor&);
    ~AnvilDisplay() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void tick();
    int editing = 0;                 // 0 = AMP, 1 = DRIVE
private:
    void parameterChanged (const juce::String&, float) override { dirty.store (true); }
    void renderPreview();
    juce::Rectangle<float> tabBounds (int) const;
    juce::Point<float> toScreen (const doomsday::kick::Shape::Pt&) const;
    doomsday::kick::Shape::Pt fromScreen (juce::Point<float>) const;
    int pointAt (juce::Point<float>) const;
    ForgeProcessor& proc;
    juce::Rectangle<float> plot;
    std::atomic<bool> dirty { true };
    int lastShapeSerial = -1, lastHit = -1, hover = -1, drag = -1;
    double hitStart = 0.0;
    float playMs = -1.f, previewMs = 410.f;
    std::vector<float> lo, hi, pitch;     // per column
    doomsday::kick::Shape shape[2];
};

class Canvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1500, H = 900;
    explicit Canvas (ForgeProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void renderBackground();
    ForgeProcessor& proc;
    doomsday::ui::StageLookAndFeel lnf;
    juce::Image background;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> panels;
    AnvilDisplay anvil;
    doomsday::ui::PresetBar presets;
    std::vector<std::unique_ptr<doomsday::ui::Knob>> tailK, distK, punchK, clickK, masterK;
    std::unique_ptr<doomsday::ui::Choice> distType;
    std::unique_ptr<doomsday::ui::Switch> punchOn;
    doomsday::ui::Meter meter;
    juce::TooltipWindow tips { this, 600 };
};

class ForgeEditor : public juce::AudioProcessorEditor
{
public:
    explicit ForgeEditor (ForgeProcessor&);
    void resized() override { doomsday::ui::scaleCanvas (*this, canvas, Canvas::W, Canvas::H); }
private:
    Canvas canvas;
};
