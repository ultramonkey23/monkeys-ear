#pragma once
#include "monkeys_ear/preset.h"
#include <cmath>
#include <algorithm>

namespace monkeys_ear::ui {
// Canonical reverse projection used by BOTH controller and processor when a
// factory preset is selected. No UI-specific DSP state or duplicated patches.
// Selectors and live hardware expression intentionally stay outside snapshots.
template<class Emit>
void emit_preset_params(const PresetData& p,Emit&& emit){
    const auto put=[&](int id,float value){emit(id,clamp(value,0.f,1.f));};
    const auto choice=[](int value,int choices){
        return (static_cast<float>(std::clamp(value,0,choices-1))+.25f)/
            static_cast<float>(choices);
    };
    const auto range=[](float x,float minimum,float maximum){
        return std::log(std::max(x,minimum)/minimum)/std::log(maximum/minimum);
    };
    for(int i=0;i<NUM_MACROS;++i)put(i,p.macros[static_cast<size_t>(i)]);
    put(8,p.master_gain_db<=-99.f?0.f:.85f+p.master_gain_db/40.f);
    put(9,choice(p.synth_waveform,4));
    put(10,p.osc_fm_amount);
    put(11,p.osc2_semi/48.f+.5f);
    put(12,p.osc_hard_sync?1.f:0.f);
    put(13,p.state_resistance);put(14,p.state_repulsion);
    put(15,p.state_coupling);put(16,p.state_persistence);
    put(17,p.state_enabled?1.f:0.f);
    put(18,choice(p.input_route_mode,3));
    put(19,p.fundamental_mix);put(20,p.sub_mix);
    put(21,choice(p.sub_ratio_denominator-1,4));
    put(22,p.sub_phase);put(23,p.sub_polarity_inverted?1.f:0.f);
    put(24,p.sub_saturation);put(25,p.sub_envelope_amount);
    put(26,p.source_level);put(27,p.character_level);
    put(28,choice(p.filter_mode_a,5));
    put(29,range(p.filter_cutoff_b_hz,40.f,18000.f));
    put(30,choice(p.filter_mode_b,5));
    put(31,choice(p.filter_routing,3));
    put(32,p.filter_wet);put(33,p.filter_key_tracking);
    put(34,p.filter_slope_a_24db?1.f:0.f);
    put(35,p.filter_slope_b_24db?1.f:0.f);
    put(36,p.filter_drive/2.f);
    for(int b=0;b<4;++b){
        const int id=37+4*b;
        put(id,choice(p.eq_type[b],6));
        put(id+1,range(p.eq_frequency_hz[b],20.f,20000.f));
        put(id+2,p.eq_gain_db[b]/36.f+.5f);
        put(id+3,range(p.eq_q[b],.15f,12.f));
    }
    put(53,p.eq_bypass?1.f:0.f);
    put(54,p.eq_gain_compensation?1.f:0.f);
    put(55,range(p.lfo1_rate_hz,.05f,30.f));
    put(56,choice(p.lfo1_waveform,5));
    put(57,p.motion_curve);
    put(58,p.mod_lfo_cutoff*.5f+.5f);
    put(59,p.mod_lfo_resonance*.5f+.5f);
    put(60,p.mod_lfo_fm*.5f+.5f);
    put(61,p.mod_lfo_sub_blend*.5f+.5f);
    put(62,p.mod_lfo_drive*.5f+.5f);
    put(63,p.mod_lfo_eq_frequency*.5f+.5f);
    put(64,p.mod_lfo_eq_gain*.5f+.5f);
    put(65,p.mod_state_direction_filter*.5f+.5f);
    put(66,p.mod_state_fast_fm*.5f+.5f);
    put(67,p.mod_state_slow_balance*.5f+.5f);
    put(68,p.mod_state_slow_resonator*.5f+.5f);
    put(69,p.mod_audio_envelope_drive*.5f+.5f);
    put(70,p.mono_mode?1.f:0.f);
    put(71,p.legato?1.f:0.f);
    put(72,std::sqrt(std::max(0.f,p.portamento_seconds/2.f)));
    put(73,(p.pitch_bend_range-1.f)/23.f);
    put(74,p.vibrato_depth_semitones/2.f);
    put(75,p.velocity_tone);
    put(76,p.aftertouch_filter*.5f+.5f);
    put(77,p.aftertouch_drive*.5f+.5f);
    put(78,p.lfo1_depth);
    // 79 legacy patch selector; 80/81 live hardware, 172 CC64, 173 palette.
    const auto& ss=p.sound_space;
    put(82,ss.enabled?1.f:0.f);
    put(83,ss.pitch_bound_cents/100.f);
    put(84,ss.harmonic_bound_cents/100.f);
    put(85,ss.modal_bound_cents/100.f);
    put(86,range(ss.movement_hz,.02f,20.f));
    put(87,ss.attraction);put(88,ss.resistance);put(89,ss.repulsion);
    put(90,ss.frequency_freedom);put(91,ss.energy_widen*.5f+.5f);
    put(92,ss.attack_freedom);put(93,ss.release_relaxation);
    put(94,ss.pitch_mix);put(95,ss.harmonic_mix);
    put(96,ss.modal_mix);put(97,ss.spectral_depth_db/18.f);
    put(98,ss.spectral_priority);
    put(99,ss.phase_offset_cycles*2.f+.5f);
    put(100,ss.phase_coupling);
    for(int b=0;b<4;++b){
        const int id=101+b*3;
        put(id,choice(ss.routes[b].source,8));
        put(id+1,choice(ss.routes[b].destination,8));
        put(id+2,ss.routes[b].depth*.5f+.5f);
    }
    const auto& vocal=p.vocal_expression;
    put(113,vocal.enabled?1.f:0.f);
    put(114,vocal.correction_strength);put(115,vocal.drift_retention);
    put(116,vocal.vibrato_retention);put(117,vocal.transition);
    put(118,vocal.formant_repair);put(119,vocal.spectral_residual_mix);
    put(120,vocal.character);put(121,vocal.mix);
    put(122,vocal.sequential_stage_mix);
    put(123,vocal.aperiodic_protection);
    const auto& rack=p.tube_rack;
    put(124,rack.enabled?1.f:0.f);
    put(125,rack.parallel?1.f:0.f);
    put(126,rack.shared_supply);put(127,rack.output);
    put(128,rack.antialias?1.f:0.f);
    for(int b=0;b<4;++b){
        const auto& t=rack.slots[b];const int id=129+8*b;
        put(id,t.enabled?1.f:0.f);
        put(id+1,choice(t.flavor,kTubeFlavorCount));
        put(id+2,t.drive);put(id+3,t.bias*.5f+.5f);
        put(id+4,t.plate_load);put(id+5,t.sag);
        put(id+6,t.coupling);put(id+7,t.mix);
    }
    const auto env=[](float seconds){
        return std::sqrt(std::clamp((seconds-.001f)/6.f,0.f,1.f));
    };
    put(161,env(p.amp_attack));put(162,env(p.amp_decay));
    put(163,p.amp_sustain);put(164,env(p.amp_release));
    put(165,env(p.filter_attack));put(166,env(p.filter_decay));
    put(167,p.filter_sustain);put(168,env(p.filter_release));
    put(169,p.lfo_tempo_sync?1.f:0.f);
    put(170,(p.tempo_bpm-40.f)/200.f);
    put(171,choice(p.lfo_subdivision,12));
    put(174,p.lfo_follow_host_tempo?1.f:0.f);
    put(175,p.lfo_groove);
    put(176,p.performance_drift);
    put(177,p.performance_fracture);

    put(178,choice(p.wave_a_family,6));
    put(179,choice(p.wave_b,9));
    put(180,clamp(p.wave_morph,0.0f,1.0f));
    put(181,clamp(p.phase_warp,0.0f,1.0f));
    put(182,clamp(p.wave_fold,0.0f,1.0f));
    put(183,choice(std::max(0,p.osc_unison-1),5));
    put(184,clamp(p.unison_detune_cents/50.0f,0.0f,1.0f));
    put(185,clamp(p.sub_harmonics,0.0f,1.0f));
    put(186,clamp((p.env_to_morph+1.0f)*0.5f,0.0f,1.0f));
    put(187,clamp(p.lfo_to_morph,0.0f,1.0f));
    put(188,clamp(p.gen_shape,0.0f,1.0f));
    put(189,clamp(p.gen_odd_even,0.0f,1.0f));
    put(190,clamp(p.gen_tilt,0.0f,1.0f));
    put(191,clamp(p.gen_formant,0.0f,1.0f));

    put(192,p.onion_enabled?1.0f:0.0f);
    put(193,choice(p.onion_intent,3));
    put(194,clamp(p.onion_amount,0.0f,1.0f));
    put(195,clamp(p.onion_sensitivity,0.0f,1.0f));
    put(196,clamp(p.onion_low_protect,0.0f,1.0f));
    put(197,clamp(p.onion_transient_protect,0.0f,1.0f));

    put(198,choice(p.eco_mode,4));
    put(199,choice(p.eco_slot,8));
    put(200,clamp(p.eco_peer_weight,0.0f,1.0f));
    put(201,clamp(p.eco_pitch_follow,0.0f,1.0f));

    put(202,choice(p.osc2_wave,10));
    put(203,clamp((p.osc2_fine_cents/100.0f)+0.5f,0.0f,1.0f));
    put(204,clamp(p.ring_mod_mix,0.0f,1.0f));
    put(205,clamp(p.unison_width,0.0f,1.0f));
    put(206,clamp(p.analog_drift,0.0f,1.0f));
    for(int h=0;h<8;++h){
        put(207+h,clamp(p.gen_harmonics[static_cast<size_t>(h)],0.0f,1.0f));
    }
    put(215,clamp(p.auto_morph_rate/8.0f,0.0f,1.0f));
    put(216,choice(p.morph_curve,4));
    put(217,clamp(p.osc_balance,0.0f,1.0f));
    put(218,clamp((p.onion_tilt_db/12.0f)+0.5f,0.0f,1.0f));
    put(219,clamp((p.onion_q_focus-0.85f)/(4.5f-0.85f),0.0f,1.0f));
    put(220,clamp((p.onion_air_db/12.0f)+0.5f,0.0f,1.0f));
    for(int b=0;b<24;++b){
        put(221+b,clamp((p.onion_band_db[static_cast<size_t>(b)]/24.0f)+0.5f,0.0f,1.0f));
    }
    const int semi_from_a = ((static_cast<int>(std::lround(p.vocal_expression.target.tuning.root_cents / 100.0f)) % 12) + 12) % 12;
    const int root_idx = (semi_from_a + 9) % 12; // A=0c -> 9, C=300c -> 0
    const int tuning_idx = std::clamp(static_cast<int>(p.vocal_expression.target.tuning.builtin), 0, 29);
    put(245, static_cast<float>(root_idx) / 11.0f);
    put(246, static_cast<float>(tuning_idx) / 29.0f);
    put(247, clamp((p.vocal_expression.target.articulation + 1.0f) * 0.5f, 0.0f, 1.0f));
    put(248, clamp(p.vocal_expression.target.portamento, 0.0f, 1.0f));
    put(249, clamp(p.vocal_expression.target.target_hysteresis, 0.0f, 1.0f));
    put(250, clamp(p.vocal_expression.target.onset_protection, 0.0f, 1.0f));
    put(251, clamp(p.onion_harmonic_protect, 0.0f, 1.0f));
    put(252, p.pattern_enabled ? 1.0f : 0.0f);
    put(253, static_cast<float>(std::clamp(p.pattern_index, 0, 7)) / 7.0f);
    put(254, static_cast<float>(std::clamp(p.pattern_steps, 1, 32) - 1) / 31.0f);
    put(255, static_cast<float>(std::clamp(p.pattern_division, 0, 5)) / 5.0f);
    put(256, clamp(p.pattern_quantize, 0.0f, 1.0f));
    put(257, clamp((p.pattern_gate - 0.10f) / 0.90f, 0.0f, 1.0f));
    put(258, clamp(static_cast<float>(std::clamp(p.pattern_transpose, -12, 12)) / 24.0f + 0.5f, 0.0f, 1.0f));
}

template <class ReadParam>
inline PresetData preset_from_params(ReadParam&& get, const std::string& growth_snapshot = {}) {
    PresetData p;
    p.growth_snapshot = growth_snapshot;
    const auto unit_log = [](float v, float mn, float mx) {
        return mn * std::pow(mx / mn, clamp(v, 0.0f, 1.0f));
    };
    for (int i = 0; i < NUM_MACROS; ++i) p.macros[static_cast<size_t>(i)] = clamp(get(i), 0.0f, 1.0f);
    const float g = clamp(get(8), 0.0f, 1.0f);
    p.master_gain_db = g > 0.001f ? (g - 0.85f) * 40.0f : -100.0f;
    p.synth_waveform = static_cast<int>(clamp(get(9), 0.0f, 1.0f) * 3.99f);
    p.osc_fm_amount = clamp(get(10), 0.0f, 1.0f);
    p.osc2_semi = static_cast<int>(std::round((clamp(get(11), 0.0f, 1.0f) - 0.5f) * 48.0f));
    p.osc_hard_sync = get(12) >= 0.5f;
    p.state_resistance = clamp(get(13), 0.0f, 1.0f);
    p.state_repulsion = clamp(get(14), 0.0f, 1.0f);
    p.state_coupling = clamp(get(15), 0.0f, 1.0f);
    p.state_persistence = clamp(get(16), 0.0f, 1.0f);
    p.state_enabled = get(17) >= 0.5f;
    p.input_route_mode = static_cast<int>(clamp(get(18), 0.0f, 1.0f) * 2.99f);
    p.fundamental_mix = clamp(get(19), 0.0f, 1.0f);
    p.sub_mix = clamp(get(20), 0.0f, 1.0f);
    p.sub_ratio_denominator = 1 + static_cast<int>(clamp(get(21), 0.0f, 1.0f) * 3.99f);
    p.sub_phase = clamp(get(22), 0.0f, 1.0f);
    p.sub_polarity_inverted = get(23) >= 0.5f;
    p.sub_saturation = clamp(get(24), 0.0f, 1.0f);
    p.sub_envelope_amount = clamp(get(25), 0.0f, 1.0f);
    p.source_level = clamp(get(26), 0.0f, 1.0f);
    p.character_level = clamp(get(27), 0.0f, 1.0f);
    p.filter_mode_a = static_cast<int>(clamp(get(28), 0.0f, 1.0f) * 4.99f);
    p.filter_cutoff_b_hz = unit_log(get(29), 40.0f, 18000.0f);
    p.filter_mode_b = static_cast<int>(clamp(get(30), 0.0f, 1.0f) * 4.99f);
    p.filter_routing = static_cast<int>(clamp(get(31), 0.0f, 1.0f) * 2.99f);
    p.filter_wet = clamp(get(32), 0.0f, 1.0f);
    p.filter_key_tracking = clamp(get(33), 0.0f, 1.0f);
    p.filter_slope_a_24db = get(34) >= 0.5f;
    p.filter_slope_b_24db = get(35) >= 0.5f;
    p.filter_drive = clamp(get(36), 0.0f, 1.0f) * 2.0f;
    for (int b = 0; b < 4; ++b) {
        const int id = 37 + 4 * b;
        p.eq_type[b] = static_cast<int>(clamp(get(id), 0.0f, 1.0f) * 5.99f);
        p.eq_frequency_hz[b] = unit_log(get(id + 1), 20.0f, 20000.0f);
        p.eq_gain_db[b] = (clamp(get(id + 2), 0.0f, 1.0f) - 0.5f) * 36.0f;
        p.eq_q[b] = unit_log(get(id + 3), 0.15f, 12.0f);
    }
    p.eq_bypass = get(53) >= 0.5f;
    p.eq_gain_compensation = get(54) >= 0.5f;
    p.lfo1_rate_hz = unit_log(get(55), 0.05f, 30.0f);
    p.lfo1_waveform = static_cast<int>(clamp(get(56), 0.0f, 1.0f) * 4.99f);
    p.motion_curve = clamp(get(57), 0.0f, 1.0f);
    p.mod_lfo_cutoff = (clamp(get(58), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.mod_lfo_resonance = (clamp(get(59), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.mod_lfo_fm = (clamp(get(60), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.mod_lfo_sub_blend = (clamp(get(61), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.mod_lfo_drive = (clamp(get(62), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.mod_lfo_eq_frequency = (clamp(get(63), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.mod_lfo_eq_gain = (clamp(get(64), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.mod_state_direction_filter = (clamp(get(65), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.mod_state_fast_fm = (clamp(get(66), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.mod_state_slow_balance = (clamp(get(67), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.mod_state_slow_resonator = (clamp(get(68), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.mod_audio_envelope_drive = (clamp(get(69), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.mono_mode = get(70) >= 0.5f;
    p.legato = get(71) >= 0.5f;
    { const float v = clamp(get(72), 0.0f, 1.0f); p.portamento_seconds = v * v * 2.0f; }
    p.pitch_bend_range = 1.0f + clamp(get(73), 0.0f, 1.0f) * 23.0f;
    p.vibrato_depth_semitones = clamp(get(74), 0.0f, 1.0f) * 2.0f;
    p.velocity_tone = clamp(get(75), 0.0f, 1.0f);
    p.aftertouch_filter = (clamp(get(76), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.aftertouch_drive = (clamp(get(77), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.lfo1_depth = clamp(get(78), 0.0f, 1.0f);
    p.sound_space.enabled = get(82) >= 0.5f;
    p.sound_space.pitch_bound_cents = clamp(get(83), 0.0f, 1.0f) * 100.0f;
    p.sound_space.harmonic_bound_cents = clamp(get(84), 0.0f, 1.0f) * 100.0f;
    p.sound_space.modal_bound_cents = clamp(get(85), 0.0f, 1.0f) * 100.0f;
    p.sound_space.movement_hz = unit_log(get(86), 0.02f, 20.0f);
    p.sound_space.attraction = clamp(get(87), 0.0f, 1.0f);
    p.sound_space.resistance = clamp(get(88), 0.0f, 1.0f);
    p.sound_space.repulsion = clamp(get(89), 0.0f, 1.0f);
    p.sound_space.frequency_freedom = clamp(get(90), 0.0f, 1.0f);
    p.sound_space.energy_widen = (clamp(get(91), 0.0f, 1.0f) - 0.5f) * 2.0f;
    p.sound_space.attack_freedom = clamp(get(92), 0.0f, 1.0f);
    p.sound_space.release_relaxation = clamp(get(93), 0.0f, 1.0f);
    p.sound_space.pitch_mix = clamp(get(94), 0.0f, 1.0f);
    p.sound_space.harmonic_mix = clamp(get(95), 0.0f, 1.0f);
    p.sound_space.modal_mix = clamp(get(96), 0.0f, 1.0f);
    p.sound_space.spectral_depth_db = clamp(get(97), 0.0f, 1.0f) * 18.0f;
    p.sound_space.spectral_priority = clamp(get(98), 0.0f, 1.0f);
    p.sound_space.phase_offset_cycles = (clamp(get(99), 0.0f, 1.0f) - 0.5f) * 0.5f;
    p.sound_space.phase_coupling = clamp(get(100), 0.0f, 1.0f);
    for (int b = 0; b < 4; ++b) {
        const int id = 101 + b * 3;
        p.sound_space.routes[b].source = static_cast<int>(clamp(get(id), 0.0f, 1.0f) * 7.99f);
        p.sound_space.routes[b].destination = static_cast<int>(clamp(get(id + 1), 0.0f, 1.0f) * 7.99f);
        p.sound_space.routes[b].depth = (clamp(get(id + 2), 0.0f, 1.0f) - 0.5f) * 2.0f;
    }
    p.vocal_expression.enabled = get(113) >= 0.5f;
    p.vocal_expression.correction_strength = clamp(get(114), 0.0f, 1.0f);
    p.vocal_expression.drift_retention = clamp(get(115), 0.0f, 1.0f);
    p.vocal_expression.vibrato_retention = clamp(get(116), 0.0f, 1.0f);
    p.vocal_expression.transition = clamp(get(117), 0.0f, 1.0f);
    p.vocal_expression.formant_repair = clamp(get(118), 0.0f, 1.0f);
    p.vocal_expression.spectral_residual_mix = clamp(get(119), 0.0f, 1.0f);
    p.vocal_expression.character = clamp(get(120), 0.0f, 1.0f);
    p.vocal_expression.mix = clamp(get(121), 0.0f, 1.0f);
    p.vocal_expression.sequential_stage_mix = clamp(get(122), 0.0f, 1.0f);
    p.vocal_expression.aperiodic_protection = clamp(get(123), 0.0f, 1.0f);
    p.tube_rack.enabled = get(124) >= 0.5f;
    p.tube_rack.parallel = get(125) >= 0.5f;
    p.tube_rack.shared_supply = clamp(get(126), 0.0f, 1.0f);
    p.tube_rack.output = clamp(get(127), 0.0f, 1.0f);
    p.tube_rack.antialias = get(128) >= 0.5f;
    for (int b = 0; b < 4; ++b) {
        const int id = 129 + 8 * b;
        auto& t = p.tube_rack.slots[b];
        t.enabled = get(id) >= 0.5f;
        t.flavor = std::min(kTubeFlavorCount - 1, static_cast<int>(clamp(get(id + 1), 0.0f, 1.0f) * kTubeFlavorCount));
        t.drive = clamp(get(id + 2), 0.0f, 1.0f);
        t.bias = clamp(get(id + 3), 0.0f, 1.0f) * 2.0f - 1.0f;
        t.plate_load = clamp(get(id + 4), 0.0f, 1.0f);
        t.sag = clamp(get(id + 5), 0.0f, 1.0f);
        t.coupling = clamp(get(id + 6), 0.0f, 1.0f);
        t.mix = clamp(get(id + 7), 0.0f, 1.0f);
    }
    const auto env_s = [](float v) { const float c = clamp(v, 0.0f, 1.0f); return 0.001f + c * c * 6.0f; };
    p.amp_attack = env_s(get(161)); p.amp_decay = env_s(get(162)); p.amp_sustain = clamp(get(163), 0.0f, 1.0f); p.amp_release = env_s(get(164));
    p.filter_attack = env_s(get(165)); p.filter_decay = env_s(get(166)); p.filter_sustain = clamp(get(167), 0.0f, 1.0f); p.filter_release = env_s(get(168));
    p.lfo_tempo_sync = get(169) >= 0.5f;
    p.tempo_bpm = 40.0f + clamp(get(170), 0.0f, 1.0f) * 200.0f;
    p.lfo_subdivision = std::min(11, static_cast<int>(clamp(get(171), 0.0f, 1.0f) * 12.0f));
    p.lfo_follow_host_tempo = get(174) >= 0.5f;
    p.lfo_groove = clamp(get(175), 0.0f, 1.0f);
    p.performance_drift = clamp(get(176), 0.0f, 1.0f);
    p.performance_fracture = clamp(get(177), 0.0f, 1.0f);
    p.wave_a_family = std::min(5, static_cast<int>(clamp(get(178), 0.0f, 1.0f) * 6.0f));
    p.wave_b = std::min(8, static_cast<int>(clamp(get(179), 0.0f, 1.0f) * 9.0f));
    p.wave_morph = clamp(get(180), 0.0f, 1.0f);
    p.phase_warp = clamp(get(181), 0.0f, 1.0f);
    p.wave_fold = clamp(get(182), 0.0f, 1.0f);
    p.osc_unison = 1 + std::min(4, static_cast<int>(clamp(get(183), 0.0f, 1.0f) * 5.0f));
    p.unison_detune_cents = clamp(get(184), 0.0f, 1.0f) * 50.0f;
    p.sub_harmonics = clamp(get(185), 0.0f, 1.0f);
    p.env_to_morph = clamp(get(186), 0.0f, 1.0f) * 2.0f - 1.0f;
    p.lfo_to_morph = clamp(get(187), 0.0f, 1.0f);
    p.gen_shape = clamp(get(188), 0.0f, 1.0f);
    p.gen_odd_even = clamp(get(189), 0.0f, 1.0f);
    p.gen_tilt = clamp(get(190), 0.0f, 1.0f);
    p.gen_formant = clamp(get(191), 0.0f, 1.0f);
    p.onion_enabled = get(192) >= 0.5f;
    p.onion_intent = std::min(2, static_cast<int>(clamp(get(193), 0.0f, 1.0f) * 3.0f));
    p.onion_amount = clamp(get(194), 0.0f, 1.0f);
    p.onion_sensitivity = clamp(get(195), 0.0f, 1.0f);
    p.onion_low_protect = clamp(get(196), 0.0f, 1.0f);
    p.onion_transient_protect = clamp(get(197), 0.0f, 1.0f);
    p.eco_mode = std::min(3, static_cast<int>(clamp(get(198), 0.0f, 1.0f) * 4.0f));
    p.eco_slot = std::min(7, static_cast<int>(clamp(get(199), 0.0f, 1.0f) * 8.0f));
    p.eco_peer_weight = clamp(get(200), 0.0f, 1.0f);
    p.eco_pitch_follow = clamp(get(201), 0.0f, 1.0f);
    p.osc2_wave = std::min(9, static_cast<int>(clamp(get(202), 0.0f, 1.0f) * 10.0f));
    p.osc2_fine_cents = (clamp(get(203), 0.0f, 1.0f) - 0.5f) * 100.0f;
    p.ring_mod_mix = clamp(get(204), 0.0f, 1.0f);
    p.unison_width = clamp(get(205), 0.0f, 1.0f);
    p.analog_drift = clamp(get(206), 0.0f, 1.0f);
    for (int h = 0; h < 8; ++h) p.gen_harmonics[static_cast<size_t>(h)] = clamp(get(207 + h), 0.0f, 1.0f);
    p.auto_morph_rate = clamp(get(215), 0.0f, 1.0f) * 8.0f;
    p.morph_curve = std::min(3, static_cast<int>(clamp(get(216), 0.0f, 1.0f) * 4.0f));
    p.osc_balance = clamp(get(217), 0.0f, 1.0f);
    p.onion_tilt_db = (clamp(get(218), 0.0f, 1.0f) - 0.5f) * 12.0f;
    p.onion_q_focus = 0.85f + clamp(get(219), 0.0f, 1.0f) * (4.5f - 0.85f);
    p.onion_air_db = (clamp(get(220), 0.0f, 1.0f) - 0.5f) * 12.0f;
    for (int b = 0; b < 24; ++b) p.onion_band_db[static_cast<size_t>(b)] = (clamp(get(221 + b), 0.0f, 1.0f) - 0.5f) * 24.0f;

    const int root_idx = std::clamp(static_cast<int>(std::lround(clamp(get(245), 0.0f, 1.0f) * 11.0f)), 0, 11);
    const int tuning_idx = std::clamp(static_cast<int>(std::lround(clamp(get(246), 0.0f, 1.0f) * 29.0f)), 0, 29);
    p.vocal_expression.target.tuning.set_builtin(static_cast<VocalBuiltinTuning>(tuning_idx));
    p.vocal_expression.target.tuning.root_cents = static_cast<float>((root_idx + 3) % 12) * 100.0f;
    p.vocal_expression.target.articulation = clamp(get(247), 0.0f, 1.0f) * 2.0f - 1.0f;
    p.vocal_expression.target.portamento = clamp(get(248), 0.0f, 1.0f);
    p.vocal_expression.target.target_hysteresis = clamp(get(249), 0.0f, 1.0f);
    p.vocal_expression.target.onset_protection = clamp(get(250), 0.0f, 1.0f);
    p.onion_harmonic_protect = clamp(get(251), 0.0f, 1.0f);
    p.pattern_enabled = get(252) >= 0.5f;
    p.pattern_index = std::clamp(static_cast<int>(std::lround(clamp(get(253), 0.0f, 1.0f) * 7.0f)), 0, 7);
    p.pattern_steps = 1 + std::clamp(static_cast<int>(std::lround(clamp(get(254), 0.0f, 1.0f) * 31.0f)), 0, 31);
    p.pattern_division = std::clamp(static_cast<int>(std::lround(clamp(get(255), 0.0f, 1.0f) * 5.0f)), 0, 5);
    p.pattern_quantize = clamp(get(256), 0.0f, 1.0f);
    p.pattern_gate = 0.10f + clamp(get(257), 0.0f, 1.0f) * 0.90f;
    p.pattern_transpose = std::clamp(static_cast<int>(std::lround((clamp(get(258), 0.0f, 1.0f) - 0.5f) * 24.0f)), -12, 12);
    return p;
}

// Selector 79 remains its original four-entry mapping. Selector 173 adds new
// palettes; both resolve to the same factory constructor as the audio engine.
inline bool factory_patch_for_selector(int id,float value,PresetData& out){
    const int patch=id==79
        ? std::clamp(static_cast<int>(value*3.99f),0,3)
        : std::clamp(static_cast<int>(value*8.f),0,7);
    switch(patch){
    case 1:out=PresetManager::create_monolith();return true;
    case 2:out=PresetManager::create_feral_wobble();return true;
    case 3:out=PresetManager::create_velvet_lead();return true;
    case 4:if(id==173){out=PresetManager::create_tube_foundry();return true;}return false;
    case 5:if(id==173){out=PresetManager::create_clockwork_sub();return true;}return false;
    case 6:if(id==173){out=PresetManager::create_glass_chamber();return true;}return false;
    case 7:if(id==173){out=PresetManager::create_sound_space();return true;}return false;
    default:return false;
    }
}
} // namespace monkeys_ear::ui
