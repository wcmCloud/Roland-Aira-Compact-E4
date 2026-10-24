#include "MidiEngine.h"
#include "InputNoteState.h"
#include "NoteNames.h"
#include "MidiSession.h"
#include "ChordPads.h"
#include "PerformanceQueue.h"
#include "SettingsExchange.h"
#include <thread>
#include <array>
#include <iostream>
#include <vector>

using Event = std::array<int, 3>;
int main() {
    int failures = 0;
    const auto check = [&failures](bool ok, const char* name) {
        if (!ok) { std::cerr << "FAIL: " << name << '\n'; ++failures; }
    };
    e4::MidiEngine engine;
    std::vector<Event> events;
    const auto emit = [&events](int s, int a, int b) { events.push_back({s,a,b}); };
    engine.process(0x90, 60, 100, emit);
    check(events == std::vector<Event>{{0x92,60,100}}, "default channel maps to 3");
    engine.process(0xb0, 123, 0, emit);
    engine.process(0x90, 60, 0, emit);
    check(events.back() == Event{0x82,60,0} && engine.activeVoices() == 0,
          "CC filtering preserves subsequent velocity-zero release");
    events.clear();
    engine.process(0x90, 64, 90, emit);
    engine.process(0x91, 64, 80, emit);
    engine.process(0x90, 64, 70, emit);
    engine.process(0x80, 64, 0, emit);
    engine.process(0x81, 64, 0, emit);
    check(events.size() == 1 && engine.activeVoices() == 1, "overlapping source ownership");
    engine.process(0x80, 64, 0, emit);
    check(events.size() == 2 && engine.activeVoices() == 0, "final owner releases note");
    events.clear();
    for (int note = 60; note < 65; ++note) engine.process(0x90, note, 100, emit);
    check(events.size() == 4 && engine.activeVoices() == 4, "reject fifth distinct note");
    engine.process(0x80, 64, 0, emit);
    check(events.size() == 4, "ignored note release does not silence accepted voices");
    engine.setChannel(16, emit);
    check(events.size() == 9 && events[4] == Event{0x82,60,0}
          && events[7] == Event{0x82,63,0} && events[8] == Event{0xe2,0,64},
          "channel change releases old channel and centres bend");
    check(engine.activeVoices() == 0 && engine.outputChannel() == 16, "channel switch resets ownership");
    events.clear();
    engine.process(0x9f, 0, 1, emit);
    engine.process(0x9f, 127, 127, emit);
    engine.process(0xef, 0, 0, emit);
    engine.process(0xef, 127, 127, emit);
    check(events.size() == 4 && events[0] == Event{0x9f,0,1}
          && events[1] == Event{0x9f,127,127} && events[2] == Event{0xef,0,0}
          && events[3] == Event{0xef,127,127}, "note and bend boundaries");
    engine.panic(emit);
    check(events[4] == Event{0x8f,0,0} && events[5] == Event{0x8f,127,0}
          && events[6] == Event{0xef,0,64}, "explicit note-off panic");
    events.clear();
    engine.panic(emit);
    engine.process(0x80, 0, 0, emit);
    check(events == std::vector<Event>{{0xef,0,64}}, "repeated panic and stale release are harmless");
    events.clear();
    for (const int status : {0xa0,0xb0,0xc0,0xd0,0xf0,0xf8,0xfa,0xfb,0xfc,0xfe})
        engine.process(status, 64, 127, emit);
    engine.process(0x90, -1, 1, emit);
    engine.process(0x90, 128, 1, emit);
    engine.process(0xe0, 0, 128, emit);
    engine.setChannel(0, emit);
    engine.setChannel(17, emit);
    check(events.empty() && engine.outputChannel() == 16, "invalid and unsupported events rejected");
    // Exercise repeated ownership and cleanup over many performance cycles.
    for (int cycle = 0; cycle < 1000; ++cycle) {
        for (int channel = 0; channel < 16; ++channel)
            for (int note = 60; note < 64; ++note) engine.process(0x90 | channel,note,100,emit);
        for (int channel = 15; channel >= 0; --channel)
            for (int note = 63; note >= 60; --note) engine.process(0x80 | channel,note,0,emit);
        check(engine.activeVoices() == 0, "stress cycle clears all owners");
        events.clear();
    }
    struct FakeSink final : e4::MidiSink {
        std::vector<Event> sent;
        void send(int status, int first, int second) override { sent.push_back({status,first,second}); }
    } oldPort, newPort;
    e4::MidiSession session;
    session.process({0x90,60,100});
    session.setOutput(&oldPort);
    check(session.activeVoices() == 0 && oldPort.sent == std::vector<Event>{{0xe2,0,64}},
          "disconnected notes never replay on connect");
    oldPort.sent.clear();
    session.process({0x90,60,100});
    session.process({0x90,60,100},1);
    session.process({0x80,60,0},1);
    check(oldPort.sent.size() == 1 && session.activeVoices() == 1,
          "on-screen keyboard cannot release matching external source note");
    session.setOutput(&newPort);
    check(oldPort.sent.size() == 3 && oldPort.sent[1] == Event{0x82,60,0}
          && newPort.sent == std::vector<Event>{{0xe2,0,64}} && session.activeVoices() == 0,
          "route switch cleans old endpoint and does not send held notes to new one");
    session.process({0x80,60,0});
    check(newPort.sent.size() == 1, "old route release ignored after switch");
    session.process({0x90,67,100},1);
    session.setChannel(5);
    check(newPort.sent[newPort.sent.size()-2] == Event{0x82,67,0}
          && session.channel() == 5, "session channel changes clean up original channel");
    session.process({0x90,69,100});
    session.setOutput(nullptr);
    check(newPort.sent[newPort.sent.size()-2] == Event{0x84,69,0}
          && session.activeVoices() == 0, "disconnect releases notes before endpoint closes");
    newPort.sent.clear();
    session.process({0x90,72,100},1);
    check(newPort.sent.empty(), "disconnected keyboard is silent");
    e4::MidiEventQueue queue;
    std::array<e4::MidiEvent,256> batch {};
    std::size_t size = 0;
    queue.push({0x90,60,100}); queue.push({0x80,60,0});
    check(queue.drain(batch,size) && size == 2 && batch[0].status == 0x90 && batch[1].status == 0x80,
          "input queue preserves release order");
    for (int n = 0; n < 257; ++n) queue.push({0x90,60,100});
    check(!queue.drain(batch,size) && size == 0, "overflow drops entire pending burst for panic recovery");
    queue.push({0x90,60,100}); queue.clear();
    check(queue.drain(batch,size) && size == 0, "route change clears stale queued input");
    queue.push({0x80,60,0});
    check(queue.drain(batch,size) && size == 1, "queue recovers after overflow");
    e4::ChordPads chords;
    newPort.sent.clear();
    session.setOutput(&newPort);
    session.setChannel(3);
    newPort.sent.clear();
    const auto chordSend = [&session](e4::MidiEvent event,int source) { session.process(event,source); };
    chords.setHeld(3,true,60,chordSend);
    check(newPort.sent == std::vector<Event>{{0x92,60,100},{0x92,64,100},{0x92,67,100},{0x92,71,100}},
          "major seventh pad plays four voices");
    chords.setHeld(3,true,62,chordSend);
    check(newPort.sent.size() == 4, "repeated down state does not retrigger chord");
    chords.setHeld(3,false,62,chordSend);
    check(newPort.sent.size() == 8 && newPort.sent[4] == Event{0x82,60,0}
          && newPort.sent[7] == Event{0x82,71,0}, "pad release uses original root snapshot");
    newPort.sent.clear();
    session.process({0x90,60,100});
    chords.setHeld(0,true,60,chordSend);
    chords.releaseAll(chordSend);
    check(session.activeVoices() == 1, "chord release preserves external held root");
    session.process({0x80,60,0});
    check(session.activeVoices() == 0, "external release clears last owner after chord");
    newPort.sent.clear();
    chords.setHeld(0,true,127,chordSend);
    chords.setHeld(-1,true,60,chordSend);
    check(newPort.sent.empty(), "invalid chord index and root do not generate invalid MIDI");
    for (int pad = 0; pad < 4; ++pad) chords.setHeld(pad,true,60,chordSend);
    check(session.activeVoices() <= 4, "overlapping pads obey global four voice limit");
    chords.releaseAll(chordSend);
    check(session.activeVoices() == 0, "all pad releases clean up overlapping chords");
    e4::PerformanceQueue commands;
    for (int n = 0; n < 256; ++n)
        check(commands.push({e4::PerformanceCommand::noteOn,n,100}),"command queue capacity");
    check(!commands.push({e4::PerformanceCommand::noteOff,60,0}) && commands.consumeOverflow(),
          "lost UI release raises overflow recovery flag");
    e4::PerformanceCommand command;
    for (int n = 0; n < 256; ++n)
        check(commands.pop(command) && command.value == n,"command FIFO order");
    check(!commands.pop(command) && !commands.consumeOverflow(),"queue is empty after draining and flag reset");
    commands.push({e4::PerformanceCommand::noteOn,60,100}); commands.discard();
    check(!commands.pop(command),"discard prevents stale editor note replay");
    e4::MidiEngine transposed;
    for (int shift : {-24,-12,0,12,24}) {
        transposed.setTranspose(shift,emit);
        for (int note = 0; note < 128; ++note) {
            events.clear();
            transposed.process(0x90,note,100,emit);
            transposed.process(0x80,note,0,emit);
            const int result = note + shift;
            if (result < 0 || result > 127) check(events.empty(),"out-of-range transposition is rejected, not clamped");
            else check(events == std::vector<Event>{{0x92,result,100},{0x82,result,0}},
                       "transpose maps both note-on and note-off across MIDI range");
            check(transposed.activeVoices() == 0,"transposed range sweep leaves no held notes");
        }
    }
    transposed.setTranspose(12,emit);
    transposed.process(0x90,60,100,emit);
    transposed.process(0x90,60,100,emit,1);
    events.clear();
    transposed.releaseSource(1,emit);
    check(events.empty() && transposed.activeVoices() == 1,"transposed UI release preserves external ownership");
    transposed.setTranspose(-12,emit);
    check(events == std::vector<Event>{{0x82,72,0},{0xe2,0,64}} && transposed.activeVoices() == 0,
          "transpose change releases original physical pitch before changing mapping");
    events.clear();
    transposed.process(0x80,60,0,emit);
    check(events.empty(),"old note release after transpose cleanup is ignored");
    transposed.process(0x90,60,100,emit,1);
    transposed.releaseSource(1,emit);
    check(events == std::vector<Event>{{0x92,48,100},{0x82,48,0}},"editor cleanup uses transposed pitch");
    events.clear();
    transposed.setTranspose(25,emit); transposed.setTranspose(-25,emit);
    check(events.empty() && transposed.semitoneOffset() == -12,"invalid transpose setting ignored");
    transposed.process(0xe0,127,127,emit);
    check(events == std::vector<Event>{{0xe2,127,127}},"transpose does not alter pitch bend encoding");
    e4::MidiEngine rejectedNotes;
    for (int note = 60; note < 64; ++note) rejectedNotes.process(0x90,note,100,emit);
    rejectedNotes.process(0x90,64,100,emit); // Fifth voice: rejected but still held by input.
    rejectedNotes.process(0x80,60,0,emit);
    events.clear();
    rejectedNotes.process(0x90,64,100,emit); // Repeated press before the rejected one was released.
    check(events.empty(),"rejected input cannot retrigger until its outstanding releases drain");
    rejectedNotes.process(0x80,64,0,emit);
    rejectedNotes.process(0x80,64,0,emit);
    rejectedNotes.process(0x90,64,100,emit);
    check(events == std::vector<Event>{{0x92,64,100}},"fresh press works after rejected holds release");
    rejectedNotes.process(0x80,64,0,emit);
    check(events.back() == Event{0x82,64,0},"rejected history does not steal a fresh note release");
    rejectedNotes.panic(emit);
    for (int note = 60; note < 64; ++note) rejectedNotes.process(0x90,note,100,emit);
    rejectedNotes.process(0x90,64,100,emit,1);
    rejectedNotes.process(0x80,60,0,emit);
    rejectedNotes.releaseSource(1,emit);
    events.clear();
    rejectedNotes.process(0x90,64,100,emit,1);
    check(events == std::vector<Event>{{0x92,64,100}},"source cleanup clears outstanding rejected UI holds");
    rejectedNotes.panic(emit);
    events.clear();
    for (int count = 0; count < 65536; ++count) rejectedNotes.process(0x90,60,100,emit);
    check(rejectedNotes.activeVoices() == 0 && events.back() == Event{0xe2,0,64},
          "ownership counter saturation performs fail-closed cleanup");
    for (int note = 60; note < 64; ++note) rejectedNotes.process(0x90,note,100,emit);
    for (int count = 0; count < 65536; ++count) rejectedNotes.process(0x90,64,100,emit);
    check(rejectedNotes.activeVoices() == 0,"rejected counter saturation also cleans up held notes");
    for (int note = 0; note < 128; ++note) {
        const auto parsed = e4::parseNote(e4::noteLabel(note));
        check(parsed && *parsed == note,"all displayed note labels round-trip to the original MIDI value");
    }
    check(e4::noteLabel(60) == "C4 (60)" && e4::parseNote("Db4") == 61 && e4::parseNote("c#4") == 61
        && e4::parseNote("C-1") == 0 && e4::parseNote("G9") == 127,"musical names, flats and MIDI endpoints");
    check(e4::parseNote(" 60 ") == 60 && e4::parseNote("B#3") == 60 && e4::parseNote("Cb4") == 59,
          "numeric entry and enharmonic spellings");
    for (auto invalid : {"", "C4 (61)", "C4junk", "H4", "128", "-1", "C10", "C99999999999", "()"})
        check(!e4::parseNote(invalid),"invalid or contradictory note labels rejected");
    e4::MidiEngine feedback;
    events.clear();
    for (int note = 60; note < 65; ++note) feedback.process(0x90,note,100,emit);
    check(feedback.rejectionCount() == 1 && feedback.lastRejection() == e4::MidiEngine::Rejection::voiceLimit,
          "voice-limit feedback identifies a rejected press");
    feedback.process(0x80,64,0,emit);
    feedback.panic(emit);
    check(feedback.rejectionCount() == 1,"releases and panic do not generate false rejection flashes");
    feedback.setTranspose(24,emit);
    events.clear();
    feedback.process(0x90,120,100,emit);
    feedback.process(0x80,120,0,emit);
    check(feedback.rejectionCount() == 2 && feedback.lastRejection() == e4::MidiEngine::Rejection::outOfRange
        && events.empty(),"out-of-range feedback is separate from voice limit and emits no invalid MIDI");
    e4::InputNoteState inputDisplay;
    inputDisplay.process(0x90,60,100); inputDisplay.process(0x91,60,100); inputDisplay.process(0x90,60,100);
    inputDisplay.process(0x80,60,0); inputDisplay.process(0x81,60,0);
    check(inputDisplay.isDown(60),"incoming display keeps repeated/channel-overlapping holds");
    inputDisplay.process(0x90,60,0);
    check(!inputDisplay.isDown(60),"velocity-zero input releases final display hold");
    inputDisplay.process(0x90,0,1); inputDisplay.process(0x9f,127,127);
    check(inputDisplay.low() == 1 && inputDisplay.high() == (std::uint64_t{1}<<63),"display masks cover MIDI endpoints");
    inputDisplay.process(0xb0,0,0); inputDisplay.process(0xe0,127,127);
    check(inputDisplay.isDown(0) && inputDisplay.isDown(127),"non-note input does not alter highlights");
    inputDisplay.clear();
    check(inputDisplay.low() == 0 && inputDisplay.high() == 0,"panic/disconnect clears incoming highlights");
    e4::PerformanceSettings settings;
    e4::PerformancePreset::Bytes preset {};
    check(settings.valid() && e4::PerformancePreset::encode(settings,preset),"factory settings encode");
    auto custom = settings;
    custom.pads[0].offsets = {{-12,0,7,12}}; custom.pads[0].noteCount=4;
    custom.pads[0].inversion=2; custom.pads[0].octave=-1;
    check(e4::PerformancePreset::encode(custom,preset),"custom settings encode");
    e4::PerformanceSettings restored;
    check(e4::PerformancePreset::decode(preset.data(),preset.size(),restored)
        && restored.pads[0].offsets == custom.pads[0].offsets && restored.pads[0].inversion == 2
        && restored.pads[0].octave == -1,"preset round-trip preserves voicing");
    e4::PerformancePreset::Bytes roundTrip {};
    check(e4::PerformancePreset::encode(restored,roundTrip) && roundTrip == preset,"preset encoding is canonical");
    for (std::size_t length=0; length<preset.size(); ++length)
        check(!e4::PerformancePreset::decode(preset.data(),length,restored),"truncated preset rejected");
    const auto validPreset=preset;
    for (int version : {0,2,255}) {
        preset=validPreset; preset[4]=static_cast<std::uint8_t>(version);
        check(!e4::PerformancePreset::decode(preset.data(),preset.size(),restored),"unknown preset version rejected");
    }
    for (std::size_t field : {std::size_t{0},std::size_t{5},std::size_t{29},std::size_t{30},std::size_t{31},std::size_t{32},std::size_t{33}}) {
        preset=validPreset; preset[field]=255;
        check(!e4::PerformancePreset::decode(preset.data(),preset.size(),restored),"malformed preset field rejected");
        check(e4::PerformancePreset::encode(restored,roundTrip) && roundTrip == validPreset,"failed import leaves current settings intact");
    }
    check(!e4::PerformancePreset::decode(nullptr,e4::PerformancePreset::size,restored),"null preset rejected");
    custom.pads[0].offsets[1]=custom.pads[0].offsets[0];
    preset=validPreset;
    check(!e4::PerformancePreset::encode(custom,preset) && preset == validPreset,"duplicate pitches rejected without replacing encoded preset");
    e4::MidiEngine generated;
    events.clear();
    for (int owner=0; owner<e4::source::count; ++owner) generated.process(0x90,60,100,emit,owner);
    check(events == std::vector<Event>{{0x92,60,100}},"all performance sources share one physical voice");
    for (int owner=1; owner<e4::source::count; ++owner) generated.releaseSource(owner,emit);
    check(events.size() == 1 && generated.activeVoices() == 1,"generator cleanup preserves input owner");
    generated.releaseSource(e4::source::input,emit);
    check(events.back() == Event{0x82,60,0} && generated.activeVoices() == 0,"last input owner releases generated overlap");
    events.clear();
    generated.process(0x90,60,100,emit,e4::source::count);
    generated.releaseSource(-1,emit);
    check(events.empty(),"invalid source IDs are ignored");
    for (int note=60; note<64; ++note) generated.process(0x90,note,100,emit,e4::source::arpeggiator);
    generated.process(0x90,64,100,emit,e4::source::sequencer);
    check(generated.activeVoices() == 4,"generators obey shared four-voice limit");
    generated.releaseSource(e4::source::arpeggiator,emit);
    generated.releaseSource(e4::source::sequencer,emit);
    events.clear();
    generated.process(0x90,64,100,emit,e4::source::sequencer);
    check(events == std::vector<Event>{{0x92,64,100}},"generator cancellation clears rejected holds");
    generated.panic(emit);
    std::array<int,4> chordNotes;
    auto chord = e4::PerformanceSettings{}.pads[0];
    for (int inversion=0;inversion<3;++inversion) {
        chord.inversion=inversion;
        const std::array<std::array<int,4>,3> expected {{{{60,64,67,-1}},{{64,67,72,-1}},{{67,72,76,-1}}}};
        check(e4::ChordPads::voicing(chord,60,chordNotes) && chordNotes == expected[static_cast<std::size_t>(inversion)],"major inversions move lowest notes above chord");
    }
    chord.octave=-1;
    check(e4::ChordPads::voicing(chord,60,chordNotes) && chordNotes == std::array<int,4>{{55,60,64,-1}},"inversion and octave combine");
    chord.offsets={{-12,0,12,24}}; chord.noteCount=4; chord.inversion=2; chord.octave=0;
    check(e4::ChordPads::voicing(chord,60,chordNotes) && chordNotes == std::array<int,4>{{72,84,96,108}},"wide inversions retain unique pitches");
    e4::ChordPads edited;
    auto editSettings=edited.getSettings(); editSettings.pads[0].offsets={{-12,0,7,0}};
    newPort.sent.clear(); session.setOutput(&newPort); newPort.sent.clear();
    edited.setHeld(0,true,60,chordSend); session.process({0x90,60,100});
    check(edited.replace(editSettings,chordSend) && session.activeVoices()==1,"bank replacement releases pad but preserves external owner");
    newPort.sent.clear(); edited.setHeld(0,true,60,chordSend);
    check(newPort.sent == std::vector<Event>{{0x92,48,100},{0x92,67,100}},"negative custom offsets play valid notes");
    edited.releaseAll(chordSend); session.process({0x80,60,0}); newPort.sent.clear();
    edited.setHeld(0,true,0,chordSend);
    check(newPort.sent.empty(),"out-of-range voicing rejected atomically without partial chord");
    auto invalidBank=editSettings; invalidBank.pads[0].noteCount=0;
    check(!edited.replace(invalidBank,chordSend) && edited.getSettings().pads[0].offsets==editSettings.pads[0].offsets,"invalid replacement preserves bank");
    for (const auto& pad : e4::PerformanceSettings{}.pads)
        for (int inv=0;inv<pad.noteCount;++inv)
            for (int root=0;root<128;++root) {
                auto candidate=pad; candidate.inversion=inv;
                if (e4::ChordPads::voicing(candidate,root,chordNotes))
                    for (int n=0;n<pad.noteCount;++n) check(chordNotes[static_cast<std::size_t>(n)]>=0 && chordNotes[static_cast<std::size_t>(n)]<=127,"all generated voicings stay within MIDI range");
            }
    e4::SettingsExchange exchange;
    e4::PerformanceSettings snapshot; std::uint64_t revision=0;
    check(exchange.consume(snapshot,revision) && !exchange.consume(snapshot,revision),"snapshot consumed once per revision");
    check(!exchange.publish(invalidBank) && !exchange.consume(snapshot,revision),"invalid publication preserves active snapshot");
    std::atomic<bool> finished {false};
    std::thread writer([&] {
        for (int n=0;n<5000;++n) {
            auto next=e4::PerformanceSettings{};
            for (auto& pad : next.pads) pad.octave=(n%5)-2;
            exchange.publish(next);
        }
        finished.store(true);
    });
    while (!finished.load()) if (exchange.consume(snapshot,revision)) {
        check(snapshot.valid(),"concurrent snapshot stays valid");
        for (const auto& pad : snapshot.pads) check(pad.octave==snapshot.pads[0].octave,"snapshot publication never mixes revisions");
    }
    writer.join();
    exchange.consume(snapshot,revision);
    check(snapshot.pads[0].octave==2,"consumer eventually observes final published bank");
    std::cout << "MIDI engine: " << failures << " failures\n";
    return failures ? 1 : 0;
}
