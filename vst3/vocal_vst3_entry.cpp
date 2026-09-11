#include "vst3_sdk_minimal.h"
#include "monkeys_ear/vocal_module.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int kParamCount = 11;
constexpr uint32 kStateMagic = 0x4D455643u; // MEVC
constexpr uint32 kStateVersion = 1u;
static const TUID kVocalComponentCID = INLINE_UID(0x4D6F6E6B, 0x65794561, 0x72566F63, 0x616C3031); // Monke yEar Vocal01
static const TUID kVocalControllerCID = INLINE_UID(0x4D6F6E6B, 0x65794561, 0x72566374, 0x726C3031); // Monke yEar Vctrl01

void copy16(char16* dst, const char* src) { size_t i=0; while(src[i] && i<127){dst[i]=static_cast<char16>(src[i]);++i;} dst[i]=0; }
bool write_stream(IBStream* s, void* data, int32 bytes) { int32 done=0; return s && s->write(data,bytes,&done)==kResultOk && done==bytes; }
bool read_stream(IBStream* s, void* data, int32 bytes) { int32 done=0; return s && s->read(data,bytes,&done)==kResultOk && done==bytes; }

struct VocalState {
    std::array<std::atomic<float>,kParamCount> values;
    std::array<std::atomic<bool>,kParamCount> dirty;
    VocalState() {
        constexpr float defaults[kParamCount]={1.0f,.45f,.35f,.80f,.55f,.50f,.10f,0.0f,1.0f,.35f,.90f};
        for(int i=0;i<kParamCount;++i){values[i].store(defaults[i]);dirty[i].store(false);}
    }
};
std::mutex g_state_mutex;
std::shared_ptr<VocalState> g_latest_state;

tresult write_state(IBStream* stream,const std::shared_ptr<VocalState>& state) {
    if(!stream||!state) return kInvalidArgument;
    uint32 magic=kStateMagic,version=kStateVersion,count=kParamCount;
    if(!write_stream(stream,&magic,4)||!write_stream(stream,&version,4)||!write_stream(stream,&count,4)) return kInternalError;
    for(int i=0;i<kParamCount;++i){float value=state->values[i].load();if(!write_stream(stream,&value,4))return kInternalError;} return kResultOk;
}
tresult read_state(IBStream* stream,const std::shared_ptr<VocalState>& state) {
    if(!stream||!state)return kInvalidArgument;
    uint32 magic=0,version=0,count=0;
    if(!read_stream(stream,&magic,4)||!read_stream(stream,&version,4)||!read_stream(stream,&count,4))return kInternalError;
    if(magic!=kStateMagic||version>kStateVersion||count>kParamCount)return kResultFalse;
    for(uint32 i=0;i<count;++i){float value=0;if(!read_stream(stream,&value,4))return kInternalError;state->values[i].store(std::clamp(value,0.0f,1.0f));state->dirty[i].store(true);}return kResultOk;
}

class VocalComponent final : public IComponent, public IAudioProcessor {
public:
    VocalComponent():ref_(1),state_(std::make_shared<VocalState>()){std::lock_guard<std::mutex> lock(g_state_mutex);g_latest_state=state_;module_.prepare(48000.0f,512);apply_controls();}
    tresult SMTG_STDCALL queryInterface(const TUID iid,void** obj) override {if(!obj)return kInvalidArgument;if(!std::memcmp(iid,FUnknown_iid,sizeof(TUID))||!std::memcmp(iid,IPluginBase_iid,sizeof(TUID))||!std::memcmp(iid,IComponent_iid,sizeof(TUID))){*obj=static_cast<IComponent*>(this);}else if(!std::memcmp(iid,IAudioProcessor_iid,sizeof(TUID))){*obj=static_cast<IAudioProcessor*>(this);}else{*obj=nullptr;return kResultFalse;}addRef();return kResultOk;}
    uint32 SMTG_STDCALL addRef() override{return ++ref_;} uint32 SMTG_STDCALL release() override{uint32 n=--ref_;if(!n)delete this;return n;}
    tresult SMTG_STDCALL initialize(FUnknown*) override{return kResultOk;} tresult SMTG_STDCALL terminate() override{return kResultOk;}
    tresult SMTG_STDCALL getControllerClassId(TUID id) override{std::memcpy(id,kVocalControllerCID,sizeof(TUID));return kResultOk;} tresult SMTG_STDCALL setIoMode(IoMode) override{return kResultOk;}
    int32 SMTG_STDCALL getBusCount(MediaType type,BusDirection dir) override{return type==kAudio?1:0;}
    tresult SMTG_STDCALL getBusInfo(MediaType type,BusDirection dir,int32 index,BusInfo& bus) override{if(type!=kAudio||index)return kInvalidArgument;bus.mediaType=type;bus.direction=dir;bus.busType=kMain;bus.flags=1;bus.channelCount=2;copy16(bus.name,dir==kInput?"Vocal In":"Vocal Out");return kResultOk;}
    tresult SMTG_STDCALL getRoutingInfo(RoutingInfo&,RoutingInfo&) override{return kNotImplemented;} tresult SMTG_STDCALL activateBus(MediaType,BusDirection,int32,bool) override{return kResultOk;}
    tresult SMTG_STDCALL setActive(bool state) override{if(!state)module_.reset();return kResultOk;} tresult SMTG_STDCALL setState(IBStream* stream) override{return read_state(stream,state_);} tresult SMTG_STDCALL getState(IBStream* stream) override{return write_state(stream,state_);}
    tresult SMTG_STDCALL setBusArrangements(SpeakerArrangement* inputs,int32 numIns,SpeakerArrangement* outputs,int32 numOuts) override{if(numIns!=1||numOuts!=1||inputs[0]!=SpeakerArr::kStereo||outputs[0]!=SpeakerArr::kStereo)return kResultFalse;return kResultOk;}
    tresult SMTG_STDCALL getBusArrangement(BusDirection,int32 index,SpeakerArrangement& arr) override{if(index)return kInvalidArgument;arr=SpeakerArr::kStereo;return kResultOk;}
    tresult SMTG_STDCALL canProcessSampleSize(int32 size) override{return size==0?kResultOk:kResultFalse;} uint32 SMTG_STDCALL getLatencySamples() override{return 0;}
    tresult SMTG_STDCALL setupProcessing(ProcessSetup& setup) override{module_.prepare(static_cast<float>(setup.sampleRate),static_cast<uint32>(std::max(1,setup.maxSamplesPerBlock)));apply_controls();return kResultOk;} tresult SMTG_STDCALL setProcessing(bool) override{return kResultOk;}
    tresult SMTG_STDCALL process(ProcessData& data) override {
        if(data.inputParameterChanges){for(int32 q=0;q<data.inputParameterChanges->getParameterCount();++q){auto* queue=data.inputParameterChanges->getParameterData(q);if(!queue||!queue->getPointCount())continue;int32 offset=0;ParamValue value=0; if(queue->getPoint(queue->getPointCount()-1,offset,value)==kResultOk){const ParamID id=queue->getParameterId();if(id<kParamCount){state_->values[id].store(std::clamp(static_cast<float>(value),0.0f,1.0f));state_->dirty[id].store(false);}}}}
        bool changed=false;for(int i=0;i<kParamCount;++i)changed=state_->dirty[i].exchange(false)||changed;if(changed)apply_controls();
        if(data.numSamples<=0||data.numInputs<1||data.numOutputs<1||!data.inputs[0].channelBuffers32||!data.outputs[0].channelBuffers32)return kResultOk;
        auto* in=data.inputs[0].channelBuffers32;auto* out=data.outputs[0].channelBuffers32;if(data.inputs[0].numChannels<2||data.outputs[0].numChannels<2)return kResultFalse;module_.process_block(in[0],in[1],out[0],out[1],static_cast<uint32>(data.numSamples));return kResultOk;
    }
    uint32 SMTG_STDCALL getTailSamples() override{return 0;}
private:
    void apply_controls(){auto value=[this](int i){return state_->values[i].load();}; monkeys_ear::VocalExpressionControls c{};c.enabled=value(0)>=.5f;c.correction_strength=value(1);c.drift_retention=value(2);c.vibrato_retention=value(3);c.transition=value(4);c.formant_repair=value(5);c.spectral_residual_mix=value(6);c.character=value(7);c.mix=value(8);c.sequential_stage_mix=value(9);c.aperiodic_protection=value(10);module_.set_controls(c);module_.set_bypass(!c.enabled);}
    std::atomic<uint32> ref_;std::shared_ptr<VocalState> state_;monkeys_ear::VocalModule module_{};
};

class VocalController final : public IEditController {
public:
    VocalController():ref_(1){std::lock_guard<std::mutex> lock(g_state_mutex);state_=g_latest_state?g_latest_state:std::make_shared<VocalState>();}
    tresult SMTG_STDCALL queryInterface(const TUID iid,void** obj) override{if(!obj)return kInvalidArgument;if(!std::memcmp(iid,FUnknown_iid,sizeof(TUID))||!std::memcmp(iid,IPluginBase_iid,sizeof(TUID))||!std::memcmp(iid,IEditController_iid,sizeof(TUID))){*obj=static_cast<IEditController*>(this);addRef();return kResultOk;}*obj=nullptr;return kResultFalse;} uint32 SMTG_STDCALL addRef() override{return ++ref_;}uint32 SMTG_STDCALL release() override{uint32 n=--ref_;if(!n)delete this;return n;}tresult SMTG_STDCALL initialize(FUnknown*) override{return kResultOk;}tresult SMTG_STDCALL terminate() override{return kResultOk;}
    tresult SMTG_STDCALL setComponentState(IBStream* s) override{return read_state(s,state_);}tresult SMTG_STDCALL setState(IBStream* s) override{return read_state(s,state_);}tresult SMTG_STDCALL getState(IBStream* s) override{return write_state(s,state_);}int32 SMTG_STDCALL getParameterCount() override{return kParamCount;}
    tresult SMTG_STDCALL getParameterInfo(int32 i,ParameterInfo& info) override{static constexpr const char* names[kParamCount]={"VOCAL: Enable","VOCAL: Correction Strength","VOCAL: Drift Retention","VOCAL: Vibrato Retention","VOCAL: Transition","VOCAL: Envelope Repair","VOCAL: Spectral Residual","VOCAL: Character","VOCAL: Mix","VOCAL: Soft Sequential Staging","VOCAL: Aperiodic Protection"};if(i<0||i>=kParamCount)return kInvalidArgument;info.id=static_cast<ParamID>(i);info.stepCount=i==0?1:0;info.unitId=0;info.flags=kCanAutomate|(i==0?kIsBypass:0);info.defaultNormalizedValue=state_->values[i].load();copy16(info.title,names[i]);copy16(info.shortTitle,names[i]);copy16(info.units,i==4?"":"%");return kResultOk;}
    tresult SMTG_STDCALL getParamStringByValue(ParamID id,ParamValue value,char16 string[128]) override{char text[32];if(id==0)std::snprintf(text,sizeof(text),"%s",value>=.5?"On":"Bypass");else if(id==4)std::snprintf(text,sizeof(text),"%.1f ms",2.0+(1.0-value)*58.0);else std::snprintf(text,sizeof(text),"%.1f %%",value*100.0);copy16(string,text);return kResultOk;}tresult SMTG_STDCALL getParamValueByString(ParamID,const char16*,ParamValue& value) override{value=.5;return kResultOk;}ParamValue SMTG_STDCALL normalizedParamToPlain(ParamID,ParamValue v) override{return v;}ParamValue SMTG_STDCALL plainParamToNormalized(ParamID,ParamValue v) override{return v;}ParamValue SMTG_STDCALL getParamNormalized(ParamID id) override{return id<kParamCount?state_->values[id].load():0.0;}tresult SMTG_STDCALL setParamNormalized(ParamID id,ParamValue value) override{if(id>=kParamCount)return kInvalidArgument;state_->values[id].store(std::clamp(static_cast<float>(value),0.0f,1.0f));state_->dirty[id].store(true);return kResultOk;}tresult SMTG_STDCALL setComponentHandler(IComponentHandler*) override{return kResultOk;}void* SMTG_STDCALL createView(const char8*) override{return nullptr;}
private:std::atomic<uint32> ref_;std::shared_ptr<VocalState> state_;
};

class VocalFactory final : public IPluginFactory2 {
public:
    VocalFactory():ref_(1){}tresult SMTG_STDCALL queryInterface(const TUID iid,void** obj) override{if(!obj)return kInvalidArgument;if(!std::memcmp(iid,FUnknown_iid,sizeof(TUID))||!std::memcmp(iid,IPluginFactory_iid,sizeof(TUID))||!std::memcmp(iid,IPluginFactory2_iid,sizeof(TUID))){*obj=static_cast<IPluginFactory2*>(this);addRef();return kResultOk;}*obj=nullptr;return kResultFalse;}uint32 SMTG_STDCALL addRef() override{return ++ref_;}uint32 SMTG_STDCALL release() override{uint32 n=--ref_;if(!n)delete this;return n;}
    tresult SMTG_STDCALL getFactoryInfo(PFactoryInfo* info) override{if(!info)return kInvalidArgument;std::strncpy(info->vendor,"Ultramonkeydog Studios",sizeof(info->vendor)-1);std::strncpy(info->url,"https://ultramonkeydog-studios.vercel.app",sizeof(info->url)-1);info->flags=0;return kResultOk;}int32 SMTG_STDCALL countClasses() override{return 2;}
    tresult SMTG_STDCALL getClassInfo(int32 i,PClassInfo* info) override{return class_info(i,info,static_cast<PClassInfo2*>(nullptr));}tresult SMTG_STDCALL getClassInfo2(int32 i,PClassInfo2* info) override{return class_info(i,static_cast<PClassInfo*>(nullptr),info);}tresult SMTG_STDCALL createInstance(const TUID cid,const TUID iid,void** obj) override{if(!std::memcmp(cid,kVocalComponentCID,sizeof(TUID))){auto* p=new VocalComponent();auto r=p->queryInterface(iid,obj);p->release();return r;}if(!std::memcmp(cid,kVocalControllerCID,sizeof(TUID))){auto* p=new VocalController();auto r=p->queryInterface(iid,obj);p->release();return r;}return kInvalidArgument;}
private:
    template<class A,class B> tresult class_info(int32 i,A* a,B* b){if(i<0||i>1)return kInvalidArgument;auto cid=i==0?kVocalComponentCID:kVocalControllerCID;const char* name=i==0?"Monkey's Ear Vocal":"Monkey's Ear Vocal Controller";if(a){std::memcpy(a->cid,cid,sizeof(TUID));a->cardinality=0x7fffffff;std::strncpy(a->category,i==0?"Audio Module Class":"Component Controller Class",sizeof(a->category)-1);std::strncpy(a->name,name,sizeof(a->name)-1);}if(b){std::memcpy(b->cid,cid,sizeof(TUID));b->cardinality=0x7fffffff;std::strncpy(b->category,i==0?"Audio Module Class":"Component Controller Class",sizeof(b->category)-1);std::strncpy(b->name,name,sizeof(b->name)-1);b->classFlags=0;std::strncpy(b->subCategories,i==0?"Fx|Pitch Shift":"",sizeof(b->subCategories)-1);std::strncpy(b->vendor,"Ultramonkeydog Studios",sizeof(b->vendor)-1);std::strncpy(b->version,"1.0.0",sizeof(b->version)-1);std::strncpy(b->sdkVersion,"VST 3.7.0",sizeof(b->sdkVersion)-1);}return kResultOk;}
    std::atomic<uint32> ref_;
};
VocalFactory* g_factory=nullptr;
} // namespace

extern "C" {
SMTG_EXPORT_SYMBOL IPluginFactory* SMTG_STDCALL GetPluginFactory(){if(!g_factory)g_factory=new VocalFactory();else g_factory->addRef();return g_factory;}
#if defined(_WIN32)
SMTG_EXPORT_SYMBOL bool SMTG_STDCALL InitDll(){return true;}
SMTG_EXPORT_SYMBOL bool SMTG_STDCALL ExitDll(){return true;}
#endif
}
