#pragma once

#include "monkeys_ear/ecosystem_state.h"
#include <atomic>
#include <cstdint>

namespace monkeys_ear {

// Race-free single-writer / multi-reader mailbox. Evidence fields are copied
// through atomics so a reader never touches storage concurrently mutated by a
// writer. No locks, waits, allocation, I/O or retry loops occur on audio paths.
template <size_t Capacity>
class EcosystemHandoff {
public:
    using Snapshot = EcosystemSnapshot<Capacity>;

    void publish(const Snapshot& snapshot) noexcept {
        const uint16_t n = snapshot.count > Capacity ? static_cast<uint16_t>(Capacity) : snapshot.count;
        for (uint16_t i = 0; i < n; ++i) store_item(i, snapshot.evidence[i]);
        count_.store(n, std::memory_order_relaxed);
        generation_.store(snapshot.generation, std::memory_order_relaxed);
        sequence_.fetch_add(1u, std::memory_order_release);
    }

    bool try_read(Snapshot& destination) const noexcept {
        const uint32_t before = sequence_.load(std::memory_order_acquire);
        const uint16_t n = count_.load(std::memory_order_relaxed);
        Snapshot candidate{}; candidate.count = n; candidate.generation = generation_.load(std::memory_order_relaxed);
        for (uint16_t i = 0; i < n; ++i) candidate.evidence[i] = load_item(i);
        const uint32_t after = sequence_.load(std::memory_order_acquire);
        if (before != after) return false;
        destination = candidate; return true;
    }

private:
    struct AtomicEvidence {
        std::atomic<uint8_t> kind{0}, horizon{0};
        std::atomic<uint32_t> module_instance{0}, generation{0};
        std::atomic<float> value{0}, confidence{0}, age_seconds{0}, low_hz{0}, high_hz{0};
    };
    void store_item(uint16_t i, const EcosystemEvidence& e) noexcept {
        auto& d=items_[i]; d.kind.store(static_cast<uint8_t>(e.kind),std::memory_order_relaxed); d.horizon.store(static_cast<uint8_t>(e.horizon),std::memory_order_relaxed);
        d.module_instance.store(e.provenance.module_instance,std::memory_order_relaxed); d.generation.store(e.provenance.generation,std::memory_order_relaxed);
        d.value.store(e.value,std::memory_order_relaxed); d.confidence.store(e.confidence,std::memory_order_relaxed); d.age_seconds.store(e.age_seconds,std::memory_order_relaxed);
        d.low_hz.store(e.context_low_hz,std::memory_order_relaxed); d.high_hz.store(e.context_high_hz,std::memory_order_relaxed);
    }
    EcosystemEvidence load_item(uint16_t i) const noexcept {
        const auto& s=items_[i]; EcosystemEvidence e{}; e.kind=static_cast<EvidenceKind>(s.kind.load(std::memory_order_relaxed)); e.horizon=static_cast<EvidenceHorizon>(s.horizon.load(std::memory_order_relaxed));
        e.provenance.module_instance=s.module_instance.load(std::memory_order_relaxed); e.provenance.generation=s.generation.load(std::memory_order_relaxed);
        e.value=s.value.load(std::memory_order_relaxed); e.confidence=s.confidence.load(std::memory_order_relaxed); e.age_seconds=s.age_seconds.load(std::memory_order_relaxed);
        e.context_low_hz=s.low_hz.load(std::memory_order_relaxed); e.context_high_hz=s.high_hz.load(std::memory_order_relaxed); return e;
    }
    AtomicEvidence items_[Capacity]{};
    alignas(64) std::atomic<uint16_t> count_{0};
    std::atomic<uint32_t> generation_{0};
    alignas(64) std::atomic<uint32_t> sequence_{0};
};

} // namespace monkeys_ear
