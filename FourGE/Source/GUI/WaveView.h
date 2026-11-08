#pragma once
// =====================================================================
//  4GE central waveform: the REAL kick, rendered offline on a background thread from the current parameters
//  (full kick + each part solo), drawn with the part colour code (Design/PALETTE.md). Drag it into FL = a WAV file.
//  GUI only: it asks fge::renderKick (DSP) on its own thread, never touches the audio thread.
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Parameters/ParamFrame.h"
#include "../DSP/Env/MultiEnv.h"
#include "../DSP/Osc/Wavetable.h"
#include "../DSP/Dist/Distortion.h"
#include "../DSP/Mod/Mod.h"
#include "../DSP/Sample/Sample.h"
#include <array>
#include <atomic>
#include <mutex>
#include <vector>

namespace fge::gui
{
    class WaveView : public juce::Component, private juce::Thread, private juce::AsyncUpdater
    {
    public:
        WaveView();
        ~WaveView() override;
        // call from the editor timer: re-renders (in the background) when the parameters changed
        using Tables = std::array<dsp::TablePtr, 8>;
        void update (const ParamFrame&, int note, const env::Bank* shapes = nullptr, uint32_t shapesVersion = 0,
                     const Tables* tables = nullptr, const dsp::dist::DistData* dist = nullptr, uint32_t distVersion = 0,
                     const dsp::mod::Data* mod = nullptr, uint32_t modVersion = 0);
        // module 10: the sample slots (kept alive by these copies while the background render reads them)
        using Samples = std::array<std::shared_ptr<const dsp::SampleData>, dsp::numSampleSlots>;
        void setSamples (const Samples& s, uint32_t version);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override { draggingHandle = false; }
        void mouseMove (const juce::MouseEvent&) override;
        // module 06: the SCREECH START handle (drawn where the screech really starts; MANUAL = it can be dragged)
        std::function<void (float ms, bool first)> onScreechStart;
        // the last rendered kick (tests): peak per part, length in ms
        std::array<float, 5> lastPeaks() const;
        bool hasRender() const { return ready.load(); }
        juce::File writeWav (const juce::File& folder);      // the full kick as 24-bit WAV (also the drag-and-drop file)
        std::function<void()> onRendered;
        static constexpr double rate = 44100.0;

    private:
        void run() override;
        void handleAsyncUpdate() override { repaint(); if (onRendered) onRendered(); }
        struct Render { std::vector<float> full; std::array<std::vector<float>, 4> part; float ms = 0.f; std::vector<float> stereoL, stereoR;
                        float scrStart = -1.f, scrEnd = -1.f; bool scrManual = false; };
        juce::Rectangle<float> plotArea() const;
        float handleX() const;
        bool draggingHandle = false;
        std::mutex lock;
        ParamFrame pending; int pendingNote = 31; bool wanted = false;
        ParamFrame lastAsked; int lastNote = -1; uint32_t lastVersion = 0;
        Tables lastTables;
        std::unique_ptr<env::Bank> pendingBank, bank;
        Tables pendingTables, tables;   // the drawn shapes (copied when they change)
        dsp::dist::DistData pendingDist, distData; uint32_t lastDist = 0xffffffffu;   // the waveshaper curves
        dsp::mod::Data pendingMod, modData; uint32_t lastMod = 0xffffffffu; bool hasMod = false;
        Samples pendingSmp, smp; uint32_t lastSmp = 0xffffffffu; bool smpChanged = false;   // module 09: the kick with its macros / modulation
        Render shown;
        std::atomic<bool> ready { false };
        juce::WaitableEvent wake;
    };
}
