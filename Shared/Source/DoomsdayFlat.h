#pragma once
// =====================================================================
//  DOOMSDAY shared UI – "Minimal / flat" look (the owner's chosen direction, see Shared/DESIGN.md)
//  namespace doomsday::flat
//
//    tokens()               the colour tokens (ground / surface / control / text / one hot + one cool accent)
//    FlatLookAndFeel        Inter fonts, thin arc rotaries, flat buttons / combos / popups / alerts / tooltips
//    Knob                   arc + value (editable) + label; any accent colour; optional modulation ring
//    Toggle                 bool parameter as a small switch (Pill) or a text chip (Chip)
//    Segmented              choice parameter as a horizontal segmented control
//    card()                 a flat surface panel with a section title
//    TabStrip               text tabs with an on/off dot per tab (pages are up to the plugin)
//
//  Needs the Inter fonts in the plugin's BinaryData (Shared/Resources/Fonts/Inter-Medium/SemiBold/Bold.ttf):
//  doomsday_plugin(... FLAT) adds them and compiles DoomsdayFlat.cpp, or add both by hand (Prophecy of Scream does).
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace doomsday::flat
{
    struct Tokens
    {
        juce::Colour ground   { 0xff16181c };   // window background
        juce::Colour surface  { 0xff1b1e23 };   // panels / cards
        juce::Colour control  { 0xff1d2025 };   // buttons, fields
        juce::Colour border   { 0xff2c3138 };   // knob track, control borders
        juce::Colour divider  { 0xff262a30 };   // grid lines, separators
        juce::Colour text     { 0xffe8eaed };
        juce::Colour textDim  { 0xff9aa1ab };   // section titles, inactive items
        juce::Colour textMute { 0xff7d848e };   // labels, axes
        juce::Colour hot      { 0xffff5a36 };   // the one main accent
        juce::Colour cool     { 0xff4ea1ff };   // the secondary accent
    };
    const Tokens& tokens();

    enum Weight { Medium, SemiBold, Bold };

    class FlatLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        FlatLookAndFeel();
        juce::Font font (float height, Weight w = SemiBold, float kerning = 0.f) const;

        void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
        juce::Label* createSliderTextBox (juce::Slider&) override;
        juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
        void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        juce::Font getPopupMenuFont() override;
        void drawPopupMenuBackground (juce::Graphics&, int, int) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
        juce::Font getTextButtonFont (juce::TextButton&, int) override;
        juce::Font getAlertWindowTitleFont() override;
        juce::Font getAlertWindowMessageFont() override;
        juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
        void drawTooltip (juce::Graphics&, const juce::String&, int, int) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;

        // a TextButton with getProperties()["primary"] = true is drawn as the filled accent button
        static void makePrimary (juce::Button& b) { b.getProperties().set ("primary", true); }
    private:
        juce::Typeface::Ptr tf[3];
    };

    FlatLookAndFeel& lnfOf (juce::Component&);

    // ------------------------------------------------------------------ Knob
    //  [ arc ]      diameter d (default 44)
    //  value        13 px SemiBold, click to type
    //  label        10.5 px, muted
    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label,
              juce::Colour accent, bool bipolar = false, int diameter = 44);
        void paint (juce::Graphics&) override;
        void resized() override;
        void setAccent (juce::Colour);
        // modulation ring outside the arc: depth = fraction of the travel (-1..1, 0 hides), nowPos01 = live dot (< 0 none)
        void setModulation (float depth, float nowPos01 = -1.f);
        static int heightFor (int diameter) { return diameter + 36; }
        juce::Slider slider;
    private:
        juce::String label;
        int diameter;
        juce::Rectangle<int> labelArea;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // ------------------------------------------------------------------ Toggle
    class Toggle : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        enum Style { Pill, Chip };
        Toggle (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& text, Style, juce::Colour accent);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { repaint(); }
        bool isOn() const { return state; }
        void setAccent (juce::Colour c) { accent = c; repaint(); }
    private:
        juce::String text;
        Style style;
        juce::Colour accent;
        bool state = false;
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };

    // ------------------------------------------------------------------ Segmented (horizontal)
    class Segmented : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        Segmented (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::StringArray labels, juce::Colour accent);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        void setAccent (juce::Colour c) { accent = c; repaint(); }
    private:
        int itemAt (juce::Point<int>) const;
        juce::Rectangle<float> itemBounds (int) const;
        juce::StringArray labels;
        juce::Colour accent;
        int index = 0, hover = -1;
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };

    // ------------------------------------------------------------------ TabStrip
    // text tabs, the selected one underlined in the accent; isOn (i) draws a small dot (effect active) before the name
    class TabStrip : public juce::Component
    {
    public:
        TabStrip (juce::StringArray names, juce::Colour accent);
        std::function<void (int)> onSelect;
        std::function<bool (int)> isOn;            // optional
        void setSelected (int);
        int getSelected() const { return selected; }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        void refresh();                             // call from the editor timer: repaints when an on-state changed
    private:
        juce::StringArray names;
        juce::Colour accent;
        int selected = 0, hover = -1;
        std::vector<bool> lastOn;
    };

    // flat surface card (radius 12) with an optional title (12 px Bold, dim, letter-spaced) at the top left
    void card (juce::Graphics&, FlatLookAndFeel&, juce::Rectangle<int> area, const juce::String& title = {}, juce::Colour titleDot = {});
}
