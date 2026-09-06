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

#define INLINE_UID(b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15) \
    { (uint8_t)(b0), (uint8_t)(b1), (uint8_t)(b2), (uint8_t)(b3), (uint8_t)(b4), (uint8_t)(b5), (uint8_t)(b6), (uint8_t)(b7), \
      (uint8_t)(b8), (uint8_t)(b9), (uint8_t)(b10), (uint8_t)(b11), (uint8_t)(b12), (uint8_t)(b13), (uint8_t)(b14), (uint8_t)(b15) }

class FUnknown {
public:
    virtual tresult SMTG_STDCALL queryInterface(const TUID _iid, void** obj) = 0;
    virtual uint32 SMTG_STDCALL addRef() = 0;
    virtual uint32 SMTG_STDCALL release() = 0;
};

// FUnknown IID: 00000000-00000000-C0000000-00000046
static const TUID FUnknown_iid = INLINE_UID(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

struct PClassInfo {
    TUID cid;
    int32 cardinality;
    char8 category[32];
    char8 name[64];
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

// IPluginFactory IID: 7A4D8170-52AE-4AB5-9E73-2D6A0832F3F4
static const TUID IPluginFactory_iid = INLINE_UID(0x7A, 0x4D, 0x81, 0x70, 0x52, 0xAE, 0x4A, 0xB5, 0x9E, 0x73, 0x2D, 0x6A, 0x08, 0x32, 0xF3, 0xF4);

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

// IEventList IID: 3A2842CB-5F7E-4545-B2E6-8186105374E6
static const TUID IEventList_iid = INLINE_UID(0x3A, 0x28, 0x42, 0xCB, 0x5F, 0x7E, 0x45, 0x45, 0xB2, 0xE6, 0x81, 0x86, 0x10, 0x53, 0x74, 0xE6);

struct BusInfo {
    MediaType mediaType;
    BusDirection direction;
    int32 channelCount;
    char16 name[128];
    BusType busType;
    uint32 flags;
};

class IComponent : public FUnknown {
public:
    virtual tresult SMTG_STDCALL initialize(FUnknown* context) = 0;
    virtual tresult SMTG_STDCALL terminate() = 0;
    virtual tresult SMTG_STDCALL getControllerClassId(TUID classId) = 0;
    virtual tresult SMTG_STDCALL setIoMode(IoMode mode) = 0;
    virtual int32 SMTG_STDCALL getBusCount(MediaType type, BusDirection dir) = 0;
    virtual tresult SMTG_STDCALL getBusInfo(MediaType type, BusDirection dir, int32 index, BusInfo& bus) = 0;
    virtual tresult SMTG_STDCALL activateBus(MediaType type, BusDirection dir, int32 index, bool state) = 0;
    virtual tresult SMTG_STDCALL setActive(bool state) = 0;
    virtual tresult SMTG_STDCALL setState(void* state) = 0;
    virtual tresult SMTG_STDCALL getState(void* state) = 0;
};

// IComponent IID: E831FF31-F2D5-4301-928E-BBEE25697802
static const TUID IComponent_iid = INLINE_UID(0xE8, 0x31, 0xFF, 0x31, 0xF2, 0xD5, 0x43, 0x01, 0x92, 0x8E, 0xBB, 0xEE, 0x25, 0x69, 0x78, 0x02);

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
    void* inputParameterChanges;
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

// IAudioProcessor IID: 420435E3-B752-4298-8564-00025F8240F6
static const TUID IAudioProcessor_iid = INLINE_UID(0x42, 0x04, 0x35, 0xE3, 0xB7, 0x52, 0x42, 0x98, 0x85, 0x64, 0x00, 0x02, 0x5F, 0x82, 0x40, 0xF6);

// Speaker arrangements
const SpeakerArrangement kSpeakerStereo = 0x03; // L + R

} // namespace Vst
} // namespace Steinberg
