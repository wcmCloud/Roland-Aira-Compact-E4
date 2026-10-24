#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "ScaleSettings.h"
#include "StudioTheme.h"
namespace e4::ui {
class ScaleControls final : public juce::Component {
public:
    ScaleControls() {
        const juce::StringArray modeNames {"Off","Reject","Nearest"};
        const juce::StringArray tonicNames {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        const juce::StringArray scaleNames {"Chromatic","Major","Natural minor","Major pentatonic","Minor pentatonic"};
        mode.addItemList(modeNames,1); tonic.addItemList(tonicNames,1); type.addItemList(scaleNames,1);
        mode.setSelectedId(1); tonic.setSelectedId(1); type.setSelectedId(2);
        pads.setButtonText("Include pads");
        int i=0;
        for (auto* title : {&modeLabel,&tonicLabel,&typeLabel}) {
            title->setText(juce::StringArray{"SCALE MODE","KEY","SCALE"}[i++],juce::dontSendNotification);
            label(*title,true); addAndMakeVisible(title);
        }
        for (auto* box : {&mode,&tonic,&type}) { addAndMakeVisible(box); box->onChange=[this] { if (onChange) onChange(); }; }
        addAndMakeVisible(pads); pads.onClick=[this] { if (onChange) onChange(); };
        mode.setTooltip("Off: unchanged. Reject: only scale notes. Nearest: snap to closest scale note; lower wins ties.");
        tonic.setTooltip("MIDI note scale tonic. Does not change the E-4 hardware AUTO PITCH key.");
        type.setTooltip("Scale is applied before global transpose. The keyboard highlights original input notes.");
        pads.setTooltip("Apply scale processing to chord pads too. Matching output pitches merge into one physical voice.");
    }
    ScaleSettings settings() const { return {mode.getSelectedId()-1,tonic.getSelectedId()-1,type.getSelectedId()-1,pads.getToggleState()}; }
    void setSettings(ScaleSettings next) {
        if (!next.valid()) return;
        mode.setSelectedId(next.mode+1,juce::dontSendNotification); tonic.setSelectedId(next.tonic+1,juce::dontSendNotification);
        type.setSelectedId(next.type+1,juce::dontSendNotification); pads.setToggleState(next.pads,juce::dontSendNotification);
    }
    void resized() override {
        const float factor=static_cast<float>(getWidth())/658.0f;
        const auto place=[factor](juce::Component& component,int x,int y,int w,int h) { component.setBounds(juce::roundToInt(static_cast<float>(x)*factor),y,juce::roundToInt(static_cast<float>(w)*factor),h); };
        place(modeLabel,0,0,132,16); place(mode,0,21,132,30);
        place(tonicLabel,144,0,86,16); place(tonic,144,21,86,30);
        place(typeLabel,242,0,230,16); place(type,242,21,230,30);
        place(pads,484,21,174,30);
    }
    juce::ComboBox mode,tonic,type;
    juce::ToggleButton pads;
    std::function<void()> onChange;
private:
    juce::Label modeLabel,tonicLabel,typeLabel;
};
}
