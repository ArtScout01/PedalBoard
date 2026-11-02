#pragma once
// Oversampling fuer nichtlineare Pedale: Polyphasen-IIR-Halbbandfilter (zwei Allpass-Zweige), minimale Latenz.
// Koeffizienten per numerischer Optimierung entworfen (Tests/design_halfband.py):
//   Stufe 1 (z.B. 48 -> 96 kHz): 5 Koeffizienten, Durchlass bis 20 kHz (Welligkeit < 0,001 dB), Sperrbereich ab 28 kHz mit 80 dB Daempfung.
//   Stufe 2 (96 -> 192 kHz): 3 Koeffizienten, Durchlass bis 24 kHz, Sperrbereich ab 72 kHz mit 89 dB Daempfung.
// Die Phase ist nichtlinear (IIR), die Gruppenlaufzeit bei tiefen Frequenzen betraegt nur wenige Samples.
#include <algorithm>
#include <memory>

namespace pb
{
struct AllpassChain
{
    static constexpr int kMax = 4;
    float a[kMax] = {}, x1[kMax] = {}, y1[kMax] = {}; int n = 0;
    void set (const float* c, int cnt) { n = std::min (cnt, kMax); for (int i = 0; i < n; ++i) a[i] = c[i]; reset(); }
    void reset() { for (int i = 0; i < kMax; ++i) x1[i] = y1[i] = 0.f; }
    float run (float x)
    {
        for (int i = 0; i < n; ++i) { const float y = a[i] * (x - y1[i]) + x1[i]; x1[i] = x; y1[i] = y; x = y; }
        return x;
    }
};

// 2x-Halbband: H(z) = 0.5 * (A0(z^2) + z^-1 A1(z^2)); Zweig 0 = gerade, Zweig 1 = ungerade Koeffizienten
struct Halfband2x
{
    AllpassChain upA, upB, dnA, dnB; float dnPrev = 0.f;
    void set (const float* c, int n)
    {
        float e[AllpassChain::kMax], o[AllpassChain::kMax]; int ne = 0, no = 0;
        for (int i = 0; i < n; ++i) { if (i % 2 == 0) e[ne++] = c[i]; else o[no++] = c[i]; }
        upA.set (e, ne); upB.set (o, no); dnA.set (e, ne); dnB.set (o, no); dnPrev = 0.f;
    }
    void reset() { upA.reset(); upB.reset(); dnA.reset(); dnB.reset(); dnPrev = 0.f; }
    void up (float x, float& o0, float& o1) { o0 = upA.run (x); o1 = upB.run (x); }
    float down (float v0, float v1) { const float y = 0.5f * (dnA.run (v0) + dnB.run (dnPrev)); dnPrev = v1; return y; }
};

struct Oversampler
{
    static constexpr int kChunk = 64;
    int factor = 1; Halfband2x s1, s2;
    void init (int f)
    {
        static const float c1[5] = { 0.061085323219f, 0.221049593525f, 0.430443141198f, 0.650947940099f, 0.876649877575f };
        static const float c2[3] = { 0.070247484896f, 0.285145980399f, 0.684587908588f };
        factor = f == 4 ? 4 : (f == 2 ? 2 : 1); s1.set (c1, 5); s2.set (c2, 3);
    }
    void reset() { s1.reset(); s2.reset(); }
    // f (float* hi, int nHi) bearbeitet den hochgetasteten Block in-place; io wird in-place ersetzt
    template <class F> void process (float* io, int n, F&& f)
    {
        float lo[2 * kChunk], hi[4 * kChunk];
        for (int pos = 0; pos < n; pos += kChunk)
        {
            const int m = std::min (kChunk, n - pos); float* x = io + pos;
            if (factor == 1) { f (x, m); continue; }
            if (factor == 2)
            {
                for (int i = 0; i < m; ++i) s1.up (x[i], hi[2 * i], hi[2 * i + 1]);
                f (hi, 2 * m);
                for (int i = 0; i < m; ++i) x[i] = s1.down (hi[2 * i], hi[2 * i + 1]);
            }
            else
            {
                for (int i = 0; i < m; ++i) s1.up (x[i], lo[2 * i], lo[2 * i + 1]);
                for (int j = 0; j < 2 * m; ++j) s2.up (lo[j], hi[2 * j], hi[2 * j + 1]);
                f (hi, 4 * m);
                for (int j = 0; j < 2 * m; ++j) lo[j] = s2.down (hi[2 * j], hi[2 * j + 1]);
                for (int i = 0; i < m; ++i) x[i] = s1.down (lo[2 * i], lo[2 * i + 1]);
            }
        }
    }
};
} // namespace pb
