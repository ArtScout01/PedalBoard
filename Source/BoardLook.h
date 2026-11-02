#pragma once
// Aussehen des Pedalboards nach dem Layout-Entwurf (Pedale auf Teppich). Alles in Entwurfskoordinaten 1536 x 800,
// der Editor skaliert. Die Grafiken sind in Code gezeichnet, keine Bilddateien.
#include <JuceHeader.h>
#include "pb/PedalFactory.h"

namespace board
{
constexpr float kDesignW = 1536.f, kDesignH = 800.f;

struct Skin { juce::uint32 top, bottom, text; const char* title; const char* subtitle; };

// Reihenfolge wie pb::Kind: FuzzFace, TS808, DynaComp, CryBaby, Phase90, CE2, UniVibe, VB2, MicroAmp, DM2, Spring6G15, MidiVerb, H910, VP330, VE20
inline const Skin& skinFor (pb::Kind k)
{
    static const Skin t[] = {
        { 0xffd8d8d6, 0xff8d8d8b, 0xff111111, "FUZZ FACE", "" },
        { 0xff2fa04a, 0xff1c7a34, 0xff0b2a12, "TUBE SCREAMER", "OVERDRIVE" },
        { 0xffd33a2f, 0xff9c1c16, 0xfff3e6d0, "DYNA COMP", "" },
        { 0xff262626, 0xff0a0a0a, 0xff9c9c9c, "Cry Baby", "" },
        { 0xffee7d1f, 0xffc9600f, 0xff2a1500, "Phase 90", "" },
        { 0xff41b6e8, 0xff2b8bc0, 0xffffffff, "Chorus", "CE-2" },
        { 0xff4b2f73, 0xff2f1a4f, 0xfff0eaf8, "Uni-Vibe", "" },
        { 0xff41b6e8, 0xff2b8bc0, 0xffffffff, "Vibrato", "VB-2" },
        { 0xfff1ead8, 0xffcfc6ab, 0xff222222, "MICRO AMP", "" },
        { 0xffd9308f, 0xffa11d66, 0xff220a18, "Delay", "DM-2" },
        { 0xffc9c9c7, 0xff8f8f8d, 0xff1a1a1a, "Reverb", "6G15" },
        { 0xff1f3a73, 0xff0d1c40, 0xffffffff, "MIDIverb", "" },
        { 0xff1c1c1c, 0xff050505, 0xffe8e8e8, "H910", "HARMONIZER" },
        { 0xffe9e0c8, 0xffc2b793, 0xff1a1a1a, "VP-330", "" },
        { 0xffc4c4c2, 0xff8d8d8b, 0xff1a1a1a, "VE-20", "VOCAL" },
    };
    return t[(int) k];
}

class PedalLNF : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override
    {
        const float r = (float) std::min (w, h) * 0.5f - 2.f;
        const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillEllipse (c.x - r, c.y - r + 2.f, 2.f * r, 2.f * r);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff555555), c.x - r * 0.5f, c.y - r * 0.6f, juce::Colour (0xff0b0b0b), c.x + r * 0.7f, c.y + r * 0.8f, true));
        g.fillEllipse (c.x - r, c.y - r, 2.f * r, 2.f * r);
        g.setColour (juce::Colour (0xff1c1c1c)); g.drawEllipse (c.x - r, c.y - r, 2.f * r, 2.f * r, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));                                   // Raendel
        for (int i = 0; i < 28; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::twoPi / 28.f;
            g.drawLine (c.x + std::sin (a) * r * 0.86f, c.y - std::cos (a) * r * 0.86f, c.x + std::sin (a) * r * 0.97f, c.y - std::cos (a) * r * 0.97f, 1.f);
        }
        const float ang = startAngle + pos * (endAngle - startAngle);
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.drawLine (c.x + std::sin (ang) * r * 0.30f, c.y - std::cos (ang) * r * 0.30f, c.x + std::sin (ang) * r * 0.82f, c.y - std::cos (ang) * r * 0.82f, std::max (1.6f, r * 0.13f));
    }

    // Wah-Pedal: senkrechter Regler = Pedalstellung. Zeichnet einen hellen Streifen an der aktuellen Position.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float, juce::Slider::SliderStyle, juce::Slider&) override
    {
        const float yy = std::clamp (sliderPos, (float) y + 12.f, (float) (y + h) - 12.f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), 0.f, yy - 16.f, juce::Colours::white.withAlpha (0.28f), 0.f, yy, false));
        g.fillRoundedRectangle ((float) x + 2.f, yy - 16.f, (float) w - 4.f, 32.f, 4.f);
        g.setColour (juce::Colours::white.withAlpha (0.45f)); g.drawLine ((float) x + 4.f, yy, (float) (x + w) - 4.f, yy, 1.5f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const float h = std::min (r.getHeight(), 16.f), w = h * 1.9f;
        const juce::Rectangle<float> pill (r.getX() + 2.f, r.getCentreY() - h * 0.5f, w, h);
        g.setColour (juce::Colour (0xff101010)); g.fillRoundedRectangle (pill, h * 0.5f);
        const float kx = b.getToggleState() ? pill.getRight() - h * 0.5f - 1.f : pill.getX() + h * 0.5f + 1.f;
        g.setColour (b.getToggleState() ? juce::Colour (0xffe8d44a) : juce::Colour (0xff9a9a9a));
        g.fillEllipse (kx - h * 0.4f, pill.getCentreY() - h * 0.4f, h * 0.8f, h * 0.8f);
        g.setColour (b.findColour (juce::ToggleButton::textColourId)); g.setFont (juce::FontOptions (h * 0.78f));
        g.drawText (b.getButtonText(), juce::Rectangle<float> (pill.getRight() + 4.f, r.getY(), std::max (1.f, r.getRight() - pill.getRight() - 4.f), r.getHeight()), juce::Justification::centredLeft);
    }
};

// Fussschalter (Chrom). Zustand = Pedal an/aus; die LED zeichnet die Pedal-Komponente.
class FootSwitch : public juce::Button
{
public:
    FootSwitch() : juce::Button ("foot") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics& g, bool, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.f);
        const float d = std::min (r.getWidth(), r.getHeight()); const auto c = r.getCentre();
        const float R = d * 0.5f;
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (c.x - R, c.y - R + 2.f, 2.f * R, 2.f * R);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a6a6a), c.x - R * 0.7f, c.y - R * 0.7f, juce::Colour (0xff202020), c.x + R * 0.7f, c.y + R * 0.8f, true));
        g.fillEllipse (c.x - R, c.y - R, 2.f * R, 2.f * R);
        const float r2 = R * 0.72f;
        g.setGradientFill (juce::ColourGradient (down ? juce::Colour (0xffb8b8b8) : juce::Colour (0xfff2f2f2), c.x - r2 * 0.5f, c.y - r2 * 0.6f, juce::Colour (0xff8c8c8c), c.x + r2 * 0.6f, c.y + r2 * 0.8f, true));
        g.fillEllipse (c.x - r2, c.y - r2 + (down ? 1.f : 0.f), 2.f * r2, 2.f * r2);
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.drawEllipse (c.x - r2, c.y - r2 + (down ? 1.f : 0.f), 2.f * r2, 2.f * r2, 1.f);
    }
};

// Teppich (Ornament-Muster), schwarze Schiene mit Klettband. Einmal in Bildgroesse gerendert.
inline juce::Image makeRug (int w, int h)
{
    juce::Image img (juce::Image::ARGB, std::max (w, 1), std::max (h, 1), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale ((float) w / kDesignW));
    g.fillAll (juce::Colour (0xff5e1717));
    for (int gy = 0; gy < 14; ++gy)
        for (int gx = 0; gx < 26; ++gx)
        {
            const float cx = (float) gx * 60.f + 30.f, cy = (float) gy * 60.f + 30.f; const int v = (gx * 7 + gy * 13) % 3;
            juce::Path d; d.addQuadrilateral (cx, cy - 24.f, cx + 20.f, cy, cx, cy + 24.f, cx - 20.f, cy);
            g.setColour ((v == 0 ? juce::Colour (0xff1b2147) : (v == 1 ? juce::Colour (0xff7a2a1e) : juce::Colour (0xff23305e))).withAlpha (0.92f)); g.fillPath (d);
            juce::Path d2; d2.addQuadrilateral (cx, cy - 10.f, cx + 8.f, cy, cx, cy + 10.f, cx - 8.f, cy);
            g.setColour (juce::Colour (0xffd9c9a0).withAlpha (0.75f)); g.fillPath (d2);
            g.setColour (juce::Colour (0xffd9c9a0).withAlpha (0.35f)); g.drawEllipse (cx + 25.f, cy - 5.f, 10.f, 10.f, 1.f);
        }
    g.setColour (juce::Colour (0xffc9b27a).withAlpha (0.6f)); g.drawRect (8.f, 8.f, kDesignW - 16.f, kDesignH - 16.f, 3.f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, kDesignW * 0.5f, kDesignH * 0.5f, juce::Colours::black.withAlpha (0.55f), 0.f, 0.f, true));
    g.fillRect (0.f, 0.f, kDesignW, kDesignH);
    // Schiene
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (42.f, 204.f, 1485.f, 562.f, 10.f);
    g.setColour (juce::Colour (0xff0c0c0c)); g.fillRoundedRectangle (38.f, 198.f, 1485.f, 562.f, 10.f);
    g.setColour (juce::Colour (0xff2a2a2a).withAlpha (0.55f));
    for (float yy = 210.f; yy < 750.f; yy += 5.f) g.drawHorizontalLine ((int) yy, 48.f, 1512.f);
    g.setColour (juce::Colour (0xff3a3a3a)); g.drawRoundedRectangle (38.f, 198.f, 1485.f, 562.f, 10.f, 2.f);
    return img;
}
} // namespace board
