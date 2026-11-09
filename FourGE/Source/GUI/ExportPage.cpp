#include "ExportPage.h"
#include "Palette.h"

using namespace juce;

namespace fge::gui
{
    namespace { int idOf (const String& id) { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; } }

    ComboBox* ExportPage::addCombo (const String& name, const StringArray& items, const String& tip)
    {
        auto c = std::make_unique<ComboBox>(); c->addItemList (items, 1); c->setTooltip (tip);
        c->onChange = [this] { if (! loading) storeSettings(); };
        auto* r = c.get(); addAndMakeVisible (r); named[name] = r; captions[r] = name; own.push_back (std::move (c)); return r;
    }
    Slider* ExportPage::addSlider (const String& name, double lo, double hi, double step, const String& suffix, const String& tip)
    {
        auto s = std::make_unique<Slider> (Slider::LinearBar, Slider::TextBoxLeft);
        s->setRange (lo, hi, step); s->setTextValueSuffix (suffix); s->setTooltip (tip);
        s->setColour (Slider::trackColourId, palette().control.brighter (0.25f)); s->setColour (Slider::textBoxTextColourId, palette().silver);
        s->onValueChange = [this] { if (! loading) storeSettings(); };
        auto* r = s.get(); addAndMakeVisible (r); named[name] = r; captions[r] = name; own.push_back (std::move (s)); return r;
    }
    ToggleButton* ExportPage::addToggle (const String& name, const String& tip)
    {
        auto t = std::make_unique<ToggleButton> (name); t->setTooltip (tip);
        t->onClick = [this] { if (! loading) storeSettings(); };
        auto* r = t.get(); addAndMakeVisible (r); named[name] = r; own.push_back (std::move (t)); return r;
    }
    Component* ExportPage::findControl (const String& id) { auto it = named.find (id); return it == named.end() ? nullptr : it->second; }

    ExportPage::ExportPage (GeProcessor& p, doomsday::flat::FlatLookAndFeel& l) : proc (p), lnf (l)
    {
        using namespace fge::xp;
        addCombo ("SOURCE", sourceNames(), "What is exported: the whole kick, one group (its buses with every processor; the master processing is included) or one layer / sample alone.");
        addCombo ("LENGTH", lengthNames(), "AUTO = at least the kick LENGTH, then until the tail is silent; FIXED = exactly this long; UNTIL SILENCE = until the sound drops under the threshold.");
        addSlider ("FIXED LENGTH", 10, 20000, 1, " ms", "FIXED: the exact length.");
        addSlider ("PRE-ROLL", 0, 500, 1, " ms", "Silence before the hit.");
        addSlider ("POST-ROLL", 0, 2000, 1, " ms", "Extra time after the sound ends (so nothing is cut).");
        addSlider ("SILENCE THRESHOLD", -120, -30, 1, " dB", "Under this level the sound counts as silent (a sound 6 dB above ends the silence - hysteresis).");
        addSlider ("MIN SILENCE", 5, 1000, 1, " ms", "How long it must stay silent before the end is decided.");
        addSlider ("MAX LENGTH", 100, 60000, 10, " ms", "Safety: never longer than this.");
        addToggle ("TRIM SILENCE", "Cut the silence before the hit and at the end.");
        addCombo ("FORMAT", formatNames(), "WAV, AIFF or FLAC.");
        addCombo ("BIT DEPTH", bitNames(), "16 / 24-bit PCM or 32-bit float (float never clips above 0 dBFS).");
        addCombo ("SAMPLE RATE", rateNames(), "The kick is rendered directly at this rate (no resampling); HOST RATE = the project's rate. The project is not changed.");
        addCombo ("DITHER", ditherNames(), "Only for 16 / 24-bit files, once at the very end. OFF by default.");
        addCombo ("NORMALIZE", normNames(), "OFF = the level as it is. PEAK / LUFS = a plain gain to the target - never a hidden limiter.");
        addSlider ("PEAK TARGET", -30, 0, 0.1, " dBFS", "PEAK normalization target.");
        addSlider ("LUFS TARGET", -30, 0, 0.1, " LUFS", "LUFS normalization target (integrated, needs >= 400 ms).");
        addSlider ("TRUE PEAK CEILING", -6, 0, 0.1, " dBTP", "The gain never pushes the true peak above this - if the target needs more, the result tells you a limiter would be needed.");
        addCombo ("SEED", seedNames(), "PRESET SEED = exactly what the plugin plays; FIXED = seed 0; NEW = a new variation every export (written in the name / history); CUSTOM = your number.");
        addSlider ("CUSTOM SEED", 1, 99999, 1, "", "The CUSTOM seed: the same number = the same kick.");
        addToggle ("F# -> Fs", "Write F# as Fs in file names (some programs dislike '#').");
        addToggle ("OVERWRITE", "Allow replacing existing files (off: a new VERSION number is used instead).");
        addCombo ("BATCH PRESETS", StringArray { "CURRENT SOUND", "ALL FACTORY PRESETS" }, "BATCH: the current sound or every factory preset.");
        addSlider ("VARIANTS", 1, 16, 1, " seeds", "BATCH: how many seed variants of each kick.");
        const char* bs[7] { "FULL", "PUNCH", "BODY", "TAIL", "SUB", "TEXTURE", "CUSTOM" };
        for (int i = 0; i < 7; ++i)
        {
            batchSrc[(size_t) i].setButtonText (bs[i]); batchSrc[(size_t) i].setClickingTogglesState (true);
            batchSrc[(size_t) i].setTooltip ("BATCH: export the " + String (bs[i]) + (i == 0 ? " kick." : " group."));
            batchSrc[(size_t) i].onClick = [this] { if (! loading) storeSettings(); };
            batchSrc[(size_t) i].setColour (TextButton::buttonOnColourId, palette().red);
            addAndMakeVisible (batchSrc[(size_t) i]);
        }
        nameTmpl.setTooltip ("NAME TEMPLATE: {PRESET} {STYLE} {NOTE} {BPM} {GROUP} {VARIANT} {VERSION} {SEED} {DATE}");
        nameTmpl.onTextChange = [this] { if (! loading) storeSettings(); };
        addAndMakeVisible (nameTmpl);
        folderBtn.setTooltip ("Where the files go (default: Documents / 4GE_EXPORT).");
        folderBtn.onClick = [this]
        {
            auto ch = std::make_shared<FileChooser> ("Export folder", proc.exportSettings().folderFile());
            SafePointer<ExportPage> self (this);
            ch->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [self, ch] (const FileChooser& fc)
            { if (self != nullptr && fc.getResult().isDirectory()) { auto s = self->proc.exportSettings(); s.folder = fc.getResult().getFullPathName(); self->proc.setExportSettings (s); self->repaint(); } });
        };
        previewBtn.setTooltip ("Render, measure and listen - nothing is written. Shows the predicted normalization gain.");
        exportBtn.setTooltip ("Render and write the file (a temporary file first - a cancelled / failed export never leaves a broken file).");
        batchBtn.setTooltip ("Export every chosen source x preset x variant: 4GE_EXPORT / STYLE / KICK_nn / KICK_<GROUP>.wav");
        cancelBtn.setTooltip ("Stop the export (the unfinished file is deleted).");
        diceBtn.setTooltip ("Give this preset a new SEED (noise, random sources, the sequencer probability) - you hear it at once; UNDO goes back.");
        previewBtn.onClick = [this] { previewNow(); };
        exportBtn.onClick = [this] { exportNow (false); };
        batchBtn.onClick = [this] { exportNow (true); };
        cancelBtn.onClick = [this] { cancel(); proc.samples().stopPreview(); };
        diceBtn.onClick = [this] { proc.setPresetSeed ((uint32_t) Random::getSystemRandom().nextInt (99999) + 1u); repaint(); };
        resetBtn.onClick = [this] { proc.meter.requestReset(); };
        resetBtn.setTooltip ("Reset the max / hold / clip / true peak / integrated LUFS.");
        reBtn.setTooltip ("Export again with the settings of the selected history row (the current sound, the same seed).");
        reBtn.onClick = [this] { reExport (histList.getSelectedRow()); };
        openBtn.onClick = [this] { if (lastOut.existsAsFile()) lastOut.revealToUser(); };
        openBtn.setTooltip ("Show the last exported file in the file manager.");
        for (auto* b : { &folderBtn, &previewBtn, &exportBtn, &batchBtn, &cancelBtn, &diceBtn, &resetBtn, &reBtn, &openBtn }) addAndMakeVisible (b);
        histList.setRowHeight (18); histList.setColour (ListBox::backgroundColourId, palette().ground);
        addAndMakeVisible (histList);
        // MASTER OUTPUT (real parameters)
        auto k = std::make_unique<doomsday::flat::Knob> (proc.apvts, "core_master", "MASTER", palette().red, false, 40);
        k->slider.setTooltip (fge::tooltip (P::coreMaster)); named["core_master"] = k.get(); addAndMakeVisible (*k); masterCtl.push_back (std::move (k));
        for (auto& [id, txt] : std::vector<std::pair<const char*, const char*>> { { "master_mute", "MUTE" }, { "master_bypass", "BYPASS" }, { "master_safety_clip", "SAFETY CLIP" }, { "master_safety_limiter", "SAFETY LIMITER" } })
        {
            auto t = std::make_unique<doomsday::flat::Toggle> (proc.apvts, id, txt, doomsday::flat::Toggle::Pill, palette().red);
            t->setTooltip (fge::tooltip (idOf (id))); named[id] = t.get(); addAndMakeVisible (*t); masterCtl.push_back (std::move (t));
        }
        auto seg = std::make_unique<doomsday::flat::Segmented> (proc.apvts, "master_output_mode", StringArray { "STEREO", "MONO" }, palette().red);
        seg->setTooltip (fge::tooltip (P::masterOutMode)); named["master_output_mode"] = seg.get(); addAndMakeVisible (*seg); masterCtl.push_back (std::move (seg));
        loadSettings();
        hist = proc.exportHistory.list(); histList.updateContent();
        startTimerHz (20);
    }
    ExportPage::~ExportPage() { *alive = false; stopTimer(); }

    void ExportPage::loadSettings()
    {
        const auto s = proc.exportSettings();
        loading = true;
        auto cb = [this] (const char* n, int v) { if (auto* c = dynamic_cast<ComboBox*> (named[n])) c->setSelectedItemIndex (v, dontSendNotification); };
        auto sl = [this] (const char* n, double v) { if (auto* c = dynamic_cast<Slider*> (named[n])) c->setValue (v, dontSendNotification); };
        auto tg = [this] (const char* n, bool v) { if (auto* c = dynamic_cast<ToggleButton*> (named[n])) c->setToggleState (v, dontSendNotification); };
        cb ("SOURCE", s.source); cb ("LENGTH", s.lengthMode); cb ("FORMAT", s.format); cb ("BIT DEPTH", s.bits); cb ("SAMPLE RATE", s.rate); cb ("DITHER", s.dither);
        cb ("NORMALIZE", s.norm); cb ("SEED", s.seedMode); cb ("BATCH PRESETS", s.batchPresets);
        sl ("FIXED LENGTH", s.fixedMs); sl ("PRE-ROLL", s.preRollMs); sl ("POST-ROLL", s.postRollMs); sl ("SILENCE THRESHOLD", s.silenceDb); sl ("MIN SILENCE", s.minSilenceMs);
        sl ("MAX LENGTH", s.maxMs); sl ("PEAK TARGET", s.peakTarget); sl ("LUFS TARGET", s.lufsTarget); sl ("TRUE PEAK CEILING", s.tpCeiling); sl ("CUSTOM SEED", s.customSeed);
        sl ("VARIANTS", s.batchVariants);
        tg ("TRIM SILENCE", s.trim); tg ("F# -> Fs", s.replaceSharp); tg ("OVERWRITE", s.overwrite);
        for (int i = 0; i < 7; ++i) batchSrc[(size_t) i].setToggleState ((s.batchSources >> i) & 1u, dontSendNotification);
        nameTmpl.setText (s.nameTemplate, false);
        loading = false;
    }
    void ExportPage::storeSettings()
    {
        auto s = proc.exportSettings();
        auto cb = [this] (const char* n) { auto* c = dynamic_cast<ComboBox*> (named[n]); return c != nullptr ? jmax (0, c->getSelectedItemIndex()) : 0; };
        auto sl = [this] (const char* n) { auto* c = dynamic_cast<Slider*> (named[n]); return c != nullptr ? (float) c->getValue() : 0.f; };
        auto tg = [this] (const char* n) { auto* c = dynamic_cast<ToggleButton*> (named[n]); return c != nullptr && c->getToggleState(); };
        s.source = cb ("SOURCE"); s.lengthMode = cb ("LENGTH"); s.format = cb ("FORMAT"); s.bits = cb ("BIT DEPTH"); s.rate = cb ("SAMPLE RATE"); s.dither = cb ("DITHER");
        s.norm = cb ("NORMALIZE"); s.seedMode = cb ("SEED"); s.batchPresets = cb ("BATCH PRESETS");
        s.fixedMs = sl ("FIXED LENGTH"); s.preRollMs = sl ("PRE-ROLL"); s.postRollMs = sl ("POST-ROLL"); s.silenceDb = sl ("SILENCE THRESHOLD"); s.minSilenceMs = sl ("MIN SILENCE");
        s.maxMs = sl ("MAX LENGTH"); s.peakTarget = sl ("PEAK TARGET"); s.lufsTarget = sl ("LUFS TARGET"); s.tpCeiling = sl ("TRUE PEAK CEILING"); s.customSeed = (uint32_t) sl ("CUSTOM SEED");
        s.batchVariants = (int) sl ("VARIANTS");
        s.trim = tg ("TRIM SILENCE"); s.replaceSharp = tg ("F# -> Fs"); s.overwrite = tg ("OVERWRITE");
        s.batchSources = 0; for (int i = 0; i < 7; ++i) if (batchSrc[(size_t) i].getToggleState()) s.batchSources |= 1u << i;
        s.nameTemplate = nameTmpl.getText();
        proc.setExportSettings (s);
        repaint();
    }

    void ExportPage::exportNow (bool batch)
    {
        if (busy()) return;
        String err;
        auto jobs = proc.makeExportJobs (proc.exportSettings(), batch, &err);
        if (jobs.empty()) { result = "nothing to export: " + err; repaint(); return; }
        result = "exporting " + String ((int) jobs.size()) + " file(s)...";
        SafePointer<ExportPage> self (this);
        proc.exporter.start (std::move (jobs), [self] (const std::vector<fge::xp::JobResult>& r) { if (self != nullptr) self->showResults (r); });
        repaint();
    }
    void ExportPage::showResults (const std::vector<fge::xp::JobResult>& r)
    {
        int ok = 0; String err, note;
        for (auto& x : r) { if (x.ok) { ++ok; lastOut = x.file; } else if (x.error.isNotEmpty() && err.isEmpty()) err = x.error; if (x.note.isNotEmpty() && note.isEmpty()) note = x.note; }
        if (r.size() == 1 && r[0].ok)
        {
            const auto& x = r[0];
            result = x.file.getFullPathName() + "\n" + fge::xp::formatNames()[proc.exportSettings().format] + "  " + String (x.bits) + "-bit  " + String (x.rate / 1000.0, 1) + " kHz  "
                     + (x.channels == 1 ? "MONO  " : "STEREO  ") + String (x.m.seconds, 3) + " s   PEAK " + String (x.m.samplePeakDb, 2) + " dBFS   TRUE PEAK " + String (x.m.truePeakDb, 2)
                     + " dBTP   " + (x.m.lufsValid ? "LUFS " + String (x.m.lufs, 1) : String ("LUFS -")) + "   SEED " + String ((juce::int64) x.seed)
                     + (x.norm.gainDb != 0.f ? "   GAIN " + String (x.norm.gainDb, 2) + " dB" : String()) + (note.isNotEmpty() ? "\n" + note : String()) + "\n(drag this box into FL)";
        }
        else result = String (ok) + " of " + String ((int) r.size()) + " file(s) written" + (err.isNotEmpty() ? "  -  " + err : String()) + (note.isNotEmpty() ? "\n" + note : String());
        hist = proc.exportHistory.list(); histList.updateContent(); histList.repaint();
        repaint();
    }
    void ExportPage::previewNow()
    {
        if (busy()) return;
        auto jobs = proc.makeExportJobs (proc.exportSettings(), false);
        if (jobs.empty()) return;
        previewing.store (true);
        result = "preview: rendering...";
        auto job = std::make_shared<fge::xp::Job> (std::move (jobs[0]));
        auto life = alive; const double host = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
        SafePointer<ExportPage> self (this);
        GeProcessor* pr = &proc;
        Thread::launch ([job, life, host, self, pr]
        {
            juce::AudioBuffer<float> audio;
            auto r = fge::xp::process (*job, nullptr, nullptr, false, &audio);
            if (! *life) return;
            if (r.ok) pr->samples().previewAudio (audio, job->spec.rate, host);
            MessageManager::callAsync ([self, r, life]
            {
                if (! *life || self == nullptr) return;
                self->previewing.store (false);
                self->result = "PREVIEW (nothing written): " + String (r.m.seconds, 3) + " s   PEAK " + String (r.m.samplePeakDb, 2) + " dBFS   TRUE PEAK " + String (r.m.truePeakDb, 2)
                               + " dBTP   " + (r.m.lufsValid ? "LUFS " + String (r.m.lufs, 1) : String ("LUFS -")) + "   RMS " + String (r.m.rmsDb, 1) + " dB"
                               + (r.norm.gainDb != 0.f || r.norm.limitedByCeiling ? "\npredicted normalization gain " + String (r.norm.gainDb, 2) + " dB" : String())
                               + (r.note.isNotEmpty() ? "\n" + r.note : String()) + (r.m.dc ? "\nDC OFFSET in the render" : String());
                self->repaint();
            });
        });
        repaint();
    }
    void ExportPage::reExport (int row)
    {
        if (! isPositiveAndBelow (row, (int) hist.size()) || busy()) return;
        if (auto x = parseXML (hist[(size_t) row].settings.toString()))
        {
            auto t = ValueTree::fromXml (*x);
            auto s = fge::xp::Settings::fromTree (t);
            s.seedMode = fge::xp::SeedCustom; s.customSeed = (uint32_t) (juce::int64) t.getProperty ("seed", 0);
            if (s.customSeed == 0) s.seedMode = fge::xp::SeedFixed;
            proc.setExportSettings (s); loadSettings();
            exportNow (false);
        }
    }

    void ExportPage::mouseDrag (const MouseEvent& e)
    {
        if (resultArea.contains (e.getMouseDownPosition()) && lastOut.existsAsFile() && e.getDistanceFromDragStart() > 6)
            DragAndDropContainer::performExternalDragDropOfFiles ({ lastOut.getFullPathName() }, false, this);
    }

    void ExportPage::timerCallback()
    {
        if (! isShowing()) return;
        repaint (meterArea.expanded (4)); repaint (resultArea.expanded (4)); repaint (masterArea.expanded (4));
        const bool b = busy();
        exportBtn.setEnabled (! b); batchBtn.setEnabled (! b); previewBtn.setEnabled (! b); cancelBtn.setEnabled (b || proc.samples().previewing());
        // controls that do nothing in the current setup are dimmed
        const auto s = proc.exportSettings();
        auto en = [this] (const char* n, bool e) { if (auto* c = named[n]) { c->setEnabled (e); c->setAlpha (e ? 1.f : 0.4f); } };
        en ("FIXED LENGTH", s.lengthMode == fge::dsp::xport::LenFixed);
        for (auto* n : { "SILENCE THRESHOLD", "MIN SILENCE" }) en (n, s.lengthMode != fge::dsp::xport::LenFixed || s.trim);
        en ("PEAK TARGET", s.norm == fge::dsp::xport::NormPeak); en ("LUFS TARGET", s.norm == fge::dsp::xport::NormLufs); en ("TRUE PEAK CEILING", s.norm != 0);
        en ("DITHER", s.bits < 2); en ("CUSTOM SEED", s.seedMode == fge::xp::SeedCustom);
    }

    void ExportPage::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
    {
        if (! isPositiveAndBelow (row, (int) hist.size())) return;
        const auto& c = palette(); const auto& e = hist[(size_t) row];
        if (selected) { g.setColour (c.control.brighter (0.2f)); g.fillRect (0, 0, w, h); }
        g.setFont (FontOptions (11.f)); g.setColour (c.silver);
        g.drawText (File (e.file).getFileName(), 4, 0, w / 2, h, Justification::centredLeft, true);
        g.setColour (c.textDim);
        g.drawText (e.when.formatted ("%d.%m %H:%M") + "  " + e.format + " " + String (e.bits) + "/" + String (e.rate / 1000.0, 1) + "k  " + String (e.seconds, 2) + " s  TP "
                    + String (e.truePeak, 1) + (e.lufsValid ? "  " + String (e.lufs, 1) + " LUFS" : String()) + "  seed " + String ((juce::int64) e.seed),
                    w / 2, 0, w / 2 - 4, h, Justification::centredRight, true);
    }

    void ExportPage::paint (Graphics& g)
    {
        const auto& c = palette();
        auto panel = [&] (Rectangle<int> r, const String& t) { g.setColour (c.panel); g.fillRoundedRectangle (r.toFloat(), 6.f); g.setColour (c.textDim); g.setFont (FontOptions (11.f, Font::bold)); g.drawText (t, r.getX() + 10, r.getY() + 4, r.getWidth() - 20, 14, Justification::left); };
        panel (leftArea, "EXPORT  -  SOURCE / LENGTH / FORMAT / QUALITY / NORMALIZATION / SEED / DESTINATION / BATCH");
        panel (masterArea, "MASTER OUTPUT");
        panel (meterArea, "MASTER METERS");
        panel (histArea, "EXPORT HISTORY");
        g.setFont (FontOptions (10.f)); g.setColour (c.textDim);
        for (auto& [comp, cap] : captions) if (comp->isVisible()) g.drawText (cap, comp->getX(), comp->getY() - 12, comp->getWidth(), 11, Justification::left);
        const auto s = proc.exportSettings();
        // destination preview
        fge::xp::NameParts np; np.preset = proc.currentPresetName(); np.group = s.source == 0 ? String ("FULL") : fge::xp::sourceNames()[s.source]; np.note = proc.auditionNote; np.bpm = 150;
        g.setFont (FontOptions (11.f)); g.setColour (c.silver);
        g.drawText ("FOLDER: " + s.folderFile().getFullPathName(), folderBtn.getRight() + 8, folderBtn.getY(), leftArea.getRight() - folderBtn.getRight() - 16, folderBtn.getHeight(), Justification::centredLeft, true);
        g.setColour (c.textDim);
        g.drawText ("NAME: " + fge::xp::makeName (s.nameTemplate, np, s.replaceSharp) + fge::xp::extOf (s.format), nameTmpl.getX(), nameTmpl.getBottom() + 2, nameTmpl.getWidth(), 14, Justification::centredLeft, true);
        g.drawText ("PRESET SEED " + String ((juce::int64) proc.presetSeed()), diceBtn.getRight() + 8, diceBtn.getY(), 160, diceBtn.getHeight(), Justification::centredLeft);
        // the group export note
        if (s.source > 0 && s.source < 7)
            g.drawText ("A group export includes the master processing (MASTER CLIP, GLOBAL distortion, MASTER STEREO, MASTER OUTPUT dynamics).", leftArea.getX() + 10,
                        named["SOURCE"]->getBottom() + 2, leftArea.getWidth() - 20, 14, Justification::centredLeft, true);
        // progress + result
        g.setColour (c.ground); g.fillRoundedRectangle (resultArea.toFloat(), 4.f);
        if (proc.exporter.busy())
        {
            const float pr = proc.exporter.progress.load();
            g.setColour (c.red.withAlpha (0.5f)); g.fillRect (resultArea.withWidth ((int) (resultArea.getWidth() * pr)).reduced (2));
            g.setColour (c.silver); g.drawText ("EXPORTING " + String (proc.exporter.current.load() + 1) + " / " + String (proc.exporter.total.load()), resultArea.reduced (8, 4), Justification::topLeft);
        }
        else { g.setColour (lastOut.existsAsFile() ? c.silver : c.textDim); g.setFont (FontOptions (11.f)); g.drawFittedText (result.isEmpty() ? String ("PREVIEW = listen + measure, EXPORT = write the file.") : result, resultArea.reduced (8, 4), Justification::topLeft, 5); }
        // MASTER OUTPUT warnings
        g.setFont (FontOptions (11.f));
        auto on = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; };
        String warn;
        if (on ("master_mute")) warn << "MUTED  ";
        if (on ("master_bypass")) warn << "BYPASS: master clip / stereo / output dynamics / safety skipped  ";
        if (on ("master_safety_clip")) warn << "SAFETY CLIP ON (it shapes the peaks)  ";
        if (on ("master_safety_limiter")) warn << (proc.getEngine().safetyActive.exchange (false) ? "SAFETY LIMITER REDUCING  " : "SAFETY LIMITER ON  ");
        g.setColour (warn.isEmpty() ? c.textDim : c.hot);
        g.drawFittedText (warn.isEmpty() ? String ("no safety processing (the export never adds any by itself)") : warn, masterArea.getX() + 10, masterArea.getBottom() - 34, masterArea.getWidth() - 20, 30, Justification::left, 2);
        // METERS
        auto& m = proc.meter;
        auto r = meterArea.reduced (10).withTrimmedTop (18);
        auto bar = [&] (const String& name, float lin, float hold, bool clipped)
        {
            auto row = r.removeFromTop (16); r.removeFromTop (4);
            g.setColour (c.textDim); g.drawText (name, row.removeFromLeft (24), Justification::centredLeft);
            auto bx = row.withTrimmedRight (60).toFloat();
            const auto pos = [&] (float v) { return bx.getX() + bx.getWidth() * jlimit (0.f, 1.f, (Decibels::gainToDecibels (v, -60.f) + 60.f) / 63.f); };
            g.setColour (c.ground); g.fillRect (bx);
            g.setColour (lin > 1.f ? c.red : Colour (0xff5fbf8f)); g.fillRect (Rectangle<float>::leftTopRightBottom (bx.getX(), bx.getY() + 2, pos (lin), bx.getBottom() - 2));
            g.setColour (c.silver); g.fillRect (pos (hold) - 1.f, bx.getY(), 2.f, bx.getHeight());
            g.setColour (c.grid); g.fillRect (pos (1.f), bx.getY(), 1.f, bx.getHeight());
            g.setColour (clipped ? c.red : c.textDim); g.drawText (clipped ? "CLIP" : String (Decibels::gainToDecibels (lin, -60.f), 1), row.removeFromRight (56), Justification::centredRight);
        };
        const bool clip = m.clip.load();
        bar ("L", m.peakL.load(), m.peakHold.load(), clip); bar ("R", m.peakR.load(), m.peakHold.load(), clip);
        auto line = [&] (const String& a, const String& b, Colour col) { auto row = r.removeFromTop (16); g.setColour (c.textDim); g.drawText (a, row.removeFromLeft (150), Justification::centredLeft); g.setColour (col); g.drawText (b, row, Justification::centredLeft); };
        const float tp = Decibels::gainToDecibels (m.truePeak.load(), -100.f);
        line ("MAX PEAK", String (Decibels::gainToDecibels (m.peakMax.load(), -100.f), 2) + " dBFS", c.silver);
        line ("TRUE PEAK", String (tp, 2) + " dBTP", tp > 0.f ? c.red : c.silver);
        line ("RMS (300 ms)", String (Decibels::gainToDecibels (m.rms.load(), -100.f), 1) + " dB", c.silver);
        line ("LUFS M / S / I", String (m.lufsM.load(), 1) + " / " + String (m.lufsS.load(), 1) + " / " + (m.lufsI.load() > -69.9f ? String (m.lufsI.load(), 1) : String ("-")), c.silver);
        line ("DC OFFSET", m.dcWarning() ? "DETECTED " + String (m.dcOffset.load(), 4) + " (remove it with an EQ low cut / DC on the SMP tools)" : String ("none"), m.dcWarning() ? c.hot : c.textDim);
    }

    void ExportPage::resized()
    {
        const int W = getWidth(), H = getHeight();
        leftArea = { 0, 0, 760, H };
        masterArea = { 776, 0, W - 776, 160 };
        meterArea = { 776, 170, W - 776, 200 };
        histArea = { 776, 380, W - 776, H - 380 };
        auto at = [this] (const char* n, int x, int y, int w, int h = 22) { if (auto* c = named[n]) c->setBounds (x, y, w, h); };
        const int x0 = 12, cw = 236, gap = 12;
        int y = 38;
        at ("SOURCE", x0, y, cw); at ("LENGTH", x0 + cw + gap, y, 150); at ("FIXED LENGTH", x0 + cw + gap + 162, y, 160); at ("TRIM SILENCE", x0 + cw + gap + 334, y, 140);
        y += 52;
        at ("PRE-ROLL", x0, y, 140); at ("POST-ROLL", x0 + 150, y, 140); at ("SILENCE THRESHOLD", x0 + 300, y, 140); at ("MIN SILENCE", x0 + 450, y, 130); at ("MAX LENGTH", x0 + 590, y, 140);
        y += 50;
        at ("FORMAT", x0, y, 110); at ("BIT DEPTH", x0 + 120, y, 140); at ("SAMPLE RATE", x0 + 270, y, 120); at ("DITHER", x0 + 400, y, 140);
        y += 50;
        at ("NORMALIZE", x0, y, 110); at ("PEAK TARGET", x0 + 120, y, 150); at ("LUFS TARGET", x0 + 280, y, 150); at ("TRUE PEAK CEILING", x0 + 440, y, 160);
        y += 50;
        at ("SEED", x0, y, 160); at ("CUSTOM SEED", x0 + 170, y, 140); diceBtn.setBounds (x0 + 320, y, 140, 22);
        y += 40;
        folderBtn.setBounds (x0, y, 100, 22);
        y += 34;
        nameTmpl.setBounds (x0, y, 470, 22); at ("F# -> Fs", x0 + 480, y, 100); at ("OVERWRITE", x0 + 590, y, 110);
        y += 50;
        for (int i = 0; i < 7; ++i) batchSrc[(size_t) i].setBounds (x0 + i * 72, y, 68, 22);
        at ("BATCH PRESETS", x0 + 7 * 72 + 6, y, 120); at ("VARIANTS", x0 + 7 * 72 + 134, y, 100);
        y += 40;
        previewBtn.setBounds (x0, y, 100, 28); exportBtn.setBounds (x0 + 110, y, 100, 28); batchBtn.setBounds (x0 + 220, y, 130, 28); cancelBtn.setBounds (x0 + 360, y, 90, 28);
        openBtn.setBounds (x0 + 460, y, 100, 28);
        resultArea = { x0, y + 38, 736, H - (y + 38) - 10 };
        // master
        if (auto* k = named["core_master"]) k->setBounds (masterArea.getX() + 10, masterArea.getY() + 22, 80, doomsday::flat::Knob::heightFor (40));
        int mx = masterArea.getX() + 100;
        at ("master_output_mode", mx, masterArea.getY() + 26, 150, 24);
        at ("master_mute", mx, masterArea.getY() + 60, 90); at ("master_bypass", mx + 100, masterArea.getY() + 60, 110);
        at ("master_safety_clip", mx, masterArea.getY() + 90, 130); at ("master_safety_limiter", mx + 140, masterArea.getY() + 90, 160);
        resetBtn.setBounds (meterArea.getRight() - 130, meterArea.getY() + 2, 120, 20);
        histList.setBounds (histArea.reduced (10).withTrimmedTop (18).withTrimmedBottom (30));
        reBtn.setBounds (histArea.getX() + 10, histArea.getBottom() - 32, 120, 24);
    }
}
