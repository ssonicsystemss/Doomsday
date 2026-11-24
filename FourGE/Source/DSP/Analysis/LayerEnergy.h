#pragma once
// =====================================================================
//  4GE LAYER ENERGY (owner 2026-11-24, ARCH_DECISIONS.md "Energia warstw"): the energy of every element of the kick on its own,
//  not only the full kick's peak / RMS - so it shows at once why a CRUNCH sounds like "DUM" or like "SZHH".
//   LayerTap: the engine adds each layer's signal here (offline renders only: Engine::setLayerTap, never on the plugin's audio
//   thread). Layers: TOK / PUNCH / BODY / CRUNCH / SUB / TAIL / SCREAM.
//    PUNCH / BODY / TAIL / SUB = the part after its group processing x its fader (what reaches the mix, before the master clip);
//    TOK = the TOK generator; CRUNCH = RAW CRUNCH (punch type RAW) or the HARDCORE crunch (before the tail's 2nd drive);
//    SCREAM = the module 06 screech (it is ALSO inside the part it is routed to).
//   analyse(): per layer E = 1/N sum x^2 over the same window (the kick) -> dB, PEAK, and the energy in 5 bands
//    LOW 20-120 / LOW-MID 120-400 / MID 400-1.5k / HIGH-MID 1.5k-5k / HIGH 5k-16k (FFT power, dB of the band's share x E).
//   Pure DSP on buffers: no files, no GUI.
// =====================================================================
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>

namespace fge::dsp::energy
{
    enum Layer { Tok = 0, Punch, Body, Crunch, Sub, Tail, Scream, NumLayers };
    inline const char* layerName (int l) { static const char* n[] { "TOK", "PUNCH", "BODY", "CRUNCH", "SUB", "TAIL", "SCREAM" }; return n[l]; }
    constexpr int numBands = 5;
    inline const char* bandName (int b) { static const char* n[] { "LOW", "LOW-MID", "MID", "HIGH-MID", "HIGH" }; return n[b]; }
    inline constexpr float bandEdges[numBands + 1] { 20.f, 120.f, 400.f, 1500.f, 5000.f, 16000.f };

    struct LayerTap
    {
        std::array<std::vector<float>, NumLayers> buf;
        size_t base = 0;
        double rate = 0.0;                                       // the engine's (oversampled) rate, set by the engine
        bool used[NumLayers] {};
        void begin (int n) { base = buf[0].size(); for (auto& b : buf) b.resize (base + (size_t) n, 0.f); }   // a new block of n samples
        void add (int layer, int i, float v) { buf[(size_t) layer][base + (size_t) i] += v; if (v != 0.f) used[layer] = true; }
    };

    struct LayerReport
    {
        bool active = false;
        float db = -120.f, peakDb = -120.f;
        std::array<float, numBands> bandDb {};
    };
    struct Report { std::array<LayerReport, NumLayers> l; };

    inline float toDb (double e) { return (float) (10.0 * std::log10 (e + 1e-12)); }

    // rate = the tap's rate (the engine's oversampled rate); n = the window (samples) all layers are measured over
    inline Report analyse (const LayerTap& t, size_t n = 0)
    {
        const double rate = t.rate > 0.0 ? t.rate : 176400.0;
        if (n == 0) n = t.buf[0].size();
        Report r;
        for (int l = 0; l < NumLayers; ++l)
        {
            auto& o = r.l[(size_t) l];
            const auto& x = t.buf[(size_t) l];
            o.active = t.used[l];
            o.bandDb.fill (-120.f);
            if (! o.active || x.empty()) continue;
            const size_t len = std::min (n, x.size());
            double e = 0.0; float pk = 0.f;
            for (size_t i = 0; i < len; ++i) { e += (double) x[i] * x[i]; pk = std::max (pk, std::abs (x[i])); }
            e /= (double) std::max<size_t> (1, n);
            o.db = toDb (e); o.peakDb = (float) (20.0 * std::log10 (pk + 1e-9));
            int order = 1; while ((size_t) (1 << order) < len) ++order;           // the whole layer in one FFT (zero padded)
            const int fftN = 1 << order;
            juce::dsp::FFT fft (order);
            std::vector<float> d ((size_t) fftN * 2, 0.f);
            std::copy (x.begin(), x.begin() + (long) len, d.begin());
            fft.performFrequencyOnlyForwardTransform (d.data());
            double tot = 0.0; std::array<double, numBands> bp {};
            for (int k = 1; k <= fftN / 2; ++k)
            {
                const double p = (double) d[(size_t) k] * d[(size_t) k], hz = k * rate / fftN;
                tot += p;
                for (int b = 0; b < numBands; ++b) if (hz >= bandEdges[b] && hz < bandEdges[b + 1]) bp[(size_t) b] += p;
            }
            for (int b = 0; b < numBands; ++b) o.bandDb[(size_t) b] = tot > 0.0 ? toDb (e * bp[(size_t) b] / tot) : -120.f;
        }
        return r;
    }
}
