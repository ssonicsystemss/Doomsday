#include "LayerPage.h"
#include "Palette.h"

using namespace juce;

namespace fge::gui
{
    namespace
    {
        // the layer list columns (one table: rows and header use the same numbers)
        constexpr int colW[14] { 40, 84, 88, 84, 74, 78, 60, 58, 40, 78, 56, 28, 28, 80 };
        constexpr int colX0 = 132, colGap = 4, waveX = 1048;
        int colX (int k) { int x = colX0; for (int i = 0; i < k; ++i) x += colW[i] + colGap; return x; }
        int idOf (const String& id) { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; }
        float rawOf (GeProcessor& p, const String& id) { auto* v = p.apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.f; }
        Colour groupCol (int g) { const auto& c = palette(); return g < 4 ? c.part[(size_t) g] : g == 4 ? c.part[5] : c.part[4]; }
    }

    // ===================================================================== control factories
    Component* LayerPage::knob (std::vector<std::unique_ptr<Component>>& own, const String& id, const String& label, Colour col, int d, bool bip)
    {
        if (idOf (id) < 0) return nullptr;
        auto k = std::make_unique<doomsday::flat::Knob> (proc.apvts, id, label, col, bip, d);
        k->slider.setTooltip (fge::tooltip (idOf (id)));
        auto* r = k.get(); addAndMakeVisible (r); byId.insert ({ id, r }); own.push_back (std::move (k)); return r;
    }
    Component* LayerPage::toggle (std::vector<std::unique_ptr<Component>>& own, const String& id, const String& text, Colour col, bool chip)
    {
        auto t = std::make_unique<doomsday::flat::Toggle> (proc.apvts, id, text, chip ? doomsday::flat::Toggle::Chip : doomsday::flat::Toggle::Pill, col);
        t->setTooltip (fge::tooltip (idOf (id)));
        auto* r = t.get(); addAndMakeVisible (r); byId.insert ({ id, r }); own.push_back (std::move (t)); return r;
    }
    Component* LayerPage::combo (std::vector<std::unique_ptr<Component>>& own, const String& id, const String& caption)
    {
        const int idx = idOf (id);
        if (idx < 0) return nullptr;
        auto cb = std::make_unique<ComboBox>();
        cb->addItemList (def (idx).choices(), 1);
        comboAtt[cb.get()] = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *cb);
        cb->setTooltip (fge::tooltip (idx));
        auto* r = cb.get(); addAndMakeVisible (r); byId.insert ({ id, r }); if (caption.isNotEmpty()) captions[r] = caption; own.push_back (std::move (cb)); return r;
    }
    Component* LayerPage::bar (std::vector<std::unique_ptr<Component>>& own, const String& id)
    {
        const int idx = idOf (id);
        if (idx < 0) return nullptr;
        auto s = std::make_unique<Slider> (Slider::LinearBar, Slider::TextBoxLeft);
        s->setColour (Slider::trackColourId, palette().control.brighter (0.25f));
        s->setColour (Slider::textBoxTextColourId, palette().silver);
        sliderAtt[s.get()] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *s);
        s->setTooltip (String (def (idx).name) + " - " + fge::tooltip (idx));
        auto* r = s.get(); listHolder.addAndMakeVisible (r); byId.insert ({ id, r }); own.push_back (std::move (s)); return r;
    }
    void LayerPage::forget (std::vector<std::unique_ptr<Component>>& own)
    {
        for (auto& c : own)
        {
            for (auto it = byId.begin(); it != byId.end();) it = it->second == c.get() ? byId.erase (it) : std::next (it);
            captions.erase (c.get()); layoutMap.erase (c.get());
            comboAtt.erase (c.get()); sliderAtt.erase (c.get());          // the attachment goes before its control
        }
        own.clear();
    }
    Component* LayerPage::findControl (const String& id) { auto it = byId.find (id); return it == byId.end() ? nullptr : it->second; }

    // ===================================================================== page
    LayerPage::LayerPage (GeProcessor& p, doomsday::flat::FlatLookAndFeel& l) : proc (p), lnf (l)
    {
        const char* views[3] { "NOISE", "TRANSIENT", "LAYERS" };
        for (int i = 0; i < 3; ++i) { viewBtn[(size_t) i].setButtonText (views[i]); viewBtn[(size_t) i].onClick = [this, i] { showView (i); }; addAndMakeVisible (viewBtn[(size_t) i]); }
        for (int i = 0; i < 2; ++i) { noiseBtn[(size_t) i].setButtonText (i == 0 ? "NOISE A" : "NOISE B"); noiseBtn[(size_t) i].onClick = [this, i] { selectNoise (i); }; addChildComponent (noiseBtn[(size_t) i]); }
        const char* ed[3] { "PROFILE", "AMP ENV", "MOD ENV" };
        for (int i = 0; i < 3; ++i) { noiseEditBtn[(size_t) i].setButtonText (ed[i]); noiseEditBtn[(size_t) i].onClick = [this, i] { noiseEdit = i; refreshEditor(); repaint(); }; addChildComponent (noiseEditBtn[(size_t) i]); }
        noiseEditBtn[0].setTooltip ("CUSTOM source: the noise spectrum (low -> high).");
        noiseEditBtn[1].setTooltip ("The drawn envelope (ENVELOPE = DRAWN).");
        noiseEditBtn[2].setTooltip ("The drawn modulation envelope (MOD MODE = DRAWN).");
        const char* det[3] { "LAYER", "GROUPS", "TOOLS" };
        for (int i = 0; i < 3; ++i) { detailBtn[(size_t) i].setButtonText (det[i]); detailBtn[(size_t) i].onClick = [this, i] { showDetail (i); }; addChildComponent (detailBtn[(size_t) i]); }
        presetBtn.onClick = [this] { showPresetMenu(); };
        saveBtn.onClick = [this] { saveFile(); }; loadBtn.onClick = [this] { loadFile(); };
        presetBtn.setTooltip ("Factory presets of this view (NOISE / TRANSIENT / LAYER stacks)."); saveBtn.setTooltip ("Save this view's settings to a file."); loadBtn.setTooltip ("Load a file into this view.");
        for (auto* b : { &presetBtn, &saveBtn, &loadBtn }) addAndMakeVisible (b);
        addChildComponent (editor);
        editor.onCommit = [this] (const env::Shape& s)
        {
            if (viewIdx == ViewNoise)
            {
                const int slot = (noiseEdit == 0 ? env::ProfileNoiseA : noiseEdit == 1 ? env::AmpNoiseA : env::ModNoiseA) + noiseSel;
                proc.setShape (slot, s);
                if (noiseEdit == 0) proc.setParamInUndoStep (nid ("type"), (float) dsp::NkCustom);
                if (noiseEdit == 1) proc.setParamInUndoStep (nid ("env"), (float) dsp::NeDrawn);
                if (noiseEdit == 2) { proc.setParamInUndoStep (nid ("mod_on"), 1.f); proc.setParamInUndoStep (nid ("mod_mode"), 1.f); }
            }
            else if (viewIdx == ViewTransient) { proc.setShape (env::TransientCustom, s); proc.setParamInUndoStep ("tr_source", (float) dsp::TrCustom); }
            else { proc.setShape (env::AmpLayer1 + layerSel, s); proc.setParamInUndoStep (lid ("env_mode"), (float) dsp::LeMulti); }
        };
        listView.setViewedComponent (&listHolder, false);
        listView.setScrollBarsShown (true, false);
        addChildComponent (listView);
        for (int i = 0; i < 8; ++i) { rows[(size_t) i].layer = i; rows[(size_t) i].onSelect = [this] (int k) { selectLayer (k); }; listHolder.addAndMakeVisible (rows[(size_t) i]); }
        // tools
        randMode.addItemList (layers::randModeNames(), 1); randMode.setSelectedId (8, dontSendNotification);
        for (auto* s : { &randIntensity, &randSeed, &alignSearch }) { s->setSliderStyle (Slider::LinearBar); s->setTextBoxStyle (Slider::TextBoxLeft, false, 60, 20); }
        randIntensity.setRange (0.0, 100.0, 1.0); randIntensity.setValue (50.0, dontSendNotification); randIntensity.setTextValueSuffix (" % intensity");
        randSeed.setRange (1.0, 9999.0, 1.0); randSeed.setValue (1.0, dontSendNotification); randSeed.setTextValueSuffix (" seed");
        alignSearch.setRange (10.0, 360.0, 5.0); alignSearch.setValue (360.0, dontSendNotification); alignSearch.setTextValueSuffix (" deg search");
        keepGroups.setToggleState (true, dontSendNotification); keepRouting.setToggleState (true, dontSendNotification);
        for (int i = 0; i < 8; ++i) { alignRef.addItem ("REF: LAYER " + String (i + 1), i + 1); alignTarget.addItem ("TARGET: LAYER " + String (i + 1), i + 1); }
        alignRef.setSelectedId (1, dontSendNotification); alignTarget.setSelectedId (2, dontSendNotification);
        randBtn.setTooltip ("Randomize the layers (LOCKED layers stay as they are). One UNDO step.");
        randBtn.onClick = [this]
        {
            layers::RandSettings rs; rs.mode = randMode.getSelectedId() - 1; rs.intensity = (float) randIntensity.getValue() / 100.f; rs.seed = (int) randSeed.getValue();
            rs.preserveGroups = keepGroups.getToggleState(); rs.preserveRouting = keepRouting.getToggleState();
            layers::randomize (proc, rs);
        };
        analyseBtn.setTooltip ("Measure the correlation of the TARGET layer against the REFERENCE layer for every phase (nothing changes yet).");
        analyseBtn.onClick = [this] { runAlign(); };
        applyBtn.setTooltip ("Set the TARGET layer's PHASE to the best value found (you decide - the highest correlation is not always the best sound).");
        applyBtn.onClick = [this] { applyAlign(); };
        copyBtn.onClick = [this] { clip = layers::layerToXml (proc, layerSel); };
        pasteBtn.onClick = [this] { if (clip) layers::xmlToLayer (proc, layerSel, *clip); };
        dupBtn.onClick = [this] { const int k = layers::duplicate (proc, layerSel); if (k >= 0) selectLayer (k); };
        delBtn.onClick = [this] { layers::deleteLayer (proc, layerSel); };
        upBtn.onClick = [this] { auto o = layers::order (proc); for (int r = 0; r < 8; ++r) if (o[(size_t) r] == layerSel) { layers::move (proc, r, -1); break; } layoutRows(); };
        downBtn.onClick = [this] { auto o = layers::order (proc); for (int r = 0; r < 8; ++r) if (o[(size_t) r] == layerSel) { layers::move (proc, r, 1); break; } layoutRows(); };
        renameBtn.onClick = [this]
        {
            auto* w = new AlertWindow ("Rename layer", "Name of layer " + String (layerSel + 1), MessageBoxIconType::NoIcon, this);
            w->addTextEditor ("n", layers::name (proc, layerSel));
            w->addButton ("OK", 1, KeyPress (KeyPress::returnKey)); w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
            SafePointer<LayerPage> self (this);
            w->enterModalState (true, ModalCallbackFunction::create ([self, w] (int r) { if (self != nullptr && r == 1) layers::setName (self->proc, self->layerSel, w->getTextEditorContents ("n")); }), true);
        };
        lockBtn.setClickingTogglesState (true);
        lockBtn.onClick = [this] { proc.undo.beginNewTransaction ("Lock layer"); layers::setLocked (proc, layerSel, lockBtn.getToggleState()); };
        for (Component* c : std::initializer_list<Component*> { &randMode, &randIntensity, &randSeed, &keepGroups, &keepRouting, &randBtn, &alignRef, &alignTarget, &alignSearch,
                                                               &analyseBtn, &applyBtn, &copyBtn, &pasteBtn, &dupBtn, &delBtn, &upBtn, &downBtn, &renameBtn, &lockBtn })
            addChildComponent (c);
        renders.onRendered = [this] { for (auto& r : rows) r.repaint(); repaint (waveArea); };
        buildNoise(); buildTransient(); buildLayers();
        setJobs();
        showView (ViewLayers);
        startTimerHz (15);
    }
    LayerPage::~LayerPage() { stopTimer(); comboAtt.clear(); sliderAtt.clear(); }

    // the background renders: 8 layers alone, noise A, noise B, the transient alone, the whole kick
    void LayerPage::setJobs()
    {
        std::vector<KickPreview::Job> j;
        auto silentKick = [] (ParamFrame& f) { for (int id : { P::mixPunchMute, P::mixBodyMute, P::mixTailMute, P::mixSubMute }) f[id] = 1.f; f[P::clipDrive] = 0.f; f[P::shOn] = 0.f; };
        const int ls = P::layer2On - P::layer1On;
        for (int i = 0; i < 8; ++i)
            j.push_back ({ [=] (ParamFrame& f) { silentKick (f); f[P::noiseNAOn] = f[P::noiseNBOn] = f[P::trOn] = 0.f; for (int k = 0; k < 8; ++k) { if (k != i) f[P::layer1On + k * ls] = 0.f; f[P::layer1Solo + k * ls] = 0.f; f[P::layer1Mute + k * ls] = 0.f; } }, 0.6f });
        for (int n = 0; n < 2; ++n)
            j.push_back ({ [=] (ParamFrame& f) { silentKick (f); for (int k = 0; k < 8; ++k) f[P::layer1On + k * ls] = 0.f; f[P::trOn] = 0.f; (n == 0 ? f[P::noiseNBOn] : f[P::noiseNAOn]) = 0.f;
                                                 f[n == 0 ? P::noiseNAOn : P::noiseNBOn] = 1.f; f[P::noiseNAToTex + n * (P::noiseNBOn - P::noiseNAOn)] = 0.f; }, 0.5f });
        j.push_back ({ [=] (ParamFrame& f) { silentKick (f); for (int k = 0; k < 8; ++k) f[P::layer1On + k * ls] = 0.f; f[P::noiseNAOn] = f[P::noiseNBOn] = 0.f; f[P::trOn] = 1.f; f[P::trOutput] = 4.f; }, 0.08f });
        j.push_back ({ nullptr, 0.5f });
        renders.setJobs (std::move (j));
    }

    void LayerPage::buildNoise()
    {
        forget (noiseCtl);
        const auto col = palette().part[5];
        const int x0 = 160;
        auto row = [&] (std::initializer_list<std::pair<const char*, const char*>> ks, int y, int x = 160, bool bipFirst = false)
        {
            int k = 0;
            for (auto [s, lab] : ks) { if (auto* c = knob (noiseCtl, nid (s), lab, col, 30, (bipFirst && k == 0) || String (s).startsWith ("mod_") && String (s) != "mod_attack" && String (s) != "mod_decay" || String (s) == "color")) place (c, x + k * 78, y, 76, doomsday::flat::Knob::heightFor (30)); ++k; }
        };
        if (auto* c = toggle (noiseCtl, nid ("on"), "ON", col)) place (c, x0, 50, 60, 22);
        int cx = x0 + 70;
        for (auto [s, cap] : std::initializer_list<std::pair<const char*, const char*>> { { "type", "SOURCE" }, { "filter", "FILTER" }, { "env", "ENVELOPE" }, { "random_mode", "RANDOM" } })
            if (auto* c = combo (noiseCtl, nid (s), cap)) { place (c, cx, 50, 130, 22); cx += 140; }
        if (auto* c = toggle (noiseCtl, nid ("track"), "PITCH TRACK", col)) place (c, cx + 10, 50, 120, 22);
        row ({ { "level", "LEVEL" }, { "color", "COLOR" }, { "cutoff", "CUTOFF" }, { "resonance", "RESO" }, { "drive", "DRIVE" }, { "attack", "ATTACK" },
               { "decay", "DECAY" }, { "length", "LENGTH" }, { "mix", "MIX" }, { "width", "WIDTH" }, { "seed", "SEED" }, { "random_amount", "RAND AMT" } }, 84);
        row ({ { "res_freq", "RES FREQ" }, { "res_q", "RES Q" }, { "res_amount", "RES AMT" }, { "metal", "METAL" }, { "bits", "BITS" }, { "sr_reduce", "SR REDUCE" },
               { "quant", "QUANT" }, { "grain", "GRAIN" }, { "to_punch", "> PUNCH" }, { "to_body", "> BODY" }, { "to_tail", "> TAIL" }, { "to_sub", "> SUB" }, { "to_texture", "> TEXTURE" } }, 162);
        if (auto* c = toggle (noiseCtl, nid ("mod_on"), "MOD", col)) place (c, x0, 252, 80, 22);
        if (auto* c = combo (noiseCtl, nid ("mod_mode"), "")) place (c, x0, 280, 80, 22);
        row ({ { "mod_attack", "MOD ATK" }, { "mod_decay", "MOD DEC" }, { "mod_level", "> LEVEL" }, { "mod_cutoff", "> CUTOFF" }, { "mod_resonance", "> RESO" },
               { "mod_drive", "> DRIVE" }, { "mod_color", "> COLOR" }, { "mod_width", "> WIDTH" }, { "mod_grain", "> GRAIN" } }, 240, x0 + 90);
    }

    void LayerPage::buildTransient()
    {
        forget (trCtl);
        const auto col = palette().part[0];
        if (auto* c = toggle (trCtl, "tr_on", "ON", col)) place (c, 0, 50, 60, 22);
        int cx = 70;
        for (auto [s, cap] : std::initializer_list<std::pair<const char*, const char*>> { { "tr_source", "SOURCE" }, { "tr_sync", "POSITION FROM" }, { "tr_output", "OUTPUT" } })
            if (auto* c = combo (trCtl, s, cap)) { place (c, cx, 50, 150, 22); cx += 160; }
        int k = 0;
        for (auto [s, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "tr_click_level", "CLICK LEVEL" }, { "tr_click_length", "LENGTH" }, { "tr_click_pitch", "PITCH" },
                 { "tr_click_tone", "TONE" }, { "tr_click_drive", "DRIVE" }, { "tr_click_mix", "MIX" }, { "tr_width", "WIDTH" }, { "tr_position", "POSITION" } })
            if (auto* c = knob (trCtl, s, lab, col, 34)) place (c, (k++) * 82, 86, 80, doomsday::flat::Knob::heightFor (34));
        k = 0;
        for (auto [s, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "tr_attack_level", "ATTACK LVL" }, { "tr_attack_time", "ATK TIME" },
                 { "tr_decay_time", "DECAY" }, { "tr_low", "LOW TRANS" }, { "tr_high", "HIGH TRANS" } })
            if (auto* c = knob (trCtl, s, lab, col, 34, true)) place (c, (k++) * 82, 186, 80, doomsday::flat::Knob::heightFor (34));
        // (titles of the two groups are painted at y 166)
        if (auto* c = toggle (trCtl, "shaper_on", "ON", col)) place (c, 700, 162, 60, 20);
        k = 0;
        for (auto [s, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "shaper_attack", "ATTACK" }, { "shaper_sustain", "SUSTAIN" },
                 { "shaper_sensitivity", "SENSITIVITY" }, { "shaper_time", "TIME" }, { "shaper_mix", "MIX" } })
            if (auto* c = knob (trCtl, s, lab, palette().red, 34, k < 2)) place (c, 430 + (k++) * 82, 186, 80, doomsday::flat::Knob::heightFor (34));
    }

    void LayerPage::buildLayers()
    {
        forget (rowCtl);
        for (int i = 0; i < 8; ++i)
        {
            const String o = "layer" + String (i + 1) + "_";
            auto cmb = [&] (const String& s) { auto* c = combo (rowCtl, o + s, ""); if (c != nullptr) { removeChildComponent (c); listHolder.addAndMakeVisible (c); } return c; };
            auto tg = [&] (const String& s, const String& t) { auto* c = toggle (rowCtl, o + s, t, palette().silver, true); if (c != nullptr) { removeChildComponent (c); listHolder.addAndMakeVisible (c); } return c; };
            std::vector<Component*> r { tg ("on", "ON"), cmb ("group"), cmb ("source"), bar (rowCtl, o + "level"), bar (rowCtl, o + "pitch"), bar (rowCtl, o + "length"),
                                        bar (rowCtl, o + "pan"), bar (rowCtl, o + "phase"), tg ("filter_on", "FLT"), bar (rowCtl, o + "cutoff"), bar (rowCtl, o + "drive"),
                                        tg ("mute", "M"), tg ("solo", "S"), cmb ("route_mode") };
            for (size_t k = 0; k < r.size(); ++k)
                if (r[k] != nullptr) { r[k]->getProperties().set ("rowx", colX ((int) k)); r[k]->getProperties().set ("roww", colW[k]); r[k]->getProperties().set ("rowl", i); }
        }
        rebuildDetail();
    }

    void LayerPage::rebuildDetail()
    {
        forget (detCtl);
        const int g = roundToInt (rawOf (proc, lid ("group")));
        const auto col = groupCol (g);
        const int y0 = 30;                                                   // relative to the detail area (resized() moves them)
        if (detailIdx == DetLayer)
        {
            int k = 0;
            for (auto [s, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "fine", "FINE" }, { "offset", "START" }, { "width", "WIDTH" }, { "resonance", "RESO" },
                     { "attack", "ATTACK" }, { "decay", "DECAY" }, { "sustain", "SUSTAIN" }, { "release", "RELEASE" }, { "env_delay", "DELAY" }, { "env_amount", "ENV AMT" }, { "time_scale", "TIME SCALE" } })
                if (auto* c = knob (detCtl, lid (s), lab, col, 30, String (s) == "fine")) place (c, (k++) * 70, y0 + 34, 68, doomsday::flat::Knob::heightFor (30));
            int cx = 0;
            for (auto [s, cap] : std::initializer_list<std::pair<const char*, const char*>> { { "env_mode", "ENVELOPE" }, { "filter_type", "FILTER" }, { "link_source", "LINK TO" } })
                if (auto* c = combo (detCtl, lid (s), cap)) { place (c, cx, y0 + 6, 120, 22); cx += 128; }
            for (auto [s, t] : std::initializer_list<std::pair<const char*, const char*>> { { "invert", "INVERT" }, { "link_level", "LINK LVL" }, { "link_pitch", "LINK PITCH" },
                     { "link_length", "LINK LEN" }, { "link_env", "LINK ENV" }, { "send_pre", "PRE-FADER" } })
                if (auto* c = toggle (detCtl, lid (s), t, col, true)) { place (c, cx, y0 + 6, 70, 22); cx += 74; }
            k = 0;
            for (auto [s, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "send_punch", "> PUNCH" }, { "send_body", "> BODY" }, { "send_tail", "> TAIL" },
                     { "send_sub", "> SUB" }, { "send_texture", "> TEXTURE" }, { "send_custom", "> CUSTOM" }, { "send_master", "> MASTER" } })
                if (auto* c = knob (detCtl, lid (s), lab, col, 28)) place (c, (k++) * 70, y0 + 116, 68, doomsday::flat::Knob::heightFor (28));
        }
        else if (detailIdx == DetGroups)
        {
            const char* gk[6] { "punch", "body", "tail", "sub", "texture", "custom" };
            for (int gi = 0; gi < 6; ++gi)
            {
                const String pre = String ("group_") + gk[gi] + "_";
                const int x = gi * 196;
                if (auto* c = toggle (detCtl, pre + "on", String (gk[gi]).toUpperCase(), groupCol (gi))) place (c, x, y0 + 4, 96, 22);
                if (auto* c = toggle (detCtl, pre + "mute", "M", groupCol (gi), true)) place (c, x + 100, y0 + 4, 30, 22);
                if (auto* c = toggle (detCtl, pre + "solo", "S", groupCol (gi), true)) place (c, x + 134, y0 + 4, 30, 22);
                if (auto* c = combo (detCtl, pre + "output", "OUTPUT")) place (c, x, y0 + 48, 180, 22);
                int k = 0;
                for (auto [s, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "level", "LEVEL" }, { "pan", "PAN" }, { "width", "WIDTH" } })
                    if (auto* c = knob (detCtl, pre + s, lab, groupCol (gi), 28, String (s) == "pan")) place (c, x + (k++) * 62, y0 + 80, 60, doomsday::flat::Knob::heightFor (28));
            }
            if (auto* c = knob (detCtl, "bus_texture_level", "TEXTURE BUS", palette().silver, 28)) place (c, 0, y0 + 160, 90, doomsday::flat::Knob::heightFor (28));
            if (auto* c = knob (detCtl, "bus_custom_level", "CUSTOM BUS", palette().silver, 28)) place (c, 96, y0 + 160, 90, doomsday::flat::Knob::heightFor (28));
            if (auto* c = combo (detCtl, "layer_solo_mode", "SOLO MODE")) place (c, 200, y0 + 176, 170, 22);
        }
        const bool tools = detailIdx == DetTools && viewIdx == ViewLayers;
        for (Component* c : std::initializer_list<Component*> { &randMode, &randIntensity, &randSeed, &keepGroups, &keepRouting, &randBtn, &alignRef, &alignTarget, &alignSearch, &analyseBtn, &applyBtn })
            c->setVisible (tools);
        for (Component* c : std::initializer_list<Component*> { &copyBtn, &pasteBtn, &dupBtn, &delBtn, &upBtn, &downBtn, &renameBtn, &lockBtn })
            c->setVisible (viewIdx == ViewLayers);
        lockBtn.setToggleState (layers::locked (proc, layerSel), dontSendNotification);
        resized();
    }

    void LayerPage::showView (int v)
    {
        viewIdx = jlimit (0, 2, v);
        for (auto& c : noiseCtl) c->setVisible (viewIdx == ViewNoise);
        for (auto& c : trCtl) c->setVisible (viewIdx == ViewTransient);
        listView.setVisible (viewIdx == ViewLayers);
        for (auto& b : noiseBtn) b.setVisible (viewIdx == ViewNoise);
        for (auto& b : noiseEditBtn) b.setVisible (viewIdx == ViewNoise);
        for (auto& b : detailBtn) b.setVisible (viewIdx == ViewLayers);
        const auto& c = palette();
        for (int i = 0; i < 3; ++i)
        {
            viewBtn[(size_t) i].setColour (TextButton::buttonColourId, i == viewIdx ? c.red : c.control);
            viewBtn[(size_t) i].setColour (TextButton::textColourOffId, i == viewIdx ? c.ground : c.silver);
        }
        editor.setVisible (true);
        rebuildDetail();
        for (auto& cc : detCtl) cc->setVisible (viewIdx == ViewLayers);
        refreshEditor();
        repaint();
    }
    void LayerPage::selectNoise (int n)
    {
        noiseSel = jlimit (0, 1, n);
        buildNoise();
        for (auto& c : noiseCtl) c->setVisible (viewIdx == ViewNoise);
        const auto& c = palette();
        for (int i = 0; i < 2; ++i) { noiseBtn[(size_t) i].setColour (TextButton::buttonColourId, i == noiseSel ? c.part[5] : c.control); noiseBtn[(size_t) i].setColour (TextButton::textColourOffId, i == noiseSel ? c.ground : c.silver); }
        refreshEditor(); resized(); repaint();
    }
    void LayerPage::selectLayer (int l) { layerSel = jlimit (0, 7, l); for (auto& r : rows) { r.selected = r.layer == layerSel; r.repaint(); } rebuildDetail(); refreshEditor(); repaint(); }
    void LayerPage::showDetail (int d)
    {
        detailIdx = jlimit (0, 2, d);
        const auto& c = palette();
        for (int i = 0; i < 3; ++i) { detailBtn[(size_t) i].setColour (TextButton::buttonColourId, i == detailIdx ? c.silver : c.control); detailBtn[(size_t) i].setColour (TextButton::textColourOffId, i == detailIdx ? c.ground : c.silver); }
        rebuildDetail(); refreshEditor(); repaint();
    }

    // the drawn shape of the current view (noise profile / envelopes, the CUSTOM transient, the layer's MULTIPOINT envelope)
    void LayerPage::refreshEditor()
    {
        int slot;
        if (viewIdx == ViewNoise) slot = (noiseEdit == 0 ? env::ProfileNoiseA : noiseEdit == 1 ? env::AmpNoiseA : env::ModNoiseA) + noiseSel;
        else if (viewIdx == ViewTransient) slot = env::TransientCustom;
        else slot = env::AmpLayer1 + layerSel;
        editor.setVisible (viewIdx != ViewLayers || detailIdx == DetLayer);
        EnvEditor::Axis a;
        a.label = [] (float v) { return String (roundToInt (v * 100.f)) + " %"; };
        a.toUser = [] (float v) { return v * 100.f; }; a.fromUser = [] (float x) { return x / 100.f; }; a.unit = "%";
        if (viewIdx == ViewNoise && noiseEdit == 0) { a.spanMs = 100.f; a.timeUnit = " % freq"; a.label = [] (float v) { return String (roundToInt (v * 36.f - 24.f)) + " dB"; }; }
        else if (viewIdx == ViewNoise) a.spanMs = noiseEdit == 1 ? rawOf (proc, nid ("length")) : rawOf (proc, nid ("mod_attack")) + rawOf (proc, nid ("mod_decay"));
        else if (viewIdx == ViewTransient) a.spanMs = rawOf (proc, "tr_click_length");
        else a.spanMs = rawOf (proc, lid ("length")) * rawOf (proc, lid ("time_scale"));
        editor.setAxis (a);
        editor.setColour (viewIdx == ViewNoise ? palette().part[5] : viewIdx == ViewTransient ? palette().part[0] : groupCol (roundToInt (rawOf (proc, lid ("group")))));
        editor.setShape (proc.shape (slot), GeProcessor::slotMin (slot), 1.f);
        String why; bool on = true;
        if (viewIdx == ViewNoise && noiseEdit == 0 && roundToInt (rawOf (proc, nid ("type"))) != dsp::NkCustom) { on = false; why = "SOURCE is not CUSTOM - drawing switches it to CUSTOM"; }
        if (viewIdx == ViewNoise && noiseEdit == 1 && roundToInt (rawOf (proc, nid ("env"))) != dsp::NeDrawn) { on = false; why = "ENVELOPE is not DRAWN - drawing switches it to DRAWN"; }
        if (viewIdx == ViewNoise && noiseEdit == 2 && (rawOf (proc, nid ("mod_on")) < 0.5f || roundToInt (rawOf (proc, nid ("mod_mode"))) != 1)) { on = false; why = "MOD ENV is not DRAWN - drawing switches it on (DRAWN)"; }
        if (viewIdx == ViewTransient && roundToInt (rawOf (proc, "tr_source")) != dsp::TrCustom) { on = false; why = "SOURCE is not CUSTOM - drawing switches it to CUSTOM"; }
        if (viewIdx == ViewLayers && roundToInt (rawOf (proc, lid ("env_mode"))) != dsp::LeMulti) { on = false; why = "ENVELOPE is not MULTIPOINT - drawing switches it"; }
        editor.setActive (on, why);
        resized();
    }

    void LayerPage::layoutRows()
    {
        const auto o = layers::order (proc);
        for (int r = 0; r < 8; ++r)
        {
            const int i = o[(size_t) r];
            rows[(size_t) i].setBounds (0, r * 34, listHolder.getWidth(), 32);
            rows[(size_t) i].col = groupCol (roundToInt (rawOf (proc, layers::lid (i, "group"))));
            rows[(size_t) i].waveArea = { waveX, 3, listHolder.getWidth() - waveX - 4, 26 };
            for (auto& c : rowCtl)
                if ((int) c->getProperties()["rowl"] == i) c->setBounds ((int) c->getProperties()["rowx"], r * 34 + 5, (int) c->getProperties()["roww"], 22);
        }
    }

    void LayerPage::resized()
    {
        const int w = getWidth(), h = getHeight();
        top = { 0, 0, w, 30 };
        for (int i = 0; i < 3; ++i) viewBtn[(size_t) i].setBounds (i * 112, 0, 108, 26);
        loadBtn.setBounds (w - 64, 0, 64, 26); saveBtn.setBounds (w - 132, 0, 64, 26); presetBtn.setBounds (w - 220, 0, 84, 26);
        content = { 0, 36, w, h - 36 };
        if (viewIdx == ViewNoise)
        {
            for (int i = 0; i < 2; ++i) noiseBtn[(size_t) i].setBounds (0, 50 + i * 32, 140, 28);
            waveArea = { 160, 340, 520, h - 350 };
            editArea = { 690, 372, w - 690, h - 382 };
            for (int i = 0; i < 3; ++i) noiseEditBtn[(size_t) i].setBounds (690 + i * 92, 340, 88, 26);
        }
        else if (viewIdx == ViewTransient) { waveArea = { 0, 290, 600, h - 300 }; editArea = { 610, 290, w - 610, h - 300 }; }
        else
        {
            listArea = { 0, 56, w, 8 * 34 + 4 };
            listView.setBounds (listArea);
            listHolder.setSize (listArea.getWidth() - 12, 8 * 34);
            layoutRows();
            detailArea = { 0, listArea.getBottom() + 6, w, h - listArea.getBottom() - 6 };
            for (int i = 0; i < 3; ++i) detailBtn[(size_t) i].setBounds (i * 90, detailArea.getY(), 86, 24);
            editArea = { 780, detailArea.getY() + 30, w - 780, 190 };
            int bx = 280;
            for (auto* b : { &copyBtn, &pasteBtn, &dupBtn, &delBtn, &upBtn, &downBtn, &renameBtn, &lockBtn }) { b->setBounds (bx, detailArea.getY(), 78, 24); bx += 82; }
            const int ty = detailArea.getY() + 40;
            randMode.setBounds (0, ty, 160, 24); randIntensity.setBounds (170, ty, 180, 24); randSeed.setBounds (360, ty, 120, 24);
            keepGroups.setBounds (0, ty + 32, 170, 22); keepRouting.setBounds (180, ty + 32, 180, 22); randBtn.setBounds (370, ty + 30, 110, 26);
            alignRef.setBounds (0, ty + 90, 160, 24); alignTarget.setBounds (170, ty + 90, 180, 24); alignSearch.setBounds (360, ty + 90, 150, 24);
            analyseBtn.setBounds (520, ty + 90, 100, 26); applyBtn.setBounds (630, ty + 90, 110, 26);
            alignArea = { 0, ty + 124, 740, detailArea.getBottom() - ty - 128 };
        }
        editor.setBounds (editArea);
        for (auto& [c, r] : layoutMap)
        {
            const bool det = std::find_if (detCtl.begin(), detCtl.end(), [cc = c] (auto& u) { return u.get() == cc; }) != detCtl.end();
            c->setBounds (det ? r.translated (0, detailArea.getY()) : r);
        }
    }

    void LayerPage::Row::paint (Graphics& g)
    {
        const auto& c = palette();
        g.setColour (selected ? c.control.brighter (0.15f) : c.panel); g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.f);
        g.setColour (col); g.fillRect (2, 4, 3, getHeight() - 8);
        g.setColour (c.silver); g.setFont (FontOptions (11.f, Font::bold));
        g.drawText (String (layer + 1), 8, 0, 18, getHeight(), Justification::centredLeft);
        if (auto* page = findParentComponentOfClass<LayerPage>())
        {
            g.setFont (FontOptions (11.f));
            g.drawText (layers::name (page->proc, layer) + (layers::locked (page->proc, layer) ? "  [L]" : ""), 26, 0, 104, getHeight(), Justification::centredLeft);
        }
        // the layer's real waveform (rendered alone)
        g.setColour (c.ground); g.fillRect (waveArea);
        if (! wave.hi.empty() && ! wave.silent)
        {
            const float sc = (float) waveArea.getHeight() * 0.48f / jmax (1.0e-4f, wave.peak);
            g.setColour (col.withAlpha (0.85f));
            for (size_t k = 0; k < wave.hi.size(); ++k)
            {
                const float x = (float) waveArea.getX() + (float) waveArea.getWidth() * (float) k / (float) wave.hi.size();
                g.drawVerticalLine ((int) x, (float) waveArea.getCentreY() - wave.hi[k] * sc, (float) waveArea.getCentreY() - wave.lo[k] * sc + 1.f);
            }
        }
    }

    void LayerPage::paint (Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.panel); g.fillRoundedRectangle (content.toFloat(), 6.f);
        g.setFont (lnf.font (9.5f, doomsday::flat::Bold, 0.06f)); g.setColour (c.textDim);
        for (auto& [comp, cap] : captions) if (comp->isVisible() && comp->getParentComponent() == this) g.drawText (cap, comp->getX(), comp->getY() - 14, comp->getWidth(), 12, Justification::left);
        const auto waves = renders.waves();
        auto drawWave = [&] (Rectangle<int> r, int job, Colour col, const String& title)
        {
            g.setColour (c.ground); g.fillRoundedRectangle (r.toFloat(), 5.f);
            g.setColour (c.textDim); g.setFont (FontOptions (10.f, Font::bold)); g.drawText (title, r.reduced (8, 4), Justification::topLeft);
            if (job >= (int) waves.size() || waves[(size_t) job].hi.empty()) return;
            const auto& w = waves[(size_t) job];
            const float sc = (float) r.getHeight() * 0.42f / jmax (1.0e-4f, w.peak);
            g.setColour (col);
            for (size_t k = 0; k < w.hi.size(); ++k)
            {
                const float x = (float) r.getX() + 4.f + (float) (r.getWidth() - 8) * (float) k / (float) w.hi.size();
                g.drawVerticalLine ((int) x, (float) r.getCentreY() - w.hi[k] * sc, (float) r.getCentreY() - w.lo[k] * sc + 1.f);
            }
        };
        if (viewIdx == ViewNoise)
        {
            drawWave (waveArea, 8 + noiseSel, c.part[5], String (noiseSel == 0 ? "NOISE A" : "NOISE B") + " alone (0 - 500 ms) - the real signal");
        }
        else if (viewIdx == ViewTransient)
        {
            auto l = waveArea.removeFromLeft (waveArea.getWidth() / 2 - 4); waveArea.removeFromLeft (8);
            drawWave (l, 10, c.part[0], "TRANSIENT alone (0 - 80 ms)");
            drawWave (waveArea, 11, c.silver, "the whole kick (0 - 500 ms)");
            g.setColour (c.textDim); g.setFont (FontOptions (10.f, Font::bold));
            g.drawText ("ATTACK (gain from each hit, LOW / HIGH split)", 0, 166, 420, 12, Justification::left);
            g.drawText ("TRANSIENT SHAPER (real envelope detector)", 430, 166, 260, 12, Justification::left);
        }
        else
        {
            g.setColour (c.textDim); g.setFont (FontOptions (9.5f, Font::bold));
            static const char* head[14] { "ON", "GROUP", "SOURCE", "LEVEL", "PITCH", "LENGTH", "PAN", "PHASE", "FLT", "CUTOFF", "DRIVE", "M", "S", "ROUTING" };
            g.drawText ("#  NAME", 8, 40, 90, 12, Justification::left);
            for (int k = 0; k < 14; ++k) g.drawText (head[k], colX (k), 40, 90, 12, Justification::left);
            g.drawText ("WAVE (this layer alone)", waveX, 40, 160, 12, Justification::left);
            if (detailIdx == DetTools)
            {
                g.setColour (c.ground); g.fillRoundedRectangle (alignArea.toFloat(), 5.f);
                g.setColour (c.textDim); g.setFont (FontOptions (10.f));
                g.drawText ("RANDOMIZER (LOCKED layers stay)", 0, detailArea.getY() + 26, 400, 12, Justification::left);
                g.drawText ("PHASE ALIGN: correlation of the TARGET with the REFERENCE for each phase - you decide with APPLY", 0, detailArea.getY() + 116, 740, 12, Justification::left);
                if (alignDone && ! lastAlign.corr.empty())
                {
                    Path p;
                    const auto r = alignArea.reduced (10, 8).toFloat();
                    for (size_t k = 0; k < lastAlign.corr.size(); ++k)
                    {
                        const float x = r.getX() + r.getWidth() * (float) k / (float) (lastAlign.corr.size() - 1), y = r.getCentreY() - lastAlign.corr[k] * r.getHeight() * 0.48f;
                        if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                    }
                    g.setColour (c.grid); g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
                    g.setColour (c.hot); g.strokePath (p, PathStrokeType (2.f));
                    g.setColour (c.silver);
                    g.drawText ("now " + String (lastAlign.nowCorr, 2) + "   best " + String (lastAlign.bestCorr, 2) + " at " + String (roundToInt (lastAlign.bestDeg)) + " deg",
                                alignArea.reduced (10, 4), Justification::topRight);
                }
            }
        }
    }

    void LayerPage::runAlign()
    {
        alignDone = false;
        SafePointer<LayerPage> self (this);
        renders.update (proc.frame(), proc.shapes(), proc.shapesVersion(), proc.currentTables());
        renders.analyse (alignRef.getSelectedId() - 1, alignTarget.getSelectedId() - 1, 0.f, 60.f, (float) alignSearch.getValue(), proc.auditionNote,
                         [self] (const dsp::AlignResult& r) { if (self != nullptr) { self->lastAlign = r; self->alignDone = true; self->repaint(); } });
    }
    void LayerPage::applyAlign()
    {
        if (! alignDone) return;
        proc.undo.beginNewTransaction ("Phase align");
        proc.setParamInUndoStep (layers::lid (alignTarget.getSelectedId() - 1, "phase"), lastAlign.bestDeg);
    }

    void LayerPage::timerCallback()
    {
        if (! isShowing()) return;
        renders.update (proc.frame(), proc.shapes(), proc.shapesVersion(), proc.currentTables());
        const auto w = renders.waves();
        for (int i = 0; i < 8; ++i) if (i < (int) w.size()) rows[(size_t) i].wave = w[(size_t) i];
        if (proc.shapesVersion() != lastShapes && ! editor.isDragging()) { lastShapes = proc.shapesVersion(); refreshEditor(); }
        // greyed when it does nothing right now
        auto en = [this] (const String& id, bool on)
        {
            auto range = byId.equal_range (id);
            for (auto it = range.first; it != range.second; ++it) if (it->second->isEnabled() != on) { it->second->setEnabled (on); it->second->setAlpha (on ? 1.f : 0.35f); }
        };
        const int type = roundToInt (rawOf (proc, nid ("type")));
        for (auto* s : { "res_freq", "res_q", "res_amount", "metal" }) en (nid (s), type == dsp::NkMetallic);
        for (auto* s : { "bits", "sr_reduce", "quant" }) en (nid (s), type == dsp::NkDigital);
        const int envSrc = roundToInt (rawOf (proc, nid ("env")));
        for (auto* s : { "attack", "decay" }) en (nid (s), envSrc == dsp::NeOwn);
        en (nid ("length"), envSrc == dsp::NeOwn || envSrc == dsp::NeDrawn);
        const bool mod = rawOf (proc, nid ("mod_on")) > 0.5f;
        for (auto* s : { "mod_mode", "mod_attack", "mod_decay", "mod_level", "mod_cutoff", "mod_resonance", "mod_drive", "mod_color", "mod_width", "mod_grain" }) en (nid (s), mod);
        en (nid ("random_amount"), roundToInt (rawOf (proc, nid ("random_mode"))) != dsp::RmFixed);
        const bool sh = rawOf (proc, "shaper_on") > 0.5f;
        for (auto* s : { "shaper_attack", "shaper_sustain", "shaper_sensitivity", "shaper_time", "shaper_mix" }) en (s, sh);
        for (int i = 0; i < 8; ++i)
        {
            const bool on = rawOf (proc, layers::lid (i, "on")) > 0.5f;
            for (auto& c : rowCtl) if ((int) c->getProperties()["rowl"] == i && byId.count (layers::lid (i, "on")) && c.get() != byId.find (layers::lid (i, "on"))->second) { c->setEnabled (on); c->setAlpha (on ? 1.f : 0.4f); }
            en (layers::lid (i, "cutoff"), on && rawOf (proc, layers::lid (i, "filter_on")) > 0.5f);
        }
        const bool multi = roundToInt (rawOf (proc, lid ("route_mode"))) == 1;
        for (auto* s : { "send_punch", "send_body", "send_tail", "send_sub", "send_texture", "send_custom", "send_master", "send_pre" }) en (lid (s), multi);
        const bool linked = roundToInt (rawOf (proc, lid ("link_source"))) > 0;
        for (auto* s : { "link_level", "link_pitch", "link_length", "link_env" }) en (lid (s), linked);
        const int em = roundToInt (rawOf (proc, lid ("env_mode")));
        en (lid ("sustain"), em == dsp::LeAdsr); en (lid ("release"), em == dsp::LeAdsr);
        for (auto* s : { "attack", "decay" }) en (lid (s), em != dsp::LeMulti);
        en (lid ("resonance"), rawOf (proc, lid ("filter_on")) > 0.5f); en (lid ("filter_type"), rawOf (proc, lid ("filter_on")) > 0.5f);
        layoutRows();
    }

    void LayerPage::showPresetMenu()
    {
        PopupMenu m;
        if (viewIdx == ViewNoise) { int k = 1; for (auto& pr : layers::noisePresets()) m.addItem (k++, pr.name); }
        else if (viewIdx == ViewTransient) { int k = 1; for (auto& pr : layers::transientPresets()) m.addItem (k++, pr.name); }
        else { int k = 1; for (auto& st : layers::layerStacks()) m.addItem (k++, st.name); }
        SafePointer<LayerPage> self (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetBtn), [self] (int r)
        {
            if (self == nullptr || r <= 0) return;
            auto& p = self->proc;
            if (self->viewIdx == ViewNoise) layers::applyNoise (p, self->noiseSel, layers::noisePresets()[(size_t) r - 1]);
            else if (self->viewIdx == ViewTransient) layers::applyTransient (p, layers::transientPresets()[(size_t) r - 1]);
            else layers::applyStack (p, layers::layerStacks()[(size_t) r - 1]);
        });
    }
    void LayerPage::saveFile()
    {
        const char* ext = viewIdx == ViewNoise ? "4gnoise" : viewIdx == ViewTransient ? "4gtrans" : "4glayers";
        std::shared_ptr<XmlElement> x = viewIdx == ViewNoise ? layers::prefixToXml (proc, "NOISE4GE", { nid ("") })
                                      : viewIdx == ViewTransient ? layers::prefixToXml (proc, "TRANSIENT4GE", { "tr_", "shaper_" }) : layers::setToXml (proc);
        if (viewIdx == ViewNoise) x->setAttribute ("from", nid (""));
        chooser = std::make_unique<FileChooser> ("Save", files::folder ("Layers").getChildFile (String ("My ") + ext), String ("*.") + ext);
        const String e (ext);
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [x, e] (const FileChooser& fc)
        { if (fc.getResult() != File()) files::writeXml (fc.getResult().withFileExtension (e), *x); });
    }
    void LayerPage::loadFile()
    {
        const char* ext = viewIdx == ViewNoise ? "4gnoise" : viewIdx == ViewTransient ? "4gtrans" : "4glayers";
        chooser = std::make_unique<FileChooser> ("Load", files::folder ("Layers"), String ("*.") + ext);
        SafePointer<LayerPage> self (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [self] (const FileChooser& fc)
        {
            if (self == nullptr) return;
            auto x = files::readXml (fc.getResult());
            if (x == nullptr) return;
            if (x->hasTagName ("NOISE4GE")) layers::xmlToParams (self->proc, *x, x->getStringAttribute ("from"), self->nid (""));
            else if (x->hasTagName ("TRANSIENT4GE")) layers::xmlToParams (self->proc, *x);
            else layers::xmlToSet (self->proc, *x);
        });
    }
}
