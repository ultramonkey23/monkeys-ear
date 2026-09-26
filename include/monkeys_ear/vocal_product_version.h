#pragma once

// Voice/Vocal has an independent release identity from the unfinished
// Monkey's Ear suite. Keep this literal as the single source consumed by the
// VST3 metadata, validation probe, and tester bundle.
#define MONKEYS_EAR_VOCAL_PRODUCT_VERSION "0.1.0-preview.1"

namespace monkeys_ear {
inline constexpr char kVocalProductVersion[] = MONKEYS_EAR_VOCAL_PRODUCT_VERSION;
}
