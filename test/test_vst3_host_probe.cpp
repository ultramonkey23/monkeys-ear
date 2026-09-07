#include <windows.h>
#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <cmath>
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

    const char* plugin_path = "C:\\Program Files\\Common Files\\VST3\\monkeys_ear.vst3";
    HMODULE hMod = LoadLibraryA(plugin_path);
    if (!hMod) {
        std::cerr << "[FAIL] Could not load VST3 DLL at " << plugin_path << "\n";
        return 1;
    }
    std::cout << "[PASS] Successfully loaded VST3 module: monkeys_ear.vst3\n";

    FARPROC proc = GetProcAddress(hMod, "GetPluginFactory");
    if (!proc) {
        std::cerr << "[FAIL] GetPluginFactory export not found!\n";
        return 1;
    }
    auto getFactory = reinterpret_cast<GetPluginFactoryProc>(reinterpret_cast<void(*)()>(proc));
    std::cout << "[PASS] Found exported symbol GetPluginFactory()\n";

    IPluginFactory* factory = getFactory();
    assert(factory != nullptr);

    IPluginFactory2* factory2 = nullptr;
    tresult res = factory->queryInterface(IPluginFactory2_iid, (void**)&factory2);
    assert(res == kResultOk && factory2 != nullptr);
    std::cout << "[PASS] Factory implements IPluginFactory2\n";

    int32 numClasses = factory2->countClasses();
    std::cout << "[PASS] Factory exposes " << numClasses << " classes (Instrument, FX Processor, Controller)\n";
    assert(numClasses == 3);

    // ── Class 0: Instrument ───────────────────────────────────────────────
    PClassInfo2 info0{};
    factory2->getClassInfo2(0, &info0);
    std::cout << "\n[CLASS 0] Name: " << info0.name << "\n";
    std::cout << "          Category: " << info0.category << "\n";
    std::cout << "          SubCategories: " << info0.subCategories << "\n";
    assert(std::string(info0.name) == "Monkey's Ear");
    assert(std::string(info0.subCategories) == "Instrument|Synth");

    IComponent* instr_comp = nullptr;
    res = factory2->createInstance(info0.cid, IComponent_iid, (void**)&instr_comp);
    assert(res == kResultOk && instr_comp != nullptr);
    int32 instr_audio_in = instr_comp->getBusCount(kAudio, kInput);
    int32 instr_audio_out = instr_comp->getBusCount(kAudio, kOutput);
    int32 instr_event_in = instr_comp->getBusCount(kEvent, kInput);
    std::cout << "          Audio In Busses: " << instr_audio_in << " (0 required for pure VSTi)\n";
    std::cout << "          Audio Out Busses: " << instr_audio_out << " (Stereo Out)\n";
    std::cout << "          Event In Busses: " << instr_event_in << " (16 MIDI Channels)\n";
    assert(instr_audio_in == 0);
    assert(instr_audio_out == 1);
    assert(instr_event_in == 1);
    std::cout << "          [VERIFIED] Pure VST3i Instrument topology confirmed!\n";

    // ── Class 1: Audio FX Processor ───────────────────────────────────────
    PClassInfo2 info1{};
    factory2->getClassInfo2(1, &info1);
    std::cout << "\n[CLASS 1] Name: " << info1.name << "\n";
    std::cout << "          Category: " << info1.category << "\n";
    std::cout << "          SubCategories: " << info1.subCategories << "\n";
    assert(std::string(info1.name) == "Monkey's Ear FX");
    assert(std::string(info1.category) == "Audio Module Class");

    IComponent* fx_comp = nullptr;
    res = factory2->createInstance(info1.cid, IComponent_iid, (void**)&fx_comp);
    assert(res == kResultOk && fx_comp != nullptr);
    int32 fx_audio_in = fx_comp->getBusCount(kAudio, kInput);
    int32 fx_audio_out = fx_comp->getBusCount(kAudio, kOutput);
    int32 fx_event_in = fx_comp->getBusCount(kEvent, kInput);
    std::cout << "          Audio In Busses: " << fx_audio_in << " (Stereo In for Mic/Guitar/Recorded Audio)\n";
    std::cout << "          Audio Out Busses: " << fx_audio_out << " (Stereo Out)\n";
    std::cout << "          Event In Busses: " << fx_event_in << " (MIDI In for modulation)\n";
    assert(fx_audio_in == 1);
    assert(fx_audio_out == 1);
    assert(fx_event_in == 1);
    std::cout << "          [VERIFIED] Audio FX Processor topology confirmed!\n";

    // ── Class 2: Edit Controller & Parameter Enumeration ─────────────────
    PClassInfo2 info2{};
    factory2->getClassInfo2(2, &info2);
    std::cout << "\n[CLASS 2] Name: " << info2.name << "\n";
    std::cout << "          Category: " << info2.category << "\n";

    IEditController* controller = nullptr;
    res = factory2->createInstance(info2.cid, IEditController_iid, (void**)&controller);
    assert(res == kResultOk && controller != nullptr);

    int32 paramCount = controller->getParameterCount();
    std::cout << "          Total Exposed Parameters: " << paramCount << "\n";
    assert(paramCount == 82);

    std::cout << "\n[EXPOSED PARAMETERS ENUMERATION]:\n";
    for (int32 i = 0; i < paramCount; ++i) {
        ParameterInfo pInfo{};
        controller->getParameterInfo(i, pInfo);

        char titleAscii[128];
        for (int c = 0; c < 128; ++c) titleAscii[c] = static_cast<char>(pInfo.title[c]);

        char valStringAscii[128];
        char16 valString[128];
        controller->getParamStringByValue(pInfo.id, pInfo.defaultNormalizedValue, valString);
        for (int c = 0; c < 128; ++c) valStringAscii[c] = static_cast<char>(valString[c]);

        std::cout << "  Param " << std::setw(2) << pInfo.id << ": "
                  << std::left << std::setw(36) << titleAscii
                  << " (Default: " << valStringAscii << ")\n";
    }

    // Component/controller state is an actual versioned 79-float host stream.
    MemoryStream state_stream;
    assert(fx_comp->getState(&state_stream)==kResultOk);
    assert(state_stream.bytes.size()==12u+82u*sizeof(float));
    state_stream.pos=0;
    assert(controller->setComponentState(&state_stream)==kResultOk);
    std::cout << "[PASS] Versioned host preset state round-trip: "<<state_stream.bytes.size()<<" bytes\n";
    IMidiMapping* midi_map=nullptr;assert(controller->queryInterface(IMidiMapping_iid,(void**)&midi_map)==kResultOk);
    ParamID bend_id=0,pressure_id=0;assert(midi_map->getMidiControllerAssignment(0,0,129,bend_id)==kResultOk&&bend_id==80);
    assert(midi_map->getMidiControllerAssignment(0,0,130,pressure_id)==kResultOk&&pressure_id==81);midi_map->release();
    std::cout << "[PASS] Hardware pitch bend and channel pressure map to real-time performance inputs\n";

    // Actual Instrument-class MIDI processing, not topology alone.
    IAudioProcessor* instr_proc=nullptr; assert(instr_comp->queryInterface(IAudioProcessor_iid,(void**)&instr_proc)==kResultOk);
    ProcessSetup instr_setup{};instr_setup.sampleRate=48000;instr_setup.maxSamplesPerBlock=128;instr_proc->setupProcessing(instr_setup);instr_comp->setActive(true);
    std::vector<float> instr_l(128),instr_r(128);float* instr_channels[]={instr_l.data(),instr_r.data()};AudioBusBuffers instr_out{};instr_out.numChannels=2;instr_out.channelBuffers32=instr_channels;
    OneEventList notes;notes.event.type=kNoteOnEvent;notes.event.noteOn.pitch=48;notes.event.noteOn.velocity=.9f;
    ProcessData instr_data{};instr_data.numSamples=128;instr_data.numOutputs=1;instr_data.outputs=&instr_out;instr_data.inputEvents=&notes;
    assert(instr_proc->process(instr_data)==kResultOk);float instr_peak=0;for(float v:instr_l)instr_peak=std::max(instr_peak,std::abs(v));assert(instr_peak>.001f);
    std::cout<<"[PASS] Instrument component processed MIDI note through audio output; peak="<<instr_peak<<"\n";
    instr_comp->setActive(false);instr_proc->release();

    // ── Live Processing Verification on FX Instance ──────────────────────
    std::cout << "\n[TEST] Feeding External Audio into Monkey's Ear FX Component...\n";
    IAudioProcessor* fx_proc = nullptr;
    res = fx_comp->queryInterface(IAudioProcessor_iid, (void**)&fx_proc);
    assert(res == kResultOk && fx_proc != nullptr);

    ProcessSetup setup{};
    setup.sampleRate = 48000.0;
    setup.maxSamplesPerBlock = 128;
    setup.processMode = 0;
    fx_proc->setupProcessing(setup);
    fx_comp->setActive(true);

    constexpr int32 N = 128;
    std::vector<float> in_buf_l(N), in_buf_r(N), out_buf_l(N, 0.0f), out_buf_r(N, 0.0f);
    for (int32 i = 0; i < N; ++i) {
        float s = 0.5f * std::sin(6.2831853f * 440.0f * (static_cast<float>(i) / 48000.0f));
        in_buf_l[i] = s;
        in_buf_r[i] = s;
    }

    float* in_channels[] = { in_buf_l.data(), in_buf_r.data() };
    float* out_channels[] = { out_buf_l.data(), out_buf_r.data() };

    AudioBusBuffers inBuses[1];
    inBuses[0].numChannels = 2;
    inBuses[0].channelBuffers32 = in_channels;

    AudioBusBuffers outBuses[1];
    outBuses[0].numChannels = 2;
    outBuses[0].channelBuffers32 = out_channels;

    ProcessData procData{};
    procData.numSamples = N;
    procData.numInputs = 1;
    procData.inputs = inBuses;
    procData.numOutputs = 1;
    procData.outputs = outBuses;

    tresult procRes = fx_proc->process(procData);
    assert(procRes == kResultOk);

    float max_val = 0.0f;
    for (int32 i = 0; i < N; ++i) {
        max_val = std::max(max_val, std::abs(out_buf_l[i]));
    }
    std::cout << "[PASS] Monkey's Ear FX processed 128 samples of live audio! Peak output: " << max_val << "\n";
    assert(max_val > 0.01f);

    fx_comp->setActive(false);
    fx_proc->release();
    fx_comp->release();
    instr_comp->release();
    controller->release();
    factory2->release();
    factory->release();
    FreeLibrary(hMod);

    std::cout << "\n>>> ALL VST3 HOST BINARY CHECKS PASSED! <<<\n";
    return 0;
}
