#include "OscScope.h"
#include "Palette.h"
#include "../DSP/Osc/OscTables.h"

using namespace juce;

namespace fge::gui
{
    OscScope::OscScope() : Thread ("4GE osc scope") { startThread (Thread::Priority::low); }
    OscScope::~OscScope() { signalThreadShouldExit(); wake.signal(); stopThread (3000); }

    void OscScope::update (const ParamFrame& f, const Tables& t, int sel, bool sum, bool post)
    {
        if (sel == lastSel && sum == lastSum && post == lastPost && t == lastT && std::equal (f.v.begin(), f.v.end(), lastF.v.begin())) return;
        lastF = f; lastT = t; lastSel = sel; lastSum = sum; lastPost = post;
        { std::lock_guard<std::mutex> l (lock); pending = f; pendingTables = t; pSel = sel; pSum = sum; pPost = post; wanted = true; }
        wake.signal();
    }

    void OscScope::run()
    {
        juce::dsp::FFT fft (fftOrder);
        std::vector<float> buf ((size_t) fftN * 2);
        while (! threadShouldExit())
        {
            wake.wait (200);
            ParamFrame f; Tables t; int sel; bool sum, post;
            { std::lock_guard<std::mutex> l (lock); if (! wanted) continue; f = pending; t = pendingTables; sel = pSel; sum = pSum; post = pPost; wanted = false; }
            dsp::OscTables ot;
            for (size_t i = 0; i < t.size(); ++i) ot.t[i] = t[i].get();
            Result r; r.sel = sel; r.sum = sum; r.post = post;
            const float hz = previewHz();
            const int settle = fftN;                           // past the first cycles (FM / scan settle), then one FFT window
            for (int k = 0; k < 5; ++k)                        // 0..3 each oscillator, 4 the sum: two cycles to draw
            {
                auto x = dsp::renderOscPreview (f, ot, k == 4 ? -1 : k, post, hz, settle + fftN, rate);
                const int per = (int) std::lround (rate / hz);
                r.wave[(size_t) k].assign (x.begin() + settle, x.begin() + settle + 2 * per);
                if ((sum && k == 4) || (! sum && k == sel))
                {
                    std::fill (buf.begin(), buf.end(), 0.f);
                    for (int i = 0; i < fftN; ++i) buf[(size_t) i] = x[(size_t) (settle + i)] * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) fftN));
                    fft.performFrequencyOnlyForwardTransform (buf.data());
                    for (int h = 0; h < 64; ++h)
                    {
                        const int b = binOf0 * (h + 1);
                        const float m = b < fftN / 2 ? buf[(size_t) b] / ((float) fftN * 0.25f) : 0.f;
                        r.spec[(size_t) h] = 20.f * std::log10 (m + 1.0e-7f);
                    }
                }
                if (threadShouldExit()) return;
            }
            {
                std::lock_guard<std::mutex> l (lock);
                if (wanted) continue;
                shown = std::move (r);
            }
            ready.store (true);
            triggerAsyncUpdate();
        }
    }

    void OscScope::paint (Graphics& g)
    {
        const auto& c = palette();
        auto b = getLocalBounds().toFloat();
        g.setColour (c.ground); g.fillRoundedRectangle (b, 5.f);
        Result r;
        { std::lock_guard<std::mutex> l (lock); r = shown; }
        auto top = b.removeFromTop (b.getHeight() * 0.5f).reduced (8.f, 6.f);
        auto bot = b.reduced (8.f, 6.f);
        g.setFont (FontOptions (10.f, Font::bold));
        g.setColour (c.textDim);
        g.drawText (String (r.sum ? "SUM" : "OSC " + String (r.sel + 1)) + (r.post ? "  -  after FM / RING / SYNC" : "  -  before modulation"),
                    top.toNearestInt().removeFromTop (14), Justification::topLeft);
        g.setColour (c.grid); g.drawHorizontalLine ((int) top.getCentreY(), top.getX(), top.getRight());
        // waveforms: the others faint, the selected (or the sum) bright
        auto drawWave = [&] (const std::vector<float>& w, Colour col, float thick)
        {
            if (w.size() < 2) return;
            float pk = 1.0e-6f; for (auto v : w) pk = jmax (pk, std::abs (v));
            const float sc = 1.f / jmax (1.f, pk);
            Path p;
            for (size_t i = 0; i < w.size(); ++i)
            {
                const float x = top.getX() + top.getWidth() * (float) i / (float) (w.size() - 1);
                const float y = top.getCentreY() - w[i] * sc * top.getHeight() * 0.42f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (col); g.strokePath (p, PathStrokeType (thick));
        };
        for (int k = 0; k < 4; ++k) if (r.sum || k != r.sel) drawWave (r.wave[(size_t) k], c.part[(size_t) k].withAlpha (0.28f), 1.f);
        drawWave (r.sum ? r.wave[4] : r.wave[(size_t) r.sel], r.sum ? c.silver : c.part[(size_t) r.sel], 2.f);
        // spectrum: harmonics 1..64, 0 .. -72 dB (relative to the strongest)
        float mx = -200.f; for (auto v : r.spec) mx = jmax (mx, v);
        const float bw = bot.getWidth() / 64.f;
        g.setColour (c.grid);
        for (int d = 0; d <= 3; ++d) g.drawHorizontalLine ((int) (bot.getY() + bot.getHeight() * (float) d / 3.f), bot.getX(), bot.getRight());
        for (int h = 0; h < 64; ++h)
        {
            const float v = jlimit (0.f, 1.f, (r.spec[(size_t) h] - mx + 72.f) / 72.f);
            const float hh = v * bot.getHeight();
            g.setColour ((h % 2 == 0 ? c.red : c.hot.brighter (0.2f)).withAlpha (0.85f));
            g.fillRect (bot.getX() + (float) h * bw + 1.f, bot.getBottom() - hh, jmax (1.f, bw - 2.f), hh);
        }
        g.setColour (c.textDim); g.setFont (FontOptions (9.5f));
        g.drawText ("HARMONICS 1 - 64 (odd red, even light)   0 / -24 / -48 / -72 dB", bot.toNearestInt().removeFromTop (12), Justification::topRight);
    }
}
