#pragma once
#include <algorithm>
#include <cmath>

namespace e4 {
// Single-threaded deterministic planner. Times are monotonic seconds.
// No Continue/SPP and no catch-up burst after a missed pulse.
class ClockScheduler {
public:
    bool setTempo(double bpm, double now) noexcept {
        if (!std::isfinite(bpm) || bpm < 20 || bpm > 300 || !std::isfinite(now)) return false;
        const double newPeriod = 60.0 / (24.0 * bpm);
        if (running) {
            const double phaseRemaining = std::clamp((next - now) / period,0.0,1.0);
            next = now + phaseRemaining * newPeriod;
        }
        period = newPeriod;
        return true;
    }
    template <typename Send> void start(double now, Send&& send) {
        if (running || !std::isfinite(now)) return;
        fault = false;
        running = true;
        next = now;
        send(0xfa);
        poll(now,send); // First clock at the new origin, following Start.
    }
    template <typename Send> void stop(Send&& send) {
        if (running) send(0xfc);
        running = false;
    }
    template <typename Send> void poll(double now, Send&& send) {
        if (!running) return;
        if (!std::isfinite(now) || now < next - period || now >= next + period) {
            stop(send); fault = true; return;
        }
        if (now >= next) { send(0xf8); next += period; }
    }
    bool isRunning() const noexcept { return running; }
    bool hasFault() const noexcept { return fault; }
    double nextDeadline() const noexcept { return next; }
private:
    double period = 60.0 / (24.0 * 120.0);
    double next = 0;
    bool running = false;
    bool fault = false;
};
}
