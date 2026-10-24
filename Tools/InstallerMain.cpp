#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <array>
#include <thread>
#include <vector>
#include <stdexcept>
#include "E4BrandData.h"

namespace {
struct Format { const char* name; const char* directory; };
const std::array<Format,3> formats {{{"E-4 Companion.vst3","Library/Audio/Plug-Ins/VST3"},
    {"E-4 Companion.component","Library/Audio/Plug-Ins/Components"},
    {"E-4 Companion.app","Applications"}}};
void command(juce::StringArray arguments) {
    juce::ChildProcess process;
    if (!process.start(arguments))
        throw std::runtime_error(("Could not start " + arguments[0]).toStdString());
    if (!process.waitForProcessToFinish(30000)) {
        process.kill();
        process.waitForProcessToFinish(5000);
        throw std::runtime_error(("Timed out: " + arguments[0]).toStdString());
    }
    if (process.getExitCode()!=0)
        throw std::runtime_error(("Operation failed: " + arguments[0] + "\n" + process.readAllProcessOutput()).toStdString());
}
juce::String install(const juce::File& source,const juce::File& home,const std::array<bool,3>& selected) {
    const auto backup = home.getChildFile("Library/Application Support/E-4 Companion/Installer Backups")
        .getNonexistentChildFile(juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S"),{},false);
    std::vector<std::pair<juce::File,juce::File>> changed;
    try {
        // Validate every selected payload before touching any existing installation.
        for (std::size_t i=0;i<formats.size();++i) if (selected[i]) {
            const auto bundle=source.getChildFile(formats[i].name);
            if (!bundle.isDirectory()) throw std::runtime_error("Keep the installer beside the app and plug-in bundles in the disk image.");
            command({"/usr/bin/codesign","--verify","--deep","--strict",bundle.getFullPathName()});
        }
        if (backup.createDirectory().failed()) throw std::runtime_error("Could not create the backup folder.");
        for (std::size_t i=0;i<formats.size();++i) if (selected[i]) {
            const auto destination=home.getChildFile(formats[i].directory).getChildFile(formats[i].name);
            if (destination.getParentDirectory().createDirectory().failed()) throw std::runtime_error("Could not create installation folder.");
            const auto previous=backup.getChildFile(formats[i].name);
            if (destination.exists() && !destination.moveFileTo(previous)) throw std::runtime_error("Could not back up an existing bundle. Quit your audio hosts and try again.");
            changed.emplace_back(destination,previous);
            command({"/usr/bin/ditto",source.getChildFile(formats[i].name).getFullPathName(),destination.getFullPathName()});
            command({"/usr/bin/codesign","--verify","--deep","--strict",destination.getFullPathName()});
        }
        return "Installed successfully. Restart Live and rescan plug-ins.\nPrevious copies, if any: " + backup.getFullPathName();
    } catch (const std::exception& error) {
        bool restored=true;
        for (auto it=changed.rbegin();it!=changed.rend();++it) {
            if (it->first.exists()) restored=it->first.moveFileTo(backup.getNonexistentChildFile("incomplete-"+it->first.getFileName(),{},false)) && restored;
            if (it->second.exists()) restored=it->second.moveFileTo(it->first) && restored;
        }
        return "Installation failed: " + juce::String(error.what()) + (restored ? "\nPrevious copies were preserved/restored." : "\nSome files could not be restored; recover them from: "+backup.getFullPathName());
    }
}
class Content final : public juce::Component {
public:
    Content() {
        title.setText("E-4 Companion",juce::dontSendNotification);title.setFont(juce::Font(juce::FontOptions(30.0f,juce::Font::bold)));
        subtitle.setText("Install for your Mac account",juce::dontSendNotification);
        detail.setText("Quit Live and other audio hosts first. Select the formats you need. Existing user copies will be backed up.",juce::dontSendNotification);
        vst.setButtonText("VST3: recommended for Ableton Live"); au.setButtonText("Audio Unit: MIDI output in Live unsupported"); app.setButtonText("Standalone companion app");
        for(auto* button : {&vst,&au,&app}) { button->setToggleState(true,juce::dontSendNotification);addAndMakeVisible(button); }
        for(auto* label : {&title,&subtitle,&detail,&status}) {addAndMakeVisible(label);label->setColour(juce::Label::textColourId,juce::Colour(0xffe5e9ef));}
        status.setText("Installs to your user plug-in folders and ~/Applications. No administrator password is needed.",juce::dontSendNotification);
        go.setButtonText("Install selected");go.setColour(juce::TextButton::buttonColourId,juce::Colour(0xffc3ef78));go.setColour(juce::TextButton::textColourOffId,juce::Colour(0xff17211c));
        go.onClick=[this] { begin(); };addAndMakeVisible(go);setSize(650,440);
    }
    ~Content() override { if(worker.joinable())worker.join(); }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171d25));g.setColour(juce::Colour(0xff27323d));g.fillRoundedRectangle(22,130,606,132,14);
        static const auto logo=[] {int size=0;const auto* data=E4BrandData::getNamedResource(E4BrandData::namedResourceList[0],size);return juce::ImageCache::getFromMemory(data,size);}();
        g.drawImageWithin(logo,24,20,64,64,juce::RectanglePlacement::centred);
    }
    void resized() override {
        title.setBounds(100,20,520,45);subtitle.setBounds(102,67,518,26);detail.setBounds(30,96,590,32);
        vst.setBounds(38,143,570,30);au.setBounds(38,182,570,30);app.setBounds(38,221,570,30);
        status.setBounds(30,277,590,91);go.setBounds(410,378,210,40);
    }
    bool isBusy() const noexcept {return busy.load();}
private:
    void begin() {
        const std::array<bool,3> selected {vst.getToggleState(),au.getToggleState(),app.getToggleState()};
        if(!selected[0]&&!selected[1]&&!selected[2]) {status.setText("Select at least one format.",juce::dontSendNotification);return;}
        if(worker.joinable())worker.join();busy.store(true);go.setEnabled(false);
        for(auto* button : {&vst,&au,&app})button->setEnabled(false);
        status.setText("Installing and verifying signatures...",juce::dontSendNotification);
        const auto source=juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
        const auto home=juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        const juce::Component::SafePointer<Content> safe(this);
        worker=std::thread([source,home,selected,safe] {
            const auto result=install(source,home,selected);
            juce::MessageManager::callAsync([safe,result] {
                if(safe) {safe->status.setText(result,juce::dontSendNotification);safe->busy.store(false);safe->go.setEnabled(true);
                    for(auto* button : {&safe->vst,&safe->au,&safe->app})button->setEnabled(true);}
            });
        });
    }
    juce::Label title,subtitle,detail,status;
    juce::ToggleButton vst,au,app;juce::TextButton go;std::thread worker;std::atomic<bool> busy {false};
};
class Installer final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override {return "E-4 Installer";}
    const juce::String getApplicationVersion() override {return "1.0.0";}
    bool moreThanOneInstanceAllowed() override {return false;}
    void initialise(const juce::String&) override {
        const auto args=getCommandLineParameterArray();
        if(args.size()==2 && args[0]=="--test-home") {
            const auto source=juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
            const auto result=install(source,juce::File(args[1]),{true,true,true});
            juce::File(args[1]).getChildFile("installer-result.txt").replaceWithText(result);
            setApplicationReturnValue(result.startsWith("Installed successfully") ? 0 : 1);quit();return;
        }
        window=std::make_unique<Window>(*this);
    }
    void shutdown() override {window.reset();}
    void systemRequestedQuit() override {if(!window || !window->content->isBusy())quit();}
private:
    struct Window final : juce::DocumentWindow {
        Window(Installer& ownerIn):DocumentWindow("E-4 Installer",juce::Colour(0xff171d25),DocumentWindow::closeButton),owner(ownerIn) {
            setUsingNativeTitleBar(true);content=new Content();setContentOwned(content,true);centreWithSize(650,440);setVisible(true);
        }
        void closeButtonPressed() override {owner.systemRequestedQuit();}
        Content* content=nullptr;Installer& owner;
    };
    std::unique_ptr<Window> window;
};
}
START_JUCE_APPLICATION(Installer)
