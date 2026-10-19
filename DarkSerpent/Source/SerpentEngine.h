#pragma once
// =====================================================================
//  DARK SERPENT - hardstyle screamer / lead engine
//
//  per voice (LEGATO / RETRIG mono, or POLY up to 3 voices for octaves):
//    CORE  supersaw (up to 16 voices) + octave copy  --+
//    EDGE  bright wide unison +1/+2 oct, high-passed --+--> TALK (3-formant vowel filter) -> LP24 --+--> AMP
//    BODY  clean mono centre (sine / tri / square) -------------------------------------------------+
//  pitch: glide, BEND into the note (every note or first of a phrase), delayed vibrato
//  MOVE: tempo-synced LFO -> cutoff / vowel / amp
//  bus (2x oversampled): RAGE (tube / hard / fold, level-matched) -> OTT -> WIDTH + mono bass -> FX
//  ENERGY macro: more rage, OTT, brighter filter, wider, deeper vibrato, more space
// =====================================================================
#include "DoomsdayDSP.h"

struct SerpentParams
{
    int   coreWave = 2, coreVoices = 12;  float coreDetune = 0.75f, coreOct = 1.f, coreGain = 0.7f;
    bool  edgeOn = true;  int edgeVoices = 9, edgeOct = 1; float edgeDetune = 0.85f, edgeToneHz = 3000.f, edgeGain = 0.7f;
    bool  bodyOn = true;  int bodyWave = 0, bodyOct = 0;   float bodyGain = 0.2f;
    float cutoff = 20000.f, reso = 0.15f, fenvOct = 1.f, fdecayMs = 400.f, tone = 0.6f;
    float talk = 0.15f, vowel = 0.f, sweep = 0.f, sweepMs = 250.f, talkKey = 0.4f;
    bool  moveOn = true;  float moveBeats = 2.f / 3.f; int moveShape = 0; float moveCut = 1.f, moveVowel = 0.2f, moveAmp = 0.f; bool moveRetrig = true;
    int   mode = 0; float glideMs = 40.f, bendSt = 0.f, bendMs = 80.f; bool bendFirst = true;
    float vibCents = 8.f, vibHz = 5.f, vibDelayMs = 250.f, velAmt = 0.3f;
    bool  rageOn = true;  int rageType = 0; float rageDb = 8.f, rageMix = 0.5f, ott = 0.5f;
    float attackMs = 3.f, decayMs = 600.f, sustain = 1.f, releaseMs = 200.f;
    float width = 1.f, monoBassHz = 180.f;
    doomsday::dsp::FxParams fx;
    float energy = 0.f, volume = 0.7f;
    double bpm = 150.0, ppq = -1.0;
};

class SerpentEngine
{
public:
    static constexpr int maxPoly = 3, numSlots = 6;
    using Bank = doomsday::dsp::UnisonBank;

    doomsday::dsp::LevelFeed<1024> feed;
    std::atomic<float> outPeak { 0.f }, liveVowel { 0.f }, liveTalk { 0.f };

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate; osr = sr * 2.0;
        maxBlockSize = juce::jmax (32, maxBlock);
        os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        os->initProcessing ((size_t) maxBlockSize);
        bus.setSize (2, maxBlockSize);
        fxChain.prepare (sr, maxBlockSize);
        feed.prepare (sr);
        lfoBuf.assign ((size_t) maxBlockSize * 2 + 16, 0.f);
        sVol.reset (sr, 0.03); sVol.setCurrentAndTargetValue (p.volume);
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v = {};
        held.clear();
        if (os) os->reset();
        fxChain.reset();
        rz = {};
        lfoPhase = 0.f; lfoHold = 0.f; pitchWheel = 0.f; lastNote = -1; phraseOpen = false;
    }

    void setParams (const SerpentParams& np) { p = np; }

    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
    {
        const int n = buffer.getNumSamples();
        blockSetup();
        offset = 0;
        int pos = 0;
        for (const auto meta : midi)
        {
            const int at = juce::jlimit (0, n, meta.samplePosition);
            if (at > pos) { renderChunked (buffer, pos, at - pos); pos = at; }
            handleMidi (meta.getMessage());
        }
        if (pos < n) renderChunked (buffer, pos, n - pos);
    }

    int activeVoices() const { int c = 0; for (auto& v : voices) c += (v.active && v.kill == 1.f) ? 1 : 0; return c; }

    // vowel table: F1 F2 F3 (Hz) and their gains, A E I O U
    struct Vowel { float f[3], g[3]; };
    static Vowel vowelAt (float pos)
    {
        static const Vowel table[5] = {
            { { 800.f, 1150.f, 2900.f }, { 1.f, 0.63f, 0.25f } },   // A
            { { 450.f, 1750.f, 2650.f }, { 1.f, 0.50f, 0.32f } },   // E
            { { 320.f, 2150.f, 2950.f }, { 1.f, 0.32f, 0.28f } },   // I
            { { 480.f,  820.f, 2800.f }, { 1.f, 0.71f, 0.13f } },   // O
            { { 340.f,  720.f, 2500.f }, { 1.f, 0.40f, 0.10f } } }; // U
        const float x = juce::jlimit (0.f, 1.f, pos) * 4.f;
        const int i = juce::jmin (3, (int) x);
        const float t = x - (float) i;
        Vowel v;
        for (int k = 0; k < 3; ++k)
        {
            v.f[k] = table[i].f[k] * std::pow (table[i + 1].f[k] / table[i].f[k], t);
            v.g[k] = table[i].g[k] + t * (table[i + 1].g[k] - table[i].g[k]);
        }
        return v;
    }

private:
    using Svf = doomsday::dsp::Svf;
    using SvfCoefs = doomsday::dsp::SvfCoefs;
    using Env = doomsday::dsp::Env;

    struct Voice
    {
        bool active = false, held = false;
        int note = -1;
        float vel = 1.f, semi = 60.f, target = 60.f, bendOff = 0.f, vibTime = 0.f, vibPh = 0.f, fenv = 0.f, sweepEnv = 0.f;
        float kill = 1.f, killStep = 0.f;
        juce::uint64 age = 0;
        int counter = 0;
        Env amp;
        Bank core, coreUp, edge, body;
        std::array<Svf, 2> edgeHp;
        std::array<std::array<Svf, 3>, 2> formant;
        std::array<doomsday::dsp::MultiFilter, 2> lp;
        std::array<SvfCoefs, 3> fc {};
        std::array<float, 3> fg {};
        SvfCoefs lpc, hpc;
    };

    // --------------------------------------------------------------- MIDI
    void handleMidi (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber();
            const float vel = m.getFloatVelocity();
            if (p.mode == 2) polyOn (n, vel); else monoOn (n, vel);
        }
        else if (m.isNoteOff())
        {
            const int n = m.getNoteNumber();
            if (p.mode == 2) { for (auto& v : voices) if (v.active && v.held && v.note == n) { v.held = false; v.amp.release(); } }
            else monoOff (n);
            bool any = false;
            for (auto& v : voices) any |= v.active && v.held;
            if (! any) phraseOpen = false;
        }
        else if (m.isPitchWheel()) pitchWheel = juce::jlimit (-1.f, 1.f, (m.getPitchWheelValue() - 8192) / 8192.f);
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            held.clear(); phraseOpen = false;
            for (auto& v : voices) if (v.active) { v.held = false; v.amp.release(); }
        }
    }

    void startVoice (Voice& v, int note, float vel, bool retrigger)
    {
        const bool glide = p.glideMs >= 1.f && lastNote >= 0 && (p.mode != 2 || phraseOpen);
        // BEND: on every note, or only on the first note of a phrase (nothing else held)
        const bool doBend = std::abs (p.bendSt) > 0.01f && (! p.bendFirst || ! phraseOpen);
        v.target = (float) note;
        if (! v.active || retrigger)
        {
            v.semi = glide ? (float) lastNote : (float) note;
            v.amp.trigger (! v.active);
            v.fenv = 1.f; v.sweepEnv = 1.f; v.vibTime = 0.f;
            if (! v.active)
            {
                v.core.resetPhases (rng); v.coreUp.resetPhases (rng); v.edge.resetPhases (rng); v.body.resetPhases (rng, 0.f);
                for (auto& f : v.lp) f = {};
                for (auto& ch : v.formant) for (auto& f : ch) f = {};
            }
            if (p.moveRetrig) lfoPhase = 0.f;
        }
        if (doBend) v.bendOff = p.bendSt;
        v.active = true; v.held = true; v.note = note; v.vel = vel;
        v.kill = 1.f; v.killStep = 0.f;
        v.age = ++ageCounter;
        lastNote = note;
        phraseOpen = true;
    }

    void monoOn (int note, float vel)
    {
        held.erase (std::remove (held.begin(), held.end(), note), held.end());
        held.push_back (note);
        auto& v = voices[0];
        for (size_t i = 1; i < voices.size(); ++i) if (voices[i].active && voices[i].killStep == 0.f) voices[i].killStep = 1.f / (float) (0.003 * osr);
        const bool legato = p.mode == 0 && v.active && held.size() > 1 && ! v.amp.idle() && v.amp.stage != Env::Release;
        startVoice (v, note, vel, ! legato);
    }

    void monoOff (int note)
    {
        const bool wasCurrent = ! held.empty() && held.back() == note;
        held.erase (std::remove (held.begin(), held.end(), note), held.end());
        auto& v = voices[0];
        if (held.empty()) { v.held = false; v.amp.release(); }
        else if (wasCurrent) { v.target = (float) held.back(); v.note = held.back(); lastNote = v.note; }
    }

    void polyOn (int note, float vel)
    {
        for (auto& v : voices) if (v.active && v.note == note && v.killStep == 0.f) { startVoice (v, note, vel, true); return; }
        int playing = 0;
        for (auto& v : voices) playing += (v.active && v.killStep == 0.f) ? 1 : 0;
        if (playing >= maxPoly)
        {
            Voice* victim = nullptr;
            for (auto& v : voices) if (v.active && v.killStep == 0.f && (victim == nullptr || v.age < victim->age)) victim = &v;
            if (victim) victim->killStep = 1.f / (float) (0.003 * osr);
        }
        Voice* slot = nullptr;
        for (auto& v : voices) if (! v.active) { slot = &v; break; }
        if (slot == nullptr) for (auto& v : voices) if (slot == nullptr || v.age < slot->age) slot = &v;
        *slot = Voice {};
        startVoice (*slot, note, vel, true);
    }

    // --------------------------------------------------------------- per block
    void blockSetup()
    {
        const float e = juce::jlimit (0.f, 1.f, p.energy);
        auto cents = [] (float d) { return 120.f * std::pow (juce::jlimit (0.f, 1.f, d), 1.4f); };
        const float stereo = juce::jlimit (0.f, 1.f, p.width + 0.25f * e);
        tCore.setup (p.coreVoices, p.coreWave, cents (p.coreDetune + 0.15f * e), stereo, 0.f, p.coreGain);
        tCoreUp.setup (juce::jmax (1, p.coreVoices - 2), p.coreWave, cents (p.coreDetune + 0.15f * e) * 0.8f, stereo, 0.f, p.coreGain * p.coreOct);
        tEdge.setup (p.edgeVoices, 2, cents (p.edgeDetune + 0.1f * e), 1.f, 0.f, p.edgeOn ? p.edgeGain : 0.f);
        tBody.setup (1, p.bodyWave, 0.f, 0.f, 0.f, p.bodyOn ? p.bodyGain : 0.f);
        ampRates = Env::rates (p.attackMs, p.decayMs, p.sustain, p.releaseMs, osr);
        fCoef = doomsday::dsp::timeCoef (p.fdecayMs, osr);
        sweepCoef = doomsday::dsp::timeCoef (p.sweepMs, osr);
        bendCoef = doomsday::dsp::timeCoef (juce::jmax (5.f, p.bendMs), osr);
        glideCoef = p.glideMs < 1.f ? 0.f : (float) std::exp (-1.0 / ((p.glideMs * 0.001 / 3.0) * osr));
        lfoInc = (float) (juce::jlimit (30.0, 400.0, p.bpm) / 60.0 / p.moveBeats / osr);
        fxChain.setup (p.fx, p.bpm, 0.1f * e);
        sVol.setTargetValue (p.volume);
    }

    void renderChunked (juce::AudioBuffer<float>& buffer, int start, int len)
    {
        while (len > 0)
        {
            const int n = juce::jmin (len, maxBlockSize);
            renderSegment (buffer, start, n);
            start += n; len -= n; offset += n;
        }
    }

    inline float lfoValue()
    {
        switch (p.moveShape)
        {
            case 0:  return doomsday::dsp::fastSin01 (lfoPhase);
            case 1:  return 1.f - 4.f * std::abs (lfoPhase - 0.5f);
            case 2:  return 1.f - 2.f * lfoPhase;
            case 3:  return lfoPhase < 0.5f ? 1.f : -1.f;
            default: return lfoHold;
        }
    }

    void renderSegment (juce::AudioBuffer<float>& buffer, int start, int len)
    {
        const float e = juce::jlimit (0.f, 1.f, p.energy);
        const float invOsr = (float) (1.0 / osr);
        bus.clear (0, len);
        float* chans[] = { bus.getWritePointer (0), bus.getWritePointer (1) };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) len);
        auto up = os->processSamplesUp (block);
        float* oL = up.getChannelPointer (0);
        float* oR = up.getChannelPointer (1);
        const int osLen = (int) up.getNumSamples();
        std::fill (oL, oL + osLen, 0.f);
        std::fill (oR, oR + osLen, 0.f);

        // LFO for this segment (shared by all voices)
        for (int i = 0; i < osLen; ++i)
        {
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.f) { lfoPhase -= 1.f; lfoHold = rng.nextFloat() * 2.f - 1.f; }
            lfoBuf[(size_t) i] = p.moveOn ? lfoValue() : 0.f;
        }

        const float k = 2.f - 1.9f * juce::jlimit (0.f, 1.f, p.reso);
        const float vibDepthSt = (p.vibCents + 15.f * e) / 100.f;
        const float vibInc = p.vibHz * invOsr;
        const float vibDelay = juce::jmax (0.001f, p.vibDelayMs * 0.001f);
        const float bendWheel = pitchWheel * 2.f;
        const float baseVowel = p.vowel;
        float lastVowel = 0.f, lastTalk = 0.f;

        for (auto& v : voices)
        {
            if (! v.active) continue;
            const float velG = 1.f - p.velAmt * 0.6f * (1.f - v.vel);
            const float velOpen = p.velAmt * (v.vel - 0.7f) * 0.5f;
            for (int i = 0; i < osLen; ++i)
            {
                const float a = v.amp.next (ampRates);
                v.semi = v.target + (v.semi - v.target) * glideCoef;
                v.bendOff *= bendCoef;
                v.fenv *= fCoef;
                v.sweepEnv *= sweepCoef;
                v.vibTime += invOsr;
                v.vibPh += vibInc; if (v.vibPh >= 1.f) v.vibPh -= 1.f;
                if (a <= 0.f) continue;

                const float lfo = lfoBuf[(size_t) i];
                const float vibAmt = juce::jlimit (0.f, 1.f, v.vibTime / vibDelay);
                const float pitch = v.semi + v.bendOff + bendWheel + vibDepthSt * vibAmt * doomsday::dsp::fastSin01 (v.vibPh);
                const float hz = 440.f * std::exp2 ((pitch - 69.f) / 12.f);

                float l = 0.f, r = 0.f;
                v.core.render (tCore, hz, invOsr, l, r);
                if (p.coreOct > 0.001f) v.coreUp.render (tCoreUp, hz * 2.f, invOsr, l, r);
                if (p.edgeOn && p.edgeGain > 0.f)
                {
                    float el = 0.f, er = 0.f;
                    v.edge.render (tEdge, hz * (float) (1 << juce::jlimit (1, 2, p.edgeOct)), invOsr, el, er);
                    l += v.edgeHp[0].hp (el, v.hpc);
                    r += v.edgeHp[1].hp (er, v.hpc);
                }

                // coefficient updates every 16 samples: formants, LP, edge HP
                if (((v.counter + i) & 15) == 0)
                {
                    const float vw = juce::jlimit (0.f, 1.f, baseVowel + p.sweep * v.sweepEnv + p.moveVowel * lfo * 0.5f + velOpen);
                    const auto vo = vowelAt (vw);
                    const float shift = std::exp2 (p.talkKey * (pitch - 60.f) / 12.f);
                    for (int f = 0; f < 3; ++f)
                    {
                        v.fc[(size_t) f] = doomsday::dsp::svfCoefs (vo.f[f] * shift, 0.16f + 0.06f * (float) f, osr);
                        v.fg[(size_t) f] = vo.g[f];
                    }
                    const float oct = p.fenvOct * v.fenv + p.moveCut * lfo + 1.f * e + 0.5f * (pitch - 60.f) / 12.f;
                    v.lpc = doomsday::dsp::svfCoefs (juce::jlimit (30.f, (float) (osr * 0.45), p.cutoff * std::exp2 (oct)), k, osr);
                    v.hpc = doomsday::dsp::svfCoefs (p.edgeToneHz, 1.2f, osr);
                    lastVowel = vw;
                }

                // TALK: three formant band-passes blended with the dry sound
                const float talkAmt = juce::jlimit (0.f, 1.f, p.talk);
                lastTalk = talkAmt;
                if (talkAmt > 0.001f)
                {
                    float fl = 0.f, fr = 0.f;
                    for (int f = 0; f < 3; ++f)
                    {
                        const auto& c = v.fc[(size_t) f];
                        fl += v.formant[0][(size_t) f].bp (l, c) * c.k * v.fg[(size_t) f];
                        fr += v.formant[1][(size_t) f].bp (r, c) * c.k * v.fg[(size_t) f];
                    }
                    l += talkAmt * (fl * 1.9f - l);
                    r += talkAmt * (fr * 1.9f - r);
                }
                l = v.lp[0].process (l, 1, v.lpc);
                r = v.lp[1].process (r, 1, v.lpc);

                float b = 0.f, b2 = 0.f;
                if (p.bodyOn) v.body.render (tBody, hz * (p.bodyOct < 0 ? 0.5f : 1.f), invOsr, b, b2);

                float g = a * velG * (1.f - p.moveAmp * (0.5f - 0.5f * lfo));
                if (v.killStep > 0.f) g *= juce::jmax (0.f, v.kill - v.killStep * (float) i);
                oL[i] += (l + b) * g;
                oR[i] += (r + b2) * g;
            }
            v.counter += osLen;
            if (v.killStep > 0.f) { v.kill -= v.killStep * (float) osLen; if (v.kill <= 0.f) { v = {}; continue; } }
            if (v.amp.idle()) v = {};
        }
        liveVowel.store (lastVowel, std::memory_order_relaxed);
        liveTalk.store (lastTalk, std::memory_order_relaxed);

        // RAGE (oversampled, level matched)
        if (p.rageOn)
        {
            const float drv = juce::Decibels::decibelsToGain (p.rageDb + 10.f * e);
            const float mix = juce::jlimit (0.f, 1.f, p.rageMix + 0.25f * e);
            const float att = 1.f - std::exp (-1.f / (0.002f * (float) osr)), rel = 1.f - std::exp (-1.f / (0.1f * (float) osr));
            for (int i = 0; i < osLen; ++i)
            {
                float* c[] = { oL, oR };
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float x = c[ch][i];
                    float y;
                    switch (p.rageType)
                    {
                        case 0:  y = std::tanh (x * drv + 0.2f) - 0.1974f; break;                      // TUBE (asym)
                        case 1:  y = juce::jlimit (-0.8f, 0.8f, x * drv); break;                        // HARD
                        default: y = std::sin (juce::jlimit (-6.f, 6.f, x * drv * 1.2f)); break;        // FOLD
                    }
                    rz.dcY[(size_t) ch] = y - rz.dcX[(size_t) ch] + 0.9995f * rz.dcY[(size_t) ch];
                    rz.dcX[(size_t) ch] = y;
                    y = rz.dcY[(size_t) ch];
                    const float ax = std::abs (x), ay = std::abs (y);
                    rz.envIn[(size_t) ch] += (ax > rz.envIn[(size_t) ch] ? att : rel) * (ax - rz.envIn[(size_t) ch]);
                    rz.envOut[(size_t) ch] += (ay > rz.envOut[(size_t) ch] ? att : rel) * (ay - rz.envOut[(size_t) ch]);
                    const float m = rz.envOut[(size_t) ch] > 1.0e-5f ? juce::jlimit (0.25f, 4.f, rz.envIn[(size_t) ch] / rz.envOut[(size_t) ch]) : 1.f;
                    rz.match[(size_t) ch] += 0.001f * (m - rz.match[(size_t) ch]);
                    c[ch][i] = x + mix * (y * rz.match[(size_t) ch] - x);
                }
            }
        }
        os->processSamplesDown (block);

        float* L = chans[0];
        float* R = chans[1];

        // OTT: 3 bands pulled towards a target (gentle upward, firm downward)
        const float ottAmt = juce::jlimit (0.f, 1.f, p.ott + 0.35f * e);
        if (ottAmt > 0.001f)
        {
            const float aLo = doomsday::dsp::onePoleCoef (150.f, sr), aHi = doomsday::dsp::onePoleCoef (2500.f, sr);
            const float att = 1.f - std::exp (-1.f / (0.002f * (float) sr)), rel = 1.f - std::exp (-1.f / (0.08f * (float) sr));
            const float targets[3] = { 0.25f, 0.22f, 0.10f };
            for (int i = 0; i < len; ++i)
            {
                float* c[] = { L, R };
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float x = c[ch][i];
                    auto& bl = rz.bandLo[(size_t) ch]; auto& bh = rz.bandHi[(size_t) ch];
                    bl += aLo * (x - bl);
                    const float rest = x - bl;
                    bh += aHi * (rest - bh);
                    const float bands[3] = { bl, bh, rest - bh };
                    float y = 0.f;
                    for (int b = 0; b < 3; ++b)
                    {
                        auto& en = rz.bandEnv[(size_t) ch][(size_t) b];
                        const float ab = std::abs (bands[b]);
                        en += (ab > en ? att : rel) * (ab - en);
                        y += bands[b] * juce::jlimit (0.3f, 3.f, std::pow (targets[b] / juce::jmax (en, 0.01f), 0.6f));
                    }
                    c[ch][i] = x + ottAmt * (y - x);
                }
            }
        }

        // TONE (+-9 dB shelf above ~1.8 kHz) + ENERGY presence (up to +6 dB more)
        {
            const float g = juce::Decibels::decibelsToGain (9.f * p.tone + 6.f * e) - 1.f;
            if (std::abs (g) > 0.001f)
            {
                const float aP = doomsday::dsp::onePoleCoef (1800.f, sr);
                for (int i = 0; i < len; ++i)
                {
                    rz.presLp[0] += aP * (L[i] - rz.presLp[0]); L[i] += g * (L[i] - rz.presLp[0]);
                    rz.presLp[1] += aP * (R[i] - rz.presLp[1]); R[i] += g * (R[i] - rz.presLp[1]);
                }
            }
        }

        // WIDTH: side gain + mono below MONO BASS
        {
            const float sideG = juce::jlimit (0.f, 1.6f, 0.4f + 0.9f * p.width + 0.3f * e);
            const float hpC = doomsday::dsp::onePoleCoef (juce::jmax (10.f, p.monoBassHz), sr);
            for (int i = 0; i < len; ++i)
            {
                const float m = 0.5f * (L[i] + R[i]);
                float s = 0.5f * (L[i] - R[i]);
                if (p.monoBassHz > 20.f) { rz.sideLp += hpC * (s - rz.sideLp); s -= rz.sideLp; }
                s *= sideG;
                L[i] = m + s; R[i] = m - s;
            }
        }

        fxChain.process (L, R, len, p.ppq, offset);

        float* outL = buffer.getWritePointer (0, start);
        float* outR = buffer.getWritePointer (1, start);
        float peak = outPeak.load (std::memory_order_relaxed);
        for (int i = 0; i < len; ++i)
        {
            const float vol = sVol.getNextValue();
            outL[i] = doomsday::dsp::softClip (L[i] * vol);
            outR[i] = doomsday::dsp::softClip (R[i] * vol);
            peak = juce::jmax (peak, std::abs (outL[i]), std::abs (outR[i]));
        }
        outPeak.store (peak, std::memory_order_relaxed);
        feed.push (outL, outR, len);
    }

    struct RageState
    {
        std::array<float, 2> dcX {}, dcY {}, envIn {}, envOut {}, match { 1.f, 1.f }, bandLo {}, bandHi {};
        std::array<std::array<float, 3>, 2> bandEnv {};
        float sideLp = 0.f;
        std::array<float, 2> presLp {};
    } rz;

    SerpentParams p;
    double sr = 44100.0, osr = 88200.0;
    int maxBlockSize = 512, offset = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::AudioBuffer<float> bus;
    doomsday::dsp::FxChain fxChain;
    std::array<Voice, numSlots> voices;
    Bank::Table tCore, tCoreUp, tEdge, tBody;
    Env::Rates ampRates;
    float fCoef = 0.f, sweepCoef = 0.f, bendCoef = 0.f, glideCoef = 0.f, lfoInc = 0.f, lfoPhase = 0.f, lfoHold = 0.f, pitchWheel = 0.f;
    std::vector<float> lfoBuf;
    std::vector<int> held;
    int lastNote = -1;
    bool phraseOpen = false;
    juce::uint64 ageCounter = 0;
    juce::SmoothedValue<float> sVol;
    juce::Random rng;
};
