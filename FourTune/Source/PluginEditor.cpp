#include "PluginEditor.h"
#include "DoomsdayBrand.h"

using namespace juce;
using namespace doomsday::flat;
using namespace tuneui;

// ======================================================================= Page helpers
Knob* Page::knob (const char* id, const char* label, Colour c, bool bipolar, int d)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, c, bipolar, d);
    if (auto* p = proc.apvts.getParameter (id)) k->slider.setTooltip (p->getName (64));
    auto* kp = k.get(); addAndMakeVisible (*kp); owned.push_back (std::move (k)); knobs.push_back (kp);
    return kp;
}
ComboBox* Page::combo (const char* id)
{
    auto cb = std::make_unique<ComboBox>();
    if (auto* p = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id)))
    {
        cb->addItemList (p->choices, 1);
        cb->setTooltip (p->getName (64));
    }
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *cb));
    auto* c = cb.get(); addAndMakeVisible (*c); owned.push_back (std::move (cb));
    return c;
}
Toggle* Page::toggle (const char* id, const char* text, Colour c, bool chip)
{
    auto t = std::make_unique<Toggle> (proc.apvts, id, text, chip ? Toggle::Chip : Toggle::Pill, c);
    if (auto* p = proc.apvts.getParameter (id)) t->setTooltip (p->getName (64));
    auto* tp = t.get(); addAndMakeVisible (*tp); owned.push_back (std::move (t));
    return tp;
}
void Page::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    for (auto& c : cards) glassCard (g, lnf, c.area, c.title, c.dot);
}

// ======================================================================= LIQUID GLASS (violet + lime)
void tuneui::glassCard (Graphics& g, FlatLookAndFeel& lnf, Rectangle<int> area, const String& title, Colour dot)
{
    const auto r = area.toFloat();
    const float rr = 14.f;
    // translucent violet-tinted glass: the canvas glows shine through
    g.setGradientFill (ColourGradient (Colour (0x9a1d1626), r.getX(), r.getY(), Colour (0xb0110e17), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, rr);
    g.setColour (Colours::white.withAlpha (0.085f)); g.drawRoundedRectangle (r.reduced (0.5f), rr, 1.f);
    // the glass highlight along the top edge
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.0f), r.getX() + rr, 0.f, Colours::white.withAlpha (0.0f), r.getRight() - rr, 0.f, false));
    ColourGradient hl (Colours::white.withAlpha (0.f), r.getX() + rr, r.getY(), Colours::white.withAlpha (0.f), r.getRight() - rr, r.getY(), false);
    hl.addColour (0.5, Colours::white.withAlpha (0.22f));
    g.setGradientFill (hl); g.fillRect (r.getX() + rr, r.getY() + 0.5f, r.getWidth() - 2.f * rr, 1.2f);
    if (title.isEmpty()) return;
    float x = r.getX() + 16.f;
    if (! dot.isTransparent())
    {
        g.setColour (dot.withAlpha (0.35f)); g.fillEllipse (x - 3.f, r.getY() + 14.f, 12.f, 12.f);      // soft glow
        g.setColour (dot); g.fillEllipse (x, r.getY() + 17.f, 6.f, 6.f);
        x += 14.f;
    }
    g.setColour (tokens().text.withAlpha (0.78f));
    g.setFont (lnf.font (12.f, Bold, 0.14f));
    g.drawText (title.toUpperCase(), Rectangle<float> (x, r.getY() + 10.f, r.getWidth() - 40.f, 20.f), Justification::centredLeft);
}

void tuneui::glassBackground (Graphics& g, Rectangle<float> a)
{
    g.setColour (Colour (0xff08060c)); g.fillRect (a);
    auto glow = [&] (float cx, float cy, float rad, Colour col)
    {
        ColourGradient gr (col, cx, cy, col.withAlpha (0.f), cx + rad, cy, true);
        g.setGradientFill (gr); g.fillEllipse (cx - rad, cy - rad, 2.f * rad, 2.f * rad);
    };
    glow (a.getRight() - a.getWidth() * 0.18f, a.getY() + a.getHeight() * 0.10f, a.getWidth() * 0.36f, Colour (0x46a066ff));   // violet, top right
    glow (a.getX() + a.getWidth() * 0.10f, a.getBottom() - a.getHeight() * 0.12f, a.getWidth() * 0.34f, Colour (0x30b6f23e));  // lime, bottom left
    glow (a.getX() + a.getWidth() * 0.45f, a.getY() + a.getHeight() * 0.55f, a.getWidth() * 0.28f, Colour (0x24743cd6));      // deep violet, middle
    glow (a.getX() + a.getWidth() * 0.05f, a.getY(), a.getWidth() * 0.20f, Colour (0x22d8ff4a));                             // a touch of lime, top left
}

// place components left -> right in a row, each 'w' wide
static void row (const std::vector<Component*>& comps, int x, int y, int w, int h)
{
    for (auto* c : comps) { c->setBounds (x, y, w, h); x += w; }
}

// ======================================================================= FORTUNE view (spec 17 / 29)
namespace tuneui
{
    class FortuneView : public Component
    {
    public:
        explicit FortuneView (TuneProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
        void paint (Graphics& g) override
        {
            auto& lnf = lnfOf (*this);
            const auto& t = tokens();
            const float f = proc.fortuneNow();
            const auto c = getLocalBounds().toFloat();
            // the five stages around the knob: CONTROLLED -> BRIGHT -> WIDE -> AGGRESSIVE -> FESTIVAL
            static const char* names[] { "CONTROLLED", "BRIGHT", "WIDE", "AGGRESSIVE", "FESTIVAL" };
            const auto centre = Point<float> (c.getCentreX(), 150.f);
            const float r = 128.f;
            for (int i = 0; i < 5; ++i)
            {
                const float a = MathConstants<float>::pi * (-0.75f + 1.5f * (float) i / 4.f);
                const auto p = centre + Point<float> (std::sin (a) * r, -std::cos (a) * r);
                const bool lit = f >= (float) i * 0.2f - 0.001f;
                const bool now = f >= (float) i * 0.2f && f < (float) (i + 1) * 0.2f + (i == 4 ? 0.01f : 0.f);
                g.setFont (lnf.font (now ? 12.5f : 11.f, now ? Bold : SemiBold, 0.08f));
                g.setColour (now ? fortune() : lit ? fortune().withAlpha (0.55f) : t.textMute.withAlpha (0.6f));
                g.drawText (names[i], Rectangle<float> (110.f, 18.f).withCentre (p), Justification::centred);
            }
            // live readout: what FORTUNE adds right now (FINAL = BASE + FORTUNE)
            const auto s = proc.gather (150.0);
            const int scrPct = (int) std::round (100.f * tune::dsp::FortuneMap::screamAdd (s, s.scr));
            String line1 = "CUTOFF +" + String (tune::dsp::FortuneMap::cutOct (s), 2) + " oct   DETUNE +"
                         + String ((int) std::round (tune::dsp::FortuneMap::detuneAdd (s, 2))) + " ct   WIDTH +"
                         + String ((int) std::round (100.f * tune::dsp::FortuneMap::widthAdd (s))) + "%";
            String line2 = "SCREAM +" + String (scrPct) + "%   REVERB MIX +" + String ((int) std::round (100.f * tune::dsp::FortuneMap::rvbMixAdd (s)))
                         + "   SIZE +" + String ((int) std::round (100.f * tune::dsp::FortuneMap::rvbSizeAdd (s)));
            g.setFont (lnf.font (11.f, Medium)); g.setColour (f > 0.001f ? t.text : t.textMute);
            g.drawText (line1, Rectangle<float> (0.f, c.getBottom() - 46.f, c.getWidth(), 18.f), Justification::centred);
            g.drawText (line2, Rectangle<float> (0.f, c.getBottom() - 28.f, c.getWidth(), 18.f), Justification::centred);
        }
    private:
        TuneProcessor& proc;
    };
}

// ======================================================================= SYNTH page
SynthPage::SynthPage (TuneProcessor& p) : Page (p)
{
    for (int o = 0; o < 4; ++o)
    {
        const Colour c = o < 2 ? groupA() : groupB();
        const String n (o + 1);
        auto id = [&n] (const char* suf) { static thread_local std::string s; s = "o" + n.toStdString() + "_" + suf; return s.c_str(); };
        auto& r = osc[(size_t) o];
        r.on = toggle (id ("on"), ("OSC " + n).toRawUTF8(), c);
        r.wave = combo (id ("wave"));
        r.retrig = toggle (id ("retrig"), "RETRIG", c);
        r.knobs = { knob (id ("oct"), "OCT", c, true, 32), knob (id ("semi"), "SEMI", c, true, 32), knob (id ("fine"), "FINE", c, true, 32),
                    knob (id ("level"), "LEVEL", c, false, 32), knob (id ("pan"), "PAN", c, true, 32), knob (id ("width"), "SPREAD", c, false, 32),
                    knob (id ("wt"), "WT POS", c, false, 32), knob (id ("warp"), "WARP", c, false, 32), knob (id ("voices"), "VOICES", c, false, 32),
                    knob (id ("detune"), "DETUNE", c, false, 32), knob (id ("uspread"), "UNI SPREAD", c, false, 32), knob (id ("umix"), "UNI MIX", c, false, 32),
                    knob (id ("phase"), "PHASE", c, false, 32), knob (id ("phrand"), "RANDOM", c, false, 32) };
    }
    grpA = { knob ("ga_vol", "VOLUME", groupA(), false, 28), knob ("ga_pan", "PAN", groupA(), true, 28), knob ("ga_width", "WIDTH", groupA(), false, 28) };
    grpB = { knob ("gb_vol", "VOLUME", groupB(), false, 28), knob ("gb_pan", "PAN", groupB(), true, 28), knob ("gb_width", "WIDTH", groupB(), false, 28) };
    fltType = combo ("flt_type"); fltRoute = combo ("flt_route");
    flt = { knob ("flt_cut", "CUTOFF", filter(), false, 48), knob ("flt_res", "RESONANCE", filter()), knob ("flt_drive", "DRIVE", filter()),
            knob ("flt_key", "KEY TRACK", filter()), knob ("flt_env", "ENV AMOUNT", filter(), true) };
    fenv = { knob ("fe_a", "ATTACK", filter()), knob ("fe_h", "HOLD", filter()), knob ("fe_d", "DECAY", filter()), knob ("fe_s", "SUSTAIN", filter()),
             knob ("fe_r", "RELEASE", filter()), knob ("fe_curve", "CURVE", filter(), true) };
    aenv = { knob ("ae_a", "ATTACK", neutral()), knob ("ae_h", "HOLD", neutral()), knob ("ae_d", "DECAY", neutral()), knob ("ae_s", "SUSTAIN", neutral()),
             knob ("ae_r", "RELEASE", neutral()), knob ("ae_curve", "CURVE", neutral(), true), knob ("vel_sens", "VELOCITY", neutral()) };
    playMode = combo ("play_mode");
    for (auto* b : { &pluckEnv, &leadEnv }) addAndMakeVisible (*b);
    pluckEnv.setTooltip ("PLUCK: short decay, low sustain"); leadEnv.setTooltip ("LEAD: longer, high sustain");
    pluckEnv.onClick = [this] { proc.envelopeMode (false); };
    leadEnv.onClick = [this] { proc.envelopeMode (true); };
    perf = { knob ("poly_voices", "VOICES", neutral()), knob ("glide", "GLIDE", neutral()), knob ("bend_range", "BEND", neutral()),
             knob ("transpose", "TRANSPOSE", neutral(), true), knob ("master", "MASTER", neutral()) };
    fview = std::make_unique<FortuneView> (p);
    addAndMakeVisible (*fview);
    fortuneKnob = knob ("fortune", "FORTUNE", fortune(), false, 150);
    fortuneKnob->setCrown (true);
    screamKnob = knob ("scr_amt", "SCREAM", scream(), false, 76);
    scr = { knob ("scr_drive", "DRIVE", scream()), knob ("scr_sat", "SATURATION", scream()), knob ("scr_tone", "TONE", scream(), true),
            knob ("scr_bite", "BITE", scream()), knob ("scr_body", "BODY", scream()), knob ("scr_mix", "MIX", scream()), knob ("scr_out", "OUTPUT", scream(), true) };
    scrOs = new Segmented (proc.apvts, "scr_os", tune::Choices::screamOs, scream());
    scrOs->setTooltip ("Scream oversampling (quality vs CPU)");
    addAndMakeVisible (*scrOs); owned.emplace_back (scrOs);
}
SynthPage::~SynthPage() = default;

void SynthPage::resized()
{
    cards.clear();
    const int kw = 58, kh = Knob::heightFor (32);
    // GROUP A / GROUP B (left): the group mixer sits in the title row, the two oscillator rows under it
    for (int gIdx = 0; gIdx < 2; ++gIdx)
    {
        const Rectangle<int> a (0, gIdx * 246, 912, 236);
        cards.push_back ({ gIdx == 0 ? "GROUP A - CORE" : "GROUP B - EXPANSION", gIdx == 0 ? groupA() : groupB(), a });
        row (gIdx == 0 ? grpA : grpB, a.getRight() - 16 - 3 * 58, a.getY() + 4, 58, Knob::heightFor (28));
        for (int k = 0; k < 2; ++k)
        {
            auto& r = osc[(size_t) (gIdx * 2 + k)];
            const int y = a.getY() + 72 + k * (kh + 12);
            r.on->setBounds (a.getX() + 16, y + 2, 82, 24);
            r.wave->setBounds (a.getX() + 16, y + 30, 82, 24);
            r.retrig->setBounds (a.getX() + 16, y + 58, 82, 22);
            row (r.knobs, a.getX() + 104, y, kw, kh);
        }
    }
    // FILTER + envelopes + play (bottom left)
    const int bk = 62, bkh = Knob::heightFor (36);
    Rectangle<int> fa (0, 492, 450, 156), fe (0, 658, 450, 154), ae (462, 492, 450, 156), pf (462, 658, 450, 154);
    cards.push_back ({ "FILTER", filter(), fa }); cards.push_back ({ "FILTER ENVELOPE", filter(), fe });
    cards.push_back ({ "AMP ENVELOPE", neutral(), ae }); cards.push_back ({ "PLAY", neutral(), pf });
    fltType->setBounds (fa.getRight() - 216, fa.getY() + 10, 96, 24);
    fltRoute->setBounds (fa.getRight() - 112, fa.getY() + 10, 96, 24);
    flt[0]->setBounds (fa.getX() + 12, fa.getY() + 40, 84, Knob::heightFor (48));
    row ({ flt[1], flt[2], flt[3], flt[4] }, fa.getX() + 100, fa.getY() + 48, 84, bkh);
    row (fenv, fe.getX() + 14, fe.getY() + 44, 70, bkh);
    row (aenv, ae.getX() + 10, ae.getY() + 48, bk, bkh);
    pluckEnv.setBounds (ae.getRight() - 160, ae.getY() + 10, 70, 24);
    leadEnv.setBounds (ae.getRight() - 84, ae.getY() + 10, 68, 24);
    playMode->setBounds (pf.getRight() - 112, pf.getY() + 10, 96, 24);
    row (perf, pf.getX() + 14, pf.getY() + 44, 84, bkh);
    // FORTUNE (right, dominant) + SCREAM
    Rectangle<int> fo (924, 0, 476, 482), sc (924, 492, 476, 320);
    cards.push_back ({ "FORTUNE", fortune(), fo }); cards.push_back ({ "SCREAM", scream(), sc });
    fview->setBounds (fo.reduced (8, 30));
    // the knob's arc centre = the centre of the five stage names (FortuneView: y 150 inside the view)
    fortuneKnob->setBounds (fo.getCentreX() - 100, fo.getY() + 30 + 150 - 75, 200, Knob::heightFor (150));
    screamKnob->setBounds (sc.getX() + 14, sc.getY() + 46, 110, Knob::heightFor (76));
    scrOs->setBounds (sc.getRight() - 150, sc.getY() + 10, 134, 24);
    row ({ scr[0], scr[1], scr[2] }, sc.getX() + 140, sc.getY() + 48, 104, bkh);
    row ({ scr[3], scr[4], scr[5], scr[6] }, sc.getX() + 30, sc.getY() + 60 + bkh + 30, 104, bkh);
}

void SynthPage::paintOverChildren (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    g.setFont (lnf.font (10.5f, Medium)); g.setColour (tokens().textMute);
    // the SCREAM rule of thumb (spec 11)
    auto sc = cards.back().area;
    g.drawText ("LOW = harmonics   MEDIUM = aggressive   HIGH = screaming   (level kept steady)", sc.withTop (sc.getBottom() - 26).reduced (16, 0),
                Justification::centredLeft);
}

void SynthPage::tick() { if (fview) fview->repaint(); }

// ======================================================================= MOD page
ModPage::ModPage (TuneProcessor& p) : Page (p)
{
    const Colour lc[2] { Colour (0xff3bd6a0), Colour (0xffb07cff) };
    for (int l = 0; l < 2; ++l)
    {
        const String n (l + 1);
        auto id = [&n] (const char* suf) { static thread_local std::string s; s = "lfo" + n.toStdString() + "_" + suf; return s.c_str(); };
        auto& u = lfo[(size_t) l];
        u.shape = combo (id ("shape")); u.div = combo (id ("div"));
        u.sync = toggle (id ("sync"), "SYNC", lc[l]); u.retrig = toggle (id ("retrig"), "RETRIG", lc[l]);
        u.knobs = { knob (id ("rate"), "RATE", lc[l]), knob (id ("depth"), "DEPTH", lc[l]), knob (id ("phase"), "PHASE", lc[l]), knob (id ("fade"), "FADE IN", lc[l]) };
    }
    menv = { knob ("me_a", "ATTACK", fortune()), knob ("me_h", "HOLD", fortune()), knob ("me_d", "DECAY", fortune()), knob ("me_s", "SUSTAIN", fortune()),
             knob ("me_r", "RELEASE", fortune()), knob ("me_curve", "CURVE", fortune(), true) };
    ftAmt = { knob ("ft_cut", "CUTOFF", fortune()), knob ("ft_det", "DETUNE", fortune()), knob ("ft_width", "WIDTH", fortune()),
              knob ("ft_scr", "SCREAM", fortune()), knob ("ft_rmix", "REVERB MIX", fortune()), knob ("ft_rsize", "REVERB SIZE", fortune()) };
    for (int k = 0; k < 16; ++k)
    {
        const String n (k + 1);
        auto id = [&n] (const char* suf) { static thread_local std::string s; s = "mm" + n.toStdString() + "_" + suf; return s.c_str(); };
        slots[(size_t) k] = { combo (id ("src")), combo (id ("dst")), knob (id ("amt"), "", neutral(), true, 24) };
    }
}

void ModPage::resized()
{
    cards.clear();
    const int kh = Knob::heightFor (36);
    for (int l = 0; l < 2; ++l)
    {
        Rectangle<int> a (0, l * 162, 450, 152);
        cards.push_back ({ "LFO " + String (l + 1), l == 0 ? Colour (0xff3bd6a0) : Colour (0xffb07cff), a });
        auto& u = lfo[(size_t) l];
        u.shape->setBounds (a.getRight() - 300, a.getY() + 10, 110, 24);
        u.sync->setBounds (a.getRight() - 182, a.getY() + 10, 56, 24);
        u.div->setBounds (a.getRight() - 120, a.getY() + 10, 104, 24);
        row (u.knobs, a.getX() + 14, a.getY() + 44, 84, kh);
        u.retrig->setBounds (a.getX() + 14 + 4 * 84 + 10, a.getY() + 66, 80, 24);
    }
    Rectangle<int> me (0, 324, 450, 152), ft (0, 486, 450, 170);
    cards.push_back ({ "MOD ENVELOPE", fortune(), me }); cards.push_back ({ "FORTUNE TARGETS", fortune(), ft });
    row (menv, me.getX() + 14, me.getY() + 44, 70, kh);
    row (ftAmt, ft.getX() + 14, ft.getY() + 44, 70, kh);
    Rectangle<int> mx (462, 0, 938, 812);
    cards.push_back ({ "MOD MATRIX", neutral(), mx });
    for (int k = 0; k < 16; ++k)
    {
        const int col = k / 8, r = k % 8;
        const int x = mx.getX() + 16 + col * 462, y = mx.getY() + 50 + r * 94;
        auto& s = slots[(size_t) k];
        s.src->setBounds (x + 30, y + 14, 150, 26);
        s.dst->setBounds (x + 188, y + 14, 160, 26);
        s.amt->setBounds (x + 354, y - 4, 72, Knob::heightFor (24));
    }
}

void ModPage::paintOverChildren (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    g.setFont (lnf.font (12.f, Bold)); g.setColour (tokens().textMute);
    const auto mx = cards.back().area;
    for (int k = 0; k < 16; ++k)
    {
        const int col = k / 8, r = k % 8;
        g.drawText (String (k + 1), mx.getX() + 16 + col * 462, mx.getY() + 50 + r * 94 + 14, 26, 26, Justification::centredLeft);
    }
    g.setFont (lnf.font (11.f, Medium));
    const auto ft = cards[cards.size() - 2].area;
    g.drawText ("How much FORTUNE moves each part (FINAL = KNOB + FORTUNE, never replaces it)", ft.withTop (ft.getBottom() - 28).reduced (16, 0), Justification::centredLeft);
}

// ======================================================================= FX page
FxPage::FxPage (TuneProcessor& p) : Page (p)
{
    const Colour c = fx();
    units[0] = { toggle ("eq_on", "ON", c), nullptr, nullptr, { knob ("eq_low", "LOW", c, true), knob ("eq_lowmid", "LOW MID", c, true), knob ("eq_highmid", "HIGH MID", c, true), knob ("eq_high", "HIGH", c, true) } };
    units[1] = { toggle ("dist_on", "ON", c), combo ("dist_os"), nullptr, { knob ("dist_drive", "DRIVE", c), knob ("dist_tone", "TONE", c, true), knob ("dist_mix", "MIX", c), knob ("dist_out", "OUTPUT", c, true) } };
    units[2] = { toggle ("cho_on", "ON", c), nullptr, nullptr, { knob ("cho_rate", "RATE", c), knob ("cho_depth", "DEPTH", c), knob ("cho_width", "WIDTH", c), knob ("cho_mix", "MIX", c) } };
    units[3] = { toggle ("dly_on", "ON", c), combo ("dly_div"), toggle ("dly_pp", "PING PONG", c), { knob ("dly_fb", "FEEDBACK", c), knob ("dly_filter", "FILTER", c), knob ("dly_mix", "MIX", c) } };
    units[4] = { toggle ("rvb_on", "ON", c), nullptr, nullptr, { knob ("rvb_pre", "PRE-DELAY", c), knob ("rvb_size", "SIZE", c), knob ("rvb_decay", "DECAY", c),
                                                                 knob ("rvb_damp", "DAMPING", c), knob ("rvb_width", "WIDTH", c), knob ("rvb_mix", "MIX", c) } };
    units[5] = { nullptr, nullptr, nullptr, { knob ("st_width", "WIDTH", c), knob ("st_balance", "BALANCE", c, true), knob ("st_monobass", "MONO BASS", c) } };
    units[6] = { toggle ("lim_on", "ON", c), nullptr, nullptr, { knob ("lim_input", "INPUT", c), knob ("lim_ceiling", "CEILING", c), knob ("lim_release", "RELEASE", c) } };
    units[7] = { nullptr, combo ("fx_order"), nullptr, { knob ("master", "MASTER", neutral(), false, 56) } };
}

void FxPage::resized()
{
    cards.clear();
    static const char* titles[] { "EQ", "DISTORTION", "CHORUS", "DELAY", "REVERB", "STEREO", "LIMITER", "FX ORDER + OUTPUT" };
    const Rectangle<int> areas[] { { 0, 0, 450, 150 }, { 462, 0, 450, 150 }, { 924, 0, 476, 150 },
                                   { 0, 162, 450, 160 }, { 462, 162, 938, 160 },
                                   { 0, 334, 450, 150 }, { 462, 334, 450, 150 }, { 924, 334, 476, 150 } };
    const int kh = Knob::heightFor (36);
    for (int u = 0; u < 8; ++u)
    {
        const auto a = areas[u];
        cards.push_back ({ titles[u], u == 7 ? neutral() : fx(), a });
        auto& x = units[(size_t) u];
        if (x.on) x.on->setBounds (a.getRight() - 62, a.getY() + 10, 46, 24);
        if (x.extra) x.extra->setBounds (a.getRight() - 274, a.getY() + 10, 92, 24);
        if (x.extra2) x.extra2->setBounds (a.getRight() - 174, a.getY() + 10, 104, 24);
        if (u == 7) { x.extra->setBounds (a.getX() + 16, a.getY() + 44, 300, 26); x.knobs[0]->setBounds (a.getRight() - 130, a.getY() + 30, 110, Knob::heightFor (56)); continue; }
        row (x.knobs, a.getX() + 14, a.getY() + 46, u == 4 ? 104 : 86, kh);
    }
}

// MELODY page: MelodyPage.cpp

// ======================================================================= FLOW bar
Rectangle<float> FlowBar::item (int i) const
{
    const float w = (float) getWidth() / 5.f;
    return { (float) i * w, 0.f, w, (float) getHeight() };
}
void FlowBar::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    static const char* names[] { "OSCILLATORS", "FILTER", "SCREAM", "FX", "OUTPUT" };
    const auto s = proc.gather (150.0);
    const float scr = s.scr + tune::dsp::FortuneMap::screamAdd (s, s.scr);
    const bool lit[] { true, true, scr > 0.002f, s.dlyOn || s.rvbOn || s.choOn || s.distOn || s.eqOn, proc.outputPeak() > 0.0005f };
    const Colour col[] { groupA(), filter(), scream(), fx(), fortune() };
    for (int i = 0; i < 5; ++i)
    {
        auto r = item (i).reduced (6.f, 4.f);
        g.setColour (lit[i] ? col[i].withAlpha (0.16f) : tokens().control);
        g.fillRoundedRectangle (r, 8.f);
        g.setColour (lit[i] ? col[i] : tokens().textMute);
        g.setFont (lnf.font (12.f, Bold, 0.1f));
        g.drawText (names[i], r, Justification::centred);
        if (i < 4)
        {
            g.setColour (tokens().textMute);
            const float x = item (i).getRight(), y = (float) getHeight() * 0.5f;
            Path a; a.addTriangle (x - 4.f, y - 4.f, x - 4.f, y + 4.f, x + 3.f, y);
            g.fillPath (a);
        }
    }
}
void FlowBar::mouseDown (const MouseEvent& e)
{
    static const int pageOf[] { 0, 0, 0, 2, 2 };
    for (int i = 0; i < 5; ++i) if (item (i).contains (e.position) && onPage) onPage (pageOf[i]);
}

// ======================================================================= canvas
doomsday::ui::Theme TuneCanvas::makeTheme()
{
    doomsday::ui::Theme t;
    const auto& k = tokens();
    t.bg = k.ground; t.panel = k.surface; t.text = k.text; t.value = k.text; t.label = k.textDim; t.dim = k.textMute;
    t.accents = { { fortune(), fortune().darker (0.7f), Colour (0xfffff0c2) } };
    return t;
}

TuneCanvas::TuneCanvas (TuneProcessor& p) : proc (p), presetBar (p, true), flow (p), synth (p), mod (p), fxp (p), melody (p), presets (p)
{
    setLookAndFeel (&lnf);
    presetBar.setLookAndFeel (&stageLnf);
    addAndMakeVisible (presetBar);
    presetBar.onBrowse = [this] { showPage (current == 4 ? 0 : 4); };            // a click on the preset name = the PRESETS page
    presets.onLoaded = [this] { presetBar.refreshList(); presetBar.refresh(); };
    addAndMakeVisible (tabs);
    tabs.onSelect = [this] (int i) { showPage (i); };
    addAndMakeVisible (fullRandom);
    fullRandom.setTooltip ("RANDOM: every sound-design parameter anywhere in its range (RANDOMIZE in the preset bar = SMART RANDOM, musical ranges)");
    fullRandom.onClick = [this] { proc.fullRandom(); };
    addAndMakeVisible (flow);
    flow.onPage = [this] (int i) { showPage (i); };
    for (auto* pg : pages()) addChildComponent (*pg);
    showPage (0);
    startTimerHz (20);
}
TuneCanvas::~TuneCanvas() { stopTimer(); presetBar.setLookAndFeel (nullptr); setLookAndFeel (nullptr); }

void TuneCanvas::showPage (int i)
{
    current = jlimit (0, 4, i);
    auto pg = pages();
    for (int k = 0; k < (int) pg.size(); ++k) pg[(size_t) k]->setVisible (k == current);
    tabs.setSelected (current);
}

void TuneCanvas::paint (Graphics& g)
{
    const auto& t = tokens();
    glassBackground (g, getLocalBounds().toFloat());
    g.setColour (Colours::white.withAlpha (0.07f)); g.fillRect (0, 96, W, 1);
    doomsday::brand::drawHeaderBadge (g, 1.f, 0.6f);
    const float tx = doomsday::brand::headerTextX (1.f);
    // 4TUNE in lime with a violet halo
    g.setFont (lnf.font (26.f, Bold, 0.12f));
    g.setColour (groupB().withAlpha (0.45f));
    g.drawText ("4TUNE", Rectangle<float> (tx + 1.5f, 11.5f, 200.f, 40.f), Justification::centredLeft);
    g.setColour (fortune());
    g.drawText ("4TUNE", Rectangle<float> (tx, 10.f, 200.f, 40.f), Justification::centredLeft);
    g.setFont (lnf.font (12.f, Medium)); g.setColour (t.textMute);
    g.drawText ("v0.2  |  hardstyle melody synthesizer  |  test build", Rectangle<float> (tx, 48.f, 420.f, 20.f), Justification::centredLeft);
}

void TuneCanvas::resized()
{
    presetBar.setBounds (W - 316, 3, 300, 90);
    tabs.setBounds (520, 30, 440, 36);
    fullRandom.setBounds (W - 316 - 130, 34, 120, 28);
    flow.setBounds (20, 104, W - 40, 40);
    const Rectangle<int> area (20, 152, W - 40, H - 152 - 16);
    for (auto* pg : pages()) pg->setBounds (area);
}

void TuneCanvas::timerCallback()
{
    presetBar.refresh();
    flow.tick();
    if (current == 0) synth.tick();
}

TuneEditor::TuneEditor (TuneProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (720, 450, 2160, 1350);
    getConstrainer()->setFixedAspectRatio ((double) TuneCanvas::W / (double) TuneCanvas::H);
    setSize (1260, 858);
}
void TuneEditor::resized() { doomsday::ui::scaleCanvas (*this, canvas, TuneCanvas::W, TuneCanvas::H); }
