#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout PedalBoardProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "inmode", 1 }, "Input", juce::StringArray { "Mono L -> A + R", "Stereo L -> A, R -> R" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "master", 1 }, "Master", juce::NormalisableRange<float> (0.f, 2.f, 0.01f), 1.f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "tape_on", 1 }, "Tape On", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "tape_machine", 1 }, "Tape Machine", juce::StringArray { "TEAC A-3340S", "Revox B77" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "tape_speed", 1 }, "Tape Speed", juce::StringArray { "15 ips", "7.5 ips", "3.75 ips (nur Revox)" }, 1));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tape_drive", 1 }, "Tape Drive (dB)", juce::NormalisableRange<float> (-12.f, 18.f, 0.1f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tape_bias", 1 }, "Tape Bias", juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tape_wf", 1 }, "Tape Wow/Flutter", juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tape_bump", 1 }, "Tape Head Bump", juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tape_xtalk", 1 }, "Tape Crosstalk", juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tape_link", 1 }, "Tape Gain Link", juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 1.f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "tape_noise", 1 }, "Tape Noise", true));
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
    pTapeOn = apvts.getRawParameterValue ("tape_on"); pTapeMachine = apvts.getRawParameterValue ("tape_machine"); pTapeSpeed = apvts.getRawParameterValue ("tape_speed");
    pTapeDrive = apvts.getRawParameterValue ("tape_drive"); pTapeBias = apvts.getRawParameterValue ("tape_bias"); pTapeWf = apvts.getRawParameterValue ("tape_wf");
    pTapeBump = apvts.getRawParameterValue ("tape_bump"); pTapeXtalk = apvts.getRawParameterValue ("tape_xtalk"); pTapeLink = apvts.getRawParameterValue ("tape_link");
    pTapeNoise = apvts.getRawParameterValue ("tape_noise");
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
    tape.prepare ((float) sampleRate);
    setLatencySamples (tape.latency());
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
    if (pTapeOn->load() > 0.5f)
    {
        pb::TapeParams tp;
        tp.machine = (int) pTapeMachine->load(); tp.speed = (int) pTapeSpeed->load(); tp.driveDb = pTapeDrive->load(); tp.bias = pTapeBias->load();
        tp.wf = pTapeWf->load(); tp.bump = pTapeBump->load(); tp.xtalk = pTapeXtalk->load(); tp.link = pTapeLink->load(); tp.noise = pTapeNoise->load() > 0.5f;
        tape.process (buf[0].data(), buf[1].data(), n, tp);
    }
    else tape.processBypass (buf[0].data(), buf[1].data(), n);      // gleiche Latenz, damit die Host-Kompensation stimmt
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
