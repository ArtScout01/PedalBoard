#pragma once
#include "PluginProcessor.h"
#include <functional>
#include <memory>

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

// Eine Pedal-Kachel: Titel (Griff zum Ziehen), bis zu 3 Regler, optionaler Schalter, Fussschalter
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
    static constexpr int kHeader = 28, kWidth = 140, kHeight = 250;

private:
    bool dragging = false;
    juce::String title;
    juce::Label knobLabel[3];
    juce::Slider knob[3];
    std::unique_ptr<SliderAtt> knobAtt[3];
    juce::ToggleButton sw;
    std::unique_ptr<ButtonAtt> swAtt;
    juce::TextButton foot { "OFF" };
    std::unique_ptr<ButtonAtt> footAtt;
    int nk = 0;
};

class ChainRow : public juce::Component
{
public:
    ChainRow (PedalBoardProcessor& p, int chain);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void layoutTiles (PedalTile* skip = nullptr);
    PedalBoardProcessor& proc;
    int chain;
    juce::OwnedArray<PedalTile> tiles;      // in aktueller Reihenfolge
    int dragStartX = 0;
};

class PedalBoardEditor : public juce::AudioProcessorEditor
{
public:
    explicit PedalBoardEditor (PedalBoardProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PedalBoardProcessor& proc;
    ChainRow rowA, rowR;
    juce::Slider master;
    juce::Label masterLabel;
    std::unique_ptr<SliderAtt> masterAtt;
    juce::ComboBox inMode;
    // Tape-Stufe am Ende der Kette
    juce::Rectangle<int> tapeRect;
    juce::ToggleButton tapeOn { "TAPE" }, tapeNoise { "Noise" };
    juce::ComboBox tapeMachine, tapeSpeed;
    juce::Slider tapeKnob[6];
    juce::Label tapeKnobLabel[6];
    std::unique_ptr<ButtonAtt> tapeOnAtt, tapeNoiseAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tapeMachineAtt, tapeSpeedAtt;
    std::unique_ptr<SliderAtt> tapeKnobAtt[6];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> inAtt;
};
