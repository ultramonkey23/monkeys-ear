#pragma once
#include "vst3_sdk_minimal.h"
#include "monkeys_ear/preset.h"
#include <string>

namespace monkeys_ear::ui {

static const Steinberg::TUID kMonkeysEarEditorBridge_iid = INLINE_UID(
    0x4D6F6E6B, 0x65794561, 0x72427264, 0x67653031
);

class IMonkeysEarEditorBridge {
public:
    virtual ~IMonkeysEarEditorBridge() = default;
    virtual std::string get_growth_snapshot() const = 0;
    virtual void set_growth_snapshot(const std::string& snapshot, bool active) = 0;
    virtual int get_sequencer_step() const = 0;
    virtual void release_bridge_ref() noexcept = 0;
};

} // namespace monkeys_ear::ui
