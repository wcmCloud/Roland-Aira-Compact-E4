#include "PluginProcessor.h"

namespace {
class E4Editor final : public juce::AudioProcessorEditor, private juce::Timer,
                       private juce::MidiKeyboardState::Listener {
public:
    explicit E4Editor(E4Processor& p)
        : AudioProcessorEditor(p), owner(p), keyboard(keys,juce::MidiKeyboardComponent::horizontalKeyboard),
          channelAttachment(p.parameters, "channel", channel), rootAttachment(p.parameters,"chordRoot",root), transposeAttachment(p.parameters,"transpose",transpose) {
        title.setText("E-4 Companion",juce::dontSendNotification);
        info.setText("Enable VOCODER, KEY IN and HARMONY on the E-4.\n"
                     "Route VST3 MIDI output to hardware; use Live port Sync for clock.\n"
                     "AU MIDI output is pending. Audio comes from the physical E-4.",juce::dontSendNotification);
        for (auto* slider : {&channel,&root,&transpose}) {
            slider->setSliderStyle(juce::Slider::IncDecButtons);
            slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,120,26);
        }
        channel.setTextValueSuffix("  MIDI ch"); root.setTextValueSuffix("  chord root"); transpose.setTextValueSuffix("  semitones");
        bend.setRange(0,16383,1); bend.setValue(8192,juce::dontSendNotification);
        bend.setTextValueSuffix("  bend"); bend.setDoubleClickReturnValue(true,8192);
        bend.onValueChange = [this] { send({e4::PerformanceCommand::bend,static_cast<int>(bend.getValue()),0}); };
        bend.onDragEnd = [this] { bend.setValue(8192); };
        panic.setButtonText("Panic / release notes");
        panic.onClick = [this] { keys.allNotesOff(0); owner.requestPanic(); bend.setValue(8192,juce::dontSendNotification); };
        const std::array<juce::String,4> names {"Major","Minor","Sus2","Maj7"};
        for (int i = 0; i < 4; ++i) {
            auto& pad = pads[static_cast<std::size_t>(i)];
            pad.setButtonText(names[static_cast<std::size_t>(i)]);
            pad.onStateChange = [this,i] { send({pads[static_cast<std::size_t>(i)].isDown()
                ? e4::PerformanceCommand::padDown : e4::PerformanceCommand::padUp,i,100}); };
            addAndMakeVisible(pad);
        }
        keys.addListener(this);
        keyboard.setAvailableRange(36,84); keyboard.setScrollButtonsVisible(false);
        for (auto* c : std::array<juce::Component*,9>{&title,&info,&channel,&root,&transpose,&panic,&bend,&status,&keyboard})
            addAndMakeVisible(c);
        setSize(740,460);
        startTimerHz(20);
    }
    ~E4Editor() override {
        stopTimer(); keys.removeListener(this); owner.requestUiRelease();
    }
    void resized() override {
        auto area = getLocalBounds().reduced(20);
        title.setBounds(area.removeFromTop(30)); info.setBounds(area.removeFromTop(74));
        auto row = area.removeFromTop(32);
        channel.setBounds(row.removeFromLeft(180)); row.removeFromLeft(12);
        root.setBounds(row.removeFromLeft(180)); row.removeFromLeft(12);
        panic.setBounds(row);
        area.removeFromTop(8); transpose.setBounds(area.removeFromTop(30).removeFromLeft(220));
        area.removeFromTop(12); bend.setBounds(area.removeFromTop(32));
        area.removeFromTop(12); row = area.removeFromTop(36);
        for (auto& pad : pads) pad.setBounds(row.removeFromLeft(170).reduced(3,0));
        status.setBounds(area.removeFromTop(32));
        keyboard.setBounds(area.reduced(0,8));
    }
private:
    void send(e4::PerformanceCommand command) { owner.postPerformance(command); }
    void handleNoteOn(juce::MidiKeyboardState*,int,int note,float velocity) override {
        send({e4::PerformanceCommand::noteOn,note,juce::jlimit(1,127,juce::roundToInt(velocity*127))});
    }
    void handleNoteOff(juce::MidiKeyboardState*,int,int note,float) override {
        send({e4::PerformanceCommand::noteOff,note,0});
    }
    void timerCallback() override {
        juce::MidiBuffer unused; keys.processNextMidiBuffer(unused,0,1,false);
        status.setText("Active voices: " + juce::String(owner.activeVoices())
            + juce::String::fromUTF8(" / 4 — clips, keyboard and pads share this limit"),juce::dontSendNotification);
    }
    E4Processor& owner;
    juce::MidiKeyboardState keys;
    juce::MidiKeyboardComponent keyboard;
    juce::Label title,info,status;
    juce::Slider channel,root,bend,transpose;
    juce::TextButton panic;
    std::array<juce::TextButton,4> pads;
    juce::AudioProcessorValueTreeState::SliderAttachment channelAttachment,rootAttachment,transposeAttachment;
};
}
E4Processor::E4Processor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "E4State", {
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"channel", 1}, "MIDI channel", 1, 16, 3),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"chordRoot", 1}, "Chord root", 36, 84, 60),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"transpose", 1}, "Transpose", -24, 24, 0)
      }) {}
void E4Processor::prepareToPlay(double, int) {
    output.ensureSize(65536);
    // Keep note ownership until the next callback can send explicit releases.
    panicRequested.store(true);
}
bool E4Processor::isBusesLayoutSupported(const BusesLayout& layout) const {
    return layout.getMainInputChannelSet().isDisabled()
        && layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
void E4Processor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals guard;
    audio.clear();
    output.clear();
    bypassed = false;
    if (audio.getNumSamples() == 0) { midi.clear(); return; }
    int position = 0;
    const auto emit = [this, &position](int status, int first, int second) {
        const juce::uint8 bytes[] = {static_cast<juce::uint8>(status),
            static_cast<juce::uint8>(first), static_cast<juce::uint8>(second)};
        output.addEvent(bytes, 3, position);
    };
    const auto send = [this,&emit](e4::MidiEvent event,int source) {
        engine.process(event.status,event.first,event.second,emit,source);
    };
    const int nextChannel = static_cast<int>(parameters.getRawParameterValue("channel")->load());
    const bool channelChanged = nextChannel != engine.outputChannel();
    const int nextTranspose = static_cast<int>(parameters.getRawParameterValue("transpose")->load());
    const bool transposeChanged = nextTranspose != engine.semitoneOffset();
    const bool queueFailed = performance.consumeOverflow();
    const bool panic = panicRequested.exchange(false) || channelChanged || transposeChanged || queueFailed;
    const bool releaseUi = uiReleaseRequested.exchange(false);
    if (panic || releaseUi) {
        performance.discard();
        chords.releaseAll(send);
        if (panic) {
            if (!channelChanged && !transposeChanged) engine.panic(emit);
        }
        else {
            for (int source = 1; source <= 5; ++source) engine.releaseSource(source,emit);
            emit(0xe0 | (engine.outputChannel()-1),0,64);
        }
    }
    engine.setChannel(nextChannel,emit);
    engine.setTranspose(nextTranspose,emit);
    const int root = static_cast<int>(parameters.getRawParameterValue("chordRoot")->load());
    if (root != lastRoot) { chords.releaseAll(send); lastRoot = root; }
    bool stopped = false;
    if (auto* playhead = getPlayHead())
        if (const auto state = playhead->getPosition()) {
            const bool playing = state->getIsPlaying();
            stopped = wasPlaying && !playing;
            wasPlaying = playing;
        }
    if (stopped) { performance.discard(); chords.releaseAll(send); engine.panic(emit); }
    e4::PerformanceCommand command;
    for (int n = 0; n < 256 && performance.pop(command); ++n) {
        switch (command.kind) {
            case e4::PerformanceCommand::noteOn: engine.process(0x90,command.value,command.velocity,emit,1); break;
            case e4::PerformanceCommand::noteOff: engine.process(0x80,command.value,0,emit,1); break;
            case e4::PerformanceCommand::padDown: chords.setHeld(command.value,true,root,send); break;
            case e4::PerformanceCommand::padUp: chords.setHeld(command.value,false,root,send); break;
            case e4::PerformanceCommand::bend:
                if (command.value >= 0 && command.value <= 16383)
                    engine.process(0xe0,command.value & 127,(command.value >> 7) & 127,emit,1);
                break;
        }
    }
    int count = 0;
    for (const auto metadata : midi) {
        position = juce::jlimit(0, audio.getNumSamples() - 1, metadata.samplePosition);
        if (++count > 2048) { chords.releaseAll(send); engine.panic(emit); break; }
        // Read raw bytes: copying a long SysEx MidiMessage could allocate here.
        if (metadata.numBytes != 3) continue;
        const auto* bytes = metadata.data;
        engine.process(bytes[0], bytes[1], bytes[2], emit);
    }
    voiceCount.store(engine.activeVoices());
    // Internal staging is reserved. Host-owned MidiBuffer capacity is host dependent.
    midi.clear();
    midi.addEvents(output, 0, -1, 0);
}
void E4Processor::processBlockBypassed(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    audio.clear();
    midi.clear();
    if (audio.getNumSamples() == 0) { panicRequested.store(true); return; }
    performance.discard();
    if (bypassed) return;
    bypassed = true;
    chords.releaseAll([](e4::MidiEvent,int) {});
    voiceCount.store(0);
    engine.panic([&midi](int status, int first, int second) {
        const juce::uint8 bytes[] = {static_cast<juce::uint8>(status),
            static_cast<juce::uint8>(first), static_cast<juce::uint8>(second)};
        midi.addEvent(bytes, 3, 0);
    });
}
juce::AudioProcessorEditor* E4Processor::createEditor() { return new E4Editor(*this); }
void E4Processor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = parameters.copyState();
    state.setProperty("schema", 3, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}
void E4Processor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        auto state = juce::ValueTree::fromXml(*xml);
        const int schema = static_cast<int>(state.getProperty("schema"));
        if (!state.hasType("E4State") || (schema != 1 && schema != 2 && schema != 3)) return;
        const auto validParameter = [&state](const char* id,int minimum,int maximum) {
            auto node = state.getChildWithProperty("id",id);
            const double value = static_cast<double>(node.getProperty("value",-1));
            return node.isValid() && std::isfinite(value) && value >= minimum && value <= maximum && value == std::floor(value);
        };
        if (!validParameter("channel",1,16)) return;
        if (schema == 1) {
            auto node = state.getChildWithProperty("id","chordRoot");
            if (node.isValid()) state.removeChild(node,nullptr);
            juce::ValueTree rootNode("PARAM");
            rootNode.setProperty("id","chordRoot",nullptr);
            rootNode.setProperty("value",60,nullptr);
            state.addChild(rootNode,-1,nullptr);
        }
        if (!validParameter("chordRoot",36,84)) return;
        if (schema < 3) {
            auto node = state.getChildWithProperty("id","transpose");
            if (node.isValid()) state.removeChild(node,nullptr);
            juce::ValueTree transposeNode("PARAM");
            transposeNode.setProperty("id","transpose",nullptr);
            transposeNode.setProperty("value",0,nullptr);
            state.addChild(transposeNode,-1,nullptr);
        }
        if (!validParameter("transpose",-24,24)) return;
        state.setProperty("schema",3,nullptr);
        parameters.replaceState(state);
        panicRequested.store(true);
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new E4Processor; }
