#include "DistPage.h"
#include "Palette.h"

using namespace juce;

namespace fge::gui
{
    namespace dd = fge::dsp::dist;
    namespace
    {
        int idOf (const String& id) { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; }
        float rawOf (GeProcessor& p, const String& id) { auto* v = p.apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.f; }
        Colour locCol (int loc)
        {
            const auto& c = palette();
            switch (loc) { case dd::LocPunch: return c.part[0]; case dd::LocBody: return c.part[1]; case dd::LocTail: return c.part[2]; case dd::LocSub: return c.part[3];
                           case dd::LocTexture: return c.part[5]; case dd::LocGlobal: return c.red; default: return c.part[4]; }
        }
        // the controls each type really uses (label of SHAPE per type) - the others are hidden, never dummies
        struct TypeKnob { const char* id; const char* label; bool bip; };
        std::vector<TypeKnob> typeKnobs (int t)
        {
            switch (t)
            {
                case dd::Soft:      return { { "symmetry", "SYMMETRY", true }, { "threshold", "THRESHOLD", false }, { "shape", "KNEE", false } };
                case dd::Hard:      return { { "symmetry", "SYMMETRY", true }, { "ceiling", "CEILING", false } };
                case dd::Tube:      return { { "symmetry", "SYMMETRY", true }, { "even", "EVEN", false }, { "odd", "ODD", false }, { "shape", "WARMTH", false } };
                case dd::Diode:     return { { "symmetry", "SYMMETRY", true }, { "threshold", "THRESHOLD", false }, { "shape", "ASYMMETRY", false } };
                case dd::Wavefold:  return { { "symmetry", "SYMMETRY", true }, { "threshold", "THRESHOLD", false }, { "shape", "FOLD AMT", false }, { "fold_stages", "STAGES", false } };
                case dd::Bitcrush:  return { { "bits", "BITS", false }, { "sr_reduce", "SR REDUCE", false }, { "shape", "QUANT", false } };
                case dd::Rectifier: return { { "symmetry", "SYMMETRY", true }, { "shape", "AMOUNT", false } };
                case dd::Asym:      return { { "threshold", "POS THR", false }, { "neg_drive", "NEG DRIVE", false }, { "neg_threshold", "NEG THR", false } };
                case dd::Foldback:  return { { "symmetry", "SYMMETRY", true }, { "threshold", "THRESHOLD", false }, { "shape", "STEEPNESS", false } };
                case dd::Digital:   return { { "symmetry", "SYMMETRY", true }, { "ceiling", "CEILING", false }, { "shape", "HARDNESS", false } };
                default:            return { { "symmetry", "SYMMETRY", true } };
            }
        }
        const char* typeCombo (int t) { return t == dd::Bitcrush ? "dither" : t == dd::Rectifier ? "rect_mode" : t == dd::Custom ? "curve_interp" : nullptr; }
        const char* typeComboCaption (int t) { return t == dd::Bitcrush ? "DITHER" : t == dd::Rectifier ? "RECTIFIER" : "INTERPOLATION"; }
    }

    // ===================================================================== CUSTOM curve editor
    Point<float> CurveEditor::toScreen (float x, float y) const
    {
        const auto r = getLocalBounds().toFloat().reduced (8.f);
        return { r.getCentreX() + x * r.getWidth() * 0.5f, r.getCentreY() - y / 1.5f * r.getHeight() * 0.5f };
    }
    Point<float> CurveEditor::fromScreen (Point<float> p) const
    {
        const auto r = getLocalBounds().toFloat().reduced (8.f);
        return { jlimit (-1.f, 1.f, (p.x - r.getCentreX()) / (r.getWidth() * 0.5f)), jlimit (-1.5f, 1.5f, -(p.y - r.getCentreY()) / (r.getHeight() * 0.5f) * 1.5f) };
    }
    int CurveEditor::hit (Point<float> p) const
    {
        for (int i = 0; i < curve.n; ++i) if (toScreen (curve.x[(size_t) i], curve.y[(size_t) i]).getDistanceFrom (p) < 7.f) return i;
        return -1;
    }
    void CurveEditor::paint (Graphics& g)
    {
        const auto& c = palette();
        const auto r = getLocalBounds().toFloat();
        g.setColour (c.ground); g.fillRoundedRectangle (r, 4.f);
        g.setColour (c.grid);
        for (float v : { -1.f, -0.5f, 0.f, 0.5f, 1.f })
        {
            g.drawLine (Line<float> (toScreen (v, -1.5f), toScreen (v, 1.5f)), v == 0.f ? 1.2f : 0.6f);
            g.drawLine (Line<float> (toScreen (-1.f, v), toScreen (1.f, v)), v == 0.f ? 1.2f : 0.6f);
        }
        g.setColour (c.textDim.withAlpha (0.5f));
        g.drawLine (Line<float> (toScreen (-1.f, -1.f), toScreen (1.f, 1.f)), 0.8f);       // the clean line (input = output)
        // the REAL curve (computed in the background from the same tables the audio uses)
        if (cx.size() > 1)
        {
            Path p; bool started = false;
            for (size_t i = 0; i < cx.size(); ++i)
            {
                if (cx[i] < -1.f || cx[i] > 1.f) continue;
                const auto s = toScreen (cx[i], jlimit (-1.5f, 1.5f, cy[i]));
                if (! started) { p.startNewSubPath (s); started = true; } else p.lineTo (s);
            }
            g.setColour (c.red); g.strokePath (p, PathStrokeType (2.f));
        }
        if (editable)
        {
            for (int i = 0; i < curve.n; ++i)
            {
                const auto s = toScreen (curve.x[(size_t) i], curve.y[(size_t) i]);
                g.setColour (i == drag ? c.hot : c.silver); g.fillEllipse (s.x - 4.f, s.y - 4.f, 8.f, 8.f);
            }
            g.setColour (c.textDim); g.setFont (FontOptions (10.f));
            g.drawText ("drag points - double-click: add / remove", getLocalBounds().reduced (8, 6), Justification::bottomRight);
        }
    }
    void CurveEditor::mouseDown (const MouseEvent& e)
    {
        if (! editable) return;
        drag = hit (e.position); first = true;
        if (drag >= 0 && e.mods.isPopupMenu() && drag > 0 && drag < curve.n - 1)
        {
            for (int i = drag; i < curve.n - 1; ++i) { curve.x[(size_t) i] = curve.x[(size_t) i + 1]; curve.y[(size_t) i] = curve.y[(size_t) i + 1]; }
            --curve.n; drag = -1;
            if (onChange) onChange (curve, true);
            repaint();
        }
    }
    void CurveEditor::mouseDrag (const MouseEvent& e)
    {
        if (! editable || drag < 0) return;
        auto p = fromScreen (e.position);
        if (drag == 0) p.x = -1.f; else if (drag == curve.n - 1) p.x = 1.f;
        else p.x = jlimit (curve.x[(size_t) drag - 1] + 0.01f, curve.x[(size_t) drag + 1] - 0.01f, p.x);
        curve.x[(size_t) drag] = p.x; curve.y[(size_t) drag] = p.y;
        if (onChange) onChange (curve, first);
        first = false;
        repaint();
    }
    void CurveEditor::mouseDoubleClick (const MouseEvent& e)
    {
        if (! editable) return;
        const int h = hit (e.position);
        if (h > 0 && h < curve.n - 1)
        {
            for (int i = h; i < curve.n - 1; ++i) { curve.x[(size_t) i] = curve.x[(size_t) i + 1]; curve.y[(size_t) i] = curve.y[(size_t) i + 1]; }
            --curve.n;
        }
        else if (h < 0 && curve.n < dd::maxCurvePoints)
        {
            const auto p = fromScreen (e.position);
            int at = 1; while (at < curve.n && curve.x[(size_t) at] < p.x) ++at;
            if (at >= curve.n || p.x <= curve.x[(size_t) at - 1] + 0.01f || p.x >= curve.x[(size_t) at] - 0.01f) return;
            for (int i = curve.n; i > at; --i) { curve.x[(size_t) i] = curve.x[(size_t) i - 1]; curve.y[(size_t) i] = curve.y[(size_t) i - 1]; }
            curve.x[(size_t) at] = p.x; curve.y[(size_t) at] = p.y; ++curve.n;
        }
        else return;
        drag = -1;
        if (onChange) onChange (curve, true);
        repaint();
    }

    // ===================================================================== control factories
    Component* DistPage::knob (const String& id, const String& label, Colour col, bool bip, int d)
    {
        if (idOf (id) < 0) return nullptr;
        auto k = std::make_unique<doomsday::flat::Knob> (proc.apvts, id, label, col, bip, d);
        k->slider.setTooltip (fge::tooltip (idOf (id)));
        auto* r = k.get(); addAndMakeVisible (r); byId.insert ({ id, r }); stageCtl.push_back (std::move (k)); return r;
    }
    Component* DistPage::toggle (const String& id, const String& text, Colour col)
    {
        if (idOf (id) < 0) return nullptr;
        auto t = std::make_unique<doomsday::flat::Toggle> (proc.apvts, id, text, doomsday::flat::Toggle::Pill, col);
        t->setTooltip (fge::tooltip (idOf (id)));
        auto* r = t.get(); addAndMakeVisible (r); byId.insert ({ id, r }); stageCtl.push_back (std::move (t)); return r;
    }
    Component* DistPage::combo (const String& id, const String& caption)
    {
        const int idx = idOf (id);
        if (idx < 0) return nullptr;
        auto cb = std::make_unique<ComboBox>();
        cb->addItemList (def (idx).choices(), 1);
        comboAtt[cb.get()] = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *cb);
        cb->setTooltip (fge::tooltip (idx));
        auto* r = cb.get(); addAndMakeVisible (r); byId.insert ({ id, r }); if (caption.isNotEmpty()) captions[r] = caption; stageCtl.push_back (std::move (cb)); return r;
    }
    Component* DistPage::fixedCombo (const String& id, const String& caption)
    {
        auto* c = combo (id, caption);
        if (c != nullptr) { fixedCtl.push_back (std::move (stageCtl.back())); stageCtl.pop_back(); }
        return c;
    }
    void DistPage::forgetStage()
    {
        for (auto& c : stageCtl)
        {
            for (auto it = byId.begin(); it != byId.end();) it = it->second == c.get() ? byId.erase (it) : std::next (it);
            captions.erase (c.get()); layoutMap.erase (c.get());
            comboAtt.erase (c.get());                                 // the attachment goes before its control
        }
        stageCtl.clear();
    }
    Component* DistPage::findControl (const String& id) { auto it = byId.find (id); return it == byId.end() ? nullptr : it->second; }
    StringArray DistPage::visibleIds() const
    {
        StringArray s;
        for (auto& [id, c] : byId) if (c->isVisible()) s.addIfNotAlreadyThere (id);
        return s;
    }

    // ===================================================================== page
    DistPage::DistPage (GeProcessor& p, doomsday::flat::FlatLookAndFeel& l) : proc (p), lnf (l)
    {
        for (int i = 0; i < dd::NumLocs; ++i)
        {
            locBtn[(size_t) i].onClick = [this, i] { selectLocation (i); };
            locBtn[(size_t) i].setTooltip ("Distortion stages on " + Choices::distLocations()[i] + (i == 0 ? " (the whole kick)" : i >= dd::LocLayer1 ? " (that layer alone)" : " (that part / group alone)"));
            addAndMakeVisible (locBtn[(size_t) i]);
        }
        addBtn.setTooltip ("Add a new stage at the end of this chain (16 stages in total for the whole kick).");
        dupBtn.setTooltip ("A copy of the selected stage right after it.");
        copyBtn.setTooltip ("Copy the selected stage (all settings + the CUSTOM curve)."); pasteBtn.setTooltip ("Paste into the selected stage (it keeps its place in the chain).");
        upBtn.setTooltip ("Move the selected stage earlier (the order changes the sound)."); downBtn.setTooltip ("Move the selected stage later.");
        delBtn.setTooltip ("Delete the selected stage (back to the defaults, OFF).");
        presetBtn.setTooltip ("Factory distortion presets (EARLY / MODERN / RAWSTYLE / EXPERIMENTAL / UTILITY) - replace the whole distortion.");
        saveBtn.setTooltip ("Save: one stage, this chain, its multiband block, its parallel block or the full distortion.");
        loadBtn.setTooltip ("Load a stage / chain / multiband / parallel / full distortion file.");
        freezeBtn.setTooltip ("Freeze the spectrum: the frozen one stays as a grey line to compare with.");
        addBtn.onClick = [this] { const int s = distt::addStage (proc, locSel); if (s >= 0) selectStage (s); };
        dupBtn.onClick = [this] { if (stageSel >= 0) { const int s = distt::duplicate (proc, stageSel); if (s >= 0) selectStage (s); } };
        copyBtn.onClick = [this] { if (stageSel >= 0) clip = distt::stageToXml (proc, stageSel); };
        pasteBtn.onClick = [this] { if (stageSel >= 0 && clip) distt::paste (proc, stageSel, *clip); };
        upBtn.onClick = [this] { if (stageSel >= 0) distt::move (proc, stageSel, -1); };
        downBtn.onClick = [this] { if (stageSel >= 0) distt::move (proc, stageSel, 1); };
        delBtn.onClick = [this] { if (stageSel >= 0) { distt::remove (proc, stageSel); stageSel = -1; } };
        presetBtn.onClick = [this] { showPresetMenu(); };
        saveBtn.onClick = [this] { saveFile(); }; loadBtn.onClick = [this] { loadFile(); };
        freezeBtn.setClickingTogglesState (true);
        freezeBtn.onClick = [this] { haveFrozen = freezeBtn.getToggleState(); if (haveFrozen) frozen = shown; repaint (specArea); };
        rangeBox.addItemList ({ "60", "90", "120" }, 1); rangeBox.setSelectedId (2, dontSendNotification); rangeBox.onChange = [this] { repaint (specArea); };
        channelBox.addItemList ({ "MID", "LEFT", "RIGHT", "SIDE" }, 1); channelBox.setSelectedId (1, dontSendNotification);
        rangeBox.setTooltip ("The spectrum range in dB (top = 0 dB)."); channelBox.setTooltip ("Which channel the waveform / spectrum show.");
        for (Component* c : std::initializer_list<Component*> { &addBtn, &dupBtn, &copyBtn, &pasteBtn, &upBtn, &downBtn, &delBtn, &presetBtn, &saveBtn, &loadBtn, &freezeBtn, &rangeBox, &channelBox })
            addAndMakeVisible (c);
        // the global distortion settings (always there)
        auto fixedKnob = [&] (const String& id, const String& label)
        {
            auto* c = knob (id, label, palette().silver, false, 28);
            if (c != nullptr) { fixedCtl.push_back (std::move (stageCtl.back())); stageCtl.pop_back(); }
            return c;
        };
        place (fixedKnob ("dist_xover_low", "LOW / MID"), 0, 548, 76, doomsday::flat::Knob::heightFor (28));
        place (fixedKnob ("dist_xover_high", "MID / HIGH"), 80, 548, 76, doomsday::flat::Knob::heightFor (28));
        place (fixedCombo ("dist_global_position", "GLOBAL POSITION"), 0, 638, 240, 22);
        place (fixedCombo ("dist_offline_quality", "OFFLINE RENDER"), 0, 682, 240, 22);
        place (fixedCombo ("dist_band_solo", ""), 1110, 304, 104, 24);
        if (auto* c = findControl ("dist_band_solo")) static_cast<ComboBox*> (c)->setTooltip ("Listen to one band of the MULTIBAND distortion (the processed band alone).");
        addAndMakeVisible (curveEd);
        curveEd.onChange = [this] (const dd::CurveData& c, bool newStep)
        {
            if (stageSel < 0) return;
            proc.setDistCurve (stageSel, c, newStep);
            if (newStep && roundToInt (rawOf (proc, did ("type"))) != dd::Custom) proc.setParamInUndoStep (did ("type"), (float) dd::Custom);
        };
        scope.onReady = [this] { adoptScope(); };
        const auto c0 = distt::chain (proc, locSel);
        selectStage (c0.empty() ? -1 : c0.front());
        selectLocation (locSel);
        startTimerHz (15);
    }
    DistPage::~DistPage() { stopTimer(); comboAtt.clear(); }

    void DistPage::selectLocation (int l)
    {
        locSel = jlimit (0, dd::NumLocs - 1, l);
        const auto c = distt::chain (proc, locSel);
        if (std::find (c.begin(), c.end(), stageSel) == c.end()) selectStage (c.empty() ? -1 : c.front());
        lastSig = {};
        timerCallback();
    }
    void DistPage::selectStage (int s)
    {
        stageSel = s >= 0 && s < dd::numStages ? s : -1;
        if (stageSel >= 0) locSel = distt::location (proc, stageSel);
        lastSig = {};
        buildStage();
        timerCallback();
    }

    String DistPage::signature() const
    {
        String s; s << locSel << "/" << stageSel << "/";
        if (stageSel >= 0) s << roundToInt (rawOf (const_cast<GeProcessor&> (proc), did ("type"))) << "/";
        for (int i = 0; i < dd::numStages; ++i)
        {
            auto& p = const_cast<GeProcessor&> (proc);
            s << (distt::on (p, i) ? 1 : 0) << distt::location (p, i) << ":" << rawOf (p, distt::sid (i, "order")) << ":" << roundToInt (rawOf (p, distt::sid (i, "type")))
              << roundToInt (rawOf (p, distt::sid (i, "path"))) << roundToInt (rawOf (p, distt::sid (i, "band"))) << ",";
        }
        return s;
    }

    // the controls of the selected stage (rebuilt when the stage or its TYPE changes)
    void DistPage::buildStage()
    {
        forgetStage();
        if (stageSel < 0) { resized(); return; }
        const int t = roundToInt (rawOf (proc, did ("type")));
        const auto col = locCol (distt::location (proc, stageSel));
        const int x0 = 256;
        place (toggle (did ("on"), "ON", col), x0, 50, 60, 22);
        int cx = 330;
        for (auto [s, cap] : std::initializer_list<std::pair<const char*, const char*>> { { "type", "TYPE" }, { "morph_type", "MORPH TO" }, { "path", "PATH" }, { "band", "BAND" },
                 { "drive_curve", "DRIVE CURVE" }, { "oversampling", "OVERSAMPLING" } })
        {
            if (t == dd::Bitcrush && String (s) == "morph_type") continue;
            place (combo (did (s), cap), cx, 50, 110, 22); cx += 116;
        }
        if (auto* tc = typeCombo (t)) { place (combo (did (tc), typeComboCaption (t)), cx, 50, 110, 22); cx += 116; }
        place (toggle (did ("auto_gain"), "AUTO GAIN", col), cx, 50, 96, 22);
        // knobs: the common ones + what this type uses
        std::vector<TypeKnob> ks { { "drive", "DRIVE", false }, { "input", "INPUT", true }, { "output", "OUTPUT", true }, { "mix", "MIX", false }, { "bias", "BIAS", true } };
        if (t != dd::Bitcrush) ks.push_back ({ "morph", "MORPH", false });
        for (auto& k : typeKnobs (t)) ks.push_back (k);
        int k = 0;
        for (auto& kk : ks) place (knob (did (kk.id), kk.label, col, kk.bip), x0 + (k++) * 76, 82, 72, doomsday::flat::Knob::heightFor (28));
        // pre / post filter
        place (toggle (did ("pre_on"), "ON", col), x0, 172, 64, 22);
        place (combo (did ("pre_type"), "TYPE"), x0 + 68, 172, 92, 22);
        place (combo (did ("pre_slope"), "SLOPE"), x0 + 164, 172, 62, 22);
        k = 0;
        for (auto [s, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "pre_cutoff", "CUTOFF" }, { "pre_resonance", "RESO" }, { "pre_drive", "DRIVE" } })
            place (knob (did (s), lab, col), x0 + 230 + (k++) * 64, 156, 62, doomsday::flat::Knob::heightFor (28));
        const int px = 730;
        place (toggle (did ("post_on"), "ON", col), px, 172, 64, 22);
        place (combo (did ("post_type"), "TYPE"), px + 68, 172, 92, 22);
        place (combo (did ("post_slope"), "SLOPE"), px + 164, 172, 62, 22);
        k = 0;
        for (auto [s, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "post_cutoff", "CUTOFF" }, { "post_resonance", "RESO" }, { "post_drive", "DRIVE" }, { "post_mix", "MIX" } })
            place (knob (did (s), lab, col), px + 230 + (k++) * 64, 156, 62, doomsday::flat::Knob::heightFor (28));
        // dynamic distortion
        place (combo (did ("mod_source"), "MOD SOURCE"), x0, 244, 110, 22);
        k = 0;
        for (auto [s, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "mod_attack", "ATTACK" }, { "mod_decay", "DECAY" }, { "mod_drive", "> DRIVE" },
                 { "mod_bias", "> BIAS" }, { "mod_threshold", "> THRESH" }, { "mod_fold", "> FOLD" }, { "mod_mix", "> MIX" }, { "mod_output", "> OUTPUT" }, { "mod_cutoff", "> CUTOFF" } })
        {
            if (t == dd::Bitcrush && String (s) == "mod_fold") continue;
            const bool bip = ! String (s).endsWith ("attack") && ! String (s).endsWith ("decay");
            place (knob (did (s), lab, col, bip), x0 + 120 + (k++) * 76, 228, 72, doomsday::flat::Knob::heightFor (28));
        }
        curveEd.setEditable (t == dd::Custom);
        curveEd.setCurve (proc.distData().curve[(size_t) stageSel]);
        updateEnabled();
        resized();
    }

    // greyed = the control works, but not in the current setting (it tells which one)
    void DistPage::updateEnabled()
    {
        if (stageSel < 0) return;
        auto en = [&] (const char* s, bool on) { for (auto [it, end] = byId.equal_range (did (s)); it != end; ++it) { it->second->setEnabled (on); it->second->setAlpha (on ? 1.f : 0.4f); } };
        const bool pre = rawOf (proc, did ("pre_on")) > 0.5f, post = rawOf (proc, did ("post_on")) > 0.5f;
        for (auto* s : { "pre_type", "pre_slope", "pre_cutoff", "pre_resonance", "pre_drive" }) en (s, pre);
        for (auto* s : { "post_type", "post_slope", "post_cutoff", "post_resonance", "post_drive", "post_mix", "mod_cutoff" }) en (s, post);
        en ("morph_type", rawOf (proc, did ("morph")) > 0.f);
        bool mod = false; for (auto* s : { "mod_drive", "mod_bias", "mod_threshold", "mod_fold", "mod_mix", "mod_output", "mod_cutoff" }) mod |= std::abs (rawOf (proc, did (s))) > 1.0e-4f;
        for (auto* s : { "mod_source", "mod_attack", "mod_decay" }) en (s, mod);
        const bool anyBand = [&] { for (int i = 0; i < dd::numStages; ++i) if (distt::on (proc, i) && rawOf (proc, distt::sid (i, "band")) > 0.5f) return true; return false; }();
        for (auto* id : { "dist_xover_low", "dist_xover_high", "dist_band_solo" })
            for (auto [it, end] = byId.equal_range (id); it != end; ++it) { it->second->setEnabled (anyBand); it->second->setAlpha (anyBand ? 1.f : 0.4f); }
    }

    void DistPage::requestScope()
    {
        if (stageSel < 0) return;
        auto q = std::make_unique<DistScope::Request>();
        q->f = proc.frame(); q->bank = proc.shapes(); q->tables = proc.currentTables(); q->dist = proc.distData();
        q->stage = stageSel; q->channel = channelBox.getSelectedId() - 1; q->note = proc.auditionNote;
        scope.ask (std::move (q));
    }

    void DistPage::timerCallback()
    {
        if (! isShowing() && lastSig.isNotEmpty()) return;
        const auto sig = signature();
        if (sig != lastSig)
        {
            const bool typeChanged = lastSig.upToLastOccurrenceOf ("/", false, false) != sig.upToLastOccurrenceOf ("/", false, false);
            lastSig = sig;
            if (stageSel >= 0 && ! distt::on (proc, stageSel)) { const auto c = distt::chain (proc, locSel); stageSel = c.empty() ? -1 : c.front(); buildStage(); }
            else if (typeChanged) buildStage();
            // the chain rows
            rows.clear();
            for (int st : distt::chain (proc, locSel))
            {
                auto b = std::make_unique<TextButton>();
                String txt; txt << "#" << (st + 1) << "  " << Choices::distTypes()[roundToInt (rawOf (proc, distt::sid (st, "type")))];
                const int band = roundToInt (rawOf (proc, distt::sid (st, "band"))), path = roundToInt (rawOf (proc, distt::sid (st, "path")));
                if (band > 0) txt << "  [" << Choices::distBands()[band] << "]"; else if (path > 0) txt << "  [" << Choices::distPaths()[path].replace ("PARALLEL ", "PAR ") << "]";
                b->setButtonText (txt);
                b->setColour (TextButton::buttonColourId, st == stageSel ? locCol (locSel) : palette().control);
                b->setColour (TextButton::textColourOffId, st == stageSel ? palette().ground : palette().silver);
                b->onClick = [this, st] { selectStage (st); };
                addAndMakeVisible (*b);
                rows.push_back (std::move (b));
            }
            for (int i = 0; i < dd::NumLocs; ++i)
            {
                const int n = (int) distt::chain (proc, i).size();
                auto& b = locBtn[(size_t) i];
                b.setButtonText (Choices::distLocations()[i].replace ("LAYER ", "L") + (n > 0 ? " (" + String (n) + ")" : String()));
                b.setColour (TextButton::buttonColourId, i == locSel ? locCol (i) : n > 0 ? palette().control.brighter (0.2f) : palette().control);
                b.setColour (TextButton::textColourOffId, i == locSel ? palette().ground : palette().silver);
            }
            const bool full = distt::freeStage (proc) < 0, has = stageSel >= 0;
            addBtn.setEnabled (! full); dupBtn.setEnabled (has && ! full);
            for (auto* b : { &copyBtn, &upBtn, &downBtn, &delBtn }) b->setEnabled (has);
            pasteBtn.setEnabled (has && clip != nullptr);
            resized(); repaint();
        }
        updateEnabled();
        if (stageSel >= 0 && proc.distDataVersion() != lastDist) { curveEd.setCurve (proc.distData().curve[(size_t) stageSel]); }
        // the visualizer: ask again when anything that changes it moved
        const auto& f = proc.frame();
        const int ch = channelBox.getSelectedId();
        if (stageSel >= 0 && (stageSel != lastStageAsked || ch != lastChannel || proc.shapesVersion() != lastShapes || proc.distDataVersion() != lastDist
                              || ! std::equal (f.v.begin(), f.v.end(), lastAsked.v.begin())))
        {
            lastAsked = f; lastShapes = proc.shapesVersion(); lastDist = proc.distDataVersion(); lastStageAsked = stageSel; lastChannel = ch;
            requestScope();
        }
    }

    void DistPage::showPresetMenu()
    {
        PopupMenu m; std::map<String, PopupMenu> cats; StringArray order;
        const auto& l = distt::presets();
        for (int i = 0; i < (int) l.size(); ++i) { const String c (l[(size_t) i].category); if (! order.contains (c)) order.add (c); cats[c].addItem (i + 1, l[(size_t) i].name); }
        for (auto& c : order) m.addSubMenu (c, cats[c]);
        SafePointer<DistPage> self (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetBtn), [self] (int r)
        {
            if (self == nullptr || r <= 0) return;
            distt::apply (self->proc, distt::presets()[(size_t) r - 1]);
            const auto c = distt::chain (self->proc, self->locSel);
            int first = -1; for (int i = 0; i < dd::numStages && first < 0; ++i) if (distt::on (self->proc, i)) first = i;
            self->selectStage (! c.empty() ? c.front() : first);
        });
    }
    void DistPage::saveFile()
    {
        PopupMenu m;
        for (int s = 0; s < 5; ++s) m.addItem (s + 1, String ("Save ") + distt::scopeName (s) + " (" + distt::extension (s) + ")", s == distt::ScStage ? stageSel >= 0 : true);
        SafePointer<DistPage> self (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&saveBtn), [self] (int r)
        {
            if (self == nullptr || r <= 0) return;
            const int sc = r - 1;
            self->chooser = std::make_unique<FileChooser> ("Save distortion", files::folder ("Distortion"), String ("*") + distt::extension (sc));
            self->chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [self, sc] (const FileChooser& fc)
            {
                if (self == nullptr || fc.getResult() == File()) return;
                auto x = distt::save (self->proc, sc, jmax (0, self->stageSel), self->locSel);
                files::writeXml (fc.getResult().withFileExtension (distt::extension (sc)), *x);
            });
        });
    }
    void DistPage::loadFile()
    {
        chooser = std::make_unique<FileChooser> ("Load distortion", files::folder ("Distortion"), "*.4gstage;*.4gchain;*.4gmband;*.4gpar;*.4gdist");
        SafePointer<DistPage> self (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [self] (const FileChooser& fc)
        {
            if (self == nullptr || fc.getResult() == File()) return;
            if (auto x = files::readXml (fc.getResult()))
            {
                const bool needsStage = x->getIntAttribute ("scope") == distt::ScStage;
                int st = self->stageSel;
                if (needsStage && st < 0) st = distt::addStage (self->proc, self->locSel);
                if (st >= 0 || ! needsStage) distt::load (self->proc, *x, jmax (0, st), self->locSel);
                self->lastSig = {};
            }
        });
    }

    void DistPage::resized()
    {
        const int w = getWidth(), h = getHeight();
        const int bw = (w - (dd::NumLocs - 1) * 4) / dd::NumLocs;
        for (int i = 0; i < dd::NumLocs; ++i) locBtn[(size_t) i].setBounds (i * (bw + 4), 0, bw, 26);
        chainArea = { 0, 36, 240, 400 };
        for (size_t k = 0; k < rows.size(); ++k) rows[k]->setBounds (0, 56 + (int) k * 24, 240, 22);
        int bx = 0;
        for (auto* b : { &addBtn, &dupBtn, &copyBtn }) { b->setBounds (bx, 446, 78, 24); bx += 81; }
        bx = 0;
        for (auto* b : { &pasteBtn, &upBtn, &downBtn, &delBtn }) { b->setBounds (bx, 474, 58, 24); bx += 61; }
        presetBtn.setBounds (0, 508, 84, 26); saveBtn.setBounds (88, 508, 74, 26); loadBtn.setBounds (166, 508, 74, 26);
        stageArea = { 256, 36, w - 256, 264 };
        transferArea = { 256, 334, 350, h - 334 };
        waveArea = { 616, 334, 300, h - 334 };
        specArea = { 926, 334, w - 926, h - 334 };
        curveEd.setBounds (transferArea);
        curveEd.setVisible (stageSel >= 0);
        freezeBtn.setBounds (926, 304, 58, 24); rangeBox.setBounds (988, 304, 56, 24); channelBox.setBounds (1048, 304, 58, 24);
        for (auto& [c, r] : layoutMap) c->setBounds (r);
    }

    void DistPage::paint (Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.panel);
        g.fillRoundedRectangle (Rectangle<int> (0, 32, 244, getHeight() - 32).toFloat().expanded (2.f, 0.f), 4.f);
        g.fillRoundedRectangle (Rectangle<int> (250, 32, getWidth() - 250, 270).toFloat(), 4.f);
        g.setColour (c.textDim); g.setFont (FontOptions (10.5f, Font::bold));
        g.drawText ("CHAIN: " + Choices::distLocations()[locSel] + "  (runs top -> bottom)", 4, 38, 236, 14, Justification::centredLeft);
        if (rows.empty()) { g.setFont (FontOptions (11.f)); g.drawText ("no stage here - ADD one", 4, 60, 236, 20, Justification::centredLeft); }
        g.drawText ("CROSSOVERS (multiband)", 4, 534, 236, 12, Justification::centredLeft);
        for (auto& [comp, cap] : captions)
            if (comp->isVisible()) g.drawText (cap, comp->getX(), comp->getY() - 12, comp->getWidth() + 30, 11, Justification::centredLeft);
        if (stageSel >= 0)
        {
            g.setColour (locCol (locSel)); g.setFont (FontOptions (12.f, Font::bold));
            g.drawText ("STAGE " + String (stageSel + 1), 256, 36, 70, 12, Justification::centredLeft);
            g.setColour (c.textDim); g.setFont (FontOptions (10.5f, Font::bold));
            g.drawText ("PRE FILTER", 256, 150, 120, 12, Justification::centredLeft);
            g.drawText ("POST FILTER", 730, 150, 120, 12, Justification::centredLeft);
            g.drawText ("DYNAMIC DISTORTION", 256, 222, 160, 12, Justification::centredLeft);
        }
        else { g.setColour (c.textDim); g.setFont (FontOptions (13.f)); g.drawText ("Select or ADD a stage", stageArea, Justification::centred); }
        g.setColour (c.textDim); g.setFont (FontOptions (10.5f, Font::bold));
        g.drawText ("TRANSFER CURVE", transferArea.getX(), 314, 200, 12, Justification::centredLeft);
        g.drawText ("WAVEFORM  before (line) / after (fill)", waveArea.getX(), 314, 290, 12, Justification::centredLeft);
        // waveform before / after (the real kick, this stage bypassed / active)
        g.setColour (c.ground); g.fillRoundedRectangle (waveArea.toFloat(), 4.f);
        g.setColour (c.ground); g.fillRoundedRectangle (specArea.toFloat(), 4.f);
        if (! shown.valid || stageSel < 0) return;
        const auto colA = locCol (locSel);
        {
            const auto r = waveArea.toFloat().reduced (4.f);
            const float sc = r.getHeight() * 0.48f / jmax (1.0e-4f, shown.peak);
            auto draw = [&] (const std::vector<float>& lo, const std::vector<float>& hi, Colour col)
            {
                Path p; const int n = (int) hi.size();
                for (int k = 0; k < n; ++k) { const float x = r.getX() + r.getWidth() * (float) k / (float) (n - 1); k == 0 ? p.startNewSubPath (x, r.getCentreY() - hi[(size_t) k] * sc) : p.lineTo (x, r.getCentreY() - hi[(size_t) k] * sc); }
                for (int k = n - 1; k >= 0; --k) p.lineTo (r.getX() + r.getWidth() * (float) k / (float) (n - 1), r.getCentreY() - lo[(size_t) k] * sc);
                p.closeSubPath(); g.setColour (col); g.fillPath (p);
            };
            draw (shown.loA, shown.hiA, colA.withAlpha (0.6f));
            // 'before' as an outline on top (so both stay visible)
            for (auto* v : { &shown.hiB, &shown.loB })
            {
                Path p; const int n = (int) v->size();
                for (int k = 0; k < n; ++k) { const float x = r.getX() + r.getWidth() * (float) k / (float) (n - 1), y = r.getCentreY() - (*v)[(size_t) k] * sc; k == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y); }
                g.setColour (c.silver.withAlpha (0.55f)); g.strokePath (p, PathStrokeType (1.f));
            }
        }
        // spectrum before / after (+ the frozen one)
        {
            const auto r = specArea.toFloat().reduced (4.f, 6.f);
            const float range = rangeBox.getSelectedId() == 1 ? 60.f : rangeBox.getSelectedId() == 3 ? 120.f : 90.f;
            g.setColour (c.grid); g.setFont (FontOptions (9.f));
            for (float hz : { 100.f, 1000.f, 10000.f })
            {
                const float x = r.getX() + r.getWidth() * std::log (hz / 20.f) / std::log (1000.f);
                g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
                g.setColour (c.textDim); g.drawText (hz >= 1000.f ? String ((int) (hz / 1000.f)) + "k" : String ((int) hz), (int) x + 2, (int) r.getBottom() - 12, 30, 10, Justification::left); g.setColour (c.grid);
            }
            auto line = [&] (const std::vector<float>& s, Colour col, float th)
            {
                if (s.empty()) return;
                Path p;
                for (size_t k = 0; k < s.size(); ++k)
                {
                    const float x = r.getX() + r.getWidth() * (float) k / (float) (s.size() - 1);
                    const float y = r.getY() + r.getHeight() * jlimit (0.f, 1.f, -s[k] / range);
                    k == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
                }
                g.setColour (col); g.strokePath (p, PathStrokeType (th));
            };
            if (haveFrozen) line (frozen.specA, c.silver.withAlpha (0.35f), 1.f);
            line (shown.specB, c.textDim.withAlpha (0.7f), 1.f);
            line (shown.specA, colA, 1.4f);
        }
    }
}
