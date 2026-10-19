#pragma once
// =====================================================================
//  DOOMSDAY shared parameter + preset helpers (header only)
//    fmt::  value <-> text for Hz, ms, dB, %, oct, pan, cents, semis, tilt
//    make:: float / bool / choice / int parameter factories (with the formatters)
//    PresetIO  user preset folder, list / save / load (*.ext XML of the APVTS state),
//              factory reset helper, state round trip that fills params unknown to old states
// =====================================================================
#include <juce_audio_processors/juce_audio_processors.h>

namespace doomsday
{
    namespace fmt
    {
        inline juce::String hz (float v, int)   { return v >= 1000.f ? juce::String (v / 1000.f, 2) + " kHz" : juce::String ((int) std::round (v)) + " Hz"; }
        inline juce::String ms (float v, int)   { return v >= 1000.f ? juce::String (v / 1000.f, 2) + " s" : juce::String (v, v < 10.f ? 1 : 0) + " ms"; }
        inline juce::String db (float v, int)   { return v <= -47.9f ? juce::String ("-inf") : (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
        inline juce::String pct (float v, int)  { return juce::String ((int) std::round (v * 100.f)) + "%"; }
        inline juce::String bip (float v, int)  { return (v > 0.005f ? "+" : "") + juce::String ((int) std::round (v * 100.f)) + "%"; }
        inline juce::String oct (float v, int)  { return (v > 0.005f ? "+" : "") + juce::String (v, 2) + " oct"; }
        inline juce::String semi (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " st"; }
        inline juce::String cents (float v, int){ return juce::String ((int) std::round (v)) + " ct"; }
        inline juce::String lfoHz (float v, int){ return juce::String (v, 2) + " Hz"; }
        inline juce::String pan (float v, int)  { return std::abs (v) < 0.01f ? juce::String ("C") : (v < 0.f ? "L " : "R ") + juce::String ((int) std::round (std::abs (v) * 100.f)); }

        inline float num (const juce::String& t)  { return t.retainCharacters ("0123456789.-+").getFloatValue(); }
        inline float hzIn (const juce::String& t) { auto v = num (t); return t.containsIgnoreCase ("k") ? v * 1000.f : v; }
        inline float msIn (const juce::String& t) { auto v = num (t); return (t.trim().endsWithIgnoreCase ("s") && ! t.containsIgnoreCase ("ms")) ? v * 1000.f : v; }
        inline float pctIn (const juce::String& t){ return num (t) / 100.f; }
        inline float panIn (const juce::String& t){ auto v = num (t) / 100.f; return t.containsIgnoreCase ("l") ? -std::abs (v) : v; }
    }

    namespace make
    {
        using Fmt = juce::String (*) (float, int);
        using Parse = float (*) (const juce::String&);
        inline juce::NormalisableRange<float> skew (float lo, float hi, float centre)
        {
            juce::NormalisableRange<float> r (lo, hi);
            r.setSkewForCentre (centre);
            return r;
        }
        inline std::unique_ptr<juce::AudioParameterFloat> f (const juce::String& id, const juce::String& name, juce::NormalisableRange<float> r,
                                                             float def, Fmt fm = fmt::pct, Parse pa = fmt::pctIn, int version = 1)
        {
            return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, version }, name, r, def,
                       juce::AudioParameterFloatAttributes().withStringFromValueFunction (fm).withValueFromStringFunction (pa));
        }
        inline std::unique_ptr<juce::AudioParameterBool> b (const juce::String& id, const juce::String& name, bool def, int version = 1)
        {
            return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, version }, name, def);
        }
        inline std::unique_ptr<juce::AudioParameterChoice> c (const juce::String& id, const juce::String& name, const juce::StringArray& items, int def, int version = 1)
        {
            return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, version }, name, items, def);
        }
        inline std::unique_ptr<juce::AudioParameterInt> i (const juce::String& id, const juce::String& name, int lo, int hi, int def, bool signedText = false, int version = 1)
        {
            auto a = juce::AudioParameterIntAttributes().withStringFromValueFunction ([signedText] (int v, int) { return (signedText && v > 0 ? "+" : "") + juce::String (v); });
            return std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, version }, name, lo, hi, def, a);
        }
    }

    // values for one preset: { param id, real value }
    using PresetValues = std::vector<std::pair<juce::String, float>>;

    struct PresetIO
    {
        juce::String product, extension;     // e.g. "Dark Serpent", "dsp"

        juce::File folder() const
        {
            return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (product).getChildFile ("Presets");
        }
        juce::Array<juce::File> list() const
        {
            auto files = folder().findChildFiles (juce::File::findFiles, false, "*." + extension);
            files.sort();
            return files;
        }
        // every parameter back to its default, then the given values
        static void apply (juce::AudioProcessor& proc, juce::AudioProcessorValueTreeState& apvts, const PresetValues& values)
        {
            for (auto* prm : proc.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                    rp->setValueNotifyingHost (rp->getDefaultValue());
            for (auto& [id, v] : values)
                if (auto* rp = apvts.getParameter (id))
                    rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        }
        bool save (juce::AudioProcessorValueTreeState& apvts, const juce::String& name, juce::String& cleanName) const
        {
            cleanName = juce::File::createLegalFileName (name.trim());
            if (cleanName.isEmpty() || ! folder().createDirectory()) return false;
            std::unique_ptr<juce::XmlElement> xml (apvts.copyState().createXml());
            if (xml == nullptr) return false;
            for (auto* a : { "program", "presetName" }) xml->removeAttribute (a);
            return xml->writeTo (folder().getChildFile (cleanName + "." + extension));
        }
        static bool load (juce::AudioProcessor& proc, juce::AudioProcessorValueTreeState& apvts, const juce::File& f)
        {
            auto xml = juce::XmlDocument::parse (f);
            if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
            PresetValues v;
            for (auto* child : xml->getChildWithTagNameIterator ("PARAM"))
                v.push_back ({ child->getStringAttribute ("id"), (float) child->getDoubleAttribute ("value") });
            apply (proc, apvts, v);
            return true;
        }
        static void getState (juce::AudioProcessorValueTreeState& apvts, juce::MemoryBlock& dest, int program, const juce::String& presetName)
        {
            auto state = apvts.copyState();
            state.setProperty ("program", program, nullptr);
            state.setProperty ("presetName", presetName, nullptr);
            if (auto xml = state.createXml()) juce::AudioProcessor::copyXmlToBinary (*xml, dest);
        }
        // returns false if the data was not ours; fills params unknown to older states with defaults
        static bool setState (juce::AudioProcessor& proc, juce::AudioProcessorValueTreeState& apvts, const void* data, int size,
                              int& program, juce::String& presetName)
        {
            auto xml = juce::AudioProcessor::getXmlFromBinary (data, size);
            if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
            auto vt = juce::ValueTree::fromXml (*xml);
            program = vt.getProperty ("program", 0);
            presetName = vt.getProperty ("presetName", presetName).toString();
            for (auto* prm : proc.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                    if (! vt.getChildWithProperty ("id", rp->paramID).isValid())
                    {
                        juce::ValueTree child ("PARAM");
                        child.setProperty ("id", rp->paramID, nullptr);
                        child.setProperty ("value", rp->convertFrom0to1 (rp->getDefaultValue()), nullptr);
                        vt.appendChild (child, nullptr);
                    }
            apvts.replaceState (vt);
            return true;
        }
        // host tempo + position: ppq < 0 when not playing
        static void hostTime (juce::AudioPlayHead* ph, double& bpm, double& ppq)
        {
            bpm = 150.0; ppq = -1.0;
            if (ph == nullptr) return;
            if (auto pos = ph->getPosition())
            {
                if (auto b = pos->getBpm()) bpm = *b;
                if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) ppq = *q;
            }
        }
    };
}
