#pragma once
// Tonband-Stufe (TEAC A-3340S / Revox B77). Belegte Werte stammen aus dem TEAC-Service-Manual (Spezifikationen, Messkurven,
// Pegel-Definition) und den Revox-B77-Schaltplaenen (Bias 150 kHz, Aufnahme-EQ-Bauteile). Alles andere ist mit EST markiert.
#include "PedalsCircuit.h"
#include <cstdint>

namespace pb
{
// Halbband-Oversampler 2x (63 Taps, Blackman), Latenz 31 Samples bei der Original-Abtastrate
struct Half2x
{
    static constexpr int N = 63;
    float h[N], zu[N], zd[N];
    void init()
    {
        double sum = 0;
        for (int n = 0; n < N; ++n)
        {
            const double t = n - 31, s = t == 0 ? 1.0 : std::sin (3.14159265358979 * t * 0.5) / (3.14159265358979 * t * 0.5);
            const double w = 0.42 - 0.5 * std::cos (2 * 3.14159265358979 * n / 62) + 0.08 * std::cos (4 * 3.14159265358979 * n / 62);
            h[n] = (float) (s * w); sum += h[n];
        }
        for (float& v : h) v = (float) (v / sum);
        for (int i = 0; i < N; ++i) zu[i] = zd[i] = 0.f;
    }
    float fir (float* z, float x) { for (int i = N - 1; i > 0; --i) z[i] = z[i - 1]; z[0] = x; float a = 0.f; for (int i = 0; i < N; ++i) a += h[i] * z[i]; return a; }
    template <class F> float process (float x, F&& f)
    {
        const float u0 = fir (zu, 2.f * x), u1 = fir (zu, 0.f);
        const float v0 = f (u0), v1 = f (u1);
        fir (zd, v0); return fir (zd, v1);
    }
};

// Jiles-Atherton-Hysterese (Ms = 1), nach der Form fuer Echtzeit-Tape-Modelle (Chowdhury, DAFx 2019):
// dM/dH = (nc*dM*(Man-M)/(nc*delta*k - alpha*(Man-M)) + c*Man') / (1 - alpha*c*Man'),  nc = 1-c.
// a = Knie der Anhysterese, c = reversibler Anteil, k = Koerzitivitaet, alpha = Domaenenkopplung. Integration: RK4 mit Unterschritten.
struct Hysteresis
{
    float a = 1.f, c = 0.9f, k = 0.2f, alpha = 0.005f;
    float M = 0.f, Hprev = 0.f; bool primed = false;
    static float lang (float x)
    {
        x = std::clamp (x, -30.f, 30.f); const float ax = std::fabs (x);
        if (ax < 0.05f) { const float x2 = x * x; return x * (1.f / 3.f - x2 * (1.f / 45.f - x2 * (2.f / 945.f))); }
        return 1.f / std::tanh (x) - 1.f / x;
    }
    static float langD (float x)
    {
        x = std::clamp (x, -30.f, 30.f); const float ax = std::fabs (x);
        if (ax < 0.05f) { const float x2 = x * x; return 1.f / 3.f - x2 * (1.f / 15.f - x2 * (2.f / 189.f)); }
        const float sh = std::sinh (x); return 1.f / (x * x) - 1.f / (sh * sh);
    }
    float dMdH (float m, float h, float delta) const
    {
        const float nc = 1.f - c, q = (h + alpha * m) / a, md = lang (q) - m;
        const float gate = ((delta > 0.f) == (md > 0.f)) ? 1.f : 0.f;                 // irreversibel nur, wenn M zur Anhysterese hinlaeuft
        float den = nc * delta * k - alpha * md; if (std::fabs (den) < 1e-5f) den = delta * 1e-5f;
        const float lp = langD (q) / a;
        const float v = (nc * gate * md / den + c * lp) / (1.f - alpha * c * lp);
        return std::max (v, 0.f);                                                      // physikalisch: M waechst mit H
    }
    float step (float H)
    {
        if (! primed) { M = lang (H); Hprev = H; primed = true; return M; }   // Start im Ruhepunkt der Anhysterese: Stille bleibt exakt still
        const float dH = H - Hprev; const float adH = std::fabs (dH);
        if (adH > 0.f)
        {
            const int n = std::min (16, 1 + (int) (adH * 10.f)); const float h = dH / (float) n, delta = dH > 0.f ? 1.f : -1.f; float hh = Hprev;
            for (int i = 0; i < n; ++i)
            {
                const float k1 = dMdH (M, hh, delta) * h, k2 = dMdH (M + 0.5f * k1, hh + 0.5f * h, delta) * h, k3 = dMdH (M + 0.5f * k2, hh + 0.5f * h, delta) * h, k4 = dMdH (M + k3, hh + h, delta) * h;
                M += (k1 + 2.f * k2 + 2.f * k3 + k4) * (1.f / 6.f); hh += h;
            }
        }
        Hprev = H; return M;
    }
    void reset() { M = 0.f; Hprev = 0.f; primed = false; }
};

struct TapeParams
{
    int machine = 0;            // 0 TEAC A-3340S, 1 Revox B77
    int speed = 0;              // 0 = 15 ips, 1 = 7,5 ips, 2 = 3,75 ips (nur Revox)
    float driveDb = 0.f, bias = 0.5f, wf = 0.5f, bump = 0.5f, xtalk = 0.5f, link = 1.f;
    bool noise = true;
    bool hysteresis = true;     // Jiles-Atherton-Hysterese (an) oder statisches tanh (aus), zum Vergleichen
    // Nur TEAC A-3340S (getrennte Schalter RECORD-BIAS und RECORD-EQ, Bedienungsanleitung S. 15):
    int tapeType = 1;           // 0 = Standard (Scotch 150), 1 = Low Noise/High Output (Scotch 203)
    int biasSw = 1;             // 0 = NORMAL, 1 = HIGH
    int eqSw = 1;               // 0 = NORMAL, 1 = HIGH
};

struct TapeMachine
{
    float fs = 48000.f;
    struct Ch { Shelf1 pre, de; Svf bumpF, lp; OnePole hp, xt1, xt2, eqLp; Hysteresis hy; Half2x os; Delay dl; Delay byp; float env = 0.f; } ch[2];
    float tau = 0.f, wowS = 0.f, flS = 0.f, scA = 0.f, scB = 0.f, motPh = 0.f, eps = 0.f, mnN[2][2] = { { 0.f, 0.f }, { 0.f, 0.f } }, nScale[4] = { 1.f, 1.f, 1.f, 1.f }, nLf[2] = { 0.f, 0.f }, nHf[2] = { 0.f, 0.f }, noiseGain = 1.f;
    uint32_t rng = 12345u; float nScale2 = 1.f;
    int cfgMachine = -1, cfgSpeed = -1; float lastBias = -1.f;
    // Konfiguration (je Maschine/Geschwindigkeit)
    float A0 = 0.126f;                                 // Referenzpegel 0 VU = -18 dBFS (Sinus-Spitze)
    float satRatio = 0.35f, loFc = 30.f, hiFc = 22000.f, wfSpec = 0.15f, preZ = 4000.f, preP = 16000.f, bumpF0 = 63.f, noiseDb = -55.f, motorHz = 15.f;
    // Hysterese (Jiles-Atherton) statt statischem tanh. hysDrive bildet v/s auf H ab, so dass die Klirrwerte (1 %/3 %) erhalten bleiben (kalibriert im Messlauf).
    bool useHys = true; float hysK = 1.0f, hysC = 0.90f, hysAlpha = 0.005f, hysDrive = 2.32f, hysGain = 1.086f;
    static constexpr float centerMs = 1.5f;

    // 4-Punkt-Hermite-Interpolation fuer die Bandverzoegerung (lineare Interpolation daempfte bei 44,1 kHz schon bei 15 kHz um 2,2 dB).
    // Gleiche Konvention wie Delay::read: nach write() ergibt read(d) eine Verzoegerung von d-1 Samples.
    static float readCubic (const Delay& d, float delay)
    {
        const int sz = (int) d.b.size();
        float rp = (float) d.w - delay;
        while (rp < 0.f) rp += (float) sz;
        int i1 = (int) rp; const float t = rp - (float) i1;
        if (i1 >= sz) i1 -= sz;
        const float y0 = d.b[(size_t) ((i1 + sz - 1) % sz)], y1 = d.b[(size_t) i1], y2 = d.b[(size_t) ((i1 + 1) % sz)], y3 = d.b[(size_t) ((i1 + 2) % sz)];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    int latency() const { return (int) (centerMs * 0.001f * fs) + 31 - 1; }   // Delay::read(d) nach write() = d-1 Samples; per Messlauf (Tests/dsp_test.cpp) bestaetigt

    float rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) ((int32_t) rng) * (1.f / 2147483648.f); }

    float xtDb = -26.f;
    void configure (int machine, int speed)
    {
        cfgMachine = machine; cfgSpeed = speed;
        if (machine == 0)   // TEAC A-3340S: Zahlen aus dem Service-Manual (nur 15 und 7,5 ips)
        {
            const bool fast = speed == 0;
            satRatio = 0.35f;                           // 0 VU = 1 % Klirr (3 % bei +6 dB), daraus tanh-Skalierung
            loFc = fast ? 30.f : 40.f; hiFc = fast ? 22000.f : 18000.f;     // Frequenzgang +-3 dB: 30 Hz-22 kHz (15 ips), 40 Hz-18 kHz (7,5 ips)
            wfSpec = fast ? 0.15f : 0.18f;              // % RMS
            noiseDb = -55.f;                            // Rauschen -55 dB bezogen auf 0 dB (ungewichtet)
            preZ = 3183.f; preP = 14000.f;                  // Nullstelle = 50 us (NAB, TEAC-Testbaender 3180+50 us), Pol EST
            motorHz = 15.f;                             // 8-polig Hysterese-Synchronmotor an 60 Hz EST
            bumpF0 = fast ? 63.f : 32.f; xtDb = -26.f;
        }
        else                // Revox B77: Daten von reel-reel.com (Herstellerangaben), Plan-Bauteile fuer die Vorentzerrung
        {
            const int sp = std::clamp (speed, 0, 2);    // 0 = 15, 1 = 7,5, 2 = 3,75 ips
            static const float hi[3] = { 22000.f, 20000.f, 16000.f }, wf[3] = { 0.06f, 0.08f, 0.10f };
            static const float sat[3] = { 0.23f, 0.23f, 0.30f };        // Klirr 7,5 ips: 0 VU < 0,6 %, +6 dB < 1,5 %; 3,75 ips: < 1 % / < 2,5 %
            static const float nz[3] = { -59.f, -61.f, -58.f };         // Rauschabstand 65/67/64 dB (Bezug +6 dB = 514 nWb/m, angenommen ungewichtet) EST
            satRatio = sat[sp]; loFc = 30.f; hiFc = hi[sp]; wfSpec = wf[sp]; noiseDb = nz[sp];
            preZ = sp == 2 ? 1768.f : 3183.f; preP = sp == 2 ? 8000.f : 14000.f;   // NAB: 50 us (7,5 ips) bzw. 90 us (3,75 ips); 15 ips wie 7,5 (EST); Pol EST
            motorHz = 25.f;                             // Zahnrad-Tacho/Servo-Restwelligkeit EST
            bumpF0 = sp == 0 ? 63.f : (sp == 1 ? 32.f : 16.f); xtDb = -22.f;   // Uebersprechen > 45 dB bei 1 kHz (Stereo)
        }
        pre0();
    }
    void pre0()
    {
        for (auto& c : ch)
        {
            c.pre.set (preZ, preP, fs); c.de.set (preP, preZ, fs); c.pre.reset(); c.de.reset();
            c.bumpF.set (bumpF0, 1.2f, fs); c.bumpF.reset(); c.lp.set (std::min (hiFc * 1.12f, 0.49f * fs), 0.707f, fs, 0.49f); c.lp.reset();
            c.hp.set (loFc, fs); c.hp.reset(); c.xt1.set (200.f, fs); c.xt2.set (200.f, fs); c.xt1.reset(); c.xt2.reset(); c.eqLp.set (6000.f, fs); c.eqLp.reset();
        }
        lastBias = -1.f;
    }
    void prepare (float f)
    {
        fs = f; for (auto& c : ch) { c.hy.reset(); c.os.init(); c.dl.init ((int) (0.02f * fs)); c.byp.init ((int) (0.02f * fs)); c.env = 0.f; }
        tau = wowS = flS = scA = scB = motPh = eps = 0.f; nLf[0] = nLf[1] = nHf[0] = nHf[1] = 0.f; configure (0, 0);
        // Rausch-Normierung: Referenz-RMS bestimmen
        float a = 1.f - std::exp (-2.f * kPi * 12000.f / fs), b = 1.f - std::exp (-2.f * kPi * 400.f / fs), lf = 0.f, hf = 0.f; double s = 0; uint32_t save = rng;
        for (int i = 0; i < 48000; ++i) { hf += a * (rnd() - hf); lf += b * (rnd() - lf); const float v = hf + 1.5f * lf; s += (double) v * v; }
        rng = save; noiseGain = (float) (1.0 / std::sqrt (s / 48000.0));
        // Normierung der Flutter-/Modulationsrausch-Quellen auf RMS 1 (Rauschen gleichverteilt in [-1,1])
        auto lpRms = [&] (float fc1, float fc2) { const float a1 = 1.f - std::exp (-2.f * kPi * fc1 / fs), a2 = fc2 > 0.f ? 1.f - std::exp (-2.f * kPi * fc2 / fs) : 0.f; float y1 = 0.f, y2 = 0.f; double q = 0; uint32_t sv = rng;
                                                  for (int i = 0; i < 96000; ++i) { const float x = rnd(); y1 += a1 * (x - y1); y2 += a2 * (x - y2); const float v = fc2 > 0.f ? y1 - y2 : y1; if (i > 8000) q += (double) v * v; } rng = sv; return (float) (1.0 / std::sqrt (q / 88000.0)); };
        auto an = [&] (float fc) { const float a = 1.f - std::exp (-2.f * kPi * fc / fs); return 1.f / std::sqrt (a / (2.f - a) / 3.f); };   // analytisch: Einpol-Tiefpass auf Gleichverteilung [-1,1]
        nScale[0] = an (1.2f); nScale[1] = an (12.f); nScale[2] = lpRms (300.f, 100.f); nScale[3] = an (20.f); nScale2 = an (800.f);
    }
    void processBypass (float* L, float* R, int n)
    {
        const float d = (float) latency() + 1.f;     // +1: gleiche write-vor-read-Konvention wie der Tape-Pfad
        for (int i = 0; i < n; ++i) { ch[0].byp.write (L[i]); ch[1].byp.write (R[i]); L[i] = ch[0].byp.read (d); R[i] = ch[1].byp.read (d); }
    }
    void process (float* L, float* R, int n, const TapeParams& p)
    {
        const int spd = p.machine == 0 ? std::min (p.speed, 1) : p.speed;
        if (p.machine != cfgMachine || spd != cfgSpeed) configure (p.machine, spd);
        // Band/Schalter-Fehlanpassung (nur TEAC). Handbuch-Tabelle S. 15: Standardband mit HIGH-Bias = Hoehenverlust (uebervormagnetisiert),
        // mit HIGH-EQ = Hoehenverlust; Low-Noise-Band mit NORMAL-Bias = metallisch/Hoehenanhebung (untervormagnetisiert), mit NORMAL-EQ ebenso.
        // +1 = Schalter "zu hoch" fuer das Band, -1 = "zu niedrig". Die Groessen (dB, Biasverschiebung) sind EST, das Handbuch nennt nur die Richtung.
        const int mmBias = p.machine == 0 ? std::clamp (p.biasSw, 0, 1) - std::clamp (p.tapeType, 0, 1) : 0;
        const int mmEq   = p.machine == 0 ? std::clamp (p.eqSw,   0, 1) - std::clamp (p.tapeType, 0, 1) : 0;
        const float effBias = std::clamp (p.bias + 0.2f * (float) mmBias, 0.f, 1.f);
        const float shelfDb = -(3.f * (float) mmEq + 2.f * (float) mmBias);          // Hoehen-Shelf ab ca. 6 kHz, EST
        const float eqG = std::pow (10.f, shelfDb / 20.f) - 1.f;
        if (std::fabs (effBias - lastBias) > 1e-4f)
        {
            lastBias = effBias; const float fc = std::min (hiFc * 1.12f * (1.08f - 0.16f * effBias), 0.49f * fs);
            for (auto& c : ch) c.lp.set (fc, 0.707f, fs, 0.49f);
        }
        const float inGain = std::pow (10.f, p.driveDb / 20.f), outGain = std::pow (10.f, -p.driveDb * p.link / 20.f);
        const float s = A0 / satRatio * (0.85f + 0.3f * effBias), bAsym = 0.03f;
        const float hk = hysK * (1.5f - effBias);                                    // EST: mehr Bias = schmalere Schleife, weniger Hysterese-Verzerrung
        for (auto& c : ch) { c.hy.a = 1.f; c.hy.c = hysC; c.hy.k = hk; c.hy.alpha = std::min (hysAlpha, 0.6f * (1.f - hysC) * hk); }
        const float hysOut = 3.f * hysGain / hysDrive, hysDc = Hysteresis::lang (hysDrive * bAsym);   // Kleinsignal-Verstaerkung 1; Gleichanteil der Asymmetrie entfernt
        const float G = std::pow (10.f, p.bump * 6.f / 20.f) - 1.f, gx = std::pow (10.f, xtDb / 20.f) * p.xtalk * 2.f;
        const float epsRms = wfSpec * 0.01f * p.wf * 2.f;
        const float aW = 1.f - std::exp (-2.f * kPi * 1.2f / fs), aF = 1.f - std::exp (-2.f * kPi * 12.f / fs);
        const float aS1 = 1.f - std::exp (-2.f * kPi * 300.f / fs), aS2 = 1.f - std::exp (-2.f * kPi * 100.f / fs);
        const float aM1 = 1.f - std::exp (-2.f * kPi * 20.f / fs), aM2 = 1.f - std::exp (-2.f * kPi * 800.f / fs);
        const float mnNarrow = std::pow (10.f, -45.f / 20.f), mnBroad = std::pow (10.f, -38.f / 20.f);   // Modulationsrauschen (EST, Quellen 25 ... 76 dB)
        const int centerSamples = (int) (centerMs * 0.001f * fs);   // ganzzahlig: ohne Flutter keine Interpolationsverluste
        const float leak = std::exp (-2.f * kPi * 0.4f / fs);
        const float nA = 1.f - std::exp (-2.f * kPi * 12000.f / fs), nB = 1.f - std::exp (-2.f * kPi * 400.f / fs);
        const float noiseAmp = p.noise ? A0 * 0.7071f * std::pow (10.f, noiseDb / 20.f) * noiseGain : 0.f;
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            float sat[2];
            for (int c = 0; c < 2; ++c)
            {
                const float x0 = io[c][i] * inGain, x = x0 + eqG * (x0 - ch[c].eqLp.lp (x0)), pe = ch[c].pre.run (x);
                if (useHys && p.hysteresis)
                    sat[c] = ch[c].os.process (pe, [&] (float v) { return s * hysOut * (ch[c].hy.step (hysDrive * (v / s + bAsym)) - hysDc); });
                else
                    sat[c] = ch[c].os.process (pe, [&] (float v) { return s * (std::tanh (v / s + bAsym) - std::tanh (bAsym)); });
            }
            wowS += aW * (rnd() - wowS); flS += aF * (rnd() - flS);
            scA += aS1 * (rnd() - scA); scB += aS2 * (rnd() - scB);
            motPh += motorHz / fs; motPh -= std::floor (motPh);
            // Wow 0,5-6 Hz und Flutter 6-200 Hz getrennt (Wikipedia/Messgeraet), Scrape-Flutter > 100 Hz, periodischer Capstan-/Motoranteil
            eps = epsRms * (0.66f * wowS * nScale[0] + 0.58f * flS * nScale[1] + 0.25f * (scA - scB) * nScale[2] + 0.40f * 1.4142f * std::sin (2.f * kPi * motPh));
            tau = std::clamp (leak * tau - eps / fs, -0.0012f, 0.0012f);
            for (int c = 0; c < 2; ++c)
            {
                float y = sat[c] + gx * ch[c].xt2.lp (ch[c].xt1.lp (sat[1 - c]));
                float l, b, h; ch[c].bumpF.run (y, l, b, h); y += G * b / 1.2f;
                y = ch[c].de.run (y);
                y = ch[c].lp.lp (ch[c].hp.hp (y));
                ch[c].dl.write (y);
                y = readCubic (ch[c].dl, (float) centerSamples + tau * fs);
                if (p.noise)
                {
                    nHf[c] += nA * (rnd() - nHf[c]); nLf[c] += nB * (rnd() - nLf[c]);
                    mnN[c][0] += aM1 * (rnd() - mnN[c][0]); mnN[c][1] += aM2 * (rnd() - mnN[c][1]);
                    y *= 1.f + mnNarrow * mnN[c][0] * nScale[3] + mnBroad * mnN[c][1] * nScale2;   // multiplikativ: Rauschsaum um jeden Ton (AES-Abstract Vanderkooy)
                    y += (nHf[c] + 1.5f * nLf[c]) * noiseAmp;                                      // Bandrauschen
                }
                io[c][i] = y * outGain;
            }
        }
    }
};
} // namespace pb
