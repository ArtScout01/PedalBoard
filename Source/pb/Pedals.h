#pragma once
// Pedal-Modelle ohne JUCE-Abhaengigkeit (testbar). Jedes Pedal verarbeitet einen Mono-Block in-place.
// Die Modelle folgen der Schaltungstopologie des jeweiligen Pedals (Stufen, Koppelglieder, Nichtlinearitaeten,
// BBD-Bandbreite, OTA-Kennlinie usw.). Mit "EST" markierte Werte sind Schaetzungen, bis ein Original-Schaltplan
// bzw. eine Messung vorliegt.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace pb
{
constexpr float kPi = 3.14159265358979f;
inline float expMap (float t, float a, float b) { return a * std::pow (b / a, t); }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
inline float coef (float ms, float fs) { return 1.f - std::exp (-1.f / (std::max (0.01f, ms) * 0.001f * fs)); }

struct OnePole
{
    float a = 0.f, z = 0.f;
    void set (float fc, float fs) { a = 1.f - std::exp (-2.f * kPi * std::min (fc, 0.45f * fs) / fs); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { z += a * (x - z); return x - z; }
    void reset() { z = 0.f; }
};

struct Svf   // TPT state variable filter
{
    float g = 0.f, k = 1.f, a1 = 0.f, a2 = 0.f, a3 = 0.f, ic1 = 0.f, ic2 = 0.f;
    void set (float fc, float Q, float fs)
    {
        g = std::tan (kPi * std::min (fc, 0.45f * fs) / fs);
        k = 1.f / Q;
        a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void run (float v0, float& l, float& b, float& h)
    {
        const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
        l = v2; b = v1; h = v0 - k * v1 - v2;
    }
    float lp (float x) { float l, b, h; run (x, l, b, h); return l; }
    void reset() { ic1 = ic2 = 0.f; }
};

struct Smooth
{
    float v = 0.f, a = 0.f; bool init = false;
    void prepare (float fs, float ms = 25.f) { a = coef (ms, fs); init = false; }
    float next (float t) { if (! init) { v = t; init = true; } v += a * (t - v); return v; }
};

struct Delay
{
    std::vector<float> b; int w = 0;
    void init (int len) { b.assign ((size_t) len + 4, 0.f); w = 0; }
    void write (float x) { b[(size_t) w] = x; if (++w >= (int) b.size()) w = 0; }
    float read (float d) const
    {
        const int sz = (int) b.size();
        float rp = (float) w - d;
        while (rp < 0.f) rp += (float) sz;
        int i0 = (int) rp; const float fr = rp - (float) i0;
        if (i0 >= sz) i0 -= sz;
        int i1 = i0 + 1; if (i1 >= sz) i1 -= sz;
        return b[(size_t) i0] * (1.f - fr) + b[(size_t) i1] * fr;
    }
};

struct AP1 { float a = 0.65f, z = 0.f; float run (float x) { const float y = a * x + z; z = x - a * y; return y; } };

struct P { float k[4] = { 0.5f, 0.5f, 0.5f, 0.5f }; int s[2] = { 0, 0 }; };

struct Pedal
{
    virtual ~Pedal() = default;
    virtual void prepare (float fs) = 0;
    virtual void process (float* x, int n, const P& p) = 0;
};

// ---------------------------------------------------------------- Fuzz Face (2 Transistoren, DC-gekoppelt)
// k0 Fuzz, k1 Volume, s0 Germanium(0)/Silizium(1)
struct FuzzFace : Pedal
{
    float fs = 48000.f; OnePole hpIn, hpDc, lpOut; Smooth sf, sv;
    void prepare (float f) override { fs = f; hpIn.set (6.f, fs); hpDc.set (40.f, fs); lpOut.set (7000.f, fs); sf.prepare (fs); sv.prepare (fs); }
    void process (float* x, int n, const P& p) override
    {
        const bool si = p.s[0] != 0;
        const float b1 = si ? 0.10f : 0.32f, b2 = si ? 0.08f : 0.22f;   // Ge: groessere Asymmetrie (EST)
        for (int i = 0; i < n; ++i)
        {
            const float fz = sf.next (p.k[0]), vol = sv.next (p.k[1]);
            const float in = hpIn.hp (x[i]);                               // 2.2u Eingangskondensator
            const float g1 = 3.f + 70.f * fz * fz, g2 = 5.f + 12.f * fz;   // Rueckkopplung ueber Fuzz-Poti (EST)
            const float q1 = std::tanh (g1 * in + b1) - std::tanh (b1);
            const float q2 = std::tanh (g2 * q1 * 0.7f + b2) - std::tanh (b2);
            x[i] = lpOut.lp (hpDc.hp (q2)) * vol * 0.55f;
        }
    }
};

// ---------------------------------------------------------------- Boss BD-2 Blues Driver
// k0 Gain, k1 Tone, k2 Level
struct BD2 : Pedal
{
    float fs = 48000.f; OnePole hpIn, tone, lpOut; Svf mid; Smooth sg, st, sl;
    void prepare (float f) override
    {
        fs = f; hpIn.set (15.f, fs); tone.set (720.f, fs); lpOut.set (12000.f, fs); mid.set (1000.f, 0.9f, fs);
        sg.prepare (fs); st.prepare (fs); sl.prepare (fs);
    }
    void process (float* x, int n, const P& p) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float gk = sg.next (p.k[0]), t = st.next (p.k[1]), lv = sl.next (p.k[2]);
            const float G = 1.f + 160.f * gk * gk;
            float l, b, h; mid.run (hpIn.hp (x[i]), l, b, h);
            const float m = x[i] * 0.f + hpIn.z * 0.f + (l + h) * 0.f + b * 0.9f + (l + b + h);   // Mitten-Betonung (~ +5 dB)
            const float s1 = (std::tanh (0.8f * G * m + 0.12f) - std::tanh (0.12f)) / 0.8f;      // Stufe 1, asymmetrisch
            const float s2 = std::tanh (1.5f * s1) / 1.5f;                                       // Stufe 2, weich
            const float lo = tone.lp (s2), hi = s2 - lo;
            x[i] = lpOut.lp ((lo * (1.f - t) + hi * t) * 1.4f) * lv * 1.6f;
        }
    }
};

// ---------------------------------------------------------------- MXR Dyna Comp (OTA CA3080 + Detektor)
// k0 Output, k1 Sensitivity
struct DynaComp : Pedal
{
    float fs = 48000.f, env = 0.f, atk = 0.f, rel = 0.f; Smooth so, ss;
    void prepare (float f) override { fs = f; atk = coef (6.f, fs); rel = coef (190.f, fs); so.prepare (fs); ss.prepare (fs); env = 0.f; }
    void process (float* x, int n, const P& p) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float out = so.next (p.k[0]), sens = ss.next (p.k[1]);
            const float a = std::fabs (x[i]);
            env += (a > env ? atk : rel) * (a - env);
            const float c = 6.f + 60.f * sens * sens;                  // Detektor-Staerke (EST)
            const float gain = 1.f / (1.f + c * env);                  // OTA-Steuerstrom sinkt mit Pegel
            const float makeup = std::pow (1.f + 0.1f * c, 0.8f);
            const float vd = x[i] * gain * 0.06f;                      // Differenz-Eingang des OTA
            x[i] = std::tanh (vd / 0.052f) * 0.052f / 0.06f * makeup * out * 2.f;   // tanh = Diffpaar-Kennlinie
        }
    }
};

// ---------------------------------------------------------------- EHX Small Stone (4 Allpass-Stufen, OTA-LFO)
// k0 Rate, s0 Color (Feedback)
struct SmallStone : Pedal
{
    float fs = 48000.f, ph = 0.f, fb = 0.f, z[4] = { 0, 0, 0, 0 }; Smooth sr;
    void prepare (float f) override { fs = f; sr.prepare (fs, 60.f); ph = 0.f; fb = 0.f; for (float& v : z) v = 0.f; }
    void process (float* x, int n, const P& p) override
    {
        const float fbAmt = p.s[0] ? 0.65f : 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float rate = expMap (sr.next (p.k[0]), 0.1f, 7.f);
            ph += rate / fs; if (ph >= 1.f) ph -= 1.f;
            const float tri = 1.f - std::fabs (2.f * ph - 1.f);
            const float fc = expMap (tri, 250.f, 3500.f);
            const float t = std::tan (kPi * std::min (fc, 0.45f * fs) / fs);
            const float a = (t - 1.f) / (t + 1.f);
            float y = x[i] + fbAmt * fb;
            for (float& zz : z) { const float o = a * y + zz; zz = y - a * o; y = o; }
            fb = std::tanh (y);
            x[i] = 0.5f * (x[i] + y) * 1.3f;
        }
    }
};

// ---------------------------------------------------------------- MXR Carbon Copy (BBD MN3205, 4096 Stufen)
// k0 Delay (20-600 ms), k1 Regen, k2 Mix, s0 Mod
struct CarbonCopy : Pedal
{
    float fs = 48000.f, lfo = 0.f, T = 0.3f; Delay d; Svf pre1, pre2, post1, post2; OnePole fbHp; Smooth sd, sr, sm; int ctr = 0;
    void prepare (float f) override
    {
        fs = f; d.init ((int) (0.72f * fs)); lfo = 0.f; fbHp.set (90.f, fs);
        sd.prepare (fs, 120.f); sr.prepare (fs); sm.prepare (fs);
        for (Svf* s : { &pre1, &pre2, &post1, &post2 }) s->reset();
    }
    void process (float* x, int n, const P& p) override
    {
        for (int i = 0; i < n; ++i)
        {
            T = sd.next (lerpf (0.02f, 0.6f, p.k[0]));
            if ((ctr++ & 63) == 0)
            {
                const float fclk = 4096.f / (2.f * T);                 // Delay = N / (2 * f_clk)
                const float fc = std::min (0.85f * fclk * 0.25f, 9500.f);   // Nyquist der BBD = f_clk / 4
                for (Svf* s : { &pre1, &pre2, &post1, &post2 }) s->set (fc, 0.7f, fs);
            }
            lfo += 0.55f / fs; if (lfo >= 1.f) lfo -= 1.f;
            const float mod = p.s[0] ? 0.0007f * (1.f - std::fabs (2.f * lfo - 1.f) * 2.f) : 0.f;   // EST
            const float rd = d.read (std::max (2.f, (T + mod) * fs));
            const float wet = post2.lp (post1.lp (rd));
            const float regen = sr.next (p.k[1]) * 1.02f;
            const float fbv = std::tanh (fbHp.hp (wet) * regen);
            d.write (pre2.lp (pre1.lp (x[i] + fbv)));
            x[i] = x[i] + wet * sm.next (p.k[2]) * 1.1f;
        }
    }
};

// ---------------------------------------------------------------- Dunlop Cry Baby (Wah, Induktivitaet 500 mH)
// k0 Pedal (Ferse..Spitze), k1 Volume
struct CryBaby : Pedal
{
    float fs = 48000.f; Svf f; Smooth sp, sv; int ctr = 0;
    void prepare (float fr) override { fs = fr; sp.prepare (fs, 15.f); sv.prepare (fs); f.reset(); }
    void process (float* x, int n, const P& p) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float pos = sp.next (p.k[0]), vol = sv.next (p.k[1]);
            if ((ctr++ & 15) == 0) f.set (expMap (pos, 420.f, 2100.f), 5.5f, fs);   // EST
            float l, b, h; f.run (x[i], l, b, h);
            x[i] = (b / 5.5f * 2.f * 2.5f + 0.15f * l) * vol * 2.f;
        }
    }
};

// ---------------------------------------------------------------- EHX Holy Grail (Spring / Hall)
// k0 Reverb, s0 Spring(0)/Hall(1)
struct Spring
{
    Delay d; AP1 ap[22]; OnePole lp, hp; float len = 0.034f, fs = 48000.f;
    void init (float f, float l) { fs = f; len = l; d.init ((int) (0.08f * fs)); lp.set (3500.f, fs); hp.set (110.f, fs); for (auto& a : ap) { a.a = 0.65f; a.z = 0.f; } }
    float run (float in)
    {
        float y = d.read (len * fs);
        for (auto& a : ap) y = a.run (y);
        y = hp.hp (lp.lp (y));
        d.write (in + 0.76f * std::tanh (y));
        return y;
    }
};
struct Comb { Delay d; int len = 0; float filt = 0.f;
    void init (int l) { len = l; d.init (l + 2); filt = 0.f; }
    float run (float in, float fb, float damp) { const float o = d.read ((float) len); filt = o * (1.f - damp) + filt * damp; d.write (in + filt * fb); return o; } };
struct Allpass { Delay d; int len = 0; void init (int l) { len = l; d.init (l + 2); }
    float run (float in) { const float b = d.read ((float) len); const float v = in + b * 0.5f; d.write (v); return -0.5f * v + b; } };

struct HolyGrail : Pedal
{
    float fs = 48000.f; Spring sp1, sp2; Comb comb[8]; Allpass ap[4]; Smooth sm; OnePole inLp;
    void prepare (float f) override
    {
        fs = f; sm.prepare (fs);
        sp1.init (fs, 0.0345f); sp2.init (fs, 0.0442f);
        const int cl[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 }, al[4] = { 556, 441, 341, 225 };
        const float sc = fs / 44100.f;
        for (int i = 0; i < 8; ++i) comb[i].init ((int) (cl[i] * sc));
        for (int i = 0; i < 4; ++i) ap[i].init ((int) (al[i] * sc));
        inLp.set (4500.f, fs);
    }
    void process (float* x, int n, const P& p) override
    {
        const bool hall = p.s[0] != 0;
        for (int i = 0; i < n; ++i)
        {
            const float k = sm.next (p.k[0]);
            float wet;
            if (hall)
            {
                const float in = x[i] * 0.015f; float s = 0.f;
                for (auto& c : comb) s += c.run (in, 0.86f, 0.25f);
                for (auto& a : ap) s = a.run (s);
                wet = s * 3.f;
            }
            else
            {
                const float in = inLp.lp (x[i]);
                wet = 0.5f * (sp1.run (in) + sp2.run (in)) * 1.2f;
            }
            x[i] = x[i] * (1.f - 0.35f * k) + wet * k * 1.5f;
        }
    }
};

// ---------------------------------------------------------------- Diamond Vibrato (modulierte Verzoegerung, 100 % nass)
// k0 Speed, k1 Depth, k2 Volume
struct Diamond : Pedal
{
    float fs = 48000.f, ph = 0.f; Delay d; Smooth sr, sd, sv;
    void prepare (float f) override { fs = f; d.init ((int) (0.03f * fs)); sr.prepare (fs, 60.f); sd.prepare (fs); sv.prepare (fs); ph = 0.f; }
    void process (float* x, int n, const P& p) override
    {
        for (int i = 0; i < n; ++i)
        {
            ph += expMap (sr.next (p.k[0]), 0.8f, 10.f) / fs; if (ph >= 1.f) ph -= 1.f;
            const float dep = sd.next (p.k[1]) * 0.0045f;
            d.write (x[i]);
            x[i] = d.read ((0.006f + dep * (0.5f + 0.5f * std::sin (2.f * kPi * ph))) * fs) * sv.next (p.k[2]) * 2.f;
        }
    }
};

// ---------------------------------------------------------------- DOD 210 Bi-FET Preamp (2 JFET-Stufen)
// k0 Gain, k1 Volume
struct BiFet : Pedal
{
    float fs = 48000.f; OnePole hp, lp; Smooth sg, sv;
    void prepare (float f) override { fs = f; hp.set (70.f, fs); lp.set (12000.f, fs); sg.prepare (fs); sv.prepare (fs); }
    void process (float* x, int n, const P& p) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float A = 1.f + 11.f * sg.next (p.k[0]);
            float y = std::tanh (0.9f * A * hp.hp (x[i]) + 0.05f) - std::tanh (0.05f);   // JFET-Stufe 1
            y = std::tanh (1.4f * y * 0.9f) / 1.26f;                                      // JFET-Stufe 2
            x[i] = lp.lp (y) / 0.9f * sv.next (p.k[1]) * 1.6f;
        }
    }
};

// ---------------------------------------------------------------- Brigade Chorus (CE-1-Modell, BBD MN3002 + Preamp)
// k0 Rate, k1 Intensity, k2 Preamp, s0 Chorus(0)/Vibrato(1)
struct Brigade : Pedal
{
    float fs = 48000.f, ph = 0.f; Delay d; Svf l1, l2; Smooth sr, si, sp;
    void prepare (float f) override
    {
        fs = f; d.init ((int) (0.03f * fs)); sr.prepare (fs, 60.f); si.prepare (fs); sp.prepare (fs); ph = 0.f;
        l1.set (7500.f, 0.7f, fs); l2.set (7500.f, 0.7f, fs); l1.reset(); l2.reset();
    }
    void process (float* x, int n, const P& p) override
    {
        const bool vib = p.s[0] != 0;
        for (int i = 0; i < n; ++i)
        {
            ph += expMap (sr.next (p.k[0]), 0.2f, 8.f) / fs; if (ph >= 1.f) ph -= 1.f;
            const float tri = 1.f - std::fabs (2.f * ph - 1.f);
            const float pre = std::tanh (x[i] * (1.f + 3.f * sp.next (p.k[2]))) / (1.f + 1.5f * p.k[2]);   // CE-1-Preamp
            d.write (pre);
            const float dt = 0.0035f + 0.0035f * si.next (p.k[1]) * (vib ? 1.5f : 1.f) * tri;               // EST
            const float wet = l2.lp (l1.lp (d.read (dt * fs)));
            x[i] = vib ? wet * 1.1f : 0.7f * (pre + wet);
        }
    }
};
} // namespace pb
