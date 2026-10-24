#include "PluginProcessor.h"
#include "PerformanceKeyboard.h"
#include <iostream>
int main() {
    juce::ScopedJuceInitialiser_GUI initialiser;
    int failures = 0;
    const auto check = [&failures](bool ok, const char* label) {
        if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
    };
    E4Processor processor;
    processor.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> audio(2, 128);
    juce::MidiBuffer midi;
    midi.ensureSize(65536);
    midi.addEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(100)), 10);
    midi.addEvent(juce::MidiMessage::controllerEvent(1,74,100), 20);
    midi.addEvent(juce::MidiMessage::noteOff(1,60), 30);
    processor.processBlock(audio, midi);
    check(midi.getNumEvents() == 3, "initial bend reset, note on and off survive filtering");
    int index = 0;
    for (const auto event : midi) {
        if (index == 1) check(event.samplePosition == 10 && event.getMessage().isNoteOn()
            && event.getMessage().getChannel() == 3, "note timestamp and channel");
        if (index == 2) check(event.samplePosition == 30 && event.getMessage().isNoteOff(), "release timestamp");
        ++index;
    }
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1,64,static_cast<juce::uint8>(100)), 0);
    processor.processBlock(audio, midi);
    processor.requestPanic();
    midi.clear();
    processor.processBlock(audio, midi);
    check(midi.getNumEvents() == 2 && (*midi.begin()).getMessage().isNoteOff(), "processor panic releases held note");
    auto* channel = processor.parameters.getParameter("channel");
    channel->setValueNotifyingHost(channel->convertTo0to1(12));
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    E4Processor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(static_cast<int>(restored.parameters.getRawParameterValue("channel")->load()) == 12, "channel persists");
    restored.setStateInformation("garbage", 7);
    check(static_cast<int>(restored.parameters.getRawParameterValue("channel")->load()) == 12, "bad state ignored");
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1,67,static_cast<juce::uint8>(100)), 0);
    processor.processBlock(audio, midi);
    midi.clear();
    processor.processBlockBypassed(audio,midi);
    check(midi.getNumEvents() == 2, "bypass cleans up notes");
    midi.clear();
    processor.processBlockBypassed(audio,midi);
    check(midi.isEmpty(), "bypass does not flood bend resets");
    // Oversized bursts are bounded and end in releases instead of stuck notes.
    midi.clear();
    for (int n = 0; n < 3000; ++n)
        midi.addEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(100)),0);
    processor.processBlock(audio,midi);
    bool released = false;
    for (const auto event : midi) if (event.getMessage().isNoteOff()) released = true;
    check(released, "event budget overflow releases owned notes");
    E4Processor performance;
    performance.prepareToPlay(48000,128);
    midi.clear(); performance.processBlock(audio,midi); // Drain initial cleanup.
    performance.postPerformance({e4::PerformanceCommand::padDown,3,100});
    midi.clear(); performance.processBlock(audio,midi);
    check(midi.getNumEvents() == 4 && performance.activeVoices() == 4,"plugin major seventh pad");
    performance.postPerformance({e4::PerformanceCommand::padUp,3,0});
    midi.clear(); performance.processBlock(audio,midi);
    check(midi.getNumEvents() == 4 && performance.activeVoices() == 0,"plugin pad release");
    performance.postPerformance({e4::PerformanceCommand::noteOn,60,100});
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(100)),20);
    performance.processBlock(audio,midi);
    check(midi.getNumEvents() == 1,"UI and clip overlap emits one physical note");
    performance.requestUiRelease();
    midi.clear(); performance.processBlock(audio,midi);
    bool noteOff = false;
    for (auto event : midi) if (event.getMessage().isNoteOff()) noteOff = true;
    check(!noteOff && performance.activeVoices() == 1,"editor close preserves matching host-owned note");
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOff(1,60),20);
    performance.processBlock(audio,midi);
    check(midi.getNumEvents() == 1 && performance.activeVoices() == 0,"clip release after editor close");
    performance.postPerformance({e4::PerformanceCommand::noteOn,67,100});
    performance.requestUiRelease();
    midi.clear(); performance.processBlock(audio,midi);
    check(performance.activeVoices() == 0,"editor close drops unsent UI presses");
    performance.postPerformance({e4::PerformanceCommand::padDown,0,100});
    midi.clear(); performance.processBlock(audio,midi);
    auto* root = performance.parameters.getParameter("chordRoot");
    root->setValueNotifyingHost(root->convertTo0to1(62));
    midi.clear(); performance.processBlock(audio,midi);
    check(midi.getNumEvents() == 3 && performance.activeVoices() == 0,"root automation releases original chord without replay");
    performance.postPerformance({e4::PerformanceCommand::bend,16383,0});
    midi.clear(); performance.processBlock(audio,midi);
    check(midi.getNumEvents() == 1 && (*midi.begin()).getMessage().isPitchWheel()
        && (*midi.begin()).getMessage().getPitchWheelValue() == 16383,"UI bend encoded on host MIDI output");
    performance.postPerformance({e4::PerformanceCommand::noteOn,64,100});
    midi.clear(); performance.processBlock(audio,midi);
    bool overflowed = false;
    for (int n = 0; n < 257; ++n)
        if (!performance.postPerformance({e4::PerformanceCommand::noteOn,72,100})) overflowed = true;
    midi.clear(); performance.processBlock(audio,midi);
    check(overflowed && performance.activeVoices() == 0,"UI queue overflow releases notes and drops stale presses");
    juce::MemoryBlock current;
    performance.getStateInformation(current);
    E4Processor saved;
    saved.setStateInformation(current.getData(),static_cast<int>(current.getSize()));
    check(static_cast<int>(saved.parameters.getRawParameterValue("chordRoot")->load()) == 62,"chord root persists in schema 3");
    auto legacy = performance.parameters.copyState();
    legacy.setProperty("schema",1,nullptr);
    legacy.removeChild(legacy.getChildWithProperty("id","chordRoot"),nullptr);
    juce::MemoryBlock oldState;
    if (auto xml = legacy.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml,oldState);
    saved.setStateInformation(oldState.getData(),static_cast<int>(oldState.getSize()));
    check(static_cast<int>(saved.parameters.getRawParameterValue("chordRoot")->load()) == 60,"legacy channel-only state migrates to default root");
    auto invalid = performance.parameters.copyState();
    invalid.setProperty("schema",2,nullptr);
    invalid.getChildWithProperty("id","chordRoot").setProperty("value",900,nullptr);
    if (auto xml = invalid.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml,oldState);
    saved.setStateInformation(oldState.getData(),static_cast<int>(oldState.getSize()));
    check(static_cast<int>(saved.parameters.getRawParameterValue("chordRoot")->load()) == 60,"invalid root state rejected atomically");
    E4Processor transposeProcessor;
    transposeProcessor.prepareToPlay(48000,128);
    midi.clear(); transposeProcessor.processBlock(audio,midi);
    auto* transpose = transposeProcessor.parameters.getParameter("transpose");
    transpose->setValueNotifyingHost(transpose->convertTo0to1(12));
    midi.clear(); transposeProcessor.processBlock(audio,midi);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(100)),31);
    transposeProcessor.processBlock(audio,midi);
    check(midi.getNumEvents() == 1 && (*midi.begin()).getMessage().getNoteNumber() == 72
        && (*midi.begin()).samplePosition == 31,"host note transposes with sample offset preserved");
    transpose->setValueNotifyingHost(transpose->convertTo0to1(-12));
    midi.clear(); transposeProcessor.processBlock(audio,midi);
    check(midi.getNumEvents() == 2 && (*midi.begin()).getMessage().isNoteOff()
        && (*midi.begin()).getMessage().getNoteNumber() == 72,"transpose automation releases old output note");
    transposeProcessor.postPerformance({e4::PerformanceCommand::padDown,0,100});
    midi.clear(); transposeProcessor.processBlock(audio,midi);
    check(midi.getNumEvents() == 3 && (*midi.begin()).getMessage().getNoteNumber() == 48,"UI chord is transposed once");
    transposeProcessor.requestUiRelease();
    midi.clear(); transposeProcessor.processBlock(audio,midi);
    check(transposeProcessor.activeVoices() == 0 && (*midi.begin()).getMessage().getNoteNumber() == 48,
          "editor close cleans transposed chord at original output pitches");
    juce::MemoryBlock transposeState;
    transposeProcessor.getStateInformation(transposeState);
    saved.setStateInformation(transposeState.getData(),static_cast<int>(transposeState.getSize()));
    check(static_cast<int>(saved.parameters.getRawParameterValue("transpose")->load()) == -12,"schema 3 saves transpose");
    auto schema2 = transposeProcessor.parameters.copyState();
    schema2.setProperty("schema",2,nullptr);
    schema2.removeChild(schema2.getChildWithProperty("id","transpose"),nullptr);
    if (auto xml = schema2.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml,oldState);
    saved.setStateInformation(oldState.getData(),static_cast<int>(oldState.getSize()));
    check(static_cast<int>(saved.parameters.getRawParameterValue("transpose")->load()) == 0,"schema 2 migration defaults to no transpose");
    auto badTranspose = transposeProcessor.parameters.copyState();
    badTranspose.setProperty("schema",3,nullptr);
    badTranspose.getChildWithProperty("id","transpose").setProperty("value",-25,nullptr);
    if (auto xml = badTranspose.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml,oldState);
    saved.setStateInformation(oldState.getData(),static_cast<int>(oldState.getSize()));
    check(static_cast<int>(saved.parameters.getRawParameterValue("transpose")->load()) == 0,"out-of-range transpose state is rejected");
    E4Processor rejectionProcessor;
    rejectionProcessor.prepareToPlay(48000,128);
    midi.clear(); rejectionProcessor.processBlock(audio,midi);
    midi.clear();
    for (int note = 60; note < 65; ++note)
        midi.addEvent(juce::MidiMessage::noteOn(1,note,static_cast<juce::uint8>(100)),0);
    rejectionProcessor.processBlock(audio,midi);
    check(rejectionProcessor.activeVoices() == 4,"processor tracks four accepted voices");
    check(rejectionProcessor.rejectionCount() == 1 && rejectionProcessor.lastRejection() == e4::MidiEngine::Rejection::voiceLimit,
          "processor publishes rejection feedback for the UI without changing output");
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
    midi.addEvent(juce::MidiMessage::noteOn(1,64,static_cast<juce::uint8>(100)),1);
    rejectionProcessor.processBlock(audio,midi);
    check(midi.getNumEvents() == 1 && (*midi.begin()).getMessage().isNoteOff(),
          "rejected fifth-note repeat cannot be prematurely accepted across blocks");
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOff(1,64),0);
    midi.addEvent(juce::MidiMessage::noteOff(1,64),1);
    midi.addEvent(juce::MidiMessage::noteOn(1,64,static_cast<juce::uint8>(100)),2);
    rejectionProcessor.processBlock(audio,midi);
    check(midi.getNumEvents() == 1 && (*midi.begin()).getMessage().isNoteOn()
        && (*midi.begin()).samplePosition == 2,"fresh fifth pitch works after full release");
    E4Processor incoming;
    incoming.prepareToPlay(48000,128);
    midi.clear(); incoming.processBlock(audio,midi);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(100)),0);
    incoming.processBlock(audio,midi);
    check(incoming.incomingNotes()[0] == (std::uint64_t{1}<<60) && midi.getNumEvents() == 1,
          "incoming host note publishes highlight without generating extra output");
    incoming.requestUiRelease(); midi.clear(); incoming.processBlock(audio,midi);
    check(incoming.incomingNotes()[0] == (std::uint64_t{1}<<60),"editor close preserves external highlight ownership");
    incoming.requestPanic(); midi.clear(); incoming.processBlock(audio,midi);
    check(incoming.incomingNotes()[0] == 0,"panic clears host note highlights");
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(16,127,static_cast<juce::uint8>(100)),0);
    incoming.processBlock(audio,midi);
    check(incoming.incomingNotes()[1] == (std::uint64_t{1}<<63),"high incoming notes publish safely");
    midi.clear(); incoming.processBlockBypassed(audio,midi);
    check(incoming.incomingNotes()[1] == 0,"bypass clears incoming highlights");
    struct KeyboardListener final : juce::MidiKeyboardState::Listener {
        int ons = 0,offs = 0;
        void handleNoteOn(juce::MidiKeyboardState*,int,int,float) override { ++ons; }
        void handleNoteOff(juce::MidiKeyboardState*,int,int,float) override { ++offs; }
    } listener;
    juce::MidiKeyboardState mouseKeys;
    mouseKeys.addListener(&listener);
    e4::ui::PerformanceKeyboard display(mouseKeys,juce::MidiKeyboardComponent::horizontalKeyboard);
    display.setSize(658,86); display.setAvailableRange(36,84);
    display.showIncoming(std::uint64_t{1}<<60,0);
    check(display.incomingDown(60) && listener.ons == 0 && listener.offs == 0
        && !mouseKeys.isNoteOn(1,60),"external highlights are visual only and cannot feed MIDI back");
    mouseKeys.noteOn(1,60,1.0f);
    display.showIncoming(0,0);
    check(mouseKeys.isNoteOn(1,60) && listener.offs == 0,"external release cannot release a mouse-held note");
    mouseKeys.noteOff(1,60,0.0f);
    juce::MidiBuffer mouseMidi;
    mouseKeys.processNextMidiBuffer(mouseMidi,0,128,true);
    check(listener.ons == 1 && listener.offs == 1 && mouseMidi.getNumEvents() == 2,"overlap still generates exactly one UI press/release pair");
    display.showIncoming(0,std::uint64_t{1}<<63);
    check(display.getRangeEnd() == 127 && display.incomingDown(127),"keyboard view follows high incoming notes");
    display.showIncoming(1,0);
    check(display.getRangeStart() == 0 && display.incomingDown(0),"keyboard view follows low incoming notes");
    mouseKeys.removeListener(&listener);
    std::cout << "Processor integration: " << failures << " failures\n";
    return failures ? 1 : 0;
}
