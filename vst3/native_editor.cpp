#include "native_editor.h"
#include "ui_assets.h"
#include "monkeys_ear/onion_skin_eq.h"
#include "monkeys_ear/wavetable.h"
#include "monkeys_ear/ecosystem_bus.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>

namespace monkeys_ear::ui {
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int kCanvasW = 1120;
constexpr int kCanvasH = 740;
constexpr int kComboId = 10301;
constexpr int kSearchId = 10302;
constexpr wchar_t kWindowClass[] = L"MonkeysEarNativeEditor_01";
int kModuleAnchor = 0; // anchored inside the DLL, never the host executable
std::atomic<int> gAttachedViews{0};
std::atomic<int> gGdiplusRefs{0};
ULONG_PTR gGdiplusToken = 0;

constexpr COLORREF kBackground = RGB(12, 16, 22);
constexpr COLORREF kPanel      = RGB(23, 29, 39);
constexpr COLORREF kPanelHover = RGB(34, 42, 56);
constexpr COLORREF kText       = RGB(238, 243, 249);
constexpr COLORREF kMuted      = RGB(154, 168, 184);
constexpr COLORREF kAccent     = RGB(52, 224, 206);
constexpr COLORREF kGold       = RGB(246, 184, 68);
constexpr COLORREF kViolet     = RGB(172, 128, 255);
constexpr COLORREF kCoral      = RGB(246, 114, 108);
constexpr COLORREF kEmerald    = RGB(66, 226, 152);
constexpr COLORREF kOutline    = RGB(52, 64, 79);

constexpr const wchar_t* kAreas[] = {
    L"All parameters", L"PLAY & Macros", L"Oscillators / Voice",
    L"Filters / Envelopes", L"Onion 24-Band EQ", L"Rhythm / Modulation",
    L"ChronoState", L"Sound Space", L"Vocal Expression",
    L"Tube Rack", L"Tube 1", L"Tube 2", L"Tube 3", L"Tube 4",
    L"Master / Routing"
};
constexpr const wchar_t* kSounds[] = {
    L"CURRENT", L"MONOLITH", L"FERAL WOBBLE", L"VELVET LEAD",
    L"TUBE FOUNDRY", L"CLOCKWORK SUB", L"GLASS CHAMBER", L"SOUND SPACE"
};

inline Gdiplus::Color gp_col(COLORREF c, BYTE alpha = 255) {
    return Gdiplus::Color(alpha, GetRValue(c), GetGValue(c), GetBValue(c));
}

void gdiplus_acquire() {
    if (gGdiplusRefs.fetch_add(1) == 0) {
        Gdiplus::GdiplusStartupInput input;
        if (Gdiplus::GdiplusStartup(&gGdiplusToken, &input, nullptr) != Gdiplus::Ok) {
            gGdiplusToken = 0;
        }
    }
}

void gdiplus_release() {
    if (gGdiplusRefs.fetch_sub(1) == 1 && gGdiplusToken != 0) {
        Gdiplus::GdiplusShutdown(gGdiplusToken);
        gGdiplusToken = 0;
    }
}

std::wstring to_wide(const char16* text) {
    std::wstring out;
    if (!text) return out;
    for (size_t i = 0; i < 127 && text[i]; ++i) out += static_cast<wchar_t>(text[i]);
    return out;
}

std::wstring ascii_wide(const char* text) {
    std::wstring out;
    if (!text) return out;
    for (size_t i = 0; i < 127 && text[i]; ++i) out += static_cast<wchar_t>(static_cast<unsigned char>(text[i]));
    return out;
}

bool contains_ci(const std::wstring& hay, const std::wstring& needle) {
    if (needle.empty()) return true;
    std::wstring a = hay, b = needle;
    for (wchar_t& x : a) x = static_cast<wchar_t>(towlower(x));
    for (wchar_t& x : b) x = static_cast<wchar_t>(towlower(x));
    return a.find(b) != std::wstring::npos;
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

void rectangle(Gdiplus::Graphics& g, int x, int y, int w, int ht,
               COLORREF fill, COLORREF outline, int rad = 12,
               BYTE fill_alpha = 232, BYTE outline_alpha = 255) {
    Gdiplus::GraphicsPath path;
    gp_round_rect_path(path, static_cast<Gdiplus::REAL>(x), static_cast<Gdiplus::REAL>(y),
                       static_cast<Gdiplus::REAL>(w), static_cast<Gdiplus::REAL>(ht),
                       static_cast<Gdiplus::REAL>(rad * 0.5f));
    Gdiplus::SolidBrush br(gp_col(fill, fill_alpha));
    g.FillPath(&br, &path);
    Gdiplus::Pen pen(gp_col(outline, outline_alpha), 1.2f);
    g.DrawPath(&pen, &path);
}

void label(HDC h, HFONT font, COLORREF color, const std::wstring& text,
           int x, int y, int w, int ht, UINT style = DT_LEFT) {
    HGDIOBJ old = SelectObject(h, font);
    SetBkMode(h, TRANSPARENT);
    SetTextColor(h, color);
    RECT r{x, y, x + w, y + ht};
    DrawTextW(h, text.c_str(), -1, &r, style | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(h, old);
}

// Preserve existing host parameter ABI/metadata while presenting real
// discrete sound-design decisions cleanly in the custom view.
int usable_steps(int id, int advertised) {
    if (id == 9) return 3;   // waveform
    if (id == 18) return 2;  // input routing
    if (id == 21) return 3;  // subharmonic ratio
    if (id == 28 || id == 30) return 4; // filter modes
    if (id == 31) return 2;  // filter connection
    if (id == 56) return 4;  // LFO shape
    if (id >= 37 && id <= 52 && (id - 37) % 4 == 0) return 5; // EQ type
    switch (id) {
    case 12: case 17: case 23: case 34: case 35: case 53: case 54:
    case 70: case 71: case 82: case 113: case 192: return 1;
    case 178: return 5;
    case 179: return 8;
    case 183: return 4;
    case 193: return 2;
    case 198: return 3;
    case 199: return 7;
    case 202: return 9;
    case 216: return 3;
    default: return advertised;
    }
}

struct Hit {
    RECT rect{};
    int id = -1;
    // action:
    //  0 = normal parameter (drag/menu/toggle)
    //  1 = switch page
    //  2 = palette preset
    //  3 = Perform XY (176/177)
    //  4 = Wave quick preset
    //  5 = stepper prev (<)
    //  6 = stepper next (>)
    //  7 = Scope XY drag (181 Phase Warp / 182 Wave Fold)
    //  8 = Morph ribbon drag (180)
    //  9 = Sweep-paint Harmonic bars H1..H8 (207..214)
    // 10 = Harmonic bar preset (FLAT/WARM/ODD/OCT/BELL)
    // 11 = Sweep-paint 24-Band ERB EQ (221..244)
    // 12 = 24-Band EQ quick curve (FLAT/WARMTH/CLARITY/POCKET)
    int action = 0;
    int target = 0;
    bool contains(int x, int y) const {
        return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
    }
};
} // namespace

class NativeEditor final : public IPlugView {
public:
    NativeEditor(IEditController* c, IComponentHandler** handler)
        : controller_(c), handler_slot_(handler) {
        controller_->addRef();
        gdiplus_acquire();
        chassis_bmp_ = assets::load_bitmap_from_bytes(
            assets::kStudioChassisPlateJpg, sizeof(assets::kStudioChassisPlateJpg));
        crest_bmp_ = assets::load_bitmap_from_bytes(
            assets::kMonkeysEarCrestPng, sizeof(assets::kMonkeysEarCrestPng));
        last_host_values_.assign(
            static_cast<size_t>(std::max<int32>(0, controller_->getParameterCount())),
            -2.0);
        title_font_ = CreateFontW(-28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        body_font_ = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        small_font_ = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        micro_font_ = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }

    ~NativeEditor() {
        removed();
        if (title_font_) DeleteObject(title_font_);
        if (body_font_) DeleteObject(body_font_);
        if (small_font_) DeleteObject(small_font_);
        if (micro_font_) DeleteObject(micro_font_);
        chassis_bmp_.reset();
        crest_bmp_.reset();
        gdiplus_release();
        controller_->release();
    }

    tresult SMTG_STDCALL queryInterface(const TUID iid, void** out) override {
        if (!out) return kInvalidArgument;
        if (memcmp(iid, FUnknown_iid, sizeof(TUID)) == 0 ||
            memcmp(iid, IPlugView_iid, sizeof(TUID)) == 0) {
            *out = static_cast<IPlugView*>(this);
            addRef();
            return kResultOk;
        }
        *out = nullptr;
        return kResultFalse;
    }
    uint32 SMTG_STDCALL addRef() override { return ++references_; }
    uint32 SMTG_STDCALL release() override {
        const uint32 count = --references_;
        if (count == 0) delete this;
        return count;
    }
    tresult SMTG_STDCALL isPlatformTypeSupported(FIDString type) override {
        return type && std::strcmp(type, "HWND") == 0 ? kResultOk : kResultFalse;
    }
    tresult SMTG_STDCALL attached(void* parent, FIDString type) override {
        if (isPlatformTypeSupported(type) != kResultOk || !parent || window_) return kResultFalse;
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(&kModuleAnchor),
                                &module)) return kResultFalse;
        module_ = module;
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.hInstance = module_;
        wc.lpfnWndProc = &NativeEditor::window_proc;
        wc.lpszClassName = kWindowClass;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return kResultFalse;
        window_ = CreateWindowExW(0, kWindowClass, L"Monkey's Ear",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, width_, height_,
            static_cast<HWND>(parent), nullptr, wc.hInstance, this);
        if (!window_) return kResultFalse;
        ++gAttachedViews;
        combo_ = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
            120, 168, 340, 400, window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kComboId)),
            wc.hInstance, nullptr);
        search_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
            550, 168, 450, 31, window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSearchId)),
            wc.hInstance, nullptr);
        if (!combo_ || !search_) { removed(); return kResultFalse; }
        SendMessageW(combo_, WM_SETFONT, reinterpret_cast<WPARAM>(body_font_), TRUE);
        SendMessageW(search_, WM_SETFONT, reinterpret_cast<WPARAM>(body_font_), TRUE);
        for (const wchar_t* area : kAreas) SendMessageW(combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(area));
        SendMessageW(combo_, CB_SETCURSEL, 0, 0);
        show_native_controls();
        SetTimer(window_, 1, 33, nullptr); // ~30 Hz UI refresh + live telemetry/scope
        InvalidateRect(window_, nullptr, FALSE);
        return kResultOk;
    }
    tresult SMTG_STDCALL removed() override {
        finish_drag();
        if (window_) {
            KillTimer(window_, 1);
            HWND old = window_;
            window_ = nullptr;
            combo_ = search_ = nullptr;
            DestroyWindow(old);
            if (--gAttachedViews == 0 && module_)
                UnregisterClassW(kWindowClass, module_);
        }
        return kResultOk;
    }
    tresult SMTG_STDCALL onWheel(float distance) override {
        if (page_ == 1) {
            scroll_rows_ = std::max(0, scroll_rows_ - (distance > 0 ? 2 : -2));
            invalidate();
            return kResultOk;
        }
        const int target_id = (hover_id_ >= 0) ? hover_id_ : selected_id_;
        if (target_id >= 0 && handler()) {
            ParameterInfo meta{};
            if (controller_->getParameterInfo(target_id, meta) == kResultOk) {
                const int steps = usable_steps(target_id, meta.stepCount);
                const bool fine = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                const double delta = (steps > 0) ? (1.0 / steps) : (fine ? 0.005 : 0.025);
                const double cur = controller_->getParamNormalized(target_id);
                change_single(target_id, cur + (distance > 0 ? delta : -delta));
                return kResultOk;
            }
        }
        return kResultFalse;
    }
    tresult SMTG_STDCALL onKeyDown(char16 key, int16, int16) override {
        if (selected_id_ < 0) return kResultFalse;
        if (key == L'+' || key == L'-') {
            ParameterInfo meta{};
            if (controller_->getParameterInfo(selected_id_, meta) != kResultOk) return kResultFalse;
            const int steps = usable_steps(selected_id_, meta.stepCount);
            const double step = steps > 0 ? 1.0 / steps : 0.01;
            change_single(selected_id_,
                controller_->getParamNormalized(selected_id_) + (key == L'+' ? step : -step));
            return kResultOk;
        }
        return kResultFalse;
    }
    tresult SMTG_STDCALL onKeyUp(char16, int16, int16) override { return kResultFalse; }
    tresult SMTG_STDCALL getSize(ViewRect* rect) override {
        if (!rect) return kInvalidArgument;
        *rect = ViewRect(0, 0, width_, height_);
        return kResultOk;
    }
    tresult SMTG_STDCALL onSize(ViewRect* rect) override {
        if (!rect) return kInvalidArgument;
        width_ = std::max(960, rect->right - rect->left);
        height_ = std::max(620, rect->bottom - rect->top);
        if (window_) {
            SetWindowPos(window_, nullptr, 0, 0, width_, height_,
                         SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
            show_native_controls();
            invalidate();
        }
        return kResultOk;
    }
    tresult SMTG_STDCALL onFocus(TBool) override { return kResultOk; }
    tresult SMTG_STDCALL setFrame(IPlugFrame* frame) override {
        frame_ = frame;
        return kResultOk;
    }
    tresult SMTG_STDCALL canResize() override { return kResultOk; }
    tresult SMTG_STDCALL checkSizeConstraint(ViewRect* r) override {
        if (!r) return kInvalidArgument;
        const int width = std::clamp(r->right - r->left, 960, 1920);
        const int height = std::clamp(r->bottom - r->top, 620, 1300);
        r->right = r->left + width;
        r->bottom = r->top + height;
        return kResultOk;
    }

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
        NativeEditor* self = reinterpret_cast<NativeEditor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = static_cast<NativeEditor*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (self) return self->window_message(hwnd, message, wp, lp);
        return DefWindowProcW(hwnd, message, wp, lp);
    }

    LRESULT window_message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC target = BeginPaint(hwnd, &ps);
            RECT actual{};
            GetClientRect(hwnd, &actual);
            const int w = std::max<LONG>(1, actual.right), h = std::max<LONG>(1, actual.bottom);
            HDC buffer = CreateCompatibleDC(target);
            HBITMAP bitmap = CreateCompatibleBitmap(target, w, h);
            HGDIOBJ old = SelectObject(buffer, bitmap);
            SetMapMode(buffer, MM_ANISOTROPIC);
            SetWindowExtEx(buffer, kCanvasW, kCanvasH, nullptr);
            SetViewportExtEx(buffer, w, h, nullptr);
            render(buffer);
            SetMapMode(buffer, MM_TEXT);
            BitBlt(target, 0, 0, w, h, buffer, 0, 0, SRCCOPY);
            SelectObject(buffer, old);
            DeleteObject(bitmap);
            DeleteDC(buffer);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER:
            ++anim_tick_;
            refresh_on_parameter_change();
            return 0;
        case WM_COMMAND:
            if (LOWORD(wp) == kComboId && HIWORD(wp) == CBN_SELCHANGE) {
                selected_area_ = std::clamp(static_cast<int>(SendMessageW(combo_, CB_GETCURSEL, 0, 0)), 0, 14);
                scroll_rows_ = 0;
                invalidate();
            } else if (LOWORD(wp) == kSearchId && HIWORD(wp) == EN_CHANGE) {
                wchar_t buffer[128]{};
                GetWindowTextW(search_, buffer, 128);
                search_text_ = buffer;
                scroll_rows_ = 0;
                invalidate();
            }
            return 0;
        case WM_LBUTTONDOWN:
            click(GET_X_LPARAM(lp) * kCanvasW / std::max(1, width_),
                  GET_Y_LPARAM(lp) * kCanvasH / std::max(1, height_), false);
            return 0;
        case WM_LBUTTONDBLCLK:
            click(GET_X_LPARAM(lp) * kCanvasW / std::max(1, width_),
                  GET_Y_LPARAM(lp) * kCanvasH / std::max(1, height_), true);
            return 0;
        case WM_RBUTTONDOWN:
            click(GET_X_LPARAM(lp) * kCanvasW / std::max(1, width_),
                  GET_Y_LPARAM(lp) * kCanvasH / std::max(1, height_), true);
            return 0;
        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lp) * kCanvasW / std::max(1, width_);
            const int y = GET_Y_LPARAM(lp) * kCanvasH / std::max(1, height_);
            if ((wp & MK_LBUTTON) && drag_mode_ != 0) {
                if (drag_mode_ == 1) drag(x);
                else if (drag_mode_ == 2) drag_xy(x, y, 176, 177);
                else if (drag_mode_ == 3) drag_xy(x, y, 181, 182);
                else if (drag_mode_ == 4) drag_sweep(x, y, 207, 8);
                else if (drag_mode_ == 5) drag_sweep(x, y, 221, 24);
            } else {
                update_hover(x, y);
            }
            return 0;
        }
        case WM_LBUTTONUP: finish_drag(); return 0;
        case WM_CAPTURECHANGED: finish_drag(); return 0;
        case WM_MOUSEWHEEL: onWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / WHEEL_DELTA); return 0;
        case WM_DESTROY: if (window_ == hwnd) window_ = nullptr; return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    void invalidate() { if (window_) InvalidateRect(window_, nullptr, FALSE); }

    void refresh_on_parameter_change() {
        bool any_changed = false;
        const auto count = std::min<size_t>(
            last_host_values_.size(),
            static_cast<size_t>(std::max<int32>(0, controller_->getParameterCount())));
        for (size_t i = 0; i < count; ++i) {
            const double current = controller_->getParamNormalized(static_cast<ParamID>(i));
            if (current != last_host_values_[i]) {
                last_host_values_[i] = current;
                any_changed = true;
            }
        }
        const bool live_page = (page_ == 4 || page_ == 5) && ((anim_tick_ & 1) == 0);
        if (any_changed || live_page) invalidate();
    }

    IComponentHandler* handler() const { return handler_slot_ ? *handler_slot_ : nullptr; }

    void show_native_controls() {
        if (!combo_ || !search_) return;
        const bool show = (page_ == 1);
        ShowWindow(combo_, show ? SW_SHOW : SW_HIDE);
        ShowWindow(search_, show ? SW_SHOW : SW_HIDE);
        const int y = 169 * height_ / kCanvasH;
        MoveWindow(combo_, 170 * width_ / kCanvasW, y, 320 * width_ / kCanvasW,
                   360 * height_ / kCanvasH, TRUE);
        MoveWindow(search_, 590 * width_ / kCanvasW, y, 444 * width_ / kCanvasW,
                   33 * height_ / kCanvasH, TRUE);
    }

    void set_page(int page) {
        finish_drag();
        page_ = page;
        scroll_rows_ = 0;
        show_native_controls();
        invalidate();
    }

    void begin_edit(int id) { if (auto* h = handler()) h->beginEdit(static_cast<ParamID>(id)); }
    void update_edit(int id, double value) {
        if (!handler()) return;
        value = std::clamp(value, 0.0, 1.0);
        if (controller_->setParamNormalized(static_cast<ParamID>(id), value) != kResultOk) return;
        handler()->performEdit(static_cast<ParamID>(id), value);
        selected_id_ = id;
        invalidate();
    }
    void finish_edit(int id) { if (auto* h = handler()) h->endEdit(static_cast<ParamID>(id)); }
    void change_single(int id, double value) {
        if (!handler()) return;
        begin_edit(id);
        update_edit(id, value);
        finish_edit(id);
    }

    void finish_drag() {
        if (drag_mode_ == 0) return;
        const int mode = drag_mode_;
        const int id = dragged_id_;
        const int sx = sweep_base_id_;
        const int sn = sweep_count_;
        drag_mode_ = 0;
        dragged_id_ = -1;
        if (window_ && GetCapture() == window_) ReleaseCapture();
        if (mode == 1 && id >= 0) finish_edit(id);
        else if (mode == 2) { finish_edit(176); finish_edit(177); }
        else if (mode == 3) { finish_edit(181); finish_edit(182); }
        else if ((mode == 4 || mode == 5) && sx >= 0) {
            for (int i = 0; i < sn; ++i) finish_edit(sx + i);
        }
    }

    void drag_xy(int x, int y, int id_x, int id_y) {
        const float w = static_cast<float>(std::max<LONG>(1, drag_rect_.right - drag_rect_.left));
        const float h = static_cast<float>(std::max<LONG>(1, drag_rect_.bottom - drag_rect_.top));
        const float vx = std::clamp((x - drag_rect_.left) / w, 0.0f, 1.0f);
        const float vy = std::clamp((drag_rect_.bottom - y) / h, 0.0f, 1.0f);
        update_edit(id_x, vx);
        update_edit(id_y, vy);
    }

    void drag_sweep(int x, int y, int base_id, int count) {
        const double w = static_cast<double>(std::max<LONG>(1, drag_rect_.right - drag_rect_.left));
        const double h = static_cast<double>(std::max<LONG>(1, drag_rect_.bottom - drag_rect_.top));
        const double rel_x = std::clamp((x - drag_rect_.left) / w, 0.0, 0.9999);
        const int idx = std::clamp(static_cast<int>(rel_x * count), 0, count - 1);
        const double val = std::clamp((drag_rect_.bottom - y) / h, 0.0, 1.0);
        update_edit(base_id + idx, val);
    }

    void drag(int x) {
        if (dragged_id_ < 0) return;
        const int size = std::max<LONG>(1, drag_rect_.right - drag_rect_.left);
        const double raw = std::clamp(static_cast<double>(x - drag_rect_.left) / size, 0.0, 1.0);
        const bool fine = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const double next = fine
            ? std::clamp(drag_anchor_value_ + (raw - drag_anchor_raw_) * 0.25, 0.0, 1.0)
            : raw;
        update_edit(dragged_id_, next);
    }

    void update_hover(int x, int y) {
        int next_hover = -1;
        for (auto it = hits_.rbegin(); it != hits_.rend(); ++it) {
            if (it->contains(x, y)) {
                next_hover = (it->id >= 0) ? it->id : -1;
                break;
            }
        }
        if (next_hover != hover_id_) {
            hover_id_ = next_hover;
            invalidate();
        }
    }

    void apply_wave_preset(int preset_idx) {
        if (!handler()) return;
        auto setv = [&](int id, double v) { change_single(id, v); };
        if (preset_idx == 0) {
            // INIT SAW
            setv(9, 0.0); setv(178, 0.0); setv(179, 0.5); setv(180, 0.0);
            setv(181, 0.5); setv(182, 0.0); setv(183, 0.0); setv(185, 0.0);
            setv(186, 0.5); setv(187, 0.0); setv(202, 0.0); setv(204, 0.0);
            setv(215, 0.0); setv(216, 0.0);
        } else if (preset_idx == 1) {
            // VOCAL MORPH
            setv(178, 0.2); setv(179, 1.0); setv(180, 0.45);
            setv(181, 0.5); setv(182, 0.18); setv(183, 0.25); setv(184, 0.24);
            setv(186, 0.70); setv(188, 0.25); setv(189, 0.50); setv(190, 0.60); setv(191, 0.42);
            setv(202, 0.52); setv(205, 0.75); setv(215, 0.14); setv(216, 0.35);
        } else if (preset_idx == 2) {
            // HOLLOW REED
            setv(9, 0.34); setv(178, 0.0); setv(179, 0.75); setv(180, 0.62);
            setv(181, 0.32); setv(182, 0.12); setv(183, 0.50); setv(184, 0.28);
            setv(189, 0.88); setv(190, 0.55); setv(191, 0.30);
            setv(202, 0.72); setv(204, 0.10); setv(216, 0.60);
        } else if (preset_idx == 3) {
            // BRASS FOLD
            setv(178, 0.4); setv(179, 1.0); setv(180, 0.50);
            setv(181, 0.55); setv(182, 0.52); setv(183, 0.50); setv(184, 0.26);
            setv(185, 0.45); setv(188, 0.45); setv(190, 0.72);
            setv(202, 0.62); setv(205, 0.82); setv(206, 0.32); setv(216, 0.35);
        } else if (preset_idx == 4) {
            // CRYSTAL BELL
            setv(178, 0.8); setv(179, 1.0); setv(180, 0.72);
            setv(181, 0.48); setv(182, 0.28); setv(183, 0.25); setv(184, 0.20);
            setv(187, 0.35); setv(188, 0.85); setv(189, 0.65); setv(190, 0.82); setv(191, 0.75);
            setv(202, 0.82); setv(204, 0.28); setv(215, 0.22); setv(216, 0.90);
        }
    }

    void apply_harmonic_preset(int idx) {
        if (!handler()) return;
        static constexpr double kPresets[5][8] = {
            {0.50, 0.50, 0.50, 0.50, 0.50, 0.50, 0.50, 0.50}, // FLAT
            {0.92, 0.74, 0.45, 0.30, 0.22, 0.16, 0.12, 0.08}, // WARM
            {0.85, 0.05, 0.80, 0.05, 0.70, 0.05, 0.55, 0.05}, // ODD
            {0.90, 0.85, 0.15, 0.82, 0.12, 0.15, 0.12, 0.72}, // OCT
            {0.75, 0.15, 0.60, 0.35, 0.85, 0.25, 0.78, 0.90}  // BELL
        };
        const int safe = std::clamp(idx, 0, 4);
        for (int h = 0; h < 8; ++h) change_single(207 + h, kPresets[safe][h]);
    }

    void apply_erb_curve_preset(int idx) {
        if (!handler()) return;
        const int safe = std::clamp(idx, 0, 3);
        for (int b = 0; b < 24; ++b) {
            double db = 0.0;
            if (safe == 1) { // WARMTH
                db = (b <= 5) ? 3.8 : (b >= 15 ? -2.2 : 0.0);
            } else if (safe == 2) { // CLARITY
                db = (b >= 6 && b <= 9) ? -2.8 : (b >= 16 ? 3.5 : 0.0);
            } else if (safe == 3) { // VOCAL POCKET
                db = (b >= 12 && b <= 18) ? -4.5 : (b <= 3 ? 2.5 : (b >= 21 ? 2.8 : 0.0));
            }
            change_single(221 + b, std::clamp(0.5 + db / 24.0, 0.0, 1.0));
        }
    }

    void popup_choice(int id, int x, int y) {
        if (!handler() || !window_) return;
        ParameterInfo info{};
        if (controller_->getParameterInfo(id, info) != kResultOk) return;
        const int steps = std::min(64, usable_steps(id, info.stepCount));
        if (steps <= 1) return;
        HMENU menu = CreatePopupMenu();
        for (int i = 0; i <= steps; ++i) {
            char16 text[128]{};
            const double value = static_cast<double>(i) / steps;
            if (controller_->getParamStringByValue(id, value, text) != kResultOk) continue;
            std::wstring item = to_wide(text);
            if (item.empty()) item = std::to_wstring(i);
            AppendMenuW(menu, MF_STRING, static_cast<UINT_PTR>(i + 1), item.c_str());
        }
        POINT pos{x * width_ / kCanvasW, y * height_ / kCanvasH};
        ClientToScreen(window_, &pos);
        const int selection = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY,
                                             pos.x, pos.y, 0, window_, nullptr);
        DestroyMenu(menu);
        if (selection >= 1 && selection <= steps + 1)
            change_single(id, static_cast<double>(selection - 1) / steps);
    }

    void click(int x, int y, bool reset) {
        for (auto it = hits_.rbegin(); it != hits_.rend(); ++it) {
            if (!it->contains(x, y)) continue;
            if (it->action == 1) { set_page(it->target); return; }
            if (it->action == 2) { change_single(173, (it->target + 0.5) / 8.0); return; }
            if (it->action == 4) { apply_wave_preset(it->target); return; }
            if (it->action == 10) { apply_harmonic_preset(it->target); return; }
            if (it->action == 12) { apply_erb_curve_preset(it->target); return; }
            if (it->action == 5 || it->action == 6) {
                const int id = it->target;
                ParameterInfo info{};
                if (controller_->getParameterInfo(id, info) != kResultOk) return;
                const int steps = std::max(1, usable_steps(id, info.stepCount));
                const int cur_idx = std::clamp(
                    static_cast<int>(controller_->getParamNormalized(id) * (steps + 1.0)), 0, steps);
                const int next_idx = (it->action == 5)
                    ? ((cur_idx + steps) % (steps + 1))
                    : ((cur_idx + 1) % (steps + 1));
                change_single(id, (next_idx + 0.5) / static_cast<double>(steps + 1));
                return;
            }
            if (it->action == 3 || it->action == 7) {
                if (!handler()) return;
                drag_mode_ = (it->action == 3) ? 2 : 3;
                drag_rect_ = it->rect;
                const int ix = (it->action == 3) ? 176 : 181;
                const int iy = (it->action == 3) ? 177 : 182;
                selected_id_ = ix;
                if (reset) {
                    ParameterInfo ax{}, ay{};
                    if (controller_->getParameterInfo(ix, ax) == kResultOk) change_single(ix, ax.defaultNormalizedValue);
                    if (controller_->getParameterInfo(iy, ay) == kResultOk) change_single(iy, ay.defaultNormalizedValue);
                    drag_mode_ = 0;
                    return;
                }
                begin_edit(ix);
                begin_edit(iy);
                SetCapture(window_);
                drag_xy(x, y, ix, iy);
                return;
            }
            if (it->action == 9 || it->action == 11) {
                if (!handler()) return;
                const int base_id = (it->action == 9) ? 207 : 221;
                const int count = (it->action == 9) ? 8 : 24;
                if (reset) {
                    const double w = std::max<LONG>(1, it->rect.right - it->rect.left);
                    const int idx = std::clamp(static_cast<int>(((x - it->rect.left) / w) * count), 0, count - 1);
                    change_single(base_id + idx, 0.5);
                    return;
                }
                drag_mode_ = (it->action == 9) ? 4 : 5;
                drag_rect_ = it->rect;
                sweep_base_id_ = base_id;
                sweep_count_ = count;
                for (int i = 0; i < count; ++i) begin_edit(base_id + i);
                SetCapture(window_);
                drag_sweep(x, y, base_id, count);
                return;
            }

            const int id = it->id;
            selected_id_ = id;
            ParameterInfo info{};
            if (controller_->getParameterInfo(id, info) != kResultOk) return;
            if (reset) { change_single(id, info.defaultNormalizedValue); return; }
            const int steps = usable_steps(id, info.stepCount);
            if (it->action != 8 && steps == 1) {
                change_single(id, controller_->getParamNormalized(id) < 0.5 ? 1.0 : 0.0);
                return;
            }
            if (it->action != 8 && steps > 1) {
                popup_choice(id, x, y);
                return;
            }
            if (!handler()) return;
            drag_mode_ = 1;
            dragged_id_ = id;
            drag_rect_ = it->rect;
            const int size = std::max<LONG>(1, drag_rect_.right - drag_rect_.left);
            drag_anchor_raw_ = std::clamp(static_cast<double>(x - drag_rect_.left) / size, 0.0, 1.0);
            drag_anchor_value_ = controller_->getParamNormalized(id);
            begin_edit(id);
            SetCapture(window_);
            drag(x);
            return;
        }
    }

    void add_hit(int x, int y, int w, int h, int id, int action = 0, int target = 0) {
        hits_.push_back(Hit{RECT{x, y, x + w, y + h}, id, action, target});
    }

    std::wstring param_title(int id) {
        ParameterInfo info{};
        if (controller_->getParameterInfo(id, info) != kResultOk) return L"Unknown";
        return to_wide(info.title);
    }

    std::wstring param_value(int id) {
        char16 text[128]{};
        const auto value = controller_->getParamNormalized(static_cast<ParamID>(id));
        if (controller_->getParamStringByValue(static_cast<ParamID>(id), value, text) != kResultOk) return L"-";
        return to_wide(text);
    }

    // Standard high-clarity parameter card (preserves exact click/drag bounding rect for automation tests)
    void draw_param(HDC dc, Gdiplus::Graphics& g, int x, int y, int w, int h, int id,
                    const wchar_t* alias = nullptr, COLORREF accent = kAccent) {
        ParameterInfo info{};
        if (controller_->getParameterInfo(id, info) != kResultOk) return;
        const bool active = (selected_id_ == id);
        const bool hovered = (hover_id_ == id);
        rectangle(g, x, y, w, h, (active || hovered) ? kPanelHover : kPanel,
                  active ? accent : (hovered ? RGB(104, 130, 152) : kOutline), 12);
        const std::wstring name = alias ? std::wstring(alias) : to_wide(info.title);
        const std::wstring value = param_value(id);
        const int steps = usable_steps(id, info.stepCount);
        const double norm = std::clamp(static_cast<double>(controller_->getParamNormalized(id)), 0.0, 1.0);

        const int split_w = std::max(44, (w * 54) / 100);
        label(dc, body_font_, kText, name, x + 10, y + 4, split_w - 10, h / 2 - 2);
        if (steps > 1 && w >= 160 && h >= 42) {
            label(dc, small_font_, accent, value, x + split_w, y + 4, w - split_w - 10, h / 2 - 2, DT_RIGHT);
            add_hit(x, y, w, h, id);
            // Inline < > steppers on right side for fast zero-menu discrete stepping
            const int sy = y + h - 22;
            rectangle(g, x + w - 48, sy, 18, 16, RGB(18, 24, 34), kOutline, 5);
            rectangle(g, x + w - 26, sy, 18, 16, RGB(18, 24, 34), kOutline, 5);
            label(dc, micro_font_, accent, L"<", x + w - 48, sy, 18, 16, DT_CENTER);
            label(dc, micro_font_, accent, L">", x + w - 26, sy, 18, 16, DT_CENTER);
            add_hit(x + w - 48, sy, 18, 16, id, 5, id);
            add_hit(x + w - 26, sy, 18, 16, id, 6, id);
            const int barx = x + 10, barw = w - 65, bary = y + h - 14;
            rectangle(g, barx, bary, barw, 5, kOutline, kOutline, 4);
            rectangle(g, barx, bary, std::max(4, static_cast<int>(barw * norm)), 5, accent, accent, 4);
        } else {
            label(dc, small_font_, accent, value, x + split_w, y + 4, w - split_w - 10, h / 2 - 2, DT_RIGHT);
            const int barx = x + 10, barw = w - 20, bary = y + h - 14;
            rectangle(g, barx, bary, barw, 6, kOutline, kOutline, 5);
            rectangle(g, barx, bary, std::max(4, static_cast<int>(barw * norm)), 6, accent, accent, 5);
            add_hit(x, y, w, h, id);
        }
    }

    // Anti-aliased GDI+ rotary arc knob (full rect draggable horizontally)
    void draw_knob(HDC dc, Gdiplus::Graphics& g, int x, int y, int w, int h, int id,
                   const wchar_t* short_name, COLORREF accent = kAccent) {
        ParameterInfo info{};
        if (controller_->getParameterInfo(id, info) != kResultOk) return;
        const bool active = (selected_id_ == id);
        const bool hovered = (hover_id_ == id);
        rectangle(g, x, y, w, h, (active || hovered) ? kPanelHover : kPanel,
                  active ? accent : (hovered ? RGB(104, 130, 152) : kOutline), 11, 224);

        const double norm = std::clamp(static_cast<double>(controller_->getParamNormalized(id)), 0.0, 1.0);
        if (h < 78) {
            label(dc, micro_font_, kMuted, short_name, x + 4, y + 4, w - 8, 14, DT_CENTER);
            label(dc, micro_font_, accent, param_value(id), x + 4, y + 18, w - 8, 16, DT_CENTER);
            const int barx = x + 6, barw = w - 12, bary = y + h - 11;
            rectangle(g, barx, bary, barw, 5, kOutline, kOutline, 4);
            rectangle(g, barx, bary, std::max(4, static_cast<int>(barw * norm)), 5, accent, accent, 4);
            add_hit(x, y, w, h, id);
            return;
        }
        const int r = std::max(14, std::min(w / 3, (h - 36) / 2));
        const float cx = static_cast<float>(x + w / 2);
        const float cy = static_cast<float>(y + 18 + r);

        // Background track arc (135 deg -> 405 deg = 270 deg sweep)
        Gdiplus::Pen track_pen(gp_col(RGB(44, 54, 68), 255), 4.0f);
        track_pen.SetStartCap(Gdiplus::LineCapRound);
        track_pen.SetEndCap(Gdiplus::LineCapRound);
        g.DrawArc(&track_pen, cx - r, cy - r, static_cast<float>(2 * r), static_cast<float>(2 * r), 135.0f, 270.0f);

        const float sweep = static_cast<float>(norm * 270.0);
        if (sweep > 1.5f) {
            Gdiplus::Pen glow_pen(gp_col(accent, 70), 7.5f);
            glow_pen.SetStartCap(Gdiplus::LineCapRound);
            glow_pen.SetEndCap(Gdiplus::LineCapRound);
            g.DrawArc(&glow_pen, cx - r, cy - r, static_cast<float>(2 * r), static_cast<float>(2 * r), 135.0f, sweep);

            Gdiplus::Pen val_pen(gp_col(accent, 255), 3.6f);
            val_pen.SetStartCap(Gdiplus::LineCapRound);
            val_pen.SetEndCap(Gdiplus::LineCapRound);
            g.DrawArc(&val_pen, cx - r, cy - r, static_cast<float>(2 * r), static_cast<float>(2 * r), 135.0f, sweep);
        }

        // Knob cap + indicator needle
        Gdiplus::SolidBrush cap(gp_col(RGB(17, 22, 30), 255));
        g.FillEllipse(&cap, cx - r + 5.0f, cy - r + 5.0f, static_cast<float>(2 * r - 10), static_cast<float>(2 * r - 10));
        const float ang = static_cast<float>((135.0 + norm * 270.0) * 3.14159265358979 / 180.0);
        Gdiplus::Pen needle(gp_col(kText, 240), 2.0f);
        needle.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLine(&needle, cx, cy, cx + std::cos(ang) * (r - 6.0f), cy + std::sin(ang) * (r - 6.0f));

        label(dc, micro_font_, kMuted, short_name, x + 4, y + 3, w - 8, 14, DT_CENTER);
        label(dc, micro_font_, accent, param_value(id), x + 4, y + h - 16, w - 8, 14, DT_CENTER);
        add_hit(x, y, w, h, id);
    }

    void draw_sound(HDC dc, Gdiplus::Graphics& g, int x, int y, int w, int index) {
        const int selected = std::clamp(static_cast<int>(controller_->getParamNormalized(173) * 8.0), 0, 7);
        const bool on = (selected == index);
        rectangle(g, x, y, w, 42, on ? RGB(26, 72, 74) : kPanel, on ? kAccent : kOutline, 10, 232);
        label(dc, body_font_, on ? kAccent : kText, kSounds[index], x + 9, y, w - 18, 42, DT_CENTER);
        add_hit(x, y, w, 42, 173, 2, index);
    }

    void title(HDC dc, const wchar_t* name, const wchar_t* description, int y) {
        label(dc, body_font_, kText, name, 44, y, 600, 28);
        label(dc, small_font_, kMuted, description, 44, y + 24, 960, 22);
    }

    void draw_play(HDC dc, Gdiplus::Graphics& g) {
        title(dc, L"CHOOSE A SOUND",
              L"Start with a musical idea. The eight macros work across the entire instrument.", 126);
        {
            const int morph_pct = static_cast<int>(std::round(controller_->getParamNormalized(180) * 100.0));
            const bool onion_on = controller_->getParamNormalized(192) >= 0.5;
            const int eco_mode = std::clamp(static_cast<int>(controller_->getParamNormalized(198) * 4.0), 0, 3);
            const int eco_ch = 1 + std::clamp(static_cast<int>(controller_->getParamNormalized(199) * 8.0), 0, 7);
            monkeys_ear::EcosystemStreamFrame vf{};
            const bool peer_live = eco_bus_.read_vocal(eco_ch - 1, vf);

            rectangle(g, 616, 125, 164, 38, RGB(20, 42, 50), kAccent, 10);
            label(dc, small_font_, kAccent, L"MORPH " + std::to_wstring(morph_pct) + L"% [WAVE]", 622, 125, 152, 38, DT_CENTER);
            add_hit(616, 125, 164, 38, -1, 1, 4);

            rectangle(g, 788, 125, 140, 38, onion_on ? RGB(45, 34, 62) : kPanel, onion_on ? kViolet : kOutline, 10);
            label(dc, small_font_, onion_on ? kViolet : kMuted, onion_on ? L"ONION EQ: ON" : L"ONION EQ: OFF", 792, 125, 132, 38, DT_CENTER);
            add_hit(788, 125, 140, 38, 192, 0, 0);

            const wchar_t* eco_txt = peer_live ? L"LIVE LINK" : (eco_mode == 0 ? L"OFF" : (eco_mode == 1 ? L"PUB" : L"LINK"));
            rectangle(g, 936, 125, 140, 38, peer_live ? RGB(52, 40, 18) : kPanel, (eco_mode > 0) ? kGold : kOutline, 10);
            label(dc, small_font_, (eco_mode > 0) ? kGold : kMuted,
                  L"ECO CH" + std::to_wstring(eco_ch) + L" " + eco_txt, 940, 125, 132, 38, DT_CENTER);
            add_hit(936, 125, 140, 38, -1, 1, 5);
        }
        for (int i = 0; i < 8; ++i) {
            draw_sound(dc, g, 44 + (i % 4) * 260, 183 + (i / 4) * 48, 249, i);
        }
        title(dc, L"SHAPE THE SOUND", L"Move these first. You can explore every mechanism later.", 286);
        rectangle(g, 838, 286, 238, 36, RGB(26, 72, 74), kAccent, 12);
        label(dc, small_font_, kAccent, L"GESTURE FIELD: DRIFT / FRACTURE", 844, 286, 226, 36, DT_CENTER);
        add_hit(838, 286, 238, 36, -1, 1, 3);
        constexpr const wchar_t* labels[8] = {
            L"BRIGHTNESS", L"BITE", L"HEAT", L"BODY",
            L"ECHO", L"SPACE", L"LIVE INPUT", L"CHARACTER"
        };
        for (int i = 0; i < 8; ++i)
            draw_param(dc, g, 44 + (i % 4) * 260, 338 + (i / 4) * 66, 249, 58, i, labels[i]);
        title(dc, L"AMP ENVELOPE", L"How the note begins, holds, and fades.", 464);
        constexpr const wchar_t* env[4] = {L"ATTACK", L"DECAY", L"SUSTAIN", L"RELEASE"};
        for (int i = 0; i < 4; ++i)
            draw_param(dc, g, 44 + i * 260, 514, 249, 62, 161 + i, env[i]);
        title(dc, L"RHYTHM", L"Sync to the DAW or use your own clock. Groove shapes motion, not MIDI note placement.", 587);
        constexpr int rhythm[4] = {169, 171, 174, 175};
        constexpr const wchar_t* names[4] = {L"TEMPO SYNC", L"DIVISION", L"FOLLOW DAW", L"GROOVE"};
        for (int i = 0; i < 4; ++i)
            draw_param(dc, g, 44 + i * 260, 640, 249, 56, rhythm[i], names[i]);
    }

    void draw_perform(HDC dc, Gdiplus::Graphics& g) {
        title(dc, L"PLAY THE SOUND AS A MOVING SYSTEM",
              L"Drag the pad while holding notes. DRIFT and FRACTURE influence the whole synth.", 126);
        constexpr int left = 78, top = 205, width = 734, height = 350;
        rectangle(g, left - 2, top - 2, width + 4, height + 4, RGB(14, 19, 27), kOutline, 16);
        {
            Gdiplus::Pen gpen(gp_col(kOutline, 140), 1.0f);
            for (int n = 1; n < 4; ++n) {
                g.DrawLine(&gpen, left + n * width / 4, top, left + n * width / 4, top + height);
                g.DrawLine(&gpen, left, top + n * height / 4, left + width, top + n * height / 4);
            }
        }
        const float x = std::clamp(static_cast<float>(controller_->getParamNormalized(176)), 0.f, 1.f);
        const float y = std::clamp(static_cast<float>(controller_->getParamNormalized(177)), 0.f, 1.f);
        const int markerX = left + static_cast<int>(x * width);
        const int markerY = top + height - static_cast<int>(y * height);
        Gdiplus::SolidBrush halo(gp_col(kAccent, 65));
        g.FillEllipse(&halo, markerX - 22, markerY - 22, 44, 44);
        Gdiplus::SolidBrush dot(gp_col(kAccent, 255));
        g.FillEllipse(&dot, markerX - 10, markerY - 10, 20, 20);
        add_hit(left, top, width, height, -1, 3, 0);

        label(dc, body_font_, kMuted, L"FRACTURE  /  harmonic edge", 78, 165, 510, 31);
        label(dc, body_font_, kAccent, L"DRIFT  /  rhythmic & state motion", 79, 570, 735, 34, DT_CENTER);

        rectangle(g, 839, 205, 236, 350, kPanel, kOutline, 16);
        label(dc, body_font_, kText, L"ONE GESTURE, FIVE SYSTEMS", 852, 216, 208, 36, DT_CENTER);
        draw_param(dc, g, 849, 267, 216, 67, 176, L"DRIFT");
        draw_param(dc, g, 849, 347, 216, 67, 177, L"FRACTURE");
        label(dc, small_font_, kMuted, L"DRIFT grows LFO, filter", 852, 438, 211, 24);
        label(dc, small_font_, kMuted, L"sub-motion and Sound Space.", 852, 463, 211, 24);
        label(dc, small_font_, kMuted, L"FRACTURE bends oscillator FM", 852, 497, 211, 24);
        label(dc, small_font_, kMuted, L"and the existing tube circuit.", 852, 519, 211, 24);
        label(dc, small_font_, kText, L"LOWER-LEFT = ORIGINAL PATCH   |   MOVING DIAGONALLY = COUPLED EXPERIMENT", 78, 622, 970, 33);
        label(dc, small_font_, kMuted, L"Automate both axes, use REAPER MIDI Learn, or assign controller knobs to CC20/21.", 78, 667, 975, 30);
    }

    void draw_keys(HDC dc, Gdiplus::Graphics& g) {
        title(dc, L"PLAY IT FROM YOUR MIDI KEYBOARD",
              L"Arturia and other standard MIDI keyboards work through the DAW's MIDI input.", 126);
        rectangle(g, 44, 188, 1032, 136, kPanel, kOutline, 16);
        label(dc, body_font_, kAccent, L"1  CONNECT", 62, 198, 290, 28);
        label(dc, small_font_, kText, L"Plug the keyboard into USB. In REAPER Preferences > MIDI Devices,", 62, 228, 980, 27);
        label(dc, small_font_, kText, L"enable the keyboard's normal MIDI port (not only its DAW/MCU port).", 62, 251, 980, 27);
        label(dc, small_font_, kText, L"2  ARM THE INSTRUMENT TRACK  /  INPUT: MIDI  /  MONITORING: ON", 62, 280, 980, 31);

        title(dc, L"FEEL & EXPRESSION",
              L"These controls act on the existing voice engine: no required vendor script.", 342);
        constexpr int keys[8] = {70, 71, 72, 73, 74, 75, 76, 77};
        constexpr const wchar_t* labels[8] = {
            L"MONO", L"LEGATO", L"GLIDE", L"BEND RANGE",
            L"VIBRATO DEPTH", L"VELOCITY TONE", L"PRESSURE TO FILTER", L"PRESSURE TO DRIVE"
        };
        for (int i = 0; i < 8; ++i)
            draw_param(dc, g, 44 + (i % 4) * 260, 390 + (i / 4) * 72, 249, 64, keys[i], labels[i]);

        title(dc, L"COMMON HARDWARE MESSAGES",
              L"Key presses and pads become MIDI notes; controls can also use REAPER MIDI Learn.", 557);
        rectangle(g, 44, 604, 1032, 100, kPanel, kOutline, 14);
        label(dc, small_font_, kText, L"KEYS / PADS: pitch + velocity    |    BEND WHEEL: pitch bend    |    PEDAL CC64: sustain", 61, 613, 1000, 28);
        label(dc, small_font_, kText, L"MOD WHEEL CC1 / CC74: brightness    |    CC71: bite    |    CC91: space    |    CC93: echo", 61, 640, 1000, 28);
        label(dc, small_font_, kText, L"CHANNEL PRESSURE: expressive filter/drive    |    POLY PRESSURE: individual-note timbre", 61, 667, 1000, 28);
    }

    void draw_wave_gen(HDC dc, Gdiplus::Graphics& g) {
        title(dc, L"OSCILLATOR STUDIO / 3D WAVETABLE MORPH & HARMONIC GENERATOR",
              L"Drag oscilloscope XY (X=Phase Warp, Y=Wave Fold)  |  Sweep-paint H1..H8 partials directly.", 122);

        const int waf = std::clamp(static_cast<int>(controller_->getParamNormalized(178) * 6.0), 0, 5);
        const int core_w = std::clamp(static_cast<int>(controller_->getParamNormalized(9) * 4.0), 0, 3);
        const int eff_a = (waf == 0) ? core_w : std::clamp(waf + 3, 4, 8);
        const int eff_b = std::clamp(static_cast<int>(controller_->getParamNormalized(179) * 9.0), 0, 8);
        const float morph = static_cast<float>(controller_->getParamNormalized(180));
        const float warp = static_cast<float>(controller_->getParamNormalized(181));
        const float fold = static_cast<float>(controller_->getParamNormalized(182));
        const int morph_curve = std::clamp(static_cast<int>(controller_->getParamNormalized(216) * 4.0), 0, 3);

        monkeys_ear::WaveformGeneratorParams gen{};
        gen.shape = static_cast<float>(controller_->getParamNormalized(188));
        gen.odd_even = static_cast<float>(controller_->getParamNormalized(189));
        gen.tilt = static_cast<float>(controller_->getParamNormalized(190));
        gen.formant = static_cast<float>(controller_->getParamNormalized(191));
        gen.phase_warp = warp;
        gen.wave_fold = fold;
        gen.morph_curve = morph_curve;
        for (int h = 0; h < 8; ++h) {
            gen.harmonic_bars[static_cast<size_t>(h)] =
                static_cast<float>(controller_->getParamNormalized(207 + h));
        }

        // Left Top: 3D Isometric Waterfall + Interactive Glowing Oscilloscope (sx=44, sy=176, sw=566, sh=172)
        constexpr int sx = 44, sy = 176, sw = 566, sh = 172;
        rectangle(g, sx, sy, sw, sh, RGB(10, 14, 21), kOutline, 14, 242);
        {
            Gdiplus::Pen gpen(gp_col(RGB(34, 44, 60), 180), 1.0f);
            for (int i = 1; i < 4; ++i) {
                g.DrawLine(&gpen, sx + i * sw / 4, sy + 8, sx + i * sw / 4, sy + sh - 8);
                g.DrawLine(&gpen, sx + 10, sy + i * sh / 4, sx + sw - 10, sy + i * sh / 4);
            }
        }

        // 7 faint isometric morph waterfall slices stacked in perspective
        std::array<float, 128> prev{};
        for (int sl = 6; sl >= 0; --sl) {
            const float tm = static_cast<float>(sl) / 6.0f;
            monkeys_ear::WaveformBank::render_preview(
                static_cast<monkeys_ear::WaveFamily>(eff_a),
                static_cast<monkeys_ear::WaveFamily>(eff_b), tm, gen, prev);
            std::array<Gdiplus::PointF, 128> pts{};
            const float iso_dx = (static_cast<float>(sl) - 3.0f) * 8.0f;
            const float iso_dy = (3.0f - static_cast<float>(sl)) * 11.0f;
            const float base_y = sy + sh * 0.50f + iso_dy;
            const float amp_s = sh * 0.25f;
            for (size_t i = 0; i < 128; ++i) {
                pts[i].X = sx + 28.0f + iso_dx + (static_cast<float>(i) / 127.0f) * (sw - 56.0f);
                pts[i].Y = base_y - std::clamp(prev[i], -1.2f, 1.2f) * amp_s;
            }
            const BYTE alpha = static_cast<BYTE>(34 + (6 - sl) * 9);
            Gdiplus::Pen sp(gp_col(sl == 6 ? kGold : kAccent, alpha), 1.3f);
            g.DrawLines(&sp, pts.data(), 128);
        }

        // Foreground crystal-sharp morphed waveform trace with neon glow
        monkeys_ear::WaveformBank::render_preview(
            static_cast<monkeys_ear::WaveFamily>(eff_a),
            static_cast<monkeys_ear::WaveFamily>(eff_b), morph, gen, prev);
        {
            std::array<Gdiplus::PointF, 128> pts{};
            const float mid_y = sy + sh * 0.52f;
            const float amp_s = sh * 0.36f;
            for (size_t i = 0; i < 128; ++i) {
                pts[i].X = sx + 16.0f + (static_cast<float>(i) / 127.0f) * (sw - 32.0f);
                pts[i].Y = mid_y - std::clamp(prev[i], -1.2f, 1.2f) * amp_s;
            }
            Gdiplus::Pen glow1(gp_col(kAccent, 55), 8.0f);
            Gdiplus::Pen glow2(gp_col(kAccent, 140), 4.0f);
            Gdiplus::Pen core(gp_col(RGB(220, 255, 250), 255), 2.0f);
            g.DrawLines(&glow1, pts.data(), 128);
            g.DrawLines(&glow2, pts.data(), 128);
            g.DrawLines(&core, pts.data(), 128);
        }
        // Warp/Fold interactive XY reticle dot on oscilloscope
        {
            const int rx = sx + 16 + static_cast<int>(warp * (sw - 32));
            const int ry = sy + sh - 14 - static_cast<int>(fold * (sh - 28));
            Gdiplus::Pen ring(gp_col(kGold, 200), 1.5f);
            g.DrawEllipse(&ring, rx - 7, ry - 7, 14, 14);
        }
        add_hit(sx, sy, sw, sh, 181, 7, 0);
        label(dc, small_font_, kAccent,
              L"WAVE: " + ascii_wide(monkeys_ear::kWaveFamilyNames[eff_a]) +
              L" -> " + ascii_wide(monkeys_ear::kWaveFamilyNames[eff_b]) +
              L"  (" + std::to_wstring(static_cast<int>(std::round(morph * 100.0f))) +
              L"% | Warp " + std::to_wstring(static_cast<int>(std::round(warp * 100.0f))) +
              L"% | Fold " + std::to_wstring(static_cast<int>(std::round(fold * 100.0f))) + L"%)",
              sx + 14, sy + 6, sw - 28, 20);

        // Morph Gradient Ribbon directly under oscilloscope (param 180)
        {
            const int mx = sx, my = 354, mw = sw, mh = 22;
            rectangle(g, mx, my, mw, mh, RGB(15, 20, 29), kOutline, 8);
            Gdiplus::LinearGradientBrush grad(
                Gdiplus::Point(mx, my), Gdiplus::Point(mx + mw, my),
                gp_col(kAccent, 180), gp_col(kGold, 210));
            const int fw = std::max(8, static_cast<int>(morph * (mw - 4)));
            Gdiplus::GraphicsPath gp;
            gp_round_rect_path(gp, static_cast<Gdiplus::REAL>(mx + 2), static_cast<Gdiplus::REAL>(my + 2),
                               static_cast<Gdiplus::REAL>(fw), static_cast<Gdiplus::REAL>(mh - 4), 4.0f);
            g.FillPath(&grad, &gp);
            label(dc, micro_font_, kText,
                  L"MORPH RIBBON A -> B: " + std::to_wstring(static_cast<int>(std::round(morph * 100.0f))) + L"%",
                  mx + 10, my, mw - 20, mh, DT_CENTER);
            add_hit(mx, my, mw, mh, 180, 8, 0);
        }

        // 5 Archetype Wave Pills
        constexpr const wchar_t* wpresets[5] = {L"INIT SAW", L"VOCAL MORPH", L"HOLLOW REED", L"BRASS FOLD", L"CRYSTAL BELL"};
        for (int i = 0; i < 5; ++i) {
            const int bx = sx + i * 114;
            rectangle(g, bx, 382, 108, 28, RGB(24, 32, 44), kGold, 8);
            label(dc, micro_font_, kGold, wpresets[i], bx + 4, 382, 100, 28, DT_CENTER);
            add_hit(bx, 382, 108, 28, -1, 4, i);
        }

        // Right-Top: Dual-Oscillator & Morph Matrix (624, 176, 452, 234)
        rectangle(g, 624, 176, 452, 234, RGB(19, 25, 34), kOutline, 14, 232);
        label(dc, small_font_, kAccent, L"DUAL OSCILLATOR & MORPH MATRIX", 636, 182, 340, 20);
        draw_param(dc, g, 634, 206, 212, 44, 178, L"WAVE A", kAccent);
        draw_param(dc, g, 854, 206, 212, 44, 9, L"CORE (A=0)", kAccent);
        draw_param(dc, g, 634, 254, 212, 44, 179, L"WAVE B", kGold);
        draw_param(dc, g, 854, 254, 212, 44, 202, L"OSC 2 WAVE", kGold);
        draw_param(dc, g, 634, 302, 212, 44, 216, L"CURVE", kViolet);
        draw_knob(dc, g, 854, 302, 68, 100, 215, L"AUTO Hz", kViolet);
        draw_knob(dc, g, 926, 302, 68, 100, 203, L"OSC2 c", kGold);
        draw_knob(dc, g, 998, 302, 68, 100, 204, L"RING", kCoral);
        draw_knob(dc, g, 634, 350, 68, 52, 186, L"ENV->M", kViolet);
        draw_knob(dc, g, 706, 350, 68, 52, 187, L"LFO->M", kViolet);
        draw_knob(dc, g, 778, 350, 68, 52, 217, L"OSC BAL", kAccent);

        // Bottom-Left: Custom Waveform Generator + Interactive Sweep-Paint H1..H8 Drawbars (44, 418, 566, 280)
        rectangle(g, 44, 418, 566, 280, RGB(19, 25, 34), kOutline, 14, 232);
        label(dc, body_font_, kGold, L"CUSTOM GENERATOR & PARTIALS", 56, 424, 300, 22);
        constexpr const wchar_t* hpre[5] = {L"FLAT", L"WARM", L"ODD", L"OCT", L"BELL"};
        for (int i = 0; i < 5; ++i) {
            const int px = 362 + i * 48;
            rectangle(g, px, 424, 45, 20, RGB(28, 36, 48), kAccent, 5);
            label(dc, micro_font_, kAccent, hpre[i], px + 1, 424, 43, 20, DT_CENTER);
            add_hit(px, 424, 45, 20, -1, 10, i);
        }
        draw_knob(dc, g, 56, 452, 130, 96, 188, L"ARCHETYPE", kGold);
        draw_knob(dc, g, 192, 452, 130, 96, 189, L"ODD / EVEN", kGold);
        draw_knob(dc, g, 328, 452, 130, 96, 190, L"TILT", kGold);
        draw_knob(dc, g, 464, 452, 130, 96, 191, L"FORMANT", kGold);

        // Interactive Sweep-Paint H1..H8 Partial Bars + faint H9..H16 tail preview
        {
            const int gx = 56, gy = 554, gw = 352, gh = 134;
            rectangle(g, gx, gy, gw, gh, RGB(12, 16, 23), kOutline, 10);
            for (int h = 0; h < 8; ++h) {
                const float v = static_cast<float>(controller_->getParamNormalized(207 + h));
                const int bw = 34, bx = gx + 12 + h * 42;
                const int bh = std::max(4, static_cast<int>(v * (gh - 30)));
                const COLORREF col = (v > 0.6f) ? kGold : kAccent;
                rectangle(g, bx, gy + gh - 20 - bh, bw, bh, col, col, 5, 225);
                label(dc, micro_font_, kMuted, L"H" + std::to_wstring(h + 1), bx, gy + gh - 18, bw, 16, DT_CENTER);
            }
            label(dc, micro_font_, kAccent, L"DRAG / SWEEP PARTIALS H1..H8", gx + 8, gy + 3, 240, 15);
            add_hit(gx, gy, gw, gh, 207, 9, 0);

            // Resulting H1..H16 harmonic spectrum preview on right of generator card
            const int sxp = 418, syp = 554, swp = 178, shp = 134;
            rectangle(g, sxp, syp, swp, shp, RGB(12, 16, 23), kOutline, 10);
            std::array<float, 16> amps{};
            monkeys_ear::WaveformBank::evaluate_generator_harmonics(gen, amps);
            for (int h = 0; h < 16; ++h) {
                const float mag = std::clamp(amps[static_cast<size_t>(h)] * 0.55f, 0.03f, 1.0f);
                const int bx = sxp + 8 + h * 10;
                const int bh = std::max(3, static_cast<int>(mag * (shp - 28)));
                rectangle(g, bx, syp + shp - 16 - bh, 7, bh, (h < 8) ? kGold : kViolet, kAccent, 3);
            }
            label(dc, micro_font_, kMuted, L"OUT SPECTRUM H1..16", sxp + 6, syp + 3, swp - 12, 15, DT_CENTER);
        }

        // Bottom-Right: Deep Stereo Unison, Analog Drift & Modulation (624, 418, 452, 280)
        rectangle(g, 624, 418, 452, 280, RGB(19, 25, 34), kOutline, 14, 232);
        label(dc, body_font_, kAccent, L"STEREO UNISON, ANALOG DRIFT & SYNC", 636, 424, 420, 22);
        draw_param(dc, g, 634, 452, 212, 46, 183, L"UNISON", kAccent);
        draw_knob(dc, g, 852, 452, 104, 114, 184, L"UNI DETUNE", kAccent);
        draw_knob(dc, g, 962, 452, 104, 114, 205, L"STEREO WIDTH", kEmerald);
        draw_param(dc, g, 634, 504, 212, 46, 12, L"HARD SYNC", kCoral);
        draw_knob(dc, g, 634, 558, 104, 130, 10, L"CROSS-FM", kAccent);
        draw_knob(dc, g, 742, 558, 104, 130, 11, L"OSC2 SEMI", kGold);
        draw_knob(dc, g, 852, 572, 104, 116, 206, L"ANALOG DRIFT", kGold);
        draw_knob(dc, g, 962, 572, 104, 116, 185, L"SUB HARM", kViolet);
    }

    void draw_onion_eco(HDC dc, Gdiplus::Graphics& g) {
        title(dc, L"ONION SKIN 24-BAND ERB EQ & CONNECTED ECOSYSTEM",
              L"Drag anywhere across the 24 critical bands (40 Hz - 9.6 kHz) to sculpt +/-12 dB + live peer carving.", 122);

        const int eco_ch = std::clamp(static_cast<int>(controller_->getParamNormalized(199) * 8.0), 0, 7);
        const int eco_mode = std::clamp(static_cast<int>(controller_->getParamNormalized(198) * 4.0), 0, 3);
        const bool onion_on = controller_->getParamNormalized(192) >= 0.5;
        const float tilt_db = static_cast<float>((controller_->getParamNormalized(218) - 0.5) * 12.0);
        const float air_db = static_cast<float>((controller_->getParamNormalized(220) - 0.5) * 12.0);
        monkeys_ear::EcosystemStreamFrame synth_f{}, vocal_f{};
        const bool has_synth = eco_bus_.read_synth(eco_ch, synth_f);
        const bool has_vocal = eco_bus_.read_vocal(eco_ch, vocal_f);

        // Top-Left: Full 24-Band Psychoacoustic ERB Analyzer & Interactive Curve Sculpt (44, 176, 630, 294)
        constexpr int ax = 44, ay = 176, aw = 630, ah = 294;
        rectangle(g, ax, ay, aw, ah, RGB(10, 14, 21), kOutline, 14, 244);
        label(dc, body_font_, kViolet, L"24-BAND COCHLEAR ERB EQ (CH " + std::to_wstring(eco_ch + 1) + L")", ax + 14, ay + 6, 305, 22);

        // 4 Quick Surgical EQ Curves right in panel top-right
        constexpr const wchar_t* eqpre[4] = {L"FLAT", L"WARM", L"CLARITY", L"POCKET"};
        for (int i = 0; i < 4; ++i) {
            const int px = ax + 320 + i * 74;
            rectangle(g, px, ay + 7, 70, 22, RGB(24, 30, 42), kEmerald, 6);
            label(dc, micro_font_, kEmerald, eqpre[i], px + 2, ay + 7, 66, 22, DT_CENTER);
            add_hit(px, ay + 7, 70, 22, -1, 12, i);
        }

        // Interactive 24-Band Canvas Rect
        constexpr int gx = ax + 16, gy = ay + 36, gw = aw - 32, gh = ah - 82;
        rectangle(g, gx, gy, gw, gh, RGB(14, 19, 27), kOutline, 10);
        const int zero_y = gy + gh / 2;
        {
            Gdiplus::Pen zero_pen(gp_col(RGB(68, 80, 96), 200), 1.0f);
            g.DrawLine(&zero_pen, gx + 4, zero_y, gx + gw - 4, zero_y);
            Gdiplus::Pen grid_pen(gp_col(RGB(34, 42, 56), 140), 1.0f);
            g.DrawLine(&grid_pen, gx + 4, gy + gh / 4, gx + gw - 4, gy + gh / 4);
            g.DrawLine(&grid_pen, gx + 4, gy + (3 * gh) / 4, gx + gw - 4, gy + (3 * gh) / 4);
        }

        std::array<Gdiplus::PointF, 24> eq_pts{};
        const float col_w = static_cast<float>(gw - 16) / 24.0f;
        for (int b = 0; b < 24; ++b) {
            const float cx = gx + 8.0f + (static_cast<float>(b) + 0.5f) * col_w;
            const float s_occ = has_synth ? std::clamp(synth_f.spectral_occupancy[static_cast<size_t>(b)], 0.0f, 1.0f) : 0.06f;
            const float v_occ = has_vocal ? std::clamp(vocal_f.spectral_occupancy[static_cast<size_t>(b)], 0.0f, 1.0f) : 0.0f;
            const float sh = s_occ * (gh * 0.85f);
            const float vh = v_occ * (gh * 0.85f);
            Gdiplus::SolidBrush sbr(gp_col(kAccent, 85));
            g.FillRectangle(&sbr, cx - col_w * 0.36f, static_cast<float>(gy + gh - 4) - sh, col_w * 0.38f, sh);
            if (vh > 2.0f) {
                Gdiplus::SolidBrush vbr(gp_col(kGold, 110));
                g.FillRectangle(&vbr, cx + 1.0f, static_cast<float>(gy + gh - 4) - vh, col_w * 0.38f, vh);
            }

            const float user_db = static_cast<float>((controller_->getParamNormalized(221 + b) - 0.5) * 24.0);
            const float pos_norm = (static_cast<float>(b) / 23.0f) * 2.0f - 1.0f;
            const float tilt_part = pos_norm * tilt_db * 0.55f;
            const float air_part = (b >= 18) ? (air_db * (static_cast<float>(b - 17) / 6.0f) * 0.65f) : 0.0f;
            const float net_db = std::clamp(user_db + tilt_part + air_part, -12.0f, 12.0f);
            const float py = zero_y - (net_db / 12.0f) * (gh * 0.44f);
            eq_pts[static_cast<size_t>(b)] = Gdiplus::PointF(cx, py);
        }
        {
            Gdiplus::Pen eq_glow(gp_col(kEmerald, 80), 6.0f);
            Gdiplus::Pen eq_line(gp_col(kEmerald, 255), 2.2f);
            g.DrawCurve(&eq_glow, eq_pts.data(), 24, 0.35f);
            g.DrawCurve(&eq_line, eq_pts.data(), 24, 0.35f);
            for (int b = 0; b < 24; ++b) {
                const Gdiplus::PointF& p = eq_pts[static_cast<size_t>(b)];
                Gdiplus::SolidBrush node_br(gp_col(kText, 230));
                g.FillEllipse(&node_br, p.X - 3.0f, p.Y - 3.0f, 6.0f, 6.0f);
            }
        }
        add_hit(gx, gy, gw, gh, 221, 11, 0);

        // Frequency markers along 24 ERB bands
        constexpr int tick_idx[7] = {0, 3, 7, 11, 15, 19, 23};
        constexpr const wchar_t* tick_lbl[7] = {L"40Hz", L"150Hz", L"400Hz", L"878Hz", L"1.8k", L"3.6k", L"9.6k"};
        for (int t = 0; t < 7; ++t) {
            const int tx = static_cast<int>(eq_pts[static_cast<size_t>(tick_idx[t])].X) - 24;
            label(dc, micro_font_, kMuted, tick_lbl[t], tx, gy + gh + 2, 48, 16, DT_CENTER);
        }

        wchar_t peer_info[140]{};
        const float hz = has_vocal ? (440.0f * std::pow(2.0f, vocal_f.pitch_cents / 1200.0f)) : 0.0f;
        std::swprintf(peer_info, 140,
                      L"CYAN=SELF  GOLD=PEER  EMERALD=24-BAND  |  VOCAL: %.1fHz (%.0f%%)  |  %ls",
                      hz, has_vocal ? (vocal_f.pitch_confidence * 100.0f) : 0.0f,
                      onion_on ? L"ONION:CARVE" : L"SCULPT ONLY");
        label(dc, micro_font_, has_vocal ? kGold : kMuted, peer_info, ax + 16, ay + ah - 22, aw - 32, 18);

        // Top-Right: Connected Ecosystem Bus (686, 176, 390, 294)
        rectangle(g, 686, 176, 390, 294, kPanel, kOutline, 14, 232);
        label(dc, body_font_, kGold, L"CONNECTED ECOSYSTEM BUS", 700, 184, 340, 22);
        label(dc, small_font_, kMuted, L"Lock-free cross-plugin 24-ERB spectrum & pitch link", 700, 206, 360, 18);
        const wchar_t* status = has_vocal
            ? (has_synth ? L"BI-DIRECTIONAL LINK LIVE" : L"VOCAL PEER DETECTED")
            : (has_synth ? L"PUBLISHING 24 ERB BANDS" : L"IDLE (PLAY OR ROUTE VOCAL)");
        rectangle(g, 700, 232, 362, 26, has_vocal ? RGB(44, 34, 14) : RGB(18, 23, 31),
                  has_vocal ? kGold : kOutline, 8);
        label(dc, micro_font_, has_vocal ? kGold : kMuted, status, 704, 232, 354, 26, DT_CENTER);
        draw_param(dc, g, 700, 268, 176, 56, 198, L"ECO LINK", kGold);
        draw_param(dc, g, 886, 268, 176, 56, 199, L"ECO CH", kGold);
        draw_param(dc, g, 700, 334, 362, 56, 200, L"PEER MASK WEIGHT", kGold);
        draw_param(dc, g, 700, 398, 362, 56, 201, L"ECO PITCH FOLLOW", eco_mode >= 2 ? kAccent : kMuted);

        // Bottom: 24-Band Onion Skin & Mastering Controls (44, 480, 1032, 218)
        rectangle(g, 44, 480, 1032, 218, kPanel, kOutline, 14, 232);
        label(dc, body_font_, kViolet, L"ONION SKIN DYNAMIC PROJECTION GATE & MASTERING TONE", 58, 488, 620, 22);
        label(dc, small_font_, kMuted, L"0-latency 24-band ERB carving (cos >= 0.86 gate) + Tilt, Surgical Q & Air Lift", 58, 510, 960, 18);
        draw_param(dc, g, 58, 536, 210, 56, 192, L"ONION EQ", onion_on ? kViolet : kMuted);
        draw_param(dc, g, 276, 536, 210, 56, 193, L"INTENT", kViolet);
        draw_knob(dc, g, 498, 536, 90, 148, 194, L"AMOUNT", kViolet);
        draw_knob(dc, g, 594, 536, 90, 148, 195, L"SENSITIV", kViolet);
        draw_knob(dc, g, 690, 536, 90, 148, 196, L"LOW PROT", kAccent);
        draw_knob(dc, g, 786, 536, 90, 148, 197, L"TRANS PT", kGold);
        draw_knob(dc, g, 882, 536, 90, 148, 218, L"SPEC TILT", kEmerald);
        draw_knob(dc, g, 978, 536, 86, 148, 219, L"Q FOCUS", kEmerald);
        draw_param(dc, g, 58, 604, 428, 80, 220, L"AIR SHIMMER (> 4.2 kHz)", kAccent);
    }

    void draw_explore(HDC dc, Gdiplus::Graphics& g) {
        title(dc, L"EXPLORE THE ENGINE",
              L"Pick an area or search every parameter. Deep controls remain available, never required.", 126);
        label(dc, small_font_, kMuted, L"AREA", 44, 166, 120, 32);
        label(dc, small_font_, kMuted, L"SEARCH", 506, 166, 82, 32);
        std::vector<int> filtered;
        for (int id = 0; id < controller_->getParameterCount(); ++id) {
            ParameterInfo info{};
            if (controller_->getParameterInfo(id, info) != kResultOk) continue;
            if (info.flags & kIsHidden) continue;
            if (search_text_.empty() && selected_area_ != 0 && info.unitId != selected_area_) continue;
            if (!search_text_.empty() && !contains_ci(to_wide(info.title), search_text_)) continue;
            filtered.push_back(id);
        }
        std::wstring count = std::to_wstring(filtered.size()) + L" controls";
        label(dc, small_font_, kMuted, count, 44, 220, 400, 24);
        const int visible = 7;
        scroll_rows_ = std::clamp(scroll_rows_, 0, std::max(0, static_cast<int>(filtered.size()) - visible));
        if (filtered.empty())
            label(dc, body_font_, kMuted, L"No controls match. Try another area or search.", 44, 298, 880, 40);
        for (int i = 0; i < visible && scroll_rows_ + i < static_cast<int>(filtered.size()); ++i)
            draw_param(dc, g, 44, 254 + i * 62, 1032, 55, filtered[scroll_rows_ + i]);
        if (static_cast<int>(filtered.size()) > visible) {
            label(dc, small_font_, kMuted,
                  L"Mouse wheel to browse    " + std::to_wstring(scroll_rows_ + 1) +
                  L" - " + std::to_wstring(std::min(static_cast<int>(filtered.size()), scroll_rows_ + visible)),
                  44, 694, 980, 24);
        }
    }

    void render(HDC dc) {
        hits_.clear();
        RECT rect{0, 0, kCanvasW, kCanvasH};
        HBRUSH bg = CreateSolidBrush(kBackground);
        FillRect(dc, &rect, bg);
        DeleteObject(bg);

        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

        if (chassis_bmp_) {
            g.DrawImage(chassis_bmp_.get(), 0, 0, kCanvasW, kCanvasH);
            // Darken recessed interior slightly so typography & waveforms keep crisp 12:1 legibility
            Gdiplus::SolidBrush veil(gp_col(kBackground, 148));
            g.FillRectangle(&veil, 28, 102, kCanvasW - 56, kCanvasH - 122);
        }

        if (crest_bmp_) {
            g.DrawImage(crest_bmp_.get(), 42, 18, 46, 46);
        }
        label(dc, title_font_, kText, L"MONKEY'S EAR", crest_bmp_ ? 96 : 44, 12, 510, 52);
        label(dc, small_font_, kMuted, L"PLAY FIRST  /  STUDIO WAVEFORM + 24-ERB PSYCHOACOUSTIC ENGINE",
              crest_bmp_ ? 96 : 44, 65, 515, 24);

        // Preserve exact tab & output button rects across the header bar
        draw_param(dc, g, 616, 23, 170, 46, 8, L"OUTPUT");
        rectangle(g, 796, 22, 63, 37, page_ == 0 ? RGB(26, 72, 74) : kPanel, page_ == 0 ? kAccent : kOutline, 10);
        label(dc, body_font_, page_ == 0 ? kAccent : kText, L"PLAY", 796, 22, 63, 37, DT_CENTER);
        add_hit(796, 22, 63, 37, -1, 1, 0);

        rectangle(g, 864, 22, 89, 37, page_ == 3 ? RGB(26, 72, 74) : kPanel, page_ == 3 ? kAccent : kOutline, 10);
        label(dc, body_font_, page_ == 3 ? kAccent : kText, L"PERFORM", 864, 22, 89, 37, DT_CENTER);
        add_hit(864, 22, 89, 37, -1, 1, 3);

        rectangle(g, 958, 22, 54, 37, page_ == 2 ? RGB(26, 72, 74) : kPanel, page_ == 2 ? kAccent : kOutline, 10);
        label(dc, body_font_, page_ == 2 ? kAccent : kText, L"KEYS", 958, 22, 54, 37, DT_CENTER);
        add_hit(958, 22, 54, 37, -1, 1, 2);

        rectangle(g, 1017, 22, 60, 37, page_ == 1 ? RGB(26, 72, 74) : kPanel, page_ == 1 ? kAccent : kOutline, 10);
        label(dc, body_font_, page_ == 1 ? kAccent : kText, L"DEEP", 1017, 22, 60, 37, DT_CENTER);
        add_hit(1017, 22, 60, 37, -1, 1, 1);

        rectangle(g, 796, 64, 136, 34, page_ == 4 ? RGB(26, 72, 74) : kPanel, kAccent, 10);
        label(dc, small_font_, page_ == 4 ? kAccent : kGold, L"WAVE / GEN", 796, 64, 136, 34, DT_CENTER);
        add_hit(796, 64, 136, 34, -1, 1, 4);

        rectangle(g, 938, 64, 139, 34, page_ == 5 ? RGB(48, 34, 68) : kPanel, kViolet, 10);
        label(dc, small_font_, page_ == 5 ? kViolet : kText, L"ONION / ECO", 938, 64, 139, 34, DT_CENTER);
        add_hit(938, 64, 139, 34, -1, 1, 5);

        {
            Gdiplus::Pen sep(gp_col(kOutline, 220), 1.5f);
            g.DrawLine(&sep, 44, 107, 1076, 107);
        }

        if (page_ == 0) draw_play(dc, g);
        else if (page_ == 2) draw_keys(dc, g);
        else if (page_ == 3) draw_perform(dc, g);
        else if (page_ == 4) draw_wave_gen(dc, g);
        else if (page_ == 5) draw_onion_eco(dc, g);
        else draw_explore(dc, g);

        const bool connected = handler() != nullptr;
        const int inspect_id = (hover_id_ >= 0) ? hover_id_ : selected_id_;
        std::wstring footer;
        if (!connected) {
            footer = L"HOST EDIT HANDLER MISSING - EDITS DISABLED";
        } else if (inspect_id >= 0) {
            footer = L"[PARAM] " + param_title(inspect_id) + L"  =  " + param_value(inspect_id) +
                     L"    |    Drag (Shift=fine)  |  Wheel  |  Right/Double-click=Reset";
        } else {
            footer = L"Drag (Shift=fine)  |  Wheel adjust  |  Right-click reset  |  Sweep-paint H1..H8 & 24 ERB bands";
        }
        label(dc, small_font_, connected ? ((inspect_id >= 0) ? kAccent : kMuted) : RGB(255, 175, 113),
              footer, 44, 708, 1032, 22);
    }

    std::atomic<uint32> references_{1};
    IEditController* controller_;
    IComponentHandler** handler_slot_;
    IPlugFrame* frame_ = nullptr;
    HMODULE module_ = nullptr;
    HWND window_ = nullptr, combo_ = nullptr, search_ = nullptr;
    HFONT title_font_ = nullptr, body_font_ = nullptr, small_font_ = nullptr, micro_font_ = nullptr;
    std::unique_ptr<Gdiplus::Bitmap> chassis_bmp_;
    std::unique_ptr<Gdiplus::Bitmap> crest_bmp_;
    int width_ = kCanvasW, height_ = kCanvasH, page_ = 0, selected_area_ = 0, scroll_rows_ = 0;
    int selected_id_ = -1, dragged_id_ = -1, hover_id_ = -1;
    int drag_mode_ = 0; // 0=none, 1=single, 2=performXY, 3=scopeXY, 4=H-bars, 5=ERB-bars
    int sweep_base_id_ = -1, sweep_count_ = 0, anim_tick_ = 0;
    double drag_anchor_raw_ = 0.0, drag_anchor_value_ = 0.0;
    RECT drag_rect_{};
    monkeys_ear::EcosystemBusClient eco_bus_{};
    std::wstring search_text_;
    std::vector<Hit> hits_;
    std::vector<double> last_host_values_;
};

IPlugView* create_editor(IEditController* controller, IComponentHandler** handler_slot) {
    return controller ? static_cast<IPlugView*>(new NativeEditor(controller, handler_slot)) : nullptr;
}
} // namespace monkeys_ear::ui

#else
namespace monkeys_ear::ui {
Steinberg::IPlugView* create_editor(Steinberg::Vst::IEditController*,
                                   Steinberg::Vst::IComponentHandler**) {
    return nullptr; // existing Windows/VST3 instrument target only
}
} // namespace monkeys_ear::ui
#endif
