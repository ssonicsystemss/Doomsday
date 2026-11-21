#pragma once
// =====================================================================
//  4GE - KICK VARIATION page (page 15, owner 2026-11-20; GUI only - the rules = Presets/KickVariation.h, render + measure + check = Presets/VariationWorker.h thread).
//   The logic (owner 2026-11-21):  DNA = WHAT may change and WHICH WAY   STYLE = the sound world we work in
//   VARIATION = how far from the BASE   HUMANIZE = how far from an identical, sterile result (never the character)
//   MUTATE = versions of MY kick   NEW KICK IN STYLE = new kicks from scratch   MORPH = BASE -> a version   SET AS BASE = the new start.
//   DNA (left): 9 parts, KEEP / EVOLVE + direction / FREE, a bar = how far that part is from the BASE now.
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"
#include "NavBar.h"
#include "GeLook.h"
#include "../Presets/VariationWorker.h"

namespace fge::gui
{
    class VariationPage : public juce::Component, private juce::Timer
    {
    public:
        VariationPage (GeProcessor& p, GeLook& l) : proc (p), lnf (l)
        {
            using namespace fge::kv;
            for (int g = 0; g < NumGenes; ++g)
            {
                auto& r = rows[(size_t) g];
                r.mode = std::make_unique<ChoiceStrip> (modeNames());
                r.mode->tipFor = [g] (int i) { return geneTip (g) + (i == Keep ? "  KEEP: never changes." : i == Evolve ? "  EVOLVE: a small step - in the direction picked on the right." : "  FREE: anything in the STYLE (also its type)."); };
                r.mode->onPick = [this, g] (int i) { auto s = load (proc); s.mode[(size_t) g] = i; save (proc, s); sync(); };
                addAndMakeVisible (*r.mode);
                r.dir = std::make_unique<ChoiceStrip> (juce::StringArray { dirName (g, false), "ANY", dirName (g, true) });
                r.dir->tipFor = [g] (int i) { return i == 1 ? juce::String ("EVOLVE in any direction.") : "EVOLVE: every version goes " + dirName (g, i == 2).toLowerCase() + " (" + geneTip (g).upToFirstOccurrenceOf (":", false, false) + ")."; };
                r.dir->onPick = [this, g] (int i) { auto s = load (proc); s.dir[(size_t) g] = i - 1; save (proc, s); sync(); };
                addAndMakeVisible (*r.dir);
            }
            styleStrip.tipFor = [] (int i) { return fge::style::names()[i] + ": " + fge::style::description (i) + "  - the sound world MUTATE and NEW KICK IN STYLE draw from."; };
            styleStrip.onPick = [this] (int st) { fge::kv::setStyle (proc, st); sync(); };
            addAndMakeVisible (styleStrip);
            freshBtn.setTooltip ("NEW KICK IN STYLE: 7 kicks built from scratch within the STYLE (its template + a roll of its ranges) - not variations of your kick. Your length and the BASE stay.");
            freshBtn.onClick = [this] { worker.submit (fge::kv::makeRequest (proc, true)); syncMorph(); repaint(); };
            addAndMakeVisible (freshBtn);
            mutateBtn.getProperties().set ("primary", true);
            mutateBtn.setTooltip ("MUTATE: 7 new versions of the BASE, made by its DNA (KEEP / EVOLVE + direction / FREE) and VARIATION. Your sound does not change until you click a version.");
            mutateBtn.onClick = [this] { worker.submit (fge::kv::makeRequest (proc, false)); syncMorph(); repaint(); };
            addAndMakeVisible (mutateBtn);
            variation.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            variation.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
            for (auto id : { juce::Slider::textBoxOutlineColourId, juce::Slider::textBoxBackgroundColourId }) variation.setColour (id, juce::Colours::transparentBlack);
            variation.getProperties().set ("accent", (juce::int64) palette().red.getARGB());
            variation.getProperties().set ("valueSize", 13.f);
            variation.setRange (5.0, 100.0, 1.0); variation.setTextValueSuffix (" %");
            variation.setDoubleClickReturnValue (true, 50.0);
            variation.setMouseDragSensitivity (220);
            variation.setTooltip ("VARIATION: how far the versions may go from the BASE - this changes the kick (5 % = a nuance, 100 % = very different). Also the amount of RANDOMIZE and of every page dice.");
            variation.onValueChange = [this] { fge::pagernd::setStrength (proc, (float) variation.getValue() / 100.f); };
            addAndMakeVisible (variation);
            humanize = std::make_unique<doomsday::flat::Knob> (proc.apvts, "kv_humanize", "HUMANIZE", palette().silver, false, 44);
            humanize->slider.setTooltip ("HUMANIZE: the same kick, tiny natural differences on every hit (pitch up to +-12 cents, level up to +-0.8 dB at 100 %). Never changes a knob or the character - only how identical the hits are.");
            addAndMakeVisible (*humanize);
            setBase.setTooltip ("SET AS BASE: what you hear now becomes the BASE - and 7 new versions grow from it.");
            setBase.onClick = [this] { fge::kv::setAsBase (proc); worker.submit (fge::kv::makeRequest (proc, false)); syncMorph(); repaint(); };
            backBase.setTooltip ("BACK TO BASE: the BASE kick again (one undo step).");
            backBase.onClick = [this] { if (proc.kvWork.hasBase) { fge::kv::select (proc, -1); syncMorph(); repaint(); } };
            for (auto* b : { &setBase, &backBase }) addAndMakeVisible (b);
            morph.setSliderStyle (juce::Slider::LinearHorizontal);
            morph.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            morph.setRange (0.0, 1.0, 0.001);
            morph.setTooltip ("MORPH: from the BASE (A) to the version you picked (B). Numbers glide, switches jump at 50 %. One undo step per move.");
            morph.onDragStart = [this] { dragStart = proc.frame(); dragging = true; };
            morph.onValueChange = [this] { if (dragging) morphTo ((float) morph.getValue(), false); };
            morph.onDragEnd = [this] { dragging = false; morphTo ((float) morph.getValue(), true); };
            addAndMakeVisible (morph);
            worker.onDone = [this] { if (auto r = worker.take()) fge::kv::accept (proc, *r); syncMorph(); repaint(); };
            sync();
            startTimerHz (15);
        }
        ~VariationPage() override { stopTimer(); worker.onDone = nullptr; }

        void sync()
        {
            const auto s = fge::kv::load (proc);
            for (int g = 0; g < fge::kv::NumGenes; ++g)
            {
                auto& r = rows[(size_t) g];
                r.mode->setLit (s.mode[(size_t) g]);
                r.dir->setLit (s.dir[(size_t) g] + 1);
                const bool ev = s.mode[(size_t) g] == fge::kv::Evolve;
                r.dir->setAlpha (ev ? 1.f : 0.3f); r.dir->setInterceptsMouseClicks (ev, false);
            }
            variation.setValue (s.variation * 100.0, juce::dontSendNotification);
            styleStrip.setLit (fge::kv::styleOf (proc));
            syncMorph();
            repaint();
        }
        // (tests) the tile at a version index (-1 = BASE)
        juce::Rectangle<int> tileBounds (int v) const { return tile[(size_t) juce::jlimit (0, 7, v + 1)]; }
        juce::Button& mutateButton() { return mutateBtn; }
        juce::Button& newKickButton() { return freshBtn; }
        bool working() const { return worker.busy(); }

        void resized() override
        {
            const int dnaW = 610;
            dnaArea = { 0, 0, dnaW, getHeight() };
            const int rowH = (getHeight() - 96) / fge::kv::NumGenes;
            for (int g = 0; g < fge::kv::NumGenes; ++g)
            {
                const int y = 66 + g * rowH;
                rowArea[(size_t) g] = { 16, y, dnaW - 32, rowH };
                rows[(size_t) g].mode->setBounds (236, y + 4, 168, 26);
                rows[(size_t) g].dir->setBounds (414, y + 4, dnaW - 16 - 414, 26);
                dirLabel = { 414, 46, dnaW - 16 - 414, 16 };
            }
            verArea = { dnaW + 12, 0, getWidth() - dnaW - 12, 560 };
            const int bottomY = verArea.getBottom() + 12, bottomH = getHeight() - bottomY;
            humArea = { verArea.getRight() - 250, bottomY, 250, bottomH };
            morphArea = { verArea.getX(), bottomY, verArea.getWidth() - 262, bottomH };
            styleStrip.setBounds (verArea.getX() + 86, 46, verArea.getWidth() - 86 - 16 - 190, 28);
            freshBtn.setBounds (verArea.getRight() - 196, 46, 180, 28);
            const int tw = (verArea.getWidth() - 32 - 3 * 10) / 4, th = 126;
            for (int i = 0; i < 8; ++i) tile[(size_t) i] = { verArea.getX() + 16 + (i % 4) * (tw + 10), 112 + (i / 4) * (th + 10), tw, th };
            const int cy = 112 + 2 * (th + 10) + 14;
            mutateBtn.setBounds (verArea.getX() + 16, cy + 4, 250, 50);
            variation.setBounds (verArea.getX() + 300, cy - 6, 90, doomsday::flat::Knob::heightFor (48) - 14);
            setBase.setBounds (verArea.getRight() - 186, cy + 4, 170, 30);
            backBase.setBounds (verArea.getRight() - 186, cy + 42, 170, 30);
            humanize->setBounds (humArea.getX() + 16, humArea.getY() + 40, 80, doomsday::flat::Knob::heightFor (44));
            morph.setBounds (morphArea.getX() + 150, morphArea.getY() + 62, morphArea.getWidth() - 300, 24);
        }

        void paint (juce::Graphics& g) override
        {
            using namespace fge::kv;
            const auto& c = palette();
            auto& w = proc.kvWork;
            const juce::Font barlow (juce::FontOptions (lnf.pctTf).withHeight (14.f));
            auto title = [&] (juce::Rectangle<int> a, const juce::String& t, const juce::String& hint)
            {
                glassPanel (g, a.toFloat(), 6.f);
                g.setColour (c.silver); g.setFont (lnf.head (12.f)); g.drawText (t, a.getX() + 16, a.getY() + 14, 300, 18, juce::Justification::left);
                g.setColour (c.textDim); g.setFont (barlow);
                g.drawText (hint, a.getX() + 16 + juce::GlyphArrangement::getStringWidthInt (lnf.head (12.f), t) + 18, a.getY() + 14, a.getWidth() - 200, 18, juce::Justification::left, true);
            };
            // ---- DNA
            title (dnaArea, "DNA", "What may change - and which way");
            const auto f = proc.frame();
            g.setColour (c.textDim); g.setFont (lnf.head (9.f));
            g.drawText ("MODE", 236, 46, 168, 16, juce::Justification::centredLeft);
            g.drawText ("EVOLVE DIRECTION", dirLabel, juce::Justification::centredLeft);
            for (int gn = 0; gn < NumGenes; ++gn)
            {
                const auto r = rowArea[(size_t) gn];
                g.setColour (c.silver); g.setFont (lnf.head (11.f)); g.drawText (geneNames()[gn], r.getX(), r.getY() + 4, 110, 26, juce::Justification::centredLeft);
                g.setColour (c.textDim); g.setFont (barlow); g.drawText (readout (f, gn), r.getX() + 102, r.getY() + 4, 124, 26, juce::Justification::centredLeft, true);
                // how far this gene is from the BASE now
                const float d = w.hasBase ? geneAway[(size_t) gn] : 0.f;
                const auto bar = juce::Rectangle<float> ((float) r.getX(), (float) r.getY() + 38.f, (float) r.getWidth(), 3.f);
                g.setColour (c.ground); g.fillRect (bar);
                g.setColour (c.part[2].withAlpha (0.9f)); g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.f, 1.f, d / 60.f)));
                if (gn < NumGenes - 1) { g.setColour (c.line); g.fillRect ((float) r.getX(), (float) r.getBottom() - 6.f, (float) r.getWidth(), 1.f); }
            }
            g.setColour (c.textDim); g.setFont (barlow.withHeight (12.5f));
            g.drawText ("Bar = how far each part is from the BASE now", dnaArea.getX() + 16, dnaArea.getBottom() - 20, dnaArea.getWidth() - 32, 14, juce::Justification::left);
            // ---- VERSIONS
            title (verArea, "VERSIONS", "Click one to hear it");
            g.setColour (c.textDim); g.setFont (lnf.head (10.f)); g.drawText ("STYLE", verArea.getX() + 16, 46, 70, 28, juce::Justification::centredLeft);
            g.setFont (barlow.withHeight (13.f));
            g.drawText ("The sound world MUTATE and NEW KICK IN STYLE draw from", verArea.getX() + 86, 78, 420, 18, juce::Justification::centredLeft);
            g.drawText ("build from scratch within STYLE", freshBtn.getX(), 78, freshBtn.getWidth(), 18, juce::Justification::centredRight);
            const bool busy = worker.busy();
            float top = 1.0e-6f;
            for (size_t i = 0; i < w.waveHi.size(); ++i) for (size_t k = 0; k < w.waveHi[i].size(); ++k) top = juce::jmax (top, w.waveHi[i][k], -w.waveLo[i][k]);
            for (int i = 0; i < 8; ++i)
            {
                const auto t = tile[(size_t) i];
                const bool exists = ! busy && (i == 0 || i - 1 < (int) w.versions.size());
                const bool now = exists && ((i == 0 && w.selected == -1) || (i > 0 && w.selected == i - 1));
                g.setColour (now ? c.control.brighter (0.1f) : c.ground.brighter (0.03f)); g.fillRoundedRectangle (t.toFloat(), 5.f);
                g.setColour (now ? c.red : (i == hoverTile && exists ? c.line.brighter (0.5f) : c.line)); g.drawRoundedRectangle (t.toFloat().reduced (0.5f), 5.f, now ? 1.5f : 1.f);
                g.setFont (lnf.head (11.f)); g.setColour (exists ? c.silver : c.textDim.withAlpha (0.5f));
                g.drawText (i == 0 ? juce::String ("BASE") : "V" + juce::String (i), t.getX() + 12, t.getY() + 10, 80, 16, juce::Justification::left);
                if (i > 0 && exists)
                {
                    g.setFont (barlow.withHeight (13.f)); g.setColour (c.textDim);
                    g.drawText (juce::String (juce::roundToInt (w.away[(size_t) i - 1])) + "% away", t.getRight() - 92, t.getY() + 10, 80, 16, juce::Justification::right);
                    g.setColour (c.silver.withAlpha (0.85f));
                    g.drawText (w.labels[i - 1], t.getX() + 12, t.getBottom() - 22, t.getWidth() - 36, 16, juce::Justification::left, true);
                }
                if (i == w.morphB + 1 && i > 0 && exists) { g.setFont (lnf.head (9.f)); g.setColour (c.red); g.drawText ("B", t.getRight() - 22, t.getBottom() - 22, 12, 12, juce::Justification::centred); }
                if (busy)
                {
                    if (i == 1)
                    {
                        g.setFont (barlow.withHeight (13.f)); g.setColour (c.textDim);
                        g.drawFittedText ("Rendering and checking " + juce::String (juce::roundToInt (worker.progress() * (float) fge::kv::candidates)) + " / " + juce::String (fge::kv::candidates),
                                          t.reduced (14, 30), juce::Justification::centred, 2);
                        auto bar = t.toFloat().reduced (14.f, 0.f).withTrimmedTop ((float) t.getHeight() - 30.f).withHeight (3.f);
                        g.setColour (c.ground); g.fillRect (bar); g.setColour (c.red); g.fillRect (bar.withWidth (bar.getWidth() * worker.progress()));
                    }
                    continue;
                }
                if (! exists)
                {
                    if (i == 1) { g.setFont (barlow.withHeight (13.f)); g.setColour (c.textDim); g.drawFittedText ("MUTATE = 7 versions of this kick, NEW KICK IN STYLE = 7 new ones", t.reduced (14, 30), juce::Justification::centred, 3); }
                    continue;
                }
                if (i < (int) w.waveHi.size())
                {
                    const auto& hi = w.waveHi[(size_t) i]; const auto& lo = w.waveLo[(size_t) i];
                    auto a = t.toFloat().reduced (12.f, 0.f).withTrimmedTop (32.f).withTrimmedBottom (i > 0 ? 28.f : 12.f);
                    const float mid = a.getCentreY(), hh = a.getHeight() * 0.5f / top;
                    juce::Path pth; const int n = (int) hi.size();
                    for (int k = 0; k < n; ++k) { const float x = a.getX() + a.getWidth() * (float) k / (float) (n - 1); if (k == 0) pth.startNewSubPath (x, mid - hi[0] * hh); else pth.lineTo (x, mid - hi[(size_t) k] * hh); }
                    for (int k = n - 1; k >= 0; --k) pth.lineTo (a.getX() + a.getWidth() * (float) k / (float) (n - 1), mid - lo[(size_t) k] * hh);
                    pth.closeSubPath();
                    const auto col = i == 0 ? c.silver : c.part[2];
                    g.setColour (col.withAlpha (0.55f)); g.fillPath (pth);
                    g.setColour (col.withAlpha (0.9f)); g.strokePath (pth, juce::PathStrokeType (0.8f));
                }
                else if (i == 0) { g.setFont (barlow.withHeight (13.f)); g.setColour (c.textDim); g.drawText ("your kick", t.reduced (12, 30), juce::Justification::centred); }
            }
            g.setColour (c.textDim); g.setFont (barlow.withHeight (13.f));
            g.drawText ("preserve the identity of BASE", mutateBtn.getX(), mutateBtn.getBottom() + 6, 250, 16, juce::Justification::centred);
            g.drawText ("how far from the BASE", variation.getX() - 20, variation.getBottom() + 14, variation.getWidth() + 40, 16, juce::Justification::centred);
            if (! w.versions.empty() && ! busy)
                g.drawText ((w.fresh ? juce::String ("New kicks in ") + fge::style::names()[fge::kv::styleOf (proc)] : juce::String ("Versions of your BASE")) + " - "
                              + juce::String (w.passed) + " of " + juce::String (w.tried) + " candidates passed the checks" + (w.relaxed ? " (tolerances relaxed)" : "") + " - your sound changes only when you click one",
                            verArea.getX() + 16, verArea.getBottom() - 24, verArea.getWidth() - 32, 16, juce::Justification::left, true);
            g.setFont (lnf.font (10.5f, doomsday::flat::SemiBold, 0.02f)); g.setColour (lnf.knobLabelColour (variation.isMouseOverOrDragging()));
            g.drawText ("VARIATION", variation.getX(), variation.getBottom(), variation.getWidth(), 14, juce::Justification::centred);
            // ---- HUMANIZE (apart from VARIATION: it never changes the kick, only how identical the hits are)
            title (humArea, "HUMANIZE", "");
            g.setColour (c.textDim); g.setFont (barlow.withHeight (13.f));
            g.drawFittedText ("Same kick, tiny natural differences on every hit", humArea.getX() + 106, humArea.getY() + 44, humArea.getWidth() - 120, 60, juce::Justification::topLeft, 4);
            // ---- MORPH
            title (morphArea, "MORPH", "From the BASE to the version marked B");
            const bool canMorph = w.hasBase && juce::isPositiveAndBelow (w.morphB, (int) w.versions.size());
            g.setFont (lnf.head (12.f)); g.setColour (c.silver);
            g.drawText ("A", morphArea.getX() + 16, morph.getY(), 20, 24, juce::Justification::centredLeft);
            g.drawText ("B", morphArea.getRight() - 32, morph.getY(), 20, 24, juce::Justification::centredRight);
            g.setFont (barlow); g.setColour (c.textDim);
            g.drawText ("BASE", morphArea.getX() + 40, morph.getY(), 100, 24, juce::Justification::centredLeft);
            g.drawText (canMorph ? "V" + juce::String (w.morphB + 1) + "  (" + juce::String (juce::roundToInt (w.morph * 100.f)) + "%)" : juce::String ("pick a version"),
                        morphArea.getRight() - 150, morph.getY(), 110, 24, juce::Justification::centredRight);
        }
        void mouseMove (const juce::MouseEvent& e) override { const int h = tileAt (e.getPosition()); if (h != hoverTile) { hoverTile = h; repaint(); } }
        void mouseExit (const juce::MouseEvent&) override { hoverTile = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            const int i = tileAt (e.getPosition());
            if (i < 0) return;
            auto& w = proc.kvWork;
            if (worker.busy()) return;
            if (i == 0) { if (w.hasBase) fge::kv::select (proc, -1); }
            else if (i - 1 < (int) w.versions.size()) fge::kv::select (proc, i - 1);
            else return;
            proc.audition (true);
            syncMorph(); repaint();
        }

    private:
        struct Row { std::unique_ptr<ChoiceStrip> mode, dir; };
        int tileAt (juce::Point<int> p) const { for (int i = 0; i < 8; ++i) if (tile[(size_t) i].contains (p)) return i; return -1; }
        void syncMorph()
        {
            auto& w = proc.kvWork;
            const bool ok = w.hasBase && juce::isPositiveAndBelow (w.morphB, (int) w.versions.size());
            morph.setEnabled (ok); morph.setAlpha (ok ? 1.f : 0.35f);
            morph.setValue (ok ? w.morph : 0.0, juce::dontSendNotification);
        }
        void morphTo (float t, bool done)
        {
            auto& w = proc.kvWork;
            if (! w.hasBase || ! juce::isPositiveAndBelow (w.morphB, (int) w.versions.size())) return;
            const auto to = fge::kv::morphFrame (w.base, w.versions[(size_t) w.morphB], t);
            if (done) fge::kv::commit (proc, dragStart, to, "Morph"); else fge::kv::setLive (proc, to);
            w.morph = t; w.selected = t >= 0.999f ? w.morphB : (t <= 0.001f ? -1 : -2);   // (-2 = between: no tile is "now")
            repaint (morphArea); repaint (verArea);
        }
        void timerCallback() override
        {
            if (! isShowing()) return;
            auto& w = proc.kvWork;
            const auto f = proc.frame();
            if (w.hasBase) for (int gn = 0; gn < fge::kv::NumGenes; ++gn) geneAway[(size_t) gn] = fge::kv::distance (f, w.base, gn);
            if (worker.busy() || wasBusy) { wasBusy = worker.busy(); repaint (verArea); }
            const float v = fge::pagernd::strength (proc) * 100.f;
            if (std::abs (v - (float) variation.getValue()) > 0.5f && ! variation.isMouseButtonDown()) variation.setValue (v, juce::dontSendNotification);
            if (! std::equal (f.v.begin(), f.v.end(), lastF.v.begin())) { lastF = f; repaint (dnaArea); }
        }

        GeProcessor& proc;
        GeLook& lnf;
        std::array<Row, fge::kv::NumGenes> rows;
        std::array<juce::Rectangle<int>, fge::kv::NumGenes> rowArea;
        std::array<float, fge::kv::NumGenes> geneAway {};
        std::array<juce::Rectangle<int>, 8> tile;
        juce::Rectangle<int> dnaArea, verArea, morphArea, humArea, dirLabel;
        ChoiceStrip styleStrip { juce::StringArray { "EARLY", "MODERN", "RAW", "EXP", "EUPHORIC", "PVC", "ZAAG" } };
        juce::TextButton mutateBtn { "MUTATE" }, freshBtn { "New kick in style" }, setBase { "Set as base" }, backBase { "Back to base" };
        juce::Slider variation, morph;
        std::unique_ptr<doomsday::flat::Knob> humanize;
        fge::kv::VariationWorker worker;
        ParamFrame dragStart, lastF;
        bool dragging = false, wasBusy = false;
        int hoverTile = -1;
    };
}
