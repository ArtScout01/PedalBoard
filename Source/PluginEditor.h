#pragma once
#include "PluginProcessor.h"
#include <functional>
#include <memory>

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

// Farben je Pedal (angelehnt an die Original-Gehaeuse)
struct Skin { juce::Colour body, text; };
Skin skinFor (pb::Kind k);

// Aussehen der Bedienelemente: schwarze Potiknoepfe mit weissem Strich, Mini-Kippschalter, verchromter Fussschalter
class PedalLaf : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override {}
};

// Eine Pedal-Kachel: Titel (Griff zum Ziehen), bis zu 3 Regler, optionaler Schalter, LED + Fussschalter
class PedalTile : public juce::Component
{
public:
    PedalTile (PedalBoardProcessor& p, int chain, int slot);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    int slot;
    std::function<void (PedalTile*, const juce::MouseEvent&)> onDown, onDrag, onUp;
    static constexpr int kHeader = 30, kWidth = 124, kHeight = 240, kGap = 22, kPitch = kWidth + kGap;
    static constexpr int kJackY = 96;   // Hoehe der Buchsen (Kachel-Koordinaten)

private:
    bool dragging = false;
    Skin skin;
    juce::String title;
    juce::Label knobLabel[3];
    juce::Slider knob[3];
    std::unique_ptr<SliderAtt> knobAtt[3];
    juce::ToggleButton sw;
    std::unique_ptr<ButtonAtt> swAtt;
    juce::TextButton foot;
    std::unique_ptr<ButtonAtt> footAtt;
    int nk = 0;
};

// Eine Kette auf ihrem Pedalboard (schwarzer Rahmen, Klettschienen, Patchkabel). Kette A laeuft wie auf dem Foto von rechts nach links.
class ChainRow : public juce::Component
{
public:
    ChainRow (PedalBoardProcessor& p, int chain);
    void resized() override;
    void paint (juce::Graphics&) override;
    static constexpr int kPadX = 44, kTop = 14, kBottomRoom = 34, kRowHeight = kTop + PedalTile::kHeight + kBottomRoom;

private:
    int xForIndex (int i) const;
    int indexForX (int centreX) const;
    void layoutTiles();
    PedalBoardProcessor& proc;
    int chain;
    bool rtl;
    juce::OwnedArray<PedalTile> tiles;      // in aktueller Signalreihenfolge
};

// Gesamtansicht in fester Entwurfsgroesse (wird vom Editor skaliert)
class BoardView : public juce::Component
{
public:
    explicit BoardView (PedalBoardProcessor&);
    ~BoardView() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kW = 1880, kH = 700;

private:
    PedalLaf laf;
    juce::Image carpet;
    ChainRow rowA, rowR;
    juce::Slider master;
    juce::Label masterLabel;
    std::unique_ptr<SliderAtt> masterAtt;
    juce::ComboBox inMode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> inAtt;
};

class PedalBoardEditor : public juce::AudioProcessorEditor
{
public:
    explicit PedalBoardEditor (PedalBoardProcessor&);
    void resized() override;

private:
    BoardView view;
};
