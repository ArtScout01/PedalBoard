#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include "pb/PedalFactory.h"

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

    std::vector<pb::Kind> defs[kChains];
    std::array<std::atomic<int>, kMaxSlots> order[kChains];
    std::unique_ptr<pb::Pedal> inst[kChains][kMaxSlots];
    bool wasOn[kChains][kMaxSlots] = {};
    std::atomic<float>* pOn[kChains][kMaxSlots] = {};
    std::atomic<float>* pK[kChains][kMaxSlots][3] = {};
    std::atomic<float>* pS[kChains][kMaxSlots] = {};
    std::atomic<float>* pIn = nullptr;
    std::atomic<float>* pMaster = nullptr;
    std::vector<float> buf[kChains];
    double sr = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalBoardProcessor)
};
