#pragma once
// =====================================================================
//  DOOMSDAY shared UI (needs the shared fonts in the plugin's BinaryData,
//  see Shared/cmake/Doomsday.cmake)
//    Theme / Accent         colours of one plugin
//    StageLookAndFeel       knobs, buttons, combos, popups in the "stage" style (Michroma + Barlow)
//    Knob, Switch, Choice   parameter-bound controls; colour = accent index into the theme
//    Meter                  segmented output meter
//    PresetBar              < preset > / RANDOMIZE / SAVE / PLAY, driven by a PresetHost
//    paint::glassPanel / paint::beam   background helpers
//    FxTabs                 tabbed effect pages with an ACTIVE switch per tab (from Fortune of Seekers)
//    FlowLane               the sound flowing top -> bottom from a dsp::LevelFeed (Fortune's THE PATH lane)
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "DoomsdayDSP.h"

namespace doomsday::ui
{
    struct Accent { juce::Colour main, deep, hot; };

    struct Theme
    {
        juce::Colour bg { 0xff030304 }, text { 0xffeef0ee }, label { 0xffa7aaa9 }, value { 0xffd6dad8 }, dim { 0xff5f6463 };
        juce::Colour panel { 0xff070808 }, knobLight { 0xff34383a }, knobDark { 0xff070808 };
        std::vector<Accent> accents { { juce::Colour (0xffe9c46a), juce::Colour (0xff7a5a17), juce::Colour (0xfffff0c2) } };
        int uiAccent = 0;                 // buttons, combos, popups, text editors
        std::vector<juce::Colour> crown;  // big-knob tick colours along the travel (empty = accent of the knob)
        const Accent& accent (int i) const { return accents[(size_t) juce::jlimit (0, (int) accents.size() - 1, i)]; }
    };

    class StageLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit StageLookAndFeel (Theme);
        const Theme theme;
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

    StageLookAndFeel& lnfOf (juce::Component&);

    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label, int accent = 0,
              bool bipolar = false, bool big = false);
        void paint (juce::Graphics&) override;
        void resized() override;
        juce::Slider slider;
        static constexpr int maxDiameter = 58;
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
        Switch (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& text, Style, int accent = 0);
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

    class Choice : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        Choice (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::StringArray labels, int accent = 0, bool vertical = false);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        int getIndex() const { return index; }
    private:
        int itemAt (juce::Point<int>) const;
        juce::Rectangle<float> itemBounds (int) const;
        juce::StringArray labels;
        int accent, index = 0, hover = -1;
        bool vertical;
        std::unique_ptr<juce::ParameterAttachment> attachment;
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

    // what the preset bar needs from a processor
    struct PresetHost
    {
        virtual ~PresetHost() = default;
        virtual int numFactoryPresets() = 0;
        virtual juce::String factoryPresetName (int) = 0;
        virtual void loadFactoryPreset (int) = 0;
        virtual juce::Array<juce::File> userPresetFiles() = 0;
        virtual bool loadUserPresetFile (const juce::File&) = 0;
        virtual bool saveUserPresetAs (const juce::String&) = 0;
        virtual void randomizeSound() = 0;
        virtual juce::String currentPresetName() = 0;
        virtual juce::String presetFolderText() = 0;
        virtual void audition (bool) {}
    };

    // [<] [ preset combo ] [>]  /  [RANDOMIZE] [SAVE] [PLAY]   (300 x 72)
    class PresetBar : public juce::Component
    {
    public:
        PresetBar (PresetHost&, bool withPlay);
        ~PresetBar() override;
        void resized() override;
        void paint (juce::Graphics&) override;
        void refresh();                     // call from the editor timer
        void refreshList();
    private:
        void step (int);
        void showSave();
        PresetHost& host;
        bool withPlay;
        juce::ComboBox box;
        juce::TextButton prev { "<" }, next { ">" }, save { "SAVE" }, random { "RANDOMIZE" };
        struct Play : public juce::TextButton
        {
            std::function<void (bool)> onPress;
            Play() : juce::TextButton ("PLAY") {}
            void mouseDown (const juce::MouseEvent& e) override { juce::TextButton::mouseDown (e); if (onPress) onPress (true); }
            void mouseUp (const juce::MouseEvent& e) override   { juce::TextButton::mouseUp (e);   if (onPress) onPress (false); }
        } play;
        juce::Array<juce::File> userFiles;
        std::unique_ptr<juce::AlertWindow> dialog;
        juce::String lastName;
        static constexpr int userIdBase = 1000;
    };

    // tabbed FX section: DIST / CHORUS / ... each with its own ACTIVE switch; inactive pages dim
    class FxTabs : public juce::Component
    {
    public:
        FxTabs (juce::AudioProcessorValueTreeState& s, int accentIndex) : apvts (s), accent (accentIndex) {}
        // cells: knobs / switches in one row; side: optional vertical Choice on the left (e.g. distortion type)
        void addTab (const juce::String& name, const juce::String& powerParamId,
                     std::vector<std::unique_ptr<juce::Component>> cells, std::unique_ptr<juce::Component> side = nullptr);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hoverTab = -1; repaint(); }
        void refresh();                 // call from the editor timer
        void selectTab (int);
        int tabWidth = 82;
    private:
        struct Page : public juce::Component
        {
            std::unique_ptr<juce::Component> side;
            std::vector<std::unique_ptr<juce::Component>> cells;
            void resized() override;
        };
        juce::Rectangle<int> tabBounds (int) const;
        juce::AudioProcessorValueTreeState& apvts;
        int accent, current = 0, hoverTab = -1;
        std::vector<std::unique_ptr<Page>> pages;
        std::vector<std::unique_ptr<Switch>> powers;
        std::vector<std::atomic<float>*> onFlags;
        std::vector<bool> lastOn;
        juce::StringArray names;
    };

    // one lane of sound flowing from top (now) to bottom (~1 s ago), drawn from a dsp::LevelFeed
    class FlowLane : public juce::Component
    {
    public:
        FlowLane (const doomsday::dsp::LevelFeed<1024>& f, int accentIndex, juce::String label) : feed (f), accent (accentIndex), text (std::move (label)) {}
        void paint (juce::Graphics&) override;
        void tick();                    // call from the editor timer; repaints only when new audio arrived
    private:
        const doomsday::dsp::LevelFeed<1024>& feed;
        int accent, lastWrite = -1;
        float energy = 0.f;
        juce::String text;
        std::vector<float> lo, hi;
    };

    namespace paint
    {
        void glassPanel (juce::Graphics&, StageLookAndFeel&, juce::Rectangle<int>, const juce::String& title, int accent, bool spot = false);
        void beam (juce::Graphics&, juce::Point<float> src, float angle, float spread, float len, juce::Colour, float alpha);
    }

    // fixed-size canvas scaled by the editor (keeps every plugin pixel-identical at any window size)
    inline void scaleCanvas (juce::Component& editor, juce::Component& canvas, int W, int H)
    {
        canvas.setBounds (0, 0, W, H);
        canvas.setTransform (juce::AffineTransform::scale ((float) editor.getWidth() / (float) W));
    }
}
