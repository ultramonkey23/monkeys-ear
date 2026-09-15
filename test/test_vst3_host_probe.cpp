#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <string>
#include "vst3_sdk_minimal.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

typedef IPluginFactory* (SMTG_STDCALL *GetPluginFactoryProc)();

#define CHECK_STAGE(condition, stage) do { if (!(condition)) { std::cerr << "[FAIL] " << stage << "\n"; return 1; } else { std::cout << "[PASS] " << stage << "\n"; } } while (0)

class OneEventList final : public IEventList {
public:
    Event event{}; uint32 refs=1;
    tresult SMTG_STDCALL queryInterface(const TUID,void** obj)override{*obj=nullptr;return kResultFalse;}
    uint32 SMTG_STDCALL addRef()override{return ++refs;} uint32 SMTG_STDCALL release()override{return --refs;}
    int32 SMTG_STDCALL getEventCount()override{return 1;}
    tresult SMTG_STDCALL getEvent(int32 index,Event& e)override{if(index!=0)return kInvalidArgument;e=event;return kResultOk;}
    tresult SMTG_STDCALL addEvent(Event& e)override{event=e;return kResultOk;}
};

class MemoryStream final : public IBStream {
public:
    std::vector<uint8_t> bytes; size_t pos=0; uint32 refs=1;
    tresult SMTG_STDCALL queryInterface(const TUID,void** obj)override{*obj=nullptr;return kResultFalse;}
    uint32 SMTG_STDCALL addRef()override{return ++refs;} uint32 SMTG_STDCALL release()override{return --refs;}
    tresult SMTG_STDCALL read(void* dst,int32 n,int32* done)override{size_t count=std::min<size_t>(n,bytes.size()-std::min(pos,bytes.size()));memcpy(dst,bytes.data()+pos,count);pos+=count;if(done)*done=static_cast<int32>(count);return count==static_cast<size_t>(n)?kResultOk:kResultFalse;}
    tresult SMTG_STDCALL write(void* src,int32 n,int32* done)override{if(pos+n>bytes.size())bytes.resize(pos+n);memcpy(bytes.data()+pos,src,n);pos+=n;if(done)*done=n;return kResultOk;}
    tresult SMTG_STDCALL seek(int64 p,int32 mode,int64* result)override{int64 base=mode==kIBSeekCur?static_cast<int64>(pos):(mode==kIBSeekEnd?static_cast<int64>(bytes.size()):0);pos=static_cast<size_t>(std::max<int64>(0,base+p));if(result)*result=pos;return kResultOk;}
    tresult SMTG_STDCALL tell(int64* p)override{if(p)*p=pos;return kResultOk;}
};

int main() {
    std::cout << "=======================================================\n  HOST VERIFICATION PROBE // VST3 BINARY INSPECTION\n=======================================================\n" << std::flush;

    char probe_path[MAX_PATH]{};
    const DWORD path_len=GetModuleFileNameA(nullptr,probe_path,MAX_PATH);
    CHECK_STAGE(path_len>0 && path_len<MAX_PATH,"resolve probe executable path");
    std::string plugin_path(probe_path);
    const size_t separator=plugin_path.find_last_of("\\/");
    plugin_path=(separator==std::string::npos?std::string{}:plugin_path.substr(0,separator+1))+"monkeys_ear.vst3";
    std::cout << "[INFO] Loading " << plugin_path << "\n" << std::flush;
    HMODULE hMod=LoadLibraryA(plugin_path.c_str());
    if(!hMod){std::cerr<<"[FAIL] load monkeys_ear.vst3 (Win32 error "<<GetLastError()<<")\n";return 1;}
    std::cout<<"[PASS] load monkeys_ear.vst3\n";

    FARPROC proc=GetProcAddress(hMod,"GetPluginFactory");
    CHECK_STAGE(proc!=nullptr,"find GetPluginFactory export");
    auto getFactory=reinterpret_cast<GetPluginFactoryProc>(reinterpret_cast<void(*)()>(proc));
    IPluginFactory* factory=getFactory();
    CHECK_STAGE(factory!=nullptr,"create plugin factory");
    IPluginFactory2* factory2=nullptr;
    tresult res=factory->queryInterface(IPluginFactory2_iid,(void**)&factory2);
    CHECK_STAGE(res==kResultOk && factory2!=nullptr,"factory implements IPluginFactory2");
    CHECK_STAGE(factory2->countClasses()==3,"factory exposes 3 classes");

    PClassInfo2 info0{};
    CHECK_STAGE(factory2->getClassInfo2(0,&info0)==kResultOk,"read instrument class info");
    CHECK_STAGE(std::string(info0.name)=="Monkey's Ear" && std::string(info0.subCategories)=="Instrument|Synth","instrument class identity");
    IComponent* instr_comp=nullptr; res=factory2->createInstance(info0.cid,IComponent_iid,(void**)&instr_comp);
    CHECK_STAGE(res==kResultOk && instr_comp!=nullptr,"create instrument component");
    CHECK_STAGE(instr_comp->getBusCount(kAudio,kInput)==0 && instr_comp->getBusCount(kAudio,kOutput)==1 && instr_comp->getBusCount(kEvent,kInput)==1,"instrument bus topology");

    PClassInfo2 info1{};
    CHECK_STAGE(factory2->getClassInfo2(1,&info1)==kResultOk,"read FX class info");
    CHECK_STAGE(std::string(info1.name)=="Monkey's Ear FX" && std::string(info1.category)=="Audio Module Class","FX class identity");
    IComponent* fx_comp=nullptr; res=factory2->createInstance(info1.cid,IComponent_iid,(void**)&fx_comp);
    CHECK_STAGE(res==kResultOk && fx_comp!=nullptr,"create FX component");
    CHECK_STAGE(fx_comp->getBusCount(kAudio,kInput)==1 && fx_comp->getBusCount(kAudio,kOutput)==1 && fx_comp->getBusCount(kEvent,kInput)==1,"FX bus topology");

    PClassInfo2 info2{};
    CHECK_STAGE(factory2->getClassInfo2(2,&info2)==kResultOk,"read controller class info");
    IEditController* controller=nullptr; res=factory2->createInstance(info2.cid,IEditController_iid,(void**)&controller);
    CHECK_STAGE(res==kResultOk && controller!=nullptr,"create edit controller");
    CHECK_STAGE(controller->getParameterCount()==124,"controller exposes 124 parameters");
    for(int32 i=0;i<124;++i){ParameterInfo pInfo{};CHECK_STAGE(controller->getParameterInfo(i,pInfo)==kResultOk,"read parameter info");}

    MemoryStream state_stream;
    CHECK_STAGE(fx_comp->getState(&state_stream)==kResultOk,"serialize FX state");
    CHECK_STAGE(state_stream.bytes.size()==12u+124u*sizeof(float),"serialized state size");
    state_stream.pos=0; CHECK_STAGE(controller->setComponentState(&state_stream)==kResultOk,"controller accepts component state");
    IMidiMapping* midi_map=nullptr; CHECK_STAGE(controller->queryInterface(IMidiMapping_iid,(void**)&midi_map)==kResultOk && midi_map!=nullptr,"controller exposes MIDI mapping");
    ParamID bend_id=0,pressure_id=0;
    CHECK_STAGE(midi_map->getMidiControllerAssignment(0,0,129,bend_id)==kResultOk && bend_id==80,"pitch-bend mapping");
    CHECK_STAGE(midi_map->getMidiControllerAssignment(0,0,130,pressure_id)==kResultOk && pressure_id==81,"pressure mapping"); midi_map->release();

    IAudioProcessor* instr_proc=nullptr; CHECK_STAGE(instr_comp->queryInterface(IAudioProcessor_iid,(void**)&instr_proc)==kResultOk && instr_proc!=nullptr,"instrument exposes audio processor");
    ProcessSetup instr_setup{}; instr_setup.sampleRate=48000; instr_setup.maxSamplesPerBlock=128;
    CHECK_STAGE(instr_proc->setupProcessing(instr_setup)==kResultOk,"instrument setupProcessing"); CHECK_STAGE(instr_comp->setActive(true)==kResultOk,"activate instrument");
    std::vector<float> instr_l(128),instr_r(128); float* instr_channels[]={instr_l.data(),instr_r.data()}; AudioBusBuffers instr_out{}; instr_out.numChannels=2; instr_out.channelBuffers32=instr_channels;
    OneEventList notes; notes.event.type=kNoteOnEvent; notes.event.noteOn.pitch=48; notes.event.noteOn.velocity=.9f;
    ProcessData instr_data{}; instr_data.numSamples=128; instr_data.numOutputs=1; instr_data.outputs=&instr_out; instr_data.inputEvents=&notes;
    CHECK_STAGE(instr_proc->process(instr_data)==kResultOk,"process instrument note"); float instr_peak=0; for(float v:instr_l)instr_peak=std::max(instr_peak,std::abs(v)); CHECK_STAGE(instr_peak>.001f,"instrument produces audio");
    instr_comp->setActive(false); instr_proc->release();

    IAudioProcessor* fx_proc=nullptr; res=fx_comp->queryInterface(IAudioProcessor_iid,(void**)&fx_proc); CHECK_STAGE(res==kResultOk && fx_proc!=nullptr,"FX exposes audio processor");
    ProcessSetup setup{}; setup.sampleRate=48000.0; setup.maxSamplesPerBlock=128; setup.processMode=0; CHECK_STAGE(fx_proc->setupProcessing(setup)==kResultOk,"FX setupProcessing"); CHECK_STAGE(fx_comp->setActive(true)==kResultOk,"activate FX");
    constexpr int32 N=128; std::vector<float> in_l(N),in_r(N),out_l(N,0.0f),out_r(N,0.0f); for(int32 i=0;i<N;++i){float s=.5f*std::sin(6.2831853f*440.0f*(static_cast<float>(i)/48000.0f));in_l[i]=s;in_r[i]=s;}
    float* in_channels[]={in_l.data(),in_r.data()}; float* out_channels[]={out_l.data(),out_r.data()}; AudioBusBuffers inBuses[1]{}; inBuses[0].numChannels=2; inBuses[0].channelBuffers32=in_channels; AudioBusBuffers outBuses[1]{}; outBuses[0].numChannels=2; outBuses[0].channelBuffers32=out_channels;
    ProcessData data{}; data.numSamples=N; data.numInputs=1; data.inputs=inBuses; data.numOutputs=1; data.outputs=outBuses; CHECK_STAGE(fx_proc->process(data)==kResultOk,"process FX audio"); float max_val=0; for(float v:out_l)max_val=std::max(max_val,std::abs(v)); CHECK_STAGE(max_val>.01f,"FX produces audio");

    fx_comp->setActive(false); fx_proc->release(); fx_comp->release(); instr_comp->release(); controller->release(); factory2->release(); factory->release(); FreeLibrary(hMod);
    std::cout<<">>> ALL VST3 HOST BINARY CHECKS PASSED! <<<\n"; return 0;
}
