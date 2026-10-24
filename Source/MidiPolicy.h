#pragma once
#include <cstdint>
namespace e4 {
// Deliberately excludes CC, SysEx, program change, Continue and active sensing.
constexpr bool supportedStatus(std::uint8_t status) noexcept {
    const auto kind = status & 0xf0;
    return kind == 0x80 || kind == 0x90 || kind == 0xe0
        || status == 0xf8 || status == 0xfa || status == 0xfc;
}
constexpr int defaultChannel = 3;
constexpr int maxVoices = 4;
}
