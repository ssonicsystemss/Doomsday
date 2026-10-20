#include "PluginEditor.h"
#include "DoomsdayBrand.h"

using namespace juce;
using namespace doomsday::ui;

Theme AbyssTheme::make()
{
    Theme t;
    t.bg = Colour (0xff050303);
    t.panel = Colour (0xff0a0705);
    t.label = Colour (0xffb7a690);
    t.knobLight = Colour (0xff3a3128);
    t.knobDark = Colour (0xff080605);
    t.accents = { { amber, amberDeep, amberHot } };
    t.crown = { amberDeep.brighter (0.4f), amber, amberHot, Colours::white };
    return t;
}

namespace
{
    float raw (AbyssProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

    struct View
    {
        bool dlyOn, revOn, freeze, sync;
        int mode;
        float delayS, fb, dlyMix, pre, decay, revMix, size;
    };
    View viewOf (AbyssProcessor& p)
    {
        using namespace ParamIDs;
        View v;
        v.dlyOn = raw (p, dlyOn) > 0.5f; v.revOn = raw (p, revOn) > 0.5f; v.freeze = raw (p, freeze) > 0.5f; v.sync = raw (p, dlySync) > 0.5f;
        v.mode = (int) std::round (raw (p, dlyMode));
        const double beats = Choices::divisionBeats[jlimit (0, 11, (int) std::round (raw (p, dlyDiv)))];
        v.delayS = v.sync ? (float) (beats * 60.0 / jlimit (40.0, 300.0, p.bpm.load())) : raw (p, dlyMs) * 0.001f;
        v.fb = raw (p, dlyFb); v.dlyMix = raw (p, dlyMix);
        v.pre = raw (p, revPre) * 0.001f; v.decay = raw (p, revDecay); v.revMix = raw (p, revMix); v.size = raw (p, revSize);
        return v;
    }
    String secText (float s) { return s < 1.f ? String ((int) std::round (s * 1000.f)) + " ms" : String (s, 2) + " s"; }
}

// ======================================================================= THE ABYSS display
AbyssDisplay::AbyssDisplay (AbyssProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

void AbyssDisplay::tick()
{
    auto& e = proc.getEngine();
    const float d = e.delayPeak.exchange (0.f), r = e.reverbPeak.exchange (0.f);
    dlyLevel = jmax (d, dlyLevel * 0.9f);
    revLevel = jmax (r, revLevel * 0.93f);
    phase += 0.02f;
    repaint();
}

void AbyssDisplay::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    const auto amber = AbyssTheme::amber, hot = AbyssTheme::amberHot;
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff040302));
    g.fillRoundedRectangle (b, 8.f);
    auto plot = b.reduced (18.f, 14.f).withTrimmedTop (18.f).withTrimmedBottom (24.f);
    const auto v = viewOf (proc);

    // timeline length: enough to see the echoes and most of the tail
    float T = 1.2f;
    if (v.dlyOn) T = jmax (T, v.delayS * (v.fb > 0.05f ? jmin (8.f, 1.f + std::log (0.05f) / std::log (jmax (0.06f, v.fb))) : 1.5f) * (v.mode == 2 ? 1.5f : 1.f));
    if (v.revOn) T = jmax (T, v.pre + (v.freeze ? 4.f : v.decay * 0.9f));
    T = jlimit (1.f, 8.f, T);
    auto xOf = [&] (float s) { return plot.getX() + plot.getWidth() * s / T; };
    const float cy = plot.getCentreY(), half = plot.getHeight() * 0.5f;

    // grid
    const float step = T <= 2.f ? 0.25f : T <= 4.f ? 0.5f : 1.f;
    g.setFont (lnf.valueFont (10.5f));
    for (float s = 0.f; s <= T + 0.001f; s += step)
    {
        const float x = xOf (s);
        g.setColour (Colours::white.withAlpha (s == 0.f ? 0.10f : 0.045f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (lnf.theme.dim);
        g.drawText (s == 0.f ? String ("0") : secText (s), (int) x - 30, (int) plot.getBottom() + 4, 60, 14, Justification::centred);
    }
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine ((int) cy, plot.getX(), plot.getRight());
    g.setFont (lnf.labelFont (10.f, 0.25f));
    g.setColour (lnf.theme.dim);
    g.drawText ("L", (int) plot.getX() - 14, (int) plot.getY(), 12, 14, Justification::centred);
    g.drawText ("R", (int) plot.getX() - 14, (int) plot.getBottom() - 14, 12, 14, Justification::centred);

    // reverb tail: a mirrored mist that fades with the decay
    if (v.revOn)
    {
        const float glow = 0.55f + 0.45f * jlimit (0.f, 1.f, revLevel * 4.f);
        const float lvl = jlimit (0.15f, 1.f, 0.35f + v.revMix * 1.3f);
        Path tail;
        const int n = 160;
        std::vector<Point<float>> top;
        for (int i = 0; i <= n; ++i)
        {
            const float s = T * (float) i / (float) n;
            float a = 0.f;
            if (s >= v.pre)
            {
                const float t = s - v.pre;
                const float build = 1.f - std::exp (-t / (0.012f + 0.06f * v.size));     // diffusion builds up
                const float dec = v.freeze ? 1.f : std::pow (10.f, -3.f * t / jmax (0.1f, v.decay));
                a = build * dec;
            }
            a *= lvl * (0.9f + 0.1f * std::sin (phase * 3.f + (float) i * 0.35f) * a);
            top.push_back ({ xOf (s), cy - a * half * 0.92f });
        }
        tail.startNewSubPath (top.front());
        for (auto& p : top) tail.lineTo (p);
        for (auto it = top.rbegin(); it != top.rend(); ++it) tail.lineTo (it->x, 2.f * cy - it->y);
        tail.closeSubPath();
        ColourGradient mist (amber.withAlpha (0.42f * glow), xOf (v.pre), cy, amber.withAlpha (0.03f), xOf (T), cy, false);
        mist.addColour (0.5, AbyssTheme::amberDeep.withAlpha (0.25f * glow));
        g.setGradientFill (mist);
        g.fillPath (tail);
        g.setColour (amber.withAlpha (0.55f * glow));
        g.strokePath (tail, PathStrokeType (1.2f));
        if (v.pre > 0.002f)
        {
            g.setColour (hot.withAlpha (0.5f));
            const float x = xOf (v.pre);
            for (float y = plot.getY(); y < plot.getBottom(); y += 6.f) g.drawLine (x, y, x, y + 3.f, 1.f);
            g.setFont (lnf.labelFont (9.5f, 0.2f));
            g.drawText ("PRE", (int) x + 4, (int) plot.getY(), 40, 12, Justification::left);
        }
    }

    // echoes: one bar per repeat (left above, right below)
    if (v.dlyOn && v.delayS > 0.0005f)
    {
        const float glow = 0.6f + 0.4f * jlimit (0.f, 1.f, dlyLevel * 4.f);
        auto tap = [&] (float s, float a, bool up)
        {
            if (s > T || a < 0.01f) return;
            const float x = xOf (s), h = half * 0.94f * a;
            auto bar = up ? Rectangle<float> (x - 3.f, cy - h, 6.f, h) : Rectangle<float> (x - 3.f, cy, 6.f, h);
            g.setColour (amber.withAlpha (0.18f * glow));
            g.fillRoundedRectangle (bar.expanded (5.f, 2.f), 4.f);
            ColourGradient cg (hot.withAlpha (glow), x, up ? cy - h : cy + h, amber.withAlpha (0.5f * glow), x, cy, false);
            g.setGradientFill (cg);
            g.fillRoundedRectangle (bar, 3.f);
        };
        const float lvl = jlimit (0.2f, 1.f, 0.3f + v.dlyMix * 2.f);
        for (int k = 1; k <= 48; ++k)
        {
            const float a = lvl * std::pow (v.fb, (float) (k - 1));
            if (a < 0.01f) break;
            if (v.mode == 1) tap (v.delayS * (float) k, a, (k & 1) == 1);                 // ping-pong
            else if (v.mode == 2) { tap (v.delayS * (float) k, a, true); tap (v.delayS * 1.5f * (float) k, a, false); }
            else { tap (v.delayS * (float) k, a, true); tap (v.delayS * (float) k, a, false); }
        }
    }
    if (v.freeze && v.revOn)
    {
        g.setFont (lnf.titleFont (13.f, 0.4f));
        g.setColour (hot.withAlpha (0.6f + 0.3f * std::sin (phase * 2.f)));
        g.drawText ("FROZEN", plot.toNearestInt().removeFromTop (30), Justification::centredRight);
    }

    // header readout
    g.setFont (lnf.labelFont (11.f, 0.22f));
    g.setColour (lnf.theme.label);
    String info;
    if (v.dlyOn) info << "DELAY " << secText (v.delayS) << "   FB " << (int) std::round (v.fb * 100.f) << "%";
    if (v.revOn) info << (info.isEmpty() ? "" : "      ") << "REVERB " << (v.freeze ? String ("FROZEN") : secText (v.decay)) << "   PRE " << secText (v.pre);
    if (info.isEmpty()) info = "STEREO ONLY";
    g.drawText (info, b.reduced (18.f, 8.f).removeFromTop (16.f).toNearestInt(), Justification::centredRight);
    g.setFont (lnf.titleFont (11.f, 0.3f));
    g.setColour (amber);
    g.drawText ("THE ABYSS", b.reduced (18.f, 8.f).removeFromTop (16.f).toNearestInt(), Justification::centredLeft);
}

// ======================================================================= STEREO FIELD
void StereoField::tick()
{
    auto& e = proc.getEngine();
    const int w = e.scopeWrite.load (std::memory_order_acquire);
    if ((float) w == lastWrite) return;
    lastWrite = (float) w;
    const int n = 900;
    pts.clear();
    double lr = 0.0, ll = 1e-12, rr = 1e-12;
    float pk = 1e-4f;
    for (int i = 0; i < n; ++i)
    {
        const int k = (w - 1 - i + EchoEngine::scopeSize) & (EchoEngine::scopeSize - 1);
        const float l = e.scopeL[(size_t) k], r = e.scopeR[(size_t) k];
        lr += l * r; ll += l * l; rr += r * r;
        pk = jmax (pk, std::abs (l), std::abs (r));
        pts.push_back ({ (l - r) * 0.7071f, (l + r) * 0.7071f });
    }
    const float scale = 1.f / jmax (0.05f, pk * 1.1f);
    for (auto& p : pts) p *= scale;
    const float c = (float) (lr / std::sqrt (ll * rr));
    corr += 0.2f * ((ll < 1e-8 ? 1.f : c) - corr);
    repaint();
}

void StereoField::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    const auto amber = AbyssTheme::amber;
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff040302));
    g.fillRoundedRectangle (b, 8.f);
    auto bar = b.removeFromBottom (40.f).reduced (24.f, 10.f);
    auto sq = b.reduced (16.f).withTrimmedTop (16.f);
    const float r = jmin (sq.getWidth(), sq.getHeight() * 1.6f) * 0.5f;
    const Point<float> c (sq.getCentreX(), sq.getBottom() - 6.f);
    // half-circle guides: mid straight up, sides to the left / right
    g.setColour (Colours::white.withAlpha (0.06f));
    for (float k : { 0.33f, 0.66f, 1.f })
    {
        Path arc; arc.addCentredArc (c.x, c.y, r * k, r * k, 0.f, -MathConstants<float>::halfPi, MathConstants<float>::halfPi, true);
        g.strokePath (arc, PathStrokeType (1.f));
    }
    g.drawLine (c.x, c.y, c.x, c.y - r, 1.f);
    g.drawLine (c.x, c.y, c.x - r * 0.7071f, c.y - r * 0.7071f, 1.f);
    g.drawLine (c.x, c.y, c.x + r * 0.7071f, c.y - r * 0.7071f, 1.f);
    g.setFont (lnf.labelFont (10.f, 0.25f));
    g.setColour (lnf.theme.dim);
    g.drawText ("L", (int) (c.x - r * 0.7071f) - 18, (int) (c.y - r * 0.7071f) - 14, 14, 14, Justification::centred);
    g.drawText ("R", (int) (c.x + r * 0.7071f) + 4, (int) (c.y - r * 0.7071f) - 14, 14, 14, Justification::centred);
    g.drawText ("MID", (int) c.x - 20, (int) (c.y - r) - 16, 40, 14, Justification::centred);
    // dots (folded to the upper half like a classic goniometer)
    for (size_t i = 0; i < pts.size(); ++i)
    {
        auto p = pts[i];
        if (p.y < 0.f) p = -p;
        const float a = 0.55f * (1.f - (float) i / (float) pts.size());
        g.setColour (amber.withAlpha (a));
        g.fillRect (c.x + p.x * r - 1.f, c.y - p.y * r - 1.f, 2.f, 2.f);
    }
    // correlation
    g.setColour (Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (bar, 4.f);
    const float x0 = bar.getCentreX(), x1 = bar.getX() + bar.getWidth() * (0.5f + 0.5f * jlimit (-1.f, 1.f, corr));
    g.setColour (corr < 0.f ? Colour (0xffff5a3c) : amber);
    g.fillRoundedRectangle (Rectangle<float> (jmin (x0, x1), bar.getY() + 3.f, std::abs (x1 - x0) + 2.f, bar.getHeight() - 6.f), 3.f);
    g.setColour (lnf.theme.dim);
    g.setFont (lnf.labelFont (9.5f, 0.2f));
    g.drawText ("-1", bar.toNearestInt().withTrimmedLeft (-22).removeFromLeft (20), Justification::centred);
    g.drawText ("+1", bar.toNearestInt().withTrimmedRight (-22).removeFromRight (20), Justification::centred);
    g.setColour (lnf.theme.label);
    g.drawText ("CORRELATION " + String (corr, 2), bar.toNearestInt().translated (0, -16), Justification::centred);
}

// ======================================================================= Canvas
Knob* Canvas::knob (const char* id, const char* label, bool bipolar)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, 0, bipolar);
    auto* raw = k.get(); addAndMakeVisible (*raw); owned.push_back (std::move (k)); return raw;
}

Canvas::Canvas (AbyssProcessor& p) : proc (p), lnf (AbyssTheme::make()), display (p), field (p), presets (p, false, false)
{
    using namespace ParamIDs;
    setLookAndFeel (&lnf);
    addAndMakeVisible (display);
    addAndMakeVisible (field);
    addAndMakeVisible (presets);
    addAndMakeVisible (meter);

    dlyPower = std::make_unique<Switch> (p.apvts, dlyOn, "", Switch::Power);
    revPower = std::make_unique<Switch> (p.apvts, revOn, "", Switch::Power);
    syncChip = std::make_unique<Switch> (p.apvts, dlySync, "SYNC", Switch::Chip);
    freezeChip = std::make_unique<Switch> (p.apvts, freeze, "FREEZE", Switch::Chip);
    dryChip = std::make_unique<Switch> (p.apvts, dry, "DRY", Switch::Chip);
    modeChoice = std::make_unique<Choice> (p.apvts, dlyMode, Choices::modes);
    for (Component* c : { (Component*) dlyPower.get(), (Component*) revPower.get(), (Component*) syncChip.get(), (Component*) freezeChip.get(),
                          (Component*) dryChip.get(), (Component*) modeChoice.get() })
        addAndMakeVisible (c);
    freezeChip->setTooltip ("FREEZE - holds the reverb tail forever (new sound stops going in)");
    dryChip->setTooltip ("DRY off = only the effect comes out (for a send / bus in the FL mixer)");
    syncChip->setTooltip ("SYNC - delay time in notes from the project tempo; off = milliseconds");

    divKnob = knob (dlyDiv, "TIME");
    msKnob = knob (dlyMs, "TIME");
    dlyCells = { divKnob, knob (dlyFb, "FEEDBACK"), knob (dlyLo, "LOW CUT"), knob (dlyHi, "HIGH CUT"),
                 knob (dlyMod, "WOBBLE"), knob (dlyDrive, "DRIVE"), knob (dlyDuck, "DUCK"), knob (dlyMix, "MIX") };
    revCells = { knob (revPre, "PRE-DELAY"), knob (revSize, "SIZE"), knob (revDecay, "DECAY"), knob (revDamp, "DAMP"), knob (revLo, "LOW CUT"),
                 knob (revMod, "MOTION"), knob (revShim, "SHIMMER"), knob (revDuck, "DUCK"), knob (revWidth, "WIDTH"), knob (revMix, "MIX") };
    outCells = { knob (d2r, "DELAY > REV"), knob (width, "WIDTH"), knob (spread, "SPREAD"), knob (monoBass, "MONO BASS"),
                 knob (mix, "MIX"), dryChip.get(), knob (output, "OUTPUT", true) };
    setSize (W, H);
    timerCallback();
    startTimerHz (30);
}

Canvas::~Canvas() { setLookAndFeel (nullptr); }

void Canvas::grid (Rectangle<int> a, const std::vector<Component*>& cells, int cols)
{
    const int rows = ((int) cells.size() + cols - 1) / cols;
    const float cw = (float) a.getWidth() / (float) cols, rh = (float) a.getHeight() / (float) rows;
    for (size_t i = 0; i < cells.size(); ++i)
    {
        const int r = (int) i / cols, c = (int) i % cols;
        Rectangle<int> cell (a.getX() + (int) (cw * (float) c), a.getY() + (int) (rh * (float) r), (int) cw, (int) rh);
        if (dynamic_cast<Switch*> (cells[i]) != nullptr) cells[i]->setBounds (cell.withSizeKeepingCentre (jmin (cell.getWidth() - 8, 96), 28));
        else cells[i]->setBounds (cell.withSizeKeepingCentre (jmin (cell.getWidth() - 2, 92), cell.getHeight()));
    }
}

void Canvas::resized()
{
    panels.clear();
    const int m = 20, gap = 14;
    presets.setBounds (W - m - 300, 34, 300, 90);
    const Rectangle<int> dispA (m, 150, 920, 300), fieldA (dispA.getRight() + gap, 150, W - m - dispA.getRight() - gap, 300);
    display.setBounds (dispA);
    panels.push_back ({ "STEREO FIELD", fieldA });
    field.setBounds (fieldA.reduced (12, 10).withTrimmedTop (26));

    const Rectangle<int> dlyA (m, dispA.getBottom() + gap, 662, 256), revA (dlyA.getRight() + gap, dlyA.getY(), W - m - dlyA.getRight() - gap, 256);
    panels.push_back ({ "DELAY", dlyA });
    panels.push_back ({ "REVERB", revA });
    {
        auto h = dlyA.reduced (14, 10).removeFromTop (28);
        dlyPower->setBounds (h.removeFromRight (56));
        h.removeFromRight (12);
        syncChip->setBounds (h.removeFromRight (86));
        h.removeFromRight (12);
        modeChoice->setBounds (h.removeFromRight (300));
        auto body = dlyA.reduced (14, 10); body.removeFromTop (38);
        grid (body, dlyCells, 4);
        msKnob->setBounds (divKnob->getBounds());
    }
    {
        auto h = revA.reduced (14, 10).removeFromTop (28);
        revPower->setBounds (h.removeFromRight (56));
        h.removeFromRight (12);
        freezeChip->setBounds (h.removeFromRight (100));
        auto body = revA.reduced (14, 10); body.removeFromTop (38);
        grid (body, revCells, 5);
    }
    const Rectangle<int> outA (m, dlyA.getBottom() + gap, W - 2 * m, H - m - dlyA.getBottom() - gap);
    panels.push_back ({ "STEREO / OUTPUT", outA });
    {
        auto body = outA.reduced (14, 10); body.removeFromTop (26);
        meter.setBounds (body.removeFromRight (16).reduced (0, 2));
        body.removeFromRight (10);
        grid (body, outCells, (int) outCells.size());
    }
    renderBackground();
}

void Canvas::renderBackground()
{
    const float scale = 2.f, ui = 1120.f / (float) W;
    background = Image (Image::RGB, (int) (W * scale), (int) (H * scale), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));
    const auto amber = AbyssTheme::amber;
    g.fillAll (lnf.theme.bg);
    // a warm glow rising out of the dark, two faint beams
    {
        ColourGradient haze (amber.withAlpha (0.16f), W * 0.5f, 70.f, amber.withAlpha (0.f), W * 0.5f + 460.f, 70.f, true);
        g.setGradientFill (haze); g.fillRect (0, 0, W, 170);
        ColourGradient deep (AbyssTheme::amberDeep.withAlpha (0.18f), W * 0.5f, (float) H + 200.f, Colours::transparentBlack, W * 0.5f, (float) H - 380.f, true);
        g.setGradientFill (deep); g.fillRect (0, H - 420, W, 420);
        for (int i = 0; i < 3; ++i)
        {
            const float a = -0.45f + (float) i * 0.2f;
            paint::beam (g, { 180.f, -20.f }, -a - 0.2f, 0.05f, 1200.f, amber, 0.035f);
            paint::beam (g, { W - 180.f, -20.f }, a + 0.2f, 0.05f, 1200.f, amber, 0.035f);
        }
        ColourGradient vig (Colours::transparentBlack, W * 0.5f, H * 0.45f, Colours::black.withAlpha (0.7f), 0.f, -80.f, true);
        g.setGradientFill (vig); g.fillRect (0, 0, W, H);
        ColourGradient line (amber.withAlpha (0.f), 0.f, 0.f, amber.withAlpha (0.f), (float) W, 0.f, false);
        line.addColour (0.5, amber.withAlpha (0.75f));
        g.setGradientFill (line); g.fillRect (0.f, 136.f, (float) W, 1.f);
    }
    // title with its echoes trailing away to the right
    {
        const String title ("ABYSS OF ECHOES");
        const auto area = Rectangle<float> (0.f, 28.f, (float) W, 56.f);
        g.setFont (lnf.titleFont (38.f, 0.22f));
        for (int e = 4; e >= 1; --e)
        {
            g.setColour (amber.withAlpha (0.11f / (float) e));
            g.drawText (title, area.translated (14.f * (float) e, 5.f * (float) e), Justification::centred);
        }
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (amber.withAlpha (0.09f / (float) i));
            for (auto o : { Point<float> (-1.5f * (float) i, 0.f), Point<float> (1.5f * (float) i, 0.f), Point<float> (0.f, 1.5f * (float) i), Point<float> (0.f, -1.5f * (float) i) })
                g.drawText (title, area.translated (o.x, o.y), Justification::centred);
        }
        ColourGradient tg (Colours::white, 0.f, area.getY() + 8.f, amber, 0.f, area.getBottom() - 6.f, false);
        g.setGradientFill (tg);
        g.drawText (title, area, Justification::centred);
        g.setFont (lnf.labelFont (12.5f, 0.6f));
        g.setColour (amber.withAlpha (0.85f));
        g.drawText ("REVERB  /  DELAY  /  STEREO", 0, 104, W, 18, Justification::centred);
    }
    // standard header: badge + version lines
    const int tx = (int) doomsday::brand::headerTextX (ui);
    g.setFont (lnf.titleFont (10.f, 0.25f));
    g.setColour (lnf.theme.text.withAlpha (0.75f));
    g.drawText ("V1.0", tx, 40, 300, 14, Justification::left);
    g.setFont (lnf.labelFont (12.f, 0.28f));
    g.setColour (lnf.theme.label);
    g.drawText ("ECHOES  /  SPACE  /  WIDTH", tx, 58, 320, 16, Justification::left);
    g.setColour (amber);
    g.drawText ("LISTEN TO THE ABYSS", tx, 76, 320, 16, Justification::left);
    doomsday::brand::drawHeaderBadge (g, ui, 0.55f);
    for (auto& [t, a] : panels) paint::glassPanel (g, lnf, a, t, 0);
    // display frame
    {
        auto a = display.getBounds().toFloat();
        g.setColour (amber.withAlpha (0.25f));
        g.drawRoundedRectangle (a.expanded (1.f), 9.f, 1.f);
    }
}

void Canvas::timerCallback()
{
    using namespace ParamIDs;
    display.tick();
    field.tick();
    presets.refresh();
    meter.setLevel (proc.getEngine().outPeak.exchange (0.f));
    const bool sync = raw (proc, dlySync) > 0.5f;
    if (divKnob->isVisible() != sync) divKnob->setVisible (sync);
    if (msKnob->isVisible() == sync) msKnob->setVisible (! sync);
    auto dim = [] (const std::vector<Component*>& cells, bool on, std::initializer_list<Component*> extra = {})
    {
        const float a = on ? 1.f : 0.32f;
        for (auto* c : cells) if (std::abs (c->getAlpha() - a) > 0.001f) c->setAlpha (a);
        for (auto* c : extra) if (std::abs (c->getAlpha() - a) > 0.001f) c->setAlpha (a);
    };
    dim (dlyCells, raw (proc, dlyOn) > 0.5f, { msKnob, syncChip.get(), modeChoice.get() });
    dim (revCells, raw (proc, revOn) > 0.5f, { freezeChip.get() });
}

void Canvas::paint (Graphics& g)
{
    if (background.isValid()) g.drawImage (background, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
}

AbyssEditor::AbyssEditor (AbyssProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (840, 540, 2100, 1350);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (1120, 720);
}
