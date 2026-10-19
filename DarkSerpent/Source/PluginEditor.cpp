#include "PluginEditor.h"

using namespace juce;
using namespace doomsday::ui;

doomsday::ui::Theme SerpentTheme::make()
{
    Theme t;
    t.bg = Colour (0xff030403);
    t.panel = Colour (0xff060806);
    t.label = Colour (0xffa3aa9c);
    t.knobLight = Colour (0xff2f352b);
    t.knobDark = Colour (0xff050605);
    t.accents = { { lime, limeDeep, limeHot } };
    t.crown = { limeDeep.brighter (0.3f), lime, limeHot, Colours::white };
    return t;
}

// ======================================================================= SmokeLogo
namespace
{
    double gauss (Random& r)        // Box-Muller
    {
        const double u = jmax (1e-9, r.nextDouble()), v = r.nextDouble();
        return std::sqrt (-2.0 * std::log (u)) * std::cos (MathConstants<double>::twoPi * v);
    }
    struct Field
    {
        int w = 0, h = 0;
        std::vector<float> v;
        Field (int W, int H) : w (W), h (H), v ((size_t) (W * H), 0.f) {}
        float& at (int x, int y) { return v[(size_t) (y * w + x)]; }
        void splat (float px, float py, float r, float a)
        {
            r = jmax (r, 0.8f);
            const float s2 = 2.f * (r * 0.6f) * (r * 0.6f);
            const int R = (int) (r * 2.2f) + 1;
            for (int y = jmax (0, (int) py - R); y < jmin (h, (int) py + R); ++y)
                for (int x = jmax (0, (int) px - R); x < jmin (w, (int) px + R); ++x)
                {
                    const float dx = (float) x - px, dy = (float) y - py;
                    at (x, y) += a * std::exp (-(dx * dx + dy * dy) / s2);
                }
        }
        // ~gaussian blur: three box passes per axis
        Field blurred (float sigma) const
        {
            Field o = *this;
            const int r = jmax (1, (int) std::round (sigma));
            std::vector<float> tmp ((size_t) jmax (w, h));
            for (int pass = 0; pass < 3; ++pass)
            {
                for (int y = 0; y < h; ++y)
                {
                    float acc = 0.f;
                    for (int x = -r; x <= r; ++x) acc += o.v[(size_t) (y * w + jlimit (0, w - 1, x))];
                    for (int x = 0; x < w; ++x)
                    {
                        tmp[(size_t) x] = acc / (float) (2 * r + 1);
                        acc += o.v[(size_t) (y * w + jmin (w - 1, x + r + 1))] - o.v[(size_t) (y * w + jmax (0, x - r))];
                    }
                    std::copy (tmp.begin(), tmp.begin() + w, o.v.begin() + y * w);
                }
                for (int x = 0; x < w; ++x)
                {
                    float acc = 0.f;
                    for (int y = -r; y <= r; ++y) acc += o.v[(size_t) (jlimit (0, h - 1, y) * w + x)];
                    for (int y = 0; y < h; ++y)
                    {
                        tmp[(size_t) y] = acc / (float) (2 * r + 1);
                        acc += o.v[(size_t) (jmin (h - 1, y + r + 1) * w + x)] - o.v[(size_t) (jmax (0, y - r) * w + x)];
                    }
                    for (int y = 0; y < h; ++y) o.v[(size_t) (y * w + x)] = tmp[(size_t) y];
                }
            }
            return o;
        }
    };
}

void SmokeLogo::resized()
{
    if (getWidth() > 0 && getHeight() > 0) build();
}

void SmokeLogo::build()
{
    auto& lnf = lnfOf (*this);
    const int w = getWidth(), h = getHeight();
    const float cy = (float) h * 0.5f;
    const String text ("DARK SERPENT");

    // ---- title mask + rendered title (glow + letters)
    Font font = lnf.titleFont (60.f, 0.12f);
    {
        GlyphArrangement ga; ga.addLineOfText (font, text, 0.f, 0.f);
        const float tw = ga.getBoundingBox (0, -1, true).getWidth();
        font = lnf.titleFont (60.f * (float) w * 0.70f / jmax (1.f, tw), 0.12f);
    }
    Image mask (Image::SingleChannel, w, h, true);
    { Graphics g (mask); g.setFont (font); g.setColour (Colours::white); g.drawText (text, 0, 0, w, h, Justification::centred); }
    Field T (w, h);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) T.at (x, y) = mask.getPixelAt (x, y).getFloatAlpha();
    {
        auto tg1 = T.blurred (6.f), tg2 = T.blurred (16.f);
        title = Image (Image::ARGB, w, h, true);
        Image::BitmapData bd (title, Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const float glowA = jlimit (0.f, 1.f, tg1.at (x, y) * 0.55f + tg2.at (x, y) * 0.45f) * 0.6f;
                const float t = T.at (x, y);
                const Colour gc = SerpentTheme::lime.withAlpha (glowA);
                const Colour tc = Colour (0xfff2ffe0).withAlpha (t);
                bd.setPixelColour (x, y, gc.interpolatedWith (tc, t).withAlpha (jmax (glowA, t)));
            }
    }

    // ---- serpent path: tail on the left, head on the right, weaving over / under the letters
    const int N = 900;
    std::vector<float> px (N), py (N), rad (N);
    std::vector<bool> isFront (N);
    const float amp = (float) h * 0.27f;
    for (int i = 0; i < N; ++i)
    {
        const float s = (float) i / (float) (N - 1);
        const float ph = MathConstants<float>::twoPi * 1.55f * s + 0.6f;
        px[(size_t) i] = (float) w * 0.05f + (float) w * 0.86f * s;
        py[(size_t) i] = cy + amp * std::sin (ph) * (0.55f + 0.45f * s) + (float) h * 0.03f * std::sin (MathConstants<float>::twoPi * 6.f * s);
        rad[(size_t) i] = 2.f + (float) h * 0.085f * std::pow (s, 0.6f);
        isFront[(size_t) i] = std::cos (ph) > 0.f;
    }
    const float dx = px[N - 1] - px[N - 30], dy = py[N - 1] - py[N - 30];
    const float dl = std::sqrt (dx * dx + dy * dy);
    const Point<float> dir (dx / dl, dy / dl), perp (-dir.y, dir.x);
    const float headLen = (float) h * 0.20f;
    eyes[0] = { px[N - 1] + dir.x * headLen * 0.58f + perp.x * h * 0.045f, py[N - 1] + dir.y * headLen * 0.58f + perp.y * h * 0.045f };
    eyes[1] = { px[N - 1] + dir.x * headLen * 0.58f - perp.x * h * 0.045f, py[N - 1] + dir.y * headLen * 0.58f - perp.y * h * 0.045f };

    for (int variant = 0; variant < 2; ++variant)
    {
        Random rng (variant == 0 ? 7 : 1234);
        Field B (w, h), F (w, h);
        // flow field for the curling wisps
        const int gw = w / 8 + 2, gh = h / 8 + 2;
        Field U (gw, gh), V (gw, gh);
        for (auto& x : U.v) x = rng.nextFloat() * 2.f - 1.f;
        for (auto& x : V.v) x = rng.nextFloat() * 2.f - 1.f;
        U = U.blurred (2.f); V = V.blurred (2.f);
        float um = 1e-6f, vm = 1e-6f;
        for (auto x : U.v) um = jmax (um, std::abs (x));
        for (auto x : V.v) vm = jmax (vm, std::abs (x));

        for (int i = 0; i < N; i += 2)
        {
            Field& tgt = isFront[(size_t) i] ? F : B;
            const float r = rad[(size_t) i];
            for (int k = 0; k < 2; ++k)
                tgt.splat (px[(size_t) i] + (float) gauss (rng) * r * 0.3f, py[(size_t) i] + (float) gauss (rng) * r * 0.3f,
                           r * (0.7f + 0.4f * rng.nextFloat()), 0.06f);
            if (rng.nextFloat() < 0.6f)
            {
                float x = px[(size_t) i] + (float) gauss (rng) * r * 0.4f, y = py[(size_t) i] + (float) gauss (rng) * r * 0.4f;
                const int steps = 12 + rng.nextInt (34);
                const float sp = 1.2f + rng.nextFloat() * 1.6f;
                for (int t = 0; t < steps; ++t)
                {
                    const int gx = jlimit (0, gw - 1, (int) (x / 8.f)), gy = jlimit (0, gh - 1, (int) (y / 8.f));
                    x += U.at (gx, gy) / um * sp * 2.2f;
                    y += (V.at (gx, gy) / vm - 0.35f) * sp * 2.2f;
                    const float a = 1.f - (float) t / (float) steps;
                    tgt.splat (x, y, 1.f + r * 0.3f * a, 0.045f * a);
                }
            }
        }
        // head: a wedge a bit wider than the neck
        {
            Field& tgt = isFront[N - 1] ? F : B;
            for (int k = 0; k < 50; ++k)
            {
                const float t = (float) k / 49.f;
                const float wd = (float) h * 0.13f * (1.f + 0.45f * std::sin (MathConstants<float>::pi * jmin (1.f, t * 1.4f))) * (1.f - 0.8f * std::pow (t, 1.6f));
                for (int j = 0; j < 3; ++j)
                {
                    const float o = (float) gauss (rng) * 0.3f;
                    tgt.splat (px[N - 1] + dir.x * headLen * t + perp.x * wd * o, py[N - 1] + dir.y * headLen * t + perp.y * wd * o, wd * 0.5f, 0.09f);
                }
            }
        }

        // smoke shading: noisy multi-scale blur -> black density + lime rim light
        Field noise (w, h);
        for (auto& x : noise.v) x = rng.nextFloat();
        noise = noise.blurred (6.f);
        float nmin = 1e9f, nmax = -1e9f;
        for (auto x : noise.v) { nmin = jmin (nmin, x); nmax = jmax (nmax, x); }
        auto shade = [&] (const Field& D)
        {
            auto b1 = D.blurred (1.5f), b2 = D.blurred (5.f), b3 = D.blurred (14.f);
            Field S (w, h);
            for (size_t k = 0; k < S.v.size(); ++k)
            {
                const float nz = (noise.v[k] - nmin) / jmax (1e-6f, nmax - nmin);
                S.v[k] = (b1.v[k] * 0.6f + b2.v[k] * 0.8f + b3.v[k] * 0.6f) * (0.35f + 1.3f * std::pow (nz, 1.5f));
            }
            return S;
        };
        auto SB = shade (B), SF = shade (F);
        std::vector<float> all (SB.v); all.insert (all.end(), SF.v.begin(), SF.v.end());
        std::nth_element (all.begin(), all.begin() + (long) (all.size() * 0.995), all.end());
        const float ref = jmax (1e-6f, all[(size_t) (all.size() * 0.995)]);

        auto toLayer = [&] (const Field& S, bool frontLayer)
        {
            Field dens (w, h);
            for (size_t k = 0; k < dens.v.size(); ++k)
            {
                float d = S.v[k] / ref;
                if (frontLayer) d *= 1.f - 0.3f * T.v[k];
                dens.v[k] = jlimit (0.f, 1.f, 1.f - std::exp (-d * 9.f));
            }
            auto spill = dens.blurred (6.f);
            Layer L { Image (Image::ARGB, w, h, true), Image (Image::ARGB, w, h, true) };
            Image::BitmapData dd (L.dark, Image::BitmapData::writeOnly), gd (L.glow, Image::BitmapData::writeOnly);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const size_t k = (size_t) (y * w + x);
                    const float de = dens.v[k];
                    const float outer = jmax (0.f, spill.v[k] - de);
                    const float rim = 1.f - std::exp (-(outer * 6.f + de * (1.f - de) * 4.f * 0.08f));
                    dd.setPixelColour (x, y, Colour (0xff020302).withAlpha (de * 0.96f));
                    gd.setPixelColour (x, y, SerpentTheme::lime.withAlpha (jlimit (0.f, 1.f, rim * 0.95f)));
                }
            return L;
        };
        back[(size_t) variant] = toLayer (SB, false);
        front[(size_t) variant] = toLayer (SF, true);
    }
}

void SmokeLogo::tick (float level)
{
    phase += 0.011f;
    const float target = jlimit (0.f, 1.f, level * 1.5f);
    glow = target > glow ? target : glow * 0.92f;
    repaint();
}

void SmokeLogo::paint (Graphics& g)
{
    if (! title.isValid()) return;
    // the two smoke variants breathe into each other; the lime light swells with the sound
    const float mix = 0.5f + 0.5f * std::sin (phase);
    const float light = 0.75f + 0.35f * glow;
    auto drawLayer = [&] (std::array<Layer, 2>& L)
    {
        g.setOpacity (1.f - mix); g.drawImageAt (L[0].dark, 0, 0);
        g.setOpacity (mix);       g.drawImageAt (L[1].dark, 0, 0);
        for (int rep = 0; rep < 2; ++rep)     // drawn twice: a stronger, saturated rim
        {
            const float k = rep == 0 ? 1.f : 0.45f;
            g.setOpacity (jmin (1.f, (1.f - mix) * light * k)); g.drawImageAt (L[0].glow, 0, 0);
            g.setOpacity (jmin (1.f, mix * light * k));         g.drawImageAt (L[1].glow, 0, 0);
        }
    };
    drawLayer (back);
    g.setOpacity (1.f);
    g.drawImageAt (title, 0, 0);
    drawLayer (front);
    g.setOpacity (1.f);
    // eyes
    for (auto& e : eyes)
    {
        const float r = 4.f + 1.5f * glow;
        ColourGradient halo (SerpentTheme::lime.withAlpha (0.55f + 0.4f * glow), e.x, e.y, SerpentTheme::lime.withAlpha (0.f), e.x + r * 5.f, e.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (Rectangle<float> (r * 10.f, r * 10.f).withCentre (e));
        g.setColour (SerpentTheme::limeHot);
        g.fillEllipse (Rectangle<float> (r * 2.f, r * 1.4f).withCentre (e));
    }
}

// ======================================================================= TalkPad
TalkPad::TalkPad (SerpentProcessor& p) : proc (p)
{
    vowelP = p.apvts.getParameter (ParamIDs::vowel);
    talkP = p.apvts.getParameter (ParamIDs::talk);
    setMouseCursor (MouseCursor::CrosshairCursor);
    setRepaintsOnMouseActivity (false);
}

Rectangle<float> TalkPad::padArea() const { return getLocalBounds().toFloat().reduced (14.f, 10.f).withTrimmedBottom (22.f); }

void TalkPad::setFrom (Point<float> pt)
{
    auto a = padArea();
    const float v = jlimit (0.f, 1.f, (pt.x - a.getX()) / a.getWidth());
    const float t = jlimit (0.f, 1.f, 1.f - (pt.y - a.getY()) / a.getHeight());
    vowelP->setValueNotifyingHost (vowelP->convertTo0to1 (v));
    talkP->setValueNotifyingHost (talkP->convertTo0to1 (t));
}
void TalkPad::mouseDown (const MouseEvent& e) { vowelP->beginChangeGesture(); talkP->beginChangeGesture(); setFrom (e.position); }
void TalkPad::mouseDrag (const MouseEvent& e) { setFrom (e.position); }
void TalkPad::mouseUp (const MouseEvent&)     { vowelP->endChangeGesture(); talkP->endChangeGesture(); }

void TalkPad::tick()
{
    const float v = vowelP->convertFrom0to1 (vowelP->getValue()), t = talkP->convertFrom0to1 (talkP->getValue());
    const float lv = proc.getEngine().liveVowel.load();
    if (std::abs (v - lastV) > 1e-4f || std::abs (t - lastT) > 1e-4f || std::abs (lv - lastLiveV) > 2e-3f)
    {
        lastV = v; lastT = t; lastLiveV = lv;
        repaint();
    }
}

void TalkPad::paint (Graphics& g)
{
    auto& lnf = lnfOf (*this);
    const auto lime = SerpentTheme::lime;
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xcc040504));
    g.fillRoundedRectangle (r, 8.f);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);

    auto a = padArea();
    g.setColour (Colours::white.withAlpha (0.045f));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * (float) i / 4.f), a.getX(), a.getRight());
    for (int i = 0; i < 5; ++i)
    {
        const float x = a.getX() + a.getWidth() * (float) i / 4.f;
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        const bool near = std::abs (lastV * 4.f - (float) i) < 0.5f;
        g.setFont (lnf.titleFont (13.f, 0.1f));
        g.setColour (near ? lime : lnf.theme.label.withAlpha (0.7f));
        g.drawText (String::charToString (Choices::vowels[i]), Rectangle<float> (x - 15.f, a.getBottom() + 4.f, 30.f, 18.f), Justification::centred);
    }

    // formant response of the current (live) vowel, drawn as a glowing curve across the pad
    {
        const auto vo = SerpentEngine::vowelAt (lastLiveV >= 0.f ? lastLiveV : lastV);
        Path curve;
        for (int i = 0; i <= 160; ++i)
        {
            const float t = (float) i / 160.f;
            const float f = 150.f * std::pow (6000.f / 150.f, t);
            float mag = 0.f;
            for (int k = 0; k < 3; ++k)
            {
                const float q = 1.f / (0.16f + 0.06f * (float) k);
                const float x = (f / vo.f[k] - vo.f[k] / f) * q;
                mag += vo.g[k] / std::sqrt (1.f + x * x);
            }
            const float y = a.getBottom() - a.getHeight() * 0.9f * jlimit (0.f, 1.f, mag * 0.75f) * (0.25f + 0.75f * lastT);
            const float x = a.getX() + a.getWidth() * t;
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        Path fill (curve);
        fill.lineTo (a.getRight(), a.getBottom()); fill.lineTo (a.getX(), a.getBottom()); fill.closeSubPath();
        ColourGradient fg (lime.withAlpha (0.20f), 0.f, a.getY(), lime.withAlpha (0.02f), 0.f, a.getBottom(), false);
        g.setGradientFill (fg);
        g.fillPath (fill);
        g.setColour (lime.withAlpha (0.25f)); g.strokePath (curve, PathStrokeType (4.f));
        g.setColour (lime.withAlpha (0.8f));  g.strokePath (curve, PathStrokeType (1.3f));
    }

    // the handle (parameters) and the live position (with MOVE / sweep applied)
    const Point<float> h (a.getX() + a.getWidth() * lastV, a.getY() + a.getHeight() * (1.f - lastT));
    const Point<float> live (a.getX() + a.getWidth() * jlimit (0.f, 1.f, lastLiveV), h.y);
    g.setColour (lime.withAlpha (0.18f)); g.drawHorizontalLine ((int) h.y, a.getX(), a.getRight());
    g.setColour (lime.withAlpha (0.5f));  g.drawEllipse (Rectangle<float> (14.f, 14.f).withCentre (live), 1.2f);
    ColourGradient halo (lime.withAlpha (0.5f), h.x, h.y, lime.withAlpha (0.f), h.x + 26.f, h.y, true);
    g.setGradientFill (halo); g.fillEllipse (Rectangle<float> (52.f, 52.f).withCentre (h));
    g.setColour (Colour (0xff050605)); g.fillEllipse (Rectangle<float> (16.f, 16.f).withCentre (h));
    g.setColour (SerpentTheme::limeHot); g.drawEllipse (Rectangle<float> (16.f, 16.f).withCentre (h), 2.f);

    g.setFont (lnf.labelFont (11.f, 0.2f));
    g.setColour (lnf.theme.label);
    g.drawText ("TALK " + String ((int) std::round (lastT * 100.f)) + "%", a.reduced (8.f, 6.f), Justification::topRight);
}

// ======================================================================= Canvas
Canvas::Knob* Canvas::knob (const char* id, const String& label, bool bipolar, bool big)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, label, 0, bipolar, big);
    auto* raw = k.get(); addAndMakeVisible (*raw); owned.push_back (std::move (k)); return raw;
}
Canvas::Switch* Canvas::toggle (const char* id, const String& label, Switch::Style st)
{
    auto t = std::make_unique<Switch> (proc.apvts, id, label, st, 0);
    auto* raw = t.get(); addAndMakeVisible (*raw); owned.push_back (std::move (t)); return raw;
}
Canvas::Choice* Canvas::choice (const char* id, const StringArray& items, bool vertical)
{
    auto c = std::make_unique<Choice> (proc.apvts, id, items, 0, vertical);
    auto* raw = c.get(); addAndMakeVisible (*raw); owned.push_back (std::move (c)); return raw;
}

Canvas::Canvas (SerpentProcessor& p) : proc (p), lnf (SerpentTheme::make()), pad (p), presets (p, true)
{
    setLookAndFeel (&lnf);
    using namespace ParamIDs;
    addAndMakeVisible (logo);
    addAndMakeVisible (pad);
    addAndMakeVisible (presets);
    addAndMakeVisible (meter);
    logo.setInterceptsMouseClicks (false, false);

    coreWave = choice (ParamIDs::coreWave, Choices::coreWaves);
    core = { knob (coreVoices, "VOICES"), knob (coreDetune, "DETUNE"), knob (coreOct, "OCT LAYER"), knob (coreLevel, "LEVEL") };
    edgeOn = toggle (ParamIDs::edgeOn, "", Switch::Power);
    edgeOct = choice (ParamIDs::edgeOct, Choices::edgeOcts);
    edge = { knob (edgeVoices, "VOICES"), knob (edgeDetune, "DETUNE"), knob (edgeTone, "TONE"), knob (edgeLevel, "LEVEL") };
    bodyOn = toggle (ParamIDs::bodyOn, "", Switch::Power);
    bodyWave = choice (ParamIDs::bodyWave, Choices::bodyWaves);
    body = { knob (bodyOct, "OCTAVE", true), knob (bodyLevel, "LEVEL") };
    filter = { knob (cutoff, "CUTOFF"), knob (reso, "RESO"), knob (tone, "TONE", true), knob (fenv, "F.ENV", true), knob (fdecay, "F.DECAY"),
               knob (sweep, "SWEEP", true), knob (sweepTime, "S.TIME"), knob (talkKey, "KEY") };
    moveOn = toggle (ParamIDs::moveOn, "", Switch::Power);
    moveRetrig = toggle (ParamIDs::moveRetrig, "RETRIG", Switch::Chip);
    moveShape = choice (ParamIDs::moveShape, Choices::shapes, true);
    move = { knob (moveDiv, "RATE"), knob (moveCut, "> CUTOFF", true), knob (moveVowel, "> VOWEL", true), knob (moveAmp, "> AMP") };
    mode = choice (ParamIDs::mode, Choices::modes);
    bendFirst = toggle (ParamIDs::bendFirst, "1ST ONLY", Switch::Chip);
    play = { knob (glide, "GLIDE"), knob (bendAmt, "BEND", true), knob (bendTime, "B.TIME"), knob (vibDepth, "VIBRATO"),
             knob (vibRate, "V.RATE"), knob (vibDelay, "V.DELAY"), knob (velocity, "VELOCITY") };
    rageOn = toggle (ParamIDs::rageOn, "", Switch::Power);
    rageType = choice (ParamIDs::rageType, Choices::rages);
    rage = { knob (rageDrive, "DRIVE"), knob (rageMix, "MIX"), knob (ott, "OTT") };
    amp = { knob (attack, "ATTACK"), knob (decay, "DECAY"), knob (sustain, "SUSTAIN"), knob (release, "RELEASE") };
    dlyOn = toggle (ParamIDs::dlyOn, "DELAY", Switch::Chip);
    revOn = toggle (ParamIDs::revOn, "REVERB", Switch::Chip);
    pumpOn = toggle (ParamIDs::pumpOn, "PUMP", Switch::Chip);
    dly = { knob (dlyDiv, "TIME"), knob (dlyFb, "FEEDBACK"), knob (dlyMix, "MIX") };
    rev = { knob (revSize, "SIZE"), knob (revMix, "MIX") };
    pump = { knob (pumpDiv, "RATE"), knob (pumpDepth, "DEPTH") };
    energyKnob = knob (energy, "", false, true);
    energyKnob->slider.setTooltip ("ENERGY - more rage, OTT, brighter, wider, more vibrato and space  (double-click = 0)");
    space = { knob (width, "WIDTH"), knob (monoBass, "MONO BASS"), knob (volume, "VOLUME") };

    setSize (W, H);
    timerCallback();
    startTimerHz (30);
}

Canvas::~Canvas() { setLookAndFeel (nullptr); }

void Canvas::row (Rectangle<int> a, const std::vector<Component*>& cells)
{
    const float cw = (float) a.getWidth() / (float) cells.size();
    for (size_t i = 0; i < cells.size(); ++i)
        cells[i]->setBounds (a.getX() + (int) (cw * (float) i), a.getY(), (int) cw, a.getHeight());
}

void Canvas::resized()
{
    panels.clear();
    const int m = 20, gap = 14, sideW = 460, midW = W - 2 * m - 2 * sideW - 2 * gap;
    const int xL = m, xM = xL + sideW + gap, xR = xM + midW + gap, y0 = 214;
    auto panel = [this] (const String& t, Rectangle<int> r, bool spot = false) { panels.push_back ({ t, r, spot }); return r; };
    auto head = [] (Rectangle<int> r) { return r.reduced (14, 10).removeFromTop (26); };
    auto body_ = [] (Rectangle<int> r, int top) { auto b = r.reduced (14, 10); b.removeFromTop (top); return b; };

    logo.setBounds (W / 2 - 470, 6, 940, 200);
    presets.setBounds (W - m - 300, 46, 300, 90);

    // left: CORE / EDGE / BODY
    auto coreA = panel ("CORE", { xL, y0, sideW, 176 });
    coreWave->setBounds (head (coreA).removeFromRight (290));
    row (body_ (coreA, 32), core);
    auto edgeA = panel ("EDGE", { xL, coreA.getBottom() + gap, sideW, 150 });
    { auto h = head (edgeA); edgeOn->setBounds (h.removeFromRight (56)); h.removeFromRight (10); edgeOct->setBounds (h.removeFromRight (170)); }
    row (body_ (edgeA, 32), edge);
    auto bodyA = panel ("BODY", { xL, edgeA.getBottom() + gap, sideW, 130 });
    { auto h = head (bodyA); bodyOn->setBounds (h.removeFromRight (56)); h.removeFromRight (10); bodyWave->setBounds (h.removeFromRight (220)); }
    row (body_ (bodyA, 30).removeFromLeft (200), body);

    // centre: TALK pad + FILTER
    auto talkA = panel ("TALK", { xM, y0, midW, 330 }, true);
    pad.setBounds (body_ (talkA, 26));
    auto filtA = panel ("FILTER", { xM, talkA.getBottom() + gap, midW, bodyA.getBottom() - talkA.getBottom() - gap });
    row (body_ (filtA, 24), filter);

    // right: MOVE / PLAY / RAGE
    auto moveA = panel ("MOVE", { xR, y0, sideW, 176 });
    { auto h = head (moveA); moveOn->setBounds (h.removeFromRight (56)); h.removeFromRight (10); moveRetrig->setBounds (h.removeFromRight (92)); }
    {
        auto b = body_ (moveA, 32);
        auto left = b.removeFromLeft (92);
        moveShape->setVisible (true);
        moveShape->setBounds (left.withSizeKeepingCentre (left.getWidth(), jmin (left.getHeight(), 116)));
        b.removeFromLeft (8);
        row (b, move);
    }
    auto playA = panel ("PLAY", { xR, moveA.getBottom() + gap, sideW, 150 });
    { auto h = head (playA); h.removeFromLeft (70); bendFirst->setBounds (h.removeFromRight (96)); h.removeFromRight (10); mode->setBounds (h); }
    row (body_ (playA, 32), play);
    auto rageA = panel ("RAGE", { xR, playA.getBottom() + gap, sideW, 130 });
    { auto h = head (rageA); rageOn->setBounds (h.removeFromRight (56)); h.removeFromRight (10); rageType->setBounds (h.removeFromRight (210)); }
    row (body_ (rageA, 30).removeFromLeft (300), rage);

    // bottom row: AMP / FX / ENERGY / SPACE + OUT
    const int yb = rageA.getBottom() + gap, hb = H - m - yb;
    auto ampA = panel ("AMP", { xL, yb, 330, hb });
    row (body_ (ampA, 28), amp);
    auto fxA = panel ("", { ampA.getRight() + gap, yb, 520, hb });
    {
        auto b = fxA.reduced (14, 10);
        auto chips = b.removeFromTop (28);
        b.removeFromTop (2);
        const int w3 = b.getWidth() * 3 / 7, w2 = b.getWidth() * 2 / 7;
        dlyOn->setBounds (chips.getX() + w3 / 2 - 50, chips.getY(), 100, 26);
        revOn->setBounds (chips.getX() + w3 + w2 / 2 - 50, chips.getY(), 100, 26);
        pumpOn->setBounds (chips.getX() + w3 + w2 + w2 / 2 - 50, chips.getY(), 100, 26);
        row (b.removeFromLeft (w3), dly);
        row (b.removeFromLeft (w2), rev);
        row (b, pump);
    }
    auto enA = panel ("ENERGY", { fxA.getRight() + gap, yb, 210, hb }, true);
    energyKnob->setBounds (body_ (enA, 18).reduced (20, 0));
    auto spA = panel ("SPACE / OUT", { enA.getRight() + gap, yb, W - m - enA.getRight() - gap, hb });
    {
        auto b = body_ (spA, 28);
        meter.setBounds (b.removeFromRight (14).reduced (0, 4));
        b.removeFromRight (6);
        row (b, space);
    }
    renderBackground();
}

void Canvas::renderBackground()
{
    const float scale = 2.f;
    background = Image (Image::RGB, (int) (W * scale), (int) (H * scale), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));
    const auto lime = SerpentTheme::lime;
    g.fillAll (lnf.theme.bg);

    // toxic haze behind the logo so the black smoke has something to stand against
    {
        ColourGradient haze (lime.withAlpha (0.42f), W * 0.5f, 105.f, lime.withAlpha (0.f), W * 0.5f + 560.f, 105.f, true);
        haze.addColour (0.45, lime.withAlpha (0.18f));
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::scale (1.f, 0.36f, W * 0.5f, 105.f));
        g.setGradientFill (haze);
        g.fillRect (0, -300, W, 900);
    }
    for (int i = 0; i < 4; ++i)
    {
        const float a = -0.5f + (float) i * 0.2f;
        paint::beam (g, { 140.f, -20.f }, -a - 0.2f, 0.05f, 1200.f, lime, 0.045f);
        paint::beam (g, { W - 140.f, -20.f }, a + 0.2f, 0.05f, 1200.f, lime, 0.045f);
    }
    {
        ColourGradient fl (lime.withAlpha (0.09f), W * 0.5f, H + 120.f, Colours::transparentBlack, W * 0.5f, H - 320.f, true);
        g.setGradientFill (fl); g.fillRect (0, H - 400, W, 400);
        ColourGradient vig (Colours::transparentBlack, W * 0.5f, H * 0.45f, Colours::black.withAlpha (0.7f), 0.f, -80.f, true);
        g.setGradientFill (vig); g.fillRect (0, 0, W, H);
    }
    {
        ColourGradient line (lime.withAlpha (0.f), 0.f, 0.f, lime.withAlpha (0.f), (float) W, 0.f, false);
        line.addColour (0.5, lime.withAlpha (0.8f));
        g.setGradientFill (line);
        g.fillRect (0.f, 203.f, (float) W, 1.f);
    }
    g.setFont (lnf.titleFont (10.f, 0.25f));
    g.setColour (lnf.theme.text.withAlpha (0.75f));
    g.drawText ("V1.0", 20, 40, 300, 14, Justification::left);
    g.setFont (lnf.labelFont (12.f, 0.28f));
    g.setColour (lnf.theme.label);
    g.drawText ("HARDSTYLE SCREAMER", 20, 58, 320, 16, Justification::left);
    g.setColour (lnf.theme.dim);
    g.drawText ("CORE  /  EDGE  /  BODY", 20, 74, 320, 16, Justification::left);
    g.setColour (lime);
    g.drawText ("FEAR THE SERPENT", 20, 90, 320, 16, Justification::left);

    for (auto& p : panels) paint::glassPanel (g, lnf, p.area, p.title, 0, p.spot);
}

void Canvas::timerCallback()
{
    const float peak = proc.getEngine().outPeak.exchange (0.f);
    meter.setLevel (peak);
    logo.tick (peak);
    pad.tick();
    presets.refresh();
    auto dim = [] (const std::vector<Component*>& cells, bool on, std::initializer_list<Component*> extra = {})
    {
        const float a = on ? 1.f : 0.32f;
        for (auto* c : cells) if (std::abs (c->getAlpha() - a) > 0.001f) c->setAlpha (a);
        for (auto* c : extra) if (std::abs (c->getAlpha() - a) > 0.001f) c->setAlpha (a);
    };
    dim (edge, edgeOn->isOn(), { edgeOct });
    dim (body, bodyOn->isOn(), { bodyWave });
    dim (move, moveOn->isOn(), { moveShape, moveRetrig });
    dim (rage, rageOn->isOn(), { rageType });
    dim (dly, dlyOn->isOn());
    dim (rev, revOn->isOn());
    dim (pump, pumpOn->isOn());
}

void Canvas::paint (Graphics& g)
{
    if (background.isValid()) g.drawImage (background, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
}

// ======================================================================= Editor
SerpentEditor::SerpentEditor (SerpentProcessor& p) : AudioProcessorEditor (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (900, 540, 2250, 1350);
    getConstrainer()->setFixedAspectRatio ((double) Canvas::W / (double) Canvas::H);
    setSize (1200, 720);
}
