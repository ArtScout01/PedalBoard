// Aliasing-Messung der Pedale: Sinus 5300 Hz, Anteil ausserhalb der Harmonischen (Aliase + Rauschen) relativ zur Grundwelle.
// Bauen: g++ -std=c++17 -O2 -I Source -I Source/pb Tests/alias_test.cpp -o alias_test && ./alias_test
#include "pb/PedalFactory.h"
#include <cstdio>
#include <complex>
#include <chrono>
using namespace pb;
static const double PI = 3.14159265358979323846;
static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1) { const auto w = std::polar (1.0, -2 * PI / (double) len); for (size_t i = 0; i < n; i += len) { std::complex<double> wn = 1; for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * wn; a[i + j] = u + v; a[i + j + len / 2] = u - v; wn *= w; } } }
}
static double aliasDb (Kind kind, const float* knobs, int sw, double f0, double amp, double fs, double* cpu = nullptr)
{
    auto ped = make (kind); ped->prepare ((float) fs);
    const int N = 1 << 16, warm = 24000; std::vector<float> x ((size_t) (N + warm));
    for (int i = 0; i < N + warm; ++i) x[(size_t) i] = (float) (amp * std::sin (2 * PI * f0 * i / fs));
    P pr; for (int j = 0; j < 4; ++j) pr.k[j] = knobs[j]; pr.s[0] = sw;
    auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < x.size(); i += 256) ped->process (&x[i], (int) std::min<size_t> (256, x.size() - i), pr);
    if (cpu) *cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / ((double) x.size() / fs) * 100.0;
    std::vector<std::complex<double>> a ((size_t) N);
    for (int i = 0; i < N; ++i) a[(size_t) i] = (0.5 - 0.5 * std::cos (2 * PI * i / (N - 1))) * x[(size_t) (warm + i)];
    fft (a); const double binHz = fs / N; double pf = 0, pr2 = 0;
    for (int k = 1; k < N / 2; ++k)
    {
        const double f = k * binHz; if (f < 100.0 || f > 20000.0) continue; const double pw = std::norm (a[(size_t) k]);
        bool harm = false; { const double hn = std::round (f / f0); if (hn >= 1.0 && std::fabs (f - hn * f0) < 6 * binHz) harm = true; }   // alle Harmonischen ausblenden
        if (std::fabs (f - f0) < 6 * binHz) pf += pw; else if (! harm) pr2 += pw;
    }
    return 10.0 * std::log10 (std::max (pr2, 1e-30) / std::max (pf, 1e-30));
}
int main()
{
    const double fs = 48000.0; std::printf ("%-16s  Aliasing/Rest re Grundwelle (dB), Sinus 5300 Hz, -10 dBFS   | CPU %% (1 Kern, 48 kHz)\n", "Pedal");
    for (int k = 0; k < (int) Kind::Count; ++k)
    {
        const Kind kd = (Kind) k; double worst = -200, cpu = 0, c2 = 0; const float cfgs[3][4] = { { 1, 1, 1, 1 }, { 0.5f, 0.5f, 0.5f, 0.5f }, { 1, 0.3f, 0.7f, 0.5f } };
        for (auto& c : cfgs) for (int sw = 0; sw < 2; ++sw) { const double a = aliasDb (kd, c, sw, 5300.0, 0.316, fs, &c2); worst = std::max (worst, a); cpu = std::max (cpu, c2); }
        std::printf ("%-16s  %7.1f dB %s | %5.1f\n", info (kd).name, worst, worst > -45 ? "  <-- hoch" : "", cpu);
    }
}
