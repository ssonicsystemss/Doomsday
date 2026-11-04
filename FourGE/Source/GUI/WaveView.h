#pragma once
// =====================================================================
//  4GE central waveform: the REAL kick, rendered offline on a background thread from the current parameters
//  (full kick + each part solo), drawn with the part colour code (Design/PALETTE.md). Drag it into FL = a WAV file.
//  GUI only: it asks fge::renderKick (DSP) on its own thread, never touches the audio thread.
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Parameters/ParamFrame.h"
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
        void update (const ParamFrame&, int note);
        void paint (juce::Graphics&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
        // the last rendered kick (tests): peak per part, length in ms
        std::array<float, 5> lastPeaks() const;
        bool hasRender() const { return ready.load(); }
        juce::File writeWav (const juce::File& folder);      // the full kick as 24-bit WAV (also the drag-and-drop file)
        std::function<void()> onRendered;
        static constexpr double rate = 44100.0;

    private:
        void run() override;
        void handleAsyncUpdate() override { repaint(); if (onRendered) onRendered(); }
        struct Render { std::vector<float> full; std::array<std::vector<float>, 4> part; float ms = 0.f; std::vector<float> stereoL, stereoR; };
        std::mutex lock;
        ParamFrame pending; int pendingNote = 31; bool wanted = false;
        ParamFrame lastAsked; int lastNote = -1;
        Render shown;
        std::atomic<bool> ready { false };
        juce::WaitableEvent wake;
    };
}
