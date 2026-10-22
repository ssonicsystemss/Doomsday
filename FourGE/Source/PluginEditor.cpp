#include "PluginEditor.h"
#include "DoomsdayBrand.h"

using namespace juce;
using namespace doomsday::ui;
using ge::Shape;

Theme ForgeTheme::make()
{
    Theme t;
    t.bg = Colour (0xff030202);
    t.panel = Colour (0xff090605);
    t.label = Colour (0xffb5a59c);
    t.value = Colour (0xffe6d6cc);
    t.knobLight = Colour (0xff3a2f2a);
    t.knobDark = Colour (0xff070404);
    t.accents = { { magma, magmaDeep, magmaHot }, { heat, Colour (0xff6b4a08), Colour (0xfffff0b8) }, { ember, Colour (0xff4a0904), Colour (0xffff9a80) } };
    t.crown = { magmaDeep.brighter (0.6f), ember, magma, magmaHot, heat };
    return t;
}

namespace
{
    // the anvil / lane drawing code asks for title / label / value fonts and text / dim / label colours: flat look
    struct ForgeLook
    {
        doomsday::flat::FlatLookAndFeel& f;
        struct Th { Colour text, dim, label; } theme;
        explicit ForgeLook (Component& c) : f (doomsday::flat::lnfOf (c)),
            theme { doomsday::flat::tokens().text, doomsday::flat::tokens().textMute, doomsday::flat::tokens().textDim } {}
        Font titleFont (float h, float = 0.f) const { return f.font (h, doomsday::flat::Bold, 0.04f); }
        Font labelFont (float h, float = 0.f) const { return f.font (h, doomsday::flat::SemiBold); }
        Font valueFont (float h) const { return f.font (h, doomsday::flat::Medium); }
    };
    float raw (GeProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    const Colour envCol[3] = { ForgeTheme::magmaHot, ForgeTheme::heat, Colour (0xffffe6d2) };
    const char* envName[3] = { "AMP", "DRIVE", "PITCH" };
}

// ======================================================================= THE ANVIL
AnvilDisplay::AnvilDisplay (GeProcessor& p) : proc (p)
{
    for (auto* prm : proc.getParameters())
        if (auto* wp = dynamic_cast<AudioProcessorParameterWithID*> (prm)) proc.apvts.addParameterListener (wp->paramID, this);
    setRepaintsOnMouseActivity (false);
}
AnvilDisplay::~AnvilDisplay()
{
    for (auto* prm : proc.getParameters())
        if (auto* wp = dynamic_cast<AudioProcessorParameterWithID*> (prm)) proc.apvts.removeParameterListener (wp->paramID, this);
}
void AnvilDisplay::resized()
{
    plot = getLocalBounds().toFloat().reduced (18.f, 14.f).withTrimmedTop (34.f).withTrimmedBottom (20.f);
    dirty.store (true);
}
Rectangle<float> AnvilDisplay::tabBounds (int i) const { return { 18.f + (float) i * 92.f, 10.f, 86.f, 26.f }; }   // AMP / DRIVE / PITCH

Point<float> AnvilDisplay::toScreen (const Shape::Pt& pt) const
{
    return { plot.getX() + plot.getWidth() * pt.ms / 1000.f, plot.getBottom() - plot.getHeight() * pt.v };
}
Shape::Pt AnvilDisplay::fromScreen (Point<float> s) const
{
    return { jlimit (0.f, 1000.f, (s.x - plot.getX()) / plot.getWidth() * 1000.f), jlimit (0.f, 1.f, (plot.getBottom() - s.y) / plot.getHeight()) };
}
int AnvilDisplay::pointAt (Point<float> s) const
{
    const auto& sh = shape[editing];
    int best = -1; float bd = 11.f;
    for (int i = 0; i < sh.n; ++i)
    {
        const float d = toScreen (sh.pts[(size_t) i]).getDistanceFrom (s);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

void AnvilDisplay::mouseMove (const MouseEvent& e)
{
    for (int i = 0; i < 3; ++i) if (tabBounds (i).contains (e.position)) { setMouseCursor (MouseCursor::PointingHandCursor); return; }
    const int h = pointAt (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? MouseCursor::DraggingHandCursor : plot.expanded (6.f).contains (e.position) ? MouseCursor::CrosshairCursor : MouseCursor::NormalCursor);
}
void AnvilDisplay::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < 3; ++i) if (tabBounds (i).contains (e.position)) { editing = i; hover = drag = -1; repaint(); return; }
    if (editing == 2 && raw (proc, ParamIDs::pitchMode) < 0.5f) { convertCurveToDrawn(); return; }
    auto sh = shape[editing];
    const int hit = pointAt (e.position);
    if (e.mods.isPopupMenu())
    {
        if (hit > 0) { sh.remove (hit); proc.setShape (editing, sh); }
        return;
    }
    if (hit >= 0) { drag = hit; return; }
    if (! plot.expanded (6.f).contains (e.position)) return;
    const auto pt = fromScreen (e.position);
    if (pt.ms <= 0.5f || ! sh.insert (pt.ms, pt.v)) return;
    for (int i = 0; i < sh.n; ++i) if (sh.pts[(size_t) i].ms == pt.ms && sh.pts[(size_t) i].v == pt.v) drag = i;
    proc.setShape (editing, sh);
    shape[editing] = sh;
}
void AnvilDisplay::mouseDrag (const MouseEvent& e)
{
    if (drag < 0) return;
    auto sh = shape[editing];
    if (drag >= sh.n) return;
    auto pt = fromScreen (e.position);
    // the first dot stays on the hit; a dot never passes its neighbours (they do not move)
    const float lo = drag == 0 ? 0.f : sh.pts[(size_t) drag - 1].ms + 0.5f;
    const float hi = drag == 0 ? 0.f : (drag + 1 < sh.n ? sh.pts[(size_t) drag + 1].ms - 0.5f : 1000.f);
    sh.pts[(size_t) drag] = { jlimit (lo, jmax (lo, hi), pt.ms), pt.v, sh.pts[(size_t) drag].bend };
    shape[editing] = sh;
    proc.setShape (editing, sh);
    repaint();
}
void AnvilDisplay::mouseUp (const MouseEvent&) { drag = -1; }

// first touch on PITCH in CURVE mode: copy the knob curve into dots and switch to DRAWN (nothing jumps)
void AnvilDisplay::convertCurveToDrawn()
{
    const auto prm = proc.gatherForGui();
    Shape s; s.n = 0;
    for (float pm : { 0.f, 15.f, 40.f, 80.f, 150.f, 300.f, 600.f, 1000.f })
    {
        const float ms = pm * prm.k.lengthMs / 1000.f;
        const float ratio = 1.f + (prm.k.startRatio - 1.f) * std::exp (-jmax (0.f, ms - prm.k.holdMs) / jmax (1.f, prm.k.dropMs) * prm.k.curve);
        s.pts[(size_t) s.n++] = { pm, jlimit (0.f, 1.f, std::log2 (ratio) / 4.f) };
    }
    s.tidy();
    proc.setShape (2, s);
    if (auto* mp = proc.apvts.getParameter (ParamIDs::pitchMode)) { mp->beginChangeGesture(); mp->setValueNotifyingHost (1.f); mp->endChangeGesture(); }
}
void AnvilDisplay::mouseDoubleClick (const MouseEvent& e)
{
    const int hit = pointAt (e.position);
    if (hit > 0) { auto sh = shape[editing]; sh.remove (hit); proc.setShape (editing, sh); hover = -1; }
}

void AnvilDisplay::renderPreview()
{
    auto prm = proc.gatherForGui();
    shape[0] = proc.getShape (0); shape[1] = proc.getShape (1); shape[2] = proc.getShape (2);
    prm.k.amp = shape[0]; prm.k.drive = shape[1]; prm.k.pitch = shape[2];
    prm.k.cook (96000.0);
    previewMs = prm.k.lengthMs;
    const double rate = 96000.0;
    const int total = (int) (rate * previewMs / 1000.0);
    const int cols = jmax (2, (int) plot.getWidth());
    lo.assign ((size_t) cols, 0.f); hi.assign ((size_t) cols, 0.f); pitch.assign ((size_t) cols, 0.f);
    ge::Voice v;
    v.prepare (rate);
    v.setRoom (prm.k.roomSize);
    v.trigger (440.f * std::pow (2.f, (float) (proc.auditionNote - 69) / 12.f), 1.f, prm.k);
    const float g = std::pow (10.f, prm.clipDriveDb / 20.f), outG = std::pow (10.f, prm.outDb / 20.f);
    for (int i = 0; i < total; ++i)
    {
        const float y = doomsday::kick::masterClip (v.render (prm.k), g, prm.clipSoft) * outG;
        const auto c = (size_t) jmin (cols - 1, (int) ((int64) i * cols / jmax (1, total)));
        lo[c] = jmin (lo[c], jmax (-1.f, y)); hi[c] = jmax (hi[c], jmin (1.f, y));     // a preset louder than 0 dBFS stays inside the plot
    }
    const float endHz = 440.f * std::pow (2.f, ((float) proc.auditionNote - 69.f + prm.k.tuneSemis) / 12.f);
    for (int c = 0; c < cols; ++c)
    {
        const float ms = previewMs * (float) c / (float) cols;
        pitch[(size_t) c] = prm.k.pitchDrawn ? endHz * std::pow (2.f, prm.k.pitch.at ((float) c / (float) cols * 1000.f) * 4.f)
                                             : (prm.k.startFixed ? endHz + (prm.k.startHz - endHz) * std::exp (-jmax (0.f, ms - prm.k.holdMs) / jmax (1.f, prm.k.dropMs) * prm.k.curve)
                                                                 : endHz * (1.f + (prm.k.startRatio - 1.f) * std::exp (-jmax (0.f, ms - prm.k.holdMs) / jmax (1.f, prm.k.dropMs) * prm.k.curve)));
    }
}

void AnvilDisplay::tick()
{
    const int ss = proc.getShapeSerial();
    if (dirty.exchange (false) || ss != lastShapeSerial) { lastShapeSerial = ss; renderPreview(); repaint(); }
    auto& e = proc.getEngine();
    const int hs = e.hitSerial.load();
    if (hs != lastHit) { lastHit = hs; hitStart = Time::getMillisecondCounterHiRes(); }
    const float was = playMs;
    playMs = lastHit > 0 ? (float) (Time::getMillisecondCounterHiRes() - hitStart) : -1.f;
    if (playMs > previewMs + 60.f) playMs = -1.f;
    if (playMs >= 0.f || was >= 0.f) repaint();
}

void AnvilDisplay::paint (Graphics& g)
{
    ForgeLook lnf (*this);
    auto b = getLocalBounds().toFloat();
    g.setColour (doomsday::flat::tokens().surface); g.fillRoundedRectangle (b, 12.f);
    {
        ColourGradient floor (ForgeTheme::magma.withAlpha (0.10f), b.getCentreX(), b.getBottom() + 40.f, ForgeTheme::magma.withAlpha (0.f), b.getCentreX(), b.getY() + 40.f, true);
        g.setGradientFill (floor); g.fillRoundedRectangle (b, 9.f);
    }
    // tabs
    for (int i = 0; i < 3; ++i)
    {
        auto r = tabBounds (i);
        const bool on = editing == i;
        g.setColour (on ? envCol[i].withAlpha (0.16f) : Colours::white.withAlpha (0.03f)); g.fillRoundedRectangle (r, 6.f);
        g.setColour (on ? envCol[i].withAlpha (0.85f) : Colours::white.withAlpha (0.12f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.f, on ? 1.4f : 1.f);
        g.setFont (lnf.titleFont (11.f, 0.25f)); g.setColour (on ? lnf.theme.text : lnf.theme.dim);
        g.drawText (envName[i], r, Justification::centred);
    }
    g.setFont (lnf.labelFont (11.5f, 0.15f)); g.setColour (lnf.theme.dim);
    const bool drawnPitch = raw (proc, ParamIDs::pitchMode) > 0.5f;
    g.drawText (editing == 0 ? "AMP  -  volume of the tail over the kick"
              : editing == 1 ? "DRIVE  -  how dirty the kick is over time (swell it up = reverse bass)"
              : drawnPitch ? "PITCH  -  drawn pitch of the tail: bottom = the note, top = +4 octaves"
                           : "PITCH  -  now set by the TAIL knobs. Click here to draw it yourself",
                Rectangle<float> (tabBounds (2).getRight() + 16.f, 10.f, 640.f, 26.f), Justification::centredLeft);
    g.drawText ("CLICK: ADD DOT    DRAG: MOVE    DOUBLE / RIGHT CLICK: DELETE", Rectangle<float> (b.getRight() - 470.f, 10.f, 450.f, 26.f), Justification::centredRight);

    // grid (time)
    g.setFont (lnf.valueFont (10.f));
    const float step = previewMs > 900.f ? 200.f : previewMs > 400.f ? 100.f : 50.f;
    for (float t = 0.f; t <= previewMs + 0.1f; t += step)
    {
        const float x = plot.getX() + plot.getWidth() * t / previewMs;
        g.setColour (Colours::white.withAlpha (0.05f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (lnf.theme.dim); g.drawText (String ((int) t) + " ms", (int) x - 30, (int) plot.getBottom() + 4, 60, 14, Justification::centred);
    }
    for (float v : { 0.25f, 0.5f, 0.75f })
    {
        g.setColour (Colours::white.withAlpha (0.035f)); g.drawHorizontalLine ((int) (plot.getBottom() - plot.getHeight() * v), plot.getX(), plot.getRight());
    }
    // waveform (centred, molten)
    const int cols = (int) lo.size();
    if (cols > 1)
    {
        const float cy = plot.getCentreY(), hh = plot.getHeight() * 0.5f;
        Path wave;
        wave.startNewSubPath (plot.getX(), cy);
        for (int c = 0; c < cols; ++c) wave.lineTo (plot.getX() + (float) c, cy - hi[(size_t) c] * hh);
        for (int c = cols - 1; c >= 0; --c) wave.lineTo (plot.getX() + (float) c, cy - lo[(size_t) c] * hh);
        wave.closeSubPath();
        ColourGradient mg (ForgeTheme::magmaHot.withAlpha (0.55f), 0.f, cy, ForgeTheme::ember.withAlpha (0.35f), 0.f, cy - hh, false);
        mg.addColour (0.55, ForgeTheme::magma.withAlpha (0.45f));
        g.setGradientFill (mg); g.fillPath (wave);
        g.setColour (ForgeTheme::magma.withAlpha (0.7f)); g.strokePath (wave, PathStrokeType (0.8f));
        // pitch (log scale 30 Hz .. 1 kHz)
        Path pp;
        for (int c = 0; c < cols; ++c)
        {
            const float y = plot.getBottom() - plot.getHeight() * jlimit (0.f, 1.f, std::log (pitch[(size_t) c] / 30.f) / std::log (1000.f / 30.f));
            if (c == 0) pp.startNewSubPath (plot.getX(), y); else pp.lineTo (plot.getX() + (float) c, y);
        }
        g.setColour (Colours::white.withAlpha (0.45f));
        PathStrokeType (1.f).createDashedStroke (pp, pp, std::array<float, 2> { 5.f, 4.f }.data(), 2);
        g.fillPath (pp);
        g.setFont (lnf.labelFont (10.5f, 0.1f));
        g.drawText ("PITCH  " + String ((int) pitch.front()) + " > " + String ((int) pitch.back()) + " Hz", (int) plot.getRight() - 220, (int) plot.getY() + 2, 216, 14, Justification::right);
    }
    // the other envelope, faint
    auto envPath = [this] (const Shape& sh)
    {
        Path p;
        for (int i = 0; i < sh.n; ++i) { auto s = toScreen (sh.pts[(size_t) i]); if (i == 0) p.startNewSubPath (s); else p.lineTo (s); }
        auto last = toScreen (sh.pts[(size_t) sh.n - 1]);
        if (last.x < plot.getRight()) p.lineTo (plot.getRight(), last.y);
        return p;
    };
    for (int other = 0; other < 2; ++other)
        if (other != editing) { g.setColour (envCol[other].withAlpha (0.28f)); g.strokePath (envPath (shape[other]), PathStrokeType (1.2f)); }
    const bool showEdit = editing < 2 || drawnPitch;
    if (showEdit)
    {
        auto p = envPath (shape[editing]);
        g.setColour (envCol[editing].withAlpha (0.22f)); g.strokePath (p, PathStrokeType (5.f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (envCol[editing]); g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        const auto& sh = shape[editing];
        for (int i = 0; i < sh.n; ++i)
        {
            auto s = toScreen (sh.pts[(size_t) i]);
            const bool h = i == hover || i == drag;
            g.setColour (Colours::black); g.fillEllipse (Rectangle<float> (h ? 13.f : 10.f, h ? 13.f : 10.f).withCentre (s));
            g.setColour (i == 0 ? Colours::white : envCol[editing]); g.drawEllipse (Rectangle<float> (h ? 13.f : 10.f, h ? 13.f : 10.f).withCentre (s), 1.8f);
            if (h)
            {
                g.setFont (lnf.valueFont (11.f)); g.setColour (lnf.theme.text);
                const auto ms = sh.pts[(size_t) i].ms * previewMs / 1000.f;
                g.drawText (String ((int) ms) + " ms  /  " + (editing == 2 ? "+" + String (sh.pts[(size_t) i].v * 48.f, 1) + " st"
                                                                                : String ((int) std::round (sh.pts[(size_t) i].v * 100.f)) + " %"),
                            Rectangle<float> (150.f, 16.f).withCentre (s.translated (0.f, s.y < plot.getY() + 30.f ? 18.f : -18.f)), Justification::centred);
            }
        }
    }
    // playhead of the last hit
    if (playMs >= 0.f && playMs <= previewMs)
    {
        const float x = plot.getX() + plot.getWidth() * playMs / previewMs;
        ColourGradient ph (ForgeTheme::magmaHot.withAlpha (0.f), x - 40.f, 0.f, ForgeTheme::magmaHot.withAlpha (0.25f), x, 0.f, false);
        g.setGradientFill (ph); g.fillRect (Rectangle<float> (x - 40.f, plot.getY(), 40.f, plot.getHeight()));
        g.setColour (ForgeTheme::magmaHot); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }
}

// ======================================================================= SOLO / MUTE strip
namespace
{
    const char* layerName[LayerStrip::count] { "Punch", "Tail", "Texture", "Sub", "Click", "Sample" };
    const int layerBit[LayerStrip::count] { ge::LPunch, ge::LTail, ge::LTexture, ge::LSub, ge::LClick, ge::LSample };
}
Rectangle<float> LayerStrip::cell (int i) const
{
    const float w = (float) getWidth() / (float) count;
    return { w * (float) i, 0.f, w - 6.f, (float) getHeight() };
}
Rectangle<float> LayerStrip::button (int i, bool solo) const
{
    auto c = cell (i).reduced (4.f, 5.f);
    return c.removeFromRight (21.f * 2.f + 3.f).removeFromLeft (21.f).translated (solo ? 24.f : 0.f, 0.f);
}
void LayerStrip::paint (Graphics& g)
{
    ForgeLook lnf (*this);
    const auto& t = doomsday::flat::tokens();
    const int mute = proc.muteMask.load(), solo = proc.soloMask.load(), eff = proc.effectiveMute();
    for (int i = 0; i < count; ++i)
    {
        auto c = cell (i);
        const bool silent = (eff & layerBit[i]) != 0;
        g.setColour (t.surface); g.fillRoundedRectangle (c, 8.f);
        g.setFont (lnf.labelFont (10.5f)); g.setColour (silent ? lnf.theme.dim.withAlpha (0.5f) : lnf.theme.text);
        g.drawText (String (layerName[i]).toUpperCase(), c.reduced (8.f, 0.f).withTrimmedRight (48.f), Justification::centredLeft, false);
        for (int sb = 0; sb < 2; ++sb)
        {
            auto r = button (i, sb == 1);
            const bool on = sb == 0 ? (mute & layerBit[i]) != 0 : (solo & layerBit[i]) != 0;
            const Colour col = sb == 0 ? ForgeTheme::ember : ForgeTheme::heat;
            g.setColour (on ? col.withAlpha (0.85f) : Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (r, 5.f);
            g.setColour (on ? Colours::black : lnf.theme.dim); g.setFont (lnf.titleFont (11.f));
            g.drawText (sb == 0 ? "M" : "S", r, Justification::centred);
        }
    }
}
void LayerStrip::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < count; ++i)
        for (int sb = 0; sb < 2; ++sb)
            if (button (i, sb == 1).expanded (2.f).contains (e.position))
            {
                auto& a = sb == 0 ? proc.muteMask : proc.soloMask;
                a.store (a.load() ^ layerBit[i]);
                repaint();
                return;
            }
}

// ======================================================================= canvas (flat, like 5CR34M 4LPH4)
using namespace doomsday::flat;

namespace { const String dotSep (CharPointer_UTF8 (" \xc2\xb7 ")); }

Canvas::Card& Canvas::card (const String& title, Colour dot, int pg)
{
    cards.push_back ({});
    auto& c = cards.back(); c.title = title; c.dot = dot; c.page = pg;
    return c;
}

Component* Canvas::knob (Card& c, const char* id, const char* label, const char* tip, bool bipolar)
{
    auto k = std::make_unique<doomsday::flat::Knob> (proc.apvts, id, label, c.dot, bipolar);
    k->slider.setTooltip (tip);
    auto* kp = k.get(); addAndMakeVisible (*kp); owned.push_back (std::move (k));
    c.knobs.push_back (kp);
    return kp;
}

Canvas::Canvas (GeProcessor& p) : proc (p), anvil (p), layers (p)
{
    using namespace ParamIDs;
    setLookAndFeel (&lnf);
    const auto& t = tokens();
    const Colour hot = t.hot, cool = t.cool, n { 0xffb4bac3 }, gold { 0xffffc46b };
    cards.reserve (20);
    auto seg = [this] (const char* id, StringArray labels, Colour c, const char* tip)
    {
        auto s = std::make_unique<Segmented> (proc.apvts, id, labels, c);
        s->setTooltip (tip);
        segW[s.get()] = jmax (84, labels.size() * 38);
        auto* sp = s.get(); addAndMakeVisible (*sp); owned.push_back (std::move (s)); return (Component*) sp;
    };
    auto toggle = [this] (const char* id, const char* text, Toggle::Style st, Colour c, const char* tip)
    {
        auto s = std::make_unique<Toggle> (proc.apvts, id, text, st, c);
        s->setTooltip (tip);
        auto* sp = s.get(); addAndMakeVisible (*sp); owned.push_back (std::move (s)); return (Component*) sp;
    };

    // ---- always: MASTER (next to THE ANVIL)
    {
        auto& c = card ("Master", n, -1);
        knob (c, length, "Length", "Length of the whole kick: the envelopes stretch with it");
        knob (c, clipDrive, "Clip", "Pushes the kick into the master clipper: louder, harder, flatter");
        knob (c, clipSoft, "Soft", "Clipper shape: 0 = hard (aggressive), 1 = soft (round)");
        knob (c, velocity, "Velocity", "0 = every hit full level, 1 = MIDI velocity sets the level");
        knob (c, output, "Output", "Output level", true);
        c.head.push_back (toggle (autoGain, "Auto gain", Toggle::Chip, n, "ON: turning a DRIVE up changes the character, not the volume (you judge the sound, not the loudness)"));
    }
    // ---- page 0: PUNCH
    {
        auto& c = card ("Punch", gold, 0); cPunch = (int) cards.size() - 1;
        knob (c, punchLevel, "Level", "The punch / tok on top of the tail");
        knob (c, punchStart, "Start", "Punch pitch at the hit in Hz (never follows the note)");
        knob (c, punchEnd, "End", "Punch pitch after the drop (can be higher than START: a rising tok)");
        knob (c, punchAmount, "Amount", "How much of the START -> END move happens: 0 = flat on END, 100% = the full move");
        knob (c, punchDrop, "Drop", "How fast the punch pitch moves from START to END");
        knob (c, punchAttack, "Attack", "Fade-in of the punch: 0 = instant click, a few ms = softer start");
        knob (c, punchDecay, "Decay", "How fast the punch fades (TOK: how fast the spike settles on the shelf)");
        knob (c, punchLen, "Length", "TOK: the punch holds its level this long and is cut (100 - 150 ms like tok samples). DECAY (0) = a fading punch");
        knob (c, punchPhase, "Phase", "Start phase of the punch oscillator: 0 = from zero, 90 = from the peak (harder hit)");
        c.head.push_back (toggle (punchOn, "On", Toggle::Pill, gold, "Punch on / off"));
        c.head.push_back (toggle (punchInto, "Into dist", Toggle::Chip, gold, "ON: the punch gets its own pass through the tail's STAGE 1 - it sounds like the start of the tail. OFF: its own filter + drive"));
        c.head.push_back (seg (punchType, { "Sine", "Tri", "Saw", "Noise", "Reso" }, gold, "Punch source: SINE clean tok, TRI a bit brighter, SAW hard, NOISE a noisy tok around the pitch, RESO a ringing peak that rides the pitch"));
    }
    {
        auto& c = card ("Fixed head", gold, 0); cHead = (int) cards.size() - 1;
        knob (c, headLen, "Length", "How long the start of the kick stays the same on every note (reference kicks: 100 - 130 ms)");
        knob (c, headHz, "Pitch", "The note the head is always played on - after LENGTH the tail moves to the key you play");
        c.head.push_back (toggle (headOn, "On", Toggle::Pill, gold, "ON: the start of the kick is exactly the same on every note; only the tail follows the key"));
    }
    {
        auto& c = card ("Click", gold, 0); cClick = (int) cards.size() - 1;
        knob (c, clickLevel, "Level", "Short noise click at the very start: helps the kick cut through");
        knob (c, clickHz, "Tone", "Colour of the click");
        knob (c, clickLen, "Length", "Length of the click");
    }
    {
        auto& c = card ("Punch filter", gold, 0); cPFilt = (int) cards.size() - 1;
        knob (c, punchFiltHz, "Cutoff", "Filter of the punch BEFORE its drive: shapes what the drive turns into harmonics");
        knob (c, punchFiltReso, "Reso", "Resonance of the punch filter");
        c.head.push_back (seg (punchFiltType, { "Off", "LP", "HP", "BP", "Notch" }, gold, "HP: no low end in the tok (it stays with the tail / sub). BP: a narrow 'talking' tok"));
    }
    {
        auto& c = card ("Punch drive", gold, 0); cPDrive = (int) cards.size() - 1;
        knob (c, punchDrive, "Drive", "Distorts the punch (when INTO DIST is off)");
        c.head.push_back (seg (punchDistType, { "Soft", "Tube", "Hard", "Fold", "Raw", "Crush" }, gold, "Character of the punch drive"));
    }
    {
        auto& c = card ("Tok > tail", hot, 0); cHand = (int) cards.size() - 1;
        knob (c, duck, "Duck", "The tail steps back while the punch plays: the tok gets space and the tail comes in under it");
        knob (c, tailAttack, "Tail att", "Fade-in of the tail: moves its start away from the tok");
    }
    // ---- page 1: TAIL
    {
        auto& c = card ("Tail pitch", hot, 1); cTail = (int) cards.size() - 1;
        knob (c, tailLevel, "Level", "Level of the distorted tail");
        knob (c, tailStart, "Start x", "RATIO mode: pitch at the hit = START x the played note");
        knob (c, tailStartHz, "Start Hz", "HZ mode: the tail always starts on this pitch (a long drop on low notes, a short one on high notes)");
        knob (c, tailDrop, "Drop", "How long the pitch falls down to the note");
        knob (c, tailHold, "Hold", "The pitch stays up at START this long before it drops");
        knob (c, tailCurve, "Curve", "Shape of the pitch fall: low = slow glide, high = fast snap then a long settle");
        knob (c, tune, "Pitch", "Pitch of the TAIL only, in semitones (PUNCH and CLICK keep their own pitch)", true);
        knob (c, tailFine, "Fine", "Fine pitch of the tail in cents", true);
        c.head.push_back (seg (pitchMode, { "Curve", "Drawn" }, hot, "CURVE = pitch from the knobs, DRAWN = pitch from the PITCH envelope on the screen"));
        c.head.push_back (seg (tailStartMode, { "Ratio", "Hz" }, hot, "START as x the note (RATIO) or as a fixed pitch (HZ)"));
    }
    {
        auto& c = card ("Osc A", hot, 1); cOscA = (int) cards.size() - 1;
        knob (c, tailWave, "Wave", "Oscillator A: sine > triangle > saw - the fundamental of the tail");
        knob (c, oscALevel, "Level", "Level of oscillator A");
        knob (c, tailPhase, "Phase", "Start phase of both tail oscillators (every hit starts the same)");
    }
    {
        auto& c = card ("Osc B", hot, 1); cOscB = (int) cards.size() - 1;
        knob (c, oscBLevel, "Level", "Oscillator B on the same pitch: harmonics for the distortion. A quiet saw under a loud sine = thicker without more drive");
        knob (c, oscBWidth, "Width", "PULSE only: pulse width (50% = square)");
        c.head.push_back (seg (oscBType, { "Saw", "Pulse", "Tri" }, hot, "Wave of oscillator B"));
    }
    {
        auto& c = card ("Pre filter", hot, 1); cPre = (int) cards.size() - 1;
        knob (c, preHz, "Cutoff", "Filter BEFORE STAGE 1: decides which frequencies the distortion bites into");
        knob (c, preReso, "Reso", "Resonance: a peak that the distortion turns into a 'voice'");
        knob (c, preEnv, "Env", "Sweep: the cutoff starts this far up (+) or down (-) at the hit and moves back over DECAY", true);
        knob (c, preDecay, "Decay", "How long the sweep takes");
        c.head.push_back (seg (preType, { "Off", "LP", "HP", "BP", "Notch" }, hot, "Type of the pre filter"));
    }
    {
        auto& c = card ("Stage 1", hot, 1); cS1 = (int) cards.size() - 1;
        knob (c, ParamIDs::drive, "Drive", "How hard the tail hits the first distortion (the DRIVE envelope moves it over time)");
        knob (c, tone, "Tone", "Before the distortion: left = more low end, right = more mids", true);
        knob (c, bias1, "Bias", "Asymmetry: shifts the curve so the top and bottom clip differently (even harmonics, raw crunch)", true);
        knob (c, mix1, "Mix", "Clean / distorted: below 100% some of the clean tail stays (parallel distortion)");
        c.head.push_back (seg (distType, { "Soft", "Tube", "Hard", "Fold", "Raw", "Crush" }, hot, "SOFT round, TUBE warm, HARD square, FOLD metallic, RAW buzzing, CRUSH digital steps"));
    }
    {
        auto& c = card ("Mid EQ", hot, 1); cMid = (int) cards.size() - 1;
        knob (c, midHz, "Freq", "A bell between the two distortions: shapes what STAGE 2 bites into");
        knob (c, midGain, "Gain", "Boost = that range gets distorted harder (more honk), cut = smoother tail", true);
        knob (c, midQ, "Q", "Width of the bell");
    }
    {
        auto& c = card ("Stage 2", hot, 1); cS2 = (int) cards.size() - 1;
        knob (c, drive2, "Drive", "How hard STAGE 2 is pushed");
        knob (c, bias2, "Bias", "Asymmetry of STAGE 2", true);
        knob (c, mix2, "Mix", "Clean / distorted for STAGE 2");
        c.head.push_back (seg (dist2Type, { "Off", "Soft", "Tube", "Hard", "Fold", "Raw", "Crush" }, hot, "Second distortion after the MID EQ. OFF = one stage only"));
    }
    // ---- page 2: TEXTURE
    {
        auto& c = card ("Texture layer", cool, 2); cTex = (int) cards.size() - 1;
        knob (c, texLevel, "Level", "Its own layer of grit on top (OFF = none). Never touches the low end");
        knob (c, texHz, "Freq", "Where the texture sits (band / resonant filter)");
        knob (c, texReso, "Reso", "Narrow and ringing (high) or wide and noisy (low)");
        knob (c, texDecay, "Decay", "How long the texture lasts");
        knob (c, texDrive, "Drive", "Distortion of the texture only");
        knob (c, texSemis, "Pitch", "PULSE / SAW: pitch against the tail (they follow its drop)", true);
        knob (c, texHp, "Low cut", "Everything below this is removed from the texture");
        c.head.push_back (seg (texSource, { "White", "Pink", "Pulse", "Saw" }, cool, "Source: WHITE / PINK noise, or a PULSE / SAW riding the tail pitch"));
    }
    {
        auto& c = card ("Character", cool, 2); cChar = (int) cards.size() - 1;
        knob (c, scream, "Scream", "A resonant peak pushed into saturation: the 'yell' of raw kicks");
        knob (c, screamHz, "Scream Hz", "Where the scream peak sits");
        knob (c, crunch, "Crunch", "Extra high distortion at the start of the tail (the 'crack')");
        knob (c, crunchDec, "Crunch dec", "How long the crunch lasts");
        knob (c, grit, "Grit", "Noisy texture riding on the tail: the dirty 'sand'");
    }
    {
        auto& c = card ("Impact", cool, 2); cImpact = (int) cards.size() - 1;
        knob (c, chaos, "Chaos", "Noise burst pushed INTO the distortion at the start: the dense, messy impact of modern kicks");
        knob (c, chaosDec, "Decay", "How long the chaos lasts");
        knob (c, chaosHz, "Tone", "Colour of the chaos: low = dark rumble, high = bright crackle");
    }
    {
        auto& c = card ("Filter + sub", cool, 2); cFilter = (int) cards.size() - 1;
        knob (c, lowCut, "Low cut", "Removes rumble under the tail");
        knob (c, highCut, "High cut", "Darkens the distortion: low = round and warm, high = open and fizzy");
        knob (c, body, "Body", "More or less low end (the fundamental of the tail)", true);
        knob (c, cleanSub, "Clean sub", "Below this frequency the distortion is replaced by a clean sine: a tight, mono-safe sub (OFF = all distorted)");
        knob (c, subLevel, "Sub level", "CLEAN SUB on: level of the clean sub (it skips the BUS distortion)");
    }
    // ---- page 3: BUS / FX
    {
        auto& c = card ("Bus distortion", hot, 3); cBus = (int) cards.size() - 1;
        knob (c, busDrive, "Drive", "Distortion on the sum of punch + tail + texture + click: glues them into one kick. The clean sub goes around it");
        knob (c, busMix, "Mix", "Clean / distorted sum");
        c.head.push_back (seg (busType, { "Off", "Soft", "Tube", "Hard", "Fold", "Raw", "Crush" }, hot, "OFF = the layers stay separate"));
    }
    {
        auto& c = card ("OTT", cool, 3); cOtt = (int) cards.size() - 1;
        knob (c, ott, "OTT", "3-band upward + downward compression on the whole kick: dense, loud, in your face");
    }
    {
        auto& c = card ("Room", cool, 3); cRoom = (int) cards.size() - 1;
        knob (c, room, "Room", "Small room. INTO DIST on: the reflections are distorted with the tail. Off: space around the kick");
        knob (c, roomSize, "Size", "Room size: small = thick, big = long tail");
        knob (c, roomLowCut, "Low cut", "The room has no low end below this (keeps the bass clean)");
        c.head.push_back (toggle (roomInto, "Into dist", Toggle::Chip, cool, "ON: the room goes INTO the distortion. OFF: the room comes after (space)"));
    }
    {
        auto& c = card ("Sample", n, 3); cSample = (int) cards.size() - 1;
        knob (c, smpLevel, "Level", "Level of your own sample (LOAD or drop a WAV on the plugin)");
        knob (c, smpPitch, "Pitch", "Pitch of the sample in semitones (it does not follow the note)", true);
        knob (c, smpStart, "Start", "Skips the start of the sample");
        loadBtn.setTooltip ("Load your own punch / kick / layer (WAV, AIFF, FLAC). You can also drag a file onto the plugin");
        clearBtn.setTooltip ("Remove the sample");
        loadBtn.onClick = [this]
        {
            chooser = std::make_unique<FileChooser> ("Load a sample", File::getSpecialLocation (File::userDocumentsDirectory), "*.wav;*.aif;*.aiff;*.flac");
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                  [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) proc.loadSample (fc.getResult()); });
        };
        clearBtn.onClick = [this] { proc.clearSample(); };
        addAndMakeVisible (loadBtn); addAndMakeVisible (clearBtn);
        c.head.push_back (&clearBtn); c.head.push_back (&loadBtn);
    }
    addAndMakeVisible (anvil);

    tabs = std::make_unique<TabStrip> (StringArray { "Punch", "Tail", "Texture", "Bus / FX" }, hot);
    tabs->onSelect = [this] (int i) { showPage (i); };
    tabs->isOn = [this] (int i) { return i == 0 ? raw (proc, ParamIDs::punchOn) > 0.5f : i == 2 ? raw (proc, ParamIDs::texLevel) > -59.f : i == 3 ? raw (proc, ParamIDs::busType) > 0.5f : true; };
    addAndMakeVisible (*tabs);
    addAndMakeVisible (layers);

    // top bar: presets like 5CR34M 4LPH4 + Folder (opens your presets) + Play (hold)
    refreshList();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < userBase) proc.setCurrentProgram (id - 1);
        else if (isPositiveAndBelow (id - userBase, userFiles.size())) proc.loadUserPresetFile (userFiles[id - userBase]);
        lastName = {};
    };
    randomB.onClick = [this] { proc.randomizeSound(); lastName = {}; };
    saveB.onClick = [this] { showSave(); };
    folderB.onClick = [this] { auto f = proc.userPresetFolder(); f.createDirectory(); f.startAsProcess(); };
    playB.onPress = [this] (bool d) { proc.audition (d); };
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    undoB.onClick = [this] { proc.undo.undo(); };
    redoB.onClick = [this] { proc.undo.redo(); };
    randomB.setTooltip ("A new kick in musical ranges");
    saveB.setTooltip ("Save the current kick to Documents / 4GE / Presets");
    folderB.setTooltip ("Open the folder with your own presets");
    playB.setTooltip ("Hold to play the kick");
    FlatLookAndFeel::makePrimary (saveB);
    for (auto* c : { (Component*) &presetBox, (Component*) &prev, (Component*) &next, (Component*) &randomB, (Component*) &saveB, (Component*) &folderB,
                     (Component*) &playB, (Component*) &undoB, (Component*) &redoB })
        addAndMakeVisible (c);

    setSize (W, H);
    showPage (0);
    timerCallback();
    startTimerHz (30);
}

Canvas::~Canvas()
{
    if (dialog != nullptr) dialog->exitModalState (0);
    dialog.reset();
    setLookAndFeel (nullptr);
}

void Canvas::refreshList()
{
    presetBox.clear (dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    userFiles = proc.userPresetFiles();
    if (! userFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER  (Folder = open them)");
        for (int i = 0; i < userFiles.size(); ++i) presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), userBase + i);
    }
    lastName = {};
}

void Canvas::step (int d)
{
    Array<int> ids;
    for (int i = 0; i < presetBox.getNumItems(); ++i) ids.add (presetBox.getItemId (i));
    if (ids.isEmpty()) return;
    int at = ids.indexOf (presetBox.getSelectedId());
    at = at < 0 ? 0 : (at + d + ids.size()) % ids.size();
    presetBox.setSelectedId (ids[at], sendNotificationSync);
}

void Canvas::showSave()
{
    if (dialog != nullptr) return;
    dialog = std::make_unique<AlertWindow> ("Save preset", "Name (Documents / 4GE / Presets):", MessageBoxIconType::NoIcon, this);
    dialog->setLookAndFeel (&lnf);
    dialog->addTextEditor ("name", proc.currentPresetName().startsWith ("Random") ? String ("My Kick") : proc.currentPresetName());
    dialog->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    SafePointer<Canvas> safe (this);
    dialog->enterModalState (true, ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->dialog == nullptr) return;
        const auto name = safe->dialog->getTextEditorContents ("name");
        MessageManager::callAsync ([safe] { if (safe != nullptr) safe->dialog.reset(); });
        if (result == 1)
        {
            if (safe->proc.saveUserPresetAs (name)) safe->refreshList();
            else AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Save preset", "Could not save the preset.");
        }
    }), false);
}

void Canvas::showPage (int pg)
{
    page = jlimit (0, 3, pg);
    if (tabs != nullptr && tabs->getSelected() != page) tabs->setSelected (page);
    for (auto& c : cards)
    {
        const bool on = c.page < 0 || c.page == page;
        for (auto* k : c.knobs) k->setVisible (on);
        if (c.side != nullptr) c.side->setVisible (on);
        for (auto* h : c.head) h->setVisible (on);
    }
    resized();
    repaint();
}

void Canvas::layoutCard (Card& c, int kw)
{
    const int kh = doomsday::flat::Knob::heightFor (44);
    auto a = c.area.reduced (16, 10);
    auto head = a.removeFromTop (28);
    for (auto* h : c.head)
    {
        const int w = dynamic_cast<Segmented*> (h) != nullptr ? jmin (segW[h], head.getWidth() - 70)
                    : dynamic_cast<TextButton*> (h) != nullptr ? (h == &clearBtn ? 28 : 64) : 92;
        h->setBounds (head.removeFromRight (w).withSizeKeepingCentre (w, 24));
        head.removeFromRight (6);
    }
    a.removeFromTop (4);
    const int cols = jmax (1, (int) c.knobs.size());
    const int perRow = jmax (1, jmin (cols, a.getWidth() / kw));
    for (int i = 0; i < cols; ++i)
    {
        const int row = i / perRow, col = i % perRow;
        c.knobs[(size_t) i]->setBounds (a.getX() + col * kw, a.getY() + row * (kh + 4), kw, kh);
    }
}

void Canvas::resized()
{
    const int m = 20, gap = 12, kw = 74;
    // top bar
    const int bx = W - m;
    redoB.setBounds (bx - 62, 14, 62, 30);
    undoB.setBounds (bx - 130, 14, 62, 30);
    playB.setBounds (bx - 206, 14, 70, 30);
    folderB.setBounds (bx - 284, 14, 72, 30);
    saveB.setBounds (bx - 362, 14, 72, 30);
    randomB.setBounds (bx - 454, 14, 86, 30);
    next.setBounds (bx - 498, 14, 34, 30);
    presetBox.setBounds (bx - 504 - 270, 14, 270, 30);
    prev.setBounds (presetBox.getX() - 40, 14, 34, 30);
    // THE ANVIL + MASTER
    auto& master = cards[0];
    master.area = { W - m - 430, 68, 430, 200 };
    anvil.setBounds (m, 68, master.area.getX() - gap - m, 200);
    tabs->setBounds (m, 280, 420, 30);
    layers.setBounds (m + 440, 278, W - 2 * m - 440, 34);
    // pages
    const int y1 = 318, rowH = 134, y2 = y1 + rowH + gap;
    auto place = [&] (std::initializer_list<std::pair<int, int>> list, int y)
    {
        int x = m;
        for (auto [idx, w] : list) { cards[(size_t) idx].area = { x, y, w == 0 ? W - m - x : w, rowH }; x += cards[(size_t) idx].area.getWidth() + gap; }
    };
    for (auto& c : cards) if (c.page >= 0) c.area = {};
    auto kws = [kw] (int k) { return k * kw + 32; };
    if (page == 0)
    {
        place ({ { cPunch, kws (9) }, { cHead, 270 }, { cClick, 0 } }, y1);
        place ({ { cPFilt, 420 }, { cPDrive, 420 }, { cHand, 0 } }, y2);
    }
    else if (page == 1)
    {
        place ({ { cTail, kws (8) }, { cOscA, kws (3) }, { cOscB, 0 } }, y1);
        place ({ { cPre, 340 }, { cS1, 370 }, { cMid, kws (3) }, { cS2, 0 } }, y2);
    }
    else if (page == 2)
    {
        place ({ { cTex, kws (7) }, { cChar, kws (5) }, { cImpact, 0 } }, y1);
        place ({ { cFilter, 0 } }, y2);
    }
    else
    {
        place ({ { cBus, 440 }, { cOtt, 170 }, { cRoom, 0 } }, y1);
        place ({ { cSample, 0 } }, y2);
    }
    for (auto& c : cards) if (! c.area.isEmpty()) layoutCard (c, c.page < 0 ? 96 : kw);
    // MASTER: 5 knobs in one row + the meter
    {
        auto a = master.area.reduced (16, 10);
        a.removeFromTop (32);
        meterArea = a.removeFromRight (14).reduced (0, 4);
        a.removeFromRight (8);
        const int w = a.getWidth() / 5;
        for (int i = 0; i < 5; ++i) master.knobs[(size_t) i]->setBounds (a.getX() + i * w, a.getY() + 20, w, doomsday::flat::Knob::heightFor (44));
    }
    if (! cards[(size_t) cSample].area.isEmpty()) sampleNameArea = cards[(size_t) cSample].area.reduced (16, 10).removeFromTop (28).withTrimmedLeft (110).withTrimmedRight (140);
    else sampleNameArea = {};
}

void Canvas::paint (Graphics& g)
{
    const auto& t = tokens();
    g.fillAll (t.ground);
    g.setColour (t.divider); g.fillRect (0, 56, W, 1);
    doomsday::brand::drawBadge (g, doomsday::brand::headerBadgeArea (1.f, 28.f), 0.6f, RectanglePlacement::xLeft | RectanglePlacement::yMid);
    const float tx = doomsday::brand::headerTextX (1.f);
    g.setFont (lnf.font (19.f, Bold, 0.06f)); g.setColour (t.text);
    g.drawText ("4GE", Rectangle<float> (tx, 0.f, 240.f, 56.f), Justification::centredLeft);
    g.setFont (lnf.font (12.f, Medium)); g.setColour (t.textMute);
    g.drawText ("v" + String (ParamIDs::versionText) + dotSep + "kick synth", Rectangle<float> (tx + 58.f, 0.f, 240.f, 56.f), Justification::centredLeft);
    for (auto& c : cards) if (! c.area.isEmpty()) doomsday::flat::card (g, lnf, c.area, c.title, c.dot);
}

void Canvas::paintOverChildren (Graphics& g)
{
    const auto& t = tokens();
    // output meter (MASTER)
    if (! meterArea.isEmpty())
    {
        auto r = meterArea.toFloat();
        g.setColour (t.control); g.fillRoundedRectangle (r, 4.f);
        const float lv = jlimit (0.f, 1.f, (Decibels::gainToDecibels (meterLevel, -48.f) + 48.f) / 50.f);
        auto fill = r.withTop (r.getBottom() - r.getHeight() * lv);
        g.setColour (meterLevel > 0.99f ? t.hot : t.cool); g.fillRoundedRectangle (fill, 4.f);
    }
    if (! sampleNameArea.isEmpty())
    {
        const String name = proc.sampleName();
        g.setFont (lnf.font (11.5f, Medium));
        g.setColour (name.isEmpty() ? t.textMute : t.hot);
        g.drawText (name.isEmpty() ? "No sample - Load or drop a WAV on the plugin" : name, sampleNameArea, Justification::centredLeft, true);
    }
    if (dragHover)
    {
        g.setColour (t.hot.withAlpha (0.10f)); g.fillRect (getLocalBounds());
        g.setColour (t.text); g.setFont (lnf.font (22.f, Bold));
        g.drawText ("Drop to load the sample", getLocalBounds(), Justification::centred);
    }
}

bool Canvas::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (File (f).hasFileExtension ("wav;aif;aiff;flac")) return true;
    return false;
}
void Canvas::filesDropped (const StringArray& files, int, int)
{
    dragHover = false;
    for (auto& f : files) if (File (f).hasFileExtension ("wav;aif;aiff;flac")) { proc.loadSample (File (f)); break; }
    repaint();
}

void Canvas::timerCallback()
{
    anvil.tick();
    layers.repaint();
    tabs->refresh();
    if (proc.currentPresetName() != lastName)
    {
        lastName = proc.currentPresetName();
        int found = 0;
        for (int i = 0; i < presetBox.getNumItems(); ++i) if (presetBox.getItemText (i) == lastName) { found = presetBox.getItemId (i); break; }
        if (found != 0) presetBox.setSelectedId (found, dontSendNotification);
        else presetBox.setText (lastName, dontSendNotification);
    }
    const float pk = proc.getEngine().outPeak.exchange (0.f);
    meterLevel = jmax (pk, meterLevel * 0.85f);
    repaint (meterArea.expanded (2));
    if (proc.getSampleSerial() != lastSampleSerial) { lastSampleSerial = proc.getSampleSerial(); repaint (sampleNameArea.expanded (4)); }
}

// ======================================================================= editor
GeEditor::GeEditor (GeProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (Canvas::W * 6 / 10, Canvas::H * 6 / 10, Canvas::W * 16 / 10, Canvas::H * 16 / 10);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (Canvas::W, Canvas::H);
}

void GeEditor::resized()
{
    canvas.setBounds (0, 0, Canvas::W, Canvas::H);
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) Canvas::W));
}
