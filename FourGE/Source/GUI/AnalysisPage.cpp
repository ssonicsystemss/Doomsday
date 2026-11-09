#include "AnalysisPage.h"
#include "Palette.h"

using namespace juce;

namespace fge::gui
{
    namespace ka = fge::dsp::analysis;

    AnalysisPage::AnalysisPage (GeProcessor& p, doomsday::flat::FlatLookAndFeel&) : proc (p)
    {
        loadBtn.setTooltip ("Load a reference kick (WAV / AIFF / FLAC / MP3) - or drop the file anywhere on this page.");
        mineBtn.setTooltip ("Render the current 4GE sound (like the export: full kick, master output included) on a background thread and analyse it.");
        clearBtn.setTooltip ("Forget the reference.");
        applyBtn.setTooltip ("Apply the ticked suggestions - all of them in ONE undo step (Ctrl+Z takes them back).");
        allBtn.setTooltip ("Apply every suggestion - in ONE undo step.");
        noteBox.setTooltip ("The note 4GE is analysed on (loading a reference sets it to the reference's tail note, so both are compared on the same note).");
        for (int m = 24; m <= 59; ++m) noteBox.addItem (ka::noteName (m), m);
        noteBox.setSelectedId (noteMidi, dontSendNotification);
        noteBox.onChange = [this] { noteMidi = noteBox.getSelectedId(); };
        loadBtn.onClick = [this]
        {
            chooser = std::make_unique<FileChooser> ("Reference kick", File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
            SafePointer<AnalysisPage> self (this);
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                  [self] (const FileChooser& fc) { if (self != nullptr && fc.getResult().existsAsFile()) self->loadReference (fc.getResult()); });
        };
        mineBtn.onClick = [this] { analyseMine(); };
        clearBtn.onClick = [this] { clearReference(); };
        applyBtn.onClick = [this] { applySelected(); };
        allBtn.onClick = [this] { applyAll(); };
        for (auto* b : { &loadBtn, &mineBtn, &clearBtn, &applyBtn, &allBtn }) addAndMakeVisible (b);
        addAndMakeVisible (noteBox);
        list.setRowHeight (22); list.setColour (ListBox::backgroundColourId, palette().ground);
        addAndMakeVisible (list);
        // the last reference comes back with the project (only its path: the file is analysed again)
        const File last (proc.apvts.state.getProperty ("anaRef").toString());
        if (last.existsAsFile()) loadReference (last);
        startTimerHz (10);
    }
    AnalysisPage::~AnalysisPage() { *alive = false; stopTimer(); }

    bool AnalysisPage::isInterestedInFileDrag (const StringArray& f)
    {
        for (auto& x : f) if (File (x).hasFileExtension ("wav;aif;aiff;flac;mp3")) return true;
        return false;
    }
    void AnalysisPage::filesDropped (const StringArray& f, int, int)
    {
        for (auto& x : f) if (File (x).hasFileExtension ("wav;aif;aiff;flac;mp3")) { loadReference (File (x)); return; }
    }

    void AnalysisPage::loadReference (const File& file)
    {
        ++jobs; status = "analysing " + file.getFileName() + "...";
        auto life = alive; SafePointer<AnalysisPage> self (this);
        Thread::launch ([file, life, self]
        {
            AudioBuffer<float> b; double rate = 0; String err;
            ka::Report r;
            if (fge::analysis::loadReference (file, b, rate, err)) { r = ka::analyse (b, rate); if (! r.valid) err = "nothing to analyse in " + file.getFileName(); }
            if (! *life) return;
            MessageManager::callAsync ([self, life, r, err, file]
            {
                if (! *life || self == nullptr) return;
                --self->jobs;
                if (err.isNotEmpty()) { self->status = err; self->repaint(); return; }
                self->ref = r; self->refName = file.getFileName();
                self->proc.apvts.state.setProperty ("anaRef", file.getFullPathName(), nullptr);
                if (r.endNote >= 24 && r.endNote <= 59) self->setNote (r.endNote);
                self->status = "reference: " + file.getFileName() + (r.endNote >= 24 && r.endNote <= 59 ? "  -  4GE is analysed on " + ka::noteName (r.endNote) + " (the reference's note)" : String());
                self->analyseMine();
            });
        });
        repaint();
    }

    void AnalysisPage::analyseMine()
    {
        ++jobs;
        auto snap = std::make_shared<fge::dsp::xport::Snapshot> (proc.exportSnapshot());
        const int n = noteMidi; const double bpm = proc.hostBpm();
        auto life = alive; SafePointer<AnalysisPage> self (this);
        Thread::launch ([snap, n, bpm, life, self]
        {
            auto audio = fge::analysis::renderMine (*snap, n, bpm, nullptr);
            auto r = ka::analyse (audio, 44100.0);
            if (! *life) return;
            MessageManager::callAsync ([self, life, r, snap]
            {
                if (! *life || self == nullptr) return;
                --self->jobs;
                self->mine = r; self->mineFrame = snap->f;
                self->rebuildSuggestions();
                self->repaint();
            });
        });
        repaint();
    }

    void AnalysisPage::clearReference()
    {
        ref = {}; refName = {}; proc.apvts.state.removeProperty ("anaRef", nullptr);
        rebuildSuggestions(); status = "no reference"; repaint();
    }

    void AnalysisPage::setNote (int m) { noteMidi = jlimit (24, 59, m); noteBox.setSelectedId (noteMidi, dontSendNotification); }

    void AnalysisPage::rebuildSuggestions()
    {
        sugg = fge::analysis::suggest (ref, mine, mineFrame);
        chosen.assign (sugg.size(), false);
        for (size_t i = 0; i < sugg.size(); ++i) chosen[i] = ! sugg[i].changes.empty();
        list.updateContent(); list.repaint();
    }

    int AnalysisPage::applyRows (const std::vector<int>& rows)
    {
        std::vector<fge::analysis::Suggestion> pick;
        for (int r : rows) if (isPositiveAndBelow (r, (int) sugg.size()) && ! sugg[(size_t) r].changes.empty()) pick.push_back (sugg[(size_t) r]);
        if (pick.empty()) { status = "nothing ticked to apply"; repaint(); return 0; }
        const int n = fge::analysis::apply (proc, pick);
        status = "applied " + String ((int) pick.size()) + " suggestion(s) = " + String (n) + " parameter change(s) in ONE undo step - analysing 4GE again...";
        reanalyseIn = 3;                                                   // (the parameters settle, then the new sound is analysed)
        repaint();
        return n;
    }
    int AnalysisPage::applySelected() { std::vector<int> r; for (int i = 0; i < (int) chosen.size(); ++i) if (chosen[(size_t) i]) r.push_back (i); return applyRows (r); }
    int AnalysisPage::applyAll() { std::vector<int> r; for (int i = 0; i < (int) sugg.size(); ++i) r.push_back (i); return applyRows (r); }

    void AnalysisPage::timerCallback()
    {
        if (reanalyseIn > 0 && --reanalyseIn == 0) analyseMine();
        mineBtn.setEnabled (! busy()); loadBtn.setEnabled (! busy());
        applyBtn.setEnabled (! busy() && ref.valid && mine.valid); allBtn.setEnabled (applyBtn.isEnabled());
    }

    void AnalysisPage::listBoxItemClicked (int row, const MouseEvent&) { if (isPositiveAndBelow (row, (int) sugg.size()) && ! sugg[(size_t) row].changes.empty()) setChosen (row, ! chosen[(size_t) row]); }
    void AnalysisPage::paintListBoxItem (int row, Graphics& g, int w, int h, bool)
    {
        if (! isPositiveAndBelow (row, (int) sugg.size())) return;
        const auto& c = palette(); const auto& s = sugg[(size_t) row];
        const bool act = ! s.changes.empty();
        if (act)
        {
            Rectangle<float> box (6.f, (h - 12) * 0.5f, 12.f, 12.f);
            g.setColour (c.control); g.fillRoundedRectangle (box, 2.f);
            if (chosen[(size_t) row]) { g.setColour (c.red); g.fillRoundedRectangle (box.reduced (2.5f), 1.5f); }
        }
        g.setColour (act ? c.silver : c.textDim); g.setFont (FontOptions (12.f));
        g.drawFittedText (s.text, 26, 0, w - 30, h, Justification::centredLeft, 2, 0.85f);
    }

    void AnalysisPage::resized()
    {
        auto r = getLocalBounds().reduced (10, 6);
        barArea = r.removeFromTop (30);
        {
            auto b = barArea.withTrimmedTop (2).withTrimmedBottom (2);
            loadBtn.setBounds (b.removeFromLeft (140)); b.removeFromLeft (6);
            clearBtn.setBounds (b.removeFromLeft (60)); b.removeFromLeft (16);
            mineBtn.setBounds (b.removeFromLeft (120)); b.removeFromLeft (6);
            noteBox.setBounds (b.removeFromLeft (70));
        }
        r.removeFromTop (6);
        auto left = r.removeFromLeft (r.getWidth() * 58 / 100); r.removeFromLeft (10);
        waveArea = left.removeFromTop (170); left.removeFromTop (8);
        pitchArea = left.removeFromTop (200); left.removeFromTop (8);
        bandArea = left.removeFromBottom (120); left.removeFromBottom (8);
        specArea = left;
        tableArea = r.removeFromTop (20 + 17 * 17); r.removeFromTop (8);
        featArea = r.removeFromTop (130); r.removeFromTop (8);
        auto bb = r.removeFromBottom (28);
        applyBtn.setBounds (bb.removeFromLeft (150)); bb.removeFromLeft (6); allBtn.setBounds (bb.removeFromLeft (110));
        r.removeFromBottom (6);
        suggArea = r;
        list.setBounds (suggArea.withTrimmedTop (18));
    }

    void AnalysisPage::paint (Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.panel); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.f);
        g.setColour (c.textDim); g.setFont (FontOptions (11.f));
        g.drawFittedText ((busy() ? "working...  " : "") + status, barArea.withTrimmedLeft (420), Justification::centredLeft, 2);
        float tMax = 0.f;
        if (ref.valid) tMax = jmax (tMax, ref.lengthMs);
        if (mine.valid) tMax = jmax (tMax, mine.lengthMs);
        tMax = jlimit (100.f, 4000.f, tMax * 1.12f);
        paintWave (g, waveArea, tMax);
        paintPitch (g, pitchArea, tMax);
        paintSpectrum (g, specArea);
        paintBands (g, bandArea);
        paintTable (g, tableArea);
        paintFeatures (g, featArea);
        g.setColour (c.textDim); g.setFont (FontOptions (11.f, Font::bold));
        g.drawText ("SUGGESTIONS (tick, then APPLY - one undo step; nothing changes before)", suggArea.getX(), suggArea.getY(), suggArea.getWidth(), 14, Justification::left);
    }

    namespace
    {
        void frame (Graphics& g, Rectangle<int> a, const String& title)
        {
            const auto& c = palette();
            g.setColour (c.ground); g.fillRoundedRectangle (a.toFloat(), 4.f);
            g.setColour (c.textDim); g.setFont (FontOptions (10.f, Font::bold)); g.drawText (title, a.getX() + 8, a.getY() + 4, a.getWidth() - 16, 12, Justification::left);
        }
        void legend (Graphics& g, Rectangle<int> a, bool haveRef, bool haveMine)
        {
            const auto& c = palette(); g.setFont (FontOptions (10.f));
            int x = a.getRight() - 8;
            if (haveMine) { g.setColour (c.red); g.drawText ("4GE", x - 30, a.getY() + 4, 30, 12, Justification::right); x -= 38; }
            if (haveRef) { g.setColour (c.silver); g.drawText ("REFERENCE", x - 70, a.getY() + 4, 70, 12, Justification::right); }
        }
    }

    void AnalysisPage::paintWave (Graphics& g, Rectangle<int> a, float tMax)
    {
        frame (g, a, "WAVEFORM"); legend (g, a, ref.valid, mine.valid);
        auto r = a.reduced (8, 18).toFloat();
        const auto& c = palette();
        g.setColour (c.grid); g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
        auto draw = [&] (const ka::Report& rep, Colour col, bool fill)
        {
            if (! rep.valid || rep.wmax.empty()) return;
            float pk = 1.0e-6f; for (size_t k = 0; k < rep.wmax.size(); ++k) pk = jmax (pk, rep.wmax[k], -rep.wmin[k]);
            Path top, bot, area; std::vector<Point<float>> lows;
            const int nb = (int) rep.wmax.size();
            for (int k = 0; k < nb; ++k)
            {
                const float t = (float) ((k + 0.5) * rep.seconds * 1000.0 / nb); if (t > tMax) break;
                const float x = r.getX() + r.getWidth() * t / tMax;
                const float y1 = r.getCentreY() - rep.wmax[(size_t) k] / pk * r.getHeight() * 0.5f, y2 = r.getCentreY() - rep.wmin[(size_t) k] / pk * r.getHeight() * 0.5f;
                if (k == 0) { top.startNewSubPath (x, y1); bot.startNewSubPath (x, y2); area.startNewSubPath (x, y1); } else { top.lineTo (x, y1); bot.lineTo (x, y2); area.lineTo (x, y1); }
                lows.push_back ({ x, y2 });
            }
            for (auto it = lows.rbegin(); it != lows.rend(); ++it) area.lineTo (*it);
            area.closeSubPath();
            if (fill) { g.setColour (col.withAlpha (0.45f)); g.fillPath (area); }
            else { g.setColour (col); g.strokePath (top, PathStrokeType (1.2f)); g.strokePath (bot, PathStrokeType (1.2f)); }
        };
        draw (ref, c.silver, true);
        draw (mine, c.red, false);
        g.setColour (c.textDim); g.setFont (FontOptions (10.f));
        g.drawText (String (roundToInt (tMax)) + " ms", a.getRight() - 70, a.getBottom() - 15, 62, 12, Justification::right);
        if (! ref.valid && ! mine.valid) g.drawText ("drop a reference WAV here", a, Justification::centred);
    }

    void AnalysisPage::paintPitch (Graphics& g, Rectangle<int> a, float tMax)
    {
        frame (g, a, "PITCH CURVE"); legend (g, a, ref.valid, mine.valid);
        auto r = a.reduced (34, 18).withTrimmedLeft (0).toFloat();
        const auto& c = palette();
        auto yOf = [&] (float hz) { return r.getBottom() - r.getHeight() * std::log (jlimit (25.f, 2000.f, hz) / 25.f) / std::log (2000.f / 25.f); };
        g.setFont (FontOptions (10.f));
        for (float hz : { 30.f, 50.f, 100.f, 200.f, 500.f, 1000.f })
        {
            g.setColour (c.grid); g.drawHorizontalLine ((int) yOf (hz), r.getX(), r.getRight());
            g.setColour (c.textDim); g.drawText (String ((int) hz), (int) a.getX() + 2, (int) yOf (hz) - 6, 30, 12, Justification::right);
        }
        auto draw = [&] (const ka::Report& rep, Colour col)
        {
            if (! rep.valid) return;
            g.setColour (col);
            Path p; bool pen = false;
            for (size_t k = 0; k < rep.pitch.size(); ++k)
            {
                const float t = (float) k * 5.f; if (t > tMax) break;
                const float hz = rep.pitch[k];
                if (hz <= 0.f || rep.pitchConf[k] < 0.6f) { pen = false; continue; }
                const float x = r.getX() + r.getWidth() * t / tMax, y = yOf (hz);
                if (! pen) { p.startNewSubPath (x, y); pen = true; } else p.lineTo (x, y);
            }
            g.strokePath (p, PathStrokeType (1.8f));
            if (rep.endHz > 0.f)
            {
                const float y = yOf (rep.endHz);
                const float dash[] { 4.f, 3.f }; g.drawDashedLine (Line<float> (r.getX(), y, r.getRight(), y), dash, 2, 0.8f);
                g.drawText (ka::noteText (rep.endHz), (int) r.getRight() - 90, (int) y - 13, 88, 12, Justification::right);
            }
        };
        draw (ref, c.silver);
        draw (mine, c.red);
    }

    void AnalysisPage::paintSpectrum (Graphics& g, Rectangle<int> a)
    {
        frame (g, a, "SPECTRUM"); legend (g, a, ref.valid, mine.valid);
        auto r = a.reduced (8, 18).toFloat();
        const auto& c = palette();
        auto xOf = [&] (float hz) { return r.getX() + r.getWidth() * std::log (hz / 20.f) / std::log (1000.f); };
        auto yOf = [&] (float db) { return r.getY() + r.getHeight() * jlimit (0.f, 1.f, -db / 60.f); };
        g.setFont (FontOptions (10.f));
        for (float hz : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
        {
            g.setColour (c.grid); g.drawVerticalLine ((int) xOf (hz), r.getY(), r.getBottom());
            g.setColour (c.textDim); g.drawText (hz >= 1000.f ? String ((int) hz / 1000) + "k" : String ((int) hz), (int) xOf (hz) + 2, (int) r.getBottom() - 12, 30, 12, Justification::left);
        }
        auto draw = [&] (const ka::Report& rep, Colour col, bool fill)
        {
            if (! rep.valid) return;
            Path p; p.startNewSubPath (r.getX(), r.getBottom());
            for (int k = 0; k < ka::specBins; ++k) p.lineTo (xOf (ka::specHz (k)), yOf (rep.spec[(size_t) k]));
            p.lineTo (r.getRight(), r.getBottom());
            if (fill) { g.setColour (col.withAlpha (0.25f)); g.fillPath (p); }
            g.setColour (col); g.strokePath (p, PathStrokeType (1.4f));
        };
        draw (ref, c.silver, true);
        draw (mine, c.red, false);
    }

    void AnalysisPage::paintBands (Graphics& g, Rectangle<int> a)
    {
        frame (g, a, "ENERGY PER BAND (dB of the whole kick)");
        auto r = a.reduced (8, 18);
        const auto& c = palette();
        const int nb = ka::numBands, w = r.getWidth() / nb;
        g.setFont (FontOptions (10.f));
        for (int b = 0; b < nb; ++b)
        {
            auto col = r.withX (r.getX() + b * w).withWidth (w).reduced (6, 0);
            auto label = col.removeFromBottom (12);
            g.setColour (c.textDim); g.drawText (ka::bandNames()[b], label, Justification::centred);
            auto bar = [&] (const ka::Report& rep, Rectangle<int> slot, Colour cc)
            {
                if (! rep.valid) return;
                const float v = jlimit (0.f, 1.f, 1.f + rep.bandDb[(size_t) b] / 40.f);
                g.setColour (cc); g.fillRect (slot.withTop (slot.getBottom() - roundToInt (v * (slot.getHeight() - 12))));
                g.setColour (c.silver); g.drawText (String (rep.bandDb[(size_t) b], 1), slot.withHeight (11).withY (slot.getBottom() - roundToInt (v * (slot.getHeight() - 12)) - 12), Justification::centred);
            };
            auto two = col.reduced (jmax (0, (col.getWidth() - 60) / 2), 0);
            bar (ref, two.removeFromLeft (two.getWidth() / 2).reduced (2, 0), c.silver.withAlpha (0.6f));
            bar (mine, two.reduced (2, 0), c.red);
        }
    }

    void AnalysisPage::paintTable (Graphics& g, Rectangle<int> a)
    {
        frame (g, a, "");
        const auto& c = palette();
        auto r = a.reduced (8, 4);
        const int cw = (r.getWidth() - 130) / 3;
        auto row = [&] (const String& l, const String& x, const String& y, const String& d, bool head = false)
        {
            auto line = r.removeFromTop (head ? 20 : 17);
            g.setFont (FontOptions (head ? 10.f : 12.f, head ? Font::bold : Font::plain));
            g.setColour (c.textDim); g.drawText (l, line.removeFromLeft (130), Justification::centredLeft);
            g.setColour (c.silver); g.drawText (x, line.removeFromLeft (cw), Justification::centredRight);
            g.setColour (head ? c.textDim : c.red); g.drawText (y, line.removeFromLeft (cw), Justification::centredRight);
            g.setColour (c.textDim); g.drawText (d, line, Justification::centredRight);
        };
        row ("", "REFERENCE", "4GE", "DIFF", true);
        auto fmt = [] (float v, int dec) { return dec == 0 ? String (roundToInt (v)) : String (v, dec); };
        auto num = [fmt] (const ka::Report& rep, float v, int dec, const String& unit) { return rep.valid ? fmt (v, dec) + unit : String ("-"); };
        auto line = [&] (const String& l, float x, float y, int dec, const String& unit, bool diff = true)
        {
            row (l, num (ref, x, dec, unit), num (mine, y, dec, unit), diff && ref.valid && mine.valid ? (y - x >= 0 ? "+" : "") + fmt (y - x, dec) : String());
        };
        line ("LENGTH", ref.lengthMs, mine.lengthMs, 0, " ms");
        row ("TAIL NOTE", ref.valid ? ka::noteText (ref.endHz) : "-", mine.valid ? ka::noteText (mine.endHz) : "-",
             ref.endHz > 0.f && mine.endHz > 0.f ? String (12.f * std::log2 (mine.endHz / ref.endHz), 1) + " st" : String());
        line ("TAIL PITCH", ref.endHz, mine.endHz, 1, " Hz");
        line ("START PITCH", ref.startHz, mine.startHz, 0, " Hz");
        line ("PITCH DROP", ref.dropMs, mine.dropMs, 0, " ms");
        line ("TOK (HELD)", ref.tokMs, mine.tokMs, 0, " ms");
        line ("ATTACK", ref.attackMs, mine.attackMs, 1, " ms");
        line ("TRANSIENT", ref.transientDb, mine.transientDb, 1, " dB");
        line ("SUB < 100 Hz", ref.subShare, mine.subShare, 0, " %");
        line ("PEAK", ref.m.samplePeakDb, mine.m.samplePeakDb, 1, " dB");
        line ("RMS", ref.m.rmsDb, mine.m.rmsDb, 1, " dB");
        row ("LUFS", ref.valid && ref.m.lufsValid ? String (ref.m.lufs, 1) : "-", mine.valid && mine.m.lufsValid ? String (mine.m.lufs, 1) : "-", "");
        line ("CREST", ref.crestDb, mine.crestDb, 1, " dB");
        line ("CORRELATION", ref.corr, mine.corr, 2, "");
        line ("LOW CORR < 150", ref.lowCorr, mine.lowCorr, 2, "");
        line ("SIDE", ref.sideDb, mine.sideDb, 1, " dB");
    }

    void AnalysisPage::paintFeatures (Graphics& g, Rectangle<int> a)
    {
        frame (g, a, "CHARACTER");
        const auto& c = palette();
        auto r = a.reduced (8, 18);
        auto col = [&] (const ka::Report& rep, Rectangle<int> area, Colour cc, const String& name)
        {
            g.setFont (FontOptions (10.f, Font::bold)); g.setColour (cc); g.drawText (name, area.removeFromTop (13), Justification::left, true);
            g.setFont (FontOptions (11.f));
            if (! rep.valid) { g.setColour (c.textDim); g.drawText ("-", area.removeFromTop (14), Justification::left); return; }
            for (auto& f : rep.features) { if (area.getHeight() < 13) break; g.setColour (c.silver); g.drawFittedText (f, area.removeFromTop (14), Justification::left, 1, 0.8f); }
        };
        auto left = r.removeFromLeft (r.getWidth() / 2);
        col (ref, left, c.silver, refName.isNotEmpty() ? "REFERENCE: " + refName : "REFERENCE");
        col (mine, r, c.red, "4GE");
    }
}
