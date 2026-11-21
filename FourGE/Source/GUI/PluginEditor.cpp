#include "PluginEditor.h"
#include <regex>
#include "../Presets/StyleEngine.h"
#include "../Presets/LengthEngine.h"
#include "../Presets/PageRandomizer.h"
#include "../Presets/PunchSubtypes.h"
#include "../DSP/Front/FrontProfiles.h"
#include "DoomsdayBrand.h"

using namespace juce;
using namespace fge;
using fge::gui::palette;

namespace
{
    const char* partName[4] { "PUNCH / TOK", "BODY", "TAIL", "SUB" };
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
    fge::gui::setTheme (fge::ui::Settings::load().theme);                   // the look before any control takes its colours
    setLookAndFeel (&lnf);
    LookAndFeel::setDefaultLookAndFeel (&lnf);                                   // (every plain font of every page = Barlow)
    presetBar.setLookAndFeel (&stageLnf);
    const auto& c = palette();
    addAndMakeVisible (presetBar);
    // module 13: STYLE (always visible) - a click applies (KEEP MY CHANGES), a right-click = APPLY ALL / PREVIEW / FORGET
    styleStrip.setTooltip ("STYLE: EARLY / EUPHORIC / MODERN / RAW / EXPERIMENTAL - a starting point for the character (generators, pitch drop, distortion chain, screech). "
                           "It never changes the LENGTH and never locks a knob; your own changes are kept. Right-click: APPLY ALL / PREVIEW / FORGET MY CHANGES.");
    styleStrip.tipFor = [] (int i) { return fge::style::names()[i] + ": " + fge::style::description (i) + "  (right-click: APPLY ALL / PREVIEW / FORGET MY CHANGES)"; };
    styleStrip.onPick = [this] (int st) { chooseStyle (st, true); };
    styleStrip.onMenu = [this] (int) { styleMenu(); };
    lengthStrip.emptyText = "SYNC";
    addChildComponent (styleStrip);                 // (owner 2026-11-21: STYLE only on the VARIATION page - it is for randomizing, not for the main screen)
    // module 14: LENGTH profiles (one click = one undo) + the ms button = the LENGTH panel
    lengthStrip.tipFor = [] (int i) { return "LENGTH " + fge::length::names()[i] + ": " + String (roundToInt (fge::length::profileMs (i))) + " ms with AUTO LENGTH (the parts scale with it) - independent of the STYLE"; };
    lengthStrip.onPick = [this] (int i) { fge::length::applyProfile (proc, i); };
    lengthStrip.onMenu = [this] (int) { openLengthPanel(); };
    addChildComponent (lengthStrip);                                     // (the profiles live in the slider's right-click menu now)
    lengthSlider.setSliderStyle (Slider::LinearBar);
    lengthSlider.setNumDecimalPlacesToDisplay (0);
    lengthAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "core_length", lengthSlider);
    lengthSlider.setTooltip ("KICK LENGTH: any length (100 - 2500 ms), drag or type - automatable in FL (\"Length\"). Everything that moves in the kick "
                             "(the ZAAG shelf / sweep / resonator / distortion envelope) scales with it. Right-click: SHORT / MEDIUM / LONG / EXTRA LONG, the LENGTH panel.");
    lengthSlider.onMenu = [this]
    {
        PopupMenu m;
        for (int i = 0; i < 4; ++i) m.addItem (i + 1, fge::length::names()[i] + "  (" + String (roundToInt (fge::length::profileMs (i))) + " ms)");
        m.addSeparator(); m.addItem (10, "LENGTH PANEL (SYNC / AUTO / FOLLOW) ...");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&lengthSlider), [this] (int r)
        {
            if (r >= 1 && r <= 4) fge::length::applyProfile (proc, r - 1);
            else if (r == 10) openLengthPanel();
        });
    };
    addAndMakeVisible (lengthSlider);
    lengthBtn.setTooltip ("LENGTH: the exact length, LENGTH SYNC to the tempo, AUTO LENGTH and how much PUNCH / BODY / TAIL / SUB follow - "
                          "independent of the STYLE.");
    lengthBtn.onClick = [this] { openLengthPanel(); };
    addAndMakeVisible (lengthBtn);
    // RANDOMIZE STRENGTH (owner 2026-11-14): how much RANDOMIZE (and every page dice) changes - 5 % a small variation, 100 % something new
    rndStrength.setRange (5.0, 100.0, 1.0);
    rndStrength.setValue (fge::pagernd::strength (proc) * 100.0, juce::dontSendNotification);
    rndStrength.setTooltip ("RANDOMIZE STRENGTH: how much RANDOMIZE (and the RANDOMIZE button of every page / tab) changes. Low = a small variation of your kick, 100 % = something new in the style.");
    rndStrength.setColour (juce::Slider::rotarySliderFillColourId, palette().red);
    rndStrength.onValueChange = [this] { fge::pagernd::setStrength (proc, (float) rndStrength.getValue() / 100.f); rndLabel.setText ("RND " + juce::String ((int) rndStrength.getValue()) + " %", juce::dontSendNotification); };
    rndLabel.setText ("RND " + juce::String ((int) rndStrength.getValue()) + " %", juce::dontSendNotification);
    rndLabel.setJustificationType (juce::Justification::centred); rndLabel.setFont (juce::FontOptions (11.f));
    addChildComponent (rndStrength); addChildComponent (rndLabel);            // (KICK VARIATION 2026-11-20: the amount = VARIATION on that page)
    detDice.setTooltip ("RANDOMIZE this tab only (its visible knobs; the engine / mode switches stay). Strength = VARIATION (page VARIATION). One undo step, the level stays.");
    detDice.onClick = [this]
    {
        std::set<juce::Component*> mine (detail[(size_t) selected].begin(), detail[(size_t) selected].end());
        juce::StringArray ids;
        for (auto& [id, c] : byId) if (mine.count (c) > 0 && c->isShowing() && c->isEnabled()) ids.addIfNotAlreadyThere (id);
        fge::pagernd::randomizeIds (proc, ids, "tab");
    };
    addChildComponent (detDice);
    punchType = combo ("punch_mode"); tailType = combo ("tail_engine");          // ARCHITECTURE.md: the TYPE lists in the panel header
    punchType->setTooltip ("PUNCH TYPE: the mechanism that makes the punch - only its controls are shown below.");
    punchSub.setTooltip ("PUNCH SUBTYPE: a sound profile of this family (EUPHORIC: CLASSIC / 2010-2012 / FESTIVAL, PVC: CLASSIC / KLOENK / MODERN) - sets the punch, CLICK, SUB PUNCH and stereo at once (one undo). Never the TOK.");
    punchSub.setTextWhenNothingSelected ("SUBTYPE...");
    punchSub.onChange = [this]
    {
        const int i = punchSub.getSelectedId() - 1;
        if (i >= 0 && fge::punchsub::family (i) == (int) raw (proc, P::punchMode)) fge::punchsub::apply (proc, i);
    };
    addChildComponent (punchSub);
    tailType->setTooltip ("TAIL TYPE: the mechanism that makes the tail (CLASSIC / PVC / ZAAG) - only its controls are shown below.");
    liveSpec = std::make_unique<fge::gui::LiveSpecView> (proc);
    addChildComponent (*liveSpec);
    {   // MAIN OVERVIEW (ARCHITECTURE.md): the key knobs of each TYPE, with their owner in the name
        const auto& pc = palette();
        using L = std::initializer_list<std::pair<const char*, const char*>>;
        auto mk = [&] (std::vector<Component*>& v, L l, Colour col, int d) { for (auto [id, lab] : l) { auto* k = knob (id, lab, col, d); k->setVisible (false); v.push_back (k); } };
        mk (ovPunch[0], { { "punch_level", "PUNCH LEVEL" }, { "punch_pitch", "PUNCH PITCH" }, { "punch_length", "PUNCH LENGTH" }, { "punch_drive", "PUNCH DRIVE" },
                          { "punch_tone", "PUNCH TONE" }, { "punch_glide", "PUNCH GLIDE" }, { "punch_tok_decay", "TOK DECAY" }, { "punch_tok_bend", "TOK BEND" },
                          { "punch_transient", "TRANSIENT" }, { "punch_attack", "ATTACK" }, { "punch_decay", "DECAY" }, { "punch_mix", "PUNCH MIX" } }, pc.part[0], 40);
        mk (ovPunch[1], { { "psh_transpose", "TRANSPOSE" }, { "psh_mid_freq", "MID FREQ" }, { "psh_mid_gain", "MID GAIN" }, { "psh_drive", "PUNCH DRIVE" },
                          { "psh_asym", "ASYMMETRY" }, { "psh_tape", "TAPE STOP" }, { "psh_spike", "SPIKE" }, { "psh_body", "SHAPE BODY" },
                          { "psh_end", "LENGTH" }, { "psh_low_cut", "LOW CUT" }, { "psh_verb", "ROOM" }, { "punch_level", "PUNCH LEVEL" } }, pc.part[0], 40);
        mk (ovPunch[2], { { "pvc_tube_hz", "TUBE HZ" }, { "pvc_tube_comb", "TONES/COMB" }, { "pvc_jump", "JUMP" }, { "pvc_drop", "DROP" },
                          { "pvc_phaser", "PHASER" }, { "pvc_bright", "BRIGHT" }, { "pvc_drive", "PUNCH DRIVE" }, { "pvc_mid", "MID" },
                          { "pvc_length", "LENGTH" }, { "pvc_decay", "DECAY" }, { "pvc_low_mid", "LOW MID" }, { "punch_level", "PUNCH LEVEL" } }, pc.part[0], 40);
        mk (ovTail[0], { { "tail_level", "TAIL LEVEL" }, { "tail_length", "TAIL LENGTH" }, { "tail_drive", "TAIL DRIVE" }, { "tail_tone", "TAIL TONE" },
                         { "tail_st2_drive", "STAGE 2 DRIVE" }, { "tail_mid_hz", "MID FREQ" }, { "tail_mid_gain", "MID GAIN" }, { "tail_color_gain", "COLOR" },
                         { "tail_room", "ROOM" }, { "tail_width", "WIDTH" }, { "tail_low_cut", "LOW CUT" }, { "tail_high_cut", "HIGH CUT" } }, pc.part[2], 40);
        mk (ovTail[1], { { "pt_rev_size", "REV SIZE" }, { "pt_rev_decay", "REV DECAY" }, { "pt_rev_cut", "HIGH CUT" }, { "pt_drive1", "TAIL DRIVE 1" },
                         { "pt_clip", "CLIP" }, { "gt_gate_len", "GATE LENGTH" }, { "gt_gate_depth", "GATE DEPTH" }, { "pt_drive2", "TAIL DRIVE 2" },
                         { "pt_low_mid", "LOW MID" }, { "pt_width", "WIDTH" }, { "pt_grow", "GROW" }, { "pt_length", "LENGTH" } }, pc.part[2], 40);
        mk (ovTail[2], { { "zg_zaag", "ZAAG" }, { "zg_grit", "ZAAG DRIVE" }, { "zg_resonance", "RESONANCE" }, { "zg_movement", "MOVEMENT" },
                         { "zg_bass", "BASS" }, { "zg_stereo", "STEREO" }, { "zg_crush", "CRUSH" }, { "zg_character", "CHARACTER" },
                         { "zg_start", "PITCH START" }, { "zg_drop", "DROP TIME" }, { "zg_sawb_level", "HIGH SAW" }, { "zg_sub", "SUB" } }, pc.part[2], 40);
        mk (ovPunch[3], { { "zg_click", "CLICK" }, { "zg_piep", "PIEP" }, { "zg_piep_hz", "PIEP HZ" }, { "zg_piep_ms", "PIEP LEN" },
                          { "zg_trask", "TRASK" }, { "zg_trask_delay", "TRASK AT" }, { "zg_trask_ms", "TRASK LEN" }, { "zg_verb", "PUNCH VERB" },
                          { "zg_verb_decay", "VERB DECAY" }, { "zg_verb_send", "VERB SEND" }, { "zg_verb_pre", "PRE-DELAY" }, { "zg_verb_gate", "VERB GATE" } }, pc.part[0], 40);
        mk (ovPunch[4], { { "rc_snap", "SNAP" }, { "rc_crack", "CRACK" }, { "rc_crack_decay", "CRACK DECAY" }, { "rc_coupling", "BODY COUPLING" },
                          { "rc_start", "PUNCH START" }, { "rc_decay", "PUNCH DECAY" }, { "rk_snap", "SNAP" }, { "rk_crack", "CRACK" },
                          { "rk_density", "DENSITY" }, { "rk_edge", "EDGE" }, { "rk_tone", "DARK/BRIGHT" }, { "rk_coupling", "BODY COUPLING" } }, pc.part[0], 40);
        mk (ovStrip[0], { { "psh_click", "LEVEL" }, { "psh_click_len", "LENGTH" }, { "psh_click_tone", "TONE" } }, pc.silver, 30);
        mk (ovStrip[1], { { "psh_sub", "LEVEL" }, { "psh_sub_delay", "DELAY" }, { "psh_sub_decay", "DECAY" } }, pc.part[3], 30);
        mk (ovStrip[2], { { "body_level", "LEVEL" }, { "body_pitch", "PITCH" }, { "body_length", "LENGTH" } }, pc.part[1], 30);
        mk (ovStrip[3], { { "scr_level", "LEVEL" }, { "scr_base_hz", "FREQ" }, { "scr_fb_drive", "SCREECH DRIVE" } }, pc.red, 30);
        mk (ovStrip[4], { { "mix_punch_level", "PUNCH" }, { "mix_body_level", "BODY" }, { "mix_tail_level", "TAIL" }, { "mix_sub_level", "SUB" } }, pc.silver, 30);
        mk (ovMaster, { { "master_clip_drive", "MASTER CLIP" }, { "master_clip_soft", "SOFT" }, { "core_master", "OUTPUT" } }, pc.red, 30);
        ovAllP.onClick = [this] { selectPart (0); };
        ovAllT.onClick = [this] { selectPart (2); };
        ovBack.onClick = [this] { overview = true; showPage (0); };
        auto ids = [this] (const std::vector<Component*>& v) { StringArray a; for (auto& [id, c] : byId) if (std::find (v.begin(), v.end(), c) != v.end() && c->isEnabled()) a.addIfNotAlreadyThere (id); return a; };
        ovDiceP.onClick = [this, ids] { fge::pagernd::randomizeIds (proc, ids (ovPunch[(size_t) punchTypeIdx()]), "punch"); };
        ovDiceT.onClick = [this, ids] { fge::pagernd::randomizeIds (proc, ids (ovTail[(size_t) tailTypeIdx()]), "tail"); };
        for (auto* b : { &ovAllP, &ovAllT, &ovDiceP, &ovDiceT }) b->setTooltip (b == &ovAllP || b == &ovAllT ? "Every parameter of this part (the full view with its tabs)." : "RANDOMIZE these knobs (strength = VARIATION, page VARIATION).");
        ovBack.setTooltip ("Back to the overview: PUNCH | TAIL, the strip, MASTER.");
        for (auto* b : { &ovAllP, &ovAllT, &ovBack, &ovDiceP, &ovDiceT }) addChildComponent (b);
    }
    // module 17: the navigation + the interface settings
    uiSet = fge::ui::Settings::load();
    fge::gui::setTheme (uiSet.theme);
    nav.onSelect = [this] (int pg) { if (pg == 0) overview = true; showPage (pg); };     // (Main = back to the CONSOLE)
    nav.font = [this] (float h, bool bold) { return lnf.font (h, bold ? doomsday::flat::Bold : doomsday::flat::Medium); };
    nav.headFont = [this] (float h) { return lnf.head (h); };
    for (auto* ch : presetBar.getChildren())                                   // GUI v2: the preset buttons drawn like every other button
        if (dynamic_cast<TextButton*> (ch) != nullptr) ch->setLookAndFeel (&lnf);
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
    kickEndTop = seg ("core_note_cut", { "LENGTH", "NOTE", "+FX" }, c.red);
    // ---- pitch engine
    pitchRow = { seg ("pe_mode", { "CLASSIC", "FAST", "SLOW", "DOUBLE", "RISE", "STABLE" }, c.red),
                 knob ("pe_start", "START", c.part[2], 38), knob ("pe_end", "END", c.part[2], 38), knob ("pe_time", "TIME", c.part[2], 38),
                 knob ("pe_curve", "CURVE", c.part[2], 38), knob ("pe_amount", "AMOUNT", c.part[2], 38, true), knob ("pe_fine", "FINE", c.part[2], 38, true),
                 knob ("tail_penv_offset", "HOLD", c.part[2], 38), knob ("tail_hold_level", "TOK LEVEL", c.part[2], 38, true),
                 seg ("pe_start_mode", { "FOLLOW MIDI", "FIXED" }, c.part[2]) };
    // TOK NOTE: START (the tok) on a note - with TOK FIXED it plays that note whatever MIDI note is played
    {
        static const char* nm[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int m = 43; m <= 100; ++m) tokNote.addItem (String (nm[m % 12]) + String (m / 12 - 1), m + 1);
        tokNote.setTooltip ("TOK NOTE: the note of the tok (the PUNCH pitch). With FIXED it plays this note on every MIDI note; BODY / TAIL / SUB land on the played note.");
        tokNote.onChange = [this]
        {
            const int m = tokNote.getSelectedId() - 1;
            if (m < 0) return;
            proc.beginUndoStep ("Tok note");
            proc.setParamInUndoStep ("punch_pitch", 440.f * std::exp2 ((float) (m - 69) / 12.f));
        };
        addAndMakeVisible (tokNote);
        for (int t = 0; t < 17; ++t)
        {
            detTab[t].setButtonText (t == 0 ? "TONE" : "ROOM / COLOR"); detTab[t].setClickingTogglesState (false);
            detTab[t].setTooltip (t == 0 ? "The tail's source, envelope, tone, unison, stage 2, tail start."
                                         : "TAIL ROOM (a room BEFORE the tail's distortion - the early / euphoric crackle) and TAIL COLOR (the EQ AFTER it: which harmonics stay).");
            detTab[t].onClick = [this, t] { detailPage = t; layoutDetail(); repaint(); };
            addChildComponent (detTab[t]);
        }
    }
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
        if (i == 0) { d.push_back (seg ("punch_source", { "SINE", "TRI", "SAW", "SQUARE", "NOISE", "CLICK" }, col));
                      d.push_back (seg ("punch_mode", { "PUNCH", "TOK", "EUPH", "PVC" }, col));
                      d.push_back (seg ("punch_note_mode", { "FIXED", "FOLLOW MIDI", "OFFSET" }, col)); }
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
        if (i == 0)
        {
            d.push_back (knob ("punch_tok_bend", "BEND", col, 40, true));
            d.push_back (knob ("punch_overlap", "OVERLAP", col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "punch_tok_decay", "TOK DECAY" }, { "punch_note_offset", "NOTE OFFSET" },
                     { "punch_transient", "TRANSIENT" }, { "punch_transient_len", "TR LENGTH" }, { "punch_transient_tone", "TR TONE" } })
                d.push_back (knob (id, lab, col));
            d.push_back (knob ("punch_phase", "PHASE", col));                      // (on the 1st tab, before the SHAPED page)
            // 2nd tab: the SHAPED PUNCH (owner 2026-11-14) - source / character / tape stop / shaper / click / sub / verb
            d.push_back (&detailPageMark);
            d.push_back (combo ("psh_source"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "psh_transpose", "TRANSPOSE" }, { "psh_mid_freq", "MID FREQ" }, { "psh_mid_gain", "MID GAIN" }, { "psh_drive", "DRIVE" },
                     { "psh_asym", "ASYM" }, { "psh_tape", "TAPE STOP" }, { "psh_tape_start", "TAPE START" }, { "psh_low_cut", "LOW CUT" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "psh_spike", "SPIKE" }, { "psh_body", "SHAPE BODY" }, { "psh_body_ms", "BODY TIME" }, { "psh_end", "END" },
                     { "psh_verb", "ROOM" }, { "psh_click_len", "CLICK LEN" }, { "psh_click_tone", "CLICK TONE" } })
                d.push_back (knob (id, lab, col));
            d.push_back (toggle ("psh_tonal_only", "TONAL ONLY", Toggle::Chip, col));
            // 3rd tab: the PVC model (owner 2026-11-14, KLOENK): source / resonance / material / blend, then dynamics / EQ / shape
            d.push_back (&detailPageMark);
            d.push_back (combo ("pvc_source"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "pvc_pitch", "PITCH" }, { "pvc_res_freq", "RESONANCE" }, { "pvc_res_q", "RES Q" }, { "pvc_res_amount", "RES AMOUNT" } })
                d.push_back (knob (id, lab, col));
            d.push_back (combo ("pvc_material"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "pvc_mix", "BLEND" }, { "pvc_ring_ratio", "RING RATIO" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "pvc_comp", "COMP" }, { "pvc_drive", "DRIVE" }, { "pvc_sat", "SATURATION" }, { "pvc_asym", "ASYM" },
                     { "pvc_low_mid", "LOW MID" }, { "pvc_mid", "MID" }, { "pvc_high_mid", "HIGH MID" },
                     { "pvc_attack", "ATTACK" }, { "pvc_length", "LENGTH" }, { "pvc_decay", "DECAY" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);                                                // 3rd row: the TUBE model (owner 2026-11-14, from his samples)
            d.push_back (combo ("pvc_model"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "pvc_tube_hz", "TUBE HZ" }, { "pvc_tube_comb", "TONES/COMB" }, { "pvc_jump", "JUMP" }, { "pvc_drop", "DROP" }, { "pvc_drop_time", "DROP TIME" } })
                d.push_back (knob (id, lab, col));
            d.push_back (combo ("pvc_ph_mode"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "pvc_phaser", "PHASER" }, { "pvc_ph_rate", "PH RATE" }, { "pvc_ph_fb", "PH FEEDBK" }, { "pvc_bright", "BRIGHT" } })
                d.push_back (knob (id, lab, col));
            // 4th tab: SUB PUNCH (own wave / pitch envelope / delay / decay), the BLEND of the whole punch engine, the FX (Neroz 2026)
            d.push_back (&detailPageMark);
            d.push_back (combo ("psh_sub_wave"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "psh_sub_start", "SUB START" }, { "psh_sub_drop", "SUB DROP" }, { "psh_sub_delay", "SUB DELAY" }, { "psh_sub_decay", "SUB DECAY" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "psh_tonal", "TONAL" }, { "psh_click", "CLICK" }, { "psh_sub", "SUB" }, { "body_level", "BODY" }, { "tail_level", "TAIL" } })
                d.push_back (knob (id, lab, col));
            d.push_back (&detailPageMark);                                        // 5th tab: PUNCH TYPE ZAAG (owner 2026-11-15, rule 0: the punch lives here)
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "zg_thump", "THUMP" }, { "zg_thump_hz", "THUMP HZ" }, { "zg_thump_ms", "THUMP LEN" }, { "zg_thump_drive", "THUMP DRIVE" },
                     { "zg_click", "CLICK" }, { "zg_piep", "PIEP" }, { "zg_piep_hz", "PIEP HZ" }, { "zg_piep_ms", "PIEP LEN" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "zg_trask", "TRASK" }, { "zg_trask_delay", "TRASK AT" }, { "zg_trask_ms", "TRASK LEN" }, { "zg_trask_tone", "TRASK TONE" },
                     { "zg_pup", "PUP" }, { "zg_pup_hz", "PUP HZ" }, { "zg_pup_ms", "PUP LEN" }, { "zg_pup_oct", "PUP DIVE" }, { "zg_pup_drive", "PUP DRIVE" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "zg_verb", "PUNCH VERB" }, { "zg_verb_decay", "DECAY" }, { "zg_verb_size", "SIZE" }, { "zg_verb_send", "SEND" }, { "zg_verb_pre", "PRE-DELAY" },
                     { "zg_verb_gate", "GATE" }, { "zg_verb_damp", "BRIGHT" }, { "zg_verb_low_cut", "LOW CUT" } })
                d.push_back (knob (id, lab, col));
            // 6th tab: THE FRONT (owner 2026-11-16) - TOK (own layer before the punch), CLICK ("ts", never through ZAAG), SUB PUNCH (!= SUB);
            // shared by every PUNCH TYPE, so this tab is there with each of them
            d.push_back (&detailPageMark);
            d.push_back (combo ("tk_mode"));
            d.push_back (combo ("tk_type"));
            d.push_back (combo ("tk_pitch_mode"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "tk_level", "TOK LEVEL" }, { "tk_tune", "TOK PITCH" }, { "tk_penv_start", "P START" }, { "tk_penv_end", "P END" }, { "tk_penv_time", "DROP TIME" },
                     { "tk_length", "TOK LENGTH" }, { "tk_peak", "ATTACK" }, { "tk_fade", "DECAY" }, { "tk_tone", "TONE" }, { "tk_drive", "DRIVE" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            d.push_back (combo ("clk_mode"));
            d.push_back (combo ("clk_type"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "clk_level", "CLICK LEVEL" }, { "clk_pitch", "CLICK PITCH" }, { "clk_length", "CLICK LEN" }, { "clk_decay", "CLICK DECAY" },
                     { "clk_tone", "CLICK TONE" }, { "clk_drive", "CLICK DRIVE" }, { "clk_mix", "CLICK MIX" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            d.push_back (combo ("spk_mode"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "spk_level", "SUB PUNCH" }, { "spk_start", "SP START HZ" }, { "spk_end", "SP END HZ" }, { "spk_delay", "SP DELAY" },
                     { "spk_length", "SP LENGTH" }, { "spk_drive", "SP DRIVE" }, { "spk_tone", "SP TONE" }, { "zg_send", "ZAAG SEND" } })
                d.push_back (knob (id, lab, col));
            // 7th tab: PUNCH TYPE CLASSIC (EARLY REWORK, owner 2026-11-16): source / pitch, spectrum + drive, shape
            d.push_back (&detailPageMark);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cp_shape", "SHAPE" }, { "cp_trans", "TRANSIENT" }, { "cp_trans_ms", "TR LENGTH" }, { "cp_trans_hz", "TR FREQ" }, { "cp_res", "RESONANCE" },
                     { "cp_res_hz", "RES FREQ" }, { "cp_res_q", "RES Q" }, { "cp_noise", "NOISE" }, { "cp_noise_hz", "NOISE FREQ" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cp_start", "START" }, { "cp_hit", "HIT" }, { "cp_drop", "DROP" }, { "cp_glide", "GLIDE" }, { "cp_curve", "CURVE" }, { "cp_end", "END" },
                     { "cp_attack", "ATTACK" }, { "cp_hold", "HOLD" }, { "cp_decay", "DECAY" }, { "cp_decay_curve", "DEC CURVE" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cp_low_cut", "LOW CUT" }, { "cp_lowmid_hz", "LOW-MID HZ" }, { "cp_lowmid_db", "LOW-MID" }, { "cp_mid_hz", "MID HZ" }, { "cp_mid_db", "MID" },
                     { "cp_sat", "SATURATION" }, { "cp_asym", "ASYM" }, { "cp_sat_decay", "SAT DECAY" } })
                d.push_back (knob (id, lab, col));
            d.push_back (combo ("cp_ws_type"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "cp_ws_drive", "SHAPER" }, { "cp_high_cut", "HIGH CUT" }, { "cp_trans_ctrl", "TR CONTROL" } })
                d.push_back (knob (id, lab, col, 40, String (id) == "cp_trans_ctrl"));
            // 8th tab: PUNCH TYPE RAW (owner 2026-11-21, RAW_PUNCH.md) - two architectures, each shows only its own knobs (hiddenByVariant)
            d.push_back (&detailPageMark);
            d.push_back (combo ("rp_type"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "rc_snap", "SNAP" }, { "rc_crack", "CRACK" }, { "rc_crack_decay", "CRACK DECAY" }, { "rc_coupling", "BODY COUPLING" },
                     { "rk_snap", "SNAP" }, { "rk_crack", "CRACK" }, { "rk_density", "DENSITY" }, { "rk_edge", "EDGE" }, { "rk_tone", "DARK/BRIGHT" },
                     { "rk_crack_decay", "CRACK DECAY" }, { "rk_coupling", "BODY COUPLING" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "rc_start", "START" }, { "rc_drop", "DROP" }, { "rc_end", "END" }, { "rc_decay", "DECAY" }, { "rc_density", "DENSITY" }, { "rc_crack_hz", "CRACK FREQ" }, { "rc_drive", "PUNCH DRIVE" },
                     { "rk_start", "START" }, { "rk_drop", "DROP" }, { "rk_end", "END" }, { "rk_decay", "DECAY" }, { "rk_dynamic", "DYNAMIC" }, { "rk_drive", "BRIDGE DRIVE" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);                                                // DYNAMIC DISTORTED CRUNCH (owner 2026-11-21): the 3rd row of the RAW tab
            d.push_back (toggle ("cn_on", "CRUNCH", Toggle::Chip, col));
            d.push_back (toggle ("cn_solo", "SOLO CRUNCH", Toggle::Chip, col));
            d.push_back (combo ("cn_src"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cn_src_mix", "SOURCE MIX" }, { "cn_pitch_off", "PITCH" }, { "cn_offset", "DELAY" }, { "cn_phase", "PHASE" }, { "cn_attack", "ATTACK" },
                     { "cn_follow", "LENGTH" }, { "cn_decay", "DECAY" }, { "cn_crunch", "CRUNCH MIX" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cn_low_cut", "LOW CUT" }, { "cn_mid_boost", "MID BOOST" }, { "cn_high_cut", "HIGH CUT" }, { "cn_pre_emph", "PRE EMPH" }, { "cn_tube", "TUBE" },
                     { "cn_grit", "ASYMMETRY" } })
                d.push_back (knob (id, lab, col));
            d.push_back (combo ("cn_ws"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cn_clip", "CLIP" }, { "cn_dist_mix", "DIST MIX" }, { "cn_tone", "POST TONE" }, { "cn_body_limit", "BODY LIMIT" } })
                d.push_back (knob (id, lab, col));
            // 9th tab: RAW PUNCH EXTENSION - CRACK SOURCE + SNAP + BODY COUPLING (owner 2026-11-21, RAW_CRACK.md); with PUNCH TYPE RAW only
            d.push_back (&detailPageMark);
            d.push_back (toggle ("cr_on", "CRACK", Toggle::Chip, col));
            d.push_back (combo ("cr_model"));                                     // TRANSIENT CRACK (owner REWORK) / CLASSIC LAYER
            d.push_back (combo ("cr_listen"));
            d.push_back (combo ("cr_source"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cr_raw", "RAW CRACK" }, { "cr_position", "POSITION" }, { "cr_width", "WIDTH" }, { "cr_pitch_off", "PITCH" }, { "cr_pitch_env", "PITCH DROP" } })
                d.push_back (knob (id, lab, col));
            d.push_back (combo ("cr_ws"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cr_asym", "ASYMMETRY" }, { "cr_bp_freq", "BAND FREQ" }, { "cr_bp_width", "BAND WIDTH" }, { "cr_bp_res", "RESONANCE" }, { "cr_cbody", "CRACK BODY" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);                                                // CRACK REVERB (owner 2026-11-21): routing + listen first, then the room
            d.push_back (toggle ("cr_rv_on", "CRACK REVERB", Toggle::Chip, col));
            d.push_back (combo ("cr_rv_route"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cr_rv_mix", "MIX" }, { "cr_rv_size", "SIZE" }, { "cr_rv_decay", "DECAY" }, { "cr_rv_predelay", "PRE-DELAY" }, { "cr_rv_early", "EARLY" },
                     { "cr_rv_late", "LATE" }, { "cr_rv_diff", "DIFFUSION" }, { "cr_rv_damp", "DAMPING" } })
                d.push_back (knob (id, lab, col));
            d.push_back (toggle ("cr_rv_gate", "GATE", Toggle::Chip, col));
            d.push_back (combo ("cr_rv_drive_type"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cr_rv_drive", "RV DRIVE" }, { "cr_rv_hp", "RV HP" }, { "cr_rv_lp", "RV LP" }, { "cr_rv_res", "RV RES" }, { "cr_hi_gain", "HI EQ" }, { "cr_hi_freq", "HI FREQ" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);                                                // CRACK MOVEMENT (owner: "brakuje ruchu")
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cr_mv_sweep", "SWEEP" }, { "cr_mv_flange", "FLANGER" }, { "cr_mv_phaser", "PHASER" }, { "cr_mv_pitch", "POGO" }, { "cr_mv_rate", "RATE" }, { "cr_mv_time", "MOVE TIME" } })
                d.push_back (knob (id, lab, col, 40, String (id) == "cr_mv_sweep"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cr_level", "CRACK LEVEL" }, { "cr_snap", "SNAP" }, { "cr_crack", "CRACK" }, { "cr_density", "DENSITY" }, { "cr_edge", "EDGE" }, { "cr_coupling", "BODY COUPLING" } })
                d.push_back (knob (id, lab, col));
            d.push_back (combo ("cr_pitch_mode"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "cr_pitch", "CRACK PITCH" }, { "cr_tone_hz", "FIXED PITCH" }, { "cr_pitch_mod", "PITCH MOD" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            d.push_back (combo ("cr_shape"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cr_drive", "CRACK DRIVE" }, { "cr_clip", "CRACK CLIP" }, { "cr_low", "LOW" }, { "cr_mid", "MID" }, { "cr_high", "HIGH" },
                     { "cr_src_punch", "SRC PUNCH" }, { "cr_src_trans", "SRC TRANS" }, { "cr_src_noise", "SRC NOISE" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "cr_snap_len", "SNAP LEN" }, { "cr_snap_tone", "SNAP TONE" }, { "cr_snap_body", "SNAP BODY" }, { "cr_snap_edge", "SNAP EDGE" },
                     { "cr_attack", "ATK" }, { "cr_decay", "DEC" }, { "cr_sustain", "SUS" }, { "cr_release", "REL" }, { "cr_length", "LENGTH" } })
                d.push_back (knob (id, lab, col));
        }
        if (i == 3) d.push_back (knob ("sub_amp_env_delay", "START", col));      // SUB START: its own time, may enter under the tok
        if (i != 0) d.push_back (knob ((s + "_phase").toRawUTF8(), "PHASE", col));
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
            d.push_back (combo ("tail_start_mode"));                              // TOK -> TAIL: AFTER TOK / INDEPENDENT + TAIL START
            d.push_back (knob ("tail_start", "TAIL START", col));
            d.push_back (&detailPageMark);                                        // 2nd tab: ROOM + COLOR
            d.push_back (knob ("tail_room", "ROOM", col));                         // EARLY: a room BEFORE the tail's distortion
            d.push_back (knob ("tail_room_decay", "ROOM DECAY", col));
            d.push_back (knob ("tail_room_tone", "ROOM TONE", col));
            d.push_back (knob ("tail_room_delay", "ROOM DELAY", col));
            d.push_back (combo ("tail_room_input"));
            d.push_back (nullptr);
            d.push_back (knob ("tail_color_gain", "COLOR", col, 40, true));         // TAIL COLOR: the EQ AFTER the tail's distortion (BOOM / GRRRAAANG / KRAAASH)
            d.push_back (knob ("tail_color_freq", "COLOR HZ", col));
            d.push_back (knob ("tail_color_harm", "HARMONIC", col));
            d.push_back (knob ("tail_color_q", "COLOR Q", col));
            d.push_back (combo ("tail_color_track"));
            d.push_back (&detailPageMark);                                        // 3rd tab: TAIL ENGINE PVC - source / pitch / resonance / reverb (owner 2026-11-14)
            d.push_back (combo ("pt_source"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "pt_start", "START" }, { "pt_hold", "HOLD" }, { "pt_drop_time", "DROP TIME" }, { "pt_root", "ROOT" }, { "pt_slide", "SLIDE" }, { "pt_delay", "TAIL START" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "pt_res_freq", "RES FREQ" }, { "pt_res_q", "RES Q" }, { "pt_res_amount", "RES AMOUNT" },
                     { "pt_rev_size", "REV SIZE" }, { "pt_rev_decay", "REV DECAY" }, { "pt_rev_pre", "PRE-DELAY" }, { "pt_rev_cut", "HIGH CUT" }, { "pt_rev_wet", "WET" } })
                d.push_back (knob (id, lab, col));
            d.push_back (&detailPageMark);                                        // 4th tab: TAIL ENGINE PVC - gate / distortion / spectrum / stereo / final shape
            d.push_back (combo ("gt_gate"));
            d.push_back (combo ("pt_gate_curve"));
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "gt_gate_len", "GATE LENGTH" }, { "gt_gate_depth", "GATE DEPTH" } })
                d.push_back (knob (id, lab, col));
            d.push_back (combo ("gt_gate_div"));
            d.push_back (knob ("pt_movement", "MOVEMENT", col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "gt_comp", "PRE-COMP" }, { "pt_drive1", "DRIVE 1" }, { "pt_clip", "CLIP" }, { "pt_drive2", "DRIVE 2" }, { "pt_asym", "ASYM" },
                     { "gt_filter", "FILTER" }, { "gt_reso", "RESONANCE" }, { "gt_move_rate", "MOVE RATE" }, { "gt_move_depth", "MOVE DEPTH" }, { "pt_grow", "GROW" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> {
                     { "pt_low", "LOW" }, { "pt_low_mid", "LOW MID" }, { "pt_mid", "MID" }, { "pt_high", "HIGH" },
                     { "pt_mono_below", "MONO BELOW" }, { "pt_width", "WIDTH" }, { "pt_side", "SIDE" },
                     { "pt_attack", "ATTACK" }, { "pt_length", "LENGTH" }, { "pt_fade", "FADE" } })
                d.push_back (knob (id, lab, col));
            using LZ = std::initializer_list<std::pair<const char*, const char*>>;
            d.push_back (&detailPageMark);                                        // 5th tab: ZAAG ENGINE - variant, 8 macros, source / pitch, EQ1 -> DIST 1 -> EQ2 -> DIST 2 (ZAAG_ENGINE.md)
            d.push_back (combo ("zg_variant"));
            for (auto [id, lab] : LZ { { "zg_zaag", "ZAAG" }, { "zg_grit", "ZAAG DRIVE" }, { "zg_resonance", "RESONANCE" }, { "zg_movement", "MOVEMENT" },
                                       { "zg_bass", "BASS" }, { "zg_stereo", "STEREO" }, { "zg_crush", "CRUSH" }, { "zg_character", "CHARACTER" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : LZ { { "zg_sawb_level", "HIGH SAW" }, { "zg_start", "PITCH START" }, { "zg_drop", "DROP TIME" }, { "zg_curve", "CURVE" }, { "zg_root", "ROOT" },
                                       { "zg_low_ctrl", "LOW INTO DIST" }, { "zg_s1_fund", "FUND BOOST" }, { "zg_s1_mid_hz", "MID FREQ" }, { "zg_s1_mid", "MID BOOST" }, { "zg_s1_low_cut", "LOW CUT" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : LZ { { "zg_s1_drive", "DIST 1 DRIVE" }, { "zg_s1_curve", "DIST 1 HARD" }, { "zg_s1_asym", "DIST 1 ASYM" }, { "zg_s1_mix", "DIST 1 MIX" },
                                       { "zg_eqm_shelf", "LOW SHELF" }, { "zg_eqm_depth", "MOTION DEPTH" }, { "zg_eqm_time", "MOTION TIMING" }, { "zg_s2_drive", "DIST 2 DRIVE" }, { "zg_dist_env", "DIST ENV" } })
                d.push_back (knob (id, lab, col));
            d.push_back (&detailPageMark);                                        // 6th tab: ZAAG ENGINE - resonant sweep, ZAAG RESONATOR, DIST 3 (character) + dirt
            for (auto [id, lab] : LZ { { "zg_peak", "SWEEP GAIN" }, { "zg_peak_q", "SWEEP Q" }, { "zg_sw_start", "SWEEP START" }, { "zg_sw_plateau", "PLATEAU" },
                                       { "zg_sw_end", "SWEEP END" }, { "zg_sw_hold", "HOLD UNTIL" }, { "zg_sw_reopen", "REOPEN" } })
                d.push_back (knob (id, lab, col));
            d.push_back (combo ("zg_rsn_mode"));
            d.push_back (nullptr);
            for (auto [id, lab] : LZ { { "zg_rsn_freq", "RSN FREQ" }, { "zg_rsn_reso", "RSN RESO" }, { "zg_rsn_width", "RSN WIDTH" }, { "zg_rsn_mix", "RSN MIX" } })
                d.push_back (knob (id, lab, col));
            d.push_back (combo ("zg_rsn_pos")); d.push_back (combo ("zg_s3_type"));
            d.push_back (nullptr);
            for (auto [id, lab] : LZ { { "zg_drive", "DIST 3 DRIVE" }, { "zg_s3_preamp", "PREAMP" }, { "zg_s3_asym", "DIST 3 ASYM" }, { "zg_s3_mix", "DIST 3 MIX" },
                                       { "zg_dirt", "DIRT" }, { "zg_dirt_tone", "DIRT TONE" } })
                d.push_back (knob (id, lab, col));
            d.push_back (&detailPageMark);                                        // 7th tab: ZAAG ENGINE - EQ4 + soft clip, ZAAG STEREO, sub, tail shape
            for (auto [id, lab] : LZ { { "zg_squash", "SUB PUSH" }, { "zg_lm_cut", "LOW-MID CUT" }, { "zg_final_tone", "HIGH ENHANCE" }, { "zg_press", "SOFT CLIP" }, { "zg_knee", "KNEE" },
                                       { "zg_side_cut", "SIDE LOW CUT" }, { "zg_side_boost_hz", "SIDE BOOST HZ" }, { "zg_side_boost", "SIDE BOOST" }, { "zg_width", "WIDTH" }, { "zg_hverb", "HIGH VERB" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            d.push_back (combo ("zg_sub_src"));
            for (auto [id, lab] : LZ { { "zg_sub", "SUB" }, { "zg_sub_freq", "SUB FREQ" }, { "zg_sub_decay", "SUB DECAY" } })
                d.push_back (knob (id, lab, col));
            d.push_back (toggle ("zg_len_follow", "FOLLOW KICK", Toggle::Chip, col));
            for (auto [id, lab] : LZ { { "zg_length", "LENGTH" }, { "zg_attack", "ATTACK" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : LZ { { "zg_body", "BODY" }, { "zg_decay", "DECAY" }, { "zg_fade", "FADE" } })
                d.push_back (knob (id, lab, col));
            // 8th-10th tabs: TAIL TYPE RAW (owner 2026-11-16, TAIL_RAW_EXP.md) - source + the fixed stack / the sweep + character / space + out
            d.push_back (&detailPageMark);
            d.push_back (combo ("rw_osc1")); d.push_back (combo ("rw_osc2"));
            for (auto [id, lab] : LZ { { "rw_osc2_level", "OSC 2 LVL" }, { "rw_osc2_tune", "OSC 2 TUNE" } }) d.push_back (knob (id, lab, col));
            d.push_back (combo ("rw_noise"));
            for (auto [id, lab] : LZ { { "rw_noise_level", "NOISE LVL" }, { "rw_start", "PITCH START" }, { "rw_drop", "DROP" }, { "rw_curve", "CURVE" }, { "rw_root", "ROOT" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            d.push_back (toggle ("rw_pre_on", "PRE-EQ", Toggle::Chip, col));
            for (auto [id, lab] : LZ { { "rw_low_cut", "LOW CUT" }, { "rw_pre_mid", "PRE MID" } }) d.push_back (knob (id, lab, col));
            d.push_back (toggle ("rw_d1_on", "DIST 1", Toggle::Chip, col)); d.push_back (combo ("rw_d1_type"));
            d.push_back (knob ("rw_d1_drive", "DIST 1 DRIVE", col));
            d.push_back (toggle ("rw_shape_on", "SHAPING EQ", Toggle::Chip, col));
            for (auto [id, lab] : LZ { { "rw_growl_hz", "GROWL HZ" }, { "rw_growl_db", "GROWL GAIN" }, { "rw_growl_q", "GROWL Q" } }) d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            d.push_back (toggle ("rw_d2_on", "DIST 2", Toggle::Chip, col)); d.push_back (combo ("rw_d2_type")); d.push_back (knob ("rw_d2_drive", "DIST 2 DRIVE", col));
            d.push_back (toggle ("rw_d3_on", "DIST 3", Toggle::Chip, col)); d.push_back (combo ("rw_d3_type")); d.push_back (knob ("rw_d3_drive", "DIST 3 DRIVE", col));
            d.push_back (toggle ("rw_post_on", "POST-EQ", Toggle::Chip, col));
            for (auto [id, lab] : LZ { { "rw_post_low_cut", "POST CUT" }, { "rw_post_high", "POST HIGH" } }) d.push_back (knob (id, lab, col));
            d.push_back (toggle ("rw_clip_on", "CLIPPER", Toggle::Chip, col)); d.push_back (knob ("rw_clip_hard", "HARDNESS", col));
            d.push_back (&detailPageMark);
            d.push_back (toggle ("rw_bp_on", "BAND-PASS", Toggle::Chip, col)); d.push_back (combo ("rw_flt_shape")); d.push_back (combo ("rw_sync"));
            for (auto [id, lab] : LZ { { "rw_flt_start", "START" }, { "rw_flt_end", "END" }, { "rw_flt_mid", "MID" }, { "rw_flt_reso", "RESONANCE" },
                                       { "rw_flt_speed", "SPEED" }, { "rw_flt_time", "TIME" }, { "rw_flt_curve", "CURVE" }, { "rw_flt_mix", "MIX" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : LZ { { "rw_growl", "GROWL" }, { "rw_crunch", "CRUNCH" }, { "tl_drive", "DRIVE" }, { "tl_tone", "TONE" }, { "tl_movement", "MOVEMENT" },
                                       { "tl_resonance", "RESONANCE" }, { "tl_density", "DENSITY" }, { "tl_space", "SPACE" }, { "tl_chaos", "CHAOS" }, { "tl_width", "WIDTH" } })
                d.push_back (knob (id, lab, col));
            d.push_back (&detailPageMark);
            for (auto [id, lab] : LZ { { "rw_verb", "REVERB" }, { "rw_verb_size", "SIZE" }, { "rw_verb_decay", "DECAY" }, { "rw_verb_damp", "DAMPING" },
                                       { "rw_verb_pre", "PRE-DELAY" }, { "rw_verb_low_cut", "LOW CUT" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : LZ { { "rw_sub", "SUB" }, { "rw_sub_hz", "SUB HZ" }, { "rw_width", "WIDTH" }, { "rw_attack", "ATTACK" }, { "rw_body", "BODY" },
                                       { "rw_tail_decay", "DECAY" }, { "rw_fade", "FADE" } })
                d.push_back (knob (id, lab, col));
            // 11th-16th tabs: TAIL TYPE EXPERIMENTAL (owner 2026-11-16) - source + pitch / the 8 FX slots (2 per tab, the slot order = the chain) / mod + out
            d.push_back (&detailPageMark);
            d.push_back (combo ("ex_osc"));
            for (auto [id, lab] : LZ { { "ex_wt_pos", "WT POS" }, { "ex_osc_level", "OSC LVL" }, { "ex_fm_level", "FM LVL" }, { "ex_fm_amount", "FM AMOUNT" }, { "ex_fm_ratio", "FM RATIO" } })
                d.push_back (knob (id, lab, col));
            d.push_back (combo ("ex_noise"));
            for (auto [id, lab] : LZ { { "ex_noise_level", "NOISE LVL" }, { "ex_ring_ratio", "RING RATIO" } }) d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            d.push_back (combo ("ex_pitch_curve"));
            for (auto [id, lab] : LZ { { "ex_start", "START" }, { "ex_end", "END" }, { "ex_mid", "MID" }, { "ex_range", "RANGE" }, { "ex_pitch_time", "TIME" },
                                       { "ex_pitch_shape", "CURVE" }, { "ex_pm_rate", "MOD RATE" }, { "ex_pm_depth", "MOD DEPTH" }, { "ex_root", "ROOT" } })
                d.push_back (knob (id, lab, col));
            d.push_back (&detailPageMark);
            d.push_back (combo ("ex_s1_type"));
            for (auto [id, lab] : LZ { { "ex_s1_mix", "S1 MIX" }, { "ex_s1_amount", "S1 AMOUNT" }, { "ex_s1_param", "S1 PARAM" } }) d.push_back (knob (id, lab, col));
            d.push_back (combo ("ex_s1_mod")); d.push_back (knob ("ex_s1_depth", "S1 DEPTH", col, 40, true));
            d.push_back (moveSlotButton (1));
            d.push_back (nullptr);
            d.push_back (combo ("ex_s2_type"));
            for (auto [id, lab] : LZ { { "ex_s2_mix", "S2 MIX" }, { "ex_s2_amount", "S2 AMOUNT" }, { "ex_s2_param", "S2 PARAM" } }) d.push_back (knob (id, lab, col));
            d.push_back (combo ("ex_s2_mod")); d.push_back (knob ("ex_s2_depth", "S2 DEPTH", col, 40, true));
            d.push_back (moveSlotButton (2));
            d.push_back (&detailPageMark);
            d.push_back (combo ("ex_s3_type"));
            for (auto [id, lab] : LZ { { "ex_s3_mix", "S3 MIX" }, { "ex_s3_amount", "S3 AMOUNT" }, { "ex_s3_param", "S3 PARAM" } }) d.push_back (knob (id, lab, col));
            d.push_back (combo ("ex_s3_mod")); d.push_back (knob ("ex_s3_depth", "S3 DEPTH", col, 40, true));
            d.push_back (moveSlotButton (3));
            d.push_back (nullptr);
            d.push_back (combo ("ex_s4_type"));
            for (auto [id, lab] : LZ { { "ex_s4_mix", "S4 MIX" }, { "ex_s4_amount", "S4 AMOUNT" }, { "ex_s4_param", "S4 PARAM" } }) d.push_back (knob (id, lab, col));
            d.push_back (combo ("ex_s4_mod")); d.push_back (knob ("ex_s4_depth", "S4 DEPTH", col, 40, true));
            d.push_back (moveSlotButton (4));
            d.push_back (&detailPageMark);
            d.push_back (combo ("ex_s5_type"));
            for (auto [id, lab] : LZ { { "ex_s5_mix", "S5 MIX" }, { "ex_s5_amount", "S5 AMOUNT" }, { "ex_s5_param", "S5 PARAM" } }) d.push_back (knob (id, lab, col));
            d.push_back (combo ("ex_s5_mod")); d.push_back (knob ("ex_s5_depth", "S5 DEPTH", col, 40, true));
            d.push_back (moveSlotButton (5));
            d.push_back (nullptr);
            d.push_back (combo ("ex_s6_type"));
            for (auto [id, lab] : LZ { { "ex_s6_mix", "S6 MIX" }, { "ex_s6_amount", "S6 AMOUNT" }, { "ex_s6_param", "S6 PARAM" } }) d.push_back (knob (id, lab, col));
            d.push_back (combo ("ex_s6_mod")); d.push_back (knob ("ex_s6_depth", "S6 DEPTH", col, 40, true));
            d.push_back (moveSlotButton (6));
            d.push_back (&detailPageMark);
            d.push_back (combo ("ex_s7_type"));
            for (auto [id, lab] : LZ { { "ex_s7_mix", "S7 MIX" }, { "ex_s7_amount", "S7 AMOUNT" }, { "ex_s7_param", "S7 PARAM" } }) d.push_back (knob (id, lab, col));
            d.push_back (combo ("ex_s7_mod")); d.push_back (knob ("ex_s7_depth", "S7 DEPTH", col, 40, true));
            d.push_back (moveSlotButton (7));
            d.push_back (nullptr);
            d.push_back (combo ("ex_s8_type"));
            for (auto [id, lab] : LZ { { "ex_s8_mix", "S8 MIX" }, { "ex_s8_amount", "S8 AMOUNT" }, { "ex_s8_param", "S8 PARAM" } }) d.push_back (knob (id, lab, col));
            d.push_back (combo ("ex_s8_mod")); d.push_back (knob ("ex_s8_depth", "S8 DEPTH", col, 40, true));
            d.push_back (moveSlotButton (8));
            d.push_back (&detailPageMark);
            for (auto [id, lab] : LZ { { "ex_lfo1_rate", "LFO 1" }, { "ex_lfo2_rate", "LFO 2" }, { "ex_rnd_rate", "RANDOM" } }) d.push_back (knob (id, lab, col));
            d.push_back (combo ("ex_seed"));
            for (auto [id, lab] : LZ { { "ex_curve_start", "CURVE A" }, { "ex_curve_mid", "CURVE B" }, { "ex_curve_end", "CURVE C" }, { "ex_fm_mod", "FM MOD" }, { "ex_fs_mod", "SHIFT MOD" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : LZ { { "tl_drive", "DRIVE" }, { "tl_tone", "TONE" }, { "tl_movement", "MOVEMENT" }, { "tl_resonance", "RESONANCE" },
                                       { "tl_density", "DENSITY" }, { "tl_space", "SPACE" }, { "tl_chaos", "CHAOS" }, { "tl_width", "WIDTH" } })
                d.push_back (knob (id, lab, col));
            d.push_back (nullptr);
            for (auto [id, lab] : LZ { { "ex_sub", "SUB" }, { "ex_sub_hz", "SUB HZ" }, { "ex_width", "WIDTH" }, { "ex_drive", "OUT DRIVE" }, { "ex_attack", "ATTACK" },
                                       { "ex_body", "BODY" }, { "ex_tail_decay", "DECAY" }, { "ex_fade", "FADE" } })
                d.push_back (knob (id, lab, col));
            // 17th tab: TAIL RISE (owner 2026-11-21, TAIL_RISE.md) - with every TAIL TYPE
            d.push_back (&detailPageMark);
            d.push_back (toggle ("rs_on", "RISE", Toggle::Chip, col));
            d.push_back (combo ("rs_sync")); d.push_back (combo ("rs_grid")); d.push_back (combo ("rs_mode"));
            d.push_back (nullptr);
            for (auto [id, lab] : LZ { { "rs_start", "START" }, { "rs_amount", "AMOUNT" }, { "rs_pitch_range", "PITCH RANGE" } })
                d.push_back (knob (id, lab, col));
            d.push_back (combo ("rs_curve")); d.push_back (combo ("rs_release"));
            d.push_back (nullptr);
            for (auto [id, lab] : LZ { { "rs_gain", "GAIN" }, { "rs_pitch", "PITCH" }, { "rs_drive", "DRIVE" }, { "rs_harm", "HARMONICS" }, { "rs_filter", "FILTER" }, { "rs_noise", "NOISE" } })
                d.push_back (knob (id, lab, col));
        }
        mixer[(size_t) i] = { knob (("mix_" + s + "_level").toRawUTF8(), "FADER", col, 34), knob (("mix_" + s + "_pan").toRawUTF8(), "PAN", col, 30, true),
                              toggle (("mix_" + s + "_invert").toRawUTF8(), "PHASE " + String (CharPointer_UTF8 ("\xc3\x98")), Toggle::Chip, col) };
    }
    masterRow = { knob ("master_clip_drive", "CLIP", c.red, 34), knob ("master_clip_soft", "SOFT", c.red, 34) };
    // module 09: the 8 MASTER MACROS (sources only - what they move = the MACRO MAPPING on the MOD page)
    for (auto [id, lab] : std::initializer_list<std::pair<const char*, const char*>> { { "macro_impact", "IMPACT" }, { "macro_body", "BODY" }, { "macro_tail", "TAIL" },
             { "macro_pitch", "PITCH" }, { "macro_grit", "GRIT" }, { "macro_scream", "SCREAM" }, { "macro_weight", "WEIGHT" }, { "macro_chaos", "CHAOS" } })
        macroRow.push_back (knob (id, lab, c.red, 36));
    buildConsole();
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
    varPg = std::make_unique<fge::gui::VariationPage> (proc, lnf);                    // KICK VARIATION (owner 2026-11-20)
    addChildComponent (*varPg);
    presetBar.onBrowse = [this] { if (pageIdx != 12) pageBeforePresets = pageIdx; showPage (pageIdx == 12 ? pageBeforePresets : 12); };
    wave.onScreechStart = [this] (float ms, bool first) { if (first) proc.undo.beginNewTransaction ("Screech start"); proc.setParamInUndoStep ("scr_start", ms); };
    applyTooltips();
    setWantsKeyboardFocus (true);
    selectPart (2); overview = true; applyOverview();               // (start on the MAIN OVERVIEW)
    showPage (uiSet.startPage < 0 ? uiSet.lastPage : uiSet.startPage);
    updateEnabled();
    startTimerHz (30);
}

GeCanvas::~GeCanvas()
{
    *alive = false; stopTimer();
    for (auto* ch : presetBar.getChildren()) ch->setLookAndFeel (nullptr);
    presetBar.setLookAndFeel (nullptr); setLookAndFeel (nullptr);
    if (sharedLook.getReferenceCount() == 1 && &LookAndFeel::getDefaultLookAndFeel() == &lnf) LookAndFeel::setDefaultLookAndFeel (nullptr);
}

GeCanvas::Knob* GeCanvas::knob (const char* id, const String& labelIn, Colour accent, int d, bool bipolar)
{
    String label = labelIn;                                                     // ARCHITECTURE.md: a parameter has its OWNER in the name - never a bare "DRIVE"
    if (label == "DRIVE")
    {
        const String s (id);
        label = (s.startsWith ("punch_") || s.startsWith ("psh_") || s.startsWith ("pvc_") ? "PUNCH" : s.startsWith ("body_") ? "BODY"
               : s.startsWith ("tail_") || s.startsWith ("pt_") || s.startsWith ("zg_") ? "TAIL" : s.startsWith ("sub_") ? "SUB"
               : s.startsWith ("scr_") ? "SCREECH" : s.startsWith ("master_") ? "MASTER" : "") + String (" DRIVE");
        label = label.trim();
    }
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
TextButton* GeCanvas::moveSlotButton (int n)                                 // EXPERIMENTAL: MOVE slot n before slot n-1 (one undo step)
{
    auto b = std::make_unique<TextButton> (n == 1 ? "MOVE" : "< MOVE");
    b->setTooltip (n == 1 ? "Slot 1 is the first in the chain." : "MOVE: swap this slot with the one before it (the order changes the sound: a shifter before a shaper is not a shaper before a shifter).");
    b->setEnabled (n > 1);
    b->onClick = [this, n]
    {
        proc.beginUndoStep ("Move slot");
        for (auto* k : { "_type", "_mix", "_amount", "_param", "_mod", "_depth" })
        {
            const String a = "ex_s" + String (n) + k, c = "ex_s" + String (n - 1) + k;
            const float va = proc.apvts.getRawParameterValue (a)->load(), vc = proc.apvts.getRawParameterValue (c)->load();
            proc.setParamInUndoStep (a, vc); proc.setParamInUndoStep (c, va);
        }
    };
    auto* raw = b.get(); addAndMakeVisible (raw); owned.push_back (std::move (b)); return raw;
}
ComboBox* GeCanvas::combo (const char* id)
{
    auto cb = std::make_unique<ComboBox>();
    const int idx = [&] { for (int i = 0; i < P::count; ++i) if (String (id) == def (i).id) return i; return 0; }();
    cb->addItemList (def (idx).choices(), 1);
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *cb));
    auto* raw = cb.get(); addAndMakeVisible (raw); byId.insert ({ id, raw }); owned.push_back (std::move (cb)); return raw;
}
Component* GeCanvas::findControl (const String& id)                     // (a param may have 2 controls, e.g. BODY / TAIL in the punch BLEND: the visible one first)
{
    auto range = byId.equal_range (id);
    for (auto it = range.first; it != range.second; ++it) if (it->second->isVisible()) return it->second;
    return range.first == range.second ? nullptr : range.first->second;
}

int GeCanvas::punchTypeIdx() const { const int m = (int) raw (proc, P::punchMode); return m <= 1 || m == 5 ? 0 : m == 2 ? 1 : m == 3 ? 2 : m == 6 ? 4 : 3; }
int GeCanvas::tailTypeIdx() const  { return jlimit (0, 2, (int) raw (proc, P::tailEngine)); }
void GeCanvas::applyOverview()                                                      // MAIN: the CONSOLE or the full detail view of a part
{
    const bool main = pageIdx == 0, ov = main && overview;
    for (int p = 0; p < 4; ++p)
    {
        for (auto* c : card[(size_t) p]) c->setVisible (main && ! ov);
        for (auto* c : mixer[(size_t) p]) c->setVisible (main && ! ov);
        if (ov) for (auto* c : detail[(size_t) p]) if (c != nullptr && c != &detailPageMark) c->setVisible (false);
    }
    for (auto* c : masterRow) c->setVisible (main && ! ov);
    if (ov) { for (auto& t : detTab) t.setVisible (false); detDice.setVisible (false); if (liveSpec != nullptr) liveSpec->setVisible (false); tokNote.setVisible (false); punchSub.setVisible (false); }
    // (the old overview's knobs stay hidden - the CONSOLE replaced it, GUI v2)
    for (auto& v : ovPunch) for (auto* c : v) c->setVisible (false);
    for (auto& v : ovTail) for (auto* c : v) c->setVisible (false);
    for (auto& v : ovStrip) for (auto* c : v) c->setVisible (false);
    for (auto* c : ovMaster) c->setVisible (false);
    for (auto* b : { &ovAllP, &ovAllT, &ovDiceP, &ovDiceT }) b->setVisible (false);
    const bool con = ov && uiSet.mainView == 0, ln = ov && uiSet.mainView == 1;
    for (auto& st : strips)
    {
        for (auto* c : st.knobs) c->setVisible (con);
        for (auto* c : st.buttons) c->setVisible (ov);
        if (st.type != nullptr) st.type->setVisible (ov);
        if (st.fader != nullptr) st.fader->setVisible (con);
    }
    for (auto* c : laneLevel) if (c != nullptr) c->setVisible (ln);
    for (auto& v : inspector) for (auto* c : v) c->setVisible (false);
    if (ln) for (auto* c : inspectorNow()) c->setVisible (true);
    inspAll.setVisible (ln);
    for (auto* c : pitchSide) c->setVisible (con);
    for (auto* c : masterSide) c->setVisible (con);
    viewConsole.setVisible (ov); viewLanes.setVisible (ov);
    viewConsole.setColour (TextButton::buttonColourId, con ? palette().control.brighter (0.25f) : palette().control);
    viewLanes.setColour (TextButton::buttonColourId, ln ? palette().control.brighter (0.25f) : palette().control);
    ovBack.setButtonText ("Console");
    ovBack.setVisible (main && ! overview);
    if (ov) { punchType->setVisible (false); tailType->setVisible (false); if (con) layoutConsole(); else layoutLanes(); }
    else wave.setBounds (waveArea);
}
void GeCanvas::layoutOverview()
{
    const int pw = (CW - 12) / 2;
    ovPArea = { X0, Y0 + 234, pw, 300 };
    ovTArea = { X0 + pw + 12, Y0 + 234, pw, 300 };
    auto grid = [] (const std::vector<Component*>& v, Rectangle<int> a, int cols, int d)
    {
        const int cw = (a.getWidth() - 20) / cols;
        for (size_t i = 0; i < v.size(); ++i)
            v[i]->setBounds (a.getX() + 10 + (int) (i % (size_t) cols) * cw, a.getY() + 74 + (int) (i / (size_t) cols) * (Knob::heightFor (d) + 14), cw - 4, Knob::heightFor (d));
    };
    for (int t = 0; t < 5; ++t) grid (ovPunch[(size_t) t], ovPArea, 6, 40);
    for (int t = 0; t < 3; ++t) grid (ovTail[(size_t) t], ovTArea, 6, 40);
    for (auto [a, cb, all, dice] : { std::make_tuple (ovPArea, punchType, &ovAllP, &ovDiceP), std::make_tuple (ovTArea, tailType, &ovAllT, &ovDiceT) })
    {
        cb->setBounds (a.getX() + 110, a.getY() + 4, 160, 24);
        all->setBounds (a.getRight() - 14 - 130, a.getY() + 5, 130, 22);
        dice->setBounds (a.getRight() - 14 - 130 - 100, a.getY() + 5, 92, 22);
    }
    const int sw = (CW - 4 * 10) / 5;
    for (int i = 0; i < 5; ++i)
    {
        ovStripArea[(size_t) i] = { X0 + i * (sw + 10), Y0 + 542, sw, 96 };
        const auto& v = ovStrip[(size_t) i];
        const int kw = (sw - 16) / (int) v.size();
        for (size_t k = 0; k < v.size(); ++k) v[k]->setBounds (ovStripArea[(size_t) i].getX() + 8 + (int) k * kw, ovStripArea[(size_t) i].getY() + 24, kw - 2, Knob::heightFor (30));
    }
    ovMasterArea = { X0, Y0 + 646, CW, 72 };
    for (size_t k = 0; k < ovMaster.size(); ++k) ovMaster[k]->setBounds (ovMasterArea.getX() + 130 + (int) k * 110, ovMasterArea.getY() + 2, 100, Knob::heightFor (30));
}
void GeCanvas::selectPart (int i)
{
    overview = false; applyOverview();
    selected = jlimit (0, 3, i);
    for (int p = 0; p < 4; ++p) for (auto* c : detail[(size_t) p]) if (c != nullptr && c != &detailPageMark) c->setVisible (p == selected && pageIdx == 0);
    tokNote.setVisible (pageIdx == 0 && selected == 0);
    layoutDetail();
    repaint();
}

void GeCanvas::showPage (int i)
{
    pageIdx = jlimit (0, 15, i);
    const bool main = pageIdx == 0, kick = pageIdx < 2;
    for (int p = 0; p < 4; ++p)
    {
        for (auto* c : card[(size_t) p]) c->setVisible (main);
        for (auto* c : mixer[(size_t) p]) c->setVisible (main);
        for (auto* c : detail[(size_t) p]) if (c != nullptr && c != &detailPageMark) c->setVisible (main && p == selected);
    }
    for (auto* c : masterRow) c->setVisible (main);
    for (auto* c : pitchRow) c->setVisible (false);                                  // (GUI v2: PITCH sits in the console)
    header[5]->setVisible (false);
    tokNote.setVisible (main && selected == 0);
    layoutDetail();
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
    varPg->setVisible (pageIdx == 15);
    if (pageIdx == 15) varPg->sync();
    wave.setVisible (kick);
    nav.setCurrent (pageIdx);
    if (pageIdx != uiSet.lastPage && pageIdx != 12) { uiSet.lastPage = pageIdx; if (isShowing()) uiSet.save(); }
    applyOverview();
    repaint();
}

void GeCanvas::mouseDown (const MouseEvent& e)
{
    if (pageIdx != 0) return;
    if (overview)
    {   // CONSOLE: a click on a strip's name = every parameter of that part
        for (auto& st : strips) if (st.nameArea.contains (e.getPosition()) && st.open) { st.open(); return; }
        if (lanesShown()) for (int i = 0; i < 7; ++i) if (laneArea[(size_t) i].contains (e.getPosition())) { selectLane (i); return; }
        return;
    }
    for (int i = 0; i < 4; ++i)
        if (cardArea[(size_t) i].contains (e.getPosition())) { selectPart (i); return; }
}

void GeCanvas::resized()
{
    // GUI v2 (Design/GUI_V2.md): row 1 = logo, the blocks, the presets; row 2 = the pages, STYLE / LENGTH / KICK END
    headerArea = { 0, 0, W, 88 };
    const int tx = roundToInt (doomsday::brand::headerTextX (1.f));
    nav.setBounds (tx + 70, 0, nav.contentWidth() + 20, 88);
    presetBar.setBounds (W - 20 - 640, 10, 640, 32);
    {
        int x = W - 20;
        auto place = [&] (Component* c, int w, int h) { if (c == nullptr) return; x -= w; c->setBounds (x, 52 + (36 - h) / 2, w, h); x -= 78; };
        place (kickEndTop, 170, 24);
        place (&lengthSlider, 130, 24);
        lengthStrip.setBounds (lengthSlider.getBounds());
        styleStrip.setBounds (0, 0, 0, 0);
        lengthBtn.setBounds (0, 0, 0, 0); lengthBtn.setVisible (false);
    }
    helpBtn.setBounds (tx + 40, 59, 24, 22);
    if (help != nullptr) help->setBounds (getLocalBounds());
    viewConsole.setBounds (nav.getX() + 330, 58, 82, 24);
    viewLanes.setBounds (nav.getX() + 414, 58, 82, 24);
    infoArea = { 0, H - 26, W, 26 };
    // bottom: MACROS + GLOBAL (always visible)
    macroArea = { X0, H - 26 - 8 - 100, 900, 100 };
    for (size_t i = 0; i < macroRow.size(); ++i) macroRow[i]->setBounds (macroArea.getX() + 82 + (int) i * 100, macroArea.getY() + 14, 80, Knob::heightFor (36));
    globalArea = { macroArea.getRight() + 12, macroArea.getY(), W - 20 - macroArea.getRight() - 12, 100 };
    {
        int x = globalArea.getX() + 10;
        for (int k : { 0, 1, 2 }) { header[(size_t) k]->setBounds (x, globalArea.getY() + 14, 68, Knob::heightFor (36)); x += 70; }
        x += 8;
        header[6]->setBounds (x, globalArea.getY() + 10, 146, 20);          // VOICE
        header[7]->setBounds (x, globalArea.getY() + 32, 146, 20);          // KICK END
        header[4]->setBounds (x, globalArea.getY() + 54, 146, 20);          // PHASE RESET
        header[3]->setBounds (x, globalArea.getY() + 76, 146, 20);          // AUTO LENGTH
        meterArea = { globalArea.getRight() - 34, globalArea.getY() + 10, 24, globalArea.getHeight() - 28 };
    }
    pitchArea = {};
    waveArea = { X0, Y0, CW, 134 };                                        // (the full detail view; the console sets its own)
    wave.setBounds (waveArea);
    // part cards
    const int cw = (CW - 3 * 12) / 4;
    for (int i = 0; i < 4; ++i)
    {
        cardArea[(size_t) i] = { X0 + i * (cw + 12), Y0 + 146, cw, 150 };
        const auto a = cardArea[(size_t) i];
        for (int k = 0; k < 4; ++k) card[(size_t) i][(size_t) k]->setBounds (a.getX() + 8 + k * 72, a.getY() + 28, 70, Knob::heightFor (40));
        card[(size_t) i][4]->setBounds (a.getRight() - 136, a.getY() + 4, 60, 20);
        card[(size_t) i][5]->setBounds (a.getRight() - 70, a.getY() + 4, 60, 20);
    }
    detailArea = { X0, Y0 + 308, CW - 12 - 340, CH - 308 };
    mixerArea = { detailArea.getRight() + 12, detailArea.getY(), X0 + CW - detailArea.getRight() - 12, CH - 308 };
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
    ovBack.setBounds (X0 + CW - 130, Y0 + 8, 120, 24);
    layoutConsole();
    applyOverview();
    const Rectangle<int> pageArea (X0, Y0, CW, CH);
    if (envPage != nullptr) envPage->setBounds (pageArea.withTrimmedTop (146));
    for (Component* pg : { (Component*) oscPage.get(), (Component*) layerPage.get(), (Component*) distortionPage.get(), (Component*) eqPg.get(), (Component*) dynPg.get(),
                           (Component*) modPg.get(), (Component*) smpPg.get(), (Component*) stPg.get(), (Component*) exPg.get(), (Component*) prPg.get(),
                           (Component*) anPg.get(), (Component*) scrPage.get(), (Component*) setPg.get(), (Component*) varPg.get() })
        if (pg != nullptr) pg->setBounds (pageArea);
}

void GeCanvas::layoutDetail()
{
    auto& d = detail[(size_t) selected];
    if (d.empty()) return;
    int y = detailArea.getY() + 32, x = detailArea.getX() + 14;
    size_t k = 0;
    // segmented rows (source, character) - the CLASSIC generator's: shown only with the CLASSIC type (ARCHITECTURE.md: only the picked TYPE's controls)
    const bool classicSegs = selected == 0 ? (int) raw (proc, P::punchMode) <= 1 : selected == 2 ? (int) raw (proc, P::tailEngine) == 0 : true;
    while (k < d.size() && dynamic_cast<Seg*> (d[k]) != nullptr)
    {
        d[k]->setVisible ((pageIdx == 0 && ! overview) && classicSegs);
        if (selected == 0)                                   // PUNCH / TOK: SOURCE | PUNCH / TOK | FIXED / FOLLOW MIDI + TOK NOTE
        {
            const int xs[3] { 0, 345, 495 }, ws[3] { 330, 140, 270 };
            d[k]->setBounds (x + xs[jmin ((int) k, 2)], y, ws[jmin ((int) k, 2)], 26);
            ++k; continue;
        }
        const int items = selected == 2 && k == 1 ? 7 : (int) (selected == 3 ? 2 : 6);
        d[k]->setBounds (x + (int) k * 420, y, jmin (400, items * 62), 26);
        ++k;
    }
    if (selected == 0) { tokNote.setBounds (x + 775, y + 1, 70, 24); tokNote.setVisible ((pageIdx == 0 && ! overview) && classicSegs); }
    y += 36;
    // a 2nd tab (TAIL: ROOM / COLOR): the items after the marker share the rows of the first tab, one tab shown at a time
    const int nAll = 1 + (int) std::count (d.begin(), d.end(), &detailPageMark);
    const auto allowed = allowedTabs();                                          // ARCHITECTURE.md: only the picked TYPE's tabs
    const auto isAllowed = [&] (int t) { return std::find (allowed.begin(), allowed.end(), t) != allowed.end() && t < nAll; };
    if (! isAllowed (detailPage)) detailPage = allowed.empty() ? 0 : allowed.front();
    const int nTabs = (int) std::count_if (allowed.begin(), allowed.end(), [&] (int t) { return t < nAll; });
    const bool tabs = nAll > 1;
    {   // the TYPE list in the header
        auto* tb = selected == 0 ? punchType : selected == 2 ? tailType : nullptr;
        for (auto* c : { punchType, tailType }) if (c != nullptr) c->setVisible ((pageIdx == 0 && ! overview) && c == tb);
        if (tb != nullptr) tb->setBounds (detailArea.getRight() - 14 - nTabs * 112 - 100 - 160, detailArea.getY() + 5, 150, 24);
        const int fam = (int) raw (proc, P::punchMode);
        const bool sub = selected == 0 && (pageIdx == 0 && ! overview) && (fam == 2 || fam == 3);
        if (sub && fam != punchSubFamily)                                       // the family's three profiles
        {
            punchSubFamily = fam; punchSub.clear (dontSendNotification);
            for (int i = 0; i < fge::punchsub::count; ++i) if (fge::punchsub::family (i) == fam) punchSub.addItem (fge::punchsub::name (i), i + 1);
        }
        if (sub) { const int cur = (int) proc.apvts.state.getProperty ("punchSub", -1);
                   punchSub.setSelectedId (cur >= 0 && fge::punchsub::family (cur) == fam ? cur + 1 : 0, dontSendNotification); }
        punchSub.setVisible (sub);
        if (sub && tb != nullptr) punchSub.setBounds (tb->getX() - 128, detailArea.getY() + 5, 122, 24);
    }
    for (int t = 0; t < 17; ++t)
    {
        detTab[t].setVisible (tabs && isAllowed (t) && nTabs > 1 && (pageIdx == 0 && ! overview));
        detDice.setVisible ((pageIdx == 0 && ! overview));
        static const char* punchTabs[] { "PUNCH / TOK", "EUPHORIC", "PVC", "SUB / BLEND", "ZAAG", "FRONT", "CLASSIC", "RAW", "RAW CRACK", "", "", "", "", "", "", "", "" };
        static const char* tailTabs[] { "TONE", "ROOM / COLOR", "PVC SOURCE", "PVC SHAPE", "ZAAG", "ZAAG SWEEP / FX", "ZAAG OUT", "RAW STACK", "RAW SWEEP / CHAR", "RAW SPACE / OUT", "EXP SOURCE", "EXP FX 1-2", "EXP FX 3-4", "EXP FX 5-6", "EXP FX 7-8", "EXP MOD / OUT", "RISE" };
        static const char* punchTips[] { "The classic PUNCH / TOK generator (a waveform with a pitch glide).",
                                         "EUPHORIC model (punch mode EUPHORIC): a broadband source -> mid boost -> asymmetric distortion -> tape stop -> volume shaper.",
                                         "PVC model (punch mode PVC, the KLOENK): source -> resonance / ring mod / flanger / comb -> blend -> compressor -> distortion -> EQ -> shape. Monotone.",
                                         "The shared part of the PUNCH ENGINE: the SUB PUNCH (own wave / pitch / delay / decay) and the BLEND (tonal / click / sub / body / tail).",
                                         "PUNCH TYPE ZAAG: a 2 ms click, the PIEP first, then the TRASK, and a reverb on them only.",
                                         "THE FRONT of the kick, with every PUNCH TYPE: TOK (its own layer before the punch, a fixed pitch), CLICK (the 'ts', straight to the punch bus, never through ZAAG) and SUB PUNCH (a short low hit, gone before the SUB). ZAAG SEND: TOK + PUNCH into the ZAAG tail cascade.", "PUNCH TYPE CLASSIC (EARLY): the old, hard, short punch built from its attack - a hybrid source (tone + its own transient + a resonant low-mid + attack noise), a 3-stage pitch (start -> hit -> glide -> fundamental), pre-EQ -> saturation -> waveshaper -> post-EQ -> transient control.",
                                         "PUNCH TYPE RAW: RAW CLASSIC (punch source + SNAP, a controlled crack: fold -> clip -> band -> envelope, BODY COUPLING - KRAK -> DUM) or RAW 2015-2017 CRACKING (snap -> crack generator: dark/bright tilt, DENSITY folds, controlled distortion, EDGE -> a distorted bridge into the body - KRAK! -> BRUTAL BODY).",
                                         "RAW PUNCH EXTENSION: an own CRACK layer (harmonic source, CRACK PITCH, CRACK DRIVE / CLIP / TONE, LEVEL) + SNAP + BODY COUPLING - after the master clip, so the kick's drive never touches it. OFF = the RAW punch as before.", "", "", "", "", "", "", "", "" };
        detTab[t].setButtonText (selected == 0 ? punchTabs[t] : tailTabs[t]);
        detTab[t].setTooltip (selected == 0 ? punchTips[t]
                                            : (t == 0 ? "The tail's source, envelope, tone, unison, stage 2, tail start."
                                               : t == 1 ? "TAIL ROOM (a room BEFORE the tail's distortion - the early / euphoric crackle) and TAIL COLOR (the EQ AFTER it: which harmonics stay)."
                                                        : t == 2 ? "TAIL ENGINE PVC (the KLOENK tail): the source (kick BODY / sine / saw / hybrid), its pitch and resonance, and the REVERB the tail is made of."
                                                                 : t == 3 ? "TAIL ENGINE PVC: the GATE, the distortion stages (drive 1 -> clip -> gate -> drive 2), the spectrum balance, mono low / stereo high, the final shape."
                                                                          : t == 4 ? "ZAAG ENGINE: VARIANT + the 8 ZAAG macros, the source / pitch, EQ1 -> DIST 1 -> moving low shelf -> DIST 2 (a cascade: every stage feeds the next)."
                                                                                   : t == 5 ? "ZAAG ENGINE: the resonant sweep (locked to the kick envelope), ZAAG RESONATOR (phaser / resonator / comb), DIST 3 = the main character (+ dirt)."
                                                                                            : t == 6 ? "ZAAG ENGINE: SUB PUSH / LOW-MID CUT / HIGH ENHANCE -> soft clip, ZAAG STEREO (the lows always mono), the sub, the tail shape."
                                                                                                     : t == 7 ? "TAIL TYPE RAW: OSC 1 / OSC 2 / NOISE, the controlled pitch drop, and the fixed stack PRE-EQ -> DIST 1 -> SHAPING EQ (GROWL) -> DIST 2 -> (band-pass) -> DIST 3 -> POST-EQ -> CLIPPER (every stage on / off)."
                                                                                                              : t == 8 ? "TAIL TYPE RAW: the resonant BAND-PASS SWEEP (DOWN / UP / DOWN-UP / UP-DOWN / CUSTOM; SYNC / FREE / BODY END) and the character: GROWL, CRUNCH + the 8 TAIL macros."
                                                                                                                       : t == 9 ? "TAIL TYPE RAW: the subtle reverb (after the attack, no lows), the SUB (clean sine on the pitch, mono), the WIDTH of the highs, the tail shape."
                                                                                                                                : t == 10 ? "TAIL TYPE EXPERIMENTAL: the OSCILLATOR (+ CUSTOM WAVETABLE), the real FM ENGINE, NOISE, the RING SOURCE, and the PITCH CURVE (8 shapes)."
                                                                                                                                          : t == 16 ? "TAIL RISE: the end of the tail GROWS - synced to the BPM grid (or the kick length): START / AMOUNT / CURVE / END, the MODE (POWER / RAW / PITCH / HARMONIC / HYBRID) and the HYBRID layers."
                                                                                                                                          : t == 15 ? "TAIL TYPE EXPERIMENTAL: LFO 1 / LFO 2 / RANDOM (FIXED SEED) / MANUAL CURVE, FM / SHIFT mod, the 8 TAIL macros, the SUB (lows mono), WIDTH, the tail shape."
                                                                                                                                                    : "TAIL TYPE EXPERIMENTAL: the FX SLOTS - the slot order is the processing order (MOVE swaps a slot with the one before it). Each: TYPE, MIX, AMOUNT, PARAM, MOD SOURCE + DEPTH."));
        const int tw = 108, step = tw + 4;
        int right = detailArea.getRight() - 14; for (int u = nAll - 1; u > t; --u) if (isAllowed (u)) right -= step;
        detTab[t].setBounds (right - tw, detailArea.getY() + 6, tw, 22);
        if (t == (allowed.empty() ? 0 : *std::min_element (allowed.begin(), allowed.end()))) detDice.setBounds (right - tw - 100, detailArea.getY() + 6, 92, 22);   // (left of the LEFTMOST tab)
        detTab[t].setColour (TextButton::buttonColourId, t == detailPage ? palette().red.withAlpha (0.5f) : palette().control);
    }
    const int y0 = y;
    int tab = 0;
    for (int i = 0; k < d.size(); ++k)
    {
        if (d[k] == &detailPageMark) { ++tab; y = y0; i = 0; continue; }
        if (d[k] == nullptr) { y += Knob::heightFor (40) + 6; i = 0; continue; }       // row break
        if (hiddenByVariant (d[k])) { d[k]->setVisible (false); continue; }               // (the row closes up)
        if (tabs) d[k]->setVisible ((pageIdx == 0 && ! overview) && tab == detailPage);
        if (dynamic_cast<ComboBox*> (d[k]) != nullptr || dynamic_cast<TextButton*> (d[k]) != nullptr) { d[k]->setBounds (x + i * 76, y + 22, 150, 26); i += 2; continue; }
        if (dynamic_cast<Toggle*> (d[k]) != nullptr) { d[k]->setBounds (x + i * 76, y + 24, 130, 22); i += 2; continue; }
        d[k]->setBounds (x + i * 76, y, 74, Knob::heightFor (40));
        ++i;
    }
    // LIVE SPECTRUM next to the EQ knobs of the tab (owner 2026-11-14): TAIL ROOM / COLOR, PUNCH EUPHORIC
    if (liveSpec != nullptr)
    {
        std::vector<fge::gui::LiveSpecView::Marker> m;
        if (selected == 2 && detailPage == 1) m = { { "tail_color_freq", "COLOR HZ" }, { "tail_color_harm", "HARMONIC", true } };
        if (selected == 0 && detailPage == 1) m = { { "psh_mid_freq", "MID FREQ" }, { "psh_low_cut", "LOW CUT" } };
        liveSpec->setMarkers (m);
        liveSpec->setVisible ((pageIdx == 0 && ! overview) && ! m.empty());
        liveSpec->setBounds (detailArea.getRight() - 14 - 230, y0 + Knob::heightFor (40) + 6, 230, Knob::heightFor (40));
    }
}

void GeCanvas::paint (Graphics& g)
{
    const auto& c = palette();
    if (c.glass) fge::gui::glassBackground (g, getLocalBounds().toFloat());     // SMOKE (optional, SETTINGS)
    else g.fillAll (c.ground);
    // the header: row 1 logo / blocks / presets, row 2 pages / STYLE / LENGTH / KICK END (GUI v2)
    g.setColour (c.glass ? Colours::black.withAlpha (0.35f) : c.panel.darker (0.12f)); g.fillRect (headerArea);
    g.setColour (c.line); g.drawHorizontalLine (headerArea.getBottom(), 0.f, (float) W);
    doomsday::brand::drawBadge (g, doomsday::brand::headerBadgeArea (1.f, 26.f), 0.55f, RectanglePlacement::xLeft | RectanglePlacement::yMid);
    const int tx = roundToInt (doomsday::brand::headerTextX (1.f));
    g.setColour (c.silver); g.setFont (lnf.head (24.f, 0.02f));
    g.drawText ("4GE", tx, 10, 90, 32, Justification::centredLeft);
    g.setColour (c.textDim); g.setFont (lnf.font (11.f, doomsday::flat::Medium));
    g.drawText ("v" + String (versionText), tx, 58, 60, 24, Justification::centredLeft);
    g.setFont (lnf.font (13.f, doomsday::flat::Medium));
    auto label = [&] (Component* comp, const char* t) { if (comp != nullptr) g.drawText (t, comp->getX() - 72, 52, 64, 36, Justification::centredRight); };
    label (&lengthSlider, "Length"); label (kickEndTop, "Kick end");
    // the INFO LINE = the status bar at the bottom (what is under the mouse)
    g.setColour (c.panel.darker (0.12f)); g.fillRect (infoArea);
    g.setColour (c.line); g.drawHorizontalLine (infoArea.getY(), 0.f, (float) W);
    if (uiSet.infoLine)
    {
        g.setColour (c.textDim); g.setFont (lnf.font (12.5f, doomsday::flat::Medium));
        g.drawFittedText (infoText, infoArea.withTrimmedLeft (20).withTrimmedRight (20), Justification::centredLeft, 1, 0.9f);
    }

    // panels: flat, a quiet title, no colour bars (GUI v2)
    auto panel = [&] (Rectangle<int> r, const String& title, Colour)
    {
        fge::gui::glassPanel (g, r.toFloat(), 5.f);
        if (title.isNotEmpty())
        {
            g.setColour (c.silver.withAlpha (0.75f)); g.setFont (lnf.head (10.5f));
            g.drawText (title.toUpperCase(), r.getX() + 14, r.getY() + 9, 300, 18, Justification::left);
        }
    };
    // MACROS + GLOBAL (every page)
    panel (macroArea, "Macros", c.red);
    for (auto* m : macroRow)
    {
        const auto h = Rectangle<float> ((float) m->getRight() - 4.f, (float) m->getY() + 4.f, 13.f, 13.f);
        g.setColour (c.control); g.fillRoundedRectangle (h, 3.f);
        g.setColour (c.textDim); g.setFont (lnf.font (11.f, doomsday::flat::Bold)); g.drawText (">", h, Justification::centred);
    }
    panel (globalArea, "", c.red);
    if (consoleShown()) paintConsole (g);
    else if (lanesShown()) paintLanes (g);
    else if (pageIdx == 0) paintMain (g, panel);
    // output meter
    {
        const float m = jlimit (0.f, 1.f, (Decibels::gainToDecibels (outMeter, -60.f) + 60.f) / 66.f);
        auto r = meterArea.reduced (8, 2);
        g.setColour (c.ground.darker (0.3f)); g.fillRect (r);
        g.setColour (outMeter > 1.f ? c.hot : c.silver.withAlpha (0.85f));
        g.fillRect (r.withTop (r.getBottom() - roundToInt ((float) r.getHeight() * m)));
        g.setColour (c.textDim); g.setFont (lnf.font (11.f, doomsday::flat::Medium));
        g.drawText ("Out", meterArea.getX() - 6, meterArea.getBottom() + 1, meterArea.getWidth() + 12, 14, Justification::centred);
    }
}

// ------------------------------------------------------------ GUI v2: the CONSOLE (owner 2026-11-19, Design/GUI_V2.md)
Slider* GeCanvas::fader (const char* id, Colour accent)
{
    auto s = std::make_unique<Slider> (Slider::LinearVertical, Slider::NoTextBox);
    s->getProperties().set ("accent", (int64) accent.getARGB());
    s->setSliderSnapsToMousePosition (false);
    faderAtt.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *s));
    if (auto* p = proc.apvts.getParameter (id)) s->setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    s->onValueChange = [this] { repaint(); };
    auto* raw = s.get(); addChildComponent (raw); byId.insert ({ id, raw }); owned.push_back (std::move (s)); return raw;
}

void GeCanvas::buildConsole()
{
    const auto& c = palette();
    const Colour grey (0xffa0a5ad);
    auto K = [this] (const char* id, const char* lab, Colour col) -> Component*
    {
        auto* k = knob (id, lab, col, 34); k->setVisible (false);
        k->slider.getProperties().set ("valueSize", 11.5f);                     // (a strip is narrow: a smaller value)
        k->slider.setTextBoxStyle (Slider::TextBoxBelow, false, 64, 16);
        return k;
    };
    auto hide = [] (Component* comp) { comp->setVisible (false); return comp; };
    auto ms = [&] (const char* part, Colour col) -> std::vector<Component*>
    {
        return { hide (toggle (("mix_" + String (part) + "_mute").toRawUTF8(), "M", Toggle::Chip, col)),
                 hide (toggle (("mix_" + String (part) + "_solo").toRawUTF8(), "S", Toggle::Chip, col)) };
    };
    auto& s = strips;
    s[0].name = "TOK";     s[0].col = grey;      s[0].type = hide (combo ("tk_type"));
    s[0].knobs = { K ("tk_tune", "Pitch", grey), K ("tk_length", "Length", grey), K ("tk_drive", "Drive", grey) };
    s[0].fader = fader ("tk_level", grey);       s[0].buttons = { hide (seg ("tk_mode", { "AUTO", "ON", "OFF" }, grey)) };
    s[0].open = [this] { selectPart (0); detailPage = 5; layoutDetail(); repaint(); };
    s[1].name = "CLICK";   s[1].col = grey;      s[1].type = hide (combo ("clk_type"));
    s[1].knobs = { K ("clk_tone", "Tone", grey), K ("clk_length", "Length", grey), K ("clk_drive", "Drive", grey) };
    s[1].fader = fader ("clk_level", grey);      s[1].buttons = { hide (seg ("clk_mode", { "AUTO", "ON", "OFF" }, grey)) };
    s[1].open = s[0].open;
    s[2].name = "PUNCH";   s[2].col = c.part[0]; s[2].type = hide (combo ("punch_mode"));
    s[2].knobs = { K ("punch_pitch", "Pitch", c.part[0]), K ("punch_length", "Length", c.part[0]), K ("punch_drive", "Drive", c.part[0]) };
    s[2].fader = fader ("mix_punch_level", c.part[0]); s[2].buttons = ms ("punch", c.part[0]); s[2].meter = 0;
    s[2].open = [this] { selectPart (0); };
    s[3].name = "BODY";    s[3].col = c.part[1];
    s[3].knobs = { K ("body_pitch", "Pitch", c.part[1]), K ("body_length", "Length", c.part[1]), K ("body_drive", "Drive", c.part[1]) };
    s[3].fader = fader ("mix_body_level", c.part[1]); s[3].buttons = ms ("body", c.part[1]); s[3].meter = 1;
    s[3].open = [this] { selectPart (1); };
    s[4].name = "TAIL";    s[4].col = c.part[2]; s[4].type = hide (combo ("tail_engine"));
    s[4].knobs = { K ("tail_length", "Length", c.part[2]), K ("tail_drive", "Drive", c.part[2]), K ("tail_tone", "Tone", c.part[2]) };
    s[4].fader = fader ("mix_tail_level", c.part[2]); s[4].buttons = ms ("tail", c.part[2]); s[4].meter = 2;
    s[4].open = [this] { selectPart (2); };
    s[5].name = "SUB";     s[5].col = c.part[3];
    s[5].knobs = { K ("sub_pitch", "Pitch", c.part[3]), K ("sub_length", "Length", c.part[3]), K ("sub_drive", "Drive", c.part[3]) };
    s[5].fader = fader ("mix_sub_level", c.part[3]); s[5].buttons = ms ("sub", c.part[3]); s[5].meter = 3;
    s[5].open = [this] { selectPart (3); };
    s[6].name = "SCREECH"; s[6].col = c.part[4]; s[6].type = hide (combo ("scr_mode"));
    s[6].knobs = { K ("scr_base_hz", "Freq", c.part[4]), K ("scr_length", "Length", c.part[4]), K ("scr_fb_drive", "Drive", c.part[4]) };
    s[6].fader = fader ("scr_level", c.part[4]); s[6].buttons = { hide (toggle ("scr_on", "ON", Toggle::Chip, c.part[4])) };
    s[6].open = [this] { showPage (5); };
    pitchSide = { hide (combo ("pe_mode")), hide (toggle ("core_key_track", "Tracking", Toggle::Pill, c.red)),
                  K ("pe_start", "Start", c.silver), K ("pe_end", "End", c.silver), K ("pe_time", "Time", c.silver), K ("pe_curve", "Curve", c.silver),
                  hide (seg ("pe_start_mode", { "FOLLOW MIDI", "FIXED" }, c.red)) };
    masterSide = { K ("master_clip_drive", "Clip", c.silver), K ("master_clip_soft", "Soft", c.silver) };

    // ---- LANES (view B): a level slider per lane, the inspector knobs of each part (PUNCH / TAIL = the knobs of their TYPE)
    const char* levelIds[7] { "tk_level", "clk_level", "mix_punch_level", "mix_body_level", "mix_tail_level", "mix_sub_level", "scr_level" };
    for (int i = 0; i < 7; ++i)
    {
        auto sl = std::make_unique<Slider> (Slider::LinearHorizontal, Slider::NoTextBox);
        sl->getProperties().set ("accent", (int64) strips[(size_t) i].col.getARGB());
        sl->setSliderSnapsToMousePosition (false);
        faderAtt.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, levelIds[i], *sl));
        sl->onValueChange = [this] { repaint(); };
        laneLevel[(size_t) i] = sl.get(); addChildComponent (sl.get()); byId.insert ({ levelIds[i], sl.get() }); owned.push_back (std::move (sl));
    }
    using LI = std::initializer_list<std::pair<const char*, const char*>>;
    auto insp = [&] (int lane, LI l)
    {
        for (auto [id, lab] : l) { auto* k = knob (id, lab, strips[(size_t) lane].col, 40); k->setVisible (false); inspector[(size_t) lane].push_back (k); }
    };
    insp (0, { { "tk_tune", "Pitch" }, { "tk_length", "Length" }, { "tk_drive", "Drive" }, { "tk_tone", "Tone" },
               { "tk_peak", "Peak" }, { "tk_fade", "Fade" }, { "tk_penv_start", "Bend start" }, { "tk_penv_end", "Bend end" }, { "tk_penv_time", "Bend time" } });
    insp (1, { { "clk_tone", "Tone" }, { "clk_length", "Length" }, { "clk_drive", "Drive" }, { "clk_decay", "Decay" }, { "clk_pitch", "Pitch" }, { "clk_mix", "Mix" } });
    insp (3, { { "body_level", "Level" }, { "body_pitch", "Pitch" }, { "body_length", "Length" }, { "body_decay", "Decay" }, { "body_drive", "Drive" }, { "body_tone", "Tone" } });
    insp (5, { { "sub_level", "Level" }, { "sub_pitch", "Pitch" }, { "sub_length", "Length" }, { "sub_decay", "Decay" }, { "sub_drive", "Drive" } });
    insp (6, { { "scr_level", "Level" }, { "scr_base_hz", "Freq" }, { "scr_length", "Length" }, { "scr_decay", "Decay" }, { "scr_fb_drive", "Drive" }, { "scr_width", "Width" } });
    viewConsole.setClickingTogglesState (false); viewLanes.setClickingTogglesState (false);
    viewConsole.setTooltip ("MAIN as a mixing console: one strip per part of the kick.");
    viewLanes.setTooltip ("MAIN as lanes: every part as a track with its real envelope, the selected part's knobs on the right.");
    viewConsole.onClick = [this] { setMainView (0); };
    viewLanes.onClick = [this] { setMainView (1); };
    inspAll.onClick = [this] { if (strips[(size_t) laneSel].open) strips[(size_t) laneSel].open(); };
    inspAll.setTooltip ("Every parameter of this part (the full view with its tabs).");
    for (auto* b : { &viewConsole, &viewLanes, &inspAll }) addChildComponent (b);
    helpBtn.setTooltip ("Help: navigation, knobs, modulation, files and shortcuts. (Test build - not the final version.)");
    helpBtn.onClick = [this] { openHelp(); };
    addAndMakeVisible (helpBtn);
    help = std::make_unique<fge::gui::HelpPanel> (lnf, String ("v") + versionText);
    addChildComponent (help.get());
    wave.onRendered = [this] { if (lanesShown()) repaint(); };
}

void GeCanvas::openHelp()
{
    help->setBounds (getLocalBounds());
    help->show();
}

std::vector<Component*> GeCanvas::inspectorNow() const
{
    if (laneSel == 2 && punchTypeIdx() == 4)                                       // PUNCH TYPE RAW: only the picked architecture's knobs
    {
        const String keep = (int) raw (proc, P::rpType) == 1 ? "rk_" : "rc_";
        std::vector<Component*> v;
        for (auto* c : ovPunch[4]) for (auto& [id, comp] : byId) if (comp == c && id.startsWith (keep)) { v.push_back (c); break; }
        return v;
    }
    if (laneSel == 2) return ovPunch[(size_t) punchTypeIdx()];
    if (laneSel == 4) return ovTail[(size_t) tailTypeIdx()];
    return inspector[(size_t) laneSel];
}
void GeCanvas::setMainView (int v)
{
    uiSet.mainView = jlimit (0, 1, v);
    if (isShowing()) uiSet.save();
    overview = true;
    showPage (0);
}
void GeCanvas::selectLane (int i)
{
    laneSel = jlimit (0, 6, i);
    applyOverview();
    repaint();
}

void GeCanvas::layoutLanes()
{
    const int inspW = 340, gap = 12, waveH = 150;
    const int areaW = CW - inspW - gap;
    if (lanesShown()) wave.setBounds (X0, Y0, areaW, waveH);
    inspArea = { X0 + areaW + gap, Y0, inspW, CH };
    const int top = Y0 + waveH + gap, lh = (CH - waveH - gap - 6 * 6) / 7;
    for (int i = 0; i < 7; ++i)
    {
        auto& st = strips[(size_t) i];
        const auto a = laneArea[(size_t) i] = { X0, top + i * (lh + 6), areaW, lh };
        if (! lanesShown()) continue;
        if (st.type != nullptr) st.type->setBounds (a.getX() + 14, a.getY() + lh / 2 + 2, 118, 22);
        laneLevel[(size_t) i]->setBounds (a.getX() + 146, a.getY() + lh / 2 + 2, 112, 22);
        if (st.buttons.size() == 2) { st.buttons[0]->setBounds (a.getX() + 270, a.getY() + lh / 2 - 10, 28, 20); st.buttons[1]->setBounds (a.getX() + 302, a.getY() + lh / 2 - 10, 28, 20); }
        else if (st.buttons.size() == 1) st.buttons[0]->setBounds (a.getX() + 268, a.getY() + lh / 2 - 10, 84, 20);
    }
    auto v = inspectorNow();
    for (size_t k = 0; k < v.size(); ++k)
        v[k]->setBounds (inspArea.getX() + 10 + (int) (k % 4) * 80, inspArea.getY() + 50 + (int) (k / 4) * (Knob::heightFor (40) + 14), 80, Knob::heightFor (40));
    inspAll.setBounds (inspArea.getX() + 14, inspArea.getBottom() - 40, inspArea.getWidth() - 28, 26);
}

void GeCanvas::paintLanes (Graphics& g)
{
    const auto& c = palette();
    fge::gui::glassPanel (g, inspArea.toFloat(), 5.f);
    {
        const auto& st = strips[(size_t) laneSel];
        g.setColour (st.col); g.fillRoundedRectangle ((float) inspArea.getX() + 14.f, (float) inspArea.getY() + 16.f, 8.f, 8.f, 2.f);
        g.setColour (c.silver); g.setFont (lnf.head (13.f));
        g.drawText (st.name, inspArea.getX() + 30, inspArea.getY() + 10, 240, 20, Justification::centredLeft);
    }
    const int headW = 360;
    for (int i = 0; i < 7; ++i)
    {
        const auto& st = strips[(size_t) i];
        const auto a = laneArea[(size_t) i];
        fge::gui::glassPanel (g, a.toFloat(), 5.f);
        if (i == laneSel) { g.setColour (c.red.withAlpha (0.7f)); g.drawRoundedRectangle (a.toFloat().reduced (0.5f), 5.f, 1.f); }
        g.setColour (st.col); g.fillRoundedRectangle ((float) a.getX() + 14.f, (float) a.getY() + 12.f, 8.f, 8.f, 2.f);
        g.setColour (c.silver); g.setFont (lnf.head (11.f));
        g.drawText (st.name, a.getX() + 28, a.getY() + 6, 120, 20, Justification::centredLeft);
        if (st.type == nullptr) { g.setColour (c.textDim); g.setFont (lnf.font (12.5f, doomsday::flat::Medium)); g.drawText (i == 3 ? "Tonal" : "Sine", a.getX() + 16, a.getY() + a.getHeight() / 2 + 2, 110, 22, Justification::centredLeft); }
        g.setColour (c.textDim); g.setFont (lnf.font (12.f, doomsday::flat::Medium));
        g.drawText ("Level", a.getX() + 146, a.getY() + 6, 50, 20, Justification::centredLeft);
        g.setColour (c.silver.withAlpha (0.85f));
        g.drawText (laneLevel[(size_t) i]->getTextFromValue (laneLevel[(size_t) i]->getValue()), a.getX() + 196, a.getY() + 6, 62, 20, Justification::centredRight);
        g.setColour (c.line); g.drawVerticalLine (a.getX() + headW, (float) a.getY() + 1.f, (float) a.getBottom() - 1.f);
        // the body: the part's real envelope (PUNCH / BODY / TAIL / SUB from the rendered kick), TOK / CLICK / SCREECH from their lengths
        const auto body = a.withTrimmedLeft (headW + 1).reduced (0, 6).toFloat();
        float ms = 500.f;
        std::vector<float> env;
        if (i >= 2 && i <= 5)
        {   // ~3 ms bins, then a short hold so one cycle of the tone does not show as a saw
            env = wave.partEnvelope (i - 2, 160, ms);
            auto held = env;
            for (size_t k = 0; k < env.size(); ++k) for (size_t j = k; j < jmin (env.size(), k + 3); ++j) held[k] = jmax (held[k], env[j]);
            env = held;
        }
        else wave.partEnvelope (0, 1, ms);
        if (ms < 50.f) ms = 500.f;
        g.setColour (c.line.withAlpha (0.5f));
        for (float t = 50.f; t < ms; t += 50.f) g.drawVerticalLine (roundToInt (body.getX() + body.getWidth() * t / ms), body.getY(), body.getBottom());
        if (env.empty())
        {   // a simple shape from the part's own timing
            const String pre = i == 0 ? "tk_" : i == 1 ? "clk_" : "scr_";
            const float len = proc.apvts.getRawParameterValue (pre + "length")->load();
            const float start = i == 6 ? jmax (0.f, proc.apvts.getRawParameterValue ("scr_start")->load()) : 0.f;
            const bool on = i == 6 ? raw (proc, P::scrOn) > 0.5f : (int) proc.apvts.getRawParameterValue (pre + "mode")->load() != 2;
            env.assign ((size_t) roundToInt (body.getWidth() / 2.f), 0.f);
            for (size_t k = 0; k < env.size(); ++k)
            {
                const float t = ms * (float) k / (float) env.size() - start;
                env[k] = ! on || t < 0.f || t > len ? 0.f : (t < 1.f ? t : 1.f - 0.85f * t / jmax (1.f, len));
            }
        }
        Path p; p.startNewSubPath (body.getX(), body.getBottom());
        for (size_t k = 0; k < env.size(); ++k)
            p.lineTo (body.getX() + body.getWidth() * (float) k / (float) jmax ((size_t) 1, env.size() - 1), body.getBottom() - body.getHeight() * jlimit (0.f, 1.f, env[k]));
        p.lineTo (body.getRight(), body.getBottom()); p.closeSubPath();
        g.setColour (st.col.withAlpha (0.16f)); g.fillPath (p);
        g.setColour (st.col.withAlpha (0.9f)); g.strokePath (p, PathStrokeType (1.3f));
    }
}

void GeCanvas::layoutConsole()
{
    const int sideW = 262, gap = 12, waveH = 200;
    const int areaW = CW - sideW - gap, top = Y0 + waveH + gap;
    if (pageIdx == 0 && overview) wave.setBounds (X0, Y0, areaW, waveH);
    pitchPanel = { X0 + areaW + gap, Y0, sideW, waveH };
    masterPanel = { pitchPanel.getX(), top, sideW, Y0 + CH - top };
    const int sw = (areaW - 6 * 8) / 7, kh = Knob::heightFor (34);
    for (int i = 0; i < 7; ++i)
    {
        auto& st = strips[(size_t) i];
        st.area = { X0 + i * (sw + 8), top, sw, Y0 + CH - top };
        if (! consoleShown()) { st.nameArea = {}; continue; }
        const auto a = st.area.reduced (8, 0);
        if (st.type != nullptr) st.type->setBounds (a.getX(), a.getY() + 10, a.getWidth(), 24);
        const int kw = (st.area.getWidth() - 4) / 3;                              // (the value boxes may overlap a little)
        for (size_t k = 0; k < st.knobs.size(); ++k) st.knobs[k]->setBounds (st.area.getX() + 2 + (int) k * kw - 5, a.getY() + 42, kw + 10, kh);
        st.nameArea = { st.area.getX(), st.area.getBottom() - 40, st.area.getWidth(), 40 };
        const int by = st.nameArea.getY() - 30;
        if (st.buttons.size() == 2) { st.buttons[0]->setBounds (st.area.getCentreX() - 31, by, 28, 20); st.buttons[1]->setBounds (st.area.getCentreX() + 3, by, 28, 20); }
        else if (st.buttons.size() == 1) st.buttons[0]->setBounds (a.getX(), by, a.getWidth(), 20);
        const int fy = a.getY() + 42 + kh + 10;
        st.faderArea = { a.getX(), fy, a.getWidth(), by - 10 - fy };
        if (st.fader != nullptr) st.fader->setBounds (st.faderArea.getCentreX() - 10, fy, 30, st.faderArea.getHeight());
    }
    const auto pp = pitchPanel;
    pitchSide[0]->setBounds (pp.getX() + 14, pp.getY() + 36, 140, 24);
    pitchSide[1]->setBounds (pp.getX() + 164, pp.getY() + 38, 86, 20);
    for (int k = 0; k < 4; ++k) pitchSide[(size_t) k + 2]->setBounds (pp.getX() + 8 + k * 62, pp.getY() + 68, 62, Knob::heightFor (34));
    pitchSide[6]->setBounds (pp.getX() + 14, pp.getBottom() - 32, pp.getWidth() - 28, 22);
    const auto mp = masterPanel;
    masterSide[0]->setBounds (mp.getX() + 10, mp.getY() + 36, 80, Knob::heightFor (34));
    masterSide[1]->setBounds (mp.getX() + 92, mp.getY() + 36, 80, Knob::heightFor (34));
    outMeterArea = { mp.getRight() - 50, mp.getY() + 40, 30, mp.getHeight() - 74 };
}

void GeCanvas::paintConsole (Graphics& g)
{
    const auto& c = palette();
    auto title = [&] (Rectangle<int> r, const char* t)
    {
        fge::gui::glassPanel (g, r.toFloat(), 5.f);
        g.setColour (c.silver.withAlpha (0.75f)); g.setFont (lnf.head (10.5f));
        g.drawText (String (t).toUpperCase(), r.getX() + 14, r.getY() + 9, 200, 18, Justification::left);
    };
    title (pitchPanel, "Pitch");
    title (masterPanel, "Master");
    {   // the output meter (big)
        const float m = jlimit (0.f, 1.f, (Decibels::gainToDecibels (outMeter, -60.f) + 60.f) / 66.f);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto r = outMeterArea.withWidth (12).translated (ch * 16, 0);
            g.setColour (c.ground.darker (0.3f)); g.fillRect (r);
            g.setColour (outMeter > 1.f ? c.hot : c.silver.withAlpha (0.85f));
            g.fillRect (r.withTop (r.getBottom() - roundToInt ((float) r.getHeight() * m)));
        }
        g.setColour (c.silver); g.setFont (lnf.font (13.f, doomsday::flat::SemiBold));
        const float db = Decibels::gainToDecibels (outMeter, -60.f);
        g.drawText (db <= -59.f ? String ("-inf") : String (db, 1) + " dB", outMeterArea.getX() - 70, outMeterArea.getBottom() + 6, 100, 18, Justification::centredRight);
    }
    for (auto& st : strips)
    {
        fge::gui::glassPanel (g, st.area.toFloat(), 5.f);
        // the name bar (the scribble strip): click = every parameter of the part
        const auto nb = st.nameArea;
        g.setColour (c.ground.darker (0.15f)); g.fillRect (nb.reduced (1, 0).withTrimmedBottom (1));
        g.setColour (st.col); g.fillRect (nb.getX() + 1, nb.getY(), nb.getWidth() - 2, 2);
        g.setColour (c.silver); g.setFont (lnf.head (11.5f));
        g.drawText (st.name, nb.withTrimmedTop (3).withHeight (20), Justification::centred);
        if (st.fader != nullptr)
        {
            g.setColour (c.textDim); g.setFont (lnf.font (11.5f, doomsday::flat::Medium));
            g.drawText (st.fader->getTextFromValue (st.fader->getValue()), nb.withTrimmedTop (21).withHeight (16), Justification::centred);
            // the dB scale left of the fader
            auto* f = st.fader;
            g.setFont (lnf.font (10.5f, doomsday::flat::Medium));
            float lastY = -100.f;
            for (double v : { 6.0, 0.0, -6.0, -12.0, -24.0, -48.0 })
            {
                if (v < f->getMinimum() || v > f->getMaximum()) continue;
                const float y = (float) f->getY() + (float) f->getPositionOfValue (v);
                if (y - lastY < 16.f) continue;                                  // (a skewed range: no labels on top of each other)
                lastY = y;
                g.setColour (c.line.brighter (0.3f)); g.drawHorizontalLine (roundToInt (y), (float) f->getX() - 12.f, (float) f->getX() - 2.f);
                g.setColour (c.textDim.withAlpha (0.8f));
                g.drawText (v > 0 ? "+" + String ((int) v) : String ((int) v), f->getX() - 44, roundToInt (y) - 7, 30, 14, Justification::centredRight);
            }
            if (st.meter >= 0)
            {
                const float m = jlimit (0.f, 1.f, (Decibels::gainToDecibels (meter[(size_t) st.meter], -60.f) + 60.f) / 66.f);
                auto r = Rectangle<int> (f->getRight() + 4, f->getY() + 15, 5, f->getHeight() - 30);
                g.setColour (c.ground.darker (0.3f)); g.fillRect (r);
                g.setColour (c.silver.withAlpha (0.7f)); g.fillRect (r.withTop (r.getBottom() - roundToInt ((float) r.getHeight() * m)));
            }
        }
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
    if (selected == 2) if (auto* cb = findControl ("tail_st2_type"); cb != nullptr && cb->isVisible())
    {
        g.setColour (c.textDim); g.setFont (lnf.font (10.5f, doomsday::flat::Bold, 0.06f));
        g.drawText ("STAGE 2", cb->getX(), cb->getY() - 18, 150, 14, Justification::left);
    }
    if (selected == 2)
        for (auto [id, t] : std::initializer_list<std::pair<const char*, const char*>> { { "tail_room_input", "ROOM INPUT" }, { "tail_color_track", "COLOR MODE" }, { "tail_start_mode", "TAIL START" } })
            if (auto* cb = findControl (id); cb != nullptr && cb->isVisible())
            {
                g.setColour (c.textDim); g.setFont (lnf.font (10.5f, doomsday::flat::Bold, 0.06f));
                g.drawText (t, cb->getX(), cb->getY() - 18, 150, 14, Justification::left);
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
    const bool tok = (int) raw (proc, P::punchMode) == 1;                  // PUNCH / TOK: what only the TOK mode uses
    for (auto* id : { "punch_note_mode", "punch_overlap", "punch_tok_decay", "punch_tok_bend", "tail_start_mode" }) en (id, tok);
    const int nMode = (int) raw (proc, P::punchNoteMode);
    en ("punch_note_offset", tok && nMode == 2);
    {
        const bool shp = (int) raw (proc, P::punchMode) >= 2, solo = raw (proc, P::pshTonalOnly) > 0.5f;
        for (auto* id : { "psh_source", "psh_transpose", "psh_mid_freq", "psh_mid_gain", "psh_drive", "psh_asym", "psh_tape", "psh_spike", "psh_body",
                          "psh_body_ms", "psh_end", "psh_low_cut", "psh_verb", "psh_tonal_only" }) en (id, shp);
        en ("psh_tape_start", shp && raw (proc, P::pshTape) > 0.001f);
        en ("psh_click", shp && ! solo); en ("psh_sub", shp && ! solo); en ("psh_tonal", shp);
        const bool ck = shp && ! solo && raw (proc, P::pshClick) > 0.001f, sb = shp && ! solo && raw (proc, P::pshSub) > 0.001f;
        for (auto* id : { "psh_click_len", "psh_click_tone" }) en (id, ck);
        for (auto* id : { "psh_sub_delay", "psh_sub_wave", "psh_sub_start", "psh_sub_drop", "psh_sub_decay" }) en (id, sb);
        const bool pvc = (int) raw (proc, P::punchMode) == 3, eup = (int) raw (proc, P::punchMode) == 2;
        for (auto* id : { "psh_source", "psh_transpose", "psh_mid_freq", "psh_mid_gain", "psh_drive", "psh_asym", "psh_tape", "psh_tape_start",
                          "psh_spike", "psh_body", "psh_body_ms", "psh_end", "psh_low_cut" }) en (id, eup);
        for (int i = 0; i < P::count; ++i)
            if (String (def (i).id).startsWith ("pvc_")) en (def (i).id, pvc);
        en ("pvc_ring_ratio", pvc && (int) raw (proc, P::pvcMaterial) == 0);
        const bool tube = pvc && (int) raw (proc, P::pvcModel) == 1;               // PVC: KLOENK vs TUBE (owner 2026-11-14)
        for (auto* id : { "pvc_source", "pvc_pitch", "pvc_res_freq", "pvc_res_q", "pvc_res_amount", "pvc_material", "pvc_mix", "pvc_ring_ratio", "pvc_comp", "pvc_sat", "pvc_asym" })
            if (tube) en (id, false);
        for (auto* id : { "pvc_tube_hz", "pvc_tube_comb", "pvc_jump", "pvc_drop", "pvc_drop_time", "pvc_phaser", "pvc_ph_mode", "pvc_ph_rate", "pvc_ph_fb", "pvc_bright" }) en (id, tube);
        en ("pvc_drop_time", tube && raw (proc, P::pvcDrop) < -0.01f);
        en ("pvc_ph_mode", tube && raw (proc, P::pvcPhaser) > 0.001f); en ("pvc_ph_fb", tube && raw (proc, P::pvcPhaser) > 0.001f);
        en ("pvc_ph_rate", tube && raw (proc, P::pvcPhaser) > 0.001f && (int) raw (proc, P::pvcPhMode) == 1);
        const bool zg = (int) raw (proc, P::tailEngine) == 2, zp = (int) raw (proc, P::punchMode) == 4;   // TAIL TYPE ZAAG / PUNCH TYPE ZAAG (rule 0)
        auto isPunchZ = [] (const String& id) { return id.startsWith ("zg_click") || id.startsWith ("zg_piep") || id.startsWith ("zg_trask") || id.startsWith ("zg_verb") || id.startsWith ("zg_thump") || id.startsWith ("zg_pup"); };
        for (int i = 0; i < P::count; ++i)
            if (const String id (def (i).id); id.startsWith ("zg_")) en (id, isPunchZ (id) ? zp : zg);
        for (auto* id : { "zg_piep_hz", "zg_piep_ms" }) en (id, zp && raw (proc, P::zgPiep) > 0.001f);
        for (auto* id : { "zg_trask_delay", "zg_trask_ms", "zg_trask_tone" }) en (id, zp && raw (proc, P::zgTrask) > 0.001f);
        for (auto* id : { "zg_thump_hz", "zg_thump_ms", "zg_thump_drive" }) en (id, zp && raw (proc, P::zgThump) > 0.001f);
        for (auto* id : { "zg_pup_hz", "zg_pup_ms", "zg_pup_oct", "zg_pup_drive" }) en (id, zp && raw (proc, P::zgPup) > 0.001f);
        for (auto [lv, pre, md, au] : std::initializer_list<std::tuple<int, const char*, int, int>> { { P::tkLevel, "tk_", P::tkMode, P::tkAuto },
                                                                                                    { P::clkLevel, "clk_", P::clkMode, P::clkAuto }, { P::spkLevel, "spk_", P::spkMode, P::spkAuto } })
        {   // THE FRONT: a module's knobs wake up when it plays (AUTO = the style's / preset's choice, ON, OFF) and has a LEVEL
            const int m = (int) raw (proc, md);
            const bool wants = md == P::tkMode || fge::dsp::front::profileFor ((int) raw (proc, P::punchMode)).globalInAuto;   // (AUTO: the PUNCH TYPE profile)
            const bool plays = m == 1 || (m == 0 && raw (proc, au) > 0.5f && wants);
            for (int i = 0; i < P::count; ++i)
                if (const String id (def (i).id); id.startsWith (pre) && ! id.endsWith ("_mode") && ! id.endsWith ("_auto"))
                    en (id, plays && (id.endsWith ("_level") || raw (proc, lv) > -59.9f));
        }
        if (raw (proc, P::tkLevel) > -59.9f) en ("tk_tune", (int) raw (proc, P::tkPitchMode) != 1);   // FOLLOW MIDI: no PITCH
        for (auto* id : { "zg_verb_decay", "zg_verb_size", "zg_verb_send", "zg_verb_pre", "zg_verb_gate", "zg_verb_damp", "zg_verb_low_cut" }) en (id, zp && raw (proc, P::zgVerb) > 0.001f);
        for (auto* id : { "zg_rsn_freq", "zg_rsn_reso", "zg_rsn_width", "zg_rsn_pos", "zg_rsn_mode" }) en (id, zg && raw (proc, P::zgRsnMix) > 0.001f);
        en ("zg_side_boost_hz", zg && raw (proc, P::zgSideBoost) > 0.01f);
        en ("zg_length", zg && raw (proc, P::zgLenFollow) < 0.5f);
        {   // TAIL TYPE RAW: its knobs live with RAW; a switched-off stage greys its own knobs
            const bool rw = (int) raw (proc, P::tailEngine) == 3;
            for (int i = 0; i < P::count; ++i)
                if (const String id (def (i).id); id.startsWith ("rw_") || id.startsWith ("tl_")) en (id, rw);
            auto stageEn = [&] (int sw, std::initializer_list<const char*> ids) { for (auto* id : ids) en (id, rw && raw (proc, sw) > 0.5f); };
            stageEn (P::rwPreOn, { "rw_low_cut", "rw_pre_mid" });
            stageEn (P::rwD1On, { "rw_d1_type", "rw_d1_drive" });
            stageEn (P::rwShapeOn, { "rw_growl_hz", "rw_growl_db", "rw_growl_q" });
            stageEn (P::rwD2On, { "rw_d2_type", "rw_d2_drive" });
            stageEn (P::rwBpOn, { "rw_flt_shape", "rw_flt_start", "rw_flt_end", "rw_flt_mid", "rw_flt_reso", "rw_flt_speed", "rw_flt_time", "rw_flt_curve", "rw_flt_mix" });
            stageEn (P::rwD3On, { "rw_d3_type", "rw_d3_drive" });
            stageEn (P::rwPostOn, { "rw_post_low_cut", "rw_post_high" });
            stageEn (P::rwClipOn, { "rw_clip_hard" });
            const bool bpOn = rw && raw (proc, P::rwBpOn) > 0.5f; const int sy = (int) raw (proc, P::rwSync);
            en ("rw_flt_mid", bpOn && (int) raw (proc, P::rwFltShape) == 4);
            en ("rw_flt_time", bpOn && sy == 1); en ("rw_flt_speed", bpOn && sy != 1);
            for (auto* id : { "rw_osc2_level", "rw_osc2_tune" }) en (id, rw && (int) raw (proc, P::rwOsc2) != 3);
            en ("rw_noise_level", rw && (int) raw (proc, P::rwNoise) != 2);
            for (auto* id : { "rw_verb_size", "rw_verb_decay", "rw_verb_damp", "rw_verb_pre", "rw_verb_low_cut" }) en (id, rw && raw (proc, P::rwVerb) > 0.001f);
        }
        {   // PUNCH TYPE CLASSIC (EARLY): its knobs live with it
            const bool cp = (int) raw (proc, P::punchMode) == 5;
            for (int i = 0; i < P::count; ++i) if (const String id (def (i).id); id.startsWith ("cp_")) en (id, cp);
            for (auto* id : { "cp_trans_ms", "cp_trans_hz" }) en (id, cp && raw (proc, P::cpTrans) > 0.001f);
            for (auto* id : { "cp_res_hz", "cp_res_q" }) en (id, cp && raw (proc, P::cpRes) > 0.001f);
            en ("cp_noise_hz", cp && raw (proc, P::cpNoise) > 0.001f);
        }
        {   // TAIL TYPE EXPERIMENTAL: its knobs live with it; an OFF slot greys its controls
            const bool ex = (int) raw (proc, P::tailEngine) == 4;
            for (int i = 0; i < P::count; ++i)
                if (const String id (def (i).id); id.startsWith ("ex_")) en (id, ex);
            if ((int) raw (proc, P::tailEngine) == 4) for (int i = 0; i < P::count; ++i) if (String (def (i).id).startsWith ("tl_")) en (def (i).id, true);
            for (int n = 1; n <= 8; ++n)
            {
                const bool on = ex && (int) proc.apvts.getRawParameterValue ("ex_s" + String (n) + "_type")->load() != 0;
                for (auto* k : { "_mix", "_amount", "_param", "_mod" }) en ("ex_s" + String (n) + k, on);
                en ("ex_s" + String (n) + "_depth", on && (int) proc.apvts.getRawParameterValue ("ex_s" + String (n) + "_mod")->load() != 0);
            }
            en ("ex_wt_pos", ex && (int) raw (proc, P::exOsc) == 4);
            for (auto* id : { "ex_fm_amount", "ex_fm_ratio", "ex_fm_mod" }) en (id, ex && raw (proc, P::exFmLvl) > 0.001f);
            en ("ex_noise", ex && raw (proc, P::exNoiseLvl) > 0.001f);
            en ("ex_mid", ex && (int) raw (proc, P::exPCurve) == 7);
        }
        const bool gtd = (int) raw (proc, P::tailEngine) == 1;                     // TAIL ENGINE PVC
        for (auto* id : { "gt_gate", "gt_gate_len", "gt_gate_depth", "gt_comp", "gt_filter", "gt_reso", "gt_move_rate", "gt_move_depth" }) en (id, gtd);
        for (int i = 0; i < P::count; ++i)
            if (String (def (i).id).startsWith ("pt_")) en (def (i).id, gtd);
        const bool osc = gtd && (int) raw (proc, P::ptSource) != 0;
        for (auto* id : { "pt_start", "pt_hold", "pt_drop_time", "pt_root", "pt_slide" }) en (id, osc);
        en ("pt_res_q", gtd && raw (proc, P::ptResAmount) > 0.001f); en ("pt_res_freq", gtd && raw (proc, P::ptResAmount) > 0.001f);
        for (auto* id : { "pt_rev_size", "pt_rev_decay", "pt_rev_pre" }) en (id, gtd && raw (proc, P::ptRevWet) > 0.001f);
        en ("pt_side", gtd && raw (proc, P::ptRevWet) > 0.001f);
        const int gm = (int) raw (proc, P::gtGate);
        en ("gt_gate_div", gtd && gm != 0); en ("pt_movement", gtd && gm != 0);
        for (auto* id : { "punch_source", "punch_attack", "punch_decay", "punch_sustain", "punch_release", "punch_tone", "punch_mix", "punch_glide", "punch_transient", "punch_pitch", "punch_length", "punch_drive", "punch_phase", "punch_transient_len", "punch_transient_tone" })
            if (shp) en (id, false);
    }
    en ("tail_start", tok && (int) raw (proc, P::tailStartMode) == 1);
    for (auto* id : { "tail_room_decay", "tail_room_tone", "tail_room_delay", "tail_room_input" }) en (id, raw (proc, P::tailRoom) > 0.001f);
    {
        const bool col = std::abs (raw (proc, P::tailColorGain)) > 0.05f, trk = (int) raw (proc, P::tailColorTrack) == 1;
        for (auto* id : { "tail_color_q", "tail_color_track" }) en (id, col);
        en ("tail_color_freq", col && ! trk); en ("tail_color_harm", col && trk);
    }
    en ("punch_glide", ! tok || std::abs (raw (proc, P::punchTokBend)) > 0.01f);   // TOK: GLIDE = the time of its BEND
    if (auto* lk = dynamic_cast<Knob*> (card[0][2])) lk->setLabel (tok ? "TOK LENGTH" : "LENGTH");
    const bool tr = raw (proc, P::punchTrLevel) > 0.001f;
    for (auto* id : { "punch_transient_len", "punch_transient_tone" }) en (id, tr);
    const bool tn = tok && nMode != 2;
    if (tokNote.isEnabled() != tn) { tokNote.setEnabled (tn); tokNote.setAlpha (tn ? 1.f : 0.35f); }
}

std::vector<int> GeCanvas::allowedTabs() const                                // ARCHITECTURE.md: the tabs of the picked PUNCH TYPE / TAIL TYPE
{
    if (selected == 0)
    {
        const int m = (int) raw (proc, P::punchMode);
        if (m == 4) return { 4, 5 };                                              // PUNCH TYPE ZAAG + THE FRONT
        if (m == 5) return { 6, 5 };                                              // PUNCH TYPE CLASSIC (EARLY) + THE FRONT
        if (m == 6) return { 7, 8, 5 };                                           // PUNCH TYPE RAW + its CRACK extension + THE FRONT
        return { m <= 1 ? 0 : m == 2 ? 1 : 2, 3, 5 };                               // + SUB / BLEND + THE FRONT (TOK / CLICK / SUB PUNCH)
    }
    if (selected == 2)
    {
        const int e = (int) raw (proc, P::tailEngine);
        if (e == 1) return { 2, 3, 16 };                                          // (+ TAIL RISE with every TAIL TYPE)
        if (e == 2) return { 4, 5, 6, 16 };
        if (e == 3) return { 7, 8, 9, 16 };                                       // TAIL TYPE RAW
        if (e == 4) return { 10, 11, 12, 13, 14, 15, 16 };                        // TAIL TYPE EXPERIMENTAL
        return { 0, 1, 16 };
    }
    return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
}
bool GeCanvas::hiddenByVariant (Component* c) const
{
    // ZAAG VARIANT (owner 2026-11-15): each variant shows only its own knobs. Everything still works underneath (presets, automation);
    // RESONANT shows the whole sweep + resonator, the others hide what they do not use.
    if (selected == 0 && (int) raw (proc, P::punchMode) == 6)                         // PUNCH TYPE RAW: only the picked architecture's knobs
    {
        const bool cracking = (int) raw (proc, P::rpType) == 1, fixed = (int) raw (proc, P::ckPitchMode) == 1;
        for (auto& [id, comp] : byId)
        {
            if (comp != c) continue;
            if (id.startsWith ("rc_") || id.startsWith ("rk_")) return cracking ? id.startsWith ("rc_") : id.startsWith ("rk_");
            if (id.startsWith ("cr_") && id != "cr_on" && id != "cr_model" && id != "cr_level")              // CRACK MODEL: only its own knobs
            {
                static const juce::StringArray tc { "cr_listen", "cr_rv_on", "cr_rv_route", "cr_rv_mix", "cr_rv_size", "cr_rv_decay", "cr_rv_predelay", "cr_rv_early",
                    "cr_rv_late", "cr_rv_diff", "cr_rv_damp", "cr_rv_gate", "cr_rv_drive_type", "cr_rv_drive", "cr_rv_hp", "cr_rv_lp", "cr_rv_res", "cr_hi_gain", "cr_hi_freq", "cr_mv_sweep", "cr_mv_flange", "cr_mv_phaser", "cr_mv_pitch", "cr_mv_rate", "cr_mv_time", "cr_source", "cr_raw", "cr_position", "cr_width", "cr_pitch_off", "cr_pitch_env", "cr_ws", "cr_asym", "cr_bp_freq", "cr_bp_width", "cr_bp_res", "cr_cbody" };
                const bool transientModel = (int) raw (proc, P::ckModel) == 0;
                if (tc.contains (id) != transientModel) return true;
                if (transientModel) return false;
            }
            if (id == "cr_tone_hz") return ! fixed;                                  // CRACK PITCH: FIXED PITCH only in FIXED, CRACK PITCH only in FOLLOW PUNCH
            if (id == "cr_pitch") return fixed;
        }
        return false;
    }
    if (selected != 2 || (int) raw (proc, P::tailEngine) != 2) return false;
    static const std::vector<const char*> hide[4] {
        { "zg_sw_reopen", "zg_sw_hold", "zg_rsn_reso", "zg_rsn_width", "zg_rsn_pos" },             // CLASSIC: the tutorial chain, a simple resonator
        {},                                                                                         // RESONANT: everything
        { "zg_sw_reopen", "zg_sw_end", "zg_rsn_pos", "zg_s3_preamp" },                             // BOUNCE: the movement, not the sweep's tail
        { "zg_sw_reopen", "zg_sw_hold", "zg_rsn_width", "zg_peak_q" } };                           // AGGRESSIVE: the distortion, not the resonance
    const int v = juce::jlimit (0, 3, (int) raw (proc, P::zgVariant));
    for (auto* id : hide[v])
    {
        auto r = byId.equal_range (id);
        for (auto it = r.first; it != r.second; ++it) if (it->second == c) return true;
    }
    return false;
}
void GeCanvas::timerCallback()
{
    {   // the TYPE changed (a click, undo, a preset): show its tabs
        const int pm = (int) raw (proc, P::punchMode), te = (int) raw (proc, P::tailEngine), zv = (int) raw (proc, P::zgVariant) + 10 * (int) raw (proc, P::rpType) + 100 * (int) raw (proc, P::ckPitchMode) + 1000 * (int) raw (proc, P::ckModel);
        if (pm != shownPunchType || te != shownTailType || zv != shownZgVariant)
        { shownPunchType = pm; shownTailType = te; shownZgVariant = zv; layoutDetail(); applyOverview(); repaint(); }
    }
    presetBar.refresh();
    syncTokNote();
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
            g.drawText ("Kick length", 12, 6, 400, 14, Justification::left);
            g.drawText ("Length sync", 12, 112, 300, 14, Justification::left);
            g.drawText ("Each part", 12, 176, 400, 14, Justification::left);
            const char* nm[4] { "PUNCH", "BODY", "TAIL", "SUB" };
            for (int i = 0; i < 4; ++i) { g.setColour (palette().part[(size_t) i]); g.drawText (nm[i], 12 + i * 125, 194, 120, 14, Justification::centred); }
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

void GeCanvas::syncTokNote()
{
    const float hz = raw (proc, P::punchPitch);
    const int m = (int) std::lround (69.0 + 12.0 * std::log2 (jmax (1.f, hz) / 440.0));
    if (tokNote.getSelectedId() != m + 1 && ! tokNote.isPopupActive()) tokNote.setSelectedId (jlimit (44, 101, m + 1), dontSendNotification);
}
