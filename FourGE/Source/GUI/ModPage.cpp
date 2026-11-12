#include "ModPage.h"
#include "Palette.h"
#include "../Presets/VariantRandomizer.h"

using namespace juce;

namespace fge::gui
{
    namespace mo = fge::dsp::mod;
    using fge::modstate::SlotDesc;
    namespace
    {
        int idOf (const String& id) { return fge::modstate::indexOf (id); }
        Colour srcCol (int s)
        {
            const auto& c = palette();
            if (mo::isMacro (s)) return c.red;
            if (s >= mo::SLfo1 && s < mo::SSeq) return c.part[2];
            if (s == mo::SSeq) return c.part[1];
            if (s == mo::SRnd1 || s == mo::SRnd2) return c.part[4];
            return c.part[3];
        }
        const char* groupName (uint32_t g)
        {
            switch (g) { case mo::GCore: return "CORE / PARTS / PITCH"; case mo::GOsc: return "OSCILLATORS"; case mo::GLayers: return "NOISE / TRANSIENT / LAYERS";
                         case mo::GDist: return "DISTORTION"; case mo::GM06: return "FILTER / RESONANCE / SCREECH"; case mo::GTone: return "EQ / MULTIBAND / SPECTRAL / STEREO";
                         case mo::GDyn: return "DYNAMICS"; case mo::GMod: return "MODULATION"; default: return "OTHER"; }
        }
        String shortName (int id) { return id >= 0 ? String (def (id).name) : String ("- choose -"); }
    }

    // ===================================================================== one matrix row
    class ModPage::Row : public Component
    {
    public:
        Row (ModPage& p) : page (p)
        {
            for (auto* b : { &on, &mute, &solo }) { b->setClickingTogglesState (true); addAndMakeVisible (*b); }
            on.setButtonText ("ON"); mute.setButtonText ("M"); solo.setButtonText ("S");
            on.setTooltip ("Switch this connection on / off (the settings stay)."); mute.setTooltip ("Mute this connection."); solo.setTooltip ("Hear only the soloed connections.");
            src.addItemList (mo::sourceNames(), 1); src.setTooltip ("SOURCE: what moves the target (macro, LFO, sequencer, random, velocity, key, envelopes).");
            pol.addItemList (mo::polNames(), 1); pol.setTooltip ("POLARITY: BIPOLAR = the source's own range, UNIPOLAR + / - = folded into 0..1 / -1..0.");
            curve.addItemList (mo::curveNames(), 1); curve.setTooltip ("CURVE of the response.");
            mode.addItemList (mo::modeNames(), 1); mode.setTooltip ("How it combines with other sources on the same target: ADD, MULTIPLY, MAX, MIN, REPLACE.");
            for (auto* c : { &src, &pol, &curve, &mode }) addAndMakeVisible (*c);
            dst.setTooltip ("DESTINATION: any modulatable parameter of 4GE (click to choose).");
            addAndMakeVisible (dst);
            auto setupS = [this] (Slider& s, double lo, double hi, const String& tip, const String& suffix)
            {
                s.setSliderStyle (Slider::LinearBar); s.setRange (lo, hi, 0.0); juce::ignoreUnused (suffix); s.setTooltip (tip);
                s.setTextBoxStyle (Slider::TextBoxLeft, false, 50, 20); s.setDoubleClickReturnValue (true, 0.0);
                addAndMakeVisible (s);
            };
            setupS (depth, -1.0, 1.0, "DEPTH -100..+100 % of the target's range.", ""); setupS (offset, -1.0, 1.0, "OFFSET added before the depth.", "");
            setupS (smooth, 0.0, 500.0, "SMOOTHING (ms) against clicks / zipper.", " ms"); setupS (lag, 0.0, 2000.0, "LAG (ms): the longest time to cross the whole range.", " ms");
            depth.textFromValueFunction = offset.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + String (roundToInt (v * 100.0)) + " %"; };
            smooth.textFromValueFunction = lag.textFromValueFunction = [] (double v) { return String (roundToInt (v)) + " ms"; };
            for (auto* sl : { &depth, &offset, &smooth, &lag }) sl->updateText();
            label.setTooltip ("LABEL: your own name for this connection."); label.setFont (FontOptions (11.f)); addAndMakeVisible (label);
            // edits -> the processor (one undo step per click / drag)
            on.onClick = [this] { edit ([&] (auto& s) { s.on = on.getToggleState(); }); };
            mute.onClick = [this] { edit ([&] (auto& s) { s.mute = mute.getToggleState(); }); };
            solo.onClick = [this] { edit ([&] (auto& s) { s.solo = solo.getToggleState(); }); };
            src.onChange = [this] { edit ([&] (auto& s) { s.src = src.getSelectedId() - 1; if (s.src > 0 && s.dst.isNotEmpty()) s.on = true; }); };
            pol.onChange = [this] { edit ([&] (auto& s) { s.pol = pol.getSelectedId() - 1; }); };
            curve.onChange = [this] { edit ([&] (auto& s) { s.curve = curve.getSelectedId() - 1; }); };
            mode.onChange = [this] { edit ([&] (auto& s) { s.mode = mode.getSelectedId() - 1; }); };
            for (auto* s : { &depth, &offset, &smooth, &lag })
            {
                s->onDragStart = [this] { page.proc.undo.beginNewTransaction ("Modulation"); dragging = true; };
                s->onDragEnd = [this] { dragging = false; };
                s->onValueChange = [this] { edit ([&] (auto& d) { d.depth = (float) depth.getValue(); d.offset = (float) offset.getValue(); d.smoothMs = (float) smooth.getValue(); d.lagMs = (float) lag.getValue(); }, ! dragging); };
            }
            label.onReturnKey = label.onFocusLost = [this] { edit ([&] (auto& s) { s.label = label.getText(); }); };
            dst.onClick = [this] { page.showDestinationMenu (slot, &dst); };
        }
        void load (int s)
        {
            slot = s; loading = true;
            const auto& d = page.proc.modDesc().slot[(size_t) s];
            on.setToggleState (d.on, dontSendNotification); mute.setToggleState (d.mute, dontSendNotification); solo.setToggleState (d.solo, dontSendNotification);
            src.setSelectedId (d.src + 1, dontSendNotification); pol.setSelectedId (d.pol + 1, dontSendNotification);
            curve.setSelectedId (d.curve + 1, dontSendNotification); mode.setSelectedId (d.mode + 1, dontSendNotification);
            const int id = d.dst.isEmpty() ? -1 : idOf (d.dst);
            dst.setButtonText (d.dst.isEmpty() ? String ("- choose -") : id < 0 || ! mo::modulatable (id) ? "? " + d.dst + " (unknown)" : shortName (id));
            if (! dragging) { depth.setValue (d.depth, dontSendNotification); offset.setValue (d.offset, dontSendNotification);
                              smooth.setValue (d.smoothMs, dontSendNotification); lag.setValue (d.lagMs, dontSendNotification); }
            if (! label.hasKeyboardFocus (true)) label.setText (d.label, dontSendNotification);
            dstId = id;
            loading = false;
            repaint();
        }
        void resized() override
        {
            int x = 26; const int h = getHeight() - 2;
            auto put = [&] (Component& c, int w) { c.setBounds (x, 1, w - 3, h); x += w; };
            put (on, 32); put (src, 104); put (dst, 196); put (depth, 104); put (offset, 78); put (pol, 92); put (curve, 92); put (mode, 80);
            put (smooth, 62); put (lag, 62); put (mute, 24); put (solo, 24); put (label, 120);
            liveX = x;
        }
        void paint (Graphics& g) override
        {
            const auto& c = palette();
            g.setColour (slot % 2 ? c.panel : c.panel.brighter (0.03f)); g.fillRect (getLocalBounds());
            if (slot == page.selectedSlot()) { g.setColour (c.red.withAlpha (0.25f)); g.fillRect (0, 0, 24, getHeight()); }
            g.setColour (c.textDim); g.setFont (FontOptions (10.f, Font::bold));
            g.drawText (String (slot + 1), 0, 0, 24, getHeight(), Justification::centred);
            const auto& d = page.proc.modDesc().slot[(size_t) slot];
            if (d.src > 0) { g.setColour (srcCol (d.src)); g.fillRect (24, 3, 2, getHeight() - 6); }
            // live: BASE (tick) and FINAL (bar) of the target, normalised
            const auto r = Rectangle<float> ((float) liveX, 6.f, (float) (getWidth() - liveX - 4), (float) getHeight() - 12.f);
            g.setColour (c.ground); g.fillRect (r);
            if (dstId >= 0 && d.on)
            {
                auto* prm = page.proc.apvts.getParameter (def (dstId).id);
                const float base = prm != nullptr ? prm->getValue() : 0.f;
                const float fin = page.proc.getEngine().slotFinal[(size_t) slot].load();
                g.setColour (srcCol (d.src).withAlpha (0.8f)); g.fillRect (r.withWidth (r.getWidth() * jlimit (0.f, 1.f, fin)));
                g.setColour (c.silver); g.fillRect (r.getX() + r.getWidth() * jlimit (0.f, 1.f, base) - 1.f, r.getY() - 2.f, 2.f, r.getHeight() + 4.f);
            }
        }
        void mouseDown (const MouseEvent& e) override { if (e.x < 26) page.selectSlot (slot); }
        void edit (const std::function<void (SlotDesc&)>& f, bool newStep = true)
        {
            if (loading) return;
            auto d = page.proc.modDesc().slot[(size_t) slot];
            const auto before = d;
            f (d);
            if (! (d == before)) page.proc.setModSlot (slot, d, newStep);
        }
        ModPage& page;
        int slot = 0, dstId = -1, liveX = 0;
        bool loading = false, dragging = false;
        TextButton on, mute, solo, dst;
        ComboBox src, pol, curve, mode;
        Slider depth, offset, smooth, lag;
        TextEditor label;
    };

    // ===================================================================== the sequencer grid (drawn with the mouse)
    class ModPage::StepGrid : public Component, public SettableTooltipClient
    {
    public:
        StepGrid (ModPage& p) : page (p) { setTooltip ("STEPS: click / drag = value. Lanes below: PROBABILITY (drag), RATCHET (click), CURVE (click), TIE / ACCENT / MUTE (click)."); }
        static constexpr int laneH = 16, lanes = 5;           // PROB, RATCHET, CURVE, TIE / ACCENT / MUTE (one lane, 3 cells)
        int length() const { return jlimit (1, mo::maxSteps, roundToInt (page.proc.apvts.getRawParameterValue ("seq_length")->load())); }
        bool unipolar() const { return page.proc.apvts.getRawParameterValue ("seq_polarity")->load() > 0.5f; }
        Rectangle<float> valueArea() const { return getLocalBounds().toFloat().withTrimmedBottom ((float) (laneH * 4 + 6)); }
        float stepW() const { return (float) getWidth() / (float) length(); }
        void paint (Graphics& g) override
        {
            const auto& c = palette();
            const auto st = page.proc.modDesc().step;
            const int n = length(); const float w = stepW();
            const auto va = valueArea();
            g.setColour (c.ground); g.fillRect (getLocalBounds());
            const float zero = unipolar() ? va.getBottom() : va.getCentreY();
            const int now = page.proc.getEngine().seqStepMon.load();
            for (int k = 0; k < n; ++k)
            {
                const auto& s = st[(size_t) k];
                const float x = (float) k * w;
                if (k % 4 == 0) { g.setColour (c.grid); g.fillRect (x, 0.f, 1.f, (float) getHeight()); }
                const float v = unipolar() ? 0.5f * (s.value + 1.f) : s.value;
                const float y = unipolar() ? va.getBottom() - v * va.getHeight() : va.getCentreY() - v * va.getHeight() * 0.5f;
                g.setColour ((s.mute ? c.textDim : c.part[1]).withAlpha (k == now ? 1.f : 0.75f));
                g.fillRect (Rectangle<float> (x + 2.f, jmin (y, zero), w - 4.f, std::abs (zero - y) + 1.f));
                if (s.accent) { g.setColour (c.red); g.fillRect (x + 2.f, va.getY(), w - 4.f, 3.f); }
                // lanes
                float ly = va.getBottom() + 6.f;
                g.setColour (c.control); g.fillRect (x + 2.f, ly, w - 4.f, (float) laneH - 2.f);
                g.setColour (c.part[2]); g.fillRect (x + 2.f, ly + ((float) laneH - 2.f) * (1.f - s.prob), w - 4.f, ((float) laneH - 2.f) * s.prob); ly += (float) laneH;
                g.setColour (c.silver); g.setFont (FontOptions (9.f, Font::bold));
                g.drawText (String (s.ratchet) + "x", Rectangle<float> (x, ly, w, (float) laneH), Justification::centred); ly += (float) laneH;
                static const char* cv[5] { "ST", "LN", "UP", "DN", "SM" };
                g.drawText (cv[jlimit (0, 4, s.curve)], Rectangle<float> (x, ly, w, (float) laneH), Justification::centred); ly += (float) laneH;
                const float cw = (w - 4.f) / 3.f;
                const bool flags[3] { s.tie, s.accent, s.mute };
                for (int f = 0; f < 3; ++f) { g.setColour (flags[f] ? c.red : c.control); g.fillRect (x + 2.f + cw * (float) f, ly + 2.f, cw - 1.f, (float) laneH - 4.f); }
            }
            g.setColour (c.textDim.withAlpha (0.5f)); g.drawHorizontalLine ((int) zero, 0.f, (float) getWidth());
        }
        void set (int k, const std::function<void (mo::Step&)>& f, bool newStep)
        {
            auto st = page.proc.modDesc().step;
            if (! isPositiveAndBelow (k, mo::maxSteps)) return;
            f (st[(size_t) k]);
            page.proc.setModSteps (st, newStep);
            repaint();
        }
        void mouseDown (const MouseEvent& e) override { first = true; lastK = -1; act (e); }
        void mouseDrag (const MouseEvent& e) override { act (e); }
        void act (const MouseEvent& e)
        {
            const int k = jlimit (0, length() - 1, (int) ((float) e.x / stepW()));
            const auto va = valueArea();
            const float y = (float) e.y;
            const bool newStep = first; first = false;
            if (y <= va.getBottom() + 3.f)
            {
                float v = unipolar() ? (va.getBottom() - y) / va.getHeight() * 2.f - 1.f : (va.getCentreY() - y) / (va.getHeight() * 0.5f);
                v = jlimit (-1.f, 1.f, v);
                set (k, [v] (mo::Step& s) { s.value = v; }, newStep);
                return;
            }
            const int lane = (int) ((y - va.getBottom() - 6.f) / (float) laneH);
            if (lane == 0) { const float p = jlimit (0.f, 1.f, 1.f - (y - va.getBottom() - 6.f) / (float) laneH); set (k, [p] (mo::Step& s) { s.prob = p; }, newStep); return; }
            if (k == lastK) return;                     // (clicks below: once per step while dragging)
            lastK = k;
            if (lane == 1) set (k, [] (mo::Step& s) { s.ratchet = s.ratchet % 8 + 1; }, newStep);
            if (lane == 2) set (k, [] (mo::Step& s) { s.curve = (s.curve + 1) % 5; }, newStep);
            if (lane == 3)
            {
                const float cw = (stepW() - 4.f) / 3.f;
                const int f = jlimit (0, 2, (int) (((float) e.x - (float) k * stepW() - 2.f) / cw));
                set (k, [f] (mo::Step& s) { if (f == 0) s.tie = ! s.tie; else if (f == 1) s.accent = ! s.accent; else s.mute = ! s.mute; }, newStep);
            }
        }
        ModPage& page;
        bool first = true;
        int lastK = -1;
    };

    // ===================================================================== factories (APVTS-attached controls)
    Component* ModPage::knob (const String& pid, const String& label, Colour col, bool bip, int d)
    {
        if (idOf (pid) < 0) return nullptr;
        auto k = std::make_unique<doomsday::flat::Knob> (proc.apvts, pid, label, col, bip, d);
        k->slider.setTooltip (fge::tooltip (idOf (pid)));
        auto* r = k.get(); addAndMakeVisible (r); byId.insert ({ pid, r }); ctl.push_back (std::move (k)); return r;
    }
    Component* ModPage::toggle (const String& pid, const String& text, Colour col)
    {
        if (idOf (pid) < 0) return nullptr;
        auto t = std::make_unique<doomsday::flat::Toggle> (proc.apvts, pid, text, doomsday::flat::Toggle::Pill, col);
        t->setTooltip (fge::tooltip (idOf (pid)));
        auto* r = t.get(); addAndMakeVisible (r); byId.insert ({ pid, r }); ctl.push_back (std::move (t)); return r;
    }
    Component* ModPage::combo (const String& pid, const String& caption)
    {
        const int idx = idOf (pid);
        if (idx < 0) return nullptr;
        auto cb = std::make_unique<ComboBox>();
        cb->addItemList (def (idx).choices(), 1);
        comboAtt[cb.get()] = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, pid, *cb);
        cb->setTooltip (fge::tooltip (idx));
        auto* r = cb.get(); addAndMakeVisible (r); byId.insert ({ pid, r }); if (caption.isNotEmpty()) captions[r] = caption; ctl.push_back (std::move (cb)); return r;
    }
    void ModPage::forget()
    {
        for (auto& c : ctl) comboAtt.erase (c.get());
        byId.clear(); captions.clear(); layoutMap.clear(); titles.clear(); ctl.clear();
    }
    Component* ModPage::findControl (const String& pid) { auto it = byId.find (pid); return it == byId.end() ? nullptr : it->second; }
    StringArray ModPage::visibleIds() const { StringArray s; for (auto& [k, c] : byId) if (c->isVisible()) s.addIfNotAlreadyThere (k); return s; }

    // ===================================================================== page
    ModPage::ModPage (GeProcessor& p, doomsday::flat::FlatLookAndFeel& l) : proc (p), lnf (l)
    {
        const char* tabs[NumTabs] { "LFO 1-8", "STEP SEQUENCER", "RANDOM / MACROS", "MORPH / MIDI / FOLLOW", "VARIANT RANDOMIZER" };
        for (int i = 0; i < NumTabs; ++i) { tabBtn[(size_t) i].setButtonText (tabs[i]); tabBtn[(size_t) i].onClick = [this, i] { showTab (i); }; addAndMakeVisible (tabBtn[(size_t) i]); }
        for (int i = 0; i < 8; ++i) { lfoBtn[(size_t) i].setButtonText ("LFO " + String (i + 1)); lfoBtn[(size_t) i].onClick = [this, i] { showLfo (i); }; addChildComponent (lfoBtn[(size_t) i]); }
        for (int i = 0; i < 4; ++i) { viewBtn[(size_t) i].setButtonText (String (i * 16 + 1) + "-" + String (i * 16 + 16)); viewBtn[(size_t) i].onClick = [this, i] { setView (i * 16); }; addAndMakeVisible (viewBtn[(size_t) i]); }
        filterBox.addItem ("SHOW: ALL SLOTS", 1);
        for (int s = 1; s < mo::NumSrc; ++s) filterBox.addItem ("SHOW: " + mo::sourceNames()[s] + (mo::isMacro (s) ? " (mapping)" : ""), s + 1);
        filterBox.setSelectedId (1, dontSendNotification);
        filterBox.onChange = [this] { setFilter (filterBox.getSelectedId() - 2); };
        filterBox.setTooltip ("Show every slot, or only the connections of one source - e.g. a MASTER MACRO = its MACRO MAPPING.");
        addAndMakeVisible (filterBox);
        clearBtn.setTooltip ("Empty every slot (one undo step) - also the default macro mapping.");
        resetBtn.setTooltip ("Back to the default MACRO MAPPING (IMPACT ... CHAOS, 3-4 targets each).");
        addBtn.setTooltip ("Add a connection in the first empty slot (with the shown source).");
        clearBtn.onClick = [this] { proc.clearModMatrix(); };
        resetBtn.onClick = [this] { proc.resetModMatrix(); };
        addBtn.onClick = [this]
        {
            for (int s = 0; s < mo::numSlots; ++s)
                if (proc.modDesc().slot[(size_t) s].empty())
                {
                    SlotDesc d; d.src = filterSrc > 0 ? filterSrc : mo::SLfo1; d.pol = mo::isMacro (d.src) ? mo::PUniPos : mo::PBipolar; d.depth = 0.25f;
                    proc.setModSlot (s, d);
                    if (filterSrc < 0) setView ((s / 16) * 16);
                    return;
                }
        };
        for (Component* c : std::initializer_list<Component*> { &clearBtn, &resetBtn, &addBtn }) addAndMakeVisible (c);
        for (int i = 0; i < 16; ++i) { rows.push_back (std::make_unique<Row> (*this)); addAndMakeVisible (*rows.back()); }
        grid = std::make_unique<StepGrid> (*this); addChildComponent (*grid);
        seqCopy.onClick = [this] { seqClip = proc.modDesc().step; hasSeqClip = true; };
        seqPaste.onClick = [this] { if (hasSeqClip) proc.setModSteps (seqClip); };
        seqRev.onClick = [this] { auto st = proc.modDesc().step; const int n = grid->length(); std::reverse (st.begin(), st.begin() + n); proc.setModSteps (st); };
        seqShift.onClick = [this] { auto st = proc.modDesc().step; const int n = grid->length(); std::rotate (st.begin(), st.begin() + n - 1, st.begin() + n); proc.setModSteps (st); };
        seqRand.onClick = [this] { auto st = proc.modDesc().step; Random r; for (int k = 0; k < grid->length(); ++k) st[(size_t) k].value = r.nextFloat() * 2.f - 1.f; proc.setModSteps (st); };
        seqClear.onClick = [this] { proc.setModSteps ({}); };
        seqRev.setTooltip ("Reverse the steps."); seqShift.setTooltip ("Shift every step one place to the right."); seqRand.setTooltip ("Random step values.");
        for (Component* c : std::initializer_list<Component*> { &seqCopy, &seqPaste, &seqRev, &seqShift, &seqRand, &seqClear }) addChildComponent (c);
        // MORPH / MIDI / FOLLOWER
        storeA.setTooltip ("MACRO MORPH: remember the 12 macro values now as set A."); storeB.setTooltip ("MACRO MORPH: remember the 12 macro values now as set B.");
        storeA.onClick = [this] { proc.storeMorph (false); }; storeB.onClick = [this] { proc.storeMorph (true); };
        for (int i = 0; i < 4; ++i)
        {
            ccLearn[(size_t) i].setButtonText ("LEARN"); ccLearn[(size_t) i].setTooltip ("MIDI LEARN: move a knob on your controller - its CC becomes CC " + String (i + 1) + ".");
            ccLearn[(size_t) i].onClick = [this, i] { proc.startMidiLearn (proc.midiLearnTarget() == i ? -1 : i); };
            addChildComponent (ccLearn[(size_t) i]);
        }
        for (int i = 0; i < 12; ++i) macroBox.addItem (mo::sourceNames()[i + 1], i + 1);
        macroBox.setSelectedId (1, dontSendNotification); macroBox.onChange = [this] { learnMacro = macroBox.getSelectedId() - 1; };
        macroBox.setTooltip ("Which macro the controller should move.");
        macroLearn.setTooltip ("MIDI LEARN for the chosen macro: move a knob on your controller (the macro knob then follows it, also in automation).");
        macroLearn.onClick = [this] { proc.startMidiLearn (proc.midiLearnTarget() == 100 + learnMacro ? -1 : 100 + learnMacro); };
        // VARIANT RANDOMIZER
        styleBox.addItemList (fge::variant::styleNames(), 1); scopeBox.addItemList (fge::variant::scopeNames(), 1);
        styleBox.setTooltip ("STYLE: the musical ranges - EARLY clean / tonal, MODERN controlled, RAW aggressive, EXPERIMENTAL everything.");
        scopeBox.setTooltip ("SCOPE: the whole kick or one area.");
        patternBox.addItemList (fge::variant::earlyPatternNames(), 1);
        patternBox.setTooltip ("EARLY has two patterns: CLASSIC 2004-08 (a 909 punch, a room before the drive, a melodic drop) and EUPHORIC 2009-12 "
                               "(Wildstylez / Headhunterz / Brennan Heart / Noisecontrollers: a short tok, a fast drop, a dense driven tail). AUTO = one of them per click.");
        patternBox.onChange = [this] { saveVariant(); };
        amountSl.setSliderStyle (Slider::LinearBar); amountSl.setRange (0.0, 1.0, 0.01); amountSl.textFromValueFunction = [] (double v) { return "AMOUNT " + String (roundToInt (v * 100.0)) + " %"; };
        amountSl.setTooltip ("AMOUNT: 100 % = a new sound inside the style, less = a variation of the current one.");
        seedSl.setSliderStyle (Slider::LinearBar); seedSl.setRange (1.0, 9999.0, 1.0); seedSl.textFromValueFunction = [] (double v) { return "SEED " + String (roundToInt (v)); };
        seedSl.setTooltip ("SEED: the same seed + settings = the same result.");
        amountSl.updateText(); seedSl.updateText();
        for (int a = 0; a < 7; ++a)
        {
            auto& b = lockBtn[(size_t) a]; b.setButtonText ("LOCK " + fge::variant::areaNames()[a]); b.setClickingTogglesState (true);
            b.setTooltip ("This area never changes."); b.onClick = [this] { saveVariant(); }; addChildComponent (b);
        }
        styleBox.onChange = scopeBox.onChange = [this] { saveVariant(); patternBox.setEnabled (styleBox.getSelectedId() == 1); };
        amountSl.onValueChange = seedSl.onValueChange = [this] { saveVariant(); };
        routingTg.onClick = [this] { saveVariant(); };
        rndGo.setTooltip ("A new seed and a new kick (one undo step; the level stays the same).");
        rndRegen.setTooltip ("Back to the sound before the last RANDOMIZE and the same seed with the settings now (try another STYLE / AMOUNT).");
        rndGo.onClick = [this] { fge::variant::randomize (proc); loadVariant(); };
        rndRegen.onClick = [this] { fge::variant::apply (proc, fge::variant::load (proc), true); };
        doomsday::flat::FlatLookAndFeel::makePrimary (rndGo);
        for (Component* c : std::initializer_list<Component*> { &storeA, &storeB, &macroBox, &macroLearn, &styleBox, &scopeBox, &patternBox, &amountSl, &seedSl, &routingTg, &rndGo, &rndRegen })
            addChildComponent (c);
        loadVariant();
        showTab (TLfo);
        startTimerHz (30);
    }
    ModPage::~ModPage() { stopTimer(); comboAtt.clear(); }

    int ModPage::rowSlot (int row) const { return isPositiveAndBelow (row, (int) visibleSlots.size()) ? visibleSlots[(size_t) row] : -1; }
    void ModPage::editSlot (int slot, const std::function<void (SlotDesc&)>& change)
    {
        if (! isPositiveAndBelow (slot, mo::numSlots)) return;
        auto d = proc.modDesc().slot[(size_t) slot];
        change (d);
        proc.setModSlot (slot, d);
    }
    void ModPage::loadVariant()
    {
        const auto v = fge::variant::load (proc);
        styleBox.setSelectedId (v.style + 1, dontSendNotification); scopeBox.setSelectedId (v.scope + 1, dontSendNotification);
        patternBox.setSelectedId (v.pattern + 1, dontSendNotification); patternBox.setEnabled (v.style == fge::variant::Early);
        amountSl.setValue (v.amount, dontSendNotification); seedSl.setValue (v.seed, dontSendNotification); routingTg.setToggleState (v.routing, dontSendNotification);
        for (int a = 0; a < 7; ++a) lockBtn[(size_t) a].setToggleState ((v.locks >> a) & 1u, dontSendNotification);
    }
    void ModPage::saveVariant()
    {
        fge::variant::Settings v;
        v.style = styleBox.getSelectedId() - 1; v.scope = scopeBox.getSelectedId() - 1; v.amount = (float) amountSl.getValue(); v.seed = (int) seedSl.getValue();
        v.routing = routingTg.getToggleState(); v.pattern = jmax (0, patternBox.getSelectedId() - 1);
        for (int a = 0; a < 7; ++a) if (lockBtn[(size_t) a].getToggleState()) v.locks |= 1u << a;
        fge::variant::save (proc, v);
    }
    juce::String ModPage::destinationMonitor() const
    {
        if (! isPositiveAndBelow (selSlot, mo::numSlots)) return "click a slot number in the matrix: BASE / MODULATION / FINAL of its target";
        const auto& d = proc.modDesc().slot[(size_t) selSlot];
        const int id = d.dst.isEmpty() ? -1 : idOf (d.dst);
        if (id < 0 || ! mo::modulatable (id)) return "SLOT " + String (selSlot + 1) + ": no target";
        auto* prm = proc.apvts.getParameter (def (id).id);
        const auto& R = mo::Ranges::get().r[(size_t) id];
        const float base = prm != nullptr ? prm->getValue() : 0.f, fin = d.on ? proc.getEngine().slotFinal[(size_t) selSlot].load() : base;
        return "SLOT " + String (selSlot + 1) + "  " + String (def (id).name).toUpperCase() + "\nBASE " + formatValue (def (id), R.convertFrom0to1 (base))
               + "   MOD " + (fin >= base ? "+" : "") + String (roundToInt ((fin - base) * 100.f)) + " %   FINAL " + formatValue (def (id), R.convertFrom0to1 (jlimit (0.f, 1.f, fin)));
    }
    // ---- CUSTOM LFO editor: the points of the selected LFO in its view (click = add, drag = move, right-click = remove)
    Point<float> ModPage::ptAt (const mo::LfoShape& sh, int k) const
    {
        const auto r = lfoView.toFloat().reduced (6.f);
        return { r.getX() + r.getWidth() * sh.x[(size_t) k], r.getCentreY() - sh.y[(size_t) k] * r.getHeight() * 0.5f };
    }
    Point<float> ModPage::fromView (Point<float> p) const
    {
        const auto r = lfoView.toFloat().reduced (6.f);
        return { jlimit (0.f, 0.999f, (p.x - r.getX()) / r.getWidth()), jlimit (-1.f, 1.f, (r.getCentreY() - p.y) / (r.getHeight() * 0.5f)) };
    }
    void ModPage::addLfoPoint (float x, float y)
    {
        auto sh = proc.modDesc().lfo[(size_t) lfoSel];
        if (sh.n >= mo::maxPts) return;
        sh.x[(size_t) sh.n] = jlimit (0.f, 0.999f, x); sh.y[(size_t) sh.n] = jlimit (-1.f, 1.f, y); ++sh.n;
        proc.setLfoShape (lfoSel, sh);                     // (the text form sorts the points)
    }
    void ModPage::mouseDown (const MouseEvent& e)
    {
        auto* w = proc.apvts.getRawParameterValue ("lfo" + String (lfoSel + 1) + "_wave");
        if (tabIdx != TLfo || w == nullptr || roundToInt (w->load()) != 8 || ! lfoView.contains (e.getPosition())) return;
        const auto& sh = proc.modDesc().lfo[(size_t) lfoSel];
        int near = -1;
        for (int k = 0; k < sh.n; ++k) if (ptAt (sh, k).getDistanceFrom (e.position) < 8.f) near = k;
        if (e.mods.isPopupMenu())
        {
            if (near >= 0 && sh.n > 1) { auto s2 = sh; for (int k = near; k < s2.n - 1; ++k) { s2.x[(size_t) k] = s2.x[(size_t) k + 1]; s2.y[(size_t) k] = s2.y[(size_t) k + 1]; } --s2.n; proc.setLfoShape (lfoSel, s2); }
            return;
        }
        if (near < 0) { const auto p = fromView (e.position); addLfoPoint (p.x, p.y); }
        dragPt = near; newPtStep = true;
    }
    void ModPage::mouseDrag (const MouseEvent& e)
    {
        if (monitorArea.contains (e.getMouseDownPosition()))          // drag a source from the monitor onto a knob
        {
            auto r = monitorArea.reduced (8, 6); r.removeFromTop (40);
            const int rowsN = mo::NumSrc / 2, colW = r.getWidth() / 2, rh = (r.getHeight() - 20) / rowsN;
            const auto p = e.getMouseDownPosition();
            const int k = ((p.x - r.getX()) / colW) * rowsN + (p.y - r.getY() - 20) / rh, src = k + 1;
            if (p.y > r.getY() + 20 && src > 0 && src < mo::NumSrc)
                if (auto* dc = DragAndDropContainer::findParentDragContainerFor (this)) if (! dc->isDragAndDropActive()) dc->startDragging ("modsrc:" + String (src), this);
            return;
        }
        if (dragPt < 0 || tabIdx != TLfo) return;
        auto sh = proc.modDesc().lfo[(size_t) lfoSel];
        if (dragPt >= sh.n) return;
        const auto p = fromView (e.position);
        const float lo = dragPt > 0 ? sh.x[(size_t) dragPt - 1] + 0.005f : 0.f, hi = dragPt + 1 < sh.n ? sh.x[(size_t) dragPt + 1] - 0.005f : 0.999f;
        sh.x[(size_t) dragPt] = dragPt == 0 ? 0.f : jlimit (lo, jmax (lo, hi), p.x); sh.y[(size_t) dragPt] = p.y;
        proc.setLfoShape (lfoSel, sh, newPtStep); newPtStep = false;
    }
    void ModPage::drawStep (int step, float value) { grid->set (step, [value] (mo::Step& s) { s.value = jlimit (-1.f, 1.f, value); }, true); }
    void ModPage::setView (int f) { firstSlot = jlimit (0, mo::numSlots - 16, f); filterSrc = -1; filterBox.setSelectedId (1, dontSendNotification); refreshRows(); }
    void ModPage::setFilter (int s) { filterSrc = s; refreshRows(); }
    void ModPage::refreshRows()
    {
        visibleSlots.clear();
        if (filterSrc > 0) { for (int s = 0; s < mo::numSlots && (int) visibleSlots.size() < 16; ++s) if (proc.modDesc().slot[(size_t) s].src == filterSrc) visibleSlots.push_back (s); }
        else for (int s = firstSlot; s < firstSlot + 16; ++s) visibleSlots.push_back (s);
        for (int r = 0; r < 16; ++r)
        {
            rows[(size_t) r]->setVisible (r < (int) visibleSlots.size());
            if (r < (int) visibleSlots.size()) rows[(size_t) r]->load (visibleSlots[(size_t) r]);
        }
        shownVersion = proc.modVersion();
        repaint();
    }
    void ModPage::showTab (int t) { tabIdx = jlimit (0, NumTabs - 1, t); build(); }
    void ModPage::showLfo (int i) { lfoSel = jlimit (0, 7, i); build(); }

    void ModPage::build()
    {
        forget();
        const auto& pc = palette();
        const int kh = doomsday::flat::Knob::heightFor (26);
        const int y0 = editArea.getY() + 34;
        auto knobRow = [&] (std::initializer_list<std::tuple<String, String, bool>> ks, int x, int y, Colour col, int step = 74)
        { int k = 0; for (auto& [pid, lab, bip] : ks) place (knob (pid, lab, col, bip), x + (k++) * step, y, step - 4, kh); };
        for (int i = 0; i < 8; ++i) lfoBtn[(size_t) i].setVisible (tabIdx == TLfo);
        for (Component* c : std::initializer_list<Component*> { &storeA, &storeB, &macroBox, &macroLearn }) c->setVisible (tabIdx == TMidi);
        for (auto& b : ccLearn) b.setVisible (tabIdx == TMidi);
        for (Component* c : std::initializer_list<Component*> { &styleBox, &scopeBox, &patternBox, &amountSl, &seedSl, &routingTg, &rndGo, &rndRegen }) c->setVisible (tabIdx == TVariant);
        for (auto& b : lockBtn) b.setVisible (tabIdx == TVariant);
        grid->setVisible (tabIdx == TSeq);
        for (Component* c : std::initializer_list<Component*> { &seqCopy, &seqPaste, &seqRev, &seqShift, &seqRand, &seqClear }) c->setVisible (tabIdx == TSeq);
        if (tabIdx == TLfo)
        {
            const String o = "lfo" + String (lfoSel + 1) + "_";
            const auto col = pc.part[2];
            int x = 0;
            for (auto& [pid, cap] : std::initializer_list<std::pair<String, String>> { { o + "wave", "WAVE" }, { o + "retrigger", "RETRIGGER" }, { o + "mode", "MODE" }, { o + "polarity", "POLARITY" }, { o + "division", "DIVISION" } })
            { place (combo (pid, cap), x, y0 + 46, 118, 22); x += 124; }
            place (toggle (o + "sync", "HOST SYNC", col), x, y0 + 46, 100, 22);
            place (combo (o + "interp", "CUSTOM"), x + 106, y0 + 46, 84, 22);
            knobRow ({ { o + "rate", "RATE", false }, { o + "phase", "PHASE", false }, { o + "fade_in", "FADE IN", false }, { o + "fade_out", "FADE OUT", false },
                       { o + "smooth", "SMOOTH", false }, { o + "pw", "PULSE W", false } }, 0, y0 + 82, col);
            lfoView = { 460, y0 + 80, 420, kh + 26 };
        }
        else if (tabIdx == TSeq)
        {
            const auto col = pc.part[1];
            grid->setBounds (editArea.getX(), y0, 520, editArea.getBottom() - y0 - 4);
            int x = 532;
            for (auto& [pid, cap] : std::initializer_list<std::pair<String, String>> { { "seq_division", "DIVISION" }, { "seq_direction", "DIRECTION" }, { "seq_reset", "RESET" } })
            { place (combo (pid, cap), x, y0 + 12, 112, 22); x += 118; }
            x = 532;
            for (auto& [pid, cap] : std::initializer_list<std::pair<String, String>> { { "seq_skip", "SKIPPED STEP" }, { "seq_polarity", "POLARITY" } })
            { place (combo (pid, cap), x, y0 + 54, 112, 22); x += 118; }
            place (toggle ("seq_sync", "HOST SYNC", col), x, y0 + 54, 110, 22);
            knobRow ({ { "seq_length", "LENGTH", false }, { "seq_rate", "RATE", false }, { "seq_swing", "SWING", false }, { "seq_humanize", "HUMANIZE", false } }, 532, y0 + 86, col, 64);
            knobRow ({ { "seq_smooth", "SMOOTH", false }, { "seq_accent", "ACCENT", false }, { "seq_seed", "SEED", false } }, 532, y0 + 86 + kh + 4, col, 64);
            const int bx = 532 + 4 * 64 + 6;
            int k = 0;
            for (Component* c : std::initializer_list<Component*> { &seqCopy, &seqPaste, &seqRev, &seqShift, &seqRand, &seqClear }) c->setBounds (editArea.getX() + bx, y0 + 86 + (k++) * 26, 70, 22);
        }
        else if (tabIdx == TMidi)
        {
            const auto col = pc.part[3];
            titles.push_back ({ { 0, y0 - 2, 300, 12 }, "MACRO MORPH (the 12 macros glide from set A to set B)" });
            place (toggle ("macro_morph_on", "MORPH ON", pc.red), 0, y0 + 14, 100, 22);
            place (knob ("macro_morph", "MORPH A > B", pc.red), 106, y0 + 4, 80, kh);
            titles.push_back ({ { 0, y0 + 76, 400, 12 }, "MIDI CC 1-4 (sources) + MIDI LEARN of a macro" });
            for (int i = 0; i < 4; ++i)
            {
                place (knob ("cc" + String (i + 1) + "_number", "CC " + String (i + 1), col, false, 22), i * 104, y0 + 92, 60, kh - 4);
                place (knob ("cc" + String (i + 1) + "_smooth", "SMOOTH", col, false, 22), i * 104 + 52, y0 + 92, 52, kh - 4);
            }
            titles.push_back ({ { 450, y0 - 2, 400, 12 }, "AUDIO FOLLOWER (the level of a part as a source)" });
            place (combo ("fol_source", "SOURCE"), 450, y0 + 26, 100, 22);
            place (combo ("fol_detect", "DETECTION"), 556, y0 + 26, 100, 22);
            knobRow ({ { "fol_attack", "ATTACK", false }, { "fol_release", "RELEASE", false }, { "fol_gain", "GAIN", true }, { "fol_threshold", "THRESHOLD", false },
                       { "fol_range", "RANGE", false }, { "fol_sensitivity", "GATE", false }, { "fol_smooth", "SMOOTH", false } }, 450, y0 + 54, pc.part[3], 60);
        }
        else if (tabIdx == TVariant)
        {
            titles.push_back ({ { 0, y0 - 2, 600, 12 }, "VARIANT RANDOMIZER: STYLE + SCOPE + LOCKS + SEED - one click = one undo step, the level stays the same" });
        }
        else
        {
            for (int i = 0; i < 2; ++i)
            {
                const String o = "rnd" + String (i + 1) + "_";
                const int x0 = i * 450, col = 4;
                place (combo (o + "mode", "RANDOM " + String (i + 1) + " MODE"), x0, y0 + 10, 150, 22);
                place (combo (o + "polarity", "POLARITY"), x0 + 160, y0 + 10, 110, 22);
                knobRow ({ { o + "seed", "SEED", false }, { o + "rate", "RATE", false }, { o + "smooth", "SMOOTH", false }, { o + "amount", "AMOUNT", false }, { o + "deviation", "MAX STEP", false } },
                         x0, y0 + 40, pc.part[(size_t) col], 66);
            }
            titles.push_back ({ { 0, y0 + 40 + kh + 8, 600, 12 }, "MASTER MACROS (sources only - the MACRO MAPPING = the matrix rows with a macro)  +  USER MACROS  +  MOVEMENT" });
            const char* mids[12] { "macro_impact", "macro_body", "macro_tail", "macro_pitch", "macro_grit", "macro_scream", "macro_weight", "macro_chaos", "macro_user1", "macro_user2", "macro_user3", "macro_user4" };
            for (int i = 0; i < 12; ++i) place (knob (mids[i], mo::sourceNames()[i + 1], i < 8 ? pc.red : pc.silver), i * 66, y0 + 40 + kh + 24, 62, kh);
            place (knob ("mod_movement", "MOVEMENT", pc.part[2]), 12 * 66 + 10, y0 + 40 + kh + 24, 70, kh);
        }
        // knobs / combos are placed relative to the editor area
        for (auto& [c, r] : layoutMap) r = r.translated (editArea.getX(), 0);
        for (auto& t : titles) t.first = t.first.translated (editArea.getX(), 0);
        lfoView = lfoView.translated (editArea.getX(), 0);
        resized();
        repaint();
    }

    void ModPage::showDestinationMenu (int slot, Component* target)
    {
        // every modulatable parameter, by section and by its first word (dist1, eq3, tail ...)
        std::map<uint32_t, std::map<String, PopupMenu>> tree;
        for (int id = 0; id < P::count; ++id)
        {
            if (! mo::modulatable (id)) continue;
            const String sid (def (id).id);
            tree[mo::groupOf (id)][sid.upToFirstOccurrenceOf ("_", false, false).toUpperCase()].addItem (id + 1, def (id).name);
        }
        PopupMenu m;
        for (auto& [g, sub] : tree)
        {
            PopupMenu gm;
            for (auto& [name, items] : sub) gm.addSubMenu (name, items);
            m.addSubMenu (groupName (g), gm);
        }
        m.addSeparator(); m.addItem (-1, "(none)");
        SafePointer<ModPage> self (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (target), [self, slot] (int r)
        {
            if (self == nullptr || r == 0) return;
            if (r < 0) { self->editSlot (slot, [] (SlotDesc& s) { s.dst = {}; s.on = false; }); return; }
            self->pickDestination (slot, def (r - 1).id);
        });
    }

    void ModPage::timerCallback()
    {
        if (proc.modVersion() != shownVersion) refreshRows();
        else for (auto& r : rows) if (r->isVisible()) r->repaint();
        const auto& pc = palette();
        for (int i = 0; i < NumTabs; ++i)
        {
            tabBtn[(size_t) i].setColour (TextButton::buttonColourId, i == tabIdx ? pc.red.withAlpha (0.8f) : pc.control);
            tabBtn[(size_t) i].setColour (TextButton::textColourOffId, i == tabIdx ? pc.ground : pc.silver);
        }
        for (int i = 0; i < 8; ++i)
        {
            lfoBtn[(size_t) i].setColour (TextButton::buttonColourId, i == lfoSel ? pc.part[2] : pc.control);
            lfoBtn[(size_t) i].setColour (TextButton::textColourOffId, i == lfoSel ? pc.ground : pc.silver);
        }
        for (int i = 0; i < 4; ++i) viewBtn[(size_t) i].setColour (TextButton::buttonColourId, filterSrc < 0 && firstSlot == i * 16 ? pc.red.withAlpha (0.6f) : pc.control);
        // a synced LFO: RATE is not used; a free LFO: DIVISION is not used; sequencer likewise
        auto en = [&] (const String& pid, bool on) { for (auto [it, end] = byId.equal_range (pid); it != end; ++it) { it->second->setEnabled (on); it->second->setAlpha (on ? 1.f : 0.4f); } };
        auto rawOf = [&] (const String& pid) { auto* v = proc.apvts.getRawParameterValue (pid); return v != nullptr ? v->load() : 0.f; };
        if (tabIdx == TLfo)
        {
            const String o = "lfo" + String (lfoSel + 1) + "_";
            const bool sync = rawOf (o + "sync") > 0.5f; const int w = roundToInt (rawOf (o + "wave"));
            en (o + "rate", ! sync); en (o + "division", sync); en (o + "pw", w == 4 || w == 5);
            repaint (lfoView);
        }
        if (tabIdx == TLfo) en ("lfo" + String (lfoSel + 1) + "_interp", roundToInt (rawOf ("lfo" + String (lfoSel + 1) + "_wave")) == 8);
        if (tabIdx == TMidi)
        {
            const int lt = proc.midiLearnTarget();
            for (int i = 0; i < 4; ++i) ccLearn[(size_t) i].setColour (TextButton::buttonColourId, lt == i ? pc.red : pc.control);
            macroLearn.setColour (TextButton::buttonColourId, lt == 100 + learnMacro ? pc.red : pc.control);
            storeA.setColour (TextButton::buttonColourId, proc.modDesc().hasA ? pc.control.brighter (0.3f) : pc.control);
            storeB.setColour (TextButton::buttonColourId, proc.modDesc().hasB ? pc.control.brighter (0.3f) : pc.control);
            en ("macro_morph", proc.modDesc().hasA && proc.modDesc().hasB);
        }
        repaint (0, 0, getWidth(), 28);
        if (tabIdx == TSeq)
        {
            const bool sync = rawOf ("seq_sync") > 0.5f;
            en ("seq_rate", ! sync); en ("seq_division", sync);
            grid->repaint();
        }
        if (tabIdx == TRandom)
            for (int i = 1; i <= 2; ++i)
            {
                const String o = "rnd" + String (i) + "_"; const int m = roundToInt (rawOf (o + "mode"));
                en (o + "rate", m == mo::RFreeRun); en (o + "deviation", m == mo::RControlled);
            }
        repaint (monitorArea);
    }

    void ModPage::resized()
    {
        const int w = getWidth(), h = getHeight();
        for (int i = 0; i < 4; ++i) viewBtn[(size_t) i].setBounds (170 + i * 66, 0, 62, 24);
        filterBox.setBounds (440, 0, 230, 24);
        addBtn.setBounds (680, 0, 70, 24); clearBtn.setBounds (756, 0, 90, 24); resetBtn.setBounds (852, 0, 130, 24);
        matrixArea = { 0, 30, w, 16 * 22 + 18 };
        for (int r = 0; r < 16; ++r) rows[(size_t) r]->setBounds (0, matrixArea.getY() + 18 + r * 22, w, 22);
        editArea = { 0, matrixArea.getBottom() + 8, w - 300, h - matrixArea.getBottom() - 8 };
        monitorArea = { w - 290, editArea.getY(), 290, editArea.getHeight() };
        for (int i = 0; i < NumTabs; ++i) tabBtn[(size_t) i].setBounds (editArea.getX() + i * 174, editArea.getY(), 170, 24);
        const int y0 = editArea.getY() + 34;
        for (int i = 0; i < 8; ++i) lfoBtn[(size_t) i].setBounds (editArea.getX() + i * 58, y0, 54, 22);
        storeA.setBounds (editArea.getX() + 196, y0 + 14, 74, 22); storeB.setBounds (editArea.getX() + 274, y0 + 14, 74, 22);
        for (int i = 0; i < 4; ++i) ccLearn[(size_t) i].setBounds (editArea.getX() + i * 104, y0 + 92 + 70, 98, 20);
        macroBox.setBounds (editArea.getX() + 450, y0 + 140, 120, 22); macroLearn.setBounds (editArea.getX() + 576, y0 + 140, 80, 22);
        styleBox.setBounds (editArea.getX(), y0 + 24, 160, 24); scopeBox.setBounds (editArea.getX() + 168, y0 + 24, 160, 24);
        amountSl.setBounds (editArea.getX() + 336, y0 + 24, 200, 24); seedSl.setBounds (editArea.getX() + 544, y0 + 24, 160, 24);
        for (int a = 0; a < 7; ++a) lockBtn[(size_t) a].setBounds (editArea.getX() + a * 122, y0 + 64, 116, 24);
        routingTg.setBounds (editArea.getX(), y0 + 100, 420, 24); patternBox.setBounds (editArea.getX() + 430, y0 + 100, 240, 24);
        rndGo.setBounds (editArea.getX(), y0 + 140, 180, 34); rndRegen.setBounds (editArea.getX() + 188, y0 + 140, 160, 34);
        if (builtFor != editArea) { builtFor = editArea; build(); return; }          // (the editors are laid out in the editor area)
        for (auto& [c, r] : layoutMap) c->setBounds (r);
    }

    void ModPage::paint (Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.silver); g.setFont (FontOptions (12.f, Font::bold));
        g.drawText ("MODULATION MATRIX", 0, 0, 160, 24, Justification::centredLeft);
        // column heads
        g.setColour (c.textDim); g.setFont (FontOptions (9.5f, Font::bold));
        const char* heads[] { "#", "ON", "SOURCE", "DESTINATION", "DEPTH", "OFFSET", "POLARITY", "CURVE", "MODE", "SMOOTH", "LAG", "M", "S", "LABEL", "BASE | FINAL" };
        const int wid[] { 26, 32, 104, 196, 104, 78, 92, 92, 80, 62, 62, 24, 24, 120, 120 };
        int x = 0; for (int i = 0; i < 15; ++i) { g.drawText (heads[i], x, matrixArea.getY(), wid[i], 16, Justification::centredLeft); x += wid[i]; }
        if (filterSrc > 0 && visibleSlots.empty())
        { g.setColour (c.textDim); g.setFont (FontOptions (12.f)); g.drawText ("no connection from " + mo::sourceNames()[filterSrc] + " yet - press + ADD", matrixArea.withTrimmedTop (30), Justification::centredTop); }
        g.setColour (c.panel); g.fillRect (editArea.withTrimmedTop (28)); g.fillRect (monitorArea);
        g.setColour (c.textDim); g.setFont (FontOptions (10.5f, Font::bold));
        for (auto& [comp, cap] : captions) if (comp->isVisible()) g.drawText (cap, comp->getX(), comp->getY() - 12, comp->getWidth() + 40, 11, Justification::centredLeft);
        for (auto& [r, t] : titles) g.drawText (t, r, Justification::centredLeft);
        // LFO: the shape of one cycle (the formula of the selected wave) + where it is now
        if (tabIdx == TLfo && ! lfoView.isEmpty())
        {
            const String o = "lfo" + String (lfoSel + 1) + "_";
            auto rawOf = [&] (const String& pid) { auto* v = proc.apvts.getRawParameterValue (pid); return v != nullptr ? v->load() : 0.f; };
            const int wv = roundToInt (rawOf (o + "wave")); const float pw = rawOf (o + "pw"); const bool uni = rawOf (o + "polarity") > 0.5f;
            const auto r = lfoView.toFloat();
            g.setColour (c.ground); g.fillRoundedRectangle (r, 4.f);
            Path p;
            for (int i = 0; i <= 200; ++i)
            {
                const float ph = (float) i / 200.f; float v;
                switch (wv)
                {
                    case 1: v = ph < 0.5f ? 4.f * ph - 1.f : 3.f - 4.f * ph; break;
                    case 2: v = 2.f * ph - 1.f; break;
                    case 3: v = 1.f - 2.f * ph; break;
                    case 4: v = ph < pw ? 1.f : -1.f; break;
                    case 5: v = ph < pw * 0.25f ? 1.f : -1.f; break;
                    case 6: v = mo::hashNoise (5u, (uint32_t) (ph * 6.f)); break;
                    case 7: { const float a = mo::hashNoise (5u, (uint32_t) (ph * 6.f)), b = mo::hashNoise (5u, (uint32_t) (ph * 6.f) + 1u), u = ph * 6.f - std::floor (ph * 6.f);
                              v = a + (b - a) * (0.5f - 0.5f * std::cos (u * MathConstants<float>::pi)); break; }
                    case 8: v = mo::Engine::customAt (proc.modDesc().lfo[(size_t) lfoSel], ph, rawOf (o + "interp") > 0.5f); break;
                    default: v = std::sin (ph * MathConstants<float>::twoPi); break;
                }
                if (uni) v = 0.5f * (v + 1.f);
                const float px = r.getX() + 6.f + (r.getWidth() - 12.f) * ph, py = uni ? r.getBottom() - 6.f - v * (r.getHeight() - 12.f) : r.getCentreY() - v * (r.getHeight() * 0.5f - 6.f);
                i == 0 ? p.startNewSubPath (px, py) : p.lineTo (px, py);
            }
            g.setColour (c.part[2]); g.strokePath (p, PathStrokeType (1.6f));
            if (wv == 8 && ! uni)
            {
                const auto& sh = proc.modDesc().lfo[(size_t) lfoSel];
                for (int k = 0; k < sh.n; ++k) { const auto q = ptAt (sh, k); g.setColour (c.silver); g.fillEllipse (q.x - 4.f, q.y - 4.f, 8.f, 8.f); }
                g.setColour (c.textDim); g.setFont (FontOptions (9.f)); g.drawText ("click = add point, drag = move, right-click = remove", r.toNearestInt().reduced (6, 3), Justification::bottomLeft);
            }
            const float now = proc.getEngine().srcMon[(size_t) (mo::SLfo1 + lfoSel)].load();
            const float ny = uni ? r.getBottom() - 6.f - now * (r.getHeight() - 12.f) : r.getCentreY() - now * (r.getHeight() * 0.5f - 6.f);
            g.setColour (c.hot); g.fillEllipse (r.getRight() - 16.f, ny - 4.f, 8.f, 8.f);
            g.setColour (c.textDim); g.setFont (FontOptions (9.f)); g.drawText ("one cycle  /  now", r.toNearestInt().reduced (6, 3), Justification::topLeft);
        }
        // DESTINATION MONITOR (the selected slot)

        if (tabIdx == TMidi)
        {
            String map;
            for (int i = 0; i < 12; ++i) if (proc.modDesc().ccMacro[(size_t) i] >= 0) map << mo::sourceNames()[i + 1] << " = CC " << (int) proc.modDesc().ccMacro[(size_t) i] << "   ";
            g.setColour (c.textDim); g.setFont (FontOptions (10.f));
            g.drawText (map.isEmpty() ? String ("no macro on a controller yet") : map, editArea.getX() + 450, editArea.getY() + 34 + 168, 430, 14, Justification::centredLeft);
        }
        // SOURCE MONITOR: every source's value now
        {
            auto r = monitorArea.reduced (8, 6);
            g.setColour (c.silver); g.setFont (FontOptions (10.f, Font::bold));
            g.drawText ("DESTINATION MONITOR", r.getX(), r.getY(), 280, 12, Justification::centredLeft);
            g.setColour (c.textDim); g.setFont (FontOptions (9.5f));
            g.drawFittedText (destinationMonitor(), r.getX(), r.getY() + 13, r.getWidth(), 24, Justification::topLeft, 2);
            r.removeFromTop (40);
            g.setColour (c.silver); g.setFont (FontOptions (10.f, Font::bold));
            g.drawText ("SOURCE MONITOR  (drag a source onto a knob)", r.getX(), r.getY(), 280, 14, Justification::centredLeft);
            const int rowsN = (mo::NumSrc) / 2, colW = r.getWidth() / 2, rh = (r.getHeight() - 20) / rowsN;
            for (int s = 1; s < mo::NumSrc; ++s)
            {
                const int k = s - 1, cx = r.getX() + (k / rowsN) * colW, cy = r.getY() + 20 + (k % rowsN) * rh;
                const float v = proc.getEngine().srcMon[(size_t) s].load();
                g.setColour (c.textDim); g.setFont (FontOptions (9.f, Font::bold));
                g.drawText (mo::sourceNames()[s], cx, cy, 64, rh, Justification::centredLeft);
                const auto bar = Rectangle<float> ((float) cx + 66.f, (float) cy + 3.f, (float) colW - 74.f, (float) rh - 6.f);
                g.setColour (c.ground); g.fillRect (bar);
                g.setColour (srcCol (s)); g.fillRect (bar.withWidth (bar.getWidth() * jlimit (0.f, 1.f, std::abs (v))));
            }
        }
    }
}
