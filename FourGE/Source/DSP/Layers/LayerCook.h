#pragma once
// =====================================================================
//  4GE module 04 - parameters -> cooked settings of the noise generators, the transient engine and the layers
//  (groups, solo / mute, LINK chains with loop protection, routing). Pure, no allocation; used by the engine per block.
// =====================================================================
#include "Layers.h"
#include "../../Parameters/ParamFrame.h"

namespace fge::dsp
{
    inline void cookLayerBank (const ParamFrame& f, const env::Bank& eb, LayerBankSettings& out, double rate)
    {
        auto db = [] (float v) { return v <= -59.9f ? 0.f : std::pow (10.f, v / 20.f); };
        // ---- noise A / B
        const int ns = P::noiseNBOn - P::noiseNAOn;
        out.anyNoise = false;
        for (int k = 0; k < 2; ++k)
        {
            auto at = [&] (int first) { return first + k * ns; };
            auto& n = out.noise[(size_t) k];
            n.on = f.on (at (P::noiseNAOn));
            out.anyNoise |= n.on;
            n.kind = f.idx (at (P::noiseNAType)); n.gain = db (f[at (P::noiseNALevel)]); n.color = f[at (P::noiseNAColor)];
            n.filter = f.idx (at (P::noiseNAFilter)); n.cutoff = f[at (P::noiseNACutoff)]; n.reso = f[at (P::noiseNAReso)]; n.drive = f[at (P::noiseNADrive)];
            n.attackMs = f[at (P::noiseNAAttack)]; n.decayMs = f[at (P::noiseNADecay)]; n.lengthMs = f[at (P::noiseNALength)];
            n.mix = f[at (P::noiseNAMix)]; n.width = f[at (P::noiseNAWidth)]; n.track = f.on (at (P::noiseNATrack)); n.envSrc = f.idx (at (P::noiseNAEnv));
            n.resHz = f[at (P::noiseNAResHz)]; n.resQ = f[at (P::noiseNAResQ)]; n.resAmt = f[at (P::noiseNAResAmt)]; n.metal = f[at (P::noiseNAMetal)];
            n.bits = f[at (P::noiseNABits)]; n.srr = f[at (P::noiseNASrr)]; n.quant = f[at (P::noiseNAQuant)]; n.grain = f[at (P::noiseNAGrain)];
            n.seed = (uint32_t) std::lround (f[at (P::noiseNASeed)]); n.randMode = f.idx (at (P::noiseNARMode)); n.randAmt = f[at (P::noiseNARAmt)];
            const int to[4] { P::noiseNAToPunch, P::noiseNAToBody, P::noiseNAToTail, P::noiseNAToSub };
            for (int p = 0; p < 4; ++p) n.route[(size_t) p] = db (f[at (to[p])]);
            n.toTexture = db (f[at (P::noiseNAToTex)]);
            n.modOn = f.on (at (P::noiseNAModOn)); n.modDrawn = f.idx (at (P::noiseNAModMode)) == 1;
            n.modA = f[at (P::noiseNAModA)]; n.modD = f[at (P::noiseNAModD)];
            n.mLevel = f[at (P::noiseNAModLvl)]; n.mCut = f[at (P::noiseNAModCut)]; n.mRes = f[at (P::noiseNAModRes)]; n.mDrive = f[at (P::noiseNAModDrv)];
            n.mColor = f[at (P::noiseNAModCol)]; n.mWidth = f[at (P::noiseNAModWid)]; n.mGrain = f[at (P::noiseNAModGrn)];
            n.ampShape = &eb.s[(size_t) (env::AmpNoiseA + k)]; n.modShape = &eb.s[(size_t) (env::ModNoiseA + k)]; n.profile = &eb.s[(size_t) (env::ProfileNoiseA + k)];
            n.cookFilters (rate);
            auto& ln = out.layerNoise[(size_t) k];                    // for layers: the source only (unity gain, the layer's envelope)
            ln = n; ln.gain = 1.f; ln.envSrc = NePunch; ln.modOn = n.modOn;
        }
        // ---- transient
        auto& c = out.click;
        c.on = f.on (P::trOn); c.source = f.idx (P::trSource); c.gain = db (f[P::trClickLvl]); c.lenMs = f[P::trClickLen]; c.hz = f[P::trClickHz];
        c.tone = f[P::trClickTone]; c.drive = f[P::trClickDrv]; c.mix = f[P::trClickMix]; c.width = f[P::trWidth];
        c.positionMs = f[P::trPosition]; c.sync = f.idx (P::trSync); c.output = f.idx (P::trOutput); c.custom = &eb.s[env::TransientCustom];
        out.layerClick = c; out.layerClick.gain = 1.f; out.layerClick.positionMs = 0.f;
        auto& sh = out.shaper;
        sh.atkDb = f.on (P::trOn) ? f[P::trAtkLevel] : 0.f; sh.lowDb = f.on (P::trOn) ? f[P::trLow] : 0.f; sh.highDb = f.on (P::trOn) ? f[P::trHigh] : 0.f;
        sh.atkMs = f[P::trAtkTime]; sh.decMs = f[P::trDecTime];
        sh.shaper = f.on (P::shOn); sh.attack = f[P::shAttack]; sh.sustain = f[P::shSustain]; sh.sens = f[P::shSens]; sh.timeMs = f[P::shTime]; sh.mix = f[P::shMix];
        // ---- groups
        const int gs = P::grpBodyOn - P::grpPunchOn;
        struct G { bool on, mute, solo; float gain, pan, width; int out; };
        std::array<G, NumGroups> g {};
        bool groupSolo = false;
        for (int k = 0; k < NumGroups; ++k)
        {
            auto at = [&] (int first) { return first + k * gs; };
            g[(size_t) k] = { f.on (at (P::grpPunchOn)), f.on (at (P::grpPunchMute)), f.on (at (P::grpPunchSolo)), db (f[at (P::grpPunchLevel)]),
                              f[at (P::grpPunchPan)], f[at (P::grpPunchWidth)], f.idx (at (P::grpPunchOut)) };
            groupSolo |= g[(size_t) k].solo;
        }
        out.texGain = db (f[P::busTextureLvl]); out.custGain = db (f[P::busCustomLvl]);
        // ---- layers
        const int ls = P::layer2On - P::layer1On;
        auto L = [&] (int i, int first) { return first + i * ls; };
        bool layerSolo = false;
        for (int i = 0; i < numLayers; ++i) layerSolo |= f.on (L (i, P::layer1On)) && f.on (L (i, P::layer1Solo));
        const bool soloSource = f.idx (P::layerSoloMode) == 1;
        // LINK: the root a layer follows (a loop falls back to INDEPENDENT)
        auto linkRoot = [&] (int i) -> int
        {
            int q = i, steps = 0;
            while (steps < numLayers)
            {
                const int src = f.idx (L (q, P::layer1LinkSrc)) - 1;
                if (src < 0 || src == q) break;
                q = src; ++steps;
                if (q == i) return -1;                                 // loop
            }
            return q == i ? -1 : q;
        };
        out.anyLayer = false;
        for (int i = 0; i < numLayers; ++i)
        {
            auto& s = out.layer[(size_t) i];
            s.on = f.on (L (i, P::layer1On));
            if (! s.on) continue;
            out.anyLayer = true;
            const int root = linkRoot (i);
            auto val = [&] (int first, bool linked, auto combine) { const float own = f[L (i, first)]; return root >= 0 && linked ? combine (own, f[L (root, first)], def (first).def) : own; };
            const bool lLvl = f.on (L (i, P::layer1LinkLvl)), lPit = f.on (L (i, P::layer1LinkPit)), lLen = f.on (L (i, P::layer1LinkLen)), lEnv = f.on (L (i, P::layer1LinkEnv));
            const int envFrom = root >= 0 && lEnv ? root : i;
            s.source = f.idx (L (i, P::layer1Src));
            s.group = f.idx (L (i, P::layer1Group));
            const auto& gr = g[(size_t) s.group];
            s.track = s.group <= LgSub ? s.group : LgTail;
            const float lvl = val (P::layer1Level, lLvl, [] (float own, float m, float d) { return own + (m - d); });
            s.groupGain = gr.gain;
            s.fader = db (juce::jmin (12.f, lvl)) * gr.gain * (f.on (L (i, P::layer1Inv)) ? -1.f : 1.f);
            const float pan = juce::jlimit (-1.f, 1.f, f[L (i, P::layer1Pan)] + gr.pan);
            const float a = (pan + 1.f) * juce::MathConstants<float>::pi * 0.25f;
            s.panL = std::cos (a) * 1.41421356f; s.panR = std::sin (a) * 1.41421356f;
            s.side = f[L (i, P::layer1Width)] * gr.width;
            const float st = val (P::layer1Pitch, lPit, [] (float own, float m, float) { return own + m; });
            s.pitchMul = std::exp2 (st / 12.f + f[L (i, P::layer1Fine)] / 1200.f);
            const float ts = f[L (envFrom, P::layer1TScale)];
            s.lenMs = val (P::layer1Len, lLen, [] (float own, float m, float d) { return own * m / d; }) * ts;
            s.offsetMs = f[L (i, P::layer1Offset)];
            s.phase01 = f[L (i, P::layer1Phase)] / 360.f;
            s.fltOn = f.on (L (i, P::layer1FltOn)); s.fltType = f.idx (L (i, P::layer1FltType));
            s.flt = doomsday::dsp::svfCoefs (f[L (i, P::layer1Cutoff)], 1.f / (0.7f + std::pow (f[L (i, P::layer1Reso)], 2.f) * 12.f), rate);
            s.drive = f[L (i, P::layer1Drive)];
            s.envMode = f.idx (L (envFrom, P::layer1EnvMode));
            s.atk = f[L (envFrom, P::layer1Atk)] * ts; s.dec = f[L (envFrom, P::layer1Dec)] * ts; s.sus = f[L (envFrom, P::layer1Sus)];
            s.rel = f[L (envFrom, P::layer1Rel)] * ts; s.delay = f[L (envFrom, P::layer1Delay)]; s.amount = f[L (envFrom, P::layer1EnvAmt)];
            s.shape = &eb.s[(size_t) (env::AmpLayer1 + envFrom)];
            s.multi = f.idx (L (i, P::layer1Route)) == 1; s.preFader = f.on (L (i, P::layer1PreFdr));
            s.outBus = gr.out;
            const int sends[NumBus] { P::layer1SPunch, P::layer1SBody, P::layer1STail, P::layer1SSub, P::layer1STex, P::layer1SCust, P::layer1SMaster };
            for (int b = 0; b < NumBus; ++b) s.send[(size_t) b] = db (f[L (i, sends[b])]);
            const bool soloed = f.on (L (i, P::layer1Solo));
            s.audible = gr.on && ! gr.mute && ! f.on (L (i, P::layer1Mute)) && (! layerSolo || soloed) && (! groupSolo || gr.solo);
            s.sourceOnly = soloSource && layerSolo && soloed;
        }
        for (int i = numLayers; i < maxLayers; ++i) out.layer[(size_t) i].on = false;
    }
}
