#pragma once
// =====================================================================
//  4GE editor - module 17 GUI & UX (the complete interface, built from the real parameters of modules 01-16):
//   TOP BAR: logo, STYLE (EARLY / MODERN / RAW / EXP), LENGTH (SHORT / MEDIUM / LONG / XL + the ms panel), the INFO LINE
//   (what is under the mouse: name, value, range, default, modulation, what it does), presets + RANDOMIZE / SAVE / PLAY / UNDO / REDO.
//   LEFT: the NAVIGATION - 5 blocks (KICK / SYNTHESIS / PROCESSING / MODULATION / SYSTEM), every page, the 17 module numbers, activity dots.
//   CENTRE: the page. MAIN = PITCH ENGINE, the central REAL waveform (drag = WAV), four part cards, the selected part, the CORE MIXER.
//   BOTTOM (always): the 8 MASTER MACROS (drag > onto any knob) + GLOBAL (LENGTH / MASTER / VELOCITY, VOICE, KICK END, PHASE RESET,
//   AUTO LENGTH, OUT meter). Drop a WAV / .4gp anywhere, right-click any knob, Ctrl+Z / Y, Ctrl+left / right, GUI scale (SETTINGS).
//  GUI only: reads parameters through the APVTS, renders on its own threads, the settings file = Presets/UiSettings.h.
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "DoomsdayFlat.h"
#include "DoomsdayUI.h"
#include "../PluginProcessor.h"
#include "WaveView.h"
#include "EnvPage.h"
#include "OscPage.h"
#include "LayerPage.h"
#include "DistPage.h"
#include "ScreechPage.h"
#include "EqPage.h"
#include "DynPage.h"
#include "ModPage.h"
#include "SamplePage.h"
#include "StereoPage.h"
#include "ExportPage.h"
#include "PresetPage.h"
#include "AnalysisPage.h"
#include "NavBar.h"
#include "SettingsPage.h"
#include "Palette.h"

class GeCanvas : public juce::Component, public juce::DragAndDropContainer, public juce::DragAndDropTarget, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    static constexpr int W = 1456, H = 942;
    static constexpr int navW = 176, X0 = 192, Y0 = 104, CW = 1248, CH = 718;       // the navigation, the page area
    explicit GeCanvas (GeProcessor&);
    ~GeCanvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void selectPart (int);
    void tailTab (int t) { detailPage = juce::jlimit (0, 3, t); layoutDetail(); repaint(); }
    int selectedPart() const { return selected; }
    fge::gui::WaveView& waveView() { return wave; }
    juce::Component* findControl (const juce::String& paramId);     // tests
    void showPage (int);                                             // 0 MAIN 1 ENV 2 OSC 3 LAYERS 4 DIST 5 SCREECH 6 EQ 7 DYN 8 MOD 9 SMP 10 STEREO 11 EXPORT 12 PRESETS 13 ANALYZE 14 SETTINGS
    fge::gui::OscPage& oscillators() { return *oscPage; }
    fge::gui::LayerPage& layersPage() { return *layerPage; }
    fge::gui::DistPage& distPage() { return *distortionPage; }
    fge::gui::ScreechPage& screechPage() { return *scrPage; }
    fge::gui::EqPage& eqPage() { return *eqPg; }
    fge::gui::DynPage& dynPage() { return *dynPg; }
    fge::gui::ModPage& modPage() { return *modPg; }
    fge::gui::SamplePage& samplePage() { return *smpPg; }
    fge::gui::StereoPage& stereoPage() { return *stPg; }
    fge::gui::ExportPage& exportPage() { return *exPg; }
    fge::gui::PresetPage& presetPage() { return *prPg; }
    fge::gui::AnalysisPage& analysisPage() { return *anPg; }
    fge::gui::NavBar& navigation() { return nav; }
    fge::gui::SettingsPage& settingsPage() { return *setPg; }
    // module 17: the INFO LINE (what is under the mouse / the page), files dropped anywhere, the knob menu, shortcuts, the size
    juce::String infoFor (juce::Component*);                        // (tests) the info text of a control
    juce::String infoLine() const { return infoText; }
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void dropFile (const juce::File&, int choice);                   // 1 = first free SAMPLES slot, 2 = ANALYZE reference, 3 = preset (tests)
    bool keyPressed (const juce::KeyPress&) override;
    void stepPage (int dir);
    fge::ui::Settings& uiSettings() { return uiSet; }
    const fge::ui::Settings& uiSettings() const { return uiSet; }
    std::function<void()> onSettingsChanged;                         // (the editor: size / tooltips)
    bool pageActive (int page);                                      // the navigation dots
    doomsday::ui::PresetBar& presets() { return presetBar; }
    int page() const { return pageIdx; }
    fge::gui::EnvPage& envelopes() { return *envPage; }
    // module 09: modulation from any knob (right-click menu, drag a source onto it), rings BASE / FINAL on modulated knobs
    int addModulation (const juce::String& paramId, int source);    // -> the slot used (-1 = none free / not modulatable)
    void knobMenu (doomsday::flat::Knob&);
    void updateModRings();
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("modsrc:"); }
    void itemDropped (const SourceDetails&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    int dropOnto (juce::Point<int> canvasPos, int source);          // (tests: the same code as a drop)
    // module 13: STYLE in the header (always visible) - choosing one applies its template (KEEP MY CHANGES), right-click = options
    void chooseStyle (int style, bool keepMine = true);
    void previewStyle (int style);
    juce::String styleStatus() const { return styleMsg; }
    juce::Component& styleSelector() { return styleStrip; }
    // module 14: LENGTH in the header (always visible) - a panel with the profiles, SYNC, AUTO LENGTH and how each part follows
    juce::TextButton& lengthButton() { return lengthBtn; }
    fge::gui::ChoiceDrop& lengthProfiles() { return lengthStrip; }
    juce::Component* openLengthPanel();                              // (tests: the panel's content)
private:
    void timerCallback() override;
    using Knob = doomsday::flat::Knob;
    using Toggle = doomsday::flat::Toggle;
    using Seg = doomsday::flat::Segmented;
    Knob* knob (const char* id, const juce::String& label, juce::Colour accent, int d = 40, bool bipolar = false);
    Toggle* toggle (const char* id, const juce::String& text, Toggle::Style, juce::Colour accent);
    Seg* seg (const char* id, juce::StringArray labels, juce::Colour accent);
    juce::ComboBox* combo (const char* id);
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    std::vector<juce::Component*> masterRow, macroRow;
    juce::Rectangle<int> macroArea;
    void layoutDetail();
    void paintMain (juce::Graphics&, const std::function<void (juce::Rectangle<int>, const juce::String&, juce::Colour)>& panel);
public:
    void updateEnabled();
private:

    GeProcessor& proc;
    doomsday::flat::FlatLookAndFeel lnf;
    std::unique_ptr<juce::TooltipWindow> tips;
    doomsday::ui::StageLookAndFeel stageLnf { makeTheme() };      // the shared preset bar draws with the stage look
    static doomsday::ui::Theme makeTheme();
    doomsday::ui::PresetBar presetBar;
    fge::gui::WaveView wave;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::multimap<juce::String, juce::Component*> byId;

    // header / pitch engine
    std::vector<juce::Component*> header, pitchRow;
    // the four part cards and their details
    std::array<std::vector<juce::Component*>, 4> card, detail, mixer;
    std::array<juce::Rectangle<int>, 4> cardArea;
    juce::Rectangle<int> headerArea, pitchArea, waveArea, detailArea, mixerArea, meterArea;
    std::array<juce::Rectangle<int>, 4> mixMeterArea;
    int selected = 2;
    int pageIdx = 0;
    std::unique_ptr<fge::gui::EnvPage> envPage;
    std::unique_ptr<fge::gui::OscPage> oscPage;
    std::unique_ptr<fge::gui::LayerPage> layerPage;
    std::unique_ptr<fge::gui::DistPage> distortionPage;
    std::unique_ptr<fge::gui::ScreechPage> scrPage;
    std::unique_ptr<fge::gui::EqPage> eqPg;
    std::unique_ptr<fge::gui::DynPage> dynPg;
    std::unique_ptr<fge::gui::ModPage> modPg;
    std::unique_ptr<fge::gui::SamplePage> smpPg;
    std::unique_ptr<fge::gui::StereoPage> stPg;
    std::unique_ptr<fge::gui::ExportPage> exPg;
    std::unique_ptr<fge::gui::PresetPage> prPg;
    std::unique_ptr<fge::gui::AnalysisPage> anPg;                     // module 16 (page 13)
    std::unique_ptr<fge::gui::SettingsPage> setPg;                    // module 17 (page 14)
    fge::ui::Settings uiSet;
    fge::gui::NavBar nav;
    // STYLE / LENGTH = drop-down lists (owner 2026-11-12); STYLE is listed in time order, the ids stay (EUPHORIC = 4, appended)
    fge::gui::ChoiceDrop styleStrip { { "EARLY  2004-08", "EUPHORIC  2009-12", "MODERN", "RAW", "EXPERIMENTAL" }, { 0, 4, 1, 2, 3 } },
                         lengthStrip { { "SHORT", "MEDIUM", "LONG", "EXTRA LONG" } };
    juce::String infoText;
    juce::Rectangle<int> infoArea, globalArea;
    std::vector<std::pair<int, int>> activeParams;                    // (page, parameter) the "on" switches behind the navigation dots
    void knobValueMenu (doomsday::flat::Knob&, juce::PopupMenu&);
    void applyTooltips();
    juce::ComboBox tokNote;                                          // TOK NOTE: sets START to a note (owner 2026-11-09)
    juce::Component detailPageMark;                                 // (a marker in a part's detail list: the items after it = the 2nd tab)
    juce::TextButton detTab[4];                                      // TAIL detail tabs: TONE / ROOM + COLOR (owner 2026-11-12)
    int detailPage = 0;
    void syncTokNote();
public:
    juce::ComboBox& tokNoteBox() { return tokNote; }
private:
    void typeValue (doomsday::flat::Knob&);
    int pageBeforePresets = 0;
    juce::TextButton lengthBtn;
    juce::Component::SafePointer<juce::CallOutBox> lengthBox;
    juce::String styleMsg;
    juce::uint32 styleMsgTime = 0;
    int styleShown = -2;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    void styleMenu();
    std::array<float, 4> meter {};
    float outMeter = 0.f;
};

class GeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GeEditor (GeProcessor&);
    void resized() override;
    void applyScale();                      // module 17: the size from the SETTINGS
    float wantedScale() const;
    GeCanvas canvas;
private:
    void timerCallback() override;
    bool ownResize = false;
    int saveIn = 0;
};
