#pragma once
#include "MidiSession.h"
#include <array>
namespace e4 {
// Snapshot notes on press so changing the root cannot change a later release.
class ChordPads {
public:
    template <typename Send> void setHeld(int pad, bool down, int root, Send&& send) {
        if (pad < 0 || pad >= 4 || (down && (root < 0 || root > 116))) return;
        auto& notes = held[static_cast<std::size_t>(pad)];
        if (down) {
            if (notes[0] >= 0) return;
            for (std::size_t i = 0; i < notes.size(); ++i) {
                const int interval = intervals[static_cast<std::size_t>(pad)][i];
                notes[i] = interval < 0 ? -1 : root + interval;
                if (notes[i] >= 0) send(MidiEvent{0x90, notes[i],100},pad + 2);
            }
        } else {
            for (int& note : notes) {
                if (note >= 0) send(MidiEvent{0x80,note,0},pad + 2);
                note = -1;
            }
        }
    }
    template <typename Send> void releaseAll(Send&& send) {
        for (int pad = 0; pad < 4; ++pad) setHeld(pad,false,0,send);
    }
private:
    inline static constexpr std::array<std::array<int,4>,4> intervals {{
        {{0,4,7,-1}}, {{0,3,7,-1}}, {{0,2,7,-1}}, {{0,4,7,11}}
    }};
    std::array<std::array<int,4>,4> held {{
        {{-1,-1,-1,-1}}, {{-1,-1,-1,-1}}, {{-1,-1,-1,-1}}, {{-1,-1,-1,-1}}
    }};
};
}
