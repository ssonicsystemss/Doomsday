#include "RefPanel.h"
#include "../Presets/SampleFile.h"

using namespace juce;
using namespace doomsday::flat;

RefPanel::Wave RefPanel::wave (const float* x, int n)
{
    Wave w; w.peaks.assign (400, 0.f);
    float mx = 1.0e-9f;
    for (int c = 0; c < 400; ++c)
    {
        const int a = c * n / 400, e = jmax (a + 1, (c + 1) * n / 400);
        float p = 0.f; for (int i = a; i < e && i < n; ++i) p = jmax (p, std::abs (x[i]));
        w.peaks[(size_t) c] = p; mx = jmax (mx, p);
    }
    for (auto& p : w.peaks) p /= mx;
    return w;
}

Rectangle<float> RefPanel::loadButton() const { return getLocalBounds().toFloat().reduced (20.f, 14.f).removeFromTop (32.f).removeFromLeft (170.f); }
Rectangle<float> RefPanel::mineButton() const { return loadButton().translated (182.f, 0.f).withWidth (200.f); }
Rectangle<float> RefPanel::waveArea() const { auto a = getLocalBounds().toFloat().reduced (20.f, 14.f); a.removeFromTop (44.f); return a.removeFromTop (a.getHeight() * 0.55f); }
Rectangle<float> RefPanel::bandArea() const { auto a = getLocalBounds().toFloat().reduced (20.f, 14.f); a.removeFromTop (44.f); a.removeFromTop (a.getHeight() * 0.55f + 12.f); return a.removeFromLeft (a.getWidth() * 0.5f - 10.f); }
Rectangle<float> RefPanel::hintArea() const { auto a = getLocalBounds().toFloat().reduced (20.f, 14.f); a.removeFromTop (44.f); a.removeFromTop (a.getHeight() * 0.55f + 12.f); return a.removeFromRight (a.getWidth() * 0.5f - 10.f); }

void RefPanel::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    auto button = [&] (Rectangle<float> r, const String& txt, bool primary)
    {
        g.setColour (primary ? t.hot : t.control); g.fillRoundedRectangle (r, 7.f);
        g.setColour (primary ? t.hot : t.border); g.drawRoundedRectangle (r.reduced (0.5f), 7.f, 1.f);
        g.setFont (lnf.font (12.f, SemiBold, 0.04f)); g.setColour (primary ? Colours::white : t.text);
        g.drawText (txt, r, Justification::centred);
    };
    button (loadButton(), "Load reference", ! ref.valid);
    button (mineButton(), busy ? "Analysing..." : "Analyse my sound again", false);
    g.setFont (lnf.font (12.f, Medium)); g.setColour (t.textMute);
    g.drawText (ref.valid ? "Reference: " + refName + "  (one beat at " + String (roundToInt (proc.getEngine().tempoNow.load())) + " BPM)"
                          : "Drop a reverse bass WAV here (or Load). One beat of it is compared with your sound.",
                mineButton().translated (216.f, 0.f).withWidth (getWidth() - 460.f), Justification::centredLeft);
    // WAVEFORMS + ENVELOPES (red = the reference, blue = RE4GE)
    auto wa = waveArea();
    g.setColour (t.ground.withAlpha (0.6f)); g.fillRoundedRectangle (wa, 8.f);
    if (dragHover) { g.setColour (t.hot); g.drawRoundedRectangle (wa.reduced (1.f), 8.f, 2.f); }
    auto drawWave = [&] (const Wave& w, Colour c)
    {
        if (w.peaks.empty()) return;
        Path p; const float mid = wa.getCentreY(), h = wa.getHeight() * 0.45f;
        p.startNewSubPath (wa.getX(), mid);
        for (size_t i = 0; i < w.peaks.size(); ++i) p.lineTo (wa.getX() + wa.getWidth() * (float) i / (float) w.peaks.size(), mid - w.peaks[i] * h);
        for (size_t i = w.peaks.size(); i-- > 0;) p.lineTo (wa.getX() + wa.getWidth() * (float) i / (float) w.peaks.size(), mid + w.peaks[i] * h);
        p.closeSubPath();
        g.setColour (c.withAlpha (0.28f)); g.fillPath (p);
    };
    auto drawEnv = [&] (const re4ge::RefProfile& r, Colour c)
    {
        if (! r.valid) return;
        Path p;
        for (int k = 0; k < 100; ++k) { const Point<float> pt (wa.getX() + wa.getWidth() * (k + 0.5f) / 100.f, wa.getBottom() - 6.f - r.env[(size_t) k] * (wa.getHeight() - 12.f)); if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt); }
        g.setColour (c); g.strokePath (p, PathStrokeType (2.f));
        g.drawVerticalLine (roundToInt (wa.getX() + wa.getWidth() * (r.peakPct + 0.5f) / 100.f), wa.getY() + 4.f, wa.getBottom() - 4.f);
    };
    drawWave (refWave, t.hot); drawWave (mineWave, t.cool);
    drawEnv (ref, t.hot); drawEnv (mine, t.cool);
    g.setFont (lnf.font (11.f, Bold, 0.1f));
    g.setColour (t.hot); g.drawText ("REFERENCE", wa.reduced (10.f, 6.f), Justification::topLeft);
    g.setColour (t.cool); g.drawText ("RE4GE", wa.reduced (10.f, 6.f).translated (90.f, 0.f), Justification::topLeft);
    // BANDS: share of energy, reference vs RE4GE
    auto ba = bandArea();
    g.setFont (lnf.font (11.f, Bold, 0.12f)); g.setColour (t.textDim); g.drawText ("BANDS (share of energy)", ba.removeFromTop (18.f), Justification::centredLeft);
    const float rowH = ba.getHeight() / 6.f;
    for (int k = 0; k < 6; ++k)
    {
        auto r = ba.removeFromTop (rowH).reduced (0.f, 3.f);
        g.setFont (lnf.font (11.f, Medium)); g.setColour (t.textMute);
        g.drawText (re4ge::RefProfile::bandNames()[k], r.removeFromLeft (120.f), Justification::centredLeft);
        auto bar = [&] (const re4ge::RefProfile& p, Colour c, Rectangle<float> rr)
        {
            if (! p.valid) return;
            const float w = jlimit (0.f, 1.f, (p.bandDb[(size_t) k] + 40.f) / 40.f);     // -40 .. 0 dB
            g.setColour (c.withAlpha (0.85f)); g.fillRoundedRectangle (rr.withWidth (rr.getWidth() * w), 2.f);
        };
        auto valueTxt = r.removeFromRight (64.f);
        bar (ref, t.hot, r.removeFromTop (r.getHeight() * 0.5f).reduced (0.f, 1.f));
        bar (mine, t.cool, r.reduced (0.f, 1.f));
        if (ref.valid && mine.valid)
        {
            const float d = mine.bandDb[(size_t) k] - ref.bandDb[(size_t) k];
            g.setFont (lnf.font (12.f, SemiBold)); g.setColour (std::abs (d) > 2.f ? t.text : t.textMute);
            g.drawText ((d > 0 ? "+" : "") + String (d, 1) + " dB", valueTxt, Justification::centredRight);
        }
    }
    // HINTS
    auto ha = hintArea();
    g.setFont (lnf.font (11.f, Bold, 0.12f)); g.setColour (t.textDim); g.drawText ("WHAT TO CHANGE", ha.removeFromTop (18.f), Justification::centredLeft);
    g.setFont (lnf.font (12.5f, Medium)); g.setColour (t.text);
    const auto h = hintsNow();
    for (auto& line : h) { g.drawFittedText (String (CharPointer_UTF8 ("\xe2\x80\xa2 ")) + line, ha.removeFromTop (38.f).toNearestInt(), Justification::topLeft, 2); }
    if (h.isEmpty()) g.drawText (ref.valid ? "..." : "Load a reference first", ha.removeFromTop (20.f), Justification::centredLeft);
}

bool RefPanel::loadNow (const File& f)
{
    auto s = re4ge::readSampleFile (f);
    if (s == nullptr || s->data.size() < 256) return false;
    ref = re4ge::RefAnalysis::analyse (s->data.data(), (int) s->data.size(), s->rate, beatSamples (s->rate));
    refWave = wave (s->data.data(), jmin ((int) s->data.size(), beatSamples (s->rate)));
    refName = f.getFileName();
    proc.apvts.state.setProperty ("refPath", f.getFullPathName(), nullptr);
    return ref.valid;
}
void RefPanel::loadAsync (const File& f)
{
    const double bpmNow = proc.getEngine().tempoNow.load();
    auto flag = alive; SafePointer<RefPanel> safe (this);
    Thread::launch ([f, flag, safe, bpmNow]
    {
        auto s = re4ge::readSampleFile (f);
        std::shared_ptr<re::Sample> sh (s.release());
        MessageManager::callAsync ([f, flag, safe, sh, bpmNow]
        {
            if (! flag->load() || safe == nullptr || sh == nullptr || sh->data.size() < 256) return;
            const int beat = (int) (sh->rate * 60.0 / jlimit (60.0, 300.0, bpmNow));
            safe->ref = re4ge::RefAnalysis::analyse (sh->data.data(), (int) sh->data.size(), sh->rate, beat);
            safe->refWave = wave (sh->data.data(), jmin ((int) sh->data.size(), beat));
            safe->refName = f.getFileName();
            safe->proc.apvts.state.setProperty ("refPath", f.getFullPathName(), nullptr);
            safe->repaint();
        });
    });
}
void RefPanel::analyseMineForTest()
{
    auto job = proc.makeExportJob ((int) proc.apvts.state.getProperty ("expNote", 31), 1);
    const auto b = re4ge::Exporter::render (job);
    std::vector<float> m ((size_t) b.getNumSamples());
    for (int i = 0; i < b.getNumSamples(); ++i) m[(size_t) i] = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
    mine = re4ge::RefAnalysis::analyse (m.data(), (int) m.size(), job.rate, (int) m.size());
    mineWave = wave (m.data(), (int) m.size());
}
void RefPanel::analyseMine()
{
    if (busy) return;
    busy = true; repaint();
    auto job = proc.makeExportJob ((int) proc.apvts.state.getProperty ("expNote", 31), 1);
    auto flag = alive; SafePointer<RefPanel> safe (this);
    Thread::launch ([job, flag, safe]
    {
        const auto b = re4ge::Exporter::render (job);
        auto m = std::make_shared<std::vector<float>> ((size_t) b.getNumSamples());
        for (int i = 0; i < b.getNumSamples(); ++i) (*m)[(size_t) i] = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
        const double rate = job.rate;
        MessageManager::callAsync ([flag, safe, m, rate]
        {
            if (! flag->load() || safe == nullptr) return;
            safe->mine = re4ge::RefAnalysis::analyse (m->data(), (int) m->size(), rate, (int) m->size());
            safe->mineWave = wave (m->data(), (int) m->size());
            safe->busy = false;
            safe->repaint();
        });
    });
}
void RefPanel::pageShown()
{
    if (! triedSaved && ! ref.valid)
    {
        triedSaved = true;
        const File f (proc.apvts.state.getProperty ("refPath").toString());
        if (f.existsAsFile()) loadAsync (f);
    }
    analyseMine();
}
void RefPanel::filesDropped (const StringArray& files, int, int)
{
    dragHover = false;
    for (auto& f : files) if (File (f).hasFileExtension ("wav;aif;aiff;flac")) { loadAsync (File (f)); break; }
    repaint();
}
void RefPanel::mouseDown (const MouseEvent& e)
{
    if (loadButton().contains (e.position))
    {
        chooser = std::make_unique<FileChooser> ("Reference reverse bass", File(), "*.wav;*.aif;*.aiff;*.flac");
        SafePointer<RefPanel> safe (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [safe] (const FileChooser& fc)
        {
            if (safe != nullptr && fc.getResult().existsAsFile()) safe->loadAsync (fc.getResult());
        });
    }
    else if (mineButton().contains (e.position)) analyseMine();
}
void RefPanel::mouseMove (const MouseEvent& e)
{
    setTooltip (loadButton().contains (e.position) ? "A reverse bass you want to get close to (WAV). One beat of it is used"
              : mineButton().contains (e.position) ? "Renders your current sound (one beat, the note of the EXPORT page) and compares again"
              : waveArea().contains (e.position) ? "Red = the reference, blue = RE4GE: waveform + envelope over one beat (the line = the peak)" : "");
}
