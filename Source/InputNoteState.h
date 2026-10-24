#pragma once
#include <array>
#include <cstdint>
#include <limits>
namespace e4 {
// Incoming-key display only. Never generates or routes MIDI.
class InputNoteState {
public:
    void process(int status,int note,int velocity) noexcept {
        const int kind = status & 0xf0;
        if (status < 0x80 || status > 0x9f || (kind != 0x80 && kind != 0x90)
            || note < 0 || note > 127 || velocity < 0 || velocity > 127) return;
        auto& count = held[static_cast<std::size_t>(status & 15)][static_cast<std::size_t>(note)];
        if (kind == 0x90 && velocity > 0) {
            if (count != std::numeric_limits<std::uint32_t>::max()) ++count;
            masks[static_cast<std::size_t>(note/64)] |= std::uint64_t{1} << (note%64);
        } else if (count != 0) {
            --count;
            bool any = false;
            for (const auto& channel : held) if (channel[static_cast<std::size_t>(note)] != 0) { any = true; break; }
            if (!any) masks[static_cast<std::size_t>(note/64)] &= ~(std::uint64_t{1} << (note%64));
        }
    }
    void clear() noexcept { for (auto& channel : held) channel.fill(0); masks.fill(0); }
    std::uint64_t low() const noexcept { return masks[0]; }
    std::uint64_t high() const noexcept { return masks[1]; }
    bool isDown(int note) const noexcept { return note >= 0 && note < 128
        && (masks[static_cast<std::size_t>(note/64)] & (std::uint64_t{1} << (note%64))) != 0; }
private:
    std::array<std::array<std::uint32_t,128>,16> held {};
    std::array<std::uint64_t,2> masks {};
};
}
