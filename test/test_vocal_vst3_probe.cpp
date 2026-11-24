#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include "vst3_sdk_minimal.h"
#include "monkeys_ear/ecosystem_bus.h"
#include "monkeys_ear/vocal_product_version.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
using GetPluginFactoryProc = IPluginFactory* (SMTG_STDCALL*)();

#define CHECK_STAGE(condition, stage) do { if (!(condition)) { std::cerr << "[FAIL] " << stage << "\n"; return 1; } else { std::cout << "[PASS] " << stage << "\n"; } } while (0)

namespace {
class MemoryStream final : public IBStream {
public:
    MemoryStream() = default;
    explicit MemoryStream(std::vector<std::uint8_t> bytes):bytes_(std::move(bytes)){}
    tresult SMTG_STDCALL queryInterface(const TUID iid,void** obj) override {
        if(!obj)return kInvalidArgument;
        if(!std::memcmp(iid,FUnknown_iid,sizeof(TUID))||!std::memcmp(iid,IBStream_iid,sizeof(TUID))){*obj=static_cast<IBStream*>(this);return kResultOk;}
        *obj=nullptr;return kResultFalse;
    }
    uint32 SMTG_STDCALL addRef() override{return 1;}
    uint32 SMTG_STDCALL release() override{return 1;}
    tresult SMTG_STDCALL read(void* buffer,int32 numBytes,int32* numBytesRead) override {
        if(numBytes<0||(!buffer&&numBytes>0))return kInvalidArgument;
        const size_t requested=static_cast<size_t>(numBytes);
        const size_t available=position_<=bytes_.size()?bytes_.size()-position_:0;
        const size_t count=std::min(requested,available);
        if(count)std::memcpy(buffer,bytes_.data()+position_,count);
        position_+=count;
        if(numBytesRead)*numBytesRead=static_cast<int32>(count);
        return count==requested?kResultOk:kResultFalse;
    }
    tresult SMTG_STDCALL write(void* buffer,int32 numBytes,int32* numBytesWritten) override {
        if(numBytes<0||(!buffer&&numBytes>0))return kInvalidArgument;
        const size_t count=static_cast<size_t>(numBytes);
        if(position_+count>bytes_.size())bytes_.resize(position_+count);
        if(count)std::memcpy(bytes_.data()+position_,buffer,count);
        position_+=count;
        if(numBytesWritten)*numBytesWritten=numBytes;
        return kResultOk;
    }
    tresult SMTG_STDCALL seek(int64 pos,int32 mode,int64* result) override {
        int64 base=0;
        if(mode==kIBSeekCur)base=static_cast<int64>(position_);
        else if(mode==kIBSeekEnd)base=static_cast<int64>(bytes_.size());
        else if(mode!=kIBSeekSet)return kInvalidArgument;
        const int64 next=base+pos;
        if(next<0||static_cast<uint64>(next)>bytes_.size())return kInvalidArgument;
        position_=static_cast<size_t>(next);
        if(result)*result=next;
        return kResultOk;
    }
    tresult SMTG_STDCALL tell(int64* pos) override {
        if(!pos)return kInvalidArgument;
        *pos=static_cast<int64>(position_);
        return kResultOk;
    }
    const std::vector<std::uint8_t>& bytes() const{return bytes_;}
private:
    std::vector<std::uint8_t> bytes_{};
    size_t position_{0};
};

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

// Two events in one parameter queue exercise sample-offset rendering, rather
// than a host that accidentally sends only a last-point/block-level change.
class TwoPointQueue final : public IParamValueQueue {
public:
    TwoPointQueue(ParamID id,int32 second_offset,ParamValue first,ParamValue second)
        : id_(id),second_offset_(second_offset),first_(first),second_(second){}
    tresult SMTG_STDCALL queryInterface(const TUID,void** obj) override {if(obj)*obj=nullptr;return kResultFalse;}
    uint32 SMTG_STDCALL addRef() override{return 1;}
    uint32 SMTG_STDCALL release() override{return 1;}
    ParamID SMTG_STDCALL getParameterId() override{return id_;}
    int32 SMTG_STDCALL getPointCount() override{return 2;}
    tresult SMTG_STDCALL getPoint(int32 index,int32& offset,ParamValue& value) override {
        if(index<0||index>=2)return kInvalidArgument;
        offset=index==0?0:second_offset_;
        value=index==0?first_:second_;
        return kResultOk;
    }
    tresult SMTG_STDCALL addPoint(int32,ParamValue,int32&) override{return kNotImplemented;}
private:
    ParamID id_;
    int32 second_offset_;
    ParamValue first_,second_;
};
class TwoParameterPoints final : public IParameterChanges {
public:
    TwoParameterPoints(ParamID id,int32 second_offset,ParamValue first,ParamValue second)
        : queue_(id,second_offset,first,second){}
    tresult SMTG_STDCALL queryInterface(const TUID,void** obj) override{if(obj)*obj=nullptr;return kResultFalse;}
    uint32 SMTG_STDCALL addRef() override{return 1;}
    uint32 SMTG_STDCALL release() override{return 1;}
    int32 SMTG_STDCALL getParameterCount() override{return 1;}
    IParamValueQueue* SMTG_STDCALL getParameterData(int32 index) override{return index==0?&queue_:nullptr;}
    IParamValueQueue* SMTG_STDCALL addParameterData(const ParamID&,int32&) override{return nullptr;}
private: TwoPointQueue queue_;
};

bool process_block(IAudioProcessor* processor, int block_size, IParameterChanges* changes, float phase, float* rms_out,
                   float* first_half_delta=nullptr,float* second_half_delta=nullptr) {
    if (!processor || !rms_out || block_size<=0 || block_size>256) return false;
    float in_l[256]{},in_r[256]{},out_l[256]{},out_r[256]{};
    for(int i=0;i<block_size;++i){const float t=phase+static_cast<float>(i);in_l[i]=in_r[i]=0.35f*std::sin(2.0f*3.14159265358979323846f*220.0f*t/48000.0f);}
    float* in_ch[2]{in_l,in_r};float* out_ch[2]{out_l,out_r};
    AudioBusBuffers inputs{};inputs.numChannels=2;inputs.channelBuffers32=in_ch;
    AudioBusBuffers outputs{};outputs.numChannels=2;outputs.channelBuffers32=out_ch;
    ProcessData data{};data.numSamples=block_size;data.numInputs=1;data.numOutputs=1;data.inputs=&inputs;data.outputs=&outputs;data.inputParameterChanges=changes;
    if (processor->process(data)!=kResultOk) return false;
    double sum=0.0,first=0.0,second=0.0;
    for(int i=0;i<block_size;++i){
        const double d=out_l[i]-in_l[i];
        sum+=d*d;
        if(i<block_size/2)first+=d*d;else second+=d*d;
    }
    *rms_out=static_cast<float>(std::sqrt(sum/block_size));
    if(first_half_delta)*first_half_delta=static_cast<float>(std::sqrt(first/(block_size/2)));
    if(second_half_delta)*second_half_delta=static_cast<float>(std::sqrt(second/(block_size-block_size/2)));
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
    CHECK_STAGE(std::string(component_info.version)==monkeys_ear::kVocalProductVersion,"Vocal product version identity");
    IComponent* component=nullptr; CHECK_STAGE(factory2->createInstance(component_info.cid,IComponent_iid,reinterpret_cast<void**>(&component))==kResultOk && component!=nullptr,"create Vocal component");
    CHECK_STAGE(component->getBusCount(kAudio,kInput)==1 && component->getBusCount(kAudio,kOutput)==1,"Vocal stereo FX bus topology");
    IAudioProcessor* processor=nullptr; CHECK_STAGE(component->queryInterface(IAudioProcessor_iid,reinterpret_cast<void**>(&processor))==kResultOk && processor!=nullptr,"Vocal component exposes audio processor");
    ProcessSetup setup{}; setup.sampleRate=48000;setup.maxSamplesPerBlock=256; CHECK_STAGE(processor->setupProcessing(setup)==kResultOk,"Vocal setupProcessing");
    CHECK_STAGE(processor->getLatencySamples()==0,"Vocal reports zero plugin latency");
    OneParameterChange zero_sample_change(8,0.625);
    ProcessData parameter_flush{};
    parameter_flush.numSamples=0;
    parameter_flush.inputParameterChanges=&zero_sample_change;
    CHECK_STAGE(processor->process(parameter_flush)==kResultOk,
                "accept host parameter-flush call with no audio buffers");
    MemoryStream flushed_state;
    CHECK_STAGE(component->getState(&flushed_state)==kResultOk,
                "serialize component after zero-sample flush");
    float flushed_mix=0.0f;
    std::memcpy(&flushed_mix,flushed_state.bytes().data()+12u+8u*sizeof(float),sizeof(flushed_mix));
    CHECK_STAGE(std::abs(flushed_mix-0.625f)<1.0e-7f,
                "zero-sample host parameter queue updates component state");

    PClassInfo2 controller_info{}; CHECK_STAGE(factory2->getClassInfo2(1,&controller_info)==kResultOk,"read Vocal controller class info");
    IEditController* controller=nullptr; CHECK_STAGE(factory2->createInstance(controller_info.cid,IEditController_iid,reinterpret_cast<void**>(&controller))==kResultOk && controller!=nullptr,"create Vocal edit controller");
    CHECK_STAGE(controller->getParameterCount()==19,"Vocal controller exposes 19 parameters");
    ParameterInfo info{}; CHECK_STAGE(controller->getParameterInfo(1,info)==kResultOk,"read Vocal parameter info");
    CHECK_STAGE(info.title[0]=='V' && info.title[7]=='C',"Vocal parameter schema identity");
    CHECK_STAGE(std::abs(info.defaultNormalizedValue-0.45)<1.0e-7,"stable factory default is not current parameter value");
    {
        for(int32 pid=11;pid<19;++pid){
            ParameterInfo pinfo{};
            CHECK_STAGE(controller->getParameterInfo(pid,pinfo)==kResultOk && pinfo.title[0]=='V',
                        "read Vocal parameter info 11..18");
            char16 disp[128]{};
            CHECK_STAGE(controller->getParamStringByValue(static_cast<ParamID>(pid),0.5,disp)==kResultOk && disp[0]!=0,
                        "format Vocal display string 11..18");
        }
        ParamValue tv=0.0;
        CHECK_STAGE(controller->getParamValueByString(11,u"D",tv)==kResultOk && std::abs(tv-2.0/11.0)<1e-6,
                    "Vocal Root Key parses D");
        CHECK_STAGE(controller->getParamValueByString(12,u"Major / Ionian",tv)==kResultOk && std::abs(tv-1.0/29.0)<1e-6,
                    "Vocal Scale / Tuning parses Major / Ionian");
        char16 ch_str[128]{};
        CHECK_STAGE(controller->getParamStringByValue(18,2.0/7.0,ch_str)==kResultOk &&
                    ch_str[0]==u'C' && ch_str[1]==u'h' && ch_str[3]==u'3',
                    "Vocal Eco Channel formats Ch 3");
    }
    CHECK_STAGE(controller->setParamNormalized(1,std::numeric_limits<double>::quiet_NaN())==kInvalidArgument,
                "reject NaN host normalized parameter");
    CHECK_STAGE(controller->setParamNormalized(1,std::numeric_limits<double>::infinity())==kInvalidArgument,
                "reject infinite host normalized parameter");

    const std::array<ParamValue,19> recalled_values{
        1.0,0.11,0.22,0.33,0.44,0.55,0.66,0.77,0.0,0.88,0.99,
        2.0/11.0,1.0/29.0,0.65,0.25,0.50,0.75,0.0,3.0/7.0
    };
    for(ParamID id=0;id<recalled_values.size();++id) {
        CHECK_STAGE(controller->setParamNormalized(id,recalled_values[id])==kResultOk,"set non-default Vocal state");
    }
    // A real VST3 host delivers controller edits to the audio component as
    // parameter queues. An independent controller must never mutate a random
    // component merely because it was the most recently constructed instance.
    float relay_rms=0.0f;
    for(ParamID id=0;id<recalled_values.size();++id) {
        OneParameterChange host_edit(id,recalled_values[id]);
        CHECK_STAGE(process_block(processor,32,&host_edit,static_cast<float>(id*32),&relay_rms),
                    "deliver host-edited control to component");
    }
    CHECK_STAGE(controller->getParameterInfo(1,info)==kResultOk &&
                std::abs(info.defaultNormalizedValue-0.45)<1.0e-7,
                "parameter schema default remains immutable after editing");

    MemoryStream saved_state;
    CHECK_STAGE(component->getState(&saved_state)==kResultOk,"serialize complete Vocal component state");
    CHECK_STAGE(saved_state.bytes().size()==12u+recalled_values.size()*sizeof(float),"Vocal v2 state envelope has exact versioned size");
    uint32 state_magic=0,state_version=0,state_count=0;
    std::memcpy(&state_magic,saved_state.bytes().data(),sizeof(state_magic));
    std::memcpy(&state_version,saved_state.bytes().data()+4,sizeof(state_version));
    std::memcpy(&state_count,saved_state.bytes().data()+8,sizeof(state_count));
    CHECK_STAGE(state_magic==0x4D455643u && state_version==2u && state_count==recalled_values.size(),"Vocal v2 state envelope identity");

    // MEVC v1 (11 params) -> v2 (19 params) backward-compatible stream loading
    {
        MemoryStream v1_stream;
        uint32 v1_magic=0x4D455643u, v1_ver=1u, v1_cnt=11u;
        v1_stream.write(&v1_magic,4,nullptr);
        v1_stream.write(&v1_ver,4,nullptr);
        v1_stream.write(&v1_cnt,4,nullptr);
        for(uint32 i=0;i<11u;++i){
            float f=static_cast<float>(recalled_values[i]);
            v1_stream.write(&f,4,nullptr);
        }
        v1_stream.seek(0,kIBSeekSet,nullptr);
        CHECK_STAGE(component->setState(&v1_stream)==kResultOk,"load MEVC v1 (11-param) stream into v2 component");
        MemoryStream v1_migrated;
        CHECK_STAGE(component->getState(&v1_migrated)==kResultOk &&
                    v1_migrated.bytes().size()==12u+19u*sizeof(float),
                    "MEVC v1 stream migrates to v2 19-param envelope");
        uint32 m_ver=0,m_cnt=0;
        std::memcpy(&m_ver,v1_migrated.bytes().data()+4,4);
        std::memcpy(&m_cnt,v1_migrated.bytes().data()+8,4);
        float def13=0.f,def17=0.f;
        std::memcpy(&def13,v1_migrated.bytes().data()+12u+13u*4u,4);
        std::memcpy(&def17,v1_migrated.bytes().data()+12u+17u*4u,4);
        CHECK_STAGE(m_ver==2u && m_cnt==19u && std::abs(def13-0.50f)<1e-6f && std::abs(def17-1.0f)<1e-6f,
                    "MEVC v1->v2 migration populates default tuning/eco controls");
    }

    const std::array<ParamValue,19> guard_values{
        0.51,0.52,0.53,0.54,0.55,0.56,0.57,0.58,0.59,0.60,0.61,
        0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8
    };
    for(ParamID id=0;id<guard_values.size();++id)controller->setParamNormalized(id,guard_values[id]);
    auto truncated_bytes=saved_state.bytes();
    truncated_bytes.resize(12u+5u*sizeof(float)+2u);
    MemoryStream truncated_state(std::move(truncated_bytes));
    CHECK_STAGE(controller->setState(&truncated_state)==kInternalError,"reject truncated Vocal state");
    bool truncated_unchanged=true;
    for(ParamID id=0;id<guard_values.size();++id)truncated_unchanged=truncated_unchanged&&std::abs(controller->getParamNormalized(id)-guard_values[id])<1.0e-7;
    CHECK_STAGE(truncated_unchanged,"truncated Vocal state is transactional");

    auto nonfinite_bytes=saved_state.bytes();
    const float quiet_nan=std::numeric_limits<float>::quiet_NaN();
    std::memcpy(nonfinite_bytes.data()+12u+3u*sizeof(float),&quiet_nan,sizeof(quiet_nan));
    MemoryStream nonfinite_state(std::move(nonfinite_bytes));
    CHECK_STAGE(controller->setState(&nonfinite_state)==kResultFalse,"reject non-finite Vocal state");
    bool nonfinite_unchanged=true;
    for(ParamID id=0;id<guard_values.size();++id)nonfinite_unchanged=nonfinite_unchanged&&std::abs(controller->getParamNormalized(id)-guard_values[id])<1.0e-7;
    CHECK_STAGE(nonfinite_unchanged,"non-finite Vocal state is transactional");

    controller->release(); controller=nullptr;
    processor->release(); processor=nullptr;
    component->release(); component=nullptr;

    CHECK_STAGE(factory2->createInstance(controller_info.cid,IEditController_iid,reinterpret_cast<void**>(&controller))==kResultOk && controller!=nullptr,"create fresh Vocal controller before component");
    CHECK_STAGE(factory2->createInstance(component_info.cid,IComponent_iid,reinterpret_cast<void**>(&component))==kResultOk && component!=nullptr,"create fresh Vocal component after controller");
    CHECK_STAGE(component->queryInterface(IAudioProcessor_iid,reinterpret_cast<void**>(&processor))==kResultOk && processor!=nullptr,"fresh Vocal component exposes audio processor");
    MemoryStream component_restore(saved_state.bytes());
    MemoryStream controller_restore(saved_state.bytes());
    CHECK_STAGE(component->setState(&component_restore)==kResultOk,"restore fresh Vocal component state");
    CHECK_STAGE(controller->setComponentState(&controller_restore)==kResultOk,"synchronize fresh Vocal controller from component state");
    bool recalled_exactly=true;
    for(ParamID id=0;id<recalled_values.size();++id)recalled_exactly=recalled_exactly&&std::abs(controller->getParamNormalized(id)-recalled_values[id])<1.0e-7;
    CHECK_STAGE(recalled_exactly,"all 19 Vocal parameters survive split component/controller reconstruction");
    CHECK_STAGE(processor->setupProcessing(setup)==kResultOk,"fresh Vocal setupProcessing after state restore");
    float recalled_dry_delta=0.0f;
    CHECK_STAGE(process_block(processor,256,nullptr,0.0f,&recalled_dry_delta),"process recalled Vocal state");
    CHECK_STAGE(recalled_dry_delta<1.0e-5f,"recalled Mix=0 reaches component DSP state");

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

    // Product-chain warmup while Mix=0 must not affect the exact dry output.
    ProcessSetup accurate_setup{};accurate_setup.sampleRate=48000.0;accurate_setup.maxSamplesPerBlock=256;
    CHECK_STAGE(processor->setupProcessing(accurate_setup)==kResultOk,"prepare sample-accurate Vocal block");
    OneParameterChange initial_dry(8,0.0);
    float rms=0.0f;
    CHECK_STAGE(process_block(processor,256,&initial_dry,0.0f,&rms),"arm dry initial state");
    for(int b=0;b<32;++b)
        CHECK_STAGE(process_block(processor,256,nullptr,static_cast<float>((b+1)*256),&rms),
                    "warm vocal detector without bypassing DSP");
    TwoParameterPoints midblock_change(8,128,0.0,1.0);
    float first_delta=0.0f,second_delta=0.0f;
    CHECK_STAGE(process_block(processor,256,&midblock_change,33.0f*256.0f,&rms,&first_delta,&second_delta),
                "render two host parameter points in one buffer");
    CHECK_STAGE(first_delta<1.0e-5f && second_delta>1.0e-5f,
                "offset 128 changes only the second half of the Vocal audio buffer");

    OneParameterChange invalid_automation(8,std::numeric_limits<double>::quiet_NaN());
    CHECK_STAGE(process_block(processor,256,&invalid_automation,34.0f*256.0f,&rms),
                "ignore nonfinite host automation in audio callback");
    MemoryStream post_invalid;
    CHECK_STAGE(component->getState(&post_invalid)==kResultOk,"serialize after invalid automation");
    float preserved_mix=0.0f;
    std::memcpy(&preserved_mix,post_invalid.bytes().data()+12u+8u*sizeof(float),sizeof(preserved_mix));
    CHECK_STAGE(std::abs(preserved_mix-1.0f)<1.0e-7f,"invalid host automation cannot poison serialized state");

    // Scale-aware pitch correction difference: Chromatic vs Major on off-scale 370 Hz (F#4 in C)
    {
        auto render_scale = [&](double scale_norm) {
            IComponent* sc_comp=nullptr; IAudioProcessor* sc_proc=nullptr;
            factory2->createInstance(component_info.cid,IComponent_iid,reinterpret_cast<void**>(&sc_comp));
            sc_comp->queryInterface(IAudioProcessor_iid,reinterpret_cast<void**>(&sc_proc));
            ProcessSetup s{}; s.sampleRate=48000.0; s.maxSamplesPerBlock=256;
            sc_proc->setupProcessing(s);
            sc_comp->setActive(true);
            class MultiChanges final : public IParameterChanges {
            public:
                std::vector<OnePointQueue> q;
                tresult SMTG_STDCALL queryInterface(const TUID,void** o) override {if(o)*o=nullptr;return kResultFalse;}
                uint32 SMTG_STDCALL addRef() override{return 1;} uint32 SMTG_STDCALL release() override{return 1;}
                int32 SMTG_STDCALL getParameterCount() override{return static_cast<int32>(q.size());}
                IParamValueQueue* SMTG_STDCALL getParameterData(int32 i) override{return (i>=0&&static_cast<size_t>(i)<q.size())?&q[static_cast<size_t>(i)]:nullptr;}
                IParamValueQueue* SMTG_STDCALL addParameterData(const ParamID&,int32&) override{return nullptr;}
            } chg;
            chg.q.emplace_back(0,1.0);
            chg.q.emplace_back(1,1.0);
            chg.q.emplace_back(2,0.0);
            chg.q.emplace_back(3,0.0);
            chg.q.emplace_back(4,1.0);
            chg.q.emplace_back(8,1.0);
            chg.q.emplace_back(11,0.0); // C
            chg.q.emplace_back(12,scale_norm);
            chg.q.emplace_back(17,0.0); // Eco Off

            std::vector<float> captured;
            float in_l[256]{},in_r[256]{},out_l[256]{},out_r[256]{};
            float* ich[2]={in_l,in_r}; float* och[2]={out_l,out_r};
            AudioBusBuffers ib{2,0,ich}, ob{2,0,och};
            ProcessData pd{}; pd.numSamples=256; pd.numInputs=1; pd.numOutputs=1; pd.inputs=&ib; pd.outputs=&ob;
            for(int b=0;b<48;++b){
                for(int i=0;i<256;++i){
                    const float ph=6.283185307179586f*370.0f*static_cast<float>(b*256+i)/48000.0f;
                    in_l[i]=in_r[i]=0.38f*std::sin(ph);
                }
                pd.inputParameterChanges=(b==0)?&chg:nullptr;
                sc_proc->process(pd);
                if(b>=32) captured.insert(captured.end(),out_l,out_l+256);
            }
            sc_comp->setActive(false); sc_proc->release(); sc_comp->release();
            return captured;
        };
        const auto chrom=render_scale(0.0);
        const auto maj=render_scale(1.0/29.0);
        double diff_sq=0.0;
        for(size_t i=0;i<chrom.size();++i){
            const double d=static_cast<double>(chrom[i])-static_cast<double>(maj[i]);
            diff_sq+=d*d;
        }
        const double scale_diff_rms=std::sqrt(diff_sq/static_cast<double>(chrom.size()));
        CHECK_STAGE(scale_diff_rms>1.0e-3,
                    "scale-aware pitch correction differentiates Chromatic vs Major on off-scale 370 Hz");
    }

    // Eco Channel (1..8) bus slot isolation on MEC4 shared memory
    {
        wchar_t shm_name[96]{};
        std::swprintf(shm_name,96,L"Local\\MonkeysEarEcoBus4_%lu",static_cast<unsigned long>(GetCurrentProcessId()));
        HANDLE hMap=OpenFileMappingW(FILE_MAP_READ,FALSE,shm_name);
        CHECK_STAGE(hMap!=nullptr,"open process MEC4 shared memory bus");
        const auto* shm=static_cast<const monkeys_ear::EcosystemSharedMemory*>(
            MapViewOfFile(hMap,FILE_MAP_READ,0,0,sizeof(monkeys_ear::EcosystemSharedMemory)));
        CHECK_STAGE(shm!=nullptr && shm->magic==monkeys_ear::EcosystemSharedMemory::kMagic,
                    "verify MEC4 shared memory header");

        IComponent *eco_a=nullptr,*eco_b=nullptr;
        IAudioProcessor *proc_a=nullptr,*proc_b=nullptr;
        factory2->createInstance(component_info.cid,IComponent_iid,reinterpret_cast<void**>(&eco_a));
        factory2->createInstance(component_info.cid,IComponent_iid,reinterpret_cast<void**>(&eco_b));
        eco_a->queryInterface(IAudioProcessor_iid,reinterpret_cast<void**>(&proc_a));
        eco_b->queryInterface(IAudioProcessor_iid,reinterpret_cast<void**>(&proc_b));
        proc_a->setupProcessing(accurate_setup); proc_b->setupProcessing(accurate_setup);
        eco_a->setActive(true); eco_b->setActive(true);

        float dummy_rms=0.0f;
        OneParameterChange a_pub(17,1.0/3.0), a_ch2(18,1.0/7.0);
        process_block(proc_a,256,&a_pub,0.0f,&dummy_rms);
        process_block(proc_a,256,&a_ch2,256.0f,&dummy_rms);
        for(int b=0;b<4;++b) process_block(proc_a,256,nullptr,static_cast<float>((b+2)*256),&dummy_rms);
        CHECK_STAGE(shm->slots[1].vocal.active && !shm->slots[0].vocal.active && !shm->slots[2].vocal.active,
                    "Eco Channel 2 isolates Vocal bus publication from Ch 1 and Ch 3");

        OneParameterChange a_ch5(18,4.0/7.0);
        process_block(proc_a,256,&a_ch5,6.0f*256.0f,&dummy_rms);
        process_block(proc_a,256,nullptr,7.0f*256.0f,&dummy_rms);
        CHECK_STAGE(!shm->slots[1].vocal.active && shm->slots[4].vocal.active,
                    "switching Eco Channel deactivates old slot 2 and activates slot 5");

        OneParameterChange b_pub(17,1.0/3.0), b_ch6(18,5.0/7.0);
        process_block(proc_b,256,&b_pub,0.0f,&dummy_rms);
        process_block(proc_b,256,&b_ch6,256.0f,&dummy_rms);
        process_block(proc_b,256,nullptr,512.0f,&dummy_rms);
        CHECK_STAGE(shm->slots[4].vocal.active && shm->slots[5].vocal.active,
                    "multiple Vocal instances coexist independently on Ch 5 and Ch 6");

        eco_a->setActive(false);
        CHECK_STAGE(!shm->slots[4].vocal.active && shm->slots[5].vocal.active,
                    "setActive(false) clears Ch 5 without disturbing Ch 6");
        eco_b->setActive(false);
        CHECK_STAGE(!shm->slots[5].vocal.active,"setActive(false) clears Ch 6");

        proc_a->release(); eco_a->release();
        proc_b->release(); eco_b->release();
        UnmapViewOfFile(shm);
        CloseHandle(hMap);
    }

    // VocalEditor 740x610 attach/paint/detach lifecycle
    {
        IPlugView* view=controller->createView("editor");
        CHECK_STAGE(view!=nullptr,"create VocalEditor view");
        CHECK_STAGE(view->isPlatformTypeSupported("HWND")==kResultOk,"VocalEditor supports HWND");
        ViewRect sz{};
        CHECK_STAGE(view->getSize(&sz)==kResultOk && (sz.right-sz.left)==740 && (sz.bottom-sz.top)==610,
                    "VocalEditor reports 740x610 dimensions");
        HWND parent=CreateWindowExA(0,"STATIC","VocalHost",WS_POPUP,0,0,740,610,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
        CHECK_STAGE(parent!=nullptr && view->attached(parent,"HWND")==kResultOk,
                    "VocalEditor attaches to 740x610 host HWND");
        HWND child=FindWindowExA(parent,nullptr,"MonkeysEarVocalEditorWnd",nullptr);
        CHECK_STAGE(child!=nullptr,"VocalEditor child window created");
        RedrawWindow(child,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW);
        SendMessageA(child,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(660,35)); // ACTIVE/BYPASS toggle
        SendMessageA(child,WM_LBUTTONUP,0,MAKELPARAM(660,35));
        CHECK_STAGE(view->removed()==kResultOk,"VocalEditor detaches cleanly");
        CHECK_STAGE(view->attached(parent,"HWND")==kResultOk,"VocalEditor reattaches cleanly");
        HWND child2=FindWindowExA(parent,nullptr,"MonkeysEarVocalEditorWnd",nullptr);
        if(child2) RedrawWindow(child2,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW);
        CHECK_STAGE(view->removed()==kResultOk,"VocalEditor detaches cleanly after reopen");
        CHECK_STAGE(view->release()==0,"VocalEditor reference count releases to 0");
        DestroyWindow(parent);
    }

    // Deliberately create both components BEFORE the controller. The old
    // g_latest_state design incorrectly attached the controller to component B.
    IComponent *sibling_a=nullptr,*sibling_b=nullptr;
    IEditController* sibling_controller=nullptr;
    CHECK_STAGE(factory2->createInstance(component_info.cid,IComponent_iid,
                reinterpret_cast<void**>(&sibling_a))==kResultOk && sibling_a,
                "create sibling Vocal component A");
    CHECK_STAGE(factory2->createInstance(component_info.cid,IComponent_iid,
                reinterpret_cast<void**>(&sibling_b))==kResultOk && sibling_b,
                "create sibling Vocal component B");
    CHECK_STAGE(factory2->createInstance(controller_info.cid,IEditController_iid,
                reinterpret_cast<void**>(&sibling_controller))==kResultOk && sibling_controller,
                "create controller after both independent components");
    CHECK_STAGE(sibling_controller->setParamNormalized(8,0.125)==kResultOk,
                "edit controller without component cross-talk");
    MemoryStream sibling_a_state,sibling_b_state;
    CHECK_STAGE(sibling_a->getState(&sibling_a_state)==kResultOk &&
                sibling_b->getState(&sibling_b_state)==kResultOk,
                "serialize both independent sibling components");
    float sibling_a_mix=0.0f,sibling_b_mix=0.0f;
    std::memcpy(&sibling_a_mix,sibling_a_state.bytes().data()+12u+8u*sizeof(float),sizeof(float));
    std::memcpy(&sibling_b_mix,sibling_b_state.bytes().data()+12u+8u*sizeof(float),sizeof(float));
    CHECK_STAGE(std::abs(sibling_a_mix-1.0f)<1.0e-7f && std::abs(sibling_b_mix-1.0f)<1.0e-7f,
                "no global latest-component state leak across Vocal instances");
    sibling_controller->release();sibling_b->release();sibling_a->release();

    std::cout<<">>> ALL STANDALONE VOCAL VST3 HOST/AUTOMATION CHECKS PASSED! <<<\n";
    controller->release();processor->release();component->release();factory2->release();factory->release();

    // Closing all factory references must not strand a global dangling
    // factory pointer. Hosts can unload/reload or query the export repeatedly.
    IPluginFactory* reopened_factory=factory_proc();
    CHECK_STAGE(reopened_factory!=nullptr && reopened_factory->countClasses()==2,
                "factory export survives complete reference release");
    reopened_factory->release();
    FreeLibrary(module);
    return 0;
}
