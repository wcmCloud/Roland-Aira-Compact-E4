#pragma once
#include <array>
#include <atomic>
#include <cstddef>
namespace e4 {
struct PerformanceCommand {
    enum Kind { noteOn, noteOff, padDown, padUp, bend } kind = noteOff;
    int value = 0;
    int velocity = 100;
};
// Single producer (editor/message thread), single consumer (audio thread).
class PerformanceQueue {
public:
    bool push(PerformanceCommand command) noexcept {
        const auto current = write.load(std::memory_order_relaxed);
        const auto next = (current + 1) % storage.size();
        if (next == read.load(std::memory_order_acquire)) {
            overflow.store(true,std::memory_order_release); return false;
        }
        storage[current] = command;
        write.store(next,std::memory_order_release);
        return true;
    }
    bool pop(PerformanceCommand& command) noexcept {
        const auto current = read.load(std::memory_order_relaxed);
        if (current == write.load(std::memory_order_acquire)) return false;
        command = storage[current];
        read.store((current + 1) % storage.size(),std::memory_order_release);
        return true;
    }
    void discard() noexcept {
        PerformanceCommand unused;
        for (int i = 0; i < 256 && pop(unused); ++i) {}
    }
    bool consumeOverflow() noexcept { return overflow.exchange(false,std::memory_order_acq_rel); }
private:
    std::array<PerformanceCommand,257> storage {};
    std::atomic<std::size_t> read {0},write {0};
    std::atomic<bool> overflow {false};
};
}
