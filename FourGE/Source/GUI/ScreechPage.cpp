#include "ScreechPage.h"
#include "Palette.h"

using namespace juce;

namespace fge::gui
{
    namespace m6 = fge::dsp::m06;
    namespace sc = fge::dsp::scr;
    namespace
    {
        int idOf (const String& id) { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; }
        struct K { const char* id; const char* label; bool bip; };
        constexpr int kw = 70, ks = 74, kd = 26;                               // knob width / step / diameter
        Colour accent() { return palette().part[4]; }                           // SCREECH pink
    }

    float ScreechPage::raw (const String& id) const { auto* v = proc.apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.f; }

    Component* ScreechPage::knob (const String& id, const String& label, Colour col, bool bip, int d)
    {
        if (idOf (id) < 0) return nullptr;
        auto k = std::make_unique<doomsday::flat::Knob> (proc.apvts, id, label, col, bip, d);
        k->slider.setTooltip (fge::tooltip (idOf (id)));
        auto* r = k.get(); addAndMakeVisible (r); byId.insert ({ id, r }); ctl.push_back (std::move (k)); return r;
    }
    Component* ScreechPage::toggle (const String& id, const String& text, Colour col)
    {
        if (idOf (id) < 0) return nullptr;
        auto t = std::make_unique<doomsday::flat::Toggle> (proc.apvts, id, text, doomsday::flat::Toggle::Pill, col);
        t->setTooltip (fge::tooltip (idOf (id)));
        auto* r = t.get(); addAndMakeVisible (r); byId.insert ({ id, r }); ctl.push_back (std::move (t)); return r;
    }
    Component* ScreechPage::combo (const String& id, const String& caption)
    {
        const int idx = idOf (id);
        if (idx < 0) return nullptr;
        auto cb = std::make_unique<ComboBox>();
        cb->addItemList (def (idx).choices(), 1);
        comboAtt[cb.get()] = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *cb);
        cb->setTooltip (fge::tooltip (idx));
        auto* r = cb.get(); addAndMakeVisible (r); byId.insert ({ id, r }); if (caption.isNotEmpty()) captions[r] = caption; ctl.push_back (std::move (cb)); return r;
    }
    void ScreechPage::forget()
    {
        for (auto& c : ctl) comboAtt.erase (c.get());                           // the attachment goes before its control
        byId.clear(); captions.clear(); layoutMap.clear(); titles.clear();
        ctl.clear();
    }
    Component* ScreechPage::findControl (const String& id) { auto it = byId.find (id); return it == byId.end() ? nullptr : it->second; }
    StringArray ScreechPage::visibleIds() const { StringArray s; for (auto& [id, c] : byId) if (c->isVisible()) s.addIfNotAlreadyThere (id); return s; }

    // ===================================================================== page
    ScreechPage::ScreechPage (GeProcessor& p, doomsday::flat::FlatLookAndFeel& l) : proc (p), lnf (l)
    {
        const char* tabs[3] { "FILTERS", "RESONANCE", "SCREECH" };
        for (int i = 0; i < 3; ++i) { tabBtn[(size_t) i].setButtonText (tabs[i]); tabBtn[(size_t) i].onClick = [this, i] { showTab (i); }; addAndMakeVisible (tabBtn[(size_t) i]); }
        for (int i = 0; i < 8; ++i) { fltBtn[(size_t) i].onClick = [this, i] { selectFilter (i); }; addChildComponent (fltBtn[(size_t) i]); }
        for (int i = 0; i < 2; ++i) { resBtn[(size_t) i].onClick = [this, i] { selectRes (i); }; addChildComponent (resBtn[(size_t) i]); }
        for (int i = 0; i < 4; ++i) { oscBtn[(size_t) i].setButtonText ("OSC " + String (i + 1)); oscBtn[(size_t) i].onClick = [this, i] { selectOsc (i); }; addChildComponent (oscBtn[(size_t) i]); }
        const char* views[5] { "SOURCE", "SHAPE + FILTERS", "ENVELOPES", "TIMING", "PATHS (CUSTOM)" };
        for (int i = 0; i < 5; ++i) { viewBtn[(size_t) i].setButtonText (views[i]); viewBtn[(size_t) i].onClick = [this, i] { showScreechView (i); }; addChildComponent (viewBtn[(size_t) i]); }
        const char* lockNames[3] { "LOCK FILTERS", "LOCK RESON.", "LOCK SCREECH" };
        for (int i = 0; i < 3; ++i) { lockBtn[(size_t) i].setButtonText (lockNames[i]); lockBtn[(size_t) i].setTooltip ("RANDOMIZE leaves this engine alone."); addAndMakeVisible (lockBtn[(size_t) i]); }
        presetBtn.setTooltip ("Module 06 presets (EARLY / MODERN / RAWSTYLE / EXPERIMENTAL) - filters, resonators and screech together.");
        saveBtn.setTooltip ("Save the selected filter, the selected resonator, the screech or the whole module.");
        loadBtn.setTooltip ("Load a filter / resonator / screech / module file (a filter or resonator goes into the selected one).");
        abBtn.setTooltip ("A / B: compare two versions of the whole module (the other one is kept while you switch).");
        randBtn.setTooltip ("Randomize the module inside musical ranges (LOCKED engines stay). One UNDO step.");
        copyBtn.setTooltip ("Copy the selected filter / resonator."); pasteBtn.setTooltip ("Paste into the selected filter / resonator.");
        presetBtn.onClick = [this] { showPresetMenu(); };
        saveBtn.onClick = [this] { saveFile(); }; loadBtn.onClick = [this] { loadFile(); };
        abBtn.onClick = [this] { compare.toggle (proc); abBtn.setButtonText (compare.onB ? "B (A / B)" : "A (A / B)"); };
        randBtn.onClick = [this] { m06t::randomize (proc, locks(), Random::getSystemRandom().nextInt (99999), 0.6f); };
        copyBtn.onClick = [this] { if (tabIdx == TFilters) clipFilter = m06t::filterToXml (proc, fltSel); else if (tabIdx == TResonance) clipRes = m06t::resToXml (proc, resSel); };
        pasteBtn.onClick = [this]
        {
            if (tabIdx == TFilters && clipFilter) m06t::load (proc, *clipFilter, fltSel, resSel);
            if (tabIdx == TResonance && clipRes) m06t::load (proc, *clipRes, fltSel, resSel);
        };
        for (auto* b : { &presetBtn, &saveBtn, &loadBtn, &abBtn, &randBtn, &copyBtn, &pasteBtn }) addAndMakeVisible (b);
        addChildComponent (customEd);
        customEd.onCommit = [this] (const env::Shape& s) { proc.setShape (env::ResCustom1 + resSel, s); proc.setParamInUndoStep (m06t::rid (resSel, "mode"), (float) m6::RCustom); };
        scope.onReady = [this] { adoptScope(); };
        showTab (TScreech);
        startTimerHz (15);
    }
    ScreechPage::~ScreechPage() { stopTimer(); comboAtt.clear(); }

    void ScreechPage::showTab (int t)
    {
        tabIdx = jlimit (0, 2, t);
        const auto& c = palette();
        for (int i = 0; i < 3; ++i) { tabBtn[(size_t) i].setColour (TextButton::buttonColourId, i == tabIdx ? accent() : c.control); tabBtn[(size_t) i].setColour (TextButton::textColourOffId, i == tabIdx ? c.ground : c.silver); }
        for (auto& b : fltBtn) b.setVisible (tabIdx == TFilters);
        for (auto& b : resBtn) b.setVisible (tabIdx == TResonance);
        copyBtn.setVisible (tabIdx != TScreech); pasteBtn.setVisible (tabIdx != TScreech);
        lastSig = {}; lastF = -1;
        timerCallback();
    }
    void ScreechPage::selectFilter (int i) { fltSel = jlimit (0, 7, i); lastSig = {}; lastF = -1; timerCallback(); }
    void ScreechPage::selectRes (int i) { resSel = jlimit (0, 1, i); lastSig = {}; lastF = -1; timerCallback(); }
    void ScreechPage::selectOsc (int i) { oscSel = jlimit (0, 3, i); lastSig = {}; timerCallback(); }
    void ScreechPage::showScreechView (int v) { scrView = jlimit (0, 4, v); lastSig = {}; timerCallback(); }

    String ScreechPage::signature() const
    {
        String s; s << tabIdx << "/" << fltSel << "/" << resSel << "/" << oscSel << "/" << scrView << "/";
        s << roundToInt (raw (m06t::fid (fltSel, "type"))) << roundToInt (raw (m06t::rid (resSel, "mode"))) << roundToInt (raw ("scr_mode"))
          << roundToInt (raw ("scr_osc" + String (oscSel + 1) + "_wave")) << roundToInt (raw ("scr_start_mode")) << roundToInt (raw ("scr_pitch_mode"));
        return s;
    }

    // the controls of the current tab / selection (rebuilt when the selection or a TYPE / MODE changes - only what really works is shown)
    void ScreechPage::build()
    {
        forget();
        const auto col = accent();
        auto knobRow = [&] (const std::vector<K>& ks2, int x, int y, int step = ks, int w = kw, const String& prefix = {})
        {
            for (auto& k : ks2) { place (knob (prefix + k.id, k.label, col, k.bip), x, y, w, doomsday::flat::Knob::heightFor (kd)); x += step; }
            return x;
        };
        const int y1 = 80, yk = 104;
        if (tabIdx == TFilters)
        {
            const String o = m06t::fid (fltSel, "");
            const int t = roundToInt (raw (o + "type"));
            place (toggle (o + "on", "ON", col), 0, y1, 56, 22);
            int x = 64;
            std::vector<std::pair<String, String>> cs { { "type", "TYPE" }, { "location", "LOCATION" }, { "position", "POSITION" }, { "path", "PATH" }, { "band", "SPLIT BAND" } };
            if (m6::slopeWorks (t)) cs.push_back ({ "slope", "SLOPE" });
            cs.push_back ({ "stereo_mode", "STEREO" }); cs.push_back ({ "env_curve", "ENV CURVE" });
            for (auto& [k, cap] : cs) { place (combo (o + k, cap), x, y1, 108, 22); x += 114; }
            place (toggle (o + "env_invert", "INVERT", col), x, y1, 90, 22); x += 96;
            place (toggle (o + "env_sync", "SYNC LEN", col), x, y1, 100, 22);
            std::vector<K> row { { "cutoff", "CUTOFF", false }, { "resonance", "RESO", false }, { "drive", "DRIVE", false }, { "mix", "MIX", false },
                                 { "key_track", "KEY TRACK", false }, { "env_amount", "ENV AMT", true }, { "velocity", "VELOCITY", true } };
            if (t == m6::FSvf) row.push_back ({ "svf_mode", "LP-BP-HP", false });
            if (t == m6::FFormant) row.push_back ({ "vowel", "VOWEL", false });
            row.push_back ({ "env_to_reso", "ENV>RESO", true }); row.push_back ({ "env_to_drive", "ENV>DRIVE", true });
            knobRow (row, 0, yk + 12, ks, kw, o);
            std::vector<K> env { { "env_attack", "ATTACK", false }, { "env_decay", "DECAY", false }, { "env_sustain", "SUSTAIN", false }, { "env_release", "RELEASE", false } };
            knobRow (env, 0, yk + 96, ks, kw, o);
            knobRow ({ { "filter_xover_low", "SPLIT LOW", false }, { "filter_xover_high", "SPLIT HIGH", false } }, 360, yk + 96);
            titles.push_back ({ { 0, yk + 84, 300, 12 }, "FILTER ENVELOPE" });
            titles.push_back ({ { 360, yk + 84, 300, 12 }, "SPLIT CROSSOVERS (all filters)" });
        }
        else if (tabIdx == TResonance)
        {
            const String o = m06t::rid (resSel, "");
            place (toggle (o + "on", "ON", col), 0, y1, 56, 22);
            int x = 64;
            for (auto [k, cap] : std::initializer_list<std::pair<const char*, const char*>> { { "mode", "MODE" }, { "source", "EXCITATION" }, { "location", "LOCATION" }, { "position", "POSITION" }, { "penv_curve", "PITCH ENV CURVE" } })
            { place (combo (o + k, cap), x, y1, 120, 22); x += 126; }
            knobRow (std::vector<K> { { "freq", "FREQ", false }, { "q", "Q", false }, { "decay", "DECAY", false }, { "amount", "AMOUNT", false }, { "damping", "DAMPING", false },
                             { "excitation", "EXCITE", false }, { "feedback", "FEEDBACK", false }, { "pitch_track", "PITCH TRK", false }, { "mix", "MIX", false } }, 0, yk + 12, ks, kw, o);
            knobRow (std::vector<K> { { "penv_amount", "P.ENV AMT", false }, { "penv_start", "START HZ", false }, { "penv_end", "END HZ", false }, { "penv_attack", "ATTACK", false },
                             { "penv_decay", "DECAY", false }, { "penv_sustain", "SUSTAIN", false }, { "penv_release", "RELEASE", false } }, 0, yk + 96, ks, kw, o);
            titles.push_back ({ { 0, yk + 84, 300, 12 }, "RESONANCE PITCH ENVELOPE" });
        }
        else
        {
            const int mode = roundToInt (raw ("scr_mode"));
            place (toggle ("scr_on", "ON", col), 0, y1 - 14, 56, 22);
            int x = 64;
            for (auto [k, cap] : std::initializer_list<std::pair<const char*, const char*>> { { "scr_mode", "MODE" }, { "scr_output", "OUTPUT" }, { "scr_pitch_mode", "PITCH" } })
            { place (combo (k, cap), x, y1 - 14, 130, 22); x += 136; }
            std::vector<K> top { { "scr_level", "LEVEL", false }, { "scr_width", "WIDTH", false } };
            if (roundToInt (raw ("scr_pitch_mode")) == 2) top.push_back ({ "scr_base_hz", "BASE HZ", false });
            knobRow (top, x + 10, 30);
            const int y2 = 150, y3 = 214;
            if (scrView == SvSource)
            {
                if (mode == sc::Noise) titles.push_back ({ { 0, y2 + 20, 900, 14 }, "NOISE SCREECH does not use the oscillators (its noise is shaped on SHAPE + FILTERS)." });
                else
                {
                    const String o = "scr_osc" + String (oscSel + 1) + "_";
                    const int w = roundToInt (raw (o + "wave"));
                    place (toggle (o + "on", "ON", col), 0, y2 + 18, 56, 22);
                    const bool fmMode = mode == sc::Fm || mode == sc::Hybrid;
                    int cx = 64;
                    if (! fmMode) { place (combo (o + "wave", "WAVE"), cx, y2 + 18, 110, 22); cx += 116; place (combo (o + "fm_source", "FM FROM"), cx, y2 + 18, 90, 22); cx += 96;
                                    place (combo (o + "ring_source", "RING FROM"), cx, y2 + 18, 90, 22); cx += 96; }
                    std::vector<K> r { { "level", "LEVEL", false }, { "pitch", "PITCH", true }, { "fine", "FINE", true } };
                    if (! fmMode) { r.push_back ({ "phase", "PHASE", false }); r.push_back ({ "detune", "DETUNE", false }); if (w == sc::WPulse) r.push_back ({ "pw", "PW", false });
                                    r.push_back ({ "fm_amount", "FM", false }); r.push_back ({ "ring", "RING", false }); }
                    r.push_back ({ "pan", "PAN", true });
                    knobRow (r, cx + 6, y2, ks, kw, o);
                    if (fmMode) titles.push_back ({ { 0, y3 + 30, 900, 14 }, "FM SCREECH / HYBRID: sine operators (RATIO / INDEX / FEEDBACK on SHAPE + FILTERS) - waveform / FM / ring are not used." });
                }
            }
            else if (scrView == SvShape)
            {
                std::vector<K> m;
                const bool cust = mode == sc::Custom;
                if (cust) m = { { "scr_mix_tonal", "TONAL", false }, { "scr_mix_metal", "METALLIC", false }, { "scr_mix_formant", "FORMANT", false },
                                { "scr_mix_fm", "FM", false }, { "scr_mix_noise", "NOISE", false }, { "scr_mix_feedback", "FEEDBACK", false } };
                if (mode == sc::Fm || mode == sc::Hybrid) for (auto k : std::initializer_list<K> { { "scr_fm_ratio", "FM RATIO", false }, { "scr_fm_index", "FM INDEX", false }, { "scr_fm_feedback", "FM FDBK", false } }) m.push_back (k);
                if (mode == sc::Metallic) m = { { "scr_metal", "METAL", false }, { "scr_metal_q", "RINGING", false } };
                if (mode == sc::Noise) m = { { "scr_noise_color", "COLOR", true }, { "scr_noise_q", "RESONANCE", false } };
                if (mode == sc::Formant || mode == sc::Hybrid) for (auto k : std::initializer_list<K> { { "scr_vowel", "VOWEL", false }, { "scr_formant_width", "F WIDTH", false }, { "scr_formant_reso", "F RESO", false } }) m.push_back (k);
                if ((mode == sc::Formant || mode == sc::Hybrid) && raw ("scr_vowel_auto") > 0.5f) m.push_back ({ "scr_vowel_end", "VOWEL END", false });
                if (mode == sc::Feedback || mode == sc::Hybrid)
                    for (auto k : std::initializer_list<K> { { "scr_fb_amount", "FEEDBACK", false }, { "scr_fb_tone", "TONE", false }, { "scr_fb_delay", "DELAY", false },
                                                             { "scr_fb_drive", "DRIVE", false }, { "scr_fb_mix", "FB MIX", false }, { "scr_fb_stability", "STABILITY", false } }) m.push_back (k);
                int xx = 0;
                if (mode == sc::Formant || mode == sc::Hybrid) { place (toggle ("scr_vowel_auto", "AUTO MORPH", col), 0, y2 + 18, 110, 22); xx = 118; }
                if (mode == sc::Feedback || mode == sc::Hybrid) { place (combo ("scr_fb_filter", "FB FILTER"), xx, y2 + 18, 100, 22); xx += 106; }
                if (mode == sc::Tonal) titles.push_back ({ { 0, y2 + 22, 600, 14 }, "TONAL: the oscillators through the two filters below." });
                knobRow (m, xx + 4, y2, 70, 66);
                for (int k = 0; k < 2; ++k)
                {
                    const String o = "scr_f" + String (k + 1) + "_";
                    const int fx = k * 600;
                    place (toggle (o + "on", "F" + String (k + 1), col), fx, y3 + 18, 50, 22);
                    place (combo (o + "type", ""), fx + 54, y3 + 18, 110, 22);
                    std::vector<K> r { { "cutoff", "CUTOFF", false }, { "resonance", "RESO", false }, { "drive", "DRIVE", false }, { "key_track", "KEY", false }, { "env_amount", "MOD ENV", true }, { "mix", "MIX", false } };
                    knobRow (r, fx + 170, y3, 70, 66, o);
                }
            }
            else if (scrView == SvEnv)
            {
                place (combo ("scr_curve", "AMP CURVE"), 0, y2 + 18, 96, 22);
                int e = knobRow ({ { "scr_attack", "ATTACK", false }, { "scr_hold", "HOLD", false }, { "scr_decay", "DECAY", false }, { "scr_sustain", "SUSTAIN", false }, { "scr_release", "RELEASE", false } }, 102, y2, 70, 66);
                place (combo ("scr_pitch_curve", "PITCH CURVE"), e + 10, y2 + 18, 96, 22);
                knobRow ({ { "scr_pitch_amount", "P AMOUNT", false }, { "scr_pitch_start", "P START", true }, { "scr_pitch_end", "P END", true }, { "scr_pitch_attack", "P ATTACK", false },
                           { "scr_pitch_decay", "P DECAY", false }, { "scr_pitch_release", "END DROP", false } }, e + 112, y2, 70, 66);
                place (combo ("scr_mod_curve", "MOD CURVE"), 0, y3 + 18, 96, 22);
                knobRow ({ { "scr_mod_attack", "MOD ATK", false }, { "scr_mod_decay", "MOD DEC", false }, { "scr_mod_pitch", "> PITCH", true }, { "scr_mod_fm", "> FM", true },
                           { "scr_mod_cutoff", "> CUTOFF", true }, { "scr_mod_reso", "> RESO", true }, { "scr_mod_feedback", "> FDBK", true }, { "scr_mod_formant", "> FORMANT", true },
                           { "scr_mod_drive", "> DRIVE", true }, { "scr_mod_detune", "> DETUNE", true } }, 102, y3, 70, 66);
                titles.push_back ({ { 0, y2 - 2, 400, 12 }, "AMP  /  PITCH ENVELOPE" });
                titles.push_back ({ { 0, y3 - 2, 400, 12 }, "MOD ENVELOPE -> targets" });
            }
            else if (scrView == SvTiming)
            {
                const int sm = roundToInt (raw ("scr_start_mode"));
                place (combo ("scr_start_mode", "START MODE"), 0, y2 + 18, 160, 22);
                place (combo ("scr_quantize", "QUANTIZE"), 166, y2 + 18, 90, 22);
                int tx = 264;
                for (auto [k, t] : std::initializer_list<std::pair<const char*, const char*>> { { "scr_pre_trigger", "PRE-TRIGGER" }, { "scr_sync_tail", "LENGTH = TAIL" }, { "scr_auto_length", "AUTO LENGTH" },
                                                                                              { "scr_follow_body", "FOLLOW BODY" }, { "scr_independent", "INDEPENDENT" } })
                { if (String (k) == "scr_follow_body" && sm != sc::SBodyEnd && sm != sc::SBodyOffset) continue; place (toggle (k, t, col), tx, y2 + 18, 120, 22); tx += 126; }
                std::vector<K> r;
                if (sm == sc::SManual || sm == sc::SHost) r.push_back ({ "scr_start", "START", false });
                if (sm == sc::SBodyOffset || sm == sc::STail || sm == sc::SKick) r.push_back ({ "scr_offset", "OFFSET", true });
                if (raw ("scr_sync_tail") < 0.5f && raw ("scr_auto_length") < 0.5f) r.push_back ({ "scr_length", "LENGTH", false });
                r.push_back ({ "scr_fade_in", "FADE IN", false }); r.push_back ({ "scr_fade_out", "FADE OUT", false });
                knobRow (r, 0, y3);
            }
            else
            {
                // CUSTOM: every path's own controls (the levels are on SHAPE + FILTERS)
                knobRow ({ { "scr_fm_ratio", "FM RATIO", false }, { "scr_fm_index", "FM INDEX", false }, { "scr_fm_feedback", "FM FDBK", false }, { "scr_metal", "METAL", false },
                           { "scr_metal_q", "RINGING", false }, { "scr_noise_color", "N COLOR", true }, { "scr_noise_q", "N RESO", false }, { "scr_vowel", "VOWEL", false },
                           { "scr_formant_width", "F WIDTH", false }, { "scr_formant_reso", "F RESO", false } }, 0, y2, 70, 66);
                place (combo ("scr_fb_filter", "FB FILTER"), 0, y3 + 18, 100, 22);
                knobRow ({ { "scr_fb_amount", "FEEDBACK", false }, { "scr_fb_tone", "TONE", false }, { "scr_fb_delay", "DELAY", false }, { "scr_fb_drive", "DRIVE", false },
                           { "scr_fb_mix", "FB MIX", false }, { "scr_fb_stability", "STABILITY", false } }, 110, y3, 70, 66);
            }
            const bool cust = mode == sc::Custom;
            viewBtn[4].setVisible (cust);
            if (! cust && scrView == SvPaths) { scrView = SvShape; lastSig = {}; }
        }
        // the visible selector buttons
        for (auto& b : oscBtn) b.setVisible (tabIdx == TScreech && scrView == SvSource);
        for (int i = 0; i < 4; ++i) viewBtn[(size_t) i].setVisible (tabIdx == TScreech);
        if (tabIdx != TScreech) viewBtn[4].setVisible (false);
        customEd.setVisible (tabIdx == TResonance && roundToInt (raw (m06t::rid (resSel, "mode"))) == m6::RCustom);
        if (customEd.isVisible())
        {
            EnvEditor::Axis a; a.spanMs = 8.f; a.timeUnit = " harmonic";
            a.label = [] (float v) { return String (roundToInt (v * 100.f)) + " %"; }; a.toUser = [] (float v) { return v * 100.f; }; a.fromUser = [] (float x) { return x / 100.f; }; a.unit = "%";
            customEd.setAxis (a); customEd.setColour (accent()); customEd.setShape (proc.shape (env::ResCustom1 + resSel), 0.f, 1.f);
        }
        updateEnabled();
        resized();
    }

    void ScreechPage::updateEnabled()
    {
        auto en = [&] (const String& id, bool on) { for (auto [it, end] = byId.equal_range (id); it != end; ++it) { it->second->setEnabled (on); it->second->setAlpha (on ? 1.f : 0.4f); } };
        if (tabIdx == TFilters)
        {
            const String o = m06t::fid (fltSel, "");
            const bool envOn = std::abs (raw (o + "env_amount")) + std::abs (raw (o + "env_to_reso")) + std::abs (raw (o + "env_to_drive")) > 1.0e-4f;
            for (auto* k : { "env_attack", "env_decay", "env_sustain", "env_release", "env_curve", "env_invert", "env_sync" }) en (o + k, envOn);
            bool anySplit = false; for (int i = 0; i < 8; ++i) anySplit |= raw (m06t::fid (i, "on")) > 0.5f && raw (m06t::fid (i, "band")) > 0.5f;
            en ("filter_xover_low", anySplit); en ("filter_xover_high", anySplit);
            en (o + "position", roundToInt (raw (o + "location")) != m6::LScreech);
        }
        if (tabIdx == TResonance)
        {
            const String o = m06t::rid (resSel, "");
            const bool pe = raw (o + "penv_amount") > 0.f;
            for (auto* k : { "penv_start", "penv_end", "penv_attack", "penv_decay", "penv_sustain", "penv_release", "penv_curve" }) en (o + k, pe);
            en (o + "position", roundToInt (raw (o + "location")) != m6::LScreech);
        }
        if (tabIdx == TScreech)
        {
            const bool pe = raw ("scr_pitch_amount") > 0.f;
            for (auto* k : { "scr_pitch_start", "scr_pitch_end", "scr_pitch_attack", "scr_pitch_decay", "scr_pitch_release", "scr_pitch_curve" }) en (k, pe);
            bool mod = false; for (auto* k : { "scr_mod_pitch", "scr_mod_fm", "scr_mod_cutoff", "scr_mod_reso", "scr_mod_feedback", "scr_mod_formant", "scr_mod_drive", "scr_mod_detune" }) mod |= std::abs (raw (k)) > 1.0e-4f;
            for (int k = 0; k < 2; ++k) mod |= std::abs (raw ("scr_f" + String (k + 1) + "_env_amount")) > 1.0e-4f;
            for (auto* k : { "scr_mod_attack", "scr_mod_decay", "scr_mod_curve" }) en (k, mod);
            const String o = "scr_osc" + String (oscSel + 1) + "_";
            en (o + "fm_amount", roundToInt (raw (o + "fm_source")) > 0); en (o + "ring", roundToInt (raw (o + "ring_source")) > 0);
            en (o + "fm_source", raw (o + "fm_amount") > 0.f); en (o + "ring_source", raw (o + "ring") > 0.f);
        }
    }

    void ScreechPage::requestScope()
    {
        auto q = std::make_unique<M06Scope::Request>();
        q->f = proc.frame(); q->bank = proc.shapes(); q->tables = proc.currentTables(); q->filter = fltSel; q->res = resSel;
        scope.ask (std::move (q));
    }

    void ScreechPage::timerCallback()
    {
        if (! isShowing() && lastSig.isNotEmpty()) return;
        const auto sig = signature();
        if (sig != lastSig) { lastSig = sig; build(); }
        updateEnabled();
        // selector captions (type / place of every filter, mode of every resonator)
        for (int i = 0; i < 8; ++i)
        {
            const bool on = raw (m06t::fid (i, "on")) > 0.5f;
            String t; t << "F" << (i + 1) << (on ? "  " + Choices::m06FilterTypes()[roundToInt (raw (m06t::fid (i, "type")))].upToFirstOccurrenceOf (" ", false, false) : String ("  off"));
            fltBtn[(size_t) i].setButtonText (t);
            fltBtn[(size_t) i].setColour (TextButton::buttonColourId, i == fltSel ? accent() : on ? palette().control.brighter (0.2f) : palette().control);
            fltBtn[(size_t) i].setColour (TextButton::textColourOffId, i == fltSel ? palette().ground : palette().silver);
        }
        for (int i = 0; i < 2; ++i)
        {
            const bool on = raw (m06t::rid (i, "on")) > 0.5f;
            resBtn[(size_t) i].setButtonText ("RES " + String (i + 1) + (on ? "  " + Choices::resModes()[roundToInt (raw (m06t::rid (i, "mode")))] : String ("  off")));
            resBtn[(size_t) i].setColour (TextButton::buttonColourId, i == resSel ? accent() : on ? palette().control.brighter (0.2f) : palette().control);
            resBtn[(size_t) i].setColour (TextButton::textColourOffId, i == resSel ? palette().ground : palette().silver);
        }
        for (int i = 0; i < 4; ++i)
        {
            const bool on = raw ("scr_osc" + String (i + 1) + "_on") > 0.5f;
            oscBtn[(size_t) i].setColour (TextButton::buttonColourId, i == oscSel ? accent() : on ? palette().control.brighter (0.2f) : palette().control);
            oscBtn[(size_t) i].setColour (TextButton::textColourOffId, i == oscSel ? palette().ground : palette().silver);
        }
        for (int i = 0; i < 5; ++i) { viewBtn[(size_t) i].setColour (TextButton::buttonColourId, i == scrView ? palette().silver : palette().control); viewBtn[(size_t) i].setColour (TextButton::textColourOffId, i == scrView ? palette().ground : palette().silver); }
        const auto& f = proc.frame();
        if (proc.shapesVersion() != lastShapes || fltSel != lastF || resSel != lastR || ! std::equal (f.v.begin(), f.v.end(), lastAsked.v.begin()))
        {
            lastAsked = f; lastShapes = proc.shapesVersion(); lastF = fltSel; lastR = resSel;
            requestScope();
        }
    }

    void ScreechPage::showPresetMenu()
    {
        PopupMenu m; std::map<String, PopupMenu> cats; StringArray order;
        const auto& l = m06t::presets();
        for (int i = 0; i < (int) l.size(); ++i) { const String c (l[(size_t) i].category); if (! order.contains (c)) order.add (c); cats[c].addItem (i + 1, l[(size_t) i].name); }
        for (auto& c : order) m.addSubMenu (c, cats[c]);
        SafePointer<ScreechPage> self (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetBtn), [self] (int r) { if (self != nullptr && r > 0) m06t::apply (self->proc, m06t::presets()[(size_t) r - 1]); });
    }
    void ScreechPage::saveFile()
    {
        PopupMenu m;
        m.addItem (1, "Save filter " + String (fltSel + 1) + " (.4gfilter)"); m.addItem (2, "Save resonator " + String (resSel + 1) + " (.4gres)");
        m.addItem (3, "Save the screech (.4gscreech)"); m.addItem (4, "Save the whole module (.4gm06)");
        SafePointer<ScreechPage> self (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&saveBtn), [self] (int r)
        {
            if (self == nullptr || r <= 0) return;
            std::shared_ptr<XmlElement> x (r == 1 ? m06t::filterToXml (self->proc, self->fltSel).release() : r == 2 ? m06t::resToXml (self->proc, self->resSel).release()
                                           : r == 3 ? m06t::screechToXml (self->proc).release() : m06t::moduleToXml (self->proc).release());
            const String ext = m06t::extensionOf (x->getTagName());
            self->chooser = std::make_unique<FileChooser> ("Save", files::folder ("Screech"), "*" + ext);
            self->chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [x, ext] (const FileChooser& fc)
            { if (fc.getResult() != File()) files::writeXml (fc.getResult().withFileExtension (ext), *x); });
        });
    }
    void ScreechPage::loadFile()
    {
        chooser = std::make_unique<FileChooser> ("Load", files::folder ("Screech"), "*.4gfilter;*.4gres;*.4gscreech;*.4gm06");
        SafePointer<ScreechPage> self (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [self] (const FileChooser& fc)
        {
            if (self == nullptr || fc.getResult() == File()) return;
            if (auto x = files::readXml (fc.getResult())) m06t::load (self->proc, *x, self->fltSel, self->resSel);
        });
    }

    void ScreechPage::resized()
    {
        const int w = getWidth(), h = getHeight();
        for (int i = 0; i < 3; ++i) tabBtn[(size_t) i].setBounds (i * 112, 0, 108, 26);
        randBtn.setBounds (w - 96, 0, 96, 26); abBtn.setBounds (w - 186, 0, 86, 26); loadBtn.setBounds (w - 254, 0, 64, 26);
        saveBtn.setBounds (w - 322, 0, 64, 26); presetBtn.setBounds (w - 410, 0, 84, 26);
        for (int i = 0; i < 3; ++i) lockBtn[(size_t) i].setBounds (w - 3 * 112 + i * 112, 34, 108, 22);
        for (int i = 0; i < 8; ++i) fltBtn[(size_t) i].setBounds (i * 92, 34, 88, 24);
        for (int i = 0; i < 2; ++i) resBtn[(size_t) i].setBounds (i * 150, 34, 146, 24);
        copyBtn.setBounds (tabIdx == TFilters ? 740 : 310, 34, 64, 24); pasteBtn.setBounds (tabIdx == TFilters ? 808 : 378, 34, 64, 24);
        for (int i = 0; i < 5; ++i) viewBtn[(size_t) i].setBounds (i * 124, 106, 120, 24);
        for (int i = 0; i < 4; ++i) oscBtn[(size_t) i].setBounds (640 + i * 70, 106, 66, 24);
        graphA = { 0, 330, 520, h - 330 }; graphB = { 530, 330, 330, h - 330 }; graphC = { 870, 330, w - 870, h - 330 };
        if (tabIdx == TResonance) customEd.setBounds (graphC.withTrimmedTop (16));
        for (auto& [c, r] : layoutMap) c->setBounds (r);
    }

    void ScreechPage::paint (Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.panel); g.fillRoundedRectangle (Rectangle<int> (0, 62, getWidth(), 262).toFloat(), 4.f);
        g.setColour (c.textDim); g.setFont (FontOptions (10.5f, Font::bold));
        for (auto& [comp, cap] : captions) if (comp->isVisible()) g.drawText (cap, comp->getX(), comp->getY() - 12, comp->getWidth() + 30, 11, Justification::centredLeft);
        for (auto& [r, t] : titles) g.drawText (t, r, Justification::centredLeft);
        auto frame = [&] (Rectangle<int> r, const String& title) { g.setColour (c.ground); g.fillRoundedRectangle (r.toFloat().withTrimmedTop (16.f), 4.f); g.setColour (c.textDim); g.setFont (FontOptions (10.5f, Font::bold)); g.drawText (title, r.getX(), r.getY(), r.getWidth(), 12, Justification::centredLeft); return r.withTrimmedTop (16).reduced (6).toFloat(); };
        auto specLine = [&] (Rectangle<float> r, const std::vector<float>& s, Colour col, float top, float range, float th)
        {
            if (s.empty()) return;
            Path p;
            for (size_t k = 0; k < s.size(); ++k)
            {
                const float x = r.getX() + r.getWidth() * (float) k / (float) (s.size() - 1), y = r.getY() + r.getHeight() * jlimit (0.f, 1.f, (top - s[k]) / range);
                k == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
            }
            g.setColour (col); g.strokePath (p, PathStrokeType (th));
        };
        auto hzGrid = [&] (Rectangle<float> r)
        {
            g.setFont (FontOptions (9.f));
            for (float hz : { 100.f, 1000.f, 10000.f })
            {
                const float x = r.getX() + r.getWidth() * std::log (hz / 30.f) / std::log (20000.f / 30.f);
                g.setColour (c.grid); g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
                g.setColour (c.textDim); g.drawText (hz >= 1000.f ? String ((int) (hz / 1000.f)) + "k" : String ((int) hz), (int) x + 2, (int) r.getBottom() - 11, 30, 10, Justification::left);
            }
        };
        // the routing: what sits where (filters F1-F8 / resonators R1-R2 per place, before | after the distortion)
        auto routing = [&] (Rectangle<int> r)
        {
            auto rr = frame (r, "ROUTING  (before | after distortion)");
            g.setFont (FontOptions (10.5f));
            int line = 0;
            for (int l = 0; l < m6::NumLocs; ++l)
            {
                String pre, post;
                for (int i = 0; i < 8; ++i) if (raw (m06t::fid (i, "on")) > 0.5f && roundToInt (raw (m06t::fid (i, "location"))) == l) (roundToInt (raw (m06t::fid (i, "position"))) == 0 || l == m6::LScreech ? pre : post) << "F" << (i + 1) << " ";
                for (int i = 0; i < 2; ++i) if (raw (m06t::rid (i, "on")) > 0.5f && roundToInt (raw (m06t::rid (i, "location"))) == l) (roundToInt (raw (m06t::rid (i, "position"))) == 0 || l == m6::LScreech ? pre : post) << "R" << (i + 1) << " ";
                if (pre.isEmpty() && post.isEmpty()) continue;
                g.setColour (c.silver);
                g.drawText (Choices::m06Locations()[l] + ":  " + pre + "| " + post, (int) rr.getX(), (int) rr.getY() + line * 15, (int) rr.getWidth(), 14, Justification::centredLeft);
                ++line;
            }
            if (raw ("scr_on") > 0.5f)
            {
                g.setColour (accent());
                g.drawText ("SCREECH -> " + Choices::scrOutputs()[roundToInt (raw ("scr_output"))], (int) rr.getX(), (int) rr.getY() + line * 15 + 4, (int) rr.getWidth(), 14, Justification::centredLeft);
            }
            if (line == 0 && raw ("scr_on") < 0.5f) { g.setColour (c.textDim); g.drawText ("nothing placed yet", rr.toNearestInt(), Justification::centred); }
        };
        if (! shown.valid) return;
        if (tabIdx == TFilters)
        {
            auto r = frame (graphA, "FILTER RESPONSE  (bright = kick start, dim = later: the envelope moves it)");
            hzGrid (r);
            specLine (r, shown.fltLater, accent().withAlpha (0.45f), 18.f, 60.f, 1.2f);
            specLine (r, shown.fltStart, accent(), 18.f, 60.f, 1.8f);
            auto e = frame (graphB, "FILTER ENVELOPE");
            Path p; for (size_t k = 0; k < shown.fltEnv.size(); ++k) { const float x = e.getX() + e.getWidth() * (float) k / (float) (shown.fltEnv.size() - 1), y = e.getBottom() - e.getHeight() * shown.fltEnv[k]; k == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y); }
            g.setColour (accent()); g.strokePath (p, PathStrokeType (1.6f));
            routing (graphC);
        }
        else if (tabIdx == TResonance)
        {
            auto r = frame (graphA, "RESONANCE  (the resonator's real response)");
            hzGrid (r);
            const float top = shown.resResp.empty() ? 0.f : *std::max_element (shown.resResp.begin(), shown.resResp.end()) + 3.f;
            specLine (r, shown.resResp, accent(), top, 70.f, 1.6f);
            auto e = frame (graphB, "RESONANCE PITCH over the kick");
            if (! shown.resPitch.empty())
            {
                Path p;
                for (size_t k = 0; k < shown.resPitch.size(); ++k)
                {
                    const float x = e.getX() + e.getWidth() * (float) k / (float) (shown.resPitch.size() - 1);
                    const float y = e.getBottom() - e.getHeight() * std::log (shown.resPitch[k] / 20.f) / std::log (1000.f);
                    k == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
                }
                g.setColour (accent()); g.strokePath (p, PathStrokeType (1.6f));
                g.setColour (c.textDim); g.setFont (FontOptions (10.f));
                g.drawText (String (roundToInt (shown.resPitch.front())) + " Hz -> " + String (roundToInt (shown.resPitch.back())) + " Hz", e.toNearestInt(), Justification::topRight);
            }
            if (! customEd.isVisible()) routing (graphC);
            else { g.setColour (c.textDim); g.setFont (FontOptions (10.5f, Font::bold)); g.drawText ("CUSTOM SPECTRUM (harmonics 1 - 8)", graphC.getX(), graphC.getY(), graphC.getWidth(), 12, Justification::centredLeft); }
        }
        else
        {
            auto r = frame (graphA, "SCREECH  (alone - the real render)");
            if (! shown.scrHi.empty() && shown.scrPeak > 1.0e-6f)
            {
                const float sc = r.getHeight() * 0.48f / shown.scrPeak;
                Path p; const int n = (int) shown.scrHi.size();
                for (int k = 0; k < n; ++k) { const float x = r.getX() + r.getWidth() * (float) k / (float) (n - 1); k == 0 ? p.startNewSubPath (x, r.getCentreY() - shown.scrHi[(size_t) k] * sc) : p.lineTo (x, r.getCentreY() - shown.scrHi[(size_t) k] * sc); }
                for (int k = n - 1; k >= 0; --k) p.lineTo (r.getX() + r.getWidth() * (float) k / (float) (n - 1), r.getCentreY() - shown.scrLo[(size_t) k] * sc);
                p.closeSubPath(); g.setColour (accent().withAlpha (0.7f)); g.fillPath (p);
                g.setColour (c.textDim); g.setFont (FontOptions (10.f)); g.drawText (String (roundToInt (shown.scrEndMs)) + " ms", r.toNearestInt(), Justification::bottomRight);
            }
            else { g.setColour (c.textDim); g.setFont (FontOptions (12.f)); g.drawText (raw ("scr_on") > 0.5f ? "silent" : "screech is OFF", r.toNearestInt(), Justification::centred); }
            auto s = frame (graphB, "SCREECH SPECTRUM");
            hzGrid (s);
            if (! shown.scrSpec.empty()) specLine (s, shown.scrSpec, accent(), *std::max_element (shown.scrSpec.begin(), shown.scrSpec.end()) + 3.f, 80.f, 1.4f);
            routing (graphC);
        }
    }
}
