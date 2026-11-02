#pragma once
#include "PluginProcessor.h"
#include "BoardLook.h"
#include <array>
#include <functional>
#include <memory>

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

// Ein Pedal wie im Layout-Entwurf. Rechteckige Pedale lassen sich per Ziehen gegeneinander tauschen (= Signalreihenfolge).
// Fuzz Face (rund) und Cry Baby (Wah) bleiben auf ihren Plaetzen.
class PedalComponent : public juce::Component, private juce::Timer
{
public:
    PedalComponent (PedalBoardProcessor& p, int pedalIndex);
    ~PedalComponent() override { stopTimer(); }

    int pedal;                                     // Index in der Kette des Prozessors
    bool isFuzz = false, isWah = false;
    bool movable() const { return ! (isFuzz || isWah); }
    float designW() const { return isFuzz ? 175.f : (isWah ? 125.f : 124.f); }
    float designH() const { return isFuzz ? 175.f : (isWah ? 275.f : 215.f); }
    std::function<void (PedalComponent*)> onDrop;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Point<float> knobCentre (int k) const;
    float knobSize (int k) const;
    juce::Point<float> footCentre() const;
    float footSize() const { return isWah ? 30.f : 36.f; }

    pb::Kind kind = pb::Kind::FuzzFace;
    const pb::Info* inf = nullptr;
    const board::Skin* sk = nullptr;
    int nk = 0;
    juce::Slider knob[3];
    std::unique_ptr<SliderAtt> knobAtt[3];
    juce::ToggleButton sw;
    std::unique_ptr<ButtonAtt> swAtt;
    board::FootSwitch foot;
    std::unique_ptr<ButtonAtt> footAtt;
    juce::ComponentDragger dragger;
    bool dragging = false, lastOn = false;
};

// Popup mit allen Tape-Einstellungen, die nicht auf dem Geraet selbst sitzen
class TapeSettings : public juce::Component
{
public:
    explicit TapeSettings (PedalBoardProcessor& p);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff1c1f22)); }

private:
    juce::ComboBox machine, speed, type, biasSw, eqSw;
    juce::ToggleButton noise { "Rauschen" }, hys { "Hysterese" };
    juce::Slider knob[4];
    juce::Label label[4];
    std::unique_ptr<ComboAtt> machineAtt, speedAtt, typeAtt, biasSwAtt, eqSwAtt;
    std::unique_ptr<ButtonAtt> noiseAtt, hysAtt;
    std::unique_ptr<SliderAtt> knobAtt[4];
};

// Tonbandgeraet unten rechts: Spulen (drehen bei "an"), VU-Meter (Ausgangspegel), Drive, Bias, Fussschalter, Einstellungen
class TapeDeck : public juce::Component, private juce::Timer
{
public:
    explicit TapeDeck (PedalBoardProcessor& p);
    ~TapeDeck() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    PedalBoardProcessor& proc;
    juce::Slider drive, bias;
    std::unique_ptr<SliderAtt> driveAtt, biasAtt;
    board::FootSwitch foot;
    std::unique_ptr<ButtonAtt> footAtt;
    juce::TextButton settings { "SET" };
    float reel = 0.f, needle = 0.f;
};

class PedalBoardEditor : public juce::AudioProcessorEditor
{
public:
    explicit PedalBoardEditor (PedalBoardProcessor&);
    ~PedalBoardEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void layoutPedals();
    void dropPedal (PedalComponent*);

    // Platzmitten der 15 Plaetze in Entwurfskoordinaten (obere Reihe, dann untere Reihe)
    static constexpr float kSlotX[15] = { 170, 340, 480, 615, 757, 889, 1037, 1204, 1357, 160, 316, 478, 625, 776, 928 };
    static constexpr float kSlotY[15] = { 325, 325, 325, 325, 312, 325, 325, 325, 325, 595, 595, 595, 595, 595, 595 };

    PedalBoardProcessor& proc;
    board::PedalLNF lnf;
    juce::OwnedArray<PedalComponent> pedals;
    std::array<int, 16> slotPedal {};
    int nSlots = 0;
    TapeDeck deck;
    juce::Slider master;
    std::unique_ptr<SliderAtt> masterAtt;
    juce::ComboBox inMode;
    std::unique_ptr<ComboAtt> inAtt;
    juce::Image rug;
};
