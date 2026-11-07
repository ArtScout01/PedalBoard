#pragma once
// JUCE-freie Routing-Hilfen (getrennt testbar): Eingang aufbereiten, Ketten je nach Routing durchlaufen, Trocken-Verzoegerung, Pan.
#include <algorithm>
#include <vector>

namespace pb
{
enum RouteMode { RouteSerial = 0, RouteOnlyA = 1, RouteOnlyR = 2, RouteSplit = 3 };

// a, b = Eingang der beiden Ketten. Split: a = L, b = R (stereoIn) bzw. L. Sonst: beide = Mono ((L+R)/2, bei Mono-Eingang L).
inline void prepareInput (int route, bool stereoIn, const float* inL, const float* inR, int n, float* a, float* b)
{
    if (route == RouteSplit) { std::copy (inL, inL + n, a); const float* s = (stereoIn && inR) ? inR : inL; std::copy (s, s + n, b); return; }
    if (inR) for (int i = 0; i < n; ++i) a[i] = 0.5f * (inL[i] + inR[i]);
    else std::copy (inL, inL + n, a);
    std::copy (a, a + n, b);
}

// chain (c, data, n) verarbeitet Kette c in-place. Nach dem Aufruf sind a und b bei allen Routen ausser Split identisch.
template <class ChainFn>
inline void runRoute (int route, int n, float* a, float* b, ChainFn&& chain)
{
    switch (route)
    {
        case RouteSplit: chain (0, a, n); chain (1, b, n); return;
        case RouteOnlyA: chain (0, a, n); break;
        case RouteOnlyR: chain (1, a, n); break;
        default:         chain (0, a, n); chain (1, a, n); break;       // seriell: Kette A, dann Kette R
    }
    std::copy (a, a + n, b);
}

// Stereo (Dual-Mono): L und R laufen getrennt durch je eine eigene Kette identischer Pedale. chain (c, side, data, n): side 0 = links, 1 = rechts.
inline void prepareInputStereo (const float* inL, const float* inR, int n, float* l, float* r)
{
    std::copy (inL, inL + n, l); const float* s = inR ? inR : inL; std::copy (s, s + n, r);
}
template <class ChainFn>
inline void runRouteStereo (int route, int n, float* l, float* r, ChainFn&& chain)
{
    auto both = [&] (int c) { chain (c, 0, l, n); chain (c, 1, r, n); };
    switch (route)
    {
        case RouteOnlyA: both (0); break;
        case RouteOnlyR: both (1); break;
        default:         both (0); both (1); break;       // seriell (Split gibt es im Stereo-Modus nicht)
    }
}

// Trockensignal um genau d Samples verzoegert (gleich der Latenz des nassen Pfads)
struct DryDelay
{
    std::vector<float> b; int w = 0, d = 0;
    void init (int delay) { d = std::max (0, delay); b.assign ((size_t) d + 1, 0.f); w = 0; }
    float process (float x)
    {
        if (b.empty()) return x;                       // vor init(): unverzoegert durchreichen
        b[(size_t) w] = x; const int sz = (int) b.size(); const float y = b[(size_t) ((w - d + sz) % sz)];
        if (++w >= sz) w = 0; return y;
    }
};

// Pan wie ein Balance-Regler: die abgewandte Seite wird leiser, die andere bleibt bei 1 (Mitte = unveraendert)
inline void panGains (float pan, float& gl, float& gr) { gl = pan > 0.f ? 1.f - pan : 1.f; gr = pan < 0.f ? 1.f + pan : 1.f; }
} // namespace pb
