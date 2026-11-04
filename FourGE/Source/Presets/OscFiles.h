#pragma once
// =====================================================================
//  4GE module 03 - FILES / PRESETS of the oscillators (never in the GUI, never on the audio thread):
//  factory oscillator presets, RESET, one oscillator / the whole set / one tab as a file (.4gosc / .4goscs / .4gtab),
//  WAV import -> a CUSTOM cycle or USER wavetable frames, single-cycle files (.4gwave), envelope files (.4genv).
//  Every change goes through the processor as ONE undo step.
// =====================================================================
#include <juce_audio_formats/juce_audio_formats.h>
#include "../PluginProcessor.h"
#include "OscPresets.h"

namespace fge::files
{
    inline juce::File folder (const char* sub)
    {
        auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("4GE").getChildFile (sub);
        d.createDirectory();
        return d;
    }
    inline juce::String oscId (int osc, const juce::String& suffix) { return "osc" + juce::String (osc + 1) + "_" + suffix; }
    inline juce::StringArray allOscSuffixes()
    {
        juce::StringArray s;
        const juce::String pre ("osc1_");
        for (int i = 0; i < P::count; ++i) { const juce::String id (def (i).id); if (id.startsWith (pre)) s.add (id.substring (pre.length())); }
        return s;
    }

    // ---- data keys of the OSCS child that belong to a tab (-1 = all)
    inline juce::StringArray dataKeys (int tab)
    {
        if (tab == TabWave)  return { "custom", "cycle" };
        if (tab == TabTable) return { "frames" };
        if (tab == TabHarm)  return { "hAmp", "hPhase", "hOn" };
        if (tab < 0)         return { "custom", "cycle", "frames", "hAmp", "hPhase", "hOn" };
        return {};
    }
    inline juce::ValueTree oscTree (GeProcessor& p, int osc)
    {
        return p.apvts.state.getChildWithName (GeProcessor::oscsTag).getChildWithName (juce::Identifier ("osc" + juce::String (osc + 1)));
    }

    // ---- one oscillator (tab < 0) or one tab -> XML
    inline std::unique_ptr<juce::XmlElement> oscToXml (GeProcessor& p, int osc, int tab)
    {
        auto x = std::make_unique<juce::XmlElement> (tab < 0 ? "OSC4GE" : "OSCTAB4GE");
        if (tab >= 0) x->setAttribute ("tab", oscTabName (tab));
        auto keys = tab < 0 ? allOscSuffixes() : oscTabParams (tab);
        for (auto& k : keys)
            if (auto* v = p.apvts.getRawParameterValue (oscId (osc, k))) { auto* e = x->createNewChildElement ("PARAM"); e->setAttribute ("id", k); e->setAttribute ("value", v->load()); }
        if (tab == TabFm)
            for (int a = 1; a <= 4; ++a) for (int b = 1; b <= 4; ++b)
                if (a != b) if (auto* v = p.apvts.getRawParameterValue ("fm_" + juce::String (a) + "_to_" + juce::String (b)))
                { auto* e = x->createNewChildElement ("FM"); e->setAttribute ("id", "fm_" + juce::String (a) + "_to_" + juce::String (b)); e->setAttribute ("value", v->load()); }
        auto t = oscTree (p, osc);
        auto* d = x->createNewChildElement ("DATA");
        for (auto& k : dataKeys (tab)) if (t.isValid() && t.hasProperty (k)) d->setAttribute (k, t.getProperty (k).toString());
        return x;
    }
    // XML -> the oscillator (only what the file holds; a whole-oscillator file first resets the oscillator). One undo step.
    inline bool xmlToOsc (GeProcessor& p, int osc, const juce::XmlElement& x)
    {
        const bool whole = x.hasTagName ("OSC4GE");
        if (! whole && ! x.hasTagName ("OSCTAB4GE")) return false;
        p.undo.beginNewTransaction ("Oscillator file");
        if (whole)
            for (auto& k : allOscSuffixes())
                if (auto* prm = p.apvts.getParameter (oscId (osc, k))) p.setParamInUndoStep (oscId (osc, k), prm->convertFrom0to1 (prm->getDefaultValue()));
        for (auto* e : x.getChildWithTagNameIterator ("PARAM")) p.setParamInUndoStep (oscId (osc, e->getStringAttribute ("id")), (float) e->getDoubleAttribute ("value"));
        for (auto* e : x.getChildWithTagNameIterator ("FM")) p.setParamInUndoStep (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));
        if (auto* d = x.getChildByName ("DATA"))
        {
            const int tab = whole ? -1 : [&] { for (int t = 0; t < NumOscTabs; ++t) if (x.getStringAttribute ("tab") == oscTabName (t)) return t; return -2; }();
            for (auto& k : dataKeys (tab)) p.setOscProperty (osc, k, d->getStringAttribute (k), true, false);
        }
        return true;
    }
    // ---- all four oscillators + the FM matrix
    inline std::unique_ptr<juce::XmlElement> setToXml (GeProcessor& p)
    {
        auto x = std::make_unique<juce::XmlElement> ("OSCSET4GE");
        for (int i = 0; i < 4; ++i) x->addChildElement (oscToXml (p, i, -1).release());
        x->addChildElement (oscToXml (p, 0, TabFm).release());
        return x;
    }
    inline bool xmlToSet (GeProcessor& p, const juce::XmlElement& x)
    {
        if (! x.hasTagName ("OSCSET4GE")) return false;
        int i = 0;
        for (auto* e : x.getChildWithTagNameIterator ("OSC4GE")) if (i < 4) xmlToOsc (p, i++, *e);
        if (auto* fm = x.getChildByName ("OSCTAB4GE")) for (auto* e : fm->getChildWithTagNameIterator ("FM")) p.setParamInUndoStep (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));
        return true;
    }
    inline bool writeXml (const juce::File& f, const juce::XmlElement& x) { return x.writeTo (f); }
    inline std::unique_ptr<juce::XmlElement> readXml (const juce::File& f) { return juce::XmlDocument::parse (f); }

    // ---- RESET and factory presets (one undo step; the oscillator stays ON)
    inline void resetOsc (GeProcessor& p, int osc, bool keepOn = true)
    {
        p.undo.beginNewTransaction ("Reset oscillator");
        for (auto& k : allOscSuffixes())
            if (auto* prm = p.apvts.getParameter (oscId (osc, k)))
                if (! (keepOn && k == "on")) p.setParamInUndoStep (oscId (osc, k), prm->convertFrom0to1 (prm->getDefaultValue()));
        for (auto& k : dataKeys (-1)) p.setOscProperty (osc, k, {}, true, false);
    }
    inline void applyPreset (GeProcessor& p, int osc, const OscPreset& pr)
    {
        resetOsc (p, osc);
        p.setParamInUndoStep (oscId (osc, "on"), 1.f);
        for (auto& [k, v] : pr.values) p.setParamInUndoStep (oscId (osc, k), v);
    }

    // ---- WAV import: the audio becomes a cycle SOURCE (never a kick sample)
    inline std::vector<float> readMono (const juce::File& f, double& rate)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (r == nullptr || r->lengthInSamples <= 0) return {};
        const int n = (int) std::min<long long> ((long long) r->lengthInSamples, 1LL << 20);
        juce::AudioBuffer<float> b ((int) r->numChannels, n);
        r->read (&b, 0, n, 0, true, true);
        std::vector<float> m ((size_t) n, 0.f);
        for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < n; ++i) m[(size_t) i] += b.getSample (c, i) / (float) b.getNumChannels();
        rate = r->sampleRate;
        return m;
    }
    // one period: autocorrelation over 30 Hz .. 2 kHz from the loudest part (a short file = the whole file is the cycle)
    inline int detectPeriod (const std::vector<float>& x, double rate)
    {
        const int n = (int) x.size();
        if (n <= 4096) return n;
        size_t loud = 0; float pk = 0.f; for (size_t i = 0; i < x.size(); ++i) if (std::abs (x[i]) > pk) { pk = std::abs (x[i]); loud = i; }
        const int start = juce::jlimit (0, n - 8192, (int) loud), lo = (int) (rate / 2000.0), hi = juce::jmin (4096, (int) (rate / 30.0));
        double best = -1.0e9; int lag = hi;
        for (int l = lo; l < hi; ++l)
        {
            double s = 0.0; for (int i = 0; i < 4096; ++i) s += (double) x[(size_t) (start + i)] * x[(size_t) (start + i + l)];
            if (s > best) { best = s; lag = l; }
        }
        return lag;
    }
    inline bool importCycle (GeProcessor& p, int osc, const juce::File& f)
    {
        double rate = 44100.0;
        auto x = readMono (f, rate);
        if (x.size() < 8) return false;
        const int per = detectPeriod (x, rate);
        auto frames = dsp::tables::framesFromAudio (x.data(), (int) x.size(), per, 1);
        if (frames.empty()) return false;
        p.setOscProperty (osc, "cycle", dsp::oscstate::floatsToText (frames[0].data(), frames[0].size()));
        p.setParamInUndoStep (oscId (osc, "wave"), (float) dsp::WCustom);
        return true;
    }
    inline bool importFrames (GeProcessor& p, int osc, const juce::File& f, int cycleLen)
    {
        double rate = 44100.0;
        auto x = readMono (f, rate);
        if (x.size() < 8) return false;
        auto frames = dsp::tables::framesFromAudio (x.data(), (int) x.size(), cycleLen, 64);
        if (frames.empty()) return false;
        p.setOscProperty (osc, "frames", dsp::oscstate::framesText (frames));
        p.setParamInUndoStep (oscId (osc, "wave"), (float) dsp::WWavetable);
        p.setParamInUndoStep (oscId (osc, "wt_table"), 5.f);
        return true;
    }
    // ---- single shapes as text files (.4genv envelopes, .4gwave drawn cycles)
    inline bool writeShape (const juce::File& f, const env::Shape& s) { return f.replaceWithText ("4GE-ENV " + s.toString()); }
    inline bool readShape (const juce::File& f, env::Shape& out, float vMin)
    {
        const auto t = f.loadFileAsString().trim();
        return t.startsWith ("4GE-ENV ") && env::Shape::fromString (t.fromFirstOccurrenceOf (" ", false, false), out, vMin, 1.f);
    }
}
