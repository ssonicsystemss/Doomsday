#pragma once
// =====================================================================
//  4GE - a small LIVE SPECTRUM for the EQ knobs of a tab (owner 2026-11-14 "widma na żywo na każdym EQ"): the real output
//  while playing (the processor's lock-free tap, analysed by dsp::live::Analyser) + a marker per EQ frequency of the tab
//  (TAIL COLOR, MID FREQ, LOW CUT ...). GUI only: it reads the tap and the parameters, it draws.
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "Palette.h"

namespace fge::gui
{
    class LiveSpecView : public juce::Component, private juce::Timer
    {
    public:
        struct Marker { juce::String id, label; bool harmonicOf = false; };   // harmonicOf: the value is a harmonic number (x the played note)
        explicit LiveSpecView (GeProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); startTimerHz (25); }
        void setMarkers (std::vector<Marker> m) { markers = std::move (m); repaint(); }
        void paint (juce::Graphics& g) override
        {
            const auto& c = palette();
            auto r = getLocalBounds().toFloat();
            g.setColour (c.ground.withAlpha (0.55f)); g.fillRoundedRectangle (r, 6.f);
            g.setColour (c.grid); g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
            auto gr = r.reduced (6.f, 14.f).withTrimmedBottom (-6.f);
            auto xOf = [&] (float hz) { return gr.getX() + gr.getWidth() * std::log (std::max (20.f, hz) / 20.f) / std::log (1000.f); };
            for (float hz : { 100.f, 1000.f, 10000.f }) { g.setColour (c.grid); g.drawVerticalLine ((int) xOf (hz), gr.getY(), gr.getBottom()); }
            g.setColour (c.textDim); g.setFont (juce::FontOptions (9.f));
            g.drawText (show.empty() ? "LIVE SPECTRUM (play the kick)" : "LIVE (output)", r.reduced (6.f, 2.f).removeFromTop (11.f), juce::Justification::left);
            if (! show.empty())
            {
                juce::Path p;
                for (int k = 0; k < (int) show.size(); ++k)
                {
                    const float x = gr.getX() + gr.getWidth() * (float) k / (float) (show.size() - 1), y = gr.getY() + gr.getHeight() * juce::jlimit (0.f, 1.f, -show[(size_t) k] / 80.f);
                    k == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
                }
                juce::Path f = p; f.lineTo (gr.getRight(), gr.getBottom()); f.lineTo (gr.getX(), gr.getBottom()); f.closeSubPath();
                g.setColour (c.silver.withAlpha (0.12f)); g.fillPath (f);
                g.setColour (c.silver.withAlpha (0.6f)); g.strokePath (p, juce::PathStrokeType (1.1f));
            }
            int row = 0;
            for (auto& m : markers)
            {
                const int ly = (int) gr.getY() + 11 * row++;
                auto* v = proc.apvts.getRawParameterValue (m.id);
                if (v == nullptr) continue;
                float hz = v->load();
                if (m.harmonicOf) hz *= 440.f * std::exp2 ((float) (proc.auditionNote - 69) / 12.f);
                const float x = xOf (hz);
                g.setColour (c.red.withAlpha (0.85f)); g.drawLine (x, gr.getY(), x, gr.getBottom(), 1.4f);
                g.setFont (juce::FontOptions (9.f)); g.drawText (m.label, (int) x + 3, ly, 70, 10, juce::Justification::left);
            }
        }
    private:
        void timerCallback() override
        {
            if (! isShowing()) return;
            const auto np = proc.liveTap.pushed.load (std::memory_order_relaxed);
            idle = np != pushed ? 0 : idle + 1; pushed = np;
            if (idle < 8)
            {
                an.compute (proc.liveTap, 120, now);
                if (show.size() != now.size()) show = now;
                for (size_t k = 0; k < now.size(); ++k) show[k] = now[k] > show[k] ? now[k] : show[k] - 3.f;
                repaint();
            }
            else if (! show.empty()) { show.clear(); repaint(); }
            else repaint();                                                  // (the markers follow the knobs)
        }
        GeProcessor& proc;
        fge::dsp::live::Analyser an;
        std::vector<float> now, show;
        std::vector<Marker> markers;
        std::int64_t pushed = -1; int idle = 99;
    };
}
