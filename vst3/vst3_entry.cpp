#include "vst3_sdk_minimal.h"
#include "native_editor.h"
#include "preset_param_snapshot.h"
#include "snapshot_replay.h"
#include "monkeys_ear/engine.h"
#include <atomic>
#include <cstring>
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <cstdio>
#include <array>
#include <bit>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cctype>

using namespace Steinberg;
using namespace Steinberg::Vst;

// Instrument Component CID: 4D6F6E6B-6579-7345-6172-496E73747201 ("MonkeysEarInstr1")
static const TUID kMonkeysEarComponentCID = INLINE_UID(
    0x4D6F6E6B, 0x65797345, 0x6172496E, 0x73747201
);

// Audio FX Component CID: 4D6F6E6B-6579-7345-6172-467870726F01 ("MonkeysEarFxpro1")
static const TUID kMonkeysEarFxComponentCID = INLINE_UID(
    0x4D6F6E6B, 0x65797345, 0x61724678, 0x70726F01
);

// Controller CID: 4D6F6E6B-6579-7345-6172-4374726C7202 ("MonkeysEarCtrlr2")
static const TUID kMonkeysEarControllerCID = INLINE_UID(
    0x4D6F6E6B, 0x65797345, 0x61724374, 0x726C7202
);

static constexpr int kNumParams = 178;
// Root + existing coherent, independently navigable instrument subsystems.
static constexpr int kUnitCount = 15;
static constexpr const char* kUnitNames[kUnitCount]={
    "Monkey's Ear","PLAY / Macros","Oscillators / Voice","Filters / Envelopes",
    "Four-band EQ","Rhythm / Modulation","ChronoState","Sound Space",
    "Vocal Expression","Tube Rack","Tube 1","Tube 2","Tube 3","Tube 4",
    "Master / Routing"
};
static int32 unit_for_parameter(int32 id){
    if(id<=7||id==79||id==172||id==173||id==176||id==177)return 1;
    if(id==8||id==18||id==80||id==81)return 14;
    if((id>=9&&id<=12)||(id>=19&&id<=27)||(id>=70&&id<=78)||(id>=161&&id<=164))return 2;
    if((id>=28&&id<=36)||(id>=165&&id<=168))return 3;
    if(id>=13&&id<=17)return 6;
    if(id>=37&&id<=54)return 4;
    if((id>=55&&id<=69)||(id>=169&&id<=171)||id>=174)return 5;
    if(id>=82&&id<=112)return 7;
    if(id>=113&&id<=123)return 8;
    if(id>=124&&id<=128)return 9;
    if(id>=129&&id<=160)return 10+(id-129)/8;
    return 0;
}
static constexpr uint32 kStateMagic = 0x4D453032u; // ME02
static constexpr uint32 kStateVersion = 7u;

// Avoid C++ fast-math assumptions when host automation includes NaN/Inf.
static bool valid_normalized(ParamValue value) {
    const uint64_t bits=std::bit_cast<uint64_t>(value);
    return (bits & 0x7ff0000000000000ULL)!=0x7ff0000000000000ULL;
}
static tresult write_plugin_state(IBStream* stream, const std::shared_ptr<struct SharedPluginState>& state);
static tresult read_plugin_state(IBStream* stream, const std::shared_ptr<struct SharedPluginState>& state);

// Helper to copy ASCII string to char16_t array
static void copy_to_char16(char16* dest, const char* src, size_t max_len) {
    size_t i = 0;
    while (src[i] && i < max_len - 1) {
        dest[i] = static_cast<char16>(src[i]);
        i++;
    }
    dest[i] = 0;
}

// ── Shared State between Component (DSP) and Controller (UI/Params) ───────
struct SharedPluginState {
    std::atomic<float> params[kNumParams];
    std::atomic<bool> dirty[kNumParams];
    std::atomic<bool> restore_in_progress{false};
    std::atomic<bool> restored_snapshot_pending{false};

    SharedPluginState(bool is_fx = false) {
        for (int i = 0; i < kNumParams; ++i) params[i].store(0.5f);
        params[0].store(0.65f); // Cutoff
        params[1].store(0.25f); // Resonance
        params[2].store(0.30f); // Drive
        params[3].store(0.40f); // Body
        params[4].store(0.25f); // Delay
        params[5].store(0.35f); // Space
        params[6].store(is_fx ? 1.00f : 0.00f); // Mic Blend
        params[7].store(0.50f); // Character
        params[8].store(0.85f); // Master Gain (0dB)
        params[9].store(0.00f); // Waveform (Saw)
        params[10].store(0.00f); // Osc FM Depth
        params[11].store(0.50f); // Osc 2 Semi (0 semi)
        params[12].store(0.00f); // Osc Hard Sync (Off)
        params[13].store(0.35f); // State Resistance
        params[14].store(0.40f); // State Repulsion (Negative Gravity)
        params[15].store(0.30f); // State Phase Coupling
        params[16].store(0.50f); // State Persistence
        params[17].store(1.00f); // State Mechanism Enable (1 = Stateful, 0 = Conventional)
        params[18].store(is_fx ? 1.00f : 0.00f); // Input Route Mode (0: Synth, 0.5: Blend, 1.0: Ext Audio)
        params[19].store(0.20f); params[20].store(0.20f); params[21].store(0.34f);
        params[22].store(0.0f); params[23].store(0.0f); params[24].store(0.0f); params[25].store(1.0f);
        params[26].store(0.85f); params[27].store(0.75f); params[28].store(0.0f); params[29].store(0.92f);
        params[30].store(0.0f); params[31].store(0.0f); params[32].store(1.0f); params[33].store(0.0f);
        params[34].store(0.0f); params[35].store(0.0f); params[36].store(0.0f);
        for(int b=0;b<4;++b){ params[37+b*4].store(0.0f); params[38+b*4].store(0.5f); params[39+b*4].store(0.5f); params[40+b*4].store(0.35f); }
        params[53].store(0.0f); params[54].store(1.0f); params[55].store(0.48f); params[56].store(0.0f);
        params[57].store(0.35f); for(int i=58;i<=69;++i) params[i].store(0.5f);
        params[70].store(0.0f); params[71].store(0.0f); params[72].store(0.0f); params[73].store(0.043f);
        params[74].store(0.0f); params[75].store(0.0f); params[76].store(0.5f); params[77].store(0.5f); params[78].store(0.0f);
        params[79].store(0.0f);
        params[80].store(0.5f); params[81].store(0.0f);
        params[82].store(0.0f); params[83].store(0.0f); params[84].store(0.0f); params[85].store(0.0f);
        params[86].store(0.47f); for(int i=87;i<=90;++i)params[i].store(0.5f); params[91].store(0.5f);
        params[92].store(0.0f);params[93].store(0.5f);for(int i=94;i<=97;++i)params[i].store(0.0f);
        params[98].store(0.5f);params[99].store(0.5f);params[100].store(0.5f);
        for(int route=0;route<4;++route){params[101+route*3].store(0.0f);params[102+route*3].store(0.0f);params[103+route*3].store(0.5f);}
        params[113].store(is_fx ? 1.0f : 0.0f); params[114].store(.72f); params[115].store(.62f); params[116].store(.82f); params[117].store(.68f); params[118].store(.58f); params[119].store(.10f); params[120].store(.24f); params[121].store(.88f); params[122].store(.34f); params[123].store(.88f);
        params[124].store(0.0f);params[125].store(0.0f);params[126].store(.35f);params[127].store(.5f);params[128].store(1.0f);
        for(int i=0;i<4;++i){
            const int b=129+8*i;
            params[b].store(1.0f);
            params[b+1].store((static_cast<float>(i)+.1f)/monkeys_ear::kTubeFlavorCount);
            params[b+2].store(.32f-.03f*i);
            params[b+3].store(.5f);params[b+4].store(.5f);
            params[b+5].store(.3f);params[b+6].store(.2f);
            params[b+7].store(.65f-.06f*i);
        }
        const auto env_norm=[](float seconds){return std::sqrt(std::max(0.0f,(seconds-.001f)/6.0f));};
        params[161].store(env_norm(.005f));params[162].store(env_norm(.15f));params[163].store(.70f);params[164].store(env_norm(.25f));
        params[165].store(env_norm(.010f));params[166].store(env_norm(.20f));params[167].store(.30f);params[168].store(env_norm(.30f));
        params[169].store(0.0f);params[170].store(.40f);params[171].store((4.1f)/12.0f);
        params[172].store(0.0f); // CC64 hold; never recalled into a held note
        params[173].store(0.0f); // expanded palette; legacy ID79 unchanged
        params[174].store(0.0f); // opt-in host tempo; historic patches keep manual clock
        params[175].store(0.0f); // straight rhythmic phase at factory defaults
        params[176].store(0.0f); // neutral DRIFT gesture
        params[177].store(0.0f); // neutral FRACTURE gesture
        for (int i = 0; i < kNumParams; ++i) dirty[i].store(false);
    }
};

static bool stream_write(IBStream* s, void* data, int32 bytes) { int32 done=0; return s&&s->write(data,bytes,&done)==kResultOk&&done==bytes; }
static bool stream_read(IBStream* s, void* data, int32 bytes) { int32 done=0; return s&&s->read(data,bytes,&done)==kResultOk&&done==bytes; }
static tresult write_plugin_state(IBStream* stream, const std::shared_ptr<SharedPluginState>& state) {
    if(!stream||!state)return kInvalidArgument;
    uint32 magic=kStateMagic,version=kStateVersion,count=kNumParams;
    if(!stream_write(stream,&magic,4)||!stream_write(stream,&version,4)||!stream_write(stream,&count,4))return kInternalError;
    for(int i=0;i<kNumParams;++i){
        // Hardware damper state is ephemeral; opening a saved session must
        // never start with the sustain pedal held down.
        float v=i==172?0.0f:state->params[i].load();
        if(!stream_write(stream,&v,4))return kInternalError;
    }
    return kResultOk;
}
static tresult read_plugin_state(IBStream* stream, const std::shared_ptr<SharedPluginState>& state) {
    if(!stream||!state)return kInvalidArgument;
    uint32 magic=0,version=0,count=0;
    if(!stream_read(stream,&magic,4)||!stream_read(stream,&version,4)||
       !stream_read(stream,&count,4))return kInternalError;
    if(magic!=kStateMagic||version>kStateVersion||
       count>static_cast<uint32>(kNumParams))return kResultFalse;
    // Stage an entire read before publishing. A short or corrupt stream
    // must not leave a half-recalled synth behind.
    std::array<float,kNumParams> values{};
    for(uint32 i=0;i<count;++i){
        float value=0.0f;
        if(!stream_read(stream,&value,4))return kInternalError;
        const uint32_t bits=std::bit_cast<uint32_t>(value);
        const bool finite=(bits&0x7f800000u)!=0x7f800000u;
        values[i]=finite?std::clamp(value,0.0f,1.0f):0.0f;
    }
    if(count<static_cast<uint32>(kNumParams)){
        const SharedPluginState defaults(false);
        for(uint32 i=count;i<static_cast<uint32>(kNumParams);++i)
            values[i]=defaults.params[i].load();
    }
    values[172]=0.0f; // never restore a physical pedal as pressed
    for(int i=0;i<kNumParams;++i){
        state->params[i].store(values[i],std::memory_order_relaxed);
        state->dirty[i].store(true,std::memory_order_relaxed);
    }
    return kResultOk;
}

// The component does not allocate in process(): MIDI storage is an owned
// fixed-capacity buffer; parameter queues have one preallocated cursor/ParamID.
struct ScheduledMidi {
    Event event{};
    int32 order=0;
};
struct ParameterCursor {
    IParamValueQueue* queue=nullptr;
    int32 next=0, count=0, offset=0;
    float value=0.0f;
    bool pending=false;
};

// ── VST3 Component (Shared DSP Engine for Instrument & FX) ────────────────
class MonkeysEarComponent : public IComponent,
                            public IAudioProcessor,
                            public IProcessContextRequirements,
                            public IConnectionPoint {
public:
    explicit MonkeysEarComponent(bool is_fx = false)
        : ref_count_(1), is_fx_(is_fx), active_(false), processing_(false) {
        state_ = std::make_shared<SharedPluginState>(is_fx_);
        engine_.init(48000.0f, 512);
        if (is_fx_) {
            engine_.load_preset(monkeys_ear::PresetManager::create_factory_vocal_resonator());
        } else {
            engine_.load_preset(monkeys_ear::PresetManager::create_factory_lead());
        }
        // The host may save this component before the first audio callback.
        // Its initial state must represent the ACTUAL loaded preset, not
        // stale guessed defaults in SharedPluginState. Use the exact same
        // reverse projection as normal factory selection and the controller.
        sync_patch_controls_for_state();
    }

    virtual ~MonkeysEarComponent() = default;

    std::shared_ptr<SharedPluginState> get_state() const { return state_; }
    bool is_fx() const { return is_fx_; }

    // ── FUnknown ─────────────────────────────────────────────────────────────
    tresult SMTG_STDCALL queryInterface(const TUID _iid, void** obj) override {
        if (!obj) return kInvalidArgument;

        if (memcmp(_iid, FUnknown_iid, sizeof(TUID)) == 0 ||
            memcmp(_iid, IPluginBase_iid, sizeof(TUID)) == 0 ||
            memcmp(_iid, IComponent_iid, sizeof(TUID)) == 0) {
            *obj = static_cast<IComponent*>(this);
            addRef();
            return kResultOk;
        }
        if (memcmp(_iid, IAudioProcessor_iid, sizeof(TUID)) == 0) {
            *obj = static_cast<IAudioProcessor*>(this);
            addRef();
            return kResultOk;
        }
        if (memcmp(_iid, IProcessContextRequirements_iid, sizeof(TUID)) == 0) {
            *obj = static_cast<IProcessContextRequirements*>(this);
            addRef();
            return kResultOk;
        }
        if (memcmp(_iid, IConnectionPoint_iid, sizeof(TUID)) == 0) {
            *obj = static_cast<IConnectionPoint*>(this);
            addRef();
            return kResultOk;
        }

        *obj = nullptr;
        return kResultFalse;
    }

    uint32 SMTG_STDCALL addRef() override { return ++ref_count_; }
    uint32 SMTG_STDCALL release() override {
        uint32 count = --ref_count_;
        if (count == 0) delete this;
        return count;
    }

    // ── IPluginBase ──────────────────────────────────────────────────────────
    tresult SMTG_STDCALL initialize(FUnknown* /*context*/) override {
        return kResultOk;
    }

    tresult SMTG_STDCALL terminate() override {
        return kResultOk;
    }

    // ── IComponent ───────────────────────────────────────────────────────────
    tresult SMTG_STDCALL getControllerClassId(TUID classId) override {
        memcpy(classId, kMonkeysEarControllerCID, sizeof(TUID));
        return kResultOk;
    }

    tresult SMTG_STDCALL setIoMode(IoMode /*mode*/) override {
        return kResultOk;
    }

    int32 SMTG_STDCALL getBusCount(MediaType type, BusDirection dir) override {
        if (type == kAudio) {
            if (dir == kOutput) return 1; // 1 Main Stereo Out
            if (dir == kInput) {
                // Audio FX: 1 Main Stereo Audio In
                // Pure VSTi: 0 Main Audio In (host categorizes as !!!VSTi)
                return is_fx_ ? 1 : 0;
            }
        } else if (type == kEvent) {
            return (dir == kInput) ? 1 : 0; // 1 MIDI In
        }
        return 0;
    }

    tresult SMTG_STDCALL getBusInfo(MediaType type, BusDirection dir, int32 index, BusInfo& bus) override {
        if (index != 0) return kInvalidArgument;

        bus.mediaType = type;
        bus.direction = dir;
        bus.busType = kMain;
        bus.flags = 1; // kDefaultActive

        if (type == kAudio && dir == kOutput) {
            bus.channelCount = 2; // Stereo
            copy_to_char16(bus.name, "Main Out", 128);
            return kResultOk;
        } else if (type == kAudio && dir == kInput && is_fx_) {
            bus.channelCount = 2; // Stereo Input for FX processing
            copy_to_char16(bus.name, "Main In", 128);
            return kResultOk;
        } else if (type == kEvent && dir == kInput) {
            bus.channelCount = 16; // 16 MIDI Channels
            copy_to_char16(bus.name, "MIDI In", 128);
            return kResultOk;
        }
        return kInvalidArgument;
    }

    tresult SMTG_STDCALL getRoutingInfo(RoutingInfo& /*inInfo*/, RoutingInfo& /*outInfo*/) override {
        return kNotImplemented;
    }

    tresult SMTG_STDCALL activateBus(MediaType /*type*/, BusDirection /*dir*/, int32 /*index*/, bool /*state*/) override {
        return kResultOk;
    }

    tresult SMTG_STDCALL setActive(bool state) override {
        active_ = state;
        if (!state) {
            engine_.reset();
        }
        return kResultOk;
    }

    tresult SMTG_STDCALL setState(IBStream* stream) override {
        state_->restore_in_progress.store(true,std::memory_order_release);
        const tresult result=read_plugin_state(stream,state_);
        if(result==kResultOk)
            state_->restored_snapshot_pending.store(true,std::memory_order_release);
        state_->restore_in_progress.store(false,std::memory_order_release);
        return result;
    }

    tresult SMTG_STDCALL getState(IBStream* stream) override {
        return write_plugin_state(stream,state_);
    }

    // ── IAudioProcessor ──────────────────────────────────────────────────────
    tresult SMTG_STDCALL setBusArrangements(SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts) override {
        if (numOuts > 0 && outputs[0] != SpeakerArr::kStereo) return kResultFalse;
        if (is_fx_) {
            if (numIns > 0 && inputs[0] != SpeakerArr::kStereo && inputs[0] != SpeakerArr::kMono) {
                return kResultFalse;
            }
        } else {
            if (numIns > 0 && inputs[0] != SpeakerArr::kEmpty) {
                return kResultFalse;
            }
        }
        return kResultOk;
    }

    tresult SMTG_STDCALL getBusArrangement(BusDirection dir, int32 index, SpeakerArrangement& arr) override {
        if (index != 0) return kInvalidArgument;
        if (dir == kOutput) {
            arr = SpeakerArr::kStereo;
            return kResultOk;
        }
        if (dir == kInput && is_fx_) {
            arr = SpeakerArr::kStereo;
            return kResultOk;
        }
        arr = SpeakerArr::kEmpty;
        return kResultOk;
    }

    // Request only the three context fields actually consumed by the same
    // production LFO. Old patches and FX retain manual-BPM fallback when a
    // host supplies neither. This is a Setup Done / UI thread query.
    uint32 SMTG_STDCALL getProcessContextRequirements() override {
        return IProcessContextRequirements::kNeedTempo |
               IProcessContextRequirements::kNeedProjectTimeMusic |
               IProcessContextRequirements::kNeedTransportState;
    }

    tresult SMTG_STDCALL canProcessSampleSize(int32 symbolicSampleSize) override {
        return (symbolicSampleSize == 0) ? kResultOk : kResultFalse; // 32-bit float
    }

    uint32 SMTG_STDCALL getLatencySamples() override {
        return 0; // 0 samples reported latency - Hard Real-Time
    }

    tresult SMTG_STDCALL setupProcessing(ProcessSetup& setup) override {
        const double hz=setup.sampleRate;
        // Clamp to a real audio rate even if a hostile/incomplete host setup
        // would otherwise overflow the reported uint32 tail length.
        const bool finite=(std::bit_cast<uint64_t>(hz)&0x7ff0000000000000ULL)!=0x7ff0000000000000ULL;
        const double sr=(finite&&hz>=8000.0&&hz<=384000.0)?hz:48000.0;
        reported_tail_sample_rate_.store(sr,std::memory_order_relaxed);
        engine_.init(static_cast<float>(sr),static_cast<size_t>(std::max(1,setup.maxSamplesPerBlock)));
        return kResultOk;
    }

    tresult SMTG_STDCALL setProcessing(bool state) override {
        processing_ = state;
        return kResultOk;
    }

    tresult SMTG_STDCALL process(ProcessData& data) override {
        if(data.numSamples<0)return kInvalidArgument;
        // A host snapshot is a complete edited patch, not 178 independent
        // updates applied in parameter-ID order. Establish the base factory
        // sound first; then apply *all* saved overrides without synchronizing
        // factory defaults over the serialized user edits.
        if(!state_->restore_in_progress.load(std::memory_order_acquire)) {
            if(state_->restored_snapshot_pending.exchange(false,
                                                           std::memory_order_acq_rel)){
                std::array<float,kNumParams> saved{};
                for(int i=0;i<kNumParams;++i)
                    saved[i]=state_->params[i].load(std::memory_order_relaxed);
                monkeys_ear::vst3::replay_patch_snapshot(saved,
                    [this](size_t id,float value){
                        engine_.set_parameter_normalized(static_cast<int>(id),value);
                    });
                for(int i=0;i<kNumParams;++i)
                    state_->dirty[i].store(false,std::memory_order_relaxed);
            }else{
                for(int i=0;i<kNumParams;++i)
                    if(state_->dirty[i].exchange(false))
                        apply_param_to_engine(static_cast<ParamID>(i),
                                              state_->params[i].load());
            }
        }

        // Resolve the clock after applying a newly selected patch, so its
        // first audio block aligns immediately to the host beat.
        const auto* context=static_cast<const ProcessContext*>(data.processContext);
        const bool tempo_valid=context&&(context->state & ProcessContext::kTempoValid);
        const bool beat_valid=context&&(context->state & ProcessContext::kProjectTimeMusicValid);
        const bool playing=context&&(context->state & ProcessContext::kPlaying);
        engine_.set_host_transport(context?context->tempo:0.0,tempo_valid,
                                    context?context->projectTimeMusic:0.0,beat_valid,playing);


        // Hosts can flush parameters with numSamples=0 and no audio buffers.
        if(data.numSamples==0){
            if(data.inputParameterChanges){
                const int32 count=std::max(0,data.inputParameterChanges->getParameterCount());
                for(int32 q=0;q<count;++q){
                    auto* queue=data.inputParameterChanges->getParameterData(q);
                    if(!queue)continue;
                    const ParamID id=queue->getParameterId();
                    if(id>=kNumParams)continue;
                    const int32 points=std::max(0,queue->getPointCount());
                    for(int32 i=0;i<points;++i){
                        int32 ignored=0;ParamValue value=0;
                        if(queue->getPoint(i,ignored,value)!=kResultOk||!valid_normalized(value))continue;
                        const float normalized=static_cast<float>(std::clamp(value,0.0,1.0));
                        apply_param_to_engine(id,normalized);
                        state_->params[id].store(normalized);
                        state_->dirty[id].store(false);
                    }
                }
            }
            return kResultOk;
        }

        if(data.numOutputs<1||!data.outputs||data.outputs[0].numChannels<2
           ||!data.outputs[0].channelBuffers32
           ||!data.outputs[0].channelBuffers32[0]
           ||!data.outputs[0].channelBuffers32[1])return kResultFalse;
        const float* in_l=nullptr;
        const float* in_r=nullptr;
        if(data.numInputs>0&&data.inputs&&data.inputs[0].numChannels>0
           &&data.inputs[0].channelBuffers32){
            in_l=data.inputs[0].channelBuffers32[0];
            in_r=data.inputs[0].numChannels>1?data.inputs[0].channelBuffers32[1]:in_l;
        }
        float* const out_l=data.outputs[0].channelBuffers32[0];
        float* const out_r=data.outputs[0].channelBuffers32[1];
        // A synth/FX with voices, delay, FDN and state memory cannot infer
        // silence merely from the *input* bus or absence of new notes.
        // Conservatively clear both output-channel silent bits for every
        // rendered block; don't advertise silence over audible release tails.
        data.outputs[0].silenceFlags=0;

        // A bounded buffer rather than heap allocations on the audio thread.
        // Sort by sampleOffset and original order; do not assume the host
        // delivers an already-sorted event list.
        int32 midi_count=0;
        if(data.inputEvents){
            const int32 count=std::max(0,data.inputEvents->getEventCount());
            if(count>static_cast<int32>(scheduled_midi_.size()))return kResultFalse;
            for(int32 i=0;i<count;++i){
                Event event{};
                if(data.inputEvents->getEvent(i,event)!=kResultOk)continue;
                if(event.busIndex!=0||event.sampleOffset<0||
                   event.sampleOffset>=data.numSamples)continue;
                if(event.type!=kNoteOnEvent&&event.type!=kNoteOffEvent&&
                   event.type!=kPolyPressureEvent)continue;
                if(event.type==kNoteOnEvent &&
                   (event.noteOn.pitch<0||event.noteOn.pitch>127||
                    event.noteOn.channel<0||event.noteOn.channel>15||
                    !valid_normalized(event.noteOn.velocity)))continue;
                if(event.type==kNoteOffEvent &&
                   (event.noteOff.pitch<0||event.noteOff.pitch>127||
                    event.noteOff.channel<0||event.noteOff.channel>15))continue;
                if(event.type==kPolyPressureEvent &&
                   (event.polyPressure.pitch<0||event.polyPressure.pitch>127||
                    event.polyPressure.channel<0||event.polyPressure.channel>15||
                    !valid_normalized(event.polyPressure.pressure)))continue;
                scheduled_midi_[midi_count++]={event,i};
            }
        }
        std::sort(scheduled_midi_.begin(),scheduled_midi_.begin()+midi_count,
            [](const ScheduledMidi& a,const ScheduledMidi& b){
                return a.event.sampleOffset!=b.event.sampleOffset
                    ? a.event.sampleOffset<b.event.sampleOffset:a.order<b.order;
            });

        std::array<ParameterCursor,kNumParams> parameter_events{};
        std::array<int32,kNumParams> active_ids{};
        int32 active_count=0;
        if(data.inputParameterChanges){
            const int32 count=std::max(0,data.inputParameterChanges->getParameterCount());
            for(int32 q=0;q<count;++q){
                auto* queue=data.inputParameterChanges->getParameterData(q);
                if(!queue)continue;
                const ParamID id=queue->getParameterId();
                if(id>=kNumParams)continue;
                auto& cursor=parameter_events[id];
                if(!cursor.queue)active_ids[active_count++]=static_cast<int32>(id);
                cursor.queue=queue;
                cursor.next=0;
                cursor.count=std::max(0,queue->getPointCount());
                cursor.pending=false;
            }
        }
        const auto advance=[&](ParameterCursor& c,int32 from){
            c.pending=false;
            while(c.queue&&c.next<c.count){
                int32 offset=0;ParamValue value=0;
                const tresult result=c.queue->getPoint(c.next++,offset,value);
                if(result!=kResultOk||offset<from||offset>=data.numSamples
                   ||!valid_normalized(value))continue;
                c.offset=offset;
                c.value=static_cast<float>(std::clamp(value,0.0,1.0));
                c.pending=true;return;
            }
        };
        for(int32 a=0;a<active_count;++a)
            advance(parameter_events[active_ids[a]],0);

        int32 midi_cursor=0;
        for(int32 position=0;position<data.numSamples;){
            // Parameter changes at this sample precede same-sample notes.
            for(int32 a=0;a<active_count;++a){
                const int32 id=active_ids[a];auto& cursor=parameter_events[id];
                while(cursor.pending&&cursor.offset==position){
                    apply_param_to_engine(static_cast<ParamID>(id),cursor.value);
                    state_->params[id].store(cursor.value);
                    state_->dirty[id].store(false);
                    advance(cursor,position);
                }
            }
            while(midi_cursor<midi_count&&
                  scheduled_midi_[midi_cursor].event.sampleOffset==position){
                const Event& event=scheduled_midi_[midi_cursor++].event;
                if(event.type==kNoteOnEvent){
                    if(event.noteOn.velocity>0.0f)
                        engine_.handle_midi_note_on(event.noteOn.pitch,
                            std::clamp(event.noteOn.velocity,0.f,1.f),
                            event.noteOn.channel,event.noteOn.noteId);
                    else engine_.handle_midi_note_off(event.noteOn.pitch,
                            event.noteOn.channel,event.noteOn.noteId);
                }else if(event.type==kNoteOffEvent){
                    engine_.handle_midi_note_off(event.noteOff.pitch,
                        event.noteOff.channel,event.noteOff.noteId);
                }else if(event.type==kPolyPressureEvent){
                    engine_.handle_midi_poly_pressure(event.polyPressure.pitch,
                        event.polyPressure.channel,event.polyPressure.noteId,
                        std::clamp(event.polyPressure.pressure,0.f,1.f));
                }
            }

            int32 next=data.numSamples;
            if(midi_cursor<midi_count)
                next=std::min(next,scheduled_midi_[midi_cursor].event.sampleOffset);
            for(int32 a=0;a<active_count;++a){
                const auto& cursor=parameter_events[active_ids[a]];
                if(cursor.pending)next=std::min(next,cursor.offset);
            }
            if(next<=position)return kInternalError;
            engine_.process_block(in_l?in_l+position:nullptr,
                                  in_r?in_r+position:nullptr,
                                  out_l+position,out_r+position,
                                  static_cast<size_t>(next-position));
            position=next;
        }
        return kResultOk;
    }

    uint32 SMTG_STDCALL getTailSamples() override {
        // This includes a conservative tail for the longest possible macro
        // delay (~0.7 s with 75% feedback: ~17 s to -60 dB) followed by the
        // existing FDN reverb (up to 6.5 s T60), plus diffusion/predelay.
        // Unlike patch-dependent reporting this stays valid as performers
        // switch sounds, without unreliable host restart notifications.
        constexpr double worst_case_tail_seconds=26.0;
        const double samples=std::ceil(worst_case_tail_seconds*
            reported_tail_sample_rate_.load(std::memory_order_relaxed));
        return static_cast<uint32>(std::clamp(samples,0.0,10000000.0));
    }

    // ── IConnectionPoint ─────────────────────────────────────────────────────
    tresult SMTG_STDCALL connect(IConnectionPoint* /*other*/) override {
        return kResultOk;
    }
    tresult SMTG_STDCALL disconnect(IConnectionPoint* /*other*/) override {
        return kResultOk;
    }
    tresult SMTG_STDCALL notify(IMessage* /*message*/) override {
        return kResultOk;
    }

private:
    void sync_patch_controls_for_state() {
        // Same factory parameter projection as the editor controller. The host
        // can save the new patch, and the UI never displays stale macros.
        monkeys_ear::ui::emit_preset_params(engine_.get_current_preset(),
            [this](int id,float value) {
                state_->params[id].store(value);
                state_->dirty[id].store(false);
            });
    }
    void apply_param_to_engine(ParamID id, float value) {
        engine_.set_parameter_normalized(static_cast<int>(id), value);
        if(id==79||id==173)sync_patch_controls_for_state();
    }

    std::atomic<uint32> ref_count_;
    bool is_fx_;
    bool active_;
    bool processing_;
    std::atomic<double> reported_tail_sample_rate_{48000.0};
    std::shared_ptr<SharedPluginState> state_;
    monkeys_ear::MonkeysEarEngine engine_;
    std::array<ScheduledMidi,512> scheduled_midi_{};
};

// ── VST3 Controller (UI Parameters & MIDI Mapping) ─────────────────────────
class MonkeysEarController : public IEditController,
                             public IMidiMapping,
                             public IConnectionPoint,
                             public IUnitInfo {
public:
    MonkeysEarController()
        : ref_count_(1), handler_(nullptr), state_(std::make_shared<SharedPluginState>()) {}


    virtual ~MonkeysEarController() = default;

    // Steinberg IUnitInfo supplies a host-native hierarchy for all controls,
    // including on hosts with no proprietary Monkey's Ear editor.
    int32 SMTG_STDCALL getUnitCount() override { return kUnitCount; }
    tresult SMTG_STDCALL getUnitInfo(int32 index, UnitInfo& info) override {
        if(index<0||index>=kUnitCount)return kInvalidArgument;
        info.id=index;
        info.parentUnitId=index==0?-1:((index>=10&&index<=13)?9:0);
        info.programListId=-1;
        copy_to_char16(info.name,kUnitNames[index],128);
        return kResultOk;
    }
    int32 SMTG_STDCALL getProgramListCount() override { return 0; }
    tresult SMTG_STDCALL getProgramListInfo(int32,ProgramListInfo&) override { return kResultFalse; }
    tresult SMTG_STDCALL getProgramName(ProgramListID,int32,char16[128]) override { return kResultFalse; }
    tresult SMTG_STDCALL getProgramInfo(ProgramListID,int32,const char8*,char16[128]) override { return kResultFalse; }
    tresult SMTG_STDCALL hasProgramPitchNames(ProgramListID,int32) override { return kResultFalse; }
    tresult SMTG_STDCALL getProgramPitchName(ProgramListID,int32,int16_t,char16[128]) override { return kResultFalse; }
    UnitID SMTG_STDCALL getSelectedUnit() override { return selected_unit_; }
    tresult SMTG_STDCALL selectUnit(UnitID unit) override {
        if(unit<0||unit>=kUnitCount)return kInvalidArgument;
        selected_unit_=unit;return kResultOk;
    }
    tresult SMTG_STDCALL getUnitByBus(MediaType type,BusDirection dir,
                                      int32 bus,int32 channel,UnitID& unit) override {
        if(bus!=0||channel<0||type!=kEvent||dir!=kInput)return kResultFalse;
        unit=0;return kResultOk;
    }
    tresult SMTG_STDCALL setUnitProgramData(int32,int32,IBStream*) override {
        return kNotImplemented; // no hidden native program list
    }

    // ── FUnknown ─────────────────────────────────────────────────────────────
    tresult SMTG_STDCALL queryInterface(const TUID _iid, void** obj) override {
        if (!obj) return kInvalidArgument;

        if (memcmp(_iid, FUnknown_iid, sizeof(TUID)) == 0 ||
            memcmp(_iid, IPluginBase_iid, sizeof(TUID)) == 0 ||
            memcmp(_iid, IEditController_iid, sizeof(TUID)) == 0) {
            *obj = static_cast<IEditController*>(this);
            addRef();
            return kResultOk;
        }
        if (memcmp(_iid, IMidiMapping_iid, sizeof(TUID)) == 0) {
            *obj = static_cast<IMidiMapping*>(this);
            addRef();
            return kResultOk;
        }
        if (memcmp(_iid, IConnectionPoint_iid, sizeof(TUID)) == 0) {
            *obj = static_cast<IConnectionPoint*>(this);
            addRef();
            return kResultOk;
        }

        if (memcmp(_iid, IUnitInfo_iid, sizeof(TUID)) == 0) {
            *obj = static_cast<IUnitInfo*>(this);
            addRef();
            return kResultOk;
        }
        *obj = nullptr;
        return kResultFalse;
    }

    uint32 SMTG_STDCALL addRef() override { return ++ref_count_; }
    uint32 SMTG_STDCALL release() override {
        uint32 count = --ref_count_;
        if (count == 0) delete this;
        return count;
    }

    // ── IPluginBase ──────────────────────────────────────────────────────────
    tresult SMTG_STDCALL initialize(FUnknown* /*context*/) override {
        return kResultOk;
    }

    tresult SMTG_STDCALL terminate() override {
        return kResultOk;
    }

    // ── IEditController ──────────────────────────────────────────────────────
    tresult SMTG_STDCALL setComponentState(IBStream* stream) override {
        return read_plugin_state(stream,state_);
    }

    tresult SMTG_STDCALL setState(IBStream* stream) override {
        return read_plugin_state(stream,state_);
    }

    tresult SMTG_STDCALL getState(IBStream* stream) override {
        return write_plugin_state(stream,state_);
    }

    int32 SMTG_STDCALL getParameterCount() override {
        return kNumParams;
    }

    tresult SMTG_STDCALL getParameterInfo(int32 paramIndex, ParameterInfo& info) override {
        if (paramIndex < 0 || paramIndex >= kNumParams) return kInvalidArgument;

        info.id = static_cast<ParamID>(paramIndex);
        info.stepCount = (paramIndex==79)?3
            : paramIndex==173?7
            : paramIndex==172||paramIndex==174?1
            : (paramIndex==82||paramIndex==113||paramIndex==124||paramIndex==125||paramIndex==128||paramIndex==169)?1
            : (paramIndex>=129&&paramIndex<=160&&((paramIndex-129)%8)==0)?1
            : (paramIndex>=129&&paramIndex<=160&&((paramIndex-129)%8)==1)?monkeys_ear::kTubeFlavorCount-1
            : paramIndex==171?11
            : ((paramIndex>=101&&paramIndex<=112&&((paramIndex-101)%3)<2)?7:0);
        info.unitId = unit_for_parameter(paramIndex);
        // Pitch bend and pressure are hidden from ordinary editing, but must
        // be automatable targets for the VST3 MIDI mapping bridge.
        info.flags=(kCanAutomate |
                    ((paramIndex==80||paramIndex==81)?kIsHidden:0) |
                    (info.stepCount>1?kIsList:0));
        // New controls have stable factory defaults, not a moving snapshot
        // of the last control value. Existing ID metadata is left compatible.
        static const SharedPluginState defaults(false);
        info.defaultNormalizedValue = defaults.params[paramIndex].load();

        static const char* titles[kNumParams] = {
            "Macro 1: Brightness (Cutoff)",
            "Macro 2: Bite (Resonance)",
            "Macro 3: Heat (Tube Drive)",
            "Macro 4: Body (Cab Resonator)",
            "Macro 5: Echo (Stereo Delay)",
            "Macro 6: Space (FDN Reverb)",
            "Macro 7: Mic Blend (Live Input)",
            "Macro 8: Character (Cathode Sag)",
            "Master Gain",
            "Synth Waveform",
            "Osc: Cross-FM Depth",
            "Osc 2: Tuning (Semitones)",
            "Osc: Hard Sync Mode",
            "State: Resistance (rho)",
            "State: Repulsion (Neg-Gravity)",
            "State: Phase Resonance Coupling",
            "State: Macro Persistence",
            "State: Mechanism Mode (A/B)",
            "Audio Input Route Mode",
            "SUB: Fundamental Level", "SUB: Subharmonic Blend", "SUB: Ratio (1/n)", "SUB: Phase", "SUB: Polarity", "SUB: Saturation", "SUB: Envelope Follow",
            "SOURCE: Weight Level", "SOURCE: Character Level",
            "FILTER: Pass A Mode", "FILTER: Pass B Cutoff", "FILTER: Pass B Mode", "FILTER: Routing", "FILTER: Wet Dry", "FILTER: Key Tracking", "FILTER: Pass A Slope", "FILTER: Pass B Slope", "FILTER: Internal Drive",
            "EQ 1: Type", "EQ 1: Frequency", "EQ 1: Gain", "EQ 1: Q", "EQ 2: Type", "EQ 2: Frequency", "EQ 2: Gain", "EQ 2: Q",
            "EQ 3: Type", "EQ 3: Frequency", "EQ 3: Gain", "EQ 3: Q", "EQ 4: Type", "EQ 4: Frequency", "EQ 4: Gain", "EQ 4: Q", "EQ: Bypass", "EQ: Gain Compensation",
            "MOTION: LFO Rate", "MOTION: LFO Waveform", "MOTION: Curve", "MOTION: LFO to Cutoff", "MOTION: LFO to Resonance", "MOTION: LFO to FM", "MOTION: LFO to Sub", "MOTION: LFO to Drive", "MOTION: LFO to EQ Frequency", "MOTION: LFO to EQ Gain",
            "STATE: Direction to Filter", "STATE: Fast Energy to FM", "STATE: Slow Energy to Balance", "STATE: Slow Energy to Resonator", "MOTION: Audio Envelope to Drive",
            "SOURCE: Mono Mode", "SOURCE: Legato", "SOURCE: Portamento", "SOURCE: Pitch Bend Range", "SOURCE: Vibrato Depth", "SOURCE: Velocity Tone", "STATE: Aftertouch to Filter", "DRIVE/BODY: Aftertouch to Drive", "MOTION: LFO Master Depth", "PRESET: Target Patch", "PERFORMANCE: Pitch Bend", "PERFORMANCE: Aftertouch",
            "SPACE: Enable", "SPACE: Pitch Bound", "SPACE: Harmonic Bound", "SPACE: Modal Bound", "SPACE: Movement Rate", "SPACE: Attraction", "SPACE: Resistance", "SPACE: Repulsion", "SPACE: Frequency Freedom", "SPACE: Energy Widen", "SPACE: Attack Freedom", "SPACE: Release Relaxation", "SPACE: Pitch Layer Mix", "SPACE: Harmonic Layer Mix", "SPACE: Modal Layer Mix", "SPACE: Spectral Yield Depth", "SPACE: Spectral Priority", "SPACE: Phase Offset", "SPACE: Phase Coupling",
            "SPACE ROUTE 1: Source", "SPACE ROUTE 1: Destination", "SPACE ROUTE 1: Depth", "SPACE ROUTE 2: Source", "SPACE ROUTE 2: Destination", "SPACE ROUTE 2: Depth", "SPACE ROUTE 3: Source", "SPACE ROUTE 3: Destination", "SPACE ROUTE 3: Depth", "SPACE ROUTE 4: Source", "SPACE ROUTE 4: Destination", "SPACE ROUTE 4: Depth",
            "VOCAL: Enable", "VOCAL: Correction Strength", "VOCAL: Drift Retention", "VOCAL: Vibrato Retention", "VOCAL: Transition", "VOCAL: Envelope Repair", "VOCAL: Spectral Residual", "VOCAL: Character", "VOCAL: Mix", "VOCAL: Soft Sequential Staging", "VOCAL: Aperiodic Protection"
        };

        char title[96]{};
        if(paramIndex>=124 && paramIndex<=128){
            constexpr const char* kGlobal[]={"RACK: Enable","RACK: Parallel Pairs","RACK: Shared Supply","RACK: Output","RACK: Anti-alias"};
            std::snprintf(title,sizeof(title),"%s",kGlobal[paramIndex-124]);
        } else if(paramIndex>=129 && paramIndex<=160){
            constexpr const char* kFields[]={"Enable","Tube Type","Drive","Bias","Plate Load","Cathode Sag","Coupling","Mix"};
            const int slot=(paramIndex-129)/8,field=(paramIndex-129)%8;
            std::snprintf(title,sizeof(title),"TUBE %d: %s",slot+1,kFields[field]);
        } else if(paramIndex>=161 && paramIndex<=168){
            constexpr const char* kEnvelopes[]={"AMP: Attack","AMP: Decay","AMP: Sustain","AMP: Release","FILTER ENV: Attack","FILTER ENV: Decay","FILTER ENV: Sustain","FILTER ENV: Release"};
            std::snprintf(title,sizeof(title),"%s",kEnvelopes[paramIndex-161]);
        } else if(paramIndex>=169&&paramIndex<=171){
            constexpr const char* kTiming[]={"LFO: Tempo Sync","LFO: Tempo BPM","LFO: Subdivision"};
            std::snprintf(title,sizeof(title),"%s",kTiming[paramIndex-169]);
        } else if(paramIndex==172){
            std::snprintf(title,sizeof(title),"PLAY: Sustain Pedal (CC64)");
        } else if(paramIndex==173){
            std::snprintf(title,sizeof(title),"PLAY: Sound Palette");
        } else if(paramIndex==174){
            std::snprintf(title,sizeof(title),"RHYTHM: Follow Host Clock");
        } else if(paramIndex==175){
            std::snprintf(title,sizeof(title),"RHYTHM: Groove Swing");
        } else if(paramIndex==176){
            std::snprintf(title,sizeof(title),"PERFORM: Drift");
        } else if(paramIndex==177){
            std::snprintf(title,sizeof(title),"PERFORM: Fracture");
        } else {
            std::snprintf(title,sizeof(title),"%s",titles[paramIndex]);
        }
        copy_to_char16(info.title,title,128);
        copy_to_char16(info.shortTitle,title,128);
        copy_to_char16(info.units,(paramIndex==170?"BPM":(paramIndex>=161&&paramIndex<=168&&paramIndex!=163&&paramIndex!=167)?"s":paramIndex>=124?"":"%"),128);

        return kResultOk;
    }

    tresult SMTG_STDCALL getParamStringByValue(ParamID id, ParamValue valueNormalized, char16 string[128]) override {
        if(id>=kNumParams||!string||!valid_normalized(valueNormalized))return kInvalidArgument;
        valueNormalized=std::clamp(valueNormalized,0.0,1.0);
        char buf[64];
        if(id>=129&&id<=160&&((id-129)%8)==1){
            int type=std::clamp(static_cast<int>(valueNormalized*monkeys_ear::kTubeFlavorCount),0,monkeys_ear::kTubeFlavorCount-1);
            snprintf(buf,sizeof(buf),"%s",monkeys_ear::kTubeFlavorNames[type]);
        }else if(id==124||id==128||id==169||id==172||id==174){
            snprintf(buf,sizeof(buf),"%s",valueNormalized>=.5?"On":"Off");
        }else if(id==125){
            snprintf(buf,sizeof(buf),"%s",valueNormalized>=.5?"Two parallel pairs":"Four in series");
        }else if(id==171){
            static const char* divs[]={"4 bars","2 bars","1 bar","1/2","1/4","1/8","1/16","1/32","1/4 triplet","1/8 triplet","1/8 dotted","1/16 dotted"};
            snprintf(buf,sizeof(buf),"%s",divs[std::clamp(static_cast<int>(valueNormalized*12.0),0,11)]);
        }else if(id==170){
            snprintf(buf,sizeof(buf),"%.1f BPM",40.0+200.0*valueNormalized);
        }else if(id>=161&&id<=168&&(id!=163&&id!=167)){
            const double seconds=.001+6.0*valueNormalized*valueNormalized;
            snprintf(buf,sizeof(buf),seconds<1.0?"%.0f ms":"%.2f s",seconds<1.0?seconds*1000.0:seconds);
        }else if(id==0) {
            float hz = 40.0f * std::pow(450.0f, static_cast<float>(valueNormalized));
            snprintf(buf, sizeof(buf), "%.1f Hz", hz);
        } else if (id == 8) {
            float db = (valueNormalized > 0.001) ? static_cast<float>((valueNormalized - 0.85) * 40.0) : -100.0f;
            snprintf(buf, sizeof(buf), "%.1f dB", db);
        } else if (id == 9) {
            int wf = static_cast<int>(valueNormalized * 3.99);
            const char* names[] = {"Sawtooth", "Pulse", "Triangle", "Sine"};
            snprintf(buf, sizeof(buf), "%s", (wf >= 0 && wf <= 3) ? names[wf] : "Sawtooth");
        } else if (id == 11) {
            int semi = static_cast<int>(std::round((valueNormalized - 0.5) * 48.0));
            snprintf(buf, sizeof(buf), "%+d st", semi);
        } else if (id == 12) {
            snprintf(buf, sizeof(buf), "%s", (valueNormalized >= 0.5) ? "Hard Sync On" : "Free Run");
        } else if (id == 17) {
            snprintf(buf, sizeof(buf), "%s", (valueNormalized >= 0.5) ? "ChronoState ON" : "Conventional Baseline");
        } else if (id == 18) {
            int m = static_cast<int>(valueNormalized * 2.99);
            const char* modes[] = {"Synth Only", "Hybrid Blend", "External Audio"};
            snprintf(buf, sizeof(buf), "%s", (m >= 0 && m <= 2) ? modes[m] : "Synth");
        } else if(id==21){
            const int ratio=std::clamp(static_cast<int>(valueNormalized*3.99),0,3)+1;
            snprintf(buf,sizeof(buf),"1:%d",ratio);
        } else if(id==28||id==30){
            static const char* modes[]={"Lowpass","Bandpass","Highpass","Notch","Peak"};
            snprintf(buf,sizeof(buf),"%s",modes[std::clamp(static_cast<int>(valueNormalized*4.99),0,4)]);
        } else if(id==31){
            static const char* routes[]={"Serial","Parallel","Split"};
            snprintf(buf,sizeof(buf),"%s",routes[std::clamp(static_cast<int>(valueNormalized*2.99),0,2)]);
        } else if(id==56){
            static const char* waves[]={"Sine","Triangle","Saw","Square","Random Hold"};
            snprintf(buf,sizeof(buf),"%s",waves[std::clamp(static_cast<int>(valueNormalized*4.99),0,4)]);
        } else if(id>=37&&id<=52&&(id-37)%4==0){
            static const char* eqTypes[]={"Bypass","Bell","Low Shelf","High Shelf","Highpass","Lowpass"};
            snprintf(buf,sizeof(buf),"%s",eqTypes[std::clamp(static_cast<int>(valueNormalized*5.99),0,5)]);
        } else if(id==23||id==34||id==35||id==53||id==54||
                  id==70||id==71||id==82){
            snprintf(buf,sizeof(buf),"%s",valueNormalized>=.5?"On":"Off");
        } else if (id == 79) {
            const int p=std::clamp(static_cast<int>(valueNormalized*3.99),0,3);
            const char* names[]={"Current","MONOLITH","FERAL WOBBLE","VELVET LEAD"};
            snprintf(buf,sizeof(buf),"%s",names[p]);
        } else if(id==173){
            const int p=std::clamp(static_cast<int>(valueNormalized*8.0),0,7);
            const char* names[]={"Current","MONOLITH","FERAL WOBBLE","VELVET LEAD",
                "TUBE FOUNDRY","CLOCKWORK SUB","GLASS CHAMBER","SOUND SPACE"};
            snprintf(buf,sizeof(buf),"%s",names[p]);
        } else if(id>=83&&id<=85){snprintf(buf,sizeof(buf),"%.1f cents",valueNormalized*100.0);}
        else if(id==86){snprintf(buf,sizeof(buf),"%.2f Hz",.02f*std::pow(1000.0f,static_cast<float>(valueNormalized)));}
        else if(id==97){snprintf(buf,sizeof(buf),"%.1f dB",valueNormalized*18.0);}
        else if(id==99){snprintf(buf,sizeof(buf),"%+.1f deg",(valueNormalized-.5)*180.0);}
        else if(id==113){snprintf(buf,sizeof(buf),"%s",valueNormalized>=.5?"Dual Layer On":"Bypass");}
        else if(id==117){snprintf(buf,sizeof(buf),"%.1f ms",2.0+(1.0-static_cast<float>(valueNormalized))*58.0);}
        else if(id==122){snprintf(buf,sizeof(buf),"%.0f %% second stage",valueNormalized*100.0);}
        else if(id>=101&&id<=112&&((id-101)%3)<2){
            int n=static_cast<int>(valueNormalized*7.99);const char* src[]={"LFO","Envelope","Fast Energy","Slow Energy","Direction","Aftertouch","Velocity","Audio Energy"};const char* dst[]={"Pitch Bound","Harmonic Bound","Modal Bound","Movement Rate","Attraction","Spectral Depth","Phase","Harmonic Mix"};
            snprintf(buf,sizeof(buf),"%s",((id-101)%3)==0?src[std::clamp(n,0,7)]:dst[std::clamp(n,0,7)]);
        } else {
            snprintf(buf, sizeof(buf), "%.1f %%", valueNormalized * 100.0);
        }
        copy_to_char16(string, buf, 128);
        return kResultOk;
    }

    tresult SMTG_STDCALL getParamValueByString(ParamID id, const char16* raw, ParamValue& out) override {
        if(id>=kNumParams||!raw)return kInvalidArgument;
        std::string input;
        for(int i=0;i<127&&raw[i];++i){
            if(raw[i]>127)return kResultFalse;
            input+=static_cast<char>(std::tolower(static_cast<unsigned char>(raw[i])));
        }
        const size_t first=input.find_first_not_of(" \t\n\r");
        if(first==std::string::npos)return kResultFalse;
        input=input.substr(first,input.find_last_not_of(" \t\n\r")-first+1);
        const auto matches=[&](const char* title){
            std::string name(title);
            for(char& c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return name==input;
        };
        if(id==79||id==173){
            constexpr const char* sounds[]={"Current","MONOLITH","FERAL WOBBLE","VELVET LEAD",
                "TUBE FOUNDRY","CLOCKWORK SUB","GLASS CHAMBER","SOUND SPACE"};
            for(int n=0;n<(id==79?4:8);++n)if(matches(sounds[n])){
                out=id==79?std::min(1.0,(n+.5)/3.99):(n+.5)/8.0;return kResultOk;
            }
        }
        if(id>=129&&id<=160&&(id-129)%8==1){
            for(int n=0;n<monkeys_ear::kTubeFlavorCount;++n)
                if(matches(monkeys_ear::kTubeFlavorNames[n])){
                    out=(n+.5)/monkeys_ear::kTubeFlavorCount;return kResultOk;
                }
        }
        if(id==171){
            constexpr const char* divisions[]={"4 bars","2 bars","1 bar","1/2",
                "1/4","1/8","1/16","1/32","1/4 triplet",
                "1/8 triplet","1/8 dotted","1/16 dotted"};
            for(int n=0;n<12;++n)if(matches(divisions[n])){out=(n+.5)/12.0;return kResultOk;}
        }
        if(id==124||id==125||id==128||id==169||id==172||id==174||
           (id>=129&&id<=160&&(id-129)%8==0)){
            if(matches("on")||matches("true")||matches("two parallel pairs")){out=1;return kResultOk;}
            if(matches("off")||matches("false")||matches("four in series")){out=0;return kResultOk;}
        }
        char* end=nullptr;
        const double number=std::strtod(input.c_str(),&end);
        if(end==input.c_str()||!valid_normalized(number))return kResultFalse;
        if(id==170)out=std::clamp((number-40.0)/200.0,0.0,1.0);
        else if(id>=161&&id<=168&&id!=163&&id!=167){
            const double seconds=input.find("ms")!=std::string::npos?number*.001:number;
            out=std::sqrt(std::clamp((seconds-.001)/6.0,0.0,1.0));
        }else if(id>=129&&id<=160&&(id-129)%8==1)
            out=(std::clamp(static_cast<int>(number),0,monkeys_ear::kTubeFlavorCount-1)+.5)/monkeys_ear::kTubeFlavorCount;
        else if(id==171)out=(std::clamp(static_cast<int>(number),0,11)+.5)/12.0;
        else if(id==173)out=(std::clamp(static_cast<int>(number),0,7)+.5)/8.0;
        else if(id==79)out=std::min(1.0,(std::clamp(static_cast<int>(number),0,3)+.5)/3.99);
        else if(id==0)out=number<=0?0:std::clamp(std::log(number/40.0)/std::log(450.0),0.0,1.0);
        else if(id==8)out=number<=-100?0:std::clamp(number/40.0+.85,0.0,1.0);
        else if(id==11)out=std::clamp(number/48.0+.5,0.0,1.0);
        else out=std::clamp(input.find('%')!=std::string::npos?number/100.0:number,0.0,1.0);
        return kResultOk;
    }

    ParamValue SMTG_STDCALL normalizedParamToPlain(ParamID /*id*/, ParamValue valueNormalized) override {
        return valueNormalized;
    }

    ParamValue SMTG_STDCALL plainParamToNormalized(ParamID /*id*/, ParamValue plainValue) override {
        return plainValue;
    }

    ParamValue SMTG_STDCALL getParamNormalized(ParamID id) override {
        if (id < kNumParams && state_) {
            return state_->params[id].load();
        }
        return 0.0;
    }

    tresult SMTG_STDCALL setParamNormalized(ParamID id, ParamValue value) override {
        if(id>=kNumParams||!valid_normalized(value)||!state_)return kInvalidArgument;
        const float normalized=static_cast<float>(std::clamp(value,0.0,1.0));
        state_->params[id].store(normalized);
        state_->dirty[id].store(true);
        if(id==79||id==173){
            monkeys_ear::PresetData selected;
            if(monkeys_ear::ui::factory_patch_for_selector(id,normalized,selected)){
                monkeys_ear::ui::emit_preset_params(selected,[this](int param,float v){
                    state_->params[param].store(v);
                    state_->dirty[param].store(false);
                });
            }
        }
        return kResultOk;
    }

    tresult SMTG_STDCALL setComponentHandler(IComponentHandler* handler) override {
        handler_ = handler;
        return kResultOk;
    }

    IPlugView* SMTG_STDCALL createView(const char8* name) override {
        if(!name||std::strcmp(name,"editor")!=0)return nullptr;
        return monkeys_ear::ui::create_editor(this,&handler_);
    }

    // ── IMidiMapping ────────────────────────────────────────────────────────
    tresult SMTG_STDCALL getMidiControllerAssignment(int32 busIndex, int16_t /*channel*/, int16_t midiControllerNumber, ParamID& id) override {
        if (busIndex != 0) return kResultFalse;

        if (midiControllerNumber == 1 || midiControllerNumber == 74) {
            id = 0; // CC 1 (Mod) or CC 74 -> Filter Cutoff
            return kResultOk;
        } else if (midiControllerNumber == 64) {
            id = 172; // Damper / sustain pedal: PLAY control
            return kResultOk;
        } else if (midiControllerNumber == 71) {
            id = 1; // CC 71 -> Filter Resonance
            return kResultOk;
        } else if (midiControllerNumber == 91) {
            id = 5; // CC 91 -> Reverb Space
            return kResultOk;
        } else if (midiControllerNumber == 93) {
            id = 4; // CC 93 -> Delay Echo
            return kResultOk;
        } else if (midiControllerNumber == 20) {
            id = 176; // Optional assignable hardware knob: DRIFT
            return kResultOk;
        } else if (midiControllerNumber == 21) {
            id = 177; // Optional assignable hardware knob: FRACTURE
            return kResultOk;
        } else if (midiControllerNumber == 129) {
            id = 80; // VST3 legacy MIDI pitch bend controller
            return kResultOk;
        } else if (midiControllerNumber == 128) {
            id = 81; // Steinberg VST3 channel pressure / aftertouch controller
            return kResultOk;
        }
        return kResultFalse;
    }

    // ── IConnectionPoint ─────────────────────────────────────────────────────
    tresult SMTG_STDCALL connect(IConnectionPoint* /*other*/) override {
        return kResultOk;
    }
    tresult SMTG_STDCALL disconnect(IConnectionPoint* /*other*/) override {
        return kResultOk;
    }
    tresult SMTG_STDCALL notify(IMessage* /*message*/) override {
        return kResultOk;
    }

private:
    std::atomic<uint32> ref_count_;
    IComponentHandler* handler_;
    std::shared_ptr<SharedPluginState> state_;
    UnitID selected_unit_=0;
};

// ── VST3 Plugin Factory ──────────────────────────────────────────────────
class MonkeysEarFactory : public IPluginFactory2 {
public:
    MonkeysEarFactory() : ref_count_(1) {}
    virtual ~MonkeysEarFactory() = default;

    tresult SMTG_STDCALL queryInterface(const TUID _iid, void** obj) override {
        if (!obj) return kInvalidArgument;
        if (memcmp(_iid, FUnknown_iid, sizeof(TUID)) == 0 ||
            memcmp(_iid, IPluginFactory_iid, sizeof(TUID)) == 0 ||
            memcmp(_iid, IPluginFactory2_iid, sizeof(TUID)) == 0) {
            *obj = static_cast<IPluginFactory2*>(this);
            addRef();
            return kResultOk;
        }
        *obj = nullptr;
        return kResultFalse;
    }

    uint32 SMTG_STDCALL addRef() override { return ++ref_count_; }
    uint32 SMTG_STDCALL release() override {
        uint32 count = --ref_count_;
        if (count == 0) delete this;
        return count;
    }

    tresult SMTG_STDCALL getFactoryInfo(PFactoryInfo* info) override {
        if (!info) return kInvalidArgument;
        strncpy(info->vendor, "Ultramonkeydog Studios", sizeof(info->vendor) - 1);
        strncpy(info->url, "https://ultramonkeydog-studios.vercel.app", sizeof(info->url) - 1);
        strncpy(info->email, "haringcody@gmail.com", sizeof(info->email) - 1);
        info->flags = 0;
        return kResultOk;
    }

    int32 SMTG_STDCALL countClasses() override {
        // Class 0: Instrument Component (VST3i: 0 audio in, 2 audio out, MIDI in)
        // Class 1: Audio FX Processor (VST3: 2 audio in, 2 audio out, MIDI in)
        // Class 2: Edit Controller (172 stable, grouped instrument parameters)
        return 3;
    }

    tresult SMTG_STDCALL getClassInfo(int32 index, PClassInfo* info) override {
        if (!info) return kInvalidArgument;
        if (index == 0) {
            memcpy(info->cid, kMonkeysEarComponentCID, sizeof(TUID));
            info->cardinality = 0x7FFFFFFF;
            strncpy(info->category, "Audio Module Class", sizeof(info->category) - 1);
            strncpy(info->name, "Monkey's Ear", sizeof(info->name) - 1);
            return kResultOk;
        } else if (index == 1) {
            memcpy(info->cid, kMonkeysEarFxComponentCID, sizeof(TUID));
            info->cardinality = 0x7FFFFFFF;
            strncpy(info->category, "Audio Module Class", sizeof(info->category) - 1);
            strncpy(info->name, "Monkey's Ear FX", sizeof(info->name) - 1);
            return kResultOk;
        } else if (index == 2) {
            memcpy(info->cid, kMonkeysEarControllerCID, sizeof(TUID));
            info->cardinality = 0x7FFFFFFF;
            strncpy(info->category, "Component Controller Class", sizeof(info->category) - 1);
            strncpy(info->name, "Monkey's Ear Controller", sizeof(info->name) - 1);
            return kResultOk;
        }
        return kInvalidArgument;
    }

    tresult SMTG_STDCALL getClassInfo2(int32 index, PClassInfo2* info) override {
        if (!info) return kInvalidArgument;
        if (index == 0) {
            memcpy(info->cid, kMonkeysEarComponentCID, sizeof(TUID));
            info->cardinality = 0x7FFFFFFF;
            strncpy(info->category, "Audio Module Class", sizeof(info->category) - 1);
            strncpy(info->name, "Monkey's Ear", sizeof(info->name) - 1);
            info->classFlags = 0;
            strncpy(info->subCategories, "Instrument|Synth", sizeof(info->subCategories) - 1);
            strncpy(info->vendor, "Ultramonkeydog Studios", sizeof(info->vendor) - 1);
            strncpy(info->version, "1.1.0", sizeof(info->version) - 1);
            strncpy(info->sdkVersion, "VST 3.7.0", sizeof(info->sdkVersion) - 1);
            return kResultOk;
        } else if (index == 1) {
            memcpy(info->cid, kMonkeysEarFxComponentCID, sizeof(TUID));
            info->cardinality = 0x7FFFFFFF;
            strncpy(info->category, "Audio Module Class", sizeof(info->category) - 1);
            strncpy(info->name, "Monkey's Ear FX", sizeof(info->name) - 1);
            info->classFlags = 0;
            strncpy(info->subCategories, "Fx|Filter|Synth|Delay", sizeof(info->subCategories) - 1);
            strncpy(info->vendor, "Ultramonkeydog Studios", sizeof(info->vendor) - 1);
            strncpy(info->version, "1.1.0", sizeof(info->version) - 1);
            strncpy(info->sdkVersion, "VST 3.7.0", sizeof(info->sdkVersion) - 1);
            return kResultOk;
        } else if (index == 2) {
            memcpy(info->cid, kMonkeysEarControllerCID, sizeof(TUID));
            info->cardinality = 0x7FFFFFFF;
            strncpy(info->category, "Component Controller Class", sizeof(info->category) - 1);
            strncpy(info->name, "Monkey's Ear Controller", sizeof(info->name) - 1);
            info->classFlags = 0;
            info->subCategories[0] = 0;
            strncpy(info->vendor, "Ultramonkeydog Studios", sizeof(info->vendor) - 1);
            strncpy(info->version, "1.1.0", sizeof(info->version) - 1);
            strncpy(info->sdkVersion, "VST 3.7.0", sizeof(info->sdkVersion) - 1);
            return kResultOk;
        }
        return kInvalidArgument;
    }

    tresult SMTG_STDCALL createInstance(const TUID cid, const TUID _iid, void** obj) override {
        if (memcmp(cid, kMonkeysEarComponentCID, sizeof(TUID)) == 0) {
            auto* comp = new MonkeysEarComponent(false); // Instrument
            tresult res = comp->queryInterface(_iid, obj);
            comp->release();
            return res;
        }
        if (memcmp(cid, kMonkeysEarFxComponentCID, sizeof(TUID)) == 0) {
            auto* comp = new MonkeysEarComponent(true); // Audio FX Processor
            tresult res = comp->queryInterface(_iid, obj);
            comp->release();
            return res;
        }
        if (memcmp(cid, kMonkeysEarControllerCID, sizeof(TUID)) == 0) {
            auto* ctrl = new MonkeysEarController();
            tresult res = ctrl->queryInterface(_iid, obj);
            ctrl->release();
            return res;
        }
        return kInvalidArgument;
    }

private:
    std::atomic<uint32> ref_count_;
};

extern "C" {

SMTG_EXPORT_SYMBOL IPluginFactory* SMTG_STDCALL GetPluginFactory() {
    return new MonkeysEarFactory();
}

#if defined(_WIN32)
SMTG_EXPORT_SYMBOL bool SMTG_STDCALL InitDll() {
    return true;
}

SMTG_EXPORT_SYMBOL bool SMTG_STDCALL ExitDll() {
    return true;
}
#endif

} // extern "C"
