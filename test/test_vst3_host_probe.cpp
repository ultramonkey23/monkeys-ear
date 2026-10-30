#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <string>
#include <limits>
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

class TwoPointQueue final : public IParamValueQueue {
public:
    TwoPointQueue(ParamID id,int32 offset,ParamValue first,ParamValue second)
        : id_(id),offset_(offset),first_(first),second_(second){}
    tresult SMTG_STDCALL queryInterface(const TUID,void** obj)override{if(obj)*obj=nullptr;return kResultFalse;}
    uint32 SMTG_STDCALL addRef()override{return 1;}
    uint32 SMTG_STDCALL release()override{return 1;}
    ParamID SMTG_STDCALL getParameterId()override{return id_;}
    int32 SMTG_STDCALL getPointCount()override{return 2;}
    tresult SMTG_STDCALL getPoint(int32 index,int32& offset,ParamValue& value)override{
        if(index<0||index>=2)return kInvalidArgument;
        offset=index==0?0:offset_;
        value=index==0?first_:second_;return kResultOk;
    }
    tresult SMTG_STDCALL addPoint(int32,ParamValue,int32&)override{return kNotImplemented;}
private:
    ParamID id_;int32 offset_;ParamValue first_,second_;
};
class TwoPointChanges final : public IParameterChanges {
public:
    TwoPointChanges(ParamID id,int32 offset,ParamValue a,ParamValue b):queue_(id,offset,a,b){}
    tresult SMTG_STDCALL queryInterface(const TUID,void** obj)override{if(obj)*obj=nullptr;return kResultFalse;}
    uint32 SMTG_STDCALL addRef()override{return 1;}
    uint32 SMTG_STDCALL release()override{return 1;}
    int32 SMTG_STDCALL getParameterCount()override{return 1;}
    IParamValueQueue* SMTG_STDCALL getParameterData(int32 index)override{return index==0?&queue_:nullptr;}
    IParamValueQueue* SMTG_STDCALL addParameterData(const ParamID&,int32&)override{return nullptr;}
private:TwoPointQueue queue_;
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

class RecordingHandler final : public IComponentHandler {
public:
    int begin_count=0,perform_count=0,end_count=0;
    ParamID last_id=static_cast<ParamID>(-1);
    ParamValue last_value=-1.0;
    tresult SMTG_STDCALL queryInterface(const TUID,void** out) override {
        if(out)*out=nullptr;return kResultFalse;
    }
    uint32 SMTG_STDCALL addRef() override { return 1; }
    uint32 SMTG_STDCALL release() override { return 1; }
    tresult SMTG_STDCALL beginEdit(ParamID id) override {
        ++begin_count;last_id=id;return kResultOk;
    }
    tresult SMTG_STDCALL performEdit(ParamID id,ParamValue v) override {
        ++perform_count;last_id=id;last_value=v;return kResultOk;
    }
    tresult SMTG_STDCALL endEdit(ParamID id) override {
        ++end_count;last_id=id;return kResultOk;
    }
    tresult SMTG_STDCALL restartComponent(int32) override { return kResultOk; }
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
    CHECK_STAGE(controller->getParameterCount()==176,"controller exposes 176 stable host parameters");
    for(int32 i=0;i<176;++i){ParameterInfo pInfo{};CHECK_STAGE(controller->getParameterInfo(i,pInfo)==kResultOk,"read parameter info");}

    RecordingHandler edits;
    CHECK_STAGE(controller->setComponentHandler(&edits)==kResultOk,
                "native editor can communicate through host automation handler");
    IPlugView* view=controller->createView("editor");
    CHECK_STAGE(view!=nullptr,"create a real VST3 instrument editor");
    CHECK_STAGE(controller->createView("not-editor")==nullptr,
                "unsupported view identity rejected");
    CHECK_STAGE(view->isPlatformTypeSupported("HWND")==kResultOk &&
                view->isPlatformTypeSupported("X11EmbedWindowID")!=kResultOk,
                "editor correctly advertises Windows HWND embedding");
    ViewRect size{};
    CHECK_STAGE(view->getSize(&size)==kResultOk &&
                size.right>=960&&size.bottom>=620,
                "editor reports usable nonzero dimensions");
    ViewRect too_small{0,0,320,200};
    CHECK_STAGE(view->checkSizeConstraint(&too_small)==kResultOk &&
                too_small.right>=960&&too_small.bottom>=620,
                "host resize cannot destroy basic readability");
    HWND parent=CreateWindowExW(0,L"STATIC",L"Monkey's Ear UI host probe",
                               WS_OVERLAPPEDWINDOW,0,0,1200,850,
                               nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    CHECK_STAGE(parent!=nullptr,"create host-owned parent HWND");
    CHECK_STAGE(view->attached(parent,"HWND")==kResultOk,
                "editor attaches child to host-owned window");
    HWND child=FindWindowExW(parent,nullptr,L"MonkeysEarNativeEditor_01",nullptr);
    CHECK_STAGE(child!=nullptr,"real native child window exists");
    RedrawWindow(child,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW);
    // First WM_PAINT constructs click-hit geometry even with a hidden test parent.
    const int before_edits=edits.perform_count;
    SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(125,365));
    SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(125,365));
    CHECK_STAGE(edits.begin_count>0&&edits.perform_count>before_edits &&
                edits.end_count==edits.begin_count&&edits.last_id==0,
                "PLAY macro drag notifies real host begin/perform/end automation");
    CHECK_STAGE(std::abs(controller->getParamNormalized(0)-edits.last_value)<1e-8,
                "editor controller mirrors the normalized value sent to host");
    // Choose CLOCKWORK SUB, then verify that all displayed controls use the
    // selected patch's real macro, rhythm, and tube state (not stale init UI).
    SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(360,247));
    SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(360,247));
    CHECK_STAGE(edits.last_id==173 &&
                controller->getParamNormalized(124)>.5 &&
                controller->getParamNormalized(169)>.5 &&
                controller->getParamNormalized(175)>.4,
                "PLAY palette selection synchronizes actual preset controls");
    const ParamValue gui_patch_edit=edits.last_value;
    ViewRect resized{0,0,1280,860};
    CHECK_STAGE(view->onSize(&resized)==kResultOk &&
                view->getSize(&size)==kResultOk && size.right==1280,
                "editor resizes with the host");
    CHECK_STAGE(view->removed()==kResultOk,"editor safely detaches from host");
    CHECK_STAGE(view->attached(parent,"HWND")==kResultOk,
                "editor can reopen without duplicated window state");
    CHECK_STAGE(view->removed()==kResultOk,"editor cleans up after reopen");
    CHECK_STAGE(view->release()==0,"editor reference lifetime completes");
    DestroyWindow(parent);
    controller->setComponentHandler(nullptr);

    MemoryStream state_stream;
    CHECK_STAGE(fx_comp->getState(&state_stream)==kResultOk,"serialize FX state");
    CHECK_STAGE(state_stream.bytes.size()==12u+176u*sizeof(float),"serialized state size");
    state_stream.pos=0; CHECK_STAGE(controller->setComponentState(&state_stream)==kResultOk,"controller accepts component state");
    ParameterInfo tube_type{},amp_sustain{},division{};
    CHECK_STAGE(controller->getParameterInfo(154,tube_type)==kResultOk && tube_type.stepCount==13,
                "tube 4 exposes all 14 types with correct enum metadata");
    CHECK_STAGE(controller->getParameterInfo(163,amp_sustain)==kResultOk &&
                controller->getParameterInfo(171,division)==kResultOk && division.stepCount==11,
                "host exposes envelope sustain and 12 rhythmic divisions");
    ParameterInfo clock{},groove{};
    CHECK_STAGE(controller->getParameterInfo(174,clock)==kResultOk &&
                clock.stepCount==1 && clock.unitId==5 &&
                controller->getParameterInfo(175,groove)==kResultOk &&
                groove.unitId==5,
                "rhythm host clock and groove live in one discoverable unit");

    IUnitInfo* units=nullptr;
    CHECK_STAGE(controller->queryInterface(IUnitInfo_iid,(void**)&units)==kResultOk && units!=nullptr,
                "host can request VST3 instrument subsystem tree");
    CHECK_STAGE(units->getUnitCount()==15,"15 meaningful native host units");
    UnitInfo root{},tubeRack{},tube1{},tube4{};
    CHECK_STAGE(units->getUnitInfo(0,root)==kResultOk && root.id==0 && root.parentUnitId==-1 &&
                units->getUnitInfo(9,tubeRack)==kResultOk && tubeRack.parentUnitId==0 &&
                units->getUnitInfo(10,tube1)==kResultOk && tube1.parentUnitId==9 &&
                units->getUnitInfo(13,tube4)==kResultOk && tube4.parentUnitId==9,
                "four tubes nested within rack instead of 32 ungrouped sliders");
    CHECK_STAGE(units->getProgramListCount()==0,"unit tree does not advertise fake program data");
    CHECK_STAGE(units->selectUnit(5)==kResultOk && units->getSelectedUnit()==5,
                "host can navigate to rhythm subgroup");
    units->release();
    ParameterInfo old_patch{},new_patch{},pedal{};
    CHECK_STAGE(controller->getParameterInfo(79,old_patch)==kResultOk && old_patch.stepCount==3 &&
                controller->getParameterInfo(173,new_patch)==kResultOk && new_patch.stepCount==7 &&
                controller->getParameterInfo(172,pedal)==kResultOk && pedal.stepCount==1,
                "old preset selector remains four choices; new palette and CC64 are appended");
    ParamValue typed=0.0;
    CHECK_STAGE(controller->getParamValueByString(170,u"122 BPM",typed)==kResultOk &&
                std::abs(typed-.41)<1e-8,"typed tempo maps to numeric BPM");
    CHECK_STAGE(controller->getParamValueByString(164,u"250 ms",typed)==kResultOk &&
                typed>.20&&typed<.21,"typed release time maps to ADSR seconds");
    CHECK_STAGE(controller->getParamValueByString(154,u"KT88",typed)==kResultOk &&
                std::abs(typed-13.5/14.0)<1e-8,"typed named tube maps to the last flavor");
    CHECK_STAGE(controller->getParamValueByString(173,u"TUBE FOUNDRY",typed)==kResultOk &&
                std::abs(typed-4.5/8.0)<1e-8,"PLAY palette accepts named sounds");
    CHECK_STAGE(controller->getParamValueByString(79,u"MONOLITH",typed)==kResultOk &&
                typed>.25&&typed<.50,"legacy preset editor keeps old name/value");
    CHECK_STAGE(controller->getParamValueByString(173,u"not a preset",typed)!=kResultOk,
                "invalid typed selector does not silently become midpoint");
    CHECK_STAGE(controller->getParamValueByString(174,u"On",typed)==kResultOk && typed==1.0,
                "typed host tempo follows normal switch semantics");
    CHECK_STAGE(controller->getParamValueByString(175,u"72%",typed)==kResultOk &&
                std::abs(typed-.72)<1e-8,
                "groove accepts a readable percent input");
    CHECK_STAGE(controller->setParamNormalized(173,std::numeric_limits<double>::quiet_NaN())==kInvalidArgument,
                "reject invalid controller automation");
    // Controller created after both components must NOT bind implicitly to
    // the last component; otherwise this edit changes FX without host routing.
    CHECK_STAGE(controller->setParamNormalized(124,1.0)==kResultOk,"edit controller-local tube enable");
    MemoryStream a_state,b_state;
    CHECK_STAGE(instr_comp->getState(&a_state)==kResultOk && fx_comp->getState(&b_state)==kResultOk,
                "serialize instrument and FX instances independently");
    float a_enabled=99.f,b_enabled=99.f;
    std::memcpy(&a_enabled,a_state.bytes.data()+12u+124u*sizeof(float),sizeof(float));
    std::memcpy(&b_enabled,b_state.bytes.data()+12u+124u*sizeof(float),sizeof(float));
    CHECK_STAGE(a_enabled==0.f&&b_enabled==0.f,
                "controller edit never leaks to an unrelated component");
    MemoryStream legacy;
    // Restore the INSTRUMENT's legacy snapshot, not the FX snapshot; the
    // latter intentionally routes external audio and would mute this test.
    legacy.bytes=a_state.bytes;legacy.bytes.resize(12u+124u*sizeof(float));
    const uint32 legacy_version=3,legacy_count=124;
    std::memcpy(legacy.bytes.data()+4,&legacy_version,4);
    std::memcpy(legacy.bytes.data()+8,&legacy_count,4);
    CHECK_STAGE(instr_comp->setState(&legacy)==kResultOk,"read legacy v3 host snapshot without new rack fields");
    MemoryStream migrated;CHECK_STAGE(instr_comp->getState(&migrated)==kResultOk &&
        migrated.bytes.size()==12u+176u*sizeof(float),"legacy snapshot migrated with appended controls");
    IMidiMapping* midi_map=nullptr; CHECK_STAGE(controller->queryInterface(IMidiMapping_iid,(void**)&midi_map)==kResultOk && midi_map!=nullptr,"controller exposes MIDI mapping");
    ParamID bend_id=0,pressure_id=0;
    CHECK_STAGE(midi_map->getMidiControllerAssignment(0,0,129,bend_id)==kResultOk && bend_id==80,"pitch-bend mapping");
    CHECK_STAGE(midi_map->getMidiControllerAssignment(0,0,130,pressure_id)==kResultOk && pressure_id==81,"pressure mapping");
    ParamID sustain_id=0;
    CHECK_STAGE(midi_map->getMidiControllerAssignment(0,0,64,sustain_id)==kResultOk && sustain_id==172,
                "hardware CC64 routes to real voice sustain"); midi_map->release();

    IAudioProcessor* instr_proc=nullptr; CHECK_STAGE(instr_comp->queryInterface(IAudioProcessor_iid,(void**)&instr_proc)==kResultOk && instr_proc!=nullptr,"instrument exposes audio processor");
    ProcessSetup instr_setup{}; instr_setup.sampleRate=48000; instr_setup.maxSamplesPerBlock=128;
    CHECK_STAGE(instr_proc->setupProcessing(instr_setup)==kResultOk,"instrument setupProcessing"); CHECK_STAGE(instr_comp->setActive(true)==kResultOk,"activate instrument");
    std::vector<float> instr_l(128),instr_r(128); float* instr_channels[]={instr_l.data(),instr_r.data()}; AudioBusBuffers instr_out{}; instr_out.numChannels=2; instr_out.channelBuffers32=instr_channels;
    OneEventList notes; notes.event.type=kNoteOnEvent; notes.event.noteOn.pitch=48; notes.event.noteOn.velocity=.9f;
    ProcessContext context{};
    context.state=ProcessContext::kTempoValid | ProcessContext::kProjectTimeMusicValid | ProcessContext::kPlaying;
    context.tempo=97.0;
    context.projectTimeMusic=5.25;
    TwoPointChanges transferred_palette(173,0,gui_patch_edit,gui_patch_edit);
    ProcessData instr_data{}; instr_data.numSamples=128; instr_data.numOutputs=1;
    instr_data.outputs=&instr_out; instr_data.inputEvents=&notes;
    instr_data.processContext=&context;
    instr_data.inputParameterChanges=&transferred_palette;
    CHECK_STAGE(instr_proc->process(instr_data)==kResultOk,"process instrument note");
    MemoryStream selected_patch_state;
    CHECK_STAGE(instr_comp->getState(&selected_patch_state)==kResultOk &&
                selected_patch_state.bytes.size()==12u+176u*sizeof(float),
                "host can serialize live selected factory patch");
    float selected_tube=0,selected_groove=0;
    std::memcpy(&selected_tube,selected_patch_state.bytes.data()+12u+124u*sizeof(float),4);
    std::memcpy(&selected_groove,selected_patch_state.bytes.data()+12u+175u*sizeof(float),4);
    CHECK_STAGE(selected_tube>.5f&&selected_groove>.4f,
                "GUI host edit reached DSP factory and synchronized component state");
    instr_data.inputParameterChanges=nullptr;
    float instr_peak=0; for(float v:instr_l)instr_peak=std::max(instr_peak,std::abs(v)); CHECK_STAGE(instr_peak>.001f,"instrument produces audio");
    instr_comp->setActive(false); // clears held voices and effect tails
    CHECK_STAGE(instr_comp->setActive(true)==kResultOk,"reactivate reset instrument");
    notes.event.sampleOffset=64;
    notes.event.noteOn.pitch=55;
    CHECK_STAGE(instr_proc->process(instr_data)==kResultOk,"schedule note at sample 64");
    float before_note=0.f,after_note=0.f;
    for(int i=0;i<64;++i)before_note=std::max(before_note,std::abs(instr_l[i]));
    for(int i=64;i<128;++i)after_note=std::max(after_note,std::abs(instr_l[i]));
    CHECK_STAGE(before_note<1.e-7f && after_note>1.e-7f,
                "VST3 MIDI note does not sound before its sampleOffset");
    instr_data.inputEvents=nullptr;
    TwoPointChanges gain_ramp(8,64,.85,0.0);
    instr_data.inputParameterChanges=&gain_ramp;
    CHECK_STAGE(instr_proc->process(instr_data)==kResultOk,"render two automation values in one audio block");
    float gain_first=0.f,gain_second=0.f;
    for(int i=0;i<64;++i)gain_first=std::max(gain_first,std::abs(instr_l[i]));
    for(int i=64;i<128;++i)gain_second=std::max(gain_second,std::abs(instr_l[i]));
    CHECK_STAGE(gain_first>1.e-7f && gain_second<gain_first*.10f,
                "master gain automation takes effect at offset 64, not block start");
    ProcessData flush{};flush.numSamples=0;flush.inputParameterChanges=&gain_ramp;
    CHECK_STAGE(instr_proc->process(flush)==kResultOk,
                "accept host zero-sample parameter flush");
    MemoryStream flush_state;
    CHECK_STAGE(instr_comp->getState(&flush_state)==kResultOk,
                "serialize after no-buffer automation flush");
    float flushed=1.f;
    memcpy(&flushed,flush_state.bytes.data()+12u+8u*sizeof(float),4);
    CHECK_STAGE(flushed==0.f,"zero-sample flush applied last master gain value");
    instr_comp->setActive(false); instr_proc->release();

    IAudioProcessor* fx_proc=nullptr; res=fx_comp->queryInterface(IAudioProcessor_iid,(void**)&fx_proc); CHECK_STAGE(res==kResultOk && fx_proc!=nullptr,"FX exposes audio processor");
    ProcessSetup setup{}; setup.sampleRate=48000.0; setup.maxSamplesPerBlock=128; setup.processMode=0; CHECK_STAGE(fx_proc->setupProcessing(setup)==kResultOk,"FX setupProcessing"); CHECK_STAGE(fx_comp->setActive(true)==kResultOk,"activate FX");
    constexpr int32 N=128; std::vector<float> in_l(N),in_r(N),out_l(N,0.0f),out_r(N,0.0f); for(int32 i=0;i<N;++i){float s=.5f*std::sin(6.2831853f*440.0f*(static_cast<float>(i)/48000.0f));in_l[i]=s;in_r[i]=s;}
    float* in_channels[]={in_l.data(),in_r.data()}; float* out_channels[]={out_l.data(),out_r.data()}; AudioBusBuffers inBuses[1]{}; inBuses[0].numChannels=2; inBuses[0].channelBuffers32=in_channels; AudioBusBuffers outBuses[1]{}; outBuses[0].numChannels=2; outBuses[0].channelBuffers32=out_channels;
    ProcessData data{}; data.numSamples=N; data.numInputs=1; data.inputs=inBuses; data.numOutputs=1; data.outputs=outBuses; CHECK_STAGE(fx_proc->process(data)==kResultOk,"process FX audio"); float max_val=0; for(float v:out_l)max_val=std::max(max_val,std::abs(v)); CHECK_STAGE(max_val>.01f,"FX produces audio");

    fx_comp->setActive(false); fx_proc->release(); fx_comp->release(); instr_comp->release(); controller->release(); factory2->release(); factory->release();
    IPluginFactory* fresh=getFactory();
    CHECK_STAGE(fresh!=nullptr && fresh->countClasses()==3,"factory export survives full reference release");
    fresh->release();
    FreeLibrary(hMod);
    std::cout<<">>> ALL VST3 HOST BINARY CHECKS PASSED! <<<\n"; return 0;
}
