#pragma once
// =====================================================================
//  4GE module 15 - PRESET LIBRARY (files / metadata only: no DSP, no GUI)
//   Every preset (FACTORY + USER .4gp files) with its METADATA: category (name prefix), STYLE, LENGTH (ms + SHORT / MEDIUM / LONG /
//   EXTRA LONG), TAIL type, automatic CHARACTER tags (CLEAN / DIRTY / HEAVY, PUNCHY, SCREECH, LAYERS, SAMPLES, STEREO, MODULATED,
//   SYNCED, NOTE CUT) + the user's own TAGS and NOTES (root attributes of the .4gp), the MODULES it uses, MISSING SAMPLES.
//   File actions: duplicate, rename, delete (to the trash), import (validated, never overwrites), export. RECENTLY USED in
//   4GE/presets.settings. The A/B slots live in the processor; preview renders run on a background thread (GUI).
// =====================================================================
#include "../PluginProcessor.h"
#include "FactoryPresets.h"

namespace fge::lib
{
    enum Source { SrcFactory = 0, SrcUser };
    struct Entry
    {
        juce::String name, category, notes;
        int source = SrcFactory, factory = -1, style = -1, lengthCat = 1;
        float lengthMs = 450.f;
        juce::File file;
        juce::StringArray tags, userTags, modules, missing;   // tags = auto + user; modules = what it uses; missing = sample files not found
        juce::String key() const { return source == SrcFactory ? "F:" + name : "U:" + file.getFullPathName(); }
    };
    inline const juce::StringArray& styleNames() { static const juce::StringArray a { "EARLY", "MODERN", "RAW", "EXPERIMENTAL", "EUPHORIC" }; return a; }
    inline const juce::StringArray& lengthNames() { static const juce::StringArray a { "SHORT", "MEDIUM", "LONG", "EXTRA LONG" }; return a; }
    inline int lengthCategory (float ms) { return ms < 250.f ? 0 : ms <= 500.f ? 1 : ms <= 1000.f ? 2 : 3; }
    inline int styleFromName (const juce::String& n)
    {
        for (int s = 0; s < styleNames().size(); ++s) if (n.startsWithIgnoreCase (styleNames()[s])) return s;
        return n.startsWithIgnoreCase ("CLASSIC") ? 0 : -1;
    }
    inline int indexOf (const juce::String& id) { for (int i = 0; i < P::count; ++i) if (id == def (i).id) return i; return -1; }

    // ---- what a preset is (from its parameter values + a few state children)
    struct Extra { bool modMatrix = false; int samplesEmbedded = 0; juce::StringArray missing; };
    inline void describe (Entry& e, const ParamFrame& f, const Extra& x)
    {
        e.lengthMs = kickLengthMs (f, 150.0);
        e.lengthCat = lengthCategory (e.lengthMs);
        juce::StringArray t;
        if (e.style >= 0) t.add (styleNames()[e.style]);
        t.add (lengthNames()[e.lengthCat]);
        t.add ("TAIL " + def (P::tailChar).choices()[f.idx (P::tailChar)]);
        int dist = 0; for (int i = 0; i < 16; ++i) dist += f.on (P::dist1On + i * (P::dist2On - P::dist1On)) ? 1 : 0;
        int layers = 0; for (int i = 0; i < 8; ++i) layers += f.on (P::layer1On + i * (P::layer2On - P::layer1On)) ? 1 : 0;
        int smp = 0; for (int i = 0; i < 8; ++i) smp += f.on (P::smp1On + i * (P::smp2On - P::smp1On)) ? 1 : 0;
        int stereo = 0; for (int i = 0; i < 7; ++i) stereo += f.on (P::spcPunchOn + i * (P::spcBodyOn - P::spcPunchOn)) ? 1 : 0;
        const float drive = f[P::tailDrive];
        t.add (dist >= 2 || drive > 0.9f || f[P::clipDrive] > 6.f ? "HEAVY" : dist == 1 || drive > 0.55f ? "DIRTY" : "CLEAN");
        if (f[P::punchLevel] > -6.f) t.add ("PUNCHY");
        if (f.on (P::scrOn)) t.add ("SCREECH");
        if (layers > 0) t.add ("LAYERS");
        if (smp > 0) t.add ("SAMPLES");
        if (stereo > 0 || f[P::tailWidth] > 0.5f) t.add ("STEREO");
        if (x.modMatrix) t.add ("MODULATED");
        if (f.idx (P::lenSync) > 0) t.add ("SYNCED");
        if (f.idx (P::coreNoteCut) > 0) t.add ("NOTE CUT");
        if (! x.missing.isEmpty()) t.add ("MISSING SAMPLES");
        for (auto& u : e.userTags) t.addIfNotAlreadyThere (u.toUpperCase());
        e.tags = t;
        e.missing = x.missing;
        juce::StringArray m;
        m.add ("STYLE: " + (e.style >= 0 ? styleNames()[e.style] : juce::String ("-")) + "   LENGTH: " + lengthNames()[e.lengthCat] + " " + juce::String (juce::roundToInt (e.lengthMs)) + " ms"
               + (f.idx (P::lenSync) > 0 ? "  (SYNC " + def (P::lenSync).choices()[f.idx (P::lenSync)] + ")" : juce::String()));
        m.add ("CORE: punch " + def (P::punchSrc).choices()[f.idx (P::punchSrc)] + ", body " + def (P::bodySrc).choices()[f.idx (P::bodySrc)] + ", tail "
               + def (P::tailChar).choices()[f.idx (P::tailChar)] + " / " + def (P::tailSrc).choices()[f.idx (P::tailSrc)] + ", pitch " + juce::String (juce::roundToInt (f[P::peStart])) + " Hz");
        m.add ("DISTORTION: " + (dist > 0 ? juce::String (dist) + " stage(s)" : juce::String ("tail drive only")) + (f[P::clipDrive] > 0.05f ? ", master clip " + juce::String (f[P::clipDrive], 1) + " dB" : juce::String()));
        if (layers > 0) m.add ("LAYERS: " + juce::String (layers));
        if (smp > 0) m.add ("SAMPLES: " + juce::String (smp) + " slot(s)" + (x.samplesEmbedded > 0 ? ", " + juce::String (x.samplesEmbedded) + " inside the preset" : juce::String()));
        if (f.on (P::scrOn)) m.add ("SCREECH: on");
        if (stereo > 0) m.add ("STEREO UNITS: " + juce::String (stereo));
        if (x.modMatrix) m.add ("MODULATION: own matrix");
        if (! x.missing.isEmpty()) m.add ("MISSING: " + x.missing.joinIntoString (", ") + "  (RELINK on the SMP page)");
        e.modules = m;
    }

    inline Entry factoryEntry (int k)
    {
        const auto& fp = factoryPresets()[(size_t) k];
        Entry e; e.name = fp.name; e.source = SrcFactory; e.factory = k;
        e.category = fp.name.containsChar ('-') ? fp.name.upToFirstOccurrenceOf ("-", false, false).trim() : juce::String ("FACTORY");
        e.style = styleFromName (fp.name);
        ParamFrame f = ParamFrame::defaults();
        for (auto& [id, v] : fp.values) if (const int i = indexOf (id); i >= 0) f[i] = v;
        describe (e, f, {});
        return e;
    }
    inline bool userEntry (const juce::File& file, Entry& e)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("FOURGE")) return false;
        e = {}; e.source = SrcUser; e.file = file; e.name = file.getFileNameWithoutExtension();
        e.category = e.name.containsChar ('-') ? e.name.upToFirstOccurrenceOf ("-", false, false).trim() : juce::String ("USER");
        e.style = xml->hasAttribute ("style") ? xml->getIntAttribute ("style") : styleFromName (e.name);
        e.notes = xml->getStringAttribute ("notes");
        e.userTags.addTokens (xml->getStringAttribute ("tags"), ",", {}); e.userTags.trim(); e.userTags.removeEmptyStrings();
        ParamFrame f = ParamFrame::defaults();
        for (auto* c : xml->getChildWithTagNameIterator ("PARAM")) if (const int i = indexOf (c->getStringAttribute ("id")); i >= 0) f[i] = (float) c->getDoubleAttribute ("value");
        Extra x;
        if (auto* m = xml->getChildByName ("MODM")) x.modMatrix = m->getNumChildElements() > 0 || m->getNumAttributes() > 0;
        if (auto* sm = xml->getChildByName ("SAMPLES"))
            for (int k = 1; k <= 8; ++k)
            {
                const juce::String path = sm->getStringAttribute ("s" + juce::String (k) + "path"), data = sm->getStringAttribute ("s" + juce::String (k) + "data");
                if (data.isNotEmpty()) ++x.samplesEmbedded;
                else if (path.isNotEmpty() && ! juce::File (path).existsAsFile()) x.missing.add (juce::File (path).getFileName());
            }
        describe (e, f, x);
        return true;
    }

    // ---- the library (message thread)
    class Library
    {
    public:
        explicit Library (GeProcessor& p, juce::File settings = {}) : proc (p)
        {
            if (settings == juce::File()) settings = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("4GE").getChildFile ("presets.settings");
            settingsFile = settings;
            recent.addTokens (settingsFile.loadFileAsString(), "\n", {}); recent.removeEmptyStrings();
        }
        void scan()
        {
            items.clear();
            for (int k = 0; k < (int) factoryPresets().size(); ++k) items.push_back (factoryEntry (k));
            for (auto& f : proc.userPresetFiles()) { Entry e; if (userEntry (f, e)) items.push_back (e); }
        }
        const std::vector<Entry>& all() const { return items; }
        // filters: style -1 = all, length -1 = all, source 0 all / 1 factory / 2 user / 3 favorites / 4 recent; tag "" = all; search = words
        std::vector<int> filter (const juce::String& search, int style, int length, int source, const juce::String& tag, const std::function<bool (const Entry&)>& isFav) const
        {
            juce::StringArray words; words.addTokens (search.toLowerCase(), " ", {}); words.removeEmptyStrings();
            std::vector<int> out;
            for (int i = 0; i < (int) items.size(); ++i)
            {
                const auto& e = items[(size_t) i];
                if (style >= 0 && e.style != style) continue;
                if (length >= 0 && e.lengthCat != length) continue;
                if (source == 1 && e.source != SrcFactory) continue;
                if (source == 2 && e.source != SrcUser) continue;
                if (source == 3 && ! (isFav && isFav (e))) continue;
                if (source == 4 && ! recent.contains (e.key())) continue;
                if (tag.isNotEmpty() && ! e.tags.contains (tag)) continue;
                const juce::String hay = (e.name + " " + e.tags.joinIntoString (" ") + " " + e.notes).toLowerCase();
                bool ok = true; for (auto& w : words) ok &= hay.contains (w);
                if (ok) out.push_back (i);
            }
            if (source == 4) std::sort (out.begin(), out.end(), [this] (int a, int b) { return recent.indexOf (items[(size_t) a].key()) < recent.indexOf (items[(size_t) b].key()); });
            return out;
        }
        juce::StringArray allTags() const { juce::StringArray t; for (auto& e : items) for (auto& x : e.tags) t.addIfNotAlreadyThere (x); t.sort (true); return t; }
        // ---- actions
        bool load (const Entry& e)
        {
            bool ok = true;
            if (e.source == SrcFactory) proc.setCurrentProgram (e.factory);
            else ok = proc.loadUserPresetFile (e.file);
            if (ok) touchRecent (e.key());
            return ok;
        }
        void touchRecent (const juce::String& key)
        {
            recent.removeString (key); recent.insert (0, key);
            while (recent.size() > 30) recent.remove (recent.size() - 1);
            settingsFile.getParentDirectory().createDirectory(); settingsFile.replaceWithText (recent.joinIntoString ("\n"));
        }
        juce::File userFolder() const { return proc.userPresetFolder(); }
        juce::File freeName (const juce::String& wanted) const
        {
            const juce::String base = juce::File::createLegalFileName (wanted.trim().isEmpty() ? juce::String ("PRESET") : wanted.trim());
            juce::File f = userFolder().getChildFile (base + ".4gp");
            for (int n = 2; f.exists() && n < 1000; ++n) f = userFolder().getChildFile (base + " " + juce::String (n) + ".4gp");
            return f;
        }
        // DUPLICATE: a user copy (a factory preset becomes an editable user preset)
        juce::File duplicate (const Entry& e)
        {
            userFolder().createDirectory();
            const auto target = freeName (e.name + (e.source == SrcUser ? " COPY" : ""));
            if (e.source == SrcUser) return e.file.copyFileTo (target) ? target : juce::File();
            return writeFactory (e.factory, target) ? target : juce::File();
        }
        bool rename (const Entry& e, const juce::String& newName, juce::String* err = nullptr)
        {
            if (e.source != SrcUser) { if (err) *err = "factory presets cannot be renamed - DUPLICATE it first"; return false; }
            const juce::String clean = juce::File::createLegalFileName (newName.trim());
            if (clean.isEmpty()) { if (err) *err = "empty name"; return false; }
            const auto target = userFolder().getChildFile (clean + ".4gp");
            if (target.exists()) { if (err) *err = "a preset with this name exists"; return false; }
            if (! e.file.moveFileTo (target)) { if (err) *err = "cannot rename the file"; return false; }
            if (proc.currentPresetName() == e.name) proc.setPresetName (clean);
            return true;
        }
        bool remove (const Entry& e)
        {
            if (e.source != SrcUser) return false;
            return e.file.moveToTrash() || e.file.deleteFile();
        }
        // IMPORT: .4gp files (checked) copied into the user folder under a free name - nothing is overwritten
        int import (const juce::Array<juce::File>& files, juce::StringArray* problems = nullptr)
        {
            int n = 0; userFolder().createDirectory();
            for (auto& f : files)
            {
                auto xml = juce::XmlDocument::parse (f);
                if (xml == nullptr || ! xml->hasTagName ("FOURGE")) { if (problems) problems->add (f.getFileName() + ": not a 4GE preset"); continue; }
                const auto target = freeName (f.getFileNameWithoutExtension());
                if (f.copyFileTo (target)) ++n; else if (problems) problems->add (f.getFileName() + ": cannot copy");
            }
            return n;
        }
        // EXPORT: a copy anywhere (samples up to 20 s are inside the file; longer ones only as paths - said by the GUI)
        bool exportTo (const Entry& e, const juce::File& target)
        {
            if (e.source == SrcUser) return e.file.copyFileTo (target);
            return writeFactory (e.factory, target);
        }
        // the user's TAGS / NOTES written into the preset file
        bool setTags (const Entry& e, const juce::StringArray& tags, const juce::String& notes)
        {
            if (e.source != SrcUser) return false;
            auto xml = juce::XmlDocument::parse (e.file);
            if (xml == nullptr) return false;
            juce::StringArray t (tags); t.trim(); t.removeEmptyStrings(); t.removeDuplicates (true);
            if (t.isEmpty()) xml->removeAttribute ("tags"); else xml->setAttribute ("tags", t.joinIntoString (","));
            if (notes.isEmpty()) xml->removeAttribute ("notes"); else xml->setAttribute ("notes", notes);
            return xml->writeTo (e.file);
        }
        const juce::StringArray& recentKeys() const { return recent; }
    private:
        bool writeFactory (int k, const juce::File& target)
        {
            auto tmp = std::make_unique<GeProcessor>();
            tmp->setCurrentProgram (k);
            std::unique_ptr<juce::XmlElement> xml (tmp->apvts.copyState().createXml());
            if (xml == nullptr) return false;
            for (auto* a : { "program", "presetName", "touched" }) xml->removeAttribute (a);
            return xml->writeTo (target);
        }
        GeProcessor& proc;
        juce::File settingsFile;
        juce::StringArray recent;
        std::vector<Entry> items;
    };
}
