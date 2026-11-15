#include "vst3_sdk_minimal.h"
#include "monkeys_ear/ecosystem_bus.h"
#include "monkeys_ear/vocal_module.h"
#include "monkeys_ear/vocal_product_version.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int kParamCount = 11;
constexpr std::array<float,kParamCount> kDefaultValues{1.0f,.45f,.35f,.80f,.55f,.50f,.10f,0.0f,1.0f,.35f,.90f};
constexpr uint32 kStateMagic = 0x4D455643u; // MEVC
constexpr uint32 kStateVersion = 1u;
static const TUID kVocalComponentCID = INLINE_UID(0x4D6F6E6B, 0x65794561, 0x72566F63, 0x616C3031); // Monke yEar Vocal01
static const TUID kVocalControllerCID = INLINE_UID(0x4D6F6E6B, 0x65794561, 0x72566374, 0x726C3031); // Monke yEar Vctrl01

void copy16(char16* dst, const char* src) { size_t i=0; while(src[i] && i<127){dst[i]=static_cast<char16>(src[i]);++i;} dst[i]=0; }
bool write_stream(IBStream* s, void* data, int32 bytes) { int32 done=0; return s && s->write(data,bytes,&done)==kResultOk && done==bytes; }
bool read_stream(IBStream* s, void* data, int32 bytes) { int32 done=0; return s && s->read(data,bytes,&done)==kResultOk && done==bytes; }
bool serialized_float_is_finite(float value) {
    std::uint32_t bits=0;
    static_assert(sizeof(bits)==sizeof(value));
    std::memcpy(&bits,&value,sizeof(bits));
    return (bits&0x7f800000u)!=0x7f800000u;
}

struct VocalState {
    std::array<std::atomic<float>,kParamCount> values;
    std::array<std::atomic<bool>,kParamCount> dirty;
    VocalState() {
        for(int i=0;i<kParamCount;++i){values[i].store(kDefaultValues[i]);dirty[i].store(false);}
    }
};

// Component and controller have independent lifetime/state. A VST3 host
// synchronizes the controller with IEditController::setComponentState and
// delivers UI/automation edits to IAudioProcessor::process. A global "latest
// component" pointer incorrectly couples unrelated plugin instances.
bool normalized_value_is_finite(ParamValue value) {
    std::uint64_t bits=0;
    static_assert(sizeof(bits)==sizeof(value));
    std::memcpy(&bits,&value,sizeof(bits));
    return (bits&0x7ff0000000000000ULL)!=0x7ff0000000000000ULL;
}

struct AutomationCursor {
    IParamValueQueue* queue=nullptr;
    int32 next_index=0;
    int32 point_count=0;
    int32 offset=0;
    float value=0.0f;
    bool pending=false;
};

void advance_automation(AutomationCursor& cursor,int32 from_offset,int32 num_samples) {
    cursor.pending=false;
    while(cursor.queue&&cursor.next_index<cursor.point_count){
        int32 offset=0;
        ParamValue raw=0.0;
        if(cursor.queue->getPoint(cursor.next_index++,offset,raw)!=kResultOk)continue;
        if(offset<from_offset||offset>=num_samples||!normalized_value_is_finite(raw))continue;
        cursor.offset=offset;
        // Clamp as double before converting; a finite hostile double could
        // otherwise overflow float to infinity under -ffast-math.
        cursor.value=static_cast<float>(std::clamp(raw,0.0,1.0));
        cursor.pending=true;
        return;
    }
}

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
    std::array<float,kParamCount> restored{};
    for(uint32 i=0;i<count;++i){
        float value=0;
        if(!read_stream(stream,&value,4))return kInternalError;
        if(!serialized_float_is_finite(value))return kResultFalse;
        restored[i]=std::clamp(value,0.0f,1.0f);
    }
    for(uint32 i=0;i<count;++i){state->values[i].store(restored[i]);state->dirty[i].store(true);}
    return kResultOk;
}

class VocalComponent final : public IComponent, public IAudioProcessor {
public:
    VocalComponent():ref_(1),state_(std::make_shared<VocalState>()){module_.prepare(48000.0f,512);apply_controls();}
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
        if(data.numSamples<0)return kInvalidArgument;
        bool changed=false;
        for(int i=0;i<kParamCount;++i)changed=state_->dirty[i].exchange(false)||changed;
        if(data.numSamples==0){
            // VST3 hosts may flush parameter changes without any audio samples.
            // There is no sample timeline here: commit the last valid value.
            if(data.inputParameterChanges){
                const int32 count=std::max(0,data.inputParameterChanges->getParameterCount());
                for(int32 q=0;q<count;++q){
                    auto* queue=data.inputParameterChanges->getParameterData(q);
                    if(!queue)continue;
                    const ParamID id=queue->getParameterId();
                    if(id>=kParamCount)continue;
                    const int32 points=std::max(0,queue->getPointCount());
                    for(int32 p=0;p<points;++p){
                        int32 ignored_offset=0;
                        ParamValue value=0.0;
                        if(queue->getPoint(p,ignored_offset,value)!=kResultOk
                           ||!normalized_value_is_finite(value))continue;
                        state_->values[id].store(static_cast<float>(std::clamp(value,0.0,1.0)));
                        changed=true;
                    }
                }
            }
            if(changed)apply_controls();
            return kResultOk;
        }
        if(data.numInputs<1||data.numOutputs<1||!data.inputs||!data.outputs
           ||!data.inputs[0].channelBuffers32||!data.outputs[0].channelBuffers32)return kResultFalse;
        auto* in=data.inputs[0].channelBuffers32;
        auto* out=data.outputs[0].channelBuffers32;
        if(data.inputs[0].numChannels<2||data.outputs[0].numChannels<2
           ||!in[0]||!in[1]||!out[0]||!out[1])return kResultFalse;

        // At most one queue per stable ParamID; no allocations, locks, or
        // unbounded per-sample scanning on the real-time thread.
        std::array<AutomationCursor,kParamCount> events{};
        if(data.inputParameterChanges){
            const int32 queue_count=std::max(0,data.inputParameterChanges->getParameterCount());
            for(int32 q=0;q<queue_count;++q){
                auto* queue=data.inputParameterChanges->getParameterData(q);
                if(!queue)continue;
                const ParamID id=queue->getParameterId();
                if(id>=kParamCount)continue;
                events[id].queue=queue;
                events[id].next_index=0;
                events[id].point_count=std::max(0,queue->getPointCount());
            }
        }
        for(auto& event:events)advance_automation(event,0,data.numSamples);

        // Split only at actual host automation offsets. Every sample is
        // rendered once, with each control transition applied at its offset.
        for(int32 position=0;position<data.numSamples;){
            for(int32 id=0;id<kParamCount;++id){
                auto& event=events[id];
                while(event.pending&&event.offset==position){
                    state_->values[id].store(event.value);
                    changed=true;
                    advance_automation(event,position,data.numSamples);
                }
            }
            if(changed){apply_controls();changed=false;}
            int32 next=data.numSamples;
            for(const auto& event:events){
                if(event.pending)next=std::min(next,event.offset);
            }
            if(next<=position)return kInternalError;
            module_.process_block(in[0]+position,in[1]+position,
                                  out[0]+position,out[1]+position,
                                  static_cast<uint32>(next-position));
            position=next;
        }
        return kResultOk;
    }
    uint32 SMTG_STDCALL getTailSamples() override{return 0;}
private:
    void apply_controls(){auto value=[this](int i){return state_->values[i].load();}; monkeys_ear::VocalExpressionControls c{};c.enabled=value(0)>=.5f;c.correction_strength=value(1);c.drift_retention=value(2);c.vibrato_retention=value(3);c.transition=value(4);c.formant_repair=value(5);c.spectral_residual_mix=value(6);c.character=value(7);c.mix=value(8);c.sequential_stage_mix=value(9);c.aperiodic_protection=value(10);module_.set_controls(c);module_.set_bypass(!c.enabled);}
    std::atomic<uint32> ref_;std::shared_ptr<VocalState> state_;monkeys_ear::VocalModule module_{};
};

#if defined(_WIN32)
class VocalEditor final : public IPlugView {
public:
    static constexpr int kWidth = 680;
    static constexpr int kHeight = 476;
    static constexpr UINT_PTR kTimerId = 1;

    VocalEditor(IEditController* controller, IComponentHandler** handler_slot)
        : ref_(1), controller_(controller), handler_slot_(handler_slot) {
        if (controller_) controller_->addRef();
        eco_.ensure_connected();
    }
    ~VocalEditor() {
        removed();
        if (controller_) controller_->release();
    }

    tresult SMTG_STDCALL queryInterface(const TUID iid, void** obj) override {
        if (!obj) return kInvalidArgument;
        if (!std::memcmp(iid, FUnknown_iid, sizeof(TUID)) ||
            !std::memcmp(iid, IPlugView_iid, sizeof(TUID))) {
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
        return type && std::strcmp(type, "HWND") == 0 ? kResultOk : kInvalidArgument;
    }
    tresult SMTG_STDCALL attached(void* parent, FIDString type) override {
        if (!parent || isPlatformTypeSupported(type) != kResultOk) return kInvalidArgument;
        removed();
        register_class();
        hwnd_ = CreateWindowExA(
            0, class_name(), "Monkey's Ear Vocal",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            0, 0, kWidth, kHeight, reinterpret_cast<HWND>(parent), nullptr,
            GetModuleHandleA(nullptr), this);
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
    struct ControlBox {
        int param_id = -1;
        RECT rect{};
        bool is_toggle = false;
    };

    static const char* class_name() { return "MonkeysEarVocalEditorWnd"; }
    static void register_class() {
        static bool registered = false;
        if (registered) return;
        WNDCLASSA wc{};
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = &VocalEditor::wnd_proc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        wc.lpszClassName = class_name();
        RegisterClassA(&wc);
        registered = true;
    }

    static std::array<ControlBox, kParamCount> layout_controls() {
        std::array<ControlBox, kParamCount> boxes{};
        boxes[0] = {0, {556, 20, 658, 52}, true}; // Enable / Bypass toggle
        // Left column: params 1..5
        for (int i = 0; i < 5; ++i) {
            const int y = 210 + i * 48;
            boxes[1 + i] = {1 + i, {26, y, 326, y + 36}, false};
        }
        // Right column: params 6..10
        for (int i = 0; i < 5; ++i) {
            const int y = 210 + i * 48;
            boxes[6 + i] = {6 + i, {354, y, 654, y + 36}, false};
        }
        return boxes;
    }

    int hit_test(int x, int y) const {
        const auto boxes = layout_controls();
        for (const auto& b : boxes) {
            if (x >= b.rect.left && x <= b.rect.right && y >= b.rect.top && y <= b.rect.bottom) {
                return b.param_id;
            }
        }
        return -1;
    }

    void push_param(int id, double norm) {
        if (!controller_ || id < 0 || id >= kParamCount) return;
        const double clamped = std::clamp(norm, 0.0, 1.0);
        controller_->setParamNormalized(static_cast<ParamID>(id), clamped);
        if (handler_slot_ && *handler_slot_) {
            (*handler_slot_)->performEdit(static_cast<ParamID>(id), clamped);
        }
        if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void begin_param(int id) {
        if (handler_slot_ && *handler_slot_ && id >= 0) {
            (*handler_slot_)->beginEdit(static_cast<ParamID>(id));
        }
    }
    void end_param(int id) {
        if (handler_slot_ && *handler_slot_ && id >= 0) {
            (*handler_slot_)->endEdit(static_cast<ParamID>(id));
        }
    }

    static void fill_round(HDC hdc, RECT r, int rad, COLORREF fill, COLORREF border) {
        HBRUSH br = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, border);
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        RoundRect(hdc, r.left, r.top, r.right, r.bottom, rad, rad);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);
    }

    void paint() {
        PAINTSTRUCT ps{};
        HDC screen = BeginPaint(hwnd_, &ps);
        HDC hdc = CreateCompatibleDC(screen);
        HBITMAP bmp = CreateCompatibleBitmap(screen, kWidth, kHeight);
        HGDIOBJ old_bmp = SelectObject(hdc, bmp);

        RECT full{0, 0, kWidth, kHeight};
        HBRUSH bg = CreateSolidBrush(RGB(14, 17, 24));
        FillRect(hdc, &full, bg);
        DeleteObject(bg);

        SetBkMode(hdc, TRANSPARENT);
        HFONT title_font = CreateFontA(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            VARIABLE_PITCH, "Segoe UI");
        HFONT sub_font = CreateFontA(13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            VARIABLE_PITCH, "Segoe UI");
        HFONT small_font = CreateFontA(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            VARIABLE_PITCH, "Consolas");

        HGDIOBJ old_font = SelectObject(hdc, title_font);
        SetTextColor(hdc, RGB(242, 245, 250));
        RECT tr{24, 16, 480, 40};
        DrawTextA(hdc, "MONKEY'S EAR VOCAL", -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, sub_font);
        SetTextColor(hdc, RGB(145, 162, 188));
        RECT sr{24, 40, 520, 58};
        DrawTextA(hdc, "PROSODIC PITCH, FORMANT & ECOSYSTEM COMPANION  |  Shift: Fine  Wheel: Step  R-Click: Reset",
                  -1, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Enable / Bypass pill
        const auto boxes = layout_controls();
        const bool enabled = controller_ ? (controller_->getParamNormalized(0) >= 0.5) : true;
        fill_round(hdc, boxes[0].rect, 10,
                   enabled ? RGB(30, 118, 92) : RGB(98, 42, 48),
                   enabled ? RGB(86, 224, 178) : RGB(224, 104, 112));
        SetTextColor(hdc, RGB(245, 248, 252));
        DrawTextA(hdc, enabled ? "ACTIVE" : "BYPASS", -1, const_cast<RECT*>(&boxes[0].rect),
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Ecosystem Telemetry Panel
        RECT eco_panel{24, 68, 656, 174};
        fill_round(hdc, eco_panel, 10, RGB(20, 25, 36), RGB(44, 56, 78));

        monkeys_ear::EcosystemStreamFrame synth_f{}, vocal_f{};
        const bool has_synth = eco_.read_synth_or_any(0, synth_f);
        const bool has_vocal = eco_.read_vocal(0, vocal_f);

        SelectObject(hdc, sub_font);
        SetTextColor(hdc, RGB(114, 214, 245));
        RECT eh{38, 76, 360, 96};
        DrawTextA(hdc, "CONNECTED ECOSYSTEM BUS (SLOT 0)", -1, &eh, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, small_font);
        char status_line[192]{};
        const float vocal_hz = (has_vocal && vocal_f.active && vocal_f.pitch_confidence > 0.05f)
            ? 440.0f * std::pow(2.0f, (vocal_f.pitch_cents - 6900.0f) / 1200.0f) : 0.0f;
        const float synth_hz = (has_synth && synth_f.active)
            ? 440.0f * std::pow(2.0f, (synth_f.pitch_cents - 6900.0f) / 1200.0f) : 0.0f;
        std::snprintf(status_line, sizeof(status_line),
            "VOCAL: %s  %.1f Hz (conf %.0f%%)     |     SYNTH PEER: %s  %.1f Hz  note %d",
            (has_vocal && vocal_f.active) ? "LIVE" : "IDLE", vocal_hz, vocal_f.pitch_confidence * 100.0f,
            (has_synth && synth_f.active) ? "LINKED" : "WAIT", synth_hz, synth_f.midi_note);
        SetTextColor(hdc, RGB(210, 222, 238));
        RECT es{38, 98, 642, 116};
        DrawTextA(hdc, status_line, -1, &es, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // 6-band ERB Vocal (cyan) vs Synth Peer (amber) spectrum bars
        static constexpr const char* kBandLabels[6] = {"140", "320", "720", "1.6k", "3.6k", "7.8k"};
        for (int b = 0; b < 6; ++b) {
            const int bx = 38 + b * 101;
            RECT bar_bg{bx, 122, bx + 92, 152};
            fill_round(hdc, bar_bg, 4, RGB(12, 15, 22), RGB(34, 42, 58));

            const float voc = std::clamp(vocal_f.spectral_occupancy[b] * 2.4f, 0.0f, 1.0f);
            const float syn = std::clamp(synth_f.spectral_occupancy[b] * 2.4f, 0.0f, 1.0f);
            if (syn > 0.01f) {
                RECT sr_bar{bx + 3, 150 - static_cast<int>(syn * 24.0f), bx + 44, 150};
                HBRUSH sb = CreateSolidBrush(RGB(238, 168, 66));
                FillRect(hdc, &sr_bar, sb);
                DeleteObject(sb);
            }
            if (voc > 0.01f) {
                RECT vr_bar{bx + 48, 150 - static_cast<int>(voc * 24.0f), bx + 89, 150};
                HBRUSH vb = CreateSolidBrush(RGB(74, 212, 240));
                FillRect(hdc, &vr_bar, vb);
                DeleteObject(vb);
            }
            RECT bl{bx, 154, bx + 92, 168};
            SetTextColor(hdc, RGB(130, 145, 168));
            DrawTextA(hdc, kBandLabels[b], -1, &bl, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // Section headers
        SelectObject(hdc, sub_font);
        SetTextColor(hdc, RGB(238, 182, 92));
        RECT lh{26, 186, 326, 204};
        DrawTextA(hdc, "PITCH & PROSODIC TRACKING", -1, &lh, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT rh{354, 186, 654, 204};
        DrawTextA(hdc, "TIMBRE, RESIDUAL & OUTPUT STAGE", -1, &rh, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        static constexpr const char* kShortNames[kParamCount] = {
            "Enable", "Correction Strength", "Drift Retention", "Vibrato Retention",
            "Transition Speed", "Envelope Repair", "Spectral Residual", "Character",
            "Dry / Wet Mix", "Sequential Staging", "Aperiodic Protection"
        };

        for (int i = 1; i < kParamCount; ++i) {
            const RECT r = boxes[i].rect;
            const bool hovered = (hover_id_ == i || drag_id_ == i);
            fill_round(hdc, r, 7,
                       hovered ? RGB(30, 38, 54) : RGB(22, 27, 39),
                       hovered ? RGB(92, 198, 232) : RGB(44, 54, 74));

            const double norm = controller_ ? std::clamp(controller_->getParamNormalized(static_cast<ParamID>(i)), 0.0, 1.0) : kDefaultValues[i];
            const int track_w = (r.right - r.left) - 16;
            const int fill_w = static_cast<int>(std::round(norm * static_cast<double>(track_w)));

            RECT track{r.left + 8, r.bottom - 11, r.right - 8, r.bottom - 5};
            HBRUSH tb = CreateSolidBrush(RGB(12, 15, 22));
            FillRect(hdc, &track, tb);
            DeleteObject(tb);

            if (fill_w > 0) {
                RECT fill{track.left, track.top, track.left + fill_w, track.bottom};
                HBRUSH fb = CreateSolidBrush(i <= 5 ? RGB(74, 212, 240) : RGB(238, 174, 72));
                FillRect(hdc, &fill, fb);
                DeleteObject(fb);
            }

            SelectObject(hdc, sub_font);
            SetTextColor(hdc, RGB(224, 232, 244));
            RECT nl{r.left + 10, r.top + 3, r.right - 90, r.top + 22};
            DrawTextA(hdc, kShortNames[i], -1, &nl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            char val_txt[32]{};
            if (i == 4) std::snprintf(val_txt, sizeof(val_txt), "%.1f ms", 2.0 + (1.0 - norm) * 58.0);
            else std::snprintf(val_txt, sizeof(val_txt), "%.0f%%", norm * 100.0);
            SelectObject(hdc, small_font);
            SetTextColor(hdc, RGB(170, 212, 232));
            RECT vl{r.right - 88, r.top + 3, r.right - 10, r.top + 22};
            DrawTextA(hdc, val_txt, -1, &vl, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }

        BitBlt(screen, 0, 0, kWidth, kHeight, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, old_font);
        SelectObject(hdc, old_bmp);
        DeleteObject(title_font);
        DeleteObject(sub_font);
        DeleteObject(small_font);
        DeleteObject(bmp);
        DeleteDC(hdc);
        EndPaint(hwnd_, &ps);
    }

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTA*>(lParam);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }
        auto* self = reinterpret_cast<VocalEditor*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
        if (!self) return DefWindowProcA(hwnd, msg, wParam, lParam);

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
                if (self->drag_id_ >= 1 && self->controller_) {
                    const double sens = (wParam & MK_SHIFT) ? 0.0012 : 0.0045;
                    const double next = std::clamp(self->drag_start_val_ - static_cast<double>(y - self->drag_start_y_) * sens, 0.0, 1.0);
                    self->push_param(self->drag_id_, next);
                } else {
                    const int h = self->hit_test(x, y);
                    if (h != self->hover_id_) {
                        self->hover_id_ = h;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
                return 0;
            }
            case WM_LBUTTONDOWN: {
                const int x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
                const int y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
                const int hit = self->hit_test(x, y);
                if (hit == 0 && self->controller_) {
                    self->begin_param(0);
                    const double cur = self->controller_->getParamNormalized(0);
                    self->push_param(0, cur >= 0.5 ? 0.0 : 1.0);
                    self->end_param(0);
                } else if (hit >= 1 && self->controller_) {
                    SetCapture(hwnd);
                    self->drag_id_ = hit;
                    self->drag_start_y_ = y;
                    self->drag_start_val_ = self->controller_->getParamNormalized(static_cast<ParamID>(hit));
                    self->begin_param(hit);
                }
                return 0;
            }
            case WM_LBUTTONUP: {
                if (self->drag_id_ >= 1) {
                    const int finished = self->drag_id_;
                    self->drag_id_ = -1;
                    ReleaseCapture();
                    self->end_param(finished);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            case WM_RBUTTONDOWN:
            case WM_LBUTTONDBLCLK: {
                const int x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
                const int y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
                const int hit = self->hit_test(x, y);
                if (hit >= 0 && hit < kParamCount) {
                    self->begin_param(hit);
                    self->push_param(hit, kDefaultValues[hit]);
                    self->end_param(hit);
                }
                return 0;
            }
            case WM_MOUSEWHEEL: {
                if (self->hover_id_ >= 1 && self->controller_) {
                    const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                    const double step = ((GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT) ? 0.008 : 0.03) * (delta > 0 ? 1.0 : -1.0);
                    self->begin_param(self->hover_id_);
                    self->push_param(self->hover_id_, self->controller_->getParamNormalized(static_cast<ParamID>(self->hover_id_)) + step);
                    self->end_param(self->hover_id_);
                    return 0;
                }
                break;
            }
        }
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    std::atomic<uint32> ref_;
    IEditController* controller_ = nullptr;
    IComponentHandler** handler_slot_ = nullptr;
    IPlugFrame* frame_ = nullptr;
    HWND hwnd_ = nullptr;
    monkeys_ear::EcosystemBusClient eco_{};
    int hover_id_ = -1;
    int drag_id_ = -1;
    int drag_start_y_ = 0;
    double drag_start_val_ = 0.0;
};
#endif

class VocalController final : public IEditController {
public:
    VocalController():ref_(1),state_(std::make_shared<VocalState>()){}
    tresult SMTG_STDCALL queryInterface(const TUID iid,void** obj) override{if(!obj)return kInvalidArgument;if(!std::memcmp(iid,FUnknown_iid,sizeof(TUID))||!std::memcmp(iid,IPluginBase_iid,sizeof(TUID))||!std::memcmp(iid,IEditController_iid,sizeof(TUID))){*obj=static_cast<IEditController*>(this);addRef();return kResultOk;}*obj=nullptr;return kResultFalse;} uint32 SMTG_STDCALL addRef() override{return ++ref_;}uint32 SMTG_STDCALL release() override{uint32 n=--ref_;if(!n)delete this;return n;}tresult SMTG_STDCALL initialize(FUnknown*) override{return kResultOk;}tresult SMTG_STDCALL terminate() override{return kResultOk;}
    tresult SMTG_STDCALL setComponentState(IBStream* s) override{return read_state(s,state_);}tresult SMTG_STDCALL setState(IBStream* s) override{return read_state(s,state_);}tresult SMTG_STDCALL getState(IBStream* s) override{return write_state(s,state_);}int32 SMTG_STDCALL getParameterCount() override{return kParamCount;}
    tresult SMTG_STDCALL getParameterInfo(int32 i,ParameterInfo& info) override{static constexpr const char* names[kParamCount]={"VOCAL: Enable","VOCAL: Correction Strength","VOCAL: Drift Retention","VOCAL: Vibrato Retention","VOCAL: Transition","VOCAL: Envelope Repair","VOCAL: Spectral Residual","VOCAL: Character","VOCAL: Mix","VOCAL: Soft Sequential Staging","VOCAL: Aperiodic Protection"};if(i<0||i>=kParamCount)return kInvalidArgument;info.id=static_cast<ParamID>(i);info.stepCount=i==0?1:0;info.unitId=0;info.flags=kCanAutomate|(i==0?kIsBypass:0);info.defaultNormalizedValue=kDefaultValues[i];copy16(info.title,names[i]);copy16(info.shortTitle,names[i]);copy16(info.units,i==4?"":"%");return kResultOk;}
    tresult SMTG_STDCALL getParamStringByValue(ParamID id,ParamValue value,char16 string[128]) override{char text[32];if(id==0)std::snprintf(text,sizeof(text),"%s",value>=.5?"On":"Bypass");else if(id==4)std::snprintf(text,sizeof(text),"%.1f ms",2.0+(1.0-value)*58.0);else std::snprintf(text,sizeof(text),"%.1f %%",value*100.0);copy16(string,text);return kResultOk;}tresult SMTG_STDCALL getParamValueByString(ParamID,const char16*,ParamValue& value) override{value=.5;return kResultOk;}ParamValue SMTG_STDCALL normalizedParamToPlain(ParamID,ParamValue v) override{return v;}ParamValue SMTG_STDCALL plainParamToNormalized(ParamID,ParamValue v) override{return v;}ParamValue SMTG_STDCALL getParamNormalized(ParamID id) override{return id<kParamCount?state_->values[id].load():0.0;}tresult SMTG_STDCALL setParamNormalized(ParamID id,ParamValue value) override{if(id>=kParamCount||!normalized_value_is_finite(value))return kInvalidArgument;state_->values[id].store(static_cast<float>(std::clamp(value,0.0,1.0)));state_->dirty[id].store(true);return kResultOk;}tresult SMTG_STDCALL setComponentHandler(IComponentHandler* handler) override { handler_ = handler; return kResultOk; }
    IPlugView* SMTG_STDCALL createView(const char8* name) override {
#if defined(_WIN32)
        if (!name || std::strcmp(name, "editor") == 0) {
            return new VocalEditor(this, &handler_);
        }
#endif
        return nullptr;
    }
private:std::atomic<uint32> ref_;std::shared_ptr<VocalState> state_;IComponentHandler* handler_ = nullptr;
};

class VocalFactory final : public IPluginFactory2 {
public:
    VocalFactory():ref_(1){}tresult SMTG_STDCALL queryInterface(const TUID iid,void** obj) override{if(!obj)return kInvalidArgument;if(!std::memcmp(iid,FUnknown_iid,sizeof(TUID))||!std::memcmp(iid,IPluginFactory_iid,sizeof(TUID))||!std::memcmp(iid,IPluginFactory2_iid,sizeof(TUID))){*obj=static_cast<IPluginFactory2*>(this);addRef();return kResultOk;}*obj=nullptr;return kResultFalse;}uint32 SMTG_STDCALL addRef() override{return ++ref_;}uint32 SMTG_STDCALL release() override{uint32 n=--ref_;if(!n)delete this;return n;}
    tresult SMTG_STDCALL getFactoryInfo(PFactoryInfo* info) override{if(!info)return kInvalidArgument;std::strncpy(info->vendor,"Ultramonkeydog Studios",sizeof(info->vendor)-1);std::strncpy(info->url,"https://ultramonkeydog-studios.vercel.app",sizeof(info->url)-1);info->flags=0;return kResultOk;}int32 SMTG_STDCALL countClasses() override{return 2;}
    tresult SMTG_STDCALL getClassInfo(int32 i,PClassInfo* info) override{return class_info(i,info,static_cast<PClassInfo2*>(nullptr));}tresult SMTG_STDCALL getClassInfo2(int32 i,PClassInfo2* info) override{return class_info(i,static_cast<PClassInfo*>(nullptr),info);}tresult SMTG_STDCALL createInstance(const TUID cid,const TUID iid,void** obj) override{if(!std::memcmp(cid,kVocalComponentCID,sizeof(TUID))){auto* p=new VocalComponent();auto r=p->queryInterface(iid,obj);p->release();return r;}if(!std::memcmp(cid,kVocalControllerCID,sizeof(TUID))){auto* p=new VocalController();auto r=p->queryInterface(iid,obj);p->release();return r;}return kInvalidArgument;}
private:
    template<class A,class B> tresult class_info(int32 i,A* a,B* b){if(i<0||i>1)return kInvalidArgument;auto cid=i==0?kVocalComponentCID:kVocalControllerCID;const char* name=i==0?"Monkey's Ear Vocal":"Monkey's Ear Vocal Controller";if(a){std::memcpy(a->cid,cid,sizeof(TUID));a->cardinality=0x7fffffff;std::strncpy(a->category,i==0?"Audio Module Class":"Component Controller Class",sizeof(a->category)-1);std::strncpy(a->name,name,sizeof(a->name)-1);}if(b){std::memcpy(b->cid,cid,sizeof(TUID));b->cardinality=0x7fffffff;std::strncpy(b->category,i==0?"Audio Module Class":"Component Controller Class",sizeof(b->category)-1);std::strncpy(b->name,name,sizeof(b->name)-1);b->classFlags=0;std::strncpy(b->subCategories,i==0?"Fx|Pitch Shift":"",sizeof(b->subCategories)-1);std::strncpy(b->vendor,"Ultramonkeydog Studios",sizeof(b->vendor)-1);std::strncpy(b->version,monkeys_ear::kVocalProductVersion,sizeof(b->version)-1);std::strncpy(b->sdkVersion,"VST 3.7.0",sizeof(b->sdkVersion)-1);}return kResultOk;}
    std::atomic<uint32> ref_;
};
} // namespace

extern "C" {
// Each factory has its own COM reference lifetime. A global factory pointer
// survives release() as a dangling pointer and races concurrent host loads.
SMTG_EXPORT_SYMBOL IPluginFactory* SMTG_STDCALL GetPluginFactory(){return new VocalFactory();}
#if defined(_WIN32)
BOOL APIENTRY DllMain(HMODULE,DWORD,LPVOID){return TRUE;}
#endif
}
