#include "EqPage.h"
#include "Palette.h"

using namespace juce;

namespace fge::gui
{
    namespace tn = fge::dsp::tone;
    namespace
    {
        int idOf (const String& id) { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; }
        Colour locCol (int loc)
        {
            const auto& c = palette();
            switch (loc) { case 1: return c.part[0]; case 2: return c.part[1]; case 3: return c.part[2]; case 4: return c.part[3]; case 5: return c.part[5];
                           case 0: return c.red; default: return c.part[4]; }
        }
        constexpr float dbRange = 24.f;
    }

    // ===================================================================== factories
    Component* EqPage::knob (const String& id, const String& label, Colour col, bool bip, int d)
    {
        if (idOf (id) < 0) return nullptr;
        auto k = std::make_unique<doomsday::flat::Knob> (proc.apvts, id, label, col, bip, d);
        k->slider.setTooltip (fge::tooltip (idOf (id)));
        auto* r = k.get(); addAndMakeVisible (r); byId.insert ({ id, r }); ctl.push_back (std::move (k)); return r;
    }
    Component* EqPage::toggle (const String& id, const String& text, Colour col)
    {
        if (idOf (id) < 0) return nullptr;
        auto t = std::make_unique<doomsday::flat::Toggle> (proc.apvts, id, text, text.length() <= 3 && text != "ON" ? doomsday::flat::Toggle::Chip : doomsday::flat::Toggle::Pill, col);
        t->setTooltip (fge::tooltip (idOf (id)));
        auto* r = t.get(); addAndMakeVisible (r); byId.insert ({ id, r }); ctl.push_back (std::move (t)); return r;
    }
    Component* EqPage::combo (const String& id, const String& caption)
    {
        const int idx = idOf (id);
        if (idx < 0) return nullptr;
        auto cb = std::make_unique<ComboBox>();
        cb->addItemList (def (idx).choices(), 1);
        comboAtt[cb.get()] = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *cb);
        cb->setTooltip (fge::tooltip (idx));
        auto* r = cb.get(); addAndMakeVisible (r); byId.insert ({ id, r }); if (caption.isNotEmpty()) captions[r] = caption; ctl.push_back (std::move (cb)); return r;
    }
    void EqPage::forget()
    {
        for (auto& c : ctl) comboAtt.erase (c.get());
        byId.clear(); captions.clear(); layoutMap.clear(); titles.clear(); ctl.clear();
    }
    Component* EqPage::findControl (const String& id) { auto it = byId.find (id); return it == byId.end() ? nullptr : it->second; }
    StringArray EqPage::visibleIds() const { StringArray s; for (auto& [id, c] : byId) if (c->isVisible()) s.addIfNotAlreadyThere (id); return s; }
    float EqPage::raw (const String& id) const { auto* v = proc.apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.f; }

    // ===================================================================== page
    EqPage::EqPage (GeProcessor& p, doomsday::flat::FlatLookAndFeel& l) : proc (p), lnf (l)
    {
        const char* tabs[5] { "PARAMETRIC", "DYNAMIC", "MULTIBAND", "SPECTRAL", "STEREO" };
        for (int i = 0; i < 5; ++i) { tabBtn[(size_t) i].setButtonText (tabs[i]); tabBtn[(size_t) i].onClick = [this, i] { showTab (i); }; addAndMakeVisible (tabBtn[(size_t) i]); }
        for (int i = 0; i < 16; ++i) { bandBtn[(size_t) i].setButtonText (String (i + 1)); bandBtn[(size_t) i].onClick = [this, i] { selectBand (i); }; addChildComponent (bandBtn[(size_t) i]); }
        for (int i = 0; i < 4; ++i) { dynBtn[(size_t) i].setButtonText ("DYN " + String (i + 1)); dynBtn[(size_t) i].onClick = [this, i] { selectDyn (i); }; addChildComponent (dynBtn[(size_t) i]); }
        const char* lockNames[5] { "LOCK EQ", "LOCK DYN", "LOCK MB", "LOCK SPEC", "LOCK ST" };
        for (int i = 0; i < m07t::NumEngines; ++i) { lockBtn[(size_t) i].setButtonText (lockNames[i]); lockBtn[(size_t) i].setTooltip ("RANDOMIZE leaves this engine as it is."); addAndMakeVisible (lockBtn[(size_t) i]); }
        presetBtn.setTooltip ("Module 07 presets (EARLY / MODERN / RAWSTYLE / EXPERIMENTAL) - the whole module.");
        saveBtn.setTooltip ("Save the selected EQ band or the whole module."); loadBtn.setTooltip ("Load an EQ band (into the selected band) or a whole module file.");
        abBtn.setTooltip ("A / B: switch between two versions of the whole module."); randBtn.setTooltip ("Randomize the ON processors in musical ranges (LOCKED engines stay). One UNDO step.");
        copyBtn.setTooltip ("Copy the selected band."); pasteBtn.setTooltip ("Paste into the selected band."); dupBtn.setTooltip ("A copy of the selected band in the first free band.");
        delBtn.setTooltip ("Reset the selected band (OFF).");
        presetBtn.onClick = [this] { showPresetMenu(); };
        saveBtn.onClick = [this] { saveFile(); }; loadBtn.onClick = [this] { loadFile(); };
        abBtn.onClick = [this] { compare.toggle (proc); abBtn.setButtonText (compare.onB ? "A / [B]" : "[A] / B"); };
        randBtn.onClick = [this] { m07t::randomize (proc, locks(), (int) Time::getMillisecondCounter(), 0.6f); };
        copyBtn.onClick = [this] { clip = m07t::bandToXml (proc, bandSel); };
        pasteBtn.onClick = [this] { if (clip) m07t::pasteBand (proc, bandSel, *clip); };
        dupBtn.onClick = [this] { const int k = m07t::duplicateBand (proc, bandSel); if (k >= 0) selectBand (k); };
        delBtn.onClick = [this] { m07t::resetBand (proc, bandSel); };
        holdBtn.setClickingTogglesState (true); holdBtn.setTooltip ("Keep the highest analyzer level per frequency.");
        resetHoldBtn.onClick = [this] { holdPost.clear(); avgPost.clear(); repaint(); };
        fftBox.addItemList ({ "FFT 2048", "FFT 4096", "FFT 8192", "FFT 16384" }, 1); fftBox.setSelectedId (3, dontSendNotification); fftBox.setTooltip ("Analyzer resolution.");
        avgBox.addItemList ({ "AVG OFF", "AVG SLOW", "AVG FAST" }, 1); avgBox.setSelectedId (1, dontSendNotification); avgBox.setTooltip ("Average the analyzer over the updates.");
        scaleBox.addItemList ({ "LOG", "LINEAR" }, 1); scaleBox.setSelectedId (1, dontSendNotification); scaleBox.setTooltip ("Frequency scale of the graph.");
        scaleBox.onChange = [this] { repaint(); }; avgBox.onChange = [this] { avgPost.clear(); };
        for (Component* c : std::initializer_list<Component*> { &presetBtn, &saveBtn, &loadBtn, &abBtn, &randBtn })
            addAndMakeVisible (c);
        for (Component* c : std::initializer_list<Component*> { &copyBtn, &pasteBtn, &dupBtn, &delBtn, &holdBtn, &resetHoldBtn, &fftBox, &avgBox, &scaleBox })
            addChildComponent (c);
        abBtn.setButtonText ("[A] / B");
        scope.onReady = [this] { adoptScope(); };
        showTab (TParametric);
        startTimerHz (15);
    }
    EqPage::~EqPage() { stopTimer(); comboAtt.clear(); }

    void EqPage::showTab (int t) { tabIdx = jlimit (0, 4, t); lastSig = {}; build(); timerCallback(); }
    void EqPage::selectBand (int b) { bandSel = jlimit (0, 15, b); lastSig = {}; build(); timerCallback(); }
    void EqPage::selectDyn (int d) { dynSel = jlimit (0, 3, d); lastSig = {}; build(); timerCallback(); }

    int EqPage::viewLoc() const
    {
        switch (tabIdx)
        {
            case TDynamic:   return roundToInt (raw (m07t::did (dynSel, "location")));
            case TMultiband: return roundToInt (raw ("mb_location"));
            case TSpectral:  return roundToInt (raw ("sp_location"));
            case TStereo:    return tn::stwLocs[jlimit (0, 6, roundToInt (raw ("stw_location")))];
            default:         return roundToInt (raw (m07t::eid (bandSel, "location")));
        }
    }

    String EqPage::signature() const
    {
        String s; s << tabIdx << "/" << bandSel << "/" << dynSel << "/" << roundToInt (raw (m07t::eid (bandSel, "type"))) << "/" << roundToInt (raw (m07t::did (dynSel, "detector")));
        for (int k = 1; k <= 4; ++k) s << roundToInt (raw ("mb" + String (k) + "_dynamics"));
        return s;
    }

    void EqPage::build()
    {
        forget();
        const auto& pc = palette();
        const int y0 = 404, kh = doomsday::flat::Knob::heightFor (26);
        auto knobRow = [&] (std::initializer_list<std::tuple<String, String, bool>> ks, int x, int y, Colour col, int step = 74)
        {
            int k = 0; for (auto& [id, lab, bip] : ks) place (knob (id, lab, col, bip), x + (k++) * step, y, step - 4, kh);
        };
        auto comboRow = [&] (std::initializer_list<std::pair<String, String>> cs, int x, int y, int w = 128)
        {
            int k = 0; for (auto& [id, cap] : cs) place (combo (id, cap), x + (k++) * (w + 6), y, w, 22);
        };
        for (auto& b : bandBtn) b.setVisible (tabIdx == TParametric);
        for (auto& b : dynBtn) b.setVisible (tabIdx == TDynamic);
        for (Component* c : std::initializer_list<Component*> { &copyBtn, &pasteBtn, &dupBtn, &delBtn }) c->setVisible (tabIdx == TParametric);
        for (Component* c : std::initializer_list<Component*> { &holdBtn, &resetHoldBtn, &fftBox, &avgBox, &scaleBox }) c->setVisible (true);
        if (tabIdx == TParametric)
        {
            const auto e = [&] (const String& k) { return m07t::eid (bandSel, k); };
            const int t = roundToInt (raw (e ("type")));
            const auto col = locCol (roundToInt (raw (e ("location"))));
            titles.push_back ({ { 0, y0 - 20, 300, 14 }, "BAND " + String (bandSel + 1) });
            place (toggle (e ("on"), "ON", col), 0, y0, 60, 22);
            int cx = 70;
            for (auto [id, cap] : std::initializer_list<std::pair<const char*, const char*>> { { "type", "TYPE" }, { "location", "LOCATION" }, { "position", "POSITION" }, { "stereo", "STEREO MODE" } })
            { place (combo (e (id), cap), cx, y0, 140, 22); cx += 146; }
            if (tn::eqHasSlope (t)) { place (combo (e ("slope"), "SLOPE"), cx, y0, 90, 22); cx += 96; }
            int k = 0;
            place (knob (e ("freq"), "FREQUENCY", col), (k++) * 80, y0 + 40, 76, kh);
            if (tn::eqHasGain (t)) place (knob (e ("gain"), t == tn::Tilt ? "TILT" : "GAIN", col, true), (k++) * 80, y0 + 40, 76, kh);
            if (tn::eqHasQ (t)) place (knob (e ("q"), t == tn::LowCut || t == tn::HighCut ? "RESONANCE (Q)" : "Q", col), (k++) * 80, y0 + 40, 76, kh);
            place (knob (e ("mix"), "MIX", col), (k++) * 80, y0 + 40, 76, kh);
        }
        else if (tabIdx == TDynamic)
        {
            const auto d = [&] (const String& k) { return m07t::did (dynSel, k); };
            const auto col = locCol (roundToInt (raw (d ("location"))));
            titles.push_back ({ { 0, y0 - 20, 300, 14 }, "DYNAMIC BAND " + String (dynSel + 1) + "  (the bell's gain follows the detector)" });
            place (toggle (d ("on"), "ON", col), 0, y0, 60, 22);
            comboRow ({ { d ("mode"), "MODE" }, { d ("detector_source"), "DETECTOR SOURCE" }, { d ("detector"), "DETECTOR" }, { d ("location"), "LOCATION" }, { d ("position"), "POSITION" } }, 70, y0, 140);
            if (roundToInt (raw (d ("detector"))) == 1)
                knobRow ({ { d ("freq"), "FREQUENCY", false }, { d ("q"), "Q", false }, { d ("threshold"), "THRESHOLD", false }, { d ("range"), "RANGE", false }, { d ("ratio"), "RATIO", false },
                           { d ("knee"), "KNEE", false }, { d ("attack"), "ATTACK", false }, { d ("release"), "RELEASE", false }, { d ("window"), "RMS WINDOW", false }, { d ("mix"), "MIX", false } }, 0, y0 + 40, col, 80);
            else
                knobRow ({ { d ("freq"), "FREQUENCY", false }, { d ("q"), "Q", false }, { d ("threshold"), "THRESHOLD", false }, { d ("range"), "RANGE", false }, { d ("ratio"), "RATIO", false },
                           { d ("knee"), "KNEE", false }, { d ("attack"), "ATTACK", false }, { d ("release"), "RELEASE", false }, { d ("mix"), "MIX", false } }, 0, y0 + 40, col, 80);
        }
        else if (tabIdx == TMultiband)
        {
            const auto col = pc.part[2];
            place (toggle ("mb_on", "ON", col), 0, 74, 60, 22);
            comboRow ({ { "mb_location", "LOCATION" }, { "mb_position", "POSITION" }, { "mb_crossover", "CROSSOVER" }, { "mb_listen", "LISTEN" } }, 70, 74, 150);
            knobRow ({ { "mb_xover1", "SUB / LOW", false }, { "mb_xover2", "LOW / MID", false }, { "mb_xover3", "MID / HIGH", false } }, 720, 58, col, 80);
            const char* nm[4] { "SUB", "LOW", "MID", "HIGH" };
            const Colour bc[4] { pc.part[3], pc.part[1], pc.part[2], pc.part[0] };
            const int cw = 300;
            for (int b = 0; b < 4; ++b)
            {
                const String o = "mb" + String (b + 1) + "_";
                const int x = b * cw, y = 150;
                titles.push_back ({ { x, y - 18, cw - 8, 14 }, String (nm[b]) + " BAND" });
                place (toggle (o + "invert", "INV", bc[b]), x, y, 48, 22); place (toggle (o + "mute", "M", bc[b]), x + 54, y, 40, 22); place (toggle (o + "solo", "S", bc[b]), x + 100, y, 40, 22);
                place (combo (o + "dynamics", ""), x + 160, y, 130, 22);
                place (knob (o + "gain", "GAIN", bc[b], true), x, y + 30, 70, kh); place (knob (o + "width", "WIDTH", bc[b]), x + 72, y + 30, 70, kh);
                const int dm = roundToInt (raw (o + "dynamics"));
                if (dm == tn::MdTransient)
                { place (knob (o + "tr_attack", "ATTACK +/-", bc[b], true), x + 144, y + 30, 70, kh); place (knob (o + "tr_sustain", "SUSTAIN +/-", bc[b], true), x + 216, y + 30, 70, kh);
                  place (knob (o + "attack", "SPEED", bc[b]), x, y + 110, 70, kh); place (knob (o + "release", "RELEASE", bc[b]), x + 72, y + 110, 70, kh); }
                else if (dm != tn::MdOff)
                {
                    place (knob (o + "threshold", "THRESHOLD", bc[b]), x + 144, y + 30, 70, kh);
                    if (dm != tn::MdLimit) place (knob (o + "ratio", dm == tn::MdGate ? "RATIO" : "RATIO", bc[b]), x + 216, y + 30, 70, kh);
                    place (knob (o + "attack", "ATTACK", bc[b]), x, y + 110, 70, kh); place (knob (o + "release", "RELEASE", bc[b]), x + 72, y + 110, 70, kh);
                    place (knob (o + "knee", "KNEE", bc[b]), x + 144, y + 110, 70, kh); place (knob (o + "makeup", "MAKEUP", bc[b], true), x + 216, y + 110, 70, kh);
                    place (knob (o + "mix", "MIX", bc[b]), x, y + 190, 70, kh);
                }
            }
        }
        else if (tabIdx == TSpectral)
        {
            const auto col = pc.part[4];
            place (toggle ("sp_on", "ON", col), 0, 74, 60, 22);
            comboRow ({ { "sp_location", "LOCATION" }, { "sp_position", "POSITION" } }, 70, 74, 150);
            place (combo ("tone_order", "ORDER (EQ / DYN / MB / SPECTRAL at one place)"), 400, 74, 300, 22);
            int y = 116;
            titles.push_back ({ { 0, y, 200, 14 }, "TILT" });
            knobRow ({ { "sp_tilt", "TILT", true }, { "sp_tilt_pivot", "PIVOT", false }, { "sp_tilt_intensity", "INTENSITY", false }, { "sp_tilt_mix", "MIX", false } }, 0, y + 16, col);
            titles.push_back ({ { 320, y, 300, 14 }, "HARMONIC SHAPER" });
            place (toggle ("sp_harm_on", "ON", col), 314, y + 30, 64, 22);
            knobRow ({ { "sp_harm_even", "EVEN", false }, { "sp_harm_odd", "ODD", false }, { "sp_harm_low", "LOW", false }, { "sp_harm_high", "HIGH", false }, { "sp_harm_split", "SPLIT", false },
                       { "sp_harm_drive", "DRIVE", false }, { "sp_harm_balance", "BALANCE", true }, { "sp_harm_mix", "MIX", false } }, 380, y + 16, col, 72);
            y += 100;
            titles.push_back ({ { 0, y, 200, 14 }, "EXCITER" });
            place (toggle ("sp_exc_on", "ON", col), 0, y + 30, 64, 22);
            place (combo ("sp_exc_mode", "MODE"), 70, y + 30, 104, 22); place (combo ("sp_exc_harmonics", "HARMONICS"), 180, y + 30, 90, 22);
            knobRow ({ { "sp_exc_low", "FROM", false }, { "sp_exc_high", "TO", false }, { "sp_exc_amount", "EXCITATION", false }, { "sp_exc_color", "COLOR", false },
                       { "sp_exc_drive", "DRIVE", false }, { "sp_exc_mix", "MIX", false } }, 280, y + 16, col, 72);
            y += 100;
            titles.push_back ({ { 0, y, 300, 14 }, "SPECTRAL COMPRESSION" });
            place (toggle ("sp_comp_on", "ON", col), 0, y + 30, 64, 22);
            knobRow ({ { "sp_comp_sensitivity", "SENSITIVITY", false }, { "sp_comp_threshold", "THRESHOLD", false }, { "sp_comp_reduction", "REDUCTION", false }, { "sp_comp_attack", "ATTACK", false },
                       { "sp_comp_release", "RELEASE", false }, { "sp_comp_low", "FROM", false }, { "sp_comp_high", "TO", false }, { "sp_comp_smoothing", "SMOOTHING", false }, { "sp_comp_mix", "MIX", false } }, 70, y + 16, col, 72);
            y += 100;
            titles.push_back ({ { 0, y, 300, 14 }, "SPECTRAL FREEZE (experimental)" });
            place (toggle ("sp_freeze", "FREEZE", col), 0, y + 30, 92, 22);
            knobRow ({ { "sp_freeze_capture", "CAPTURE AT", false }, { "sp_freeze_decay", "DECAY", false }, { "sp_freeze_smoothing", "SMOOTHING", false }, { "sp_freeze_pitch", "PITCH", true },
                       { "sp_freeze_mix", "MIX", false } }, 100, y + 16, col, 72);
        }
        else
        {
            const auto col = pc.silver;
            place (toggle ("stw_on", "ON", col), 0, 74, 60, 22);
            place (combo ("stw_location", "LOCATION"), 70, 74, 150, 22);
            place (toggle ("stw_mono_listen", "MONO LISTEN", col), 230, 74, 120, 22);
            knobRow ({ { "stw_width", "WIDTH", false }, { "stw_low_width", "LOW WIDTH", false }, { "stw_mid_width", "MID WIDTH", false }, { "stw_high_width", "HIGH WIDTH", false },
                       { "stw_xover_low", "LOW / MID", false }, { "stw_xover_high", "MID / HIGH", false }, { "stw_mono_below", "MONO BELOW", false }, { "stw_mix", "MIX", false } }, 0, 120, col, 80);
            titles.push_back ({ { 0, 206, 300, 14 }, "MID / SIDE GAIN PER BAND" });
            knobRow ({ { "stw_low_mid_gain", "LOW MID", true }, { "stw_low_side_gain", "LOW SIDE", true }, { "stw_mid_mid_gain", "MID MID", true }, { "stw_mid_side_gain", "MID SIDE", true },
                       { "stw_high_mid_gain", "HIGH MID", true }, { "stw_high_side_gain", "HIGH SIDE", true } }, 0, 222, col, 80);
        }
        updateEnabled();
        resized();
        repaint();
    }

    void EqPage::updateEnabled()
    {
        auto en = [&] (const String& id, bool on) { for (auto [it, end] = byId.equal_range (id); it != end; ++it) { it->second->setEnabled (on); it->second->setAlpha (on ? 1.f : 0.4f); } };
        if (tabIdx == TSpectral)
        {
            const bool h = raw ("sp_harm_on") > 0.5f, x = raw ("sp_exc_on") > 0.5f, c = raw ("sp_comp_on") > 0.5f, f = raw ("sp_freeze") > 0.5f;
            for (auto& [id, comp] : byId)
            {
                if (id.startsWith ("sp_harm_") && id != "sp_harm_on") en (id, h);
                if (id.startsWith ("sp_exc_") && id != "sp_exc_on") en (id, x);
                if (id.startsWith ("sp_comp_") && id != "sp_comp_on") en (id, c);
                if (id.startsWith ("sp_freeze_")) en (id, f);
                if (id.startsWith ("sp_tilt_")) en (id, raw ("sp_tilt") != 0.f);
            }
        }
        if (tabIdx == TMultiband) { const bool lr = raw ("mb_listen") < 0.5f; for (auto& [id, comp] : byId) if (id.startsWith ("mb") && ! id.startsWith ("mb_") && ! id.endsWith ("mute") && ! id.endsWith ("solo")) en (id, lr); }
        if (tabIdx == TStereo) { const bool mid = true; juce::ignoreUnused (mid); }
    }

    void EqPage::requestScope()
    {
        auto q = std::make_unique<EqScope::Request>();
        q->f = proc.frame(); q->bank = proc.shapes(); q->tables = proc.currentTables(); q->dist = proc.distData();
        q->loc = viewLoc(); q->band = bandSel; q->fftOrder = 10 + fftBox.getSelectedId(); q->note = proc.auditionNote;
        scope.ask (std::move (q));
    }
    void EqPage::adoptScope()
    {
        shown = scope.result();
        if (shown.valid)
        {
            const int am = avgBox.getSelectedId();
            if (am == 1 || avgPost.size() != shown.post.size()) avgPost = shown.post;
            else { const float a = am == 2 ? 0.2f : 0.5f; for (size_t k = 0; k < avgPost.size(); ++k) avgPost[k] += a * (shown.post[k] - avgPost[k]); }
            if (holdPost.size() != shown.post.size()) holdPost = shown.post;
            else for (size_t k = 0; k < holdPost.size(); ++k) holdPost[k] = jmax (holdPost[k], shown.post[k]);
        }
        repaint();
    }

    void EqPage::timerCallback()
    {
        if (! isShowing() && lastSig.isNotEmpty()) return;
        const auto sig = signature();
        if (sig != lastSig) { const bool first = lastSig.isEmpty(); lastSig = sig; if (! first) build(); }
        for (int i = 0; i < 16; ++i)
        {
            const bool on = raw (m07t::eid (i, "on")) > 0.5f;
            auto& b = bandBtn[(size_t) i];
            b.setColour (TextButton::buttonColourId, i == bandSel ? palette().red : on ? palette().control.brighter (0.35f) : palette().control);
            b.setColour (TextButton::textColourOffId, i == bandSel ? palette().ground : palette().silver);
        }
        for (int i = 0; i < 4; ++i)
        {
            auto& b = dynBtn[(size_t) i];
            b.setColour (TextButton::buttonColourId, i == dynSel ? palette().red : raw (m07t::did (i, "on")) > 0.5f ? palette().control.brighter (0.35f) : palette().control);
            b.setColour (TextButton::textColourOffId, i == dynSel ? palette().ground : palette().silver);
        }
        pasteBtn.setEnabled (clip != nullptr);
        updateEnabled();
        const auto& f = proc.frame();
        const int loc = viewLoc(), ff = fftBox.getSelectedId();
        if (loc != lastLoc || bandSel != lastBandAsk || ff != lastFft || proc.shapesVersion() != lastShapes || proc.distDataVersion() != lastDist
            || ! std::equal (f.v.begin(), f.v.end(), lastAsked.v.begin()))
        {
            if (loc != lastLoc) { holdPost.clear(); avgPost.clear(); }
            lastAsked = f; lastShapes = proc.shapesVersion(); lastDist = proc.distDataVersion(); lastLoc = loc; lastBandAsk = bandSel; lastFft = ff;
            requestScope();
        }
    }

    // ===================================================================== graph mapping + mouse (PARAMETRIC: the points)
    Point<float> EqPage::toGraph (float hz, float db) const
    {
        const auto g = graph.toFloat().reduced (6.f, 8.f);
        const bool lin = scaleBox.getSelectedId() == 2;
        const float u = lin ? (hz - 20.f) / (20000.f - 20.f) : std::log (hz / 20.f) / std::log (1000.f);
        return { g.getX() + g.getWidth() * jlimit (0.f, 1.f, u), g.getCentreY() - g.getHeight() * 0.5f * jlimit (-1.2f, 1.2f, db / dbRange) };
    }
    float EqPage::fromX (float x) const
    {
        const auto g = graph.toFloat().reduced (6.f, 8.f);
        const float u = jlimit (0.f, 1.f, (x - g.getX()) / g.getWidth());
        return scaleBox.getSelectedId() == 2 ? 20.f + u * (20000.f - 20.f) : 20.f * std::pow (1000.f, u);
    }
    float EqPage::fromY (float y) const { const auto g = graph.toFloat().reduced (6.f, 8.f); return jlimit (-dbRange, dbRange, (g.getCentreY() - y) / (g.getHeight() * 0.5f) * dbRange); }
    int EqPage::hitBand (Point<float> p) const
    {
        const int loc = viewLoc();
        for (int i = 0; i < 16; ++i)
        {
            if (raw (m07t::eid (i, "on")) < 0.5f || roundToInt (raw (m07t::eid (i, "location"))) != loc) continue;
            const int t = roundToInt (raw (m07t::eid (i, "type")));
            if (toGraph (raw (m07t::eid (i, "freq")), tn::eqHasGain (t) ? raw (m07t::eid (i, "gain")) : 0.f).getDistanceFrom (p) < 9.f) return i;
        }
        return -1;
    }
    void EqPage::mouseDown (const MouseEvent& e)
    {
        if (tabIdx != TParametric || ! graph.contains (e.getPosition())) return;
        const int h = hitBand (e.position);
        if (h < 0) return;
        if (e.mods.isPopupMenu()) { m07t::resetBand (proc, h); return; }
        if (h != bandSel) selectBand (h);
        dragBand = h;
        proc.undo.beginNewTransaction ("Move EQ band");
    }
    void EqPage::mouseDrag (const MouseEvent& e)
    {
        if (dragBand < 0) return;
        const float hz = jlimit (20.f, 20000.f, fromX (e.position.x));
        if (auto* p = proc.apvts.getParameter (m07t::eid (dragBand, "freq"))) p->setValueNotifyingHost (p->convertTo0to1 (hz));
        if (tn::eqHasGain (roundToInt (raw (m07t::eid (dragBand, "type")))))
            if (auto* p = proc.apvts.getParameter (m07t::eid (dragBand, "gain"))) p->setValueNotifyingHost (p->convertTo0to1 (fromY (e.position.y)));
    }
    void EqPage::mouseDoubleClick (const MouseEvent& e)
    {
        if (tabIdx != TParametric || ! graph.contains (e.getPosition())) return;
        const int h = hitBand (e.position);
        if (h >= 0) { selectBand (h); return; }
        const int k = m07t::addBand (proc, fromX (e.position.x), fromY (e.position.y), viewLoc());
        if (k >= 0) selectBand (k);
    }
    void EqPage::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
    {
        if (tabIdx != TParametric || ! graph.contains (e.getPosition())) return;
        const int h = hitBand (e.position);
        if (h < 0) return;
        if (auto* p = proc.apvts.getParameter (m07t::eid (h, "q")))
        {
            const float q = jlimit (0.1f, 30.f, raw (m07t::eid (h, "q")) * std::exp2 (w.deltaY * 2.f));
            p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (q)); p->endChangeGesture();
        }
    }

    // ===================================================================== presets / files
    void EqPage::showPresetMenu()
    {
        PopupMenu m; std::map<String, PopupMenu> cats; StringArray order;
        const auto& l = m07t::presets();
        for (int i = 0; i < (int) l.size(); ++i) { const String c (l[(size_t) i].category); if (! order.contains (c)) order.add (c); cats[c].addItem (i + 1, l[(size_t) i].name); }
        for (auto& c : order) m.addSubMenu (c, cats[c]);
        SafePointer<EqPage> self (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetBtn), [self] (int r) { if (self != nullptr && r > 0) { m07t::apply (self->proc, m07t::presets()[(size_t) r - 1]); self->lastSig = {}; } });
    }
    void EqPage::saveFile()
    {
        PopupMenu m; m.addItem (1, "Save EQ band " + String (bandSel + 1) + " (.4geq)"); m.addItem (2, "Save the whole module 07 (.4gm07)");
        SafePointer<EqPage> self (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&saveBtn), [self] (int r)
        {
            if (self == nullptr || r <= 0) return;
            const bool band = r == 1;
            self->chooser = std::make_unique<FileChooser> ("Save", files::folder ("EQ"), band ? "*.4geq" : "*.4gm07");
            self->chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [self, band] (const FileChooser& fc)
            {
                if (self == nullptr || fc.getResult() == File()) return;
                auto x = band ? m07t::bandToXml (self->proc, self->bandSel) : m07t::moduleToXml (self->proc);
                files::writeXml (fc.getResult().withFileExtension (m07t::extensionOf (x->getTagName())), *x);
            });
        });
    }
    void EqPage::loadFile()
    {
        chooser = std::make_unique<FileChooser> ("Load", files::folder ("EQ"), "*.4geq;*.4gm07");
        SafePointer<EqPage> self (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [self] (const FileChooser& fc)
        {
            if (self == nullptr || fc.getResult() == File()) return;
            if (auto x = files::readXml (fc.getResult())) { m07t::load (self->proc, *x, self->bandSel); self->lastSig = {}; }
        });
    }

    // ===================================================================== layout / paint
    void EqPage::resized()
    {
        const int w = getWidth(), h = getHeight();
        for (int i = 0; i < 5; ++i) tabBtn[(size_t) i].setBounds (i * 112, 0, 108, 26);
        randBtn.setBounds (w - 96, 0, 96, 26); abBtn.setBounds (w - 184, 0, 84, 26); loadBtn.setBounds (w - 248, 0, 60, 26); saveBtn.setBounds (w - 312, 0, 60, 26);
        presetBtn.setBounds (w - 400, 0, 84, 26);
        for (int i = 0; i < m07t::NumEngines; ++i) lockBtn[(size_t) i].setBounds (w - 5 * 96 + i * 96, 32, 94, 22);
        for (int i = 0; i < 16; ++i) bandBtn[(size_t) i].setBounds (i * 40, 32, 37, 22);
        for (int i = 0; i < 4; ++i) dynBtn[(size_t) i].setBounds (i * 74, 32, 70, 22);
        const bool big = tabIdx == TParametric || tabIdx == TDynamic;
        graph = big ? Rectangle<int> (0, 88, w, 290) : tabIdx == TSpectral ? Rectangle<int> (840, 300, w - 840, h - 300) : tabIdx == TStereo ? Rectangle<int> (0, 330, w - 380, h - 330)
                    : Rectangle<int> (0, 580, w, h - 580);
        lower = { 0, 392, w, h - 392 };
        int bx = graph.getRight() - 4 * 88 - 60;
        for (Component* c : std::initializer_list<Component*> { &fftBox, &avgBox, &scaleBox, &holdBtn }) { c->setBounds (bx, graph.getY() - 26, 84, 22); bx += 88; }
        resetHoldBtn.setBounds (bx, graph.getY() - 26, 56, 22);
        int cx = w - 4 * 92;
        for (Component* c : std::initializer_list<Component*> { &copyBtn, &pasteBtn, &dupBtn, &delBtn }) { c->setBounds (cx, 404, 88, 22); cx += 92; }
        if (tabIdx == TMultiband) graph = { 0, 580, w, h - 580 };
        for (auto& [c, r] : layoutMap) c->setBounds (r);
    }

    void EqPage::paint (Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.textDim); g.setFont (FontOptions (10.5f, Font::bold));
        for (auto& [comp, cap] : captions) if (comp->isVisible()) g.drawText (cap, comp->getX(), comp->getY() - 12, comp->getWidth() + 80, 11, Justification::centredLeft);
        for (auto& [r, t] : titles) g.drawText (t, r, Justification::centredLeft);
        // ---- the graph: grid, analyzer PRE / POST (+ average / hold), response (whole place + selected band), points
        g.setColour (c.ground); g.fillRoundedRectangle (graph.toFloat(), 4.f);
        const auto gr = graph.toFloat().reduced (6.f, 8.f);
        g.setFont (FontOptions (9.f));
        for (float hz : { 30.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
        {
            const float x = toGraph (hz, 0).x; g.setColour (c.grid); g.drawVerticalLine ((int) x, gr.getY(), gr.getBottom());
            g.setColour (c.textDim); g.drawText (hz >= 1000.f ? String (hz / 1000.f, hz < 10000.f && std::fmod (hz, 1000.f) != 0.f ? 1 : 0) + "k" : String ((int) hz), (int) x + 2, (int) gr.getBottom() - 11, 30, 10, Justification::left);
        }
        for (float db : { -18.f, -12.f, -6.f, 0.f, 6.f, 12.f, 18.f })
        { const float y = toGraph (100, db).y; g.setColour (db == 0.f ? c.grid.brighter (0.3f) : c.grid); g.drawHorizontalLine ((int) y, gr.getX(), gr.getRight());
          if (tabIdx == TParametric || tabIdx == TDynamic) { g.setColour (c.textDim); g.drawText ((db > 0 ? "+" : "") + String ((int) db), (int) gr.getX() + 2, (int) y - 10, 30, 10, Justification::left); } }
        const int loc = viewLoc();
        const auto lc = locCol (loc);
        if (shown.valid && shown.loc == loc)
        {
            // analyzer: 0 dBFS at the top, 90 dB down to the bottom
            auto line = [&] (const std::vector<float>& s, Colour col, float th, bool fill)
            {
                if (s.empty()) return;
                Path p;
                for (int k = 0; k < (int) s.size(); ++k)
                {
                    const float x = toGraph (EqScope::binHz (k), 0).x, y = gr.getY() + gr.getHeight() * jlimit (0.f, 1.f, -s[(size_t) k] / 90.f);
                    k == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
                }
                if (fill) { Path f = p; f.lineTo (gr.getRight(), gr.getBottom()); f.lineTo (gr.getX(), gr.getBottom()); f.closeSubPath(); g.setColour (col.withAlpha (0.18f)); g.fillPath (f); }
                g.setColour (col); g.strokePath (p, PathStrokeType (th));
            };
            line (shown.pre, c.textDim.withAlpha (0.6f), 1.f, false);
            line (avgBox.getSelectedId() == 1 ? shown.post : avgPost, lc.withAlpha (0.7f), 1.f, true);
            if (holdBtn.getToggleState()) line (holdPost, c.silver.withAlpha (0.35f), 1.f, false);
            if (tabIdx == TParametric || tabIdx == TDynamic)
            {
                auto resp = [&] (const std::vector<float>& s, Colour col, float th)
                {
                    Path p; for (int k = 0; k < (int) s.size(); ++k) { const auto pt = toGraph (EqScope::binHz (k), s[(size_t) k]); k == 0 ? p.startNewSubPath (pt) : p.lineTo (pt); }
                    g.setColour (col); g.strokePath (p, PathStrokeType (th));
                };
                if (tabIdx == TParametric) resp (shown.bandResp, c.silver.withAlpha (0.5f), 1.f);
                resp (shown.resp, c.red, 2.f);
            }
            g.setColour (c.silver); g.setFont (FontOptions (10.f, Font::bold));
            g.drawText ("POST peak " + String (roundToInt (shown.domHz)) + " Hz  " + String (shown.domDb, 1) + " dBFS   (grey = PRE, colour = POST)", graph.getX() + 8, graph.getY() + 4, 420, 12, Justification::left);
        }
        // the points of this place
        if (tabIdx == TParametric)
            for (int i = 0; i < 16; ++i)
            {
                if (raw (m07t::eid (i, "on")) < 0.5f || roundToInt (raw (m07t::eid (i, "location"))) != loc) continue;
                const int t = roundToInt (raw (m07t::eid (i, "type")));
                const auto pt = toGraph (raw (m07t::eid (i, "freq")), tn::eqHasGain (t) ? raw (m07t::eid (i, "gain")) : 0.f);
                g.setColour (i == bandSel ? c.hot : lc); g.fillEllipse (pt.x - 6.f, pt.y - 6.f, 12.f, 12.f);
                g.setColour (c.ground); g.setFont (FontOptions (9.f, Font::bold)); g.drawText (String (i + 1), (int) pt.x - 6, (int) pt.y - 6, 12, 12, Justification::centred);
            }
        if (tabIdx == TParametric)
        { g.setColour (c.textDim); g.setFont (FontOptions (10.f)); g.drawText ("drag = frequency / gain, wheel = Q, double-click = new band here, right-click = delete", graph.getRight() - 470, graph.getBottom() + 2, 470, 12, Justification::right); }
        // ---- multiband overview: the 4 bands, their ranges and levels
        if (tabIdx == TMultiband)
        {
            const char* nm[4] { "SUB", "LOW", "MID", "HIGH" };
            const Colour bc[4] { c.part[3], c.part[1], c.part[2], c.part[0] };
            const float x1 = raw ("mb_xover1"), x2 = jmax (raw ("mb_xover2"), x1 * 1.25f), x3 = jmax (raw ("mb_xover3"), x2 * 1.25f);
            const float lo[4] { 20.f, x1, x2, x3 }, hi[4] { x1, x2, x3, 20000.f };
            for (int b = 0; b < 4; ++b)
            {
                const float xa = toGraph (lo[b], 0).x, xb = toGraph (hi[b], 0).x;
                const auto r = Rectangle<float> (xa + 1, (float) graph.getY() + 20.f, xb - xa - 2, 26.f);
                g.setColour (bc[b].withAlpha (0.35f)); g.fillRoundedRectangle (r, 3.f);
                g.setColour (c.silver); g.setFont (FontOptions (10.f, Font::bold));
                const auto fmt = [] (float hz) { return hz >= 1000.f ? String (hz / 1000.f, 1) + "k" : String ((int) hz); };
                g.drawText (String (nm[b]) + " " + fmt (lo[b]) + "-" + fmt (hi[b]) + (shown.valid ? "  " + String (shown.mbLevel[(size_t) b], 1) + " dB" : String()), r.toNearestInt(), Justification::centred);
            }
        }
        if (tabIdx == TStereo && shown.valid)
        {
            const int x = getWidth() - 370, y = 330;
            g.setColour (c.panel); g.fillRoundedRectangle (Rectangle<int> (x, y, 370, getHeight() - y).toFloat(), 4.f);
            g.setColour (c.silver); g.setFont (FontOptions (11.f, Font::bold));
            g.drawText ("CORRELATION " + String (shown.corr, 2) + (shown.corr < 0.f ? "   (phase problem: mono will lose energy)" : ""), x + 10, y + 8, 350, 14, Justification::left);
            const char* nm[3] { "LOW", "MID", "HIGH" };
            for (int b = 0; b < 3; ++b)
            {
                const int yy = y + 40 + b * 46;
                g.setColour (c.textDim); g.drawText (String (nm[b]) + "  corr " + String (shown.corrBand[(size_t) b], 2) + "   side / mid " + String (shown.sideRatio[(size_t) b], 2), x + 10, yy, 350, 14, Justification::left);
                g.setColour (c.grid); g.fillRect (x + 10, yy + 18, 340, 8);
                const float u = (shown.corrBand[(size_t) b] + 1.f) * 0.5f;
                g.setColour (shown.corrBand[(size_t) b] < 0.f ? c.red : c.silver); g.fillRect ((float) x + 10.f + 340.f * u - 3.f, (float) yy + 16.f, 6.f, 12.f);
            }
        }
    }
}
