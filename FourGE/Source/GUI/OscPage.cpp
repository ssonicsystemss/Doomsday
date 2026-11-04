#include "OscPage.h"
#include "Palette.h"
#include "../Presets/OscFiles.h"

using namespace juce;

namespace fge::gui
{
    namespace
    {
        int idOf (const String& id) { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; }
        float rawOf (GeProcessor& p, const String& id) { auto* v = p.apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.f; }
        Colour oscCol (int i) { static const Colour c[4] { Colour (0xffe33135), Colour (0xffe6a23c), Colour (0xff9b70d5), Colour (0xff36a9d6) }; return c[jlimit (0, 3, i)]; }
    }

    // ===================================================================== HARMONIC editor
    int HarmonicEditor::barAt (float x) const { return jlimit (0, dsp::numHarmonics - 1, (int) ((x - 4.f) / jmax (1.f, (float) getWidth() - 8.f) * (float) dsp::numHarmonics)); }
    void HarmonicEditor::setFrom (Point<float> p)
    {
        const int h = barAt (p.x);
        data.hAmp[(size_t) h] = jlimit (0.f, 1.f, 1.f - (p.y - 4.f) / jmax (1.f, (float) getHeight() - 8.f));
        repaint();
    }
    void HarmonicEditor::mouseDown (const MouseEvent& e)
    {
        if (e.mods.isPopupMenu()) { auto& on = data.hOn[(size_t) barAt (e.position.x)]; on = ! on; repaint(); if (onCommit) onCommit (data); return; }
        dragging = true; setFrom (e.position);
    }
    void HarmonicEditor::mouseDrag (const MouseEvent& e) { if (dragging) setFrom (e.position); }
    void HarmonicEditor::mouseUp (const MouseEvent&) { if (dragging) { dragging = false; if (onCommit) onCommit (data); } }
    void HarmonicEditor::paint (Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.ground); g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.f);
        const float w = ((float) getWidth() - 8.f) / (float) dsp::numHarmonics, hgt = (float) getHeight() - 8.f;
        for (int h = 0; h < dsp::numHarmonics; ++h)
        {
            const float a = data.hAmp[(size_t) h], x = 4.f + (float) h * w;
            const bool on = data.hOn[(size_t) h];
            g.setColour ((on ? (h % 2 == 0 ? c.red : c.hot.brighter (0.2f)) : c.textDim).withAlpha (on ? 0.9f : 0.35f));
            g.fillRect (x + 1.f, 4.f + hgt * (1.f - a), jmax (1.f, w - 2.f), hgt * a);
            if (! on) { g.setColour (c.textDim); g.drawLine (x + 1.f, 4.f + hgt, x + w - 1.f, 4.f + hgt - 6.f, 1.f); }
        }
        g.setColour (c.textDim); g.setFont (FontOptions (9.5f));
        g.drawText ("drag = amplitude of each harmonic (1 .. 64), right-click = harmonic on / off", getLocalBounds().reduced (6, 4), Justification::topRight);
    }

    // ===================================================================== page
    OscPage::OscPage (GeProcessor& p, doomsday::flat::FlatLookAndFeel& l) : proc (p), lnf (l)
    {
        const auto& c = palette();
        auto add = [this] (std::unique_ptr<Component> comp, const String& id) -> Component*
        {
            auto* r = comp.get(); addAndMakeVisible (r); if (id.isNotEmpty()) byId.insert ({ id, r }); owned.push_back (std::move (comp)); return r;
        };
        for (int i = 0; i < 4; ++i)
        {
            const String o = "osc" + String (i + 1) + "_";
            const auto col = oscCol (i);
            auto knob = [&] (const String& s, const String& lab, bool bip = false)
            { auto k = std::make_unique<doomsday::flat::Knob> (proc.apvts, o + s, lab, col, bip, 26); k->slider.setTooltip (fge::tooltip (idOf (o + s))); return add (std::move (k), o + s); };
            auto tog = [&] (const String& s, const String& lab, doomsday::flat::Toggle::Style st)
            { auto t = std::make_unique<doomsday::flat::Toggle> (proc.apvts, o + s, lab, st, col); t->setTooltip (fge::tooltip (idOf (o + s))); return add (std::move (t), o + s); };
            auto combo = [&] (const String& s)
            {
                auto cb = std::make_unique<ComboBox>();
                cb->addItemList (def (idOf (o + s)).choices(), 1);
                comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, o + s, *cb));
                cb->setTooltip (fge::tooltip (idOf (o + s)));
                return add (std::move (cb), o + s);
            };
            auto& st = strip[(size_t) i];
            st = { tog ("on", "ON", doomsday::flat::Toggle::Pill), tog ("mute", "MUTE", doomsday::flat::Toggle::Chip), tog ("solo", "SOLO", doomsday::flat::Toggle::Chip),
                   combo ("wave"), combo ("track"),
                   knob ("level", "LEVEL"), knob ("octave", "OCT", true), knob ("semitone", "SEMI", true), knob ("fine", "FINE", true), knob ("phase", "PHASE"), knob ("unison", "UNISON"),
                   knob ("to_punch", "> PUNCH"), knob ("to_body", "> BODY"), knob ("to_tail", "> TAIL"), knob ("to_sub", "> SUB"), knob ("pan", "PAN", true) };
            resetBtn[(size_t) i].setButtonText ("RESET");
            resetBtn[(size_t) i].setTooltip ("Every setting of this oscillator back to its default (it stays ON).");
            resetBtn[(size_t) i].onClick = [this, i] { files::resetOsc (proc, i); };
            presetBtn[(size_t) i].setButtonText ("PRESET");
            presetBtn[(size_t) i].setTooltip ("Oscillator presets by category, save / load this oscillator.");
            presetBtn[(size_t) i].onClick = [this, i] { showPresetMenu (i); };
            addAndMakeVisible (resetBtn[(size_t) i]); addAndMakeVisible (presetBtn[(size_t) i]);
        }
        for (int t = 0; t < NumOscTabs; ++t)
        {
            tabBtn[(size_t) t].setButtonText (oscTabName (t));
            tabBtn[(size_t) t].onClick = [this, t] { selectTab (t); };
            addAndMakeVisible (tabBtn[(size_t) t]);
        }
        tabSave.onClick = [this] { saveFile (0); }; tabLoad.onClick = [this] { loadFile (0); };
        oscSave.onClick = [this] { saveFile (1); }; oscLoad.onClick = [this] { loadFile (1); };
        setSave.onClick = [this] { saveFile (2); }; setLoad.onClick = [this] { loadFile (2); };
        tabSave.setTooltip ("Save only this tab's settings (e.g. the FM configuration, the unison, the harmonic profile, the wavetable).");
        tabLoad.setTooltip ("Load a tab file into the selected oscillator.");
        oscSave.setTooltip ("Save the selected oscillator (all settings + its drawn / imported data).");
        oscLoad.setTooltip ("Load an oscillator file into the selected oscillator.");
        setSave.setTooltip ("Save all four oscillators + the FM matrix.");
        setLoad.setTooltip ("Load a set of four oscillators.");
        for (auto* b : { &tabSave, &tabLoad, &oscSave, &oscLoad, &setSave, &setLoad }) addAndMakeVisible (b);
        viewSum.setClickingTogglesState (true); viewPost.setClickingTogglesState (true); viewPost.setToggleState (true, dontSendNotification);
        viewSum.setTooltip ("Scope: the selected oscillator / the sum of the audible oscillators.");
        viewPost.setTooltip ("Scope: before / after FM, ring modulation and sync.");
        addAndMakeVisible (viewSum); addAndMakeVisible (viewPost);
        addAndMakeVisible (scopeView);
        cycleEditor.onCommit = [this] (const env::Shape& s) { proc.setOscCustom (sel, s); };
        harmEditor.onCommit = [this] (const dsp::OscData::One& d)
        {
            proc.undo.beginNewTransaction ("Harmonics");
            proc.setOscProperty (sel, "hAmp", dsp::oscstate::harmonicsText (d, 0), true, false);
            proc.setOscProperty (sel, "hOn", dsp::oscstate::harmonicsText (d, 2), true, false);
        };
        addChildComponent (cycleEditor); addChildComponent (harmEditor);
        {
            EnvEditor::Axis ax;                                                   // the cycle: 0 .. 360 degrees, -100 .. +100 %
            ax.spanMs = 360.f; ax.timeUnit = String (CharPointer_UTF8 ("\xc2\xb0"));
            ax.label = [] (float v) { return String (roundToInt (v * 100.f)) + " %"; };
            ax.toUser = [] (float v) { return v * 100.f; }; ax.fromUser = [] (float x) { return x / 100.f; }; ax.unit = "%";
            cycleEditor.setAxis (ax);
        }
        cycleLen.addItemList ({ "256", "512", "1024", "2048", "4096", "WHOLE FILE" }, 1);
        cycleLen.setSelectedId (4, dontSendNotification);
        cycleLen.setTooltip ("WAV import: how many samples one wavetable frame has (2048 = the usual wavetable format).");
        addChildComponent (cycleLen);
        ignoreUnused (c);
        selectOsc (0);
        startTimerHz (15);
    }

    OscPage::~OscPage() { stopTimer(); }

    Component* OscPage::findControl (const String& id) { auto it = byId.find (id); return it == byId.end() ? nullptr : it->second; }

    void OscPage::selectOsc (int i) { sel = jlimit (0, 3, i); lastData = 0; lastWave = -1; rebuildTab(); repaint(); }
    void OscPage::selectTab (int t) { tab = jlimit (0, NumOscTabs - 1, t); lastData = 0; lastWave = -1; rebuildTab(); repaint(); }

    void OscPage::mouseDown (const MouseEvent& e)
    {
        for (int i = 0; i < 4; ++i) if (stripArea[(size_t) i].contains (e.getPosition())) { selectOsc (i); return; }
    }

    // the controls of the selected tab (rebuilt when the oscillator or the tab changes)
    void OscPage::rebuildTab()
    {
        for (auto& comp : tabCtl) for (auto it = byId.begin(); it != byId.end();) it = it->second == comp.get() ? byId.erase (it) : std::next (it);
        tabComboAtt.clear(); tabCtl.clear();
        const auto col = oscCol (sel);
        int rowA = 0, rowB = 0;
        auto place = [&] (Component* comp, bool isKnob, int w)
        {
            comp->getProperties().set ("row", isKnob ? 1 : 0);
            comp->getProperties().set ("slot", isKnob ? rowB++ : rowA);
            comp->getProperties().set ("w", w);
            if (! isKnob) rowA += w + 10;
        };
        auto knob = [&] (const String& id, const String& lab, bool bip = false)
        {
            if (idOf (id) < 0) return;
            auto k = std::make_unique<doomsday::flat::Knob> (proc.apvts, id, lab, col, bip, 34);
            k->slider.setTooltip (fge::tooltip (idOf (id)));
            place (k.get(), true, 76); byId.insert ({ id, k.get() }); addAndMakeVisible (*k); tabCtl.push_back (std::move (k));
        };
        auto combo = [&] (const String& id, const String& caption)
        {
            const int idx = idOf (id);
            if (idx < 0) return;
            auto cb = std::make_unique<ComboBox>();
            cb->addItemList (def (idx).choices(), 1);
            tabComboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *cb));
            cb->setTooltip (fge::tooltip (idx));
            cb->getProperties().set ("caption", caption);
            place (cb.get(), false, 150); byId.insert ({ id, cb.get() }); addAndMakeVisible (*cb); tabCtl.push_back (std::move (cb));
        };
        auto toggle = [&] (const String& id, const String& text)
        {
            auto t = std::make_unique<doomsday::flat::Toggle> (proc.apvts, id, text, doomsday::flat::Toggle::Pill, col);
            t->setTooltip (fge::tooltip (idOf (id)));
            place (t.get(), false, 110); byId.insert ({ id, t.get() }); addAndMakeVisible (*t); tabCtl.push_back (std::move (t));
        };
        auto button = [&] (const String& text, const String& tip, std::function<void()> fn)
        {
            auto b = std::make_unique<TextButton> (text);
            b->setTooltip (tip); b->onClick = std::move (fn);
            b->getProperties().set ("row", 2); b->getProperties().set ("slot", (int) b->getProperties().size());
            addAndMakeVisible (*b); tabCtl.push_back (std::move (b));
        };
        cycleEditor.setVisible (false); harmEditor.setVisible (false); cycleLen.setVisible (false);
        switch (tab)
        {
            case TabWave:
                combo (sid ("noise"), "NOISE TYPE");
                knob (sid ("pw"), "PULSE W"); knob (sid ("freq"), "FIXED Hz"); knob (sid ("ratio"), "RATIO"); knob (sid ("ratio_fine"), "RATIO FINE", true);
                button ("IMPORT WAV", "Take one cycle of a WAV as the CUSTOM waveform (a cycle source, not a kick sample).", [this]
                {
                    chooser = std::make_unique<FileChooser> ("Import a cycle", File::getSpecialLocation (File::userHomeDirectory), "*.wav;*.aif;*.aiff;*.flac");
                    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
                    { if (fc.getResult().existsAsFile()) { proc.undo.beginNewTransaction ("Import cycle"); files::importCycle (proc, sel, fc.getResult()); } });
                });
                button ("CLEAR IMPORT", "Back to the drawn cycle.", [this] { proc.setOscProperty (sel, "cycle", {}); });
                button ("INVERT", "Flip the drawn cycle upside down (phase invert of the waveform).", [this]
                {
                    auto s = proc.oscData().osc[(size_t) sel].custom;
                    for (int k = 0; k < s.n; ++k) s.p[(size_t) k].v = -s.p[(size_t) k].v;
                    proc.setOscCustom (sel, s);
                });
                button ("NORMALIZE", "Scale the drawn cycle so its loudest point is at full level.", [this]
                {
                    auto s = proc.oscData().osc[(size_t) sel].custom;
                    float pk = 0.f; for (int k = 0; k < s.n; ++k) pk = jmax (pk, std::abs (s.p[(size_t) k].v));
                    if (pk > 1.0e-4f) { for (int k = 0; k < s.n; ++k) s.p[(size_t) k].v /= pk; proc.setOscCustom (sel, s); }
                });
                button ("SAVE CYCLE", "Save the drawn cycle (Documents / 4GE / Waveforms).", [this]
                {
                    const auto shape = proc.oscData().osc[(size_t) sel].custom;
                    chooser = std::make_unique<FileChooser> ("Save waveform", files::folder ("Waveforms").getChildFile ("My wave.4gwave"), "*.4gwave");
                    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [shape] (const FileChooser& fc)
                    { if (fc.getResult() != File()) files::writeShape (fc.getResult().withFileExtension ("4gwave"), shape); });
                });
                button ("LOAD CYCLE", "Load a saved waveform into CUSTOM.", [this]
                {
                    chooser = std::make_unique<FileChooser> ("Load waveform", files::folder ("Waveforms"), "*.4gwave");
                    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
                    { env::Shape s; if (files::readShape (fc.getResult(), s, -1.f)) { proc.setOscCustom (sel, s); proc.setParamInUndoStep (sid ("wave"), (float) dsp::WCustom); } });
                });
                cycleEditor.setVisible (true);
                break;
            case TabTable:
                combo (sid ("wt_table"), "WAVETABLE"); combo (sid ("wt_mode"), "SCAN MODE");
                knob (sid ("wt_position"), "POSITION"); knob (sid ("wt_morph"), "MORPH"); knob (sid ("wt_start"), "START"); knob (sid ("wt_end"), "END");
                knob (sid ("wt_scan"), "SCAN TIME"); knob (sid ("seed"), "SEED");
                button ("IMPORT WAV", "Cut a WAV into wavetable frames (USER table). Frame length below.", [this]
                {
                    chooser = std::make_unique<FileChooser> ("Import a wavetable", File::getSpecialLocation (File::userHomeDirectory), "*.wav;*.aif;*.aiff;*.flac");
                    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
                    {
                        if (! fc.getResult().existsAsFile()) return;
                        static const int lens[6] { 256, 512, 1024, 2048, 4096, 0 };
                        proc.undo.beginNewTransaction ("Import wavetable");
                        files::importFrames (proc, sel, fc.getResult(), lens[jlimit (0, 5, cycleLen.getSelectedId() - 1)]);
                    });
                });
                button ("CLEAR USER", "Remove the imported frames (USER = your drawn CUSTOM cycle again).", [this] { proc.setOscProperty (sel, "frames", {}); });
                cycleLen.setVisible (true);
                break;
            case TabHarm:
                for (auto [s, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "h_amount", "AMOUNT" }, { "h_odd", "ODD" }, { "h_even", "EVEN" },
                         { "h_rolloff", "ROLL-OFF" }, { "h_tilt", "TILT" }, { "h_spread", "SPREAD" }, { "h_phase", "PHASE" }, { "h_random", "RANDOM" }, { "seed", "SEED" } })
                    knob (sid (s), lab, String (s) == "h_tilt");
                for (auto [name, kind] : std::initializer_list<std::pair<const char*, int>> { { "SAW", 0 }, { "SQUARE", 1 }, { "TRIANGLE", 2 }, { "ORGAN", 3 }, { "ALL ON", 4 } })
                    button (name, "Fill the harmonic profile with a classic spectrum.", [this, kind]
                    {
                        auto d = proc.oscData().osc[(size_t) sel];
                        for (int h = 0; h < dsp::numHarmonics; ++h)
                        {
                            const int n = h + 1;
                            auto& a = d.hAmp[(size_t) h];
                            if (kind == 0) a = 1.f / (float) n;
                            else if (kind == 1) a = n % 2 == 1 ? 1.f / (float) n : 0.f;
                            else if (kind == 2) a = n % 2 == 1 ? 1.f / (float) (n * n) : 0.f;
                            else if (kind == 3) a = (n == 1 || n == 2 || n == 3 || n == 4 || n == 6 || n == 8) ? 0.8f / std::sqrt ((float) n) : 0.f;
                            if (kind == 4) d.hOn[(size_t) h] = true;
                        }
                        harmEditor.onCommit (d);
                    });
                harmEditor.setVisible (true);
                break;
            case TabUnison:
                combo (sid ("unison_mode"), "UNISON MODE"); combo (sid ("unison_phase"), "VOICE PHASES");
                knob (sid ("unison"), "VOICES"); knob (sid ("detune"), "DETUNE"); knob (sid ("spread"), "SPREAD"); knob (sid ("blend"), "BLEND"); knob (sid ("seed"), "SEED");
                break;
            case TabFm:
                knob (sid ("ratio"), "RATIO"); knob (sid ("ratio_fine"), "RATIO FINE", true); knob (sid ("fm_env"), "FM ENV"); knob (sid ("mod_decay"), "DECAY");
                // the 4 x 4 matrix: row = modulator, column = carrier, the diagonal = feedback
                for (int a = 1; a <= 4; ++a)
                    for (int b = 1; b <= 4; ++b)
                    {
                        const String id = a == b ? "osc" + String (a) + "_fm_feedback" : "fm_" + String (a) + "_to_" + String (b);
                        auto k = std::make_unique<doomsday::flat::Knob> (proc.apvts, id, a == b ? "FEEDBACK" : String (a) + " > " + String (b), oscCol (a - 1), false, 26);
                        k->slider.setTooltip (fge::tooltip (idOf (id)));
                        k->getProperties().set ("row", 3); k->getProperties().set ("slot", (a - 1) * 4 + (b - 1));
                        byId.insert ({ id, k.get() }); addAndMakeVisible (*k); tabCtl.push_back (std::move (k));
                    }
                break;
            case TabRing:
                combo (sid ("ring_source"), "RING SOURCE");
                knob (sid ("ring_amount"), "AMOUNT"); knob (sid ("ring_mix"), "MIX"); knob (sid ("ring_freq"), "SINE Hz"); knob (sid ("ring_env"), "RING ENV"); knob (sid ("mod_decay"), "DECAY");
                break;
            case TabPhase:
                combo (sid ("phase_mode"), "PHASE MODE"); combo (sid ("phase_source"), "ALIGN TO"); toggle (sid ("invert"), "INVERT");
                knob (sid ("phase"), "PHASE"); knob (sid ("phase_random"), "RANDOM"); knob (sid ("seed"), "SEED");
                break;
            case TabSync:
                combo (sid ("sync_source"), "SYNC MASTER"); combo (sid ("sync_mode"), "SYNC MODE");
                knob (sid ("sync_amount"), "AMOUNT");
                break;
            default: break;
        }
        const auto& c = palette();
        for (int t = 0; t < NumOscTabs; ++t)
        {
            tabBtn[(size_t) t].setColour (TextButton::buttonColourId, t == tab ? col : c.control);
            tabBtn[(size_t) t].setColour (TextButton::textColourOffId, t == tab ? c.ground : c.silver);
        }
        cycleEditor.setColour (col);
        cycleEditor.setShape (proc.oscData().osc[(size_t) sel].custom, -1.f, 1.f);
        harmEditor.setData (proc.oscData().osc[(size_t) sel]);
        resized();
    }

    void OscPage::resized()
    {
        const int w = getWidth(), h = getHeight();
        const int sw = (w - 3 * 8) / 4;
        for (int i = 0; i < 4; ++i)
        {
            const auto a = Rectangle<int> (i * (sw + 8), 0, sw, 196);
            stripArea[(size_t) i] = a;
            auto& st = strip[(size_t) i];
            st[0]->setBounds (a.getX() + 68, a.getY() + 6, 42, 20);
            st[1]->setBounds (a.getX() + 114, a.getY() + 6, 38, 20);
            st[2]->setBounds (a.getX() + 156, a.getY() + 6, 38, 20);
            resetBtn[(size_t) i].setBounds (a.getX() + 198, a.getY() + 5, 42, 22);
            presetBtn[(size_t) i].setBounds (a.getX() + 244, a.getY() + 5, jmin (48, a.getRight() - 4 - (a.getX() + 244)), 22);
            st[3]->setBounds (a.getX() + 8, a.getY() + 34, sw / 2 - 12, 22);
            st[4]->setBounds (a.getX() + sw / 2, a.getY() + 34, sw / 2 - 8, 22);
            const int kw = (sw - 8) / 6;
            for (int k = 0; k < 6; ++k) st[(size_t) (5 + k)]->setBounds (a.getX() + 4 + k * kw, a.getY() + 62, kw, doomsday::flat::Knob::heightFor (26));
            const int rw = (sw - 8) / 5;
            for (int k = 0; k < 5; ++k) st[(size_t) (11 + k)]->setBounds (a.getX() + 4 + k * rw, a.getY() + 128, rw, doomsday::flat::Knob::heightFor (26));
        }
        tabArea = { 0, 206, 808, h - 206 };
        scopeArea = { 818, 206, w - 818, h - 206 };
        for (int t = 0; t < NumOscTabs; ++t) tabBtn[(size_t) t].setBounds (tabArea.getX() + t * 82, tabArea.getY(), 80, 26);
        tabSave.setBounds (tabArea.getRight() - 112, tabArea.getY(), 54, 26);
        tabLoad.setBounds (tabArea.getRight() - 54, tabArea.getY(), 54, 26);
        viewSum.setBounds (scopeArea.getX(), scopeArea.getY(), 70, 26);
        viewPost.setBounds (scopeArea.getX() + 76, scopeArea.getY(), 96, 26);
        scopeView.setBounds (scopeArea.withTrimmedTop (32));
        tabContent = tabArea.withTrimmedTop (32).withTrimmedBottom (36);
        const int bw = 98;
        int bx = tabArea.getX();
        for (auto* b : { &oscSave, &oscLoad, &setSave, &setLoad }) { b->setBounds (bx, h - 28, bw, 26); bx += bw + 6; }
        // tab controls: row 0 choices (captions painted above), row 1 knobs, row 2 buttons, row 3 the FM grid
        int buttons = 0;
        for (auto& comp : tabCtl)
        {
            const int row = comp->getProperties()["row"], slot = comp->getProperties()["slot"];
            if (row == 0) comp->setBounds (tabContent.getX() + 8 + slot, tabContent.getY() + 18, (int) comp->getProperties()["w"], 24);
            else if (row == 1) comp->setBounds (tabContent.getX() + 4 + slot * 78, tabContent.getY() + 50, 76, doomsday::flat::Knob::heightFor (34));
            else if (row == 2) { comp->setBounds (tabContent.getX() + 8 + buttons * 112, tabContent.getY() + 128, 106, 24); ++buttons; }
            else if (row == 3)
            {
                const int a = slot / 4, b = slot % 4;
                comp->setBounds (tabContent.getX() + 380 + b * 100, tabContent.getY() + 132 + a * 66, 92, doomsday::flat::Knob::heightFor (26));
            }
        }
        tabEditorArea = { tabContent.getX(), tabContent.getY() + (buttons > 0 ? 160 : 132), tabContent.getWidth(), tabContent.getBottom() - tabContent.getY() - (buttons > 0 ? 160 : 132) };
        cycleEditor.setBounds (tabEditorArea);
        harmEditor.setBounds (tabEditorArea);
        cycleLen.setBounds (tabContent.getX() + 8 + buttons * 112, tabContent.getY() + 128, 120, 24);
    }

    void OscPage::paint (Graphics& g)
    {
        const auto& c = palette();
        for (int i = 0; i < 4; ++i)
        {
            const auto a = stripArea[(size_t) i];
            g.setColour (c.panel); g.fillRoundedRectangle (a.toFloat(), 6.f);
            if (i == sel) { g.setColour (oscCol (i).withAlpha (0.85f)); g.drawRoundedRectangle (a.toFloat().reduced (0.5f), 6.f, 1.5f); }
            const bool on = rawOf (proc, "osc" + String (i + 1) + "_on") > 0.5f;
            g.setColour (on ? oscCol (i) : c.textDim); g.fillRect (a.getX() + 10, a.getY() + 10, 3, 12);
            g.setColour (c.silver.withAlpha (on ? 0.9f : 0.5f)); g.setFont (lnf.font (11.5f, doomsday::flat::Bold, 0.08f));
            g.drawText ("OSC " + String (i + 1), a.getX() + 17, a.getY() + 6, 50, 20, Justification::left);
        }
        g.setColour (c.panel);
        g.fillRoundedRectangle (tabArea.withTrimmedTop (30).toFloat(), 6.f);
        g.fillRoundedRectangle (scopeArea.withTrimmedTop (30).toFloat().expanded (0.f, 2.f), 6.f);
        g.setFont (lnf.font (9.5f, doomsday::flat::Bold, 0.06f)); g.setColour (c.textDim);
        for (auto& comp : tabCtl)
            if (auto cap = comp->getProperties()["caption"].toString(); cap.isNotEmpty())
                g.drawText (cap, comp->getX(), comp->getY() - 15, comp->getWidth(), 13, Justification::left);
        g.setColour (oscCol (sel)); g.setFont (lnf.font (10.5f, doomsday::flat::Bold, 0.06f));
        g.drawText ("OSC " + String (sel + 1) + "  -  " + oscTabName (tab), tabContent.getRight() - 300, tabContent.getY() + 2, 292, 14, Justification::right);
        const int wave = roundToInt (rawOf (proc, sid ("wave")));
        g.setColour (c.textDim); g.setFont (lnf.font (10.f, doomsday::flat::Medium));
        const auto hint = tabContent.withHeight (14).withTrimmedLeft (200).withTrimmedRight (310).translated (0, 2);
        if (tab == TabWave)
            g.drawText (wave == dsp::WCustom ? "CUSTOM: draw the cycle below (double-click = point, right-click = curve)" : "set WAVEFORM to CUSTOM to play the drawn cycle",
                        hint, Justification::left);
        if (tab == TabHarm)
            g.drawText (wave == dsp::WHarmonic ? "HARMONIC: the profile below x the knobs" : "set WAVEFORM to HARMONIC to play this profile", hint, Justification::left);
        if (tab == TabFm)
        {
            g.setColour (c.textDim); g.setFont (lnf.font (10.f, doomsday::flat::Bold));
            g.drawText ("FM MATRIX: row = modulator, column = carrier", tabContent.getX() + 380, tabContent.getY() + 112, 400, 14, Justification::left);
            g.drawText ("(the modulator can be MUTED and still modulate)", tabContent.getX() + 8, tabContent.getY() + 140, 360, 14, Justification::left);
        }
        if (tab == TabTable)
        {
            // the frames of the table this oscillator plays (stacked, the current scan area brighter)
            const auto tb = proc.currentTables();
            if (const auto* t = tb[(size_t) sel].get(); t != nullptr && wave == dsp::WWavetable)
            {
                auto r = tabEditorArea.toFloat().reduced (8.f);
                g.setColour (c.ground); g.fillRoundedRectangle (tabEditorArea.toFloat(), 5.f);
                const int frames = t->frames;
                for (int f = 0; f < frames; ++f)
                {
                    const float off = (float) f / (float) jmax (1, frames - 1);
                    const float x0 = r.getX() + off * r.getWidth() * 0.25f, y0 = r.getBottom() - off * r.getHeight() * 0.35f - r.getHeight() * 0.3f;
                    const float fw = r.getWidth() * 0.72f, fh = r.getHeight() * 0.28f;
                    Path p;
                    const float* d = t->frame (f, 0);
                    for (int i = 0; i <= 128; ++i)
                    {
                        const float v = d[i * dsp::Table::N / 128 > dsp::Table::N ? dsp::Table::N : i * dsp::Table::N / 128];
                        const float x = x0 + fw * (float) i / 128.f, y = y0 - v * fh * 0.5f;
                        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                    }
                    g.setColour (oscCol (sel).withAlpha (0.25f + 0.6f * (1.f - std::abs (off - rawOf (proc, sid ("wt_position")))))); g.strokePath (p, PathStrokeType (1.2f));
                }
                g.setColour (c.textDim); g.setFont (FontOptions (10.f));
                g.drawText (String (frames) + " frames", tabEditorArea.reduced (8, 4), Justification::topRight);
            }
            else g.drawText ("set WAVEFORM to WAVETABLE to play a table", hint, Justification::left);
        }
        if (tab == TabPhase)
        {
            // the start phases of the four oscillators side by side (RESET mode)
            for (int i = 0; i < 4; ++i)
            {
                const auto cc = Point<float> ((float) tabEditorArea.getX() + 70.f + (float) i * 130.f, (float) tabEditorArea.getY() + 70.f);
                g.setColour (c.control); g.drawEllipse (Rectangle<float> (90.f, 90.f).withCentre (cc), 2.f);
                const float ph = rawOf (proc, "osc" + String (i + 1) + "_phase") / 360.f * MathConstants<float>::twoPi;
                const bool on = rawOf (proc, "osc" + String (i + 1) + "_on") > 0.5f;
                g.setColour (on ? oscCol (i) : c.textDim);
                g.drawLine (cc.x, cc.y, cc.x + 42.f * std::sin (ph), cc.y - 42.f * std::cos (ph), 3.f);
                g.setFont (FontOptions (10.f, Font::bold));
                g.drawText ("OSC " + String (i + 1), (int) cc.x - 40, (int) cc.y + 50, 80, 14, Justification::centred);
            }
        }
    }

    void OscPage::timerCallback()
    {
        if (! isShowing()) return;
        const auto& d = proc.oscData();
        const int wave = roundToInt (rawOf (proc, sid ("wave")));
        if (proc.oscDataVersion() != lastData)
        {
            lastData = proc.oscDataVersion();
            cycleEditor.setShape (d.osc[(size_t) sel].custom, -1.f, 1.f);
            harmEditor.setData (d.osc[(size_t) sel]);
        }
        cycleEditor.setActive (wave == dsp::WCustom && d.osc[(size_t) sel].importedCycle.empty(),
                               wave != dsp::WCustom ? "WAVEFORM is not CUSTOM - this drawing is not played" : "An imported WAV cycle is playing - CLEAR IMPORT to play the drawing");
        if (wave != lastWave) { lastWave = wave; repaint(); }
        auto f = proc.frame();
        scopeView.update (f, proc.currentTables(), sel, viewSum.getToggleState(), viewPost.getToggleState());
        // what does nothing right now is greyed out
        auto en = [this] (const String& id, bool on)
        {
            auto range = byId.equal_range (id);
            for (auto it = range.first; it != range.second; ++it) if (it->second->isEnabled() != on) { it->second->setEnabled (on); it->second->setAlpha (on ? 1.f : 0.35f); }
        };
        for (int i = 0; i < 4; ++i)
        {
            const String o = "osc" + String (i + 1) + "_";
            const bool on = rawOf (proc, o + "on") > 0.5f;
            for (auto* comp : strip[(size_t) i]) if (comp != strip[(size_t) i][0]) { comp->setEnabled (on); comp->setAlpha (on ? 1.f : 0.35f); }
            const bool tailStereo = rawOf (proc, o + "to_tail") > -59.9f;
            en (o + "pan", on && tailStereo);
        }
        const String o = "osc" + String (sel + 1) + "_";
        const int w = roundToInt (rawOf (proc, o + "wave"));
        en (o + "noise", w == dsp::WNoise);
        en (o + "pw", w == dsp::WPulse);
        en (o + "freq", roundToInt (rawOf (proc, o + "track")) == dsp::TrackFixed);
        const bool uni = rawOf (proc, o + "unison") > 1.5f && w != dsp::WNoise;
        for (auto* s : { "detune", "spread", "blend", "unison_mode", "unison_phase" }) en (o + s, uni);
        const int wtMode = roundToInt (rawOf (proc, o + "wt_mode"));
        en (o + "wt_position", wtMode == dsp::WtStatic);
        for (auto* s : { "wt_start", "wt_end", "wt_scan" }) en (o + s, wtMode != dsp::WtStatic);
        const bool ring = roundToInt (rawOf (proc, o + "ring_source")) > 0;
        for (auto* s : { "ring_amount", "ring_mix", "ring_env" }) en (o + s, ring);
        en (o + "ring_freq", roundToInt (rawOf (proc, o + "ring_source")) == 5);
        const bool sync = roundToInt (rawOf (proc, o + "sync_source")) > 0;
        for (auto* s : { "sync_mode", "sync_amount" }) en (o + s, sync);
        const int pm = roundToInt (rawOf (proc, o + "phase_mode"));
        en (o + "phase_random", pm == dsp::PhRandom);
        en (o + "phase_source", pm == dsp::PhSynced);
        repaint (stripArea[0].getUnion (stripArea[3]).withHeight (28));
    }

    void OscPage::showPresetMenu (int osc)
    {
        PopupMenu m;
        int item = 1;
        std::vector<const OscPreset*> map;
        for (auto& cat : oscPresetCategories())
        {
            PopupMenu sub;
            for (auto& pr : oscPresets()) if (cat == pr.category) { sub.addItem (item++, pr.name); map.push_back (&pr); }
            m.addSubMenu (cat, sub);
        }
        m.addSeparator();
        m.addItem (1000, "Save this oscillator...");
        m.addItem (1001, "Load into this oscillator...");
        SafePointer<OscPage> self (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetBtn[(size_t) osc]), [self, osc, map] (int r)
        {
            if (self == nullptr || r == 0) return;
            self->selectOsc (osc);
            if (r == 1000) self->saveFile (1);
            else if (r == 1001) self->loadFile (1);
            else if (r - 1 < (int) map.size()) files::applyPreset (self->proc, osc, *map[(size_t) r - 1]);
        });
    }

    void OscPage::saveFile (int what)
    {
        const char* ext[3] { "4gtab", "4gosc", "4goscs" };
        const String name = what == 0 ? String (oscTabName (tab)) : what == 1 ? "Oscillator" : "Oscillator set";
        std::shared_ptr<XmlElement> x = what == 0 ? files::oscToXml (proc, sel, tab) : what == 1 ? files::oscToXml (proc, sel, -1) : files::setToXml (proc);
        chooser = std::make_unique<FileChooser> ("Save", files::folder ("Oscillators").getChildFile (name + "." + ext[what]), String ("*.") + ext[what]);
        const String e (ext[what]);
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [x, e] (const FileChooser& fc)
        { if (fc.getResult() != File()) files::writeXml (fc.getResult().withFileExtension (e), *x); });
    }
    void OscPage::loadFile (int what)
    {
        const char* ext[3] { "4gtab", "4gosc", "4goscs" };
        chooser = std::make_unique<FileChooser> ("Load", files::folder ("Oscillators"), String ("*.") + ext[what]);
        SafePointer<OscPage> self (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [self, what] (const FileChooser& fc)
        {
            if (self == nullptr) return;
            auto x = files::readXml (fc.getResult());
            if (x == nullptr) return;
            if (what == 2) files::xmlToSet (self->proc, *x);
            else files::xmlToOsc (self->proc, self->sel, *x);
        });
    }
}
