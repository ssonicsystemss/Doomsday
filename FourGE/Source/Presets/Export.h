#pragma once
// =====================================================================
//  4GE module 12 - EXPORT files (no DSP, no GUI): the export SETTINGS (state child EXPORT - saved with the project), the FILE NAME
//  generator (template tokens), safe WRITING (a ".part" file, renamed only after a complete, verified write; free space and the
//  folder are checked first; a cancel / error deletes the temporary file), the EXPORT HISTORY (4GE/export_history.json) and the
//  background EXPORTER thread that runs single and BATCH jobs (render = DSP/Export/Render.h, finish = DSP/Export/Finish.h).
// =====================================================================
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include "../DSP/Export/Render.h"
#include "../DSP/Export/Finish.h"
#include <atomic>
#include <deque>
#include <mutex>

namespace fge::xp
{
    using namespace fge::dsp::xport;
    namespace dsp = fge::dsp;
    enum Format { FmtWav = 0, FmtAiff, FmtFlac };
    enum SeedMode { SeedPreset = 0, SeedFixed, SeedNew, SeedCustom };
    inline const juce::StringArray& formatNames() { static const juce::StringArray a { "WAV", "AIFF", "FLAC" }; return a; }
    inline const juce::StringArray& bitNames()    { static const juce::StringArray a { "16-BIT PCM", "24-BIT PCM", "32-BIT FLOAT" }; return a; }
    inline const juce::StringArray& rateNames()   { static const juce::StringArray a { "HOST RATE", "44.1 kHz", "48 kHz", "88.2 kHz", "96 kHz", "176.4 kHz", "192 kHz" }; return a; }
    inline double rateOf (int i, double host) { static const double r[7] { 0, 44100, 48000, 88200, 96000, 176400, 192000 }; return i <= 0 ? (host > 0 ? host : 44100.0) : r[juce::jlimit (1, 6, i)]; }
    inline int bitsOf (int i) { return i == 0 ? 16 : i == 1 ? 24 : 32; }
    inline const juce::StringArray& seedNames()   { static const juce::StringArray a { "PRESET SEED", "FIXED SEED (0)", "NEW SEED", "CUSTOM SEED" }; return a; }
    inline const juce::StringArray& lengthNames() { static const juce::StringArray a { "AUTO", "FIXED", "UNTIL SILENCE" }; return a; }
    inline const juce::StringArray& normNames()   { static const juce::StringArray a { "OFF", "PEAK", "LUFS" }; return a; }
    inline const juce::StringArray& ditherNames() { static const juce::StringArray a { "OFF", "TPDF", "NOISE SHAPING" }; return a; }
    inline const juce::StringArray& sourceNames()
    {
        static const juce::StringArray a = [] { juce::StringArray x { "FULL KICK", "PUNCH", "BODY", "TAIL", "SUB", "TEXTURE", "CUSTOM GROUP" };
                                                for (int i = 1; i <= 8; ++i) x.add ("LAYER " + juce::String (i)); for (int i = 1; i <= 8; ++i) x.add ("SAMPLE " + juce::String (i)); return x; }();
        return a;
    }
    // source list index -> (Source, index)
    inline std::pair<int, int> sourceOf (int i) { return i < 7 ? std::pair<int, int> { i, 0 } : i < 15 ? std::pair<int, int> { SrcLayer, i - 7 } : std::pair<int, int> { SrcSample, i - 15 }; }

    struct Settings
    {
        int source = 0, lengthMode = LenAuto, format = FmtWav, bits = 1, rate = 0, dither = DitherOff, norm = NormOff, seedMode = SeedPreset;
        float fixedMs = 1000.f, preRollMs = 0.f, postRollMs = 20.f, silenceDb = -80.f, minSilenceMs = 60.f, maxMs = 6000.f;
        float peakTarget = -0.3f, lufsTarget = -8.f, tpCeiling = -0.3f;
        bool trim = false, replaceSharp = true, overwrite = false;
        juce::String folder, nameTemplate { "4GE_{STYLE}_{PRESET}_{NOTE}_{BPM}BPM_{GROUP}_V{VERSION}" };
        uint32_t customSeed = 1;
        // BATCH
        uint32_t batchSources = 0b1111;                                    // FULL / PUNCH / BODY / TAIL (bits = source list 0..6)
        int batchPresets = 0, batchVariants = 1;                           // 0 = the current sound, 1 = every factory preset

        juce::File folderFile() const { return folder.isNotEmpty() ? juce::File (folder) : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("4GE_EXPORT"); }
        void toTree (juce::ValueTree t) const
        {
            auto set = [&t] (const char* k, const juce::var& v) { t.setProperty (k, v, nullptr); };
            set ("source", source); set ("len", lengthMode); set ("fmt", format); set ("bits", bits); set ("rate", rate); set ("dither", dither); set ("norm", norm);
            set ("seedMode", seedMode); set ("fixedMs", fixedMs); set ("pre", preRollMs); set ("post", postRollMs); set ("silDb", silenceDb); set ("minSil", minSilenceMs);
            set ("maxMs", maxMs); set ("peakT", peakTarget); set ("lufsT", lufsTarget); set ("tpC", tpCeiling); set ("trim", trim); set ("sharp", replaceSharp);
            set ("over", overwrite); set ("folder", folder); set ("tmpl", nameTemplate); set ("cseed", (int) customSeed); set ("bSrc", (int) batchSources);
            set ("bPre", batchPresets); set ("bVar", batchVariants);
        }
        static Settings fromTree (const juce::ValueTree& t)
        {
            Settings s;
            if (! t.isValid()) return s;
            auto gi = [&t] (const char* k, int d) { return (int) t.getProperty (k, d); };
            auto gf = [&t] (const char* k, float d) { return (float) (double) t.getProperty (k, d); };
            s.source = gi ("source", s.source); s.lengthMode = gi ("len", s.lengthMode); s.format = gi ("fmt", s.format); s.bits = gi ("bits", s.bits);
            s.rate = gi ("rate", s.rate); s.dither = gi ("dither", s.dither); s.norm = gi ("norm", s.norm); s.seedMode = gi ("seedMode", s.seedMode);
            s.fixedMs = gf ("fixedMs", s.fixedMs); s.preRollMs = gf ("pre", s.preRollMs); s.postRollMs = gf ("post", s.postRollMs); s.silenceDb = gf ("silDb", s.silenceDb);
            s.minSilenceMs = gf ("minSil", s.minSilenceMs); s.maxMs = gf ("maxMs", s.maxMs); s.peakTarget = gf ("peakT", s.peakTarget); s.lufsTarget = gf ("lufsT", s.lufsTarget);
            s.tpCeiling = gf ("tpC", s.tpCeiling); s.trim = (bool) t.getProperty ("trim", s.trim); s.replaceSharp = (bool) t.getProperty ("sharp", s.replaceSharp);
            s.overwrite = (bool) t.getProperty ("over", s.overwrite); s.folder = t.getProperty ("folder", s.folder).toString(); s.nameTemplate = t.getProperty ("tmpl", s.nameTemplate).toString();
            s.customSeed = (uint32_t) gi ("cseed", (int) s.customSeed); s.batchSources = (uint32_t) gi ("bSrc", (int) s.batchSources);
            s.batchPresets = gi ("bPre", s.batchPresets); s.batchVariants = juce::jlimit (1, 16, gi ("bVar", s.batchVariants));
            return s;
        }
    };

    // ---- the file name: {PRESET} {STYLE} {NOTE} {BPM} {GROUP} {VARIANT} {VERSION} {SEED} {DATE}
    inline juce::String noteName (int n, bool replaceSharp)
    {
        static const char* names[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        juce::String s = juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
        return replaceSharp ? s.replace ("#", "s") : s;
    }
    inline juce::String styleOf (const juce::String& preset) { return preset.containsChar ('-') ? preset.upToFirstOccurrenceOf ("-", false, false).trim() : juce::String ("KICK"); }
    struct NameParts { juce::String preset, group, style; int note = 31, variant = 1, version = 1; double bpm = 150.0; uint32_t seed = 0; };
    inline juce::String makeName (const juce::String& tmpl, const NameParts& p, bool replaceSharp)
    {
        juce::String presetPart = p.preset.contains (" - ") ? p.preset.fromFirstOccurrenceOf (" - ", false, false) : p.preset;
        juce::String n = tmpl.isEmpty() ? juce::String ("4GE_{PRESET}_{GROUP}") : tmpl;
        n = n.replace ("{PRESET}", presetPart.trim().replaceCharacter (' ', '_')).replace ("{STYLE}", (p.style.isNotEmpty() ? p.style : styleOf (p.preset)).replaceCharacter (' ', '_'))
             .replace ("{NOTE}", noteName (p.note, replaceSharp)).replace ("{BPM}", juce::String (juce::roundToInt (p.bpm))).replace ("{GROUP}", p.group.replaceCharacter (' ', '_'))
             .replace ("{VARIANT}", juce::String (p.variant).paddedLeft ('0', 2)).replace ("{VERSION}", juce::String (p.version).paddedLeft ('0', 2))
             .replace ("{SEED}", juce::String ((juce::int64) p.seed)).replace ("{DATE}", juce::Time::getCurrentTime().formatted ("%Y%m%d"));
        if (replaceSharp) n = n.replace ("#", "s");
        return juce::File::createLegalFileName (n);
    }
    inline const char* extOf (int fmt) { return fmt == FmtAiff ? ".aif" : fmt == FmtFlac ? ".flac" : ".wav"; }

    // ---- writing (any thread but the audio thread): ".part" first, renamed after a complete write; never a broken "finished" file
    struct WriteResult { bool ok = false; juce::String error; juce::File file; };
    inline int64_t testFreeBytes = -1;                                      // (tests: pretend the disk has only this much space)
    inline WriteResult writeFile (const juce::AudioBuffer<float>& b, double rate, int format, int bits, const juce::File& target, bool overwrite,
                                  const std::atomic<bool>* cancel = nullptr)
    {
        WriteResult r; r.file = target;
        const juce::File dir = target.getParentDirectory();
        if (target.getFileName().isEmpty() || ! target.isAChildOf (dir)) { r.error = "invalid file name"; return r; }
        if (dir.existsAsFile()) { r.error = "the destination is a file, not a folder: " + dir.getFullPathName(); return r; }
        if (! dir.createDirectory()) { r.error = "cannot create the folder (no permission?): " + dir.getFullPathName(); return r; }
        if (! dir.hasWriteAccess()) { r.error = "no permission to write into " + dir.getFullPathName(); return r; }
        if (target.exists() && ! overwrite) { r.error = "the file exists (OVERWRITE is off): " + target.getFileName(); return r; }
        const int64_t need = (int64_t) b.getNumSamples() * b.getNumChannels() * (bits / 8) + 65536;
        const int64_t freeB = testFreeBytes >= 0 ? testFreeBytes : dir.getBytesFreeOnVolume();
        if (freeB >= 0 && freeB < need) { r.error = "not enough disk space (" + juce::File::descriptionOfSizeInBytes (need) + " needed)"; return r; }
        std::unique_ptr<juce::AudioFormat> fmt;
        if (format == FmtAiff) fmt = std::make_unique<juce::AiffAudioFormat>();
        else if (format == FmtFlac) fmt = std::make_unique<juce::FlacAudioFormat>();
        else fmt = std::make_unique<juce::WavAudioFormat>();
        if (format == FmtFlac && bits == 32) { r.error = "FLAC has no 32-bit float - choose 16 / 24-bit"; return r; }
        const juce::File part = target.getSiblingFile (target.getFileName() + ".part");
        part.deleteFile();
        bool ok = false;
        {
            auto stream = std::make_unique<juce::FileOutputStream> (part);
            if (! stream->openedOk()) { r.error = "cannot write " + part.getFullPathName(); return r; }
            const auto opts = juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (b.getNumChannels()).withBitsPerSample (bits)
                                  .withSampleFormat (bits == 32 ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint : juce::AudioFormatWriterOptions::SampleFormat::integral);
            std::unique_ptr<juce::OutputStream> os (stream.release());
            auto w = fmt->createWriterFor (os, opts);
            if (w == nullptr) { part.deleteFile(); r.error = "this format / bit depth / rate combination cannot be written"; return r; }
            ok = true;
            const int chunk = 32768;
            for (int pos = 0; pos < b.getNumSamples() && ok; pos += chunk)
            {
                if (cancel != nullptr && cancel->load()) { ok = false; r.error = "cancelled"; break; }
                ok = w->writeFromAudioSampleBuffer (b, pos, juce::jmin (chunk, b.getNumSamples() - pos));
                if (! ok) r.error = "write error (disk full?)";
            }
            if (ok) w->flush();                                              // (not every writer supports flush - the read-back below checks)
        }
        if (! ok) { part.deleteFile(); if (r.error.isEmpty()) r.error = "write error"; return r; }
        // verify: the file reads back with the expected length, then it gets its real name
        {
            std::unique_ptr<juce::AudioFormatReader> rd (fmt->createReaderFor (new juce::FileInputStream (part), true));
            if (rd == nullptr || rd->lengthInSamples != b.getNumSamples()) { part.deleteFile(); r.error = "the written file does not read back correctly"; return r; }
        }
        if (target.exists()) target.deleteFile();
        if (! part.moveFileTo (target)) { part.deleteFile(); r.error = "cannot rename the finished file"; return r; }
        r.ok = true;
        return r;
    }

    // ---- EXPORT HISTORY (the last 50 exports, newest first)
    struct HistoryEntry
    {
        juce::String file, preset, format; juce::Time when; int bits = 24; double rate = 44100.0, seconds = 0.0; float peak = 0, truePeak = 0, lufs = -70; bool lufsValid = false;
        uint32_t seed = 0; juce::var settings;                                  // (the settings + source, for RE-EXPORT)
    };
    class History
    {
    public:
        explicit History (juce::File f = {}) : file (f == juce::File() ? juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("4GE").getChildFile ("export_history.json") : f) { load(); }
        void add (const HistoryEntry& e) { std::lock_guard<std::mutex> l (lock); items.push_front (e); while (items.size() > 50) items.pop_back(); save(); }
        std::vector<HistoryEntry> list() const { std::lock_guard<std::mutex> l (lock); return { items.begin(), items.end() }; }
        void clear() { std::lock_guard<std::mutex> l (lock); items.clear(); save(); }
    private:
        void load()
        {
            const auto v = juce::JSON::parse (file);
            if (auto* a = v.getArray())
                for (auto& o : *a)
                {
                    HistoryEntry e; e.file = o["file"]; e.preset = o["preset"]; e.format = o["format"]; e.when = juce::Time ((juce::int64) o["when"]);
                    e.bits = o["bits"]; e.rate = o["rate"]; e.seconds = o["seconds"]; e.peak = o["peak"]; e.truePeak = o["tp"]; e.lufs = o["lufs"]; e.lufsValid = o["lufsOk"];
                    e.seed = (uint32_t) (juce::int64) o["seed"]; e.settings = o["settings"]; items.push_back (e);
                }
        }
        void save() const
        {
            juce::Array<juce::var> a;
            for (auto& e : items)
            {
                auto* o = new juce::DynamicObject();
                o->setProperty ("file", e.file); o->setProperty ("preset", e.preset); o->setProperty ("format", e.format); o->setProperty ("when", e.when.toMilliseconds());
                o->setProperty ("bits", e.bits); o->setProperty ("rate", e.rate); o->setProperty ("seconds", e.seconds); o->setProperty ("peak", e.peak); o->setProperty ("tp", e.truePeak);
                o->setProperty ("lufs", e.lufs); o->setProperty ("lufsOk", e.lufsValid); o->setProperty ("seed", (juce::int64) e.seed); o->setProperty ("settings", e.settings);
                a.add (juce::var (o));
            }
            file.getParentDirectory().createDirectory();
            file.replaceWithText (juce::JSON::toString (juce::var (a)));
        }
        juce::File file;
        mutable std::mutex lock;
        std::deque<HistoryEntry> items;
    };

    // ---- one job = one file
    struct Job
    {
        Snapshot snap; Spec spec; Settings set;
        juce::File target;
        juce::String presetName, sourceLabel;
    };
    struct JobResult
    {
        bool ok = false, cancelled = false; juce::String error, note; juce::File file;
        dsp::meter::Measure m; NormResult norm; double rate = 44100.0; int bits = 24, channels = 2; uint32_t seed = 0;
    };
    // the render + finish of one job (also used by PREVIEW without writing)
    inline JobResult process (const Job& j, const std::atomic<bool>* cancel, std::atomic<float>* progress, bool write, juce::AudioBuffer<float>* keepAudio = nullptr)
    {
        JobResult res; res.rate = j.spec.rate; res.bits = bitsOf (j.set.bits); res.seed = j.spec.seed;
        auto rr = render (j.snap, j.spec, cancel, progress);
        if (rr.cancelled) { res.cancelled = true; res.error = "cancelled"; return res; }
        auto& b = rr.audio;
        if (b.getNumSamples() == 0) { res.error = "the render is empty"; return res; }
        if (j.snap.f.idx (P::masterOutMode) == 1) toMono (b);              // OUTPUT MODE: MONO -> a 1-channel file
        res.channels = b.getNumChannels();
        const auto before = dsp::meter::measure (b, j.spec.rate);
        res.norm = planNormalize (before, j.set.norm, j.set.norm == NormLufs ? j.set.lufsTarget : j.set.peakTarget, j.set.tpCeiling);
        applyGain (b, res.norm.gainDb);
        dither (b, res.bits, j.set.dither, j.spec.seed ^ 0xD17Eu);
        res.m = dsp::meter::measure (b, j.spec.rate);
        res.note = res.norm.note;
        if (res.bits < 32 && res.m.samplePeakDb > 0.f)                     // integer files cannot hold more than 0 dBFS: say it (nothing is added)
            res.note << (res.note.isEmpty() ? "" : " | ") << "the kick goes " << juce::String (res.m.samplePeakDb, 2) << " dB over 0 dBFS - an integer file clips it: use 32-BIT FLOAT, PEAK normalization or the SAFETY LIMITER";
        if (keepAudio != nullptr) *keepAudio = b;
        if (! write) { res.ok = true; return res; }
        const auto w = writeFile (b, j.spec.rate, j.set.format, res.bits, j.target, j.set.overwrite, cancel);
        res.ok = w.ok; res.error = w.error; res.file = w.file;
        if (! w.ok && w.error == "cancelled") res.cancelled = true;
        return res;
    }

    // ---- the background EXPORTER: a queue of jobs (one EXPORT = 1 job, BATCH = many), progress, CANCEL
    class Exporter : private juce::Thread
    {
    public:
        explicit Exporter (History& h) : juce::Thread ("4GE export"), history (h) {}
        ~Exporter() override { cancel(); stopThread (10000); }
        // start a list of jobs; 'onFinished' runs on the message thread with every result
        bool start (std::vector<Job> jobs, std::function<void (const std::vector<JobResult>&)> onFinished)
        {
            if (isThreadRunning()) return false;
            queue = std::move (jobs); done = onFinished; results.clear(); cancelFlag.store (false); progress.store (0.f); current.store (0); total.store ((int) queue.size());
            startThread (juce::Thread::Priority::normal);
            return true;
        }
        void cancel() { cancelFlag.store (true); }
        bool busy() const { return isThreadRunning(); }
        void waitDone (int ms = 120000) { stopThread (ms); }
        std::atomic<float> progress { 0.f };
        std::atomic<int> current { 0 }, total { 0 };
        std::vector<JobResult> lastResults() const { std::lock_guard<std::mutex> l (lock); return results; }
    private:
        void run() override
        {
            for (size_t i = 0; i < queue.size() && ! cancelFlag.load(); ++i)
            {
                current.store ((int) i);
                std::atomic<float> one { 0.f };
                auto r = process (queue[i], &cancelFlag, &one, true);
                progress.store (((float) i + 1.f) / (float) queue.size());
                if (r.ok)
                {
                    HistoryEntry e; e.file = r.file.getFullPathName(); e.preset = queue[i].presetName; e.format = formatNames()[queue[i].set.format];
                    e.when = juce::Time::getCurrentTime(); e.bits = r.bits; e.rate = r.rate; e.seconds = r.m.seconds; e.peak = r.m.samplePeakDb; e.truePeak = r.m.truePeakDb;
                    e.lufs = r.m.lufs; e.lufsValid = r.m.lufsValid; e.seed = r.seed;
                    juce::ValueTree t ("X"); queue[i].set.toTree (t); t.setProperty ("srcIdx", queue[i].spec.source, nullptr); t.setProperty ("srcN", queue[i].spec.index, nullptr);
                    t.setProperty ("seed", (juce::int64) r.seed, nullptr); t.setProperty ("note", queue[i].spec.note, nullptr); t.setProperty ("bpm", queue[i].spec.bpm, nullptr);
                    if (auto x = t.createXml()) e.settings = x->toString (juce::XmlElement::TextFormat().singleLine());
                    history.add (e);
                }
                std::lock_guard<std::mutex> l (lock); results.push_back (r);
            }
            if (cancelFlag.load()) { std::lock_guard<std::mutex> l (lock); JobResult c; c.cancelled = true; c.error = "cancelled"; if (results.empty() || ! results.back().cancelled) results.push_back (c); }
            auto cb = done; auto res = lastResults();
            auto life = alive;
            if (cb) juce::MessageManager::callAsync ([cb, res, life] { if (*life) cb (res); });
        }
        History& history;
        std::vector<Job> queue;
        std::function<void (const std::vector<JobResult>&)> done;
        std::vector<JobResult> results;
        mutable std::mutex lock;
        std::atomic<bool> cancelFlag { false };
        std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    public:
        void detach() { *alive = false; }
    };
}
