#pragma once

#include "monkeys_ear/ecosystem_state.h"
#include <atomic>
#include <cstdint>

namespace monkeys_ear {

// Single-writer / multi-reader snapshot handoff. The writer publishes complete
// immutable slots; readers never lock or wait. If a concurrent publish races a
// read, the reader keeps its previous valid snapshot and tries again next block.
template <size_t Capacity>
class EcosystemHandoff {
public:
    using Snapshot = EcosystemSnapshot<Capacity>;

    void publish(const Snapshot& snapshot) noexcept {
        const uint32_t next = (published_slot_.load(std::memory_order_relaxed) + 1u) % 3u;
        slots_[next] = snapshot;
        slots_[next].generation = generation_.fetch_add(1u, std::memory_order_relaxed) + 1u;
        published_slot_.store(next, std::memory_order_release);
    }

    bool try_read(Snapshot& destination) const noexcept {
        const uint32_t first = published_slot_.load(std::memory_order_acquire);
        const Snapshot candidate = slots_[first];
        const uint32_t second = published_slot_.load(std::memory_order_acquire);
        if (first != second) return false;
        destination = candidate;
        return true;
    }

private:
    Snapshot slots_[3]{};
    alignas(64) std::atomic<uint32_t> published_slot_{0};
    alignas(64) std::atomic<uint32_t> generation_{0};
};

} // namespace monkeys_ear
