#include "PluginEditor.h"
#include <regex>
#include "../Presets/StyleEngine.h"
#include "../Presets/LengthEngine.h"
#include "DoomsdayBrand.h"

using namespace juce;
using namespace fge;
using fge::gui::palette;

namespace
{
    const char* partName[4] { "PUNCH", "BODY", "TAIL", "SUB" };
    float raw (GeProcessor& p, int id) { return p.apvts.getRawParameterValue (def (id).id)->load(); }
}

doomsday::ui::Theme GeCanvas::makeTheme()
{
    doomsday::ui::Theme t;
    const auto& c = palette();
    t.bg = c.ground; t.panel = c.panel; t.text = c.silver; t.value = c.silver; t.label = c.textDim; t.dim = c.textDim;
    t.accents = { { c.red, c.red.darker (0.7f), c.hot } };
    return t;
}

GeCanvas::GeCanvas (GeProcessor& p) : proc (p), presetBar (p, true)
{
    setLookAndFeel (&lnf);
    presetBar.setLookAndFeel (&stageLnf);
    const auto& c = palette();
    addAndMakeVisible (presetBar);
    // module 13: STYLE (always visible) - a click applies (KEEP MY CHANGES), a right-click = APPLY ALL / PREVIEW / FORGET
    styleStrip.setTooltip ("STYLE: EARLY / MODERN / RAW / EXPERIMENTAL - a starting point for the character (generators, pitch drop, distortion chain, screech). "
                           "It never changes the LENGTH and never locks a knob; your own changes are kept. Right-click: APPLY ALL / PREVIEW / FORGET MY CHANGES.");
    styleStrip.tipFor = [] (int i) { return fge::style::names()[i] + ": " + fge::style::description (i) + "  (right-click: APPLY ALL / PREVIEW / FORGET MY CHANGES)"; };
    styleStrip.onPick = [this] (int st) { chooseStyle (st, true); };
    styleStrip.onMenu = [this] (int) { styleMenu(); };
    addAndMakeVisible (styleStrip);
    // module 14: LENGTH profiles (one click = one undo) + the ms button = the LENGTH panel
    lengthStrip.tipFor = [] (int i) { return "LENGTH " + fge::length::names()[i] + ": " + String (roundToInt (fge::length::profileMs (i))) + " ms with AUTO LENGTH (the parts scale with it) - independent of the STYLE"; };
    lengthStrip.onPick = [this] (int i) { fge::length::applyProfile (proc, i); };
    lengthStrip.onMenu = [this] (int) { openLengthPanel(); };
    addAndMakeVisible (lengthStrip);
    lengthBtn.setTooltip ("LENGTH: the exact length, LENGTH SYNC to the tempo, AUTO LENGTH and how much PUNCH / BODY / TAIL / SUB follow - "
                          "independent of the STYLE.");
    lengthBtn.onClick = [this] { openLengthPanel(); };
    addAndMakeVisible (lengthBtn);
    // module 17: the navigation + the interface settings
    uiSet = fge::ui::Settings::load();
    nav.onSelect = [this] (int pg) { showPage (pg); };
    nav.isActive = [this] (int pg) { return pageActive (pg); };
    addAndMakeVisible (nav);
    {
        const std::pair<const char*, int> rules[] { { "^osc[0-9]+_on$", 2 }, { "^layer[0-9]+_on$", 3 }, { "^noise[A-Z]_on$", 3 }, { "^tr_on$", 3 },
            { "^dist[0-9]+_on$", 4 }, { "^flt[0-9]+_on$", 5 }, { "^res[0-9]+_on$", 5 }, { "^scr_on$", 5 },
            { "^eq[0-9]+_on$", 6 }, { "^deq[0-9]+_on$", 6 }, { "^mb_on$", 6 }, { "^sp_on$", 6 }, { "^stw_on$", 6 },
            { "^cmp[0-9]+_on$", 7 }, { "^ts[0-9]+_on$", 7 }, { "^clp[0-9]+_on$", 7 }, { "^lim[0-9]+_on$", 7 }, { "^crest_on$", 7 },
            { "^spc_(punch|body|tail|sub|texture|custom|master)_on$", 10 } };
        for (int i = 0; i < P::count; ++i)
            for (auto& [re, pg] : rules)
                if (std::regex_match (def (i).id, std::regex (re))) { activeParams.push_back ({ pg, i }); break; }
    }
    addAndMakeVisible (wave);

    // ---- header: global
    header = { knob ("core_length", "LENGTH", c.red, 44), knob ("core_master", "MASTER", c.silver, 44), knob ("core_velocity", "VELOCITY", c.silver, 44),
               toggle ("core_auto_length", "AUTO LENGTH", Toggle::Pill, c.red), toggle ("core_phase_reset", "PHASE RESET", Toggle::Pill, c.red),
               toggle ("core_key_track", "PITCH TRACKING", Toggle::Pill, c.red), seg ("core_voice_mode", { "CHOKE", "POLY" }, c.red),
               seg ("core_note_cut", { "LENGTH", "NOTE", "+FX" }, c.red) };
    // ---- pitch engine
    pitchRow = { seg ("pe_mode", { "CLASSIC", "FAST", "SLOW", "DOUBLE", "RISE", "STABLE" }, c.red),
                 knob ("pe_start", "START", c.part[2], 38), knob ("pe_end", "END", c.part[2], 38), knob ("pe_time", "TIME", c.part[2], 38),
                 knob ("pe_curve", "CURVE", c.part[2], 38), knob ("pe_amount", "AMOUNT", c.part[2], 38, true), knob ("pe_fine", "FINE", c.part[2], 38, true),
                 knob ("tail_penv_offset", "HOLD", c.part[2], 38), knob ("tail_hold_level", "TOK LEVEL", c.part[2], 38, true) };
    // ---- the four parts: card (always visible) + detail (selected part) + mixer column
    const char* pre[4] { "punch", "body", "tail", "sub" };
    for (int i = 0; i < 4; ++i)
    {
        const auto col = c.part[(size_t) i];
        const String s (pre[i]);
        const String pitchId = i == 2 ? "pe_start" : s + "_pitch";
        card[(size_t) i] = { knob ((s + "_level").toRawUTF8(), "LEVEL", col), knob (pitchId.toRawUTF8(), i == 2 ? "PITCH (START)" : "PITCH", col),
                             knob ((s + "_length").toRawUTF8(), "LENGTH", col), knob ((s + "_drive").toRawUTF8(), "DRIVE", col),
                             toggle (("mix_" + s + "_mute").toRawUTF8(), "MUTE", Toggle::Chip, c.silver), toggle (("mix_" + s + "_solo").toRawUTF8(), "SOLO", Toggle::Chip, col) };
        auto& d = detail[(size_t) i];
        if (i == 0) d.push_back (seg ("punch_source", { "SINE", "TRI", "SAW", "SQUARE", "NOISE", "CLICK" }, col));
        if (i == 1) d.push_back (seg ("body_source", { "SINE", "TRI", "SAW", "SQUARE", "NOISE" }, col));
        if (i == 2) { d.push_back (seg ("tail_source", { "SINE", "TRI", "SAW", "SQUARE", "PULSE", "NOISE" }, col));
                      d.push_back (seg ("tail_character", { "CLEAN", "TONAL", "DIRTY", "SCREAM", "METAL", "BUZZ", "CRUSH" }, col)); }
        if (i == 3) d.push_back (seg ("sub_source", { "SINE", "TRI" }, col));
        for (auto* k : { "_attack", "_decay", "_sustain", "_release" })
            d.push_back (knob ((s + k).toRawUTF8(), String (k + 1).toUpperCase(), col));
        if (i < 3) d.push_back (knob ((s + "_tone").toRawUTF8(), "TONE", col));
        if (i == 1 || i == 2) d.push_back (knob ((s + "_resonance").toRawUTF8(), "RESONANCE", col));
        if (i < 3) d.push_back (knob ((s + "_mix").toRawUTF8(), "MIX", col));
        if (i < 2) d.push_back (knob ((s + "_glide").toRawUTF8(), "GLIDE", col));
        if (i == 2) d.push_back (knob ("tail_pulse_width", "PULSE W", col));
        d.push_back (knob ((s + "_phase").toRawUTF8(), "PHASE", col));
        if (i == 2)
        {
            // sound pass: UNISON / WIDTH, the cuts and the mid bell, STAGE 2
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "tail_uni_voices", "UNISON" }, { "tail_uni_detune", "DETUNE" }, { "tail_uni_drift", "DRIFT" }, { "tail_uni_mix", "UNI MIX" },
                     { "tail_width", "WIDTH" }, { "tail_width_from", "WIDTH FROM" }, { "tail_low_cut", "LOW CUT" }, { "tail_high_cut", "HIGH CUT" },
                     { "tail_mid_hz", "MID FREQ" }, { "tail_mid_gain", "MID GAIN" }, { "tail_mid_q", "MID Q" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            d.push_back (combo ("tail_st2_type"));
            d.push_back (knob ("tail_st2_drive", "ST2 DRIVE", col));
            d.push_back (knob ("tail_st2_bias", "ST2 BIAS", col, 40, true));
            d.push_back (knob ("tail_st2_mix", "ST2 MIX", col));
        }
        mixer[(size_t) i] = { knob (("mix_" + s + "_level").toRawUTF8(), "FADER", col, 34), knob (("mix_" + s + "_pan").toRawUTF8(), "PAN", col, 30, true),
                              toggle (("mix_" + s + "_invert").toRawUTF8(), "PHASE " + String (CharPointer_UTF8 ("\xc3\x98")), Toggle::Chip, col) };
    }
    masterRow = { knob ("master_clip_drive", "CLIP", c.red, 34), knob ("master_clip_soft", "SOFT", c.red, 34) };
    // module 09: the 8 MASTER MACROS (sources only - what they move = the MACRO MAPPING on the MOD page)
    for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "macro_impact", "IMPACT" }, { "macro_body", "BODY" }, { "macro_tail", "TAIL" },
             { "macro_pitch", "PITCH" }, { "macro_grit", "GRIT" }, { "macro_scream", "SCREAM" }, { "macro_weight", "WEIGHT" }, { "macro_chaos", "CHAOS" } })
        macroRow.push_back (knob (id, lab, c.red, 30));
    for (auto& [id, comp] : byId)
    {
        const int idx = [&] { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; }();
        if (idx >= 0)
        {
            const String tip = fge::tooltip (idx);
            if (auto* k = dynamic_cast<Knob*> (comp)) k->slider.setTooltip (tip);
            else if (auto* t = dynamic_cast<SettableTooltipClient*> (comp)) t->setTooltip (tip);
        }
    }
    envPage = std::make_unique<fge::gui::EnvPage> (proc, lnf);
    addChildComponent (*envPage);
    oscPage = std::make_unique<fge::gui::OscPage> (proc, lnf);
    addChildComponent (*oscPage);
    layerPage = std::make_unique<fge::gui::LayerPage> (proc, lnf);
    addChildComponent (*layerPage);
    distortionPage = std::make_unique<fge::gui::DistPage> (proc, lnf);
    addChildComponent (*distortionPage);
    scrPage = std::make_unique<fge::gui::ScreechPage> (proc, lnf);
    addChildComponent (*scrPage);
    eqPg = std::make_unique<fge::gui::EqPage> (proc, lnf);
    addChildComponent (*eqPg);
    dynPg = std::make_unique<fge::gui::DynPage> (proc, lnf);
    addChildComponent (*dynPg);
    modPg = std::make_unique<fge::gui::ModPage> (proc, lnf);
    addChildComponent (*modPg);
    smpPg = std::make_unique<fge::gui::SamplePage> (proc, lnf);
    addChildComponent (*smpPg);
    stPg = std::make_unique<fge::gui::StereoPage> (proc, lnf);
    addChildComponent (*stPg);
    exPg = std::make_unique<fge::gui::ExportPage> (proc, lnf);
    addChildComponent (*exPg);
    prPg = std::make_unique<fge::gui::PresetPage> (proc, presetBar);                // module 15: a click on the preset name
    prPg->onClose = [this] { showPage (pageBeforePresets); };
    prPg->onRelink = [this] { showPage (9); };
    addChildComponent (*prPg);
    anPg = std::make_unique<fge::gui::AnalysisPage> (proc, lnf);                       // module 16
    addChildComponent (*anPg);
    setPg = std::make_unique<fge::gui::SettingsPage> (uiSet);                        // module 17
    setPg->version = fge::versionText;
    setPg->onChanged = [this] { uiSet.save(); applyTooltips(); if (onSettingsChanged) onSettingsChanged(); };
    addChildComponent (*setPg);
    presetBar.onBrowse = [this] { if (pageIdx != 12) pageBeforePresets = pageIdx; showPage (pageIdx == 12 ? pageBeforePresets : 12); };
    wave.onScreechStart = [this] (float ms, bool first) { if (first) proc.undo.beginNewTransaction ("Screech start"); proc.setParamInUndoStep ("scr_start", ms); };
    applyTooltips();
    setWantsKeyboardFocus (true);
    selectPart (2);
    showPage (uiSet.startPage < 0 ? uiSet.lastPage : uiSet.startPage);
    updateEnabled();
    startTimerHz (30);
}

GeCanvas::~GeCanvas() { *alive = false; stopTimer(); presetBar.setLookAndFeel (nullptr); setLookAndFeel (nullptr); }

GeCanvas::Knob* GeCanvas::knob (const char* id, const String& label, Colour accent, int d, bool bipolar)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, accent, bipolar, d);
    auto* raw = k.get(); addAndMakeVisible (raw); byId.insert ({ id, raw }); owned.push_back (std::move (k)); return raw;
}
GeCanvas::Toggle* GeCanvas::toggle (const char* id, const String& text, Toggle::Style st, Colour accent)
{
    auto t = std::make_unique<Toggle> (proc.apvts, id, text, st, accent);
    auto* raw = t.get(); addAndMakeVisible (raw); byId.insert ({ id, raw }); owned.push_back (std::move (t)); return raw;
}
GeCanvas::Seg* GeCanvas::seg (const char* id, StringArray labels, Colour accent)
{
    auto s = std::make_unique<Seg> (proc.apvts, id, labels, accent);
    auto* raw = s.get(); addAndMakeVisible (raw); byId.insert ({ id, raw }); owned.push_back (std::move (s)); return raw;
}
ComboBox* GeCanvas::combo (const char* id)
{
    auto cb = std::make_unique<ComboBox>();
    const int idx = [&] { for (int i = 0; i < P::count; ++i) if (String (id) == def (i).id) return i; return 0; }();
    cb->addItemList (def (idx).choices(), 1);
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *cb));
    auto* raw = cb.get(); addAndMakeVisible (raw); byId.insert ({ id, raw }); owned.push_back (std::move (cb)); return raw;
}
Component* GeCanvas::findControl (const String& id) { auto it = byId.find (id); return it == byId.end() ? nullptr : it->second; }

void GeCanvas::selectPart (int i)
{
    selected = jlimit (0, 3, i);
    for (int p = 0; p < 4; ++p) for (auto* c : detail[(size_t) p]) if (c != nullptr) c->setVisible (p == selected && pageIdx == 0);
    layoutDetail();
    repaint();
}

void GeCanvas::showPage (int i)
{
    pageIdx = jlimit (0, 14, i);
    const bool main = pageIdx == 0, kick = pageIdx < 2;
    for (int p = 0; p < 4; ++p)
    {
        for (auto* c : card[(size_t) p]) c->setVisible (main);
        for (auto* c : mixer[(size_t) p]) c->setVisible (main);
        for (auto* c : detail[(size_t) p]) if (c != nullptr) c->setVisible (main && p == selected);
    }
    for (auto* c : masterRow) c->setVisible (main);
    for (auto* c : pitchRow) c->setVisible (kick);
    header[5]->setVisible (kick);                                                    // PITCH TRACKING sits in the pitch engine
    envPage->setVisible (pageIdx == 1);
    oscPage->setVisible (pageIdx == 2);
    layerPage->setVisible (pageIdx == 3);
    distortionPage->setVisible (pageIdx == 4);
    scrPage->setVisible (pageIdx == 5);
    eqPg->setVisible (pageIdx == 6);
    dynPg->setVisible (pageIdx == 7);
    modPg->setVisible (pageIdx == 8);
    smpPg->setVisible (pageIdx == 9);
    stPg->setVisible (pageIdx == 10);
    exPg->setVisible (pageIdx == 11);
    prPg->setVisible (pageIdx == 12);
    anPg->setVisible (pageIdx == 13);
    setPg->setVisible (pageIdx == 14);
    if (pageIdx == 14) setPg->sync();
    wave.setVisible (kick);
    nav.setCurrent (pageIdx);
    if (pageIdx != uiSet.lastPage && pageIdx != 12) { uiSet.lastPage = pageIdx; if (isShowing()) uiSet.save(); }
    repaint();
}

void GeCanvas::mouseDown (const MouseEvent& e)
{
    if (pageIdx != 0) return;
    for (int i = 0; i < 4; ++i)
        if (cardArea[(size_t) i].contains (e.getPosition())) { selectPart (i); return; }
}

void GeCanvas::resized()
{
    headerArea = { 0, 0, W, 96 };
    presetBar.setBounds (W - 16 - 300, 5, 300, 90);
    // top bar: STYLE, LENGTH (+ the ms panel), the INFO LINE under them
    styleStrip.setBounds (300, 28, 300, 30);
    lengthStrip.setBounds (624, 28, 280, 30);
    lengthBtn.setBounds (910, 28, 210, 30);
    infoArea = { 300, 66, W - 16 - 300 - 300 - 14, 26 };
    // the navigation (left)
    nav.setBounds (16, Y0, navW - 16, CH);
    // bottom: MASTER MACROS + GLOBAL (always visible)
    macroArea = { 16, Y0 + CH + 8, 924, 104 };
    for (size_t i = 0; i < macroRow.size(); ++i) macroRow[i]->setBounds (macroArea.getX() + 170 + (int) i * 94, macroArea.getY() + 8, 84, Knob::heightFor (40));
    globalArea = { macroArea.getRight() + 12, macroArea.getY(), W - 16 - macroArea.getRight() - 12, 104 };
    {
        int x = globalArea.getX() + 8;
        for (int k : { 0, 1, 2 }) { header[(size_t) k]->setBounds (x, globalArea.getY() + 22, 68, Knob::heightFor (40)); x += 70; }
        x += 6;
        header[6]->setBounds (x, globalArea.getY() + 8, 146, 20);           // VOICE
        header[7]->setBounds (x, globalArea.getY() + 31, 146, 20);          // KICK END
        header[4]->setBounds (x, globalArea.getY() + 54, 146, 20);          // PHASE RESET
        header[3]->setBounds (x, globalArea.getY() + 77, 146, 20);          // AUTO LENGTH
        meterArea = { globalArea.getRight() - 34, globalArea.getY() + 8, 24, globalArea.getHeight() - 26 };
    }
    // MAIN: pitch engine
    pitchArea = { X0, Y0, CW, 84 };
    pitchRow[0]->setBounds (pitchArea.getX() + 14, pitchArea.getY() + 40, 520, 28);
    header[5]->setBounds (pitchArea.getX() + 140, pitchArea.getY() + 7, 146, 20);  // PITCH TRACKING
    for (int i = 1; i < 9; ++i) pitchRow[(size_t) i]->setBounds (pitchArea.getX() + 570 + (i - 1) * 80, pitchArea.getY() + 4, 76, Knob::heightFor (38));
    // the central waveform
    waveArea = { X0, Y0 + 92, CW, 134 };
    wave.setBounds (waveArea);
    // part cards
    const int cw = (CW - 3 * 12) / 4;
    for (int i = 0; i < 4; ++i)
    {
        cardArea[(size_t) i] = { X0 + i * (cw + 12), Y0 + 234, cw, 160 };
        const auto a = cardArea[(size_t) i];
        for (int k = 0; k < 4; ++k) card[(size_t) i][(size_t) k]->setBounds (a.getX() + 8 + k * 72, a.getY() + 28, 70, Knob::heightFor (40));
        card[(size_t) i][4]->setBounds (a.getRight() - 136, a.getY() + 4, 60, 20);
        card[(size_t) i][5]->setBounds (a.getRight() - 70, a.getY() + 4, 60, 20);
    }
    detailArea = { X0, Y0 + 404, 860, 314 };
    mixerArea = { detailArea.getRight() + 12, detailArea.getY(), X0 + CW - detailArea.getRight() - 12, 314 };
    if (masterRow.size() == 2)
    {
        masterRow[0]->setBounds (mixerArea.getX() + 90, mixerArea.getBottom() - 86, 80, Knob::heightFor (34));
        masterRow[1]->setBounds (mixerArea.getX() + 180, mixerArea.getBottom() - 86, 80, Knob::heightFor (34));
    }
    const int mw = mixerArea.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        const int mx = mixerArea.getX() + i * mw;
        mixer[(size_t) i][0]->setBounds (mx + 4, mixerArea.getY() + 40, mw - 22, Knob::heightFor (34));
        mixer[(size_t) i][1]->setBounds (mx + 4, mixerArea.getY() + 112, mw - 22, Knob::heightFor (30));
        mixer[(size_t) i][2]->setBounds (mx + 6, mixerArea.getY() + 186, mw - 26, 22);
        mixMeterArea[(size_t) i] = { mx + mw - 16, mixerArea.getY() + 44, 6, 132 };
    }
    layoutDetail();
    const Rectangle<int> pageArea (X0, Y0, CW, CH);
    if (envPage != nullptr) envPage->setBounds (pageArea.withTrimmedTop (234));
    for (Component* pg : { (Component*) oscPage.get(), (Component*) layerPage.get(), (Component*) distortionPage.get(), (Component*) eqPg.get(), (Component*) dynPg.get(),
                           (Component*) modPg.get(), (Component*) smpPg.get(), (Component*) stPg.get(), (Component*) exPg.get(), (Component*) prPg.get(),
                           (Component*) anPg.get(), (Component*) scrPage.get(), (Component*) setPg.get() })
        if (pg != nullptr) pg->setBounds (pageArea);
}

void GeCanvas::layoutDetail()
{
    auto& d = detail[(size_t) selected];
    if (d.empty()) return;
    int y = detailArea.getY() + 32, x = detailArea.getX() + 14;
    size_t k = 0;
    // segmented rows (source, character)
    while (k < d.size() && dynamic_cast<Seg*> (d[k]) != nullptr)
    {
        const int items = selected == 2 && k == 1 ? 7 : (int) (selected == 3 ? 2 : 6);
        d[k]->setBounds (x + (int) k * 420, y, jmin (400, items * 62), 26);
        ++k;
    }
    y += 36;
    for (int i = 0; k < d.size(); ++k)
    {
        if (d[k] == nullptr) { y += Knob::heightFor (40) + 6; i = 0; continue; }       // row break
        if (dynamic_cast<ComboBox*> (d[k]) != nullptr) { d[k]->setBounds (x + i * 76, y + 22, 150, 26); i += 2; continue; }
        d[k]->setBounds (x + i * 76, y, 74, Knob::heightFor (40));
        ++i;
    }
}

void GeCanvas::paint (Graphics& g)
{
    const auto& c = palette();
    g.fillAll (c.ground);
    // top bar
    g.setColour (c.panel.darker (0.25f)); g.fillRect (0, 0, W, 96);
    g.setColour (c.grid); g.drawHorizontalLine (96, 0.f, (float) W);
    doomsday::brand::drawHeaderBadge (g, 1.f, 0.55f);
    const float tx = doomsday::brand::headerTextX (1.f);
    g.setColour (c.silver); g.setFont (lnf.font (30.f, doomsday::flat::Bold, 0.12f));
    g.drawText ("4GE", (int) tx, 14, 200, 34, Justification::left);
    g.setColour (c.red); g.setFont (lnf.font (11.f, doomsday::flat::Bold, 0.18f));
    g.drawText ("HARDSTYLE KICK DESIGNER", (int) tx, 50, 180, 16, Justification::left);
    g.setColour (c.textDim); g.setFont (lnf.font (11.f, doomsday::flat::Medium));
    g.drawText ("v" + String (versionText), (int) tx, 68, 40, 16, Justification::left);
    g.setFont (lnf.font (10.f, doomsday::flat::Bold, 0.14f));
    g.drawText ("STYLE", styleStrip.getX(), 10, 100, 14, Justification::left);
    g.drawText ("LENGTH", lengthStrip.getX(), 10, 100, 14, Justification::left);
    // the INFO LINE
    if (uiSet.infoLine)
    {
        g.setColour (c.ground); g.fillRoundedRectangle (infoArea.toFloat(), 4.f);
        g.setColour (c.red); g.fillEllipse ((float) infoArea.getX() + 8.f, (float) infoArea.getCentreY() - 2.5f, 5.f, 5.f);
        g.setColour (c.silver.withAlpha (0.85f)); g.setFont (lnf.font (11.f, doomsday::flat::Medium));
        g.drawFittedText (infoText, infoArea.withTrimmedLeft (20).withTrimmedRight (8), Justification::centredLeft, 1, 0.85f);
    }

    // panels
    auto panel = [&] (Rectangle<int> r, const String& title, Colour dot)
    {
        g.setColour (c.panel); g.fillRoundedRectangle (r.toFloat(), 6.f);
        if (title.isNotEmpty())
        {
            g.setColour (dot); g.fillRect (r.getX() + 12, r.getY() + 12, 3, 12);
            g.setColour (c.silver.withAlpha (0.8f)); g.setFont (lnf.font (11.5f, doomsday::flat::Bold, 0.1f));
            g.drawText (title, r.getX() + 20, r.getY() + 8, 300, 20, Justification::left);
        }
    };
    if (pageIdx < 2)
    {
        panel (pitchArea, "PITCH ENGINE", c.part[2]);
        g.setColour (c.textDim); g.setFont (lnf.font (10.5f, doomsday::flat::Medium));
        const bool track = raw (proc, P::coreKeyTrack) > 0.5f;
        g.drawText (track ? "END = the played note, START moves with it" : "FIXED: START -> END in Hz",
                    pitchArea.getX() + 296, pitchArea.getY() + 8, 260, 20, Justification::left);
    }
    // MASTER MACROS + GLOBAL (every page)
    panel (macroArea, "MASTER MACROS", c.red);
    g.setColour (c.textDim); g.setFont (lnf.font (9.5f, doomsday::flat::Medium));
    g.drawFittedText ("drag  >  onto any knob\nright-click a knob = modulate\nMOD MATRIX = what they move", macroArea.getX() + 14, macroArea.getY() + 34, 150, 44, Justification::topLeft, 3);
    for (auto* m : macroRow)
    {
        const auto h = Rectangle<float> ((float) m->getRight() + 2.f, (float) m->getY() + 12.f, 14.f, 14.f);
        g.setColour (c.control); g.fillRoundedRectangle (h, 3.f);
        g.setColour (c.red); g.drawText (">", h, Justification::centred);
    }
    panel (globalArea, "", c.red);
    if (pageIdx == 0) paintMain (g, panel);
    // output meter
    {
        const float m = jlimit (0.f, 1.f, (Decibels::gainToDecibels (outMeter, -60.f) + 60.f) / 66.f);
        auto r = meterArea.reduced (6, 2);
        g.setColour (c.control); g.fillRect (r);
        g.setColour (outMeter > 1.f ? c.hot : c.silver.withAlpha (0.85f));
        g.fillRect (r.withTop (r.getBottom() - roundToInt ((float) r.getHeight() * m)));
        g.setColour (c.textDim); g.setFont (lnf.font (9.f, doomsday::flat::Bold));
        g.drawText ("OUT", meterArea.getX() - 4, meterArea.getBottom() + 2, meterArea.getWidth() + 8, 12, Justification::centred);
    }
}

void GeCanvas::paintMain (Graphics& g, const std::function<void (Rectangle<int>, const String&, Colour)>& panel)
{
    const auto& c = palette();
    for (int i = 0; i < 4; ++i)
    {
        const auto a = cardArea[(size_t) i];
        panel (a, partName[i], c.part[(size_t) i]);
        if (i == selected) { g.setColour (c.part[(size_t) i].withAlpha (0.8f)); g.drawRoundedRectangle (a.toFloat().reduced (0.5f), 6.f, 1.5f); }
        // peak meter of the part (from the core mixer)
        const float m = jlimit (0.f, 1.f, (Decibels::gainToDecibels (meter[(size_t) i], -60.f) + 60.f) / 66.f);
        g.setColour (c.control); g.fillRect (a.getX() + 12, a.getBottom() - 10, a.getWidth() - 24, 3);
        g.setColour (c.part[(size_t) i]); g.fillRect ((float) a.getX() + 12.f, (float) a.getBottom() - 10.f, (float) (a.getWidth() - 24) * m, 3.f);
    }
    panel (detailArea, String (partName[selected]) + "  -  SOURCE / ENVELOPE / TONE", c.part[(size_t) selected]);
    panel (mixerArea, "CORE MIXER", c.red);
    const int mw = mixerArea.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (c.part[(size_t) i]); g.setFont (lnf.font (10.5f, doomsday::flat::Bold, 0.06f));
        g.drawText (partName[i], mixerArea.getX() + i * mw, mixerArea.getY() + 26, mw - 12, 14, Justification::centred);
        const auto r = mixMeterArea[(size_t) i];
        const float m = jlimit (0.f, 1.f, (Decibels::gainToDecibels (meter[(size_t) i], -60.f) + 60.f) / 66.f);
        g.setColour (c.control); g.fillRect (r);
        g.setColour (c.part[(size_t) i]); g.fillRect (r.withTop (r.getBottom() - roundToInt ((float) r.getHeight() * m)));
    }
    g.setColour (c.silver.withAlpha (0.8f)); g.setFont (lnf.font (10.5f, doomsday::flat::Bold, 0.06f));
    g.drawText ("MASTER CLIP", mixerArea.getX() + 12, mixerArea.getBottom() - 100, 200, 14, Justification::left);
    g.setColour (c.textDim); g.setFont (lnf.font (9.5f, doomsday::flat::Medium));
    g.drawFittedText ("the SUB goes\naround it", mixerArea.getX() + 12, mixerArea.getBottom() - 70, 76, 30, Justification::topLeft, 2);
    if (selected == 2) if (auto* cb = findControl ("tail_st2_type"))
    {
        g.setColour (c.textDim); g.setFont (lnf.font (10.5f, doomsday::flat::Bold, 0.06f));
        g.drawText ("STAGE 2", cb->getX(), cb->getY() - 18, 150, 14, Justification::left);
    }
}

void GeCanvas::updateEnabled()
{
    auto en = [this] (const String& id, bool on)
    {
        auto range = byId.equal_range (id);
        for (auto it = range.first; it != range.second; ++it)
            if (it->second->isEnabled() != on) { it->second->setEnabled (on); it->second->setAlpha (on ? 1.f : 0.35f); }
    };
    const bool stable = (int) raw (proc, P::peMode) == 5;
    for (auto* id : { "pe_start", "pe_time", "pe_curve", "pe_amount" }) en (id, ! stable);
    en ("tail_hold_level", raw (proc, P::tailPOffset) > 0.5f);        // TOK LEVEL only acts during the HOLD
    en ("sub_pitch", raw (proc, P::coreKeyTrack) < 0.5f);          // with tracking the sub sits on the note
    en ("core_length", (int) raw (proc, P::lenSync) == 0);             // module 14: LENGTH SYNC decides the length
    en ("tail_pulse_width", (int) raw (proc, P::tailSrc) == 4);
    en ("punch_tone", (int) raw (proc, P::punchSrc) != 5);           // CLICK tunes its own band
    const bool uni = raw (proc, P::tailUniVoices) > 1.5f;
    for (auto* id : { "tail_uni_detune", "tail_uni_drift", "tail_uni_mix", "tail_width" }) en (id, uni);
    en ("tail_width_from", uni && raw (proc, P::tailWidth) > 0.001f);
    const bool st2 = (int) raw (proc, P::tailSt2Type) > 0;
    for (auto* id : { "tail_st2_drive", "tail_st2_bias", "tail_st2_mix" }) en (id, st2);
    const bool mid = std::abs (raw (proc, P::tailMidGain)) > 0.05f;
    en ("tail_mid_hz", mid); en ("tail_mid_q", mid);
    en ("master_clip_soft", raw (proc, P::clipDrive) > 0.01f);
}

void GeCanvas::timerCallback()
{
    presetBar.refresh();
    styleStrip.setLit (proc.currentStyle());                                       // module 13: the STYLE shown (undo / presets change it too)
    {
        const auto f = proc.frame();                                               // module 14: the length category (none while SYNC decides)
        const float ms = kickLengthMs (f, proc.hostBpm());
        lengthStrip.setLit (f.idx (P::lenSync) > 0 ? -1 : fge::length::category (ms));
        lengthBtn.setButtonText (fge::length::label (proc, proc.hostBpm()) + "  ...");
    }
    // module 17: the INFO LINE + the navigation dots
    {
        String t;
        if (Time::getMillisecondCounter() - styleMsgTime < 4000 && styleMsg.isNotEmpty()) t = styleMsg;
        else
        {
            auto* under = Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
            if (under != nullptr && (under == this || isParentOf (under))) t = infoFor (under);
            if (t.isEmpty()) t = fge::gui::NavBar::blockOf (pageIdx) + "  >  " + fge::gui::NavBar::pageName (pageIdx) + "   -   " + fge::gui::NavBar::pageTip (pageIdx);
        }
        if (t != infoText) { infoText = t; repaint (infoArea); }
        nav.refresh();
    }
    const auto tb = proc.currentTables();
    {
        fge::gui::WaveView::Samples ss;
        for (int k = 0; k < fge::dsp::numSampleSlots; ++k) ss[(size_t) k] = proc.samples().data (k);
        wave.setSamples (ss, proc.samples().version());
    }
    wave.update (proc.frame(), proc.auditionNote, &proc.shapes(), proc.shapesVersion(), &tb, &proc.distData(), proc.distDataVersion(), &proc.modAudioCopy(), proc.modVersion());
    updateModRings();
    auto& e = proc.getEngine();
    for (int i = 0; i < 4; ++i) meter[(size_t) i] = jmax (e.partPeak[(size_t) i].exchange (0.f), meter[(size_t) i] * 0.85f);
    outMeter = jmax (e.outPeak.exchange (0.f), outMeter * 0.85f);
    updateEnabled();
    repaint (meterArea.expanded (4));
    if (pageIdx == 0) repaint (mixerArea);
    if (pageIdx == 0) for (auto& a : cardArea) repaint (a.withTop (a.getBottom() - 14));
}

// ======================================================================= editor
// ---------------------------------------------------------------- module 09: modulation from the knobs
int GeCanvas::addModulation (const String& paramId, int source)
{
    namespace mo = fge::dsp::mod;
    const int id = fge::modstate::indexOf (paramId);
    if (! mo::modulatable (id) || source <= mo::SNone || source >= mo::NumSrc) return -1;
    for (int s = 0; s < mo::numSlots; ++s)
        if (proc.modDesc().slot[(size_t) s].empty())
        {
            fge::modstate::SlotDesc d; d.on = true; d.src = source; d.dst = paramId; d.depth = 0.25f;
            d.pol = (mo::isMacro (source) || source == mo::SVel || (source >= mo::SAmpPunch && source <= mo::SAmpKick) || source >= mo::SFollower) ? mo::PUniPos : mo::PBipolar;
            d.smoothMs = mo::isMacro (source) ? 20.f : 0.f;
            d.label = mo::sourceNames()[source] + " > " + String (fge::def (id).name).toUpperCase();
            proc.setModSlot (s, d);
            if (modPg != nullptr) modPg->selectSlot (s);
            return s;
        }
    return -1;
}
void GeCanvas::knobMenu (doomsday::flat::Knob& k)
{
    namespace mo = fge::dsp::mod;
    const String pid = k.parameterId();
    const int id = fge::modstate::indexOf (pid);
    PopupMenu m;
    knobValueMenu (k, m);                                                            // module 17: reset / copy / paste / type
    m.addSeparator();
    if (! mo::modulatable (id)) { m.addItem (-1, "(this control is not a modulation target)", false); m.showMenuAsync (PopupMenu::Options().withTargetComponent (&k)); return; }
    PopupMenu with;
    for (int s = 1; s < mo::NumSrc; ++s) with.addItem (s, mo::sourceNames()[s]);
    m.addSubMenu ("MODULATE " + String (fge::def (id).name).toUpperCase() + " WITH", with);
    std::vector<int> mine;
    for (int s = 0; s < mo::numSlots; ++s) { const auto& d = proc.modDesc().slot[(size_t) s]; if (d.dst == pid && ! d.empty()) mine.push_back (s); }
    if (! mine.empty())
    {
        m.addSeparator();
        for (int s : mine)
        {
            const auto& d = proc.modDesc().slot[(size_t) s];
            m.addItem (1000 + s, "REMOVE slot " + String (s + 1) + ": " + mo::sourceNames()[d.src] + " " + (d.depth >= 0 ? "+" : "") + String (roundToInt (d.depth * 100.f)) + " %");
        }
        m.addItem (999, "SHOW IN THE MATRIX");
    }
    SafePointer<GeCanvas> self (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&k), [self, pid, mine] (int r)
    {
        if (self == nullptr || r <= 0) return;
        if (r < 1000 && r != 999) { self->addModulation (pid, r); return; }
        if (r == 999) { self->showPage (8); self->modPg->setView ((mine.front() / 16) * 16); self->modPg->selectSlot (mine.front()); return; }
        if (r >= 2000) return;
        self->proc.setModSlot (r - 1000, {});
    });
}
void GeCanvas::updateModRings()
{
    namespace mo = fge::dsp::mod;
    std::map<String, std::pair<float, int>> mod;                      // target -> (sum of depths, a slot)
    for (int s = 0; s < mo::numSlots; ++s)
    {
        const auto& d = proc.modDesc().slot[(size_t) s];
        if (! d.on || d.mute || d.dst.isEmpty() || d.src <= 0) continue;
        auto& e = mod[d.dst]; e.first += d.depth; e.second = s;
    }
    std::function<void (Component&)> walk = [&] (Component& c)
    {
        for (auto* ch : c.getChildren())
        {
            if (! ch->isVisible()) continue;
            if (auto* k = dynamic_cast<Knob*> (ch))
            {
                const auto it = mod.find (k->parameterId());
                if (it == mod.end()) k->setModulation (0.f);
                else k->setModulation (jlimit (-1.f, 1.f, it->second.first), proc.getEngine().slotFinal[(size_t) it->second.second].load());
                continue;
            }
            walk (*ch);
        }
    };
    walk (*this);
}
int GeCanvas::dropOnto (Point<int> pos, int source)
{
    for (auto* c = getComponentAt (pos); c != nullptr && c != this; c = c->getParentComponent())
        if (auto* k = dynamic_cast<Knob*> (c)) return addModulation (k->parameterId(), source);
    return -1;
}
void GeCanvas::itemDropped (const SourceDetails& d) { dropOnto (d.localPosition, d.description.toString().fromFirstOccurrenceOf ("modsrc:", false, false).getIntValue()); }
void GeCanvas::mouseDrag (const MouseEvent& e)
{
    // the small handles next to the macro knobs (bottom bar, every page): drag a macro onto any knob
    if (isDragAndDropActive() || ! macroArea.contains (e.getMouseDownPosition())) return;
    for (size_t i = 0; i < macroRow.size(); ++i)
        if (macroRow[i]->getBounds().withX (macroRow[i]->getRight()).withWidth (22).contains (e.getMouseDownPosition()))
            startDragging ("modsrc:" + String (fge::dsp::mod::SMacro1 + (int) i), this);
}

GeEditor::GeEditor (GeProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    doomsday::flat::Knob::onContextMenu = [] (doomsday::flat::Knob& k, const MouseEvent&)
    { if (auto* cv = k.findParentComponentOfClass<GeCanvas>()) cv->knobMenu (k); };
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (GeCanvas::W * 6 / 10, GeCanvas::H * 6 / 10, GeCanvas::W * 16 / 10, GeCanvas::H * 16 / 10);
    getConstrainer()->setFixedAspectRatio ((double) GeCanvas::W / (double) GeCanvas::H);
    canvas.onSettingsChanged = [this] { applyScale(); };
    applyScale();
}
// module 17: the size = SETTINGS (FIT = the largest step that fits the screen); dragging the corner remembers the new size
float GeEditor::wantedScale() const
{
    Rectangle<int> area;
    if (auto* d = Desktop::getInstance().getDisplays().getPrimaryDisplay()) area = d->userArea;
    return canvas.uiSettings().resolve (area, GeCanvas::W, GeCanvas::H);
}
void GeEditor::applyScale()
{
    const float s = wantedScale();
    ownResize = true;
    setSize (roundToInt (GeCanvas::W * s), roundToInt (GeCanvas::H * s));
    ownResize = false;
}
void GeEditor::resized()
{
    canvas.setBounds (0, 0, GeCanvas::W, GeCanvas::H);
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) GeCanvas::W));
    if (! ownResize && isShowing())                                           // the user dragged the corner
    {
        const float s = jlimit (0.6f, 1.6f, (float) getWidth() / (float) GeCanvas::W);
        if (std::abs (s - wantedScale()) > 0.005f) { canvas.uiSettings().scale = s; saveIn = 15; startTimer (40); }
    }
}
void GeEditor::timerCallback()
{
    if (--saveIn <= 0) { stopTimer(); canvas.uiSettings().save(); }
}

// ------------------------------------------------------------ module 13: STYLE ENGINE in the header
void GeCanvas::chooseStyle (int st, bool keepMine)
{
    if (! isPositiveAndBelow (st, (int) fge::style::NumStyles)) return;
    const auto r = fge::style::apply (proc, st, keepMine);
    proc.apvts.state.setProperty ("vrStyle", st, nullptr);                        // RANDOMIZE stays in the chosen style
    styleMsg = fge::style::names()[st] + ": " + String (r.changed) + " settings set" + (r.kept > 0 ? ", " + String (r.kept) + " of your changes kept" : String());
    styleMsgTime = Time::getMillisecondCounter();
    if (isShowing())
    {
        auto* bubble = new BubbleMessageComponent (300);
        addChildComponent (bubble);
        AttributedString t; t.append (styleMsg, FontOptions (13.f), palette().silver);
        bubble->showAt (&styleStrip, t, 3500, true, true);
    }
    styleShown = -2;
    repaint();
}
void GeCanvas::previewStyle (int st)
{
    auto snap = proc.exportSnapshot();
    GeProcessor* pr = &proc;
    snap.f = fge::style::onFrame (snap.f, st, [pr] (int i) { return pr->isTouched (i); });
    fge::dsp::xport::Spec spec; spec.note = proc.auditionNote; spec.rate = 44100.0; spec.seed = snap.presetSeed;
    const double host = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    auto life = alive;
    styleMsg = "PREVIEW " + fge::style::names()[st] + " (nothing changed)"; styleMsgTime = Time::getMillisecondCounter(); repaint();
    Thread::launch ([snap, spec, host, life, pr]
    {
        auto r = fge::dsp::xport::render (snap, spec);
        if (*life && ! r.cancelled) pr->samples().previewAudio (r.audio, spec.rate, host);
    });
}
void GeCanvas::styleMenu()
{
    const int cur = proc.currentStyle();
    // the result ids: style * 10 + action
    PopupMenu menu;
    for (int s = 0; s < fge::style::NumStyles; ++s)
    {
        PopupMenu sub;
        sub.addItem (s * 10 + 1, "APPLY (keep my " + String (proc.touchedCount()) + " changes)");
        sub.addItem (s * 10 + 2, "APPLY ALL (reset my changes too)");
        sub.addItem (s * 10 + 3, "PREVIEW (listen, change nothing)");
        sub.addSeparator();
        sub.addItem (99, fge::style::description (s), false);
        menu.addSubMenu (fge::style::names()[s] + (s == cur ? "  (current)" : ""), sub);
    }
    menu.addSeparator();
    menu.addItem (500, "FORGET MY CHANGES (" + String (proc.touchedCount()) + " remembered)", proc.touchedCount() > 0);
    SafePointer<GeCanvas> self (this);
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&styleStrip), [self] (int r)
    {
        if (self == nullptr || r <= 0) return;
        if (r == 500) { self->proc.forgetTouched(); self->styleMsg = "your changes are forgotten - the next style sets everything"; self->styleMsgTime = Time::getMillisecondCounter(); self->repaint(); return; }
        const int st = r / 10, act = r % 10;
        if (act == 1) self->chooseStyle (st, true);
        else if (act == 2) self->chooseStyle (st, false);
        else if (act == 3) self->previewStyle (st);
    });
}

// ------------------------------------------------------------ module 14: the LENGTH panel (a call-out from the header)
namespace
{
    class LengthPanel : public Component
    {
    public:
        LengthPanel (GeProcessor& p, doomsday::flat::FlatLookAndFeel& l) : proc (p)
        {
            setLookAndFeel (&l);
            const auto& c = palette();
            for (int i = 0; i < fge::length::NumProfiles; ++i)
            {
                auto& b = prof[(size_t) i];
                b.setButtonText ((i == fge::length::ExtraLong ? String ("X-LONG") : fge::length::names()[i]) + "  " + String (roundToInt (fge::length::profileMs (i))));
                b.setTooltip ("Set the LENGTH to " + String (roundToInt (fge::length::profileMs (i))) + " ms with AUTO LENGTH (the parts scale with it).");
                b.onClick = [this, i] { fge::length::applyProfile (proc, i); };
                addAndMakeVisible (b);
            }
            sync = std::make_unique<doomsday::flat::Segmented> (proc.apvts, "len_sync", StringArray { "OFF", "1/4", "1/2", "1 BEAT", "2 BEATS" }, c.red);
            sync->setTooltip (fge::tooltip (P::lenSync)); addAndMakeVisible (*sync);
            autoT = std::make_unique<doomsday::flat::Toggle> (proc.apvts, "core_auto_length", "AUTO LENGTH", doomsday::flat::Toggle::Pill, c.red);
            autoT->setTooltip (fge::tooltip (P::coreAutoLen)); addAndMakeVisible (*autoT);
            lenK = std::make_unique<doomsday::flat::Knob> (proc.apvts, "core_length", "LENGTH", c.red, false, 40);
            lenK->slider.setTooltip (fge::tooltip (P::coreLength)); addAndMakeVisible (*lenK);
            const char* fid[4] { "len_follow_punch", "len_follow_body", "len_follow_tail", "len_follow_sub" };
            const char* lid[4] { "punch_length", "body_length", "tail_length", "sub_length" };
            for (int i = 0; i < 4; ++i)
            {
                fol[(size_t) i] = std::make_unique<doomsday::flat::Knob> (proc.apvts, fid[i], "FOLLOW", c.part[(size_t) i], false, 32);
                fol[(size_t) i]->slider.setTooltip (fge::tooltip (P::lenFollowPunch + i)); addAndMakeVisible (*fol[(size_t) i]);
                plen[(size_t) i] = std::make_unique<doomsday::flat::Knob> (proc.apvts, lid[i], "LENGTH", c.part[(size_t) i], false, 32);
                for (int k = 0; k < P::count; ++k) if (String (def (k).id) == lid[i]) plen[(size_t) i]->slider.setTooltip (fge::tooltip (k));
                addAndMakeVisible (*plen[(size_t) i]);
            }
            setSize (520, 330);
        }
        ~LengthPanel() override { setLookAndFeel (nullptr); }
        void paint (Graphics& g) override
        {
            const auto& c = palette();
            g.fillAll (c.panel);
            g.setColour (c.textDim); g.setFont (FontOptions (11.f, Font::bold));
            g.drawText ("KICK LENGTH  -  independent of the STYLE", 12, 6, 400, 14, Justification::left);
            g.drawText ("LENGTH SYNC (to the tempo)", 12, 112, 300, 14, Justification::left);
            g.drawText ("EACH PART: how much it follows LENGTH  /  its own length", 12, 176, 400, 14, Justification::left);
            const char* nm[4] { "PUNCH", "BODY", "TAIL", "SUB" };
            for (int i = 0; i < 4; ++i) { g.setColour (palette().part[(size_t) i]); g.drawText (nm[i], 12 + i * 125, 194, 120, 14, Justification::centred); }
            g.setColour (c.textDim); g.setFont (FontOptions (10.f));
            g.drawText ("KICK END (header): LENGTH / NOTE / +FX - the piano-roll note can end the kick", 12, getHeight() - 18, 500, 14, Justification::left);
        }
        void resized() override
        {
            for (int i = 0; i < 4; ++i) prof[(size_t) i].setBounds (12 + i * 92, 26, 86, 40);
            autoT->setBounds (12, 76, 150, 22);
            lenK->setBounds (400, 20, 100, doomsday::flat::Knob::heightFor (40));
            sync->setBounds (12, 130, 380, 26);
            for (int i = 0; i < 4; ++i)
            {
                fol[(size_t) i]->setBounds (12 + i * 125, 210, 60, doomsday::flat::Knob::heightFor (32));
                plen[(size_t) i]->setBounds (72 + i * 125, 210, 60, doomsday::flat::Knob::heightFor (32));
            }
        }
        GeProcessor& proc;
        std::array<TextButton, 4> prof;
        std::unique_ptr<doomsday::flat::Segmented> sync;
        std::unique_ptr<doomsday::flat::Toggle> autoT;
        std::unique_ptr<doomsday::flat::Knob> lenK;
        std::array<std::unique_ptr<doomsday::flat::Knob>, 4> fol, plen;
    };
}
Component* GeCanvas::openLengthPanel()
{
    auto panel = std::make_unique<LengthPanel> (proc, lnf);
    auto* raw = panel.get();
    if (isShowing()) lengthBox = &CallOutBox::launchAsynchronously (std::move (panel), lengthBtn.getScreenBounds(), nullptr);
    else { owned.push_back (std::move (panel)); raw->setBounds (0, 0, 520, 330); }
    return raw;
}

// ======================================================================= module 17: GUI & UX
void GeCanvas::applyTooltips()
{
    if (uiSet.tooltips && tips == nullptr) tips = std::make_unique<TooltipWindow> (this, 600);
    if (! uiSet.tooltips) tips.reset();
    repaint();
}

// the INFO LINE: what is under the mouse - a parameter (name, value, range, default, modulation, "yours") or the control's tooltip
String GeCanvas::infoFor (Component* comp)
{
    namespace mo = fge::dsp::mod;
    auto paramInfo = [this] (const String& pid) -> String
    {
        const int idx = fge::modstate::indexOf (pid);
        if (idx < 0) return {};
        const auto& d = def (idx);
        const float v = raw (proc, idx);
        String t = String (d.name).toUpperCase() + " = " + formatValue (d, v);
        if (d.kind == Kind::Float) t << "   |   " << formatValue (d, d.min) << " .. " << formatValue (d, d.max) << ",  default " << formatValue (d, d.def);
        else if (d.kind == Kind::Choice && d.choices != nullptr) t << "   |   " << d.choices().joinIntoString (" / ");
        else t << "   |   default " << (d.def > 0.5f ? "ON" : "OFF");
        int mods = 0; for (auto& sl : proc.modDesc().slot) mods += (! sl.empty() && sl.on && sl.dst == pid) ? 1 : 0;
        if (mods > 0) t << "   |   MODULATED (" << mods << ")";
        if (proc.isTouched (idx)) t << "   |   YOURS (a STYLE keeps it)";
        const String tip = fge::tooltip (idx);
        if (tip.isNotEmpty()) t << "   -   " << tip;
        return t;
    };
    for (auto* c = comp; c != nullptr && c != this; c = c->getParentComponent())
    {
        if (auto* k = dynamic_cast<Knob*> (c)) return paramInfo (k->parameterId());
        for (auto& [id, cc] : byId) if (cc == c) return paramInfo (id);
        if (auto* tc = dynamic_cast<TooltipClient*> (c)) { const String t = tc->getTooltip(); if (t.isNotEmpty()) return t; }
    }
    return {};
}

// the navigation dots: this page changes the sound now
bool GeCanvas::pageActive (int page)
{
    namespace mo = fge::dsp::mod;
    if (page == 9) { for (int k = 0; k < fge::dsp::numSampleSlots; ++k) if (proc.sampleInfo (k).status != fge::smp::SEmpty) return true; return false; }
    if (page == 8)
    {
        for (auto& sl : proc.modDesc().slot)
        {
            if (sl.empty() || ! sl.on || sl.mute || sl.dst.isEmpty()) continue;
            if (! mo::isMacro (sl.src)) return true;
            for (auto* m : macroRow) if (auto* k = dynamic_cast<Knob*> (m)) if (proc.apvts.getRawParameterValue (k->parameterId())->load() > 0.001f) return true;
        }
        return false;
    }
    for (auto& [pg, idx] : activeParams) if (pg == page && raw (proc, idx) > 0.5f) return true;
    return false;
}

// right-click on any knob (all pages): the value
void GeCanvas::knobValueMenu (Knob& k, PopupMenu& m)
{
    const String pid = k.parameterId();
    auto* prm = proc.apvts.getParameter (pid);
    if (prm == nullptr) return;
    auto set = [prm] (float plain) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (plain)); prm->endChangeGesture(); };
    const float now = prm->convertFrom0to1 (prm->getValue()), defv = prm->convertFrom0to1 (prm->getDefaultValue());
    const int idx = fge::modstate::indexOf (pid);
    const String txt = idx >= 0 ? formatValue (def (idx), defv) : String (defv);
    m.addSectionHeader (idx >= 0 ? String (def (idx).name).toUpperCase() : pid);
    m.addItem ("RESET TO DEFAULT (" + txt + ")", std::abs (now - defv) > 1.0e-6f, false, [set, defv] { set (defv); });
    m.addItem ("TYPE A VALUE...", [sp = Component::SafePointer<Slider> (&k.slider)] { if (sp != nullptr) sp->showTextBox(); });
    m.addItem ("COPY VALUE", [now, pid] { SystemClipboard::copyTextToClipboard ("4GE:" + pid + "=" + String (now, 6)); });
    const String clip = SystemClipboard::getTextFromClipboard();
    const bool canPaste = clip.startsWith ("4GE:") && clip.contains ("=");
    const float pv = clip.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
    m.addItem ("PASTE VALUE" + (canPaste ? " (" + (idx >= 0 ? formatValue (def (idx), pv) : String (pv, 2)) + ")" : String()),
               canPaste, false, [set, clip, prm]
               {
                   const auto& r = prm->getNormalisableRange();
                   set (jlimit (r.start, r.end, clip.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
               });
}

// a file dropped anywhere (pages with their own drop - SAMPLES slots, ANALYZE - take it first)
bool GeCanvas::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;4gp")) return true;
    return false;
}
void GeCanvas::filesDropped (const StringArray& files, int, int)
{
    File f; for (auto& x : files) if (File (x).hasFileExtension ("wav;aif;aiff;flac;mp3;4gp")) { f = File (x); break; }
    if (! f.existsAsFile()) return;
    if (f.hasFileExtension ("4gp")) { dropFile (f, 3); return; }
    PopupMenu m;
    m.addSectionHeader (f.getFileName());
    m.addItem (1, "LOAD INTO THE NEXT FREE SAMPLE SLOT (a layer of the kick)");
    m.addItem (2, "USE AS THE ANALYZE REFERENCE (compare, get suggestions)");
    SafePointer<GeCanvas> self (this);
    m.showMenuAsync (PopupMenu::Options().withMousePosition(), [self, f] (int r) { if (self != nullptr && r > 0) self->dropFile (f, r); });
}
void GeCanvas::dropFile (const File& f, int choice)
{
    if (choice == 3) { if (proc.loadUserPresetFile (f)) { proc.setPresetName (f.getFileNameWithoutExtension()); styleMsg = "preset loaded: " + f.getFileNameWithoutExtension(); styleMsgTime = Time::getMillisecondCounter(); } return; }
    if (choice == 2) { showPage (13); anPg->loadReference (f); return; }
    int slot = -1;
    for (int k = 0; k < fge::dsp::numSampleSlots && slot < 0; ++k) if (proc.sampleInfo (k).status == fge::smp::SEmpty) slot = k;
    if (slot < 0) { styleMsg = "all 8 sample slots are used - clear one on the SAMPLES page"; styleMsgTime = Time::getMillisecondCounter(); showPage (9); return; }
    proc.importSample (slot, f);
    showPage (9);
    smpPg->selectSlot (slot);
    styleMsg = f.getFileName() + " -> SAMPLE SLOT " + String (slot + 1) + " (layer " + String (9 + slot) + ")"; styleMsgTime = Time::getMillisecondCounter();
}

// shortcuts: Ctrl+Z / Ctrl+Y (Ctrl+Shift+Z), Ctrl+left / right = the page before / after
bool GeCanvas::keyPressed (const KeyPress& k)
{
    const bool cmd = k.getModifiers().isCommandDown();
    if (cmd && k.getKeyCode() == 'Z' && ! k.getModifiers().isShiftDown()) { proc.undo.undo(); return true; }
    if (cmd && (k.getKeyCode() == 'Y' || (k.getKeyCode() == 'Z' && k.getModifiers().isShiftDown()))) { proc.undo.redo(); return true; }
    if (cmd && k.getKeyCode() == KeyPress::leftKey) { stepPage (-1); return true; }
    if (cmd && k.getKeyCode() == KeyPress::rightKey) { stepPage (1); return true; }
    return false;
}
void GeCanvas::stepPage (int dir)
{
    const auto o = fge::gui::NavBar::order();
    int at = 0; for (int i = 0; i < (int) o.size(); ++i) if (o[(size_t) i] == pageIdx) at = i;
    showPage (o[(size_t) ((at + dir + (int) o.size()) % (int) o.size())]);
}
