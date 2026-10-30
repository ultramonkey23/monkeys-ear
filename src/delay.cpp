#include "monkeys_ear/delay.h"
#include <cmath>

namespace monkeys_ear {

StereoDelay::StereoDelay()
    : sample_rate_(48000.0f),
      time_l_(0.300f),
      time_r_(0.450f),
      target_time_l_(0.300f),
      target_time_r_(0.450f),
      feedback_(0.35f),
      cross_feedback_(0.25f),
      high_damping_(0.30f),
      mix_(0.30f),
      write_pos_(0),
      damp_l_(0.0f),
      damp_r_(0.0f) {
    buffer_l_.resize(MAX_DELAY_SAMPLES, 0.0f);
    buffer_r_.resize(MAX_DELAY_SAMPLES, 0.0f);
}

void StereoDelay::set_sample_rate(float sr) {
    // Host setup is the sole place permitted to enlarge delay storage.
    // Class-level 3.5s range must remain true at 44.1/48/96/192/384 kHz.
    sample_rate_=clamp(sr,1000.0f,384000.0f);
    const size_t required=std::max(MAX_DELAY_SAMPLES,
        static_cast<size_t>(std::ceil(sample_rate_*3.5f))+8);
    if(buffer_l_.size()!=required){
        buffer_l_.assign(required,0.0f);
        buffer_r_.assign(required,0.0f);
        write_pos_=0;
        damp_l_=damp_r_=0.0f;
    }
    time_slew_coefficient_=1.0f-std::exp(-1.0f/(0.035f*sample_rate_));
    time_l_=target_time_l_;
    time_r_=target_time_r_;
}

void StereoDelay::set_time_left(float seconds) {
    target_time_l_ = clamp(seconds, 0.001f, 3.5f);
}

void StereoDelay::set_time_right(float seconds) {
    target_time_r_ = clamp(seconds, 0.001f, 3.5f);
}

void StereoDelay::set_feedback(float fb) {
    feedback_ = clamp(fb, 0.0f, 0.95f);
}

void StereoDelay::set_cross_feedback(float xfb) {
    cross_feedback_ = clamp(xfb, 0.0f, 1.0f);
}

void StereoDelay::set_high_damping(float damp) {
    high_damping_ = clamp(damp, 0.0f, 0.95f);
}

void StereoDelay::set_mix(float mix) {
    mix_ = clamp(mix, 0.0f, 1.0f);
}

void StereoDelay::reset() {
    std::fill(buffer_l_.begin(), buffer_l_.end(), 0.0f);
    std::fill(buffer_r_.begin(), buffer_r_.end(), 0.0f);
    write_pos_ = 0;
    damp_l_ = 0.0f;
    damp_r_ = 0.0f;
    time_l_=target_time_l_;
    time_r_=target_time_r_;
}

float StereoDelay::read_interpolated(const std::vector<float>& buf, float delay_samples) const {
    const size_t capacity=buf.size();
    float r_pos = static_cast<float>(write_pos_) - delay_samples;
    while (r_pos < 0.0f) r_pos += static_cast<float>(capacity);
    while (r_pos >= static_cast<float>(capacity)) r_pos -= static_cast<float>(capacity);

    int idx1 = static_cast<int>(r_pos);
    float frac = r_pos - static_cast<float>(idx1);

    int idx0 = (idx1 - 1 + capacity) % capacity;
    int idx2 = (idx1 + 1) % capacity;
    int idx3 = (idx1 + 2) % capacity;

    float y0 = buf[idx0];
    float y1 = buf[idx1];
    float y2 = buf[idx2];
    float y3 = buf[idx3];

    // 4-point Hermite cubic interpolation
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void StereoDelay::process(float in_l, float in_r, float& out_l, float& out_r) {
    // Tick the feedback delay while dry. A later fade-in must not reveal
    // a frozen buffer from an earlier, unrelated performance moment.

    // Tape-like time changes glide rather than moving the tap abruptly.
    time_l_+=(target_time_l_-time_l_)*time_slew_coefficient_;
    time_r_+=(target_time_r_-time_r_)*time_slew_coefficient_;
    const float delay_samples_l=time_l_*sample_rate_;
    const float delay_samples_r=time_r_*sample_rate_;

    float delayed_l = read_interpolated(buffer_l_, delay_samples_l);
    float delayed_r = read_interpolated(buffer_r_, delay_samples_r);

    // High frequency damping in feedback path
    damp_l_ = lerp(delayed_l, damp_l_, high_damping_);
    damp_r_ = lerp(delayed_r, damp_r_, high_damping_);

    // Soft saturation on feedback to guarantee stability
    float fb_sig_l = fast_tanh(damp_l_);
    float fb_sig_r = fast_tanh(damp_r_);

    // Cross feedback calculation (ping-pong)
    float fb_l = lerp(fb_sig_l, fb_sig_r, cross_feedback_) * feedback_;
    float fb_r = lerp(fb_sig_r, fb_sig_l, cross_feedback_) * feedback_;

    // Write to delay buffer
    buffer_l_[write_pos_] = sanitize(in_l + fb_l);
    buffer_r_[write_pos_] = sanitize(in_r + fb_r);

    write_pos_ = (write_pos_ + 1) % buffer_l_.size();

    // Wet/Dry mix
    out_l = sanitize(lerp(in_l, delayed_l, mix_));
    out_r = sanitize(lerp(in_r, delayed_r, mix_));
}

} // namespace monkeys_ear
