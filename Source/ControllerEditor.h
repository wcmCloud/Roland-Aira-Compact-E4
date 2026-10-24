#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "ControllerService.h"
#include "StudioTheme.h"
namespace e4::ui {
inline juce::String profileText(const ControllerProfile& profile) {
    ControllerPreset::Bytes bytes; if (!ControllerPreset::encode(profile,bytes)) return {};
    return juce::Base64::toBase64(bytes.data(),bytes.size());
}
inline bool parseProfile(const juce::String& encoded,ControllerProfile& profile) {
    if (encoded.length()>128) return false;
    juce::MemoryOutputStream stream;
    return juce::Base64::convertFromBase64(stream,encoded) && ControllerPreset::decode(static_cast<const std::uint8_t*>(stream.getData()),stream.getDataSize(),profile);
}
class ControllerEditor final : public juce::Component, private juce::Timer {
public:
    ControllerEditor(ControllerProfile initial,std::function<bool(ControllerProfile)> accept,
                     std::function<void(int)> request,std::function<int()> result)
        : apply(std::move(accept)),learn(std::move(request)),takeResult(std::move(result)) {
        setLookAndFeel(&theme);
        title.setText("CONTROLLER MAPPINGS / MIDI LEARN",juce::dontSendNotification);label(title);addAndMakeVisible(title);
        const juce::StringArray headers {"INPUT","CHANNEL","NOTE / CC","ACTION","CONSUME"};
        for (int i=0;i<5;++i) { headings[static_cast<std::size_t>(i)].setText(headers[i],juce::dontSendNotification);label(headings[static_cast<std::size_t>(i)],true);addAndMakeVisible(headings[static_cast<std::size_t>(i)]); }
        for (std::size_t i=0;i<8;++i) {
            auto& row=rows[i];row.kind.addItemList({"Off","Note","CC"},1);row.channel.addItem("Any",1);
            for (int channel=1;channel<=16;++channel) row.channel.addItem(juce::String(channel),channel+1);
            row.number.setRange(0,127,1); row.number.setSliderStyle(juce::Slider::IncDecButtons);row.number.setTextBoxStyle(juce::Slider::TextBoxLeft,false,48,28);
            row.action.addItemList({"Pad 1","Pad 2","Pad 3","Pad 4","Transpose","Pad 1 inversion","Pad 2 inversion","Pad 3 inversion","Pad 4 inversion","Pitch bend","Panic"},1);
            row.learn.setButtonText("Learn");row.learn.onClick=[this,i] { learn(static_cast<int>(i));message.setText("Listening for slot "+juce::String(static_cast<int>(i)+1)+". Move a control or press a note; then review and Apply.",juce::dontSendNotification); };
            row.consume.setTooltip("Mapped notes: consume prevents the trigger note also playing. CCs are always interpreted locally.");
            for (auto* component : std::array<juce::Component*,6>{&row.kind,&row.channel,&row.number,&row.action,&row.consume,&row.learn}) addAndMakeVisible(component);
        }
        cancel.setButtonText("Cancel learn");cancel.onClick=[this] { learn(-1);message.setText("Learning cancelled.",juce::dontSendNotification); };
        commit.setButtonText("Apply mappings");commit.getProperties().set("role","primary");commit.onClick=[this] {
            ControllerProfile candidate;
            if (!read(candidate)) { message.setText("Check duplicate controls/actions, bend calibration and note targets. Notes support pads/Panic; other actions need CC.",juce::dontSendNotification);return; }
            learn(-1);
            message.setText(apply(candidate) ? "Mappings applied. Release triggers, then use a fresh gesture." : "Could not apply mappings.",juce::dontSendNotification);
        };
        load.setButtonText("Load profile...");save.setButtonText("Save profile...");
        load.onClick=[this] { chooseProfile(false); };save.onClick=[this] { chooseProfile(true); };
        reset.setButtonText("Clear all");reset.onClick=[this] { populate(ControllerProfile{});learn(-1);message.setText("Cleared for review. Apply to activate.",juce::dontSendNotification); };
        centre.setRange(1,126,1);deadband.setRange(0,8,1);
        for (auto* slider : {&centre,&deadband}) { slider->setSliderStyle(juce::Slider::IncDecButtons);slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,42,28);addAndMakeVisible(slider); }
        bendHint.setText("BEND CENTRE / DEADBAND",juce::dontSendNotification);label(bendHint,true);addAndMakeVisible(bendHint);
        hint.setText("CC pads/Panic: press >=80, release <=48. Continuous CC: transpose -24..+24, inversions 0..3. Bend centres on Panic.",juce::dontSendNotification);label(hint,true);addAndMakeVisible(hint);
        label(message,true);addAndMakeVisible(message);
        for (auto* button : {&cancel,&commit,&reset,&load,&save}) addAndMakeVisible(button);
        populate(initial);setSize(920,610);startTimerHz(20);
    }
    ~ControllerEditor() override { stopTimer();learn(-1);setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override { g.fillAll(background); }
    void resized() override {
        title.setBounds(24,16,872,30);
        const int xs[] {24,124,224,360,620},widths[] {88,88,124,248,96};
        for (int i=0;i<5;++i) headings[static_cast<std::size_t>(i)].setBounds(xs[i],55,widths[i],22);
        for (int i=0;i<8;++i) {
            auto& row=rows[static_cast<std::size_t>(i)];const int y=86+i*45;
            row.kind.setBounds(24,y,88,30);row.channel.setBounds(124,y,88,30);row.number.setBounds(224,y,124,30);
            row.action.setBounds(360,y,248,30);row.consume.setBounds(644,y,26,30);row.learn.setBounds(740,y,156,30);
        }
        bendHint.setBounds(24,456,226,22);centre.setBounds(260,452,120,30);deadband.setBounds(396,452,120,30);
        hint.setBounds(24,490,872,30);message.setBounds(24,522,872,34);
        reset.setBounds(24,568,135,30);cancel.setBounds(171,568,135,30);
        load.setBounds(318,568,170,30);save.setBounds(500,568,170,30);commit.setBounds(696,568,200,30);
    }
private:
    bool read(ControllerProfile& profile) const {
        for (std::size_t i=0;i<8;++i) {
            const auto& row=rows[i];auto& binding=profile.bindings[i];
            binding.kind=row.kind.getSelectedId()-1;binding.channel=row.channel.getSelectedId()-1;
            binding.number=static_cast<int>(row.number.getValue());binding.action=row.action.getSelectedId()-1;
            binding.consume=row.consume.getToggleState();binding.centre=static_cast<int>(centre.getValue());binding.deadband=static_cast<int>(deadband.getValue());
        }
        return profile.valid();
    }
    void populate(const ControllerProfile& profile) {
        for (std::size_t i=0;i<8;++i) {
            auto& row=rows[i];const auto& binding=profile.bindings[i];
            row.kind.setSelectedId(binding.kind+1,juce::dontSendNotification);row.channel.setSelectedId(binding.channel+1,juce::dontSendNotification);
            row.number.setValue(binding.number,juce::dontSendNotification);row.action.setSelectedId(binding.action+1,juce::dontSendNotification);row.consume.setToggleState(binding.consume,juce::dontSendNotification);
        }
        // Bend calibration belongs to the single permitted bend action.
        auto binding=profile.bindings[0];for (const auto& item : profile.bindings) if (item.kind!=0 && item.action==ControllerBinding::bend) binding=item;
        centre.setValue(binding.centre,juce::dontSendNotification);deadband.setValue(binding.deadband,juce::dontSendNotification);
    }
    void chooseProfile(bool writing) {
        ControllerProfile candidate;
        if (writing && !read(candidate)) { message.setText("Correct mapping conflicts before saving.",juce::dontSendNotification);return; }
        learn(-1);
        chooser=std::make_unique<juce::FileChooser>(writing ? "Save controller profile" : "Load controller profile",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("E4 controller.e4map"),"*.e4map");
        auto safe=juce::Component::SafePointer<ControllerEditor>(this);
        chooser->launchAsync((writing ? juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode)
            |juce::FileBrowserComponent::canSelectFiles,[safe,writing,candidate](const juce::FileChooser& dialog) {
                if (!safe) return;
                const auto file=dialog.getResult();if (file==juce::File{}) return;
                if (writing) safe->message.setText(file.replaceWithText(profileText(candidate)) ? "Profile saved; current edits are not automatically applied." : "Could not save profile.",juce::dontSendNotification);
                else {
                    ControllerProfile imported;
                    if (file.getSize()>128 || !parseProfile(file.loadFileAsString().trim(),imported)) {
                        safe->message.setText("Invalid or unsupported profile; current edits preserved.",juce::dontSendNotification);return;
                    }
                    safe->populate(imported);safe->message.setText("Profile loaded for review. Apply mappings to activate it.",juce::dontSendNotification);
                }
            });
    }
    void timerCallback() override {
        const int packed=takeResult();if (packed<=0) return;
        const int value=packed-1,slot=(value>>14)&7;
        auto& row=rows[static_cast<std::size_t>(slot)];row.kind.setSelectedId(((value>>12)&3)+1,juce::dontSendNotification);
        row.channel.setSelectedId(((value>>7)&31)+1,juce::dontSendNotification);row.number.setValue(value&127,juce::dontSendNotification);
        message.setText("Captured slot "+juce::String(slot+1)+". Choose its action, review conflicts and Apply mappings.",juce::dontSendNotification);
    }
    struct Row { juce::ComboBox kind,channel,action;juce::Slider number;juce::ToggleButton consume;juce::TextButton learn; };
    Theme theme;std::array<Row,8> rows;std::array<juce::Label,5> headings;
    juce::Label title,hint,message,bendHint;juce::TextButton cancel,commit,reset,load,save;juce::Slider centre,deadband;
    std::unique_ptr<juce::FileChooser> chooser;
    std::function<bool(ControllerProfile)> apply;std::function<void(int)> learn;std::function<int()> takeResult;
};
inline juce::DialogWindow* openControllerEditor(ControllerProfile profile,std::function<bool(ControllerProfile)> apply,
                                               std::function<void(int)> learn,std::function<int()> result) {
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new ControllerEditor(profile,std::move(apply),std::move(learn),std::move(result)));
    options.dialogTitle="E-4 MIDI Learn";options.dialogBackgroundColour=background;options.useNativeTitleBar=true;options.resizable=false;return options.launchAsync();
}
}
