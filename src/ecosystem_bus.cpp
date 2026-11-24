#include "monkeys_ear/ecosystem_bus.h"
#include "monkeys_ear/types.h"
#include <bit>
#include <cstring>
#include <new>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdio.h>
#endif

namespace monkeys_ear {

namespace {

inline bool finite_safe(float x) noexcept {
    const uint32_t bits = std::bit_cast<uint32_t>(x);
    return (bits & 0x7F800000u) != 0x7F800000u;
}

inline EcosystemStreamFrame sanitize_frame(const EcosystemStreamFrame& in) noexcept {
    EcosystemStreamFrame f = in;
    f.pitch_cents = finite_safe(f.pitch_cents) ? clamp(f.pitch_cents, -4800.0f, 8400.0f) : 0.0f;
    f.pitch_confidence = finite_safe(f.pitch_confidence) ? clamp(f.pitch_confidence, 0.0f, 1.0f) : 0.0f;
    f.note_gate = finite_safe(f.note_gate) ? clamp(f.note_gate, 0.0f, 1.0f) : 0.0f;
    f.velocity = finite_safe(f.velocity) ? clamp(f.velocity, 0.0f, 1.0f) : 0.0f;
    f.loudness = finite_safe(f.loudness) ? clamp(f.loudness, 0.0f, 1.0f) : 0.0f;
    for (float& b : f.spectral_occupancy) {
        b = finite_safe(b) ? clamp(b, 0.0f, 1.0f) : 0.0f;
    }
    return f;
}

inline void seqlock_write(std::atomic<uint64_t>& seq, EcosystemStreamFrame& dst, const EcosystemStreamFrame& src) noexcept {
    const uint64_t s = seq.load(std::memory_order_relaxed);
    seq.store(s + 1, std::memory_order_release); // odd = writing
    std::atomic_signal_fence(std::memory_order_acq_rel);
    dst = src;
    std::atomic_signal_fence(std::memory_order_acq_rel);
    seq.store(s + 2, std::memory_order_release); // even = committed
}

inline bool seqlock_read(const std::atomic<uint64_t>& seq, const EcosystemStreamFrame& src, EcosystemStreamFrame& out) noexcept {
    for (int attempt = 0; attempt < 4; ++attempt) {
        const uint64_t s0 = seq.load(std::memory_order_acquire);
        if ((s0 & 1u) != 0u) {
            continue;
        }
        if (s0 == 0u) {
            return false;
        }
        const EcosystemStreamFrame copy = src;
        std::atomic_signal_fence(std::memory_order_acquire);
        const uint64_t s1 = seq.load(std::memory_order_acquire);
        if (s0 == s1) {
            out = sanitize_frame(copy);
            return out.active;
        }
    }
    return false;
}

EcosystemSharedMemory& fallback_static_bus() noexcept {
    static EcosystemSharedMemory bus{};
    if (bus.magic != EcosystemSharedMemory::kMagic) {
        bus.magic = EcosystemSharedMemory::kMagic;
    }
    return bus;
}

} // namespace

EcosystemBusClient::EcosystemBusClient() noexcept {
    ensure_connected();
}

EcosystemBusClient::~EcosystemBusClient() noexcept {
#ifdef _WIN32
    if (owns_view_ && mem_ != nullptr) {
        ::UnmapViewOfFile(mem_);
    }
    if (platform_handle_ != nullptr) {
        ::CloseHandle(static_cast<HANDLE>(platform_handle_));
    }
#endif
    mem_ = nullptr;
    platform_handle_ = nullptr;
    owns_view_ = false;
}

void EcosystemBusClient::ensure_connected() noexcept {
    if (mem_ != nullptr) {
        return;
    }
#ifdef _WIN32
    wchar_t name[96]{};
    const DWORD pid = ::GetCurrentProcessId();
    _snwprintf_s(name, 96, _TRUNCATE, L"Local\\MonkeysEarEcoBus4_%lu", static_cast<unsigned long>(pid));
    HANDLE h = ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                    static_cast<DWORD>(sizeof(EcosystemSharedMemory)), name);
    if (h != nullptr) {
        const bool already_exists = (::GetLastError() == ERROR_ALREADY_EXISTS);
        void* view = ::MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(EcosystemSharedMemory));
        if (view != nullptr) {
            auto* shm = static_cast<EcosystemSharedMemory*>(view);
            if (!already_exists || shm->magic != EcosystemSharedMemory::kMagic) {
                new (shm) EcosystemSharedMemory{};
                shm->magic = EcosystemSharedMemory::kMagic;
                shm->version = 3;
            }
            mem_ = shm;
            platform_handle_ = h;
            owns_view_ = true;
            return;
        }
        ::CloseHandle(h);
    }
#endif
    mem_ = &fallback_static_bus();
    owns_view_ = false;
}

void EcosystemBusClient::publish_synth(int slot, const EcosystemStreamFrame& frame) noexcept {
    if (!mem_) return;
    const size_t idx = static_cast<size_t>(clamp(slot, 0, static_cast<int>(kSlotCount) - 1));
    EcosystemStreamFrame clean_f = sanitize_frame(frame);
    clean_f.role = 1;
    seqlock_write(mem_->slots[idx].synth_seq, mem_->slots[idx].synth, clean_f);
}

void EcosystemBusClient::publish_vocal(int slot, const EcosystemStreamFrame& frame) noexcept {
    if (!mem_) return;
    const size_t idx = static_cast<size_t>(clamp(slot, 0, static_cast<int>(kSlotCount) - 1));
    EcosystemStreamFrame clean_f = sanitize_frame(frame);
    clean_f.role = 2;
    seqlock_write(mem_->slots[idx].vocal_seq, mem_->slots[idx].vocal, clean_f);
}

void EcosystemBusClient::publish_aux_eq(int slot, const EcosystemStreamFrame& frame) noexcept {
    if (!mem_) return;
    const size_t idx = static_cast<size_t>(clamp(slot, 0, static_cast<int>(kSlotCount) - 1));
    EcosystemStreamFrame clean_f = sanitize_frame(frame);
    clean_f.role = 3;
    seqlock_write(mem_->slots[idx].aux_eq_seq, mem_->slots[idx].aux_eq, clean_f);
}

void EcosystemBusClient::publish_inactive(int slot, int role_index) noexcept {
    if (!mem_) return;
    const size_t idx = static_cast<size_t>(clamp(slot, 0, static_cast<int>(kSlotCount) - 1));
    EcosystemStreamFrame cleared{};
    cleared.active = false;
    cleared.note_gate = 0.0f;
    if (role_index == 0) {
        cleared.role = 1;
        seqlock_write(mem_->slots[idx].synth_seq, mem_->slots[idx].synth, cleared);
    } else if (role_index == 1) {
        cleared.role = 2;
        seqlock_write(mem_->slots[idx].vocal_seq, mem_->slots[idx].vocal, cleared);
    } else if (role_index == 2) {
        cleared.role = 3;
        seqlock_write(mem_->slots[idx].aux_eq_seq, mem_->slots[idx].aux_eq, cleared);
    }
}

bool EcosystemBusClient::read_synth(int slot, EcosystemStreamFrame& out) const noexcept {
    if (!mem_) return false;
    const size_t idx = static_cast<size_t>(clamp(slot, 0, static_cast<int>(kSlotCount) - 1));
    return seqlock_read(mem_->slots[idx].synth_seq, mem_->slots[idx].synth, out);
}

bool EcosystemBusClient::read_vocal(int slot, EcosystemStreamFrame& out) const noexcept {
    if (!mem_) return false;
    const size_t idx = static_cast<size_t>(clamp(slot, 0, static_cast<int>(kSlotCount) - 1));
    return seqlock_read(mem_->slots[idx].vocal_seq, mem_->slots[idx].vocal, out);
}

bool EcosystemBusClient::read_aux_eq(int slot, EcosystemStreamFrame& out) const noexcept {
    if (!mem_) return false;
    const size_t idx = static_cast<size_t>(clamp(slot, 0, static_cast<int>(kSlotCount) - 1));
    return seqlock_read(mem_->slots[idx].aux_eq_seq, mem_->slots[idx].aux_eq, out);
}

bool EcosystemBusClient::read_synth_or_any(int preferred_slot, EcosystemStreamFrame& out) const noexcept {
    if (read_synth(preferred_slot, out)) {
        return true;
    }
    if (!mem_) return false;
    for (size_t i = 0; i < kSlotCount; ++i) {
        if (seqlock_read(mem_->slots[i].synth_seq, mem_->slots[i].synth, out)) {
            return true;
        }
    }
    return false;
}

} // namespace monkeys_ear
