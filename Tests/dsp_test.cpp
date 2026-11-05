// JUCE-freier Messlauf fuer Pedale und Tape-Stufe.
// Bauen:  g++ -std=c++17 -O2 -I Source -I Source/pb Tests/dsp_test.cpp -o dsp_test && ./dsp_test
#include "pb/PedalFactory.h"
#include "pb/Tape.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <complex>
#include <type_traits>
using namespace pb;
static const double PI = 3.14159265358979323846;
static double dB (double x) { return 20.0 * std::log10 (std::max (x, 1e-12)); }

static double goertzel (const std::vector<float>& x, int start, int n, double f, double fs)   // Amplitude mit Hann-Fenster
{
    std::complex<double> acc = 0; double wsum = 0;
    for (int i = 0; i < n; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * PI * i / (n - 1)); acc += w * x[(size_t) (start + i)] * std::polar (1.0, -2 * PI * f * i / fs); wsum += w; }
    return 2.0 * std::abs (acc) / wsum;
}
static double rmsOf (const std::vector<float>& x, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) x[(size_t) i] * x[(size_t) i]; return std::sqrt (s / (b - a)); }
static std::vector<float> sine (double f, double amp, int n, double fs) { std::vector<float> v ((size_t) n); for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (amp * std::sin (2 * PI * f * i / fs)); return v; }

static std::vector<float> runTape (double fs, const TapeParams& p, const std::vector<float>& in)
{
    TapeMachine t; t.prepare ((float) fs);
    std::vector<float> L (in), R (in.size(), 0.f);
    for (size_t i = 0; i < in.size(); i += 512) { const int n = (int) std::min<size_t> (512, in.size() - i); t.process (&L[i], &R[i], n, p); }
    return L;
}
static TapeParams clean (int machine, int speed) { TapeParams p; p.machine = machine; p.speed = speed; p.wf = 0.f; p.bump = 0.f; p.xtalk = 0.f; p.noise = false; return p; }

int main()
{
    const double fs = 48000.0; const float A0 = 0.126f;
    int fails = 0;
    auto check = [&] (bool ok, const char* msg) { std::printf ("  [%s] %s\n", ok ? "ok" : "FEHLER", msg); if (! ok) ++fails; };

    std::printf ("=== 1. Pedale: Endlichkeit und Pegel (Sinus 440 Hz, -23 dBFS RMS, 1 s) ===\n");
    for (int k = 0; k < (int) Kind::Count; ++k)
    {
        const Kind kind = (Kind) k; const auto& inf = info (kind);
        bool finite = true; double peak = 0, rmsDefault = 0;
        const float cfg[5][4] = { { .5f, .5f, .5f, .5f }, { 0, 0, 0, 0 }, { 1, 1, 1, 1 }, { 1, 0, 1, 0 }, { 0, 1, 0, 1 } };
        for (int c = 0; c < 5; ++c) for (int sw = 0; sw < 2; ++sw)
        {
            auto ped = make (kind); ped->prepare ((float) fs);
            P pr; for (int j = 0; j < 4; ++j) pr.k[j] = cfg[c][j]; pr.s[0] = sw;
            auto x = sine (440.0, 0.1, 48000, fs);
            for (size_t i = 0; i < x.size(); i += 256) ped->process (&x[i], (int) std::min<size_t> (256, x.size() - i), pr);
            for (float v : x) { if (! std::isfinite (v)) finite = false; peak = std::max (peak, (double) std::fabs (v)); }
            if (c == 0 && sw == 0) rmsDefault = rmsOf (x, 12000, 48000);
        }
        std::printf ("  %-16s  Ausgang bei Mittelstellung: %6.1f dBFS RMS, Spitze max. %5.2f  %s\n", inf.name, dB (rmsDefault), peak, finite ? "" : "NaN/Inf!");
        check (finite && peak < 30.0, inf.name);
    }

    std::printf ("\n=== 2. Tape: Klirr bei 400 Hz (Handbuch: 0 VU = 1 %%, +6 dB = 3 %% 3. Harmonische) ===\n");
    for (int m = 0; m < 2; ++m)
        for (double lvl : { 0.0, 6.0 })
        {
            auto p = clean (m, 0); const double amp = A0 * std::pow (10.0, lvl / 20.0);
            auto y = runTape (fs, p, sine (400.0, amp, 48000, fs)); const int s0 = 12000, n = 24000;
            const double h1 = goertzel (y, s0, n, 400, fs); double sq = 0; double h[6] = {};
            for (int j = 2; j <= 5; ++j) { h[j] = goertzel (y, s0, n, 400.0 * j, fs); sq += h[j] * h[j]; }
            std::printf ("  %-12s %+3.0f VU: Pegel aus/ein %+5.2f dB, THD %.2f %%, H3 %.2f %%, H2 %.2f %%\n", m ? "Revox B77" : "TEAC A-3340S", lvl, dB (h1 / amp), 100 * std::sqrt (sq) / h1, 100 * h[3] / h1, 100 * h[2] / h1);
        }

    std::printf ("\n=== 3. Tape: Frequenzgang (-20 VU, Bump 0, Crosstalk 0), bezogen auf 1 kHz ===\n");
    const double fr[] = { 30, 40, 100, 1000, 10000, 15000, 16000, 18000, 20000, 22000 };
    struct Cfg { int m, s; const char* n; double lo, hi; } cf[] = { { 0, 0, "TEAC 15 ips", 30, 22000 }, { 0, 1, "TEAC 7,5 ips", 40, 18000 }, { 1, 0, "Revox 15 ips", 30, 22000 }, { 1, 1, "Revox 7,5 ips", 30, 20000 }, { 1, 2, "Revox 3,75 ips", 30, 16000 } };
    for (auto& c : cf)
    {
        std::printf ("  %-15s", c.n); double ref = 0; std::vector<double> g;
        for (double f : fr) { auto y = runTape (fs, clean (c.m, c.s), sine (f, A0 * 0.1, 48000, fs)); g.push_back (goertzel (y, 12000, 24000, f, fs) / (A0 * 0.1)); if (f == 1000) ref = g.back(); }
        for (size_t i = 0; i < g.size(); ++i) std::printf (" %6.0f:%+5.1f", fr[i], dB (g[i] / ref));
        std::printf ("\n");
        // Spezifikationsgrenzen +-3 dB an den Eckfrequenzen pruefen
        for (size_t i = 0; i < g.size(); ++i) if (fr[i] == c.lo || fr[i] == c.hi) { char b[96]; std::snprintf (b, sizeof b, "%s bei %.0f Hz innerhalb +-3 dB (%.1f dB)", c.n, fr[i], dB (g[i] / ref)); check (std::fabs (dB (g[i] / ref)) <= 3.2, b); }
    }


    std::printf ("\n=== 3b. Tape: TEAC 15 ips bei verschiedenen Abtastraten (re 1 kHz) ===\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("  %6.0f Hz:", sr); double ref = 0;
        for (double f : { 1000.0, 15000.0, 18000.0, 20000.0, 22000.0 }) { if (f >= 0.5 * sr) continue; const int N = (int) sr; auto y = runTape (sr, clean (0, 0), sine (f, A0 * 0.1, N, sr)); const double g = goertzel (y, N / 4, N / 2, f, sr) / (A0 * 0.1); if (f == 1000) ref = g; std::printf (" %6.0f:%+5.1f", f, dB (g / ref)); if (f == 22000 && sr >= 48000) check (std::fabs (dB (g / ref)) <= 3.2, "22 kHz innerhalb +-3 dB"); }
        std::printf ("\n");
    }

    std::printf ("\n=== 4. Tape: Rauschen (kein Signal) bezogen auf 0 VU ===\n");
    for (auto& c : cf)
    {
        auto p = clean (c.m, c.s); p.noise = true; std::vector<float> z (96000, 0.f); auto y = runTape (fs, p, z);
        const double r = dB (rmsOf (y, 12000, 96000) / (A0 / std::sqrt (2.0)));
        TapeMachine t; t.prepare ((float) fs); t.configure (c.m, c.s);
        std::printf ("  %-15s gemessen %+6.1f dB re 0 VU (Soll nach Code %+5.1f)\n", c.n, r, t.noiseDb);
    }

    std::printf ("\n=== 5. Tape: Wow & Flutter (3 kHz, Regler 0,5 => Sollwert = wfSpec, ungewichtet RMS) ===\n");
    for (auto& c : cf)
    {
        auto p = clean (c.m, c.s); p.wf = 0.5f; const int N = 48000 * 10; auto y = runTape (fs, p, sine (3000.0, 0.1, N, fs));
        std::vector<double> I (N), Q (N); OnePole fi[6], fq[6]; for (int k = 0; k < 6; ++k) { fi[k].set (1200.f, (float) fs); fq[k].set (1200.f, (float) fs); }
        for (int i = 0; i < N; ++i) { const double ph = 2 * PI * 3000.0 * i / fs; float vi = y[(size_t) i] * (float) std::cos (ph), vq = y[(size_t) i] * (float) -std::sin (ph); for (int k = 0; k < 6; ++k) { vi = fi[k].lp (vi); vq = fq[k].lp (vq); } I[(size_t) i] = vi; Q[(size_t) i] = vq; }
        double prev = std::atan2 (Q[20000], I[20000]), acc = 0; int cnt = 0;
        for (int i = 20001; i < N; ++i) { double ph = std::atan2 (Q[(size_t) i], I[(size_t) i]); double d = ph - prev; while (d > PI) d -= 2 * PI; while (d < -PI) d += 2 * PI; prev = ph; const double dev = d * fs / (2 * PI) / 3000.0; acc += dev * dev; ++cnt; }
        TapeMachine t; t.prepare ((float) fs); t.configure (c.m, c.s);
        std::printf ("  %-15s gemessen %.3f %% RMS (Sollwert %.2f %%)\n", c.n, 100 * std::sqrt (acc / cnt), t.wfSpec);
    }

    std::printf ("\n=== 6. Tape: Latenz (1 kHz Kreuzkorrelation) gegen latency() ===\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        TapeMachine t; t.prepare ((float) sr); const int N = (int) sr; std::vector<float> x ((size_t) N); uint32_t r = 777u;
        for (auto& v : x) { r ^= r << 13; r ^= r >> 17; r ^= r << 5; v = A0 * 0.1f * (float) ((int32_t) r) / 2147483648.f; }
        auto y = runTape (sr, clean (0, 0), x);
        double best = -1e9; int bl = 0; for (int lag = 0; lag < 400; ++lag) { double s = 0; for (int i = 6000; i < 30000; ++i) s += (double) x[(size_t) i] * y[(size_t) (i + lag)]; if (s > best) { best = s; bl = lag; } }
        std::printf ("  %6.0f Hz: gemessen %d Samples, latency() = %d\n", sr, bl, t.latency());
        check (std::abs (bl - t.latency()) <= 1, "Latenz stimmt (+-1 Sample)");
    }

    std::printf ("\n=== 7. Tape: Band-Typ / Bias- / EQ-Schalter (TEAC 15 ips, Pegel bei 10 kHz und 15 kHz re 1 kHz) ===\n");
    for (int ty = 0; ty < 2; ++ty) for (int b = 0; b < 2; ++b) for (int e = 0; e < 2; ++e)
    {
        auto p = clean (0, 0); p.tapeType = ty; p.biasSw = b; p.eqSw = e; double ref = 0, g10 = 0, g15 = 0;
        for (double f : { 1000.0, 10000.0, 15000.0 }) { auto y = runTape (fs, p, sine (f, A0 * 0.1, 48000, fs)); const double g = goertzel (y, 12000, 24000, f, fs) / (A0 * 0.1); if (f == 1000) ref = g; if (f == 10000) g10 = g; if (f == 15000) g15 = g; }
        const bool matched = (b == ty) && (e == ty);
        std::printf ("  Band %-9s Bias %-6s EQ %-6s : 10 kHz %+5.1f dB, 15 kHz %+5.1f dB  %s\n", ty ? "LowNoise" : "Standard", b ? "HIGH" : "NORMAL", e ? "HIGH" : "NORMAL", dB (g10 / ref), dB (g15 / ref), matched ? "(passend)" : "");
    }

    std::printf ("\n=== 8. Tape: Hysterese (Jiles-Atherton) gegen statisches tanh ===\n");
    {
        auto runCfg = [&] (bool hys, int m, int sp, double f, double vu, double& g, double& thd, double& ph)
        {
            TapeMachine t; t.prepare ((float) fs); t.useHys = hys; auto p = clean (m, sp); const double amp = A0 * std::pow (10.0, vu / 20.0);
            std::vector<float> L (48000), R (48000, 0.f); for (int i = 0; i < 48000; ++i) L[(size_t) i] = (float) (amp * std::sin (2 * PI * f * i / fs));
            for (size_t i = 0; i < L.size(); i += 512) { const int n = (int) std::min<size_t> (512, L.size() - i); t.process (&L[i], &R[i], n, p); }
            std::complex<double> h1 = 0, acc; double wsum = 0; double sq = 0;
            for (int j = 1; j <= 5; ++j) { acc = 0; wsum = 0; const double fj = f * j; if (fj >= 0.45 * fs) break; for (int i = 0; i < 24000; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * PI * i / 23999); acc += w * L[(size_t) (12000 + i)] * std::polar (1.0, -2 * PI * fj * i / fs); wsum += w; } acc *= 2.0 / wsum; if (j == 1) h1 = acc; else sq += std::norm (acc); }
            g = std::abs (h1) / amp; thd = std::sqrt (sq) / std::abs (h1); ph = std::arg (h1);
        };
        std::printf ("  Vergleich bei 400 Hz (THD %%): Pegel      tanh   Hysterese | Revox 15 ips tanh/Hysterese\n");
        for (double vu : { -20.0, -10.0, 0.0, 6.0, 9.0 })
        {
            double g1, t1, p1, g2, t2, p2, g3, t3, p3, g4, t4, p4; runCfg (false, 0, 0, 400, vu, g1, t1, p1); runCfg (true, 0, 0, 400, vu, g2, t2, p2); runCfg (false, 1, 0, 400, vu, g3, t3, p3); runCfg (true, 1, 0, 400, vu, g4, t4, p4);
            std::printf ("     %+4.0f VU:  TEAC %5.2f   %5.2f   (Pegel %+5.2f / %+5.2f dB) | Revox %5.2f / %5.2f\n", vu, 100 * t1, 100 * t2, 20 * std::log10 (g1), 20 * std::log10 (g2), 100 * t3, 100 * t4);
            if (vu == 0.0) { check (std::fabs (100 * t2 - 1.07) < 0.15, "Hysterese: TEAC 0 VU = 1 % Klirr (+-0,15)"); check (std::fabs (20 * std::log10 (g2 / g1)) < 0.3, "Hysterese: Pegel bei 0 VU wie tanh (+-0,3 dB)"); }
            if (vu == 6.0) check (std::fabs (100 * t2 - 3.0) < 1.0, "Hysterese: TEAC +6 VU etwa 3 % Klirr");
        }
        double g, th, ph0, ph1; runCfg (true, 0, 0, 400, -30, g, th, ph0); runCfg (true, 0, 0, 400, 0, g, th, ph1);
        std::printf ("  Gedaechtnis: Phase der Grundwelle bei 0 VU gegen -30 VU: %+.2f Grad (tanh: 0)\n", (ph1 - ph0) * 180 / PI);
        check (std::fabs (ph1 - ph0) > 0.001, "Hysterese: pegelabhaengige Phase (Gedaechtnis vorhanden)");
        // Stabilitaet: extreme Pegel und Frequenzen, beide Maschinen, alle Geschwindigkeiten
        bool finite = true; double mx = 0;
        for (int m = 0; m < 2; ++m) for (int sp = 0; sp < (m ? 3 : 2); ++sp) for (double f : { 50.0, 400.0, 5000.0, 15000.0, 20000.0 }) for (double vu : { 0.0, 12.0, 24.0 })
        { auto p = clean (m, sp); p.driveDb = 0.f; const double amp = A0 * std::pow (10.0, vu / 20.0); auto y = runTape (fs, p, sine (f, amp, 24000, fs)); for (float v : y) { if (! std::isfinite (v)) finite = false; mx = std::max (mx, (double) std::fabs (v)); } }
        char b[96]; std::snprintf (b, sizeof b, "Hysterese stabil bei +24 VU, 20 kHz, allen Maschinen (Spitze %.2f)", mx); check (finite && mx < 10.0, b);
        // Sprung / Impuls / Stille
        auto z = runTape (fs, clean (0, 0), std::vector<float> (24000, 0.f)); double zm = 0; for (float v : z) zm = std::max (zm, (double) std::fabs (v)); check (zm < 1e-4, "Stille bleibt still (kein Gleichanteil/Rauschen der Hysterese)");
        // Revox-Klirr gegen Herstellerangabe (7,5 ips: 0 VU < 0,6 %, +6 dB < 1,5 %)
        double ga, ta, pa, gb, tb, pb2; runCfg (true, 1, 1, 400, 0, ga, ta, pa); runCfg (true, 1, 1, 400, 6, gb, tb, pb2);
        std::printf ("  Revox 7,5 ips mit Hysterese: 0 VU %.2f %%, +6 VU %.2f %% (Angabe: < 0,6 %% / < 1,5 %%)\n", 100 * ta, 100 * tb);
        check (100 * ta < 0.75 && 100 * tb < 1.9, "Revox 7,5 ips Klirr im Rahmen der Herstellerangabe (+-25 %)");
    }

    std::printf ("\n=== 9. Oversampler (Halbband-IIR) und Aliasing der Verzerrer ===\n");
    {
        auto osTest = [&] (auto& os, int OS, const char* name)
        {
            for (double f : { 1000.0, 10000.0, 18000.0 })
            {
                const int N = 24000; std::vector<double> big ((size_t) (N * OS)), back ((size_t) N);
                { auto up = os; (void) up; }
                std::decay_t<decltype (os)> a, b;
                for (int i = 0; i < N; ++i) { double o[4]; a.up (std::sin (2 * PI * f * i / fs), o); for (int k = 0; k < OS; ++k) big[(size_t) (i * OS + k)] = o[k]; }
                for (int i = 0; i < N; ++i) { double in[4]; for (int k = 0; k < OS; ++k) in[k] = big[(size_t) (i * OS + k)]; back[(size_t) i] = b.down (in); }
                auto amp = [&] (const std::vector<double>& v, int s, int n, double fr, double sr) { std::complex<double> acc = 0; double w = 0; for (int i = 0; i < n; ++i) { const double ww = 0.5 - 0.5 * std::cos (2 * PI * i / (n - 1)); acc += ww * v[(size_t) (s + i)] * std::polar (1.0, -2 * PI * fr * i / sr); w += ww; } return 2 * std::abs (acc) / w; };
                const double a1 = amp (big, 8000, 16000, f, fs * OS), im = amp (big, 8000, 16000, fs - f, fs * OS), rt = amp (back, 6000, 12000, f, fs);
                char b2[128]; std::snprintf (b2, sizeof b2, "%s %5.0f Hz: Spiegel %.0f dB, Hin+Rueck %+.2f dB", name, f, dB (im / a1), dB (rt)); std::printf ("  %s\n", b2);
                check (dB (im / a1) < -60.0 && std::fabs (dB (rt)) < 0.2, b2);
            }
        };
        Oversampler<2> o2; Oversampler<4> o4; osTest (o2, 2, "2x"); osTest (o4, 4, "4x");
        const float cfgA[4] = { 1.f, 0.5f, 0.5f, 0.5f };
        struct A { const char* n; Kind k; int sw; double f0, amp; double limit; } al[] = {
            { "Tube Screamer, Drive 1,0, 5,3 kHz, -20 dBFS", Kind::TS808, 0, 5300.0, 0.1, -45.0 },
            { "Fuzz Face (Ge), Fuzz 0,3, 5,3 kHz, -20 dBFS", Kind::FuzzFace, 0, 5300.0, 0.1, -45.0 } };
        for (auto& e : al)
        {
            auto ped = make (e.k); ped->prepare ((float) fs); P pr; for (int j = 0; j < 4; ++j) pr.k[j] = cfgA[j]; pr.s[0] = e.sw; if (e.k == Kind::FuzzFace) pr.k[0] = 0.3f;
            const int N = 1 << 16, warm = 24000; std::vector<float> x ((size_t) (N + warm)); for (int i = 0; i < N + warm; ++i) x[(size_t) i] = (float) (e.amp * std::sin (2 * PI * e.f0 * i / fs));
            for (size_t i = 0; i < x.size(); i += 256) ped->process (&x[i], (int) std::min<size_t> (256, x.size() - i), pr);
            std::vector<std::complex<double>> a ((size_t) N); for (int i = 0; i < N; ++i) a[(size_t) i] = (0.5 - 0.5 * std::cos (2 * PI * i / (N - 1))) * x[(size_t) (warm + i)];
            // einfache DFT-Auswertung nur an den Rest-Frequenzen waere zu teuer: FFT
            for (size_t i = 1, j = 0; i < a.size(); ++i) { size_t bit = a.size() >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
            for (size_t len = 2; len <= a.size(); len <<= 1) { const auto w = std::polar (1.0, -2 * PI / (double) len); for (size_t i = 0; i < a.size(); i += len) { std::complex<double> wn = 1; for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * wn; a[i + j] = u + v; a[i + j + len / 2] = u - v; wn *= w; } } }
            const double binHz = fs / N; double pf = 0, pr2 = 0;
            for (int k = 1; k < N / 2; ++k) { const double f = k * binHz; if (f < 100.0 || f > 20000.0) continue; const double pw = std::norm (a[(size_t) k]); const double hn = std::round (f / e.f0); if (std::fabs (f - e.f0) < 6 * binHz) pf += pw; else if (! (hn >= 1.0 && std::fabs (f - hn * e.f0) < 6 * binHz)) pr2 += pw; }
            char b2[160]; std::snprintf (b2, sizeof b2, "%s: Rest %.1f dB (Grenze %.0f dB)", e.n, 10 * std::log10 (pr2 / pf), e.limit); std::printf ("  %s\n", b2); check (10 * std::log10 (pr2 / pf) < e.limit, b2);
        }
    }

    std::printf ("\n%s (%d Fehler)\n", fails ? "FEHLER" : "ALLE PRUEFUNGEN BESTANDEN", fails);
    return fails ? 1 : 0;
}
