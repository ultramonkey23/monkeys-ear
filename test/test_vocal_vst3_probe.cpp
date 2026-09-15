#include <windows.h>
#include <cmath>
#include <iostream>
#include <string>
#include "vst3_sdk_minimal.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
using GetPluginFactoryProc = IPluginFactory* (SMTG_STDCALL*)();

#define CHECK_STAGE(condition, stage) do { if (!(condition)) { std::cerr << "[FAIL] " << stage << "\n"; return 1; } else { std::cout << "[PASS] " << stage << "\n"; } } while (0)

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

bool process_block(IAudioProcessor* processor, int block_size, IParameterChanges* changes, float phase, float* rms_out) {
    if (!processor || !rms_out || block_size<=0 || block_size>256) return false;
    float in_l[256]{},in_r[256]{},out_l[256]{},out_r[256]{};
    for(int i=0;i<block_size;++i){const float t=phase+static_cast<float>(i);in_l[i]=in_r[i]=0.35f*std::sin(2.0f*3.14159265358979323846f*220.0f*t/48000.0f);}
    float* in_ch[2]{in_l,in_r};float* out_ch[2]{out_l,out_r};
    AudioBusBuffers inputs{};inputs.numChannels=2;inputs.channelBuffers32=in_ch;
    AudioBusBuffers outputs{};outputs.numChannels=2;outputs.channelBuffers32=out_ch;
    ProcessData data{};data.numSamples=block_size;data.numInputs=1;data.numOutputs=1;data.inputs=&inputs;data.outputs=&outputs;data.inputParameterChanges=changes;
    if (processor->process(data)!=kResultOk) return false;
    double sum=0.0;for(int i=0;i<block_size;++i){const double d=out_l[i]-in_l[i];sum+=d*d;}*rms_out=static_cast<float>(std::sqrt(sum/block_size));
    return std::isfinite(*rms_out);
}
}

int main() {
    std::cout << "=======================================================\n  STANDALONE VOCAL HOST/AUTOMATION PROBE\n=======================================================\n" << std::flush;
    char exe[MAX_PATH]{};
    const DWORD path_len=GetModuleFileNameA(nullptr,exe,MAX_PATH);
    CHECK_STAGE(path_len>0 && path_len<MAX_PATH,"resolve probe executable path");
    std::string path(exe); const size_t separator=path.find_last_of("\\/");
    CHECK_STAGE(separator!=std::string::npos,"resolve Vocal VST3 directory");
    path=path.substr(0,separator+1)+"monkeys_ear_vocal.vst3";
    std::cout << "[INFO] Loading " << path << "\n" << std::flush;
    HMODULE module=LoadLibraryA(path.c_str());
    if(!module){std::cerr<<"[FAIL] load monkeys_ear_vocal.vst3 (Win32 error "<<GetLastError()<<")\n";return 1;}
    std::cout << "[PASS] load monkeys_ear_vocal.vst3\n";
    FARPROC proc=GetProcAddress(module,"GetPluginFactory");
    CHECK_STAGE(proc!=nullptr,"find GetPluginFactory export");
    auto factory_proc=reinterpret_cast<GetPluginFactoryProc>(reinterpret_cast<void(*)()>(proc));
    IPluginFactory* factory=factory_proc(); CHECK_STAGE(factory!=nullptr,"create Vocal plugin factory");
    IPluginFactory2* factory2=nullptr; CHECK_STAGE(factory->queryInterface(IPluginFactory2_iid,reinterpret_cast<void**>(&factory2))==kResultOk && factory2!=nullptr,"factory implements IPluginFactory2");
    CHECK_STAGE(factory2->countClasses()==2,"factory exposes component and controller");
    PClassInfo2 component_info{}; CHECK_STAGE(factory2->getClassInfo2(0,&component_info)==kResultOk,"read Vocal component class info");
    CHECK_STAGE(std::string(component_info.name)=="Monkey's Ear Vocal" && std::string(component_info.subCategories)=="Fx|Pitch Shift","Vocal component identity");
    IComponent* component=nullptr; CHECK_STAGE(factory2->createInstance(component_info.cid,IComponent_iid,reinterpret_cast<void**>(&component))==kResultOk && component!=nullptr,"create Vocal component");
    CHECK_STAGE(component->getBusCount(kAudio,kInput)==1 && component->getBusCount(kAudio,kOutput)==1,"Vocal stereo FX bus topology");
    IAudioProcessor* processor=nullptr; CHECK_STAGE(component->queryInterface(IAudioProcessor_iid,reinterpret_cast<void**>(&processor))==kResultOk && processor!=nullptr,"Vocal component exposes audio processor");
    ProcessSetup setup{}; setup.sampleRate=48000;setup.maxSamplesPerBlock=256; CHECK_STAGE(processor->setupProcessing(setup)==kResultOk,"Vocal setupProcessing");
    CHECK_STAGE(processor->getLatencySamples()==0,"Vocal reports zero plugin latency");
    PClassInfo2 controller_info{}; CHECK_STAGE(factory2->getClassInfo2(1,&controller_info)==kResultOk,"read Vocal controller class info");
    IEditController* controller=nullptr; CHECK_STAGE(factory2->createInstance(controller_info.cid,IEditController_iid,reinterpret_cast<void**>(&controller))==kResultOk && controller!=nullptr,"create Vocal edit controller");
    CHECK_STAGE(controller->getParameterCount()==11,"Vocal controller exposes 11 parameters");
    ParameterInfo info{}; CHECK_STAGE(controller->getParameterInfo(1,info)==kResultOk,"read Vocal parameter info");
    CHECK_STAGE(info.title[0]=='V' && info.title[7]=='C',"Vocal parameter schema identity");

    for(int block_size : {32,64,128,256}) {
        ProcessSetup block_setup{};block_setup.sampleRate=48000.0;block_setup.maxSamplesPerBlock=block_size;
        CHECK_STAGE(processor->setupProcessing(block_setup)==kResultOk,"automation block setupProcessing");
        OneParameterChange dry_mix(8,0.0); float dry_delta=0.0f;
        CHECK_STAGE(process_block(processor,block_size,&dry_mix,0.0f,&dry_delta),"process dry automation block");
        if(!(dry_delta < 1.0e-5f)){std::cerr<<"[FAIL] Mix=0 dry contract at block "<<block_size<<" (delta="<<dry_delta<<")\n";return 1;}
        std::cout<<"[PASS] Mix=0 dry contract at block "<<block_size<<" (delta="<<dry_delta<<")\n";
        OneParameterChange wet_mix(8,1.0); float wet_delta=0.0f;
        for(int b=0;b<16;++b) {
            if(!process_block(processor,block_size,b==0?static_cast<IParameterChanges*>(&wet_mix):nullptr,static_cast<float>((b+1)*block_size),&wet_delta)){
                std::cerr<<"[FAIL] process wet automation block at size "<<block_size<<", warmup block "<<b<<"\n";return 1;
            }
        }
        if(!(wet_delta > 1.0e-5f)){std::cerr<<"[FAIL] Mix=1 reaches active Vocal DSP at block "<<block_size<<" (delta="<<wet_delta<<")\n";return 1;}
        std::cout<<"[PASS] Mix=1 reaches active Vocal DSP at block "<<block_size<<" (delta="<<wet_delta<<")\n";
    }

    std::cout<<">>> ALL STANDALONE VOCAL VST3 HOST/AUTOMATION CHECKS PASSED! <<<\n";
    controller->release();processor->release();component->release();factory2->release();factory->release();FreeLibrary(module);
    return 0;
}
