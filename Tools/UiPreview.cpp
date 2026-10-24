#include "PluginProcessor.h"
#include "ChordBankEditor.h"
#include "ControllerEditor.h"
#include <iostream>
std::unique_ptr<juce::Component> createStandalonePreview();
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI initialiser;
    if (argc != 2) { std::cerr << "Usage: e4_ui_preview OUTPUT_FOLDER\n"; return 2; }
    const juce::File folder{juce::String(argv[1])};
    if (folder.createDirectory().failed()) return 1;
    const auto render = [&](juce::Component& component,const char* name) {
        component.setVisible(true);
        auto image = component.createComponentSnapshot(component.getLocalBounds(),true,2.0f);
        juce::FileOutputStream stream(folder.getChildFile(name));
        if (!stream.openedOk()) return false;
        stream.setPosition(0);
        if (stream.truncate().failed()) return false;
        juce::PNGImageFormat format;
        return format.writeImageToStream(image,stream);
    };
    auto standalone = createStandalonePreview();
    if (!render(*standalone,"standalone.png")) return 1;
    E4Processor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
    if (!render(*editor,"plugin.png")) return 1;
    editor.reset();
    processor.prepareToPlay(48000,128);
    juce::AudioBuffer<float> audio(2,128);
    juce::MidiBuffer midi;
    processor.processBlock(audio,midi);
    for (int note : {60,64,67}) midi.addEvent(juce::MidiMessage::noteOn(1,note,static_cast<juce::uint8>(100)),0);
    processor.processBlock(audio,midi);
    editor.reset(processor.createEditor());
    if (!render(*editor,"plugin-incoming.png")) return 1;
    e4::ui::ChordBankEditor bankEditor(e4::PerformanceSettings{},60,[](e4::PerformanceSettings) {});
    if (!render(bankEditor,"chord-bank.png")) return 1;
    e4::ui::ControllerEditor controllerEditor(e4::ControllerProfile{},[](e4::ControllerProfile) { return true; },[](int) {},[] { return 0; });
    if (!render(controllerEditor,"controller-mappings.png")) return 1;
    std::cout << "Rendered standalone and plug-in UI without opening devices or loading preferences.\n";
    return 0;
}
