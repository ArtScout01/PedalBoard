#pragma once
// JUCE-freie Pedal-Fabrik: Typen, Reglerbeschriftungen, Standard-Ketten. Getrennt testbar.
#include "PedalsCircuit.h"
#include <memory>
#include <vector>

namespace pb
{
enum class Kind { FuzzFace, TS808, DynaComp, CryBaby, Phase90, CE2, UniVibe, VB2, MicroAmp, DM2, Spring6G15, MidiVerb, H910, VP330, VE20, Count };

struct Info
{
    const char* name; int nk; const char* knob[3];
    const char* sw;   // nullptr = kein Schalter; sonst Text, wenn Schalter AN
};

inline const Info& info (Kind k)
{
    static const Info t[] = {
        { "Fuzz Face",     2, { "Fuzz", "Volume", "" },            "Silicon" },
        { "Tube Screamer", 3, { "Drive", "Tone", "Level" },        nullptr },
        { "Dyna Comp",     2, { "Output", "Sensitivity", "" },     nullptr },
        { "Cry Baby",      2, { "Pedal", "Volume", "" },           nullptr },
        { "Phase 90",      1, { "Speed", "", "" },                 "Feedback" },
        { "CE-2 Chorus",   2, { "Rate", "Depth", "" },             nullptr },
        { "Uni-Vibe",      3, { "Speed", "Intensity", "Volume" },  "Vibrato" },
        { "VB-2 Vibrato",  3, { "Rate", "Depth", "Rise Time" },    nullptr },
        { "Micro Amp",     1, { "Gain", "", "" },                  nullptr },
        { "DM-2 Delay",    3, { "Repeat Rate", "Echo", "Intensity" }, "MN3205" },
        { "6G15 Reverb",   3, { "Dwell", "Tone", "Mixer" },        nullptr },
        { "MIDIverb",      3, { "Program", "Mix", "Decay" },       nullptr },
        { "H910 Harmonizer", 3, { "Pitch", "Feedback", "Mix" },    "Glitch" },
        { "VP-330 Voice",  3, { "Vowel", "Ensemble", "Mix" },      nullptr },
        { "VE-20 Vocal",   3, { "Harmony", "Interval", "Reverb" }, nullptr },
    };
    return t[(int) k];
}

inline std::unique_ptr<Pedal> make (Kind k)
{
    switch (k)
    {
        case Kind::FuzzFace:   return std::make_unique<FuzzFace2>();
        case Kind::TS808:      return std::make_unique<TS808>();
        case Kind::DynaComp:   return std::make_unique<DynaComp2>();
        case Kind::CryBaby:    return std::make_unique<CryBaby2>();
        case Kind::Phase90:    return std::make_unique<Phase90>();
        case Kind::CE2:        return std::make_unique<CE2>();
        case Kind::UniVibe:    return std::make_unique<UniVibe>();
        case Kind::VB2:        return std::make_unique<VB2>();
        case Kind::MicroAmp:   return std::make_unique<MicroAmp>();
        case Kind::DM2:        return std::make_unique<DM2>();
        case Kind::Spring6G15: return std::make_unique<Fender6G15>();
        case Kind::MidiVerb:   return std::make_unique<MidiVerb>();
        case Kind::H910:       return std::make_unique<H910>();
        case Kind::VP330:      return std::make_unique<VP330>();
        case Kind::VE20:       return std::make_unique<VE20>();
        default: return nullptr;
    }
}

// Ketten wie auf dem Foto (Ersatz fuer Pedale ohne Schaltungsdaten siehe Projektnotizen)
inline std::vector<Kind> defaultChain (int chain)
{
    if (chain == 0)   // A (Pfeil nach links): Cry Baby, Carbon Copy->DM-2, Holy Grail->6G15, Small Stone->Phase 90, BD-2->TS808, Dyna Comp, Diamond->Uni-Vibe, VB-2, Bi-FET->Micro Amp
        return { Kind::CryBaby, Kind::DM2, Kind::Spring6G15, Kind::Phase90, Kind::TS808, Kind::DynaComp, Kind::UniVibe, Kind::VB2, Kind::MicroAmp, Kind::MidiVerb, Kind::H910, Kind::VP330 };
    return { Kind::FuzzFace, Kind::TS808, Kind::Spring6G15, Kind::CE2, Kind::VE20 };   // R (Pfeil nach rechts): Fuzz Face, BD-2->TS808, Holy Grail->6G15, Brigade->CE-2, VE-20
}
} // namespace pb
