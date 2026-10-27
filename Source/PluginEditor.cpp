#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
juce::Font bold (float h) { return juce::Font (juce::FontOptions (h, juce::Font::bold)); }
juce::Font plain (float h) { return juce::Font (juce::FontOptions (h)); }

void silverPlug (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f0f2), r.getX(), r.getY(), juce::Colour (0xff7a7a80), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 2.f);
    g.setColour (juce::Colour (0xff2b2b2e));
    g.drawRoundedRectangle (r, 2.f, 0.8f);
}
} // namespace

Skin skinFor (pb::Kind k)
{
    using K = pb::Kind;
    const juce::Colour W = juce::Colours::white, B = juce::Colour (0xff111112);
    switch (k)
    {
        case K::FuzzFace:   return { juce::Colour (0xffa5221f), W };
        case K::TS808:      return { juce::Colour (0xff2fa44f), W };
        case K::DynaComp:   return { juce::Colour (0xffd13b2f), W };
        case K::CryBaby:    return { juce::Colour (0xff1e1e20), juce::Colour (0xffd8d8dc) };
        case K::Phase90:    return { juce::Colour (0xfff08a1c), B };
        case K::CE2:        return { juce::Colour (0xff7fb7e6), B };
        case K::UniVibe:    return { juce::Colour (0xff2d5f5a), W };
        case K::VB2:        return { juce::Colour (0xff6ec2c0), B };
        case K::MicroAmp:   return { juce::Colour (0xfff2c230), B };
        case K::DM2:        return { juce::Colour (0xff6d3c9a), W };
        case K::Spring6G15: return { juce::Colour (0xffd8c08a), B };
        case K::MidiVerb:   return { juce::Colour (0xff25262a), juce::Colour (0xffe8e8e8) };
        case K::H910:       return { juce::Colour (0xff1b2b4a), W };
        case K::VP330:      return { juce::Colour (0xff7a4a2e), W };
        case K::VE20:       return { juce::Colour (0xffd0462f), W };
        default:            return { juce::Colour (0xff444444), W };
    }
}

// ------------------------------------------------------------------ Look and Feel
void PedalLaf::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const float r = (float) std::min (w, h) * 0.5f - 7.f;
    const auto tick = s.findColour (juce::Slider::thumbColourId);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / 10.f;
        g.setColour (tick.withAlpha (0.85f));
        g.fillEllipse (cx + (r + 5.f) * std::sin (a) - 1.3f, cy - (r + 5.f) * std::cos (a) - 1.3f, 2.6f, 2.6f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (cx - r + 1.f, cy - r + 3.f, 2.f * r, 2.f * r);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff48484b), cx - r * 0.5f, cy - r * 0.6f, juce::Colour (0xff0c0c0d), cx + r * 0.6f, cy + r * 0.8f, true));
    g.fillEllipse (cx - r, cy - r, 2.f * r, 2.f * r);
    g.setColour (juce::Colour (0xff77777c));
    g.drawEllipse (cx - r, cy - r, 2.f * r, 2.f * r, 1.4f);
    const float ri = r * 0.68f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e2e30), cx - ri * 0.4f, cy - ri * 0.5f, juce::Colour (0xff111113), cx + ri * 0.5f, cy + ri * 0.6f, false));
    g.fillEllipse (cx - ri, cy - ri, 2.f * ri, 2.f * ri);
    const float a = a0 + pos * (a1 - a0);
    g.setColour (juce::Colours::white);
    g.drawLine (cx + r * 0.22f * std::sin (a), cy - r * 0.22f * std::cos (a), cx + r * 0.86f * std::sin (a), cy - r * 0.86f * std::cos (a), 2.6f);
}

void PedalLaf::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto track = r.removeFromLeft (34.f).withSizeKeepingCentre (32.f, 14.f);
    const bool on = b.getToggleState();
    g.setColour (juce::Colour (0xff101012)); g.fillRoundedRectangle (track, 7.f);
    if (on) { g.setColour (juce::Colour (0xffff9a2e)); g.fillRoundedRectangle (track.reduced (1.8f), 5.5f); }
    g.setColour (juce::Colour (0xff5a5a5e)); g.drawRoundedRectangle (track, 7.f, 1.f);
    const float kx = on ? track.getRight() - 13.f : track.getX() + 1.f;
    silverPlug (g, { kx, track.getY() + 0.5f, 12.f, 13.f });
    g.setColour (b.findColour (juce::ToggleButton::textColourId));
    g.setFont (bold (11.f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (4.f), juce::Justification::centredLeft, true);
}

void PedalLaf::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.f);
    const float R = std::min (r.getWidth(), r.getHeight()) * 0.5f;
    const auto c = r.getCentre();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - R, c.y - R + 2.f, 2.f * R, 2.f * R);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeaeaec), c.x - R * 0.6f, c.y - R * 0.8f, juce::Colour (0xff66666a), c.x + R * 0.6f, c.y + R * 0.8f, false));
    g.fillEllipse (c.x - R, c.y - R, 2.f * R, 2.f * R);
    const float R2 = R * 0.76f, off = (down || b.getToggleState()) ? 1.5f : 0.f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8c8c92), c.x - R2 * 0.5f, c.y - R2 * 0.6f + off, juce::Colour (0xffd9d9dd), c.x + R2 * 0.6f, c.y + R2 * 0.8f + off, false));
    g.fillEllipse (c.x - R2, c.y - R2 + off, 2.f * R2, 2.f * R2);
    g.setColour (juce::Colours::white.withAlpha (hi ? 0.5f : 0.3f));
    g.fillEllipse (c.x - R2 * 0.5f, c.y - R2 * 0.75f + off, R2 * 0.8f, R2 * 0.4f);
}

// ------------------------------------------------------------------ Pedal-Kachel
PedalTile::PedalTile (PedalBoardProcessor& p, int chainIdx, int slotIdx) : slot (slotIdx)
{
    const auto kind = p.slotKind (chainIdx, slotIdx);
    const auto& inf = pb::info (kind);
    skin = skinFor (kind);
    title = inf.name; nk = inf.nk;
    for (int k = 0; k < nk; ++k)
    {
        knob[k].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob[k].setRotaryParameters (1.25f * juce::MathConstants<float>::pi, 2.75f * juce::MathConstants<float>::pi, true);
        knob[k].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob[k].setColour (juce::Slider::thumbColourId, skin.text);
        addAndMakeVisible (knob[k]);
        knobLabel[k].setText (juce::String (inf.knob[k]).toUpperCase(), juce::dontSendNotification);
        knobLabel[k].setJustificationType (juce::Justification::centred);
        knobLabel[k].setFont (bold (10.f));
        knobLabel[k].setColour (juce::Label::textColourId, skin.text);
        knobLabel[k].setInterceptsMouseClicks (false, false);
        addAndMakeVisible (knobLabel[k]);
        knobAtt[k] = std::make_unique<SliderAtt> (p.apvts, PedalBoardProcessor::id (chainIdx, slotIdx, "k" + juce::String (k)), knob[k]);
    }
    if (inf.sw)
    {
        sw.setButtonText (juce::String (inf.sw).toUpperCase());
        sw.setColour (juce::ToggleButton::textColourId, skin.text);
        addAndMakeVisible (sw);
        swAtt = std::make_unique<ButtonAtt> (p.apvts, PedalBoardProcessor::id (chainIdx, slotIdx, "s0"), sw);
    }
    foot.setClickingTogglesState (true);
    foot.onStateChange = [this] { repaint(); };
    addAndMakeVisible (foot);
    footAtt = std::make_unique<ButtonAtt> (p.apvts, PedalBoardProcessor::id (chainIdx, slotIdx, "on"), foot);
}

void PedalTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (r.translated (2.f, 5.f), 9.f);
    g.setGradientFill (juce::ColourGradient (skin.body.brighter (0.2f), r.getX(), r.getY(), skin.body.darker (0.3f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 9.f);
    g.setColour (skin.body.darker (0.8f));
    g.drawRoundedRectangle (r, 9.f, 1.3f);
    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawLine (r.getX() + 9.f, r.getY() + 1.8f, r.getRight() - 9.f, r.getY() + 1.8f, 1.f);

    g.setColour (skin.text);
    g.setFont (bold (14.f));
    g.drawFittedText (title, juce::Rectangle<int> (8, 5, getWidth() - 16, kHeader - 6), juce::Justification::centred, 1, 0.7f);
    g.setColour (skin.text.withAlpha (0.28f));
    g.drawLine (12.f, (float) kHeader + 1.f, (float) getWidth() - 12.f, (float) kHeader + 1.f, 1.f);

    // Klinkenbuchsen links und rechts
    for (float jx : { r.getX() - 3.f, r.getRight() - 4.f })
        silverPlug (g, { jx, (float) kJackY - 8.f, 7.f, 16.f });

    // LED ueber dem Fussschalter
    const bool on = foot.getToggleState();
    const float lx = (float) getWidth() * 0.5f, ly = (float) foot.getY() - 10.f;
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff3030).withAlpha (0.55f), lx, ly, juce::Colours::transparentBlack, lx + 14.f, ly, true));
        g.fillEllipse (lx - 14.f, ly - 14.f, 28.f, 28.f);
    }
    g.setColour (on ? juce::Colour (0xffff5a4a) : juce::Colour (0xff4a1412));
    g.fillEllipse (lx - 4.f, ly - 4.f, 8.f, 8.f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (lx - 4.f, ly - 4.f, 8.f, 8.f, 1.f);
}

void PedalTile::resized()
{
    foot.setBounds ((getWidth() - 46) / 2, kHeight - 12 - 46, 46, 46);
    if (sw.isVisible()) sw.setBounds (14, 142, getWidth() - 20, 20);

    auto area = juce::Rectangle<int> (8, kHeader + 5, getWidth() - 16, 104);
    std::vector<juce::Rectangle<int>> cells;
    if (nk == 1) cells = { area };
    else if (nk == 2) { auto a = area; cells = { a.removeFromLeft (a.getWidth() / 2), a }; }
    else if (nk == 3)
    {
        auto top = area.removeFromTop (52);
        cells = { top.removeFromLeft (top.getWidth() / 2), top, area.withSizeKeepingCentre (area.getWidth() / 2, area.getHeight()) };
    }
    for (int k = 0; k < nk && k < (int) cells.size(); ++k)
    {
        auto c = cells[(size_t) k];
        knobLabel[k].setBounds (c.removeFromBottom (13));
        const int d = std::min (c.getWidth(), c.getHeight()) + (nk == 1 ? 0 : 2);
        knob[k].setBounds (c.withSizeKeepingCentre (d, d));
    }
}

void PedalTile::mouseDown (const juce::MouseEvent& e) { dragging = e.y < kHeader; if (dragging && onDown) onDown (this, e); }
void PedalTile::mouseDrag (const juce::MouseEvent& e) { if (dragging && onDrag) onDrag (this, e); }
void PedalTile::mouseUp (const juce::MouseEvent& e) { if (dragging && onUp) onUp (this, e); dragging = false; }

// ------------------------------------------------------------------ Kette auf dem Board
ChainRow::ChainRow (PedalBoardProcessor& p, int chainIdx) : proc (p), chain (chainIdx), rtl (chainIdx == 0)
{
    for (int pos = 0; pos < proc.slotCount (chain); ++pos)
    {
        auto* t = new PedalTile (proc, chain, proc.getOrder (chain, pos));
        tiles.add (t);
        addAndMakeVisible (t);
        t->onDown = [] (PedalTile* tile, const juce::MouseEvent&) { tile->toFront (false); };
        t->onDrag = [this] (PedalTile* tile, const juce::MouseEvent& e)
        {
            const auto pe = e.getEventRelativeTo (this);
            tile->setTopLeftPosition (juce::jlimit (0, std::max (0, getWidth() - PedalTile::kWidth), pe.x - PedalTile::kWidth / 2), tile->getY());
            repaint();
        };
        t->onUp = [this] (PedalTile* tile, const juce::MouseEvent&)
        {
            const int to = indexForX (tile->getX()), from = tiles.indexOf (tile);
            tiles.move (from, to);
            std::vector<int> order;
            for (auto* t2 : tiles) order.push_back (t2->slot);
            proc.setOrder (chain, order);
            layoutTiles();
            repaint();
        };
    }
}

int ChainRow::xForIndex (int i) const { return rtl ? getWidth() - kPadX - PedalTile::kWidth - i * PedalTile::kPitch : kPadX + i * PedalTile::kPitch; }

int ChainRow::indexForX (int left) const
{
    const float f = rtl ? (float) (getWidth() - kPadX - PedalTile::kWidth - left) / (float) PedalTile::kPitch
                        : (float) (left - kPadX) / (float) PedalTile::kPitch;
    return juce::jlimit (0, std::max (0, tiles.size() - 1), (int) std::lround (f));
}

void ChainRow::layoutTiles()
{
    for (int i = 0; i < tiles.size(); ++i) tiles[i]->setBounds (xForIndex (i), kTop, PedalTile::kWidth, PedalTile::kHeight);
}
void ChainRow::resized() { layoutTiles(); }

void ChainRow::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0f0f10)); g.fillRoundedRectangle (b, 10.f);
    g.setColour (juce::Colour (0xff2c2c2f)); g.drawRoundedRectangle (b.reduced (0.6f), 10.f, 1.4f);
    for (float ry : { (float) kTop + 36.f, (float) kTop + 150.f })      // Klettschienen mit Holzkante
    {
        g.setColour (juce::Colour (0xff1d1d1f)); g.fillRect (14.f, ry, b.getWidth() - 28.f, 24.f);
        g.setColour (juce::Colour (0xffb97d44)); g.fillRect (14.f, ry + 24.f, b.getWidth() - 28.f, 3.f);
    }
    if (tiles.isEmpty()) return;

    const float yj = (float) kTop + (float) PedalTile::kJackY, yB = (float) (kTop + PedalTile::kHeight);
    const float d = rtl ? -1.f : 1.f;
    auto outPt = [&] (PedalTile* t) { return juce::Point<float> (rtl ? (float) t->getX() + 2.f : (float) t->getRight() - 2.f, yj); };
    auto inPt  = [&] (PedalTile* t) { return juce::Point<float> (rtl ? (float) t->getRight() - 2.f : (float) t->getX() + 2.f, yj); };
    auto plug = [&] (juce::Point<float> p, float dir) { silverPlug (g, { dir > 0 ? p.x : p.x - 10.f, p.y - 7.f, 10.f, 14.f }); };
    auto drawCable = [&] (const juce::Path& path, juce::Colour c)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.strokePath (path, juce::PathStrokeType (7.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (1.f, 2.f));
        g.setColour (c);
        g.strokePath (path, juce::PathStrokeType (5.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.strokePath (path, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (-1.2f, -1.2f));
    };
    const juce::Colour black (0xff1b1b1d), red (0xffc0242a);

    for (int i = 0; i + 1 < tiles.size(); ++i)                          // Patchkabel zwischen den Pedalen
    {
        const auto po = outPt (tiles[i]), pi = inPt (tiles[i + 1]);
        const float gx = 0.5f * (po.x + pi.x);
        juce::Path path;
        path.startNewSubPath (po.x + d * 9.f, yj);
        path.cubicTo (po.x + d * 17.f, yj, po.x + d * 17.f, yB + 12.f, gx, yB + 14.f);
        path.cubicTo (pi.x - d * 17.f, yB + 12.f, pi.x - d * 17.f, yj, pi.x - d * 9.f, yj);
        drawCable (path, black);
        plug (po, d); plug (pi, -d);
    }

    {   // Eingangskabel (rot, wie auf dem Foto) vom Rand zum ersten Pedal
        const auto pi = inPt (tiles.getFirst());
        const juce::Point<float> s (rtl ? b.getWidth() - 2.f : 2.f, yj + 30.f), e (pi.x - d * 9.f, yj);
        juce::Path path; path.startNewSubPath (s);
        path.cubicTo (s.x + d * 24.f, s.y, e.x - d * 24.f, e.y + 4.f, e.x, e.y);
        drawCable (path, red); plug (pi, -d);
    }
    {   // Ausgangskabel zum Verstaerker
        const auto po = outPt (tiles.getLast());
        const juce::Point<float> s (po.x + d * 9.f, yj), e (rtl ? 2.f : b.getWidth() - 2.f, yj + 30.f);
        juce::Path path; path.startNewSubPath (s);
        path.cubicTo (s.x + d * 24.f, s.y - 2.f, e.x - d * 24.f, e.y, e.x, e.y);
        drawCable (path, black); plug (po, d);
    }
}

// ------------------------------------------------------------------ Gesamtansicht
BoardView::BoardView (PedalBoardProcessor& p) : rowA (p, 0), rowR (p, 1)
{
    setLookAndFeel (&laf);
    carpet = juce::ImageCache::getFromMemory (BinaryData::carpet_jpg, BinaryData::carpet_jpgSize);
    addAndMakeVisible (rowA); addAndMakeVisible (rowR);

    master.setSliderStyle (juce::Slider::LinearHorizontal);
    master.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 22);
    master.setColour (juce::Slider::trackColourId, juce::Colour (0xfff08a1c));
    master.setColour (juce::Slider::thumbColourId, juce::Colour (0xffe8dcc0));
    master.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe8dcc0));
    master.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (master);
    masterAtt = std::make_unique<SliderAtt> (p.apvts, "master", master);
    masterLabel.setText ("MASTER", juce::dontSendNotification);
    masterLabel.setFont (bold (12.f));
    masterLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe8dcc0));
    addAndMakeVisible (masterLabel);
    inMode.addItemList ({ "Mono L -> A + R", "Stereo L -> A, R -> R" }, 1);
    addAndMakeVisible (inMode);
    inAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "inmode", inMode);
}

void BoardView::paint (juce::Graphics& g)
{
    if (carpet.isValid()) { g.setTiledImageFill (carpet, 0, 0, 1.f); g.fillAll(); }
    else g.fillAll (juce::Colour (0xff3d5c8a));
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, (float) kW * 0.5f, (float) kH * 0.5f,
                                              juce::Colours::black.withAlpha (0.5f), 0.f, 0.f, true));
    g.fillAll();

    g.setColour (juce::Colour (0xd00e0e10));
    g.fillRoundedRectangle (20.f, 10.f, (float) kW - 40.f, 42.f, 10.f);
    g.setColour (juce::Colour (0xffe8dcc0));
    g.setFont (bold (22.f));
    g.drawText ("PEDALBOARD", 38, 10, 300, 42, juce::Justification::centredLeft);

    const auto left = juce::String::fromUTF8 ("\xE2\x86\x90"), right = juce::String::fromUTF8 ("\xE2\x86\x92");
    g.setFont (bold (14.f));
    const std::pair<juce::String, int> labels[] = { { "KETTE A   " + left, 60 }, { "KETTE R   " + right, 376 } };
    for (auto& l : labels)
    {
        g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawText (l.first, 27, l.second + 1, 300, 18, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xfff4ead2));             g.drawText (l.first, 26, l.second, 300, 18, juce::Justification::centredLeft);
    }
}

void BoardView::resized()
{
    auto top = juce::Rectangle<int> (20, 10, kW - 40, 42).reduced (14, 6);
    master.setBounds (top.removeFromRight (240));
    masterLabel.setBounds (top.removeFromRight (70));
    top.removeFromRight (16);
    inMode.setBounds (top.removeFromRight (220).reduced (0, 2));
    rowA.setBounds (20, 80, kW - 40, ChainRow::kRowHeight);
    rowR.setBounds (20, 396, kW - 40, ChainRow::kRowHeight);
}

PedalBoardEditor::PedalBoardEditor (PedalBoardProcessor& p) : juce::AudioProcessorEditor (&p), view (p)
{
    addAndMakeVisible (view);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) BoardView::kW / (double) BoardView::kH);
    setResizeLimits (940, 940 * BoardView::kH / BoardView::kW, 2400, 2400 * BoardView::kH / BoardView::kW);
    setSize (1480, 1480 * BoardView::kH / BoardView::kW);
}

void PedalBoardEditor::resized()
{
    view.setBounds (0, 0, BoardView::kW, BoardView::kH);
    view.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) BoardView::kW));
}
