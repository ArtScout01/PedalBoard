// Referenz-Ausgabe der Pedale (Sinus 1 kHz / 400 Hz) zum Vergleich vor/nach Aenderungen.
// Aufruf: pedal_ref write <datei>   |   pedal_ref compare <datei>
#include "pb/PedalFactory.h"
#include <cstdio>
#include <cstring>
#include <complex>
using namespace pb;
static const double PI = 3.14159265358979323846;
struct Case { const char* n; Kind k; int sw; float kn[4]; double f, amp; };
static const Case C[] = {
  { "Fuzz Ge 0.6 / 1 kHz -20", Kind::FuzzFace, 0, { 0.6f, 0.5f, 0.5f, 0.5f }, 1000, 0.1 },
  { "Fuzz Ge 1.0 / 400 Hz -20", Kind::FuzzFace, 0, { 1.0f, 0.5f, 0.5f, 0.5f }, 400, 0.1 },
  { "Fuzz Si 0.6 / 1 kHz -10", Kind::FuzzFace, 1, { 0.6f, 0.5f, 0.5f, 0.5f }, 1000, 0.316 },
  { "TS808 0.6 / 1 kHz -20", Kind::TS808, 0, { 0.6f, 0.5f, 0.5f, 0.5f }, 1000, 0.1 },
  { "TS808 1.0 / 400 Hz -10", Kind::TS808, 0, { 1.0f, 0.7f, 0.5f, 0.5f }, 400, 0.316 },
  { "TS808 0.3 / 2 kHz -20", Kind::TS808, 0, { 0.3f, 0.3f, 0.6f, 0.5f }, 2000, 0.1 },
};
static std::vector<float> run (const Case& c)
{
    auto ped = make (c.k); ped->prepare (48000.f); std::vector<float> x (48000); for (int i = 0; i < 48000; ++i) x[(size_t) i] = (float) (c.amp * std::sin (2 * PI * c.f * i / 48000.0));
    P p; for (int j = 0; j < 4; ++j) p.k[j] = c.kn[j]; p.s[0] = c.sw; for (size_t i = 0; i < x.size(); i += 256) ped->process (&x[i], (int) std::min<size_t> (256, x.size() - i), p); return x;
}
int main (int argc, char** argv)
{
    if (argc < 3) return 1; const bool wr = ! std::strcmp (argv[1], "write");
    FILE* f = std::fopen (argv[2], wr ? "wb" : "rb"); if (! f) return 2;
    for (const auto& c : C)
    {
        auto y = run (c);
        if (wr) { std::fwrite (y.data(), 4, y.size(), f); continue; }
        std::vector<float> r (y.size()); if (std::fread (r.data(), 4, r.size(), f) != r.size()) return 3;
        auto harm = [&] (const std::vector<float>& v, int h) { std::complex<double> acc = 0; double w = 0; for (int i = 0; i < 24000; ++i) { const double ww = 0.5 - 0.5 * std::cos (2 * PI * i / 23999); acc += ww * v[(size_t) (24000 + i)] * std::polar (1.0, -2 * PI * c.f * h * i / 48000.0); w += ww; } return 2.0 * acc / w; };
        std::printf ("  %-26s", c.n);
        for (int h = 1; h <= 5; ++h) { if (c.f * h > 20000) break; auto a1 = harm (r, h), a2 = harm (y, h); std::printf ("  H%d %+5.2f dB", h, 20 * std::log10 (std::abs (a2) / std::max (std::abs (a1), 1e-12))); if (h == 1) std::printf (" (%+5.1f deg)", (std::arg (a2) - std::arg (a1)) * 180 / PI); }
        std::printf ("\n");
    }
    std::fclose (f); return 0;
}
