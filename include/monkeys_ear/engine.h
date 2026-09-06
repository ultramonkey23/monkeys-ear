#pragma once

#include "monkeys_ear/types.h"
#include "monkeys_ear/voice.h"
#include "monkeys_ear/filter.h"
#include "monkeys_ear/drive_tube.h"
#include "monkeys_ear/resonator_cab.h"
#include "monkeys_ear/delay.h"
#include "monkeys_ear/fdn_reverb.h"
#include "monkeys_ear/audio_input.h"
#include "monkeys_ear/safety_limiter.h"
#include "monkeys_ear/latency_meter.h"
#include "monkeys_ear/preset.h"

namespace monkeys_ear {

class MonkeysEarEngine {
public:
    MonkeysEarEngine();
    ~MonkeysEarEngine() = default;

    // Initialization & lifecycle (called from UI / Host config thread)
    void init(float sample_rate, size_t max_block_size);
    void reset();

    // Real-time Audio Callback (CALLED ON AUDIO THREAD - MUST BE ZERO ALLOC / LOCK FREE)
    void process_block(
        const float* input_l,
        const float* input_r,
        float* output_l,
        float* output_r,
        size_t num_samples
    );

    // MIDI Event Input (CALLED ON AUDIO THREAD)
    void handle_midi_note_on(int note, float velocity);
    void handle_midi_note_off(int note);
    void handle_midi_pitch_bend(float semitones);
    void handle_midi_cc(int cc_number, float value_0_to_1);
    void handle_all_notes_off();

    // Macro & Preset Control
    void set_macro(MacroId id, float value);
    float get_macro(MacroId id) const;
    void load_preset(const PresetData& preset);
    const PresetData& get_current_preset() const;

    // Latency & Real-time Metrics Query
    LatencyStats get_latency_stats() const;
    float get_mic_rms() const { return audio_input_.get_rms_level(); }
    float get_mic_peak() const { return audio_input_.get_peak_level(); }
    size_t get_active_voices() const { return voice_manager_.active_voice_count(); }

private:
    float sample_rate_;
    size_t max_block_size_;

    VoiceManager voice_manager_;
    AudioInputProcessor audio_input_;
    StateVariableFilter filter_;
    TubeDriveStage drive_tube_;
    CabinetResonator resonator_cab_;
    StereoDelay delay_;
    FDNReverb fdn_reverb_;
    SafetyLimiter safety_limiter_;
    LatencyMeter latency_meter_;
    PresetManager preset_manager_;

    float master_volume_;

    void apply_macros();
};

} // namespace monkeys_ear
