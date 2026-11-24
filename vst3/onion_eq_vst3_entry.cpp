#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#endif

#include "vst3_sdk_minimal.h"
#include "ui_assets.h"
#include "monkeys_ear/ecosystem_bus.h"
#include "monkeys_ear/onion_eq_module.h"
#include "monkeys_ear/onion_eq_product_version.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

constexpr int kParamCount = 42;
// 0: Bypass (0=Active)
// 1: Dynamic Enable (1=On)
// 2: Intent (0.5=Yield)
// 3: Amount (0.65)
// 4: Sensitivity (0.55)
// 5: Low Protect (0.70)
// 6: Transient Protect (0.60)
// 7: Spectrum Tilt (0.50 -> 0 dB)
// 8: Carve Q Focus (0.274 -> 1.85)
// 9: Air Shimmer (0.50 -> 0 dB)
// 10: Ref Source (0.0 -> Auto SC+Eco)
// 11: Eco Link (1.0 -> BiDir)
// 12: Eco Channel (0.0 -> Ch 1)
// 13: Peer Mask Weight (0.75)
// 14: Stereo Mode (0.0 -> Stereo L/R)
// 15: Dry/Wet Mix (1.0 -> 100%)
// 16: Output Trim (0.50 -> 0.0 dB)
// 17..40: 24 ERB bands (0.50 -> 0.0 dB)
// 41: Harmonic F0 Guard (0.0 -> Off; preserves f0/H2/H3 fundamental focus)
constexpr std::array<float, kParamCount> kDefaultValues = []() {
    std::array<float, kParamCount> d{};
    d[0] = 0.0f;
    d[1] = 1.0f;
    d[2] = 0.5f;
    d[3] = 0.65f;
    d[4] = 0.55f;
    d[5] = 0.70f;
    d[6] = 0.60f;
    d[7] = 0.50f;
    d[8] = 0.274f;
    d[9] = 0.50f;
    d[10] = 0.0f;
    d[11] = 1.0f;
    d[12] = 0.0f;
    d[13] = 0.75f;
    d[14] = 0.0f;
    d[15] = 1.0f;
    d[16] = 0.50f;
    for (int i = 17; i <= 40; ++i) d[static_cast<size_t>(i)] = 0.50f;
    d[41] = 0.0f;
    return d;
}();

constexpr uint32 kStateMagic = 0x4D454F51u; // "MEOQ"
constexpr uint32 kStateVersion = 2u;

static const TUID kOnionComponentCID  = INLINE_UID(0x4D6F6E6B, 0x65794561, 0x724F6E69, 0x6F6E4531); // MonkeyEarOnionE1
static const TUID kOnionControllerCID = INLINE_UID(0x4D6F6E6B, 0x65794561, 0x724F6374, 0x726C4531); // MonkeyEarOctrlE1
static const TUID kOnionTelemetryProvider_iid = INLINE_UID(0x4D6F6E6B, 0x65794561, 0x724F5465, 0x6C654D31);

void copy16(char16* dst, const char* src) {
    size_t i = 0;
    while (src[i] && i < 127) {
        dst[i] = static_cast<char16>(src[i]);
        ++i;
    }
    dst[i] = 0;
}

bool write_stream(IBStream* s, void* data, int32 bytes) {
    int32 done = 0;
    return s && s->write(data, bytes, &done) == kResultOk && done == bytes;
}

bool read_stream(IBStream* s, void* data, int32 bytes) {
    int32 done = 0;
    return s && s->read(data, bytes, &done) == kResultOk && done == bytes;
}

bool serialized_float_is_finite(float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

bool normalized_value_is_finite(ParamValue value) {
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7ff0000000000000ULL) != 0x7ff0000000000000ULL;
}

struct OnionState {
    std::array<std::atomic<float>, kParamCount> values{};
    std::array<std::atomic<bool>, kParamCount> dirty{};
    OnionState() {
        for (int i = 0; i < kParamCount; ++i) {
            values[static_cast<size_t>(i)].store(kDefaultValues[static_cast<size_t>(i)], std::memory_order_relaxed);
            dirty[static_cast<size_t>(i)].store(false, std::memory_order_relaxed);
        }
    }
};

struct OnionLiveTelemetry {
    std::array<std::atomic<float>, monkeys_ear::OnionSkinEQ::kNumBands> self_occ{};
    std::array<std::atomic<float>, monkeys_ear::OnionSkinEQ::kNumBands> ref_occ{};
    std::array<std::atomic<float>, monkeys_ear::OnionSkinEQ::kNumBands> carve_db{};
    std::array<std::atomic<float>, monkeys_ear::OnionSkinEQ::kNumBands> total_eq_db{};
    std::atomic<float> identity_cosine{1.0f};
    std::atomic<float> conflict_reduction{0.0f};
    std::atomic<float> transient_hold{0.0f};
    std::atomic<float> in_peak{0.0f};
    std::atomic<float> out_peak{0.0f};
    std::atomic<bool> sidechain_active{false};
    std::atomic<bool> eco_peer_active{false};
    std::atomic<bool> self_deharsh_active{false};
};

struct TelemetrySlot {
    std::mutex mu;
    std::shared_ptr<OnionLiveTelemetry> ptr;

    std::shared_ptr<OnionLiveTelemetry> snapshot() {
        std::lock_guard<std::mutex> lock(mu);
        return ptr;
    }
    void set(std::shared_ptr<OnionLiveTelemetry> next) {
        std::lock_guard<std::mutex> lock(mu);
        ptr = std::move(next);
    }
};

class IOnionTelemetryProvider {
public:
    virtual ~IOnionTelemetryProvider() = default;
    virtual std::shared_ptr<OnionLiveTelemetry> onion_telemetry() const noexcept = 0;
    virtual void release_provider_ref() noexcept = 0;
};

struct AutomationCursor {
    IParamValueQueue* queue = nullptr;
    int32 next_index = 0;
    int32 point_count = 0;
    int32 offset = 0;
    float value = 0.0f;
    bool pending = false;
};

void advance_automation(AutomationCursor& cursor, int32 from_offset, int32 num_samples) {
    cursor.pending = false;
    while (cursor.queue && cursor.next_index < cursor.point_count) {
        int32 offset = 0;
        ParamValue raw = 0.0;
        if (cursor.queue->getPoint(cursor.next_index++, offset, raw) != kResultOk) continue;
        if (offset < from_offset || offset >= num_samples || !normalized_value_is_finite(raw)) continue;
        cursor.offset = offset;
        cursor.value = static_cast<float>(std::clamp(raw, 0.0, 1.0));
        cursor.pending = true;
        return;
    }
}

tresult write_state(IBStream* stream, const std::shared_ptr<OnionState>& state) {
    if (!stream || !state) return kInvalidArgument;
    uint32 magic = kStateMagic, version = kStateVersion, count = kParamCount;
    if (!write_stream(stream, &magic, 4) || !write_stream(stream, &version, 4) || !write_stream(stream, &count, 4))
        return kInternalError;
    for (int i = 0; i < kParamCount; ++i) {
        float value = state->values[static_cast<size_t>(i)].load();
        if (!write_stream(stream, &value, 4)) return kInternalError;
    }
    return kResultOk;
}

tresult read_state(IBStream* stream, const std::shared_ptr<OnionState>& state) {
    if (!stream || !state) return kInvalidArgument;
    uint32 magic = 0, version = 0, count = 0;
    if (!read_stream(stream, &magic, 4) || !read_stream(stream, &version, 4) || !read_stream(stream, &count, 4))
        return kInternalError;
    if (magic != kStateMagic || version > kStateVersion || count > kParamCount) return kResultFalse;
    std::array<float, kParamCount> restored = kDefaultValues;
    for (uint32 i = 0; i < count; ++i) {
        float value = 0.0f;
        if (!read_stream(stream, &value, 4)) return kInternalError;
        if (!serialized_float_is_finite(value)) return kResultFalse;
        restored[i] = std::clamp(value, 0.0f, 1.0f);
    }
    for (int i = 0; i < kParamCount; ++i) {
        state->values[static_cast<size_t>(i)].store(restored[static_cast<size_t>(i)]);
        state->dirty[static_cast<size_t>(i)].store(true);
    }
    return kResultOk;
}

monkeys_ear::OnionEqControls decode_controls(const std::shared_ptr<OnionState>& s) noexcept {
    auto v = [&](int idx) { return s->values[static_cast<size_t>(idx)].load(std::memory_order_relaxed); };
    monkeys_ear::OnionEqControls c{};
    c.bypass = v(0) >= 0.5f;
    c.dynamic_enabled = v(1) >= 0.5f;
    const int intent_idx = std::clamp(static_cast<int>(std::lround(v(2) * 2.0f)), 0, 2);
    c.intent = static_cast<monkeys_ear::OnionSkinIntent>(intent_idx);
    c.amount = v(3);
    c.sensitivity = v(4);
    c.low_protect = v(5);
    c.transient_protect = v(6);
    c.tilt_db = (v(7) - 0.5f) * 12.0f;
    c.q_focus = 0.85f + v(8) * 3.65f;
    c.air_db = (v(9) - 0.5f) * 12.0f;
    c.ref_source = static_cast<monkeys_ear::OnionRefSource>(std::clamp(static_cast<int>(std::lround(v(10) * 3.0f)), 0, 3));
    c.eco_mode = static_cast<monkeys_ear::EcoLinkMode>(std::clamp(static_cast<int>(std::lround(v(11) * 3.0f)), 0, 3));
    c.eco_slot = std::clamp(static_cast<int>(std::lround(v(12) * 7.0f)), 0, 7);
    c.peer_mask_weight = v(13);
    c.stereo_mode = static_cast<monkeys_ear::OnionStereoMode>(std::clamp(static_cast<int>(std::lround(v(14) * 2.0f)), 0, 2));
    c.mix = v(15);
    c.output_trim_db = (v(16) - 0.5f) * 24.0f;
    for (size_t b = 0; b < monkeys_ear::OnionSkinEQ::kNumBands; ++b) {
        c.manual_db[b] = (v(17 + static_cast<int>(b)) - 0.5f) * 24.0f;
    }
    c.harmonic_protect = v(41);
    return c;
}

class OnionComponent final : public IComponent,
                             public IAudioProcessor,
                             public IConnectionPoint,
                             public IOnionTelemetryProvider {
public:
    OnionComponent()
        : ref_(1),
          state_(std::make_shared<OnionState>()),
          telemetry_(std::make_shared<OnionLiveTelemetry>()) {
        module_.prepare(48000.0f, 512);
        apply_controls();
    }

    tresult SMTG_STDCALL queryInterface(const TUID iid, void** obj) override {
        if (!obj) return kInvalidArgument;
        if (!std::memcmp(iid, FUnknown_iid, sizeof(TUID)) ||
            !std::memcmp(iid, IPluginBase_iid, sizeof(TUID)) ||
            !std::memcmp(iid, IComponent_iid, sizeof(TUID))) {
            *obj = static_cast<IComponent*>(this);
        } else if (!std::memcmp(iid, IAudioProcessor_iid, sizeof(TUID))) {
            *obj = static_cast<IAudioProcessor*>(this);
        } else if (!std::memcmp(iid, IConnectionPoint_iid, sizeof(TUID))) {
            *obj = static_cast<IConnectionPoint*>(this);
        } else if (!std::memcmp(iid, kOnionTelemetryProvider_iid, sizeof(TUID))) {
            *obj = static_cast<IOnionTelemetryProvider*>(this);
            addRef();
            return kResultOk;
        } else {
            *obj = nullptr;
            return kResultFalse;
        }
        addRef();
        return kResultOk;
    }

    uint32 SMTG_STDCALL addRef() override { return ++ref_; }
    uint32 SMTG_STDCALL release() override {
        uint32 n = --ref_;
        if (!n) delete this;
        return n;
    }

    tresult SMTG_STDCALL initialize(FUnknown*) override { return kResultOk; }
    tresult SMTG_STDCALL terminate() override { return kResultOk; }
    tresult SMTG_STDCALL getControllerClassId(TUID id) override {
        std::memcpy(id, kOnionControllerCID, sizeof(TUID));
        return kResultOk;
    }
    tresult SMTG_STDCALL setIoMode(IoMode) override { return kResultOk; }

    int32 SMTG_STDCALL getBusCount(MediaType type, BusDirection dir) override {
        if (type != kAudio) return 0;
        return dir == kInput ? 2 : 1; // Main In + optional Sidechain In; Main Out
    }

    tresult SMTG_STDCALL getBusInfo(MediaType type, BusDirection dir, int32 index, BusInfo& bus) override {
        if (type != kAudio) return kInvalidArgument;
        if (dir == kInput) {
            if (index == 0) {
                bus.mediaType = kAudio;
                bus.direction = kInput;
                bus.busType = kMain;
                bus.flags = 1; // kDefaultActive
                bus.channelCount = 2;
                copy16(bus.name, "Main In");
                return kResultOk;
            }
            if (index == 1) {
                bus.mediaType = kAudio;
                bus.direction = kInput;
                bus.busType = kAux;
                bus.flags = 0; // optional sidechain bus
                bus.channelCount = 2;
                copy16(bus.name, "Sidechain In");
                return kResultOk;
            }
            return kInvalidArgument;
        }
        if (dir == kOutput && index == 0) {
            bus.mediaType = kAudio;
            bus.direction = kOutput;
            bus.busType = kMain;
            bus.flags = 1;
            bus.channelCount = 2;
            copy16(bus.name, "Main Out");
            return kResultOk;
        }
        return kInvalidArgument;
    }

    tresult SMTG_STDCALL getRoutingInfo(RoutingInfo&, RoutingInfo&) override { return kNotImplemented; }
    tresult SMTG_STDCALL activateBus(MediaType type, BusDirection dir, int32 index, bool state) override {
        if (type == kAudio && dir == kInput && index == 1) {
            sidechain_bus_active_ = state;
        }
        return kResultOk;
    }
    tresult SMTG_STDCALL setActive(bool state) override {
        if (!state) module_.reset();
        return kResultOk;
    }
    tresult SMTG_STDCALL setState(IBStream* stream) override { return read_state(stream, state_); }
    tresult SMTG_STDCALL getState(IBStream* stream) override { return write_state(stream, state_); }

    tresult SMTG_STDCALL setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                            SpeakerArrangement* outputs, int32 numOuts) override {
        if (numOuts != 1 || !outputs || outputs[0] != SpeakerArr::kStereo) return kResultFalse;
        if (numIns < 1 || numIns > 2 || !inputs) return kResultFalse;
        if (inputs[0] != SpeakerArr::kStereo) return kResultFalse;
        if (numIns == 2 && inputs[1] != SpeakerArr::kStereo && inputs[1] != SpeakerArr::kMono && inputs[1] != SpeakerArr::kEmpty)
            return kResultFalse;
        return kResultOk;
    }

    tresult SMTG_STDCALL getBusArrangement(BusDirection dir, int32 index, SpeakerArrangement& arr) override {
        if (dir == kInput && (index == 0 || index == 1)) {
            arr = SpeakerArr::kStereo;
            return kResultOk;
        }
        if (dir == kOutput && index == 0) {
            arr = SpeakerArr::kStereo;
            return kResultOk;
        }
        return kInvalidArgument;
    }

    tresult SMTG_STDCALL canProcessSampleSize(int32 size) override { return size == 0 ? kResultOk : kResultFalse; }
    uint32 SMTG_STDCALL getLatencySamples() override { return 0; }
    uint32 SMTG_STDCALL getTailSamples() override { return 0; }
    tresult SMTG_STDCALL setupProcessing(ProcessSetup& setup) override {
        module_.prepare(static_cast<float>(setup.sampleRate), static_cast<uint32_t>(std::max(1, setup.maxSamplesPerBlock)));
        apply_controls();
        return kResultOk;
    }
    tresult SMTG_STDCALL setProcessing(bool) override { return kResultOk; }

    tresult SMTG_STDCALL process(ProcessData& data) override {
        if (data.numSamples < 0) return kInvalidArgument;
        bool changed = false;
        for (int i = 0; i < kParamCount; ++i) {
            changed = state_->dirty[static_cast<size_t>(i)].exchange(false) || changed;
        }

        if (data.numSamples == 0) {
            if (data.inputParameterChanges) {
                const int32 count = std::max(0, data.inputParameterChanges->getParameterCount());
                for (int32 q = 0; q < count; ++q) {
                    auto* queue = data.inputParameterChanges->getParameterData(q);
                    if (!queue) continue;
                    const ParamID id = queue->getParameterId();
                    if (id >= kParamCount) continue;
                    const int32 points = std::max(0, queue->getPointCount());
                    for (int32 p = 0; p < points; ++p) {
                        int32 ignored_offset = 0;
                        ParamValue value = 0.0;
                        if (queue->getPoint(p, ignored_offset, value) != kResultOk || !normalized_value_is_finite(value))
                            continue;
                        state_->values[id].store(static_cast<float>(std::clamp(value, 0.0, 1.0)));
                        changed = true;
                    }
                }
            }
            if (changed) apply_controls();
            return kResultOk;
        }

        if (data.numInputs < 1 || data.numOutputs < 1 || !data.inputs || !data.outputs
            || !data.inputs[0].channelBuffers32 || !data.outputs[0].channelBuffers32) {
            return kResultFalse;
        }
        auto* in = data.inputs[0].channelBuffers32;
        auto* out = data.outputs[0].channelBuffers32;
        if (data.inputs[0].numChannels < 2 || data.outputs[0].numChannels < 2 || !in[0] || !in[1] || !out[0] || !out[1]) {
            return kResultFalse;
        }

        const float* sc_l = nullptr;
        const float* sc_r = nullptr;
        if (data.numInputs >= 2 && data.inputs[1].numChannels >= 1 && data.inputs[1].channelBuffers32) {
            sc_l = data.inputs[1].channelBuffers32[0];
            sc_r = (data.inputs[1].numChannels >= 2 && data.inputs[1].channelBuffers32[1])
                ? data.inputs[1].channelBuffers32[1]
                : sc_l;
        }

        std::array<AutomationCursor, kParamCount> events{};
        if (data.inputParameterChanges) {
            const int32 queue_count = std::max(0, data.inputParameterChanges->getParameterCount());
            for (int32 q = 0; q < queue_count; ++q) {
                auto* queue = data.inputParameterChanges->getParameterData(q);
                if (!queue) continue;
                const ParamID id = queue->getParameterId();
                if (id >= kParamCount) continue;
                auto& cursor = events[id];
                cursor.queue = queue;
                cursor.point_count = std::max(0, queue->getPointCount());
                advance_automation(cursor, 0, data.numSamples);
            }
        }

        int32 offset = 0;
        while (offset < data.numSamples) {
            int32 next_change = data.numSamples;
            for (auto& cursor : events) {
                if (cursor.pending && cursor.offset == offset) {
                    const ParamID id = cursor.queue->getParameterId();
                    state_->values[id].store(cursor.value);
                    changed = true;
                    advance_automation(cursor, offset + 1, data.numSamples);
                }
                if (cursor.pending && cursor.offset > offset) {
                    next_change = std::min(next_change, cursor.offset);
                }
            }
            if (changed) {
                apply_controls();
                changed = false;
            }
            const uint32_t chunk = static_cast<uint32_t>(next_change - offset);
            module_.process_block(
                in[0] + offset, in[1] + offset,
                sc_l ? (sc_l + offset) : nullptr,
                sc_r ? (sc_r + offset) : nullptr,
                out[0] + offset, out[1] + offset,
                chunk);
            offset = next_change;
        }

        publish_telemetry();
        return kResultOk;
    }

    // IConnectionPoint
    tresult SMTG_STDCALL connect(IConnectionPoint*) override { return kResultOk; }
    tresult SMTG_STDCALL disconnect(IConnectionPoint*) override { return kResultOk; }
    tresult SMTG_STDCALL notify(IMessage*) override { return kResultOk; }

    // IOnionTelemetryProvider
    std::shared_ptr<OnionLiveTelemetry> onion_telemetry() const noexcept override { return telemetry_; }
    void release_provider_ref() noexcept override { release(); }

private:
    void apply_controls() {
        module_.set_controls(decode_controls(state_));
    }

    void publish_telemetry() noexcept {
        if (!telemetry_) return;
        const auto& m = module_.metrics();
        const auto& h = module_.health();
        for (size_t b = 0; b < monkeys_ear::OnionSkinEQ::kNumBands; ++b) {
            telemetry_->self_occ[b].store(m.self_occupancy[b], std::memory_order_relaxed);
            telemetry_->ref_occ[b].store(m.ref_occupancy[b], std::memory_order_relaxed);
            telemetry_->carve_db[b].store(m.band_carve_db[b], std::memory_order_relaxed);
            telemetry_->total_eq_db[b].store(m.total_eq_db[b], std::memory_order_relaxed);
        }
        telemetry_->identity_cosine.store(m.identity_cosine, std::memory_order_relaxed);
        telemetry_->conflict_reduction.store(m.conflict_reduction, std::memory_order_relaxed);
        telemetry_->transient_hold.store(m.transient_hold, std::memory_order_relaxed);
        telemetry_->in_peak.store(h.in_peak, std::memory_order_relaxed);
        telemetry_->out_peak.store(h.out_peak, std::memory_order_relaxed);
        telemetry_->sidechain_active.store(h.sidechain_active, std::memory_order_relaxed);
        telemetry_->eco_peer_active.store(h.eco_peer_active, std::memory_order_relaxed);
        telemetry_->self_deharsh_active.store(h.self_deharsh_active, std::memory_order_relaxed);
    }

    std::atomic<uint32> ref_;
    std::shared_ptr<OnionState> state_;
    std::shared_ptr<OnionLiveTelemetry> telemetry_;
    monkeys_ear::OnionEqModule module_;
    bool sidechain_bus_active_ = false;
};

#if defined(_WIN32)
int kOnionModuleAnchor = 0;
std::atomic<int> gOnionGdiplusRefs{0};
ULONG_PTR gOnionGdiplusToken = 0;

void gdiplus_acquire() {
    if (gOnionGdiplusRefs.fetch_add(1) == 0) {
        Gdiplus::GdiplusStartupInput input;
        if (Gdiplus::GdiplusStartup(&gOnionGdiplusToken, &input, nullptr) != Gdiplus::Ok) {
            gOnionGdiplusToken = 0;
        }
    }
}

void gdiplus_release() {
    if (gOnionGdiplusRefs.fetch_sub(1) == 1 && gOnionGdiplusToken != 0) {
        Gdiplus::GdiplusShutdown(gOnionGdiplusToken);
        gOnionGdiplusToken = 0;
    }
}

HINSTANCE module_instance() {
    HMODULE mod = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&kOnionModuleAnchor), &mod);
    return mod;
}

Gdiplus::Color gp_col(COLORREF c, BYTE a = 255) {
    return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

void gp_round_rect_path(Gdiplus::GraphicsPath& path, Gdiplus::REAL x, Gdiplus::REAL y,
                         Gdiplus::REAL w, Gdiplus::REAL h, Gdiplus::REAL r) {
    const Gdiplus::REAL d = std::min(std::min(w, h), std::max(2.0f, r * 2.0f));
    path.Reset();
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

void draw_rect_gp(Gdiplus::Graphics& g, int x, int y, int w, int h, COLORREF fill, COLORREF outline,
                  int rad = 10, BYTE fill_alpha = 232, BYTE outline_alpha = 255) {
    Gdiplus::GraphicsPath path;
    gp_round_rect_path(path, static_cast<Gdiplus::REAL>(x), static_cast<Gdiplus::REAL>(y),
                       static_cast<Gdiplus::REAL>(w), static_cast<Gdiplus::REAL>(h),
                       static_cast<Gdiplus::REAL>(rad));
    Gdiplus::SolidBrush fbr(gp_col(fill, fill_alpha));
    Gdiplus::Pen open(gp_col(outline, outline_alpha), 1.3f);
    g.FillPath(&fbr, &path);
    g.DrawPath(&open, &path);
}

void draw_text_w(HDC dc, HFONT f, COLORREF c, const std::wstring& s, int x, int y, int w, int h, UINT align = DT_LEFT) {
    HFONT old = reinterpret_cast<HFONT>(SelectObject(dc, f));
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    RECT r{x, y, x + w, y + h};
    DrawTextW(dc, s.c_str(), -1, &r, align | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old);
}

class OnionEQEditor final : public IPlugView {
public:
    static constexpr int kWidth = 980;
    static constexpr int kHeight = 620;
    static constexpr UINT_PTR kTimerId = 101;

    OnionEQEditor(IEditController* controller, IComponentHandler** handler_slot,
                  std::shared_ptr<TelemetrySlot> telemetry_slot)
        : ref_(1),
          controller_(controller),
          handler_slot_(handler_slot),
          telemetry_slot_(std::move(telemetry_slot)) {
        gdiplus_acquire();
        plate_bmp_ = monkeys_ear::ui::assets::load_bitmap_from_bytes(
            monkeys_ear::ui::assets::kStudioChassisPlateJpg, sizeof(monkeys_ear::ui::assets::kStudioChassisPlateJpg));
        crest_bmp_ = monkeys_ear::ui::assets::load_bitmap_from_bytes(
            monkeys_ear::ui::assets::kMonkeysEarCrestPng, sizeof(monkeys_ear::ui::assets::kMonkeysEarCrestPng));
        title_font_ = CreateFontW(21, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        body_font_  = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        small_font_ = CreateFontW(12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        micro_font_ = CreateFontW(11, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        eco_bus_.ensure_connected();
    }

    ~OnionEQEditor() {
        removed();
        plate_bmp_.reset();
        crest_bmp_.reset();
        if (title_font_) DeleteObject(title_font_);
        if (body_font_) DeleteObject(body_font_);
        if (small_font_) DeleteObject(small_font_);
        if (micro_font_) DeleteObject(micro_font_);
        gdiplus_release();
    }

    std::shared_ptr<OnionLiveTelemetry> snap_telemetry() const {
        return telemetry_slot_ ? telemetry_slot_->snapshot() : nullptr;
    }

    tresult SMTG_STDCALL queryInterface(const TUID iid, void** obj) override {
        if (!obj) return kInvalidArgument;
        if (!std::memcmp(iid, FUnknown_iid, sizeof(TUID)) || !std::memcmp(iid, IPlugView_iid, sizeof(TUID))) {
            *obj = static_cast<IPlugView*>(this);
            addRef();
            return kResultOk;
        }
        *obj = nullptr;
        return kResultFalse;
    }
    uint32 SMTG_STDCALL addRef() override { return ++ref_; }
    uint32 SMTG_STDCALL release() override {
        uint32 n = --ref_;
        if (!n) delete this;
        return n;
    }

    tresult SMTG_STDCALL isPlatformTypeSupported(FIDString type) override {
        return (type && std::strcmp(type, "HWND") == 0) ? kResultOk : kInvalidArgument;
    }
    tresult SMTG_STDCALL attached(void* parent, FIDString type) override {
        if (!parent || !type || std::strcmp(type, "HWND") != 0) return kInvalidArgument;
        if (hwnd_) return kResultOk;
        register_class();
        hwnd_ = CreateWindowExW(
            0, class_name(), L"Monkey's Ear Onion Skin EQ",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            0, 0, kWidth, kHeight, reinterpret_cast<HWND>(parent), nullptr, module_instance(), this);
        if (!hwnd_) return kInternalError;
        SetTimer(hwnd_, kTimerId, 33, nullptr);
        return kResultOk;
    }
    tresult SMTG_STDCALL removed() override {
        if (hwnd_) {
            KillTimer(hwnd_, kTimerId);
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
        return kResultOk;
    }
    tresult SMTG_STDCALL onWheel(float) override { return kResultFalse; }
    tresult SMTG_STDCALL onKeyDown(char16, int16, int16) override { return kResultFalse; }
    tresult SMTG_STDCALL onKeyUp(char16, int16, int16) override { return kResultFalse; }
    tresult SMTG_STDCALL getSize(ViewRect* size) override {
        if (!size) return kInvalidArgument;
        size->left = 0; size->top = 0; size->right = kWidth; size->bottom = kHeight;
        return kResultOk;
    }
    tresult SMTG_STDCALL onSize(ViewRect*) override { return kResultOk; }
    tresult SMTG_STDCALL onFocus(TBool) override { return kResultOk; }
    tresult SMTG_STDCALL setFrame(IPlugFrame* frame) override { frame_ = frame; return kResultOk; }
    tresult SMTG_STDCALL canResize() override { return kResultFalse; }
    tresult SMTG_STDCALL checkSizeConstraint(ViewRect* rect) override {
        if (!rect) return kInvalidArgument;
        rect->right = rect->left + kWidth;
        rect->bottom = rect->top + kHeight;
        return kResultOk;
    }

private:
    enum class HitKind {
        None = 0,
        Knob,
        Toggle,
        StepPrev,
        StepNext,
        EqCanvas,
        EqPreset,
        Slider
    };

    struct HitTarget {
        RECT rect{};
        int param_id = -1;
        HitKind kind = HitKind::None;
        int extra = 0;
    };

    static constexpr COLORREF kBg      = RGB(11, 15, 21);
    static constexpr COLORREF kPanel   = RGB(22, 28, 38);
    static constexpr COLORREF kPanelHi = RGB(31, 39, 53);
    static constexpr COLORREF kText    = RGB(238, 243, 249);
    static constexpr COLORREF kMuted   = RGB(148, 162, 179);
    static constexpr COLORREF kAccent  = RGB(52, 224, 200);
    static constexpr COLORREF kViolet  = RGB(164, 128, 255);
    static constexpr COLORREF kEmerald = RGB(82, 228, 156);
    static constexpr COLORREF kGold    = RGB(244, 184, 72);
    static constexpr COLORREF kCoral   = RGB(242, 108, 108);
    static constexpr COLORREF kOutline = RGB(52, 66, 86);

    static const wchar_t* class_name() { return L"MonkeysEarOnionSkinEQEditorWnd_01"; }
    static void register_class() {
        static bool registered = false;
        if (registered) return;
        WNDCLASSW wc{};
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = &OnionEQEditor::wnd_proc;
        wc.hInstance = module_instance();
        wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
        wc.lpszClassName = class_name();
        RegisterClassW(&wc);
        registered = true;
    }

    IComponentHandler* handler() const { return handler_slot_ ? *handler_slot_ : nullptr; }

    void begin_param(int id) {
        if (auto* h = handler()) h->beginEdit(static_cast<ParamID>(id));
    }
    void push_param(int id, double value) {
        if (!controller_ || id < 0 || id >= kParamCount) return;
        const double clamped = std::clamp(value, 0.0, 1.0);
        controller_->setParamNormalized(static_cast<ParamID>(id), clamped);
        if (auto* h = handler()) h->performEdit(static_cast<ParamID>(id), clamped);
        if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
    }
    void end_param(int id) {
        if (auto* h = handler()) h->endEdit(static_cast<ParamID>(id));
    }

    std::wstring format_param(int id) const {
        if (!controller_ || id < 0 || id >= kParamCount) return L"";
        char16 buf[128]{};
        if (controller_->getParamStringByValue(static_cast<ParamID>(id),
                                               controller_->getParamNormalized(static_cast<ParamID>(id)),
                                               buf) != kResultOk) {
            return L"";
        }
        std::wstring out;
        for (size_t i = 0; i < 127 && buf[i]; ++i) out += static_cast<wchar_t>(buf[i]);
        return out;
    }

    double param_val(int id) const {
        return controller_ ? controller_->getParamNormalized(static_cast<ParamID>(id)) : 0.0;
    }

    void draw_knob(HDC dc, Gdiplus::Graphics& g, int x, int y, int w, int h, int id, const wchar_t* title, COLORREF color) {
        const bool active = (hover_id_ == id || drag_id_ == id);
        draw_rect_gp(g, x, y, w, h, active ? kPanelHi : kPanel, active ? color : kOutline, 10, 224);
        draw_text_w(dc, micro_font_, kMuted, title, x + 4, y + 4, w - 8, 16, DT_CENTER);

        const float cx = static_cast<float>(x + w / 2);
        const float cy = static_cast<float>(y + 52);
        const float r = 22.0f;
        const double val = param_val(id);
        constexpr float start_deg = 135.0f;
        constexpr float sweep_max = 270.0f;
        const float sweep_val = static_cast<float>(val) * sweep_max;

        {
            Gdiplus::Pen track_pen(gp_col(RGB(38, 48, 64), 220), 4.0f);
            track_pen.SetStartCap(Gdiplus::LineCapRound);
            track_pen.SetEndCap(Gdiplus::LineCapRound);
            g.DrawArc(&track_pen, cx - r, cy - r, r * 2.0f, r * 2.0f, start_deg, sweep_max);

            Gdiplus::SolidBrush cap_br(gp_col(RGB(14, 19, 27), 245));
            g.FillEllipse(&cap_br, cx - (r - 5.0f), cy - (r - 5.0f), (r - 5.0f) * 2.0f, (r - 5.0f) * 2.0f);

            if (sweep_val > 0.5f) {
                Gdiplus::Pen val_pen(gp_col(color, 255), 4.0f);
                val_pen.SetStartCap(Gdiplus::LineCapRound);
                val_pen.SetEndCap(Gdiplus::LineCapRound);
                g.DrawArc(&val_pen, cx - r, cy - r, r * 2.0f, r * 2.0f, start_deg, sweep_val);
            }

            const float ang = (start_deg + sweep_val) * 3.14159265f / 180.0f;
            Gdiplus::Pen ptr_pen(gp_col(kText, 240), 2.0f);
            ptr_pen.SetStartCap(Gdiplus::LineCapRound);
            ptr_pen.SetEndCap(Gdiplus::LineCapRound);
            g.DrawLine(&ptr_pen, cx, cy, cx + std::cos(ang) * (r - 6.0f), cy + std::sin(ang) * (r - 6.0f));
        }
        draw_text_w(dc, small_font_, color, format_param(id), x + 4, y + h - 22, w - 8, 18, DT_CENTER);
        hits_.push_back({{x, y, x + w, y + h}, id, HitKind::Knob, 0});
    }

    void draw_slider(HDC dc, Gdiplus::Graphics& g, int x, int y, int w, int h, int id, const wchar_t* title, COLORREF color) {
        const bool active = (hover_id_ == id || drag_id_ == id);
        draw_rect_gp(g, x, y, w, h, active ? kPanelHi : kPanel, active ? color : kOutline, 8, 224);
        const int lw = static_cast<int>(w * 0.54f);
        draw_text_w(dc, micro_font_, kMuted, title, x + 8, y + 4, lw - 8, 18, DT_LEFT);
        draw_text_w(dc, small_font_, color, format_param(id), x + lw, y + 4, w - lw - 8, 18, DT_RIGHT);

        const int bar_x = x + 8, bar_y = y + h - 14, bar_w = w - 16, bar_h = 6;
        draw_rect_gp(g, bar_x, bar_y, bar_w, bar_h, RGB(14, 18, 25), kOutline, 3, 240, 120);
        const double v = param_val(id);
        const int fill_w = std::max(6, static_cast<int>(std::round(v * static_cast<double>(bar_w))));
        draw_rect_gp(g, bar_x, bar_y, fill_w, bar_h, color, color, 3, 240, 0);
        hits_.push_back({{x, y, x + w, y + h}, id, HitKind::Slider, 0});
    }

    void draw_stepper(HDC dc, Gdiplus::Graphics& g, int x, int y, int w, int h, int id,
                      const wchar_t* title, COLORREF color, int steps) {
        const bool active = (hover_id_ == id);
        draw_rect_gp(g, x, y, w, h, active ? kPanelHi : kPanel, active ? color : kOutline, 8, 224);
        draw_text_w(dc, micro_font_, kMuted, title, x + 8, y + 3, w - 58, 16, DT_LEFT);
        draw_text_w(dc, small_font_, color, format_param(id), x + 8, y + 19, w - 58, 18, DT_LEFT);

        const int bx1 = x + w - 46, bx2 = x + w - 24, by = y + 8;
        draw_rect_gp(g, bx1, by, 18, 20, RGB(16, 22, 31), kOutline, 4);
        draw_rect_gp(g, bx2, by, 18, 20, RGB(16, 22, 31), kOutline, 4);
        draw_text_w(dc, micro_font_, kText, L"<", bx1, by, 18, 20, DT_CENTER);
        draw_text_w(dc, micro_font_, kText, L">", bx2, by, 18, 20, DT_CENTER);

        hits_.push_back({{bx1, by, bx1 + 18, by + 20}, id, HitKind::StepPrev, steps});
        hits_.push_back({{bx2, by, bx2 + 18, by + 20}, id, HitKind::StepNext, steps});
    }

    void paint() {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd_, &ps);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, kWidth, kHeight);
        HGDIOBJ old_bmp = SelectObject(mem, bmp);
        const auto telemetry_ = snap_telemetry();

        hits_.clear();
        {
            Gdiplus::Graphics g(mem);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

            // Base background & ComfyUI chassis plate bezel
            g.Clear(gp_col(kBg, 255));
            if (plate_bmp_) {
                g.DrawImage(plate_bmp_.get(), 0, 0, kWidth, kHeight);
                Gdiplus::SolidBrush veil(Gdiplus::Color(158, 8, 11, 17));
                g.FillRectangle(&veil, 0, 72, kWidth, kHeight - 72);
            }

            // Header glass bar
            draw_rect_gp(g, 14, 10, kWidth - 28, 58, RGB(16, 22, 31), kOutline, 12, 228);
            if (crest_bmp_) {
                g.DrawImage(crest_bmp_.get(), 22, 16, 46, 46);
            }
            draw_text_w(mem, title_font_, kText, L"MONKEY'S EAR  //  ONION SKIN 24-ERB EQ", 76, 14, 460, 26);
            draw_text_w(mem, micro_font_, kMuted, L"STANDALONE COCHLEAR DYNAMIC + SURGICAL SPECTRAL ENGINE", 76, 42, 460, 18);

            const bool bypassed = param_val(0) >= 0.5;
            const bool sc_live = telemetry_ && telemetry_->sidechain_active.load(std::memory_order_relaxed);
            const bool eco_live = telemetry_ && telemetry_->eco_peer_active.load(std::memory_order_relaxed);
            const bool self_live = telemetry_ && telemetry_->self_deharsh_active.load(std::memory_order_relaxed);
            const float cos_val = telemetry_ ? telemetry_->identity_cosine.load(std::memory_order_relaxed) : 1.0f;
            const float conflict_pct = telemetry_ ? (telemetry_->conflict_reduction.load(std::memory_order_relaxed) * 100.0f) : 0.0f;
            const float hold_pct = telemetry_ ? (telemetry_->transient_hold.load(std::memory_order_relaxed) * 100.0f) : 0.0f;

            // Projection Gate & Route status pill
            std::wstring status_txt = sc_live ? L"SIDECHAIN 3/4 LIVE"
                : (eco_live ? L"ECO PEER LINKED"
                : (self_live ? L"SELF DE-HARSH" : L"SURGICAL / STANDBY"));
            COLORREF status_col = sc_live ? kAccent : (eco_live ? kGold : (self_live ? kViolet : kMuted));
            draw_rect_gp(g, 548, 18, 152, 42, RGB(13, 18, 26), status_col, 8, 232);
            draw_text_w(mem, micro_font_, status_col, status_txt, 552, 20, 144, 18, DT_CENTER);
            wchar_t cos_buf[64]{};
            std::swprintf(cos_buf, 64, L"GATE COS %.2f", cos_val);
            draw_text_w(mem, micro_font_, cos_val >= 0.86f ? kEmerald : kGold, cos_buf, 552, 38, 144, 18, DT_CENTER);

            // Stereo Peak Meters (IN / OUT)
            const float in_pk = telemetry_ ? std::clamp(telemetry_->in_peak.load(std::memory_order_relaxed), 0.0f, 1.0f) : 0.0f;
            const float out_pk = telemetry_ ? std::clamp(telemetry_->out_peak.load(std::memory_order_relaxed), 0.0f, 1.0f) : 0.0f;
            draw_rect_gp(g, 710, 18, 126, 42, RGB(13, 18, 26), kOutline, 8, 232);
            draw_text_w(mem, micro_font_, kMuted, L"IN", 716, 22, 24, 16);
            draw_text_w(mem, micro_font_, kMuted, L"OUT", 716, 40, 24, 16);
            draw_rect_gp(g, 744, 26, 84, 8, RGB(24, 30, 42), kOutline, 3);
            draw_rect_gp(g, 744, 44, 84, 8, RGB(24, 30, 42), kOutline, 3);
            if (in_pk > 0.005f) {
                draw_rect_gp(g, 744, 26, std::max(4, static_cast<int>(in_pk * 84.0f)), 8, kAccent, kAccent, 3, 240, 0);
            }
            if (out_pk > 0.005f) {
                draw_rect_gp(g, 744, 44, std::max(4, static_cast<int>(out_pk * 84.0f)), 8, kEmerald, kEmerald, 3, 240, 0);
            }

            // Master Active / Bypass toggle
            draw_rect_gp(g, 846, 18, 108, 42,
                         bypassed ? RGB(52, 24, 24) : RGB(22, 48, 44),
                         bypassed ? kCoral : kEmerald, 8, 240);
            draw_text_w(mem, body_font_, bypassed ? kCoral : kEmerald,
                        bypassed ? L"BYPASSED" : L"ACTIVE", 846, 18, 108, 42, DT_CENTER);
            hits_.push_back({{846, 18, 954, 60}, 0, HitKind::Toggle, 0});

            // ── Left/Main 24-Band Interactive ERB Analyzer & Curve Editor (16, 78, 668, 326)
            constexpr int ax = 16, ay = 78, aw = 668, ah = 326;
            draw_rect_gp(g, ax, ay, aw, ah, RGB(11, 15, 22), kOutline, 12, 238);
            const int eco_ch = std::clamp(static_cast<int>(std::lround(param_val(12) * 7.0)), 0, 7);
            wchar_t eq_hdr[80]{};
            std::swprintf(eq_hdr, 80, L"24-BAND COCHLEAR ERB EQ  |  ECO CH %d", eco_ch + 1);
            draw_text_w(mem, body_font_, kViolet, eq_hdr, ax + 14, ay + 8, 300, 22);

            // 4 Quick Surgical EQ Presets
            constexpr const wchar_t* pre_lbl[4] = {L"FLAT", L"WARM", L"CLARITY", L"POCKET"};
            for (int i = 0; i < 4; ++i) {
                const int px = ax + 370 + i * 72;
                draw_rect_gp(g, px, ay + 8, 66, 22, RGB(24, 30, 42), kEmerald, 6);
                draw_text_w(mem, micro_font_, kEmerald, pre_lbl[i], px + 2, ay + 8, 62, 22, DT_CENTER);
                hits_.push_back({{px, ay + 8, px + 66, ay + 30}, -1, HitKind::EqPreset, i});
            }

            constexpr int gx = ax + 16, gy = ay + 38, gw = aw - 32, gh = ah - 82;
            draw_rect_gp(g, gx, gy, gw, gh, RGB(14, 19, 27), kOutline, 10);
            const int zero_y = gy + gh / 2;
            {
                Gdiplus::Pen zero_pen(gp_col(RGB(74, 88, 108), 210), 1.0f);
                g.DrawLine(&zero_pen, gx + 4, zero_y, gx + gw - 4, zero_y);
                Gdiplus::Pen grid_pen(gp_col(RGB(34, 42, 56), 145), 1.0f);
                g.DrawLine(&grid_pen, gx + 4, gy + gh / 4, gx + gw - 4, gy + gh / 4);
                g.DrawLine(&grid_pen, gx + 4, gy + (3 * gh) / 4, gx + gw - 4, gy + (3 * gh) / 4);
            }

            // Read live Ecosystem peer if component hasn't populated ref_occ (or combine cleanly)
            monkeys_ear::EcosystemStreamFrame sf{}, vf{};
            const bool bus_s = eco_bus_.read_synth(eco_ch, sf);
            const bool bus_v = eco_bus_.read_vocal(eco_ch, vf);
            const float tilt_db = static_cast<float>((param_val(7) - 0.5) * 12.0);
            const float air_db  = static_cast<float>((param_val(9) - 0.5) * 12.0);
            const bool dyn_on = param_val(1) >= 0.5 && !bypassed;

            std::array<Gdiplus::PointF, 24> user_pts{};
            std::array<Gdiplus::PointF, 24> total_pts{};
            const float col_w = static_cast<float>(gw - 16) / 24.0f;

            for (int b = 0; b < 24; ++b) {
                const size_t bu = static_cast<size_t>(b);
                const float cx = gx + 8.0f + (static_cast<float>(b) + 0.5f) * col_w;

                const float self_o = telemetry_
                    ? std::clamp(telemetry_->self_occ[bu].load(std::memory_order_relaxed), 0.0f, 1.0f)
                    : 0.04f;
                float ref_o = telemetry_
                    ? std::clamp(telemetry_->ref_occ[bu].load(std::memory_order_relaxed), 0.0f, 1.0f)
                    : 0.0f;
                if (ref_o < 0.01f && (bus_s || bus_v)) {
                    ref_o = std::max(bus_s ? sf.spectral_occupancy[bu] : 0.0f,
                                     bus_v ? vf.spectral_occupancy[bu] : 0.0f);
                }
                const float sh = self_o * (gh * 0.85f);
                const float rh = ref_o * (gh * 0.85f);

                Gdiplus::SolidBrush sbr(gp_col(kAccent, 92));
                g.FillRectangle(&sbr, cx - col_w * 0.36f, static_cast<float>(gy + gh - 4) - sh, col_w * 0.38f, sh);
                if (rh > 2.0f) {
                    Gdiplus::SolidBrush rbr(gp_col(kGold, 115));
                    g.FillRectangle(&rbr, cx + 1.0f, static_cast<float>(gy + gh - 4) - rh, col_w * 0.38f, rh);
                }

                // Dynamic carve overlay bar from 0 dB line
                const float carve_db = telemetry_
                    ? std::clamp(telemetry_->carve_db[bu].load(std::memory_order_relaxed), -12.0f, 2.0f)
                    : 0.0f;
                if (dyn_on && std::abs(carve_db) > 0.08f) {
                    const float ch_px = (std::abs(carve_db) / 12.0f) * (gh * 0.44f);
                    Gdiplus::SolidBrush cbr(gp_col(carve_db < 0.0f ? kViolet : kGold, 95));
                    if (carve_db < 0.0f) {
                        g.FillRectangle(&cbr, cx - col_w * 0.30f, static_cast<float>(zero_y), col_w * 0.60f, ch_px);
                    } else {
                        g.FillRectangle(&cbr, cx - col_w * 0.30f, static_cast<float>(zero_y) - ch_px, col_w * 0.60f, ch_px);
                    }
                }

                const float u_db = static_cast<float>((param_val(17 + b) - 0.5) * 24.0);
                const float pos_norm = (static_cast<float>(b) / 23.0f) * 2.0f - 1.0f;
                const float tilt_part = pos_norm * tilt_db * 0.55f;
                const float air_part = (b >= 18) ? (air_db * (static_cast<float>(b - 17) / 6.0f) * 0.65f) : 0.0f;
                const float net_user = std::clamp(u_db + tilt_part + air_part, -12.0f, 12.0f);
                const float total_db = telemetry_
                    ? std::clamp(telemetry_->total_eq_db[bu].load(std::memory_order_relaxed), -15.0f, 12.0f)
                    : net_user;

                user_pts[bu] = Gdiplus::PointF(cx, zero_y - (net_user / 12.0f) * (gh * 0.44f));
                total_pts[bu] = Gdiplus::PointF(cx, zero_y - (total_db / 12.0f) * (gh * 0.44f));
            }

            {
                if (dyn_on) {
                    Gdiplus::Pen dyn_pen(gp_col(kViolet, 210), 1.8f);
                    dyn_pen.SetDashStyle(Gdiplus::DashStyleDash);
                    g.DrawCurve(&dyn_pen, total_pts.data(), 24, 0.35f);
                }
                Gdiplus::Pen eq_glow(gp_col(kEmerald, 85), 6.0f);
                Gdiplus::Pen eq_line(gp_col(kEmerald, 255), 2.2f);
                g.DrawCurve(&eq_glow, user_pts.data(), 24, 0.35f);
                g.DrawCurve(&eq_line, user_pts.data(), 24, 0.35f);
                for (int b = 0; b < 24; ++b) {
                    const Gdiplus::PointF& p = user_pts[static_cast<size_t>(b)];
                    Gdiplus::SolidBrush node_br(gp_col(kText, 235));
                    g.FillEllipse(&node_br, p.X - 3.0f, p.Y - 3.0f, 6.0f, 6.0f);
                }
            }
            hits_.push_back({{gx, gy, gx + gw, gy + gh}, 17, HitKind::EqCanvas, 0});

            constexpr int tick_idx[7] = {0, 3, 7, 11, 15, 19, 23};
            constexpr const wchar_t* tick_lbl[7] = {L"40Hz", L"150Hz", L"400Hz", L"878Hz", L"1.8k", L"3.6k", L"9.6k"};
            for (int t = 0; t < 7; ++t) {
                const int tx = static_cast<int>(user_pts[static_cast<size_t>(tick_idx[t])].X) - 24;
                draw_text_w(mem, micro_font_, kMuted, tick_lbl[t], tx, gy + gh + 2, 48, 16, DT_CENTER);
            }

            wchar_t legend[140]{};
            std::swprintf(legend, 140,
                          L"CYAN=SELF  GOLD=PEER/SC  VIOLET=DYN CARVE  EMERALD=SURGICAL  |  CONFLICT -%.0f%%  |  HOLD %.0f%%",
                          conflict_pct, hold_pct);
            draw_text_w(mem, micro_font_, kMuted, legend, ax + 16, ay + ah - 22, aw - 32, 18);

            // ── Right Routing & Ecosystem Column (694, 78, 270, 326)
            constexpr int rx = 694, ry = 78, rw = 270, rh = 326;
            draw_rect_gp(g, rx, ry, rw, rh, RGB(15, 20, 29), kOutline, 12, 232);
            draw_text_w(mem, body_font_, kGold, L"CARVE & ECOSYSTEM ROUTING", rx + 12, ry + 8, rw - 24, 20);

            const bool dyn_enabled = param_val(1) >= 0.5;
            draw_rect_gp(g, rx + 12, ry + 34, rw - 24, 34,
                         dyn_enabled ? RGB(36, 28, 56) : RGB(22, 26, 35),
                         dyn_enabled ? kViolet : kOutline, 8, 238);
            draw_text_w(mem, body_font_, dyn_enabled ? kViolet : kMuted,
                        dyn_enabled ? L"DYNAMIC CARVE: ON" : L"DYNAMIC CARVE: OFF (EQ ONLY)",
                        rx + 16, ry + 34, rw - 32, 34, DT_CENTER);
            hits_.push_back({{rx + 12, ry + 34, rx + rw - 12, ry + 68}, 1, HitKind::Toggle, 0});

            draw_stepper(mem, g, rx + 12, ry + 76, rw - 24, 42, 10, L"REFERENCE SOURCE", kAccent, 3);
            draw_stepper(mem, g, rx + 12, ry + 124, rw - 24, 42, 2, L"CARVE INTENT", kViolet, 2);
            draw_stepper(mem, g, rx + 12, ry + 172, rw - 24, 42, 14, L"STEREO MODE", kEmerald, 2);
            draw_stepper(mem, g, rx + 12, ry + 220, 126, 42, 11, L"ECO LINK", kGold, 3);
            draw_stepper(mem, g, rx + 144, ry + 220, 114, 42, 12, L"ECO CH", kGold, 7);
            draw_slider(mem, g, rx + 12, ry + 268, rw - 24, 46, 13, L"PEER MASK WEIGHT", kGold);

            // ── Bottom Mastering & Dynamic Parameter Strip (16, 414, 948, 174)
            draw_rect_gp(g, 16, 414, 948, 174, RGB(14, 19, 27), kOutline, 12, 234);
            draw_text_w(mem, body_font_, kEmerald, L"DYNAMIC PROJECTION GATE & COCHLEAR MASTERING", 30, 421, 520, 20);
            draw_text_w(mem, micro_font_, kMuted, L"Drag vertical / sweep ERB bands  |  Shift=fine  |  Wheel adjust  |  Right-click reset",
                        430, 422, 520, 18, DT_RIGHT);

            struct KnobSpec {
                int id;
                const wchar_t* title;
                COLORREF col;
            };
            constexpr KnobSpec knobs[10] = {
                {3,  L"AMOUNT",      kViolet},
                {4,  L"SENSITIVITY", kViolet},
                {5,  L"LOW PROT",    kAccent},
                {6,  L"TRANSIENT",   kGold},
                {41, L"F0 GUARD",    kAccent},
                {7,  L"SPEC TILT",   kEmerald},
                {8,  L"Q FOCUS",     kEmerald},
                {9,  L"AIR SHIMMER", kAccent},
                {15, L"DRY / WET",   kGold},
                {16, L"OUT TRIM",    kText},
            };
            for (int i = 0; i < 10; ++i) {
                const int kx = 24 + i * 93;
                draw_knob(mem, g, kx, 446, 89, 132, knobs[i].id, knobs[i].title, knobs[i].col);
            }

            // Bottom Footer Bar
            const int show_id = drag_id_ >= 0 ? drag_id_ : hover_id_;
            std::wstring footer = L"Ready — Sweep-paint 24 ERB critical bands (40 Hz - 9.6 kHz) or route Sidechain 3/4 / Ecosystem Peer.";
            if (show_id >= 17 && show_id <= 40) {
                const int b = show_id - 17;
                const float hz = monkeys_ear::OnionSkinEQ::kBandCentersHz[static_cast<size_t>(b)];
                wchar_t fbuf[128]{};
                std::swprintf(fbuf, 128, L"[BAND %d] %.0f Hz = %ls   |   Projection Gate enforces cosine >= 0.86",
                              b + 1, hz, format_param(show_id).c_str());
                footer = fbuf;
            } else if (show_id >= 0 && show_id < kParamCount) {
                wchar_t fbuf[128]{};
                std::swprintf(fbuf, 128, L"[PARAM %d] Value: %ls   |   Right-click or double-click any control to reset",
                              show_id, format_param(show_id).c_str());
                footer = fbuf;
            }
            draw_text_w(mem, micro_font_, show_id >= 0 ? kAccent : kMuted, footer, 20, 594, kWidth - 40, 20);
        }

        BitBlt(dc, 0, 0, kWidth, kHeight, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old_bmp);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd_, &ps);
    }

    const HitTarget* find_hit(int x, int y) const {
        for (const auto& h : hits_) {
            if (x >= h.rect.left && x < h.rect.right && y >= h.rect.top && y < h.rect.bottom) {
                return &h;
            }
        }
        return nullptr;
    }

    void handle_eq_canvas_drag(const HitTarget& canvas, int x, int y) {
        const int gw = std::max(24, static_cast<int>(canvas.rect.right - canvas.rect.left));
        const int gh = std::max(24, static_cast<int>(canvas.rect.bottom - canvas.rect.top));
        const float col_w = static_cast<float>(gw - 16) / 24.0f;
        const float fx = static_cast<float>(x - canvas.rect.left - 8) / col_w;
        const int band = std::clamp(static_cast<int>(std::floor(fx)), 0, 23);
        const int zero_y = static_cast<int>(canvas.rect.top) + gh / 2;
        const float half_span = static_cast<float>(gh) * 0.44f;
        const float db = std::clamp((static_cast<float>(zero_y - y) / std::max(1.0f, half_span)) * 12.0f, -12.0f, 12.0f);
        const double norm = std::clamp(static_cast<double>(db) / 24.0 + 0.5, 0.0, 1.0);
        const int pid = 17 + band;

        if (drag_band_ != pid) {
            if (drag_band_ >= 17) end_param(drag_band_);
            drag_band_ = pid;
            begin_param(pid);
        }
        hover_id_ = pid;
        push_param(pid, norm);
    }

    void apply_eq_preset(int idx) {
        static constexpr std::array<std::array<float, 24>, 4> kCurves{{
            // 0: FLAT
            {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
            // 1: WARM
            {1.5f,2.0f,2.2f,1.8f,1.2f,0.6f,0.2f,-0.5f,-1.0f,-0.8f,-0.4f,0.0f,0.0f,0.2f,0.4f,0.5f,0.2f,0.0f,-0.4f,-0.8f,-1.0f,-0.8f,-0.5f,-0.5f},
            // 2: CLARITY
            {-1.5f,-1.0f,-0.5f,0.0f,-0.6f,-1.2f,-1.8f,-1.5f,-0.8f,0.0f,0.8f,1.4f,2.0f,2.5f,2.8f,3.0f,2.6f,2.2f,2.0f,2.4f,2.8f,3.2f,3.0f,2.5f},
            // 3: POCKET (carves vocal presence pocket cleanly for mix tracks)
            {1.0f,1.4f,1.2f,0.6f,0.0f,0.0f,-0.5f,-1.2f,-2.0f,-3.0f,-3.8f,-4.2f,-4.5f,-4.0f,-3.2f,-2.2f,-1.2f,-0.4f,0.4f,0.8f,1.2f,1.5f,1.4f,1.0f}
        }};
        if (idx < 0 || idx >= 4) return;
        for (int b = 0; b < 24; ++b) {
            const int pid = 17 + b;
            begin_param(pid);
            push_param(pid, std::clamp(static_cast<double>(kCurves[static_cast<size_t>(idx)][static_cast<size_t>(b)]) / 24.0 + 0.5, 0.0, 1.0));
            end_param(pid);
        }
    }

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }
        auto* self = reinterpret_cast<OnionEQEditor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

        switch (msg) {
            case WM_TIMER:
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            case WM_PAINT:
                self->paint();
                return 0;
            case WM_MOUSEMOVE: {
                const int x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
                const int y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
                if (self->drag_kind_ == HitKind::EqCanvas) {
                    self->handle_eq_canvas_drag(self->drag_canvas_, x, y);
                } else if (self->drag_id_ >= 0 && (self->drag_kind_ == HitKind::Knob || self->drag_kind_ == HitKind::Slider)) {
                    const double sens = (wParam & MK_SHIFT) ? 0.0012 : 0.0045;
                    const double delta = (self->drag_kind_ == HitKind::Knob)
                        ? static_cast<double>(self->drag_start_y_ - y) * sens
                        : static_cast<double>(x - self->drag_start_x_) / std::max(24.0, static_cast<double>(self->drag_rect_w_ - 16));
                    if (self->drag_kind_ == HitKind::Slider) {
                        const double v = (wParam & MK_SHIFT)
                            ? self->drag_start_val_ + static_cast<double>(x - self->drag_start_x_) * sens
                            : self->drag_start_val_ + delta;
                        self->push_param(self->drag_id_, v);
                    } else {
                        self->push_param(self->drag_id_, self->drag_start_val_ + delta);
                    }
                } else {
                    const HitTarget* h = self->find_hit(x, y);
                    int nh = h ? h->param_id : -1;
                    if (h && h->kind == HitKind::EqCanvas) {
                        const float col_w = static_cast<float>(std::max<LONG>(24, h->rect.right - h->rect.left) - 16) / 24.0f;
                        const int b = std::clamp(static_cast<int>((x - h->rect.left - 8) / col_w), 0, 23);
                        nh = 17 + b;
                    }
                    if (nh != self->hover_id_) {
                        self->hover_id_ = nh;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
                return 0;
            }
            case WM_LBUTTONDOWN: {
                const int x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
                const int y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
                if (const HitTarget* h = self->find_hit(x, y)) {
                    if (h->kind == HitKind::Toggle && h->param_id >= 0) {
                        self->begin_param(h->param_id);
                        self->push_param(h->param_id, self->param_val(h->param_id) >= 0.5 ? 0.0 : 1.0);
                        self->end_param(h->param_id);
                    } else if (h->kind == HitKind::EqPreset) {
                        self->apply_eq_preset(h->extra);
                    } else if ((h->kind == HitKind::StepPrev || h->kind == HitKind::StepNext) && h->param_id >= 0 && h->extra > 0) {
                        const int steps = h->extra;
                        int cur = std::clamp(static_cast<int>(std::lround(self->param_val(h->param_id) * steps)), 0, steps);
                        cur = (h->kind == HitKind::StepPrev) ? std::max(0, cur - 1) : std::min(steps, cur + 1);
                        self->begin_param(h->param_id);
                        self->push_param(h->param_id, static_cast<double>(cur) / static_cast<double>(steps));
                        self->end_param(h->param_id);
                    } else if (h->kind == HitKind::EqCanvas) {
                        SetCapture(hwnd);
                        self->drag_kind_ = HitKind::EqCanvas;
                        self->drag_canvas_ = *h;
                        self->drag_band_ = -1;
                        self->handle_eq_canvas_drag(*h, x, y);
                    } else if ((h->kind == HitKind::Knob || h->kind == HitKind::Slider) && h->param_id >= 0) {
                        SetCapture(hwnd);
                        self->drag_kind_ = h->kind;
                        self->drag_id_ = h->param_id;
                        self->drag_start_x_ = x;
                        self->drag_start_y_ = y;
                        self->drag_rect_w_ = h->rect.right - h->rect.left;
                        self->drag_start_val_ = self->param_val(h->param_id);
                        self->begin_param(h->param_id);
                    }
                }
                return 0;
            }
            case WM_LBUTTONUP: {
                if (self->drag_kind_ != HitKind::None) {
                    if (self->drag_kind_ == HitKind::EqCanvas && self->drag_band_ >= 17) {
                        self->end_param(self->drag_band_);
                    } else if (self->drag_id_ >= 0) {
                        self->end_param(self->drag_id_);
                    }
                    self->drag_kind_ = HitKind::None;
                    self->drag_id_ = -1;
                    self->drag_band_ = -1;
                    ReleaseCapture();
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            case WM_RBUTTONDOWN:
            case WM_LBUTTONDBLCLK: {
                const int x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
                const int y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
                if (const HitTarget* h = self->find_hit(x, y)) {
                    int reset_id = h->param_id;
                    if (h->kind == HitKind::EqCanvas) {
                        const float col_w = static_cast<float>(std::max<LONG>(24, h->rect.right - h->rect.left) - 16) / 24.0f;
                        const int b = std::clamp(static_cast<int>((x - h->rect.left - 8) / col_w), 0, 23);
                        reset_id = 17 + b;
                    }
                    if (reset_id >= 0 && reset_id < kParamCount) {
                        self->begin_param(reset_id);
                        self->push_param(reset_id, kDefaultValues[static_cast<size_t>(reset_id)]);
                        self->end_param(reset_id);
                    }
                }
                return 0;
            }
            case WM_MOUSEWHEEL: {
                if (self->hover_id_ >= 0 && self->hover_id_ < kParamCount) {
                    const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                    const double step = ((GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT) ? 0.008 : 0.03) * (delta > 0 ? 1.0 : -1.0);
                    self->begin_param(self->hover_id_);
                    self->push_param(self->hover_id_, self->param_val(self->hover_id_) + step);
                    self->end_param(self->hover_id_);
                    return 0;
                }
                break;
            }
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    std::atomic<uint32> ref_;
    IEditController* controller_ = nullptr;
    IComponentHandler** handler_slot_ = nullptr;
    IPlugFrame* frame_ = nullptr;
    HWND hwnd_ = nullptr;
    std::shared_ptr<TelemetrySlot> telemetry_slot_;
    monkeys_ear::EcosystemBusClient eco_bus_{};
    std::unique_ptr<Gdiplus::Bitmap> plate_bmp_;
    std::unique_ptr<Gdiplus::Bitmap> crest_bmp_;
    HFONT title_font_ = nullptr;
    HFONT body_font_ = nullptr;
    HFONT small_font_ = nullptr;
    HFONT micro_font_ = nullptr;
    std::vector<HitTarget> hits_{};
    int hover_id_ = -1;
    HitKind drag_kind_ = HitKind::None;
    HitTarget drag_canvas_{};
    int drag_id_ = -1;
    int drag_band_ = -1;
    int drag_start_x_ = 0;
    int drag_start_y_ = 0;
    int drag_rect_w_ = 0;
    double drag_start_val_ = 0.0;
};
#endif

class OnionController final : public IEditController, public IConnectionPoint {
public:
    OnionController()
        : ref_(1),
          state_(std::make_shared<OnionState>()),
          telemetry_slot_(std::make_shared<TelemetrySlot>()) {}

    tresult SMTG_STDCALL queryInterface(const TUID iid, void** obj) override {
        if (!obj) return kInvalidArgument;
        if (!std::memcmp(iid, FUnknown_iid, sizeof(TUID)) ||
            !std::memcmp(iid, IPluginBase_iid, sizeof(TUID)) ||
            !std::memcmp(iid, IEditController_iid, sizeof(TUID))) {
            *obj = static_cast<IEditController*>(this);
        } else if (!std::memcmp(iid, IConnectionPoint_iid, sizeof(TUID))) {
            *obj = static_cast<IConnectionPoint*>(this);
        } else {
            *obj = nullptr;
            return kResultFalse;
        }
        addRef();
        return kResultOk;
    }
    uint32 SMTG_STDCALL addRef() override { return ++ref_; }
    uint32 SMTG_STDCALL release() override {
        uint32 n = --ref_;
        if (!n) delete this;
        return n;
    }

    tresult SMTG_STDCALL initialize(FUnknown*) override { return kResultOk; }
    tresult SMTG_STDCALL terminate() override {
        if (telemetry_slot_) telemetry_slot_->set(nullptr);
        return kResultOk;
    }
    tresult SMTG_STDCALL setComponentState(IBStream* s) override { return read_state(s, state_); }
    tresult SMTG_STDCALL setState(IBStream* s) override { return read_state(s, state_); }
    tresult SMTG_STDCALL getState(IBStream* s) override { return write_state(s, state_); }
    int32 SMTG_STDCALL getParameterCount() override { return kParamCount; }

    tresult SMTG_STDCALL getParameterInfo(int32 i, ParameterInfo& info) override {
        if (i < 0 || i >= kParamCount) return kInvalidArgument;
        static constexpr const char* kFixedNames[17] = {
            "ONION: Bypass", "ONION: Dynamic Carve", "ONION: Intent", "ONION: Amount",
            "ONION: Sensitivity", "ONION: Low Protect", "ONION: Transient Protect",
            "ONION: Spectrum Tilt", "ONION: Carve Q Focus", "ONION: Air Shimmer",
            "ONION: Reference Source", "ONION: Eco Link Mode", "ONION: Eco Channel",
            "ONION: Peer Mask Weight", "ONION: Stereo Mode", "ONION: Dry/Wet Mix",
            "ONION: Output Trim"
        };
        char name_buf[128]{};
        const char* title = nullptr;
        if (i < 17) {
            title = kFixedNames[i];
        } else if (i == 41) {
            title = "ONION: Harmonic F0 Guard";
        } else {
            const int b = i - 17;
            std::snprintf(name_buf, sizeof(name_buf), "ONION: ERB Band %d (%.0f Hz)",
                          b + 1, monkeys_ear::OnionSkinEQ::kBandCentersHz[static_cast<size_t>(b)]);
            title = name_buf;
        }
        info.id = static_cast<ParamID>(i);
        info.unitId = 0;
        info.stepCount = 0;
        if (i == 0 || i == 1) info.stepCount = 1;
        else if (i == 2 || i == 14) info.stepCount = 2;
        else if (i == 10 || i == 11) info.stepCount = 3;
        else if (i == 12) info.stepCount = 7;

        info.flags = kCanAutomate | (i == 0 ? kIsBypass : 0);
        info.defaultNormalizedValue = kDefaultValues[static_cast<size_t>(i)];
        copy16(info.title, title);
        copy16(info.shortTitle, title);
        const char* unit = "";
        if (i == 7 || i == 9 || (i >= 16 && i <= 40)) unit = "dB";
        else if (i == 3 || i == 4 || i == 5 || i == 6 || i == 13 || i == 15 || i == 41) unit = "%";
        copy16(info.units, unit);
        return kResultOk;
    }

    tresult SMTG_STDCALL getParamStringByValue(ParamID id, ParamValue value, char16 string[128]) override {
        if (id >= kParamCount) return kInvalidArgument;
        char text[64]{};
        const double v = std::clamp(value, 0.0, 1.0);
        if (id == 0) {
            std::snprintf(text, sizeof(text), "%s", v >= 0.5 ? "Bypass" : "Active");
        } else if (id == 1) {
            std::snprintf(text, sizeof(text), "%s", v >= 0.5 ? "On" : "Off");
        } else if (id == 2) {
            constexpr const char* names[3] = {"Equal", "Yield", "Preserve"};
            std::snprintf(text, sizeof(text), "%s", names[std::clamp(static_cast<int>(std::lround(v * 2.0)), 0, 2)]);
        } else if (id == 7 || id == 9) {
            std::snprintf(text, sizeof(text), "%+.1f dB", (v - 0.5) * 12.0);
        } else if (id == 8) {
            std::snprintf(text, sizeof(text), "Q %.2f", 0.85 + v * 3.65);
        } else if (id == 10) {
            constexpr const char* refs[4] = {"Auto SC+Eco", "Sidechain 3/4", "Eco Peer", "Self De-Harsh"};
            std::snprintf(text, sizeof(text), "%s", refs[std::clamp(static_cast<int>(std::lround(v * 3.0)), 0, 3)]);
        } else if (id == 11) {
            constexpr const char* links[4] = {"Off", "Publish", "Follow", "Bi-Dir"};
            std::snprintf(text, sizeof(text), "%s", links[std::clamp(static_cast<int>(std::lround(v * 3.0)), 0, 3)]);
        } else if (id == 12) {
            std::snprintf(text, sizeof(text), "Ch %d", std::clamp(static_cast<int>(std::lround(v * 7.0)), 0, 7) + 1);
        } else if (id == 14) {
            constexpr const char* st[3] = {"Stereo L/R", "Mid Only", "Side Only"};
            std::snprintf(text, sizeof(text), "%s", st[std::clamp(static_cast<int>(std::lround(v * 2.0)), 0, 2)]);
        } else if (id == 16 || (id >= 17 && id <= 40)) {
            std::snprintf(text, sizeof(text), "%+.1f dB", (v - 0.5) * 24.0);
        } else {
            std::snprintf(text, sizeof(text), "%.0f %%", v * 100.0);
        }
        copy16(string, text);
        return kResultOk;
    }

    tresult SMTG_STDCALL getParamValueByString(ParamID, const char16*, ParamValue& value) override {
        value = 0.5;
        return kResultOk;
    }
    ParamValue SMTG_STDCALL normalizedParamToPlain(ParamID, ParamValue v) override { return v; }
    ParamValue SMTG_STDCALL plainParamToNormalized(ParamID, ParamValue v) override { return v; }
    ParamValue SMTG_STDCALL getParamNormalized(ParamID id) override {
        return id < kParamCount ? state_->values[id].load() : 0.0;
    }
    tresult SMTG_STDCALL setParamNormalized(ParamID id, ParamValue value) override {
        if (id >= kParamCount || !normalized_value_is_finite(value)) return kInvalidArgument;
        state_->values[id].store(static_cast<float>(std::clamp(value, 0.0, 1.0)));
        state_->dirty[id].store(true);
        return kResultOk;
    }
    tresult SMTG_STDCALL setComponentHandler(IComponentHandler* handler) override {
        handler_ = handler;
        return kResultOk;
    }
    IPlugView* SMTG_STDCALL createView(const char8* name) override {
#if defined(_WIN32)
        if (!name || std::strcmp(name, "editor") == 0) {
            return new OnionEQEditor(this, &handler_, telemetry_slot_);
        }
#endif
        return nullptr;
    }

    // IConnectionPoint
    tresult SMTG_STDCALL connect(IConnectionPoint* other) override {
        if (!other) return kInvalidArgument;
        IOnionTelemetryProvider* provider = nullptr;
        if (other->queryInterface(kOnionTelemetryProvider_iid, reinterpret_cast<void**>(&provider)) == kResultOk && provider) {
            if (telemetry_slot_) telemetry_slot_->set(provider->onion_telemetry());
            provider->release_provider_ref();
        }
        return kResultOk;
    }
    tresult SMTG_STDCALL disconnect(IConnectionPoint*) override {
        if (telemetry_slot_) telemetry_slot_->set(nullptr);
        return kResultOk;
    }
    tresult SMTG_STDCALL notify(IMessage*) override { return kResultOk; }

private:
    std::atomic<uint32> ref_;
    std::shared_ptr<OnionState> state_;
    std::shared_ptr<TelemetrySlot> telemetry_slot_;
    IComponentHandler* handler_ = nullptr;
};

class OnionFactory final : public IPluginFactory2 {
public:
    OnionFactory() : ref_(1) {}

    tresult SMTG_STDCALL queryInterface(const TUID iid, void** obj) override {
        if (!obj) return kInvalidArgument;
        if (!std::memcmp(iid, FUnknown_iid, sizeof(TUID)) ||
            !std::memcmp(iid, IPluginFactory_iid, sizeof(TUID)) ||
            !std::memcmp(iid, IPluginFactory2_iid, sizeof(TUID))) {
            *obj = static_cast<IPluginFactory2*>(this);
            addRef();
            return kResultOk;
        }
        *obj = nullptr;
        return kResultFalse;
    }
    uint32 SMTG_STDCALL addRef() override { return ++ref_; }
    uint32 SMTG_STDCALL release() override {
        uint32 n = --ref_;
        if (!n) delete this;
        return n;
    }
    tresult SMTG_STDCALL getFactoryInfo(PFactoryInfo* info) override {
        if (!info) return kInvalidArgument;
        std::strncpy(info->vendor, "Ultramonkeydog Studios", sizeof(info->vendor) - 1);
        std::strncpy(info->url, "https://ultramonkeydog-studios.vercel.app", sizeof(info->url) - 1);
        info->flags = 0;
        return kResultOk;
    }
    int32 SMTG_STDCALL countClasses() override { return 2; }
    tresult SMTG_STDCALL getClassInfo(int32 i, PClassInfo* info) override {
        return class_info(i, info, static_cast<PClassInfo2*>(nullptr));
    }
    tresult SMTG_STDCALL getClassInfo2(int32 i, PClassInfo2* info) override {
        return class_info(i, static_cast<PClassInfo*>(nullptr), info);
    }
    tresult SMTG_STDCALL createInstance(const TUID cid, const TUID iid, void** obj) override {
        if (!std::memcmp(cid, kOnionComponentCID, sizeof(TUID))) {
            auto* p = new OnionComponent();
            auto r = p->queryInterface(iid, obj);
            p->release();
            return r;
        }
        if (!std::memcmp(cid, kOnionControllerCID, sizeof(TUID))) {
            auto* p = new OnionController();
            auto r = p->queryInterface(iid, obj);
            p->release();
            return r;
        }
        return kInvalidArgument;
    }

private:
    template <class A, class B>
    tresult class_info(int32 i, A* a, B* b) {
        if (i < 0 || i > 1) return kInvalidArgument;
        auto cid = i == 0 ? kOnionComponentCID : kOnionControllerCID;
        const char* name = i == 0 ? "Monkey's Ear Onion Skin EQ" : "Monkey's Ear Onion Skin EQ Controller";
        if (a) {
            std::memcpy(a->cid, cid, sizeof(TUID));
            a->cardinality = 0x7fffffff;
            std::strncpy(a->category, i == 0 ? "Audio Module Class" : "Component Controller Class", sizeof(a->category) - 1);
            std::strncpy(a->name, name, sizeof(a->name) - 1);
        }
        if (b) {
            std::memcpy(b->cid, cid, sizeof(TUID));
            b->cardinality = 0x7fffffff;
            std::strncpy(b->category, i == 0 ? "Audio Module Class" : "Component Controller Class", sizeof(b->category) - 1);
            std::strncpy(b->name, name, sizeof(b->name) - 1);
            b->classFlags = 0;
            std::strncpy(b->subCategories, i == 0 ? "Fx|EQ" : "", sizeof(b->subCategories) - 1);
            std::strncpy(b->vendor, "Ultramonkeydog Studios", sizeof(b->vendor) - 1);
            std::strncpy(b->version, monkeys_ear::kOnionEqProductVersion, sizeof(b->version) - 1);
            std::strncpy(b->sdkVersion, "VST 3.7.0", sizeof(b->sdkVersion) - 1);
        }
        return kResultOk;
    }

    std::atomic<uint32> ref_;
};

} // namespace

extern "C" {
SMTG_EXPORT_SYMBOL IPluginFactory* SMTG_STDCALL GetPluginFactory() { return new OnionFactory(); }
#if defined(_WIN32)
BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
#endif
}
