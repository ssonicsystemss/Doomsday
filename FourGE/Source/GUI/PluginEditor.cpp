#include "PluginEditor.h"
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
    // module 13: STYLE (always visible)
    for (int st = 0; st < fge::style::NumStyles; ++st) styleBox.addItem (fge::style::names()[st], st + 1);
    styleBox.setTextWhenNothingSelected ("STYLE ...");
    styleBox.setTooltip ("STYLE: EARLY / MODERN / RAW / EXPERIMENTAL - a starting point for the character (generators, pitch drop, distortion chain, screech). "
                         "It never changes the LENGTH and never locks a knob; your own changes are kept. Right-click: APPLY ALL / PREVIEW / FORGET MY CHANGES.");
    styleBox.onChange = [this] { const int id = styleBox.getSelectedId(); if (id > 0 && id - 1 != proc.currentStyle()) chooseStyle (id - 1, true); };
    styleBox.addMouseListener (this, false);
    addAndMakeVisible (styleBox);
    lengthBtn.setTooltip ("LENGTH: SHORT / MEDIUM / LONG / EXTRA LONG, LENGTH SYNC to the tempo, AUTO LENGTH and how much PUNCH / BODY / TAIL / SUB follow - "
                          "independent of the STYLE.");
    lengthBtn.onClick = [this] { openLengthPanel(); };
    addAndMakeVisible (lengthBtn);
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
    anaBtn.onClick = [this] { showPage (13); };
    anaBtn.setTooltip ("Kick analysis: load a reference kick, compare it with 4GE (waveform, pitch, spectrum, bands, transient, length, stereo, loudness) and apply the suggested settings (one undo step).");
    addAndMakeVisible (anaBtn);
    presetBar.onBrowse = [this] { if (pageIdx != 12) pageBeforePresets = pageIdx; showPage (pageIdx == 12 ? pageBeforePresets : 12); };
    const char* pages[12] { "MAIN", "ENV", "OSC", "LAYERS", "DIST", "SCREECH", "EQ", "DYN", "MOD", "SMP", "STEREO", "EXPORT" };
    for (int i = 0; i < 12; ++i)
    {
        pageBtn[(size_t) i].setButtonText (pages[i]);
        pageBtn[(size_t) i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (pageBtn[(size_t) i]);
    }
    pageBtn[0].setTooltip ("The kick: parts, sources, mixer.");
    pageBtn[1].setTooltip ("Draw the pitch / amplitude / filter / distortion / resonance envelope of every part.");
    pageBtn[2].setTooltip ("Four extra oscillators (waveforms, wavetables, harmonics, unison, FM, ring, sync) routed into the parts.");
    pageBtn[3].setTooltip ("Noise generators, the transient engine and 8 layers with groups and routing.");
    pageBtn[4].setTooltip ("16 distortion stages: 11 types, serial / parallel / multiband chains on the whole kick, a part, the texture or a layer; pre / post filters, dynamic modulation, visualizer.");
    wave.onScreechStart = [this] (float ms, bool first) { if (first) proc.undo.beginNewTransaction ("Screech start"); proc.setParamInUndoStep ("scr_start", ms); };
    pageBtn[5].setTooltip ("Filters (8), resonators (2) and the SCREECH engine: synthesised screeches with their own envelopes and timing.");
    pageBtn[6].setTooltip ("EQ (16 bands), dynamic EQ, multiband, spectral shaper and stereo engine - on the whole kick, a part, the texture, a layer or the screech.");
    pageBtn[8].setTooltip ("Modulation: the matrix (64 slots), LFO 1-8, the step sequencer, random sources and the MACRO MAPPING of the 8 MASTER MACROS.");
    pageBtn[11].setTooltip ("Master output (level, mute, mono, safety), master meters (peak, true peak, RMS, LUFS, DC) and EXPORT: WAV / AIFF / FLAC, groups / layers, length, normalization, seeds, batch, history.");
    pageBtn[10].setTooltip ("Stereo & space: PAN / BALANCE / WIDTH (M/S, decorrelation, Haas), low mono, M/S, polarity, channel delay per group and on the master; SUB MONO LOCK, MONO AUDITION, vector scope and correlation.");
    pageBtn[9].setTooltip ("Samples: 8 slots (WAV / AIFF / FLAC), the waveform editor (start / end / loop / fades / reverse), the browser - each slot is one more layer of the kick.");
    pageBtn[7].setTooltip ("Dynamics: 4 compressors, 5 transient shapers, 2 clippers, 2 limiters, crest factor control, auto gain - where the punch and the loudness are made.");
    selectPart (2);
    showPage (0);
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
    pageIdx = jlimit (0, 13, i);
    const bool main = pageIdx == 0;
    for (int p = 0; p < 4; ++p)
    {
        for (auto* c : card[(size_t) p]) c->setVisible (main);
        for (auto* c : mixer[(size_t) p]) c->setVisible (main);
        for (auto* c : detail[(size_t) p]) if (c != nullptr) c->setVisible (main && p == selected);
    }
    for (auto* c : masterRow) c->setVisible (main);
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
    wave.setVisible (pageIdx < 2);
    for (auto* m : macroRow) m->setVisible (pageIdx < 2);
    const auto& c = palette();
    for (int k = 0; k < 12; ++k)
    {
        pageBtn[(size_t) k].setColour (TextButton::buttonColourId, k == pageIdx ? c.red : c.control);
        pageBtn[(size_t) k].setColour (TextButton::textColourOffId, k == pageIdx ? c.ground : c.silver);
    }
    anaBtn.setColour (TextButton::buttonColourId, pageIdx == 13 ? c.red : c.control);
    anaBtn.setColour (TextButton::textColourOffId, pageIdx == 13 ? c.ground : c.silver);
    repaint();
}

void GeCanvas::mouseDown (const MouseEvent& e)
{
    if (e.eventComponent == &styleBox) { if (e.mods.isPopupMenu()) styleMenu(); return; }
    if (pageIdx != 0) return;
    for (int i = 0; i < 4; ++i)
        if (cardArea[(size_t) i].contains (e.getPosition())) { selectPart (i); return; }
}

void GeCanvas::resized()
{
    headerArea = { 0, 0, W, 100 };
    presetBar.setBounds (W - 16 - 300, 5, 300, 90);
    styleBox.setBounds ((int) doomsday::brand::headerTextX (1.f) + 40, 61, 150, 19);
    lengthBtn.setBounds ((int) doomsday::brand::headerTextX (1.f) + 40, 82, 150, 19);
    // header: LENGTH / MASTER / VELOCITY, the three switches, VOICE
    int x = 316;
    for (int i = 0; i < 3; ++i) { header[(size_t) i]->setBounds (x, 10, 68, Knob::heightFor (44)); x += 70; }
    for (int i = 0; i < 3; ++i) header[(size_t) 3 + (size_t) i]->setBounds (x + 10, 8 + i * 23, 146, 20);
    header[7]->setBounds (x + 10, 8 + 3 * 23, 146, 20);
    // the page buttons: a 5 x 3 grid (row 1: VOICE + STEREO, rows 2 - 3: the pages in module order)
    const int gx = 692, gw = 52, gs = 54;
    header[6]->setBounds (gx, 8, gw + gs, 26);
    const int order[10] { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    for (int k = 0; k < 10; ++k) pageBtn[(size_t) order[k]].setBounds (gx + (k % 5) * gs, 40 + (k / 5) * 30, gw, 26);
    pageBtn[10].setBounds (gx + 2 * gs, 8, gw, 26);
    anaBtn.setBounds (gx + 3 * gs, 8, gw, 26);
    pageBtn[11].setBounds (gx + 4 * gs, 8, gw, 26);
    // pitch engine
    pitchArea = { 16, 104, W - 32, 84 };
    pitchRow[0]->setBounds (pitchArea.getX() + 14, pitchArea.getY() + 40, 520, 28);
    for (int i = 1; i < 9; ++i) pitchRow[(size_t) i]->setBounds (pitchArea.getX() + 570 + (i - 1) * 80, pitchArea.getY() + 4, 76, Knob::heightFor (38));
    // waveform + output meter
    waveArea = { 16, 196, W - 32 - 34, 152 };
    macroArea = { 16, 354, W - 32, 70 };
    for (size_t i = 0; i < macroRow.size(); ++i) macroRow[i]->setBounds (macroArea.getX() + 160 + (int) i * 130, macroArea.getY() + 2, 100, Knob::heightFor (30));
    wave.setBounds (waveArea);
    meterArea = { waveArea.getRight() + 10, waveArea.getY(), 24, waveArea.getHeight() };
    // part cards
    const int cw = (W - 32 - 3 * 12) / 4;
    for (int i = 0; i < 4; ++i)
    {
        cardArea[(size_t) i] = { 16 + i * (cw + 12), 430, cw, 160 };
        const auto a = cardArea[(size_t) i];
        for (int k = 0; k < 4; ++k) card[(size_t) i][(size_t) k]->setBounds (a.getX() + 8 + k * 72, a.getY() + 28, 70, Knob::heightFor (40));
        card[(size_t) i][4]->setBounds (a.getRight() - 136, a.getY() + 4, 60, 20);
        card[(size_t) i][5]->setBounds (a.getRight() - 70, a.getY() + 4, 60, 20);
    }
    detailArea = { 16, 600, 860, 314 };
    mixerArea = { detailArea.getRight() + 12, 600, W - 16 - detailArea.getRight() - 12, 314 };
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
    if (envPage != nullptr) envPage->setBounds (16, 430, W - 32, H - 430 - 16);
    if (oscPage != nullptr) oscPage->setBounds (16, 196, W - 32 - 34, H - 196 - 16);
    if (layerPage != nullptr) layerPage->setBounds (16, 196, W - 32 - 34, H - 196 - 16);
    if (distortionPage != nullptr) distortionPage->setBounds (16, 196, W - 32 - 34, H - 196 - 16);
    if (eqPg != nullptr) eqPg->setBounds (16, 196, W - 32 - 34, H - 196 - 16);
    if (dynPg != nullptr) dynPg->setBounds (16, 196, W - 32 - 34, H - 196 - 16);
    if (modPg != nullptr) modPg->setBounds (16, 196, W - 32 - 34, H - 196 - 16);
    if (smpPg != nullptr) smpPg->setBounds (16, 196, W - 32 - 34, H - 196 - 16);
    if (stPg != nullptr) stPg->setBounds (16, 196, W - 32 - 34, H - 196 - 16);
    if (exPg != nullptr) exPg->setBounds (16, 196, W - 32 - 34, H - 196 - 16);
    if (prPg != nullptr) prPg->setBounds (16, 104, W - 32 - 34, H - 104 - 16);
    if (anPg != nullptr) anPg->setBounds (16, 104, W - 32 - 34, H - 104 - 16);
    if (scrPage != nullptr) scrPage->setBounds (16, 196, W - 32 - 34, H - 196 - 16);
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
    // header
    doomsday::brand::drawHeaderBadge (g, 1.f, 0.55f);
    const float tx = doomsday::brand::headerTextX (1.f);
    g.setColour (c.silver); g.setFont (lnf.font (30.f, doomsday::flat::Bold, 0.12f));
    g.drawText ("4GE", (int) tx, 14, 200, 34, Justification::left);
    g.setColour (c.red); g.setFont (lnf.font (11.f, doomsday::flat::Bold, 0.18f));
    g.drawText ("HARDSTYLE KICK DESIGNER", (int) tx, 50, 260, 16, Justification::left);
    g.setColour (c.textDim); g.setFont (lnf.font (11.f, doomsday::flat::Medium));
    g.drawText ("v" + String (versionText), (int) tx, 68, 40, 16, Justification::left);

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
    panel (pitchArea, "PITCH ENGINE", c.part[2]);
    g.setColour (c.textDim); g.setFont (lnf.font (10.5f, doomsday::flat::Medium));
    const bool track = raw (proc, P::coreKeyTrack) > 0.5f;
    g.drawText (track ? "TRACKING: END = the played note, START moves with it" : "FIXED: START -> END in Hz",
                pitchArea.getX() + 140, pitchArea.getY() + 8, 400, 20, Justification::left);
    if (pageIdx < 2)
    {
        panel (macroArea, "MASTER MACROS", c.red);
        g.setColour (c.textDim); g.setFont (lnf.font (9.5f, doomsday::flat::Medium));
        g.drawFittedText ("drag  >  onto any knob\nright-click a knob = modulate", macroArea.getX() + 14, macroArea.getY() + 30, 140, 30, Justification::topLeft, 2);
        for (auto* m : macroRow)
        {
            const auto h = Rectangle<float> ((float) m->getRight() + 4.f, (float) m->getY() + 8.f, 14.f, 14.f);
            g.setColour (c.control); g.fillRoundedRectangle (h, 3.f);
            g.setColour (c.red); g.drawText (">", h, Justification::centred);
        }
    }
    if (pageIdx == 0) paintMain (g, panel);
    // output meter
    {
        const float m = jlimit (0.f, 1.f, (Decibels::gainToDecibels (outMeter, -60.f) + 60.f) / 66.f);
        g.setColour (c.panel); g.fillRoundedRectangle (meterArea.toFloat(), 4.f);
        auto r = meterArea.reduced (6, 8);
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
    if (const int st = proc.currentStyle(); st != styleShown)                      // module 13: the STYLE shown (undo / presets change it too)
    {
        styleShown = st;
        if (st >= 0) styleBox.setSelectedId (st + 1, dontSendNotification); else styleBox.setText ("STYLE ...", dontSendNotification);
    }
    lengthBtn.setButtonText ("LENGTH: " + fge::length::label (proc, proc.hostBpm()));
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
    repaint (mixerArea);
    for (auto& a : cardArea) repaint (a.withTop (a.getBottom() - 14));
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
    // the small handles next to the macro knobs on MAIN: drag a macro onto any knob
    if (pageIdx >= 2 || isDragAndDropActive() || ! macroArea.contains (e.getMouseDownPosition())) return;
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
    setSize (GeCanvas::W * 9 / 10, GeCanvas::H * 9 / 10);
}
void GeEditor::resized()
{
    canvas.setBounds (0, 0, GeCanvas::W, GeCanvas::H);
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) GeCanvas::W));
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
        bubble->showAt (&styleBox, t, 3500, true, true);
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
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&styleBox), [self] (int r)
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
