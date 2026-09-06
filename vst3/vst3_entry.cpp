#include "vst3_sdk_minimal.h"
#include "monkeys_ear/engine.h"
#include <atomic>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

// Unique Class IDs for MonkeysEar VST3
// Component CID: 4D6F6E6B-6579-7345-6172-496E73747201 ("MonkeysEarInstr1")
static const TUID kMonkeysEarComponentCID = INLINE_UID(
    0x4D, 0x6F, 0x6E, 0x6B, 0x65, 0x79, 0x73, 0x45, 0x61, 0x72, 0x49, 0x6E, 0x73, 0x74, 0x72, 0x01
);

class MonkeysEarVST3 : public IComponent, public IAudioProcessor {
public:
    MonkeysEarVST3() : ref_count_(1), active_(false), processing_(false) {
        engine_.init(48000.0f, 512);
        engine_.load_preset(monkeys_ear::PresetManager::create_factory_lead());
    }

    virtual ~MonkeysEarVST3() = default;

    // FUnknown
    tresult SMTG_STDCALL queryInterface(const TUID _iid, void** obj) override {
        if (!obj) return kInvalidArgument;

        if (memcmp(_iid, FUnknown_iid, sizeof(TUID)) == 0 ||
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

        *obj = nullptr;
        return kResultFalse;
    }

    uint32 SMTG_STDCALL addRef() override {
        return ++ref_count_;
    }

    uint32 SMTG_STDCALL release() override {
        uint32 count = --ref_count_;
        if (count == 0) {
            delete this;
        }
        return count;
    }

    // IComponent
    tresult SMTG_STDCALL initialize(FUnknown* /*context*/) override {
        return kResultOk;
    }

    tresult SMTG_STDCALL terminate() override {
        return kResultOk;
    }

    tresult SMTG_STDCALL getControllerClassId(TUID classId) override {
        memcpy(classId, kMonkeysEarComponentCID, sizeof(TUID));
        return kResultOk;
    }

    tresult SMTG_STDCALL setIoMode(IoMode /*mode*/) override {
        return kResultOk;
    }

    int32 SMTG_STDCALL getBusCount(MediaType type, BusDirection dir) override {
        if (type == kAudio) {
            return 1; // 1 Audio In (mic/sidechain), 1 Audio Out (stereo)
        } else if (type == kEvent) {
            return (dir == kInput) ? 1 : 0; // 1 Event In (MIDI)
        }
        return 0;
    }

    tresult SMTG_STDCALL getBusInfo(MediaType type, BusDirection dir, int32 index, BusInfo& bus) override {
        if (index != 0) return kInvalidArgument;

        bus.mediaType = type;
        bus.direction = dir;
        bus.busType = kMain;
        bus.flags = 1; // kDefaultActive

        if (type == kAudio) {
            bus.channelCount = 2; // Stereo
            const char16* name = (dir == kInput) ? u"Audio/Mic In" : u"Main Out";
            memcpy(bus.name, name, (std::char_traits<char16_t>::length(name) + 1) * sizeof(char16));
        } else if (type == kEvent) {
            bus.channelCount = 16;
            const char16* name = u"MIDI In";
            memcpy(bus.name, name, (std::char_traits<char16_t>::length(name) + 1) * sizeof(char16));
        }
        return kResultOk;
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

    // IAudioProcessor
    tresult SMTG_STDCALL setBusArrangements(SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts) override {
        if (numIns > 0 && inputs[0] != kSpeakerStereo) return kResultFalse;
        if (numOuts > 0 && outputs[0] != kSpeakerStereo) return kResultFalse;
        return kResultOk;
    }

    tresult SMTG_STDCALL getBusArrangement(BusDirection /*dir*/, int32 index, SpeakerArrangement& arr) override {
        if (index != 0) return kInvalidArgument;
        arr = kSpeakerStereo;
        return kResultOk;
    }

    tresult SMTG_STDCALL canProcessSampleSize(int32 symbolicSampleSize) override {
        // 0 = 32-bit float
        return (symbolicSampleSize == 0) ? kResultOk : kResultFalse;
    }

    uint32 SMTG_STDCALL getLatencySamples() override {
        // 0 samples reported latency as required by Monkey's Ear contract
        return 0;
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

        // Process incoming MIDI Events
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

        // Get Audio Inputs (Mic/Aux input)
        const float* in_l = nullptr;
        const float* in_r = nullptr;
        if (data.numInputs > 0 && data.inputs[0].numChannels >= 1) {
            in_l = data.inputs[0].channelBuffers32[0];
            in_r = (data.inputs[0].numChannels >= 2) ? data.inputs[0].channelBuffers32[1] : in_l;
        }

        // Get Audio Outputs (Stereo out)
        if (data.numOutputs > 0 && data.outputs[0].numChannels >= 2) {
            float* out_l = data.outputs[0].channelBuffers32[0];
            float* out_r = data.outputs[0].channelBuffers32[1];

            // Render block through Monkey's Ear engine
            engine_.process_block(in_l, in_r, out_l, out_r, static_cast<size_t>(data.numSamples));
        }

        return kResultOk;
    }

    uint32 SMTG_STDCALL getTailSamples() override {
        // Reverb tail ~48000 samples (~1 second)
        return 48000;
    }

private:
    std::atomic<uint32> ref_count_;
    bool active_;
    bool processing_;
    monkeys_ear::MonkeysEarEngine engine_;
};

// Plugin Factory Implementation
class MonkeysEarFactory : public IPluginFactory {
public:
    MonkeysEarFactory() : ref_count_(1) {}
    virtual ~MonkeysEarFactory() = default;

    tresult SMTG_STDCALL queryInterface(const TUID _iid, void** obj) override {
        if (!obj) return kInvalidArgument;
        if (memcmp(_iid, FUnknown_iid, sizeof(TUID)) == 0 ||
            memcmp(_iid, IPluginFactory_iid, sizeof(TUID)) == 0) {
            *obj = static_cast<IPluginFactory*>(this);
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
        return 1;
    }

    tresult SMTG_STDCALL getClassInfo(int32 index, PClassInfo* info) override {
        if (index != 0 || !info) return kInvalidArgument;
        memcpy(info->cid, kMonkeysEarComponentCID, sizeof(TUID));
        info->cardinality = 0x7FFFFFFF;
        strncpy(info->category, "Audio Module Class", sizeof(info->category) - 1);
        strncpy(info->name, "Monkey's Ear", sizeof(info->name) - 1);
        return kResultOk;
    }

    tresult SMTG_STDCALL createInstance(const TUID cid, const TUID _iid, void** obj) override {
        if (memcmp(cid, kMonkeysEarComponentCID, sizeof(TUID)) != 0) {
            return kInvalidArgument;
        }
        auto* instance = new MonkeysEarVST3();
        tresult res = instance->queryInterface(_iid, obj);
        instance->release();
        return res;
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
