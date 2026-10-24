#include <juce_audio_utils/juce_audio_utils.h>
#include "MidiSession.h"
#include "ChordPads.h"
#include "ClockScheduler.h"

namespace {
class PortSink final : public e4::MidiSink, private juce::Thread {
public:
    explicit PortSink(std::unique_ptr<juce::MidiOutput> device)
        : Thread("E4 MIDI clock"), output(std::move(device)) {
        threadReady = startThread(juce::Thread::Priority::high);
    }
    ~PortSink() override {
        signalThreadShouldExit(); notify();
        stopThread(-1); // Join before the endpoint or scheduler is destroyed.
        stopClock();
    }
    void send(int status, int first, int second) override {
        const juce::ScopedLock lock(portLock);
        const juce::uint8 bytes[] = {static_cast<juce::uint8>(status),
            static_cast<juce::uint8>(first), static_cast<juce::uint8>(second)};
        output->sendMessageNow(juce::MidiMessage(bytes, 3));
    }
    bool startClock(double bpm) {
        const juce::ScopedLock lock(portLock);
        if (!threadReady || !clock.setTempo(bpm,now())) return false;
        faultReported = false; watchdogFault = false;
        lastHeartbeat = now();
        clock.start(now(),[this](int status) { sendRealtime(status); });
        notify();
        return true;
    }
    void setTempo(double bpm) {
        const juce::ScopedLock lock(portLock);
        clock.poll(now(),[this](int status) { sendRealtime(status); });
        clock.setTempo(bpm,now());
        notify();
    }
    void stopClock() {
        const juce::ScopedLock lock(portLock);
        clock.stop([this](int status) { sendRealtime(status); });
    }
    void heartbeat() {
        const juce::ScopedLock lock(portLock);
        lastHeartbeat = now();
    }
    bool takeClockFault() {
        const juce::ScopedLock lock(portLock);
        if (!faultReported && (clock.hasFault() || watchdogFault)) { faultReported = true; return true; }
        return false;
    }
    bool clockRunning() {
        const juce::ScopedLock lock(portLock);
        return clock.isRunning();
    }
private:
    static double now() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }
    void sendRealtime(int status) {
        const auto byte = static_cast<juce::uint8>(status);
        output->sendMessageNow(juce::MidiMessage(&byte,1));
    }
    void run() override {
        while (!threadShouldExit()) {
            int sleepMilliseconds = 100;
            { const juce::ScopedLock lock(portLock);
              const double time = now();
              if (clock.isRunning() && time - lastHeartbeat > 2.0) {
                  clock.stop([this](int status) { sendRealtime(status); }); watchdogFault = true;
              }
              clock.poll(time,[this](int status) { sendRealtime(status); });
              sleepMilliseconds = clock.isRunning() ? 1 : 100;
            }
            wait(sleepMilliseconds); // Deadline accumulation is absolute; OS timing is still best effort.
        }
    }
    std::unique_ptr<juce::MidiOutput> output;
    juce::CriticalSection portLock;
    e4::ClockScheduler clock;
    bool threadReady = false;
    bool faultReported = false,watchdogFault = false;
    double lastHeartbeat = 0;
};

class Companion final : public juce::Component, private juce::Timer,
                        private juce::MidiInputCallback, private juce::MidiKeyboardState::Listener {
public:
    Companion() : keyboard(keys, juce::MidiKeyboardComponent::horizontalKeyboard) {
        juce::PropertiesFile::Options options;
        options.applicationName = "E4Companion";
        options.filenameSuffix = "settings";
        options.osxLibrarySubFolder = "Application Support";
        preferences.setStorageParameters(options);
        auto* settings = preferences.getUserSettings();
        title.setText(juce::String::fromUTF8("E-4 Companion • Standalone"), juce::dontSendNotification);
        help.setText("Enable VOCODER + KEY IN + HARMONY on the E-4. Match its MIDI channel.\n"
                     "Choose a MIDI output and press Connect. Audio stays on the hardware.\n"
                     "Keep internal clock off when Live or another device is the clock master.", juce::dontSendNotification);
        inputLabel.setText("MIDI input", juce::dontSendNotification);
        outputLabel.setText("MIDI output", juce::dontSendNotification);
        channelLabel.setText("Hardware channel", juce::dontSendNotification);
        bendLabel.setText("Pitch bend", juce::dontSendNotification);
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
        bend.setRange(0,16383,1);
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
        refresh.setButtonText("Refresh ports");
        refresh.onClick = [this] { refreshPorts(); };
        panic.setButtonText("Panic / release notes");
        panic.onClick = [this] {
            stopTransport();
            bend.setValue(8192,juce::dontSendNotification);
        };
        clockEnable.setButtonText("Internal clock (sole master)");
        clockEnable.setToggleState(false,juce::dontSendNotification);
        clockEnable.onClick = [this] { if (!clockEnable.getToggleState()) stopTransport(); };
        tempo.setRange(20,300,0.1);
        tempo.setValue(juce::jlimit(20.0,300.0,settings->getDoubleValue("tempo",120.0)),juce::dontSendNotification);
        tempo.setTextValueSuffix(" BPM");
        tempo.onValueChange = [this] { if (sink) sink->setTempo(tempo.getValue()); };
        start.setButtonText("Start (restart)");
        start.onClick = [this] {
            if (!sink || !clockEnable.getToggleState()) {
                status.setText("Connect an output and enable internal clock before Start.",juce::dontSendNotification); return;
            }
            if (!sink->startClock(tempo.getValue()))
                status.setText("Could not start timing thread. Clock remains stopped.",juce::dontSendNotification);
        };
        stop.setButtonText("Stop"); stop.onClick = [this] { stopTransport(); };
        for (auto* c : std::array<juce::Component*,5>{&clockEnable,&tempo,&start,&stop,&clockState}) addAndMakeVisible(c);
        rootLabel.setText("Chord root (MIDI note)",juce::dontSendNotification);
        root.setRange(36,84,1);
        root.setValue(juce::jlimit(36,84,settings->getIntValue("chordRoot",60)),juce::dontSendNotification);
        root.setSliderStyle(juce::Slider::IncDecButtons);
        root.setTextBoxStyle(juce::Slider::TextBoxLeft,false,60,26);
        root.onValueChange = [this] { releasePads(); };
        const std::array<juce::String,4> names {"Major","Minor","Sus2","Maj7"};
        for (int i = 0; i < 4; ++i) {
            auto& pad = pads[static_cast<std::size_t>(i)];
            pad.setButtonText(names[static_cast<std::size_t>(i)]);
            pad.onStateChange = [this,i] {
                chords.setHeld(i,pads[static_cast<std::size_t>(i)].isDown(),static_cast<int>(root.getValue()),
                    [this](e4::MidiEvent event,int source) { session.process(event,source); });
            };
            addAndMakeVisible(pad);
        }
        addAndMakeVisible(rootLabel); addAndMakeVisible(root);
        keyboard.setAvailableRange(36,84);
        keyboard.setScrollButtonsVisible(false);
        keyboard.setMidiChannel(1); // Separate ownership source, independent of hardware channel.
        keys.addListener(this);
        for (auto* component : std::array<juce::Component*,16>{&title,&help,&inputLabel,&outputLabel,
                &inputs,&outputs,&connect,&disconnect,&refresh,&channelLabel,&channel,&bendLabel,&bend,
                &panic,&status,&keyboard}) addAndMakeVisible(component);
        addAndMakeVisible(voices);
        refreshPorts();
        rememberedInput = settings->getValue("input");
        rememberedOutput = settings->getValue("output");
        selectRemembered(inputs,inputDevices,rememberedInput);
        selectRemembered(outputs,outputDevices,rememberedOutput);
        inputs.onChange = [this] { rememberedInput = selectedIdentifier(inputs,inputDevices); };
        outputs.onChange = [this] { rememberedOutput = selectedIdentifier(outputs,outputDevices); };
        status.setText("Disconnected. Restored choices never auto-connect or replay notes.",juce::dontSendNotification);
        startTimer(5);
        setSize(740,630);
    }
    ~Companion() override {
        stopTimer();
        auto* settings = preferences.getUserSettings();
        settings->setValue("channel",static_cast<int>(channel.getValue()));
        settings->setValue("chordRoot",static_cast<int>(root.getValue()));
        settings->setValue("transpose",static_cast<int>(transpose.getValue()));
        settings->setValue("tempo",tempo.getValue());
        settings->setValue("input",rememberedInput);
        settings->setValue("output",rememberedOutput);
        preferences.saveIfNeeded();
        disconnectPorts({});
        keys.removeListener(this);
    }
    void resized() override {
        auto area = getLocalBounds().reduced(20);
        title.setBounds(area.removeFromTop(30));
        help.setBounds(area.removeFromTop(74));
        auto row = area.removeFromTop(32);
        inputLabel.setBounds(row.removeFromLeft(110)); inputs.setBounds(row);
        area.removeFromTop(8); row = area.removeFromTop(32);
        outputLabel.setBounds(row.removeFromLeft(110)); outputs.setBounds(row);
        area.removeFromTop(10); row = area.removeFromTop(30);
        connect.setBounds(row.removeFromLeft(130)); row.removeFromLeft(8);
        disconnect.setBounds(row.removeFromLeft(130)); row.removeFromLeft(8);
        refresh.setBounds(row.removeFromLeft(130));
        area.removeFromTop(12); row = area.removeFromTop(30);
        channelLabel.setBounds(row.removeFromLeft(140)); channel.setBounds(row.removeFromLeft(110));
        row.removeFromLeft(16); panic.setBounds(row.removeFromLeft(210));
        area.removeFromTop(8); row = area.removeFromTop(32);
        bendLabel.setBounds(row.removeFromLeft(110)); bend.setBounds(row);
        status.setBounds(area.removeFromTop(30));
        voices.setBounds(area.removeFromTop(24));
        row = area.removeFromTop(30);
        rootLabel.setBounds(row.removeFromLeft(180)); root.setBounds(row.removeFromLeft(120)); row.removeFromLeft(20); transpose.setBounds(row.removeFromLeft(180));
        area.removeFromTop(6); row = area.removeFromTop(34);
        for (auto& pad : pads) { pad.setBounds(row.removeFromLeft(160).reduced(3,0)); }
        area.removeFromTop(8); row = area.removeFromTop(32);
        clockEnable.setBounds(row.removeFromLeft(250)); tempo.setBounds(row.removeFromLeft(190));
        start.setBounds(row.removeFromLeft(135)); stop.setBounds(row);
        clockState.setBounds(area.removeFromTop(26));
        area.removeFromTop(8);
        keyboard.setBounds(area.reduced(0,4));
    }
private:
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
        status.setText("Connected: " + outputs.getText() + ". Avoid MIDI echo/THRU loops.",juce::dontSendNotification);
    }
    void disconnectPorts(const juce::String& reason) {
        if (sink) sink->stopClock();
        clockEnable.setToggleState(false,juce::dontSendNotification);
        if (input) { input->stop(); input.reset(); }
        clearPending();
        resetKeyboard();
        session.setOutput(nullptr); // Sends Note Offs and bend centre while sink is alive.
        sink.reset();
        activeInput.clear(); activeOutput.clear();
        bend.setValue(8192,juce::dontSendNotification);
        if (reason.isNotEmpty()) status.setText(reason,juce::dontSendNotification);
    }
    void stopTransport() {
        if (sink) sink->stopClock();
        clearPending(); resetKeyboard(); session.panic();
        bend.setValue(8192,juce::dontSendNotification);
    }
    void clearPending() { const juce::ScopedLock lock(queueLock); queue.clear(); }
    void releasePads() {
        chords.releaseAll([this](e4::MidiEvent event,int source) { session.process(event,source); });
    }
    void resetKeyboard() { releasePads(); keys.allNotesOff(0); }
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
    void timerCallback() override {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastTick > 2000.0)
            disconnectPorts("App paused or Mac slept. Reconnect explicitly to resume.");
        lastTick = now;
        std::array<e4::MidiEvent,256> pending {};
        std::size_t count = 0;
        bool healthy;
        { const juce::ScopedLock lock(queueLock); healthy = queue.drain(pending,count); }
        if (!healthy) {
            resetKeyboard(); session.panic();
            status.setText("MIDI input overflow: notes released. Reduce input traffic.",juce::dontSendNotification);
        } else {
            for (std::size_t i = 0; i < count; ++i) session.process(pending[i]);
        }
        if (sink && sink->takeClockFault()) {
            stopTransport();
            clockEnable.setToggleState(false,juce::dontSendNotification);
            status.setText("Clock missed a deadline and stopped. Disable other masters, then explicitly restart.",juce::dontSendNotification);
        }
        if (sink) sink->heartbeat();
        start.setEnabled(sink != nullptr && clockEnable.getToggleState() && !sink->clockRunning());
        stop.setEnabled(sink != nullptr);
        clockState.setText(sink && sink->clockRunning() ? "Clock: internal master running (24 PPQN)" : juce::String::fromUTF8("Clock: stopped — incoming clock is not forwarded"),juce::dontSendNotification);
        juce::MidiBuffer unused;
        keys.processNextMidiBuffer(unused,0,1,false); // Clear keyboard's indirect-event history.
        voices.setText("Active voices: " + juce::String(session.activeVoices()) + juce::String::fromUTF8(" / 4 — keyboard and pads share this limit"),juce::dontSendNotification);
        if (++polls == 200) { polls = 0; refreshPorts(); }
    }
    juce::ApplicationProperties preferences;
    e4::MidiSession session;
    e4::ChordPads chords;
    e4::MidiEventQueue queue;
    juce::CriticalSection queueLock;
    std::unique_ptr<PortSink> sink;
    std::unique_ptr<juce::MidiInput> input;
    juce::Array<juce::MidiDeviceInfo> inputDevices,outputDevices;
    juce::String activeInput,activeOutput,rememberedInput,rememberedOutput;
    juce::MidiKeyboardState keys;
    juce::MidiKeyboardComponent keyboard;
    juce::Label title,help,inputLabel,outputLabel,channelLabel,bendLabel,status,voices;
    juce::ComboBox inputs,outputs;
    juce::Slider channel,bend,root,transpose,tempo;
    juce::ToggleButton clockEnable;
    juce::TextButton start,stop;
    juce::Label clockState;
    juce::Label rootLabel;
    std::array<juce::TextButton,4> pads;
    juce::TextButton connect,disconnect,refresh,panic;
    int polls = 0;
    double lastTick = juce::Time::getMillisecondCounterHiRes();
};

class Window final : public juce::DocumentWindow {
public:
    explicit Window(std::function<void()> close)
        : DocumentWindow("E-4 Companion",juce::Colour(0xff20252b),allButtons),onClose(std::move(close)) {
        setUsingNativeTitleBar(true);
        setContentOwned(new Companion(),true);
        setResizable(true,false);
        setResizeLimits(740,630,1100,900);
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
START_JUCE_APPLICATION(App)
