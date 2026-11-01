#pragma once
// RE4GE - the user's own distortion curves: Documents / RE4GE / Shapes / <name>.re4shape (one line: the mod::Shape text).
// File handling only (message thread); the GUI calls it, the engine never sees files.
#include <juce_core/juce_core.h>
#include "DoomsdayMod.h"

namespace re4ge::shapefiles
{
    inline juce::File folder() { return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("RE4GE").getChildFile ("Shapes"); }
    inline juce::Array<juce::File> list() { auto f = folder().findChildFiles (juce::File::findFiles, false, "*.re4shape"); f.sort(); return f; }
    inline bool save (const juce::String& name, const doomsday::mod::Shape& s)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty() || ! folder().createDirectory()) return false;
        return folder().getChildFile (clean + ".re4shape").replaceWithText (s.toString());
    }
    inline bool load (const juce::File& f, doomsday::mod::Shape& out) { return doomsday::mod::Shape::fromString (f.loadFileAsString().trim(), out); }
}
