#pragma once
// =====================================================================
//  4GE module 03 - the oscillator scope: the REAL waveform (two cycles) and the REAL spectrum (harmonics 1..64)
//  of the selected oscillator or of the sum, before / after FM / ring / sync. Rendered with dsp::renderOscPreview on its own
//  background thread (GUI never runs DSP on the message thread), from the same tables the engine plays.
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Parameters/ParamFrame.h"
#include "../DSP/Osc/Wavetable.h"
#include <array>
#include <mutex>

namespace fge::gui
{
    class OscScope : public juce::Component, private juce::Thread, private juce::AsyncUpdater
    {
    public:
        using Tables = std::array<dsp::TablePtr, 8>;
        OscScope();
        ~OscScope() override;
        void update (const ParamFrame&, const Tables&, int selected, bool sum, bool post);
        void paint (juce::Graphics&) override;
        // tests: the last spectrum (dB per harmonic, index 0 = fundamental)
        std::array<float, 64> lastSpectrum() const { std::lock_guard<std::mutex> l (lock); return shown.spec; }
        bool hasRender() const { return ready.load(); }
        static constexpr int fftOrder = 13, fftN = 1 << fftOrder, binOf0 = 16;      // fundamental exactly on bin 16
        static constexpr double rate = 44100.0;
        static float previewHz() { return (float) (rate * binOf0 / fftN); }       // ~86 Hz
    private:
        void run() override;
        void handleAsyncUpdate() override { repaint(); }
        struct Result { std::array<std::vector<float>, 5> wave; std::array<float, 64> spec {}; int sel = 0; bool sum = false, post = true; };
        mutable std::mutex lock;
        ParamFrame pending; Tables pendingTables; int pSel = 0; bool pSum = false, pPost = true, wanted = false;
        ParamFrame lastF; Tables lastT; int lastSel = -1; bool lastSum = false, lastPost = true;
        Result shown;
        std::atomic<bool> ready { false };
        juce::WaitableEvent wake;
    };
}
