#pragma once
// =====================================================================
//  4GE - one snapshot of every parameter's (real, final) value, read once per block.
//  The DSP only ever sees a ParamFrame (index = P::Id): no string lookups, no allocation on the audio thread.
//  Module 09 will compute FINAL = BASE + modulation into a frame like this one (the base values stay untouched).
// =====================================================================
#include "ParamRegistry.h"
#include <atomic>
#include <cmath>

namespace fge
{
    struct ParamFrame
    {
        std::array<float, P::count> v {};
        inline float operator[] (int id) const { return v[(size_t) id]; }
        inline float& operator[] (int id) { return v[(size_t) id]; }
        inline bool on (int id) const { return v[(size_t) id] > 0.5f; }
        inline int idx (int id) const { return (int) std::lround (v[(size_t) id]); }

        static ParamFrame defaults()
        {
            ParamFrame f;
            for (int i = 0; i < P::count; ++i) f.v[(size_t) i] = def (i).def;
            return f;
        }
    };

    // the APVTS raw value pointers, recorded once (message thread) - gather() only loads atomics
    struct ParamPointers
    {
        std::array<std::atomic<float>*, P::count> ptr {};
        inline void gather (ParamFrame& f) const
        {
            for (size_t i = 0; i < ptr.size(); ++i) if (ptr[i] != nullptr) f.v[i] = ptr[i]->load (std::memory_order_relaxed);
        }
    };
}
