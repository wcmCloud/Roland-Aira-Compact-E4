#include "PluginProcessor.h"
#include "StudioTheme.h"

namespace {
class E4Editor final : public juce::AudioProcessorEditor, private juce::Timer,
                       private juce::MidiKeyboardState::Listener {
public:
    explicit E4Editor(E4Processor& p)
        : AudioProcessorEditor(p), owner(p), keyboard(keys,juce::MidiKeyboardComponent::horizontalKeyboard),
          channelAttachment(p.parameters, "channel", channel), rootAttachment(p.parameters,"chordRoot",root), transposeAttachment(p.parameters,"transpose",transpose) {
        setLookAndFeel(&theme);
        title.setText("E-4 Companion",juce::dontSendNotification);
        info.setText("VST3: route MIDI to your E-4. Clock comes from your host. AU output is experimental.",juce::dontSendNotification);
        channelLabel.setText("MIDI CHANNEL",juce::dontSendNotification);
        rootLabel.setText("CHORD ROOT",juce::dontSendNotification);
        transposeLabel.setText("TRANSPOSE",juce::dontSendNotification);
        bendLabel.setText("PITCH BEND",juce::dontSendNotification);
        for (auto* label : {&info,&channelLabel,&rootLabel,&transposeLabel,&bendLabel}) { e4::ui::label(*label,true); addAndMakeVisible(label); }
        e4::ui::label(status); status.setJustificationType(juce::Justification::right);
        status.setText(juce::String(owner.activeVoices()) + " / 4 voices",juce::dontSendNotification);
        for (auto* slider : {&channel,&root,&transpose}) {
            slider->setLookAndFeel(&theme);
            slider->setSliderStyle(juce::Slider::IncDecButtons);
            slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,120,26);
        }
        channel.setTextValueSuffix(""); root.setTextValueSuffix("  MIDI note"); transpose.setTextValueSuffix(" st");
        bend.setLookAndFeel(&theme);
        bend.setRange(0,16383,1); e4::ui::bendStyle(bend); bend.setValue(8192,juce::dontSendNotification);
        bend.setTextValueSuffix(""); bend.setTextBoxStyle(juce::Slider::TextBoxRight,false,76,32); bend.setDoubleClickReturnValue(true,8192);
        bend.onValueChange = [this] { send({e4::PerformanceCommand::bend,static_cast<int>(bend.getValue()),0}); };
        bend.onDragEnd = [this] { bend.setValue(8192); };
        panic.getProperties().set("role","danger");
        panic.setButtonText("Panic / release notes");
        panic.onClick = [this] { keys.allNotesOff(0); owner.requestPanic(); bend.setValue(8192,juce::dontSendNotification); };
        const std::array<juce::String,4> names {"Major","Minor","Sus2","Maj7"};
        for (int i = 0; i < 4; ++i) {
            auto& pad = pads[static_cast<std::size_t>(i)];
            pad.getProperties().set("role","pad"); pad.getProperties().set("index",i);
            pad.getProperties().set("detail",i == 2 ? "SUSPENDED" : i == 3 ? "FOUR VOICES" : "TRIAD");
            pad.setButtonText(names[static_cast<std::size_t>(i)]);
            pad.onStateChange = [this,i] { send({pads[static_cast<std::size_t>(i)].isDown()
                ? e4::PerformanceCommand::padDown : e4::PerformanceCommand::padUp,i,100}); };
            addAndMakeVisible(pad);
        }
        e4::ui::keyboardStyle(keyboard);
        keys.addListener(this);
        keyboard.setAvailableRange(36,84); keyboard.setScrollButtonsVisible(false);
        for (auto* c : std::array<juce::Component*,9>{&title,&info,&channel,&root,&transpose,&panic,&bend,&status,&keyboard})
            addAndMakeVisible(c);
        setSize(960,650);
        startTimerHz(20);
    }
    ~E4Editor() override {
        stopTimer(); keys.removeListener(this); owner.requestUiRelease(); setLookAndFeel(nullptr);
    }
    void paint(juce::Graphics& g) override { e4::ui::surface(g,getWidth(),getHeight(),false,false); }
    void resized() override {
        const auto place = [this](juce::Component& c,int x,int y,int w,int h) { c.setBounds(e4::ui::bounds(x,y,w,h,getWidth(),getHeight(),false)); };
        title.setBounds(0,0,0,0);
        place(panic,736,28,200,36); place(info,44,144,870,24);
        place(channelLabel,44,179,220,18); place(channel,44,202,220,36);
        place(rootLabel,286,179,220,18); place(root,286,202,220,36);
        place(transposeLabel,528,179,220,18); place(transpose,528,202,220,36);
        place(bendLabel,44,265,100,22); place(bend,156,255,760,38);
        for (int i = 0; i < 4; ++i) place(pads[static_cast<std::size_t>(i)],44+i*221,370,209,78);
        place(status,560,499,354,22); place(keyboard,44,534,872,58);
        keyboard.setKeyWidth(static_cast<float>(keyboard.getWidth())/29.0f);
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
        status.setText(juce::String(owner.activeVoices()) + " / 4 voices",juce::dontSendNotification);
    }
    e4::ui::Theme theme;
    E4Processor& owner;
    juce::MidiKeyboardState keys;
    juce::MidiKeyboardComponent keyboard;
    juce::Label title,info,status,channelLabel,rootLabel,transposeLabel,bendLabel;
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
