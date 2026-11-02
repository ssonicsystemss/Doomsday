#include "ExportPanel.h"

using namespace juce;
using namespace doomsday::flat;

namespace
{
    const int lengths[4] { 1, 2, 4, 8 };
    const char* noteNames[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

ExportPanel::~ExportPanel() { alive->store (false); }
int ExportPanel::beats() const { return jlimit (1, 16, (int) proc.apvts.state.getProperty ("expBeats", 4)); }
int ExportPanel::note() const { return jlimit (12, 96, (int) proc.apvts.state.getProperty ("expNote", 31)); }

std::vector<std::pair<Rectangle<float>, int>> ExportPanel::layout() const
{
    std::vector<std::pair<Rectangle<float>, int>> z;
    auto a = getLocalBounds().toFloat().reduced (20.f, 16.f);
    const float label = 110.f, rowH = 34.f, gap = 18.f;
    auto row = [&] { auto r = a.removeFromTop (rowH); a.removeFromTop (gap); return r.withTrimmedLeft (label); };
    { auto r = row().removeFromLeft (4 * 90.f); for (int i = 0; i < 4; ++i) z.push_back ({ r.removeFromLeft (90.f).reduced (3.f, 0.f), 100 + i }); }
    { auto r = row(); const float w = jmin (64.f, r.getWidth() / 12.f); for (int i = 0; i < 12; ++i) z.push_back ({ r.removeFromLeft (w).reduced (3.f, 0.f), 200 + i }); }
    { auto r = row().removeFromLeft (3 * 90.f); for (int i = 0; i < 3; ++i) z.push_back ({ r.removeFromLeft (90.f).reduced (3.f, 0.f), 300 + i }); }
    a.removeFromTop (4.f);
    auto b = a.removeFromTop (54.f).withTrimmedLeft (label);
    z.push_back ({ b.removeFromLeft (260.f), 500 });
    b.removeFromLeft (18.f);
    if (last.existsAsFile())
    {
        z.push_back ({ b.removeFromLeft (360.f), 600 });
        b.removeFromLeft (12.f);
        z.push_back ({ b.removeFromLeft (110.f).withSizeKeepingCentre (110.f, 34.f), 700 });
    }
    return z;
}
int ExportPanel::hit (Point<float> p) const { for (auto& [r, c] : layout()) if (r.contains (p)) return c; return -1; }

void ExportPanel::paint (Graphics& g)
{
    const auto& t = tokens();
    auto& lnf = lnfOf (*this);
    auto chip = [&] (Rectangle<float> r, const String& text, bool on, bool hov, Colour c)
    {
        g.setColour (on ? c.withAlpha (0.18f) : hov ? t.control.brighter (0.08f) : t.control); g.fillRoundedRectangle (r, 7.f);
        g.setColour (on ? c : t.border); g.drawRoundedRectangle (r.reduced (0.5f), 7.f, 1.f);
        g.setFont (lnf.font (12.f, SemiBold, 0.04f)); g.setColour (on ? t.text : t.textDim);
        g.drawText (text, r, Justification::centred);
    };
    auto a = getLocalBounds().toFloat().reduced (20.f, 16.f);
    g.setFont (lnf.font (11.f, Bold, 0.15f)); g.setColour (t.textDim);
    const char* rows[] { "LENGTH", "NOTE", "OCTAVE" };
    for (int i = 0; i < 3; ++i) g.drawText (rows[i], a.getX(), a.getY() + i * 52.f, 110.f, 34.f, Justification::centredLeft);
    const int nb = beats(), nn = note();
    for (auto& [r, c] : layout())
    {
        const bool hov = hover == c;
        if (c >= 100 && c < 200) chip (r, String (lengths[c - 100]) + (lengths[c - 100] == 1 ? " beat" : " beats"), nb == lengths[c - 100], hov, t.cool);
        else if (c >= 200 && c < 300) chip (r, noteNames[c - 200], nn % 12 == c - 200, hov, t.hot);
        else if (c >= 300 && c < 400) chip (r, String (c - 300), nn / 12 - 1 == c - 300, hov, t.hot);
        else if (c == 500)
        {
            g.setColour (busy ? t.control : hov ? t.hot.brighter (0.15f) : t.hot); g.fillRoundedRectangle (r, 10.f);
            g.setFont (lnf.font (15.f, Bold, 0.12f)); g.setColour (busy ? t.textDim : Colours::white);
            g.drawText (busy ? "RENDERING..." : "EXPORT WAV", r, Justification::centred);
        }
        else if (c == 600)
        {
            g.setColour (hov ? t.cool.withAlpha (0.25f) : t.cool.withAlpha (0.14f)); g.fillRoundedRectangle (r, 10.f);
            g.setColour (t.cool); g.drawRoundedRectangle (r.reduced (0.5f), 10.f, 1.2f);
            g.setFont (lnf.font (12.f, SemiBold)); g.setColour (t.text);
            g.drawFittedText ("DRAG INTO FL:  " + last.getFileName(), r.reduced (12.f, 0.f).toNearestInt(), Justification::centredLeft, 2);
        }
        else if (c == 700) chip (r, "Folder", false, hov, t.cool);
    }
    g.setFont (lnf.font (12.f, Medium)); g.setColour (t.textMute);
    const float y = a.getY() + 3 * 52.f + 62.f;
    g.drawText (status.isNotEmpty() ? status : "One note per beat on the FL tempo (" + String (roundToInt (proc.getEngine().tempoNow.load())) + " BPM), EXPORT quality, 24-bit WAV -> Documents / RE4GE / Export",
                Rectangle<float> (a.getX() + 110.f, y, a.getWidth() - 110.f, 20.f), Justification::centredLeft);
}

void ExportPanel::start()
{
    if (busy) return;
    busy = true; status = "Rendering..."; repaint();
    auto job = proc.makeExportJob (note(), beats());
    auto flag = alive;
    SafePointer<ExportPanel> safe (this);
    Thread::launch ([job, flag, safe]
    {
        const File f = re4ge::Exporter::run (job);
        MessageManager::callAsync ([f, flag, safe]
        {
            if (! flag->load() || safe == nullptr) return;
            safe->busy = false;
            safe->last = f;
            safe->status = f.existsAsFile() ? "Saved: " + f.getFullPathName() : String ("Could not write the WAV (Documents / RE4GE / Export)");
            safe->repaint();
        });
    });
}

void ExportPanel::mouseDown (const MouseEvent& e)
{
    dragging = false;
    const int c = hit (e.position);
    if (c < 0) return;
    auto& st = proc.apvts.state;
    if (c >= 100 && c < 200) st.setProperty ("expBeats", lengths[c - 100], nullptr);
    else if (c >= 200 && c < 300) st.setProperty ("expNote", (note() / 12) * 12 + (c - 200), nullptr);
    else if (c >= 300 && c < 400) st.setProperty ("expNote", (c - 300 + 1) * 12 + note() % 12, nullptr);
    else if (c == 500) start();
    else if (c == 700) { re4ge::Exporter::folder().createDirectory(); re4ge::Exporter::folder().startAsProcess(); }
    repaint();
}
void ExportPanel::mouseDrag (const MouseEvent& e)
{
    if (dragging || hit (e.mouseDownPosition) != 600 || ! last.existsAsFile() || e.getDistanceFromDragStart() < 6) return;
    dragging = true;
    performExternalDragDropOfFiles ({ last.getFullPathName() }, false, this);
}
void ExportPanel::mouseMove (const MouseEvent& e)
{
    const int c = hit (e.position);
    if (c == hover) return;
    hover = c;
    setTooltip (c == 500 ? "Renders the bass with the current sound, one note per beat on the FL tempo"
              : c == 600 ? "Drag the WAV into the FL playlist or a sampler channel"
              : c >= 200 && c < 400 ? "The note of the export (the key of your track)" : c >= 100 && c < 200 ? "How many beats (one bass per beat)" : "");
    repaint();
}
File ExportPanel::exportNowForTest()
{
    const auto f = re4ge::Exporter::run (proc.makeExportJob (note(), beats()));
    last = f;
    return f;
}
