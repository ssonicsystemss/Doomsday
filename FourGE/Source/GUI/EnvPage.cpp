#include "EnvPage.h"
#include "Palette.h"

using namespace juce;

namespace fge::gui
{
    namespace
    {
        const char* partKey[4]  { "punch", "body", "tail", "sub" };
        const char* partName[4] { "PUNCH", "BODY", "TAIL", "SUB" };
        const char* viewName[EnvPage::NumViews] { "PITCH", "AMPLITUDE", "FILTER", "DISTORTION", "RESONANCE" };
        const char* viewKey[EnvPage::NumViews]  { "penv", "amp_env", "filter_env", "drive_env", "reso_env" };
        int idOf (const String& id) { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; }
        String pitchModeId (int p) { return p == 2 ? String ("pe_mode") : String (partKey[p]) + "_penv_mode"; }
        constexpr int customMode = 8;
    }

    int EnvPage::slotOf (int p, int v)
    {
        switch (v)
        {
            case Pitch:  return env::PitchPunch + p;
            case Amp:    return env::AmpPunch + p;
            case Filter: return p < 3 ? env::FilterPunch + p : -1;
            case Drive:  return env::DrivePunch + p;
            case Reso:   return p == 1 ? (int) env::ResoBody : p == 2 ? (int) env::ResoTail : -1;
            default:     return -1;
        }
    }
    int EnvPage::slot() const { return slotOf (part, view); }
    String EnvPage::paramPrefix() const { return String (partKey[part]) + "_" + viewKey[view]; }
    float EnvPage::rawParam (const String& id) const { auto* v = proc.apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.f; }
    void EnvPage::setParam (const String& id, float v) { proc.setParamInUndoStep (id, v); }

    EnvPage::EnvPage (GeProcessor& p, doomsday::flat::FlatLookAndFeel& l) : proc (p), lnf (l)
    {
        cooker.prepare (44100.0, 256);
        addAndMakeVisible (ed);
        ed.onCommit = [this] (const env::Shape& s) { commitShape (s); };
        for (int i = 0; i < 4; ++i)
        {
            auto& b = partBtn[(size_t) i];
            b.setButtonText (partName[i]); b.setClickingTogglesState (false);
            b.onClick = [this, i] { select (i, view); };
            addAndMakeVisible (b);
        }
        for (int v = 0; v < NumViews; ++v)
        {
            auto& b = viewBtn[(size_t) v];
            b.setButtonText (viewName[v]);
            b.onClick = [this, v] { select (part, v); };
            addAndMakeVisible (b);
        }
        copyBtn.onClick  = [this] { copyShape(); };
        pasteBtn.onClick = [this] { pasteShape(); };
        resetBtn.onClick = [this] { resetShape(); };
        fitBtn.onClick   = [this] { ed.fit(); };
        snapBtn.setClickingTogglesState (true); snapBtn.setToggleState (true, dontSendNotification);
        snapBtn.onClick  = [this] { ed.setSnap (snapBtn.getToggleState()); };
        saveBtn.onClick  = [this] { saveShapeFile(); };
        loadBtn.onClick  = [this] { loadShapeFile(); };
        copyBtn.setTooltip ("Copy this envelope (points + curves) to the clipboard.");
        pasteBtn.setTooltip ("Paste a copied envelope here (also from another part or view).");
        resetBtn.setTooltip ("Back to the default curve.");
        fitBtn.setTooltip ("Zoom out: the whole envelope in view.");
        snapBtn.setTooltip ("Snap points to the grid (off = free).");
        saveBtn.setTooltip ("Save this envelope as its own file (Documents / 4GE / Envelopes).");
        loadBtn.setTooltip ("Load an envelope file into this envelope.");
        for (auto* b : { &copyBtn, &pasteBtn, &resetBtn, &fitBtn, &snapBtn, &saveBtn, &loadBtn }) addAndMakeVisible (b);

        // global envelope time controls (always visible)
        const auto& c = palette();
        auto addG = [this] (std::unique_ptr<Component> comp, const char* id)
        {
            auto* r = comp.get(); addAndMakeVisible (r); byId.insert ({ id, r }); globalCtl.push_back (std::move (comp));
        };
        addG (std::make_unique<doomsday::flat::Knob> (proc.apvts, "env_time_scale", "TIME SCALE", c.silver, false, 34), "env_time_scale");
        addG (std::make_unique<doomsday::flat::Knob> (proc.apvts, "env_time_offset", "TIME OFFSET", c.silver, false, 34), "env_time_offset");
        addG (std::make_unique<doomsday::flat::Toggle> (proc.apvts, "env_sync", "SYNC", doomsday::flat::Toggle::Pill, c.red), "env_sync");
        addG (std::make_unique<doomsday::flat::Knob> (proc.apvts, "env_local_bpm", "LOCAL BPM", c.silver, false, 34), "env_local_bpm");
        for (auto& [id, comp] : byId)
        {
            const String tip = fge::tooltip (idOf (id));
            if (auto* k = dynamic_cast<doomsday::flat::Knob*> (comp)) k->slider.setTooltip (tip);
            else if (auto* t = dynamic_cast<SettableTooltipClient*> (comp)) t->setTooltip (tip);
        }
        select (2, Pitch);
        startTimerHz (15);
    }

    EnvPage::~EnvPage() { stopTimer(); }

    Component* EnvPage::findControl (const String& id) { auto it = byId.find (id); return it == byId.end() ? nullptr : it->second; }

    void EnvPage::select (int p, int v)
    {
        part = jlimit (0, 3, p); view = jlimit (0, NumViews - 1, v);
        if (slotOf (part, view) < 0) view = Pitch;                           // FILTER has no SUB, RESONANCE only BODY / TAIL
        const auto& c = palette();
        for (int i = 0; i < 4; ++i)
        {
            partBtn[(size_t) i].setColour (TextButton::buttonColourId, i == part ? c.part[(size_t) i].withAlpha (0.85f) : c.control);
            partBtn[(size_t) i].setColour (TextButton::textColourOffId, i == part ? c.ground : c.silver);
        }
        for (int k = 0; k < NumViews; ++k)
        {
            const bool ok = slotOf (part, k) >= 0;
            viewBtn[(size_t) k].setEnabled (ok);
            viewBtn[(size_t) k].setColour (TextButton::buttonColourId, k == view ? c.silver.withAlpha (0.85f) : c.control);
            viewBtn[(size_t) k].setColour (TextButton::textColourOffId, k == view ? c.ground : (ok ? c.silver : c.textDim));
        }
        ed.setColour (c.part[(size_t) part]);
        ed.fit();
        rebuildControls();
        lastVersion = 0;
        lastFrame.fill (std::numeric_limits<float>::quiet_NaN());
        refreshEditor();
        repaint();
    }

    // the real parameters of the selected envelope (every one is wired to the engine; the every-parameter test covers them)
    void EnvPage::rebuildControls()
    {
        for (auto& comp : slotCtl) for (auto it = byId.begin(); it != byId.end();) it = it->second == comp.get() ? byId.erase (it) : std::next (it);
        comboAtt.clear(); slotCtl.clear();
        const auto col = palette().part[(size_t) part];
        const String pk (partKey[part]);
        auto knob = [&] (const String& id, const String& label, bool bip = false)
        {
            if (idOf (id) < 0) return;
            auto k = std::make_unique<doomsday::flat::Knob> (proc.apvts, id, label, col, bip, 38);
            k->slider.setTooltip (fge::tooltip (idOf (id)));
            k->getProperties().set ("row", 1);
            byId.insert ({ id, k.get() }); addAndMakeVisible (*k); slotCtl.push_back (std::move (k));
        };
        auto toggle = [&] (const String& id, const String& text)
        {
            auto t = std::make_unique<doomsday::flat::Toggle> (proc.apvts, id, text, doomsday::flat::Toggle::Pill, col);
            t->setTooltip (fge::tooltip (idOf (id)));
            t->getProperties().set ("row", 0);
            byId.insert ({ id, t.get() }); addAndMakeVisible (*t); slotCtl.push_back (std::move (t));
        };
        auto combo = [&] (const String& id, const String& caption)
        {
            const int idx = idOf (id);
            if (idx < 0) return;
            auto cb = std::make_unique<ComboBox>();
            cb->addItemList (def (idx).choices(), 1);
            comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *cb));
            cb->setTooltip (fge::tooltip (idx));
            cb->getProperties().set ("row", 0);
            cb->getProperties().set ("caption", caption);
            byId.insert ({ id, cb.get() }); addAndMakeVisible (*cb); slotCtl.push_back (std::move (cb));
        };
        auto seg = [&] (const String& id, StringArray labels)
        {
            auto s = std::make_unique<doomsday::flat::Segmented> (proc.apvts, id, labels, col);
            s->setTooltip (fge::tooltip (idOf (id)));
            s->getProperties().set ("row", 0);
            byId.insert ({ id, s.get() }); addAndMakeVisible (*s); slotCtl.push_back (std::move (s));
        };

        if (view == Pitch)
        {
            combo (pitchModeId (part), "PITCH MODE");
            toggle (pk + "_penv_on", "PITCH ENV ON");
            combo (pk + "_link", "LINK");
            combo (pk + "_link_source", "FOLLOWS");
            if (part == 2)      { knob ("pe_start", "START"); knob ("pe_end", "END"); knob ("pe_amount", "AMOUNT +", true); knob ("pe_time", "TIME"); knob ("pe_curve", "CURVE"); }
            else if (part == 3) { knob ("sub_penv_depth", "DEPTH", true); knob ("sub_penv_time", "TIME"); knob ("sub_penv_curve", "CURVE"); }
            else                { knob (pk + "_pitch", "START"); knob (pk + "_glide", "TIME"); knob (pk + "_penv_curve", "CURVE"); }
            knob (pk + "_penv_amount", "DEPTH %");
            knob (pk + "_penv_scale", "TIME SCALE");
            knob (pk + "_penv_offset", part == 2 ? "HOLD" : "OFFSET");
            if (part == 2) knob ("tail_hold_level", "TOK LEVEL", true);
            knob (pk + "_follow_depth", "FOLLOW DEPTH");
            knob (pk + "_follow_offset", "FOLLOW +ST", true);
            knob (pk + "_follow_time", "FOLLOW TIME");
        }
        else if (view == Amp)
        {
            const String e = pk + "_amp_env";
            seg (e + "_mode", { "ADSR", "DRAWN" });
            combo (e + "_trigger", "TRIGGER");
            toggle (e + "_sync", "SYNC");
            for (auto [s, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "_attack", "ATTACK" }, { "_decay", "DECAY" }, { "_sustain", "SUSTAIN" },
                                                                                             { "_release", "RELEASE" }, { "_length", "LENGTH" } })
                knob (pk + s, lab);
            knob (e + "_delay", "DELAY"); knob (e + "_curve", "CURVE"); knob (e + "_amount", "AMOUNT"); knob (pk + "_level", "LEVEL");
        }
        else
        {
            const String e = paramPrefix();
            toggle (e + "_on", String (viewName[view]) + " ENV ON");
            seg (e + "_mode", { "ADSR", "DRAWN" });
            combo (e + "_polarity", "POLARITY");
            combo (e + "_trigger", "TRIGGER");
            toggle (e + "_sync", "SYNC");
            knob (e + "_attack", "ATTACK"); knob (e + "_decay", "DECAY"); knob (e + "_sustain", "SUSTAIN"); knob (e + "_release", "RELEASE");
            knob (e + "_delay", "DELAY"); knob (e + "_curve", "CURVE"); knob (e + "_amount", "AMOUNT", true);
            knob (pk + (view == Filter ? "_tone" : view == Drive ? "_drive" : "_resonance"), view == Filter ? "TONE (BASE)" : view == Drive ? "DRIVE (BASE)" : "RESO (BASE)");
        }
        resized();
    }

    bool EnvPage::slotActive (String& why) const
    {
        const String pk (partKey[part]);
        if (view == Pitch)
        {
            if (rawParam (pk + "_penv_on") < 0.5f) { why = "PITCH ENV is OFF - drawing turns it on and switches to CUSTOM"; return false; }
            const int m = roundToInt (rawParam (pitchModeId (part)));
            if (m != customMode) { why = "Pitch mode is " + def (idOf (pitchModeId (part))).choices()[m] + " - drawing switches it to CUSTOM"; return false; }
            return true;
        }
        const String e = paramPrefix();
        if (view != Amp && rawParam (e + "_on") < 0.5f) { why = String (viewName[view]) + " ENV is OFF - drawing turns it on (DRAWN)"; return false; }
        if (roundToInt (rawParam (e + "_mode")) != 1) { why = "ADSR mode - drawing switches this envelope to DRAWN"; return false; }
        return true;
    }

    void EnvPage::commitShape (const env::Shape& s)
    {
        const int sl = slot();
        if (sl < 0) return;
        proc.setShape (sl, s);                                   // opens the undo step; the switches below join it
        const String pk (partKey[part]);
        if (view == Pitch)
        {
            if (rawParam (pk + "_penv_on") < 0.5f) setParam (pk + "_penv_on", 1.f);
            if (roundToInt (rawParam (pitchModeId (part))) != customMode) setParam (pitchModeId (part), (float) customMode);
        }
        else
        {
            const String e = paramPrefix();
            if (view != Amp && rawParam (e + "_on") < 0.5f) setParam (e + "_on", 1.f);
            if (roundToInt (rawParam (e + "_mode")) != 1) setParam (e + "_mode", 1.f);
        }
        lastVersion = 0;
        refreshEditor();
    }

    void EnvPage::copyShape()
    {
        if (slot() >= 0) SystemClipboard::copyTextToClipboard ("4GE-ENV " + proc.shape (slot()).toString());
    }
    bool EnvPage::pasteShape()
    {
        auto t = SystemClipboard::getTextFromClipboard().trim();
        if (! t.startsWith ("4GE-ENV ") || slot() < 0) return false;
        env::Shape s;
        if (! env::Shape::fromString (t.fromFirstOccurrenceOf (" ", false, false), s, GeProcessor::slotMin (slot()), 1.f)) return false;
        commitShape (s);
        return true;
    }
    void EnvPage::resetShape() { if (slot() >= 0) { proc.setShape (slot(), env::defaultShape (slot())); refreshEditor(); } }

    void EnvPage::saveShapeFile()
    {
        if (slot() < 0) return;
        auto dir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("4GE").getChildFile ("Envelopes");
        dir.createDirectory();
        const String name = String (partName[part]) + " " + viewName[view];
        chooser = std::make_unique<FileChooser> ("Save envelope", dir.getChildFile (name + ".4genv"), "*.4genv");
        const String text = "4GE-ENV " + proc.shape (slot()).toString();
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [text] (const FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f != File()) f.withFileExtension ("4genv").replaceWithText (text);
        });
    }
    void EnvPage::loadShapeFile()
    {
        if (slot() < 0) return;
        auto dir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("4GE").getChildFile ("Envelopes");
        chooser = std::make_unique<FileChooser> ("Load envelope", dir, "*.4genv");
        SafePointer<EnvPage> self (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [self] (const FileChooser& fc)
        {
            auto f = fc.getResult();
            if (self == nullptr || ! f.existsAsFile()) return;
            const auto t = f.loadFileAsString().trim();
            env::Shape s;
            if (t.startsWith ("4GE-ENV ") && env::Shape::fromString (t.fromFirstOccurrenceOf (" ", false, false), s, GeProcessor::slotMin (self->slot()), 1.f))
                self->commitShape (s);
        });
    }

    // the editor follows the processor (undo, presets, another view) and the axes follow the parameters
    void EnvPage::refreshEditor()
    {
        const int sl = slot();
        if (sl < 0) return;
        auto f = proc.frame();
        bool changed = false;
        for (int i = 0; i < P::count; ++i) if (! (f[i] == lastFrame[(size_t) i])) { changed = true; lastFrame[(size_t) i] = f[i]; }
        if (changed)
        {
            cooker.setBank (&proc.shapes());
            cooker.cookOnly (f);
            const auto& cfg = cooker.config();
            EnvEditor::Axis a;
            if (view == Pitch)
            {
                const auto& pp = cfg.pitch[(size_t) part];
                const float depth = pp.depthSt;
                a.spanMs = jmax (1.f, pp.timeMs); a.offsetMs = pp.offsetMs;
                if (std::abs (depth) > 0.01f)
                {
                    a.label = [depth] (float v) { const float st = v * depth; return (st > 0.05f ? "+" : "") + (std::abs (st) < 10.f ? String (st, 1) : String (roundToInt (st))) + " st"; };
                    a.toUser = [depth] (float v) { return v * depth; };
                    a.fromUser = [depth] (float x) { return x / depth; };
                    a.unit = "st";
                }
                else
                {
                    a.label = [] (float v) { return String (roundToInt (v * 100.f)) + " %"; };
                    a.toUser = [] (float v) { return v * 100.f; }; a.fromUser = [] (float x) { return x / 100.f; }; a.unit = "%";
                }
            }
            else
            {
                const dsp::EnvSlot& s = view == Amp ? cfg.amp[(size_t) part] : view == Filter ? cfg.filter[(size_t) part]
                                      : view == Drive ? cfg.drive[(size_t) part] : cfg.reso[(size_t) part];
                a.spanMs = jmax (1.f, s.adsr.lengthMs); a.offsetMs = s.delayMs;
                a.label = [] (float v) { return String (roundToInt (v * 100.f)) + " %"; };
                a.toUser = [] (float v) { return v * 100.f; }; a.fromUser = [] (float x) { return x / 100.f; }; a.unit = "%";
            }
            ed.setAxis (a);
        }
        if (proc.shapesVersion() != lastVersion && ! ed.isDragging())
        {
            lastVersion = proc.shapesVersion();
            ed.setShape (proc.shape (sl), GeProcessor::slotMin (sl), 1.f);
        }
        String why;
        const bool on = slotActive (why);
        ed.setActive (on, why);
    }

    void EnvPage::timerCallback() { if (isShowing()) { refreshEditor(); repaint (top); } }

    void EnvPage::resized()
    {
        const int w = getWidth(), h = getHeight();
        left = { 0, 0, 170, h };
        top = { 182, 0, w - 182, 34 };
        edArea = { 182, 38, w - 182, h - 38 - 160 };
        bottom = { 182, edArea.getBottom() + 8, w - 182, h - edArea.getBottom() - 8 };
        for (int i = 0; i < 4; ++i) partBtn[(size_t) i].setBounds (left.getX() + 12, 28 + i * 28, left.getWidth() - 24, 24);
        for (int v = 0; v < NumViews; ++v) viewBtn[(size_t) v].setBounds (left.getX() + 12, 166 + v * 28, left.getWidth() - 24, 24);
        if (globalCtl.size() == 4)
        {
            const int gy = 332;
            globalCtl[0]->setBounds (left.getX() + 8, gy, 76, doomsday::flat::Knob::heightFor (34));
            globalCtl[1]->setBounds (left.getX() + 86, gy, 76, doomsday::flat::Knob::heightFor (34));
            globalCtl[2]->setBounds (left.getX() + 12, gy + 98, 70, 22);
            globalCtl[3]->setBounds (left.getX() + 86, gy + 74, 76, doomsday::flat::Knob::heightFor (34));
        }
        int bx = top.getRight();
        for (auto* b : { &loadBtn, &saveBtn, &snapBtn, &fitBtn, &resetBtn, &pasteBtn, &copyBtn })
        {
            bx -= 66; b->setBounds (bx, top.getY() + 4, 62, 24);
        }
        ed.setBounds (edArea);
        // row 0: switches and choices (captions painted above), row 1: knobs
        int x0 = bottom.getX() + 8, x1 = bottom.getX() + 8;
        for (auto& comp : slotCtl)
        {
            const int row = comp->getProperties()["row"];
            if (row == 0)
            {
                const bool isSeg = dynamic_cast<doomsday::flat::Segmented*> (comp.get()) != nullptr;
                const bool isCombo = dynamic_cast<ComboBox*> (comp.get()) != nullptr;
                const int cw = isSeg ? 150 : isCombo ? 140 : 130;
                comp->setBounds (x0, bottom.getY() + 18, cw, 24);
                x0 += cw + 12;
            }
            else
            {
                comp->setBounds (x1, bottom.getY() + 52, 76, doomsday::flat::Knob::heightFor (38));
                x1 += 78;
            }
        }
    }

    void EnvPage::paint (Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.panel); g.fillRoundedRectangle (left.toFloat(), 6.f);
        g.fillRoundedRectangle (top.withBottom (getHeight()).toFloat(), 6.f);
        g.setFont (lnf.font (11.f, doomsday::flat::Bold, 0.1f));
        g.setColour (c.textDim);
        g.drawText ("PART", left.getX() + 14, 8, 100, 18, Justification::left);
        g.drawText ("VIEW", left.getX() + 14, 144, 100, 18, Justification::left);
        g.drawText ("ALL ENVELOPES", left.getX() + 14, 312, 150, 18, Justification::left);
        // which envelope is being edited
        const auto col = c.part[(size_t) part];
        g.setColour (col); g.fillRect (top.getX() + 12, top.getY() + 11, 3, 12);
        String what = String ("EDITING:  ") + partName[part] + "  -  " + viewName[view];
        String why;
        const bool on = slotActive (why);
        if (view == Pitch) what << "  (" << def (idOf (pitchModeId (part))).choices()[roundToInt (rawParam (pitchModeId (part)))] << ")";
        else what << (on ? "  (DRAWN)" : "  (ADSR / OFF)");
        g.setColour (c.silver); g.setFont (lnf.font (12.f, doomsday::flat::Bold, 0.08f));
        g.drawText (what, top.getX() + 22, top.getY() + 6, 560, 22, Justification::left);
        // captions over the choice boxes
        g.setFont (lnf.font (9.5f, doomsday::flat::Bold, 0.06f)); g.setColour (c.textDim);
        for (auto& comp : slotCtl)
            if (auto cap = comp->getProperties()["caption"].toString(); cap.isNotEmpty())
                g.drawText (cap, comp->getX(), comp->getY() - 15, comp->getWidth(), 13, Justification::left);
        g.drawText ("double-click = add / delete point   drag = move   shift = select   ALT-drag = tension   right-click = curve / value   wheel = zoom",
                    bottom.getX() + 8, getHeight() - 14, bottom.getWidth() - 16, 12, Justification::left);
    }
}
