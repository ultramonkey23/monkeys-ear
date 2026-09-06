#include "monkeys_ear/fdn_reverb.h"
#include <cmath>
#include <algorithm>

namespace monkeys_ear {

// 8 mutual coprime delay lengths in samples (at 44.1kHz baseline)
// Tuned for high echo density and zero flutter echoes
static const size_t BASE_DELAYS[8] = {
    541, 691, 853, 1069, 1373, 1723, 2179, 2749
};

FDNReverb::FDNReverb()
    : sample_rate_(48000.0f),
      room_size_(1.0f),
      decay_time_(2.2f),
      damping_(0.35f),
      pre_delay_s_(0.015f),
      stereo_width_(0.85f),
      mix_(0.25f),
      pre_delay_pos_(0) {
    pre_delay_buffer_.resize(19200, 0.0f); // up to 100ms @ 192kHz
    damp_states_.fill(0.0f);
    write_indices_.fill(0);
    recalculate();
}

void FDNReverb::set_sample_rate(float sr) {
    sample_rate_ = std::max(1000.0f, sr);
    recalculate();
}

void FDNReverb::set_room_size(float size) {
    room_size_ = clamp(size, 0.4f, 2.5f);
    recalculate();
}

void FDNReverb::set_decay_time(float t60_s) {
    decay_time_ = clamp(t60_s, 0.1f, 20.0f);
    recalculate();
}

void FDNReverb::set_damping(float damp) {
    damping_ = clamp(damp, 0.0f, 0.95f);
}

void FDNReverb::set_pre_delay(float seconds) {
    pre_delay_s_ = clamp(seconds, 0.0f, 0.1f);
}

void FDNReverb::set_stereo_width(float width) {
    stereo_width_ = clamp(width, 0.0f, 1.0f);
}

void FDNReverb::set_mix(float mix) {
    mix_ = clamp(mix, 0.0f, 1.0f);
}

void FDNReverb::reset() {
    for (auto& buf : delay_buffers_) {
        std::fill(buf.begin(), buf.end(), 0.0f);
    }
    std::fill(pre_delay_buffer_.begin(), pre_delay_buffer_.end(), 0.0f);
    damp_states_.fill(0.0f);
    write_indices_.fill(0);
    pre_delay_pos_ = 0;
    for (auto& diff : input_diffusers_) {
        diff.reset();
    }
}

void FDNReverb::recalculate() {
    float sr_scale = sample_rate_ / 44100.0f;

    // Initialize 4 diffusion allpass stages
    size_t diff_delays[4] = {
        static_cast<size_t>(142 * sr_scale * room_size_),
        static_cast<size_t>(237 * sr_scale * room_size_),
        static_cast<size_t>(379 * sr_scale * room_size_),
        static_cast<size_t>(511 * sr_scale * room_size_)
    };
    for (size_t i = 0; i < 4; ++i) {
        input_diffusers_[i].init(std::max<size_t>(10, diff_delays[i]), 0.6f);
    }

    // Initialize 8 FDN delay lines
    for (size_t i = 0; i < NUM_DELAYS; ++i) {
        size_t len = static_cast<size_t>(static_cast<float>(BASE_DELAYS[i]) * sr_scale * room_size_);
        len = std::max<size_t>(16, len);
        delay_lengths_[i] = len;

        if (delay_buffers_[i].size() != len) {
            delay_buffers_[i].assign(len, 0.0f);
            write_indices_[i] = 0;
        }

        // T60 decay formula: gain = 10^(-3 * delay_time / T60)
        float delay_time_s = static_cast<float>(len) / sample_rate_;
        decay_gains_[i] = std::pow(10.0f, -3.0f * delay_time_s / decay_time_);
    }
}

void FDNReverb::apply_hadamard_matrix(std::array<float, NUM_DELAYS>& s) const {
    // Fast Walsh-Hadamard Transform of size 8
    // Stage 1 (stride 1)
    float a0 = s[0] + s[1]; float a1 = s[0] - s[1];
    float a2 = s[2] + s[3]; float a3 = s[2] - s[3];
    float a4 = s[4] + s[5]; float a5 = s[4] - s[5];
    float a6 = s[6] + s[7]; float a7 = s[6] - s[7];

    // Stage 2 (stride 2)
    float b0 = a0 + a2; float b1 = a1 + a3;
    float b2 = a0 - a2; float b3 = a1 - a3;
    float b4 = a4 + a6; float b5 = a5 + a7;
    float b6 = a4 - a6; float b7 = a5 - a7;

    // Stage 3 (stride 4)
    constexpr float INV_SQRT8 = 0.35355339059327376f;
    s[0] = (b0 + b4) * INV_SQRT8;
    s[1] = (b1 + b5) * INV_SQRT8;
    s[2] = (b2 + b6) * INV_SQRT8;
    s[3] = (b3 + b7) * INV_SQRT8;
    s[4] = (b0 - b4) * INV_SQRT8;
    s[5] = (b1 - b5) * INV_SQRT8;
    s[6] = (b2 - b6) * INV_SQRT8;
    s[7] = (b3 - b7) * INV_SQRT8;
}

void FDNReverb::process(float in_l, float in_r, float& out_l, float& out_r) {
    if (mix_ < 0.001f) {
        out_l = in_l;
        out_r = in_r;
        return;
    }

    // Input mono mix for reverb engine
    float in_mono = 0.5f * (in_l + in_r);

    // Pre-delay
    size_t pre_delay_samples = static_cast<size_t>(pre_delay_s_ * sample_rate_);
    pre_delay_samples = std::min(pre_delay_samples, pre_delay_buffer_.size() - 1);

    size_t read_pre_pos = (pre_delay_pos_ + pre_delay_buffer_.size() - pre_delay_samples) % pre_delay_buffer_.size();
    float pre_delayed = pre_delay_buffer_[read_pre_pos];
    pre_delay_buffer_[pre_delay_pos_] = in_mono;
    pre_delay_pos_ = (pre_delay_pos_ + 1) % pre_delay_buffer_.size();

    // Input diffusion through 4 cascaded allpass filters
    float diffused = pre_delayed;
    for (auto& diff : input_diffusers_) {
        diffused = diff.process(diffused);
    }

    // Read delay lines output
    std::array<float, NUM_DELAYS> delay_outputs;
    for (size_t i = 0; i < NUM_DELAYS; ++i) {
        delay_outputs[i] = delay_buffers_[i][write_indices_[i]];
    }

    // Apply per-delay one-pole damping filter & T60 decay gain
    for (size_t i = 0; i < NUM_DELAYS; ++i) {
        damp_states_[i] = lerp(delay_outputs[i], damp_states_[i], damping_);
        delay_outputs[i] = damp_states_[i] * decay_gains_[i];
    }

    // Mix delay lines into stereo outputs (decorrelated left and right taps)
    // Left: +1, -1, +1, -1, +1, -1, +1, -1
    // Right: +1, +1, -1, -1, +1, +1, -1, -1
    float rev_l = delay_outputs[0] - delay_outputs[1] + delay_outputs[2] - delay_outputs[3] +
                  delay_outputs[4] - delay_outputs[5] + delay_outputs[6] - delay_outputs[7];
    float rev_r = delay_outputs[0] + delay_outputs[1] - delay_outputs[2] - delay_outputs[3] +
                  delay_outputs[4] + delay_outputs[5] - delay_outputs[6] - delay_outputs[7];

    rev_l *= 0.25f;
    rev_r *= 0.25f;

    // Apply stereo width cross-matrix
    float mid = 0.5f * (rev_l + rev_r);
    float side = 0.5f * (rev_l - rev_r) * stereo_width_;
    rev_l = mid + side;
    rev_r = mid - side;

    // Multiply by orthogonal Hadamard feedback matrix for next iteration
    apply_hadamard_matrix(delay_outputs);

    // Feedback inject input and write to delay lines
    // Alternate signs for input injection to improve diffusion
    float sign = 1.0f;
    for (size_t i = 0; i < NUM_DELAYS; ++i) {
        float next_sample = delay_outputs[i] + diffused * sign;
        // Soft saturate feedback to prevent any numerical explosion
        delay_buffers_[i][write_indices_[i]] = fast_tanh(next_sample);
        write_indices_[i] = (write_indices_[i] + 1) % delay_lengths_[i];
        sign = -sign;
    }

    // Mix dry and wet
    out_l = sanitize(lerp(in_l, rev_l, mix_));
    out_r = sanitize(lerp(in_r, rev_r, mix_));
}

} // namespace monkeys_ear
