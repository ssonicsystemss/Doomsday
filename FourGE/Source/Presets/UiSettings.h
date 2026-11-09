#pragma once
// =====================================================================
//  4GE module 17 - INTERFACE SETTINGS (a file of the user, NOT the project / preset: message thread only, no DSP, no GUI)
//   GUI SCALE (or FIT TO SCREEN), pop-up TOOLTIPS, the INFO LINE, the page 4GE opens on, the last page.
//   Stored in <user app data>/4GE/interface.settings (XML). Missing / broken file = the defaults.
// =====================================================================
#include <juce_core/juce_core.h>

namespace fge::ui
{
    struct Settings
    {
        float scale = 0.f;            // 0 = FIT TO SCREEN, else 0.6 .. 1.6
        bool tooltips = true;         // the pop-up tooltip windows (the INFO LINE always explains what is under the mouse)
        bool infoLine = true;
        int startPage = 0;            // -1 = the last page used
        int lastPage = 0;

        static juce::File defaultFile()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("4GE").getChildFile ("interface.settings");
        }
        static const juce::Array<float>& scales() { static const juce::Array<float> s { 0.f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.25f, 1.5f }; return s; }
        static juce::String scaleName (float s) { return s <= 0.f ? juce::String ("FIT") : juce::String (juce::roundToInt (s * 100.f)) + " %"; }

        static Settings load (const juce::File& f = defaultFile())
        {
            Settings s;
            if (auto xml = juce::XmlDocument::parse (f))
            {
                s.scale = juce::jlimit (0.f, 1.6f, (float) xml->getDoubleAttribute ("scale", 0.0));
                if (s.scale > 0.f && s.scale < 0.6f) s.scale = 0.6f;
                s.tooltips = xml->getBoolAttribute ("tooltips", true);
                s.infoLine = xml->getBoolAttribute ("infoLine", true);
                s.startPage = juce::jlimit (-1, 14, xml->getIntAttribute ("startPage", 0));
                s.lastPage = juce::jlimit (0, 14, xml->getIntAttribute ("lastPage", 0));
            }
            return s;
        }
        bool save (const juce::File& f = defaultFile()) const
        {
            juce::XmlElement x ("FGE_INTERFACE");
            x.setAttribute ("scale", scale); x.setAttribute ("tooltips", tooltips); x.setAttribute ("infoLine", infoLine);
            x.setAttribute ("startPage", startPage); x.setAttribute ("lastPage", lastPage);
            return f.getParentDirectory().createDirectory() && x.writeTo (f);
        }
        // the editor scale for a screen area (FIT = the largest of the list that leaves room for the host around it, never above 100 %)
        float resolve (juce::Rectangle<int> userArea, int w, int h) const
        {
            if (scale > 0.f) return scale;
            if (userArea.isEmpty()) return 0.8f;
            const float fit = juce::jmin (userArea.getWidth() * 0.9f / (float) w, userArea.getHeight() * 0.88f / (float) h, 1.f);
            float best = 0.6f;
            for (float s : scales()) if (s > 0.f && s <= fit + 1.0e-4f) best = juce::jmax (best, s);
            return best;
        }
    };
}
