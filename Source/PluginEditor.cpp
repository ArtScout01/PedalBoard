#include "PluginEditor.h"

PedalTile::PedalTile (PedalBoardProcessor& p, int chainIdx, int slotIdx) : slot (slotIdx)
{
    const auto& inf = pb::info (p.slotKind (chainIdx, slotIdx));
    title = inf.name; nk = inf.nk;
    for (int k = 0; k < nk; ++k)
    {
        knob[k].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob[k].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (knob[k]);
        knobLabel[k].setText (inf.knob[k], juce::dontSendNotification);
        knobLabel[k].setJustificationType (juce::Justification::centred);
        knobLabel[k].setFont (juce::FontOptions (11.f));
        addAndMakeVisible (knobLabel[k]);
        knobAtt[k] = std::make_unique<SliderAtt> (p.apvts, PedalBoardProcessor::id (chainIdx, slotIdx, "k" + juce::String (k)), knob[k]);
    }
    if (inf.sw)
    {
        sw.setButtonText (inf.sw);
        addAndMakeVisible (sw);
        swAtt = std::make_unique<ButtonAtt> (p.apvts, PedalBoardProcessor::id (chainIdx, slotIdx, "s0"), sw);
    }
    foot.setClickingTogglesState (true);
    foot.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff333333));
    foot.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2e9e4f));
    foot.onStateChange = [this] { foot.setButtonText (foot.getToggleState() ? "ON" : "OFF"); };
    addAndMakeVisible (foot);
    footAtt = std::make_unique<ButtonAtt> (p.apvts, PedalBoardProcessor::id (chainIdx, slotIdx, "on"), foot);
    foot.setButtonText (foot.getToggleState() ? "ON" : "OFF");
}

void PedalTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.f);
    g.setColour (juce::Colour (0xff1f2326)); g.fillRoundedRectangle (r, 8.f);
    g.setColour (juce::Colour (0xff3b4248)); g.fillRoundedRectangle (r.removeFromTop ((float) kHeader), 8.f);
    g.setColour (juce::Colours::white); g.setFont (juce::FontOptions (13.f, juce::Font::bold));
    g.drawText (title, getLocalBounds().removeFromTop (kHeader).reduced (8, 0), juce::Justification::centredLeft);
}

void PedalTile::resized()
{
    auto r = getLocalBounds().reduced (6);
    r.removeFromTop (kHeader);
    auto bottom = r.removeFromBottom (44); foot.setBounds (bottom.reduced (20, 6));
    if (sw.isVisible()) sw.setBounds (r.removeFromBottom (24));
    const int rows = nk > 1 ? (nk + 1) / 2 : 1, rowH = r.getHeight() / std::max (rows, 1);
    for (int k = 0; k < nk; ++k)
    {
        const int row = nk > 1 ? k / 2 : 0, col = nk > 1 ? k % 2 : 0, perRow = nk > 1 ? std::min (2, nk - row * 2) : 1;
        const int w = r.getWidth() / perRow;
        juce::Rectangle<int> cell (r.getX() + col * w, r.getY() + row * rowH, w, rowH);
        knobLabel[k].setBounds (cell.removeFromBottom (16));
        knob[k].setBounds (cell.reduced (2));
    }
}

void PedalTile::mouseDown (const juce::MouseEvent& e) { dragging = e.y < kHeader; if (dragging && onDown) onDown (this, e); }
void PedalTile::mouseDrag (const juce::MouseEvent& e) { if (dragging && onDrag) onDrag (this, e); }
void PedalTile::mouseUp (const juce::MouseEvent& e) { if (dragging && onUp) onUp (this, e); dragging = false; }

ChainRow::ChainRow (PedalBoardProcessor& p, int chainIdx) : proc (p), chain (chainIdx)
{
    for (int pos = 0; pos < proc.slotCount (chain); ++pos)
    {
        auto* t = new PedalTile (proc, chain, proc.getOrder (chain, pos));
        tiles.add (t); addAndMakeVisible (t);
        t->onDown = [this] (PedalTile* tile, const juce::MouseEvent&) { dragStartX = tile->getX(); tile->toFront (false); };
        t->onDrag = [this] (PedalTile* tile, const juce::MouseEvent& e)
        {
            const auto pe = e.getEventRelativeTo (this);
            tile->setTopLeftPosition (juce::jlimit (0, std::max (0, getWidth() - PedalTile::kWidth), pe.x - PedalTile::kWidth / 2), tile->getY());
        };
        t->onUp = [this] (PedalTile* tile, const juce::MouseEvent&)
        {
            const int n = tiles.size();
            const int target = juce::jlimit (0, n - 1, (tile->getX() + PedalTile::kWidth / 2) / PedalTile::kWidth);
            const int from = tiles.indexOf (tile);
            tiles.move (from, target);
            std::vector<int> order;
            for (auto* t2 : tiles) order.push_back (t2->slot);
            proc.setOrder (chain, order);
            layoutTiles();
        };
    }
    layoutTiles();
}

void ChainRow::layoutTiles (PedalTile*)
{
    for (int i = 0; i < tiles.size(); ++i) tiles[i]->setBounds (i * PedalTile::kWidth, 0, PedalTile::kWidth, PedalTile::kHeight);
}
void ChainRow::resized() { layoutTiles(); }
void ChainRow::paint (juce::Graphics&) {}

PedalBoardEditor::PedalBoardEditor (PedalBoardProcessor& p) : AudioProcessorEditor (&p), proc (p), rowA (p, 0), rowR (p, 1)
{
    addAndMakeVisible (rowA); addAndMakeVisible (rowR);
    master.setSliderStyle (juce::Slider::LinearHorizontal); master.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
    addAndMakeVisible (master);
    masterAtt = std::make_unique<SliderAtt> (p.apvts, "master", master);
    masterLabel.setText ("Master", juce::dontSendNotification); addAndMakeVisible (masterLabel);
    inMode.addItemList ({ "Mono L -> A + R", "Stereo L -> A, R -> R" }, 1); addAndMakeVisible (inMode);
    inAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "inmode", inMode);
    tapeOnAtt = std::make_unique<ButtonAtt> (p.apvts, "tape_on", tapeOn); addAndMakeVisible (tapeOn);
    tapeNoiseAtt = std::make_unique<ButtonAtt> (p.apvts, "tape_noise", tapeNoise); addAndMakeVisible (tapeNoise);
    tapeMachine.addItemList ({ "TEAC A-3340S", "Revox B77" }, 1); addAndMakeVisible (tapeMachine);
    tapeMachineAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "tape_machine", tapeMachine);
    tapeSpeed.addItemList ({ "15 ips", "7.5 ips", "3.75 ips (nur Revox)" }, 1); addAndMakeVisible (tapeSpeed);
    tapeSpeedAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "tape_speed", tapeSpeed);
    const char* ids[6] = { "tape_drive", "tape_bias", "tape_wf", "tape_bump", "tape_xtalk", "tape_link" };
    const char* names[6] = { "Drive", "Bias", "Wow/Flutter", "Head Bump", "Crosstalk", "Gain Link" };
    for (int i = 0; i < 6; ++i)
    {
        tapeKnob[i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        tapeKnob[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
        addAndMakeVisible (tapeKnob[i]);
        tapeKnobLabel[i].setText (names[i], juce::dontSendNotification);
        tapeKnobLabel[i].setJustificationType (juce::Justification::centred);
        addAndMakeVisible (tapeKnobLabel[i]);
        tapeKnobAtt[i] = std::make_unique<SliderAtt> (p.apvts, ids[i], tapeKnob[i]);
    }
    setSize (PedalTile::kWidth * std::max (p.slotCount (0), p.slotCount (1)) + 40, 2 * PedalTile::kHeight + 140 + 150);
}

void PedalBoardEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101214));
    g.setColour (juce::Colours::white); g.setFont (juce::FontOptions (15.f, juce::Font::bold));
    g.drawText ("Kette A  <---", 20, 40, 300, 20, juce::Justification::centredLeft);
    g.drawText ("Kette R  --->", 20, 40 + PedalTile::kHeight + 40, 300, 20, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff1f2326)); g.fillRoundedRectangle (tapeRect.toFloat(), 8.f);
    g.setColour (juce::Colours::white); g.drawText ("Tape-Stufe am Ende (beide Ketten)", tapeRect.getX() + 12, tapeRect.getY() + 2, 400, 20, juce::Justification::centredLeft);
}

void PedalBoardEditor::resized()
{
    auto r = getLocalBounds().reduced (20, 10);
    auto top = r.removeFromTop (28);
    masterLabel.setBounds (top.removeFromRight (60)); master.setBounds (top.removeFromRight (220).withTrimmedLeft (0));
    inMode.setBounds (top.removeFromRight (220).reduced (4, 0));
    r.removeFromTop (30); rowA.setBounds (r.removeFromTop (PedalTile::kHeight));
    r.removeFromTop (50); rowR.setBounds (r.removeFromTop (PedalTile::kHeight));
    r.removeFromTop (16); tapeRect = r.removeFromTop (130);
    auto t = tapeRect.reduced (8, 22);
    auto left = t.removeFromLeft (200);
    tapeOn.setBounds (left.removeFromTop (24)); tapeMachine.setBounds (left.removeFromTop (26).reduced (0, 2));
    tapeSpeed.setBounds (left.removeFromTop (26).reduced (0, 2)); tapeNoise.setBounds (left.removeFromTop (24));
    const int w = std::min (110, t.getWidth() / 6);
    for (int i = 0; i < 6; ++i)
    {
        auto cell = t.removeFromLeft (w);
        tapeKnobLabel[i].setBounds (cell.removeFromTop (16)); tapeKnob[i].setBounds (cell);
    }
}
