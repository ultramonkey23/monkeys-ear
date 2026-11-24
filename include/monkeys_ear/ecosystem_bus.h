#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace monkeys_ear {

// Fixed lock-free, cross-module VST3 Ecosystem Bus connecting MonkeysEar (Synth/FX)
// and MonkeysEar Vocal within the same DAW/host process.
// Shared memory is opened ONCE in non-RT setup and mapped into POD atomics;
// audio threads only perform non-blocking seqlock reads and writes.

struct EcosystemStreamFrame {
    uint64_t frame_id = 0;
    uint32_t role = 0;       // 1 = Instrument/FX, 2 = Vocal, 3 = Aux Onion EQ
    bool active = false;
    float pitch_cents = 0.0f;
    float pitch_confidence = 0.0f;
    float note_gate = 0.0f;
    float velocity = 0.0f;
    float loudness = 0.0f;
    int32_t midi_note = -1;
    std::array<float, 24> spectral_occupancy{};
};

struct alignas(64) EcosystemChannel {
    std::atomic<uint64_t> synth_seq{0};
    EcosystemStreamFrame synth{};

    std::atomic<uint64_t> vocal_seq{0};
    EcosystemStreamFrame vocal{};

    std::atomic<uint64_t> aux_eq_seq{0};
    EcosystemStreamFrame aux_eq{};
};

struct alignas(64) EcosystemSharedMemory {
    static constexpr uint32_t kMagic = 0x4D454334u; // "MEC4" (24 ERB bands + 3 peer roles)
    static constexpr size_t kSlotCount = 8;

    uint32_t magic = 0;
    uint32_t version = 3;
    std::array<EcosystemChannel, kSlotCount> slots{};
};

enum class EcoLinkMode : int {
    Off = 0,
    Publish = 1,
    Follow = 2,
    Bidirectional = 3
};

class EcosystemBusClient {
public:
    static constexpr size_t kSlotCount = EcosystemSharedMemory::kSlotCount;

    EcosystemBusClient() noexcept;
    ~EcosystemBusClient() noexcept;

    EcosystemBusClient(const EcosystemBusClient&) = delete;
    EcosystemBusClient& operator=(const EcosystemBusClient&) = delete;

    // Non-RT setup: ensures shared memory mapping is acquired once per client instance.
    void ensure_connected() noexcept;

    // Strictly lock-free, zero-alloc RT read/write.
    void publish_synth(int slot, const EcosystemStreamFrame& frame) noexcept;
    void publish_vocal(int slot, const EcosystemStreamFrame& frame) noexcept;
    void publish_aux_eq(int slot, const EcosystemStreamFrame& frame) noexcept;
    void publish_inactive(int slot, int role_index) noexcept; // 0=synth, 1=vocal, 2=aux_eq
    bool read_synth(int slot, EcosystemStreamFrame& out) const noexcept;
    bool read_vocal(int slot, EcosystemStreamFrame& out) const noexcept;
    bool read_aux_eq(int slot, EcosystemStreamFrame& out) const noexcept;

    // Helper: aggregate most recent synth pitch across slots if needed, or exact slot.
    bool read_synth_or_any(int preferred_slot, EcosystemStreamFrame& out) const noexcept;

private:
    EcosystemSharedMemory* mem_ = nullptr;
    void* platform_handle_ = nullptr;
    bool owns_view_ = false;
};

} // namespace monkeys_ear
