#pragma once
#include "vst3_sdk_minimal.h"

namespace monkeys_ear::ui {
// The host owns parameter transfer and automation. An editor never touches DSP
// or the audio thread. The host handler is resolved at edit time because VST3
// may call setComponentHandler after it has requested the editor view.
Steinberg::IPlugView* create_editor(
    Steinberg::Vst::IEditController* controller,
    Steinberg::Vst::IComponentHandler** handler_slot);
}
