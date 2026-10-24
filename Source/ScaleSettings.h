#pragma once
#include "PerformanceSettings.h"
#include <array>
namespace e4 {
struct ScaleSettings {
    enum Mode { off, reject, nearest };
    enum Type { chromatic, major, naturalMinor, majorPentatonic, minorPentatonic };
    int mode=off, tonic=0, type=major;
    bool pads=false;
    bool valid() const noexcept { return mode>=off && mode<=nearest && tonic>=0 && tonic<12 && type>=chromatic && type<=minorPentatonic; }
    bool operator==(const ScaleSettings& other) const noexcept { return mode==other.mode && tonic==other.tonic && type==other.type && pads==other.pads; }
    bool operator!=(const ScaleSettings& other) const noexcept { return !(*this==other); }
    bool contains(int note) const noexcept {
        if (note<0 || note>127 || !valid()) return false;
        constexpr std::array<unsigned,5> masks {{0xfff,0xab5,0x5ad,0x295,0x4a9}};
        const int degree=(note-tonic+12)%12;
        return (masks[static_cast<std::size_t>(type)] & (1u<<degree))!=0;
    }
    int map(int note,int owner=source::input) const noexcept {
        if (note<0 || note>127 || !valid()) return -1;
        const bool direct=owner==source::input || owner==source::keyboard;
        const bool pad=owner>=source::firstPad && owner<source::firstPad+source::padCount;
        if (mode==off || (!direct && !(pad && pads)) || contains(note)) return note;
        if (mode==reject) return -1;
        for (int distance=1;distance<12;++distance) {
            if (contains(note-distance)) return note-distance; // Lower note wins ties.
            if (contains(note+distance)) return note+distance;
        }
        return -1;
    }
};
}
