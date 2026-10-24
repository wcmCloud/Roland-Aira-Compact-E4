#pragma once
#include "MidiPolicy.h"
#include <array>
#include <cstdint>

namespace e4 {
// Audio-thread owned. The caller supplies a bounded, non-allocating event sink.
class MidiEngine {
public:
    enum class Rejection { none, voiceLimit, outOfRange, counterOverflow };
    template <typename Emit> void panic(Emit&& emit) noexcept {
        for (int note = 0; note < 128; ++note)
            if (voices[static_cast<std::size_t>(note)] != 0)
                emit(0x80 | (channel - 1), note, 0);
        emit(0xe0 | (channel - 1), 0, 64);
        voices.fill(0);
        for (auto& input : held) input.fill(0);
        for (auto& input : rejected) input.fill(0);
        active = 0;
    }
    template <typename Emit> void setChannel(int next, Emit&& emit) noexcept {
        if (next < 1 || next > 16 || next == channel) return;
        panic(emit); // Releases must use the old channel.
        channel = next;
    }
    template <typename Emit> void setTranspose(int next, Emit&& emit) noexcept {
        if (next < -24 || next > 24 || next == transpose) return;
        panic(emit); // Releases use the pitches that were actually sent.
        transpose = next;
    }
    template <typename Emit> void process(int status, int data1, int data2, Emit&& emit, int source = 0) noexcept {
        if (status < 0x80 || status > 0xef || data1 < 0 || data1 > 127
            || data2 < 0 || data2 > 127 || source < 0 || source > 5) return;
        const int kind = status & 0xf0;
        if (kind == 0xe0) { emit(0xe0 | (channel - 1), data1, data2); return; }
        if (kind != 0x80 && kind != 0x90) return;
        const int outputNote = data1 + transpose;
        if (outputNote < 0 || outputNote > 127) {
            if (kind == 0x90 && data2 != 0) reject(Rejection::outOfRange);
            return;
        } // Reject, do not clamp distinct notes together.
        auto& owner = held[static_cast<std::size_t>((status & 15) + source * 16)][static_cast<std::size_t>(data1)];
        auto& ignored = rejected[static_cast<std::size_t>((status & 15) + source * 16)][static_cast<std::size_t>(data1)];
        auto& references = voices[static_cast<std::size_t>(outputNote)];
        if (kind == 0x90 && data2 != 0) {
            // Keep rejected input holds until their releases drain. Otherwise a
            // late release from a fifth voice can silence a later accepted press.
            if (ignored != 0 || (references == 0 && active == maxVoices)) {
                reject(Rejection::voiceLimit);
                if (ignored == UINT16_MAX) { reject(Rejection::counterOverflow); panic(emit); }
                else ++ignored;
                return;
            }
            if (owner == UINT16_MAX) { reject(Rejection::counterOverflow); panic(emit); return; }
            ++owner;
            if (references++ == 0) { ++active; emit(0x90 | (channel - 1), outputNote, data2); }
        } else if (ignored != 0) {
            --ignored;
        } else if (owner != 0) {
            --owner;
            if (--references == 0) { --active; emit(0x80 | (channel - 1), outputNote, data2); }
        }
    }
    template <typename Emit> void releaseSource(int source, Emit&& emit) noexcept {
        if (source < 0 || source > 5) return;
        for (int inputChannel = 0; inputChannel < 16; ++inputChannel)
            for (int note = 0; note < 128; ++note) {
                auto& owner = held[static_cast<std::size_t>(source * 16 + inputChannel)][static_cast<std::size_t>(note)];
                rejected[static_cast<std::size_t>(source * 16 + inputChannel)][static_cast<std::size_t>(note)] = 0;
                if (owner == 0) continue;
                const int outputNote = note + transpose; // Only in-range input notes can be owned.
                auto& references = voices[static_cast<std::size_t>(outputNote)];
                references -= owner;
                owner = 0;
                if (references == 0) { --active; emit(0x80 | (channel - 1),outputNote,0); }
            }
    }
    std::uint32_t rejectionCount() const noexcept { return rejectionSerial; }
    Rejection lastRejection() const noexcept { return rejection; }
    int activeVoices() const noexcept { return active; }
    int semitoneOffset() const noexcept { return transpose; }
    int outputChannel() const noexcept { return channel; }
private:
    void reject(Rejection reason) noexcept { rejection = reason; ++rejectionSerial; }
    Rejection rejection = Rejection::none;
    std::uint32_t rejectionSerial = 0;
    std::array<std::array<std::uint16_t, 128>, 96> held {};
    std::array<std::array<std::uint16_t, 128>, 96> rejected {};
    std::array<std::uint32_t, 128> voices {};
    int channel = defaultChannel;
    int active = 0;
    int transpose = 0;
};
}
