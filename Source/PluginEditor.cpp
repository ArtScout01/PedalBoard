#include "PluginEditor.h"

using juce::Point;
using juce::Rectangle;

// ============================================================================ Pedal
PedalComponent::PedalComponent (PedalBoardProcessor& p, int idx) : pedal (idx)
{
    kind = p.slotKind (0, idx); inf = &pb::info (kind); sk = &board::skinFor (kind); nk = inf->nk;
    isFuzz = kind == pb::Kind::FuzzFace; isWah = kind == pb::Kind::CryBaby;
    for (int k = 0; k < nk; ++k)
    {
        if (isWah && k == 0) knob[k].setSliderStyle (juce::Slider::LinearVertical);
        else
        {
            knob[k].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            knob[k].setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        }
        knob[k].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob[k].setPopupDisplayEnabled (true, false, nullptr);
        addAndMakeVisible (knob[k]);
        knobAtt[k] = std::make_unique<SliderAtt> (p.apvts, PedalBoardProcessor::id (0, idx, "k" + juce::String (k)), knob[k]);
    }
    if (inf->sw)
    {
        sw.setButtonText (inf->sw);
        sw.setColour (juce::ToggleButton::textColourId, juce::Colour (sk->text));
        addAndMakeVisible (sw);
        swAtt = std::make_unique<ButtonAtt> (p.apvts, PedalBoardProcessor::id (0, idx, "s0"), sw);
    }
    addAndMakeVisible (foot);
    footAtt = std::make_unique<ButtonAtt> (p.apvts, PedalBoardProcessor::id (0, idx, "on"), foot);
    lastOn = foot.getToggleState();
    startTimerHz (15);
}

void PedalComponent::timerCallback()
{
    const bool on = foot.getToggleState();
    if (on != lastOn) { lastOn = on; repaint(); }
}

Point<float> PedalComponent::knobCentre (int k) const
{
    if (isFuzz) return k == 0 ? Point<float> (125.f, 48.f) : Point<float> (50.f, 48.f);   // Fuzz rechts, Volume links
    if (isWah) return Point<float> (32.f, 246.f);                                          // Volume
    if (nk == 1) return Point<float> (62.f, 42.f);
    if (nk == 2) return k == 0 ? Point<float> (36.f, 42.f) : Point<float> (88.f, 42.f);
    return k == 0 ? Point<float> (36.f, 42.f) : (k == 1 ? Point<float> (88.f, 42.f) : Point<float> (62.f, 96.f));
}

float PedalComponent::knobSize (int k) const { return isWah ? 30.f : ((nk == 3 && k == 2) ? 34.f : 38.f); }

Point<float> PedalComponent::footCentre() const
{
    if (isFuzz) return Point<float> (88.f, 128.f);
    if (isWah) return Point<float> (92.f, 244.f);
    return Point<float> (62.f, 190.f);
}

void PedalComponent::resized()
{
    const float s = (float) getWidth() / designW();
    auto place = [s] (juce::Component& c, Point<float> ctr, float d)
    { c.setBounds (Rectangle<float> ((ctr.x - d * 0.5f) * s, (ctr.y - d * 0.5f) * s, d * s, d * s).toNearestInt()); };
    for (int k = 0; k < nk; ++k)
    {
        if (isWah && k == 0) knob[k].setBounds (Rectangle<float> (12.f * s, 20.f * s, (designW() - 24.f) * s, 196.f * s).toNearestInt());
        else place (knob[k], knobCentre (k), knobSize (k));
    }
    place (foot, footCentre(), footSize());
    if (inf->sw)
    {
        if (isFuzz) sw.setBounds (Rectangle<float> (48.f * s, 92.f * s, 90.f * s, 16.f * s).toNearestInt());
        else sw.setBounds (Rectangle<float> (12.f * s, 158.f * s, (designW() - 24.f) * s, 14.f * s).toNearestInt());
    }
}

void PedalComponent::paint (juce::Graphics& g)
{
    const float W = designW(), H = designH(), s = (float) getWidth() / W;
    g.addTransform (juce::AffineTransform::scale (s));
    const juce::Colour top (sk->top), bot (sk->bottom), txt (sk->text);
    const juce::String title (sk->title), sub (sk->subtitle);

    if (isFuzz)
    {
        const Rectangle<float> disc (2.f, 2.f, W - 4.f, H - 4.f);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (disc.translated (0.f, 3.f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2f0), W * 0.38f, H * 0.30f, juce::Colour (0xff6f6f6d), W * 0.95f, H * 0.95f, true));
        g.fillEllipse (disc);
        unsigned int seed = 7u;                                                           // Metall-Körnung, deterministisch
        for (int i = 0; i < 220; ++i)
        {
            seed = seed * 1664525u + 1013904223u; const float a = (float) (seed >> 8 & 0xffff) / 65535.f * juce::MathConstants<float>::twoPi;
            seed = seed * 1664525u + 1013904223u; const float rr = std::sqrt ((float) (seed >> 8 & 0xffff) / 65535.f) * (W * 0.5f - 6.f);
            g.setColour (juce::Colours::black.withAlpha (0.10f)); g.fillEllipse (W * 0.5f + std::cos (a) * rr, H * 0.5f + std::sin (a) * rr, 2.f, 2.f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawEllipse (disc, 1.5f);
        juce::Path wedge; wedge.addTriangle (30.f, 112.f, W - 30.f, 112.f, W * 0.5f, H - 14.f);
        g.setColour (juce::Colour (0xff141414)); g.fillPath (wedge);
        g.setColour (txt); g.setFont (juce::FontOptions (13.f, juce::Font::bold));
        g.drawText (title, Rectangle<float> (0.f, 70.f, W, 18.f), juce::Justification::centred);
    }
    else if (isWah)
    {
        const Rectangle<float> body (1.f, 1.f, W - 2.f, H - 2.f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (body.translated (0.f, 3.f), 12.f);
        g.setGradientFill (juce::ColourGradient (top, 0.f, 0.f, bot, 0.f, H, false)); g.fillRoundedRectangle (body, 12.f);
        g.setColour (juce::Colour (0xff6a6a6a)); g.drawRoundedRectangle (body, 12.f, 1.5f);
        const Rectangle<float> tread (12.f, 20.f, W - 24.f, 196.f);
        g.setColour (juce::Colour (0xff111111)); g.fillRoundedRectangle (tread, 6.f);
        g.setColour (juce::Colours::white.withAlpha (0.13f));
        for (float y = tread.getY() + 6.f; y < tread.getBottom() - 4.f; y += 9.f) g.drawLine (tread.getX() + 6.f, y, tread.getRight() - 6.f, y, 2.f);
        g.setColour (txt); g.setFont (juce::FontOptions (11.f, juce::Font::bold | juce::Font::italic));
        g.drawText (title, Rectangle<float> (0.f, 3.f, W, 14.f), juce::Justification::centred);
    }
    else
    {
        const Rectangle<float> body (1.f, 1.f, W - 2.f, H - 2.f);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (body.translated (0.f, 3.f), 10.f);
        g.setGradientFill (juce::ColourGradient (top, 0.f, 0.f, bot, 0.f, H, false)); g.fillRoundedRectangle (body, 10.f);
        g.setColour (juce::Colours::white.withAlpha (0.25f)); g.drawRoundedRectangle (body.reduced (1.5f), 9.f, 1.2f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (body, 10.f, 1.f);
        g.setColour (juce::Colour (0xff3b3b3b));
        for (const auto pt : { Point<float> (9.f, 9.f), Point<float> (W - 9.f, 9.f), Point<float> (9.f, H - 9.f), Point<float> (W - 9.f, H - 9.f) }) g.fillEllipse (pt.x - 3.f, pt.y - 3.f, 6.f, 6.f);
        if (kind == pb::Kind::TS808)
        {
            const Rectangle<float> plate (10.f, 122.f, W - 20.f, 40.f);
            g.setColour (juce::Colour (0xffbfe3b8)); g.fillRoundedRectangle (plate, 4.f);
            g.setColour (txt); g.setFont (juce::FontOptions (12.f, juce::Font::bold));
            g.drawText (title, Rectangle<float> (plate.getX(), plate.getY() + 4.f, plate.getWidth(), 18.f), juce::Justification::centred);
            g.setFont (juce::FontOptions (9.f, juce::Font::bold));
            g.drawText (sub, Rectangle<float> (plate.getX(), plate.getY() + 22.f, plate.getWidth(), 12.f), juce::Justification::centred);
        }
        else
        {
            const float fs = title.length() > 11 ? 12.f : (title.length() > 8 ? 15.f : 19.f);
            g.setColour (txt); g.setFont (juce::FontOptions (fs, juce::Font::bold));
            g.drawText (title, Rectangle<float> (4.f, 126.f, W - 8.f, 24.f), juce::Justification::centred);
            if (sub.isNotEmpty())
            {
                g.setFont (juce::FontOptions (9.5f, juce::Font::bold)); g.setColour (txt.withAlpha (0.8f));
                g.drawText (sub, Rectangle<float> (4.f, 148.f, W - 8.f, 12.f), juce::Justification::centred);
            }
        }
    }

    g.setColour (txt.withAlpha (0.85f)); g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
    for (int k = 0; k < nk; ++k)
    {
        if (isWah && k == 0) continue;
        const auto c = knobCentre (k);
        g.drawText (juce::String (inf->knob[k]).toUpperCase(), Rectangle<float> (c.x - 34.f, c.y + knobSize (k) * 0.5f + 1.f, 68.f, 11.f), juce::Justification::centred);
    }
    const bool on = foot.getToggleState();
    const Point<float> led = isFuzz ? Point<float> (88.f, 30.f) : (isWah ? Point<float> (62.f, 230.f) : Point<float> (16.f, 16.f));
    if (on) { g.setColour (juce::Colour (0xffff3030).withAlpha (0.25f)); g.fillEllipse (led.x - 9.f, led.y - 9.f, 18.f, 18.f); }
    g.setColour (on ? juce::Colour (0xffff4a3a) : juce::Colour (0xff3a0e0e)); g.fillEllipse (led.x - 4.f, led.y - 4.f, 8.f, 8.f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (led.x - 4.f, led.y - 4.f, 8.f, 8.f, 1.f);
}

void PedalComponent::mouseDown (const juce::MouseEvent& e) { if (! movable()) return; dragging = true; toFront (false); dragger.startDraggingComponent (this, e); }
void PedalComponent::mouseDrag (const juce::MouseEvent& e) { if (dragging) dragger.dragComponent (this, e, nullptr); }
void PedalComponent::mouseUp (const juce::MouseEvent&) { if (dragging) { dragging = false; if (onDrop) onDrop (this); } }

// ============================================================================ Tape-Einstellungen (Popup)
TapeSettings::TapeSettings (PedalBoardProcessor& p)
{
    machine.addItemList ({ "TEAC A-3340S", "Revox B77" }, 1);
    speed.addItemList ({ "15 ips", "7.5 ips", "3.75 ips (nur Revox)" }, 1);
    type.addItemList ({ "Band: Standard (150)", "Band: Low Noise (203)" }, 1);
    biasSw.addItemList ({ "Bias: NORMAL", "Bias: HIGH" }, 1);
    eqSw.addItemList ({ "EQ: NORMAL", "EQ: HIGH" }, 1);
    for (auto* c : { &machine, &speed, &type, &biasSw, &eqSw }) addAndMakeVisible (c);
    machineAtt = std::make_unique<ComboAtt> (p.apvts, "tape_machine", machine);
    speedAtt = std::make_unique<ComboAtt> (p.apvts, "tape_speed", speed);
    typeAtt = std::make_unique<ComboAtt> (p.apvts, "tape_type", type);
    biasSwAtt = std::make_unique<ComboAtt> (p.apvts, "tape_biassw", biasSw);
    eqSwAtt = std::make_unique<ComboAtt> (p.apvts, "tape_eqsw", eqSw);
    // Band-Typ, Bias- und EQ-Schalter gibt es nur beim TEAC
    auto update = [this] { const bool teac = machine.getSelectedItemIndex() <= 0; type.setEnabled (teac); biasSw.setEnabled (teac); eqSw.setEnabled (teac); };
    machine.onChange = update; update();
    for (auto* b : { &noise, &hys }) { b->setColour (juce::ToggleButton::textColourId, juce::Colours::white); addAndMakeVisible (b); }
    noiseAtt = std::make_unique<ButtonAtt> (p.apvts, "tape_noise", noise);
    hysAtt = std::make_unique<ButtonAtt> (p.apvts, "tape_hys", hys);
    const char* ids[4] = { "tape_wf", "tape_bump", "tape_xtalk", "tape_link" };
    const char* names[4] = { "Wow/Flutter", "Head Bump", "Crosstalk", "Gain Link" };
    for (int i = 0; i < 4; ++i)
    {
        knob[i].setSliderStyle (juce::Slider::LinearHorizontal); knob[i].setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
        addAndMakeVisible (knob[i]);
        label[i].setText (names[i], juce::dontSendNotification); label[i].setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (label[i]);
        knobAtt[i] = std::make_unique<SliderAtt> (p.apvts, ids[i], knob[i]);
    }
    setSize (360, 350);
}

void TapeSettings::resized()
{
    auto r = getLocalBounds().reduced (12);
    for (auto* c : { &machine, &speed, &type, &biasSw, &eqSw }) c->setBounds (r.removeFromTop (28).reduced (0, 1));
    r.removeFromTop (8);
    for (int i = 0; i < 4; ++i) { auto row = r.removeFromTop (30); label[i].setBounds (row.removeFromLeft (92)); knob[i].setBounds (row); }
    r.removeFromTop (6);
    auto row = r.removeFromTop (26); noise.setBounds (row.removeFromLeft (150)); hys.setBounds (row);
}

// ============================================================================ Tape-Geraet
TapeDeck::TapeDeck (PedalBoardProcessor& p) : proc (p)
{
    for (auto* k : { &drive, &bias })
    {
        k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0); k->setPopupDisplayEnabled (true, false, nullptr);
        addAndMakeVisible (k);
    }
    driveAtt = std::make_unique<SliderAtt> (p.apvts, "tape_drive", drive);
    biasAtt = std::make_unique<SliderAtt> (p.apvts, "tape_bias", bias);
    addAndMakeVisible (foot); footAtt = std::make_unique<ButtonAtt> (p.apvts, "tape_on", foot);
    settings.onClick = [this] { juce::CallOutBox::launchAsynchronously (std::make_unique<TapeSettings> (proc), settings.getScreenBounds(), nullptr); };
    addAndMakeVisible (settings);
    startTimerHz (30);
}

void TapeDeck::resized()
{
    const float s = (float) getWidth() / 495.f;
    auto place = [s] (juce::Component& c, Point<float> ctr, float d) { c.setBounds (Rectangle<float> ((ctr.x - d * 0.5f) * s, (ctr.y - d * 0.5f) * s, d * s, d * s).toNearestInt()); };
    place (drive, Point<float> (80.f, 205.f), 64.f); place (bias, Point<float> (170.f, 205.f), 64.f); place (foot, Point<float> (440.f, 222.f), 44.f);
    settings.setBounds (Rectangle<float> (330.f * s, 232.f * s, 70.f * s, 24.f * s).toNearestInt());
}

void TapeDeck::timerCallback()
{
    const float db = 20.f * std::log10 (std::max (proc.getOutLevel(), 1e-5f));
    const float target = std::clamp ((db + 38.f) / 23.f, 0.f, 1.f);              // 0 VU = -18 dBFS
    needle += (target - needle) * (target > needle ? 0.25f : 0.06f);
    if (foot.getToggleState()) reel += 0.12f;
    repaint();
}

void TapeDeck::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / 495.f));
    const Rectangle<float> body (1.f, 1.f, 493.f, 273.f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e2e2e), 0.f, 0.f, juce::Colour (0xff0b0b0b), 0.f, 275.f, false)); g.fillRoundedRectangle (body, 8.f);
    g.setColour (juce::Colour (0xff8a8a8a)); g.drawRoundedRectangle (body.reduced (4.f), 6.f, 1.5f);
    g.setColour (juce::Colour (0xff141414)); g.fillRoundedRectangle (Rectangle<float> (8.f, 150.f, 479.f, 117.f), 6.f);
    // Fenster mit Spulen
    const Rectangle<float> win (24.f, 30.f, 262.f, 112.f);
    g.setColour (juce::Colour (0xff070707)); g.fillRoundedRectangle (win, 4.f);
    g.setColour (juce::Colour (0xff9a9a9a)); g.drawRoundedRectangle (win, 4.f, 1.5f);
    const Point<float> rc[2] = { Point<float> (95.f, 86.f), Point<float> (215.f, 86.f) };
    g.setColour (juce::Colour (0xff3a2a20)); g.drawLine (rc[0].x, rc[0].y + 41.f, rc[1].x, rc[1].y + 41.f, 2.f);
    for (int i = 0; i < 2; ++i)
    {
        const auto c = rc[i];
        g.setColour (juce::Colour (0xff2a1f18)); g.fillEllipse (c.x - 40.f, c.y - 40.f, 80.f, 80.f);
        g.setColour (juce::Colour (0xff8b8b8b)); g.drawEllipse (c.x - 40.f, c.y - 40.f, 80.f, 80.f, 2.f);
        g.setColour (juce::Colour (0xff3d3d3d)); g.fillEllipse (c.x - 16.f, c.y - 16.f, 32.f, 32.f);
        for (int k = 0; k < 3; ++k)
        {
            const float a = reel * (i == 0 ? 1.f : 1.1f) + (float) k * juce::MathConstants<float>::twoPi / 3.f;
            g.setColour (juce::Colour (0xffb5b5b5)); g.drawLine (c.x, c.y, c.x + std::cos (a) * 32.f, c.y + std::sin (a) * 32.f, 5.f);
        }
        g.setColour (juce::Colour (0xffd0d0d0)); g.fillEllipse (c.x - 7.f, c.y - 7.f, 14.f, 14.f);
    }
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), win.getX(), win.getY(), juce::Colours::transparentWhite, win.getX(), win.getBottom(), false)); g.fillRoundedRectangle (win, 4.f);
    // VU-Meter
    const Rectangle<float> face (306.f, 34.f, 170.f, 108.f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4ecd0), face.getX(), face.getY(), juce::Colour (0xffd8cba2), face.getX(), face.getBottom(), false)); g.fillRoundedRectangle (face, 4.f);
    g.setColour (juce::Colour (0xff222222)); g.drawRoundedRectangle (face, 4.f, 2.f);
    const Point<float> piv (face.getCentreX(), face.getBottom() + 28.f);
    g.saveState(); g.reduceClipRegion (face.toNearestInt());
    for (int t = 0; t <= 10; ++t)
    {
        const float a = ((float) t / 10.f - 0.5f) * 100.f * juce::MathConstants<float>::pi / 180.f;
        g.setColour (t >= 8 ? juce::Colour (0xffb02a20) : juce::Colour (0xff202020));
        g.drawLine (piv.x + std::sin (a) * 104.f, piv.y - std::cos (a) * 104.f, piv.x + std::sin (a) * (t % 5 == 0 ? 92.f : 97.f), piv.y - std::cos (a) * (t % 5 == 0 ? 92.f : 97.f), 1.3f);
    }
    g.setColour (juce::Colour (0xff202020)); g.setFont (juce::FontOptions (11.f, juce::Font::bold));
    g.drawText ("VU", Rectangle<float> (face.getX(), face.getY() + 46.f, face.getWidth(), 14.f), juce::Justification::centred);
    const float na = (needle - 0.5f) * 100.f * juce::MathConstants<float>::pi / 180.f;
    g.setColour (juce::Colour (0xff111111)); g.drawLine (piv.x, piv.y, piv.x + std::sin (na) * 96.f, piv.y - std::cos (na) * 96.f, 1.6f);
    g.restoreState();
    // Beschriftung
    g.setColour (juce::Colour (0xffe8e0cc)); g.setFont (juce::FontOptions (21.f, juce::Font::bold));
    g.drawText ("TAPE", Rectangle<float> (300.f, 168.f, 130.f, 26.f), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff9a9a9a)); g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("SATURATION", Rectangle<float> (300.f, 194.f, 130.f, 14.f), juce::Justification::centredLeft);
    g.drawText ("DRIVE", Rectangle<float> (48.f, 240.f, 64.f, 12.f), juce::Justification::centred);
    g.drawText ("BIAS", Rectangle<float> (138.f, 240.f, 64.f, 12.f), juce::Justification::centred);
    const bool on = foot.getToggleState(); const Point<float> led (440.f, 176.f);
    if (on) { g.setColour (juce::Colour (0xffff3030).withAlpha (0.25f)); g.fillEllipse (led.x - 10.f, led.y - 10.f, 20.f, 20.f); }
    g.setColour (on ? juce::Colour (0xffff4a3a) : juce::Colour (0xff3a0e0e)); g.fillEllipse (led.x - 5.f, led.y - 5.f, 10.f, 10.f);
    // Klebeband-Etikett
    g.saveState(); g.addTransform (juce::AffineTransform::rotation (-0.03f, 270.f, 19.f));
    g.setColour (juce::Colour (0xffd9c9a3)); g.fillRect (Rectangle<float> (150.f, 2.f, 240.f, 34.f));
    g.setColour (juce::Colour (0xff2b2b2b)); g.setFont (juce::FontOptions (22.f, juce::Font::bold | juce::Font::italic));
    g.drawText ("herewithme", Rectangle<float> (150.f, 2.f, 240.f, 34.f), juce::Justification::centred);
    g.restoreState();
}

// ============================================================================ Editor
PedalBoardEditor::PedalBoardEditor (PedalBoardProcessor& p) : AudioProcessorEditor (&p), proc (p), deck (p)
{
    setLookAndFeel (&lnf);
    nSlots = std::min (p.slotCount (0), 15);
    for (int i = 0; i < nSlots; ++i)
    {
        auto* c = new PedalComponent (p, i); pedals.add (c); addAndMakeVisible (c);
        c->onDrop = [this] (PedalComponent* pc) { dropPedal (pc); };
    }
    for (int pos = 0; pos < nSlots; ++pos) slotPedal[(size_t) pos] = p.getOrder (0, pos);
    bool ok = true;                                    // feste Pedale (Fuzz Face, Wah) muessen auf ihrem Platz stehen
    for (int pos = 0; pos < nSlots; ++pos) if (slotPedal[(size_t) pos] < 0 || slotPedal[(size_t) pos] >= nSlots || (! pedals[slotPedal[(size_t) pos]]->movable() && slotPedal[(size_t) pos] != pos)) ok = false;
    if (! ok) { for (int pos = 0; pos < nSlots; ++pos) slotPedal[(size_t) pos] = pos; proc.setOrder (0, std::vector<int> (slotPedal.begin(), slotPedal.begin() + nSlots)); }

    addAndMakeVisible (deck);
    master.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    master.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    master.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0); master.setPopupDisplayEnabled (true, false, nullptr);
    addAndMakeVisible (master); masterAtt = std::make_unique<SliderAtt> (p.apvts, "master", master);
    inMode.addItemList ({ "Eingang: Links", "Eingang: Rechts", "Eingang: Summe" }, 1); addAndMakeVisible (inMode);
    inAtt = std::make_unique<ComboAtt> (p.apvts, "inmode", inMode);
    setResizable (true, true); setResizeLimits (768, 400, 2304, 1200);
    if (auto* cs = getConstrainer()) cs->setFixedAspectRatio ((double) board::kDesignW / (double) board::kDesignH);
    setSize (1152, 600);
}

PedalBoardEditor::~PedalBoardEditor() { setLookAndFeel (nullptr); }

void PedalBoardEditor::layoutPedals()
{
    const float s = (float) getWidth() / board::kDesignW;
    for (int pos = 0; pos < nSlots; ++pos)
    {
        auto* c = pedals[slotPedal[(size_t) pos]];
        const float w = c->designW(), h = c->designH();
        c->setBounds (Rectangle<float> ((kSlotX[pos] - w * 0.5f) * s, (kSlotY[pos] - h * 0.5f) * s, w * s, h * s).toNearestInt());
    }
}

void PedalBoardEditor::resized()
{
    const float s = (float) getWidth() / board::kDesignW;
    rug = board::makeRug (getWidth(), getHeight());
    layoutPedals();
    deck.setBounds (Rectangle<float> (1015.f * s, 460.f * s, 495.f * s, 275.f * s).toNearestInt());
    master.setBounds (Rectangle<float> (42.f * s, 46.f * s, 56.f * s, 56.f * s).toNearestInt());
    inMode.setBounds (Rectangle<float> (130.f * s, 62.f * s, 200.f * s, 28.f * s).toNearestInt());
}

void PedalBoardEditor::dropPedal (PedalComponent* pc)
{
    const float s = (float) getWidth() / board::kDesignW;
    const Point<float> cen = pc->getBounds().getCentre().toFloat() / s;
    int from = -1; for (int pos = 0; pos < nSlots; ++pos) if (slotPedal[(size_t) pos] == pc->pedal) from = pos;
    int best = -1; float bd = 120.f * 120.f;
    for (int pos = 0; pos < nSlots; ++pos)
    {
        if (! pedals[slotPedal[(size_t) pos]]->movable()) continue;
        const float dx = cen.x - kSlotX[pos], dy = cen.y - kSlotY[pos], d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = pos; }
    }
    if (from >= 0 && best >= 0 && best != from)
    {
        std::swap (slotPedal[(size_t) from], slotPedal[(size_t) best]);
        proc.setOrder (0, std::vector<int> (slotPedal.begin(), slotPedal.begin() + nSlots));
    }
    layoutPedals(); repaint();
}

void PedalBoardEditor::paint (juce::Graphics& g)
{
    g.drawImage (rug, getLocalBounds().toFloat());
    const float s = (float) getWidth() / board::kDesignW;
    g.addTransform (juce::AffineTransform::scale (s));
    g.setColour (juce::Colour (0xffe9dcc0)); g.setFont (juce::FontOptions (13.f, juce::Font::bold));
    g.drawText ("MASTER", Rectangle<float> (30.f, 106.f, 80.f, 14.f), juce::Justification::centred);
    g.setFont (juce::FontOptions (36.f, juce::Font::bold)); g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawText ("PEDALBOARD", Rectangle<float> (402.f, 62.f, 600.f, 50.f), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffe9dcc0)); g.drawText ("PEDALBOARD", Rectangle<float> (400.f, 60.f, 600.f, 50.f), juce::Justification::centredLeft);

    // Patchkabel zwischen benachbarten Plaetzen (Lesereihenfolge = Signalweg)
    static const juce::uint32 palette[6] = { 0xffe3c13a, 0xff3aa84e, 0xffd6402f, 0xff3a63d1, 0xff7a4bc2, 0xffe67f22 };
    auto halfW = [] (int pos) { return pos == 0 ? 88.f : 62.f; };
    auto jackY = [] (int pos) { return pos == 0 ? -40.f : (pos == 4 ? -80.f : -62.f); };
    auto cable = [&g] (Point<float> p0, Point<float> c1, Point<float> c2, Point<float> p1, juce::Colour col)
    {
        juce::Path path; path.startNewSubPath (p0); path.cubicTo (c1, c2, p1);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.strokePath (path, juce::PathStrokeType (7.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col); g.strokePath (path, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffb0b0b0)); g.fillRoundedRectangle (p0.x - 6.f, p0.y - 5.f, 12.f, 10.f, 2.f); g.fillRoundedRectangle (p1.x - 6.f, p1.y - 5.f, 12.f, 10.f, 2.f);
    };
    for (int pos = 0; pos + 1 < nSlots; ++pos)
    {
        const Point<float> p0 (kSlotX[pos] + halfW (pos) - 2.f, kSlotY[pos] + jackY (pos)), p1 (kSlotX[pos + 1] - halfW (pos + 1) + 2.f, kSlotY[pos + 1] + jackY (pos + 1));
        const auto col = juce::Colour (palette[pos % 6]);
        if (pos == 8) cable (p0, p0 + Point<float> (60.f, 150.f), p1 + Point<float> (-60.f, -150.f), p1, col);
        else cable (p0, p0 + Point<float> (18.f, -34.f), p1 + Point<float> (-18.f, -34.f), p1, col);
    }
    if (nSlots > 0)
    {
        const int last = nSlots - 1;
        cable (Point<float> (kSlotX[last] + halfW (last) - 2.f, kSlotY[last] + jackY (last)), Point<float> (kSlotX[last] + 90.f, kSlotY[last] - 90.f), Point<float> (990.f, 440.f), Point<float> (1019.f, 520.f), juce::Colour (palette[last % 6]));
    }
}
