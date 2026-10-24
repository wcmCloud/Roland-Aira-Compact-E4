#pragma once
#include "MidiEngine.h"
#include <array>
#include <cstddef>

namespace e4 {
// MIDI/UI thread only. Input callbacks enqueue under the caller's short lock.
struct MidiEvent { int status = 0, first = 0, second = 0; };
class MidiEventQueue {
public:
    void push(MidiEvent event) noexcept {
        if (count == events.size()) { overflow = true; return; }
        events[count++] = event;
    }
    // On overflow drop the entire burst; caller must panic before accepting more.
    bool drain(std::array<MidiEvent, 256>& dest, std::size_t& size) noexcept {
        size = overflow ? 0 : count;
        for (std::size_t i = 0; i < size; ++i) dest[i] = events[i];
        const bool healthy = !overflow;
        clear();
        return healthy;
    }
    void clear() noexcept { count = 0; overflow = false; }
private:
    std::array<MidiEvent, 256> events {};
    std::size_t count = 0;
    bool overflow = false;
};

class MidiSink {
public:
    virtual ~MidiSink() = default;
    virtual void send(int status, int first, int second) = 0;
};
class MidiSession {
    auto emitter() { return [this](int status, int first, int second) {
        if (sink != nullptr) sink->send(status, first, second);
    }; }
public:
    // Call while both sinks are still alive. Releases always go to the old sink.
    void setOutput(MidiSink* next) {
        panic();
        sink = next;
        if (sink != nullptr) panic(); // Centre bend, never replay held notes.
    }
    void panic() { engine.panic(emitter()); }
    void setChannel(int channel) { engine.setChannel(channel, emitter()); }
    void setTranspose(int semitones) { engine.setTranspose(semitones,emitter()); }
    void process(MidiEvent event, int source = 0) {
        if (sink != nullptr) engine.process(event.status, event.first, event.second, emitter(), source);
    }
    int activeVoices() const noexcept { return engine.activeVoices(); }
    int channel() const noexcept { return engine.outputChannel(); }
private:
    MidiEngine engine;
    MidiSink* sink = nullptr;
};
}
