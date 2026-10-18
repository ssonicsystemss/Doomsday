#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// =====================================================================
//  ANCIENT DRUMS v2 - white & gold interface with a blue kick screen
// =====================================================================
namespace Palette
{
    const juce::Colour ivory     { 0xfff4efe6 };
    const juce::Colour ivoryDeep { 0xffe6ddcd };
    const juce::Colour gold      { 0xffc8962e };
    const juce::Colour goldLight { 0xfff2d27c };
    const juce::Colour goldDeep  { 0xff94681a };
    const juce::Colour ink       { 0xff2c2721 };
    const juce::Colour label     { 0xff6c6253 };
    const juce::Colour dim       { 0xffa69c8d };
    // screen
    const juce::Colour lcd       { 0xff031228 };
    const juce::Colour lcdDeep   { 0xff010916 };
    const juce::Colour cyan      { 0xff47dcff };
    const juce::Colour ice       { 0xffbff6ff };
    const juce::Colour blue      { 0xff2d8cff };
    const juce::Colour grid      { 0xff0c2c4c };

    inline juce::Colour layerColour (int layer)
    {
        return layer == Punch ? juce::Colour (0xffa8f4ff) : layer == Crunch ? juce::Colour (0xff3fd2ff) : juce::Colour (0xff3a90ff);
    }
}

// ------------------------------------------------------------------ look & feel
class GoldLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GoldLookAndFeel();
    juce::Font titleFont (float h, float k = 0.f) const;   // Cinzel
    juce::Font headFont  (float h, float k = 0.f) const;   // Michroma
    juce::Font labelFont (float h, float k = 0.f) const;   // Barlow SemiBold
    juce::Font valueFont (float h, float k = 0.f) const;   // Barlow Medium
    juce::Font monoFont  (float h, float k = 0.f) const;   // JetBrains Mono

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
    juce::Typeface::Ptr tfTitle, tfHead, tfLabel, tfValue, tfMono;
};

GoldLookAndFeel& goldLnf (juce::Component&);

// ------------------------------------------------------------------ controls
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label, bool bipolar = false);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
private:
    juce::String label;
    juce::Rectangle<int> labelArea;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

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

class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::StringArray labels);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    juce::StringArray labels;
    int index = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ------------------------------------------------------------------ selected-layer controls (left panel)
class LayerControls : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    LayerControls (AncientProcessor&, int layer);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
private:
    AncientProcessor& proc;
    const int layer;
    Switch onSwitch, revChip, forgeChip;
    std::unique_ptr<Switch> keyChip;
    juce::ComboBox source;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment;
    juce::TextButton loadButton { "LOAD" }, resetEnvButton { "RESET ENVELOPE" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::String info;
};

// ------------------------------------------------------------------ the blue screen
class KickScreen : public juce::Component, public juce::FileDragAndDropTarget, public juce::SettableTooltipClient
{
public:
    explicit KickScreen (AncientProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();                        // called by the canvas timer

    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverNode = -1; repaint(); }

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragHover = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    struct LayerView
    {
        SamplePtr sample;
        EnvPtr env;
        float offsetMs = 0.f, rate = 1.f;
        bool on = true, reverse = false;
    };
    LayerView viewOf (int layer) const;
    float msToX (float ms) const;
    float xToMs (float x) const;
    float gainToY (float g) const;
    float yToGain (float y) const;
    int nodeAt (juce::Point<float>) const;
    void drawLayer (juce::Graphics&, int layer, bool selected);
    void drawEnvelope (juce::Graphics&, int layer);
    juce::Rectangle<int> chipBounds (int i) const;
    juce::Rectangle<int> zoomBounds (int i) const;
    juce::RangedAudioParameter* offsetParam (int layer) const;

    AncientProcessor& proc;
    juce::Rectangle<float> plot;
    int hoverNode = -1, dragNode = -1;
    bool draggingOffset = false, dragHover = false;
    float dragStartMs = 0.f, dragStartOffset = 0.f;
    Envelope editing;              // envelope being dragged (committed on every move)
    int lastSerial = -1, lastLen = -1;
    juce::String lastSignature;
    static constexpr float zooms[3] = { 500.f, 1000.f, 2000.f };
};

// ------------------------------------------------------------------ canvas
class Canvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1360, H = 780;
    explicit Canvas (AncientProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Panel { juce::String title; juce::Rectangle<int> area; };
    void timerCallback() override;
    void renderBackground();
    void drawPanel (juce::Graphics&, const Panel&);
    void refreshPresetList();
    void stepPreset (int);
    void showSaveDialog();
    Knob* knob (const juce::String& id, const juce::String& label, bool bipolar = false);
    Switch* toggle (const juce::String& id, const juce::String& label, Switch::Style);

    AncientProcessor& proc;
    GoldLookAndFeel lnf;
    juce::Image background;
    std::vector<Panel> panels;

    std::array<std::unique_ptr<LayerControls>, NumSampleLayers> layerControls;
    KickScreen screen;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<juce::Component*> forgeCells, subCells, rumCells, masterCells;
    Switch* forgePower = nullptr;
    Switch* subPower = nullptr;
    Switch* rumPower = nullptr;
    int shownLayer = -1;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, randomButton { "RANDOMIZE" }, playButton { "PLAY" };
    juce::Array<juce::File> userPresetFiles;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::String lastPresetName;
    static constexpr int userIdBase = 1000;
};

class AncientEditor : public juce::AudioProcessorEditor
{
public:
    explicit AncientEditor (AncientProcessor&);
    void resized() override;
private:
    Canvas canvas;
};
