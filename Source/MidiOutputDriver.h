#pragma once
#include "ClockScheduler.h"
#include <array>
#include <atomic>
#include <bitset>
#include <cstddef>
namespace e4 {
enum class OutputFault { none, queueOverflow, clockDeadline, heartbeat };
struct OutputCommand {
    enum Kind { midi, start, stop, tempo } kind = midi;
    int status = 0,first = 0,second = 0;
    double bpm = 120;
};
// One message-thread producer, one device-thread consumer. No device API calls here.
class MidiOutputDriver {
public:
    bool post(OutputCommand command) noexcept {
        if (!accepting.load(std::memory_order_acquire)) return false;
        const auto current = write.load(std::memory_order_relaxed);
        const auto next = (current+1)%commands.size();
        if (next == read.load(std::memory_order_acquire)) {
            overflow.store(true,std::memory_order_release); return false;
        }
        commands[current] = command;
        write.store(next,std::memory_order_release);
        return true;
    }
    void requestPanic() noexcept { panicRequested.store(true,std::memory_order_release); }
    bool hasPending() const noexcept { return read.load(std::memory_order_acquire) != write.load(std::memory_order_acquire); }
    bool clockRunning() const noexcept { return running.load(std::memory_order_acquire); }
    bool isAccepting() const noexcept { return accepting.load(std::memory_order_acquire); }
    OutputFault takeFault() noexcept { return fault.exchange(OutputFault::none,std::memory_order_acq_rel); }

    // Device-thread only. Sender receives status/data/size, including one-byte realtime.
    template <typename Send> void service(double now,double lastHeartbeat,Send&& send) {
        if (!isAccepting()) { discard(); return; }
        if (!std::isfinite(now) || !std::isfinite(lastHeartbeat)) { fail(OutputFault::heartbeat,send); return; }
        if (overflow.exchange(false,std::memory_order_acq_rel)) { fail(OutputFault::queueOverflow,send); return; }
        if (panicRequested.exchange(false,std::memory_order_acq_rel)) { discard(); cleanup(send); return; }
        if (now-lastHeartbeat > 2.0 && (clock.isRunning() || activeNotes != 0 || hasPending())) {
            fail(OutputFault::heartbeat,send); return;
        }
        clock.poll(now,[&send](int status) { send(status,0,0,1); });
        if (clock.hasFault()) { fail(OutputFault::clockDeadline,send); return; }
        OutputCommand command;
        for (int budget = 0; budget < 64 && pop(command); ++budget) {
            if (overflow.load(std::memory_order_acquire)) { fail(OutputFault::queueOverflow,send); return; }
            if (panicRequested.exchange(false,std::memory_order_acq_rel)) { discard(); cleanup(send); return; }
            switch (command.kind) {
                case OutputCommand::midi: sendMidi(command,send); break;
                case OutputCommand::start:
                    if (!clock.isRunning() && clock.setTempo(command.bpm,now))
                        clock.start(now,[&send](int status) { send(status,0,0,1); });
                    break;
                case OutputCommand::stop: clock.stop([&send](int status) { send(status,0,0,1); }); break;
                case OutputCommand::tempo: clock.setTempo(command.bpm,now); break;
            }
        }
        running.store(clock.isRunning(),std::memory_order_release);
    }
    template <typename Send> void shutdown(Send&& send) {
        accepting.store(false,std::memory_order_release);
        discard(); cleanup(send);
    }
private:
    bool pop(OutputCommand& command) noexcept {
        const auto current = read.load(std::memory_order_relaxed);
        if (current == write.load(std::memory_order_acquire)) return false;
        command = commands[current];
        read.store((current+1)%commands.size(),std::memory_order_release); return true;
    }
    void discard() noexcept {
        OutputCommand unused;
        for (int i = 0; i < 1024 && pop(unused); ++i) {}
    }
    template <typename Send> void sendMidi(OutputCommand command,Send&& send) {
        const int kind = command.status & 0xf0;
        if (command.status < 0x80 || command.status > 0xef || command.first < 0 || command.first > 127
            || command.second < 0 || command.second > 127 || (kind != 0x80 && kind != 0x90 && kind != 0xe0)) return;
        send(command.status,command.first,command.second,3);
        const auto channel = static_cast<std::size_t>(command.status & 15);
        const auto note = static_cast<std::size_t>(command.first);
        if (kind == 0x90 && command.second > 0) {
            if (!sentNotes[channel].test(note)) ++activeNotes;
            sentNotes[channel].set(note);
        } else if (kind == 0x80 || kind == 0x90) {
            if (sentNotes[channel].test(note)) --activeNotes;
            sentNotes[channel].reset(note);
        } else bentChannels.set(channel);
    }
    template <typename Send> void cleanup(Send&& send) {
        clock.stop([&send](int status) { send(status,0,0,1); });
        for (std::size_t channel = 0; channel < sentNotes.size(); ++channel) {
            const bool owned = sentNotes[channel].any();
            for (int note = 0; note < 128; ++note)
                if (sentNotes[channel].test(static_cast<std::size_t>(note))) send(0x80|static_cast<int>(channel),note,0,3);
            if (owned || bentChannels.test(channel)) send(0xe0|static_cast<int>(channel),0,64,3);
            sentNotes[channel].reset();
        }
        bentChannels.reset(); activeNotes = 0;
        running.store(false,std::memory_order_release);
    }
    template <typename Send> void fail(OutputFault reason,Send&& send) {
        accepting.store(false,std::memory_order_release);
        discard(); cleanup(send);
        fault.store(reason,std::memory_order_release);
    }
    std::array<OutputCommand,1025> commands {};
    std::atomic<std::size_t> read {0},write {0};
    std::atomic<bool> overflow {false},accepting {true},running {false};
    std::atomic<bool> panicRequested {false};
    std::atomic<OutputFault> fault {OutputFault::none};
    std::array<std::bitset<128>,16> sentNotes {};
    std::bitset<16> bentChannels;
    int activeNotes = 0;
    ClockScheduler clock;
};
}
