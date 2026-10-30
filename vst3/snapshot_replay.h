#pragma once
#include <array>
#include <cstddef>

namespace monkeys_ear::vst3 {

// Factory identity is an instruction to load a base sound; all other stored
// values are the player's edits on top. Replaying in raw ID order is wrong
// because the palette selectors (79 and 173) appear after many controls.
// Keep the whole snapshot immutable while this transaction is applied.
template<std::size_t Count, class Apply>
void replay_patch_snapshot(const std::array<float, Count>& snapshot, Apply&& apply) {
    static_assert(Count > 173, "Snapshot must include both factory selectors");
    apply(79, snapshot[79]);
    apply(173, snapshot[173]);
    for(std::size_t id=0; id<Count; ++id) {
        if(id!=79 && id!=173) apply(id, snapshot[id]);
    }
}

} // namespace monkeys_ear::vst3
