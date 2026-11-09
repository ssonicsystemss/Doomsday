#include "WaveView.h"
#include "Palette.h"
#include "../DSP/Engine.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;

namespace fge::gui
{
    WaveView::WaveView() : Thread ("4GE wave") { startThread (Thread::Priority::low); }
    WaveView::~WaveView() { signalThreadShouldExit(); wake.signal(); stopThread (3000); }

    void WaveView::update (const ParamFrame& f, int note, const env::Bank* shapes, uint32_t shapesVersion, const Tables* tb, const dsp::dist::DistData* dist, uint32_t distVersion,
                           const dsp::mod::Data* mod, uint32_t modVersion)
    {
        const bool tablesChanged = tb != nullptr && *tb != lastTables;
        const bool distChanged = dist != nullptr && distVersion != lastDist;
        const bool modChanged = mod != nullptr && modVersion != lastMod;
        if (distChanged) { lastDist = distVersion; std::lock_guard<std::mutex> l (lock); pendingDist = *dist; }
        if (modChanged) { lastMod = modVersion; std::lock_guard<std::mutex> l (lock); pendingMod = *mod; hasMod = true; }
        if (! tablesChanged && ! distChanged && ! modChanged && lastNote == note && shapesVersion == lastVersion && std::equal (f.v.begin(), f.v.end(), lastAsked.v.begin())) return;
        if (tb != nullptr) lastTables = *tb;
        lastAsked = f; lastNote = note;
        std::unique_ptr<env::Bank> copy;
        if (shapes != nullptr && shapesVersion != lastVersion) copy = std::make_unique<env::Bank> (*shapes);
        lastVersion = shapesVersion;
        { std::lock_guard<std::mutex> l (lock); pending = f; pendingNote = note; wanted = true; if (copy) pendingBank = std::move (copy); pendingTables = lastTables; }
        wake.signal();
    }

    void WaveView::setSamples (const Samples& s, uint32_t version)
    {
        if (version == lastSmp) return;
        lastSmp = version;
        { std::lock_guard<std::mutex> l (lock); pendingSmp = s; smpChanged = true; wanted = true; }
        wake.signal();
    }

    void WaveView::run()
    {
        while (! threadShouldExit())
        {
            wake.wait (200);
            ParamFrame f; int note;
            { std::lock_guard<std::mutex> l (lock); if (! wanted) continue; f = pending; note = pendingNote; wanted = false;
              if (pendingBank) bank = std::move (pendingBank); tables = pendingTables; distData = pendingDist; modData = pendingMod; if (smpChanged) { smp = pendingSmp; smpChanged = false; } }
            Engine::SamplePtrs sp; bool anySmp = false;
            for (int k = 0; k < dsp::numSampleSlots; ++k) { sp[(size_t) k].store (smp[(size_t) k].get()); anySmp |= smp[(size_t) k] != nullptr; }
            const Engine::SamplePtrs* spp = anySmp ? &sp : nullptr;
            const dsp::mod::Data* md = hasMod ? &modData : nullptr;
            const env::Bank* b0 = bank.get();
            dsp::OscTables ot;
            for (size_t i = 0; i < tables.size(); ++i) ot.t[i] = tables[i].get();
            const float lenMs = juce::jlimit (150.f, 4100.f, kickLengthMs (f, 150.0) + 60.f);
            const double secs = lenMs / 1000.0;
            Render r;
            r.ms = lenMs;
            auto full = renderKick (f, note, rate, secs, -1, 1.f, b0, 0.0, -1.0, &ot, nullptr, &distData, nullptr, md, spp);
            const int n = full.getNumSamples();
            r.full.resize ((size_t) n); r.stereoL.resize ((size_t) n); r.stereoR.resize ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                r.stereoL[(size_t) i] = full.getSample (0, i); r.stereoR[(size_t) i] = full.getSample (1, i);
                r.full[(size_t) i] = 0.5f * (r.stereoL[(size_t) i] + r.stereoR[(size_t) i]);
            }
            if (f.on (P::scrOn))                                          // module 06: where the screech sits (the engine's own timing)
            {
                auto e = std::make_unique<Engine>();
                e->prepare (rate, 256); e->setBank (b0); e->cookOnly (f); e->trigger (note, 1.f);
                r.scrStart = e->screechTiming().startMs; r.scrEnd = e->screechTiming().endMs;
                r.scrManual = f.idx (P::scrStartMode) == 0;
            }
            for (int p = 0; p < 4 && ! threadShouldExit(); ++p)
            {
                auto b = renderKick (f, note, rate, secs, p, 1.f, b0, 0.0, -1.0, &ot, nullptr, &distData, nullptr, md, spp);
                auto& v = r.part[(size_t) p];
                v.resize ((size_t) n);
                for (int i = 0; i < n; ++i) v[(size_t) i] = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
            }
            {
                std::lock_guard<std::mutex> l (lock);
                if (wanted) continue;          // already out of date: render the newer one first
                shown = std::move (r);
            }
            ready.store (true);
            triggerAsyncUpdate();
        }
    }

    std::array<float, 5> WaveView::lastPeaks() const
    {
        std::array<float, 5> pk {};
        auto& self = const_cast<WaveView&> (*this);
        std::lock_guard<std::mutex> l (self.lock);
        for (auto v : shown.full) pk[0] = jmax (pk[0], std::abs (v));
        for (int p = 0; p < 4; ++p) for (auto v : shown.part[(size_t) p]) pk[(size_t) p + 1] = jmax (pk[(size_t) p + 1], std::abs (v));
        return pk;
    }

    void WaveView::paint (Graphics& g)
    {
        const auto& c = palette();
        auto area = getLocalBounds().toFloat();
        g.setColour (c.panel); g.fillRoundedRectangle (area, 6.f);
        auto plot = plotArea();
        std::lock_guard<std::mutex> l (lock);
        const float ms = jmax (1.f, shown.ms);
        // time grid every 50 / 100 ms
        g.setFont (FontOptions (11.f));
        const float step = ms > 1200.f ? 200.f : (ms > 500.f ? 100.f : 50.f);
        for (float t = 0.f; t <= ms; t += step)
        {
            const float x = plot.getX() + plot.getWidth() * t / ms;
            g.setColour (c.grid); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (c.textDim); g.drawText (String ((int) t) + " ms", (int) x + 3, (int) plot.getBottom() + 2, 60, 14, Justification::left);
        }
        g.setColour (c.grid.brighter (0.15f)); g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
        if (shown.full.empty()) { g.setColour (c.textDim); g.drawText ("rendering...", plot.toNearestInt(), Justification::centred); return; }
        const int n = (int) shown.full.size();
        const int px = jmax (1, (int) plot.getWidth());
        auto envPath = [&] (const std::vector<float>& v, float scale)
        {
            Path p;
            p.startNewSubPath (plot.getX(), plot.getCentreY());
            std::vector<float> hi ((size_t) px), lo ((size_t) px);
            for (int x = 0; x < px; ++x)
            {
                const int a = x * n / px, b = jmax (a + 1, (x + 1) * n / px);
                float mx = 0.f, mn = 0.f;
                for (int i = a; i < b && i < n; ++i) { mx = jmax (mx, v[(size_t) i]); mn = jmin (mn, v[(size_t) i]); }
                hi[(size_t) x] = mx; lo[(size_t) x] = mn;
            }
            for (int x = 0; x < px; ++x) p.lineTo (plot.getX() + (float) x, plot.getCentreY() - hi[(size_t) x] * scale);
            for (int x = px - 1; x >= 0; --x) p.lineTo (plot.getX() + (float) x, plot.getCentreY() - lo[(size_t) x] * scale);
            p.closeSubPath();
            return p;
        };
        float peak = 0.f;
        for (auto v : shown.full) peak = jmax (peak, std::abs (v));
        const float scale = plot.getHeight() * 0.48f / jmax (0.05f, peak);
        // the parts (colour code), then the full kick outline on top
        for (int p = 3; p >= 0; --p)
        {
            g.setColour (c.part[(size_t) p].withAlpha (0.28f));
            g.fillPath (envPath (shown.part[(size_t) p], scale));
        }
        auto fullPath = envPath (shown.full, scale);
        g.setColour (c.silver.withAlpha (0.12f)); g.fillPath (fullPath);
        g.setColour (c.silver.withAlpha (0.85f)); g.strokePath (fullPath, PathStrokeType (1.f));
        // legend + drag hint
        const char* names[4] { "PUNCH", "BODY", "TAIL", "SUB" };
        float lx = plot.getX() + 4.f;
        g.setFont (FontOptions (11.f, Font::bold));
        for (int p = 0; p < 4; ++p)
        {
            g.setColour (c.part[(size_t) p]); g.fillRect (lx, plot.getY() + 4.f, 8.f, 8.f);
            g.setColour (c.textDim); g.drawText (names[p], (int) lx + 11, (int) plot.getY(), 60, 16, Justification::left);
            lx += 64.f;
        }
        g.setColour (c.textDim);
        g.drawText ("drag the kick into FL", plot.toNearestInt().removeFromTop (16).withTrimmedRight (4), Justification::right);
        // SCREECH START handle (MANUAL: drag it; BODY END / TAIL ...: it follows them)
        if (shown.scrStart >= 0.f)
        {
            const float x0 = plot.getX() + plot.getWidth() * jlimit (0.f, 1.f, shown.scrStart / ms), x1 = plot.getX() + plot.getWidth() * jlimit (0.f, 1.f, shown.scrEnd / ms);
            g.setColour (c.part[4].withAlpha (0.10f)); g.fillRect (x0, plot.getY() + 20.f, jmax (1.f, x1 - x0), plot.getHeight() - 20.f);
            g.setColour (c.part[4]); g.drawLine (x0, plot.getY() + 20.f, x0, plot.getBottom(), 1.5f);
            g.fillRoundedRectangle (x0 - 5.f, plot.getY() + 20.f, 10.f, 14.f, 2.f);
            g.setFont (FontOptions (10.f, Font::bold));
            g.drawText (String ("SCREECH ") + (shown.scrManual ? "(drag)" : "(synced)"), (int) x0 + 8, (int) plot.getY() + 20, 110, 14, Justification::left);
        }
    }
    Rectangle<float> WaveView::plotArea() const { return getLocalBounds().toFloat().reduced (10.f, 8.f).withTrimmedBottom (16.f); }
    float WaveView::handleX() const
    {
        const auto plot = plotArea();
        return shown.scrStart < 0.f ? -1.0e6f : plot.getX() + plot.getWidth() * jlimit (0.f, 1.f, shown.scrStart / jmax (1.f, shown.ms));
    }
    void WaveView::mouseMove (const MouseEvent& e)
    {
        std::lock_guard<std::mutex> l (lock);
        const bool onHandle = shown.scrManual && std::abs (e.position.x - handleX()) < 7.f;
        setMouseCursor (onHandle ? MouseCursor::LeftRightResizeCursor : MouseCursor::DraggingHandCursor);
    }
    void WaveView::mouseDown (const MouseEvent& e)
    {
        float st = 0.f;
        { std::lock_guard<std::mutex> l (lock); draggingHandle = shown.scrManual && onScreechStart != nullptr && std::abs (e.position.x - handleX()) < 7.f; st = shown.scrStart; }
        if (draggingHandle) onScreechStart (st, true);
    }

    File WaveView::writeWav (const File& folder)
    {
        std::vector<float> L, R;
        { std::lock_guard<std::mutex> l (lock); L = shown.stereoL; R = shown.stereoR; }
        if (L.empty()) return {};
        // trim the silence at the end
        int n = (int) L.size();
        while (n > 1 && std::abs (L[(size_t) n - 1]) < 1.0e-5f && std::abs (R[(size_t) n - 1]) < 1.0e-5f) --n;
        folder.createDirectory();
        auto f = folder.getChildFile ("4GE Kick.wav");
        f.deleteFile();
        AudioBuffer<float> b (2, n);
        b.copyFrom (0, 0, L.data(), n); b.copyFrom (1, 0, R.data(), n);
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), rate, 2, 24, {}, 0));
        if (w == nullptr) return {};
        w->writeFromAudioSampleBuffer (b, 0, n);
        return f;
    }

    void WaveView::mouseDrag (const MouseEvent& e)
    {
        if (draggingHandle)
        {
            const auto plot = plotArea();
            float ms;
            { std::lock_guard<std::mutex> l (lock); ms = jlimit (0.f, 2500.f, (e.position.x - plot.getX()) / jmax (1.f, plot.getWidth()) * jmax (1.f, shown.ms)); shown.scrStart = ms; }
            repaint();
            onScreechStart (ms, false);
            return;
        }
        if (e.getDistanceFromDragStart() < 6 || ! ready.load()) return;
        auto f = writeWav (File::getSpecialLocation (File::tempDirectory).getChildFile ("4GE"));
        if (f.existsAsFile()) DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
    }
}
