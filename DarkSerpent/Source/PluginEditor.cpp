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
    const String text ("DARK SERPENT");
    constexpr float pi = MathConstants<float>::pi, twoPi = MathConstants<float>::twoPi;

    // ---- title mask + rendered title (glow + letters)
    Font font = lnf.titleFont (60.f, 0.12f);
    {
        GlyphArrangement ga; ga.addLineOfText (font, text, 0.f, 0.f);
        const float tw = ga.getBoundingBox (0, -1, true).getWidth();
        font = lnf.titleFont (60.f * (float) w * 0.64f / jmax (1.f, tw), 0.12f);
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

    // ---- serpent centre line: coils (a helix seen from the side) -> rising neck -> head
    struct Pt { Point<float> p; float r, z, tail, head; };     // head: 0 = body, 0..1 along the head
    std::vector<Pt> pts;
    const float cy = (float) h * 0.52f, R = (float) h * 0.27f;
    // a helix seen from the side: two round coils whose front arcs cross the title line just before "D"
    // and in the gap between the words; the tail dissolves into the smoke on the left
    const float F0 = (float) w * 0.172f, F1 = (float) w * 0.418f;
    const float A = (F1 - F0) / twoPi, loopW = (float) h * 0.46f, B = (loopW + A * pi) * 0.5f;
    const float tA = -pi * 0.12f, tB = pi * 4.f;
    const int NC = 6000;
    for (int i = 0; i <= NC; ++i)
    {
        const float u = (float) i / (float) NC, t = tA + (tB - tA) * u;
        const float grow = std::sin (jmin (1.f, u * 2.f) * pi * 0.5f);
        const float x = F0 - B + A * (t - pi * 0.5f) + B * std::sin (t);
        const float y = cy - R * (0.8f + 0.2f * grow) * std::cos (t) + (float) h * 0.02f * std::sin (0.6f * t);
        const float z = std::sin (t);
        const float r = (1.0f + (float) h * 0.064f * std::pow (u, 0.35f)) * (1.f + 0.1f * z);
        pts.push_back ({ { x, y }, r, z, jlimit (0.f, 1.f, u / 0.16f), 0.f });
    }
    const float rNeck = pts.back().r;
    headDir = Point<float> (0.90f, -0.43f);
    {
        // over the top of "SERPENT", diving behind the last letters, rising in front after the "T"
        const Point<float> P0 = pts.back().p, P1 = P0 + Point<float> ((float) w * 0.20f, 0.f);
        const Point<float> P3 ((float) w * 0.872f, (float) h * 0.34f), P2 ((float) w * 0.84f, (float) h * 1.02f);
        for (int i = 1; i <= 1600; ++i)
        {
            const float t = (float) i / 1600.f, m = 1.f - t;
            const Point<float> p = P0 * (m * m * m) + P1 * (3.f * m * m * t) + P2 * (3.f * m * t * t) + P3 * (t * t * t);
            const float ramp = jlimit (0.f, 1.f, (t - 0.72f) / 0.25f);
            const float zz = -0.9f * std::sin (pi * jmin (1.f, t / 0.8f)) + 1.5f * ramp * ramp * (3.f - 2.f * ramp);
            pts.push_back ({ p, rNeck * (0.97f - 0.06f * std::sin (pi * t)), zz, 1.f, 0.f });
        }
        const float L = (float) h * 0.28f;
        auto d0 = pts.back().p - pts[pts.size() - 20].p;
        const Point<float> neckDir = d0 / jmax (1e-6f, d0.getDistanceFromOrigin());
        for (int i = 1; i <= 500; ++i)
        {
            const float t = (float) i / 500.f;
            const float bl = jmin (1.f, t * 2.5f);
            auto dir = neckDir * (1.f - bl) + headDir * bl;
            dir = dir / jmax (1e-6f, dir.getDistanceFromOrigin());
            const float r = rNeck * 0.92f * (1.f + 0.62f * std::sin (pi * jmin (1.f, t * 1.15f))) * (1.f - 0.66f * std::pow (t, 2.0f));
            pts.push_back ({ pts.back().p + dir * (L / 500.f), r, 1.5f, 1.f, t });
        }
    }
    const int N = (int) pts.size();
    std::vector<Point<float>> tang ((size_t) N);
    std::vector<float> q ((size_t) N, 0.f);
    const float scale = 0.46f;                   // scale size relative to body radius
    for (int i = 0; i < N; ++i)
    {
        auto d = pts[(size_t) jmin (N - 1, i + 3)].p - pts[(size_t) jmax (0, i - 3)].p;
        const float l = jmax (1e-6f, d.getDistanceFromOrigin());
        tang[(size_t) i] = d / l;
        if (i > 0) q[(size_t) i] = q[(size_t) i - 1] + pts[(size_t) i].p.getDistanceFrom (pts[(size_t) i - 1].p) / (scale * jmax (1.5f, pts[(size_t) i].r));
    }
    {
        const int iEye = N - 500 + (int) (500 * 0.56f);
        const auto P = Point<float> (-tang[(size_t) iEye].y, tang[(size_t) iEye].x);
        eye = pts[(size_t) iEye].p - P * (pts[(size_t) iEye].r * 0.42f);
        headR = pts[(size_t) (N - 500 + 200)].r;
        snout = pts.back().p + tang.back() * pts.back().r * 0.6f;
        headDir = tang.back();
    }

    // ---- render the body at 2x: z-buffered tube, glossy black scales, lime rim light
    const int SS = 2, W2 = w * SS, H2 = h * SS;
    std::vector<float> zb ((size_t) (W2 * H2), -1e9f), cu ((size_t) (W2 * H2)), cv ((size_t) (W2 * H2)), cov ((size_t) (W2 * H2));
    std::vector<int> id ((size_t) (W2 * H2), -1);
    const float zScale = R * (float) SS;
    for (int i = 0; i < N; ++i)
    {
        const auto& s = pts[(size_t) i];
        const float cx = s.p.x * SS, cyy = s.p.y * SS, r = s.r * SS, zc = s.z * zScale;
        const Point<float> Tn = tang[(size_t) i], Pn (-Tn.y, Tn.x);
        const int X0 = jmax (0, (int) (cx - r - 2)), X1 = jmin (W2 - 1, (int) (cx + r + 2));
        const int Y0 = jmax (0, (int) (cyy - r - 2)), Y1 = jmin (H2 - 1, (int) (cyy + r + 2));
        for (int y = Y0; y <= Y1; ++y)
            for (int x = X0; x <= X1; ++x)
            {
                const float dx = (float) x + 0.5f - cx, dy = (float) y + 0.5f - cyy, d2 = dx * dx + dy * dy;
                if (d2 > (r + 0.7f) * (r + 0.7f)) continue;
                const float sv = zc + std::sqrt (jmax (0.f, r * r - d2));
                const size_t k = (size_t) (y * W2 + x);
                if (sv <= zb[k]) continue;
                zb[k] = sv; id[k] = i;
                cu[k] = (dx * Pn.x + dy * Pn.y) / r;
                cv[k] = (dx * Tn.x + dy * Tn.y) / r;
                cov[k] = jlimit (0.f, 1.f, r - std::sqrt (d2) + 0.6f);
            }
    }
    Image back2 (Image::ARGB, W2, H2, true), front2 (Image::ARGB, W2, H2, true);
    {
        Image::BitmapData bb (back2, Image::BitmapData::writeOnly), fb (front2, Image::BitmapData::writeOnly);
        const float lx = -0.30f, ly = -0.78f, lz = 0.55f, ll = std::sqrt (lx * lx + ly * ly + lz * lz);
        const float Lx = lx / ll, Ly = ly / ll, Lz = lz / ll;
        float hx = Lx, hy = Ly, hz = Lz + 1.f; { const float hl = std::sqrt (hx * hx + hy * hy + hz * hz); hx /= hl; hy /= hl; hz /= hl; }
        const auto lime = SerpentTheme::lime;
        auto hash = [] (int a, int b) { uint32 x = (uint32) (a * 73856093) ^ (uint32) (b * 19349663); x ^= x >> 13; x *= 0x5bd1e995; x ^= x >> 15; return (float) (x & 0xffff) / 65535.f; };
        for (int y = 0; y < H2; ++y)
            for (int x = 0; x < W2; ++x)
            {
                const size_t k = (size_t) (y * W2 + x);
                const int i = id[k];
                if (i < 0) continue;
                const auto& s = pts[(size_t) i];
                const Point<float> Tn = tang[(size_t) i], Pn (-Tn.y, Tn.x);
                const float u = jlimit (-0.999f, 0.999f, cu[k]);
                // scales: diamond lattice in (along, around) coordinates
                const float qa = q[(size_t) i] + cv[k] / scale, qb = std::asin (u) / scale;
                const float m = qa + qb, n = qa - qb;
                const float fm = m - std::floor (m), fn = n - std::floor (n);
                const float groove = jmin (jmin (fm, 1.f - fm), jmin (fn, 1.f - fn));
                const float edge = 1.f - jlimit (0.f, 1.f, groove / 0.10f);
                const float var = hash ((int) std::floor (m), (int) std::floor (n));
                // normal: tube + a dome per scale
                const float nzT = std::sqrt (1.f - u * u);
                const float tx = Pn.x * u, ty = Pn.y * u, tz = nzT;          // smooth tube normal
                const float tdh = jmax (0.f, tx * hx + ty * hy + tz * hz);
                float nx = tx + (Tn.x * (fm - 0.5f) + Pn.x * (fn - 0.5f)) * 0.24f * nzT;
                float ny = ty + (Tn.y * (fm - 0.5f) + Pn.y * (fn - 0.5f)) * 0.24f * nzT;
                float nz = nzT;
                { const float nl = std::sqrt (nx * nx + ny * ny + nz * nz); nx /= nl; ny /= nl; nz /= nl; }
                const float diff = jmax (0.f, nx * Lx + ny * Ly + nz * Lz);
                const float ndh = jmax (0.f, nx * hx + ny * hy + nz * hz);
                const float streak = std::pow (tdh, 18.f);                      // the long gloss line along the body
                const float spec = (streak * (0.35f + 0.65f * std::pow (ndh, 6.f)) * 0.75f + std::pow (ndh, 60.f) * 0.35f * var)
                                   * (1.f - 0.9f * edge);
                const float sheen = std::pow (ndh, 4.f) * 0.05f;
                const float depthDim = s.z < 0.f ? 1.f + 0.35f * s.z : 1.f;       // far side of the coils a bit darker
                float v = (0.018f + 0.055f * diff + sheen) * (1.f - 0.6f * edge) + spec;
                v *= depthDim;
                if (s.head > 0.42f && std::abs (u - 0.30f) < 0.045f + 0.03f * s.head) v *= 0.15f;   // mouth line
                const float rim = std::pow (1.f - nzT, 2.2f) * jmax (0.f, ny + 0.35f) * 0.55f;
                const float r_ = jlimit (0.f, 1.f, v * 0.93f + rim * lime.getFloatRed());
                const float g_ = jlimit (0.f, 1.f, v * 0.98f + rim * lime.getFloatGreen());
                const float b_ = jlimit (0.f, 1.f, v * 0.92f + rim * lime.getFloatBlue());
                const float a = cov[k] * s.tail;
                // back layer holds everything behind the letters, the front layer fades in across z = 0
                const float fz = jlimit (0.f, 1.f, (s.z + 0.35f) / 0.7f), fa = fz * fz * (3.f - 2.f * fz);
                if (fa > 0.f)    fb.setPixelColour (x, y, Colour::fromFloatRGBA (r_, g_, b_, a * fa));
                if (s.z < 0.35f) bb.setPixelColour (x, y, Colour::fromFloatRGBA (r_, g_, b_, a));
            }
    }
    bodyBack = back2.rescaled (w, h, Graphics::highResamplingQuality);
    bodyFront = front2.rescaled (w, h, Graphics::highResamplingQuality);

    // ---- smoke ribbons
    ribbons.clear();
    Random rng (31);
    for (int k = 0; k < 15; ++k)
    {
        Ribbon r {};
        r.layer = k < 7 ? 0 : (k < 11 ? 1 : 2);
        r.baseY = cy + (float) gauss (rng) * (float) h * 0.15f;
        r.a1 = (float) h * (0.05f + 0.10f * rng.nextFloat());
        r.f1 = twoPi / ((float) w * (0.25f + 0.40f * rng.nextFloat()));
        r.w1 = 0.20f + 0.30f * rng.nextFloat();
        r.p1 = twoPi * rng.nextFloat();
        r.a2 = (float) h * (0.02f + 0.04f * rng.nextFloat());
        r.f2 = twoPi / ((float) w * (0.07f + 0.12f * rng.nextFloat()));
        r.w2 = 0.35f + 0.5f * rng.nextFloat();
        r.p2 = twoPi * rng.nextFloat();
        r.drift = 0.15f + 0.25f * rng.nextFloat();
        r.thick = (float) h * (0.07f + 0.17f * rng.nextFloat());
        r.tf = twoPi / ((float) w * (0.10f + 0.22f * rng.nextFloat()));
        r.tw = 0.25f + 0.5f * rng.nextFloat();
        r.tp = twoPi * rng.nextFloat();
        r.x0 = (float) w * (-0.08f + 0.45f * rng.nextFloat());
        r.x1 = jmin ((float) w * 0.93f, r.x0 + (float) w * (0.45f + 0.55f * rng.nextFloat()));
        r.alpha = r.layer == 2 ? 0.45f : 0.7f + 0.3f * rng.nextFloat();
        r.colour = Colour (0xffc4ccc0).interpolatedWith (SerpentTheme::lime, rng.nextFloat() * 0.55f);
        ribbons.push_back (r);
    }
}

void SmokeLogo::drawSmoke (Graphics& g, int layer)
{
    const float h = (float) getHeight(), w = (float) getWidth();
    const float boost = 1.f + 0.8f * glow;
    for (auto& r : ribbons)
    {
        if (r.layer != layer) continue;
        const float xa = r.x0 + w * 0.05f * std::sin (0.11f * time + r.p1), xb = r.x1 + w * 0.05f * std::sin (0.09f * time + r.p2);
        const int n = jmax (8, (int) ((xb - xa) / 7.f));
        Path fill, e1, e2, f1, f2;
        std::vector<Point<float>> lo ((size_t) n + 1);
        for (int i = 0; i <= n; ++i)
        {
            const float t = (float) i / (float) n, x = xa + (xb - xa) * t;
            const float y = r.baseY + r.a1 * (0.55f + 0.45f * std::sin (0.37f * r.f1 * x + 0.13f * time + r.tp)) * std::sin (r.f1 * x - r.w1 * time + r.p1)
                          + r.a2 * std::sin (r.f2 * x + r.w2 * time + r.p2) + 0.4f * r.a2 * std::sin (2.3f * r.f2 * x - 1.7f * r.w2 * time + r.p1)
                          + h * 0.035f * std::sin (r.drift * time + r.p2);
            const float env = std::pow (std::sin (MathConstants<float>::pi * t), 0.7f);
            const float tw = std::sin (r.tf * x - r.tw * time + r.tp);
            const float half = 0.5f * r.thick * env * tw;
            const Point<float> up (x, y - half), dn (x, y + half);
            lo[(size_t) i] = dn;
            if (i == 0) { fill.startNewSubPath (up); e1.startNewSubPath (up); e2.startNewSubPath (dn);
                          f1.startNewSubPath (x, y - half * 0.45f); f2.startNewSubPath (x, y + half * 0.15f + 2.f * env); }
            else        { fill.lineTo (up); e1.lineTo (up); e2.lineTo (dn); f1.lineTo (x, y - half * 0.45f); f2.lineTo (x, y + half * 0.15f + 2.f * env); }
        }
        for (int i = n; i >= 0; --i) fill.lineTo (lo[(size_t) i]);
        fill.closeSubPath();
        auto grad = [&] (float a)
        {
            ColourGradient cg (r.colour.withAlpha (0.f), xa, 0.f, r.colour.withAlpha (0.f), xb, 0.f, false);
            cg.addColour (0.25, r.colour.withAlpha (jmin (1.f, a)));
            cg.addColour (0.75, r.colour.withAlpha (jmin (1.f, a)));
            return cg;
        };
        g.setGradientFill (grad (0.10f * r.alpha * boost)); g.fillPath (fill);
        g.setGradientFill (grad (0.22f * r.alpha * boost));
        g.strokePath (e1, PathStrokeType (0.9f)); g.strokePath (e2, PathStrokeType (0.9f));
        g.setGradientFill (grad (0.10f * r.alpha * boost));
        g.strokePath (f1, PathStrokeType (0.7f)); g.strokePath (f2, PathStrokeType (0.7f));
    }
}

void SmokeLogo::tick (float level)
{
    time += 1.f / 30.f;
    const float target = jlimit (0.f, 1.f, level * 1.5f);
    glow = target > glow ? target : glow * 0.92f;
    repaint();
}

void SmokeLogo::paint (Graphics& g)
{
    if (! title.isValid()) return;
    drawSmoke (g, 0);
    g.drawImageAt (bodyBack, 0, 0);
    drawSmoke (g, 1);
    g.drawImageAt (title, 0, 0);
    g.drawImageAt (bodyFront, 0, 0);

    // tongue: a quick double flick every few seconds
    {
        const float period = 3.4f, ph = std::fmod (time, period);
        const float ext = ph < 0.9f ? std::pow (std::sin (MathConstants<float>::pi * ph / 0.9f), 0.6f) * (0.75f + 0.25f * std::sin (ph * 22.f)) : 0.f;
        if (ext > 0.02f)
        {
            const float len = headR * 3.4f * ext;
            const Point<float> perp (-headDir.y, headDir.x);
            const float wig = 0.18f * std::sin (time * 37.f);
            const Point<float> mid = snout + (headDir + perp * wig) * len * 0.72f;
            Path t;
            t.startNewSubPath (snout);
            t.quadraticTo (snout + headDir * len * 0.35f + perp * (wig * len * 0.3f), mid);
            for (float side : { -1.f, 1.f })
            {
                t.startNewSubPath (mid);
                t.lineTo (mid + (headDir + perp * (0.55f * side + wig)) * len * 0.30f);
            }
            g.setColour (SerpentTheme::lime.withAlpha (0.35f + 0.3f * glow));
            g.strokePath (t, PathStrokeType (3.6f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (Colour (0xff0b0d0a));
            g.strokePath (t, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }
    // eye
    {
        const float r = headR * 0.30f * (1.f + 0.15f * glow);
        ColourGradient halo (SerpentTheme::lime.withAlpha (0.45f + 0.45f * glow), eye.x, eye.y, SerpentTheme::lime.withAlpha (0.f), eye.x + r * 4.5f, eye.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (Rectangle<float> (r * 9.f, r * 9.f).withCentre (eye));
        g.setColour (SerpentTheme::lime);
        g.fillEllipse (Rectangle<float> (r * 2.2f, r * 1.6f).withCentre (eye));
        g.setColour (SerpentTheme::limeHot);
        g.fillEllipse (Rectangle<float> (r * 1.2f, r * 0.8f).withCentre (eye.translated (-r * 0.2f, -r * 0.2f)));
        g.setColour (Colour (0xff050605));
        g.fillEllipse (Rectangle<float> (r * 0.42f, r * 1.45f).withCentre (eye));
    }
    drawSmoke (g, 2);
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
