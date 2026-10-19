#pragma once
#include "PluginProcessor.h"
#include "DoomsdayUI.h"

// =====================================================================
//  DARK SERPENT - black + toxic lime
// =====================================================================
namespace SerpentTheme
{
    const juce::Colour lime { 0xffb6ff3a }, limeDeep { 0xff3f6a0a }, limeHot { 0xffe8ffb0 };
    doomsday::ui::Theme make();
}

// the title: a glossy black scaled serpent - tail curled into an infinity sign, body framing "DARK SERPENT",
// open jaws with fangs above the title (lime throat, dripping venom, flicking tongue), in flowing ribbons of smoke
class SmokeLogo : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (float level);            // ~30 fps from the canvas
private:
    void build();
    struct Ribbon
    {
        float baseY, a1, f1, w1, p1, a2, f2, w2, p2, drift, thick, tf, tw, tp, x0, x1, alpha;
        juce::Colour colour;
        int layer;                      // 0 behind the serpent, 1 between its back and front coils, 2 in front
    };
    void drawSmoke (juce::Graphics&, int layer);
    std::vector<Ribbon> ribbons;
    juce::Image bodyBack, bodyFront, title;
    juce::Path mouth, fangNear, fangFar, fangLow;
    juce::Point<float> eye, eyeDir, eyeUp, hinge, throat, tongueDir, fangTip, upperTip, lowerTip;
    float eyeR = 4.f, time = 0.f, glow = 0.f;
};

// vowel pad: X = vowel A..U, Y = talk amount
class TalkPad : public juce::Component
{
public:
    explicit TalkPad (SerpentProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
private:
    juce::Rectangle<float> padArea() const;
    void setFrom (juce::Point<float>);
    SerpentProcessor& proc;
    juce::RangedAudioParameter* vowelP;
    juce::RangedAudioParameter* talkP;
    float lastV = -1.f, lastT = -1.f, lastLiveV = -1.f;
};

class Canvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1500, H = 900;
    explicit Canvas (SerpentProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    struct Panel { juce::String title; juce::Rectangle<int> area; bool spot = false; };
    void timerCallback() override;
    void renderBackground();
    using Knob = doomsday::ui::Knob;
    using Switch = doomsday::ui::Switch;
    using Choice = doomsday::ui::Choice;
    Knob* knob (const char* id, const juce::String& label, bool bipolar = false, bool big = false);
    Switch* toggle (const char* id, const juce::String& label, Switch::Style);
    Choice* choice (const char* id, const juce::StringArray&, bool vertical = false);
    void row (juce::Rectangle<int> area, const std::vector<juce::Component*>&);

    SerpentProcessor& proc;
    doomsday::ui::StageLookAndFeel lnf;
    juce::Image background;
    std::vector<Panel> panels;
    std::vector<std::unique_ptr<juce::Component>> owned;

    SmokeLogo logo;
    TalkPad pad;
    doomsday::ui::PresetBar presets;
    doomsday::ui::Meter meter;

    std::vector<juce::Component*> core, edge, body, filter, move, play, rage, amp, dly, rev, pump, space;
    Choice *coreWave = nullptr, *edgeOct = nullptr, *bodyWave = nullptr, *moveShape = nullptr, *mode = nullptr, *rageType = nullptr;
    Switch *edgeOn = nullptr, *bodyOn = nullptr, *moveOn = nullptr, *moveRetrig = nullptr, *bendFirst = nullptr, *rageOn = nullptr,
           *dlyOn = nullptr, *revOn = nullptr, *pumpOn = nullptr;
    Knob* energyKnob = nullptr;
};

class SerpentEditor : public juce::AudioProcessorEditor
{
public:
    explicit SerpentEditor (SerpentProcessor&);
    void resized() override { doomsday::ui::scaleCanvas (*this, canvas, Canvas::W, Canvas::H); }
private:
    Canvas canvas;
};
