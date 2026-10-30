#include "native_editor.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cwctype>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

namespace monkeys_ear::ui {
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int kCanvasW=1120;
constexpr int kCanvasH=740;
constexpr int kComboId=10301;
constexpr int kSearchId=10302;
constexpr wchar_t kWindowClass[]=L"MonkeysEarNativeEditor_01";
int kModuleAnchor=0; // anchored inside the DLL, never the host executable
std::atomic<int> gAttachedViews{0};
constexpr COLORREF kBackground=RGB(15,19,25);
constexpr COLORREF kPanel=RGB(27,33,43);
constexpr COLORREF kPanelHover=RGB(36,44,56);
constexpr COLORREF kText=RGB(236,241,247);
constexpr COLORREF kMuted=RGB(165,178,190);
constexpr COLORREF kAccent=RGB(96,213,189);
constexpr COLORREF kOutline=RGB(59,70,83);
constexpr const wchar_t* kAreas[]={
    L"All parameters",L"PLAY & Macros",L"Oscillators / Voice",
    L"Filters / Envelopes",L"Four-band EQ",L"Rhythm / Modulation",
    L"ChronoState",L"Sound Space",L"Vocal Expression",
    L"Tube Rack",L"Tube 1",L"Tube 2",L"Tube 3",L"Tube 4",
    L"Master / Routing"
};
constexpr const wchar_t* kSounds[]={
    L"CURRENT",L"MONOLITH",L"FERAL WOBBLE",L"VELVET LEAD",
    L"TUBE FOUNDRY",L"CLOCKWORK SUB",L"GLASS CHAMBER",L"SOUND SPACE"
};
std::wstring to_wide(const char16* text) {
    std::wstring out;
    if(!text)return out;
    for(size_t i=0;i<127&&text[i];++i)out+=static_cast<wchar_t>(text[i]);
    return out;
}
bool contains_ci(const std::wstring& hay,const std::wstring& needle) {
    if(needle.empty())return true;
    std::wstring a=hay,b=needle;
    for(wchar_t& x:a)x=static_cast<wchar_t>(towlower(x));
    for(wchar_t& x:b)x=static_cast<wchar_t>(towlower(x));
    return a.find(b)!=std::wstring::npos;
}
void rectangle(HDC h,int x,int y,int w,int ht,COLORREF fill,COLORREF outline,int rad=14) {
    HBRUSH brush=CreateSolidBrush(fill);
    HPEN pen=CreatePen(PS_SOLID,1,outline);
    HGDIOBJ oldB=SelectObject(h,brush),oldP=SelectObject(h,pen);
    RoundRect(h,x,y,x+w,y+ht,rad,rad);
    SelectObject(h,oldB);SelectObject(h,oldP);
    DeleteObject(brush);DeleteObject(pen);
}
void label(HDC h,HFONT font,COLORREF color,const std::wstring& text,int x,int y,int w,int ht,UINT style=DT_LEFT) {
    HGDIOBJ old=SelectObject(h,font);
    SetBkMode(h,TRANSPARENT);
    SetTextColor(h,color);
    RECT r{x,y,x+w,y+ht};
    DrawTextW(h,text.c_str(),-1,&r,style|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    SelectObject(h,old);
}
// Preserve existing host parameter ABI/metadata while presenting real
// discrete sound-design decisions as menus in the custom view.
int usable_steps(int id,int advertised) {
    if(id==9)return 3;  // waveform
    if(id==18)return 2; // input routing
    if(id==21)return 3; // subharmonic ratio
    if(id==28||id==30)return 4; // filter modes
    if(id==31)return 2; // filter connection
    if(id==56)return 4; // LFO shape
    if(id>=37&&id<=52&&(id-37)%4==0)return 5; // EQ type
    switch(id){
    case 12:case 17:case 23:case 34:case 35:case 53:case 54:
    case 70:case 71:case 82:case 113:return 1;
    default:return advertised;
    }
}
struct Hit {
    RECT rect{};
    int id=-1;
    int action=0;  // 0 parameter, 1 page, 2 preset
    int target=0;
    bool contains(int x,int y) const {
        return x>=rect.left&&x<rect.right&&y>=rect.top&&y<rect.bottom;
    }
};
} // namespace

class NativeEditor final : public IPlugView {
public:
    NativeEditor(IEditController* c,IComponentHandler** handler)
        : controller_(c),handler_slot_(handler) {
        controller_->addRef();
        title_font_=CreateFontW(-30,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        body_font_=CreateFontW(-17,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        small_font_=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    }
    ~NativeEditor() override {
        removed();
        if(title_font_)DeleteObject(title_font_);
        if(body_font_)DeleteObject(body_font_);
        if(small_font_)DeleteObject(small_font_);
        controller_->release();
    }
    tresult SMTG_STDCALL queryInterface(const TUID iid,void** out) override {
        if(!out)return kInvalidArgument;
        if(memcmp(iid,FUnknown_iid,sizeof(TUID))==0 ||
           memcmp(iid,IPlugView_iid,sizeof(TUID))==0) {
            *out=static_cast<IPlugView*>(this);addRef();return kResultOk;
        }
        *out=nullptr;return kResultFalse;
    }
    uint32 SMTG_STDCALL addRef() override { return ++references_; }
    uint32 SMTG_STDCALL release() override {
        const uint32 count=--references_;
        if(count==0)delete this;
        return count;
    }
    tresult SMTG_STDCALL isPlatformTypeSupported(FIDString type) override {
        return type&&std::strcmp(type,"HWND")==0?kResultOk:kResultFalse;
    }
    tresult SMTG_STDCALL attached(void* parent,FIDString type) override {
        if(isPlatformTypeSupported(type)!=kResultOk||!parent||window_)return kResultFalse;
        HMODULE module=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&kModuleAnchor),
                               &module))return kResultFalse;
        module_=module;
        WNDCLASSEXW wc{};
        wc.cbSize=sizeof(wc);
        wc.hInstance=module_;
        wc.lpfnWndProc=&NativeEditor::window_proc;
        wc.lpszClassName=kWindowClass;
        wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        wc.style=CS_DBLCLKS|CS_HREDRAW|CS_VREDRAW;
        if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return kResultFalse;
        window_=CreateWindowExW(0,kWindowClass,L"Monkey's Ear",
            WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,width_,height_,
            static_cast<HWND>(parent),nullptr,wc.hInstance,this);
        if(!window_)return kResultFalse;
        ++gAttachedViews;
        combo_=CreateWindowExW(0,L"COMBOBOX",L"",
            WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,
            120,168,340,400,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(kComboId)),
            wc.hInstance,nullptr);
        search_=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|WS_TABSTOP,
            550,168,450,31,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSearchId)),
            wc.hInstance,nullptr);
        if(!combo_||!search_){removed();return kResultFalse;}
        SendMessageW(combo_,WM_SETFONT,reinterpret_cast<WPARAM>(body_font_),TRUE);
        SendMessageW(search_,WM_SETFONT,reinterpret_cast<WPARAM>(body_font_),TRUE);
        for(const wchar_t* area:kAreas)SendMessageW(combo_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(area));
        SendMessageW(combo_,CB_SETCURSEL,0,0);
        show_native_controls();
        SetTimer(window_,1,90,nullptr); // GUI thread repaint; no DSP callbacks
        InvalidateRect(window_,nullptr,FALSE);
        return kResultOk;
    }
    tresult SMTG_STDCALL removed() override {
        finish_drag();
        if(window_) {
            KillTimer(window_,1);
            HWND old=window_;
            window_=nullptr;
            combo_=search_=nullptr;
            DestroyWindow(old);
            if(--gAttachedViews==0&&module_)
                UnregisterClassW(kWindowClass,module_);
        }
        return kResultOk;
    }
    tresult SMTG_STDCALL onWheel(float distance) override {
        if(page_!=1)return kResultFalse;
        scroll_rows_=std::max(0,scroll_rows_-(distance>0?2:-2));
        invalidate();
        return kResultOk;
    }
    tresult SMTG_STDCALL onKeyDown(char16 key,int16,int16) override {
        if(selected_id_<0)return kResultFalse;
        if(key==L'+'||key==L'-') {
            ParameterInfo meta{};
            if(controller_->getParameterInfo(selected_id_,meta)!=kResultOk)return kResultFalse;
            const int steps=usable_steps(selected_id_,meta.stepCount);
            const double step=steps>0?1.0/steps:0.01;
            change_single(selected_id_,
                controller_->getParamNormalized(selected_id_)+(key==L'+'?step:-step));
            return kResultOk;
        }
        return kResultFalse;
    }
    tresult SMTG_STDCALL onKeyUp(char16,int16,int16) override { return kResultFalse; }
    tresult SMTG_STDCALL getSize(ViewRect* rect) override {
        if(!rect)return kInvalidArgument;
        *rect=ViewRect(0,0,width_,height_);return kResultOk;
    }
    tresult SMTG_STDCALL onSize(ViewRect* rect) override {
        if(!rect)return kInvalidArgument;
        width_=std::max(960,rect->right-rect->left);
        height_=std::max(620,rect->bottom-rect->top);
        if(window_) {
            SetWindowPos(window_,nullptr,0,0,width_,height_,
                SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
            show_native_controls();
            invalidate();
        }
        return kResultOk;
    }
    tresult SMTG_STDCALL onFocus(TBool) override { return kResultOk; }
    tresult SMTG_STDCALL setFrame(IPlugFrame* frame) override {
        // Never retain a raw host frame after detachment or controller teardown.
        frame_=frame;return kResultOk;
    }
    tresult SMTG_STDCALL canResize() override { return kResultOk; }
    tresult SMTG_STDCALL checkSizeConstraint(ViewRect* r) override {
        if(!r)return kInvalidArgument;
        const int width=std::clamp(r->right-r->left,960,1920);
        const int height=std::clamp(r->bottom-r->top,620,1300);
        r->right=r->left+width;r->bottom=r->top+height;
        return kResultOk;
    }

private:
    static LRESULT CALLBACK window_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
        NativeEditor* self=reinterpret_cast<NativeEditor*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE) {
            auto* create=reinterpret_cast<CREATESTRUCTW*>(lp);
            self=static_cast<NativeEditor*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if(self) return self->window_message(hwnd,message,wp,lp);
        return DefWindowProcW(hwnd,message,wp,lp);
    }
    LRESULT window_message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
        switch(msg) {
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{
            PAINTSTRUCT ps{};
            HDC target=BeginPaint(hwnd,&ps);
            RECT actual{};GetClientRect(hwnd,&actual);
            const int w=std::max(1,actual.right),h=std::max(1,actual.bottom);
            HDC buffer=CreateCompatibleDC(target);
            HBITMAP bitmap=CreateCompatibleBitmap(target,w,h);
            HGDIOBJ old=SelectObject(buffer,bitmap);
            SetMapMode(buffer,MM_ANISOTROPIC);
            SetWindowExtEx(buffer,kCanvasW,kCanvasH,nullptr);
            SetViewportExtEx(buffer,w,h,nullptr);
            render(buffer);
            SetMapMode(buffer,MM_TEXT);
            BitBlt(target,0,0,w,h,buffer,0,0,SRCCOPY);
            SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);
            EndPaint(hwnd,&ps);return 0;
        }
        case WM_TIMER:invalidate();return 0;
        case WM_COMMAND:
            if(LOWORD(wp)==kComboId&&HIWORD(wp)==CBN_SELCHANGE) {
                selected_area_=std::clamp(static_cast<int>(SendMessageW(combo_,CB_GETCURSEL,0,0)),0,14);
                scroll_rows_=0;invalidate();
            }else if(LOWORD(wp)==kSearchId&&HIWORD(wp)==EN_CHANGE) {
                wchar_t buffer[128]{};
                GetWindowTextW(search_,buffer,128);
                search_text_=buffer;scroll_rows_=0;invalidate();
            }
            return 0;
        case WM_LBUTTONDOWN:
            click(GET_X_LPARAM(lp)*kCanvasW/std::max(1,width_),
                  GET_Y_LPARAM(lp)*kCanvasH/std::max(1,height_),false);
            return 0;
        case WM_LBUTTONDBLCLK:
            click(GET_X_LPARAM(lp)*kCanvasW/std::max(1,width_),
                  GET_Y_LPARAM(lp)*kCanvasH/std::max(1,height_),true);
            return 0;
        case WM_MOUSEMOVE:
            if((dragged_id_>=0||dragged_id_==-2)&&(wp&MK_LBUTTON)){
                const int x=GET_X_LPARAM(lp)*kCanvasW/std::max(1,width_);
                const int y=GET_Y_LPARAM(lp)*kCanvasH/std::max(1,height_);
                if(dragged_id_==-2)drag_xy(x,y);
                else drag(x);
            }
            return 0;
        case WM_LBUTTONUP:finish_drag();return 0;
        case WM_CAPTURECHANGED:finish_drag();return 0;
        case WM_MOUSEWHEEL:onWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp))/WHEEL_DELTA);return 0;
        case WM_DESTROY:if(window_==hwnd)window_=nullptr;return 0;
        }
        return DefWindowProcW(hwnd,msg,wp,lp);
    }
    void invalidate(){if(window_)InvalidateRect(window_,nullptr,FALSE);}
    IComponentHandler* handler() const {return handler_slot_?*handler_slot_:nullptr;}
    void show_native_controls() {
        if(!combo_||!search_)return;
        const bool show=page_==1;
        ShowWindow(combo_,show?SW_SHOW:SW_HIDE);
        ShowWindow(search_,show?SW_SHOW:SW_HIDE);
        const int y=169*height_/kCanvasH;
        MoveWindow(combo_,170*width_/kCanvasW,y,320*width_/kCanvasW,
                   360*height_/kCanvasH,TRUE);
        MoveWindow(search_,590*width_/kCanvasW,y,444*width_/kCanvasW,
                   33*height_/kCanvasH,TRUE);
    }
    void set_page(int page) {
        finish_drag();
        page_=page;scroll_rows_=0;show_native_controls();invalidate();
    }
    void begin_edit(int id) {
        if(auto* h=handler())h->beginEdit(static_cast<ParamID>(id));
    }
    void update_edit(int id,double value) {
        if(!handler())return; // never claim a UI-only edit affected real audio
        value=std::clamp(value,0.0,1.0);
        if(controller_->setParamNormalized(static_cast<ParamID>(id),value)!=kResultOk)return;
        handler()->performEdit(static_cast<ParamID>(id),value);
        selected_id_=id;invalidate();
    }
    void finish_edit(int id) {
        if(auto* h=handler())h->endEdit(static_cast<ParamID>(id));
    }
    void change_single(int id,double value) {
        if(!handler())return;
        begin_edit(id);update_edit(id,value);finish_edit(id);
    }
    void finish_drag() {
        if(dragged_id_<0&&dragged_id_!=-2)return;
        const int id=dragged_id_;
        dragged_id_=-1; // ReleaseCapture sends WM_CAPTURECHANGED synchronously
        if(window_&&GetCapture()==window_)ReleaseCapture();
        if(id==-2){finish_edit(176);finish_edit(177);}
        else finish_edit(id);
    }
    void drag_xy(int x,int y) {
        if(dragged_id_!=-2)return;
        const float width=static_cast<float>(std::max(1,drag_rect_.right-drag_rect_.left));
        const float height=static_cast<float>(std::max(1,drag_rect_.bottom-drag_rect_.top));
        // Upper-right is maximum motion + instability; lower-left neutral.
        const float drift=std::clamp((x-drag_rect_.left)/width,0.0f,1.0f);
        const float fracture=std::clamp((drag_rect_.bottom-y)/height,0.0f,1.0f);
        update_edit(176,drift);
        update_edit(177,fracture);
    }
    void drag(int x) {
        if(dragged_id_<0)return;
        const int size=std::max(1,drag_rect_.right-drag_rect_.left);
        update_edit(dragged_id_,static_cast<double>(x-drag_rect_.left)/size);
    }
    void popup_choice(int id,int x,int y) {
        if(!handler()||!window_)return;
        ParameterInfo info{};
        if(controller_->getParameterInfo(id,info)!=kResultOk)return;
        const int steps=std::min(64,usable_steps(id,info.stepCount));
        if(steps<=1)return;
        HMENU menu=CreatePopupMenu();
        for(int i=0;i<=steps;++i) {
            char16 text[128]{};
            const double value=static_cast<double>(i)/steps;
            if(controller_->getParamStringByValue(id,value,text)!=kResultOk)continue;
            std::wstring item=to_wide(text);
            if(item.empty())item=std::to_wstring(i);
            AppendMenuW(menu,MF_STRING,static_cast<UINT_PTR>(i+1),item.c_str());
        }
        POINT pos{x*width_/kCanvasW,y*height_/kCanvasH};
        ClientToScreen(window_,&pos);
        const int selection=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,
                                           pos.x,pos.y,0,window_,nullptr);
        DestroyMenu(menu);
        if(selection>=1&&selection<=steps+1)
            change_single(id,static_cast<double>(selection-1)/steps);
    }
    void click(int x,int y,bool reset) {
        for(auto it=hits_.rbegin();it!=hits_.rend();++it) {
            if(!it->contains(x,y))continue;
            if(it->action==1){set_page(it->target);return;}
            if(it->action==2){change_single(173,(it->target+.5)/8.0);return;}
            if(it->action==3){
                if(!handler())return;
                dragged_id_=-2;
                drag_rect_=it->rect;
                selected_id_=176;
                begin_edit(176);begin_edit(177);
                SetCapture(window_);
                drag_xy(x,y);
                return;
            }
            const int id=it->id;selected_id_=id;
            ParameterInfo info{};
            if(controller_->getParameterInfo(id,info)!=kResultOk)return;
            if(reset){change_single(id,info.defaultNormalizedValue);return;}
            const int steps=usable_steps(id,info.stepCount);
            if(steps==1) {
                change_single(id,controller_->getParamNormalized(id)<.5?1:0);
                return;
            }
            if(steps>1) {
                popup_choice(id,x,y);
                return;
            }
            if(!handler())return;
            dragged_id_=id;drag_rect_=it->rect;
            begin_edit(id);SetCapture(window_);drag(x);
            return;
        }
    }
    void add_hit(int x,int y,int w,int h,int id,int action=0,int target=0){
        hits_.push_back(Hit{RECT{x,y,x+w,y+h},id,action,target});
    }
    std::wstring param_title(int id) {
        ParameterInfo info{};
        if(controller_->getParameterInfo(id,info)!=kResultOk)return L"Unknown";
        return to_wide(info.title);
    }
    std::wstring param_value(int id) {
        char16 text[128]{};
        const auto value=controller_->getParamNormalized(static_cast<ParamID>(id));
        if(controller_->getParamStringByValue(static_cast<ParamID>(id),value,text)!=kResultOk)return L"-";
        return to_wide(text);
    }
    void draw_param(HDC dc,int x,int y,int w,int h,int id,
                    const wchar_t* alias=nullptr) {
        ParameterInfo info{};
        if(controller_->getParameterInfo(id,info)!=kResultOk)return;
        const bool active=selected_id_==id;
        rectangle(dc,x,y,w,h,active?kPanelHover:kPanel,
                  active?kAccent:kOutline,14);
        const std::wstring name=alias?std::wstring(alias):to_wide(info.title);
        const std::wstring value=param_value(id);
        label(dc,body_font_,kText,name,x+13,y+6,w-25,h/2);
        label(dc,small_font_,kAccent,value,x+13,y+6,w-25,h/2,DT_RIGHT);
        const int barx=x+13,barw=w-26,bary=y+h-15;
        rectangle(dc,barx,bary,barw,6,kOutline,kOutline,6);
        const double norm=std::clamp(static_cast<double>(
            controller_->getParamNormalized(id)),0.0,1.0);
        rectangle(dc,barx,bary,std::max(4,static_cast<int>(barw*norm)),
                  6,kAccent,kAccent,6);
        add_hit(x,y,w,h,id);
    }
    void draw_sound(HDC dc,int x,int y,int w,int index) {
        const int selected=std::clamp(static_cast<int>(
            controller_->getParamNormalized(173)*8.0),0,7);
        const bool on=selected==index;
        rectangle(dc,x,y,w,42,on?RGB(31,77,76):kPanel,
                  on?kAccent:kOutline,10);
        label(dc,body_font_,on?kAccent:kText,kSounds[index],
              x+9,y,w-18,42,DT_CENTER);
        add_hit(x,y,w,42,173,2,index);
    }
    void title(HDC dc,const wchar_t* name,const wchar_t* description,int y){
        label(dc,body_font_,kText,name,44,y,1020,30);
        label(dc,small_font_,kMuted,description,44,y+25,1020,25);
    }
    void draw_play(HDC dc) {
        title(dc,L"CHOOSE A SOUND",
              L"Start with a musical idea. The eight macros work across the entire instrument.",129);
        for(int i=0;i<8;++i) {
            const int col=i%4,row=i/4;
            draw_sound(dc,44+col*260,183+row*48,249,i);
        }
        title(dc,L"SHAPE THE SOUND",L"Move these first. You can explore every mechanism later.",286);
        rectangle(dc,838,286,238,36,RGB(31,77,76),kAccent,12);
        label(dc,body_font_,kAccent,L"GESTURE FIELD: DRIFT / FRACTURE",848,286,218,36,DT_CENTER);
        add_hit(838,286,238,36,-1,1,3);
        constexpr const wchar_t* labels[8]={
            L"BRIGHTNESS",L"BITE",L"HEAT",L"BODY",
            L"ECHO",L"SPACE",L"LIVE INPUT",L"CHARACTER"
        };
        for(int i=0;i<8;++i)
            draw_param(dc,44+(i%4)*260,338+(i/4)*66,249,58,i,labels[i]);
        title(dc,L"AMP ENVELOPE",L"How the note begins, holds, and fades.",464);
        constexpr const wchar_t* env[4]={L"ATTACK",L"DECAY",L"SUSTAIN",L"RELEASE"};
        for(int i=0;i<4;++i)
            draw_param(dc,44+i*260,514,249,62,161+i,env[i]);
        title(dc,L"RHYTHM",L"Sync to the DAW or use your own clock. Groove shapes motion, not MIDI note placement.",587);
        constexpr int rhythm[4]={169,171,174,175};
        constexpr const wchar_t* names[4]={L"TEMPO SYNC",L"DIVISION",L"FOLLOW DAW",L"GROOVE"};
        for(int i=0;i<4;++i)
            draw_param(dc,44+i*260,640,249,56,rhythm[i],names[i]);
    }
    void draw_perform(HDC dc) {
        title(dc,L"PLAY THE SOUND AS A MOVING SYSTEM",
              L"Drag the pad while holding notes. DRIFT and FRACTURE influence the whole synth.",129);
        constexpr int left=78,top=205,width=734,height=350;
        rectangle(dc,left-2,top-2,width+4,height+4,kPanel,kOutline,16);
        HBRUSH grid=CreateSolidBrush(kOutline);
        for(int n=1;n<4;++n){
            const int xx=left+n*width/4;
            RECT line{xx,top,xx+1,top+height};FillRect(dc,&line,grid);
            const int yy=top+n*height/4;
            RECT across{left,yy,left+width,yy+1};FillRect(dc,&across,grid);
        }
        DeleteObject(grid);
        const float x=std::clamp(static_cast<float>(controller_->getParamNormalized(176)),0.f,1.f);
        const float y=std::clamp(static_cast<float>(controller_->getParamNormalized(177)),0.f,1.f);
        const int markerX=left+static_cast<int>(x*width);
        const int markerY=top+height-static_cast<int>(y*height);
        rectangle(dc,markerX-10,markerY-10,20,20,kAccent,kAccent,20);
        add_hit(left,top,width,height,-1,3,0);

        label(dc,body_font_,kMuted,L"FRACTURE  /  harmonic edge",78,165,510,31);
        label(dc,body_font_,kAccent,L"DRIFT  /  rhythmic & state motion",79,570,735,34,DT_CENTER);

        rectangle(dc,839,205,236,350,kPanel,kOutline,16);
        label(dc,body_font_,kText,L"ONE GESTURE, FIVE SYSTEMS",852,216,208,36,DT_CENTER);
        draw_param(dc,849,267,216,67,176,L"DRIFT");
        draw_param(dc,849,347,216,67,177,L"FRACTURE");
        label(dc,small_font_,kMuted,L"DRIFT grows LFO, filter",852,438,211,24);
        label(dc,small_font_,kMuted,L"sub-motion and Sound Space.",852,463,211,24);
        label(dc,small_font_,kMuted,L"FRACTURE bends oscillator FM",852,497,211,24);
        label(dc,small_font_,kMuted,L"and the existing tube circuit.",852,519,211,24);
        label(dc,small_font_,kText,L"LOWER-LEFT = ORIGINAL PATCH   |   MOVING DIAGONALLY = COUPLED EXPERIMENT",78,622,970,33);
        label(dc,small_font_,kMuted,L"Automate both axes, use REAPER MIDI Learn, or assign controller knobs to CC20/21.",78,667,975,30);
    }

    void draw_keys(HDC dc) {
        title(dc,L"PLAY IT FROM YOUR MIDI KEYBOARD",
              L"Arturia and other standard MIDI keyboards work through the DAW's MIDI input.",129);
        rectangle(dc,44,188,1032,136,kPanel,kOutline,16);
        label(dc,body_font_,kAccent,L"1  CONNECT",62,198,290,28);
        label(dc,small_font_,kText,L"Plug the keyboard into USB. In REAPER Preferences > MIDI Devices,",62,228,980,27);
        label(dc,small_font_,kText,L"enable the keyboard's normal MIDI port (not only its DAW/MCU port).",62,251,980,27);
        label(dc,small_font_,kText,L"2  ARM THE INSTRUMENT TRACK  /  INPUT: MIDI  /  MONITORING: ON",62,280,980,31);

        title(dc,L"FEEL & EXPRESSION",
              L"These controls act on the existing voice engine: no required vendor script.",342);
        constexpr int keys[8]={70,71,72,73,74,75,76,77};
        constexpr const wchar_t* labels[8]={
            L"MONO",L"LEGATO",L"GLIDE",L"BEND RANGE",
            L"VIBRATO DEPTH",L"VELOCITY TONE",L"PRESSURE TO FILTER",L"PRESSURE TO DRIVE"
        };
        for(int i=0;i<8;++i)
            draw_param(dc,44+(i%4)*260,390+(i/4)*72,249,64,keys[i],labels[i]);

        title(dc,L"COMMON HARDWARE MESSAGES",
              L"Key presses and pads become MIDI notes; controls can also use REAPER MIDI Learn.",557);
        rectangle(dc,44,604,1032,100,kPanel,kOutline,14);
        label(dc,small_font_,kText,L"KEYS / PADS: pitch + velocity    |    BEND WHEEL: pitch bend    |    PEDAL CC64: sustain",61,613,1000,28);
        label(dc,small_font_,kText,L"MOD WHEEL CC1 / CC74: brightness    |    CC71: bite    |    CC91: space    |    CC93: echo",61,640,1000,28);
        label(dc,small_font_,kText,L"CHANNEL PRESSURE: expressive filter/drive    |    POLY PRESSURE: individual-note timbre",61,667,1000,28);
    }
    void draw_explore(HDC dc) {
        title(dc,L"EXPLORE THE ENGINE",
              L"Pick an area or search every parameter. Deep controls remain available, never required.",129);
        label(dc,small_font_,kMuted,L"AREA",44,166,120,32);
        label(dc,small_font_,kMuted,L"SEARCH",506,166,82,32);
        std::vector<int> filtered;
        for(int id=0;id<controller_->getParameterCount();++id) {
            ParameterInfo info{};
            if(controller_->getParameterInfo(id,info)!=kResultOk)continue;
            if((info.flags&kIsHidden)&&search_text_.empty())continue;
            if(search_text_.empty()&&selected_area_!=0&&info.unitId!=selected_area_)continue;
            if(!search_text_.empty()&&!contains_ci(to_wide(info.title),search_text_))continue;
            filtered.push_back(id);
        }
        std::wstring count=std::to_wstring(filtered.size())+L" controls";
        label(dc,small_font_,kMuted,count,44,220,400,24);
        const int visible=7;
        scroll_rows_=std::clamp(scroll_rows_,0,std::max(0,static_cast<int>(filtered.size())-visible));
        if(filtered.empty())
            label(dc,body_font_,kMuted,L"No controls match. Try another area or search.",44,298,880,40);
        for(int i=0;i<visible&&scroll_rows_+i<static_cast<int>(filtered.size());++i)
            draw_param(dc,44,254+i*62,1032,55,filtered[scroll_rows_+i]);
        if(static_cast<int>(filtered.size())>visible){
            label(dc,small_font_,kMuted,
                L"Mouse wheel to browse    "+std::to_wstring(scroll_rows_+1)
                  +L" - "+std::to_wstring(std::min(static_cast<int>(filtered.size()),scroll_rows_+visible)),
                44,694,980,24);
        }
    }
    void render(HDC dc) {
        hits_.clear();
        RECT rect{0,0,kCanvasW,kCanvasH};
        HBRUSH bg=CreateSolidBrush(kBackground);
        FillRect(dc,&rect,bg);DeleteObject(bg);
        label(dc,title_font_,kText,L"MONKEY'S EAR",44,12,570,60);
        label(dc,small_font_,kMuted,L"PLAY FIRST  /  EXPERIMENT DEEPER",44,69,680,28);
        // Output remains accessible in BOTH views, never hidden in deep menus.
        draw_param(dc,616,23,170,46,8,L"OUTPUT");
        rectangle(dc,796,22,63,37,page_==0?RGB(31,77,76):kPanel,kOutline);
        label(dc,body_font_,page_==0?kAccent:kText,L"PLAY",796,22,63,37,DT_CENTER);
        add_hit(796,22,63,37,-1,1,0);
        rectangle(dc,864,22,89,37,page_==3?RGB(31,77,76):kPanel,kOutline);
        label(dc,body_font_,page_==3?kAccent:kText,L"PERFORM",864,22,89,37,DT_CENTER);
        add_hit(864,22,89,37,-1,1,3);
        rectangle(dc,958,22,54,37,page_==2?RGB(31,77,76):kPanel,kOutline);
        label(dc,body_font_,page_==2?kAccent:kText,L"KEYS",958,22,54,37,DT_CENTER);
        add_hit(958,22,54,37,-1,1,2);
        rectangle(dc,1017,22,60,37,page_==1?RGB(31,77,76):kPanel,kOutline);
        label(dc,body_font_,page_==1?kAccent:kText,L"DEEP",1017,22,60,37,DT_CENTER);
        add_hit(1017,22,60,37,-1,1,1);
        HBRUSH line=CreateSolidBrush(kOutline);
        RECT sep{44,108,1076,110};FillRect(dc,&sep,line);DeleteObject(line);
        if(page_==0)draw_play(dc);
        else if(page_==2)draw_keys(dc);
        else if(page_==3)draw_perform(dc);
        else draw_explore(dc);
        const bool connected=handler()!=nullptr;
        label(dc,small_font_,connected?kMuted:RGB(255,175,113),
             connected?L"Drag a slider. Click a choice. Double-click to reset.  Host automation is recorded."
                      :L"HOST EDIT HANDLER MISSING - EDITS DISABLED",
             44,710,1030,22);
    }

    std::atomic<uint32> references_{1};
    IEditController* controller_;
    IComponentHandler** handler_slot_;
    IPlugFrame* frame_=nullptr; // non-owning; never used after detach
    HMODULE module_=nullptr;
    HWND window_=nullptr,combo_=nullptr,search_=nullptr;
    HFONT title_font_=nullptr,body_font_=nullptr,small_font_=nullptr;
    int width_=kCanvasW,height_=kCanvasH,page_=0,selected_area_=0,scroll_rows_=0;
    int selected_id_=-1,dragged_id_=-1;
    RECT drag_rect_{};
    std::wstring search_text_;
    std::vector<Hit> hits_;
};

IPlugView* create_editor(IEditController* controller,IComponentHandler** handler_slot){
    return controller?static_cast<IPlugView*>(new NativeEditor(controller,handler_slot)):nullptr;
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
