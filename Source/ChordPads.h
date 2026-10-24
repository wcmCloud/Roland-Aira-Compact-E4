#pragma once
#include "MidiSession.h"
#include <array>
namespace e4 {
// Snapshot notes on press so changing the root cannot change a later release.
class ChordPads {
public:
    template <typename Send> void setHeld(int pad, bool down, int root, Send&& send) {
        if (pad < 0 || pad >= source::padCount || (down && (root < 0 || root > 116))) return;
        auto& notes = held[static_cast<std::size_t>(pad)];
        if (down) {
            if (notes[0] >= 0) return;
            for (std::size_t i = 0; i < notes.size(); ++i) {
                const auto& definition = settings.pads[static_cast<std::size_t>(pad)];
                const int interval = i < static_cast<std::size_t>(definition.noteCount) ? definition.offsets[i] : -1;
                notes[i] = interval < 0 ? -1 : root + interval;
                if (notes[i] >= 0) send(MidiEvent{0x90, notes[i],100},pad + source::firstPad);
            }
        } else {
            for (int& note : notes) {
                if (note >= 0) send(MidiEvent{0x80,note,0},pad + source::firstPad);
                note = -1;
            }
        }
    }
    template <typename Send> void releaseAll(Send&& send) {
        for (int pad = 0; pad < source::padCount; ++pad) setHeld(pad,false,0,send);
    }
private:
    const PerformanceSettings settings {};
    std::array<std::array<int,4>,4> held {{
        {{-1,-1,-1,-1}}, {{-1,-1,-1,-1}}, {{-1,-1,-1,-1}}, {{-1,-1,-1,-1}}
    }};
};
}
