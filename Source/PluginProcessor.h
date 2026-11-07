#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include "pb/PedalFactory.h"
#include "pb/Tape.h"
#include "pb/Routing.h"

class PedalBoardProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kChains = 2, kMaxSlots = 16;

    PedalBoardProcessor();
    ~PedalBoardProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PedalBoard"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Reihenfolge einer Kette: order[chain][position] = Slot-Index (Slots haben feste Parameter)
    int slotCount (int chain) const { return (int) defs[chain].size(); }
    pb::Kind slotKind (int chain, int slot) const { return defs[chain][(size_t) slot]; }
    int getOrder (int chain, int pos) const { return order[chain][(size_t) pos].load(); }
    void setOrder (int chain, const std::vector<int>& newOrder);   // vom Editor aufgerufen

    static juce::String id (int chain, int slot, const juce::String& what) { return juce::String (chain == 0 ? "A" : "R") + juce::String (slot) + "_" + what; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void loadOrderFromState();
    void processChain (int chain, int side, float* data, int n);

    std::vector<pb::Kind> defs[kChains];
    std::array<std::atomic<int>, kMaxSlots> order[kChains];
    std::unique_ptr<pb::Pedal> inst[kChains][kMaxSlots];
    std::unique_ptr<pb::Pedal> instR[kChains][kMaxSlots];      // zweiter Satz fuer den rechten Kanal (Stereo, Dual-Mono)
    bool wasOn[kChains][kMaxSlots] = {};
    std::atomic<float>* pOn[kChains][kMaxSlots] = {};
    std::atomic<float>* pK[kChains][kMaxSlots][3] = {};
    std::atomic<float>* pS[kChains][kMaxSlots] = {};
    std::atomic<float>* pIn = nullptr;
    std::atomic<float>* pTapeOn = nullptr; std::atomic<float>* pTapeMachine = nullptr; std::atomic<float>* pTapeSpeed = nullptr;
    std::atomic<float>* pTapeDrive = nullptr; std::atomic<float>* pTapeBias = nullptr; std::atomic<float>* pTapeWf = nullptr;
    std::atomic<float>* pTapeBump = nullptr; std::atomic<float>* pTapeXtalk = nullptr; std::atomic<float>* pTapeLink = nullptr; std::atomic<float>* pTapeNoise = nullptr;
    std::atomic<float>* pTapeHys = nullptr; std::atomic<float>* pTapeType = nullptr; std::atomic<float>* pTapeBiasSw = nullptr; std::atomic<float>* pTapeEqSw = nullptr;
    pb::TapeMachine tape;
    std::atomic<float>* pMaster = nullptr; std::atomic<float>* pRoute = nullptr; std::atomic<float>* pStereo = nullptr; std::atomic<float>* pMix = nullptr; std::atomic<float>* pPan = nullptr;
    pb::DryDelay dry[kChains]; pb::Smooth sMix, sPan; std::vector<float> dryIn[kChains];
    std::vector<float> buf[kChains];
    double sr = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalBoardProcessor)
};
