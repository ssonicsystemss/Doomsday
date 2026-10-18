#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// ------------------------------------------------------------ formatting
namespace
{
    String fmtHz (float v, int)   { return v >= 1000.f ? String (v / 1000.f, 2) + " kHz" : String ((int) std::round (v)) + " Hz"; }
    String fmtMs (float v, int)   { return v >= 1000.f ? String (v / 1000.f, 2) + " s" : String (v, v < 10.f ? 1 : 0) + " ms"; }
    String fmtDb (float v, int)   { return v <= -47.9f ? String ("-inf") : (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }
    String fmtPct (float v, int)  { return String ((int) std::round (v * 100.f)) + "%"; }
    String fmtOct (float v, int)  { return (v > 0.005f ? "+" : "") + String (v, 2) + " oct"; }
    String fmtHzLfo (float v, int){ return String (v, 2) + " Hz"; }
    String fmtPan (float v, int)  { return std::abs (v) < 0.01f ? String ("C") : (v < 0.f ? "L " : "R ") + String ((int) std::round (std::abs (v) * 100.f)); }
    String fmtPhase (float v, int){ return v >= 0.995f ? String ("FREE") : String ((int) std::round (v * 360.f)) + juce::String (CharPointer_UTF8 ("\xc2\xb0")); }
    float  parseNum (const String& t) { return t.retainCharacters ("0123456789.-+").getFloatValue(); }
    float  parseHz (const String& t)  { auto v = parseNum (t); return t.containsIgnoreCase ("k") ? v * 1000.f : v; }
    float  parseMs (const String& t)  { auto v = parseNum (t); return (t.trim().endsWithIgnoreCase ("s") && ! t.containsIgnoreCase ("ms")) ? v * 1000.f : v; }
    float  parsePct (const String& t) { return parseNum (t) / 100.f; }
    float  parsePan (const String& t) { auto v = parseNum (t) / 100.f; return t.containsIgnoreCase ("l") ? -std::abs (v) : v; }
    float  parsePhase (const String& t) { return t.containsIgnoreCase ("free") ? 1.f : jlimit (0.f, 0.99f, parseNum (t) / 360.f); }

    NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }
    std::unique_ptr<AudioParameterFloat> fp (const String& id, const String& name, NormalisableRange<float> r, float def,
                                             String (*fmt) (float, int), float (*parse) (const String&))
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                   AudioParameterFloatAttributes().withStringFromValueFunction (fmt).withValueFromStringFunction (parse));
    }
    std::unique_ptr<AudioParameterBool> bp (const String& id, const String& name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def);
    }
    std::unique_ptr<AudioParameterChoice> cp (const String& id, const String& name, const StringArray& c, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def);
    }
    std::unique_ptr<AudioParameterInt> ip (const String& id, const String& name, int lo, int hi, int def, bool signedFmt = false)
    {
        auto attrs = AudioParameterIntAttributes().withStringFromValueFunction ([signedFmt] (int v, int)
                     { return (signedFmt && v > 0 ? "+" : "") + String (v); });
        return std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def, attrs);
    }
    const char* partName (int p) { return p == 0 ? "A" : "B"; }
}

AudioProcessorValueTreeState::ParameterLayout FortuneProcessor::createLayout()
{
    using namespace ParamIDs;
    AudioProcessorValueTreeState::ParameterLayout l;

    for (int p = 0; p < numParts; ++p)
    {
        const String P (partName (p));
        for (int o = 0; o < oscsPerPart; ++o)
        {
            const String n = "Osc " + P + String (o + 1) + " ";
            auto id = [p, o] (const char* b) { return ParamIDs::osc (p, o, b); };
            l.add (bp (id (on), n + "On", o == 0 && p == 0));
            l.add (cp (id (wave), n + "Wave", Choices::waves, 2));
            l.add (ip (id (voices), n + "Voices", 1, FortuneEngine::maxUnison, 5));
            l.add (ip (id (oct), n + "Octave", -3, 3, 0, true));
            l.add (ip (id (semi), n + "Semi", -12, 12, 0, true));
            l.add (fp (id (vol), n + "Volume", { -48.f, 6.f }, -6.f, fmtDb, parseNum));
            l.add (fp (id (phase), n + "Phase", { 0.f, 1.f }, 1.f, fmtPhase, parsePhase));
            l.add (fp (id (detune), n + "Detune", { 0.f, 1.f }, 0.3f, fmtPct, parsePct));
            l.add (fp (id (stereo), n + "Stereo", { 0.f, 1.f }, 0.7f, fmtPct, parsePct));
            l.add (fp (id (pan), n + "Pan", { -1.f, 1.f }, 0.f, fmtPan, parsePan));
            l.add (fp (id (scream), n + "Scream", { 0.f, 1.f }, 0.f, fmtPct, parsePct));
        }
        const String n = "Part " + P + " ";
        auto id = [p] (const char* b) { return ParamIDs::part (p, b); };
        l.add (bp (id (partOn), n + "On", true));
        l.add (cp (id (ftype), n + "Filter Type", Choices::filters, 1));
        l.add (fp (id (cutoff), n + "Cutoff", skewed (20.f, 20000.f, 1500.f), 12000.f, fmtHz, parseHz));
        l.add (fp (id (reso), n + "Resonance", { 0.f, 1.f }, 0.15f, fmtPct, parsePct));
        l.add (fp (id (fdrive), n + "Filter Drive", { 0.f, 1.f }, 0.f, fmtPct, parsePct));
        l.add (fp (id (fenv), n + "Filter Env", { -4.f, 4.f }, 0.f, fmtOct, parseNum));
        l.add (fp (id (fdecay), n + "Filter Decay", skewed (5.f, 3000.f, 300.f), 300.f, fmtMs, parseMs));
        l.add (fp (id (attack), n + "Attack", skewed (0.3f, 5000.f, 120.f), 2.f, fmtMs, parseMs));
        l.add (fp (id (decay), n + "Decay", skewed (5.f, 5000.f, 500.f), 400.f, fmtMs, parseMs));
        l.add (fp (id (sustain), n + "Sustain", { 0.f, 1.f }, 1.f, fmtPct, parsePct));
        l.add (fp (id (release), n + "Release", skewed (5.f, 8000.f, 500.f), 250.f, fmtMs, parseMs));
        l.add (fp (id (level), n + "Level", { -36.f, 6.f }, -3.f, fmtDb, parseNum));

        l.add (bp (id (distOn), n + "Dist On", false));
        l.add (cp (id (distType), n + "Dist Type", Choices::dists, 0));
        l.add (fp (id (distDrive), n + "Dist Drive", { 0.f, 36.f }, 12.f, fmtDb, parseNum));
        l.add (fp (id (distMix), n + "Dist Mix", { 0.f, 1.f }, 1.f, fmtPct, parsePct));
        l.add (bp (id (choOn), n + "Chorus On", false));
        l.add (fp (id (choRate), n + "Chorus Rate", skewed (0.05f, 5.f, 0.8f), 0.6f, fmtHzLfo, parseNum));
        l.add (fp (id (choDepth), n + "Chorus Depth", { 0.f, 1.f }, 0.5f, fmtPct, parsePct));
        l.add (fp (id (choMix), n + "Chorus Mix", { 0.f, 1.f }, 0.35f, fmtPct, parsePct));
        l.add (bp (id (dlyOn), n + "Delay On", false));
        l.add (cp (id (dlyDiv), n + "Delay Time", Choices::delayDivs, 3));
        l.add (fp (id (dlyFb), n + "Delay Feedback", { 0.f, 0.9f }, 0.35f, fmtPct, parsePct));
        l.add (fp (id (dlyMix), n + "Delay Mix", { 0.f, 1.f }, 0.2f, fmtPct, parsePct));
        l.add (bp (id (dlyPP), n + "Delay Ping-Pong", true));
        l.add (bp (id (revOn), n + "Reverb On", true));
        l.add (fp (id (revSize), n + "Reverb Size", { 0.f, 1.f }, 0.75f, fmtPct, parsePct));
        l.add (fp (id (revDamp), n + "Reverb Damp", { 0.f, 1.f }, 0.4f, fmtPct, parsePct));
        l.add (fp (id (revMix), n + "Reverb Mix", { 0.f, 1.f }, 0.22f, fmtPct, parsePct));
        l.add (bp (id (pumpOn), n + "Pump On", false));
        l.add (cp (id (pumpDiv), n + "Pump Rate", Choices::pumpDivs, 1));
        l.add (fp (id (pumpDepth), n + "Pump Depth", { 0.f, 1.f }, 0.6f, fmtPct, parsePct));
        l.add (fp (id (pumpShape), n + "Pump Shape", { 0.f, 1.f }, 0.5f, fmtPct, parsePct));
    }
    l.add (bp (mono, "Mono", false));
    l.add (fp (glide, "Glide", skewed (0.f, 1000.f, 120.f), 0.f, fmtMs, parseMs));
    l.add (fp (fortune, "Fortune", { 0.f, 1.f }, 0.f, fmtPct, parsePct));
    l.add (fp (volume, "Volume", { -36.f, 6.f }, -6.f, fmtDb, parseNum));
    return l;
}

// ------------------------------------------------------------ factory presets
namespace
{
    enum { SINE, TRI, SAW, SQR, PLS, NSE };

    struct PB
    {
        String name;
        std::vector<std::pair<String, float>> v;
        PB& o (int p, int o, const char* k, float x) { v.push_back ({ ParamIDs::osc (p, o, k), x }); return *this; }
        PB& on (int p, int o, int w, int voices, float volDb)
        {
            return this->o (p, o, ParamIDs::on, 1).o (p, o, ParamIDs::wave, (float) w).o (p, o, ParamIDs::voices, (float) voices)
                        .o (p, o, ParamIDs::vol, volDb);
        }
        PB& off (int p, int o) { return this->o (p, o, ParamIDs::on, 0); }
        PB& pt (int p, const char* k, float x) { v.push_back ({ ParamIDs::part (p, k), x }); return *this; }
        PB& env (int p, float a, float d, float s, float r)
        {
            return pt (p, ParamIDs::attack, a).pt (p, ParamIDs::decay, d).pt (p, ParamIDs::sustain, s).pt (p, ParamIDs::release, r);
        }
        PB& g (const char* k, float x) { v.push_back ({ k, x }); return *this; }
    };

    const std::vector<PB>& presets()
    {
        using namespace ParamIDs;
        static const std::vector<PB> list = []
        {
            std::vector<PB> L;
            {   // big euphoric supersaw lead with a square body underneath
                PB p { "Seeker's Anthem", {} };
                p.on (0, 0, SAW, 9, -4).o (0, 0, detune, 0.45f).o (0, 0, stereo, 0.9f)
                 .on (0, 1, SAW, 7, -10).o (0, 1, oct, 1).o (0, 1, detune, 0.35f)
                 .pt (0, cutoff, 9000).pt (0, reso, 0.1f).env (0, 2, 600, 0.85f, 350)
                 .pt (0, dlyOn, 1).pt (0, dlyDiv, 3).pt (0, dlyMix, 0.18f).pt (0, revMix, 0.3f).pt (0, revSize, 0.8f)
                 .pt (0, pumpOn, 1).pt (0, pumpDepth, 0.5f)
                 .on (1, 0, SQR, 3, -8).o (1, 0, oct, -1).o (1, 0, detune, 0.15f).o (1, 0, stereo, 0.3f)
                 .pt (1, cutoff, 2500).pt (1, fenv, 1.5f).pt (1, fdecay, 250).env (1, 2, 600, 0.85f, 350)
                 .pt (1, level, -6).pt (1, pumpOn, 1).pt (1, pumpDepth, 0.5f).pt (1, revMix, 0.15f);
                L.push_back (p);
            }
            {   // pluck: filter envelope bite + dotted delay
                PB p { "Golden Compass", {} };
                p.on (0, 0, SAW, 7, -4).o (0, 0, detune, 0.3f)
                 .on (0, 1, SQR, 3, -12).o (0, 1, oct, 1).o (0, 1, detune, 0.12f)
                 .env (0, 1, 260, 0.f, 220).pt (0, cutoff, 1800).pt (0, fenv, 3.f).pt (0, fdecay, 220).pt (0, reso, 0.25f)
                 .pt (0, dlyOn, 1).pt (0, dlyDiv, 3).pt (0, dlyFb, 0.4f).pt (0, dlyMix, 0.25f).pt (0, revMix, 0.3f)
                 .on (1, 0, SINE, 1, -6).o (1, 0, oct, -1).o (1, 0, phase, 0.f)
                 .env (1, 1, 300, 0.f, 220).pt (1, cutoff, 20000).pt (1, revOn, 0).pt (1, level, -8);
                L.push_back (p);
            }
            {   // screaming mono lead
                PB p { "Treasure Screech", {} };
                p.g (mono, 1).g (glide, 60)
                 .on (0, 0, SAW, 5, -4).o (0, 0, detune, 0.2f).o (0, 0, scream, 0.55f)
                 .on (0, 1, SQR, 3, -9).o (0, 1, semi, 7).o (0, 1, scream, 0.4f).o (0, 1, detune, 0.15f)
                 .pt (0, distOn, 1).pt (0, distType, 3).pt (0, distDrive, 8).pt (0, distMix, 0.5f)
                 .pt (0, cutoff, 6000).pt (0, reso, 0.35f).env (0, 3, 400, 0.9f, 200)
                 .pt (0, dlyOn, 1).pt (0, dlyDiv, 2).pt (0, dlyMix, 0.15f).pt (0, revMix, 0.2f)
                 .on (1, 0, SINE, 1, -10).o (1, 0, oct, -1).env (1, 3, 400, 0.9f, 200).pt (1, revOn, 0);
                L.push_back (p);
            }
            {   // slow pumping pad
                PB p { "Lost Map Pad", {} };
                p.on (0, 0, SAW, 9, -6).o (0, 0, detune, 0.5f).o (0, 0, stereo, 1.f)
                 .on (0, 1, TRI, 3, -10).o (0, 1, oct, 1)
                 .env (0, 600, 1500, 0.8f, 1800).pt (0, cutoff, 3500).pt (0, choOn, 1).pt (0, revMix, 0.45f).pt (0, revSize, 0.9f)
                 .pt (0, pumpOn, 1).pt (0, pumpDepth, 0.7f)
                 .on (1, 0, SAW, 7, -8).o (1, 0, oct, -1).o (1, 0, detune, 0.4f)
                 .env (1, 600, 1500, 0.8f, 1800).pt (1, cutoff, 1200).pt (1, level, -6).pt (1, revMix, 0.35f)
                 .pt (1, pumpOn, 1).pt (1, pumpDepth, 0.7f);
                L.push_back (p);
            }
            {   // rawstyle lead
                PB p { "Raw Fortune", {} };
                p.g (mono, 1).g (glide, 40)
                 .on (0, 0, SAW, 7, -4).o (0, 0, detune, 0.25f).o (0, 0, scream, 0.7f)
                 .on (0, 1, PLS, 5, -8).o (0, 1, detune, 0.2f).o (0, 1, scream, 0.5f)
                 .pt (0, distOn, 1).pt (0, distType, 1).pt (0, distDrive, 14).pt (0, distMix, 0.6f)
                 .pt (0, cutoff, 4500).pt (0, reso, 0.4f).pt (0, fdrive, 0.4f).env (0, 2, 300, 0.9f, 150)
                 .pt (0, dlyOn, 1).pt (0, dlyDiv, 4).pt (0, dlyMix, 0.12f).pt (0, revMix, 0.18f)
                 .on (1, 0, SQR, 1, -9).o (1, 0, oct, -1).o (1, 0, scream, 0.3f)
                 .pt (1, distOn, 1).pt (1, distType, 2).env (1, 2, 300, 0.9f, 150).pt (1, revOn, 0);
                L.push_back (p);
            }
            {   // wide chords: supersaws in both parts, pumping
                PB p { "Euphoric Horizon", {} };
                p.on (0, 0, SAW, 9, -5).o (0, 0, detune, 0.4f).o (0, 0, stereo, 1.f)
                 .on (0, 1, SAW, 9, -9).o (0, 1, oct, 1).o (0, 1, detune, 0.45f).o (0, 1, stereo, 1.f)
                 .env (0, 4, 800, 0.8f, 500).pt (0, cutoff, 11000).pt (0, revMix, 0.3f).pt (0, pumpOn, 1).pt (0, pumpDepth, 0.65f)
                 .on (1, 0, SAW, 7, -7).o (1, 0, oct, -1).o (1, 0, detune, 0.3f)
                 .on (1, 1, PLS, 5, -11).o (1, 1, detune, 0.35f)
                 .env (1, 4, 800, 0.8f, 500).pt (1, cutoff, 5000).pt (1, choOn, 1).pt (1, pumpOn, 1).pt (1, pumpDepth, 0.65f)
                 .pt (1, level, -5);
                L.push_back (p);
            }
            {   // hoover
                PB p { "Hoover Seekers", {} };
                p.g (mono, 1).g (glide, 120)
                 .on (0, 0, PLS, 7, -5).o (0, 0, detune, 0.7f).o (0, 0, stereo, 1.f)
                 .on (0, 1, SAW, 5, -8).o (0, 1, oct, -1).o (0, 1, detune, 0.6f)
                 .pt (0, choOn, 1).pt (0, choDepth, 0.8f).pt (0, choMix, 0.5f).pt (0, cutoff, 5000).env (0, 5, 500, 0.9f, 300)
                 .pt (0, revMix, 0.25f)
                 .pt (1, partOn, 0);
                L.push_back (p);
            }
            {   // glassy arp
                PB p { "Crystal Arp", {} };
                p.on (0, 0, TRI, 3, -4).o (0, 0, detune, 0.1f)
                 .on (0, 1, SINE, 1, -14).o (0, 1, oct, 2)
                 .env (0, 1, 180, 0.f, 150).pt (0, dlyOn, 1).pt (0, dlyDiv, 1).pt (0, dlyMix, 0.3f).pt (0, revMix, 0.35f)
                 .on (1, 0, SQR, 1, -16).o (1, 0, oct, 1).env (1, 1, 120, 0.f, 100).pt (1, cutoff, 6000);
                L.push_back (p);
            }
            L.push_back (PB { "Init", {} });
            return L;
        }();
        return list;
    }
}

int FortuneProcessor::getNumPrograms() { return (int) presets().size(); }
const String FortuneProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? presets()[(size_t) i].name : String();
}

void FortuneProcessor::setCurrentProgram (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentProgram = index;
    const auto& pr = presets()[(size_t) index];
    currentPresetName = pr.name;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [pid, v] : pr.v)
        if (auto* rp = apvts.getParameter (pid))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
}

// ------------------------------------------------------------ construction
FortuneProcessor::FortuneProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FORTUNE", createLayout())
{
    using namespace ParamIDs;
    auto g = [this] (const String& id) { auto* r = apvts.getRawParameterValue (id); jassert (r != nullptr); return r; };
    for (int p = 0; p < numParts; ++p)
    {
        auto& P = ptr.part[(size_t) p];
        for (int o = 0; o < oscsPerPart; ++o)
        {
            auto& O = P.osc[(size_t) o];
            auto id = [p, o] (const char* b) { return ParamIDs::osc (p, o, b); };
            O.on = g (id (on)); O.wave = g (id (wave)); O.voices = g (id (voices)); O.oct = g (id (oct)); O.semi = g (id (semi));
            O.vol = g (id (vol)); O.phase = g (id (phase)); O.detune = g (id (detune)); O.stereo = g (id (stereo));
            O.pan = g (id (pan)); O.scream = g (id (scream));
        }
        auto id = [p] (const char* b) { return ParamIDs::part (p, b); };
        P.on = g (id (partOn)); P.ftype = g (id (ftype)); P.cutoff = g (id (cutoff)); P.reso = g (id (reso));
        P.fdrive = g (id (fdrive)); P.fenv = g (id (fenv)); P.fdecay = g (id (fdecay));
        P.att = g (id (attack)); P.dec = g (id (decay)); P.sus = g (id (sustain)); P.rel = g (id (release)); P.level = g (id (level));
        P.distOn = g (id (distOn)); P.distType = g (id (distType)); P.distDrive = g (id (distDrive)); P.distMix = g (id (distMix));
        P.choOn = g (id (choOn)); P.choRate = g (id (choRate)); P.choDepth = g (id (choDepth)); P.choMix = g (id (choMix));
        P.dlyOn = g (id (dlyOn)); P.dlyDiv = g (id (dlyDiv)); P.dlyFb = g (id (dlyFb)); P.dlyMix = g (id (dlyMix)); P.dlyPP = g (id (dlyPP));
        P.revOn = g (id (revOn)); P.revSize = g (id (revSize)); P.revDamp = g (id (revDamp)); P.revMix = g (id (revMix));
        P.pumpOn = g (id (pumpOn)); P.pumpDiv = g (id (pumpDiv)); P.pumpDepth = g (id (pumpDepth)); P.pumpShape = g (id (pumpShape));
    }
    ptr.mono = g (mono); ptr.glide = g (glide); ptr.fortune = g (fortune); ptr.volume = g (volume);

    setCurrentProgram (0);
}

bool FortuneProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void FortuneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (gatherParams (150.0, -1.0));
    engine.prepare (sampleRate, samplesPerBlock);
}

EngineParams FortuneProcessor::gatherParams (double bpm, double ppq) const
{
    auto on = [] (A a) { return a->load() > 0.5f; };
    auto ri = [] (A a) { return (int) std::round (a->load()); };
    EngineParams e;
    for (int p = 0; p < ParamIDs::numParts; ++p)
    {
        const auto& P = ptr.part[(size_t) p];
        auto& E = e.part[(size_t) p];
        for (int o = 0; o < ParamIDs::oscsPerPart; ++o)
        {
            const auto& O = P.osc[(size_t) o];
            auto& EO = E.osc[(size_t) o];
            EO.on = on (O.on); EO.wave = jlimit (0, 5, ri (O.wave)); EO.voices = ri (O.voices);
            EO.octave = ri (O.oct); EO.semi = ri (O.semi);
            EO.gain = O.vol->load() <= -47.9f ? 0.f : Decibels::decibelsToGain (O.vol->load());
            EO.phase = O.phase->load(); EO.detune = O.detune->load(); EO.stereo = O.stereo->load();
            EO.pan = O.pan->load(); EO.scream = O.scream->load();
        }
        E.on = on (P.on); E.ftype = jlimit (0, 3, ri (P.ftype)); E.cutoff = P.cutoff->load(); E.reso = P.reso->load();
        E.fdrive = P.fdrive->load(); E.fenvOct = P.fenv->load(); E.fdecayMs = P.fdecay->load();
        E.attackMs = P.att->load(); E.decayMs = P.dec->load(); E.sustain = P.sus->load(); E.releaseMs = P.rel->load();
        E.level = Decibels::decibelsToGain (P.level->load(), -60.f);
        E.distOn = on (P.distOn); E.distType = jlimit (0, 3, ri (P.distType)); E.distDriveDb = P.distDrive->load(); E.distMix = P.distMix->load();
        E.choOn = on (P.choOn); E.choRate = P.choRate->load(); E.choDepth = P.choDepth->load(); E.choMix = P.choMix->load();
        E.dlyOn = on (P.dlyOn); E.dlyBeats = Choices::delayBeats[jlimit (0, Choices::delayDivs.size() - 1, ri (P.dlyDiv))];
        E.dlyFb = P.dlyFb->load(); E.dlyMix = P.dlyMix->load(); E.dlyPP = on (P.dlyPP);
        E.revOn = on (P.revOn); E.revSize = P.revSize->load(); E.revDamp = P.revDamp->load(); E.revMix = P.revMix->load();
        E.pumpOn = on (P.pumpOn); E.pumpBeats = Choices::pumpBeats[jlimit (0, Choices::pumpDivs.size() - 1, ri (P.pumpDiv))];
        E.pumpDepth = P.pumpDepth->load(); E.pumpShape = P.pumpShape->load();
    }
    e.mono = on (ptr.mono);
    e.glideMs = ptr.glide->load();
    e.fortune = ptr.fortune->load();
    e.volume = Decibels::decibelsToGain (ptr.volume->load(), -60.f);
    e.bpm = bpm;
    e.ppq = ppq;
    return e;
}

void FortuneProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, buffer.getNumSamples());
    if (buffer.getNumChannels() < 2) { buffer.clear(); return; }

    double bpm = 150.0, ppq = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) ppq = *q;
        }

    // PLAY button in the GUI: a chord at C4
    const int aud = audition.exchange (0);
    if (aud == 1 && ! auditionHeld)
    {
        for (int n : { 60, 64, 67, 72 }) midi.addEvent (MidiMessage::noteOn (1, n, (uint8) 110), 0);
        auditionHeld = true;
    }
    else if (aud == 2 && auditionHeld)
    {
        for (int n : { 60, 64, 67, 72 }) midi.addEvent (MidiMessage::noteOff (1, n), 0);
        auditionHeld = false;
    }

    engine.setParams (gatherParams (bpm, ppq));
    AudioBuffer<float> stereo (buffer.getArrayOfWritePointers(), 2, buffer.getNumSamples());
    engine.process (stereo, midi);
}

// ------------------------------------------------------------ user presets
File FortuneProcessor::getUserPresetFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Fortune of Seekers").getChildFile ("Presets");
}

Array<File> FortuneProcessor::getUserPresets() const
{
    auto files = getUserPresetFolder().findChildFiles (File::findFiles, false, "*.fos");
    files.sort();
    return files;
}

bool FortuneProcessor::saveUserPreset (const String& name)
{
    const auto clean = File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto folder = getUserPresetFolder();
    if (! folder.createDirectory()) return false;
    std::unique_ptr<XmlElement> xml (apvts.copyState().createXml());
    if (xml == nullptr) return false;
    for (auto* a : { "program", "presetName" }) xml->removeAttribute (a);
    if (! xml->writeTo (folder.getChildFile (clean + ".fos"))) return false;
    currentPresetName = clean;
    return true;
}

void FortuneProcessor::applyValues (const XmlElement& xml)
{
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto* child : xml.getChildWithTagNameIterator ("PARAM"))
        if (auto* rp = apvts.getParameter (child->getStringAttribute ("id")))
            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child->getDoubleAttribute ("value")));
}

bool FortuneProcessor::loadUserPreset (const File& f)
{
    auto xml = XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    applyValues (*xml);
    currentPresetName = f.getFileNameWithoutExtension();
    return true;
}

// ------------------------------------------------------------ randomize (musical ranges)
void FortuneProcessor::randomize()
{
    using namespace ParamIDs;
    auto& r = rng;
    auto set = [this] (const String& id, float v)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    };
    auto between = [&r] (float lo, float hi) { return lo + r.nextFloat() * (hi - lo); };
    auto logBetween = [&r] (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); };
    auto chance = [&r] (float p) { return r.nextFloat() < p ? 1.f : 0.f; };
    auto pickWave = [&r]
    {
        const float x = r.nextFloat();
        return (float) (x < 0.55f ? SAW : x < 0.72f ? SQR : x < 0.84f ? PLS : x < 0.93f ? TRI : SINE);
    };

    // envelope character shared by both parts so they play as one sound
    const int kind = r.nextInt (3);            // 0 lead, 1 pluck, 2 pad
    const float att = kind == 2 ? logBetween (200.f, 1200.f) : logBetween (0.5f, 8.f);
    const float dec = kind == 1 ? logBetween (150.f, 450.f) : logBetween (300.f, 1500.f);
    const float sus = kind == 1 ? between (0.f, 0.15f) : between (0.6f, 1.f);
    const float rel = kind == 2 ? logBetween (800.f, 2500.f) : logBetween (120.f, 500.f);

    for (int p = 0; p < numParts; ++p)
    {
        set (part (p, partOn), p == 0 ? 1.f : chance (0.75f));
        for (int o = 0; o < oscsPerPart; ++o)
        {
            auto id = [p, o] (const char* b) { return ParamIDs::osc (p, o, b); };
            set (id (on), o == 0 ? 1.f : chance (0.55f));
            set (id (wave), pickWave());
            set (id (voices), (float) (1 + r.nextInt (9)));
            set (id (oct), p == 1 && o == 0 ? (chance (0.6f) > 0.f ? -1.f : 0.f) : (float) (r.nextInt (3) - (o == 1 ? 0 : 1)));
            set (id (semi), chance (0.2f) > 0.f ? (chance (0.5f) > 0.f ? 7.f : 12.f) : 0.f);
            set (id (vol), o == 0 ? between (-6.f, -2.f) : between (-14.f, -6.f));
            set (id (phase), chance (0.7f) > 0.f ? 1.f : between (0.f, 0.5f));
            set (id (detune), between (0.05f, 0.55f));
            set (id (stereo), between (0.3f, 1.f));
            set (id (pan), chance (0.75f) > 0.f ? 0.f : between (-0.4f, 0.4f));
            set (id (scream), chance (0.3f) > 0.f ? between (0.1f, 0.6f) : 0.f);
        }
        set (part (p, ftype), chance (0.85f) > 0.f ? 1.f : 0.f);
        set (part (p, cutoff), logBetween (1500.f, 14000.f));
        set (part (p, reso), between (0.f, 0.4f));
        set (part (p, fdrive), chance (0.3f) > 0.f ? between (0.f, 0.5f) : 0.f);
        set (part (p, fenv), kind == 1 ? between (1.5f, 3.5f) : between (0.f, 1.f));
        set (part (p, fdecay), logBetween (120.f, 600.f));
        set (part (p, attack), att); set (part (p, decay), dec); set (part (p, sustain), sus); set (part (p, release), rel);
        set (part (p, level), p == 0 ? between (-4.f, -2.f) : between (-9.f, -4.f));
        set (part (p, distOn), chance (0.25f));
        set (part (p, distType), (float) r.nextInt (4));
        set (part (p, distDrive), between (4.f, 16.f));
        set (part (p, distMix), between (0.3f, 0.8f));
        set (part (p, choOn), chance (0.35f));
        set (part (p, dlyOn), chance (0.6f));
        set (part (p, dlyDiv), (float) (1 + r.nextInt (4)));
        set (part (p, dlyFb), between (0.2f, 0.5f));
        set (part (p, dlyMix), between (0.1f, 0.25f));
        set (part (p, revOn), 1.f);
        set (part (p, revSize), between (0.55f, 0.9f));
        set (part (p, revMix), between (0.15f, 0.4f));
        set (part (p, pumpOn), chance (0.5f));
        set (part (p, pumpDepth), between (0.4f, 0.75f));
    }
    set (mono, kind == 0 ? chance (0.5f) : 0.f);
    set (glide, chance (0.4f) > 0.f ? between (20.f, 120.f) : 0.f);
    currentPresetName = "Random #" + String (1000 + r.nextInt (9000));
}

// ------------------------------------------------------------ state
AudioProcessorEditor* FortuneProcessor::createEditor() { return new FortuneEditor (*this); }

void FortuneProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void FortuneProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = ValueTree::fromXml (*xml);
            currentProgram = vt.getProperty ("program", 0);
            currentPresetName = vt.getProperty ("presetName", getProgramName (currentProgram)).toString();
            for (auto* prm : getParameters())
                if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
                    if (! vt.getChildWithProperty ("id", rp->paramID).isValid())
                    {
                        ValueTree child ("PARAM");
                        child.setProperty ("id", rp->paramID, nullptr);
                        child.setProperty ("value", rp->convertFrom0to1 (rp->getDefaultValue()), nullptr);
                        vt.appendChild (child, nullptr);
                    }
            apvts.replaceState (vt);
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FortuneProcessor(); }
