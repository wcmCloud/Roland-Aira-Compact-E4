#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "ChordPads.h"
#include "NoteNames.h"
#include "StudioTheme.h"
namespace e4::ui {
inline juce::String bankText(const PerformanceSettings& settings) {
    PerformancePreset::Bytes bytes;
    if (!PerformancePreset::encode(settings,bytes)) return {};
    return juce::Base64::toBase64(bytes.data(),bytes.size());
}
inline bool parseBank(const juce::String& encodedText, PerformanceSettings& settings) {
    if (encodedText.length() > 256) return false;
    juce::MemoryOutputStream stream;
    return juce::Base64::convertFromBase64(stream,encodedText)
        && PerformancePreset::decode(static_cast<const std::uint8_t*>(stream.getData()),stream.getDataSize(),settings);
}
class ChordBankEditor final : public juce::Component {
public:
    ChordBankEditor(PerformanceSettings initial, int rootNote, std::function<void(PerformanceSettings)> accepted, ScaleSettings scaleAtOpen = {})
        : root(rootNote), scale(scaleAtOpen), apply(std::move(accepted)) {
        setLookAndFeel(&theme);
        heading.setText("CHORD BANK  /  four momentary pads",juce::dontSendNotification);
        columns.setText("NAME",juce::dontSendNotification);
        offsetHeading.setText("OFFSETS (semitones)",juce::dontSendNotification);
        inversionHeading.setText("INVERSION",juce::dontSendNotification);
        octaveHeading.setText("OCTAVE",juce::dontSendNotification);
        hint.setText("Offsets: 1-4 increasing values, -24 to 24. Preview uses scale at opening; before transpose.",juce::dontSendNotification);
        for (auto* item : {&heading,&columns,&offsetHeading,&inversionHeading,&octaveHeading,&hint,&message}) { label(*item,true); addAndMakeVisible(item); }
        for (std::size_t i=0;i<4;++i) {
            auto& row=rows[i];
            row.name.setInputRestrictions(24," ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789#b-+_/()");
            row.offsets.setInputRestrictions(32,"-0123456789, ");
            for (auto* box : {&row.name,&row.offsets}) { addAndMakeVisible(box); box->onTextChange=[this] { preview(); }; }
            for (int n=0;n<4;++n) row.inversion.addItem(juce::String(n),n+1);
            for (int n=-2;n<=2;++n) row.octave.addItem(juce::String(n),n+3);
            row.inversion.onChange=[this] { preview(); }; row.octave.onChange=[this] { preview(); };
            addAndMakeVisible(row.inversion); addAndMakeVisible(row.octave); addAndMakeVisible(row.preview); label(row.preview,true);
        }
        load.setButtonText("Load bank..."); save.setButtonText("Save bank..."); reset.setButtonText("Factory bank"); commit.setButtonText("Apply bank");
        commit.getProperties().set("role","primary");
        for (auto* button : {&load,&save,&reset,&commit}) addAndMakeVisible(button);
        reset.onClick=[this] { populate(PerformanceSettings{}); };
        load.onClick=[this] { choose(false); }; save.onClick=[this] { choose(true); };
        commit.onClick=[this] {
            PerformanceSettings candidate;
            if (!read(candidate)) { message.setText("Check names and unique, increasing offsets; inversion must be below voice count.",juce::dontSendNotification); return; }
            apply(candidate);
            message.setText("Bank applied. Existing pad notes released; press a pad to play.",juce::dontSendNotification);
        };
        templates.setButtonText("Chord templates..."); addAndMakeVisible(templates);
        templates.onClick=[this] {
            juce::PopupMenu menu;
            const juce::StringArray names {"Major","Minor","Sus2","Sus4","Diminished","Augmented","Maj7","Minor7","Dominant7"};
            for (int pad=0;pad<4;++pad) {
                juce::PopupMenu choices;
                for (int kind=0;kind<names.size();++kind) choices.addItem(1+pad*10+kind,names[kind]);
                menu.addSubMenu("Pad "+juce::String(pad+1),choices);
            }
            auto safe=juce::Component::SafePointer<ChordBankEditor>(this);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&templates),[safe,names](int choice) {
                if (!safe || choice==0) return;
                const int pad=(choice-1)/10,kind=(choice-1)%10;
                const std::array<std::array<int,4>,9> offsets {{{{0,4,7,0}},{{0,3,7,0}},{{0,2,7,0}},{{0,5,7,0}},{{0,3,6,0}},{{0,4,8,0}},{{0,4,7,11}},{{0,3,7,10}},{{0,4,7,10}}}};
                if (pad>3 || kind>8) return;
                auto& row=safe->rows[static_cast<std::size_t>(pad)];
                row.name.setText(names[kind],false);
                juce::StringArray values; for (int i=0;i<(kind>=6 ? 4 : 3);++i) values.add(juce::String(offsets[static_cast<std::size_t>(kind)][static_cast<std::size_t>(i)]));
                row.offsets.setText(values.joinIntoString(", "),false);
                row.inversion.setSelectedId(1,juce::dontSendNotification); row.octave.setSelectedId(3,juce::dontSendNotification);
                safe->preview();
            });
        };
        populate(initial); setSize(760,470);
    }
    ~ChordBankEditor() override { setLookAndFeel(nullptr); }
    void paint(juce::Graphics& g) override { g.fillAll(background); }
    void resized() override {
        heading.setBounds(24,16,530,30); templates.setBounds(567,16,169,30); columns.setBounds(24,55,184,24); offsetHeading.setBounds(224,55,240,24);
        inversionHeading.setBounds(480,55,116,24); octaveHeading.setBounds(612,55,124,24);
        for (int i=0;i<4;++i) {
            auto& row=rows[static_cast<std::size_t>(i)]; const int y=86+i*65;
            row.name.setBounds(24,y,184,30); row.offsets.setBounds(224,y,240,30);
            row.inversion.setBounds(480,y,116,30); row.octave.setBounds(612,y,124,30);
            row.preview.setBounds(24,y+32,712,22);
        }
        hint.setBounds(24,348,712,22); message.setBounds(24,376,712,36);
        load.setBounds(24,424,145,30); save.setBounds(181,424,145,30);
        reset.setBounds(338,424,145,30); commit.setBounds(567,424,169,30);
    }
private:
    bool read(PerformanceSettings& settings) const {
        for (std::size_t i=0;i<4;++i) {
            const auto& row=rows[i]; auto& pad=settings.pads[i];
            const auto name=row.name.getText().trim();
            if (name.isEmpty() || name.length()>24) return false;
            pad.name.fill(0); for (int c=0;c<name.length();++c) { if (name[c]<32 || name[c]>126) return false; pad.name[static_cast<std::size_t>(c)]=static_cast<char>(name[c]); }
            auto tokens=juce::StringArray::fromTokens(row.offsets.getText(),", ",""); tokens.removeEmptyStrings();
            if (tokens.size()<1 || tokens.size()>4) return false;
            pad.offsets.fill(0); pad.noteCount=tokens.size();
            for (int n=0;n<tokens.size();++n) {
                auto token=tokens[n]; auto digits=token.startsWithChar('-') ? token.substring(1) : token;
                if (digits.isEmpty() || digits.length()>2 || !digits.containsOnly("0123456789")) return false;
                pad.offsets[static_cast<std::size_t>(n)]=token.getIntValue();
            }
            pad.inversion=row.inversion.getSelectedId()-1; pad.octave=row.octave.getSelectedId()-3;
        }
        return settings.valid();
    }
    void populate(const PerformanceSettings& settings) {
        for (std::size_t i=0;i<4;++i) {
            auto& row=rows[i]; const auto& pad=settings.pads[i];
            row.name.setText(juce::String(pad.name.data()),false);
            juce::StringArray values; for (int n=0;n<pad.noteCount;++n) values.add(juce::String(pad.offsets[static_cast<std::size_t>(n)]));
            row.offsets.setText(values.joinIntoString(", "),false);
            row.inversion.setSelectedId(pad.inversion+1,juce::dontSendNotification);
            row.octave.setSelectedId(pad.octave+3,juce::dontSendNotification);
        }
        preview();
    }
    void preview() {
        PerformanceSettings settings; const bool valid=read(settings);
        for (std::size_t i=0;i<4;++i) {
            std::array<int,4> notes; juce::StringArray labels;
            if (valid && ChordPads::voicing(settings.pads[i],root,notes)) {
                for (int note : notes) if (note>=0) {
                    const int mapped=scale.map(note,source::firstPad+static_cast<int>(i));
                    if (mapped<0) labels.add(juce::String(noteLabel(note))+" filtered");
                    else {
                        const auto name=juce::String(noteLabel(mapped));
                        if (labels.contains(name)) labels.add("merged"); else labels.add(name);
                    }
                }
                rows[i].preview.setText(labels.joinIntoString("   /   "),juce::dontSendNotification);
            } else rows[i].preview.setText(valid ? "Outside MIDI range at this root - pad will stay silent." : "Check chord values.",juce::dontSendNotification);
        }
    }
    void choose(bool writing) {
        PerformanceSettings candidate;
        if (writing && !read(candidate)) { message.setText("Correct chord values before saving.",juce::dontSendNotification); return; }
        chooser=std::make_unique<juce::FileChooser>(writing ? "Save chord bank" : "Load chord bank",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Chord bank.e4bank"),"*.e4bank");
        auto safe=juce::Component::SafePointer<ChordBankEditor>(this);
        chooser->launchAsync((writing ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode)
            | juce::FileBrowserComponent::canSelectFiles,[safe,writing,candidate](const juce::FileChooser& dialog) {
                if (!safe) return;
                auto file=dialog.getResult(); if (file==juce::File{}) return;
                if (writing) {
                    // JUCE replaces via a temporary sibling rather than truncating an existing bank.
                    const bool ok=file.replaceWithText(bankText(candidate));
                    safe->message.setText(ok ? "Bank saved." : "Could not save bank.",juce::dontSendNotification);
                } else {
                    PerformanceSettings imported;
                    if (file.getSize()>256 || !parseBank(file.loadFileAsString().trim(),imported)) {
                        safe->message.setText("Invalid or unsupported bank. Current edits preserved.",juce::dontSendNotification); return;
                    }
                    safe->populate(imported); safe->message.setText("Bank loaded for review. Press Apply bank to use it.",juce::dontSendNotification);
                }
            });
    }
    struct Row { juce::TextEditor name,offsets; juce::ComboBox inversion,octave; juce::Label preview; };
    Theme theme;
    std::array<Row,4> rows;
    juce::Label heading,columns,offsetHeading,inversionHeading,octaveHeading,hint,message;
    juce::TextButton load,save,reset,commit,templates;
    int root;
    ScaleSettings scale;
    std::function<void(PerformanceSettings)> apply;
    std::unique_ptr<juce::FileChooser> chooser;
};
inline juce::DialogWindow* openChordBank(PerformanceSettings settings,int root,std::function<void(PerformanceSettings)> apply, ScaleSettings scale = {}) {
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new ChordBankEditor(settings,root,std::move(apply),scale));
    options.dialogTitle="E-4 Chord Bank"; options.dialogBackgroundColour=background;
    options.useNativeTitleBar=true; options.resizable=false; return options.launchAsync();
}
}
