#include <windows.h>
#include <cassert>
#include <iostream>
#include <string>
#include "vst3_sdk_minimal.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
using GetPluginFactoryProc = IPluginFactory* (SMTG_STDCALL*)();

int main() {
    char exe[MAX_PATH]{}; GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string path(exe); path=path.substr(0,path.find_last_of("\\/")+1)+"monkeys_ear_vocal.vst3";
    HMODULE module=LoadLibraryA(path.c_str()); assert(module);
    auto factory_proc=reinterpret_cast<GetPluginFactoryProc>(reinterpret_cast<void(*)()>(GetProcAddress(module,"GetPluginFactory"))); assert(factory_proc);
    IPluginFactory* factory=factory_proc(); assert(factory);
    IPluginFactory2* factory2=nullptr; assert(factory->queryInterface(IPluginFactory2_iid,reinterpret_cast<void**>(&factory2))==kResultOk);
    assert(factory2->countClasses()==2);
    PClassInfo2 component_info{}; assert(factory2->getClassInfo2(0,&component_info)==kResultOk);
    assert(std::string(component_info.name)=="Monkey's Ear Vocal"); assert(std::string(component_info.subCategories)=="Fx|Pitch Shift");
    IComponent* component=nullptr; assert(factory2->createInstance(component_info.cid,IComponent_iid,reinterpret_cast<void**>(&component))==kResultOk);
    assert(component->getBusCount(kAudio,kInput)==1 && component->getBusCount(kAudio,kOutput)==1);
    IAudioProcessor* processor=nullptr; assert(component->queryInterface(IAudioProcessor_iid,reinterpret_cast<void**>(&processor))==kResultOk);
    ProcessSetup setup{}; setup.sampleRate=48000;setup.maxSamplesPerBlock=64; assert(processor->setupProcessing(setup)==kResultOk);assert(processor->getLatencySamples()==0);
    PClassInfo2 controller_info{}; assert(factory2->getClassInfo2(1,&controller_info)==kResultOk);
    IEditController* controller=nullptr; assert(factory2->createInstance(controller_info.cid,IEditController_iid,reinterpret_cast<void**>(&controller))==kResultOk);
    assert(controller->getParameterCount()==11); ParameterInfo info{}; assert(controller->getParameterInfo(1,info)==kResultOk);assert(info.title[0]=='V' && info.title[7]=='C');
    std::cout<<"PASS standalone Vocal VST3: stereo FX, 0 samples latency, 11 Vocal controls\n";
    controller->release();processor->release();component->release();factory2->release();factory->release();FreeLibrary(module);
}
