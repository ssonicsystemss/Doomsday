#pragma once
// =====================================================================
//  4GE module 10 - the SAMPLE BROWSER's files (no GUI, no DSP): one folder scanned on a background thread, search,
//  FAVORITES and RECENT kept in the user's settings file (4GE/browser.settings), never inside a preset.
// =====================================================================
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <memory>
#include <mutex>

namespace fge::smp
{
    class Browser
    {
    public:
        enum Mode { All = 0, Favorites, Recent };
        explicit Browser (juce::File settingsFile = {})
        {
            if (settingsFile == juce::File())
                settingsFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("4GE").getChildFile ("browser.settings");
            juce::PropertiesFile::Options o; o.applicationName = "4GE"; o.filenameSuffix = "settings"; o.storageFormat = juce::PropertiesFile::storeAsXML;
            settingsFile.getParentDirectory().createDirectory();
            props = std::make_unique<juce::PropertiesFile> (settingsFile, o);
            fav.addTokens (props->getValue ("favorites"), "\n", {}); fav.removeEmptyStrings();
            rec.addTokens (props->getValue ("recent"), "\n", {}); rec.removeEmptyStrings();
            dir = juce::File (props->getValue ("folder"));
        }
        ~Browser() { *alive = false; }
        static bool isAudio (const juce::File& f) { return f.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3"); }

        juce::File folder() const { return dir; }
        // scan a folder (3 levels deep, at most 4000 files) on a background thread; 'done' runs on the message thread
        void setFolder (const juce::File& f, std::function<void()> done = {})
        {
            dir = f; props->setValue ("folder", f.getFullPathName()); props->saveIfNeeded();
            auto life = alive;
            const auto gen = ++scanGen;
            juce::Thread::launch ([this, f, life, gen, done]
            {
                juce::Array<juce::File> out;
                std::function<void (const juce::File&, int)> walk = [&] (const juce::File& d, int depth)
                {
                    for (const auto& e : juce::RangedDirectoryIterator (d, false, "*", juce::File::findFilesAndDirectories))
                    {
                        if (out.size() >= 4000 || ! *life) return;
                        const auto& x = e.getFile();
                        if (e.isDirectory()) { if (depth < 3) walk (x, depth + 1); }
                        else if (isAudio (x)) out.add (x);
                    }
                };
                if (f.isDirectory()) walk (f, 0);
                out.sort();
                juce::MessageManager::callAsync ([this, life, gen, out, done]
                {
                    if (! *life || gen != scanGen.load()) return;
                    { std::lock_guard<std::mutex> l (lock); files = out; }
                    if (done) done();
                });
            });
        }
        // the visible list: mode + search (every word must be in the file name)
        juce::Array<juce::File> list (int mode, const juce::String& search) const
        {
            juce::Array<juce::File> src;
            if (mode == Favorites) for (auto& s : fav) src.add (juce::File (s));
            else if (mode == Recent) for (auto& s : rec) src.add (juce::File (s));
            else { std::lock_guard<std::mutex> l (lock); src = files; }
            juce::StringArray words; words.addTokens (search.toLowerCase(), " ", {}); words.removeEmptyStrings();
            juce::Array<juce::File> out;
            for (auto& f : src)
            {
                const auto n = f.getFileName().toLowerCase();
                bool okW = true; for (auto& w : words) okW &= n.contains (w);
                if (okW) out.add (f);
            }
            return out;
        }
        bool isFavorite (const juce::File& f) const { return fav.contains (f.getFullPathName()); }
        void toggleFavorite (const juce::File& f)
        {
            const auto p = f.getFullPathName();
            if (fav.contains (p)) fav.removeString (p); else fav.add (p);
            props->setValue ("favorites", fav.joinIntoString ("\n")); props->saveIfNeeded();
        }
        void addRecent (const juce::File& f)
        {
            const auto p = f.getFullPathName();
            rec.removeString (p); rec.insert (0, p);
            while (rec.size() > 30) rec.remove (rec.size() - 1);
            props->setValue ("recent", rec.joinIntoString ("\n")); props->saveIfNeeded();
        }
        int scanned() const { std::lock_guard<std::mutex> l (lock); return files.size(); }

    private:
        std::unique_ptr<juce::PropertiesFile> props;
        juce::StringArray fav, rec;
        juce::File dir;
        juce::Array<juce::File> files;
        mutable std::mutex lock;
        std::atomic<uint32_t> scanGen { 0 };
        std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    };
}
