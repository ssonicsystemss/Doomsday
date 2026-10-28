#pragma once
#include "PluginProcessor.h"
#include "DoomsdayUI.h"
#include "DoomsdayFlat.h"
#include <map>

// =====================================================================
//  4GE - flat UI (the 5CR34M 4LPH4 look, Shared/DoomsdayFlat.h) with a magma accent (Forge of Wrath's editor, regrouped
//  after the kick architecture: PUNCH / TAIL / TEXTURE / BUS + a SOLO / MUTE strip for the layers)
// =====================================================================
namespace ForgeTheme
{
    const juce::Colour magma { 0xffff5a1f }, magmaDeep { 0xff5c1604 }, magmaHot { 0xffffc46b }, ember { 0xffff2d1a }, heat { 0xffffd34d };
    doomsday::ui::Theme make();
}

// THE ANVIL: preview of the kick (waveform + pitch), the last hit's playhead and the drawn AMP / DRIVE envelopes
class AnvilDisplay : public juce::Component,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit AnvilDisplay (GeProcessor&);
    ~AnvilDisplay() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void tick();
    int editing = 0;                 // 0 = AMP, 1 = DRIVE, 2 = PITCH
private:
    void parameterChanged (const juce::String&, float) override { dirty.store (true); }
    void renderPreview();
    juce::Rectangle<float> tabBounds (int) const;
    juce::Point<float> toScreen (const ge::Shape::Pt&) const;
    ge::Shape::Pt fromScreen (juce::Point<float>) const;
    int pointAt (juce::Point<float>) const;
    GeProcessor& proc;
    juce::Rectangle<float> plot;
    std::atomic<bool> dirty { true };
    int lastShapeSerial = -1, lastHit = -1, hover = -1, drag = -1;
    double hitStart = 0.0;
    float playMs = -1.f, previewMs = 410.f;
    std::vector<float> lo, hi, pitch;     // per column
    ge::Shape shape[3];
    void convertCurveToDrawn();
};

// SOLO / MUTE per layer (not saved): click M or S; a soloed layer plays alone (several can be soloed)
class LayerStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LayerStrip (GeProcessor& p) : proc (p) { setTooltip ("M = mute the layer, S = solo (only the soloed layers play). Not saved with the preset"); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    static constexpr int count = 6;
private:
    juce::Rectangle<float> cell (int i) const;
    juce::Rectangle<float> button (int i, bool solo) const;
    GeProcessor& proc;
};

// the whole editor: top bar (presets) | THE ANVIL + MASTER | pages PUNCH / TAIL / TEXTURE / BUS
class Canvas : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    static constexpr int W = 1400, H = 612;
    explicit Canvas (GeProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragHover = false; repaint(); }
    void tickForTest() { timerCallback(); }
    void showPageForTest (int p) { showPage (p); }
    void openPresetBrowserForTest() { refreshList(); }       // the preset list (FACTORY + USER) is rebuilt like a click would
private:
    struct Card
    {
        juce::String title; juce::Colour dot; int page = -1;       // -1 = always visible
        std::vector<juce::Component*> knobs;
        juce::Component* side = nullptr; int sideW = 0;            // a vertical choice on the left of the knobs
        std::vector<juce::Component*> head;                        // toggles / buttons in the title row (right to left)
        juce::Rectangle<int> area;
    };
    void timerCallback() override;
    Card& card (const juce::String& title, juce::Colour dot, int page);
    juce::Component* knob (Card&, const char* id, const char* label, const char* tip, bool bipolar = false);
    void showPage (int);
    void refreshList();
    void step (int);
    void showSave();
    void layoutCard (Card&, int kw);

    GeProcessor& proc;
    doomsday::flat::FlatLookAndFeel lnf;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Card> cards;
    AnvilDisplay anvil;
    LayerStrip layers;
    std::map<juce::Component*, int> segW;
    int cPunch = 0, cHead = 0, cClick = 0, cPFilt = 0, cPDrive = 0, cHand = 0, cTail = 0, cOscA = 0, cOscB = 0, cPre = 0, cS1 = 0, cMid = 0, cS2 = 0,
        cTex = 0, cChar = 0, cImpact = 0, cFilter = 0, cBus = 0, cOtt = 0, cRoom = 0, cSample = 0;
    std::unique_ptr<doomsday::flat::TabStrip> tabs;
    int page = 4, lastAdvanced = 0;          // 4 = MAIN (simple), 0..3 = ADVANCED tabs
    juce::TextButton advB { "Advanced >" };
    int cSPunch = 0, cSTail = 0, cSDist = 0, cSChar = 0, cSLow = 0, cSGlue = 0;
    juce::ComboBox presetBox;
    juce::TextButton prev { "<" }, next { ">" }, randomB { "Random" }, saveB { "Save" }, folderB { "Folder" },
                     undoB { "Undo" }, redoB { "Redo" }, loadBtn { "Load" }, clearBtn { "x" };
    struct PlayButton : public juce::TextButton
    {
        std::function<void (bool)> onPress;
        PlayButton() : juce::TextButton ("Play") {}
        void mouseDown (const juce::MouseEvent& e) override { juce::TextButton::mouseDown (e); if (onPress) onPress (true); }
        void mouseUp (const juce::MouseEvent& e) override   { juce::TextButton::mouseUp (e);   if (onPress) onPress (false); }
    } playB;
    juce::Array<juce::File> userFiles;
    std::unique_ptr<juce::AlertWindow> dialog;
    std::unique_ptr<juce::FileChooser> chooser;
    static constexpr int userBase = 1000;
    juce::String lastName;
    juce::Rectangle<int> sampleNameArea, meterArea;
    float meterLevel = 0.f;
    int lastSampleSerial = -1;
    bool dragHover = false;
    juce::TooltipWindow tips { this, 600 };
};

class GeEditor : public juce::AudioProcessorEditor
{
public:
    explicit GeEditor (GeProcessor&);
    void resized() override;
    Canvas& getCanvas() { return canvas; }
private:
    Canvas canvas;
};
