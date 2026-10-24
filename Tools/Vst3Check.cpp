#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI initialiser;
    if (argc != 2) { std::cerr << "Usage: e4_vst3_check /absolute/path/to/E-4 Companion.vst3\n"; return 2; }
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions,juce::String(argv[1]));
    if (descriptions.size() != 1) { std::cerr << "Expected one VST3 plug-in type, found " << descriptions.size() << '\n'; return 1; }
    juce::String error;
    auto plugin = format.createInstanceFromDescription(*descriptions[0],48000,128,error);
    if (!plugin) { std::cerr << "VST3 load failed: " << error << '\n'; return 1; }
    int failures = 0;
    juce::Array<juce::var> results;
    const auto check = [&](bool ok,const char* name) {
        auto* item = new juce::DynamicObject();
        item->setProperty("check",name); item->setProperty("passed",ok);
        results.add(juce::var(item));
        if (!ok) ++failures;
    };
    plugin->setPlayConfigDetails(0,2,48000,128);
    plugin->prepareToPlay(48000,128);
    juce::AudioBuffer<float> audio(2,128);
    juce::MidiBuffer midi;
    midi.ensureSize(65536);
    plugin->processBlock(audio,midi); // Initial cleanup.
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(100)),10);
    midi.addEvent(juce::MidiMessage::controllerEvent(1,74,127),20);
    midi.addEvent(juce::MidiMessage::noteOff(1,60),30);
    plugin->processBlock(audio,midi);
    bool on = false,off = false,cc = false;
    for (const auto event : midi) {
        const auto message = event.getMessage();
        if (message.isNoteOn() && message.getNoteNumber() == 60 && event.samplePosition == 10) on = true;
        if (message.isNoteOff() && message.getNoteNumber() == 60 && event.samplePosition == 30) off = true;
        if (message.isController()) cc = true;
    }
    check(on && off,"VST3 note input/output preserves sample offsets");
    check(!cc,"unsupported CC is filtered through wrapper");
    check(audio.getMagnitude(0,audio.getNumSamples()) == 0,"instrument audio output is silent");
    midi.clear(); midi.addEvent(juce::MidiMessage::pitchWheel(1,16383),40);
    plugin->processBlock(audio,midi);
    bool bend = false;
    for (const auto event : midi) if (event.getMessage().isPitchWheel() && event.getMessage().getPitchWheelValue() == 16383) bend = true;
    check(bend,"VST3 pitch bend survives both wrapper directions");
    for (double rate : {44100.0,48000.0,96000.0}) {
        for (int block : {32,64,128,512,1024}) {
            plugin->releaseResources();
            plugin->setRateAndBufferSizeDetails(rate,block);
            plugin->prepareToPlay(rate,block);
            audio.setSize(2,block);
            midi.clear(); plugin->processBlock(audio,midi);
            midi.clear();
            midi.addEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(100)),1);
            midi.addEvent(juce::MidiMessage::noteOff(1,60),block-1);
            plugin->processBlock(audio,midi);
            bool started = false,ended = false;
            for (const auto event : midi) {
                if (event.getMessage().isNoteOn() && event.samplePosition == 1) started = true;
                if (event.getMessage().isNoteOff() && event.samplePosition == block-1) ended = true;
            }
            const auto label = "note timing at " + juce::String(rate,0) + " Hz / " + juce::String(block) + " samples";
            check(started && ended,label.toRawUTF8());
        }
    }
    juce::AudioProcessorParameter* transpose = nullptr;
    for (auto* parameter : plugin->getParameters())
        if (parameter->getName(128) == "Transpose") transpose = parameter;
    check(transpose != nullptr,"transpose parameter exposed to host");
    if (transpose != nullptr) transpose->setValueNotifyingHost(0.75f); // -24..24 => +12.
    midi.clear(); plugin->processBlock(audio,midi);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(100)),0);
    plugin->processBlock(audio,midi);
    bool shifted = false;
    for (const auto event : midi) if (event.getMessage().isNoteOn() && event.getMessage().getNoteNumber() == 72) shifted = true;
    check(shifted,"host transpose automation changes actual VST3 output");
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
    plugin->processBlock(audio,midi);
    bool released = false;
    for (const auto event : midi) if (event.getMessage().isNoteOff() && event.getMessage().getNoteNumber() == 72) released = true;
    check(released,"transposed VST3 note releases at actual output pitch");
    juce::MemoryBlock state;
    plugin->getStateInformation(state);
    if (transpose != nullptr) transpose->setValueNotifyingHost(0.5f);
    midi.clear(); plugin->processBlock(audio,midi);
    plugin->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    midi.clear(); plugin->processBlock(audio,midi);
    bool replayed = false;
    for (const auto event : midi) if (event.getMessage().isNoteOn()) replayed = true;
    check(!replayed,"VST3 state restore does not replay notes");
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(100)),0);
    plugin->processBlock(audio,midi);
    bool restored = false;
    for (const auto event : midi) if (event.getMessage().isNoteOn() && event.getMessage().getNoteNumber() == 72) restored = true;
    check(restored,"VST3 state round-trip restores transpose");
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
    plugin->processBlock(audio,midi);
    plugin->releaseResources(); plugin.reset();
    auto* report = new juce::DynamicObject();
    report->setProperty("plugin",descriptions[0]->name);
    report->setProperty("format","VST3"); report->setProperty("failures",failures);
    report->setProperty("checks",juce::var(results));
    report->setProperty("scope","JUCE software host smoke test; not Ableton, Steinberg validator or E-4 hardware acceptance");
    std::cout << juce::JSON::toString(juce::var(report)).toStdString() << '\n';
    return failures ? 1 : 0;
}
