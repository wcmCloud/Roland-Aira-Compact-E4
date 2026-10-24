#pragma once
#include <cmath>
#include <cstdint>
namespace e4 {
// Arrival-time diagnostics, not a hardware transmission timestamp guarantee.
class MidiCaptureStats {
public:
    void observe(int status,double seconds) noexcept {
        if (!std::isfinite(seconds)) { ++invalidTimes; return; }
        if (status == 0xfa) { ++starts; previous = -1; return; }
        if (status == 0xfc) { ++stops; previous = -1; return; }
        if (status != 0xf8) return;
        ++clocks;
        if (previous >= 0) {
            const double interval = seconds - previous;
            if (interval <= 0) ++nonpositiveIntervals;
            else {
                ++intervals;
                if (intervals == 1 || interval < minimum) minimum = interval;
                if (interval > maximum) maximum = interval;
                const double delta = interval - mean;
                mean += delta / static_cast<double>(intervals);
                varianceSum += delta * (interval - mean);
            }
        }
        previous = seconds;
    }
    double stddev() const noexcept { return intervals > 1 ? std::sqrt(varianceSum/static_cast<double>(intervals-1)) : 0; }
    double estimatedBpm() const noexcept { return intervals > 0 ? 60.0/(24.0*mean) : 0; }
    std::uint64_t clocks = 0, starts = 0, stops = 0, intervals = 0;
    std::uint64_t nonpositiveIntervals = 0,invalidTimes = 0;
    double minimum = 0,maximum = 0,mean = 0;
private:
    double previous = -1,varianceSum = 0;
};
}
