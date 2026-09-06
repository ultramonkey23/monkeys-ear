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

// Component CID: 4D6F6E6B-6579-7345-6172-496E73747201 ("MonkeysEarInstr1")
static const TUID kMonkeysEarComponentCID = INLINE_UID(
    0x4D6F6E6B, 0x65797345, 0x6172496E, 0x73747201
);

// Controller CID: 4D6F6E6B-6579-7345-6172-4374726C7202 ("MonkeysEarCtrlr2")
static const TUID kMonkeysEarControllerCID = INLINE_UID(
    0x4D6F6E6B, 0x65797345, 0x61724374, 0x726C7202
);

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
    std::atomic<float> params[10];
    std::atomic<bool> dirty[10];

    SharedPluginState() {
        params[0].store(0.65f); // Cutoff
        params[1].store(0.25f); // Resonance
        params[2].store(0.30f); // Drive
        params[3].store(0.40f); // Body
        params[4].store(0.25f); // Delay
        params[5].store(0.35f); // Space
        params[6].store(0.00f); // Mic Blend
        params[7].store(0.50f); // Character
        params[8].store(0.85f); // Master Gain
        params[9].store(0.00f); // Waveform
        for (int i = 0; i < 10; ++i) dirty[i].store(false);
    }
};

static std::mutex g_state_mutex;
static std::shared_ptr<SharedPluginState> g_last_created_state = nullptr;

// ── VST3 Component (Audio Processor & Synth Engine) ───────────────────────
class MonkeysEarComponent : public IComponent,
                            public IAudioProcessor,
                            public IConnectionPoint {
public:
    MonkeysEarComponent()
        : ref_count_(1), active_(false), processing_(false) {
        state_ = std::make_shared<SharedPluginState>();
        {
            std::lock_guard<std::mutex> lock(g_state_mutex);
            g_last_created_state = state_;
        }
        engine_.init(48000.0f, 512);
        engine_.load_preset(monkeys_ear::PresetManager::create_factory_lead());
    }

    virtual ~MonkeysEarComponent() = default;

    std::shared_ptr<SharedPluginState> get_state() const { return state_; }

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
            // Pure Virtual Instrument: 0 Audio In, 1 Stereo Audio Out
            return (dir == kOutput) ? 1 : 0;
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

    tresult SMTG_STDCALL setState(void* /*state*/) override {
        return kResultOk;
    }

    tresult SMTG_STDCALL getState(void* /*state*/) override {
        return kResultOk;
    }

    // ── IAudioProcessor ──────────────────────────────────────────────────────
    tresult SMTG_STDCALL setBusArrangements(SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts) override {
        if (numOuts > 0 && outputs[0] != SpeakerArr::kStereo) return kResultFalse;
        if (numIns > 0 && inputs[0] != SpeakerArr::kEmpty) return kResultFalse;
        return kResultOk;
    }

    tresult SMTG_STDCALL getBusArrangement(BusDirection dir, int32 index, SpeakerArrangement& arr) override {
        if (index != 0) return kInvalidArgument;
        if (dir == kOutput) {
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
                        if (id < 10 && state_) {
                            state_->params[id].store(static_cast<float>(val));
                            state_->dirty[id].store(false);
                        }
                    }
                }
            }
        }

        // 2. Apply UI / Controller Changes from shared state
        if (state_) {
            for (int i = 0; i < 10; ++i) {
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

        // 4. Synthesize Audio to Stereo Output
        if (data.numOutputs > 0 && data.outputs[0].numChannels >= 2) {
            float* out_l = data.outputs[0].channelBuffers32[0];
            float* out_r = data.outputs[0].channelBuffers32[1];

            engine_.process_block(nullptr, nullptr, out_l, out_r, static_cast<size_t>(data.numSamples));
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
        if (id < monkeys_ear::NUM_MACROS) {
            engine_.set_macro(static_cast<monkeys_ear::MacroId>(id), value);
        } else if (id == 8) {
            float db = (value > 0.001f) ? (value - 0.85f) * 40.0f : -100.0f;
            engine_.set_master_gain_db(db);
        } else if (id == 9) {
            auto wf = static_cast<monkeys_ear::Waveform>(static_cast<int>(value * 3.99f));
            engine_.set_waveform(wf);
        }
    }

    std::atomic<uint32> ref_count_;
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
    tresult SMTG_STDCALL setComponentState(void* /*state*/) override {
        return kResultOk;
    }

    tresult SMTG_STDCALL setState(void* /*state*/) override {
        return kResultOk;
    }

    tresult SMTG_STDCALL getState(void* /*state*/) override {
        return kResultOk;
    }

    int32 SMTG_STDCALL getParameterCount() override {
        return 10; // 8 Macros + Master Gain + Waveform
    }

    tresult SMTG_STDCALL getParameterInfo(int32 paramIndex, ParameterInfo& info) override {
        if (paramIndex < 0 || paramIndex >= 10) return kInvalidArgument;

        info.id = static_cast<ParamID>(paramIndex);
        info.stepCount = 0; // Continuous
        info.unitId = 0;
        info.flags = kCanAutomate;
        info.defaultNormalizedValue = state_ ? state_->params[paramIndex].load() : 0.5;

        static const char* titles[10] = {
            "Macro 1: Brightness (Cutoff)",
            "Macro 2: Bite (Resonance)",
            "Macro 3: Heat (Tube Drive)",
            "Macro 4: Body (Cab Resonator)",
            "Macro 5: Echo (Stereo Delay)",
            "Macro 6: Space (FDN Reverb)",
            "Macro 7: Mic Blend (Live Input)",
            "Macro 8: Character (Cathode Sag)",
            "Master Gain",
            "Synth Waveform"
        };
        static const char* shortTitles[10] = {
            "Bright", "Bite", "Heat", "Body", "Echo", "Space", "MicMix", "Char", "Gain", "Wave"
        };
        static const char* units[10] = {
            "Hz", "%", "%", "%", "%", "%", "%", "%", "dB", "type"
        };

        copy_to_char16(info.title, titles[paramIndex], 128);
        copy_to_char16(info.shortTitle, shortTitles[paramIndex], 128);
        copy_to_char16(info.units, units[paramIndex], 128);

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
            const char* names[] = {"Sawtooth", "Square", "Triangle", "Sine"};
            snprintf(buf, sizeof(buf), "%s", (wf >= 0 && wf <= 3) ? names[wf] : "Sawtooth");
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
        if (id < 10 && state_) {
            return state_->params[id].load();
        }
        return 0.0;
    }

    tresult SMTG_STDCALL setParamNormalized(ParamID id, ParamValue value) override {
        if (id < 10 && state_) {
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
        return 2; // Class 0: Instrument Component, Class 1: Edit Controller
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
            strncpy(info->version, "1.0.0", sizeof(info->version) - 1);
            strncpy(info->sdkVersion, "VST 3.7.0", sizeof(info->sdkVersion) - 1);
            return kResultOk;
        } else if (index == 1) {
            memcpy(info->cid, kMonkeysEarControllerCID, sizeof(TUID));
            info->cardinality = 0x7FFFFFFF;
            strncpy(info->category, "Component Controller Class", sizeof(info->category) - 1);
            strncpy(info->name, "Monkey's Ear Controller", sizeof(info->name) - 1);
            info->classFlags = 0;
            info->subCategories[0] = 0;
            strncpy(info->vendor, "Ultramonkeydog Studios", sizeof(info->vendor) - 1);
            strncpy(info->version, "1.0.0", sizeof(info->version) - 1);
            strncpy(info->sdkVersion, "VST 3.7.0", sizeof(info->sdkVersion) - 1);
            return kResultOk;
        }
        return kInvalidArgument;
    }

    tresult SMTG_STDCALL createInstance(const TUID cid, const TUID _iid, void** obj) override {
        if (memcmp(cid, kMonkeysEarComponentCID, sizeof(TUID)) == 0) {
            auto* comp = new MonkeysEarComponent();
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
