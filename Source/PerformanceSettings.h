#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace e4 {
// Stable IDs: existing sessions keep input=0, keyboard=1 and factory pads=2..5.
namespace source {
inline constexpr int input = 0, keyboard = 1, firstPad = 2, padCount = 4;
inline constexpr int arpeggiator = firstPad + padCount, sequencer = arpeggiator + 1;
inline constexpr int firstMappedPad = sequencer + 1;
inline constexpr int count = firstMappedPad + padCount;
}
struct ChordDefinition {
    std::array<char, 25> name {};
    std::array<int, 4> offsets {{0,4,7,0}};
    int noteCount = 3;
    int inversion = 0;
    int octave = 0;
};
// Value snapshot only: no held notes, device routes or enabled generators.
struct PerformanceSettings {
    static constexpr int schema = 1;
    std::array<ChordDefinition, source::padCount> pads {{
        {{{'M','a','j','o','r'}}, {{0,4,7,0}}, 3,0,0},
        {{{'M','i','n','o','r'}}, {{0,3,7,0}}, 3,0,0},
        {{{'S','u','s','2'}}, {{0,2,7,0}}, 3,0,0},
        {{{'M','a','j','7'}}, {{0,4,7,11}}, 4,0,0}
    }};
    bool valid() const noexcept {
        for (const auto& pad : pads) {
            if (pad.name[0] == 0 || pad.name.back() != 0 || pad.noteCount < 1 || pad.noteCount > 4
                || pad.inversion < 0 || pad.inversion >= pad.noteCount || pad.octave < -2 || pad.octave > 2) return false;
            bool ended = false;
            for (char c : pad.name) {
                if (c == 0) ended = true;
                else if (ended || c < 32 || c > 126) return false;
            }
            for (int i = 0; i < 4; ++i) {
                const int offset = pad.offsets[static_cast<std::size_t>(i)];
                if (i >= pad.noteCount) { if (offset != 0) return false; }
                else if (offset < -24 || offset > 24 || (i > 0 && offset <= pad.offsets[static_cast<std::size_t>(i-1)])) return false;
            }
        }
        return true;
    }
};
static_assert(std::is_trivially_copyable<PerformanceSettings>::value, "Runtime settings must be bounded value snapshots");

// Explicit wire format, independent of compiler padding/endian layout. No file I/O.
// ASCII names initially; unused bytes must be zero. Future versions fail closed.
class PerformancePreset {
public:
    static constexpr std::size_t size = 5 + source::padCount * 32;
    using Bytes = std::array<std::uint8_t, size>;
    static bool encode(const PerformanceSettings& settings, Bytes& destination) noexcept {
        if (!settings.valid()) return false;
        Bytes bytes {}; bytes[0]='E'; bytes[1]='4'; bytes[2]='P'; bytes[3]='F'; bytes[4]=PerformanceSettings::schema;
        std::size_t at=5;
        for (const auto& pad : settings.pads) {
            for (char c : pad.name) bytes[at++]=static_cast<std::uint8_t>(c);
            bytes[at++]=static_cast<std::uint8_t>(pad.noteCount);
            bytes[at++]=static_cast<std::uint8_t>(pad.inversion);
            bytes[at++]=static_cast<std::uint8_t>(pad.octave+2);
            for (int offset : pad.offsets) bytes[at++]=static_cast<std::uint8_t>(offset+24);
        }
        destination=bytes; return true;
    }
    static bool decode(const std::uint8_t* bytes, std::size_t length, PerformanceSettings& destination) noexcept {
        if (bytes == nullptr || length != size || bytes[0]!='E' || bytes[1]!='4' || bytes[2]!='P'
            || bytes[3]!='F' || bytes[4]!=PerformanceSettings::schema) return false;
        PerformanceSettings candidate;
        std::size_t at=5;
        for (auto& pad : candidate.pads) {
            for (char& c : pad.name) { const auto value=bytes[at++]; if (value > 126) return false; c=static_cast<char>(value); }
            pad.noteCount=bytes[at++]; pad.inversion=bytes[at++]; pad.octave=static_cast<int>(bytes[at++])-2;
            for (int& offset : pad.offsets) offset=static_cast<int>(bytes[at++])-24;
        }
        if (!candidate.valid()) return false;
        destination=candidate; return true;
    }
};
}
