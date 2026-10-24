#include "ControllerMappings.h"
#include <iostream>
#include <vector>
int main() {
    int failures=0;
    const auto check=[&](bool ok,const char* label) { if (!ok) { ++failures;std::cerr<<"FAIL: "<<label<<'\n'; } };
    using B=e4::ControllerBinding;
    e4::ControllerProfile profile;profile.bindings[0]={B::note,0,48,B::pad1,true,64,2};
    check(profile.valid(),"note pad valid");
    auto invalid=profile;invalid.bindings[1]=profile.bindings[0];invalid.bindings[1].action=B::pad2;
    check(!invalid.valid(),"overlapping omni input rejected");
    invalid.bindings[1].number=49;invalid.bindings[1].action=B::pad1;
    check(!invalid.valid(),"duplicate target rejected");
    invalid=profile;invalid.bindings[0].action=B::transpose;check(!invalid.valid(),"continuous action requires CC");
    e4::ControllerPreset::Bytes bytes {};
    check(e4::ControllerPreset::encode(profile,bytes),"profile encoding");
    e4::ControllerProfile decoded;
    check(e4::ControllerPreset::decode(bytes.data(),bytes.size(),decoded) && decoded.bindings[0].number==48,"profile round-trip");
    for (std::size_t size=0;size<bytes.size();++size) check(!e4::ControllerPreset::decode(bytes.data(),size,decoded),"truncated profile rejected");
    auto broken=bytes;broken[4]=2;check(!e4::ControllerPreset::decode(broken.data(),broken.size(),decoded),"future schema rejected");
    for (std::size_t field=5;field<12;++field) { broken=bytes;broken[field]=255;check(!e4::ControllerPreset::decode(broken.data(),broken.size(),decoded) && decoded.bindings[0].number==48,"invalid field rejected without replacement"); }
    std::vector<e4::MappingAction> actions;
    const auto act=[&](e4::MappingAction action) { actions.push_back(action); };
    e4::ControllerRouter router;std::atomic<int> learned {0};router.replace(profile,act);
    check(router.process({0x90,48,100},act,learned) && actions.size()==1 && actions[0].value==1,"consumed note triggers pad once");
    router.process({0x90,48,100},act,learned);router.process({0x91,48,100},act,learned);
    router.process({0x80,48,0},act,learned);router.process({0x81,48,0},act,learned);
    check(actions.size()==1,"repeats and overlapping channels preserve pad hold");
    router.process({0x90,48,0},act,learned);check(actions.size()==2 && actions.back().value==0,"final velocity-zero release clears pad");
    profile.bindings[0].consume=false;router.replace(profile,act);actions.clear();
    check(!router.process({0x90,48,100},act,learned),"pass-through trigger also leaves raw note available");
    auto replacement=profile;replacement.bindings[0].number=49;replacement.bindings[0].action=B::pad2;
    router.replace(replacement,act);actions.clear();
    check(!router.process({0x80,48,0},act,learned) && actions.empty(),"mapping edits retain old pass-through release policy");
    router.process({0x90,49,100},act,learned);router.replace(profile,act);actions.clear();
    check(!router.process({0x80,49,0},act,learned) && actions.empty(),"retired trigger cannot release replacement action");
    profile.bindings[0].consume=true;router.replace(profile,act);actions.clear();router.learn(2);
    check(router.process({0x90,48,100},act,learned) && actions.empty() && learned.load()>0,"learning consumes capture without triggering existing mapping");
    const int capture=learned.exchange(0)-1;
    check(((capture>>14)&7)==2 && ((capture>>12)&3)==B::note && ((capture>>7)&31)==1 && (capture&127)==48,"learn result preserves slot/type/channel/note");
    router.process({0x90,48,100},act,learned);router.process({0x80,48,0},act,learned);
    check(actions.empty(),"captured repeated hold waits for final release");router.process({0x80,48,0},act,learned);
    router.process({0x90,48,100},act,learned);check(actions.size()==1,"fresh gesture after capture triggers");
    router.reset(act);router.learn(-1);actions.clear();
    profile={};profile.bindings[0]={B::cc,2,20,B::pad1,true,64,2};router.replace(profile,act);
    router.process({0xb0,20,127},act,learned);check(actions.empty(),"channel-specific mapping ignores other channels");
    router.process({0xb1,20,127},act,learned);router.process({0xb1,20,127},act,learned);router.process({0xb1,20,64},act,learned);
    check(actions.size()==1,"CC threshold hysteresis suppresses repeated highs");
    router.process({0xb1,20,48},act,learned);check(actions.size()==2 && actions.back().value==0,"CC threshold release");
    router.process({0xb1,20,100},act,learned);router.reset(act);actions.clear();router.process({0xb1,20,127},act,learned);
    check(actions.empty(),"held CC cannot restart pad after Panic");router.process({0xb1,20,0},act,learned);router.process({0xb1,20,127},act,learned);
    check(actions.size()==1,"CC rearmed by release after Panic");
    router.reset(act);actions.clear();profile.bindings[0].action=B::transpose;router.replace(profile,act);
    router.process({0xb1,20,0},act,learned);actions.clear();router.learn(0);
    router.process({0xb1,20,100},act,learned);check(actions.empty(),"CC learn suppresses captured action");
    router.replace(profile,act);router.process({0xb1,20,100},act,learned);check(actions.empty(),"captured CC repeat suppressed after Apply");
    router.process({0xb1,20,101},act,learned);check(actions.size()==1,"continuous CC accepts fresh changed value without requiring zero");
    router.process({0xb1,20,0},act,learned);check(actions.back().value==-24,"transpose CC lower endpoint");
    router.process({0xb1,20,127},act,learned);check(actions.back().value==24,"transpose CC upper endpoint");
    B bend;int last=-1;
    for (int value=0;value<128;++value) { const int mapped=e4::ControllerRouter::bendValue(value,bend);check(mapped>=last && mapped>=0 && mapped<=16383,"bend mapping monotonic and bounded");last=mapped; }
    check(e4::ControllerRouter::bendValue(0,bend)==0 && e4::ControllerRouter::bendValue(127,bend)==16383 && e4::ControllerRouter::bendValue(64,bend)==8192,"bend endpoints and centre");
    e4::MappedParameters parameters;parameters.begin({{0,0,0,0,0}});
    check(!parameters.set(0,12),"initial host state wins same block");parameters.begin({{0,0,0,0,0}});
    check(parameters.set(0,12) && parameters.effective[0]==12,"controller override after initial block");
    parameters.begin({{0,0,0,0,0}});check(parameters.effective[0]==12,"controller value persists until host change");
    parameters.begin({{5,0,0,0,0}});check(!parameters.set(0,24) && parameters.effective[0]==5,"changed host automation wins same block");
    e4::ControllerRouter retired;profile={};profile.bindings[0]={B::cc,1,22,B::pad1,true,64,2};retired.replace(profile,act);actions.clear();
    retired.process({0xb0,22,127},act,learned);profile.bindings[0].action=B::transpose;retired.replace(profile,act);actions.clear();
    retired.process({0xb0,22,0},act,learned);retired.process({0xb0,22,0},act,learned);
    check(actions.empty(),"retired CC release cannot operate replacement continuous action");
    retired.process({0xb0,22,1},act,learned);check(actions.size()==1,"replacement continuous action requires fresh changed gesture");
    profile={};profile.bindings[0]={B::note,1,44,B::panic,false,64,2};retired.replace(profile,act);actions.clear();
    check(retired.process({0x90,44,100},act,learned) && retired.process({0x90,44,100},act,learned)
        && retired.process({0x80,44,0},act,learned) && retired.process({0x80,44,0},act,learned),"Panic consumes entire repeated trigger even with pass-through selected");
    std::cout<<"Controller mappings: "<<failures<<" failures\n";return failures ? 1 : 0;
}
