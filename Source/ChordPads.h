#pragma once
#include "MidiSession.h"
#include <array>
namespace e4 {
// Snapshot notes on press so changing the root cannot change a later release.
class ChordPads {
public:
    explicit ChordPads(int firstSource = source::firstPad) : ownerBase(firstSource) {}
    const PerformanceSettings& getSettings() const noexcept { return settings; }
    template <typename Send> bool replace(const PerformanceSettings& next, Send&& send) {
        if (!next.valid()) return false;
        releaseAll(send); settings=next; return true;
    }
    static bool voicing(const ChordDefinition& definition, int root, std::array<int,4>& result) noexcept {
        if (root < 0 || root > 127 || definition.noteCount < 1 || definition.noteCount > 4
            || definition.inversion < 0 || definition.inversion >= definition.noteCount
            || definition.octave < -2 || definition.octave > 2) return false;
        for (int i=0;i<definition.noteCount;++i) {
            const int offset=definition.offsets[static_cast<std::size_t>(i)];
            if (offset < -24 || offset > 24 || (i>0 && offset<=definition.offsets[static_cast<std::size_t>(i-1)])) return false;
        }
        result.fill(-1);
        auto offsets=definition.offsets;
        for (int turn=0;turn<definition.inversion;++turn) {
            const int lowest=offsets[0],highest=offsets[static_cast<std::size_t>(definition.noteCount-1)];
            for (int i=0;i<definition.noteCount-1;++i) offsets[static_cast<std::size_t>(i)]=offsets[static_cast<std::size_t>(i+1)];
            offsets[static_cast<std::size_t>(definition.noteCount-1)]=lowest+((highest-lowest)/12+1)*12;
        }
        for (int i=0;i<definition.noteCount;++i) {
            const int note=root+offsets[static_cast<std::size_t>(i)]+definition.octave*12;
            if (note < 0 || note > 127) { result.fill(-1); return false; }
            result[static_cast<std::size_t>(i)]=note;
        }
        return true;
    }
    template <typename Send> void setHeld(int pad, bool down, int root, Send&& send) {
        if (pad < 0 || pad >= source::padCount) return;
        auto& notes = held[static_cast<std::size_t>(pad)];
        if (down) {
            if (notes[0] >= 0) return;
            std::array<int,4> candidate;
            if (!voicing(settings.pads[static_cast<std::size_t>(pad)],root,candidate)) return;
            notes=candidate;
            for (int note : notes)
                if (note >= 0) send(MidiEvent{0x90,note,100},pad + ownerBase);
        } else {
            for (int& note : notes) {
                if (note >= 0) send(MidiEvent{0x80,note,0},pad + ownerBase);
                note = -1;
            }
        }
    }
    template <typename Send> void releaseAll(Send&& send) {
        for (int pad = 0; pad < source::padCount; ++pad) setHeld(pad,false,0,send);
    }
private:
    int ownerBase;
    PerformanceSettings settings {};
    std::array<std::array<int,4>,4> held {{
        {{-1,-1,-1,-1}}, {{-1,-1,-1,-1}}, {{-1,-1,-1,-1}}, {{-1,-1,-1,-1}}
    }};
};
}
