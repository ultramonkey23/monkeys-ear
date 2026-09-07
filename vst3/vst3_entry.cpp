#include "vst3_sdk_minimal.h"
#include "monkeys_ear/engine.h"
#include <atomic>
#include <cstring>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <cmath>

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

static constexpr int kNumParams = 80;
static constexpr uint32 kStateMagic = 0x4D453032u; // ME02
static constexpr uint32 kStateVersion = 2u;

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
        for (int i = 0; i < kNumParams; ++i) dirty[i].store(false);
    }
};

static bool stream_write(IBStream* s, void* data, int32 bytes) { int32 done=0; return s&&s->write(data,bytes,&done)==kResultOk&&done==bytes; }
static bool stream_read(IBStream* s, void* data, int32 bytes) { int32 done=0; return s&&s->read(data,bytes,&done)==kResultOk&&done==bytes; }
static tresult write_plugin_state(IBStream* stream, const std::shared_ptr<SharedPluginState>& state) {
    if(!stream||!state)return kInvalidArgument;
    uint32 magic=kStateMagic,version=kStateVersion,count=kNumParams;
    if(!stream_write(stream,&magic,4)||!stream_write(stream,&version,4)||!stream_write(stream,&count,4))return kInternalError;
    for(int i=0;i<kNumParams;++i){float v=state->params[i].load();if(!stream_write(stream,&v,4))return kInternalError;} return kResultOk;
}
static tresult read_plugin_state(IBStream* stream, const std::shared_ptr<SharedPluginState>& state) {
    if(!stream||!state)return kInvalidArgument;
    uint32 magic=0,version=0,count=0;
    if(!stream_read(stream,&magic,4)||!stream_read(stream,&version,4)||!stream_read(stream,&count,4))return kInternalError;
    if(magic!=kStateMagic||version>kStateVersion||count>static_cast<uint32>(kNumParams))return kResultFalse;
    for(uint32 i=0;i<count;++i){float v=0;if(!stream_read(stream,&v,4))return kInternalError;state->params[i].store(std::clamp(v,0.0f,1.0f));state->dirty[i].store(true);} return kResultOk;
}

static std::mutex g_state_mutex;
static std::shared_ptr<SharedPluginState> g_last_created_state = nullptr;

// ── VST3 Component (Shared DSP Engine for Instrument & FX) ────────────────
class MonkeysEarComponent : public IComponent,
                            public IAudioProcessor,
                            public IConnectionPoint {
public:
    explicit MonkeysEarComponent(bool is_fx = false)
        : ref_count_(1), is_fx_(is_fx), active_(false), processing_(false) {
        state_ = std::make_shared<SharedPluginState>(is_fx_);
        {
            std::lock_guard<std::mutex> lock(g_state_mutex);
            g_last_created_state = state_;
        }
        engine_.init(48000.0f, 512);
        if (is_fx_) {
            engine_.load_preset(monkeys_ear::PresetManager::create_factory_vocal_resonator());
        } else {
            engine_.load_preset(monkeys_ear::PresetManager::create_factory_lead());
        }
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
        return read_plugin_state(stream,state_);
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

    tresult SMTG_STDCALL canProcessSampleSize(int32 symbolicSampleSize) override {
        return (symbolicSampleSize == 0) ? kResultOk : kResultFalse; // 32-bit float
    }

    uint32 SMTG_STDCALL getLatencySamples() override {
        return 0; // 0 samples reported latency - Hard Real-Time
    }

    tresult SMTG_STDCALL setupProcessing(ProcessSetup& setup) override {
        engine_.init(static_cast<float>(setup.sampleRate), static_cast<size_t>(setup.maxSamplesPerBlock));
        return kResultOk;
    }

    tresult SMTG_STDCALL setProcessing(bool state) override {
        processing_ = state;
        return kResultOk;
    }

    tresult SMTG_STDCALL process(ProcessData& data) override {
        if (data.numSamples <= 0) return kResultOk;

        // 1. Process Host Automation Parameter Changes
        if (data.inputParameterChanges) {
            int32 numParams = data.inputParameterChanges->getParameterCount();
            for (int32 i = 0; i < numParams; ++i) {
                auto* queue = data.inputParameterChanges->getParameterData(i);
                if (queue && queue->getPointCount() > 0) {
                    ParamID id = queue->getParameterId();
                    int32 sampleOffset = 0;
                    ParamValue val = 0.0;
                    if (queue->getPoint(queue->getPointCount() - 1, sampleOffset, val) == kResultOk) {
                        apply_param_to_engine(id, static_cast<float>(val));
                        if (id < kNumParams && state_) {
                            state_->params[id].store(static_cast<float>(val));
                            state_->dirty[id].store(false);
                        }
                    }
                }
            }
        }

        // 2. Apply UI / Controller Changes from shared state
        if (state_) {
            for (int i = 0; i < kNumParams; ++i) {
                if (state_->dirty[i].exchange(false)) {
                    apply_param_to_engine(static_cast<ParamID>(i), state_->params[i].load());
                }
            }
        }

        // 3. Process MIDI Note Events
        if (data.inputEvents) {
            int32 event_count = data.inputEvents->getEventCount();
            for (int32 i = 0; i < event_count; ++i) {
                Event e{};
                if (data.inputEvents->getEvent(i, e) == kResultOk) {
                    if (e.type == kNoteOnEvent) {
                        if (e.noteOn.velocity > 0.0f) {
                            engine_.handle_midi_note_on(e.noteOn.pitch, e.noteOn.velocity);
                        } else {
                            engine_.handle_midi_note_off(e.noteOn.pitch);
                        }
                    } else if (e.type == kNoteOffEvent) {
                        engine_.handle_midi_note_off(e.noteOff.pitch);
                    }
                }
            }
        }

        // 4. Resolve Input Audio Buffers (Causal routing of real external/recorded audio)
        const float* in_l = nullptr;
        const float* in_r = nullptr;
        if (data.numInputs > 0 && data.inputs[0].channelBuffers32) {
            if (data.inputs[0].numChannels > 0) in_l = data.inputs[0].channelBuffers32[0];
            if (data.inputs[0].numChannels > 1) in_r = data.inputs[0].channelBuffers32[1];
            else in_r = in_l;
        }

        // 5. Synthesize & Process through the Shared Engine
        if (data.numOutputs > 0 && data.outputs[0].numChannels >= 2) {
            float* out_l = data.outputs[0].channelBuffers32[0];
            float* out_r = data.outputs[0].channelBuffers32[1];

            engine_.process_block(in_l, in_r, out_l, out_r, static_cast<size_t>(data.numSamples));
        }

        return kResultOk;
    }

    uint32 SMTG_STDCALL getTailSamples() override {
        return 48000; // ~1 second reverb tail
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
    void apply_param_to_engine(ParamID id, float value) {
        engine_.set_parameter_normalized(static_cast<int>(id), value);
    }

    std::atomic<uint32> ref_count_;
    bool is_fx_;
    bool active_;
    bool processing_;
    std::shared_ptr<SharedPluginState> state_;
    monkeys_ear::MonkeysEarEngine engine_;
};

// ── VST3 Controller (UI Parameters & MIDI Mapping) ─────────────────────────
class MonkeysEarController : public IEditController,
                             public IMidiMapping,
                             public IConnectionPoint {
public:
    MonkeysEarController()
        : ref_count_(1), handler_(nullptr) {
        {
            std::lock_guard<std::mutex> lock(g_state_mutex);
            if (g_last_created_state) {
                state_ = g_last_created_state;
            } else {
                state_ = std::make_shared<SharedPluginState>();
            }
        }
    }

    virtual ~MonkeysEarController() = default;

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
        info.stepCount = 0; // Continuous
        info.unitId = 0;
        info.flags = kCanAutomate;
        info.defaultNormalizedValue = state_ ? state_->params[paramIndex].load() : 0.5;

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
            "SOURCE: Mono Mode", "SOURCE: Legato", "SOURCE: Portamento", "SOURCE: Pitch Bend Range", "SOURCE: Vibrato Depth", "SOURCE: Velocity Tone", "STATE: Aftertouch to Filter", "DRIVE/BODY: Aftertouch to Drive", "MOTION: LFO Master Depth", "PRESET: Target Patch"
        };

        copy_to_char16(info.title, titles[paramIndex], 128);
        copy_to_char16(info.shortTitle, titles[paramIndex], 128);
        copy_to_char16(info.units, "%", 128);

        return kResultOk;
    }

    tresult SMTG_STDCALL getParamStringByValue(ParamID id, ParamValue valueNormalized, char16 string[128]) override {
        char buf[64];
        if (id == 0) {
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
        } else if (id == 79) {
            int p=static_cast<int>(valueNormalized*3.99); const char* names[]={"Current","MONOLITH","FERAL WOBBLE","VELVET LEAD"};
            snprintf(buf,sizeof(buf),"%s",names[std::clamp(p,0,3)]);
        } else {
            snprintf(buf, sizeof(buf), "%.1f %%", valueNormalized * 100.0);
        }
        copy_to_char16(string, buf, 128);
        return kResultOk;
    }

    tresult SMTG_STDCALL getParamValueByString(ParamID /*id*/, const char16* /*string*/, ParamValue& valueNormalized) override {
        valueNormalized = 0.5;
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
        if (id < kNumParams && state_) {
            state_->params[id].store(static_cast<float>(value));
            state_->dirty[id].store(true);
            return kResultOk;
        }
        return kInvalidArgument;
    }

    tresult SMTG_STDCALL setComponentHandler(IComponentHandler* handler) override {
        handler_ = handler;
        return kResultOk;
    }

    void* SMTG_STDCALL createView(const char8* /*name*/) override {
        return nullptr; // Host creates native parameter controls
    }

    // ── IMidiMapping ────────────────────────────────────────────────────────
    tresult SMTG_STDCALL getMidiControllerAssignment(int32 busIndex, int16_t /*channel*/, int16_t midiControllerNumber, ParamID& id) override {
        if (busIndex != 0) return kResultFalse;

        if (midiControllerNumber == 1 || midiControllerNumber == 74) {
            id = 0; // CC 1 (Mod) or CC 74 -> Filter Cutoff
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
        // Class 2: Edit Controller (All 19 parameters)
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

static MonkeysEarFactory* g_factory = nullptr;

extern "C" {

SMTG_EXPORT_SYMBOL IPluginFactory* SMTG_STDCALL GetPluginFactory() {
    if (!g_factory) {
        g_factory = new MonkeysEarFactory();
    } else {
        g_factory->addRef();
    }
    return g_factory;
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
