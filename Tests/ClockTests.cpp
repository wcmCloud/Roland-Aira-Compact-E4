#include "ClockScheduler.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
int main() {
    int failures = 0;
    const auto check = [&failures](bool ok,const char* label) {
        if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
    };
    std::vector<int> sent;
    const auto emit = [&sent](int status) { sent.push_back(status); };
    e4::ClockScheduler clock;
    clock.poll(10,emit); clock.stop(emit);
    check(sent.empty() && !clock.isRunning(),"clock starts silent and Stop is idempotent");
    clock.start(10,emit);
    check(sent == std::vector<int>{0xfa,0xf8},"Start precedes first clock");
    clock.start(11,emit);
    check(sent.size() == 2,"duplicate Start cannot reset a running clock");
    clock.stop(emit); clock.stop(emit);
    check(sent == std::vector<int>{0xfa,0xf8,0xfc},"Stop is emitted exactly once");
    clock.poll(100,emit);
    check(sent.size() == 3,"no clock after stop");
    clock.start(100,emit);
    check(sent[3] == 0xfa && sent[4] == 0xf8,"resume restarts without Continue or song position");
    clock.poll(102,emit);
    check(clock.hasFault() && !clock.isRunning() && sent.back() == 0xfc,"long delay stops instead of clock burst");
    const auto count = sent.size(); clock.poll(103,emit);
    check(sent.size() == count,"missed deadline stop is not repeated");
    clock.start(110,emit);
    check(clock.isRunning() && !clock.hasFault(),"explicit restart clears fault");
    clock.poll(109,emit);
    check(clock.hasFault() && !clock.isRunning(),"backward clock jump stops transport");
    check(!clock.setTempo(0,0) && !clock.setTempo(301,0)
        && !clock.setTempo(std::numeric_limits<double>::quiet_NaN(),0),"invalid tempos rejected");
    e4::ClockScheduler tempo;
    tempo.start(0,emit);
    check(tempo.setTempo(60,0.010),"tempo change accepted");
    check(std::abs(tempo.nextDeadline() - (0.010 + (60.0/(24*120)-0.010)*2)) < 1e-12,
          "tempo change preserves remaining fractional pulse phase");
    for (double bpm : {60.0,120.0,180.0}) {
        e4::ClockScheduler simulation;
        simulation.setTempo(bpm,0);
        int clocks = 0,starts = 0,stops = 0;
        const auto capture = [&](int status) { if (status == 0xf8) ++clocks; if (status == 0xfa) ++starts; if (status == 0xfc) ++stops; };
        simulation.start(0,capture);
        for (int millisecond = 1; millisecond < 600000; ++millisecond)
            simulation.poll(static_cast<double>(millisecond)/1000.0,capture);
        const int expected = static_cast<int>(bpm * 24 * 10);
        check(clocks == expected && starts == 1 && stops == 0 && !simulation.hasFault(),
              "ten minute simulated 1ms service has exact pulse count");
        check(std::abs(simulation.nextDeadline()-600.0) < 1e-6,"ten minute accumulated phase drift below one microsecond in simulation");
        std::cout << "Simulated " << bpm << " BPM: " << clocks << " pulses / 10 minutes\n";
    }
    e4::ClockScheduler exact;
    exact.setTempo(300,0);
    int fastCount = 0;
    const auto fastEmit = [&fastCount](int status) { if (status == 0xf8) ++fastCount; };
    exact.start(0,fastEmit);
    for (int n = 0; n < 10000; ++n) exact.poll(exact.nextDeadline(),fastEmit);
    check(fastCount == 10001 && !exact.hasFault(),"maximum tempo at exact deadlines");
    exact.poll(std::numeric_limits<double>::infinity(),fastEmit);
    check(!exact.isRunning() && exact.hasFault(),"invalid timer value fails closed");
    for (int status : sent) check(status == 0xfa || status == 0xf8 || status == 0xfc,"only documented realtime messages emitted");
    std::cout << "Clock planner: " << failures << " failures\n";
    return failures ? 1 : 0;
}
