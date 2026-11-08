#include "StereoPage.h"
#include "Palette.h"

using namespace juce;

namespace fge::gui
{
    namespace
    {
        int idOf (const String& id) { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; }
        const char* targetKey[7] { "punch", "body", "tail", "sub", "texture", "custom", "master" };
        const char* targetName[7] { "PUNCH", "BODY", "TAIL", "SUB", "TEXTURE", "CUSTOM", "MASTER" };
        Colour targetCol (int t) { const auto& c = palette(); return t < 4 ? c.part[(size_t) t] : t == 4 ? c.part[5] : t == 5 ? c.part[4] : c.silver; }
    }

    // ===================================================================== the visualizer (draws what the processor's analyzer holds)
    void StereoScope::paint (Graphics& g)
    {
        const auto& c = palette();
        auto& an = proc.analyzer;
        auto r = getLocalBounds();
        g.setColour (c.panel); g.fillRoundedRectangle (r.toFloat(), 6.f);
        g.setColour (c.textDim); g.setFont (FontOptions (11.f, Font::bold));
        g.drawText ("OUTPUT  -  VECTOR SCOPE / CORRELATION / MID - SIDE / L - R", r.getX() + 10, r.getY() + 4, r.getWidth() - 20, 14, Justification::left);
        r.removeFromTop (22); r.reduce (10, 6);
        // VECTOR SCOPE: up = mid, sideways = side (a mono sound is a vertical line)
        const int sz = jmin (r.getWidth(), r.getHeight() - 150);
        auto sq = r.removeFromTop (sz).withSizeKeepingCentre (sz, sz).toFloat();
        g.setColour (c.ground); g.fillRect (sq);
        g.setColour (c.grid); g.drawLine (sq.getCentreX(), sq.getY(), sq.getCentreX(), sq.getBottom()); g.drawLine (sq.getX(), sq.getCentreY(), sq.getRight(), sq.getCentreY());
        g.drawLine (sq.getX(), sq.getY(), sq.getRight(), sq.getBottom(), 0.5f); g.drawLine (sq.getRight(), sq.getY(), sq.getX(), sq.getBottom(), 0.5f);
        g.setFont (FontOptions (10.f)); g.setColour (c.textDim);
        g.drawText ("L", sq.toNearestInt().removeFromLeft (16).removeFromTop (16), Justification::centred);
        g.drawText ("R", sq.toNearestInt().removeFromRight (16).removeFromTop (16), Justification::centred);
        g.drawText ("M", Rectangle<float> (sq.getCentreX() + 2, sq.getY(), 14, 14), Justification::centred);
        float mx = 0.f;
        for (int i = 0; i < fge::dsp::spatial::Analyzer::scopeN; ++i) mx = jmax (mx, std::abs (an.sx[(size_t) i].load()), std::abs (an.sy[(size_t) i].load()));
        peakHold = jmax (mx, peakHold * 0.97f, 0.05f);
        const float sc = 0.48f * sz / peakHold;
        g.setColour (Colour (0xff5fbf8f).withAlpha (0.55f));
        for (int i = 0; i < fge::dsp::spatial::Analyzer::scopeN; ++i)
        {
            const float x = sq.getCentreX() + an.sx[(size_t) i].load() * sc, y = sq.getCentreY() - an.sy[(size_t) i].load() * sc;
            g.fillRect (x, y, 1.5f, 1.5f);
        }
        r.removeFromTop (10);
        // CORRELATION
        const float corr = an.corr.load();
        auto bar = r.removeFromTop (18).toFloat();
        g.setColour (c.ground); g.fillRect (bar);
        const float cx = bar.getCentreX(), px = cx + corr * bar.getWidth() * 0.5f;
        g.setColour (corr < 0.f ? c.red : Colour (0xff5fbf8f)); g.fillRect (Rectangle<float>::leftTopRightBottom (jmin (cx, px), bar.getY() + 3, jmax (cx, px), bar.getBottom() - 3));
        g.setColour (c.silver); g.setFont (FontOptions (11.f));
        g.drawText ("-1", bar.toNearestInt(), Justification::centredLeft); g.drawText ("+1", bar.toNearestInt(), Justification::centredRight);
        g.drawText ("CORRELATION " + String (corr, 2) + (corr < 0.f ? "  - PHASE PROBLEM (mono loses it)" : ""), r.removeFromTop (16), Justification::centred);
        r.removeFromTop (6);
        // MID / SIDE and L / R meters
        auto meter = [&] (const String& name, float v, Colour col)
        {
            auto row = r.removeFromTop (16);
            g.setColour (c.textDim); g.drawText (name, row.removeFromLeft (44), Justification::centredLeft);
            const float dbv = jlimit (-60.f, 0.f, Decibels::gainToDecibels (v, -60.f));
            auto b = row.toFloat().reduced (0, 3);
            g.setColour (c.ground); g.fillRect (b);
            g.setColour (col); g.fillRect (b.withWidth (b.getWidth() * (dbv + 60.f) / 60.f));
            r.removeFromTop (4);
        };
        meter ("MID", an.midRms.load(), c.silver); meter ("SIDE", an.sideRms.load(), Colour (0xff36a9d6));
        meter ("L", an.lRms.load(), c.textDim); meter ("R", an.rRms.load(), c.textDim);
        // STEREO FIELD: where the weight sits (L / R) and how wide (side / mid)
        auto f = r.removeFromTop (18).toFloat();
        g.setColour (c.ground); g.fillRect (f);
        const float l = an.lRms.load(), rr = an.rRms.load(), m = an.midRms.load(), s = an.sideRms.load();
        const float pos = (l + rr) > 1.0e-6f ? (rr - l) / (l + rr) : 0.f, wid = m + s > 1.0e-6f ? jlimit (0.f, 1.f, s / (m + s)) : 0.f;
        const float fc = f.getCentreX() + pos * f.getWidth() * 0.5f, hw = jmax (2.f, wid * f.getWidth() * 0.5f);
        g.setColour (Colour (0xff36a9d6).withAlpha (0.8f)); g.fillRect (Rectangle<float>::leftTopRightBottom (fc - hw, f.getY() + 3, fc + hw, f.getBottom() - 3));
        g.setColour (c.silver); g.fillRect (fc - 1.f, f.getY(), 2.f, f.getHeight());
        g.drawText ("STEREO FIELD", r.removeFromTop (16), Justification::centred);
    }

    // ===================================================================== page
    StereoPage::StereoPage (GeProcessor& p, doomsday::flat::FlatLookAndFeel& l) : proc (p), lnf (l), scope (p)
    {
        for (int i = 0; i < 7; ++i)
        {
            tabBtn[(size_t) i].setButtonText (targetName[i]);
            tabBtn[(size_t) i].onClick = [this, i] { select (i); };
            tabBtn[(size_t) i].setTooltip (i < 6 ? String ("The stereo unit of the ") + targetName[i] + " group (after its distortion / filters / EQ / dynamics)."
                                                 : String ("MASTER STEREO: the whole kick, before the output dynamics."));
            addAndMakeVisible (tabBtn[(size_t) i]);
        }
        monoBtn.setClickingTogglesState (true);
        monoBtn.setTooltip ("MONO AUDITION: listen in mono (L + R) - only for your ears, the preset does not change.");
        monoBtn.onClick = [this] { proc.monoAudition.store (monoBtn.getToggleState()); };
        addAndMakeVisible (monoBtn);
        lockTgl = std::make_unique<doomsday::flat::Toggle> (proc.apvts, "spc_sub_mono_lock", "SUB MONO LOCK", doomsday::flat::Toggle::Pill, palette().part[3]);
        lockTgl->setTooltip (fge::tooltip (idOf ("spc_sub_mono_lock")));
        byId.insert ({ "spc_sub_mono_lock", lockTgl.get() });
        addAndMakeVisible (*lockTgl);
        addAndMakeVisible (scope);
        select (2);
        startTimerHz (30);
    }
    StereoPage::~StereoPage() { stopTimer(); forget(); }

    String StereoPage::id (const char* k) const { return "spc_" + String (targetKey[sel]) + "_" + k; }
    Component* StereoPage::knob (const char* k, const String& label, bool bip)
    {
        const String pid = id (k); const int idx = idOf (pid);
        if (idx < 0) return nullptr;
        auto kn = std::make_unique<doomsday::flat::Knob> (proc.apvts, pid, label, targetCol (sel), bip, 34);
        kn->slider.setTooltip (fge::tooltip (idx));
        auto* r = kn.get(); addAndMakeVisible (r); byId.insert ({ pid, r }); ctl.push_back (std::move (kn)); return r;
    }
    Component* StereoPage::toggle (const char* k, const String& text)
    {
        const String pid = id (k);
        auto t = std::make_unique<doomsday::flat::Toggle> (proc.apvts, pid, text, doomsday::flat::Toggle::Pill, targetCol (sel));
        t->setTooltip (fge::tooltip (idOf (pid)));
        auto* r = t.get(); addAndMakeVisible (r); byId.insert ({ pid, r }); ctl.push_back (std::move (t)); return r;
    }
    Component* StereoPage::combo (const char* k, const String& caption)
    {
        const String pid = id (k); const int idx = idOf (pid);
        if (idx < 0) return nullptr;
        auto cb = std::make_unique<ComboBox>();
        cb->addItemList (def (idx).choices(), 1);
        comboAtt[cb.get()] = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, pid, *cb);
        cb->setTooltip (fge::tooltip (idx));
        auto* r = cb.get(); addAndMakeVisible (r); byId.insert ({ pid, r }); captions[r] = caption; ctl.push_back (std::move (cb)); return r;
    }
    void StereoPage::forget()
    {
        for (auto& c : ctl) { comboAtt.erase (c.get()); for (auto it = byId.begin(); it != byId.end();) it = it->second == c.get() ? byId.erase (it) : std::next (it); }
        ctl.clear(); layoutMap.clear(); captions.clear(); titles.clear();
    }
    Component* StereoPage::findControl (const String& pid) { auto it = byId.find (pid); return it == byId.end() ? nullptr : it->second; }
    StringArray StereoPage::visibleIds() const { StringArray a; for (auto& [k, c] : byId) if (c->isVisible()) a.add (k); return a; }
    void StereoPage::setMonoAudition (bool on) { monoBtn.setToggleState (on, dontSendNotification); proc.monoAudition.store (on); }

    void StereoPage::select (int t)
    {
        sel = jlimit (0, 6, t);
        const auto& c = palette();
        for (int i = 0; i < 7; ++i)
        {
            tabBtn[(size_t) i].setColour (TextButton::buttonColourId, i == sel ? targetCol (i) : c.control);
            tabBtn[(size_t) i].setColour (TextButton::textColourOffId, i == sel ? c.ground : c.silver);
        }
        build();
        repaint();
    }

    void StereoPage::build()
    {
        forget();
        const int kh = doomsday::flat::Knob::heightFor (34), kw = 78;
        auto put = [&] (Component* comp, int x, int y, int w, int h) { if (comp != nullptr) layoutMap[comp] = { x, y, w, h }; };
        // 1. the unit: ON, image, width algorithm
        titles.push_back ({ { 0, 0, 760, 18 }, "1  UNIT  -  ON = processed, OFF = bypass" });
        put (toggle ("on", "ON"), 0, 24, 70, 22);
        put (toggle ("swap", "CHANNEL SWAP"), 84, 24, 140, 22);
        put (combo ("polarity", "POLARITY INVERT"), 250, 24, 120, 22);
        put (combo ("mode", "WIDTH ALGORITHM"), 390, 24, 150, 22);
        put (combo ("haas_channel", "HAAS CHANNEL"), 560, 24, 120, 22);
        int x = 0;
        for (auto& [k, t, b] : std::vector<std::tuple<const char*, const char*, bool>> { { "pan", "PAN", true }, { "balance", "BALANCE", true }, { "width", "WIDTH", false },
                                                                                         { "mix", "MIX", false }, { "gain", "OUTPUT", false }, { "delay", "CH DELAY", true },
                                                                                         { "haas_time", "HAAS TIME", false } })
        { put (knob (k, t, b), x, 56, kw, kh); x += kw + 8; }
        // 2. low frequency mono
        titles.push_back ({ { 0, 56 + kh + 12, 760, 18 }, "2  LOW FREQUENCY MONO" });
        const int y2 = 56 + kh + 36;
        put (toggle ("low_mono", "LOW MONO"), 0, y2 + 20, 110, 22);
        put (knob ("crossover", "CROSSOVER"), 130, y2, kw, kh);
        // 3. M/S engine
        titles.push_back ({ { 260, 56 + kh + 12, 500, 18 }, "3  MID / SIDE ENGINE" });
        put (toggle ("ms_on", "M/S ON"), 260, y2 + 20, 90, 22);
        put (knob ("mid", "MID"), 360, y2, kw, kh); put (knob ("side", "SIDE"), 446, y2, kw, kh); put (knob ("ms_width", "M/S WIDTH"), 532, y2, kw, kh);
        put (toggle ("mid_solo", "MID SOLO"), 626, y2 + 8, 110, 22); put (toggle ("side_solo", "SIDE SOLO"), 626, y2 + 38, 110, 22);
        resized();
    }

    String StereoPage::warning() const
    {
        auto v = [this] (const char* k) { auto* a = proc.apvts.getRawParameterValue (id (k)); return a != nullptr ? a->load() : 0.f; };
        if (v ("on") < 0.5f) return "OFF - this unit passes the sound untouched.";
        if ((int) v ("mode") == 2) return "HAAS is a creative effect: check MONO AUDITION - the mono sum can lose the sound.";
        if (sel == 3 && proc.apvts.getRawParameterValue ("spc_sub_mono_lock")->load() > 0.5f) return "SUB MONO LOCK is on: the SUB group always leaves this unit mono.";
        if (sel == 6 && proc.apvts.getRawParameterValue ("spc_sub_mono_lock")->load() > 0.5f) return "SUB MONO LOCK: the master keeps everything below its crossover (min. 120 Hz) mono.";
        return "SWAP > POLARITY > CH DELAY > LOW MONO > WIDTH (+ MIX) > M/S > PAN > BALANCE > OUTPUT";
    }

    void StereoPage::timerCallback()
    {
        if (! isShowing()) return;
        scope.repaint();
        static int n = 0; if (++n % 6 == 0) repaint (0, 0, scopeArea.getX(), getHeight());
        // controls that do nothing in the current setup are dimmed (they still remember their value)
        auto v = [this] (const char* k) { auto* a = proc.apvts.getRawParameterValue (id (k)); return a != nullptr ? a->load() : 0.f; };
        const int mode = (int) v ("mode");
        auto en = [this] (const char* k, bool e) { if (auto* c = findControl (id (k))) { c->setEnabled (e); c->setAlpha (e ? 1.f : 0.4f); } };
        en ("haas_time", mode == 2); en ("haas_channel", mode == 2);
        en ("crossover", v ("low_mono") > 0.5f || sel == 6);
        const bool ms = v ("ms_on") > 0.5f;
        for (auto* k : { "mid", "side", "ms_width", "mid_solo", "side_solo" }) en (k, ms);
    }

    void StereoPage::paint (Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.panel); g.fillRoundedRectangle (ctlArea.expanded (10, 10).toFloat(), 6.f);
        g.setFont (FontOptions (11.f, Font::bold));
        for (auto& [r, t] : titles) { g.setColour (c.textDim); g.drawText (t, r.translated (ctlArea.getX(), ctlArea.getY()), Justification::left); }
        g.setFont (FontOptions (10.f));
        for (auto& [comp, cap] : captions) { g.setColour (c.textDim); g.drawText (cap, comp->getX(), comp->getY() - 13, comp->getWidth() + 40, 12, Justification::left); }
        g.setFont (FontOptions (12.f));
        const String w = warning();
        g.setColour (w.startsWith ("HAAS") ? c.hot : c.silver);
        g.drawText (targetName[sel] + String ("  -  ") + w, ctlArea.getX(), ctlArea.getBottom() + 18, ctlArea.getWidth(), 16, Justification::left);
        // ALL UNITS at a glance (click a row = open it)
        auto ov = overviewArea();
        g.setColour (c.panel); g.fillRoundedRectangle (ov.toFloat(), 6.f);
        g.setColour (c.textDim); g.setFont (FontOptions (11.f, Font::bold));
        g.drawText ("ALL UNITS", ov.getX() + 10, ov.getY() + 4, 200, 14, Justification::left);
        const char* head[6] { "UNIT", "ON", "WIDTH", "ALGORITHM", "PAN", "LOW MONO" };
        const int cx[6] { 10, 130, 200, 290, 430, 510 };
        g.setFont (FontOptions (10.f));
        for (int k = 0; k < 6; ++k) g.drawText (head[k], ov.getX() + cx[k], ov.getY() + 22, 120, 14, Justification::left);
        auto raw = [this] (int t, const char* k) { auto* a = proc.apvts.getRawParameterValue ("spc_" + String (targetKey[t]) + "_" + k); return a != nullptr ? a->load() : 0.f; };
        g.setFont (FontOptions (12.f));
        for (int t = 0; t < 7; ++t)
        {
            const int y = ov.getY() + 40 + t * 22;
            const bool on = raw (t, "on") > 0.5f;
            if (t == sel) { g.setColour (c.control); g.fillRect (ov.getX() + 4, y - 2, ov.getWidth() - 8, 20); }
            g.setColour (targetCol (t)); g.fillRect (ov.getX() + 10, y + 3, 4, 12);
            g.setColour (on ? c.silver : c.textDim);
            g.drawText (targetName[t], ov.getX() + cx[0] + 10, y, 110, 16, Justification::left);
            g.drawText (on ? "ON" : "off", ov.getX() + cx[1], y, 60, 16, Justification::left);
            g.drawText (String (roundToInt (raw (t, "width") * 100.f)) + " %", ov.getX() + cx[2], y, 80, 16, Justification::left);
            g.drawText (def (idOf ("spc_punch_mode")).choices()[(int) raw (t, "mode")], ov.getX() + cx[3], y, 140, 16, Justification::left);
            const float pn = raw (t, "pan");
            g.drawText (std::abs (pn) < 0.005f ? String ("C") : String (roundToInt (std::abs (pn) * 100.f)) + (pn < 0 ? " L" : " R"), ov.getX() + cx[4], y, 70, 16, Justification::left);
            const bool lock = (t == 3 || t == 6) && proc.apvts.getRawParameterValue ("spc_sub_mono_lock")->load() > 0.5f;
            g.drawText (raw (t, "low_mono") > 0.5f ? String (roundToInt (raw (t, "crossover"))) + " Hz" + (lock ? "  (LOCK)" : "") : lock ? String ("LOCK") : String ("-"),
                        ov.getX() + cx[5], y, 200, 16, Justification::left);
        }
    }
    void StereoPage::mouseDown (const MouseEvent& e)
    {
        auto ov = overviewArea();
        if (! ov.contains (e.getPosition())) return;
        const int row = (e.y - ov.getY() - 38) / 22;
        if (isPositiveAndBelow (row, 7)) select (row);
    }

    void StereoPage::resized()
    {
        for (int i = 0; i < 7; ++i) tabBtn[(size_t) i].setBounds (i * 96, 0, 92, 26);
        lockTgl->setBounds (7 * 96 + 16, 2, 160, 22);
        monoBtn.setBounds (7 * 96 + 196, 0, 130, 26);
        ctlArea = { 10, 46, 760, 330 };
        for (auto& [comp, r] : layoutMap) comp->setBounds (r.translated (ctlArea.getX(), ctlArea.getY()));
        scopeArea = { 800, 40, getWidth() - 800, getHeight() - 40 };
        scope.setBounds (scopeArea);
    }
}
