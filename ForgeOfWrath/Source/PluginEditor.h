#pragma once
#include "PluginProcessor.h"
#include "DoomsdayUI.h"
#include "DoomsdayFlat.h"

// =====================================================================
//  FORGE OF WRATH - flat UI (the 5CR34M 4LPH4 look, Shared/DoomsdayFlat.h) with a magma accent
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
    explicit AnvilDisplay (ForgeProcessor&);
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
    juce::Point<float> toScreen (const doomsday::kick::Shape::Pt&) const;
    doomsday::kick::Shape::Pt fromScreen (juce::Point<float>) const;
    int pointAt (juce::Point<float>) const;
    ForgeProcessor& proc;
    juce::Rectangle<float> plot;
    std::atomic<bool> dirty { true };
    int lastShapeSerial = -1, lastHit = -1, hover = -1, drag = -1;
    double hitStart = 0.0;
    float playMs = -1.f, previewMs = 410.f;
    std::vector<float> lo, hi, pitch;     // per column
    doomsday::kick::Shape shape[3];
    void convertCurveToDrawn();
};

// REVERSE BASS lane: one beat - the kick (its LENGTH at the FL tempo) and the bass swells; warns when they overlap
class BassLane : public juce::Component
{
public:
    explicit BassLane (ForgeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    ForgeProcessor& proc;
    float lastSig = -1.f;
};

// the whole editor: top bar (presets) | THE ANVIL + MASTER | pages KICK / LAYERS / REVERSE BASS
class Canvas : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    static constexpr int W = 1400, H = 612;
    explicit Canvas (ForgeProcessor&);
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

    ForgeProcessor& proc;
    doomsday::flat::FlatLookAndFeel lnf;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Card> cards;
    AnvilDisplay anvil;
    BassLane bassLane;
    std::unique_ptr<doomsday::flat::TabStrip> tabs;
    int page = 0;
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

class ForgeEditor : public juce::AudioProcessorEditor
{
public:
    explicit ForgeEditor (ForgeProcessor&);
    void resized() override;
    Canvas& getCanvas() { return canvas; }
private:
    Canvas canvas;
};
