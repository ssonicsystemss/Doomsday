#pragma once
// =====================================================================
//  4GE module 17 - the SETTINGS page (page 14, GUI only; the file = Presets/UiSettings.h):
//   INTERFACE SIZE (FIT / 70 - 150 %), pop-up TOOLTIPS, the INFO LINE, the page 4GE opens on,
//   all mouse / keyboard shortcuts in one place, where the files live.
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"
#include "NavBar.h"
#include "../Presets/UiSettings.h"

namespace fge::gui
{
    class SettingsPage : public juce::Component
    {
    public:
        explicit SettingsPage (fge::ui::Settings& s) : st (s)
        {
            juce::StringArray sc; for (float v : fge::ui::Settings::scales()) sc.add (fge::ui::Settings::scaleName (v));
            size = std::make_unique<ChoiceStrip> (sc);
            size->tipFor = [] (int i) { return i == 0 ? juce::String ("FIT: the largest size that fits your screen (never above 100 %)") : "The whole window at " + fge::ui::Settings::scaleName (fge::ui::Settings::scales()[i]); };
            size->onPick = [this] (int i) { st.scale = fge::ui::Settings::scales()[i]; changed(); };
            addAndMakeVisible (*size);
            theme = std::make_unique<ChoiceStrip> (juce::StringArray { "FLAT", "SMOKE" });
            theme->tipFor = [] (int i) { return i == 0 ? juce::String ("FLAT: the original black industrial look") : juce::String ("SMOKE: liquid glass - smoked translucent panels over soft glows"); };
            theme->onPick = [this] (int i) { st.theme = i; setTheme (i); changed(); if (auto* t = getTopLevelComponent()) t->repaint(); };
            addAndMakeVisible (*theme);
            for (auto* b : { &tips, &info }) { b->setClickingTogglesState (true); addAndMakeVisible (b); }
            tips.setTooltip ("The pop-up tooltip windows (after ~0.6 s over a control).");
            info.setTooltip ("The line under the top bar: name, value, range, default, modulation and what the control under the mouse does.");
            tips.onClick = [this] { st.tooltips = tips.getToggleState(); changed(); };
            info.onClick = [this] { st.infoLine = info.getToggleState(); changed(); };
            start.addItem ("THE LAST PAGE USED", 1);
            for (int p : NavBar::order()) start.addItem (NavBar::pageName (p), p + 2);
            start.onChange = [this] { st.startPage = start.getSelectedId() == 1 ? -1 : start.getSelectedId() - 2; changed(); };
            start.setTooltip ("The page 4GE shows when you open it.");
            addAndMakeVisible (start);
            sync();
        }
        std::function<void()> onChanged;          // the canvas applies (size, tooltips) and saves
        void sync()
        {
            int idx = 0; for (int i = 0; i < fge::ui::Settings::scales().size(); ++i) if (std::abs (fge::ui::Settings::scales()[i] - st.scale) < 0.01f) idx = i;
            size->setLit (idx);
            theme->setLit (st.theme);
            tips.setToggleState (st.tooltips, juce::dontSendNotification); tips.setButtonText (st.tooltips ? "TOOLTIPS: ON" : "TOOLTIPS: OFF");
            info.setToggleState (st.infoLine, juce::dontSendNotification); info.setButtonText (st.infoLine ? "INFO LINE: ON" : "INFO LINE: OFF");
            start.setSelectedId (st.startPage < 0 ? 1 : st.startPage + 2, juce::dontSendNotification);
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            const auto& c = palette();
            glassPanel (g, getLocalBounds().toFloat(), 6.f);
            auto title = [&] (int x, int y, const juce::String& t)
            { g.setColour (c.red); g.fillRect (x, y + 3, 3, 12); g.setColour (c.silver.withAlpha (0.8f)); g.setFont (juce::FontOptions (11.5f, juce::Font::bold)); g.drawText (t, x + 9, y, 400, 18, juce::Justification::left); };
            title (20, 16, "INTERFACE");
            g.setColour (c.textDim); g.setFont (juce::FontOptions (11.f));
            g.drawText ("SIZE", 20, 48, 120, 16, juce::Justification::left);
            g.drawText ("OPEN ON", 20, 150, 120, 16, juce::Justification::left);
            g.drawText ("LOOK", 330, 150, 120, 16, juce::Justification::left);
            g.drawFittedText ("The window can also be dragged bigger / smaller at its corner - the size is remembered (for every project).",
                              20, 104, 560, 32, juce::Justification::topLeft, 2);
            title (640, 16, "MOUSE & KEYBOARD");
            static const char* rows[][2] {
                { "Double-click a knob", "back to its default" },
                { "Click the value under a knob", "type a value (\"120 hz\", \"-3 db\", \"1.2 s\")" },
                { "Right-click a knob", "reset, copy / paste the value, type it, modulate, show its modulation" },
                { "Drag  >  next to a macro onto a knob", "the macro modulates that knob" },
                { "Drag the waveform into FL", "the kick as a WAV file" },
                { "Drop a WAV / AIFF / FLAC on 4GE", "into a SAMPLES slot, or as the ANALYZE reference" },
                { "Drop a .4gp preset on 4GE", "load it" },
                { "Click a part card (MAIN)", "edit PUNCH / BODY / TAIL / SUB below" },
                { "Wheel over the preset name", "next / previous preset" },
                { "Click the preset name", "the preset browser" },
                { "Right-click a STYLE", "APPLY / APPLY ALL / PREVIEW / FORGET MY CHANGES" },
                { "Ctrl + Z  /  Ctrl + Y", "undo / redo (every action, also style / length / analysis)" },
                { "Ctrl + left / right", "previous / next page" } };
            int y = 48;
            for (auto& r : rows)
            {
                g.setColour (c.silver); g.setFont (juce::FontOptions (11.5f, juce::Font::bold)); g.drawText (r[0], 640, y, 260, 18, juce::Justification::left, true);
                g.setColour (c.textDim); g.setFont (juce::FontOptions (11.5f)); g.drawText (r[1], 905, y, getWidth() - 925, 18, juce::Justification::left, true);
                y += 24;
            }
            title (20, 260, "FILES");
            g.setColour (c.textDim); g.setFont (juce::FontOptions (11.f));
            const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("4GE");
            g.drawText ("Presets:  " + docs.getChildFile ("Presets").getFullPathName(), 20, 290, 600, 16, juce::Justification::left, true);
            g.drawText ("Interface settings:  " + fge::ui::Settings::defaultFile().getFullPathName(), 20, 310, 600, 16, juce::Justification::left, true);
            title (20, 360, "ABOUT");
            g.setColour (c.silver); g.setFont (juce::FontOptions (12.f, juce::Font::bold));
            g.drawText (juce::String ("4GE  -  HARDSTYLE KICK DESIGNER  v") + version, 20, 390, 600, 18, juce::Justification::left);
            g.setColour (c.textDim); g.setFont (juce::FontOptions (11.f));
            g.drawFittedText ("sonicsystems / Doomsday. 17 modules: core, envelopes, oscillators, noise & layers, distortion, filter & screech, EQ, dynamics, "
                              "modulation, samples, stereo, export, styles, length, presets, analysis, interface. Reverse bass = RE4GE (its own plugin).",
                              20, 412, 580, 48, juce::Justification::topLeft, 3);
        }
        void resized() override
        {
            size->setBounds (20, 68, 560, 28);
            tips.setBounds (20, 200, 170, 28); info.setBounds (200, 200, 170, 28);
            start.setBounds (20, 168, 260, 24);
            theme->setBounds (330, 168, 250, 24);
        }
        juce::String version;
        ChoiceStrip& sizeStrip() { return *size; }
    private:
        void changed() { sync(); if (onChanged) onChanged(); }
        fge::ui::Settings& st;
        std::unique_ptr<ChoiceStrip> size, theme;
        juce::TextButton tips, info;
        juce::ComboBox start;
    };
}
