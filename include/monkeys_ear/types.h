#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <array>
#include <vector>

namespace monkeys_ear {

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 6.28318530717958647692f;
constexpr float INV_TWO_PI = 0.15915494309189533576f;
constexpr float DENORMAL_THRESHOLD = 1e-15f;

inline float sanitize(float v) {
    if (!std::isfinite(v)) return 0.0f;
    if (std::abs(v) < DENORMAL_THRESHOLD) return 0.0f;
    return v;
}

// Bounded rational tanh approximation. Clamp first so x*x cannot overflow
// when hostile host/input state reaches a nonlinear stage.
inline float fast_tanh(float x) {
    x = std::clamp(sanitize(x), -9.0f, 9.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

inline float clamp(float v, float min_val, float max_val) {
    return std::max(min_val, std::min(max_val, v));
}

inline float midi_to_freq(float note) {
    return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
}

template <typename T, size_t Capacity>
class LockFreeRingBuffer {
    static_assert(Capacity >= 2, "LockFreeRingBuffer requires at least two samples");
public:
    LockFreeRingBuffer() : write_idx_(0) { buffer_.fill(T{}); }

    void push(T sample) noexcept {
        const size_t next = (write_idx_ + 1) % Capacity;
        buffer_[write_idx_] = sample;
        write_idx_ = next;
    }

    T read(size_t lag) const noexcept {
        if (lag >= Capacity) lag = Capacity - 1;
        const size_t idx = (write_idx_ + Capacity - 1 - lag) % Capacity;
        return buffer_[idx];
    }

    T read_interpolated(float lag) const noexcept {
        if (!std::isfinite(lag)) lag = 0.0f;
        lag = std::clamp(lag, 0.0f, static_cast<float>(Capacity - 2));
        const size_t whole = static_cast<size_t>(lag);
        return lerp(read(whole), read(whole + 1), lag - static_cast<float>(whole));
    }

    size_t capacity() const noexcept { return Capacity; }

    void reset() noexcept {
        buffer_.fill(T{});
        write_idx_ = 0;
    }

private:
    std::array<T, Capacity> buffer_;
    size_t write_idx_;
};

} // namespace monkeys_ear