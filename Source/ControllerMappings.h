#pragma once
#include "MidiSession.h"
#include "SettingsExchange.h"
#include <atomic>
namespace e4 {
struct ControllerBinding {
    enum Kind { disabled, note, cc };
    enum Action { pad1,pad2,pad3,pad4,transpose,inversion1,inversion2,inversion3,inversion4,bend,panic };
    int kind=disabled,channel=0,number=0,action=pad1;
    bool consume=true;
    int centre=64,deadband=2;
};
struct ControllerProfile {
    static constexpr std::size_t slots=8;
    std::array<ControllerBinding,slots> bindings {};
    bool valid() const noexcept {
        for (std::size_t i=0;i<slots;++i) {
            const auto& a=bindings[i];
            if (a.kind<0 || a.kind>2 || a.channel<0 || a.channel>16 || a.number<0 || a.number>127
                || a.action<0 || a.action>ControllerBinding::panic || a.centre<1 || a.centre>126 || a.deadband<0 || a.deadband>8
                || a.centre-a.deadband<=0 || a.centre+a.deadband>=127) return false;
            if (a.kind==ControllerBinding::disabled) continue;
            if (a.kind==ControllerBinding::note && a.action>=ControllerBinding::transpose && a.action<=ControllerBinding::bend) return false;
            for (std::size_t j=0;j<i;++j) {
                const auto& b=bindings[j]; if (b.kind==0) continue;
                if (a.action==b.action || (a.kind==b.kind && a.number==b.number && (a.channel==0 || b.channel==0 || a.channel==b.channel))) return false;
            }
        }
        return true;
    }
};
class ControllerPreset {
public:
    static constexpr std::size_t size=5+ControllerProfile::slots*7;
    using Bytes=std::array<std::uint8_t,size>;
    static bool encode(const ControllerProfile& settings,Bytes& out) noexcept {
        if (!settings.valid()) return false;
        Bytes bytes {}; bytes[0]='E';bytes[1]='4';bytes[2]='M';bytes[3]='P';bytes[4]=1;
        std::size_t at=5;
        for (const auto& b : settings.bindings) for (int value : {b.kind,b.channel,b.number,b.action,b.consume ? 1 : 0,b.centre,b.deadband}) bytes[at++]=static_cast<std::uint8_t>(value);
        out=bytes; return true;
    }
    static bool decode(const std::uint8_t* bytes,std::size_t length,ControllerProfile& out) noexcept {
        if (!bytes || length!=size || bytes[0]!='E' || bytes[1]!='4' || bytes[2]!='M' || bytes[3]!='P' || bytes[4]!=1) return false;
        ControllerProfile next; std::size_t at=5;
        for (auto& b : next.bindings) {
            b.kind=bytes[at++];b.channel=bytes[at++];b.number=bytes[at++];b.action=bytes[at++];
            if (bytes[at]>1) return false;
            b.consume=bytes[at++]!=0;b.centre=bytes[at++];b.deadband=bytes[at++];
        }
        if (!next.valid()) return false;
        out=next;return true;
    }
};
using ControllerExchange=ValueExchange<ControllerProfile,ControllerPreset>;
struct MappingAction { int action=0,value=0; };
// Audio/MIDI-thread owned; the UI sees only the atomic learn result.
class ControllerRouter {
public:
    ControllerRouter() { for (auto& row : blockedCC) row.fill(-1);for (auto& row : lastCC) row.fill(-1); }
    void learn(int slot) noexcept { learning=(slot>=0 && slot<8) ? slot : -1; }
    int learningSlot() const noexcept { return learning; }
    template <typename Act> void reset(Act&& act) noexcept {
        for (std::size_t i=0;i<active.size();++i) if (active[i]>0) act(MappingAction{profile.bindings[i].action,0});
        active.fill(0);
        for (auto& row : notes) for (auto& hold : row) hold.slot=-1; // Keep consumed releases until they drain.
        for (std::size_t i=0;i<8;++i) for (std::size_t channel=0;channel<16;++channel) {
            if (ccHigh[i][channel]) blockedCC[channel][static_cast<std::size_t>(profile.bindings[i].number)]=128;
            const auto& b=profile.bindings[i];
            if (b.kind==ControllerBinding::cc && b.action>=ControllerBinding::transpose && b.action<=ControllerBinding::bend
                && lastCC[channel][static_cast<std::size_t>(b.number)]>=0
                && blockedCC[channel][static_cast<std::size_t>(b.number)]<0)
                blockedCC[channel][static_cast<std::size_t>(b.number)]=lastCC[channel][static_cast<std::size_t>(b.number)];
            ccHigh[i][channel]=false;
        }
    }
    template <typename Act> bool replace(ControllerProfile next,Act&& act) noexcept {
        if (!next.valid()) return false;
        reset(act);profile=next;return true;
    }
    const ControllerProfile& settings() const noexcept { return profile; }
    static int bendValue(int value,const ControllerBinding& binding) noexcept {
        const int low=binding.centre-binding.deadband,high=binding.centre+binding.deadband;
        if (value>=low && value<=high) return 8192;
        if (value<low) return low==0 ? 0 : value*8192/low;
        return high==127 ? 16383 : 8192+(value-high)*8191/(127-high);
    }
    template <typename Act> bool process(MidiEvent event,Act&& act,std::atomic<int>& learned) noexcept {
        const int kind=event.status&0xf0,channel=event.status&15;
        if (event.status<0x80 || event.status>0xef || event.first<0 || event.first>127 || event.second<0 || event.second>127) return false;
        const bool on=kind==0x90 && event.second>0,off=kind==0x80 || (kind==0x90 && event.second==0);
        auto& hold=notes[static_cast<std::size_t>(channel)][static_cast<std::size_t>(event.first)];
        if (off && hold.count>0) {
            const bool consumed=hold.consume;
            if (--hold.count==0 && hold.slot>=0) release(hold.slot,act);
            if (hold.count==0) hold={};
            return consumed;
        }
        if (on && hold.count>0) {
            if (hold.count==UINT16_MAX) { reset(act); for (auto& row : notes) row.fill(Hold{});act(MappingAction{ControllerBinding::panic,1}); return true; }
            ++hold.count;return hold.consume;
        }
        auto& blocked=blockedCC[static_cast<std::size_t>(channel)][static_cast<std::size_t>(event.first)];
        if (kind==0xb0 && blocked>=0) {
            bool momentary=false;
            for (const auto& b : profile.bindings) if (b.kind==ControllerBinding::cc && b.number==event.first && (b.channel==0 || b.channel==channel+1))
                momentary=momentary || b.action<=ControllerBinding::pad4 || b.action==ControllerBinding::panic;
            if (blocked==128 || momentary) {
                if (event.second<=48) blocked=momentary ? -1 : event.second;
                return true; // A retired button release must not operate a replacement control.
            }
            if (event.second!=blocked) blocked=-1;else return true;
        }
        if (learning>=0 && (on || kind==0xb0)) {
            const int inputKind=on ? ControllerBinding::note : ControllerBinding::cc;
            learned.store(1+event.first+((channel+1)<<7)+(inputKind<<12)+(learning<<14));
            learning=-1;
            if (on) { hold.count=1;hold.consume=true;hold.slot=-1; }
            else blocked=event.second;
            return true; // Capture never triggers an action.
        }
        if (!on && !off && kind!=0xb0) return false;
        for (std::size_t i=0;i<profile.bindings.size();++i) {
            const auto& b=profile.bindings[i];
            if (b.kind==0 || b.number!=event.first || (b.channel!=0 && b.channel!=channel+1)
                || (b.kind==ControllerBinding::cc ? kind!=0xb0 : (!on && !off))) continue;
            if (b.kind==ControllerBinding::note) {
                if (on) { hold.count=1;hold.slot=static_cast<int>(i);hold.consume=b.consume || b.action==ControllerBinding::panic; press(static_cast<int>(i),act); }
                return b.consume || b.action==ControllerBinding::panic;
            }
            lastCC[static_cast<std::size_t>(channel)][static_cast<std::size_t>(event.first)]=event.second;
            if (b.action==ControllerBinding::transpose) act(MappingAction{b.action,(event.second*48+63)/127-24});
            else if (b.action>=ControllerBinding::inversion1 && b.action<=ControllerBinding::inversion4) act(MappingAction{b.action,event.second*4/128});
            else if (b.action==ControllerBinding::bend) act(MappingAction{b.action,bendValue(event.second,b)});
            else {
                auto& high=ccHigh[i][static_cast<std::size_t>(channel)];
                if (!high && event.second>=80) { high=true;press(static_cast<int>(i),act); }
                else if (high && event.second<=48) { high=false;release(static_cast<int>(i),act); }
            }
            return true; // CC is interpreted locally and never forwarded.
        }
        return false;
    }
private:
    template <typename Act> void press(int slot,Act&& act) noexcept {
        auto& count=active[static_cast<std::size_t>(slot)];
        if (count++==0) act(MappingAction{profile.bindings[static_cast<std::size_t>(slot)].action,1});
    }
    template <typename Act> void release(int slot,Act&& act) noexcept {
        auto& count=active[static_cast<std::size_t>(slot)];
        if (count>0 && --count==0) act(MappingAction{profile.bindings[static_cast<std::size_t>(slot)].action,0});
    }
    struct Hold { std::uint16_t count=0;int slot=-1;bool consume=true; };
    ControllerProfile profile;
    std::array<unsigned,8> active {};
    std::array<std::array<Hold,128>,16> notes {};
    std::array<std::array<bool,16>,8> ccHigh {};
    std::array<std::array<int,128>,16> blockedCC {},lastCC {};
    int learning=-1;
};
struct MappedParameters {
    using Values=std::array<int,5>; // transpose, then pad inversions.
    Values effective {},base {};
    std::array<bool,5> changed {};
    bool initialized=false;
    void begin(Values next,bool force=false) noexcept {
        for (std::size_t i=0;i<5;++i) {
            changed[i]=!initialized || force || base[i]!=next[i];
            if (changed[i]) effective[i]=next[i];
        }
        base=next;initialized=true;
    }
    bool set(std::size_t index,int value) noexcept {
        if (index>=5 || changed[index] || effective[index]==value) return false;
        effective[index]=value;return true;
    }
};
}
