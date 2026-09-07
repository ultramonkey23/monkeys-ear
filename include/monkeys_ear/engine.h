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
#include "monkeys_ear/chrono_state.h"
#include "monkeys_ear/lfo.h"

namespace monkeys_ear {

class MonkeysEarEngine {
public:
    MonkeysEarEngine();
    ~MonkeysEarEngine() = default;

    // Initialization & lifecycle
    void init(float sample_rate, size_t max_block_size);
    void reset();

    // Real-time Audio Callback (CALLED ON AUDIO THREAD - ZERO ALLOC / LOCK FREE)
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

    // Macro & Parameter Control
    void set_macro(MacroId id, float value);
    float get_macro(MacroId id) const;
    void load_preset(const PresetData& preset);
    const PresetData& get_current_preset() const;

    void set_master_gain_db(float db);
    void set_waveform(Waveform wf);
    void set_osc_fm(float fm);
    void set_osc2_semi(int semi);
    void set_osc_hard_sync(bool sync);
    void set_state_resistance(float r);
    void set_state_repulsion(float k);
    void set_state_coupling(float kappa);
    void set_state_persistence(float tau);
    void set_state_enabled(bool en);
    void set_lfo1_rate(float hz);
    void set_lfo1_depth(float depth);
    void set_input_route_mode(int mode);

    // Latency & Real-time Metrics Query
    LatencyStats get_latency_stats() const;
    void reset_latency_stats() { latency_meter_.reset(); }
    float get_mic_rms() const { return audio_input_.get_rms_level(); }
    float get_mic_peak() const { return audio_input_.get_peak_level(); }
    size_t get_active_voices() const { return voice_manager_.active_voice_count(); }
    const ChronoStateVariables& get_chrono_state() const { return chrono_body_.get_state(); }

private:
    float sample_rate_;
    size_t max_block_size_;

    VoiceManager voice_manager_;
    AudioInputProcessor audio_input_;
    ChronoStateBody chrono_body_;
    LFO lfo1_;
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
