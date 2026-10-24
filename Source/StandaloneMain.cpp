#include <juce_audio_utils/juce_audio_utils.h>
#include "MidiSession.h"
#include "ChordPads.h"
#include "MidiOutputDriver.h"
#include "StudioTheme.h"
#include "PerformanceKeyboard.h"
#include "InputNoteState.h"
#include "ChordBankEditor.h"
#include "ScaleControls.h"
#include "ControllerEditor.h"

namespace {
class PortSink final : public e4::MidiSink, private juce::Thread {
public:
    explicit PortSink(std::unique_ptr<juce::MidiOutput> device)
        : Thread("E4 MIDI sender"), output(std::move(device)) {
        lastHeartbeat.store(now());
        threadReady = startThread(juce::Thread::Priority::high);
    }
    ~PortSink() override {
        signalThreadShouldExit(); notify();
        stopThread(-1); // Worker cleans actual sent notes before closing the endpoint.
    }
    bool ready() const noexcept { return threadReady; }
    void send(int status,int first,int second) override { post({e4::OutputCommand::midi,status,first,second,120}); }
    bool startClock(double bpm) {
        if (!threadReady || !std::isfinite(bpm) || bpm < 20 || bpm > 300) return false;
        heartbeat();
        return post({e4::OutputCommand::start,0,0,0,bpm});
    }
    void setTempo(double bpm) { post({e4::OutputCommand::tempo,0,0,0,bpm}); }
    void panicOutput() noexcept { driver.requestPanic(); notify(); }
    void heartbeat() noexcept { lastHeartbeat.store(now(),std::memory_order_release); }
    e4::OutputFault takeFault() noexcept { return driver.takeFault(); }
    bool clockRunning() const noexcept { return driver.clockRunning(); }
private:
    static double now() { return juce::Time::getMillisecondCounterHiRes()/1000.0; }
    bool post(e4::OutputCommand command) {
        const bool accepted = threadReady && driver.post(command);
        notify(); return accepted;
    }
    void sendOnWorker(int status,int first,int second,int size) {
        const juce::uint8 bytes[] = {static_cast<juce::uint8>(status),static_cast<juce::uint8>(first),static_cast<juce::uint8>(second)};
        output->sendMessageNow(juce::MidiMessage(bytes,size));
    }
    void run() override {
        const auto sender = [this](int status,int first,int second,int size) { sendOnWorker(status,first,second,size); };
        while (!threadShouldExit()) {
            driver.service(now(),lastHeartbeat.load(std::memory_order_acquire),sender);
            wait(driver.clockRunning() || driver.hasPending() ? 1 : 100);
        }
        driver.shutdown(sender); // Discard unsent presses; release only physically sent notes.
    }
    std::unique_ptr<juce::MidiOutput> output;
    e4::MidiOutputDriver driver;
    std::atomic<double> lastHeartbeat {0};
    bool threadReady = false;
};

class Companion final : public juce::Component, private juce::Timer,
                        private juce::MidiInputCallback, private juce::MidiKeyboardState::Listener {
public:
    explicit Companion(bool preview = false) : previewMode(preview), keyboard(keys, juce::MidiKeyboardComponent::horizontalKeyboard) {
        juce::PropertiesFile::Options options;
        options.applicationName = "E4Companion";
        options.filenameSuffix = "settings";
        options.osxLibrarySubFolder = "Application Support";
        setLookAndFeel(&theme);
        if (!previewMode) preferences.setStorageParameters(options);
        juce::PropertySet previewSettings;
        auto* settings = previewMode ? &previewSettings : preferences.getUserSettings();
        title.setText(juce::String::fromUTF8("E-4 Companion • Standalone"), juce::dontSendNotification);
        help.setText("Enable VOCODER, KEY IN and HARMONY. Audio comes from your E-4.",juce::dontSendNotification);
        inputLabel.setText("INPUT DEVICE",juce::dontSendNotification);
        outputLabel.setText("OUTPUT DEVICE",juce::dontSendNotification);
        channelLabel.setText("MIDI CHANNEL",juce::dontSendNotification);
        bendLabel.setText("PITCH BEND",juce::dontSendNotification);
        transposeLabel.setText("TRANSPOSE",juce::dontSendNotification);
        addAndMakeVisible(transposeLabel);
        for (auto* label : {&help,&inputLabel,&outputLabel,&channelLabel,&bendLabel,&rootLabel,&transposeLabel,&clockState}) e4::ui::label(*label,true);
        e4::ui::label(status); e4::ui::label(voices);
        voices.setJustificationType(juce::Justification::right);
        connect.getProperties().set("role","primary"); start.getProperties().set("role","primary");
        panic.getProperties().set("role","danger");
        for (auto* slider : {&channel,&root,&transpose,&bend,&tempo}) slider->setLookAndFeel(&theme);
        channel.setRange(1, 16, 1);
        channel.setSliderStyle(juce::Slider::IncDecButtons);
        channel.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 26);
        channel.setValue(juce::jlimit(1,16,settings->getIntValue("channel",3)), juce::dontSendNotification);
        session.setChannel(static_cast<int>(channel.getValue()));
        transpose.setRange(-24,24,1);
        transpose.setSliderStyle(juce::Slider::IncDecButtons);
        transpose.setTextBoxStyle(juce::Slider::TextBoxLeft,false,90,26);
        transpose.setTextValueSuffix(" st");
        transpose.setValue(juce::jlimit(-24,24,settings->getIntValue("transpose",0)),juce::dontSendNotification);
        session.setTranspose(static_cast<int>(transpose.getValue()));
        transpose.onValueChange = [this] {
            resetKeyboard(); clearPending(); session.setTranspose(static_cast<int>(transpose.getValue()));
            bend.setValue(8192,juce::dontSendNotification);
        };
        addAndMakeVisible(transpose);
        channel.onValueChange = [this] {
            resetKeyboard();
            clearPending();
            session.setChannel(static_cast<int>(channel.getValue()));
            bend.setValue(8192, juce::dontSendNotification);
        };
        bend.setTextBoxStyle(juce::Slider::TextBoxRight,false,76,32);
        bend.setRange(0,16383,1); e4::ui::bendStyle(bend);
        bend.setValue(8192,juce::dontSendNotification);
        bend.setDoubleClickReturnValue(true,8192);
        bend.onValueChange = [this] {
            const int value = static_cast<int>(bend.getValue());
            session.process({0xe0,value & 127,(value >> 7) & 127},1);
        };
        bend.onDragEnd = [this] { bend.setValue(8192); };
        connect.setButtonText("Connect");
        connect.onClick = [this] { connectSelected(); };
        disconnect.setButtonText("Disconnect");
        disconnect.onClick = [this] { disconnectPorts("Disconnected. Choose ports and press Connect."); };
        refresh.setButtonText("Refresh");
        refresh.onClick = [this] { refreshPorts(); };
        panic.setButtonText("Panic / release notes");
        panic.onClick = [this] {
            stopTransport();
            bend.setValue(8192,juce::dontSendNotification);
        };
        clockEnable.setButtonText("Internal clock master");
        clockEnable.setToggleState(false,juce::dontSendNotification);
        clockEnable.onClick = [this] { if (!clockEnable.getToggleState()) stopTransport(); };
        tempo.setRange(20,300,0.1);
        tempo.setValue(juce::jlimit(20.0,300.0,settings->getDoubleValue("tempo",120.0)),juce::dontSendNotification);
        tempo.setTextValueSuffix(" BPM");
        tempo.onValueChange = [this] { if (sink) sink->setTempo(tempo.getValue()); };
        start.setButtonText("Start");
        start.onClick = [this] {
            if (!sink || !clockEnable.getToggleState()) {
                status.setText("Connect an output and enable internal clock before Start.",juce::dontSendNotification); return;
            }
            if (!sink->startClock(tempo.getValue()))
                status.setText("Clock could not start. Check the connection and reconnect if needed.",juce::dontSendNotification);
        };
        stop.setButtonText("Stop"); stop.onClick = [this] { stopTransport(); };
        for (auto* c : std::array<juce::Component*,5>{&clockEnable,&tempo,&start,&stop,&clockState}) addAndMakeVisible(c);
        rootLabel.setText("CHORD ROOT",juce::dontSendNotification);
        root.setRange(36,84,1);
        root.setValue(juce::jlimit(36,84,settings->getIntValue("chordRoot",60)),juce::dontSendNotification);
        root.setSliderStyle(juce::Slider::IncDecButtons);
        root.setTextBoxStyle(juce::Slider::TextBoxLeft,false,60,26);
        e4::ui::rootStyle(root);
        root.onValueChange = [this] { releasePads(); };
        const std::array<juce::String,4> names {"Major","Minor","Sus2","Maj7"};
        for (int i = 0; i < 4; ++i) {
            auto& pad = pads[static_cast<std::size_t>(i)];
            pad.getProperties().set("role","pad"); pad.getProperties().set("index",i);
            pad.getProperties().set("detail",i == 2 ? "SUSPENDED" : i == 3 ? "FOUR VOICES" : "TRIAD");
            pad.setButtonText(names[static_cast<std::size_t>(i)]);
            pad.onStateChange = [this,i] {
                chords.setHeld(i,pads[static_cast<std::size_t>(i)].isDown(),static_cast<int>(root.getValue()),
                    [this](e4::MidiEvent event,int source) { session.process(event,source); });
            };
            addAndMakeVisible(pad);
        }
        e4::PerformanceSettings savedBank;
        if (e4::ui::parseBank(settings->getValue("chordBank"),savedBank))
            chords.replace(savedBank,[](e4::MidiEvent,int) {});
        mappedChords.replace(chords.getSettings(),[](e4::MidiEvent,int) {});
        updatePadNames();
        bankButton.setButtonText("Edit / load chord bank...");
        bankButton.onClick=[this] {
            if (bankWindow) { bankWindow->toFront(true); return; }
            auto safe=juce::Component::SafePointer<Companion>(this);
            bankWindow=e4::ui::openChordBank(chords.getSettings(),static_cast<int>(root.getValue()),[safe](e4::PerformanceSettings bank) {
                if (!safe) return;
                safe->releasePads();
                safe->chords.replace(bank,[safe](e4::MidiEvent event,int source) { if (safe) safe->session.process(event,source); });
                safe->mappedChords.replace(bank,[safe](e4::MidiEvent event,int source) { if (safe) safe->session.process(event,source); });
                safe->updatePadNames();
            },scaleControls.settings());
        };
        e4::ControllerProfile savedProfile;
        if (e4::ui::parseProfile(settings->getValue("controllerProfile"),savedProfile)) controllers.exchange.publish(savedProfile);
        mappingButton.setButtonText("MIDI learn...");
        mappingButton.onClick=[this] {
            if (mappingWindow) { mappingWindow->toFront(true);return; }
            auto safe=juce::Component::SafePointer<Companion>(this);
            mappingWindow=e4::ui::openControllerEditor(controllers.exchange.get(),
                [safe](e4::ControllerProfile profile) { return safe && safe->controllers.exchange.publish(profile); },
                [safe](int slot) { if (safe) safe->controllers.requestLearn(slot); },
                [safe] { return safe ? safe->controllers.learned.exchange(0) : 0; });
        };
        addAndMakeVisible(mappingButton);
        addAndMakeVisible(bankButton);
        addAndMakeVisible(rootLabel); addAndMakeVisible(root);
        e4::ui::keyboardStyle(keyboard);
        keyboard.setAvailableRange(36,84);
        keyboard.setScrollButtonsVisible(false);
        keyboard.setMidiChannel(1); // Separate ownership source, independent of hardware channel.
        keys.addListener(this);
        for (auto* component : std::array<juce::Component*,16>{&title,&help,&inputLabel,&outputLabel,
                &inputs,&outputs,&connect,&disconnect,&refresh,&channelLabel,&channel,&bendLabel,&bend,
                &panic,&status,&keyboard}) addAndMakeVisible(component);
        addAndMakeVisible(voices);
        addAndMakeVisible(voiceDisplay);
        if (!previewMode) refreshPorts();
        else { inputs.addItem("None",1); outputs.addItem("Choose a MIDI output",1); }
        rememberedInput = settings->getValue("input");
        rememberedOutput = settings->getValue("output");
        selectRemembered(inputs,inputDevices,rememberedInput);
        selectRemembered(outputs,outputDevices,rememberedOutput);
        inputs.onChange = [this] { rememberedInput = selectedIdentifier(inputs,inputDevices); portSelectionChanged(); };
        outputs.onChange = [this] { rememberedOutput = selectedIdentifier(outputs,outputDevices); portSelectionChanged(); };
        portSelectionChanged();
        voices.setText("0 / 4 voices",juce::dontSendNotification);
        clockState.setText("Clock stopped. Use one clock master.",juce::dontSendNotification);
        start.setEnabled(false); stop.setEnabled(false);
        connect.setEnabled(selectedIdentifier(outputs,outputDevices).isNotEmpty());
        disconnect.setEnabled(false);
        if (!previewMode) startTimer(5);
        e4::ScaleSettings savedScale {settings->getIntValue("scaleMode",0),settings->getIntValue("scaleTonic",0),settings->getIntValue("scaleType",1),settings->getBoolValue("scalePads",false)};
        if (!savedScale.valid()) savedScale={};
        scaleControls.setSettings(savedScale); session.setScale(savedScale);
        scaleControls.onChange=[this] {
            resetKeyboard(); clearPending(); session.setScale(scaleControls.settings());
            bend.setValue(8192,juce::dontSendNotification);
        };
        addAndMakeVisible(scaleControls);
        setSize(1040,850);
    }
    ~Companion() override {
        if (mappingWindow) delete mappingWindow.getComponent();
        if (bankWindow) delete bankWindow.getComponent();
        stopTimer();
        if (previewMode) { keys.removeListener(this); setLookAndFeel(nullptr); return; }
        auto* settings = preferences.getUserSettings();
        settings->setValue("controllerProfile",e4::ui::profileText(controllers.exchange.get()));
        const auto savedScale=scaleControls.settings();
        settings->setValue("scaleMode",savedScale.mode); settings->setValue("scaleTonic",savedScale.tonic);
        settings->setValue("scaleType",savedScale.type); settings->setValue("scalePads",savedScale.pads);
        settings->setValue("chordBank",e4::ui::bankText(chords.getSettings()));
        settings->setValue("channel",static_cast<int>(channel.getValue()));
        settings->setValue("chordRoot",static_cast<int>(root.getValue()));
        settings->setValue("transpose",static_cast<int>(transpose.getValue()));
        settings->setValue("tempo",tempo.getValue());
        settings->setValue("input",rememberedInput);
        settings->setValue("output",rememberedOutput);
        preferences.saveIfNeeded();
        disconnectPorts({});
        keys.removeListener(this); setLookAndFeel(nullptr);
    }
    void paint(juce::Graphics& g) override { e4::ui::surface(g,getWidth(),getHeight(),true,!activeOutput.isEmpty()); }
    void resized() override {
        const auto place = [this](juce::Component& c,int x,int y,int w,int h) { c.setBounds(e4::ui::bounds(x,y,w,h,getWidth(),getHeight(),true)); };
        title.setBounds(0,0,0,0);
        place(panic,816,28,200,36);
        place(refresh,200,115,74,28);
        place(inputLabel,44,163,230,18); place(inputs,44,187,230,38);
        place(outputLabel,44,243,230,18); place(outputs,44,267,230,38);
        place(connect,44,319,112,28); place(disconnect,164,319,110,28);
        place(clockEnable,44,426,230,28); place(tempo,44,466,230,40);
        place(start,44,528,112,36); place(stop,164,528,110,36);
        place(clockState,44,584,230,62);
        place(help,334,146,658,28);
        place(channelLabel,334,187,160,18); place(channel,334,211,160,36);
        place(mappingButton,866,211,126,36);
        place(rootLabel,514,187,160,18); place(root,514,211,160,36);
        place(transposeLabel,694,187,160,18); place(transpose,694,211,160,36);
        place(scaleControls,334,326,658,52);
        place(bendLabel,334,278,90,20); place(bend,432,267,560,40);
        for (int i = 0; i < 4; ++i) place(pads[static_cast<std::size_t>(i)],334+i*168,468,154,78);
        place(bankButton,732,425,260,28);
        voices.setBounds(0,0,0,0); place(voiceDisplay,768,597,224,22); place(keyboard,334,634,658,86);
        keyboard.setKeyWidth(static_cast<float>(keyboard.getWidth())/29.0f);
        place(status,44,771,952,38);
    }

private:
    void updatePadNames() {
        const auto& bank=chords.getSettings();
        for (std::size_t i=0;i<4;++i) {
            pads[i].setButtonText(juce::String(bank.pads[i].name.data()));
            pads[i].getProperties().set("detail",juce::String(bank.pads[i].noteCount)+" VOICES / INV "+juce::String(bank.pads[i].inversion));
        }
    }
    static juce::String selectedIdentifier(const juce::ComboBox& box, const juce::Array<juce::MidiDeviceInfo>& devices) {
        const int index = box.getSelectedId() - 2;
        return juce::isPositiveAndBelow(index,devices.size()) ? devices[index].identifier : juce::String{};
    }
    static void selectRemembered(juce::ComboBox& box, const juce::Array<juce::MidiDeviceInfo>& devices,
                                 const juce::String& identifier) {
        int match = 0, count = 0;
        for (int i = 0; i < devices.size(); ++i)
            if (devices[i].identifier == identifier) { match = i + 2; ++count; }
        box.setSelectedId(count == 1 ? match : 1,juce::dontSendNotification);
    }
    static bool contains(const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& identifier) {
        for (const auto& device : devices) if (device.identifier == identifier) return true;
        return false;
    }
    void refreshPorts() {
        const auto oldInput = selectedIdentifier(inputs,inputDevices);
        const auto oldOutput = selectedIdentifier(outputs,outputDevices);
        const auto nextInputs = juce::MidiInput::getAvailableDevices();
        const auto nextOutputs = juce::MidiOutput::getAvailableDevices();
        if (inputs.getNumItems() > 0 && nextInputs == inputDevices && nextOutputs == outputDevices) return;
        inputDevices = nextInputs;
        outputDevices = nextOutputs;
        inputs.clear(juce::dontSendNotification); outputs.clear(juce::dontSendNotification);
        inputs.addItem("None",1); outputs.addItem("None",1);
        for (int i = 0; i < inputDevices.size(); ++i) inputs.addItem(inputDevices[i].name,i+2);
        for (int i = 0; i < outputDevices.size(); ++i) outputs.addItem(outputDevices[i].name,i+2);
        selectRemembered(inputs,inputDevices,oldInput); selectRemembered(outputs,outputDevices,oldOutput);
        if ((!activeInput.isEmpty() && !contains(inputDevices,activeInput))
            || (!activeOutput.isEmpty() && !contains(outputDevices,activeOutput)))
            disconnectPorts("Device removed. Refresh and reconnect explicitly; notes will not replay.");
    }
    void portSelectionChanged() {
        const bool hasOutput = selectedIdentifier(outputs,outputDevices).isNotEmpty();
        const bool staged = selectedIdentifier(inputs,inputDevices) != activeInput
            || selectedIdentifier(outputs,outputDevices) != activeOutput;
        connect.setButtonText(sink && staged && hasOutput ? "Apply ports" : "Connect");
        connect.setEnabled(hasOutput);
        if (!hasOutput) status.setText(sink ? "No output selected. The current route remains active; use Disconnect to close it."
            : "Choose an output device, then press Connect.",juce::dontSendNotification);
        else if (staged) status.setText(sink ? "Port choices changed. Press Apply ports to switch; the current route is still active."
            : "Port selected. Press Connect to enable MIDI output.",juce::dontSendNotification);
        else if (sink) status.setText("Connected: " + outputs.getText(),juce::dontSendNotification);
    }
    void connectSelected() {
        const auto desiredInput = selectedIdentifier(inputs,inputDevices);
        const auto desiredOutput = selectedIdentifier(outputs,outputDevices);
        disconnectPorts({}); // Always release the old output before closing it.
        if (desiredOutput.isEmpty()) {
            status.setText("Choose a MIDI output before connecting.",juce::dontSendNotification); return;
        }
        auto output = juce::MidiOutput::openDevice(desiredOutput);
        if (!output) {
            status.setText("Could not open output. Check device connection or port ownership.",juce::dontSendNotification); return;
        }
        sink = std::make_unique<PortSink>(std::move(output));
        if (!sink->ready()) {
            sink.reset();
            status.setText("Could not start the MIDI sender. Try reconnecting.",juce::dontSendNotification);
            return;
        }
        session.setOutput(sink.get());
        activeOutput = desiredOutput;
        if (!desiredInput.isEmpty()) {
            input = juce::MidiInput::openDevice(desiredInput,this);
            if (!input) {
                disconnectPorts("Could not open input. Check the device and reconnect."); return;
            }
            activeInput = desiredInput;
            input->start();
        }
        connect.setButtonText("Connect");
        status.setText("Connected: " + outputs.getText() + ". Avoid MIDI echo/THRU loops.",juce::dontSendNotification);
    }
    void disconnectPorts(const juce::String& reason) {
        if (sink) sink->panicOutput();
        clockEnable.setToggleState(false,juce::dontSendNotification);
        if (input) { input->stop(); input.reset(); }
        clearPending();
        resetKeyboard();
        session.setOutput(nullptr); // Sends Note Offs and bend centre while sink is alive.
        sink.reset();
        activeInput.clear(); activeOutput.clear();
        connect.setButtonText("Connect");
        bend.setValue(8192,juce::dontSendNotification);
        if (reason.isNotEmpty()) status.setText(reason,juce::dontSendNotification);
    }
    void stopTransport() {
        if (sink) sink->panicOutput();
        clearPending(); resetKeyboard(); session.panic();
        bend.setValue(8192,juce::dontSendNotification);
    }
    void clearPending() { const juce::ScopedLock lock(queueLock); queue.clear(); }
    void releasePads() {
        mappedChords.releaseAll([this](e4::MidiEvent event,int source) { session.process(event,source); });
        chords.releaseAll([this](e4::MidiEvent event,int source) { session.process(event,source); });
    }
    void resetKeyboard() {
        controllers.router.reset([this](e4::MappingAction action) { if (action.action<=e4::ControllerBinding::pad4) mappedChords.setHeld(action.action,false,0,[this](e4::MidiEvent event,int source) { session.process(event,source); }); });
        controllers.router.learn(-1);
        releasePads(); keys.allNotesOff(0); inputNotes.clear(); keyboard.showIncoming(0,0); }
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) override {
        if (message.getRawDataSize() != 3) return;
        const auto* data = message.getRawData();
        const juce::ScopedLock lock(queueLock);
        queue.push({data[0],data[1],data[2]});
    }
    void handleNoteOn(juce::MidiKeyboardState*,int channelNumber,int note,float velocity) override {
        session.process({0x90 | (channelNumber-1),note,juce::jlimit(1,127,juce::roundToInt(velocity*127))},1);
    }
    void handleNoteOff(juce::MidiKeyboardState*,int channelNumber,int note,float) override {
        session.process({0x80 | (channelNumber-1),note,0},1);
    }
    void handleMappedAction(e4::MappingAction action) {
        const auto send=[this](e4::MidiEvent event,int source) { session.process(event,source); };
        if (action.action<=e4::ControllerBinding::pad4) mappedChords.setHeld(action.action,action.value!=0,static_cast<int>(root.getValue()),send);
        else if (action.action==e4::ControllerBinding::bend) session.process({0xe0,action.value&127,(action.value>>7)&127},e4::source::firstMappedPad);
        else if (action.action==e4::ControllerBinding::transpose) transpose.setValue(action.value);
        else if (action.action>=e4::ControllerBinding::inversion1 && action.action<=e4::ControllerBinding::inversion4) {
            auto bank=chords.getSettings();auto& pad=bank.pads[static_cast<std::size_t>(action.action-e4::ControllerBinding::inversion1)];
            const int next=juce::jlimit(0,pad.noteCount-1,action.value);
            if (pad.inversion!=next) { pad.inversion=next;chords.replace(bank,send);mappedChords.replace(bank,send);updatePadNames(); }
        }
    }
    void timerCallback() override {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastTick > 2000.0)
            disconnectPorts("App paused or computer slept. Reconnect explicitly to resume.");
        lastTick = now;
        std::array<e4::MidiEvent,256> pending {};
        std::size_t count = 0;
        bool healthy;
        { const juce::ScopedLock lock(queueLock); healthy = queue.drain(pending,count); }
        if (controllers.update()) controllers.apply([this](e4::MappingAction action) { handleMappedAction(action); });
        if (!healthy) {
            resetKeyboard(); session.panic();
            status.setText("MIDI input overflow: notes released. Reduce input traffic.",juce::dontSendNotification);
        } else {
            for (std::size_t i = 0; i < count; ++i) {
                inputNotes.process(pending[i].status,pending[i].first,pending[i].second);
                bool mappingPanic=false;
                const bool consumed=controllers.router.process(pending[i],[this,&mappingPanic](e4::MappingAction action) {
                    if (action.action==e4::ControllerBinding::panic) mappingPanic=mappingPanic || action.value!=0;
                    else handleMappedAction(action);
                },controllers.learned);
                if (mappingPanic) stopTransport();else if (!consumed) session.process(pending[i]);
            }
            keyboard.showIncoming(inputNotes.low(),inputNotes.high());
        }
        const auto outputFault = sink ? sink->takeFault() : e4::OutputFault::none;
        if (outputFault != e4::OutputFault::none) {
            disconnectPorts(outputFault == e4::OutputFault::queueOverflow
                ? "MIDI sender overloaded and released its notes. Reconnect explicitly."
                : outputFault == e4::OutputFault::heartbeat
                    ? "Interface paused: MIDI stopped and notes were released. Reconnect explicitly."
                    : "Clock deadline missed: MIDI stopped and notes were released. Reconnect explicitly.");
        }
        if (sink) sink->heartbeat();
        start.setEnabled(sink != nullptr && clockEnable.getToggleState() && !sink->clockRunning());
        stop.setEnabled(sink != nullptr);
        clockState.setText(sink && sink->clockRunning() ? "Clock: internal master running (24 PPQN)" : juce::String::fromUTF8("Clock: stopped — incoming clock is not forwarded"),juce::dontSendNotification);
        juce::MidiBuffer unused;
        keys.processNextMidiBuffer(unused,0,1,false); // Clear keyboard's indirect-event history.
        voiceDisplay.update(session.activeVoices(),session.rejectionCount(),session.lastRejection());
        disconnect.setEnabled(sink != nullptr);
        const bool canConnect = selectedIdentifier(outputs,outputDevices).isNotEmpty();
        connect.setEnabled(canConnect);
        const bool connected = sink != nullptr;
        if (connected != lastConnection) { lastConnection = connected; repaint(); }
        if (++polls == 200) { polls = 0; refreshPorts(); }
    }
    e4::ui::Theme theme;
    bool previewMode = false,lastConnection = false;
    juce::ApplicationProperties preferences;
    e4::MidiSession session;
    e4::ChordPads chords;
    e4::ChordPads mappedChords {e4::source::firstMappedPad};
    e4::ControllerService controllers;
    e4::MidiEventQueue queue;
    juce::CriticalSection queueLock;
    std::unique_ptr<PortSink> sink;
    std::unique_ptr<juce::MidiInput> input;
    juce::Array<juce::MidiDeviceInfo> inputDevices,outputDevices;
    juce::String activeInput,activeOutput,rememberedInput,rememberedOutput;
    juce::MidiKeyboardState keys;
    e4::InputNoteState inputNotes;
    e4::ui::PerformanceKeyboard keyboard;
    juce::Label title,help,inputLabel,outputLabel,channelLabel,bendLabel,status,voices;
    juce::ComboBox inputs,outputs;
    juce::Slider channel,bend,root,transpose,tempo;
    e4::ui::ScaleControls scaleControls;
    juce::ToggleButton clockEnable;
    juce::TextButton start,stop;
    juce::Label clockState;
    juce::Label rootLabel,transposeLabel;
    e4::ui::VoiceDisplay voiceDisplay;
    juce::TooltipWindow tooltips {this,600};
    std::array<juce::TextButton,4> pads;
    juce::TextButton connect,disconnect,refresh,panic,bankButton,mappingButton;
    juce::Component::SafePointer<juce::DialogWindow> bankWindow,mappingWindow;
    int polls = 0;
    double lastTick = juce::Time::getMillisecondCounterHiRes();
};

class Window final : public juce::DocumentWindow {
public:
    explicit Window(std::function<void()> close)
        : DocumentWindow("E-4 Companion",e4::ui::background,allButtons),onClose(std::move(close)) {
        setUsingNativeTitleBar(true);
        setContentOwned(new Companion(),true);
        setResizable(true,false);
        setResizeLimits(1040,850,1440,1180);
        centreWithSize(getWidth(),getHeight());
        setVisible(true);
    }
    void closeButtonPressed() override { onClose(); }
private:
    std::function<void()> onClose;
};
class App final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "E-4 Companion"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String&) override { window = std::make_unique<Window>([] { quit(); }); }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override { if (window) window->toFront(true); }
private:
    std::unique_ptr<Window> window;
};
}
#if defined(E4_UI_PREVIEW)
std::unique_ptr<juce::Component> createStandalonePreview() { return std::make_unique<Companion>(true); }
#else
START_JUCE_APPLICATION(App)
#endif
