#pragma once

#include "monkeys_ear/types.h"

namespace monkeys_ear {

class AudioInputProcessor {
public:
    static constexpr size_t RING_BUFFER_SIZE = 8192;

    AudioInputProcessor();
    void set_sample_rate(float sr);
    void set_gain(float gain_db);     // -24dB to +24dB
    void set_mix(float mix);         // 0.0 (synth only) to 1.0 (mic only)
    void set_highpass_enabled(bool en);
    void reset();

    // Process one input frame: updates ring buffer, measures levels, returns blended signal
    float process_sample(float mic_in, float synth_in);

    float get_rms_level() const { return rms_level_; }
    float get_peak_level() const { return peak_level_; }
    float read_ring_buffer(size_t lag) const { return ring_buffer_.read(lag); }

private:
    float sample_rate_;
    float gain_linear_;
    float mix_;
    bool highpass_enabled_;

    LockFreeRingBuffer<float, RING_BUFFER_SIZE> ring_buffer_;

    // DC-block / highpass filter
    float hp_x1_;
    float hp_y1_;

    // Envelope followers for live feedback
    float rms_accumulator_;
    float rms_level_;
    float peak_level_;
    size_t rms_count_;
};

} // namespace monkeys_ear
