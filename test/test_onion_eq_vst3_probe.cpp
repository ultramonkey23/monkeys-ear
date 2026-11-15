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
    CHECK_STAGE(ctrl->getParameterCount() == 41, "controller exposes all 41 parameters");

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
    ProcessData flush_pd{};
    flush_pd.numSamples = 0;
    flush_pd.inputParameterChanges = &flush_chg;
    CHECK_STAGE(proc->process(flush_pd) == kResultOk, "accept zero-sample parameter flush");

    MemoryStream saved;
    CHECK_STAGE(comp->getState(&saved) == kResultOk && saved.bytes().size() == 12u + 41u * sizeof(float),
                "serialize MEOQ component state");
    uint32 magic = 0, ver = 0, cnt = 0;
    std::memcpy(&magic, saved.bytes().data(), 4);
    std::memcpy(&ver, saved.bytes().data() + 4, 4);
    std::memcpy(&cnt, saved.bytes().data() + 8, 4);
    CHECK_STAGE(magic == 0x4D454F51u && ver == 1u && cnt == 41u, "verify MEOQ state header");

    // Reset mix to 1.0 and test flat vs surgical boost vs bypass vs sidechain carve
    {
        MultiParameterChanges restore;
        restore.add_queue(15).add(0, 1.0); // Mix 100%
        restore.add_queue(1).add(0, 0.0);  // Dynamic Off first for exact surgical check
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

    // Ecosystem MEC3 peer carving test on Ch 3 (slot=2): publish loud peer at 1265 Hz, Onion EQ follows Ch 3
    {
        monkeys_ear::EcosystemBusClient peer_pub;
        peer_pub.ensure_connected();
        monkeys_ear::EcosystemStreamFrame sf{};
        sf.frame_id = 999001ULL;
        sf.role = 1;
        sf.active = true;
        sf.loudness = 0.8f;
        sf.spectral_occupancy.fill(0.0f);
        sf.spectral_occupancy[13] = 0.95f; // 1265 Hz loud peer
        peer_pub.publish_synth(2, sf);

        MultiParameterChanges eco_follow;
        eco_follow.add_queue(10).add(0, 2.0 / 3.0); // Eco Peer only
        eco_follow.add_queue(11).add(0, 2.0 / 3.0); // Follow
        eco_follow.add_queue(12).add(0, 2.0 / 7.0); // Ch 3 (slot 2)
        eco_follow.add_queue(13).add(0, 1.0);       // Peer weight 100%
        render_block(proc, 1024, 1265.0f, 0.0f, &eco_follow);
        float rms_eco_carved = 0.0f;
        for (int k = 0; k < 6; ++k) rms_eco_carved = render_block(proc, 1024, 1265.0f, 0.0f);
        CHECK_STAGE(rms_eco_carved < rms_nosc * 0.88f, "Ecosystem MEC3 peer occupancy carves matching ERB band");
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
