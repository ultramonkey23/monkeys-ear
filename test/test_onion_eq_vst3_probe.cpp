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
#include "monkeys_ear/onion_eq_product_version.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
using GetPluginFactoryProc = IPluginFactory* (SMTG_STDCALL*)();

#define CHECK_STAGE(condition, stage) do { if (!(condition)) { std::cerr << "[FAIL] " << stage << "\n"; return 1; } else { std::cout << "[PASS] " << stage << "\n"; } } while (0)

namespace {

class MemoryStream final : public IBStream {
public:
    MemoryStream() = default;
    explicit MemoryStream(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}
    tresult SMTG_STDCALL queryInterface(const TUID iid, void** obj) override {
        if (!obj) return kInvalidArgument;
        if (!std::memcmp(iid, FUnknown_iid, sizeof(TUID)) || !std::memcmp(iid, IBStream_iid, sizeof(TUID))) {
            *obj = static_cast<IBStream*>(this);
            return kResultOk;
        }
        *obj = nullptr;
        return kResultFalse;
    }
    uint32 SMTG_STDCALL addRef() override { return 1; }
    uint32 SMTG_STDCALL release() override { return 1; }
    tresult SMTG_STDCALL read(void* buffer, int32 numBytes, int32* numBytesRead) override {
        if (numBytes < 0 || (!buffer && numBytes > 0)) return kInvalidArgument;
        const size_t requested = static_cast<size_t>(numBytes);
        const size_t available = position_ <= bytes_.size() ? bytes_.size() - position_ : 0;
        const size_t count = std::min(requested, available);
        if (count) std::memcpy(buffer, bytes_.data() + position_, count);
        position_ += count;
        if (numBytesRead) *numBytesRead = static_cast<int32>(count);
        return count == requested ? kResultOk : kResultFalse;
    }
    tresult SMTG_STDCALL write(void* buffer, int32 numBytes, int32* numBytesWritten) override {
        if (numBytes < 0 || (!buffer && numBytes > 0)) return kInvalidArgument;
        const size_t count = static_cast<size_t>(numBytes);
        if (position_ + count > bytes_.size()) bytes_.resize(position_ + count);
        if (count) std::memcpy(bytes_.data() + position_, buffer, count);
        position_ += count;
        if (numBytesWritten) *numBytesWritten = numBytes;
        return kResultOk;
    }
    tresult SMTG_STDCALL seek(int64 pos, int32 mode, int64* result) override {
        int64 base = 0;
        if (mode == kIBSeekCur) base = static_cast<int64>(position_);
        else if (mode == kIBSeekEnd) base = static_cast<int64>(bytes_.size());
        else if (mode != kIBSeekSet) return kInvalidArgument;
        const int64 next = base + pos;
        if (next < 0 || static_cast<uint64>(next) > bytes_.size()) return kInvalidArgument;
        position_ = static_cast<size_t>(next);
        if (result) *result = next;
        return kResultOk;
    }
    tresult SMTG_STDCALL tell(int64* pos) override {
        if (!pos) return kInvalidArgument;
        *pos = static_cast<int64>(position_);
        return kResultOk;
    }
    const std::vector<std::uint8_t>& bytes() const { return bytes_; }
private:
    std::vector<std::uint8_t> bytes_{};
    size_t position_{0};
};

class ParamQueue final : public IParamValueQueue {
public:
    explicit ParamQueue(ParamID id) : id_(id) {}
    void add(int32 offset, ParamValue val) { points_.push_back({offset, val}); }
    tresult SMTG_STDCALL queryInterface(const TUID, void** obj) override { if (obj) *obj = nullptr; return kResultFalse; }
    uint32 SMTG_STDCALL addRef() override { return 1; }
    uint32 SMTG_STDCALL release() override { return 1; }
    ParamID SMTG_STDCALL getParameterId() override { return id_; }
    int32 SMTG_STDCALL getPointCount() override { return static_cast<int32>(points_.size()); }
    tresult SMTG_STDCALL getPoint(int32 index, int32& sampleOffset, ParamValue& value) override {
        if (index < 0 || static_cast<size_t>(index) >= points_.size()) return kInvalidArgument;
        sampleOffset = points_[static_cast<size_t>(index)].first;
        value = points_[static_cast<size_t>(index)].second;
        return kResultOk;
    }
    tresult SMTG_STDCALL addPoint(int32, ParamValue, int32&) override { return kNotImplemented; }
private:
    ParamID id_;
    std::vector<std::pair<int32, ParamValue>> points_;
};

class MultiParameterChanges final : public IParameterChanges {
public:
    ParamQueue& add_queue(ParamID id) {
        queues_.emplace_back(id);
        return queues_.back();
    }
    tresult SMTG_STDCALL queryInterface(const TUID, void** obj) override { if (obj) *obj = nullptr; return kResultFalse; }
    uint32 SMTG_STDCALL addRef() override { return 1; }
    uint32 SMTG_STDCALL release() override { return 1; }
    int32 SMTG_STDCALL getParameterCount() override { return static_cast<int32>(queues_.size()); }
    IParamValueQueue* SMTG_STDCALL getParameterData(int32 index) override {
        if (index < 0 || static_cast<size_t>(index) >= queues_.size()) return nullptr;
        return &queues_[static_cast<size_t>(index)];
    }
    IParamValueQueue* SMTG_STDCALL addParameterData(const ParamID&, int32&) override { return nullptr; }
private:
    std::vector<ParamQueue> queues_;
};

float render_block(IAudioProcessor* proc, int32 n, float freq_hz, float sc_freq_hz,
                   IParameterChanges* changes = nullptr,
                   std::vector<float>* out_buf_l = nullptr) {
    std::vector<float> in_l(static_cast<size_t>(n), 0.0f), in_r(static_cast<size_t>(n), 0.0f);
    std::vector<float> sc_l(static_cast<size_t>(n), 0.0f), sc_r(static_cast<size_t>(n), 0.0f);
    std::vector<float> out_l(static_cast<size_t>(n), 0.0f), out_r(static_cast<size_t>(n), 0.0f);
    constexpr float kTwoPi = 6.283185307179586f;
    for (int32 i = 0; i < n; ++i) {
        const float s = 0.25f * std::sin(kTwoPi * freq_hz * static_cast<float>(i) / 48000.0f);
        in_l[static_cast<size_t>(i)] = s;
        in_r[static_cast<size_t>(i)] = s;
        if (sc_freq_hz > 0.0f) {
            const float sc = 0.65f * std::sin(kTwoPi * sc_freq_hz * static_cast<float>(i) / 48000.0f);
            sc_l[static_cast<size_t>(i)] = sc;
            sc_r[static_cast<size_t>(i)] = sc;
        }
    }
    float* in_ptrs[2] = {in_l.data(), in_r.data()};
    float* sc_ptrs[2] = {sc_l.data(), sc_r.data()};
    float* out_ptrs[2] = {out_l.data(), out_r.data()};

    AudioBusBuffers ins[2]{};
    ins[0].numChannels = 2;
    ins[0].channelBuffers32 = in_ptrs;
    ins[1].numChannels = 2;
    ins[1].channelBuffers32 = sc_ptrs;

    AudioBusBuffers out{};
    out.numChannels = 2;
    out.channelBuffers32 = out_ptrs;

    ProcessData data{};
    data.numSamples = n;
    data.numInputs = sc_freq_hz > 0.0f ? 2 : 1;
    data.numOutputs = 1;
    data.inputs = ins;
    data.outputs = &out;
    data.inputParameterChanges = changes;

    if (proc->process(data) != kResultOk) return -1.0f;

    double sum_sq = 0.0;
    for (int32 i = 0; i < n; ++i) {
        if (!std::isfinite(out_l[static_cast<size_t>(i)]) || !std::isfinite(out_r[static_cast<size_t>(i)])) return -1.0f;
        sum_sq += static_cast<double>(out_l[static_cast<size_t>(i)]) * out_l[static_cast<size_t>(i)];
    }
    if (out_buf_l) *out_buf_l = std::move(out_l);
    return static_cast<float>(std::sqrt(sum_sq / static_cast<double>(n)));
}

} // namespace

int main() {
    char exe[MAX_PATH]{};
    const DWORD len = GetModuleFileNameA(nullptr, exe, MAX_PATH);
    CHECK_STAGE(len > 0 && len < MAX_PATH, "resolve probe directory");
    std::string path(exe);
    const size_t sep = path.find_last_of("\\/");
    CHECK_STAGE(sep != std::string::npos, "find separator");
    path = path.substr(0, sep + 1) + "monkeys_ear_onion_eq.vst3";

    HMODULE mod = LoadLibraryA(path.c_str());
    CHECK_STAGE(mod != nullptr, "load monkeys_ear_onion_eq.vst3");
    auto factory_fn = reinterpret_cast<GetPluginFactoryProc>(reinterpret_cast<void(*)()>(GetProcAddress(mod, "GetPluginFactory")));
    CHECK_STAGE(factory_fn != nullptr, "find GetPluginFactory");
    IPluginFactory* factory = factory_fn();
    CHECK_STAGE(factory != nullptr, "create factory");
    IPluginFactory2* factory2 = nullptr;
    CHECK_STAGE(factory->queryInterface(IPluginFactory2_iid, reinterpret_cast<void**>(&factory2)) == kResultOk && factory2 != nullptr,
                "query IPluginFactory2");
    CHECK_STAGE(factory2->countClasses() == 2, "factory exports component and controller");

    PClassInfo2 comp_info{}, ctrl_info{};
    CHECK_STAGE(factory2->getClassInfo2(0, &comp_info) == kResultOk &&
                std::string(comp_info.name) == "Monkey's Ear Onion Skin EQ" &&
                std::string(comp_info.subCategories) == "Fx|EQ" &&
                std::string(comp_info.version) == monkeys_ear::kOnionEqProductVersion,
                "Onion Skin EQ component class metadata");
    CHECK_STAGE(factory2->getClassInfo2(1, &ctrl_info) == kResultOk, "read controller class metadata");

    IComponent* comp = nullptr;
    CHECK_STAGE(factory2->createInstance(comp_info.cid, IComponent_iid, reinterpret_cast<void**>(&comp)) == kResultOk && comp != nullptr,
                "create Onion Skin EQ component");
    CHECK_STAGE(comp->getBusCount(kAudio, kInput) == 2 && comp->getBusCount(kAudio, kOutput) == 1,
                "Main In + Sidechain In + Main Out bus topology");

    BusInfo sc_bus{};
    CHECK_STAGE(comp->getBusInfo(kAudio, kInput, 1, sc_bus) == kResultOk && sc_bus.busType == kAux && sc_bus.channelCount == 2,
                "Sidechain bus is auxiliary stereo");

    IAudioProcessor* proc = nullptr;
    CHECK_STAGE(comp->queryInterface(IAudioProcessor_iid, reinterpret_cast<void**>(&proc)) == kResultOk && proc != nullptr,
                "component exposes IAudioProcessor");

    SpeakerArrangement ins[2] = {SpeakerArr::kStereo, SpeakerArr::kStereo};
    SpeakerArrangement outs[1] = {SpeakerArr::kStereo};
    CHECK_STAGE(proc->setBusArrangements(ins, 2, outs, 1) == kResultOk, "accept stereo + stereo sidechain bus arrangement");
    CHECK_STAGE(proc->setBusArrangements(ins, 1, outs, 1) == kResultOk, "accept stereo main-only bus arrangement");
    CHECK_STAGE(proc->getLatencySamples() == 0, "zero plugin latency");

    ProcessSetup setup{};
    setup.sampleRate = 48000.0;
    setup.maxSamplesPerBlock = 2048;
    CHECK_STAGE(proc->setupProcessing(setup) == kResultOk, "setupProcessing at 48 kHz");

    IEditController* ctrl = nullptr;
    CHECK_STAGE(factory2->createInstance(ctrl_info.cid, IEditController_iid, reinterpret_cast<void**>(&ctrl)) == kResultOk && ctrl != nullptr,
                "create Onion Skin EQ controller");
    CHECK_STAGE(ctrl->getParameterCount() == 42, "controller exposes all 42 parameters");
    {
        ParameterInfo info41{};
        CHECK_STAGE(ctrl->getParameterInfo(41, info41) == kResultOk &&
                    std::string(reinterpret_cast<const char*>(&info41.title[0])).empty() == false,
                    "read parameter 41 info");
        std::string title41;
        for (int i = 0; i < 127 && info41.title[i]; ++i) title41 += static_cast<char>(info41.title[i]);
        CHECK_STAGE(title41 == "ONION: Harmonic F0 Guard", "parameter 41 title is ONION: Harmonic F0 Guard");
        char16 str41[128]{};
        CHECK_STAGE(ctrl->getParamStringByValue(41, 0.75, str41) == kResultOk && str41[0] == u'7' && str41[1] == u'5',
                    "parameter 41 formats percentage display string");
    }

    IConnectionPoint* comp_cp = nullptr;
    IConnectionPoint* ctrl_cp = nullptr;
    CHECK_STAGE(comp->queryInterface(IConnectionPoint_iid, reinterpret_cast<void**>(&comp_cp)) == kResultOk &&
                ctrl->queryInterface(IConnectionPoint_iid, reinterpret_cast<void**>(&ctrl_cp)) == kResultOk,
                "component and controller expose IConnectionPoint");
    CHECK_STAGE(comp_cp->connect(ctrl_cp) == kResultOk && ctrl_cp->connect(comp_cp) == kResultOk,
                "connect telemetry link via IConnectionPoint");

    CHECK_STAGE(ctrl->setParamNormalized(3, std::numeric_limits<double>::quiet_NaN()) == kInvalidArgument,
                "reject NaN normalized parameter");

    // Verify zero-sample parameter flush updates component state
    MultiParameterChanges flush_chg;
    flush_chg.add_queue(15).add(0, 0.42);
    flush_chg.add_queue(41).add(0, 0.85);
    ProcessData flush_pd{};
    flush_pd.numSamples = 0;
    flush_pd.inputParameterChanges = &flush_chg;
    CHECK_STAGE(proc->process(flush_pd) == kResultOk, "accept zero-sample parameter flush");

    MemoryStream saved;
    CHECK_STAGE(comp->getState(&saved) == kResultOk && saved.bytes().size() == 12u + 42u * sizeof(float),
                "serialize MEOQ v2 component state");
    uint32 magic = 0, ver = 0, cnt = 0;
    std::memcpy(&magic, saved.bytes().data(), 4);
    std::memcpy(&ver, saved.bytes().data() + 4, 4);
    std::memcpy(&cnt, saved.bytes().data() + 8, 4);
    CHECK_STAGE(magic == 0x4D454F51u && ver == 2u && cnt == 42u, "verify MEOQ v2 state header");

    // Verify MEOQ v1 (41 params) -> v2 (42 params) backward-compatible stream loading and v2 round-trip
    {
        MemoryStream v1_stream;
        uint32 v1_magic = 0x4D454F51u, v1_ver = 1u, v1_cnt = 41u;
        v1_stream.write(&v1_magic, 4, nullptr);
        v1_stream.write(&v1_ver, 4, nullptr);
        v1_stream.write(&v1_cnt, 4, nullptr);
        for (uint32 i = 0; i < 41u; ++i) {
            float val = (i == 15u) ? 0.80f : (i == 17u ? 0.60f : 0.50f);
            v1_stream.write(&val, 4, nullptr);
        }
        v1_stream.seek(0, kIBSeekSet, nullptr);
        CHECK_STAGE(comp->setState(&v1_stream) == kResultOk, "load MEOQ v1 (41-param) state into v2 component");
        MemoryStream migrated_v2;
        CHECK_STAGE(comp->getState(&migrated_v2) == kResultOk &&
                    migrated_v2.bytes().size() == 12u + 42u * sizeof(float),
                    "MEOQ v1 migrates cleanly to v2 42-param envelope");
        uint32 m_ver = 0, m_cnt = 0;
        float m_mix = 0.0f, m_guard = 99.0f;
        std::memcpy(&m_ver, migrated_v2.bytes().data() + 4, 4);
        std::memcpy(&m_cnt, migrated_v2.bytes().data() + 8, 4);
        std::memcpy(&m_mix, migrated_v2.bytes().data() + 12u + 15u * 4u, 4);
        std::memcpy(&m_guard, migrated_v2.bytes().data() + 12u + 41u * 4u, 4);
        CHECK_STAGE(m_ver == 2u && m_cnt == 42u && std::abs(m_mix - 0.80f) < 1e-6f && m_guard == 0.0f,
                    "MEOQ v1->v2 migration preserves params and defaults param 41 to 0.0");

        // v2 round-trip restoring param 41 = 0.85
        MemoryStream restore_v2(saved.bytes());
        CHECK_STAGE(comp->setState(&restore_v2) == kResultOk, "restore MEOQ v2 component state");
        MemoryStream ctrl_restore_v2(saved.bytes());
        CHECK_STAGE(ctrl->setComponentState(&ctrl_restore_v2) == kResultOk, "sync controller from MEOQ v2 state");
        CHECK_STAGE(std::abs(ctrl->getParamNormalized(41) - 0.85) < 1e-5,
                    "MEOQ v2 round-trip preserves Harmonic F0 Guard");
    }

    // Reset mix to 1.0 and test flat vs surgical boost vs bypass vs sidechain carve
    {
        MultiParameterChanges restore;
        restore.add_queue(15).add(0, 1.0); // Mix 100%
        restore.add_queue(1).add(0, 0.0);  // Dynamic Off first for exact surgical check
        restore.add_queue(41).add(0, 0.0); // F0 guard default off
        render_block(proc, 1024, 878.0f, 0.0f, &restore);
    }
    const float rms_flat = render_block(proc, 1024, 878.0f, 0.0f);

    MultiParameterChanges boost_878;
    boost_878.add_queue(17 + 11).add(0, 0.90); // ERB band 12 (878 Hz) boost ~+9.6 dB
    const float rms_boost = render_block(proc, 1024, 878.0f, 0.0f, &boost_878);
    CHECK_STAGE(rms_boost > rms_flat * 1.45f, "24-band ERB surgical boost strongly increases target band energy");

    // Bypass check: returns exact input level cleanly
    MultiParameterChanges bypass_on;
    bypass_on.add_queue(0).add(0, 1.0);
    const float rms_bypass = render_block(proc, 1024, 878.0f, 0.0f, &bypass_on);
    CHECK_STAGE(std::abs(rms_bypass - rms_flat) < 0.02f, "master bypass passes uncolored audio");

    // Sidechain 3/4 Dynamic Carving test: reset band 12 to 0 dB, enable Dynamic Carve + Yield, feed loud 878 Hz sidechain
    MultiParameterChanges sc_setup;
    sc_setup.add_queue(0).add(0, 0.0);       // Active
    sc_setup.add_queue(1).add(0, 1.0);       // Dynamic Carve ON
    sc_setup.add_queue(2).add(0, 0.5);       // Intent: Yield
    sc_setup.add_queue(3).add(0, 1.0);       // Amount 100%
    sc_setup.add_queue(4).add(0, 0.9);       // High sensitivity
    sc_setup.add_queue(10).add(0, 1.0 / 3.0); // Ref Source = Sidechain 3/4
    sc_setup.add_queue(17 + 11).add(0, 0.5); // Flat 0 dB manual
    render_block(proc, 1024, 878.0f, 0.0f, &sc_setup);
    float rms_nosc = 0.0f;
    for (int k = 0; k < 4; ++k) rms_nosc = render_block(proc, 1024, 878.0f, 0.0f);
    float rms_with_sc = 0.0f;
    for (int k = 0; k < 6; ++k) rms_with_sc = render_block(proc, 1024, 878.0f, 878.0f);
    std::cout << "[INFO] sidechain carve 878 Hz: no_sc=" << rms_nosc << " with_sc=" << rms_with_sc << "\n";
    CHECK_STAGE(rms_with_sc < rms_nosc * 0.85f, "auxiliary sidechain 3/4 carves overlapping ERB energy");

    // Continuous sample-to-sample dynamic carve smoothness across 0 dB transition boundary
    {
        comp->setActive(false);
        comp->setActive(true);
        constexpr int32 kBlock = 512;
        constexpr float kTwoPi = 6.283185307179586f;
        std::vector<float> in_l(kBlock), in_r(kBlock), sc_l(kBlock), sc_r(kBlock), out_l(kBlock), out_r(kBlock);
        float* in_p[2] = {in_l.data(), in_r.data()};
        float* sc_p[2] = {sc_l.data(), sc_r.data()};
        float* out_p[2] = {out_l.data(), out_r.data()};
        AudioBusBuffers ins[2]{};
        ins[0].numChannels = 2; ins[0].channelBuffers32 = in_p;
        ins[1].numChannels = 2; ins[1].channelBuffers32 = sc_p;
        AudioBusBuffers outb{}; outb.numChannels = 2; outb.channelBuffers32 = out_p;
        ProcessData pd{}; pd.numSamples = kBlock; pd.numInputs = 2; pd.numOutputs = 1; pd.inputs = ins; pd.outputs = &outb;

        MultiParameterChanges zero_cross_setup;
        zero_cross_setup.add_queue(0).add(0, 0.0);        // Active
        zero_cross_setup.add_queue(1).add(0, 1.0);        // Dynamic ON
        zero_cross_setup.add_queue(2).add(0, 1.0);        // Preserve
        zero_cross_setup.add_queue(3).add(0, 1.0);        // Amount 100%
        zero_cross_setup.add_queue(4).add(0, 0.9);        // Sensitivity
        zero_cross_setup.add_queue(10).add(0, 1.0 / 3.0); // Sidechain 3/4
        zero_cross_setup.add_queue(15).add(0, 1.0);       // Mix 100%
        zero_cross_setup.add_queue(17 + 11).add(0, 0.5);  // Band 12 0 dB manual
        pd.inputParameterChanges = &zero_cross_setup;

        float prev_y = 0.0f;
        float max_step = 0.0f;
        int max_blk = -1, max_idx = -1;
        bool all_finite = true;
        size_t sample_idx = 0;
        for (int blk = 0; blk < 28; ++blk) {
            const bool sc_on = (blk >= 4 && blk < 12);
            MultiParameterChanges step_chg;
            if (blk == 4) {
                step_chg.add_queue(2).add(0, 0.5); // Yield across 0 dB
                pd.inputParameterChanges = &step_chg;
            } else if (blk > 0) {
                pd.inputParameterChanges = nullptr;
            }
            for (int32 i = 0; i < kBlock; ++i, ++sample_idx) {
                const float ph = kTwoPi * 878.0f * static_cast<float>(sample_idx) / 48000.0f;
                in_l[static_cast<size_t>(i)] = in_r[static_cast<size_t>(i)] = 0.25f * std::sin(ph);
                sc_l[static_cast<size_t>(i)] = sc_r[static_cast<size_t>(i)] = sc_on ? (0.65f * std::sin(ph)) : 0.0f;
            }
            if (proc->process(pd) != kResultOk) {
                CHECK_STAGE(false, "process continuous carve zero-crossing block");
            }
            for (int32 i = 0; i < kBlock; ++i) {
                const float y = out_l[static_cast<size_t>(i)];
                if (!std::isfinite(y)) all_finite = false;
                if (blk >= 2) {
                    const float d = std::abs(y - prev_y);
                    if (d > max_step) {
                        max_step = d;
                        max_blk = blk;
                        max_idx = i;
                    }
                }
                prev_y = y;
            }
        }
        CHECK_STAGE(all_finite, "continuous carve outputs are all finite");
        std::cout << "[INFO] max sample-to-sample step across 0 dB carve boundary = " << max_step
                  << " (blk=" << max_blk << ", idx=" << max_idx << ")\n";
        CHECK_STAGE(max_step < 0.08f, "dynamic carve transitions across 0 dB without step discontinuity or click");
    }

    // Harmonic F0 Guard (param 41 = 1.0 vs 0.0) preserves self-pitch fundamental against peer masking
    {
        auto measure_f0_rms = [&](double guard_val) -> float {
            comp->setActive(false);
            comp->setActive(true);
            MultiParameterChanges cfg;
            cfg.add_queue(0).add(0, 0.0);        // Active
            cfg.add_queue(1).add(0, 1.0);        // Dynamic ON
            cfg.add_queue(2).add(0, 0.5);        // Yield
            cfg.add_queue(3).add(0, 1.0);        // Amount 100%
            cfg.add_queue(4).add(0, 0.95);       // High sensitivity
            cfg.add_queue(5).add(0, 0.0);        // Low Protect 0% so only Harmonic F0 Guard protects 255 Hz
            cfg.add_queue(6).add(0, 0.0);        // Transient Protect 0%
            cfg.add_queue(10).add(0, 1.0 / 3.0); // Sidechain 3/4
            cfg.add_queue(15).add(0, 1.0);       // Mix 100%
            cfg.add_queue(41).add(0, guard_val); // Harmonic F0 Guard
            render_block(proc, 1024, 255.0f, 255.0f, &cfg);
            float out_rms = 0.0f;
            for (int k = 0; k < 10; ++k) {
                out_rms = render_block(proc, 1024, 255.0f, 255.0f);
            }
            return out_rms;
        };
        const float rms_unguarded = measure_f0_rms(0.0);
        const float rms_guarded = measure_f0_rms(1.0);
        std::cout << "[INFO] Harmonic F0 Guard @ 255 Hz: unguarded=" << rms_unguarded
                  << " guarded=" << rms_guarded << "\n";
        CHECK_STAGE(rms_guarded > rms_unguarded * 1.04f,
                    "Harmonic F0 Guard (param 41) measurably preserves self-pitch fundamental under masking");
        // Restore Low/Transient protect defaults
        MultiParameterChanges restore_prot;
        restore_prot.add_queue(5).add(0, 0.70);
        restore_prot.add_queue(6).add(0, 0.60);
        restore_prot.add_queue(41).add(0, 0.0);
        render_block(proc, 1024, 878.0f, 0.0f, &restore_prot);
    }

    // Self De-Harsh mode test on isolated single-track tone (no sidechain, no eco peer)
    MultiParameterChanges flat_2550;
    flat_2550.add_queue(1).add(0, 0.0); // Dynamic OFF
    render_block(proc, 1024, 2550.0f, 0.0f, &flat_2550);
    const float rms_2550_flat = render_block(proc, 1024, 2550.0f, 0.0f);

    MultiParameterChanges self_deharsh;
    self_deharsh.add_queue(1).add(0, 1.0);  // Dynamic ON
    self_deharsh.add_queue(10).add(0, 1.0); // Ref Source = Self De-Harsh
    render_block(proc, 1024, 2550.0f, 0.0f, &self_deharsh);
    float rms_deharsh_settled = 0.0f;
    for (int k = 0; k < 8; ++k) rms_deharsh_settled = render_block(proc, 1024, 2550.0f, 0.0f);
    std::cout << "[INFO] self de-harsh 2550 Hz: flat=" << rms_2550_flat << " carved=" << rms_deharsh_settled << "\n";
    CHECK_STAGE(rms_deharsh_settled > 0.0f && rms_deharsh_settled < rms_2550_flat * 0.96f,
                "Self De-Harsh attenuates isolated narrowband ERB spike");

    // Ecosystem MEC4 3-peer (synth + vocal + aux_eq) coexistence on Ch 3 (slot=2) + setActive(false) clears aux_eq
    {
        monkeys_ear::EcosystemBusClient peer_pub;
        peer_pub.ensure_connected();
        monkeys_ear::EcosystemStreamFrame sf{}, vf{};
        sf.frame_id = 999001ULL;
        sf.role = 1;
        sf.active = true;
        sf.loudness = 0.8f;
        sf.spectral_occupancy.fill(0.0f);
        sf.spectral_occupancy[13] = 0.95f; // 1265 Hz loud synth peer
        peer_pub.publish_synth(2, sf);

        vf.frame_id = 999002ULL;
        vf.role = 2;
        vf.active = true;
        vf.loudness = 0.7f;
        vf.spectral_occupancy.fill(0.0f);
        vf.spectral_occupancy[15] = 0.85f; // 1800 Hz vocal peer
        peer_pub.publish_vocal(2, vf);

        MultiParameterChanges eco_follow;
        eco_follow.add_queue(10).add(0, 2.0 / 3.0); // Eco Peer only
        eco_follow.add_queue(11).add(0, 1.0);       // Bi-Dir (Follow + Publish aux_eq)
        eco_follow.add_queue(12).add(0, 2.0 / 7.0); // Ch 3 (slot 2)
        eco_follow.add_queue(13).add(0, 1.0);       // Peer weight 100%
        render_block(proc, 1024, 1265.0f, 0.0f, &eco_follow);
        float rms_eco_carved = 0.0f;
        for (int k = 0; k < 6; ++k) rms_eco_carved = render_block(proc, 1024, 1265.0f, 0.0f);
        CHECK_STAGE(rms_eco_carved < rms_nosc * 0.88f, "Ecosystem MEC4 peer occupancy carves matching ERB band");

        monkeys_ear::EcosystemStreamFrame s_chk{}, v_chk{}, a_chk{};
        CHECK_STAGE(peer_pub.read_aux_eq(2, a_chk) && a_chk.active && a_chk.role == 3u,
                    "MEC4 aux_eq frame published on slot 2");
        CHECK_STAGE(peer_pub.read_synth(2, s_chk) && s_chk.active && s_chk.frame_id == 999001ULL &&
                    peer_pub.read_vocal(2, v_chk) && v_chk.active && v_chk.frame_id == 999002ULL,
                    "MEC4 3-peer aux_eq coexists alongside synth & vocal on same channel");

        CHECK_STAGE(comp->setActive(false) == kResultOk, "deactivate Onion EQ component");
        CHECK_STAGE(!peer_pub.read_aux_eq(2, a_chk),
                    "setActive(false) clears MEC4 aux_eq bus activity");
        CHECK_STAGE(peer_pub.read_synth(2, s_chk) && peer_pub.read_vocal(2, v_chk),
                    "deactivating aux_eq does not clear synth or vocal peers");
    }

    // Sibling instance isolation + GDI+ editor attach/detach check
    IComponent* sib_comp = nullptr;
    IEditController* sib_ctrl = nullptr;
    CHECK_STAGE(factory2->createInstance(comp_info.cid, IComponent_iid, reinterpret_cast<void**>(&sib_comp)) == kResultOk &&
                factory2->createInstance(ctrl_info.cid, IEditController_iid, reinterpret_cast<void**>(&sib_ctrl)) == kResultOk,
                "instantiate independent sibling component & controller");
    ctrl->setParamNormalized(16, 0.95);
    CHECK_STAGE(std::abs(sib_ctrl->getParamNormalized(16) - 0.5) < 1e-5,
                "sibling instances keep independent parameter state");

    IPlugView* view = ctrl->createView("editor");
    CHECK_STAGE(view != nullptr, "create Onion Skin EQ Win32 editor view");
    ViewRect rect{};
    CHECK_STAGE(view->getSize(&rect) == kResultOk && (rect.right - rect.left) == 980 && (rect.bottom - rect.top) == 620,
                "editor view reports 980x620 size");
    HWND host_wnd = CreateWindowExW(0, L"STATIC", L"Host", WS_POPUP, 0, 0, 980, 620, nullptr, nullptr, nullptr, nullptr);
    CHECK_STAGE(host_wnd != nullptr && view->attached(host_wnd, "HWND") == kResultOk, "attach GDI+ editor to host HWND");
    view->removed();
    DestroyWindow(host_wnd);
    view->release();

    comp_cp->release();
    ctrl_cp->release();
    sib_ctrl->release();
    sib_comp->release();
    ctrl->release();
    proc->release();
    comp->release();
    factory2->release();
    factory->release();
    FreeLibrary(mod);

    std::cout << ">>> ALL STANDALONE ONION SKIN EQ VST3 CHECKS PASSED! <<<\n";
    return 0;
}
