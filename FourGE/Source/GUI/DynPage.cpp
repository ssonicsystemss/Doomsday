#include "DynPage.h"
#include "Palette.h"

using namespace juce;

namespace fge::gui
{
    namespace dy = fge::dsp::dyn;
    namespace
    {
        int idOf (const String& id) { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; }
        Colour locCol (int loc)
        {
            const auto& c = palette();
            switch (loc) { case 1: return c.part[0]; case 2: return c.part[1]; case 3: return c.part[2]; case 4: return c.part[3]; case 5: return c.part[5];
                           case 0: return c.red; case 15: return c.silver; default: return c.part[4]; }
        }
        const char* tabNames[DynPage::NumTabs] { "COMPRESSOR", "MULTIBAND", "TRANSIENT", "CLIPPER", "LIMITER", "CREST & LEVEL" };
        const int kinds[DynPage::NumTabs] { m08t::KCmp, -1, m08t::KTs, m08t::KClp, m08t::KLim, m08t::KCrest };
    }

    // ===================================================================== factories
    Component* DynPage::knob (const String& pid, const String& label, Colour col, bool bip, int d)
    {
        if (idOf (pid) < 0) return nullptr;
        auto k = std::make_unique<doomsday::flat::Knob> (proc.apvts, pid, label, col, bip, d);
        k->slider.setTooltip (fge::tooltip (idOf (pid)));
        auto* r = k.get(); addAndMakeVisible (r); byId.insert ({ pid, r }); ctl.push_back (std::move (k)); return r;
    }
    Component* DynPage::toggle (const String& pid, const String& text, Colour col)
    {
        if (idOf (pid) < 0) return nullptr;
        auto t = std::make_unique<doomsday::flat::Toggle> (proc.apvts, pid, text, text.length() <= 3 && text != "ON" ? doomsday::flat::Toggle::Chip : doomsday::flat::Toggle::Pill, col);
        t->setTooltip (fge::tooltip (idOf (pid)));
        auto* r = t.get(); addAndMakeVisible (r); byId.insert ({ pid, r }); ctl.push_back (std::move (t)); return r;
    }
    Component* DynPage::combo (const String& pid, const String& caption)
    {
        const int idx = idOf (pid);
        if (idx < 0) return nullptr;
        auto cb = std::make_unique<ComboBox>();
        cb->addItemList (def (idx).choices(), 1);
        comboAtt[cb.get()] = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, pid, *cb);
        cb->setTooltip (fge::tooltip (idx));
        auto* r = cb.get(); addAndMakeVisible (r); byId.insert ({ pid, r }); if (caption.isNotEmpty()) captions[r] = caption; ctl.push_back (std::move (cb)); return r;
    }
    void DynPage::forget()
    {
        for (auto& c : ctl) comboAtt.erase (c.get());
        byId.clear(); captions.clear(); layoutMap.clear(); titles.clear(); ctl.clear();
    }
    Component* DynPage::findControl (const String& pid) { auto it = byId.find (pid); return it == byId.end() ? nullptr : it->second; }
    StringArray DynPage::visibleIds() const { StringArray s; for (auto& [k, c] : byId) if (c->isVisible()) s.addIfNotAlreadyThere (k); return s; }
    float DynPage::raw (const String& pid) const { auto* v = proc.apvts.getRawParameterValue (pid); return v != nullptr ? v->load() : 0.f; }
    int DynPage::kind() const { return kinds[jlimit (0, NumTabs - 1, tabIdx)]; }
    int DynPage::meterIndex() const
    {
        switch (kind()) { case m08t::KCmp: return sel; case m08t::KTs: return 4 + sel; case m08t::KClp: return 9 + sel; case m08t::KLim: return 11 + sel; case m08t::KCrest: return 13; default: return -1; }
    }

    // ===================================================================== page
    DynPage::DynPage (GeProcessor& p, doomsday::flat::FlatLookAndFeel& l) : proc (p), lnf (l)
    {
        for (int i = 0; i < NumTabs; ++i) { tabBtn[(size_t) i].setButtonText (tabNames[i]); tabBtn[(size_t) i].onClick = [this, i] { showTab (i); }; addAndMakeVisible (tabBtn[(size_t) i]); }
        for (int i = 0; i < 5; ++i) { selBtn[(size_t) i].onClick = [this, i] { select (i); }; addChildComponent (selBtn[(size_t) i]); }
        presetBtn.setTooltip ("Module 08 presets (EARLY / MODERN / RAWSTYLE / EXPERIMENTAL) - the whole dynamics chain incl. where each processor sits.");
        saveBtn.setTooltip ("Save the selected processor or the whole module."); loadBtn.setTooltip ("Load a processor (into the selected one) or a whole module file.");
        abBtn.setTooltip ("A / B: switch between two versions of the whole module.");
        copyBtn.setTooltip ("Copy the selected processor."); pasteBtn.setTooltip ("Paste into the selected processor (only the same kind: a compressor never lands on a limiter).");
        resetBtn.setTooltip ("Reset the selected processor (OFF)."); maxBtn.setTooltip ("Clear the max gain reduction and the clip warning.");
        presetBtn.onClick = [this] { showPresetMenu(); };
        saveBtn.onClick = [this] { saveFile(); }; loadBtn.onClick = [this] { loadFile(); };
        abBtn.onClick = [this] { compare.toggle (proc); abBtn.setButtonText (compare.onB ? "A / [B]" : "[A] / B"); };
        copyBtn.onClick = [this] { if (kind() >= 0) clip = m08t::procToXml (proc, kind(), sel); };
        pasteBtn.onClick = [this] { if (clip && kind() >= 0) m08t::pasteProc (proc, kind(), sel, *clip); };
        resetBtn.onClick = [this] { if (kind() >= 0) m08t::resetProc (proc, kind(), sel); };
        maxBtn.onClick = [this] { proc.getEngine().resetMeterMax(); };
        for (Component* c : std::initializer_list<Component*> { &presetBtn, &saveBtn, &loadBtn, &abBtn, &copyBtn, &pasteBtn, &resetBtn, &maxBtn }) addAndMakeVisible (c);
        addChildComponent (clipEd);
        clipEd.onChange = [this] (const dsp::dist::CurveData& c, bool newStep)
        {
            if (kind() != m08t::KClp) return;
            proc.setClipCurve (sel, c, newStep);
            if (newStep && roundToInt (raw (id ("type"))) != dy::CCustom) proc.setParamInUndoStep (id ("type"), (float) dy::CCustom);
        };
        scope.onReady = [this] { adoptScope(); };
        showTab (TComp);
        startTimerHz (15);
    }
    DynPage::~DynPage() { stopTimer(); comboAtt.clear(); }

    void DynPage::showTab (int t) { selOf[(size_t) tabIdx] = sel; tabIdx = jlimit (0, NumTabs - 1, t); sel = selOf[(size_t) tabIdx]; build(); timerCallback(); }
    void DynPage::select (int s) { sel = jlimit (0, jmax (0, (kind() >= 0 ? m08t::countOf (kind()) : 1) - 1), s); build(); timerCallback(); }

    void DynPage::build()
    {
        forget();
        const auto& pc = palette();
        const int kh = doomsday::flat::Knob::heightFor (26);
        auto knobRow = [&] (std::initializer_list<std::tuple<String, String, bool>> ks, int x, int y, Colour col, int step = 80)
        { int k = 0; for (auto& [pid, lab, bip] : ks) place (knob (pid, lab, col, bip), x + (k++) * step, y, step - 4, kh); };
        auto comboRow = [&] (std::initializer_list<std::pair<String, String>> cs, int x, int y, int w = 150)
        { int k = 0; for (auto& [pid, cap] : cs) place (combo (pid, cap), x + (k++) * (w + 6), y, w, 22); };
        const int kd = kind(), n = kd >= 0 ? m08t::countOf (kd) : 0;
        const char* sn = kd == m08t::KCmp ? "COMP " : kd == m08t::KTs ? "TRANS " : kd == m08t::KClp ? "CLIP " : "LIMIT ";
        for (int i = 0; i < 5; ++i)
        {
            selBtn[(size_t) i].setVisible (kd >= 0 && kd != m08t::KCrest && i < n);
            selBtn[(size_t) i].setButtonText (String (sn) + String (i + 1));
        }
        for (Component* c : std::initializer_list<Component*> { &copyBtn, &pasteBtn, &resetBtn }) c->setVisible (kd >= 0);
        clipEd.setVisible (kd == m08t::KClp);
        const int y0 = 74, yk = 120;
        const auto col = kd >= 0 ? locCol (roundToInt (raw (kd == m08t::KCrest ? String ("crest_location") : id ("location")))) : pc.part[2];
        if (kd == m08t::KCmp)
        {
            titles.push_back ({ { 440, 36, 400, 14 }, "COMPRESSOR " + String (sel + 1) });
            place (toggle (id ("on"), "ON", col), 0, y0, 60, 22);
            comboRow ({ { id ("location"), "LOCATION" }, { id ("position"), "POSITION" }, { id ("type"), "TYPE" }, { id ("detection"), "DETECTION" } }, 70, y0, 120);
            knobRow ({ { id ("threshold"), "THRESHOLD", false }, { id ("ratio"), "RATIO", false }, { id ("attack"), "ATTACK", false }, { id ("release"), "RELEASE", false },
                       { id ("knee"), "KNEE", false }, { id ("input"), "INPUT", true }, { id ("output"), "OUTPUT", true }, { id ("mix"), "MIX", false } }, 0, yk, col);
            place (combo (id ("auto_release"), "AUTO RELEASE"), 0, yk + kh + 24, 150, 22);
            place (combo (id ("sc_source"), "SIDECHAIN SOURCE"), 160, yk + kh + 24, 150, 22);
            knobRow ({ { id ("dry_level"), "DRY (PAR.)", false }, { id ("wet_level"), "WET (PAR.)", false }, { id ("lookahead"), "LOOKAHEAD", false }, { id ("stereo_link"), "ST. LINK", false },
                       { id ("sc_hpf"), "SC HPF", false }, { id ("sc_lpf"), "SC LPF", false }, { id ("sc_gain"), "SC GAIN", true }, { id ("sc_mix"), "SC MIX", false } }, 320, yk + kh + 8, col, 68);
        }
        else if (kd == m08t::KTs)
        {
            titles.push_back ({ { 440, 36, 400, 14 }, "TRANSIENT SHAPER " + String (sel + 1) + "  (+ = more, - = less; a real envelope, not a fixed curve)" });
            place (toggle (id ("on"), "ON", col), 0, y0, 60, 22);
            comboRow ({ { id ("location"), "LOCATION" }, { id ("position"), "POSITION" }, { id ("detection"), "DETECTION" }, { id ("mode"), "MODE" } }, 70, y0, 120);
            knobRow ({ { id ("attack"), "ATTACK", true }, { id ("sustain"), "SUSTAIN", true }, { id ("sensitivity"), "SENSITIVITY", false }, { id ("time"), "TIME", false },
                       { id ("release"), "RELEASE", false }, { id ("mix"), "MIX", false }, { id ("output"), "OUTPUT", true } }, 0, yk, col);
        }
        else if (kd == m08t::KClp)
        {
            titles.push_back ({ { 440, 36, 400, 14 }, "CLIPPER " + String (sel + 1) + "  (CUSTOM CURVE: draw it on the right)" });
            place (toggle (id ("on"), "ON", col), 0, y0, 60, 22);
            comboRow ({ { id ("location"), "LOCATION" }, { id ("position"), "POSITION" }, { id ("type"), "TYPE" }, { id ("oversampling"), "OVERSAMPLING" } }, 70, y0, 120);
            knobRow ({ { id ("input"), "INPUT", true }, { id ("ceiling"), "CEILING", false }, { id ("drive"), "DRIVE", false }, { id ("symmetry"), "SYMMETRY", true },
                       { id ("softness"), "SOFTNESS", false }, { id ("mix"), "MIX", false }, { id ("output"), "OUTPUT", true } }, 0, yk, col);
            clipEd.setCurve (proc.distData().clip[(size_t) sel]);
        }
        else if (kd == m08t::KLim)
        {
            titles.push_back ({ { 440, 36, 400, 14 }, "LIMITER " + String (sel + 1) + "  (the ceiling is never passed)" });
            place (toggle (id ("on"), "ON", col), 0, y0, 60, 22);
            comboRow ({ { id ("location"), "LOCATION" }, { id ("position"), "POSITION" }, { id ("mode"), "MODE" } }, 70, y0, 120);
            place (toggle (id ("true_peak"), "TRUE PEAK", col), 450, y0, 100, 22);
            place (combo (id ("oversampling"), "TP OVERSAMPLING"), 560, y0, 100, 22);
            knobRow ({ { id ("ceiling"), "CEILING", false }, { id ("input"), "INPUT", true }, { id ("release"), "RELEASE", false }, { id ("lookahead"), "LOOKAHEAD", false },
                       { id ("attack"), "ATTACK", false }, { id ("knee"), "KNEE", false }, { id ("stereo_link"), "ST. LINK", false }, { id ("mix"), "MIX", false } }, 0, yk, col);
        }
        else if (kd == m08t::KCrest)
        {
            titles.push_back ({ { 440, 36, 500, 14 }, "CREST FACTOR CONTROL  (peak / RMS: low = dense and loud, high = spiky and punchy)" });
            place (toggle ("crest_on", "ON", col), 0, y0, 60, 22);
            comboRow ({ { "crest_location", "LOCATION" }, { "crest_position", "POSITION" } }, 70, y0, 120);
            knobRow ({ { "crest_target", "TARGET", false }, { "crest_punch", "PUNCH", true }, { "crest_body", "BODY", true }, { "crest_tail", "TAIL", true },
                       { "crest_sensitivity", "SENSITIVITY", false }, { "crest_mix", "MIX", false } }, 0, yk, col);
            titles.push_back ({ { 0, yk + kh + 10, 500, 14 }, "LEVEL (all dynamics processors)" });
            place (combo ("dyn_auto_gain", "AUTO GAIN"), 0, yk + kh + 44, 150, 22);
            place (toggle ("dyn_level_match", "LEVEL MATCH", col), 160, yk + kh + 44, 120, 22);
            place (combo ("dyn_order", "ORDER AT ONE PLACE"), 290, yk + kh + 44, 320, 22);
        }
        else
        {
            titles.push_back ({ { 440, 36, 600, 14 }, "MULTIBAND DYNAMICS (module 07 engine) + BAND LINK" });
            place (toggle ("mb_on", "ON", col), 0, y0, 60, 22);
            comboRow ({ { "mb_location", "LOCATION" }, { "mb_position", "POSITION" }, { "mb_crossover", "CROSSOVER" }, { "mb_link", "BAND LINK" } }, 70, y0, 140);
            knobRow ({ { "mb_xover1", "SUB / LOW", false }, { "mb_xover2", "LOW / MID", false }, { "mb_xover3", "MID / HIGH", false } }, 670, y0 - 16, col);
            const char* nm[4] { "SUB", "LOW", "MID", "HIGH" };
            const Colour bc[4] { pc.part[3], pc.part[1], pc.part[2], pc.part[0] };
            for (int b = 0; b < 4; ++b)
            {
                const String o = "mb" + String (b + 1) + "_";
                const int y = 150 + b * 130;
                titles.push_back ({ { 0, y - 18, 200, 14 }, String (nm[b]) + " BAND" });
                place (combo (o + "dynamics", ""), 0, y, 130, 22);
                knobRow ({ { o + "threshold", "THRESHOLD", false }, { o + "ratio", "RATIO", false }, { o + "attack", "ATTACK", false }, { o + "release", "RELEASE", false },
                           { o + "knee", "KNEE", false }, { o + "makeup", "MAKEUP", true }, { o + "mix", "MIX", false }, { o + "tr_attack", "TR ATTACK", true }, { o + "tr_sustain", "TR SUSTAIN", true } },
                         140, y - 10, bc[b], 76);
            }
        }
        if (kd >= 0 && kd != m08t::KCrest)
        {
            place (combo ("dyn_auto_gain", "AUTO GAIN"), 0, 330, 150, 22);
            place (toggle ("dyn_level_match", "LEVEL MATCH", pc.silver), 160, 330, 120, 22);
            place (combo ("dyn_order", "ORDER AT ONE PLACE"), 290, 330, 320, 22);
        }
        updateEnabled();
        resized();
        repaint();
    }

    void DynPage::updateEnabled()
    {
        auto en = [&] (const String& pid, bool on) { for (auto [it, end] = byId.equal_range (pid); it != end; ++it) { it->second->setEnabled (on); it->second->setAlpha (on ? 1.f : 0.4f); } };
        const int kd = kind();
        if (kd < 0)
        {
            for (int b = 1; b <= 4; ++b)
            {
                const String o = "mb" + String (b) + "_";
                const int dm = roundToInt (raw (o + "dynamics"));
                const bool on = dm != dsp::tone::MdOff, tr = dm == dsp::tone::MdTransient;
                for (auto* k : { "threshold", "ratio", "knee", "makeup", "mix" }) en (o + k, on && ! tr);
                en (o + "ratio", on && ! tr && dm != dsp::tone::MdLimit);
                en (o + "attack", on); en (o + "release", on); en (o + "tr_attack", tr); en (o + "tr_sustain", tr);
            }
            return;
        }
        if (kd == m08t::KCrest) return;
        const int loc = roundToInt (raw (id ("location")));
        en (id ("position"), loc != dy::LMaster && loc != 14);                   // (MASTER = after everything, SCREECH = no distortion of its own)
        if (kd == m08t::KCmp)
        {
            const int t = roundToInt (raw (id ("type")));
            const bool sc = loc <= 5, scOn = sc && (roundToInt (raw (id ("sc_source"))) > 0 || t == dy::Sidechain);
            en (id ("lookahead"), loc == dy::LMaster);
            en (id ("dry_level"), t == dy::Parallel); en (id ("wet_level"), t == dy::Parallel);
            en (id ("sc_source"), sc);
            for (auto* k : { "sc_gain", "sc_mix" }) en (id (k), scOn);
            en (id ("sc_mix"), scOn && t != dy::Sidechain);
            en (id ("release"), roundToInt (raw (id ("auto_release"))) == dy::ArManual);
        }
        if (kd == m08t::KLim)
        {
            en (id ("lookahead"), loc == dy::LMaster);
            en (id ("oversampling"), raw (id ("true_peak")) > 0.5f || roundToInt (raw (id ("mode"))) == 4);
        }
        if (kd == m08t::KTs) en (id ("time"), true);
        if (kd == m08t::KClp) clipEd.setEditable (roundToInt (raw (id ("type"))) == dy::CCustom);
    }

    StringArray DynPage::warnings() const
    {
        StringArray w;
        const int mi = meterIndex();
        if (mi >= 0)
        {
            auto& m = proc.getEngine().m8Meters[(size_t) mi];
            if (m.clip.load()) w.add ("CLIPPING: the output went over 0 dBFS");
            if (kind() == m08t::KCmp && m.grMax.load() < -15.f) w.add ("HEAVY COMPRESSION: more than 15 dB gain reduction - the punch flattens");
            if (kind() == m08t::KLim && m.grMax.load() < -8.f) w.add ("THE LIMITER WORKS HARD: more than 8 dB - lower the INPUT or use a clipper before it");
        }
        if (shown.valid)
        {
            const float jump = shown.rms - shown.dryRms;
            if (std::abs (jump) > 6.f && roundToInt (raw ("dyn_auto_gain")) == 0) w.add ("LEVEL JUMP " + String (jump, 1) + " dB vs no dynamics - try AUTO GAIN to judge the shape");
            if (shown.crest < 4.f) w.add ("VERY DENSE: crest " + String (shown.crest, 1) + " dB - little punch left");
            if (shown.peak > -0.1f) w.add ("THE KICK PEAKS AT 0 dBFS");
        }
        return w;
    }

    void DynPage::requestScope()
    {
        auto q = std::make_unique<DynScope::Request>();
        q->f = proc.frame(); q->bank = proc.shapes(); q->tables = proc.currentTables(); q->dist = proc.distData();
        q->kind = jmax (0, kind()); q->index = kind() == m08t::KCrest ? 0 : sel; q->note = proc.auditionNote;
        scope.ask (std::move (q));
    }
    void DynPage::adoptScope()
    {
        shown = scope.result();
        if (shown.valid && shown.kind == m08t::KClp && shown.index == sel) clipEd.setResult (shown.curveX, shown.curveY);
        repaint();
    }

    void DynPage::timerCallback()
    {
        if (! isShowing() && lastKind != -2) return;
        const auto& pc = palette();
        for (int i = 0; i < 5; ++i)
        {
            auto& b = selBtn[(size_t) i];
            const bool on = kind() >= 0 && kind() != m08t::KCrest && i < m08t::countOf (kind()) && raw (m08t::pid (kind(), i, "on")) > 0.5f;
            b.setColour (TextButton::buttonColourId, i == sel ? pc.red : on ? pc.control.brighter (0.35f) : pc.control);
            b.setColour (TextButton::textColourOffId, i == sel ? pc.ground : pc.silver);
        }
        for (int i = 0; i < NumTabs; ++i)
        {
            tabBtn[(size_t) i].setColour (TextButton::buttonColourId, i == tabIdx ? pc.red.withAlpha (0.8f) : pc.control);
            tabBtn[(size_t) i].setColour (TextButton::textColourOffId, i == tabIdx ? pc.ground : pc.silver);
        }
        pasteBtn.setEnabled (clip != nullptr && clip->getIntAttribute ("kind", -1) == kind());
        updateEnabled();
        if (kind() == m08t::KClp && proc.distDataVersion() != lastDist) clipEd.setCurve (proc.distData().clip[(size_t) sel]);
        const auto& f = proc.frame();
        if (kind() != lastKind || sel != lastSel || proc.shapesVersion() != lastShapes || proc.distDataVersion() != lastDist || ! std::equal (f.v.begin(), f.v.end(), lastAsked.v.begin()))
        {
            lastAsked = f; lastShapes = proc.shapesVersion(); lastDist = proc.distDataVersion(); lastKind = kind(); lastSel = sel;
            if (kind() >= 0) requestScope();
        }
        repaint (meterArea);
        repaint (histArea.withHeight (16));
    }

    // ===================================================================== presets / files
    void DynPage::showPresetMenu()
    {
        PopupMenu m; std::map<String, PopupMenu> cats; StringArray order;
        const auto& l = m08t::presets();
        for (int i = 0; i < (int) l.size(); ++i) { const String c (l[(size_t) i].category); if (! order.contains (c)) order.add (c); cats[c].addItem (i + 1, l[(size_t) i].name); }
        for (auto& c : order) m.addSubMenu (c, cats[c]);
        SafePointer<DynPage> self (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetBtn), [self] (int r) { if (self != nullptr && r > 0) { m08t::apply (self->proc, m08t::presets()[(size_t) r - 1]); self->build(); } });
    }
    void DynPage::saveFile()
    {
        PopupMenu m;
        if (kind() >= 0) m.addItem (1, "Save " + String (tabNames[tabIdx]) + (kind() == m08t::KCrest ? String() : " " + String (sel + 1)) + " (.4gdyn)");
        m.addItem (2, "Save the whole module 08 (.4gm08)");
        SafePointer<DynPage> self (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&saveBtn), [self] (int r)
        {
            if (self == nullptr || r <= 0) return;
            const bool one = r == 1;
            self->chooser = std::make_unique<FileChooser> ("Save", files::folder ("Dynamics"), one ? "*.4gdyn" : "*.4gm08");
            self->chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [self, one] (const FileChooser& fc)
            {
                if (self == nullptr || fc.getResult() == File()) return;
                auto x = one ? m08t::procToXml (self->proc, self->kind(), self->sel) : m08t::moduleToXml (self->proc);
                files::writeXml (fc.getResult().withFileExtension (m08t::extensionOf (x->getTagName())), *x);
            });
        });
    }
    void DynPage::loadFile()
    {
        chooser = std::make_unique<FileChooser> ("Load", files::folder ("Dynamics"), "*.4gdyn;*.4gm08");
        SafePointer<DynPage> self (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [self] (const FileChooser& fc)
        {
            if (self == nullptr || fc.getResult() == File()) return;
            if (auto x = files::readXml (fc.getResult()))
            {
                // a single processor file goes into the tab of its kind
                if (x->hasTagName ("DYN4GE")) { const int k = x->getIntAttribute ("kind", -1); for (int t = 0; t < NumTabs; ++t) if (kinds[t] == k && t != self->tabIdx) self->showTab (t); }
                m08t::load (self->proc, *x, jmax (0, self->kind()), self->sel); self->build();
            }
        });
    }

    // ===================================================================== layout / paint
    void DynPage::resized()
    {
        const int w = getWidth(), h = getHeight();
        for (int i = 0; i < NumTabs; ++i) tabBtn[(size_t) i].setBounds (i * 112, 0, 108, 26);
        abBtn.setBounds (w - 84, 0, 84, 26); loadBtn.setBounds (w - 148, 0, 60, 26); saveBtn.setBounds (w - 212, 0, 60, 26); presetBtn.setBounds (w - 300, 0, 84, 26);
        for (int i = 0; i < 5; ++i) selBtn[(size_t) i].setBounds (i * 84, 32, 80, 22);
        copyBtn.setBounds (w - 3 * 76, 32, 72, 22); pasteBtn.setBounds (w - 2 * 76, 32, 72, 22); resetBtn.setBounds (w - 76, 32, 72, 22);
        curveArea = { w - 330, 66, 330, 270 };
        histArea = kind() < 0 ? Rectangle<int> (0, 690, w, 0) : Rectangle<int> (0, 380, w - 200, h - 380);
        meterArea = kind() < 0 ? Rectangle<int>() : Rectangle<int> (w - 190, 380, 190, h - 380);
        maxBtn.setVisible (kind() >= 0);
        maxBtn.setBounds (meterArea.getX() + 10, meterArea.getBottom() - 26, 100, 22);
        clipEd.setBounds (curveArea);
        for (auto& [c, r] : layoutMap) c->setBounds (r);
    }

    void DynPage::paint (Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.textDim); g.setFont (FontOptions (10.5f, Font::bold));
        for (auto& [comp, cap] : captions) if (comp->isVisible()) g.drawText (cap, comp->getX(), comp->getY() - 12, comp->getWidth() + 80, 11, Justification::centredLeft);
        for (auto& [r, t] : titles) g.drawText (t, r, Justification::centredLeft);
        const int kd = kind();
        if (kd < 0) return;
        const bool mine = shown.valid && shown.kind == jmax (0, kd) && shown.index == (kd == m08t::KCrest ? 0 : sel);
        // ---- the static curve (compressor: dB in -> dB out; limiter: linear) - the clipper draws its own (editable) curve
        if (kd == m08t::KCmp || kd == m08t::KLim || kd == m08t::KTs || kd == m08t::KCrest)
        {
            const auto r = curveArea.toFloat();
            g.setColour (c.ground); g.fillRoundedRectangle (r, 4.f);
            const auto gr = r.reduced (24.f, 18.f);
            g.setFont (FontOptions (9.f));
            if (kd == m08t::KCmp)
            {
                auto pt = [&] (float in, float out) { return Point<float> (gr.getX() + gr.getWidth() * (in + 60.f) / 60.f, gr.getBottom() - gr.getHeight() * jlimit (0.f, 1.2f, (out + 60.f) / 60.f)); };
                for (float d : { -48.f, -36.f, -24.f, -12.f, 0.f })
                { g.setColour (c.grid); g.drawLine (Line<float> (pt (d, -60.f), pt (d, 0.f)), 0.6f); g.drawLine (Line<float> (pt (-60.f, d), pt (0.f, d)), 0.6f);
                  g.setColour (c.textDim); g.drawText (String ((int) d), (int) pt (d, -60.f).x - 12, (int) gr.getBottom() + 2, 24, 10, Justification::centred); }
                g.setColour (c.textDim.withAlpha (0.5f)); g.drawLine (Line<float> (pt (-60.f, -60.f), pt (0.f, 0.f)), 0.8f);
                const float thr = raw (id ("threshold"));
                g.setColour (c.textDim); g.drawLine (Line<float> (pt (thr, -60.f), pt (thr, 0.f)), 0.6f);
                if (mine && shown.curveX.size() > 1)
                { Path p; for (size_t k = 0; k < shown.curveX.size(); ++k) { const auto s = pt (shown.curveX[k], shown.curveY[k]); k == 0 ? p.startNewSubPath (s) : p.lineTo (s); } g.setColour (c.red); g.strokePath (p, PathStrokeType (2.f)); }
                g.setColour (c.silver); g.setFont (FontOptions (10.f, Font::bold));
                g.drawText ("IN (dB) -> OUT (dB)   THRESHOLD " + String (thr, 1) + "  RATIO " + String (raw (id ("ratio")), 1) + ":1", r.toNearestInt().reduced (6, 3), Justification::topLeft);
            }
            else if (kd == m08t::KLim)
            {
                auto pt = [&] (float x, float y) { return Point<float> (gr.getCentreX() + gr.getWidth() * 0.5f * x / 1.5f, gr.getCentreY() - gr.getHeight() * 0.5f * y / 1.5f); };
                g.setColour (c.grid); g.drawLine (Line<float> (pt (-1.5f, 0), pt (1.5f, 0)), 0.8f); g.drawLine (Line<float> (pt (0, -1.5f), pt (0, 1.5f)), 0.8f);
                if (mine && shown.curveX.size() > 1)
                { Path p; bool st = false; for (size_t k = 0; k < shown.curveX.size(); ++k) { if (std::abs (shown.curveX[k]) > 1.5f) continue; const auto s = pt (shown.curveX[k], shown.curveY[k]); if (! st) { p.startNewSubPath (s); st = true; } else p.lineTo (s); }
                  g.setColour (c.red); g.strokePath (p, PathStrokeType (2.f)); }
                const float ce = Decibels::decibelsToGain (raw (id ("ceiling")));
                g.setColour (c.hot.withAlpha (0.6f)); g.drawLine (Line<float> (pt (-1.5f, ce), pt (1.5f, ce)), 1.f); g.drawLine (Line<float> (pt (-1.5f, -ce), pt (1.5f, -ce)), 1.f);
                g.setColour (c.silver); g.setFont (FontOptions (10.f, Font::bold));
                g.drawText ("IN -> OUT   CEILING " + String (raw (id ("ceiling")), 1) + " dB" + (raw (id ("true_peak")) > 0.5f ? "  TRUE PEAK" : ""), r.toNearestInt().reduced (6, 3), Justification::topLeft);
            }
            else
            {
                // transient / crest: the numbers of the kick (processed vs without dynamics)
                g.setColour (c.silver); g.setFont (FontOptions (11.f, Font::bold));
                g.drawText (kd == m08t::KTs ? "THE KICK (with / without dynamics)" : "CREST ANALYZER", r.toNearestInt().reduced (10, 6), Justification::topLeft);
                if (mine)
                {
                    const String rows[4] { "PEAK   " + String (shown.peak, 1) + " dBFS   (" + String (shown.dryPeak, 1) + ")", "RMS    " + String (shown.rms, 1) + " dBFS   (" + String (shown.dryRms, 1) + ")",
                                           "CREST  " + String (shown.crest, 1) + " dB   (" + String (shown.dryCrest, 1) + ")", kd == m08t::KCrest ? "TARGET " + String (raw ("crest_target"), 1) + " dB" : "MAX GAIN " + String (shown.maxGr, 1) + " dB" };
                    g.setFont (FontOptions (12.f));
                    for (int i = 0; i < 4; ++i) g.drawText (rows[i], (int) r.getX() + 14, (int) r.getY() + 34 + i * 22, (int) r.getWidth() - 20, 18, Justification::left);
                    // crest bar: dry vs processed vs target
                    const float bx = r.getX() + 14.f, bw = r.getWidth() - 28.f, by = r.getY() + 140.f;
                    g.setColour (c.grid); g.fillRect (bx, by, bw, 10.f);
                    auto mark = [&] (float v, Colour col) { g.setColour (col); g.fillRect (bx + bw * jlimit (0.f, 1.f, v / 30.f) - 2.f, by - 3.f, 4.f, 16.f); };
                    mark (shown.dryCrest, c.textDim); mark (shown.crest, c.red); if (kd == m08t::KCrest) mark (raw ("crest_target"), c.hot);
                    g.setColour (c.textDim); g.setFont (FontOptions (9.f)); g.drawText ("0 dB (dense)                    crest                   30 dB (spiky)", (int) bx, (int) by + 14, (int) bw, 10, Justification::centred);
                }
            }
        }
        // ---- the history: gain reduction (transient: its gain) + IN / OUT envelopes over the first 600 ms of a real render
        {
            const auto r = histArea.toFloat();
            g.setColour (c.ground); g.fillRoundedRectangle (r, 4.f);
            const auto gr = r.reduced (8.f, 18.f);
            for (int ms = 0; ms <= 600; ms += 100)
            { const float x = gr.getX() + gr.getWidth() * (float) ms / 600.f; g.setColour (c.grid); g.drawVerticalLine ((int) x, gr.getY(), gr.getBottom()); g.setColour (c.textDim); g.setFont (FontOptions (9.f)); g.drawText (String (ms) + " ms", (int) x + 2, (int) gr.getBottom() + 2, 50, 10, Justification::left); }
            if (mine)
            {
                auto line = [&] (const std::vector<float>& v, float lo, float hi, Colour col, float th, bool fill)
                {
                    if (v.size() < 2) return;
                    Path p;
                    for (size_t k = 0; k < v.size(); ++k)
                    { const float x = gr.getX() + gr.getWidth() * (float) k / (float) (v.size() - 1), y = gr.getBottom() - gr.getHeight() * jlimit (0.f, 1.f, (v[k] - lo) / (hi - lo)); k == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y); }
                    if (fill) { Path f = p; f.lineTo (gr.getRight(), gr.getBottom()); f.lineTo (gr.getX(), gr.getBottom()); f.closeSubPath(); g.setColour (col.withAlpha (0.15f)); g.fillPath (f); }
                    g.setColour (col); g.strokePath (p, PathStrokeType (th));
                };
                if (kd == m08t::KCrest)
                    line (shown.crestHist, 0.f, 30.f, c.red, 1.5f, false);
                else
                {
                    line (shown.in, -60.f, 0.f, c.textDim.withAlpha (0.7f), 1.f, true);
                    line (shown.out, -60.f, 0.f, locCol (roundToInt (raw (id ("location")))).withAlpha (0.8f), 1.f, true);
                    if (kd == m08t::KTs) line (shown.gr, -18.f, 18.f, c.hot, 2.f, false);
                    else
                    {
                        // gain reduction hangs from the top: 0 dB at the top, -24 dB at the bottom
                        Path p;
                        for (size_t k = 0; k < shown.gr.size(); ++k)
                        { const float x = gr.getX() + gr.getWidth() * (float) k / (float) jmax<size_t> (1, shown.gr.size() - 1), y = gr.getY() + gr.getHeight() * jlimit (0.f, 1.f, -shown.gr[k] / 24.f); k == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y); }
                        g.setColour (c.hot); g.strokePath (p, PathStrokeType (2.f));
                    }
                }
                if (shown.gr.empty() && kd != m08t::KCrest)
                { g.setColour (c.textDim); g.setFont (FontOptions (11.f)); g.drawText ("(off, or on a layer: switch it on to see its gain)", gr.toNearestInt(), Justification::centred); }
            }
            g.setColour (c.silver); g.setFont (FontOptions (10.f, Font::bold));
            g.drawText (kd == m08t::KCrest ? "SHORT-TERM CREST (10 ms windows, 0..30 dB)" : kd == m08t::KTs ? "TRANSIENT GAIN (orange, +/-18 dB)  -  IN grey / OUT colour"
                                          : "GAIN REDUCTION (orange, 0..-24 dB from the top)  -  IN grey / OUT colour", histArea.reduced (8, 3), Justification::topLeft);
        }
        // ---- live meters (the plugin playing): IN / OUT peak, GR now / max / average + warnings
        {
            const auto r = meterArea.toFloat();
            g.setColour (c.panel); g.fillRoundedRectangle (r, 4.f);
            const int mi = meterIndex();
            if (mi >= 0)
            {
                auto& m = proc.getEngine().m8Meters[(size_t) mi];
                const float vals[3] { Decibels::gainToDecibels (m.inPk.load(), -60.f), Decibels::gainToDecibels (m.outPk.load(), -60.f), m.gr.load() };
                const char* nm[3] { "IN", "OUT", kind() == m08t::KTs ? "GAIN" : "GR" };
                for (int i = 0; i < 3; ++i)
                {
                    const float x = r.getX() + 14.f + (float) i * 40.f, top = r.getY() + 26.f, hh = r.getHeight() - 110.f;
                    g.setColour (c.grid); g.fillRect (x, top, 14.f, hh);
                    if (i < 2) { const float u = jlimit (0.f, 1.f, (vals[i] + 60.f) / 60.f); g.setColour (vals[i] > -0.1f ? c.red : c.silver); g.fillRect (x, top + hh * (1.f - u), 14.f, hh * u); }
                    else { const float u = jlimit (0.f, 1.f, std::abs (vals[2]) / 24.f); g.setColour (c.hot); if (vals[2] < 0.f) g.fillRect (x, top, 14.f, hh * u); else g.fillRect (x, top + hh * (1.f - u), 14.f, hh * u); }
                    g.setColour (c.textDim); g.setFont (FontOptions (9.f, Font::bold)); g.drawText (nm[i], (int) x - 8, (int) r.getY() + 10, 30, 12, Justification::centred);
                }
                g.setColour (c.silver); g.setFont (FontOptions (10.f));
                g.drawText ("MAX " + String (m.grMax.load(), 1) + " dB", (int) r.getX() + 130, (int) r.getY() + 30, 60, 12, Justification::left);
                g.drawText ("AVG " + String (m.grAvg.load(), 1) + " dB", (int) r.getX() + 130, (int) r.getY() + 46, 60, 12, Justification::left);
                if (m.clip.load()) { g.setColour (c.red); g.setFont (FontOptions (11.f, Font::bold)); g.drawText ("CLIP", (int) r.getX() + 130, (int) r.getY() + 66, 60, 14, Justification::left); }
            }
            const auto w = warnings();
            g.setFont (FontOptions (9.5f, Font::bold));
            int y = (int) r.getBottom() - 80;
            for (auto& s : w) { g.setColour (c.red); g.drawFittedText (s, (int) r.getX() + 8, y, (int) r.getWidth() - 16, 24, Justification::topLeft, 2); y -= 26; }
        }
    }
}
