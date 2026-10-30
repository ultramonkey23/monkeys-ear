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
