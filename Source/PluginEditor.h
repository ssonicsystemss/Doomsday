#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "../Shared/Source/DoomsdayFlat.h"

// =====================================================================
//  PROPHECY OF SCREAM v3.1 - "minimal / flat" interface (Shared/DESIGN.md)
//  top bar / big response graph (drag the CUTOFF and FORMANT nodes) + SCREAM /
//  OSC 1 (hot accent) and OSC 2 (cool accent) with tabs / VOICE AMP DELAY REVERB OUTPUT
// =====================================================================
using FlatLnf = doomsday::flat::FlatLookAndFeel;
using FlatKnob = doomsday::flat::Knob;
using FlatToggle = doomsday::flat::Toggle;

juce::Colour oscAccent (int osc);

// ------------------------------------------------------------------ the big graph
// filter + formant response of both oscillators over the live output spectrum;
// the selected oscillator has two draggable nodes: CUTOFF (x = cutoff, y = resonance) and FORMANT (x = freq, y = gain, wheel = Q)
class ResponseGraph : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ResponseGraph (ProphecyProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void tick();                                   // editor timer: spectrum + repaint when something changed
    void selectOsc (int);
    int selectedOsc() const { return osc; }
    std::function<void (int osc, int tab)> onNodeTouched;   // tab: 2 = FILTER, 4 = FORMANT

private:
    enum Node { None = -1, Cut = 0, Form = 1 };
    struct OscView { bool on, filtOn, formOn; int ftype; float cutoff, reso, formHz, formGain, formQ; };
    OscView view (int o) const;
    float responseDb (const OscView&, float hz) const;
    float xOfHz (float hz) const;
    float hzOfX (float x) const;
    float yOfDb (float db) const;
    juce::Point<float> nodePos (int o, Node) const;
    Node nodeAt (juce::Point<float>) const;
    juce::Rectangle<float> chipBounds (int o) const;
    juce::RangedAudioParameter* param (const char* base) const;
    void setParam (const char* base, float value);

    ProphecyProcessor& proc;
    int osc = 0;
    Node hover = None, drag = None;
    juce::Point<float> lastDrag;
    juce::Rectangle<float> plot;

    // spectrum of the output (from the processor's scope buffer)
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, numBars = 110;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize * 2> fftData {};
    std::array<float, numBars> bars {};
    int lastWrite = -1;
    juce::String lastKey;
};

// ------------------------------------------------------------------ small output scope + meter
class Scope : public juce::Component
{
public:
    explicit Scope (ProphecyProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    ProphecyProcessor& proc;
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

// ------------------------------------------------------------------ one oscillator card: OSC / DIST / FILTER / LFO / FORMANT tabs
class OscCard : public juce::Component
{
public:
    OscCard (ProphecyProcessor&, int osc);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();                                // editor timer
    void selectTab (int);
    std::function<void (int tab)> onTab;
    static constexpr int numTabs = 5;              // OSC, DIST, FILTER, LFO, FORMANT
private:
    struct Page : public juce::Component
    {
        std::unique_ptr<juce::Component> seg, chip;          // top row (segmented control, optional chip)
        std::vector<std::unique_ptr<juce::Component>> knobs; // knob grid
        std::unique_ptr<juce::Component> overlay;            // shares the cell of knobs[0] (LFO: free RATE vs synced RATE)
        int columns = 6;
        void resized() override;
    };
    ProphecyProcessor& proc;
    const int osc;
    doomsday::flat::TabStrip tabs;
    std::array<Page, numTabs> pages;
    std::array<std::unique_ptr<FlatToggle>, numTabs> powers;
    std::array<std::atomic<float>*, numTabs> onFlags {};
    std::atomic<float>* syncFlag = nullptr;
    FlatKnob* rateKnob = nullptr;
    FlatKnob* divKnob = nullptr;
    int current = 0;
};

// ------------------------------------------------------------------ canvas (fixed size, scaled by the editor)
class Canvas : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1360, H = 800;
    explicit Canvas (ProphecyProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Section { juce::String title; juce::Rectangle<int> area; std::vector<juce::Component*> cells; FlatToggle* power = nullptr; };

    void timerCallback() override;
    void renderBackground();
    void refreshPresetList();
    void stepPreset (int delta);
    void showSaveDialog();
    FlatKnob* knob (const juce::String& id, const juce::String& label, juce::Colour, bool bipolar = false, int d = 44);
    FlatToggle* toggle (const juce::String& id, const juce::String& label, FlatToggle::Style, juce::Colour);

    ProphecyProcessor& proc;
    FlatLnf lnf;
    juce::Image background;
    std::vector<std::unique_ptr<juce::Component>> owned;

    ResponseGraph graph;
    std::unique_ptr<OscCard> osc1, osc2;
    FlatKnob* screamKnob = nullptr;
    Scope scope;
    Meter meter;
    std::vector<Section> sections;
    juce::Rectangle<int> graphArea, screamArea;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" }, randomButton { "Randomize" };
    juce::Array<juce::File> userPresetFiles;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::String lastPresetName;
    static constexpr int userIdBase = 1000;
    int lastScopePos = -1;
    juce::TooltipWindow tooltips { this, 600 };
};

// ------------------------------------------------------------------ editor
class ProphecyEditor : public juce::AudioProcessorEditor
{
public:
    explicit ProphecyEditor (ProphecyProcessor&);
    void resized() override;
    Canvas canvas;
};
