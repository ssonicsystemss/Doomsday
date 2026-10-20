#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// =====================================================================
//  PROPHECY OF SCREAM v2 - "stage" interface
//  glass panels under festival lighting, Michroma + Barlow typography
// =====================================================================
namespace Palette
{
    const juce::Colour bg       { 0xff050304 };
    const juce::Colour red      { 0xffff2440 };
    const juce::Colour hot      { 0xffff6b7d };
    const juce::Colour deepRed  { 0xff7a0016 };
    const juce::Colour text     { 0xfff3ecec };
    const juce::Colour label    { 0xffb3a3a6 };
    const juce::Colour value    { 0xffd8cdcf };
    const juce::Colour dim      { 0xff6d6164 };
    const juce::Colour glass    { 0x0fffffff };
    const juce::Colour edge     { 0x1affffff };
}

// ------------------------------------------------------------------ look & feel
class StageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StageLookAndFeel();

    juce::Font titleFont (float h, float kerning = 0.f) const;   // Michroma
    juce::Font labelFont (float h, float kerning = 0.f) const;   // Barlow SemiBold
    juce::Font valueFont (float h, float kerning = 0.f) const;   // Barlow Medium
    juce::Font boldFont  (float h, float kerning = 0.f) const;   // Barlow Bold

    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getLabelFont (juce::Label&) override;

private:
    juce::Typeface::Ptr tfTitle, tfLabel, tfValue, tfBold;
};

StageLookAndFeel& stageLnf (juce::Component&);

// ------------------------------------------------------------------ controls
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label,
          bool bipolar = false, bool big = false);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
private:
    juce::String label;
    bool big;
    juce::Rectangle<int> labelArea;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// on/off switch bound to a bool parameter: a pill (power) or a chip with LED + text
class Switch : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum Style { Power, Chip };
    Switch (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& text, Style);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    bool isOn() const { return state; }
private:
    juce::String text;
    Style style;
    bool state = false;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// vertical list of choices bound to a choice parameter
class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::StringArray labels);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
private:
    int itemAt (juce::Point<int>) const;
    juce::StringArray labels;
    int index = 0, hover = -1;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ------------------------------------------------------------------ one oscillator panel
class OscPanel : public juce::Component
{
public:
    OscPanel (ProphecyProcessor&, int osc);
    void resized() override;
    void refresh();
private:
    ProphecyProcessor& proc;
    const int osc;
    Switch power;
    juce::Component content;
    std::vector<std::unique_ptr<Knob>> knobs;
};

// ------------------------------------------------------------------ effect tabs under an oscillator
class FxTabs : public juce::Component
{
public:
    FxTabs (ProphecyProcessor&, int osc);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverTab = -1; repaint(); }
    void refresh();
    void selectTab (int);

private:
    struct Page : public juce::Component
    {
        std::unique_ptr<Segmented> seg;
        std::vector<std::unique_ptr<juce::Component>> cells;
        juce::Component* overlay = nullptr;    // shares the slot of cells[overlaySlot]
        std::unique_ptr<juce::Component> overlayOwner;
        int overlaySlot = -1;
        void resized() override;
    };

    juce::Rectangle<int> tabBounds (int i) const;
    ProphecyProcessor& proc;
    const int osc;
    int current = 0, hoverTab = -1;
    static constexpr int numTabs = 4;
    std::array<Page, numTabs> pages;
    std::array<std::unique_ptr<Switch>, numTabs> powers;
    std::array<std::atomic<float>*, numTabs> onFlags {};
    std::atomic<float>* syncFlag = nullptr;
    juce::Component* rateKnob = nullptr;
    juce::Component* divKnob = nullptr;
    std::array<bool, numTabs> lastOn { false, false, false, false };
};

// ------------------------------------------------------------------ oscilloscope
class Scope : public juce::Component
{
public:
    explicit Scope (ProphecyProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    ProphecyProcessor& proc;
};

// ------------------------------------------------------------------ output meter
class Meter : public juce::Component
{
public:
    void setLevel (float peak);
    void paint (juce::Graphics&) override;
private:
    float level = 0.f, hold = 0.f;
    int holdCount = 0;
};

// ------------------------------------------------------------------ canvas (fixed size, scaled by the editor)
class Canvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1360, H = 800;
    explicit Canvas (ProphecyProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Panel { juce::String title; juce::Rectangle<int> area; bool spot = false; };

    void timerCallback() override;
    void renderBackground();
    void drawStage (juce::Graphics&);
    void drawGlassPanel (juce::Graphics&, const Panel&);
    void refreshPresetList();
    void stepPreset (int delta);
    void showSaveDialog();
    Knob* knob (const juce::String& id, const juce::String& label, bool bipolar = false, bool big = false);
    Switch* toggle (const juce::String& id, const juce::String& label, Switch::Style);

    ProphecyProcessor& proc;
    StageLookAndFeel lnf;
    juce::Image background;
    std::vector<Panel> panels;

    std::unique_ptr<OscPanel> osc1, osc2;
    std::unique_ptr<FxTabs> fx1, fx2;
    std::vector<std::unique_ptr<juce::Component>> owned;
    Knob* screamKnob = nullptr;
    std::vector<juce::Component*> voiceCells, ampCells, delayCells, reverbCells;
    Switch* delayPower = nullptr;
    Switch* reverbPower = nullptr;
    Knob* volumeKnob = nullptr;
    std::vector<juce::Component*> outCells;
    Scope scope;
    Meter meter;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, randomButton { "RANDOMIZE" };
    juce::Array<juce::File> userPresetFiles;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::String lastPresetName;
    static constexpr int userIdBase = 1000;

    float glow = 0.f, lastGlow = -1.f;
    int lastScopePos = -1;
};

// ------------------------------------------------------------------ editor
class ProphecyEditor : public juce::AudioProcessorEditor
{
public:
    explicit ProphecyEditor (ProphecyProcessor&);
    void resized() override;
private:
    Canvas canvas;
};
