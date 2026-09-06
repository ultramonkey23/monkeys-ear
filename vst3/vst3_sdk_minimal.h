#pragma once

#include <cstdint>
#include <cstring>

#if defined(_WIN32)
  #define SMTG_STDCALL __stdcall
  #define SMTG_EXPORT_SYMBOL __declspec(dllexport)
#else
  #define SMTG_STDCALL
  #define SMTG_EXPORT_SYMBOL __attribute__((visibility("default")))
#endif

namespace Steinberg {

typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
typedef int32 tresult;
typedef char char8;
typedef char16_t char16;
typedef uint8_t TUID[16];

const tresult kResultOk = 0;
const tresult kResultFalse = 1;
const tresult kInvalidArgument = -1;
const tresult kNotImplemented = -2;
const tresult kInternalError = -3;
const tresult kNotInitialized = -4;

// Steinberg COM_COMPATIBLE INLINE_UID macro (matches Windows COM GUID memory layout)
#define INLINE_UID(l1, l2, l3, l4) \
{ \
    static_cast<uint8_t>(((uint32_t)(l1) & 0x000000FF)      ), \
    static_cast<uint8_t>(((uint32_t)(l1) & 0x0000FF00) >>  8), \
    static_cast<uint8_t>(((uint32_t)(l1) & 0x00FF0000) >> 16), \
    static_cast<uint8_t>(((uint32_t)(l1) & 0xFF000000) >> 24), \
    static_cast<uint8_t>(((uint32_t)(l2) & 0x00FF0000) >> 16), \
    static_cast<uint8_t>(((uint32_t)(l2) & 0xFF000000) >> 24), \
    static_cast<uint8_t>(((uint32_t)(l2) & 0x000000FF)      ), \
    static_cast<uint8_t>(((uint32_t)(l2) & 0x0000FF00) >>  8), \
    static_cast<uint8_t>(((uint32_t)(l3) & 0xFF000000) >> 24), \
    static_cast<uint8_t>(((uint32_t)(l3) & 0x00FF0000) >> 16), \
    static_cast<uint8_t>(((uint32_t)(l3) & 0x0000FF00) >>  8), \
    static_cast<uint8_t>(((uint32_t)(l3) & 0x000000FF)      ), \
    static_cast<uint8_t>(((uint32_t)(l4) & 0xFF000000) >> 24), \
    static_cast<uint8_t>(((uint32_t)(l4) & 0x00FF0000) >> 16), \
    static_cast<uint8_t>(((uint32_t)(l4) & 0x0000FF00) >>  8), \
    static_cast<uint8_t>(((uint32_t)(l4) & 0x000000FF)      )  \
}

class FUnknown {
public:
    virtual tresult SMTG_STDCALL queryInterface(const TUID _iid, void** obj) = 0;
    virtual uint32 SMTG_STDCALL addRef() = 0;
    virtual uint32 SMTG_STDCALL release() = 0;
};

// FUnknown IID: 00000000-00000000-C0000000-00000046
static const TUID FUnknown_iid = INLINE_UID(0x00000000, 0x00000000, 0xC0000000, 0x00000046);

// IPluginBase: 22888DDB-156E-45AE-8358-B34808190625
static const TUID IPluginBase_iid = INLINE_UID(0x22888DDB, 0x156E45AE, 0x8358B348, 0x08190625);

class IPluginBase : public FUnknown {
public:
    virtual tresult SMTG_STDCALL initialize(FUnknown* context) = 0;
    virtual tresult SMTG_STDCALL terminate() = 0;
};

struct PClassInfo {
    TUID cid;
    int32 cardinality;
    char8 category[32];
    char8 name[64];
};

struct PClassInfo2 {
    TUID cid;
    int32 cardinality;
    char8 category[32];
    char8 name[64];
    uint32 classFlags;
    char8 subCategories[128];
    char8 vendor[64];
    char8 version[64];
    char8 sdkVersion[64];
};

struct PFactoryInfo {
    char8 vendor[64];
    char8 url[64];
    char8 email[64];
    int32 flags;
};

class IPluginFactory : public FUnknown {
public:
    virtual tresult SMTG_STDCALL getFactoryInfo(PFactoryInfo* info) = 0;
    virtual int32 SMTG_STDCALL countClasses() = 0;
    virtual tresult SMTG_STDCALL getClassInfo(int32 index, PClassInfo* info) = 0;
    virtual tresult SMTG_STDCALL createInstance(const TUID cid, const TUID _iid, void** obj) = 0;
};

// IPluginFactory IID: 7A4D811C-5211-4A1F-AED9-D2EE0B43BF9F
static const TUID IPluginFactory_iid = INLINE_UID(0x7A4D811C, 0x52114A1F, 0xAED9D2EE, 0x0B43BF9F);

class IPluginFactory2 : public IPluginFactory {
public:
    virtual tresult SMTG_STDCALL getClassInfo2(int32 index, PClassInfo2* info) = 0;
};

// IPluginFactory2 IID: 0007B650-F24B-4C0B-A464-EDB9F00B2ABB
static const TUID IPluginFactory2_iid = INLINE_UID(0x0007B650, 0xF24B4C0B, 0xA464EDB9, 0xF00B2ABB);

namespace Vst {

typedef int32 MediaType;
typedef int32 BusDirection;
typedef int32 BusType;
typedef int32 IoMode;
typedef uint64 SpeakerArrangement;
typedef double ParamValue;
typedef uint32 ParamID;

enum MediaTypes { kAudio = 0, kEvent };
enum BusDirections { kInput = 0, kOutput };
enum BusTypes { kMain = 0, kAux };
enum IoModes { kSimple = 0, kAdvanced };

enum EventTypes {
    kNoteOnEvent = 0,
    kNoteOffEvent = 1,
    kDataEvent = 2,
    kPolyPressureEvent = 3,
    kNoteExpressionValueEvent = 4
};

struct NoteOnEvent {
    int16_t channel;
    int16_t pitch;
    float tuning;
    float velocity;
    int32 length;
    int32 noteId;
};

struct NoteOffEvent {
    int16_t channel;
    int16_t pitch;
    float velocity;
    int32 noteId;
    float tuning;
};

struct Event {
    int32 busIndex;
    int32 sampleOffset;
    double ppqPosition;
    uint16_t flags;
    uint16_t type;
    union {
        NoteOnEvent noteOn;
        NoteOffEvent noteOff;
        uint8_t raw[32];
    };
};

class IEventList : public FUnknown {
public:
    virtual int32 SMTG_STDCALL getEventCount() = 0;
    virtual tresult SMTG_STDCALL getEvent(int32 index, Event& e) = 0;
    virtual tresult SMTG_STDCALL addEvent(Event& e) = 0;
};

// IEventList IID: 3A2C4214-346349FE-B2C4F397-B9695A44
static const TUID IEventList_iid = INLINE_UID(0x3A2C4214, 0x346349FE, 0xB2C4F397, 0xB9695A44);

// Parameter Changes
class IParamValueQueue : public FUnknown {
public:
    virtual ParamID SMTG_STDCALL getParameterId() = 0;
    virtual int32 SMTG_STDCALL getPointCount() = 0;
    virtual tresult SMTG_STDCALL getPoint(int32 index, int32& sampleOffset, ParamValue& value) = 0;
    virtual tresult SMTG_STDCALL addPoint(int32 sampleOffset, ParamValue value, int32& index) = 0;
};

// IParamValueQueue IID: 01263A18-ED074F6F-98C9D356-4686F9BA
static const TUID IParamValueQueue_iid = INLINE_UID(0x01263A18, 0xED074F6F, 0x98C9D356, 0x4686F9BA);

class IParameterChanges : public FUnknown {
public:
    virtual int32 SMTG_STDCALL getParameterCount() = 0;
    virtual IParamValueQueue* SMTG_STDCALL getParameterData(int32 index) = 0;
    virtual IParamValueQueue* SMTG_STDCALL addParameterData(const ParamID& id, int32& index) = 0;
};

// IParameterChanges IID: A4779663-0BB64A56-B44384A8-466FEB9D
static const TUID IParameterChanges_iid = INLINE_UID(0xA4779663, 0x0BB64A56, 0xB44384A8, 0x466FEB9D);

struct BusInfo {
    MediaType mediaType;
    BusDirection direction;
    int32 channelCount;
    char16 name[128];
    BusType busType;
    uint32 flags;
};

struct RoutingInfo {
    MediaType mediaType;
    int32 busIndex;
    int32 channel;
};

class IComponent : public IPluginBase {
public:
    virtual tresult SMTG_STDCALL getControllerClassId(TUID classId) = 0;
    virtual tresult SMTG_STDCALL setIoMode(IoMode mode) = 0;
    virtual int32 SMTG_STDCALL getBusCount(MediaType type, BusDirection dir) = 0;
    virtual tresult SMTG_STDCALL getBusInfo(MediaType type, BusDirection dir, int32 index, BusInfo& bus) = 0;
    virtual tresult SMTG_STDCALL getRoutingInfo(RoutingInfo& inInfo, RoutingInfo& outInfo) = 0;
    virtual tresult SMTG_STDCALL activateBus(MediaType type, BusDirection dir, int32 index, bool state) = 0;
    virtual tresult SMTG_STDCALL setActive(bool state) = 0;
    virtual tresult SMTG_STDCALL setState(void* state) = 0;
    virtual tresult SMTG_STDCALL getState(void* state) = 0;
};

// IComponent IID: E831FF31-F2D5-4301-928E-BBEE25697802
static const TUID IComponent_iid = INLINE_UID(0xE831FF31, 0xF2D54301, 0x928EBBEE, 0x25697802);

struct ProcessSetup {
    int32 processMode;
    int32 symbolicSampleSize;
    int32 maxSamplesPerBlock;
    double sampleRate;
};

struct AudioBusBuffers {
    int32 numChannels;
    uint64 silenceFlags;
    union {
        float** channelBuffers32;
        double** channelBuffers64;
    };
};

struct ProcessData {
    int32 processMode;
    int32 symbolicSampleSize;
    int32 numSamples;
    int32 numInputs;
    int32 numOutputs;
    AudioBusBuffers* inputs;
    AudioBusBuffers* outputs;
    IParameterChanges* inputParameterChanges;
    void* outputParameterChanges;
    IEventList* inputEvents;
    void* outputEvents;
    void* processContext;
};

class IAudioProcessor : public FUnknown {
public:
    virtual tresult SMTG_STDCALL setBusArrangements(SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts) = 0;
    virtual tresult SMTG_STDCALL getBusArrangement(BusDirection dir, int32 index, SpeakerArrangement& arr) = 0;
    virtual tresult SMTG_STDCALL canProcessSampleSize(int32 symbolicSampleSize) = 0;
    virtual uint32 SMTG_STDCALL getLatencySamples() = 0;
    virtual tresult SMTG_STDCALL setupProcessing(ProcessSetup& setup) = 0;
    virtual tresult SMTG_STDCALL setProcessing(bool state) = 0;
    virtual tresult SMTG_STDCALL process(ProcessData& data) = 0;
    virtual uint32 SMTG_STDCALL getTailSamples() = 0;
};

// IAudioProcessor IID: 42043F99-B7DA453C-A569E79D-9AAEC33D
static const TUID IAudioProcessor_iid = INLINE_UID(0x42043F99, 0xB7DA453C, 0xA569E79D, 0x9AAEC33D);

// Parameter Info
struct ParameterInfo {
    ParamID id;
    char16 title[128];
    char16 shortTitle[128];
    char16 units[128];
    int32 stepCount;
    ParamValue defaultNormalizedValue;
    int32 unitId;
    int32 flags;
};

enum ParameterFlags {
    kCanAutomate = 1 << 0,
    kIsReadOnly = 1 << 1,
    kIsWrapAround = 1 << 2,
    kIsList = 1 << 3,
    kIsHidden = 1 << 4,
    kIsProgramChange = 1 << 15,
    kIsBypass = 1 << 16
};

class IComponentHandler : public FUnknown {
public:
    virtual tresult SMTG_STDCALL beginEdit(ParamID id) = 0;
    virtual tresult SMTG_STDCALL performEdit(ParamID id, ParamValue valueNormalized) = 0;
    virtual tresult SMTG_STDCALL endEdit(ParamID id) = 0;
    virtual tresult SMTG_STDCALL restartComponent(int32 flags) = 0;
};

// IComponentHandler IID: 93A0BEA3-0BD0-45DB-8E89-0B0CC1E46AC6
static const TUID IComponentHandler_iid = INLINE_UID(0x93A0BEA3, 0x0BD045DB, 0x8E890B0C, 0xC1E46AC6);

class IEditController : public IPluginBase {
public:
    virtual tresult SMTG_STDCALL setComponentState(void* state) = 0;
    virtual tresult SMTG_STDCALL setState(void* state) = 0;
    virtual tresult SMTG_STDCALL getState(void* state) = 0;
    virtual int32 SMTG_STDCALL getParameterCount() = 0;
    virtual tresult SMTG_STDCALL getParameterInfo(int32 paramIndex, ParameterInfo& info) = 0;
    virtual tresult SMTG_STDCALL getParamStringByValue(ParamID id, ParamValue valueNormalized, char16 string[128]) = 0;
    virtual tresult SMTG_STDCALL getParamValueByString(ParamID id, const char16* string, ParamValue& valueNormalized) = 0;
    virtual ParamValue SMTG_STDCALL normalizedParamToPlain(ParamID id, ParamValue valueNormalized) = 0;
    virtual ParamValue SMTG_STDCALL plainParamToNormalized(ParamID id, ParamValue plainValue) = 0;
    virtual ParamValue SMTG_STDCALL getParamNormalized(ParamID id) = 0;
    virtual tresult SMTG_STDCALL setParamNormalized(ParamID id, ParamValue value) = 0;
    virtual tresult SMTG_STDCALL setComponentHandler(IComponentHandler* handler) = 0;
    virtual void* SMTG_STDCALL createView(const char8* name) = 0;
};

// IEditController IID: DCD7BBE3-7742448D-A874AACC-979C759E
static const TUID IEditController_iid = INLINE_UID(0xDCD7BBE3, 0x7742448D, 0xA874AACC, 0x979C759E);

class IMidiMapping : public FUnknown {
public:
    virtual tresult SMTG_STDCALL getMidiControllerAssignment(int32 busIndex, int16_t channel, int16_t midiControllerNumber, ParamID& id) = 0;
};

// IMidiMapping IID: DF0FF9F7-49B74669-B63AB732-7ADBF5E5
static const TUID IMidiMapping_iid = INLINE_UID(0xDF0FF9F7, 0x49B74669, 0xB63AB732, 0x7ADBF5E5);

class IMessage : public FUnknown {
public:
    virtual const char8* SMTG_STDCALL getMessageID() = 0;
    virtual void SMTG_STDCALL setMessageID(const char8* id) = 0;
    virtual void* SMTG_STDCALL getAttributes() = 0;
};

// IMessage IID: 936F033B-C6C047DB-BB0882F8-13C1E613
static const TUID IMessage_iid = INLINE_UID(0x936F033B, 0xC6C047DB, 0xBB0882F8, 0x13C1E613);

class IConnectionPoint : public FUnknown {
public:
    virtual tresult SMTG_STDCALL connect(IConnectionPoint* other) = 0;
    virtual tresult SMTG_STDCALL disconnect(IConnectionPoint* other) = 0;
    virtual tresult SMTG_STDCALL notify(IMessage* message) = 0;
};

// IConnectionPoint IID: 70A4156F-6E6E4026-989148BF-AA60D8D1
static const TUID IConnectionPoint_iid = INLINE_UID(0x70A4156F, 0x6E6E4026, 0x989148BF, 0xAA60D8D1);

// Speaker arrangements
namespace SpeakerArr {
    const SpeakerArrangement kEmpty  = 0;
    const SpeakerArrangement kMono   = (1ULL << 19);
    const SpeakerArrangement kStereo = (1ULL << 0) | (1ULL << 1); // 0x03
}

} // namespace Vst
} // namespace Steinberg
