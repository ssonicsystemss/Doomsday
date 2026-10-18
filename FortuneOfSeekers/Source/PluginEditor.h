#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// =====================================================================
//  FORTUNE OF SEEKERS - black stage, PART A green, PART B purple, gold centre
// =====================================================================
namespace Palette
{
    const juce::Colour bg    { 0xff030304 };
    const juce::Colour text  { 0xffeef0ee };
    const juce::Colour label { 0xffa7aaa9 };
    const juce::Colour value { 0xffd6dad8 };
    const juce::Colour dim   { 0xff5f6463 };

    struct Accent { juce::Colour main, deep, hot; };
    const Accent green  { juce::Colour (0xff2bff8c), juce::Colour (0xff0b6b3c), juce::Colour (0xffa6ffd0) };
    const Accent purple { juce::Colour (0xffb44dff), juce::Colour (0xff4a1680), juce::Colour (0xffdfb6ff) };
    const Accent gold   { juce::Colour (0xffe9c46a), juce::Colour (0xff7a5a17), juce::Colour (0xfffff0c2) };
    inline const Accent& accent (int i) { return i == 0 ? green : i == 1 ? purple : gold; }
}

// ------------------------------------------------------------------ look & feel
class SeekersLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SeekersLookAndFeel();
    juce::Font titleFont (float h, float k = 0.f) const;   // Michroma
    juce::Font labelFont (float h, float k = 0.f) const;   // Barlow SemiBold
    juce::Font valueFont (float h, float k = 0.f) const;   // Barlow Medium
    juce::Font boldFont  (float h, float k = 0.f) const;   // Barlow Bold

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
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }

private:
    juce::Typeface::Ptr tfTitle, tfLabel, tfValue, tfBold;
};

SeekersLookAndFeel& seekLnf (juce::Component&);

// ------------------------------------------------------------------ controls
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label, int accent,
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

class Switch : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum Style { Power, Chip };
    Switch (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& text, Style, int accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    bool isOn() const { return state; }
private:
    juce::String text;
    Style style;
    int accent;
    bool state = false;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// a row (horizontal) or column (vertical) of choices bound to a choice parameter
class Choice : public juce::Component, public juce::SettableTooltipClient
{
public:
    Choice (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::StringArray labels, int accent, bool vertical);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
private:
    int itemAt (juce::Point<int>) const;
    juce::Rectangle<float> itemBounds (int) const;
    juce::StringArray labels;
    int accent, index = 0, hover = -1;
    bool vertical;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ------------------------------------------------------------------ one oscillator
class OscPanel : public juce::Component
{
public:
    OscPanel (FortuneProcessor&, int part, int osc);
    void resized() override;
    void refresh();
private:
    const int part, osc;
    Switch power;
    Choice waves;
    juce::Component content;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::Component* detuneStereo[3] {};
    std::atomic<float>* voicesParam = nullptr;
    std::atomic<float>* waveParam = nullptr;
};

// ------------------------------------------------------------------ filter + amp of one part
class PartPanel : public juce::Component
{
public:
    PartPanel (FortuneProcessor&, int part);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    const int part;
    Switch power;
    Choice ftype;
    juce::Component content;
    std::vector<std::unique_ptr<Knob>> knobs;
    int dividerX = 0;
};

// ------------------------------------------------------------------ FX tabs of one part
class FxTabs : public juce::Component
{
public:
    FxTabs (FortuneProcessor&, int part);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverTab = -1; repaint(); }
    void refresh();
    void selectTab (int);
    static constexpr int numTabs = 5;
private:
    struct Page : public juce::Component
    {
        std::unique_ptr<Choice> seg;
        std::vector<std::unique_ptr<juce::Component>> cells;
        void resized() override;
    };
    juce::Rectangle<int> tabBounds (int) const;
    const int part;
    int current = 0, hoverTab = -1;
    std::array<Page, numTabs> pages;
    std::array<std::unique_ptr<Switch>, numTabs> powers;
    std::array<std::atomic<float>*, numTabs> onFlags {};
    std::array<bool, numTabs> lastOn {};
};

// ------------------------------------------------------------------ the path: both parts flowing top -> bottom
class PathView : public juce::Component
{
public:
    explicit PathView (FortuneProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();                       // called by the canvas timer
private:
    FortuneProcessor& proc;
    std::array<int, 2> lastWrite { -1, -1 };
    std::array<float, 2> energy {};
};

class Meter : public juce::Component
{
public:
    void setLevel (float peak);
    void paint (juce::Graphics&) override;
private:
    float level = 0.f, hold = 0.f;
    int holdCount = 0;
};

// ------------------------------------------------------------------ canvas
class Canvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1480, H = 830;
    explicit Canvas (FortuneProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Panel { juce::String title; juce::Rectangle<int> area; int accent; bool spot = false; };
    void timerCallback() override;
    void renderBackground();
    void drawStage (juce::Graphics&);
    void drawGlassPanel (juce::Graphics&, const Panel&);
    void refreshPresetList();
    void stepPreset (int);
    void showSaveDialog();
    Knob* knob (const juce::String& id, const juce::String& label, int accent, bool bipolar = false, bool big = false);
    Switch* toggle (const juce::String& id, const juce::String& label, Switch::Style, int accent);

    FortuneProcessor& proc;
    SeekersLookAndFeel lnf;
    juce::Image background;
    std::vector<Panel> panels;

    std::array<std::array<std::unique_ptr<OscPanel>, 2>, 2> oscPanels;
    std::array<std::unique_ptr<PartPanel>, 2> partPanels;
    std::array<std::unique_ptr<FxTabs>, 2> fxTabs;
    std::vector<std::unique_ptr<juce::Component>> owned;
    Knob* fortuneKnob = nullptr;
    std::vector<juce::Component*> globalCells;
    Switch* monoChip = nullptr;
    PathView pathView;
    Meter meter;

    struct PlayButton : public juce::TextButton
    {
        std::function<void (bool)> onPress;
        PlayButton() : juce::TextButton ("PLAY") {}
        void mouseDown (const juce::MouseEvent& e) override { juce::TextButton::mouseDown (e); if (onPress) onPress (true); }
        void mouseUp (const juce::MouseEvent& e) override   { juce::TextButton::mouseUp (e);   if (onPress) onPress (false); }
    } playButton;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, randomButton { "RANDOMIZE" };
    juce::Array<juce::File> userPresetFiles;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::String lastPresetName;
    static constexpr int userIdBase = 1000;
    float glow = 0.f, lastGlow = -1.f;
};

class FortuneEditor : public juce::AudioProcessorEditor
{
public:
    explicit FortuneEditor (FortuneProcessor&);
    void resized() override;
private:
    Canvas canvas;
};
