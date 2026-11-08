#include "SamplePage.h"
#include "Palette.h"

using namespace juce;

namespace fge::gui
{
    namespace
    {
        int idOf (const String& id) { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; }
        const Colour smpCol { 0xff5fbf8f };                                   // the sample colour (a cool green next to the part colours)
        const Colour loopCol { 0xff36a9d6 };
    }

    // ===================================================================== WAVEFORM EDITOR
    double SampleWave::markerPos (int i) const
    {
        const double a = jmin (raw ("start"), raw ("end")), b = jmax (raw ("start"), raw ("end"));
        if (i == 0) return raw ("start");
        if (i == 1) return raw ("end");
        return a + (double) raw (i == 2 ? "loop_start" : "loop_end") * (b - a);
    }
    void SampleWave::setMarker (int i, double p, bool snap)
    {
        p = jlimit (0.0, 1.0, p);
        if (snap)
            if (auto ov = proc.samples().overview (slot); ov != nullptr && ! ov->transients.empty())
            {
                double best = p, d = 1.0;
                for (float t : ov->transients) if (std::abs (t - p) < d) { d = std::abs (t - p); best = t; }
                if (d * getWidth() / jmax (1.0e-9, v1 - v0) < 24.0) p = best;            // only when a transient is near (24 px)
            }
        if (i <= 1) { proc.setParamInUndoStep (id (i == 0 ? "start" : "end"), (float) p); return; }
        const double a = jmin (raw ("start"), raw ("end")), b = jmax (raw ("start"), raw ("end"));
        if (b - a < 1.0e-6) return;
        proc.setParamInUndoStep (id (i == 2 ? "loop_start" : "loop_end"), (float) jlimit (0.0, 1.0, (p - a) / (b - a)));
    }
    void SampleWave::zoom (double factor, double around)
    {
        const double w = jlimit (0.0005, 1.0, (v1 - v0) * factor);
        double a = around - (around - v0) * w / jmax (1.0e-9, v1 - v0);
        a = jlimit (0.0, 1.0 - w, a);
        v0 = a; v1 = a + w; repaint();
    }
    void SampleWave::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
    {
        if (e.mods.isShiftDown()) { const double d = (v1 - v0) * (double) -w.deltaY * 0.5; const double a = jlimit (0.0, 1.0 - (v1 - v0), v0 + d); v1 = a + (v1 - v0); v0 = a; repaint(); return; }
        zoom (w.deltaY > 0 ? 0.8 : 1.25, toPos ((float) e.x));
    }
    void SampleWave::mouseDown (const MouseEvent& e)
    {
        dragging = -1;
        if (proc.samples().data (slot) == nullptr) return;
        const int mode = (int) raw ("play_mode");
        float best = 8.f;
        for (int i = 0; i < 4; ++i)
        {
            if (i >= 2 && mode < 2) continue;                                          // the loop markers only for LOOP / PING-PONG
            const float d = std::abs (toX (markerPos (i)) - (float) e.x);
            if (d < best) { best = d; dragging = i; }
        }
        if (dragging >= 0) { proc.undo.beginNewTransaction ("Sample marker"); return; }
        dragging = 9; dragFrom = toPos ((float) e.x); dragV0 = v0;                     // scroll
    }
    void SampleWave::mouseDrag (const MouseEvent& e)
    {
        if (dragging < 0) return;
        if (dragging == 9)
        {
            const double w = v1 - v0, d = (double) -e.getDistanceFromDragStartX() / jmax (1, getWidth()) * w;
            v0 = jlimit (0.0, 1.0 - w, dragV0 + d); v1 = v0 + w; repaint(); return;
        }
        setMarker (dragging, toPos ((float) e.x), e.mods.isShiftDown());
        repaint();
    }
    void SampleWave::paint (Graphics& g)
    {
        const auto& c = palette();
        const auto r = getLocalBounds().toFloat();
        g.setColour (c.ground); g.fillRoundedRectangle (r, 4.f);
        g.setColour (c.grid); g.drawRoundedRectangle (r.reduced (0.5f), 4.f, 1.f);
        const auto info = proc.sampleInfo (slot);
        auto ov = proc.samples().overview (slot);
        if (ov == nullptr || ov->mn.empty())
        {
            g.setColour (c.textDim); g.setFont (FontOptions (14.f));
            g.drawText (info.status == smp::SEmpty ? String ("DROP A SAMPLE HERE  -  or LOAD / the browser on the right")
                                                   : String (smp::statusText (info.status)) + (info.name.isNotEmpty() ? "  -  " + info.name : String()) + (info.message.isNotEmpty() ? "  (" + info.message + ")" : String()),
                        getLocalBounds(), Justification::centred);
            return;
        }
        const float h = r.getHeight(), mid = h * 0.5f, W = (float) getWidth();
        const double a = jmin (raw ("start"), raw ("end")), b = jmax (raw ("start"), raw ("end"));
        const int mode = (int) raw ("play_mode");
        // region + loop
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRect (Rectangle<float> (0.f, 0.f, jmax (0.f, toX (a)), h));
        g.fillRect (Rectangle<float> (toX (b), 0.f, jmax (0.f, W - toX (b)), h));
        if (mode >= 2) { g.setColour (loopCol.withAlpha (0.12f)); g.fillRect (Rectangle<float>::leftTopRightBottom (toX (markerPos (2)), 0.f, toX (markerPos (3)), h)); }
        // the waveform (min / max of the overview bins under each pixel)
        const int bins = smp::Overview::bins;
        Path p; Path q;
        for (int x = 0; x < getWidth(); ++x)
        {
            const double p0 = toPos ((float) x), p1 = toPos ((float) x + 1.f);
            int k0 = jlimit (0, bins - 1, (int) (p0 * bins)), k1 = jlimit (k0, bins - 1, (int) (p1 * bins));
            float lo = 0.f, hi = 0.f;
            for (int k = k0; k <= k1; ++k) { lo = jmin (lo, ov->mn[(size_t) k]); hi = jmax (hi, ov->mx[(size_t) k]); }
            const float y0 = mid - hi * mid * 0.92f, y1 = mid - lo * mid * 0.92f;
            if (x == 0) { p.startNewSubPath ((float) x, y0); q.startNewSubPath ((float) x, y1); } else { p.lineTo ((float) x, y0); q.lineTo ((float) x, y1); }
        }
        g.setColour (smpCol.withAlpha (0.85f)); g.strokePath (p, PathStrokeType (1.f)); g.strokePath (q, PathStrokeType (1.f));
        for (int x = 0; x < getWidth(); x += 1)
        {
            const double p0 = toPos ((float) x), p1 = toPos ((float) x + 1.f);
            int k0 = jlimit (0, bins - 1, (int) (p0 * bins)), k1 = jlimit (k0, bins - 1, (int) (p1 * bins));
            float lo = 0.f, hi = 0.f;
            for (int k = k0; k <= k1; ++k) { lo = jmin (lo, ov->mn[(size_t) k]); hi = jmax (hi, ov->mx[(size_t) k]); }
            g.setColour (smpCol.withAlpha (0.35f));
            g.drawVerticalLine (x, mid - hi * mid * 0.92f, mid - lo * mid * 0.92f + 1.f);
        }
        g.setColour (c.grid); g.drawHorizontalLine ((int) mid, 0.f, W);
        // fades (in ms of the source at rate 1)
        if (info.sourceRate > 0.0 && ov->sourceLen > 0)
        {
            const double msToPos = info.sourceRate / 1000.0 / (double) ov->sourceLen;
            const bool rev = raw ("reverse") > 0.5f;
            const double fi = raw ("fade_in") * msToPos, fo = raw ("fade_out") * msToPos;
            const double s0 = rev ? b : a, s1 = rev ? a : b;                      // where the playback starts / ends
            g.setColour (c.silver.withAlpha (0.5f));
            if (fi > 0.0) g.drawLine (toX (s0), h, toX (s0 + (rev ? -fi : fi)), 2.f, 1.2f);
            if (fo > 0.0 && mode == 0) g.drawLine (toX (s1 - (rev ? -fo : fo)), 2.f, toX (s1), h, 1.2f);
        }
        // transients
        g.setColour (c.textDim);
        for (float t : ov->transients) { const float x = toX (t); if (x >= 0.f && x <= W) g.drawVerticalLine ((int) x, h - 8.f, h); }
        // markers
        auto marker = [&] (double pos, Colour col, const char* txt)
        {
            const float x0 = toX (pos);
            if (x0 < -2.f || x0 > W + 2.f) return;
            const float x = jlimit (1.f, W - 1.f, x0), lx = x > W - 24.f ? x - 21.f : x - 1.f;      // (the label stays inside)
            g.setColour (col); g.fillRect (x - 1.f, 0.f, 2.f, h);
            g.fillRect (lx, 0.f, 22.f, 14.f);
            g.setColour (c.ground); g.setFont (FontOptions (10.f, Font::bold)); g.drawText (txt, Rectangle<float> (lx + 1.f, 0.f, 20.f, 14.f), Justification::centred);
        };
        marker (markerPos (0), c.red, "S"); marker (markerPos (1), c.red, "E");
        if (mode >= 2) { marker (markerPos (2), loopCol, "L1"); marker (markerPos (3), loopCol, "L2"); }
        // the live playback position
        lastPlayPos = proc.samplePlayPos (slot);
        if (lastPlayPos >= 0.f) { g.setColour (Colours::white.withAlpha (0.85f)); g.drawVerticalLine ((int) toX (lastPlayPos), 0.f, h); }
        // zoom bar
        if (v1 - v0 < 0.999)
        {
            g.setColour (c.control); g.fillRect (0.f, h - 3.f, W, 3.f);
            g.setColour (c.silver.withAlpha (0.6f)); g.fillRect ((float) v0 * W, h - 3.f, (float) (v1 - v0) * W, 3.f);
        }
        g.setColour (c.silver); g.setFont (FontOptions (11.f));
        g.drawText (info.name + "   " + String (info.channels == 2 ? "STEREO" : "MONO") + "   " + String (info.sourceRate / 1000.0, 1) + " kHz   "
                    + String ((double) info.sourceLen / jmax (1.0, info.sourceRate), 3) + " s" + (info.message.isNotEmpty() ? "   " + info.message : String()),
                    getLocalBounds().reduced (30, 2), Justification::topLeft);
    }

    // ===================================================================== PAGE
    SamplePage::SamplePage (GeProcessor& p, doomsday::flat::FlatLookAndFeel& l) : proc (p), lnf (l), waveEd (p)
    {
        for (int i = 0; i < 8; ++i)
        {
            slotBtn[(size_t) i].onClick = [this, i] { selectSlot (i); };
            slotBtn[(size_t) i].setTooltip ("Sample slot " + String (i + 1) + " (8 slots, each one more layer of the kick).");
            addAndMakeVisible (slotBtn[(size_t) i]);
        }
        auto chooseFile = [this] (std::function<void (const File&)> then)
        {
            chooser = std::make_unique<FileChooser> ("Choose a sample", files.folder(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                  [then] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) then (fc.getResult()); });
        };
        loadBtn.onClick = [this, chooseFile] { chooseFile ([this] (const File& f) { files.addRecent (f); proc.importSample (sel, f); }); };
        relinkBtn.onClick = [this]
        {
            chooser = std::make_unique<FileChooser> ("RELINK: the folder with the missing samples", files.folder());
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc)
            {
                const File d = fc.getResult(); if (! d.isDirectory()) return;
                const int n = proc.relinkSamples (d);
                status = n > 0 ? "RELINK: " + String (n) + " sample(s) found" : "RELINK: nothing found in " + d.getFileName();
                repaint();
            });
        };
        clearBtn.onClick = [this] { proc.clearSample (sel); };
        normBtn.onClick = [this] { proc.applySampleTool (sel, smp::TNormalize); };
        dcBtn.onClick = [this] { proc.applySampleTool (sel, smp::TDcOffset); };
        trimBtn.onClick = [this] { proc.applySampleTool (sel, smp::TTrimSilence); };
        collectBtn.onClick = [this]
        {
            chooser = std::make_unique<FileChooser> ("COLLECT SAMPLES: copy the used files into this folder", File::getSpecialLocation (File::userMusicDirectory));
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc)
            {
                const File d = fc.getResult(); if (! d.isDirectory()) return;
                status = "COLLECT: " + String (proc.collectSamples (d)) + " file(s) copied to " + d.getFileName(); repaint();
            });
        };
        loadBtn.setTooltip ("Load a WAV / AIFF / FLAC / OGG / MP3 into this slot (up to 60 s; up to 20 s is stored inside the project).");
        relinkBtn.setTooltip ("Missing files: choose a folder, the slots marked MISSING look for their file there (by name).");
        clearBtn.setTooltip ("Empty this slot (UNDO brings it back).");
        normBtn.setTooltip ("NORMALIZE the slot's working copy to -0.2 dB (the original file is never changed).");
        dcBtn.setTooltip ("Remove the DC OFFSET of the working copy.");
        trimBtn.setTooltip ("TRIM SILENCE at the start and the end of the working copy.");
        collectBtn.setTooltip ("COLLECT SAMPLES: copy the files of all slots into a folder you choose and point the slots there (only when you click it).");
        fitBtn.onClick = [this] { waveEd.fit(); }; zinBtn.onClick = [this] { waveEd.zoom (0.5, 0.5 * (waveEd.viewStart() + waveEd.viewEnd())); };
        zoutBtn.onClick = [this] { waveEd.zoom (2.0, 0.5 * (waveEd.viewStart() + waveEd.viewEnd())); };
        regionBtn.onClick = [this]
        {
            auto v = [this] (const char* k) { auto* a = proc.apvts.getRawParameterValue (id (k)); return a != nullptr ? a->load() : 0.f; };
            proc.samples().previewRegion (sel, v ("start"), v ("end"), v ("reverse") > 0.5f, proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0);
        };
        stopBtn.onClick = [this] { proc.samples().stopPreview(); };
        fitBtn.setTooltip ("Show the whole sample (or double-click the waveform)."); zinBtn.setTooltip ("Zoom in (or the mouse wheel)."); zoutBtn.setTooltip ("Zoom out.");
        regionBtn.setTooltip ("Listen to START..END of this slot alone (without a kick)."); stopBtn.setTooltip ("Stop the preview.");
        waveEd.setTooltip ("Drag S / E (and L1 / L2 in LOOP / PING-PONG); SHIFT = snap to a transient (the ticks). Wheel = zoom, drag the background = scroll, double-click = fit. Drop a file here to load it.");
        waveEd.onFile = [this] (const File& f) { files.addRecent (f); proc.importSample (sel, f); };
        for (auto* b : { &loadBtn, &relinkBtn, &clearBtn, &normBtn, &dcBtn, &trimBtn, &collectBtn, &fitBtn, &zinBtn, &zoutBtn, &regionBtn, &stopBtn })
            addAndMakeVisible (b);
        addAndMakeVisible (waveEd);
        // browser
        folderBtn.onClick = [this]
        {
            chooser = std::make_unique<FileChooser> ("Sample folder", files.folder());
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc)
            { if (fc.getResult().isDirectory()) { files.setFolder (fc.getResult(), [this] { refreshList(); }); mode = smp::Browser::All; refreshList(); } });
        };
        allBtn.onClick = [this] { setBrowserMode (smp::Browser::All); };
        favBtn.onClick = [this] { setBrowserMode (smp::Browser::Favorites); };
        recentBtn.onClick = [this] { setBrowserMode (smp::Browser::Recent); };
        prevBtn.onClick = [this] { previewListed (list.getSelectedRow()); };
        starBtn.onClick = [this] { const int r = list.getSelectedRow(); if (isPositiveAndBelow (r, listed.size())) { files.toggleFavorite (listed[r]); refreshList(); } };
        toSlotBtn.onClick = [this] { loadListed (list.getSelectedRow()); };
        folderBtn.setTooltip ("The folder the browser shows (3 levels of sub folders)."); allBtn.setTooltip ("Every audio file of the folder.");
        favBtn.setTooltip ("Your FAVORITES (kept in the 4GE settings, not in the preset)."); recentBtn.setTooltip ("The files you loaded last.");
        prevBtn.setTooltip ("Listen to the selected file without loading it."); starBtn.setTooltip ("Add / remove the selected file from the FAVORITES.");
        toSlotBtn.setTooltip ("Load the selected file into the selected slot (or double-click it).");
        search.setTextToShowWhenEmpty ("search...", palette().textDim);
        search.onTextChange = [this] { refreshList(); };
        search.setTooltip ("Search: every word must be in the file name.");
        list.setRowHeight (20);
        list.setColour (ListBox::backgroundColourId, palette().ground);
        for (Component* c : std::initializer_list<Component*> { &folderBtn, &allBtn, &favBtn, &recentBtn, &prevBtn, &starBtn, &toSlotBtn, &search, &list })
            addAndMakeVisible (c);
        if (files.folder().isDirectory()) files.setFolder (files.folder(), [this] { refreshList(); });
        build();
        selectSlot (0);
        startTimerHz (30);
    }
    SamplePage::~SamplePage() { stopTimer(); forget(); }

    Component* SamplePage::knob (const char* k, const String& label, bool bip)
    {
        const String pid = id (k);
        if (idOf (pid) < 0) return nullptr;
        auto kn = std::make_unique<doomsday::flat::Knob> (proc.apvts, pid, label, smpCol, bip, 28);
        kn->slider.setTooltip (fge::tooltip (idOf (pid)));
        auto* r = kn.get(); addAndMakeVisible (r); byId.insert ({ pid, r }); ctl.push_back (std::move (kn)); return r;
    }
    Component* SamplePage::toggle (const char* k, const String& text)
    {
        const String pid = id (k);
        auto t = std::make_unique<doomsday::flat::Toggle> (proc.apvts, pid, text, doomsday::flat::Toggle::Pill, smpCol);
        t->setTooltip (fge::tooltip (idOf (pid)));
        auto* r = t.get(); addAndMakeVisible (r); byId.insert ({ pid, r }); ctl.push_back (std::move (t)); return r;
    }
    Component* SamplePage::combo (const char* k)
    {
        const String pid = id (k);
        const int idx = idOf (pid);
        if (idx < 0) return nullptr;
        auto cb = std::make_unique<ComboBox>();
        cb->addItemList (def (idx).choices(), 1);
        comboAtt[cb.get()] = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, pid, *cb);
        cb->setTooltip (fge::tooltip (idx));
        auto* r = cb.get(); addAndMakeVisible (r); byId.insert ({ pid, r }); ctl.push_back (std::move (cb)); return r;
    }
    void SamplePage::forget()
    {
        for (auto& c : ctl) comboAtt.erase (c.get());
        ctl.clear(); byId.clear(); layoutMap.clear(); captions.clear();
    }
    Component* SamplePage::findControl (const String& pid) { auto it = byId.find (pid); return it == byId.end() ? nullptr : it->second; }

    void SamplePage::build()
    {
        forget();
        // row 1: playback / region / loop
        const std::pair<const char*, const char*> r1[] { { "level", "LEVEL" }, { "pan", "PAN" }, { "pitch", "PITCH" }, { "fine", "FINE" }, { "rate", "RATE" },
                                                         { "start", "START" }, { "end", "END" }, { "fade_in", "FADE IN" }, { "fade_out", "FADE OUT" },
                                                         { "loop_start", "LOOP S" }, { "loop_end", "LOOP E" }, { "loop_xfade", "LOOP X" }, { "root", "ROOT" },
                                                         { "offset", "OFFSET" }, { "phase", "PHASE" } };
        const std::pair<const char*, const char*> r2[] { { "width", "WIDTH" }, { "drive", "DRIVE" }, { "cutoff", "CUTOFF" }, { "resonance", "RESO" },
                                                         { "length", "LENGTH" }, { "attack", "ATTACK" }, { "decay", "DECAY" }, { "sustain", "SUSTAIN" },
                                                         { "release", "RELEASE" }, { "env_delay", "DELAY" }, { "env_amount", "ENV AMT" }, { "time_scale", "T SCALE" } };
        const std::pair<const char*, const char*> r3[] { { "send_punch", "> PUNCH" }, { "send_body", "> BODY" }, { "send_tail", "> TAIL" }, { "send_sub", "> SUB" },
                                                         { "send_texture", "> TEXTURE" }, { "send_custom", "> CUSTOM" }, { "send_master", "> MASTER" } };
        const int kw = 58, kh = doomsday::flat::Knob::heightFor (28);
        int i = 0;
        for (auto& [k, t] : r1) { if (auto* c = knob (k, t, String (k) == "pan")) layoutMap[c] = { i * kw, 0, kw, kh }; ++i; }
        i = 0;
        for (auto& [k, t] : r2) { if (auto* c = knob (k, t)) layoutMap[c] = { i * kw, kh + 6, kw, kh }; ++i; }
        const int ty = 2 * (kh + 6) + 16, sy = ty + 36;
        i = 0;
        for (auto& [k, t] : r3) { if (auto* c = knob (k, t)) layoutMap[c] = { i * kw, sy, kw, kh }; ++i; }
        // switches / choices
        int x = 0;
        auto put = [&] (Component* c, int w, int y, const String& cap = {}) { if (c == nullptr) return; layoutMap[c] = { x, y, w, 22 }; if (cap.isNotEmpty()) captions[c] = cap; x += w + 8; };
        put (toggle ("on", "ON"), 58, ty); put (combo ("play_mode"), 104, ty, "MODE"); put (toggle ("reverse", "REVERSE"), 100, ty); put (toggle ("tracking", "KEY TRACK"), 112, ty);
        put (combo ("group"), 96, ty, "GROUP"); put (toggle ("filter_on", "FILTER"), 90, ty); put (combo ("filter_type"), 92, ty, "FILTER TYPE"); put (combo ("env_mode"), 100, ty, "ENVELOPE");
        x = 7 * kw + 16;
        put (combo ("route_mode"), 104, sy + 12, "ROUTE"); put (toggle ("send_pre", "PRE"), 66, sy + 12); put (toggle ("invert", "INV"), 66, sy + 12);
        put (toggle ("mute", "M"), 52, sy + 12); put (toggle ("solo", "S"), 52, sy + 12);
        x = 7 * kw + 16;
        put (combo ("link_source"), 104, sy + 54, "LINK"); put (toggle ("link_level", "LVL"), 66, sy + 54); put (toggle ("link_pitch", "PIT"), 66, sy + 54);
        put (toggle ("link_length", "LEN"), 66, sy + 54); put (toggle ("link_env", "ENV"), 66, sy + 54);
        resized();
    }

    void SamplePage::selectSlot (int k)
    {
        sel = jlimit (0, 7, k);
        waveEd.setSlot (sel);
        build();
        updateSlotButtons();
        repaint();
    }
    void SamplePage::updateSlotButtons()
    {
        const auto& c = palette();
        for (int i = 0; i < 8; ++i)
        {
            const auto in = proc.sampleInfo (i);
            auto* on = proc.apvts.getRawParameterValue ("smp" + String (i + 1) + "_on");
            const bool active = on != nullptr && on->load() > 0.5f;
            String t = String (i + 1) + (in.name.isNotEmpty() ? " " + in.name.substring (0, 9) : String (" -"));
            if (in.status == smp::SMissing) t = String (i + 1) + " MISSING";
            if (in.status == smp::SLoading) t = String (i + 1) + " ...";
            auto& b = slotBtn[(size_t) i];
            b.setButtonText (t);
            b.setColour (TextButton::buttonColourId, i == sel ? c.red : c.control);
            b.setColour (TextButton::textColourOffId, i == sel ? c.ground : (in.status == smp::SMissing || in.status == smp::SError ? c.hot : active ? smpCol : c.silver));
        }
    }
    void SamplePage::refreshList()
    {
        listed = files.list (mode, search.getText());
        list.updateContent(); list.repaint();
        const auto& c = palette();
        allBtn.setColour (TextButton::buttonColourId, mode == 0 ? c.red : c.control);
        favBtn.setColour (TextButton::buttonColourId, mode == 1 ? c.red : c.control);
        recentBtn.setColour (TextButton::buttonColourId, mode == 2 ? c.red : c.control);
    }
    void SamplePage::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
    {
        if (! isPositiveAndBelow (row, listed.size())) return;
        const auto& c = palette();
        if (selected) { g.setColour (c.control.brighter (0.2f)); g.fillRect (0, 0, w, h); }
        const auto& f = listed[row];
        g.setColour (files.isFavorite (f) ? c.part[1] : c.textDim); g.setFont (FontOptions (12.f));
        g.drawText (files.isFavorite (f) ? "*" : "", 2, 0, 12, h, Justification::centred);
        g.setColour (c.silver);
        g.drawText (f.getFileNameWithoutExtension(), 16, 0, w - 60, h, Justification::centredLeft, true);
        g.setColour (c.textDim);
        g.drawText (f.getFileExtension().substring (1).toUpperCase(), w - 44, 0, 40, h, Justification::centredRight);
    }
    void SamplePage::loadListed (int row)
    {
        if (! isPositiveAndBelow (row, listed.size())) return;
        files.addRecent (listed[row]);
        proc.importSample (sel, listed[row]);
    }
    void SamplePage::previewListed (int row)
    {
        if (! isPositiveAndBelow (row, listed.size())) return;
        proc.samples().preview (listed[row], proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0);
    }

    void SamplePage::timerCallback()
    {
        if (! isShowing()) return;
        const auto v = proc.samples().version();
        if (v != lastVersion) { lastVersion = v; updateSlotButtons(); waveEd.repaint(); }
        if (proc.samplePlayPos (sel) >= 0.f || waveEd.lastPlayPos >= 0.f) waveEd.repaint();
        static int n = 0; if (++n % 10 == 0) updateSlotButtons();
    }

    void SamplePage::paint (Graphics& g)
    {
        const auto& c = palette();
        auto panel = [&] (Rectangle<int> r, const String& t)
        {
            g.setColour (c.panel); g.fillRoundedRectangle (r.toFloat(), 6.f);
            g.setColour (c.textDim); g.setFont (FontOptions (11.f, Font::bold)); g.drawText (t, r.getX() + 10, r.getY() + 4, r.getWidth() - 20, 14, Justification::left);
        };
        panel (ctlArea.expanded (8, 24).withBottom (ctlArea.getBottom() + 8), "SLOT " + String (sel + 1) + "  -  PLAYBACK / REGION / LOOP / SOUND / ENVELOPE / ROUTING");
        panel (browserArea.expanded (8, 24).withBottom (browserArea.getBottom() + 8), "BROWSER");
        g.setFont (FontOptions (10.f));
        for (auto& [comp, cap] : captions)
        {
            if (! comp->isVisible()) continue;
            g.setColour (c.textDim);
            g.drawText (cap, comp->getX(), comp->getY() - 13, comp->getWidth(), 12, Justification::left);
        }
        g.setColour (c.textDim); g.setFont (FontOptions (11.f));
        String hint = status.isNotEmpty() ? status : String ("The samples are layers 9-16 of the kick: their GROUP's level / mute / solo, the modules 05-08 of that part and the modulation work on them.");
        g.drawText (hint, waveArea.getX(), waveArea.getBottom() + 34, waveArea.getWidth(), 14, Justification::left);
        if (listed.isEmpty())
        {
            g.drawText (mode == 1 ? "no favorites yet" : mode == 2 ? "nothing loaded yet" : files.folder().isDirectory() ? "no audio files" : "choose a FOLDER...",
                        list.getBounds(), Justification::centred);
        }
    }
    void SamplePage::resized()
    {
        const int W = getWidth(), H = getHeight();
        const int bw = 300;
        const int left = W - bw - 16;
        // slot row
        const int sw = 88;
        for (int i = 0; i < 8; ++i) slotBtn[(size_t) i].setBounds (i * (sw + 4), 0, sw, 26);
        int x = 8 * (sw + 4) + 8;
        for (auto* b : { &loadBtn, &relinkBtn, &clearBtn, &normBtn, &dcBtn, &trimBtn, &collectBtn }) { const int w = b == &dcBtn ? 34 : b == &normBtn ? 74 : 56; b->setBounds (x, 0, w, 26); x += w + 4; }
        waveArea = { 0, 34, left, 250 };
        waveEd.setBounds (waveArea);
        x = 0;
        for (auto* b : { &fitBtn, &zinBtn, &zoutBtn }) { b->setBounds (x, waveArea.getBottom() + 6, b == &fitBtn ? 44 : 26, 22); x += (b == &fitBtn ? 44 : 26) + 4; }
        regionBtn.setBounds (x + 10, waveArea.getBottom() + 6, 116, 22); stopBtn.setBounds (x + 130, waveArea.getBottom() + 6, 50, 22);
        ctlArea = { 8, waveArea.getBottom() + 82, left - 16, H - waveArea.getBottom() - 90 };
        for (auto& [c, r] : layoutMap) c->setBounds (r.translated (ctlArea.getX(), ctlArea.getY()));
        browserArea = { left + 24, 58, bw - 16, H - 66 };
        auto b = browserArea;
        folderBtn.setBounds (b.removeFromTop (24));
        b.removeFromTop (6); search.setBounds (b.removeFromTop (24));
        b.removeFromTop (6);
        { auto row = b.removeFromTop (24); const int w = (row.getWidth() - 8) / 3; allBtn.setBounds (row.removeFromLeft (w)); row.removeFromLeft (4); favBtn.setBounds (row.removeFromLeft (w)); row.removeFromLeft (4); recentBtn.setBounds (row); }
        b.removeFromTop (6);
        { auto row = b.removeFromBottom (24); const int w = (row.getWidth() - 8) / 3; prevBtn.setBounds (row.removeFromLeft (w)); row.removeFromLeft (4); starBtn.setBounds (row.removeFromLeft (w)); row.removeFromLeft (4); toSlotBtn.setBounds (row); }
        b.removeFromBottom (6);
        list.setBounds (b);
    }
}
