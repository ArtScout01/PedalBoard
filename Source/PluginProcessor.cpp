#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout PedalBoardProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "inmode", 1 }, "Input", juce::StringArray { "Mono L -> A + R", "Stereo L -> A, R -> R" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "master", 1 }, "Master", juce::NormalisableRange<float> (0.f, 2.f, 0.01f), 1.f));
    for (int c = 0; c < kChains; ++c)
    {
        const auto chain = pb::defaultChain (c);
        for (int s = 0; s < (int) chain.size(); ++s)
        {
            const auto& inf = pb::info (chain[(size_t) s]);
            const juce::String pre = juce::String (c == 0 ? "Kette A " : "Kette R ") + inf.name + " ";
            layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id (c, s, "on"), 1 }, pre + "On", false));
            for (int k = 0; k < inf.nk; ++k)
                layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id (c, s, "k" + juce::String (k)), 1 }, pre + inf.knob[k],
                                                                         juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
            if (inf.sw) layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id (c, s, "s0"), 1 }, pre + inf.sw, false));
        }
    }
    return layout;
}

PedalBoardProcessor::PedalBoardProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pIn = apvts.getRawParameterValue ("inmode");
    pMaster = apvts.getRawParameterValue ("master");
    for (int c = 0; c < kChains; ++c)
    {
        defs[c] = pb::defaultChain (c);
        for (int s = 0; s < kMaxSlots; ++s) order[c][(size_t) s].store (s);
        for (int s = 0; s < (int) defs[c].size(); ++s)
        {
            const auto& inf = pb::info (defs[c][(size_t) s]);
            inst[c][s] = pb::make (defs[c][(size_t) s]);
            pOn[c][s] = apvts.getRawParameterValue (id (c, s, "on"));
            for (int k = 0; k < inf.nk; ++k) pK[c][s][k] = apvts.getRawParameterValue (id (c, s, "k" + juce::String (k)));
            if (inf.sw) pS[c][s] = apvts.getRawParameterValue (id (c, s, "s0"));
        }
    }
}

void PedalBoardProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (int c = 0; c < kChains; ++c)
    {
        buf[c].assign ((size_t) std::max (samplesPerBlock, 1), 0.f);
        for (int s = 0; s < (int) defs[c].size(); ++s) { inst[c][s]->prepare ((float) sampleRate); wasOn[c][s] = false; }
    }
}

bool PedalBoardProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (l.getMainInputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainInputChannelSet() == juce::AudioChannelSet::mono());
}

void PedalBoardProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = b.getNumSamples(), nIn = getTotalNumInputChannels();
    const bool stereoIn = (int) pIn->load() == 1 && nIn >= 2;
    for (int c = 0; c < kChains; ++c)
    {
        if ((int) buf[c].size() < n) buf[c].assign ((size_t) n, 0.f);          // nur falls der Host den Block vergroessert
        const float* src = b.getReadPointer (stereoIn ? c : 0);
        std::copy (src, src + n, buf[c].begin());
    }
    for (int c = 0; c < kChains; ++c)
        for (int pos = 0; pos < (int) defs[c].size(); ++pos)
        {
            const int s = order[c][(size_t) pos].load();
            if (s < 0 || s >= (int) defs[c].size()) continue;
            const bool on = pOn[c][s]->load() > 0.5f;
            if (on && ! wasOn[c][s]) if (auto* v = dynamic_cast<pb::VB2*> (inst[c][s].get())) v->retrigger();
            wasOn[c][s] = on;
            if (! on) continue;
            pb::P p; const auto& inf = pb::info (defs[c][(size_t) s]);
            for (int k = 0; k < inf.nk; ++k) p.k[k] = pK[c][s][k]->load();
            if (pS[c][s]) p.s[0] = pS[c][s]->load() > 0.5f ? 1 : 0;
            inst[c][s]->process (buf[c].data(), n, p);
        }
    const float master = pMaster->load();
    float* l = b.getWritePointer (0);
    float* r = b.getNumChannels() > 1 ? b.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        l[i] = std::tanh (buf[0][(size_t) i] * master);                          // weiche Begrenzung
        if (r) r[i] = std::tanh (buf[1][(size_t) i] * master);
    }
}

void PedalBoardProcessor::setOrder (int chain, const std::vector<int>& newOrder)
{
    juce::StringArray sa;
    for (size_t i = 0; i < newOrder.size(); ++i) { order[chain][i].store (newOrder[i]); sa.add (juce::String (newOrder[i])); }
    apvts.state.setProperty (chain == 0 ? "orderA" : "orderR", sa.joinIntoString (","), nullptr);
}

void PedalBoardProcessor::loadOrderFromState()
{
    for (int c = 0; c < kChains; ++c)
    {
        const auto sa = juce::StringArray::fromTokens (apvts.state.getProperty (c == 0 ? "orderA" : "orderR").toString(), ",", "");
        const int n = (int) defs[c].size();
        std::vector<int> o; std::vector<bool> used ((size_t) n, false);
        for (auto& t : sa) { const int v = t.getIntValue(); if (v >= 0 && v < n && ! used[(size_t) v]) { used[(size_t) v] = true; o.push_back (v); } }
        if ((int) o.size() != n) { o.clear(); for (int i = 0; i < n; ++i) o.push_back (i); }   // ungueltig -> Standardreihenfolge
        for (int i = 0; i < n; ++i) order[c][(size_t) i].store (o[(size_t) i]);
    }
}

void PedalBoardProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void PedalBoardProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) { apvts.replaceState (juce::ValueTree::fromXml (*xml)); loadOrderFromState(); }
}

juce::AudioProcessorEditor* PedalBoardProcessor::createEditor() { return new PedalBoardEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PedalBoardProcessor(); }
