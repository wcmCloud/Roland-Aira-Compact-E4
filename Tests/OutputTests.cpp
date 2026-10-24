#include "MidiOutputDriver.h"
#include <array>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
using Event = std::array<int,4>;
int main() {
    int failures = 0;
    const auto check = [&failures](bool ok,const char* label) { if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; } };
    std::vector<Event> events;
    const auto send = [&events](int status,int first,int second,int size) { events.push_back({status,first,second,size}); };
    e4::MidiOutputDriver driver;
    driver.post({e4::OutputCommand::midi,0x92,60,100,120});
    check(events.empty(),"posting from UI never sends MIDI directly");
    driver.service(0,0,send);
    check(events == std::vector<Event>{{0x92,60,100,3}},"worker delivers queued note");
    driver.post({e4::OutputCommand::midi,0x82,60,0,120});
    driver.post({e4::OutputCommand::midi,0xe2,127,127,120});
    driver.service(0,0,send);
    check(events[1] == Event{0x82,60,0,3} && events[2] == Event{0xe2,127,127,3},"note release and bend keep producer order");
    events.clear();
    driver.shutdown(send);
    check(events == std::vector<Event>{{0xe2,0,64,3}},"shutdown centres channels actually bent");
    check(!driver.post({e4::OutputCommand::midi,0x92,60,100,120}),"closed sender refuses new events");
    driver.shutdown(send);
    check(events.size() == 1,"shutdown cleanup is idempotent");

    e4::MidiOutputDriver close;
    events.clear();
    close.post({e4::OutputCommand::midi,0x9f,0,100,120});
    close.post({e4::OutputCommand::midi,0x9f,127,100,120});
    close.post({e4::OutputCommand::start,0,0,0,120});
    close.service(0,0,send);
    check(close.clockRunning(),"clock shares worker transport");
    close.post({e4::OutputCommand::midi,0x9f,64,100,120}); // Not sent: must not play while closing.
    events.clear(); close.shutdown(send);
    check(events == std::vector<Event>{{0xfc,0,0,1},{0x8f,0,0,3},{0x8f,127,0,3},{0xef,0,64,3}},
          "shutdown drops pending presses, stops clock and releases sent pitches on original channel");

    e4::MidiOutputDriver overload;
    events.clear();
    overload.post({e4::OutputCommand::midi,0x92,60,100,120});
    overload.post({e4::OutputCommand::start,0,0,0,120});
    overload.service(0,0,send);
    bool rejected = false;
    for (int i = 0; i < 1025; ++i)
        if (!overload.post({e4::OutputCommand::midi,0x92,70,100,120})) rejected = true;
    events.clear(); overload.service(0.001,0.001,send);
    check(rejected && overload.takeFault() == e4::OutputFault::queueOverflow && !overload.isAccepting(),"output overflow seals route for explicit reconnect");
    check(events == std::vector<Event>{{0xfc,0,0,1},{0x82,60,0,3},{0xe2,0,64,3}},"overflow drops queued note-ons and cleans sent notes");
    overload.service(0.002,0.002,send);
    check(events.size() == 3 && overload.takeFault() == e4::OutputFault::none,"fault cleanup is emitted once");

    e4::MidiOutputDriver delayed;
    events.clear();
    delayed.post({e4::OutputCommand::start,0,0,0,120});
    delayed.post({e4::OutputCommand::midi,0x93,64,100,120});
    delayed.service(0,0,send);
    events.clear(); delayed.service(0.1,0.1,send);
    check(delayed.takeFault() == e4::OutputFault::clockDeadline && !delayed.isAccepting()
        && events == std::vector<Event>{{0xfc,0,0,1},{0x83,64,0,3},{0xe3,0,64,3}},"missed clock deadline stops transport and releases notes");

    e4::MidiOutputDriver stale;
    events.clear(); stale.post({e4::OutputCommand::midi,0x90,60,100,120}); stale.service(0,0,send);
    events.clear(); stale.service(2.01,0,send);
    check(stale.takeFault() == e4::OutputFault::heartbeat && events == std::vector<Event>{{0x80,60,0,3},{0xe0,0,64,3}},
          "UI heartbeat timeout releases notes even when clock is off");
    e4::MidiOutputDriver resume;
    events.clear(); resume.post({e4::OutputCommand::midi,0x90,60,100,120}); resume.service(3,0,send);
    check(resume.takeFault() == e4::OutputFault::heartbeat && events.empty(),"stale queued notes do not play after UI pause");

    e4::MidiOutputDriver stop;
    events.clear(); stop.post({e4::OutputCommand::start,0,0,0,60}); stop.service(0,0,send);
    stop.post({e4::OutputCommand::stop}); stop.post({e4::OutputCommand::stop}); stop.service(0.001,0.001,send);
    check(events == std::vector<Event>{{0xfa,0,0,1},{0xf8,0,0,1},{0xfc,0,0,1}},"queued Start and duplicate Stop have exact realtime ordering");
    stop.post({e4::OutputCommand::midi,0xb0,123,0,120}); stop.service(0.002,0.002,send);
    check(events.size() == 3,"unsupported CC cannot reach physical sender");

    e4::MidiOutputDriver panic;
    events.clear();
    panic.post({e4::OutputCommand::midi,0x94,60,100,120});
    panic.post({e4::OutputCommand::start,0,0,0,120}); panic.service(0,0,send);
    for (int i = 0; i < 100; ++i) panic.post({e4::OutputCommand::midi,0x94,64,100,120});
    events.clear(); panic.requestPanic(); panic.service(0.001,0.001,send);
    check(events == std::vector<Event>{{0xfc,0,0,1},{0x84,60,0,3},{0xe4,0,64,3}}
        && panic.isAccepting() && panic.takeFault() == e4::OutputFault::none,
          "priority panic drops pending presses and releases sent notes without closing route");
    events.clear(); panic.post({e4::OutputCommand::midi,0x94,67,100,120}); panic.service(0.002,0.002,send);
    check(events == std::vector<Event>{{0x94,67,100,3}},"fresh input resumes after priority panic");
    panic.shutdown(send);

    // Exercise the actual SPSC boundary with separate producer and worker threads.
    e4::MidiOutputDriver concurrent;
    std::atomic<int> consumed {0};
    std::atomic<bool> done {false};
    std::vector<Event> captured;
    captured.reserve(20000);
    const auto capture = [&](int s,int a,int b,int size) {
        captured.push_back({s,a,b,size}); consumed.fetch_add(1,std::memory_order_release);
    };
    bool postFailed = false;
    std::thread producer([&] {
        for (int i = 0; i < 20000; ++i) {
            while (i-consumed.load(std::memory_order_acquire) >= 32) std::this_thread::yield();
            if (!concurrent.post({e4::OutputCommand::midi,(i%2 == 0 ? 0x90 : 0x80),(i/2)%128,(i%2 == 0 ? 100 : 0),120})) {
                postFailed = true; break;
            }
        }
        done.store(true,std::memory_order_release);
    });
    while (!done.load(std::memory_order_acquire) || concurrent.hasPending()) {
        concurrent.service(0,0,capture); std::this_thread::yield();
    }
    producer.join(); concurrent.shutdown(capture);
    check(!postFailed && captured.size() == 20000,"concurrent producer/consumer loses no events");
    bool ordered = captured.size() == 20000;
    for (std::size_t i = 0; i < captured.size() && ordered; ++i)
        ordered = captured[i] == Event{(i%2 == 0 ? 0x90 : 0x80),static_cast<int>((i/2)%128),(i%2 == 0 ? 100 : 0),3};
    check(ordered,"concurrent sender preserves note-on/off order");
    std::cout << "MIDI sender: " << failures << " failures\n";
    return failures ? 1 : 0;
}
