#include "PluginEditor.h"
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
    addAndMakeVisible (wave);

    // ---- header: global
    header = { knob ("core_length", "LENGTH", c.red, 44), knob ("core_master", "MASTER", c.silver, 44), knob ("core_velocity", "VELOCITY", c.silver, 44),
               toggle ("core_auto_length", "AUTO LENGTH", Toggle::Pill, c.red), toggle ("core_phase_reset", "PHASE RESET", Toggle::Pill, c.red),
               toggle ("core_key_track", "PITCH TRACKING", Toggle::Pill, c.red), seg ("core_voice_mode", { "CHOKE", "POLY" }, c.red) };
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
    const char* pages[6] { "MAIN", "ENVELOPES", "OSC", "LAYERS", "DIST", "SCREECH" };
    for (int i = 0; i < 6; ++i)
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
    selectPart (2);
    showPage (0);
    updateEnabled();
    startTimerHz (30);
}

GeCanvas::~GeCanvas() { stopTimer(); presetBar.setLookAndFeel (nullptr); setLookAndFeel (nullptr); }

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
    pageIdx = jlimit (0, 5, i);
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
    wave.setVisible (pageIdx < 2);
    const auto& c = palette();
    for (int k = 0; k < 6; ++k)
    {
        pageBtn[(size_t) k].setColour (TextButton::buttonColourId, k == pageIdx ? c.red : c.control);
        pageBtn[(size_t) k].setColour (TextButton::textColourOffId, k == pageIdx ? c.ground : c.silver);
    }
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
    headerArea = { 0, 0, W, 100 };
    presetBar.setBounds (W - 16 - 300, 5, 300, 90);
    // header: LENGTH / MASTER / VELOCITY, the three switches, VOICE
    int x = 330;
    for (int i = 0; i < 3; ++i) { header[(size_t) i]->setBounds (x, 10, 70, Knob::heightFor (44)); x += 74; }
    for (int i = 0; i < 3; ++i) header[(size_t) 3 + (size_t) i]->setBounds (x + 14, 14 + i * 26, 150, 22);
    header[6]->setBounds (742, 8, 216, 26);
    // pitch engine
    pitchArea = { 16, 104, W - 32, 84 };
    pitchRow[0]->setBounds (pitchArea.getX() + 14, pitchArea.getY() + 40, 520, 28);
    for (int i = 1; i < 9; ++i) pitchRow[(size_t) i]->setBounds (pitchArea.getX() + 570 + (i - 1) * 80, pitchArea.getY() + 4, 76, Knob::heightFor (38));
    // waveform + output meter
    waveArea = { 16, 196, W - 32 - 34, 226 };
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
    pageBtn[0].setBounds (742, 40, 70, 26);
    pageBtn[1].setBounds (815, 40, 70, 26);
    pageBtn[2].setBounds (888, 40, 70, 26);
    pageBtn[3].setBounds (742, 70, 70, 26);
    pageBtn[4].setBounds (815, 70, 70, 26);
    pageBtn[5].setBounds (888, 70, 70, 26);
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
    g.drawText ("v" + String (versionText) + "  -  CORE ENGINE", (int) tx, 68, 260, 16, Justification::left);
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
    const auto tb = proc.currentTables();
    wave.update (proc.frame(), proc.auditionNote, &proc.shapes(), proc.shapesVersion(), &tb, &proc.distData(), proc.distDataVersion());
    auto& e = proc.getEngine();
    for (int i = 0; i < 4; ++i) meter[(size_t) i] = jmax (e.partPeak[(size_t) i].exchange (0.f), meter[(size_t) i] * 0.85f);
    outMeter = jmax (e.outPeak.exchange (0.f), outMeter * 0.85f);
    updateEnabled();
    repaint (meterArea.expanded (4));
    repaint (mixerArea);
    for (auto& a : cardArea) repaint (a.withTop (a.getBottom() - 14));
}

// ======================================================================= editor
GeEditor::GeEditor (GeProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
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
