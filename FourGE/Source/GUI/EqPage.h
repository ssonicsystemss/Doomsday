#pragma once
// =====================================================================
//  4GE module 07 - the EQ page (GUI only; presets / files / tools in Presets/M07Tools.h, displays in EqScope on a thread):
//   PARAMETRIC - 16 bands: the response curve of the selected place with draggable points (drag = freq / gain, wheel = Q,
//                double-click = add a band there, right-click a point = delete), the analyzer PRE / POST (peak hold, average,
//                FFT size, LIN / LOG), the dominant frequency; the selected band's controls (only those its type uses);
//   DYNAMIC    - 4 dynamic bands; MULTIBAND - crossovers + 4 band strips + the band levels; SPECTRAL - tilt, harmonic shaper,
//                exciter, spectral compression, freeze; STEREO - widths, mid / side gains, mono below, correlation.
//   PRESETS / SAVE / LOAD / A-B / RANDOMIZE (+ LOCK per engine), COPY / PASTE / DUPLICATE / DELETE band - one UNDO step each.
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "../PluginProcessor.h"
#include "EqScope.h"
#include "../Presets/M07Tools.h"

namespace fge::gui
{
    class EqPage : public juce::Component, private juce::Timer
    {
    public:
        enum Tab { TParametric = 0, TDynamic, TMultiband, TSpectral, TStereo };
        EqPage (GeProcessor&, doomsday::flat::FlatLookAndFeel&);
        ~EqPage() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override { dragBand = -1; }
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void showTab (int);
        void selectBand (int);
        void selectDyn (int);
        int tab() const { return tabIdx; }
        int band() const { return bandSel; }
        juce::Component* findControl (const juce::String& id);
        juce::StringArray visibleIds() const;
        void requestScope();
        void adoptScope();
        EqScope::Result scopeResult() const { return scope.result(); }
        std::array<bool, m07t::NumEngines> locks() const { std::array<bool, m07t::NumEngines> l {}; for (int i = 0; i < m07t::NumEngines; ++i) l[(size_t) i] = lockBtn[(size_t) i].getToggleState(); return l; }
        // the graph mapping (tests)
        juce::Point<float> toGraph (float hz, float db) const;

    private:
        void timerCallback() override;
        void build();
        void forget();
        void updateEnabled();
        void showPresetMenu();
        void saveFile();
        void loadFile();
        juce::String signature() const;
        juce::Component* knob (const juce::String& id, const juce::String& label, juce::Colour, bool bip = false, int d = 26);
        juce::Component* toggle (const juce::String& id, const juce::String& text, juce::Colour);
        juce::Component* combo (const juce::String& id, const juce::String& caption);
        void place (juce::Component* c, int x, int y, int w, int h) { if (c != nullptr) layoutMap[c] = { x, y, w, h }; }
        float raw (const juce::String& id) const;
        int viewLoc() const;                                      // the place the graph shows (the selected band's / processor's)
        float fromX (float x) const; float fromY (float y) const;
        int hitBand (juce::Point<float>) const;

        GeProcessor& proc;
        doomsday::flat::FlatLookAndFeel& lnf;
        int tabIdx = TParametric, bandSel = 0, dynSel = 0, dragBand = -1;
        std::array<juce::TextButton, 5> tabBtn;
        std::array<juce::TextButton, 16> bandBtn;
        std::array<juce::TextButton, 4> dynBtn;
        std::array<juce::ToggleButton, m07t::NumEngines> lockBtn;
        juce::TextButton presetBtn { "PRESETS" }, saveBtn { "SAVE" }, loadBtn { "LOAD" }, abBtn { "A / B" }, randBtn { "RANDOMIZE" },
                         copyBtn { "COPY" }, pasteBtn { "PASTE" }, dupBtn { "DUPLICATE" }, delBtn { "DELETE" }, holdBtn { "PEAK HOLD" }, resetHoldBtn { "CLEAR" };
        juce::ComboBox fftBox, avgBox, scaleBox, viewBox;
        std::vector<std::unique_ptr<juce::Component>> ctl;
        std::map<juce::Component*, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
        std::multimap<juce::String, juce::Component*> byId;
        std::map<juce::Component*, juce::String> captions;
        std::map<juce::Component*, juce::Rectangle<int>> layoutMap;
        std::vector<std::pair<juce::Rectangle<int>, juce::String>> titles;
        EqScope scope;
        EqScope::Result shown;
        std::vector<float> avgPost, holdPost;
        m07t::Compare compare;
        std::unique_ptr<juce::XmlElement> clip;
        std::unique_ptr<juce::FileChooser> chooser;
        juce::String lastSig;
        ParamFrame lastAsked; uint32_t lastShapes = 0, lastDist = 0xffffffffu; int lastLoc = -2, lastBandAsk = -2, lastFft = -1;
        juce::Rectangle<int> graph, lower;
    };
}
