#pragma once
// =====================================================================
//  4TUNE - PRESET LIBRARY (spec 27; files / metadata only: no DSP, no GUI)
//  FACTORY + USER presets (.4tune in Documents/4TUNE/Presets) with CATEGORY (the "CAT - " name prefix), AUTHOR (root attribute
//  "author" of the file), FAVOURITES (the same file the preset bar's star uses), SEARCH (name / category / author),
//  RENAME / DELETE (to the trash) / DUPLICATE. A user preset stores the sound only: the MELODY stays in the project.
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>

namespace tune::lib
{
    inline const juce::StringArray& categories()
    {
        static const juce::StringArray c { "PLUCKS", "LEADS", "SUPERSAWS", "PADS", "BASS", "KEYS", "EXPERIMENTAL", "FESTIVAL", "EUPHORIC", "RAW", "CLASSIC", "USER" };
        return c;
    }
    inline juce::String categoryOf (const juce::String& name)
    {
        const auto pre = name.upToFirstOccurrenceOf (" - ", false, false).trim().toUpperCase();
        return name.contains (" - ") && categories().contains (pre) ? pre : juce::String ("USER");
    }
    inline juce::String displayName (const juce::String& name) { return name.contains (" - ") ? name.fromFirstOccurrenceOf (" - ", false, false) : name; }

    struct Entry
    {
        juce::String name, category, author;
        bool factory = true; int index = -1; juce::File file;
        juce::String key() const { return name; }          // favourites are stored by name (like the preset bar)
    };

    class Library
    {
    public:
        // factoryNames: the processor's programs; folder: the user presets; favFile: the preset bar's favourites file
        Library (juce::StringArray factoryNames, juce::File folder, juce::String ext, juce::File favFile)
            : factory (std::move (factoryNames)), dir (std::move (folder)), extension (std::move (ext)), favFile (std::move (favFile)) { rescan(); }

        void rescan()
        {
            all.clear();
            for (int i = 0; i < factory.size(); ++i) all.push_back ({ factory[i], factory[i].contains (" - ") ? categoryOf (factory[i]) : juce::String ("INIT"), "sonicsystems", true, i, {} });
            for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*." + extension))
            {
                Entry e; e.name = f.getFileNameWithoutExtension(); e.category = categoryOf (e.name); e.factory = false; e.file = f;
                if (auto x = juce::XmlDocument::parse (f)) e.author = x->getStringAttribute ("author");
                all.push_back (e);
            }
            std::stable_sort (all.begin() + factory.size(), all.end(), [] (const Entry& a, const Entry& b) { return a.name.compareIgnoreCase (b.name) < 0; });
            favs.clear(); favs.addLines (favFile.loadFileAsString()); favs.removeEmptyStrings();
        }

        const std::vector<Entry>& entries() const { return all; }
        bool isFavourite (const Entry& e) const { return favs.contains (e.key()); }
        void toggleFavourite (const Entry& e)
        {
            if (favs.contains (e.key())) favs.removeString (e.key()); else favs.add (e.key());
            favFile.getParentDirectory().createDirectory();
            favFile.replaceWithText (favs.joinIntoString ("\n"));
        }

        // filter: "ALL", "FAVOURITES", "USER" or a category; search in name / category / author
        std::vector<int> filter (const juce::String& cat, const juce::String& search) const
        {
            std::vector<int> v;
            for (int i = 0; i < (int) all.size(); ++i)
            {
                const auto& e = all[(size_t) i];
                if (cat == "FAVOURITES" && ! isFavourite (e)) continue;
                if (cat == "USER" && e.factory) continue;
                if (cat != "ALL" && cat != "FAVOURITES" && cat != "USER" && e.category != cat) continue;
                if (search.isNotEmpty() && ! (e.name + " " + e.category + " " + e.author).containsIgnoreCase (search)) continue;
                v.push_back (i);
            }
            return v;
        }

        // ---- file actions (user presets only)
        static bool setAuthorAndStrip (const juce::File& f, const juce::String& author)
        {
            auto x = juce::XmlDocument::parse (f);
            if (x == nullptr) return false;
            if (author.isNotEmpty()) x->setAttribute ("author", author);
            if (auto* m = x->getChildByName ("MELODY")) x->removeChildElement (m, true);    // a sound preset never carries the melody
            return x->writeTo (f);
        }
        bool rename (const Entry& e, const juce::String& newName)
        {
            if (e.factory || newName.trim().isEmpty()) return false;
            const auto target = dir.getChildFile (juce::File::createLegalFileName (newName.trim()) + "." + extension);
            if (target.exists()) return false;
            const bool wasFav = favs.contains (e.key());
            if (! e.file.moveFileTo (target)) return false;
            if (wasFav) { toggleFavourite (e); Entry n = e; n.name = target.getFileNameWithoutExtension(); toggleFavourite (n); }
            rescan(); return true;
        }
        bool remove (const Entry& e)
        {
            if (e.factory) return false;
            const bool ok = e.file.moveToTrash() || e.file.deleteFile();
            if (favs.contains (e.key())) toggleFavourite (e);
            rescan(); return ok;
        }
        bool duplicate (const Entry& e)
        {
            if (e.factory) return false;
            auto target = e.file.getNonexistentSibling (false);
            const bool ok = e.file.copyFileTo (target);
            rescan(); return ok;
        }
    private:
        juce::StringArray factory; juce::File dir; juce::String extension; juce::File favFile;
        std::vector<Entry> all; juce::StringArray favs;
    };
}
