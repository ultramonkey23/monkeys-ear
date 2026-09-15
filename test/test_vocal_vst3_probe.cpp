#include <windows.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include "vst3_sdk_minimal.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
using GetPluginFactoryProc = IPluginFactory* (SMTG_STDCALL*)();

namespace {
class OnePointQueue final : public IParamValueQueue {
public:
    OnePointQueue(ParamID id, ParamValue value, int32 offset=0):id_(id),value_(value),offset_(offset){}
    tresult SMTG_STDCALL queryInterface(const TUID,void** obj) override { if(obj)*obj=nullptr; return kResultFalse; }
    uint32 SMTG_STDCALL addRef() override { return 1; }
    uint32 SMTG_STDCALL release() override { return 1; }
    ParamID SMTG_STDCALL getParameterId() override { return id_; }
    int32 SMTG_STDCALL getPointCount() override { return 1; }
    tresult SMTG_STDCALL getPoint(int32 index,int32& sampleOffset,ParamValue& value) override { if(index!=0)return kInvalidArgument; sampleOffset=offset_;value=value_;return kResultOk; }
    tresult SMTG_STDCALL addPoint(int32,ParamValue,int32&) override { return kNotImplemented; }
private: ParamID id_; ParamValue value_; int32 offset_;
};
class OneParameterChange final : public IParameterChanges {
public:
    OneParameterChange(ParamID id,ParamValue value,int32 offset=0):queue_(id,value,offset){}
    tresult SMTG_STDCALL queryInterface(const TUID,void** obj) override { if(obj)*obj=nullptr; return kResultFalse; }
    uint32 SMTG_STDCALL addRef() override { return 1; }
    uint32 SMTG_STDCALL release() override { return 1; }
    int32 SMTG_STDCALL getParameterCount() override { return 1; }
    IParamValueQueue* SMTG_STDCALL getParameterData(int32 index) override { return index==0?&queue_:nullptr; }
    IParamValueQueue* SMTG_STDCALL addParameterData(const ParamID&,int32&) override { return nullptr; }
private: OnePointQueue queue_;
};

void process_block(IAudioProcessor* processor, int block_size, IParameterChanges* changes, float phase, float* rms_out) {
    float in_l[256]{},in_r[256]{},out_l[256]{},out_r[256]{};
    assert(block_size<=256);
    for(int i=0;i<block_size;++i){const float t=phase+static_cast<float>(i);in_l[i]=in_r[i]=0.35f*std::sin(2.0f*3.14159265358979323846f*220.0f*t/48000.0f);}
    float* in_ch[2]{in_l,in_r};float* out_ch[2]{out_l,out_r};
    AudioBusBuffers inputs{};inputs.numChannels=2;inputs.channelBuffers32=in_ch;
    AudioBusBuffers outputs{};outputs.numChannels=2;outputs.channelBuffers32=out_ch;
    ProcessData data{};data.numSamples=block_size;data.numInputs=1;data.numOutputs=1;data.inputs=&inputs;data.outputs=&outputs;data.inputParameterChanges=changes;
    assert(processor->process(data)==kResultOk);
    double sum=0.0;for(int i=0;i<block_size;++i){const double d=out_l[i]-in_l[i];sum+=d*d;}*rms_out=static_cast<float>(std::sqrt(sum/block_size));
}
}

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
    ProcessSetup setup{}; setup.sampleRate=48000;setup.maxSamplesPerBlock=256; assert(processor->setupProcessing(setup)==kResultOk);assert(processor->getLatencySamples()==0);
    PClassInfo2 controller_info{}; assert(factory2->getClassInfo2(1,&controller_info)==kResultOk);
    IEditController* controller=nullptr; assert(factory2->createInstance(controller_info.cid,IEditController_iid,reinterpret_cast<void**>(&controller))==kResultOk);
    assert(controller->getParameterCount()==11); ParameterInfo info{}; assert(controller->getParameterInfo(1,info)==kResultOk);assert(info.title[0]=='V' && info.title[7]=='C');

    // Host-style process-time automation must reach the DSP, not merely update stored state.
    // Mix=0 is dry by contract; Mix=1 exercises the active vocal path after warm-up.
    for(int block_size : {32,64,128,256}) {
        processor->setupProcessing(ProcessSetup{0,0,block_size,48000.0});
        OneParameterChange dry_mix(8,0.0); float dry_delta=0.0f;
        process_block(processor,block_size,&dry_mix,0.0f,&dry_delta);
        assert(dry_delta < 1.0e-5f);
        OneParameterChange wet_mix(8,1.0); float wet_delta=0.0f;
        for(int b=0;b<16;++b) process_block(processor,block_size,b==0?static_cast<IParameterChanges*>(&wet_mix):nullptr,static_cast<float>((b+1)*block_size),&wet_delta);
        assert(wet_delta > 1.0e-5f);
    }

    std::cout<<"PASS standalone Vocal VST3: stereo FX, 0 samples latency, 11 Vocal controls, host automation reaches DSP at 32/64/128/256 frames\n";
    controller->release();processor->release();component->release();factory2->release();factory->release();FreeLibrary(module);
}
