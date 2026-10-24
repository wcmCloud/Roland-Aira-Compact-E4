#pragma once
#include "PerformanceSettings.h"
#include <atomic>
#include <mutex>
namespace e4 {
// Writers may be UI or host-state threads. The audio reader never locks/retries.
template <typename Settings,typename Codec> class ValueExchange {
public:
    ValueExchange() { publish(Settings{}); }
    bool publish(const Settings& next) {
        typename Codec::Bytes encoded;
        if (!Codec::encode(next,encoded)) return false;
        const std::lock_guard<std::mutex> lock(writers);
        serial.fetch_add(1); // Odd while publishing. Sequential consistency across bytes.
        for (std::size_t i=0;i<encoded.size();++i) bytes[i].store(encoded[i]);
        current=next;
        serial.fetch_add(1);
        return true;
    }
    Settings get() const { const std::lock_guard<std::mutex> lock(writers); return current; }
    bool consume(Settings& result, std::uint64_t& seen) const noexcept {
        const auto before=serial.load();
        if ((before & 1) != 0 || before == seen) return false;
        typename Codec::Bytes encoded;
        for (std::size_t i=0;i<encoded.size();++i) encoded[i]=bytes[i].load();
        if (serial.load() != before || !Codec::decode(encoded.data(),encoded.size(),result)) return false;
        seen=before; return true;
    }
private:
    mutable std::mutex writers;
    Settings current;
    std::array<std::atomic<std::uint8_t>,Codec::size> bytes {};
    std::atomic<std::uint64_t> serial {0};
};
using SettingsExchange = ValueExchange<PerformanceSettings,PerformancePreset>;
static_assert(std::atomic<std::uint8_t>::is_always_lock_free && std::atomic<std::uint64_t>::is_always_lock_free,
              "Settings publication requires lock-free atomics");
}
