#include "PluginProcessor.h"
#include "StudioTheme.h"
#include "PerformanceKeyboard.h"
#include "ChordBankEditor.h"
#include "ScaleControls.h"
#include "ControllerEditor.h"

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
        addAndMakeVisible(voiceDisplay);
        voiceDisplay.reset(owner.activeVoices(),owner.rejectionCount());
        status.setText(juce::String(owner.activeVoices()) + " / 4 voices",juce::dontSendNotification);
        for (auto* slider : {&channel,&root,&transpose}) {
            slider->setLookAndFeel(&theme);
            slider->setSliderStyle(juce::Slider::IncDecButtons);
            slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,120,26);
        }
        channel.setTextValueSuffix(""); e4::ui::rootStyle(root); transpose.setTextValueSuffix(" st");
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
        bankButton.setButtonText("Edit / load chord bank...");
        bankButton.onClick=[this] {
            if (bankWindow) { bankWindow->toFront(true); return; }
            auto safe=juce::Component::SafePointer<E4Editor>(this);
            bankWindow=e4::ui::openChordBank(owner.chordBank(),static_cast<int>(root.getValue()),[safe](e4::PerformanceSettings bank) {
                if (safe) safe->owner.setChordBank(bank);
            },owner.scaleSettings());
        };
        mappingButton.setButtonText("MIDI learn...");
        mappingButton.onClick=[this] {
            if (mappingWindow) { mappingWindow->toFront(true);return; }
            auto safe=juce::Component::SafePointer<E4Editor>(this);
            mappingWindow=e4::ui::openControllerEditor(owner.controllers.exchange.get(),
                [safe](e4::ControllerProfile profile) { return safe && safe->owner.controllers.exchange.publish(profile); },
                [safe](int slot) { if (safe) safe->owner.controllers.requestLearn(slot); },
                [safe] { return safe ? safe->owner.controllers.learned.exchange(0) : 0; });
        };
        mappingButton.setTooltip("Map incoming MIDI controls. If Live does not deliver a CC, use its host parameter mapping instead.");
        addAndMakeVisible(mappingButton);
        addAndMakeVisible(bankButton);
        updatePadNames();
        e4::ui::keyboardStyle(keyboard);
        keys.addListener(this);
        keyboard.setAvailableRange(36,84); keyboard.setScrollButtonsVisible(false);
        const auto incoming = owner.incomingNotes(); keyboard.showIncoming(incoming[0],incoming[1]);
        for (auto* c : std::array<juce::Component*,9>{&title,&info,&channel,&root,&transpose,&panic,&bend,&status,&keyboard})
            addAndMakeVisible(c);
        addAndMakeVisible(scaleControls);
        modeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.parameters,"scaleMode",scaleControls.mode);
        tonicAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.parameters,"scaleTonic",scaleControls.tonic);
        typeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.parameters,"scaleType",scaleControls.type);
        padsAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters,"scalePads",scaleControls.pads);
        setSize(960,720);
        startTimerHz(20);
    }
    ~E4Editor() override {
        if (mappingWindow) delete mappingWindow.getComponent();
        owner.controllers.requestLearn(-1);
        if (bankWindow) delete bankWindow.getComponent();
        stopTimer(); keys.removeListener(this); owner.requestUiRelease(); setLookAndFeel(nullptr);
    }
    void paint(juce::Graphics& g) override { e4::ui::surface(g,getWidth(),getHeight(),false,false); }
    void resized() override {
        const auto place = [this](juce::Component& c,int x,int y,int w,int h) { c.setBounds(e4::ui::bounds(x,y,w,h,getWidth(),getHeight(),false)); };
        title.setBounds(0,0,0,0);
        place(panic,736,28,200,36); place(info,44,144,870,24);
        place(channelLabel,44,179,220,18); place(channel,44,202,220,36);
        place(mappingButton,770,202,146,36);
        place(rootLabel,286,179,220,18); place(root,286,202,220,36);
        place(transposeLabel,528,179,220,18); place(transpose,528,202,220,36);
        place(bendLabel,44,265,100,22); place(bend,156,255,760,38);
        for (int i = 0; i < 4; ++i) place(pads[static_cast<std::size_t>(i)],44+i*221,440,209,78);
        place(bankButton,656,404,260,28);
        status.setBounds(0,0,0,0); place(voiceDisplay,676,569,240,22); place(keyboard,44,604,872,58);
        place(scaleControls,44,318,872,52);
        keyboard.setKeyWidth(static_cast<float>(keyboard.getWidth())/29.0f);
    }

private:
    void updatePadNames() {
        const auto bank=owner.chordBank();
        for (std::size_t i=0;i<4;++i) {
            pads[i].setButtonText(juce::String(bank.pads[i].name.data()));
            pads[i].getProperties().set("detail",juce::String(bank.pads[i].noteCount)+" VOICES / INV "+juce::String(bank.pads[i].inversion));
        }
    }
    void send(e4::PerformanceCommand command) { owner.postPerformance(command); }
    void handleNoteOn(juce::MidiKeyboardState*,int,int note,float velocity) override {
        send({e4::PerformanceCommand::noteOn,note,juce::jlimit(1,127,juce::roundToInt(velocity*127))});
    }
    void handleNoteOff(juce::MidiKeyboardState*,int,int note,float) override {
        send({e4::PerformanceCommand::noteOff,note,0});
    }
    void timerCallback() override {
        updatePadNames();
        transpose.setValue(owner.effectiveValue(0),juce::dontSendNotification);
        juce::MidiBuffer unused; keys.processNextMidiBuffer(unused,0,1,false);
        const auto incoming = owner.incomingNotes(); keyboard.showIncoming(incoming[0],incoming[1]);
        voiceDisplay.update(owner.activeVoices(),owner.rejectionCount(),owner.lastRejection());
    }
    e4::ui::Theme theme;
    E4Processor& owner;
    juce::MidiKeyboardState keys;
    e4::ui::PerformanceKeyboard keyboard;
    e4::ui::VoiceDisplay voiceDisplay;
    juce::TooltipWindow tooltips {this,600};
    juce::Label title,info,status,channelLabel,rootLabel,transposeLabel,bendLabel;
    juce::Slider channel,root,bend,transpose;
    e4::ui::ScaleControls scaleControls;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment,tonicAttachment,typeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> padsAttachment;
    juce::TextButton panic,bankButton,mappingButton;
    juce::Component::SafePointer<juce::DialogWindow> bankWindow,mappingWindow;
    std::array<juce::TextButton,4> pads;
    juce::AudioProcessorValueTreeState::SliderAttachment channelAttachment,rootAttachment,transposeAttachment;
};
}
E4Processor::E4Processor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "E4State", {
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"channel", 1}, "MIDI channel", 1, 16, 3),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"chordRoot", 1}, "Chord root", 36, 84, 60),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"transpose", 1}, "Transpose", -24, 24, 0),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"pad0Inversion", 1}, "Pad 1 inversion", 0, 3, 0),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"pad0Octave", 1}, "Pad 1 octave", -2, 2, 0),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"pad1Inversion", 1}, "Pad 2 inversion", 0, 3, 0),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"pad1Octave", 1}, "Pad 2 octave", -2, 2, 0),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"pad2Inversion", 1}, "Pad 3 inversion", 0, 3, 0),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"pad2Octave", 1}, "Pad 3 octave", -2, 2, 0),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"pad3Inversion", 1}, "Pad 4 inversion", 0, 3, 0),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"pad3Octave", 1}, "Pad 4 octave", -2, 2, 0),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"scaleMode",1},"Scale mode",0,2,0),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"scaleTonic",1},"Scale tonic",0,11,0),
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"scaleType",1},"Scale type",0,4,1),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"scalePads",1},"Scale includes pads",false)
      }) {}
int E4Processor::effectiveValue(std::size_t index) const {
    constexpr const char* ids[] {"transpose","pad0Inversion","pad1Inversion","pad2Inversion","pad3Inversion"};
    if (index>=5) return 0;
    const int base=static_cast<int>(parameters.getRawParameterValue(ids[index])->load());
    return feedbackReady.load() && mappedBase[index].load()==base ? mappedFeedback[index].load() : base;
}
e4::ScaleSettings E4Processor::scaleSettings() const {
    return {static_cast<int>(parameters.getRawParameterValue("scaleMode")->load()),
            static_cast<int>(parameters.getRawParameterValue("scaleTonic")->load()),
            static_cast<int>(parameters.getRawParameterValue("scaleType")->load()),
            parameters.getRawParameterValue("scalePads")->load()>=0.5f};
}
e4::PerformanceSettings E4Processor::chordBank() const {
    auto bank=bankExchange.get();
    for (int i=0;i<4;++i) {
        auto& pad=bank.pads[static_cast<std::size_t>(i)];
        pad.inversion=juce::jlimit(0,pad.noteCount-1,effectiveValue(static_cast<std::size_t>(i)+1));
        pad.octave=static_cast<int>(parameters.getRawParameterValue("pad"+juce::String(i)+"Octave")->load());
    }
    return bank;
}
bool E4Processor::setChordBank(const e4::PerformanceSettings& bank) {
    if (!bank.valid()) return false;
    for (int i=0;i<4;++i) {
        auto* inversion=parameters.getParameter("pad"+juce::String(i)+"Inversion");
        auto* octave=parameters.getParameter("pad"+juce::String(i)+"Octave");
        inversion->setValueNotifyingHost(inversion->convertTo0to1(static_cast<float>(bank.pads[static_cast<std::size_t>(i)].inversion)));
        octave->setValueNotifyingHost(octave->convertTo0to1(static_cast<float>(bank.pads[static_cast<std::size_t>(i)].octave)));
    }
    return bankExchange.publish(bank);
}
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
    constexpr const char* mappedIds[] {"transpose","pad0Inversion","pad1Inversion","pad2Inversion","pad3Inversion"};
    e4::MappedParameters::Values baseValues;
    for (std::size_t i=0;i<5;++i) baseValues[i]=static_cast<int>(parameters.getRawParameterValue(mappedIds[i])->load());
    const bool profileChanged=controllers.update();
    mappedValues.begin(baseValues,resetMappedValues.exchange(false));
    const auto releaseMapped=[this,&send](e4::MappingAction action) {
        if (action.action<=e4::ControllerBinding::pad4) mappedChords.setHeld(action.action,false,0,send);
    };
    if (profileChanged) controllers.apply(releaseMapped);
    auto nextBank=chords.getSettings();
    bool bankChanged=bankExchange.consume(nextBank,bankSerial);
    constexpr const char* octaves[] = {"pad0Octave","pad1Octave","pad2Octave","pad3Octave"};
    for (std::size_t i=0;i<4;++i) {
        auto& pad=nextBank.pads[i];
        const int inversion=juce::jlimit(0,pad.noteCount-1,mappedValues.effective[i+1]);
        const int octave=static_cast<int>(parameters.getRawParameterValue(octaves[i])->load());
        bankChanged=bankChanged || inversion!=pad.inversion || octave!=pad.octave;
        pad.inversion=inversion; pad.octave=octave;
    }
    if (bankChanged) { chords.replace(nextBank,send);mappedChords.replace(nextBank,send); }
    const int nextChannel = static_cast<int>(parameters.getRawParameterValue("channel")->load());
    const bool channelChanged = nextChannel != engine.outputChannel();
    const int nextTranspose = mappedValues.effective[0];
    const bool transposeChanged = nextTranspose != engine.semitoneOffset();
    const auto nextScale=scaleSettings();
    const bool scaleChanged=nextScale!=engine.scaleSettings();
    const bool queueFailed = performance.consumeOverflow();
    const bool panic = panicRequested.exchange(false) || channelChanged || transposeChanged || scaleChanged || queueFailed;
    const bool releaseUi = uiReleaseRequested.exchange(false);
    if (panic || releaseUi) {
        performance.discard();
        chords.releaseAll(send);
        if (panic) {
            controllers.router.reset(releaseMapped);controllers.router.learn(-1);
            mappedChords.releaseAll(send);
            inputNotes.clear();
            if (!channelChanged && !transposeChanged && !scaleChanged) engine.panic(emit);
        }
        else {
            for (int source = 1; source < e4::source::arpeggiator; ++source) engine.releaseSource(source,emit);
            emit(0xe0 | (engine.outputChannel()-1),0,64);
        }
    }
    engine.setScale(nextScale,emit);
    engine.setChannel(nextChannel,emit);
    engine.setTranspose(nextTranspose,emit);
    const int root = static_cast<int>(parameters.getRawParameterValue("chordRoot")->load());
    if (root != lastRoot) { chords.releaseAll(send);mappedChords.releaseAll(send);lastRoot = root; }
    bool stopped = false;
    if (auto* playhead = getPlayHead())
        if (const auto state = playhead->getPosition()) {
            const bool playing = state->getIsPlaying();
            stopped = wasPlaying && !playing;
            wasPlaying = playing;
        }
    if (stopped) { controllers.router.reset(releaseMapped);controllers.router.learn(-1);mappedChords.releaseAll(send);inputNotes.clear(); performance.discard(); chords.releaseAll(send); engine.panic(emit); }
    e4::PerformanceCommand command;
    for (int n = 0; n < 256 && performance.pop(command); ++n) {
        switch (command.kind) {
            case e4::PerformanceCommand::noteOn: engine.process(0x90,command.value,command.velocity,emit,1); break;
            case e4::PerformanceCommand::noteOff: engine.process(0x80,command.value,0,emit,1); break;
            case e4::PerformanceCommand::padDown: if (!bankChanged) chords.setHeld(command.value,true,root,send); break;
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
        if (++count > 2048) { controllers.router.reset(releaseMapped);mappedChords.releaseAll(send);inputNotes.clear(); chords.releaseAll(send); engine.panic(emit); break; }
        // Read raw bytes: copying a long SysEx MidiMessage could allocate here.
        if (metadata.numBytes != 3) continue;
        const auto* bytes = metadata.data;
        inputNotes.process(bytes[0],bytes[1],bytes[2]);
        bool mappingPanic=false;
        const auto mappedAction=[&](e4::MappingAction action) {
            if (action.action<=e4::ControllerBinding::pad4) mappedChords.setHeld(action.action,action.value!=0,root,send);
            else if (action.action==e4::ControllerBinding::bend) engine.process(0xe0,action.value&127,(action.value>>7)&127,emit,e4::source::firstMappedPad);
            else if (action.action==e4::ControllerBinding::panic) mappingPanic=mappingPanic || action.value!=0;
            else if (action.action==e4::ControllerBinding::transpose) {
                if (mappedValues.set(0,action.value)) { chords.releaseAll(send);mappedChords.releaseAll(send);engine.setTranspose(action.value,emit); }
            } else {
                const auto index=static_cast<std::size_t>(action.action-e4::ControllerBinding::inversion1);
                if (mappedValues.set(index+1,action.value)) {
                    auto bank=chords.getSettings();bank.pads[index].inversion=juce::jlimit(0,bank.pads[index].noteCount-1,action.value);
                    chords.replace(bank,send);mappedChords.replace(bank,send);
                }
            }
        };
        const bool consumed=controllers.router.process({bytes[0],bytes[1],bytes[2]},mappedAction,controllers.learned);
        if (mappingPanic) {
            controllers.router.reset(releaseMapped);controllers.router.learn(-1);mappedChords.releaseAll(send);
            chords.releaseAll(send);engine.panic(emit);performance.discard();inputNotes.clear();
        } else if (!consumed) engine.process(bytes[0],bytes[1],bytes[2],emit);
    }
    for (std::size_t i=0;i<5;++i) { mappedFeedback[i].store(mappedValues.effective[i]);mappedBase[i].store(mappedValues.base[i]); }
    feedbackReady.store(true);
    inputLow.store(inputNotes.low()); inputHigh.store(inputNotes.high());
    voiceCount.store(engine.activeVoices());
    rejectedReason.store(static_cast<int>(engine.lastRejection()));
    rejectedCount.store(engine.rejectionCount());
    // Internal staging is reserved. Host-owned MidiBuffer capacity is host dependent.
    midi.clear();
    midi.addEvents(output, 0, -1, 0);
}
void E4Processor::processBlockBypassed(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    audio.clear();
    midi.clear();
    inputNotes.clear(); inputLow.store(0); inputHigh.store(0);
    if (audio.getNumSamples() == 0) { panicRequested.store(true); return; }
    performance.discard();
    if (bypassed) return;
    bypassed = true;
    controllers.router.reset([](e4::MappingAction) {});controllers.router.learn(-1);
    mappedChords.releaseAll([](e4::MidiEvent,int) {});
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
    state.setProperty("schema", 6, nullptr);
    state.setProperty("controllerProfile",e4::ui::profileText(controllers.exchange.get()),nullptr);
    constexpr const char* ids[] {"transpose","pad0Inversion","pad1Inversion","pad2Inversion","pad3Inversion"};
    for (std::size_t i=0;i<5;++i) state.getChildWithProperty("id",ids[i]).setProperty("value",effectiveValue(i),nullptr);
    state.setProperty("chordBank",e4::ui::bankText(chordBank()),nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}
void E4Processor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        auto state = juce::ValueTree::fromXml(*xml);
        const int schema = static_cast<int>(state.getProperty("schema"));
        if (!state.hasType("E4State") || (schema != 1 && schema != 2 && schema != 3 && schema != 4 && schema != 5 && schema != 6)) return;
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
        e4::PerformanceSettings bank;
        if (schema >= 4 && !e4::ui::parseBank(state.getProperty("chordBank").toString(),bank)) return;
        state.setProperty("schema",6,nullptr);
        for (int i=0;i<4;++i) {
            for (const auto* suffix : {"Inversion","Octave"}) {
                const auto id="pad"+juce::String(i)+suffix;
                if (schema < 4) {
                    auto node=state.getChildWithProperty("id",id);
                    if (node.isValid()) state.removeChild(node,nullptr);
                    juce::ValueTree value("PARAM"); value.setProperty("id",id,nullptr); value.setProperty("value",0,nullptr); state.addChild(value,-1,nullptr);
                }
                const bool isInversion=juce::String(suffix)=="Inversion";
                if (!validParameter(id.toRawUTF8(),isInversion ? 0 : -2,isInversion ? 3 : 2)) return;
            }
        }
        constexpr const char* scaleIds[] = {"scaleMode","scaleTonic","scaleType","scalePads"};
        constexpr int maximums[] = {2,11,4,1};
        for (std::size_t i=0;i<4;++i) {
            const auto id=scaleIds[i];
            if (schema<5) {
                auto old=state.getChildWithProperty("id",id); if (old.isValid()) state.removeChild(old,nullptr);
                juce::ValueTree node("PARAM"); node.setProperty("id",id,nullptr); node.setProperty("value",i==2 ? 1 : 0,nullptr); state.addChild(node,-1,nullptr);
            }
            if (!validParameter(id,0,maximums[i])) return;
        }
        e4::ControllerProfile profile;
        if (schema>=6 && !e4::ui::parseProfile(state.getProperty("controllerProfile").toString(),profile)) return;
        controllers.exchange.publish(profile);
        resetMappedValues.store(true);feedbackReady.store(false);
        bankExchange.publish(bank);
        parameters.replaceState(state);
        panicRequested.store(true);
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new E4Processor; }
