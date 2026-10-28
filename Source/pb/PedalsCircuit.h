#pragma once
// Schaltungsnahe Modelle nach recherchierten Bauteilwerten. "EST" = Schaetzung (Wert nicht belegt).
#include "Pedals.h"

namespace pb
{
// Gauss-Elimination fuer kleine lineare Systeme (n<=6), M ist n x (n+1)
inline void solveLin (double M[6][7], int n, double* x)
{
    for (int c = 0; c < n; ++c)
    {
        int pv = c; for (int i = c + 1; i < n; ++i) if (std::fabs (M[i][c]) > std::fabs (M[pv][c])) pv = i;
        for (int k = 0; k <= n; ++k) std::swap (M[c][k], M[pv][k]);
        for (int i = c + 1; i < n; ++i) { const double m = M[i][c] / M[c][c]; for (int k = c; k <= n; ++k) M[i][k] -= m * M[c][k]; }
    }
    for (int i = n - 1; i >= 0; --i) { double a = M[i][n]; for (int k = i + 1; k < n; ++k) a -= M[i][k] * x[k]; x[i] = a / M[i][i]; }
}

// Allpass erster Ordnung mit Eckfrequenz fc (TPT-Form wie im Small Stone)
struct AllpassFc
{
    float z = 0.f;
    float run (float x, float fc, float fs)
    {
        const float t = std::tan (kPi * std::min (fc, 0.45f * fs) / fs);
        const float a = (t - 1.f) / (t + 1.f);
        const float o = a * x + z; z = x - a * o; return o;
    }
};

// Erster Ordnung (1 + s/wz) / (1 + s/wp), bilinear. Fuer Pre-/De-Emphasis des CE-2.
struct Shelf1
{
    float b0 = 1.f, b1 = 0.f, a1 = 0.f, xz = 0.f, yz = 0.f;
    void set (float fz, float fp, float fs)
    {
        const float K = 2.f * fs, wz = 2.f * kPi * fz, wp = 2.f * kPi * fp;
        const float n0 = 1.f + K / wz, n1 = 1.f - K / wz, d0 = 1.f + K / wp, d1 = 1.f - K / wp;
        b0 = n0 / d0; b1 = n1 / d0; a1 = d1 / d0;
    }
    float run (float x) { const float y = b0 * x + b1 * xz - a1 * yz; xz = x; yz = y; return y; }
    void reset() { xz = yz = 0.f; }
};

// ---------------------------------------------------------------- MXR Phase 90 (Script-Logo, ElectroSmash-Bauteilliste)
// 4 identische Allpass-Stufen: C = 47 nF, R = 24k parallel zu JFET 2N5952 (rDS). Notches bei JFET aus: 58 Hz und 340 Hz.
// Eingangs-HP 33 Hz (10 nF / 470k), Ausgangs-HP 22 Hz, Mix 50:50. LFO: Dreieck am Gate.
// k0 Speed, s0 R28-Feedback (Block-Logo/EVH, auf Stufe 2 zurueckgefuehrt)
struct Phase90 : Pedal
{
    float fs = 48000.f, ph = 0.f, fb = 0.f; AllpassFc st[4]; Smooth ss; OnePole hpIn, hpOut;
    void prepare (float f) override
    {
        fs = f; ss.prepare (fs, 60.f); ph = 0.f; fb = 0.f; hpIn.set (33.f, fs); hpOut.set (22.f, fs);
        hpIn.reset(); hpOut.reset(); for (auto& s : st) s.z = 0.f;
    }
    void process (float* x, int n, const P& p) override
    {
        const float C = 47e-9f, R0 = 24000.f;
        const float vp[4] = { 1.00f, 1.06f, 0.95f, 1.03f };     // Abschnuerspannung je JFET unterschiedlich (EST, 2N5952: -1,3 ... -3,5 V)
        const float fbAmt = p.s[0] ? 0.3f : 0.f;                // EST
        for (int i = 0; i < n; ++i)
        {
            ph += expMap (ss.next (p.k[0]), 0.2f, 7.f) / fs; if (ph >= 1.f) ph -= 1.f;   // "Zehntel Hz bis einige Hz" (EST)
            const float tri = 1.f - std::fabs (2.f * ph - 1.f);
            const float in = hpIn.hp (x[i]);
            float y = in;
            for (int s = 0; s < 4; ++s)
            {
                const float u = std::min (1.f, 1.02f * (1.f - tri) / vp[s]);         // normierte Gate-Spannung, 1 = abgeschnuert
                const float rds = 800.f / std::max (0.001f, 1.f - u);                 // rDS = r0 / (1 - Vgs/Vp), r0 EST
                const float R = (R0 * rds) / (R0 + rds);
                y = st[s].run (y, 1.f / (2.f * kPi * R * C), fs);
                if (s == 0) y += fbAmt * fb;
            }
            fb = std::tanh (y);
            x[i] = hpOut.hp (0.5f * (in + y)) * 1.4f;
        }
    }
};

// ---------------------------------------------------------------- Ibanez TS808 Tube Screamer
// Clipping-Stufe: Opamp, Gegenkopplung = Drive-Poti + 51k parallel 51 pF und 2 Dioden, Masse-Zweig 4k7 + 47 nF (Newton-Loesung).
// Tone/Volume: R7 1k + C5 0,22 uF (723 Hz), zweite Opamp-Stufe mit 20k-Tone-Poti zwischen (-) und (+), Wiper ueber R8 220 + C6 0,22 uF
// nach Masse. Das Netzwerk wird pro Sample linear geloest (trapezfoermig). R9 10k, R14 10k, R11 1k sind aus der Standard-Topologie
// uebernommen (EST), der Text der Quelle nennt sie nicht einzeln.
// k0 Drive, k1 Tone, k2 Level
struct TS808 : Pedal
{
    float fs = 48000.f, vc = 0.f, ig = 0.f, vdp = 0.f, icfp = 0.f;
    double Ap = 0., icp5 = 0., Xp = 0., icp6 = 0.;
    OnePole hpIn, hpC; Smooth sd, st, sl;
    void prepare (float f) override
    {
        fs = f; hpIn.set (15.6f, fs); hpC.set (16.f, fs);
        sd.prepare (fs); st.prepare (fs); sl.prepare (fs);
        vc = ig = vdp = icfp = 0.f; Ap = icp5 = Xp = icp6 = 0.; hpIn.reset(); hpC.reset();
    }
    void process (float* x, int n, const P& p) override
    {
        const float h = 1.f / fs, Rg = 4700.f, Cg = 47e-9f, Cf = 51e-12f;
        const float Is = 2.52e-9f, nVt = 1.752f * 0.02585f;    // Diodenwerte 1N4148-Klasse (EST fuer MA150)
        const float gC = 2.f * Cg / h, gF = 2.f * Cf / h;
        const double g5 = 2. * 0.22e-6 * fs, g6 = 2. * 0.22e-6 * fs, R7 = 1000., R9 = 10000., R14 = 10000., R8 = 220., R11 = 1000.;
        for (int i = 0; i < n; ++i)
        {
            const float d = sd.next (p.k[0]), t = st.next (p.k[1]), lv = sl.next (p.k[2]);
            const float Rf = 51000.f + 500000.f * d * d;                      // Drive-Poti 500k log (Taper EST)
            const float u = hpIn.hp (x[i]) * 0.5f;                            // Einheit -> Volt (EST)
            const float vcn = (u / Rg + gC * vc + ig) / (gC + 1.f / Rg);
            const float in = (u - vcn) / Rg; vc = vcn; ig = in;
            float vd = vdp;
            for (int it = 0; it < 12; ++it)
            {
                const float a = std::clamp (vd / nVt, -40.f, 40.f);
                const float f = 2.f * Is * std::sinh (a) + vd / Rf + gF * (vd - vdp) - icfp - in;
                const float df = 2.f * Is / nVt * std::cosh (a) + 1.f / Rf + gF;
                vd -= std::clamp (f / df, -0.2f, 0.2f);
            }
            icfp = gF * (vd - vdp) - icfp; vdp = vd;
            const double vcl = hpC.hp (u + vd);
            // Tone-Stufe: Unbekannte A (C5-Knoten), v (+/- Eingang), W (Wiper), X (R8/C6), Vo
            const double tp = (double) t * t * (3. - 2. * t);                // G-Taper vereinfacht
            const double Rm = std::max (1., 20000. * (1. - tp)), Rp = std::max (1., 20000. * tp);
            double M[6][7] = {};
            M[0][0] = 1. / R7 + g5 + 1. / R9; M[0][1] = -1. / R9; M[0][5] = vcl / R7 + g5 * Ap + icp5;
            M[1][0] = 1. / R9; M[1][1] = -1. / R9 - 1. / R14 - 1. / Rp; M[1][2] = 1. / Rp;
            M[2][1] = 1. / Rm + 1. / Rp; M[2][2] = -1. / Rm - 1. / Rp - 1. / R8; M[2][3] = 1. / R8;
            M[3][2] = 1. / R8; M[3][3] = -1. / R8 - g6; M[3][5] = -g6 * Xp - icp6;
            M[4][1] = -1. / R11 - 1. / Rm; M[4][2] = 1. / Rm; M[4][4] = 1. / R11;
            double sol[5]; solveLin (M, 5, sol);
            icp5 = g5 * (sol[0] - Ap) - icp5; Ap = sol[0]; icp6 = g6 * (sol[3] - Xp) - icp6; Xp = sol[3];
            x[i] = (float) (sol[4] * 4.) * lv * lv * 0.9f;                   // Pegel EST (Volume-Poti audio)
        }
    }
};

// ---------------------------------------------------------------- Boss CE-2 Chorus (BBD MN3007, 1024 Stufen)
// Pre-Emphasis (Zero 410 Hz, Pol 2,3 kHz = bis +15 dB), Anti-Aliasing 3. Ordnung (~6,6/6,9 kHz), Verzoegerung, Rekonstruktion, De-Emphasis.
// k0 Rate, k1 Depth
struct CE2 : Pedal
{
    float fs = 48000.f, ph = 0.f; Delay d; Shelf1 pre, de; Svf aaI, aaO; OnePole aaI2, aaO2; Smooth sr, sd;
    void prepare (float f) override
    {
        fs = f; d.init ((int) (0.03f * fs)); ph = 0.f; sr.prepare (fs, 60.f); sd.prepare (fs);
        pre.set (410.f, 2300.f, fs); de.set (2300.f, 410.f, fs); pre.reset(); de.reset();
        aaI.set (6600.f, 0.9f, fs); aaO.set (6600.f, 0.9f, fs); aaI.reset(); aaO.reset();
        aaI2.set (6900.f, fs); aaO2.set (6900.f, fs); aaI2.reset(); aaO2.reset();
    }
    void process (float* x, int n, const P& p) override
    {
        for (int i = 0; i < n; ++i)
        {
            ph += expMap (sr.next (p.k[0]), 0.1f, 3.5f) / fs; if (ph >= 1.f) ph -= 1.f;   // max. 3,5 Hz laut Analyse
            const float tri = 1.f - std::fabs (2.f * ph - 1.f);
            const float T = 0.0075f + (0.0006f + 0.0030f * sd.next (p.k[1])) * (2.f * tri - 1.f);   // Mitte/Hub EST
            d.write (aaI2.lp (aaI.lp (pre.run (x[i]))));
            const float wet = de.run (aaO2.lp (aaO.lp (d.read (T * fs))));
            x[i] = 0.7f * (x[i] + wet);
        }
    }
};

// ---------------------------------------------------------------- Shin-ei / Univox Uni-Vibe (nach Darabundit/Wedelich/Bischoff, DAFx-2019)
// Pro Stufe H = alpha*He - beta*Hc mit Block-Kondensator C_DC (Hochshelf), Messwerte der Arbeit: Cp = 15n/220n/470n(*)/4,7n,
// alpha 1,01/0,98/0,97/0,95, beta 1,11/1,09/1,10/1,09, LDR min 12,7k/6,86k/7,69k/6,22k, max 2,79M/2,59M/3,32M/4,16M.
// (*) Die Tabelle der Arbeit nennt "470p", ich nehme 470 nF wie bei allen bekannten Schaltplaenen. C_DC (1 uF), R6 (4k7),
// Lampenkurve, LDR-Zeitkonstanten (8 ms/78 ms aus einem Forumsbeitrag) und die Tanh-Werte sind geschaetzt: die gemessenen
// 640 Widerstandskurven der Arbeit liegen nicht als Zahlen vor, daher parametrisch statt Wavetable.
// k0 Speed (bis 7,6 Hz), k1 Intensity, k2 Volume, s0 Chorus(0)/Vibrato(1)
struct UniVibe : Pedal
{
    float fs = 48000.f, ph = 0.f, lamp = 0.2f, lr[4]; OnePole hpIn, dcb[4]; Smooth sr, si, sv;
    float x1[4] = {}, y1[4] = {};
    void prepare (float f) override
    {
        fs = f; ph = 0.f; lamp = 0.2f; hpIn.set (20.f, fs); sr.prepare (fs, 60.f); si.prepare (fs); sv.prepare (fs);
        for (int s = 0; s < 4; ++s) { lr[s] = std::log (300000.f); x1[s] = y1[s] = 0.f; dcb[s].set (8.f, fs); dcb[s].reset(); }
    }
    void process (float* x, int n, const P& p) override
    {
        const float Cp[4] = { 15e-9f, 220e-9f, 470e-9f, 4.7e-9f }, al[4] = { 1.01f, 0.98f, 0.97f, 0.95f }, be[4] = { 1.11f, 1.09f, 1.10f, 1.09f };
        const float rmin[4] = { 12700.f, 6860.f, 7690.f, 6220.f }, rmax[4] = { 2.79e6f, 2.59e6f, 3.32e6f, 4.16e6f };
        const float Cdc = 1e-6f, R6 = 4700.f, g = 1.5f, u0 = 0.1f;
        const float aLamp = coef (25.f, fs), aFall = coef (8.f, fs), aRise = coef (78.f, fs);
        const bool vib = p.s[0] != 0;
        for (int i = 0; i < n; ++i)
        {
            ph += expMap (sr.next (p.k[0]), 0.5f, 7.6f) / fs; if (ph >= 1.f) ph -= 1.f;
            const float inten = si.next (p.k[1]);
            const float target = 0.2f + 0.75f * inten * (0.5f + 0.5f * std::sin (2.f * kPi * ph));
            lamp += aLamp * (target - lamp);
            const float dry = hpIn.hp (x[i]);
            float y = dry;
            for (int s = 0; s < 4; ++s)
            {
                const float b = std::pow (std::clamp (lamp, 0.f, 1.f), 1.3f);
                const float lt = std::log (rmax[s] * std::pow (rmin[s] / rmax[s], b));
                lr[s] += (lt < lr[s] ? aFall : aRise) * (lt - lr[s]);
                const float R0 = std::exp (lr[s]) + R6;
                const float wo = (Cdc + Cp[s]) / (R0 * Cp[s] * Cdc);       // Mittenfrequenz in rad/s
                const float t = std::tan (std::min (wo / (2.f * fs), 1.5f));
                const float kc = Cp[s] / (Cp[s] + Cdc), ke = Cdc / (Cp[s] + Cdc);
                const float xin = (std::tanh (g * (y + u0)) - std::tanh (g * u0)) / g;   // Emitterfolger asymmetrisch
                const float xd = xin - dcb[s].lp (xin);                                    // DC-Sperre wie in der Arbeit
                const float b0 = (al[s] * ke * t - be[s] * (kc * t + 1.f)) / (t + 1.f);
                const float b1 = (al[s] * ke * t - be[s] * (kc * t - 1.f)) / (t + 1.f);
                const float a1 = (t - 1.f) / (t + 1.f);
                const float o = b0 * xd + b1 * x1[s] - a1 * y1[s]; x1[s] = xd; y1[s] = o; y = o;
            }
            x[i] = (vib ? y : 0.5f * (dry + y)) * sv.next (p.k[2]) * 2.f;
        }
    }
};

// ======================================================================= Fuzz Face (ElectroSmash, AC128-Variante)
// Echte Schaltung: 2 Transistoren (Ebers-Moll), C1 2,2 uF, C2 20 uF, R1 33k, R2 470, R3 8k2, R4 100k, Fuzz-Poti 1k, Volume 500k.
// Alle fuenf Knotenspannungen werden pro Sample mit Newton geloest (Kondensatoren trapezfoermig), 2x Oversampling.
// k0 Fuzz, k1 Volume, s0 Germanium(0)/Silizium(1)
struct FuzzFace2 : Pedal
{
    struct Tr { double Is, bf, br; };
    float fs = 48000.f; double x[5] = { 0.2, 0.66, 0.46, 4.5, 0.2 }, vc1p = 0, icp1 = 0, wp = 0, icp2 = 0, xdc = 0, prevIn = 0;
    OnePole hpOut; Smooth sf, sv; Tr q1 { 85.8e-9, 85., 20. }, q2 { 120.8e-9, 120., 20. }; int lastSi = -1;
    double Rs = 12000.;   // Quellimpedanz Gitarre (Tonabnehmer + Poti), ohne sie wirkt die R4-Rueckkopplung nicht auf die Verstaerkung (EST)
    static constexpr double Vt = 0.02585, Vcc = 9., R1 = 33e3, R4 = 100e3, R3 = 8200., R2 = 470., kIn = 0.3;
    static double expl (double a) { return a < 30. ? std::exp (a) : std::exp (30.) * (1. + a - 30.); }
    static void em (const Tr& t, double vbe, double vbc, double& ic, double& ib)
    {
        const double ef = expl (vbe / Vt), er = expl (vbc / Vt);
        ic = t.Is * ((ef - er) - (er - 1.) / t.br); ib = t.Is * ((ef - 1.) / t.bf + (er - 1.) / t.br);
    }
    void resid (const double* v, double vin, double RwA, double RwB, double g1, double g2, double* r, double* ic2o) const
    {
        const double b1 = v[0], c1 = v[1], e2 = v[2], c2 = v[3], w = v[4];
        double ic1, ib1, ic2, ib2; em (q1, b1, b1 - c1, ic1, ib1); em (q2, c1 - e2, c1 - c2, ic2, ib2);
        const double iC1 = (g1 * (vin - b1 - vc1p) - icp1) / (1. + g1 * Rs), iC2 = g2 * (w - wp) - icp2;
        r[0] = iC1 + (e2 - b1) / R4 - ib1;
        r[1] = (Vcc - c1) / R1 - ic1 - ib2;
        r[2] = ic2 + ib2 - (e2 - b1) / R4 - (e2 - w) / RwA;
        r[3] = ic2 - (Vcc - c2) / (R2 + R3);
        r[4] = (e2 - w) / RwA - w / RwB - iC2;
        if (ic2o) *ic2o = ic2;
    }
    void solve (double vin, double RwA, double RwB, double g1, double g2)
    {
        for (int it = 0; it < 40; ++it)
        {
            double r[5], J[5][6];
            resid (x, vin, RwA, RwB, g1, g2, r, nullptr);
            for (int j = 0; j < 5; ++j)
            {
                double xp[5]; for (int k = 0; k < 5; ++k) xp[k] = x[k];
                const double h = 1e-6; xp[j] += h; double rp[5]; resid (xp, vin, RwA, RwB, g1, g2, rp, nullptr);
                for (int i = 0; i < 5; ++i) J[i][j] = (rp[i] - r[i]) / h;
            }
            for (int i = 0; i < 5; ++i) J[i][5] = r[i];
            for (int c = 0; c < 5; ++c)
            {
                int pv = c; for (int i = c + 1; i < 5; ++i) if (std::fabs (J[i][c]) > std::fabs (J[pv][c])) pv = i;
                for (int k = 0; k < 6; ++k) std::swap (J[c][k], J[pv][k]);
                for (int i = c + 1; i < 5; ++i) { const double m = J[i][c] / J[c][c]; for (int k = c; k < 6; ++k) J[i][k] -= m * J[c][k]; }
            }
            double dx[5];
            for (int i = 4; i >= 0; --i) { double s2 = J[i][5]; for (int k = i + 1; k < 5; ++k) s2 -= J[i][k] * dx[k]; dx[i] = s2 / J[i][i]; }
            double mx = 0; for (double d : dx) mx = std::max (mx, std::fabs (d));
            const double sc = mx > 0.2 ? 0.2 / mx : 1.;
            for (int i = 0; i < 5; ++i) x[i] -= sc * dx[i];
            if (mx < 1e-9) break;
        }
    }
    void setType (int si)
    {
        lastSi = si;
        if (si) { q1 = { 1e-14, 250., 5. }; q2 = { 1e-14, 300., 5. }; } else { q1 = { 85.8e-9, 85., 20. }; q2 = { 120.8e-9, 120., 20. }; }
    }
    void dcSolve (double f)
    {
        x[0] = lastSi ? 0.62 : 0.2; x[1] = lastSi ? 1.3 : 0.66; x[2] = lastSi ? 0.7 : 0.46; x[3] = 4.5; x[4] = x[2] * f;
        vc1p = icp1 = icp2 = 0; solve (0., std::max (5., 1000. * (1. - f)), std::max (5., 1000. * f), 0., 0.);
        vc1p = -x[0]; wp = x[4]; icp1 = icp2 = 0; double ic2; double r[5]; resid (x, 0., 1., 1., 0., 0., r, &ic2);
        xdc = Vcc - ic2 * R2; hpOut.z = (float) xdc;
    }
    void prepare (float f) override { fs = f; sf.prepare (fs); sv.prepare (fs); lastSi = -1; prevIn = 0; hpOut.reset(); }
    void process (float* in, int n, const P& p) override
    {
        if (p.s[0] != lastSi) { setType (p.s[0]); dcSolve (sf.init ? sf.v : p.k[0]); }
        const double fsS = 2. * fs, g1 = 2. * 2.2e-6 * fsS, g2 = 2. * 20e-6 * fsS;
        for (int i = 0; i < n; ++i)
        {
            const double f = sf.next (p.k[0]), vol = sv.next (p.k[1]);
            const double RwA = std::max (5., 1000. * (1. - f)), RwB = std::max (5., 1000. * f);
            const double u = in[i] * kIn; double acc = 0;
            for (int sub = 0; sub < 2; ++sub)
            {
                const double vin = sub == 0 ? 0.5 * (prevIn + u) : u;
                const double iC1 = 0; (void) iC1;
                const double vc1o = vc1p, ic1o = icp1, wpo = wp, ic2o2 = icp2;
                solve (vin, RwA, RwB, g1, g2);
                const double nIC1 = (g1 * (vin - x[0] - vc1o) - ic1o) / (1. + g1 * Rs), nIC2 = g2 * (x[4] - wpo) - ic2o2;
                vc1p = vin - nIC1 * Rs - x[0]; icp1 = nIC1; wp = x[4]; icp2 = nIC2;
                double ic2, r[5]; resid (x, vin, RwA, RwB, g1, g2, r, &ic2);
                acc += Vcc - ic2 * R2;
            }
            prevIn = u;
            hpOut.set ((float) std::min (600., 31. / std::max (0.05, vol)), fs);
            const float xv = (float) (0.5 * acc);
            in[i] = hpOut.hp (xv) / (float) kIn * (float) (vol * vol) * 0.45f;   // Pegel EST
        }
    }
};

// ======================================================================= MXR Dyna Comp (ElectroSmash + DAFx-11 Kroening/Dempwolf/Zoelzer)
// OTA: I_OTA = I_ABC * tanh(Vd / 2Vt). Vd = V- minus V+ (V+ ist um ~1 dB (300 Hz) bis 4 dB (10 kHz) abgesenkt).
// Knoten R8 150k || C4 1 nF, Detektor: Q4/Q5 ziehen C10 (10 uF) ueber R14 (150k) herunter, C10 steuert I_ABC (R_sens + 27k).
// k0 Output, k1 Sensitivity
struct DynaComp2 : Pedal
{
    float fs = 48000.f; double vc10 = 9., iabc = 1e-5, vo = 0., cap[2] = { 0., 0. };
    OnePole hpIn, lpPlus, hpOut; Smooth so, ss;
    static constexpr double Vt = 0.02585, kIn = 0.3, I0 = 5.5e-13;
    double solveIabc (double vc, double R) const
    {
        double I = std::max (iabc, 1e-12);
        for (int it = 0; it < 14; ++it)
        {
            const double g = 3. * Vt * std::log (I / I0) + I * R - vc, gp = 3. * Vt / I + R;
            double n = I - g / gp; if (n <= 0.) n = 0.1 * I; I = n;
        }
        return I;
    }
    void prepare (float f) override
    {
        fs = f; hpIn.set (31.8f, fs); lpPlus.set (10000.f, fs); hpOut.set (100.f, fs); so.prepare (fs); ss.prepare (fs);
        vc10 = 9.; iabc = 1e-5; vo = 0.; cap[0] = cap[1] = 0.; hpIn.reset(); lpPlus.reset(); hpOut.reset();
    }
    void process (float* x, int n, const P& p) override
    {
        const double dt = 1. / fs, aNode = 1. - std::exp (-dt / (150e3 * 1e-9)), leak = std::exp (-dt / (1e6 * 10e-9));
        for (int i = 0; i < n; ++i)
        {
            const double outL = so.next (p.k[0]), sens = ss.next (p.k[1]);
            const double vm = hpIn.hp (x[i] * (float) kIn), vp = 0.89 * lpPlus.lp ((float) vm);
            const double iota = iabc * std::tanh ((vm - vp) / (2. * Vt));
            vo += aNode * (iota * 150e3 - vo);
            const double vq = vo > 0. ? 6. * std::tanh (vo / 6.) : 2.9 * std::tanh (vo / 2.9);   // Q2-Aussteuergrenzen (EST)
            double ic = 0.;
            for (int d = 0; d < 2; ++d)
            {
                const double sig = d == 0 ? vq : -vq; double vb = sig + cap[d];
                if (vb < -0.6) { cap[d] = -0.6 - sig; vb = -0.6; }
                cap[d] *= leak; ic += 150. * std::max (0., vb - 0.6) / 10e3;                         // beta = 150 (EST)
            }
            const double sat = std::min (1., vc10 / 0.3);
            vc10 += ((9. - vc10) / 150e3 - ic * sat) / 10e-6 * dt; vc10 = std::clamp (vc10, 0., 9.);
            iabc = solveIabc (vc10, 470e3 * (1. - sens) + 27e3);
            hpOut.set ((float) (310. - 258. * outL), fs);
            x[i] = hpOut.hp ((float) (vq * 0.83 * outL * outL)) * 1.5f;   // Pegel EST
        }
    }
};

// ======================================================================= Dunlop Cry Baby GCB-95 (Verhaltensmodell, physikalisch begruendetes Q)
// Mittenfrequenz 450 Hz ... 1,6 kHz (Mitte 750 Hz) aus der ElectroSmash-Analyse. Der Q-Wert folgt dem Parallelkreis aus L1 = 500 mH
// (Gleichstromwiderstand 150 Ohm), R7 = 33k als Daempfung: Q = Rp / (w L) mit Rp = R7 || ((w L)^2 / Rdc), dazu ein pauschaler
// Zusatzverlust (EST, 100k). Der echte Rueckkopplungskreis (C2, R2, Q1/Q2) ist ohne Schaltplanbild nicht verdrahtbar.
// Spitze bis 18 dB, Eingangs-HP 16 Hz. k0 Pedal, k1 Volume
struct CryBaby2 : Pedal
{
    float fs = 48000.f, Qcur = 5.f; Svf f; OnePole hpIn; Smooth sp, sv; int ctr = 0;
    void prepare (float fr) override { fs = fr; sp.prepare (fs, 15.f); sv.prepare (fs); f.reset(); hpIn.set (16.f, fs); hpIn.reset(); }
    void process (float* x, int n, const P& p) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float pos = sp.next (p.k[0]), vol = sv.next (p.k[1]);
            if ((ctr++ & 7) == 0)
            {
                const float f0 = expMap (std::pow (pos, 1.35f), 450.f, 1600.f), wl = 2.f * kPi * f0 * 0.5f;
                const float rp = 1.f / (1.f / 33000.f + 150.f / (wl * wl) + 1.f / 100000.f);
                Qcur = std::clamp (rp / wl, 2.f, 12.f);
                f.set (f0, Qcur, fs);
            }
            float l, b, h; f.run (hpIn.hp (x[i]), l, b, h);
            const float peak = 7.9f * (0.8f + 0.2f * pos);                     // +18 dB
            const float y = b / Qcur * peak + 0.1f * l;
            x[i] = 2.f * std::tanh (0.5f * y) * vol * 2.f;
        }
    }
};

// ======================================================================= MXR Micro Amp (ElectroSmash, M-133)
// Nicht-invertierender TL061: G = 1 + R4/(R5+R6) mal R10/(R9+R10), R4 56k, R6 2k7, R5 500k Reverse-Log, C3 4,7 uF, C2 47 pF,
// Ausgangs-HP 1,06 Hz, Begrenzung an +-4,5 V. Gain 1 ... 20,6 (0 ... 26 dB). k0 Gain
struct MicroAmp : Pedal
{
    float fs = 48000.f; OnePole hpG, hpOut, lpF; Smooth sg; int ctr = 0;
    void prepare (float f) override { fs = f; sg.prepare (fs); hpOut.set (1.06f, fs); lpF.set (60400.f, fs); hpG.reset(); hpOut.reset(); lpF.reset(); }
    void process (float* x, int n, const P& p) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float k = sg.next (p.k[0]);
            const float R5 = 500000.f * std::pow (1.f - k, 3.f), Rg = R5 + 2700.f;          // Reverse-Log-Taper (EST)
            if ((ctr++ & 15) == 0) hpG.set (1.f / (2.f * kPi * 4.7e-6f * Rg), fs);
            const float v = x[i] * 0.3f;                                                      // Einheit -> Volt (EST)
            const float hp = hpG.hp (v);
            const float y = v + 56000.f / Rg * lpF.lp (hp);
            const float lim = 4.5f * std::tanh (y / 4.5f);
            x[i] = hpOut.hp (lim * (10000.f / 10470.f)) / 0.3f;
        }
    }
};

// ======================================================================= Boss DM-2 (BBD MN3005/MN3205, NE570-Kompander, MN3101/3102)
// 20 ... 300 ms (Repeat Rate), Echo (Mix), Intensity (Feedback bis Selbstoszillation). Belegt (AionFX-Doku): 4096 Stufen, 300 ms = 6,8 kHz Takt,
// NE570-Kompander, Feedback-Poti 50k sehr empfindlich. Die Netzliste liegt nur als Bild vor, daher:
// Kompander-Zeiten, BBD-Filter und Verzerrung sind geschaetzt. k0 Repeat Rate, k1 Echo, k2 Intensity, s0 MN3205(1)/MN3005(0)
struct DM2 : Pedal
{
    float fs = 48000.f, T = 0.2f, envC = 0.f, envE = 0.f, lfo = 0.f; Delay d; Svf pre1, pre2, post1, post2; OnePole fbHp; Smooth sd, se, si; int ctr = 0;
    void prepare (float f) override
    {
        fs = f; d.init ((int) (0.34f * fs)); envC = envE = 0.f; lfo = 0.f; fbHp.set (60.f, fs); sd.prepare (fs, 120.f); se.prepare (fs); si.prepare (fs);
        for (Svf* s : { &pre1, &pre2, &post1, &post2 }) s->reset();
    }
    void process (float* x, int n, const P& p) override
    {
        const float aAtt = coef (2.f, fs), aRel = coef (30.f, fs), bbdK = p.s[0] ? 0.30f : 0.35f;   // 1,0 % / 0,8 % Verzerrung (EST)
        for (int i = 0; i < n; ++i)
        {
            T = sd.next (lerpf (0.3f, 0.02f, p.k[0]));   // Original: Repeat Rate hoeher = kuerzere Verzoegerung, 300 ms = 6,8 kHz Takt
            if ((ctr++ & 63) == 0)
            {
                const float fclk = 4096.f / (2.f * T), fc = std::min (0.85f * fclk * 0.25f, 9000.f);
                for (Svf* s : { &pre1, &pre2, &post1, &post2 }) s->set (fc, 0.7f, fs);
            }
            lfo += 0.4f / fs; if (lfo >= 1.f) lfo -= 1.f;
            const float rd = d.read (std::max (2.f, (T + 0.0003f * std::sin (2.f * kPi * lfo)) * fs));   // Taktdrift (EST)
            const float wetB = post2.lp (post1.lp (rd));
            envE += (std::fabs (wetB) > envE ? aAtt : aRel) * (std::fabs (wetB) - envE);               // Expander
            const float wet = wetB * envE * 4.f;
            const float fbv = std::tanh (fbHp.hp (wet) * si.next (p.k[2]) * 1.05f);
            const float in = x[i] + fbv;
            envC += (std::fabs (in) > envC ? aAtt : aRel) * (std::fabs (in) - envC);                    // Kompressor 2:1
            const float comp = in / (std::sqrt (envC) + 0.02f);
            d.write (pre2.lp (pre1.lp (std::tanh (bbdK * comp) / bbdK)) * 0.5f);
            x[i] = x[i] + wet * se.next (p.k[1]);
        }
    }
};

// ======================================================================= Boss VB-2 Vibrato (BBD MN3207 + MN3102)
// Reines Wet-Signal mit modulierter Verzoegerung, Pre-/De-Emphasis wie CE-2 (Annahme, gleiche Boss-Familie, EST).
// k0 Rate, k1 Depth, k2 Rise Time (Einblendzeit, 0 ... ca. 2 s). retrigger() startet die Einblendung neu.
struct VB2 : Pedal
{
    float fs = 48000.f, ph = 0.f, rise = 0.f; Delay d; Shelf1 pre, de; Svf aaI, aaO; OnePole aaI2, aaO2; Smooth sr, sd;
    void retrigger() { rise = 0.f; }
    void prepare (float f) override
    {
        fs = f; d.init ((int) (0.03f * fs)); ph = 0.f; rise = 0.f; sr.prepare (fs, 60.f); sd.prepare (fs);
        pre.set (410.f, 2300.f, fs); de.set (2300.f, 410.f, fs); pre.reset(); de.reset();
        aaI.set (6600.f, 0.9f, fs); aaO.set (6600.f, 0.9f, fs); aaI.reset(); aaO.reset();
        aaI2.set (6900.f, fs); aaO2.set (6900.f, fs); aaI2.reset(); aaO2.reset();
    }
    void process (float* x, int n, const P& p) override
    {
        const float riseTime = 0.05f + 2.f * p.k[2], dr = 1.f / (riseTime * fs);
        for (int i = 0; i < n; ++i)
        {
            ph += expMap (sr.next (p.k[0]), 0.5f, 10.f) / fs; if (ph >= 1.f) ph -= 1.f;
            rise = std::min (1.f, rise + dr);
            const float depth = sd.next (p.k[1]) * rise;
            const float T = 0.0060f + 0.0045f * depth * std::sin (2.f * kPi * ph);                   // Mitte/Hub EST
            d.write (aaI2.lp (aaI.lp (pre.run (x[i]))));
            x[i] = de.run (aaO2.lp (aaO.lp (d.read (T * fs)))) * 1.4f;
        }
    }
};

// ======================================================================= Fender 6G15 Reverb Unit (Roehren: 12AT7, 6K6, 12AX7)
// Signalweg laut Beschreibung: 12AT7 Vorstufe -> Hochpass -> 6K6 Treiber -> Federtank -> 12AX7 Rueckholstufe -> Mixer 250k.
// Alle Bauteilwerte (Plan nur als Bild) und die Feder sind geschaetzt. k0 Dwell, k1 Tone, k2 Mixer
struct Fender6G15 : Pedal
{
    float fs = 48000.f; Spring sp1, sp2; OnePole drvHp, tone, recHp; Smooth sd, st, sm;
    void prepare (float f) override
    {
        fs = f; sp1.init (fs, 0.0345f); sp2.init (fs, 0.0442f); drvHp.set (90.f, fs); recHp.set (80.f, fs); tone.set (3000.f, fs);
        sd.prepare (fs); st.prepare (fs); sm.prepare (fs); drvHp.reset(); recHp.reset(); tone.reset();
    }
    void process (float* x, int n, const P& p) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float dwell = sd.next (p.k[0]), t = st.next (p.k[1]), mix = sm.next (p.k[2]);
            tone.set (expMap (t, 1500.f, 6000.f), fs);                                               // EST
            const float pre = std::tanh (1.2f * x[i] + 0.15f) - std::tanh (0.15f);                    // 12AT7, leicht asymmetrisch
            const float drv = std::tanh ((0.4f + 3.f * dwell) * drvHp.hp (pre));                      // 6K6 saettigt mit Dwell
            const float tank = 0.5f * (sp1.run (drv) + sp2.run (drv));
            const float wet = std::tanh (3.f * tone.lp (recHp.hp (tank))) / 3.f * 3.f;                // 12AX7 Rueckholung
            x[i] = x[i] * (1.f - 0.4f * mix) + wet * mix * 1.2f;
        }
    }
};

// ======================================================================= Hilfsbausteine fuer die Digital-Geraete
inline float quant (float x, int bits) { const float q = (float) (1 << (bits - 1)); return std::round (std::clamp (x, -1.f, 1.f) * q) / q; }

// Pitch-Shifter nach dem Prinzip der Harmonizer: Verzoegerungsleitung mit Saegezahn-Zeigern, zwei Abgriffe ueberblendet.
// crossFrac klein = harte Spleissstelle (Eventide-"Glitch"), 0,5 = weich (Hann).
struct PitchShifter
{
    Delay d; float ph = 0.f;
    void init (float fs) { d.init ((int) (0.12f * fs)); ph = 0.f; }
    float process (float x, float ratio, float winSamples, float crossFrac)
    {
        d.write (x);
        ph += (1.f - ratio) / winSamples;
        ph -= std::floor (ph);
        const float pa = ph, pb2 = ph + 0.5f - std::floor (ph + 0.5f);
        auto win = [crossFrac] (float p) { return std::min (1.f, std::sin (kPi * p) / std::sin (kPi * crossFrac)); };
        const float ga = win (pa), gb = win (pb2), sum = std::max (ga + gb, 1e-3f);
        return (d.read (2.f + winSamples * pa) * ga + d.read (2.f + winSamples * pb2) * gb) / sum;
    }
};

// ======================================================================= Eventide H910 Harmonizer
// Belegt (Eventide-Handbuch/Website): Pitch-Aenderung ueber eine Verzoegerung, die sich kontinuierlich ueber ca. 30 ms aendert,
// zwei Oktaven Bereich, bis 112,5 ms Delay, Feedback, Frequenzgang 20 Hz ... 12 kHz, nur die Verzoegerung war digital (Filter,
// Feedback, Mischer analog). Der Glitch an der Spleissstelle gehoerte zum Klang. Wandler-Aufloesung und Spleissform sind geschaetzt.
// Delay 2 laeuft laut Handbuch vor dem Pitch-Shifter und wird hier nicht nachgebaut.
// k0 Pitch (Oktave runter ... Oktave hoch), k1 Feedback, k2 Delay (0 ... 112,5 ms), s0 Glitch (hart) / entglitcht
struct H910 : Pedal
{
    float fs = 48000.f, fb = 0.f; PitchShifter ps; Delay dl; OnePole lpA, lpB, hpA; Smooth sp, sf, sdl;
    void prepare (float f) override
    {
        fs = f; ps.init (fs); dl.init ((int) (0.12f * fs)); fb = 0.f; lpA.set (12000.f, fs); lpB.set (12000.f, fs); hpA.set (20.f, fs);
        lpA.reset(); lpB.reset(); hpA.reset(); sp.prepare (fs, 40.f); sf.prepare (fs); sdl.prepare (fs, 60.f);
    }
    void process (float* x, int n, const P& p) override
    {
        const float win = 0.030f * fs, cross = p.s[0] ? 0.04f : 0.35f;
        for (int i = 0; i < n; ++i)
        {
            const float ratio = std::pow (2.f, (sp.next (p.k[0]) * 2.f - 1.f));        // 0,5 ... 2
            const float in = hpA.hp (x[i] + sf.next (p.k[1]) * 0.7f * fb);
            const float shifted = quant (ps.process (in, ratio, win, cross), 12);
            dl.write (shifted);
            const float w = lpB.lp (lpA.lp (dl.read (std::max (2.f, sdl.next (p.k[2]) * 0.1125f * fs))));
            fb = std::tanh (w);
            x[i] = 0.5f * (x[i] + w);
        }
    }
};

// ======================================================================= Alesis MIDIverb (Keith-Barr-Topologie, eigene Laengen)
// Belegt (Reverse-Engineering von E. Brombaugh und thement, Valhalla-Forum): 8031-gesteuerter DASP-Prozessor, ca. 23,4 kHz Abtastrate,
// 16 Bit, 63 Programme; die Algorithmen sind Ketten aus Allpaessen (Koeffizient 0,5) und Verzoegerungen in einer Schleife mit
// Rueckkopplung (z. B. 5/8 oder 3/4). Die echten Programme sind urheberrechtlich geschuetzte ROM-Daten (Emulator unter GPL-3 mit
// eigener ROM), deshalb bilde ich NUR die Topologie mit eigenen Laengen nach. 4 Programme: Hall, Room, Plate, Gate.
// k0 Programm, k1 Mix, k2 Decay
struct MidiVerb : Pedal
{
    struct Stage { Allpass ap; Delay d; int len = 8; void init (int apLen, int dLen) { ap.init (std::max (4, apLen)); d.init (dLen + 4); len = std::max (4, dLen); }
                   float run (float x) { const float y = ap.run (x); d.write (y); return d.read ((float) len); } };
    float fs = 48000.f, hold = 0.f, gateEnv = 0.f, tail = 0.f; Allpass din[4]; Stage st[3]; OnePole damp, inLp, outLp; Smooth sm, sdc; int prog = -1;
    void setup (int pr)
    {
        prog = pr;
        static const float size[4] = { 1.0f, 0.4f, 0.65f, 0.85f };
        const float dm[4] = { 1.3f, 2.0f, 3.5f, 5.4f }, am[3] = { 22.8f, 31.7f, 41.3f }, lm[3] = { 12.3f, 18.9f, 26.1f };   // ms (eigene Werte)
        for (int i = 0; i < 4; ++i) din[i].init (std::max (4, (int) (dm[i] * 0.001f * fs)));
        for (int i = 0; i < 3; ++i) st[i].init ((int) (am[i] * size[pr] * 0.001f * fs), (int) (lm[i] * size[pr] * 0.001f * fs));
        tail = 0.f;
    }
    void prepare (float f) override { fs = f; inLp.set (10000.f, fs); outLp.set (10000.f, fs); damp.set (5000.f, fs); inLp.reset(); outLp.reset(); damp.reset(); sm.prepare (fs); sdc.prepare (fs); prog = -1; gateEnv = 0.f; hold = 0.f; setup (0); }
    void process (float* x, int n, const P& p) override
    {
        const int pr = std::clamp ((int) (p.k[0] * 4.f), 0, 3);
        if (pr != prog) setup (pr);
        static const float fbBase[4] = { 0.68f, 0.55f, 0.62f, 0.68f };
        for (int i = 0; i < n; ++i)
        {
            const float mix = sm.next (p.k[1]), dc = sdc.next (p.k[2]);
            const float fb = std::clamp (fbBase[pr] + (dc - 0.5f) * 0.16f, 0.3f, 0.88f);
            float s = quant (inLp.lp (x[i]), 16) * 0.5f;
            for (auto& a : din) s = a.run (s);
            float y = s + fb * tail, o = 0.f;
            for (int k = 0; k < 3; ++k) { y = st[k].run (y); o += y * (0.45f - 0.1f * (float) k); }
            tail = damp.lp (y);
            float wet = outLp.lp (o * 4.f);
            if (pr == 3)   // Gate: Hall wird nach ca. 250 ms abgeschnitten
            {
                const float a = std::fabs (x[i]); if (a > 0.05f) hold = 0.25f * fs; else hold = std::max (0.f, hold - 1.f);
                gateEnv += (hold > 0.f ? 0.002f : 0.0004f) * ((hold > 0.f ? 1.f : 0.f) - gateEnv); wet *= gateEnv;
            }
            x[i] = x[i] * (1.f - 0.3f * mix) + wet * mix * 1.5f;
        }
    }
};

// ======================================================================= Roland VP-330 "Human Voice" + Ensemble
// Belegt (VP-330-Service-Notes laut notebook.zoeblade.com, Gearspace, florian-anwander.de): 7 Formantfilter-Bloecke bei
// 200, 230, 600, 900, 1300, 2800, 3300 Hz, jeder Block = zwei 2-Pol-Bandpaesse in Reihe; jeder der vier Chor-Klaenge summiert vier Bloecke
// parallel: Upper Female 4' = F3,F4,F5,F7; Upper Male 8' = F2,F3,F4,F6; Lower Male 4' = F2,F3,F4,F6 (laut Tabelle); Lower Male 8' = F1,F3,F4,F6.
// Ensemble: 4 BBDs (MN3004) mit je eigenem LFO. LFO-Frequenzen/Bandgueten/Pegelgewichte sind nicht belegt (EST).
// Der Vocoder (10 Baender 200 ... 6000 Hz) braucht einen zweiten Eingang (Sidechain) und fehlt.
// k0 Voice (4 Stufen), k1 Ensemble, k2 Mix
struct VP330 : Pedal
{
    float fs = 48000.f, ph[4] = {}; Svf bpA[7], bpB[7]; Delay d; Smooth se, sm;
    void prepare (float f) override
    {
        fs = f; d.init ((int) (0.03f * fs)); se.prepare (fs); sm.prepare (fs);
        const float fc[7] = { 200.f, 230.f, 600.f, 900.f, 1300.f, 2800.f, 3300.f };
        for (int i = 0; i < 7; ++i) { bpA[i].set (fc[i], 5.f, fs); bpB[i].set (fc[i], 5.f, fs); bpA[i].reset(); bpB[i].reset(); }
        for (int t = 0; t < 4; ++t) ph[t] = 0.25f * (float) t;
    }
    void process (float* x, int n, const P& p) override
    {
        static const int use[4][4] = { { 2, 3, 4, 6 }, { 1, 2, 3, 5 }, { 1, 2, 3, 5 }, { 0, 2, 3, 5 } };   // Filterindex 0-6 = F1-F7
        const int v = std::clamp ((int) (p.k[0] * 4.f), 0, 3);
        const float rate[4] = { 0.55f, 0.62f, 5.4f, 6.1f };                                                // EST
        for (int i = 0; i < n; ++i)
        {
            const float e = se.next (p.k[1]), mix = sm.next (p.k[2]);
            const float rich = std::tanh (4.f * x[i]);                                                      // Saegezahn-aehnliche Obertoene
            float sum = 0.f;
            for (int k = 0; k < 4; ++k)
            {
                const int b = use[v][k]; float l, bb, h, l2, b2, h2;
                bpA[b].run (rich, l, bb, h); bpB[b].run (bb / 5.f, l2, b2, h2); sum += b2 / 5.f;
            }
            d.write (sum * 0.9f);
            float ens = 0.f;
            for (int t = 0; t < 4; ++t)
            {
                ph[t] += rate[t] / fs; ph[t] -= std::floor (ph[t]);
                const float dl = 0.0065f + e * (t < 2 ? 0.0028f : 0.0007f) * std::sin (2.f * kPi * ph[t]);
                ens += d.read (dl * fs) * 0.25f;
            }
            x[i] = x[i] * (1.f - 0.5f * mix) + ens * mix * 1.4f;
        }
    }
};

// ======================================================================= Boss VE-20 Vocal Performer (Verhaltensmodell)
// Mikrofon-Aufbereitung (Hochpass, Kompressor), Harmonie ueber Pitch-Shifter (Intervall waehlbar) und Hall.
// Die echten Algorithmen sind unbekannt. k0 Harmony Level, k1 Interval (-12, -5, +3, +4, +7, +12 Halbtoene), k2 Reverb
struct VE20 : Pedal
{
    float fs = 48000.f, env = 0.f; OnePole hp; PitchShifter ps; Comb comb[4]; Allpass ap[2]; Smooth sh, si, sr;
    void prepare (float f) override
    {
        fs = f; hp.set (90.f, fs); hp.reset(); ps.init (fs); env = 0.f; sh.prepare (fs); si.prepare (fs, 80.f); sr.prepare (fs);
        const int cl[4] = { 1116, 1277, 1422, 1557 }, al[2] = { 556, 341 }; const float sc = fs / 44100.f * 0.6f;
        for (int i = 0; i < 4; ++i) comb[i].init ((int) (cl[i] * sc)); for (int i = 0; i < 2; ++i) ap[i].init ((int) (al[i] * sc));
    }
    void process (float* x, int n, const P& p) override
    {
        static const float st[6] = { -12.f, -5.f, 3.f, 4.f, 7.f, 12.f };
        for (int i = 0; i < n; ++i)
        {
            const float lvl = sh.next (p.k[0]), rv = sr.next (p.k[2]);
            const int idx = std::clamp ((int) (si.next (p.k[1]) * 5.999f), 0, 5);
            float y = hp.hp (x[i]);
            env += (std::fabs (y) > env ? 0.01f : 0.0005f) * (std::fabs (y) - env);
            y *= 1.f / (1.f + 3.f * std::max (0.f, env - 0.1f)) * 1.2f;                    // Kompressor, Schwelle/Verhaeltnis EST
            const float harm = ps.process (y, std::pow (2.f, st[idx] / 12.f), 0.040f * fs, 0.25f);
            const float dry = y + harm * lvl;
            float s = 0.f; for (auto& c : comb) s += c.run (dry * 0.02f, 0.82f, 0.3f);
            for (auto& a : ap) s = a.run (s);
            x[i] = dry + s * rv * 3.f;
        }
    }
};
} // namespace pb
