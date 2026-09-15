#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <cmath>
#include <string>
#include "vst3_sdk_minimal.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

typedef IPluginFactory* (SMTG_STDCALL *GetPluginFactoryProc)();

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
    std::cout << "=======================================================\n";
    std::cout << "  HOST VERIFICATION PROBE // VST3 BINARY INSPECTION\n";
    std::cout << "=======================================================\n\n";

    char probe_path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, probe_path, MAX_PATH);
    std::string plugin_path(probe_path);
    const size_t separator = plugin_path.find_last_of("\\/");
    plugin_path = (separator == std::string::npos ? std::string{} : plugin_path.substr(0, separator + 1)) + "monkeys_ear.vst3";
    HMODULE hMod = LoadLibraryA(plugin_path.c_str());
    if (!hMod) { std::cerr << "[FAIL] Could not load VST3 DLL at " << plugin_path << "\n"; return 1; }
    std::cout << "[PASS] Successfully loaded VST3 module: monkeys_ear.vst3\n";

    FARPROC proc = GetProcAddress(hMod, "GetPluginFactory");
    if (!proc) { std::cerr << "[FAIL] GetPluginFactory export not found!\n"; return 1; }
    auto getFactory = reinterpret_cast<GetPluginFactoryProc>(reinterpret_cast<void(*)()>(proc));
    std::cout << "[PASS] Found exported symbol GetPluginFactory()\n";

    IPluginFactory* factory = getFactory(); assert(factory != nullptr);
    IPluginFactory2* factory2 = nullptr;
    tresult res = factory->queryInterface(IPluginFactory2_iid, (void**)&factory2); assert(res == kResultOk && factory2 != nullptr);
    std::cout << "[PASS] Factory implements IPluginFactory2\n";
    int32 numClasses = factory2->countClasses(); assert(numClasses == 3);

    PClassInfo2 info0{}; factory2->getClassInfo2(0, &info0);
    assert(std::string(info0.name) == "Monkey's Ear"); assert(std::string(info0.subCategories) == "Instrument|Synth");
    IComponent* instr_comp = nullptr; res = factory2->createInstance(info0.cid, IComponent_iid, (void**)&instr_comp); assert(res == kResultOk && instr_comp != nullptr);
    assert(instr_comp->getBusCount(kAudio, kInput) == 0); assert(instr_comp->getBusCount(kAudio, kOutput) == 1); assert(instr_comp->getBusCount(kEvent, kInput) == 1);

    PClassInfo2 info1{}; factory2->getClassInfo2(1, &info1);
    assert(std::string(info1.name) == "Monkey's Ear FX"); assert(std::string(info1.category) == "Audio Module Class");
    IComponent* fx_comp = nullptr; res = factory2->createInstance(info1.cid, IComponent_iid, (void**)&fx_comp); assert(res == kResultOk && fx_comp != nullptr);
    assert(fx_comp->getBusCount(kAudio, kInput) == 1); assert(fx_comp->getBusCount(kAudio, kOutput) == 1); assert(fx_comp->getBusCount(kEvent, kInput) == 1);

    PClassInfo2 info2{}; factory2->getClassInfo2(2, &info2);
    IEditController* controller = nullptr; res = factory2->createInstance(info2.cid, IEditController_iid, (void**)&controller); assert(res == kResultOk && controller != nullptr);
    int32 paramCount = controller->getParameterCount(); assert(paramCount == 124);
    for (int32 i = 0; i < paramCount; ++i) { ParameterInfo pInfo{}; controller->getParameterInfo(i, pInfo); }

    MemoryStream state_stream; assert(fx_comp->getState(&state_stream)==kResultOk); assert(state_stream.bytes.size()==12u+124u*sizeof(float));
    state_stream.pos=0; assert(controller->setComponentState(&state_stream)==kResultOk);
    IMidiMapping* midi_map=nullptr; assert(controller->queryInterface(IMidiMapping_iid,(void**)&midi_map)==kResultOk);
    ParamID bend_id=0,pressure_id=0; assert(midi_map->getMidiControllerAssignment(0,0,129,bend_id)==kResultOk&&bend_id==80); assert(midi_map->getMidiControllerAssignment(0,0,130,pressure_id)==kResultOk&&pressure_id==81); midi_map->release();

    IAudioProcessor* instr_proc=nullptr; assert(instr_comp->queryInterface(IAudioProcessor_iid,(void**)&instr_proc)==kResultOk);
    ProcessSetup instr_setup{}; instr_setup.sampleRate=48000; instr_setup.maxSamplesPerBlock=128; instr_proc->setupProcessing(instr_setup); instr_comp->setActive(true);
    std::vector<float> instr_l(128),instr_r(128); float* instr_channels[]={instr_l.data(),instr_r.data()}; AudioBusBuffers instr_out{}; instr_out.numChannels=2; instr_out.channelBuffers32=instr_channels;
    OneEventList notes; notes.event.type=kNoteOnEvent; notes.event.noteOn.pitch=48; notes.event.noteOn.velocity=.9f;
    ProcessData instr_data{}; instr_data.numSamples=128; instr_data.numOutputs=1; instr_data.outputs=&instr_out; instr_data.inputEvents=&notes;
    assert(instr_proc->process(instr_data)==kResultOk); float instr_peak=0; for(float v:instr_l) instr_peak=std::max(instr_peak,std::abs(v)); assert(instr_peak>.001f);
    instr_comp->setActive(false); instr_proc->release();

    IAudioProcessor* fx_proc = nullptr; res = fx_comp->queryInterface(IAudioProcessor_iid, (void**)&fx_proc); assert(res == kResultOk && fx_proc != nullptr);
    ProcessSetup setup{}; setup.sampleRate = 48000.0; setup.maxSamplesPerBlock = 128; setup.processMode = 0; fx_proc->setupProcessing(setup); fx_comp->setActive(true);
    constexpr int32 N = 128; std::vector<float> in_buf_l(N), in_buf_r(N), out_buf_l(N, 0.0f), out_buf_r(N, 0.0f);
    for (int32 i = 0; i < N; ++i) { float s = 0.5f * std::sin(6.2831853f * 440.0f * (static_cast<float>(i) / 48000.0f)); in_buf_l[i] = s; in_buf_r[i] = s; }
    float* in_channels[] = { in_buf_l.data(), in_buf_r.data() }; float* out_channels[] = { out_buf_l.data(), out_buf_r.data() };
    AudioBusBuffers inBuses[1]; inBuses[0].numChannels = 2; inBuses[0].channelBuffers32 = in_channels;
    AudioBusBuffers outBuses[1]; outBuses[0].numChannels = 2; outBuses[0].channelBuffers32 = out_channels;
    ProcessData procData{}; procData.numSamples = N; procData.numInputs = 1; procData.inputs = inBuses; procData.numOutputs = 1; procData.outputs = outBuses;
    assert(fx_proc->process(procData) == kResultOk); float max_val = 0.0f; for (int32 i = 0; i < N; ++i) max_val = std::max(max_val, std::abs(out_buf_l[i])); assert(max_val > 0.01f);

    fx_comp->setActive(false); fx_proc->release(); fx_comp->release(); instr_comp->release(); controller->release(); factory2->release(); factory->release(); FreeLibrary(hMod);
    std::cout << ">>> ALL VST3 HOST BINARY CHECKS PASSED! <<<\n"; return 0;
}
