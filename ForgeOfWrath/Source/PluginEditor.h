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
    int editing = 0;                 // 0 = AMP, 1 = DRIVE, 2 = PITCH
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
    doomsday::kick::Shape shape[3];
    void convertCurveToDrawn();
};

// REVERSE BASS lane: one beat - the kick (its LENGTH at the FL tempo) and the bass swells; warns when they overlap
class BassLane : public juce::Component
{
public:
    explicit BassLane (ForgeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    ForgeProcessor& proc;
    float lastSig = -1.f;
};

class Canvas : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    static constexpr int W = 1500, H = 1180;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void paintOverChildren (juce::Graphics&) override;
    explicit Canvas (ForgeProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tickForTest() { timerCallback(); }      // tests: one UI update without the message loop
private:
    void timerCallback() override;
    void renderBackground();
    ForgeProcessor& proc;
    doomsday::ui::StageLookAndFeel lnf;
    juce::Image background;
    AnvilDisplay anvil;
    doomsday::ui::PresetBar presets;
    struct Section { juce::String title; std::vector<std::unique_ptr<doomsday::ui::Knob>> knobs; juce::Component* side = nullptr; int sideW = 0; int accent = 0; juce::Rectangle<int> area; };
    std::vector<Section> sections;
    Section& section (const juce::String& title, int accent);
    std::unique_ptr<doomsday::ui::Choice> distType, dist2Type, pitchMode, rbPattern;
    std::unique_ptr<doomsday::ui::Switch> punchOn, roomInto, rbOn, punchInto, headOn, rbTail;
    BassLane bassLane;
    juce::TextButton loadBtn { "LOAD" }, clearBtn { "X" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> sampleNameArea;
    int lastSampleSerial = -1;
    bool dragHover = false;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragHover = false; repaint(); }
    doomsday::ui::Meter meter;
    juce::TooltipWindow tips { this, 600 };
};

class ForgeEditor : public juce::AudioProcessorEditor
{
public:
    explicit ForgeEditor (ForgeProcessor&);
    void resized() override { doomsday::ui::scaleCanvas (*this, canvas, Canvas::W, Canvas::H); }
    Canvas& getCanvas() { return canvas; }
private:
    Canvas canvas;
};
