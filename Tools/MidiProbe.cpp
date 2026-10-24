#include <juce_audio_devices/juce_audio_devices.h>
#include "MidiCaptureStats.h"
#include <array>
#include <atomic>
#include <iostream>
#include <mutex>

namespace {
juce::var deviceList(const juce::Array<juce::MidiDeviceInfo>& devices) {
    juce::Array<juce::var> result;
    for (const auto& device : devices) {
        auto* item = new juce::DynamicObject();
        item->setProperty("name",device.name);
        item->setProperty("identifier",device.identifier);
        result.add(juce::var(item));
    }
    return result;
}
void print(const juce::var& report) { std::cout << juce::JSON::toString(report).toStdString() << '\n'; }
int fail(const juce::String& message) {
    auto* report = new juce::DynamicObject();
    report->setProperty("error",message); print(juce::var(report)); return 1;
}
class Capture final : public juce::MidiInputCallback {
public:
    void handleIncomingMidiMessage(juce::MidiInput*,const juce::MidiMessage& message) override {
        if (message.getRawDataSize() < 1) return;
        const double time = juce::Time::getMillisecondCounterHiRes()/1000.0;
        const std::lock_guard<std::mutex> guard(lock);
        ++messages;
        const auto* data = message.getRawData();
        const int status = data[0];
        stats.observe(status,time);
        if (message.isNoteOn()) ++noteOns;
        if (message.isNoteOff()) ++noteOffs;
        if (message.isPitchWheel()) ++bends;
        // Store short messages only; never copy long SysEx payloads in a callback.
        if (previewCount < preview.size() && message.getRawDataSize() <= 3) {
            auto& event = preview[previewCount++];
            event.time = time; event.size = message.getRawDataSize();
            for (int i = 0; i < event.size; ++i) event.bytes[static_cast<std::size_t>(i)] = data[i];
        }
    }
    juce::var report(const juce::String& identifier,double duration) {
        const std::lock_guard<std::mutex> guard(lock);
        auto* report = new juce::DynamicObject();
        report->setProperty("mode","receive-only");
        report->setProperty("inputIdentifier",identifier);
        report->setProperty("durationSeconds",duration);
        report->setProperty("messages",static_cast<juce::int64>(messages));
        report->setProperty("noteOns",static_cast<juce::int64>(noteOns));
        report->setProperty("noteOffs",static_cast<juce::int64>(noteOffs));
        report->setProperty("pitchBends",static_cast<juce::int64>(bends));
        report->setProperty("clockPulses",static_cast<juce::int64>(stats.clocks));
        report->setProperty("starts",static_cast<juce::int64>(stats.starts));
        report->setProperty("stops",static_cast<juce::int64>(stats.stops));
        report->setProperty("validClockIntervals",static_cast<juce::int64>(stats.intervals));
        report->setProperty("nonpositiveClockIntervals",static_cast<juce::int64>(stats.nonpositiveIntervals));
        report->setProperty("clockMeanIntervalMs",stats.mean*1000);
        report->setProperty("clockMinIntervalMs",stats.minimum*1000);
        report->setProperty("clockMaxIntervalMs",stats.maximum*1000);
        report->setProperty("clockIntervalStddevMs",stats.stddev()*1000);
        report->setProperty("estimatedBpm",stats.estimatedBpm());
        report->setProperty("timingBasis","local callback arrival times; batching and OS scheduling affect results");
        report->setProperty("hardwareValidated",false);
        juce::Array<juce::var> events;
        for (std::size_t n = 0; n < previewCount; ++n) {
            auto* event = new juce::DynamicObject();
            event->setProperty("arrivalSeconds",preview[n].time);
            juce::Array<juce::var> bytes;
            for (int i = 0; i < preview[n].size; ++i) bytes.add(preview[n].bytes[static_cast<std::size_t>(i)]);
            event->setProperty("bytes",juce::var(bytes)); events.add(juce::var(event));
        }
        report->setProperty("shortMessagePreview",juce::var(events));
        report->setProperty("previewLimit",static_cast<int>(preview.size()));
        return juce::var(report);
    }
private:
    struct Event { double time = 0; int size = 0; std::array<int,3> bytes {}; };
    std::mutex lock;
    e4::MidiCaptureStats stats;
    std::array<Event,256> preview {};
    std::size_t previewCount = 0;
    std::uint64_t messages = 0,noteOns = 0,noteOffs = 0,bends = 0;
};
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI initialiser; // CoreMIDI discovery requires a JUCE message thread.
    if (argc == 1 || (argc == 2 && juce::String(argv[1]) == "--list")) {
        auto* report = new juce::DynamicObject();
        report->setProperty("inputs",deviceList(juce::MidiInput::getAvailableDevices()));
        report->setProperty("outputs",deviceList(juce::MidiOutput::getAvailableDevices()));
        report->setProperty("mode","list-only: no ports opened and no messages sent");
        report->setProperty("note","Empty arrays can mean CoreMIDI access is restricted; verify in a normal Terminal before assuming no devices are connected.");
        print(juce::var(report)); return 0;
    }
    if (argc == 2 && juce::String(argv[1]) == "--help") {
        std::cout << "e4_midi_probe --list\ne4_midi_probe --capture INPUT_IDENTIFIER --seconds 1..600\n"
                     "Capture is receive-only. JSON goes to standard output. No MIDI output is opened.\n";
        return 0;
    }
    if (argc != 5 || juce::String(argv[1]) != "--capture" || juce::String(argv[3]) != "--seconds")
        return fail("Use --list or --capture INPUT_IDENTIFIER --seconds 1..600");
    const juce::String secondsText(argv[4]);
    if (secondsText.isEmpty() || !secondsText.containsOnly("0123456789")) return fail("Seconds must be an integer in 1..600");
    const auto seconds = secondsText.getLargeIntValue();
    if (seconds < 1 || seconds > 600) return fail("Seconds must be an integer in 1..600");
    const juce::String identifier(argv[2]);
    int matches = 0;
    for (const auto& device : juce::MidiInput::getAvailableDevices()) if (device.identifier == identifier) ++matches;
    if (matches != 1) return fail("Input identifier is absent or ambiguous; run --list again");
    Capture capture;
    auto input = juce::MidiInput::openDevice(identifier,&capture);
    if (!input) return fail("Could not open selected MIDI input");
    const double begin = juce::Time::getMillisecondCounterHiRes();
    input->start();
    juce::Thread::sleep(static_cast<int>(seconds*1000));
    input->stop(); input.reset(); // Quiesce callbacks before serialising the result.
    print(capture.report(identifier,(juce::Time::getMillisecondCounterHiRes()-begin)/1000.0));
    return 0;
}
