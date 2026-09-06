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

// Denormal prevention and NaN sanitization
inline float sanitize(float v) {
    if (std::isnan(v) || std::isinf(v)) return 0.0f;
    if (std::abs(v) < DENORMAL_THRESHOLD) return 0.0f;
    return v;
}

// Fast tanh approximation with smooth saturation
inline float fast_tanh(float x) {
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Linear interpolation
inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Clamp utility
inline float clamp(float v, float min_val, float max_val) {
    return std::max(min_val, std::min(max_val, v));
}

// Convert MIDI note number to frequency in Hertz
inline float midi_to_freq(float note) {
    return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
}

// Lock-free circular buffer for continuous live audio/mic capture
template <typename T, size_t Capacity>
class LockFreeRingBuffer {
public:
    LockFreeRingBuffer() : write_idx_(0), read_idx_(0) {
        buffer_.fill(T{});
    }

    void push(T sample) {
        size_t next = (write_idx_ + 1) % Capacity;
        buffer_[write_idx_] = sample;
        write_idx_ = next;
    }

    T read(size_t lag) const {
        if (lag >= Capacity) lag = Capacity - 1;
        size_t idx = (write_idx_ + Capacity - 1 - lag) % Capacity;
        return buffer_[idx];
    }

    size_t capacity() const { return Capacity; }

    void reset() {
        buffer_.fill(T{});
        write_idx_ = 0;
        read_idx_ = 0;
    }

private:
    std::array<T, Capacity> buffer_;
    volatile size_t write_idx_;
    volatile size_t read_idx_;
};

} // namespace monkeys_ear
