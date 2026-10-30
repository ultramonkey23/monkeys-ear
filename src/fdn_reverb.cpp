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
    prepare_buffers(); // constructor is outside real-time processing
    damp_states_.fill(0.0f);
}

void FDNReverb::set_sample_rate(float sr) {
    // setupProcessing prepares buffers before the first process callback.
    sample_rate_=clamp(sr,1000.0f,384000.0f);
    prepare_buffers();
}

void FDNReverb::set_room_size(float size) {
    const float next=clamp(size,0.4f,2.5f);
    if(next==room_size_)return; // unrelated parameters must not restart the tail
    room_size_=next;
    recalculate();
}

void FDNReverb::set_decay_time(float t60_s) {
    const float next=clamp(t60_s,0.1f,20.0f);
    if(next==decay_time_)return;
    decay_time_=next;
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
    current_taps_=target_taps_;
    current_decay_gains_=target_decay_gains_;
    pre_delay_pos_ = 0;
    for (auto& diff : input_diffusers_) {
        diff.reset();
    }
}

void FDNReverb::prepare_buffers() {
    const float scale=sample_rate_/44100.0f;
    // Max room of 2.5x plus interpolation guard. No vector resize,
    // allocation or deallocation can occur inside process or automation.
    for(size_t i=0;i<NUM_DELAYS;++i){
        const size_t capacity=std::max<size_t>(32,
            static_cast<size_t>(std::ceil(BASE_DELAYS[i]*scale*2.5f))+8);
        delay_buffers_[i].assign(capacity,0.0f);
        write_indices_[i]=0;
    }
    constexpr size_t DIFF_DELAYS[4]={142,237,379,511};
    for(size_t i=0;i<4;++i){
        const float initial=static_cast<float>(std::max<size_t>(10,
            static_cast<size_t>(DIFF_DELAYS[i]*scale*room_size_)));
        const size_t capacity=std::max<size_t>(32,
            static_cast<size_t>(std::ceil(DIFF_DELAYS[i]*scale*2.5f))+8);
        input_diffusers_[i].prepare(capacity,initial,.6f,sample_rate_);
    }
    // Adapt the fixed pre-delay to the actual host sample rate in setup.
    pre_delay_buffer_.assign(std::max<size_t>(128,
        static_cast<size_t>(std::ceil(sample_rate_*.1f))+8),0.f);
    pre_delay_pos_=0;
    damp_states_.fill(0.f);
    slew_coefficient_=1.0f-std::exp(-1.0f/(.035f*sample_rate_));
    recalculate(true);
}

void FDNReverb::recalculate(bool snap) noexcept {
    const float scale=sample_rate_/44100.0f;
    constexpr size_t DIFF_DELAYS[4]={142,237,379,511};
    for(size_t i=0;i<4;++i)
        input_diffusers_[i].set_delay(static_cast<float>(std::max<size_t>(10,
            static_cast<size_t>(DIFF_DELAYS[i]*scale*room_size_))));
    for(size_t i=0;i<NUM_DELAYS;++i){
        const size_t length=std::max<size_t>(16,
            static_cast<size_t>(BASE_DELAYS[i]*scale*room_size_));
        const float taps=static_cast<float>(std::min(
            length,delay_buffers_[i].size()-2));
        target_taps_[i]=taps;
        const float delay_seconds=taps/sample_rate_;
        target_decay_gains_[i]=std::pow(10.0f,
                          -3.0f*delay_seconds/decay_time_);
        if(snap){
            current_taps_[i]=target_taps_[i];
            current_decay_gains_[i]=target_decay_gains_[i];
        }
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
    // Even at zero wet mix the live network must keep advancing: otherwise
    // a later automation raise resurrects a frozen, unrelated old tail.

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
    for(size_t i=0;i<NUM_DELAYS;++i){
        // A continuously slewed fractional read tap changes the perceived
        // room without reallocating buffers, clearing tails or clicky jumps.
        current_taps_[i]+=(target_taps_[i]-current_taps_[i])*slew_coefficient_;
        current_decay_gains_[i]+=(target_decay_gains_[i]-
                                  current_decay_gains_[i])*slew_coefficient_;
        const auto& line=delay_buffers_[i];
        float read_pos=static_cast<float>(write_indices_[i])-current_taps_[i];
        if(read_pos<0.0f)read_pos+=static_cast<float>(line.size());
        const size_t idx=static_cast<size_t>(read_pos);
        const size_t next=(idx+1)%line.size();
        delay_outputs[i]=lerp(line[idx],line[next],
            read_pos-static_cast<float>(idx));
        damp_states_[i]=lerp(delay_outputs[i],damp_states_[i],damping_);
        delay_outputs[i]=damp_states_[i]*current_decay_gains_[i];
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
        write_indices_[i] = (write_indices_[i] + 1) % delay_buffers_[i].size();
        sign = -sign;
    }

    // Mix dry and wet
    out_l = sanitize(lerp(in_l, rev_l, mix_));
    out_r = sanitize(lerp(in_r, rev_r, mix_));
}

} // namespace monkeys_ear
