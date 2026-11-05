#pragma once
// =====================================================================
//  4GE module 05 - the DISTORTION page (GUI only; the pool / presets / files live in Presets/DistTools.h,
//  the visualizer data is computed in DistScope on a background thread):
//   LOCATIONS  - GLOBAL / PUNCH / BODY / TAIL / SUB / TEXTURE / LAYER 1-8 (the number of stages on each),
//   CHAIN      - the stages of the selected location in the order they run (ADD / DUPLICATE / COPY / PASTE / UP / DOWN / DELETE),
//                PRESETS / SAVE / LOAD (stage, chain, multiband, parallel, full), crossovers, band solo, global position, offline quality,
//   STAGE      - type, morph, path, band, drive, gains, the type's own controls (only those the type uses), pre / post filter, modulation,
//   VISUALIZER - TRANSFER CURVE (+ the CUSTOM curve editor), WAVEFORM before / after, SPECTRUM before / after (freeze, range, channel).
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "../PluginProcessor.h"
#include "DistScope.h"
#include "../Presets/DistTools.h"

namespace fge::gui
{
    // the CUSTOM waveshaper: drag points, double-click = add, right-click / double-click on a point = delete
    class CurveEditor : public juce::Component
    {
    public:
        std::function<void (const dsp::dist::CurveData&, bool newStep)> onChange;
        void setCurve (const dsp::dist::CurveData& c) { if (drag < 0) { curve = c; repaint(); } }
        void setResult (std::vector<float> xs, std::vector<float> ys) { cx = std::move (xs); cy = std::move (ys); repaint(); }
        void setEditable (bool e) { editable = e; repaint(); }
        const dsp::dist::CurveData& data() const { return curve; }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override { drag = -1; }
        void mouseDoubleClick (const juce::MouseEvent&) override;
    private:
        juce::Point<float> toScreen (float x, float y) const;
        juce::Point<float> fromScreen (juce::Point<float>) const;
        int hit (juce::Point<float>) const;
        dsp::dist::CurveData curve;
        std::vector<float> cx, cy;
        bool editable = false; int drag = -1; bool first = false;
    };

    class DistPage : public juce::Component, private juce::Timer
    {
    public:
        DistPage (GeProcessor&, doomsday::flat::FlatLookAndFeel&);
        ~DistPage() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void selectLocation (int);
        void selectStage (int);
        int location() const { return locSel; }
        int stage() const { return stageSel; }
        juce::Component* findControl (const juce::String& id);
        DistScope::Result scopeResult() const { return scope.result(); }
        void requestScope();                                      // tests: analyse now
        void adoptScope() { shown = scope.result(); if (shown.stage == stageSel) curveEd.setResult (shown.curveX, shown.curveY); repaint(); }
        // the visible type-specific knobs (tests)
        juce::StringArray visibleIds() const;

    private:
        void timerCallback() override;
        void buildStage();
        void updateEnabled();
        void showPresetMenu();
        void saveFile();
        void loadFile();
        juce::String did (const juce::String& s) const { return distt::sid (stageSel, s); }
        juce::Component* knob (const juce::String& id, const juce::String& label, juce::Colour, bool bip = false, int d = 28);
        juce::Component* toggle (const juce::String& id, const juce::String& text, juce::Colour);
        juce::Component* combo (const juce::String& id, const juce::String& caption);
        juce::Component* fixedCombo (const juce::String& id, const juce::String& caption);
        void forgetStage();
        juce::String signature() const;

        GeProcessor& proc;
        doomsday::flat::FlatLookAndFeel& lnf;
        int locSel = dsp::dist::LocTail, stageSel = -1;
        std::array<juce::TextButton, dsp::dist::NumLocs> locBtn;
        std::vector<juce::TextButton*> rowBtns;
        std::vector<std::unique_ptr<juce::TextButton>> rows;
        juce::TextButton addBtn { "ADD" }, dupBtn { "DUPLICATE" }, copyBtn { "COPY" }, pasteBtn { "PASTE" }, upBtn { "UP" }, downBtn { "DOWN" }, delBtn { "DELETE" },
                         presetBtn { "PRESETS" }, saveBtn { "SAVE" }, loadBtn { "LOAD" }, freezeBtn { "FREEZE" };
        juce::ComboBox rangeBox, channelBox;
        std::vector<std::unique_ptr<juce::Component>> stageCtl, fixedCtl;
        std::map<juce::Component*, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
        std::multimap<juce::String, juce::Component*> byId;
        std::map<juce::Component*, juce::String> captions;
        std::map<juce::Component*, juce::Rectangle<int>> layoutMap;
        void place (juce::Component* c, int x, int y, int w, int h) { if (c != nullptr) layoutMap[c] = { x, y, w, h }; }
        std::unique_ptr<juce::XmlElement> clip;
        std::unique_ptr<juce::FileChooser> chooser;
        CurveEditor curveEd;
        DistScope scope;
        DistScope::Result shown, frozen;
        bool haveFrozen = false;
        juce::String lastSig;
        ParamFrame lastAsked; uint32_t lastShapes = 0, lastDist = 0xffffffffu; int lastStageAsked = -2, lastChannel = -1;
        juce::Rectangle<int> chainArea, stageArea, transferArea, waveArea, specArea;
    };
}
