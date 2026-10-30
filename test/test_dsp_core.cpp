#include "monkeys_ear/engine.h"
#include "../vst3/preset_param_snapshot.h"
#include "monkeys_ear/lfo.h"
#include "monkeys_ear/chrono_state.h"
#include "monkeys_ear/vocal_module.h"
#include "wav_writer.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <fstream>
#include <limits>
#include <string>

using namespace monkeys_ear;

static float rms_of(const std::vector<float>& x, size_t begin = 0) {
    double e=0.0; for(size_t i=begin;i<x.size();++i)e+=static_cast<double>(x[i])*x[i];
    return x.size()>begin?static_cast<float>(std::sqrt(e/static_cast<double>(x.size()-begin))):0.0f;
}

static float estimate_frequency(const std::vector<float>& x, float sr, size_t begin) {
    int crossings=0; size_t first=0,last=0;
    for(size_t i=std::max<size_t>(begin,1);i<x.size();++i) if(x[i-1]<=0&&x[i]>0){if(crossings==0)first=i;last=i;++crossings;}
    return crossings>1 ? sr*static_cast<float>(crossings-1)/static_cast<float>(last-first) : 0.0f;
}

void test_standalone_vocal_module() {
    std::cout << "[TEST] Standalone Vocal Module Local Tracking / Bypass... ";
    constexpr size_t N=64; constexpr float SR=48000.0f;
    VocalModule vocal; vocal.prepare(SR,N);
    VocalExpressionControls controls=vocal.controls();
    controls.enabled=true; controls.correction_strength=.90f; controls.drift_retention=.05f;
    controls.vibrato_retention=.15f; controls.mix=1.0f; vocal.set_controls(controls);
    std::array<float,N> in{},out_l{},out_r{}; double changed=0.0;
    for(size_t block=0;block<180;++block){
        for(size_t i=0;i<N;++i){float t=float(block*N+i)/SR;in[i]=.35f*std::sin(TWO_PI*433.0f*t);}
        vocal.process_block(in.data(),in.data(),out_l.data(),out_r.data(),N);
        for(size_t i=0;i<N;++i){assert(std::isfinite(out_l[i])&&std::isfinite(out_r[i]));if(block>120){float d=out_l[i]-in[i];changed+=d*d;}}
    }
    assert(vocal.analysis().pitch_confidence>0.35f);
    assert(changed>1e-5);
    vocal.set_bypass(true); vocal.process_block(in.data(),in.data(),out_l.data(),out_r.data(),N);
    for(size_t i=0;i<N;++i){assert(out_l[i]==in[i]&&out_r[i]==in[i]);}
    std::cout << "PASS (local confidence=" << vocal.analysis().pitch_confidence << ")\n";
}

// ── Test 1: PolyBLEP Oscillator & Cross-FM / Sync ────────────────────────
void test_polyblep_oscillator() {
    std::cout << "[TEST] PolyBLEP Oscillator... ";
    PolyBLEPOscillator osc;
    osc.set_sample_rate(48000.0f);
    osc.set_frequency(440.0f);
    osc.set_waveform(Waveform::Saw);

    for (int i = 0; i < 48000; ++i) {
        float sample = osc.process();
        assert(!std::isnan(sample));
        assert(!std::isinf(sample));
        assert(sample >= -1.2f && sample <= 1.2f);
    }
    std::cout << "PASS\n";
}

void test_osc_cross_fm_and_sync() {
    std::cout << "[TEST] Oscillator Cross-FM & Hard Sync Depth... ";
    SynthVoice voice;
    voice.set_sample_rate(48000.0f);
    voice.set_waveform(Waveform::Saw);
    voice.note_on(60, 0.9f);

    // Render baseline (no FM)
    voice.set_fm_amount(0.0f);
    float env1 = 0.0f;
    std::vector<float> base_buf(1024);
    for (size_t i = 0; i < 1024; ++i) base_buf[i] = voice.process(env1);

    // Render with Cross-FM
    voice.note_on(60, 0.9f);
    voice.set_osc2_semi(7); // Fifth
    voice.set_fm_amount(0.65f);
    float env2 = 0.0f;
    std::vector<float> fm_buf(1024);
    double diff_sq = 0.0;
    for (size_t i = 0; i < 1024; ++i) {
        fm_buf[i] = voice.process(env2);
        float d = fm_buf[i] - base_buf[i];
        diff_sq += d * d;
    }
    assert(diff_sq > 1.0); // Measurable timbral expansion!

    // Verify hard sync reset
    voice.set_hard_sync(true);
    voice.note_on(60, 0.9f);
    float env3 = 0.0f;
    for (size_t i = 0; i < 512; ++i) {
        float s = voice.process(env3);
        assert(!std::isnan(s) && !std::isinf(s));
    }

    std::cout << "PASS (FM harmonic deviation = " << std::sqrt(diff_sq / 1024.0) << ")\n";
}

// ── Test 2: Cody's Chrono-Stateful Body Mathematics ──────────────────────
void test_chrono_state_mathematics() {
    std::cout << "[TEST] Chrono-Stateful Body Mathematics (4 Operators)... ";
    ChronoStateBody body;
    body.set_sample_rate(48000.0f);
    body.set_frequency(220.0f);
    body.set_resistance(0.40f);
    body.set_repulsion(0.50f);
    body.set_coupling(0.35f);
    body.set_enabled(true);

    // 1. Verify Negative Gravity soft-core kernel bounding
    // As clearance d -> 0, K(d; ell, p) approaches 1.0, generating maximum repulsion
    float k_far = ChronoStateBody::negative_gravity_kernel(1.0f, 0.35f, 2.0f);
    float k_near = ChronoStateBody::negative_gravity_kernel(0.01f, 0.35f, 2.0f);
    assert(k_near > k_far); // Repulsion strengthens near boundary
    assert(k_near <= 1.0f); // Bounded, no singularity!

    // 2. Massive overdrive impulse: verify state variables remain strictly bounded
    for (int i = 0; i < 1000; ++i) {
        float extreme_input = (i == 0) ? 20.0f : 0.0f;
        float out = body.process_sample(extreme_input);
        assert(!std::isnan(out) && !std::isinf(out));
        assert(std::abs(out) <= 2.0f);
    }
    const auto& s = body.get_state();
    assert(std::abs(s.displacement) <= 1.5f);
    assert(s.energy_fast >= 0.0f && s.energy_slow >= 0.0f);

    // 3. History dependence: verify macro energy builds up under sustained excitation
    body.reset();
    for (int i = 0; i < 100; ++i) {
        body.process_sample(std::sin(static_cast<float>(i) * 0.1f));
    }
    float e_early = body.get_state().energy_slow;

    for (int i = 0; i < 5000; ++i) {
        body.process_sample(std::sin(static_cast<float>(i) * 0.1f));
    }
    float e_sustained = body.get_state().energy_slow;
    assert(e_sustained > e_early); // History-dependent accumulated energy verified!

    // 4. Reset determinism
    body.reset();
    assert(body.get_state().displacement == 0.0f);
    assert(body.get_state().velocity == 0.0f);
    assert(body.get_state().energy_slow == 0.0f);

    std::cout << "PASS\n";
}

// ── Test 3: Audio Input Processor Causality & Live Audio Repair ──────────
void test_external_audio_processor_causality() {
    std::cout << "[TEST] External Audio Processor Causality & Routing... ";
    constexpr float SR = 48000.0f;
    constexpr size_t N = 512;

    MonkeysEarEngine engine;
    engine.init(SR, N);

    // Load Vocal/Mic Preset (which routes external audio)
    engine.load_preset(PresetManager::create_factory_vocal_resonator());

    std::vector<float> in_l(N), in_r(N), out_l(N), out_r(N);
    // 440 Hz test tone representing microphone input
    for (size_t i = 0; i < N; ++i) {
        float sig = 0.5f * std::sin(TWO_PI * 440.0f * (static_cast<float>(i) / SR));
        in_l[i] = sig;
        in_r[i] = sig;
    }

    // 1. Process block with external audio
    engine.process_block(in_l.data(), in_r.data(), out_l.data(), out_r.data(), N);

    // Verify external audio actually reached output!
    float peak_out = 0.0f;
    for (size_t i = 0; i < N; ++i) {
        float p = std::max(std::abs(out_l[i]), std::abs(out_r[i]));
        if (p > peak_out) peak_out = p;
    }
    assert(peak_out > 0.05f); // External audio is NOT silent!

    // 2. Change Filter Cutoff: verify causality on external audio
    engine.set_macro(MACRO_CUTOFF, 0.10f); // Closed dark
    std::vector<float> dark_out(N);
    engine.process_block(in_l.data(), in_r.data(), dark_out.data(), out_r.data(), N);

    engine.set_macro(MACRO_CUTOFF, 0.95f); // Open bright
    std::vector<float> bright_out(N);
    engine.process_block(in_l.data(), in_r.data(), bright_out.data(), out_r.data(), N);

    double cutoff_diff = 0.0;
    for (size_t i = 0; i < N; ++i) {
        float d = bright_out[i] - dark_out[i];
        cutoff_diff += d * d;
    }
    assert(cutoff_diff > 0.01); // Filter control causally shapes external audio!

    // 3. Change Tube Drive: verify causality on external audio
    engine.set_macro(MACRO_DRIVE, 0.0f);
    std::vector<float> clean_out(N);
    engine.process_block(in_l.data(), in_r.data(), clean_out.data(), out_r.data(), N);

    engine.set_macro(MACRO_DRIVE, 0.95f);
    std::vector<float> dirty_out(N);
    engine.process_block(in_l.data(), in_r.data(), dirty_out.data(), out_r.data(), N);

    double drive_diff = 0.0;
    for (size_t i = 0; i < N; ++i) {
        float d = dirty_out[i] - clean_out[i];
        drive_diff += d * d;
    }
    assert(drive_diff > 0.01); // Tube Drive causally shapes external audio!

    std::cout << "PASS (External Audio Reaches DSP & All Controls Causally Shape It)\n";
}

void test_dual_layer_vocal_expression() {
    std::cout << "[TEST] Causal Dual-Layer Vocal Expression... ";
    constexpr float SR=48000.0f;
    AudioInputProcessor vocal; vocal.set_sample_rate(SR); vocal.set_mix(1.0f);
    VocalExpressionControls c{}; c.enabled=true;c.correction_strength=.9f;c.drift_retention=.15f;c.vibrato_retention=.8f;c.transition=.75f;c.formant_repair=.65f;c.spectral_residual_mix=.9f;c.character=.2f;c.mix=1.0f;
    vocal.set_vocal_controls(c);
    float peak=0.0f;
    for(size_t i=0;i<48000;++i){float x=.45f*std::sin(TWO_PI*445.0f*static_cast<float>(i)/SR);float conditioned=vocal.process_sample(x,0.0f);float y=vocal.process_vocal_sample(conditioned,445.0f,0.96f);assert(std::isfinite(y));peak=std::max(peak,std::abs(y));}
    const auto voiced=vocal.vocal_metrics(); assert(voiced.voiced&&voiced.voiced_mix>.9f);assert(voiced.source_type==VocalSourceType::Periodic);assert(voiced.periodic_mix>.65f);assert(std::abs(voiced.correction_cents)>1.0f);assert(voiced.residual_mix<=.25f);assert(peak>.02f);
    float dry=.17f;float fallback=vocal.process_vocal_sample(dry,0.0f,0.0f);assert(std::abs(fallback-dry)<1e-6f);
    std::cout<<"PASS (correction="<<voiced.correction_cents<<"c, residual="<<voiced.residual_mix<<")\n";
}

void test_vocal_reset_clears_all_causal_histories() {
    std::cout << "[TEST] Vocal Reset Clears Both PSOLA Histories... ";
    constexpr float SR = 48000.0f;
    VocalExpressionControls c{};
    c.enabled = true;
    c.correction_strength = .82f;
    c.drift_retention = .2f;
    c.transition = .7f;
    c.formant_repair = .5f;
    c.mix = 1.0f;
    c.sequential_stage_mix = 1.0f;

    AudioInputProcessor restarted, fresh;
    for (auto* processor : {&restarted, &fresh}) {
        processor->set_sample_rate(SR);
        processor->set_mix(1.0f);
        processor->set_vocal_controls(c);
    }

    // Fill both causal PSOLA histories, then ensure reset produces exactly the
    // same restart as a fresh processor. This catches a stale second-stage
    // grain without adding latency, allocation, or a runtime branch.
    for (size_t i = 0; i < 4096; ++i) {
        const float x = .43f * std::sin(TWO_PI * 437.0f * static_cast<float>(i) / SR);
        const float conditioned = restarted.process_sample(x, 0.0f);
        restarted.process_vocal_sample(conditioned, 437.0f, .96f);
    }
    restarted.reset();

    float max_restart_delta = 0.0f;
    for (size_t i = 0; i < 512; ++i) {
        const float x = .31f * std::sin(TWO_PI * 445.0f * static_cast<float>(i) / SR);
        const float restarted_y = restarted.process_vocal_sample(
            restarted.process_sample(x, 0.0f), 445.0f, .96f);
        const float fresh_y = fresh.process_vocal_sample(
            fresh.process_sample(x, 0.0f), 445.0f, .96f);
        assert(std::isfinite(restarted_y) && std::isfinite(fresh_y));
        max_restart_delta = std::max(max_restart_delta, std::abs(restarted_y - fresh_y));
    }
    assert(max_restart_delta < 1e-7f);
    std::cout << "PASS (restart delta=" << max_restart_delta << ")\n";
}

void test_audio_input_extreme_finite_safety() {
    std::cout << "[TEST] Audio Input Extreme-Finite Safety... ";
    constexpr float SR = 48000.0f;
    AudioInputProcessor input;
    input.set_sample_rate(SR);
    input.set_gain(24.0f);
    input.set_mix(1.0f);
    VocalExpressionControls c{};
    c.enabled = true;
    c.correction_strength = .7f;
    c.mix = 1.0f;
    input.set_vocal_controls(c);
    for (size_t i = 0; i < 1024; ++i) {
        const float hostile = (i & 1) ? std::numeric_limits<float>::max()
                                       : -std::numeric_limits<float>::max();
        const float conditioned = input.process_sample(hostile, 0.0f);
        const float vocal = input.process_vocal_sample(conditioned, 440.0f, .96f);
        assert(std::isfinite(conditioned) && std::isfinite(vocal));
        assert(std::isfinite(input.read_ring_buffer(0)));
    }
    std::cout << "PASS (no non-finite history under max-finite input)\n";
}

void test_vocal_tuning_space_and_articulation() {
    std::cout << "[TEST] Vocal Tuning Space / Gravity / Articulation... ";
    constexpr float SR=48000.0f;
    assert(std::abs(TuningSpace::ratio_to_cents(3.0f,2.0f)-701.955f)<.02f);
    assert(std::abs(TuningSpace::cents_to_ratio(701.955f)-1.5f)<.001f);

    VocalTargetControls c{};c.tuning.clear();c.tuning.add_degree_cents(0.0f);c.tuning.add_degree_ratio(3.0f,2.0f,.85f);
    VocalTargetEngine target;target.set_sample_rate(SR);target.set_controls(c);target.reset();
    target.process(688.0f,0.0f,.95f,1.0f);assert(std::abs(target.metrics().selected_cents-701.955f)<.03f);

    // A strong anchor wins over a weak passing degree even when the latter is closer.
    c.tuning.clear();c.tuning.add_degree_cents(0.0f,1.0f);c.tuning.add_degree_cents(100.0f,.15f);c.articulation=.5f;c.transition_preservation=0;
    target.set_controls(c);target.reset();target.process(86.0f,0.0f,.95f,1.0f);assert(target.metrics().selected_degree==0);

    // Direction is part of target selection, not a post-target glide convention.
    c.tuning.clear();c.tuning.add_degree_cents(0.0f,.8f,.1f,1.0f);c.tuning.add_degree_cents(100.0f,.8f,1.0f,.1f);c.directionality=1.0f;
    target.set_controls(c);target.reset();target.process(50.0f,120.0f,.95f,1.0f);assert(target.metrics().selected_degree==1);
    target.reset();target.process(50.0f,-120.0f,.95f,1.0f);assert(target.metrics().selected_degree==0);

    // Hysteresis prevents vibrato around a degree boundary from target chatter.
    c.tuning.clear();c.tuning.add_degree_cents(0);c.tuning.add_degree_cents(100);c.articulation=-.7f;c.target_hysteresis=.9f;
    target.set_controls(c);target.reset();target.process(20.0f,0.0f,.95f,0.0f);for(int i=0;i<600;++i)target.process(50.0f+11.0f*std::sin(TWO_PI*i/47.0f),0.0f,.95f,0.0f);assert(target.metrics().switch_count==0);

    // Legato keeps center correction available but carries its target as a trajectory;
    // staccato onset deliberately reacquires the next target instead.
    c.tuning.clear();c.tuning.add_degree_cents(0);c.tuning.add_degree_cents(300);c.transition_preservation=.9f;c.portamento=.8f;c.articulation=-1.0f;c.target_hysteresis=.2f;
    target.set_controls(c);target.reset();target.process(0,0,.95f,1);target.process(300,140,.95f,0);assert(target.metrics().selected_cents>299&&target.metrics().trajectory_cents<1.0f);
    c.articulation=1.0f;target.set_controls(c);target.reset();target.process(0,0,.95f,1);for(int i=0;i<500;++i)target.process(300,140,.95f,1);assert(target.metrics().trajectory_cents>100.0f);

    c.tuning.set_builtin(VocalBuiltinTuning::BohlenPierce13);assert(c.tuning.period_cents>1800.0f&&c.tuning.degree_count==13);
    target.set_controls(c);target.reset();target.process(720.0f,0,.95f,1);assert(std::abs(target.metrics().selected_cents-731.521f)<.1f);
    for(int i=0;i<static_cast<int>(VocalBuiltinTuning::Count);++i){TuningSpace library;library.set_builtin(static_cast<VocalBuiltinTuning>(i));assert(library.degree_count>0&&library.period_cents>0.0f&&library.name()[0]);}

    // Custom cents/ratio spaces are data, and retain their targeting semantics on recall.
    PresetData custom;custom.vocal_expression.target=c;custom.vocal_expression.target.articulation=-.37f;custom.vocal_expression.target.tuning.clear();
    custom.vocal_expression.target.tuning.root_cents=17.0f;custom.vocal_expression.target.tuning.add_degree_cents(0,.95f);custom.vocal_expression.target.tuning.add_degree_ratio(7,6,.42f,.8f,.3f);
    PresetData restored;assert(restored.deserialize(custom.serialize()));const auto& recalled=restored.vocal_expression.target;
    assert(recalled.tuning.degree_count==2&&std::abs(recalled.tuning.root_cents-17.0f)<.01f&&std::abs(recalled.tuning.degrees[1].cents-TuningSpace::ratio_to_cents(7,6))<.02f&&std::abs(recalled.articulation+.37f)<.01f);
    std::cout << "PASS (JI, gravity, direction, hysteresis, articulation, non-octave)\n";
}

void test_time_varying_vocal_phrase() {
    std::cout << "[TEST] Time-Varying Vocal Phrase / Component Protection... ";
    constexpr float SR=48000.0f;
    VocalExpressionControls c{}; c.enabled=true;c.correction_strength=.72f;c.drift_retention=.38f;c.vibrato_retention=1.0f;c.transition=.84f;c.formant_repair=.58f;c.spectral_residual_mix=.12f;c.mix=1.0f;c.sequential_stage_mix=.62f;c.aperiodic_protection=.93f;
    AudioInputProcessor staged; staged.set_sample_rate(SR); staged.set_mix(1.0f); staged.set_vocal_controls(c);
    float last_cents=0.0f, max_cents_step=0.0f, peak=0.0f; bool saw_periodic=false,saw_mixed=false;
    for(size_t i=0;i<static_cast<size_t>(SR*3.0f);++i){
        float t=static_cast<float>(i)/SR;
        float hz=220.0f;
        float confidence=.96f;
        float x=0.0f;
        if(t<.55f){ hz=226.0f+5.0f*std::sin(TWO_PI*5.2f*t); x=.42f*std::sin(TWO_PI*hz*t); } // drift + vibrato
        else if(t<1.10f){ float u=(t-.55f)/.55f;hz=226.0f+150.0f*u;x=.42f*std::sin(TWO_PI*(226.0f*t+.5f*150.0f*u*t)); } // slide
        else if(t<1.65f){ hz=371.0f+12.0f*std::sin(TWO_PI*(4.0f+3.0f*(t-1.1f))*(t-1.1f));x=.38f*std::sin(TWO_PI*hz*t)+.14f*std::sin(TWO_PI*2.02f*hz*t); } // changing vibrato / harmonic ambiguity
        else if(t<2.20f){ hz=300.0f;confidence=.68f;x=.24f*std::sin(TWO_PI*hz*t)+.12f*std::sin(TWO_PI*3800.0f*t); } // mixed aspiration
        else { confidence=.0f;x=.13f*std::sin(TWO_PI*5100.0f*t); } // sibilant-like aperiodic release
        float conditioned=staged.process_sample(x,0.0f); float y=staged.process_vocal_sample(conditioned,hz,confidence); assert(std::isfinite(y));peak=std::max(peak,std::abs(y));
        const auto& m=staged.vocal_metrics(); max_cents_step=std::max(max_cents_step,std::abs(m.correction_cents-last_cents));last_cents=m.correction_cents;
        saw_periodic|=m.source_type==VocalSourceType::Periodic;saw_mixed|=m.source_type==VocalSourceType::Mixed;
        if(t>2.25f) assert(std::abs(y-conditioned)<1e-6f); // protected release remains direct.
    }
    assert(saw_periodic&&saw_mixed);assert(max_cents_step<1.5f);assert(peak>.02f);
    std::cout<<"PASS (max control step="<<max_cents_step<<"c/sample, peak="<<peak<<")\n";
}

// ── Test 4: Parameter Causality ──────────────────────────────────────────
void test_parameter_causality() {
    std::cout << "[TEST] Parameter Causality Verification (state + expanded automation)... ";
    MonkeysEarEngine engine;
    engine.init(48000.0f, 256);
    engine.handle_midi_note_on(60, 0.8f);

    std::vector<float> dummy_in(256, 0.0f), out1(256, 0.0f), out2(256, 0.0f), dummy_r(256, 0.0f);

    // Verify State Resistance causality
    engine.set_state_resistance(0.05f);
    engine.process_block(dummy_in.data(), dummy_in.data(), out1.data(), dummy_r.data(), 256);
    engine.set_state_resistance(0.95f);
    engine.process_block(dummy_in.data(), dummy_in.data(), out2.data(), dummy_r.data(), 256);
    double res_diff = 0.0;
    for (size_t i = 0; i < 256; ++i) res_diff += std::abs(out2[i] - out1[i]);
    assert(res_diff > 0.001);

    // Verify State Repulsion causality
    engine.set_state_repulsion(0.05f);
    engine.process_block(dummy_in.data(), dummy_in.data(), out1.data(), dummy_r.data(), 256);
    engine.set_state_repulsion(0.95f);
    engine.process_block(dummy_in.data(), dummy_in.data(), out2.data(), dummy_r.data(), 256);
    double rep_diff = 0.0;
    for (size_t i = 0; i < 256; ++i) rep_diff += std::abs(out2[i] - out1[i]);
    assert(rep_diff > 0.001);

    // Verify State Coupling causality
    engine.set_state_coupling(0.05f);
    engine.process_block(dummy_in.data(), dummy_in.data(), out1.data(), dummy_r.data(), 256);
    engine.set_state_coupling(0.95f);
    engine.process_block(dummy_in.data(), dummy_in.data(), out2.data(), dummy_r.data(), 256);
    double cpl_diff = 0.0;
    for (size_t i = 0; i < 256; ++i) cpl_diff += std::abs(out2[i] - out1[i]);
    assert(cpl_diff > 0.001);

    engine.set_parameter_normalized(20, 0.91f);
    engine.set_parameter_normalized(29, 0.42f);
    engine.set_parameter_normalized(45, 0.73f);
    engine.set_parameter_normalized(60, 0.88f);
    engine.set_parameter_normalized(122, 0.66f);
    engine.set_parameter_normalized(123, 0.91f);
    const auto& automated=engine.get_current_preset();
    assert(std::abs(automated.sub_mix-0.91f)<1e-5f);
    assert(automated.eq_frequency_hz[2]>20.0f);
    assert(automated.mod_lfo_fm>0.7f);
    assert(std::abs(automated.vocal_expression.sequential_stage_mix-.66f)<1e-5f);
    assert(std::abs(automated.vocal_expression.aperiodic_protection-.91f)<1e-5f);
    engine.set_parameter_normalized(79,0.67f);
    assert(engine.get_current_preset().name=="FERAL WOBBLE");

    std::cout << "PASS\n";
}

void test_weight_and_external_subharmonics() {
    std::cout << "[TEST] Pitch-Locked Weight Path & Causal External Tracking... ";
    constexpr float SR=48000.0f; SynthVoice voice; voice.set_sample_rate(SR); voice.set_waveform(Waveform::Sine);
    voice.set_fundamental_mix(0.0f); voice.set_sub_mix(1.0f); voice.set_sub_ratio(3); voice.set_sub_envelope(0.0f); voice.note_on(69,1.0f);
    std::vector<float> sub(48000); for(size_t i=0;i<sub.size();++i){float env,w;voice.process_split(env,w);sub[i]=w;}
    float f=estimate_frequency(sub,SR,4000); assert(std::abs(f-(440.0f/3.0f))<0.8f);
    ExternalSubharmonic tracker; tracker.set_sample_rate(SR); tracker.set_ratio(2); std::vector<float> divided(48000);
    for(size_t i=0;i<divided.size();++i) divided[i]=tracker.process(0.6f*std::sin(TWO_PI*110.0f*static_cast<float>(i)/SR));
    assert(std::abs(tracker.tracked_frequency_hz()-110.0f)<1.0f); assert(tracker.confidence()>0.75f);
    float sf=estimate_frequency(divided,SR,12000); assert(std::abs(sf-55.0f)<0.8f);
    std::cout << "PASS (MIDI 1/3="<<f<<" Hz, tracked="<<tracker.tracked_frequency_hz()<<" Hz, divided="<<sf<<" Hz)\n";
}

static float filter_tone_rms(MultiPassFilter& f,float hz,float sr){std::vector<float>x(24000);for(size_t i=0;i<x.size();++i)x[i]=f.process(std::sin(TWO_PI*hz*static_cast<float>(i)/sr));return rms_of(x,4000);}
void test_multipass_filter_and_eq() {
    std::cout << "[TEST] Multipass Filter Responses & Parametric EQ... "; constexpr float SR=48000.0f;
    MultiPassFilter lp; lp.set_sample_rate(SR); lp.set_stage(0,FilterMode::Lowpass,300,0.1f,0,false); lp.set_stage(1,FilterMode::Lowpass,18000,0,0,false); lp.set_routing(FilterRouting::Serial);
    float low=filter_tone_rms(lp,100,SR); lp.reset(); float high=filter_tone_rms(lp,5000,SR); assert(low>high*5.0f);
    MultiPassFilter hp; hp.set_sample_rate(SR); hp.set_stage(0,FilterMode::Highpass,300,0.1f,0,false); hp.set_stage(1,FilterMode::Lowpass,18000,0,0,false); float hp_low=filter_tone_rms(hp,100,SR); hp.reset(); float hp_high=filter_tone_rms(hp,5000,SR); assert(hp_high>hp_low*3.0f);
    ParametricEQ flat,boost; flat.set_sample_rate(SR); boost.set_sample_rate(SR); boost.set_band(1,{EQType::Bell,1000,12,2.0f}); boost.set_gain_compensation(false);
    std::vector<float>a(48000),b(48000);for(size_t i=0;i<a.size();++i){float s=.1f*std::sin(TWO_PI*1000*static_cast<float>(i)/SR);a[i]=flat.process(s);b[i]=boost.process(s);} assert(rms_of(b,12000)>rms_of(a,12000)*2.4f);
    std::cout << "PASS (LP rejection="<<20*std::log10(low/high)<<" dB, bell boost="<<20*std::log10(rms_of(b,12000)/rms_of(a,12000))<<" dB)\n";
}

// Real instrument performance, not a standalone demo: sustain controls
// existing voice envelopes and the two patch selectors retain their meaning.
void test_host_clock_and_groove_in_product() {
    std::cout<<"[TEST] Native Beat Clock, Swing, Fallback and Whole-Patch Recall... ";
    // Groove must change temporal feel rather than only turning up a depth knob.
    LFO straight,swung;
    for(auto* l:{&straight,&swung}) {
        l->set_sample_rate(1000.0f);
        l->set_rate_hz(1.0f);
        l->set_depth(1.0f);
        l->set_waveform(LFOWaveform::Square);
        l->reset_phase();
    }
    swung.set_swing(1.0f);
    float mid_straight=0.0f,mid_swung=0.0f;
    for(int sample=0;sample<1000;++sample) {
        const float a=straight.process(),b=swung.process();
        if(sample==600){mid_straight=a;mid_swung=b;}
    }
    assert(mid_straight<-.9f&&mid_swung>.9f); // 50/50 vs 75/25 groove
    bool wrapped_straight=false,wrapped_swung=false;
    for(int tail=0;tail<4;++tail) {
        wrapped_straight|=straight.process()>.9f;
        wrapped_swung|=swung.process()>.9f;
    }
    assert(wrapped_straight&&wrapped_swung); // same nominal 1Hz period

    auto patch=PresetManager::create_clockwork_sub();
    assert(patch.lfo_tempo_sync&&patch.lfo_follow_host_tempo);
    assert(patch.lfo_groove>.5f);
    PresetData restored;
    assert(restored.deserialize(patch.serialize()));
    assert(restored.lfo_follow_host_tempo&&std::abs(restored.lfo_groove-patch.lfo_groove)<1.e-5f);
    patch.lfo1_waveform=0;patch.lfo1_depth=1.0f;patch.lfo_groove=0.0f;
    patch.lfo_subdivision=4; // quarter-note cycle
    patch.tempo_bpm=120.0f;

    MonkeysEarEngine manual,follow;
    manual.init(48000,32);follow.init(48000,32);
    auto fixed=patch; fixed.lfo_follow_host_tempo=false;
    manual.load_preset(fixed);follow.load_preset(patch);
    std::array<float,32> zeros{},outL{},outR{};
    manual.set_host_transport(90.0,true,0.0,true,false);
    follow.set_host_transport(90.0,true,0.0,true,false);
    for(int block=0;block<375;++block) {
        manual.process_block(zeros.data(),zeros.data(),outL.data(),outR.data(),32);
        follow.process_block(zeros.data(),zeros.data(),outL.data(),outR.data(),32);
    }
    // 120 BPM -> 1/2 cycle in 12k samples: sine is 0;
    // 90 BPM -> 3/8 cycle in 12k samples: sine is ~0.707.
    assert(std::abs(manual.get_lfo_value()-follow.get_lfo_value())>.5f);

    manual.reset();follow.reset();
    follow.set_host_transport(0.0,false,0.0,false,false);
    for(int block=0;block<375;++block) {
        manual.process_block(zeros.data(),zeros.data(),outL.data(),outR.data(),32);
        follow.process_block(zeros.data(),zeros.data(),outL.data(),outR.data(),32);
    }
    assert(std::abs(manual.get_lfo_value()-follow.get_lfo_value())<.005f);

    // DAW timeline alignment is explicit, not just matching LFO frequency.
    follow.set_host_transport(120.0,true,0.25,true,true);
    follow.process_block(zeros.data(),zeros.data(),outL.data(),outR.data(),1);
    assert(follow.get_lfo_value()>.99f); // sine quarter-phase
    follow.set_parameter_normalized(175,1.0f);
    follow.set_host_transport(120.0,true,0.375,true,true);
    follow.process_block(zeros.data(),zeros.data(),outL.data(),outR.data(),1);
    assert(follow.get_lfo_value()>.99f); // 75/25 groove shifts the sine peak to beat 0.375
    std::cout<<"PASS\n";
}

void test_gesture_field_and_mono_held_key_priority() {
    std::cout<<"[TEST] Gestural performance field, bounded DSP and mono key return... ";
    const float NaN=std::numeric_limits<float>::quiet_NaN();
    for(int i=0;i<32768;++i){
        const float lfo=std::sin(.004f*i);
        const float energy=(i%1000)/1000.f;
        const auto neutral=evaluate_performance_field(0,0,lfo,energy,.7f,-.3f,.8f,.25f);
        assert(neutral.filter_offset==0.f && neutral.fm_offset==0.f &&
               neutral.drive_offset==0.f && neutral.space_lfo==0.f &&
               neutral.sub_offset==0.f);
        const auto active=evaluate_performance_field(.75f,.65f,lfo,energy,.7f,-.3f,.8f,.25f);
        assert(std::abs(active.filter_offset)<.56f);
        assert(active.fm_offset>=0.f && active.fm_offset<=.4f);
        assert(active.drive_offset>=0.f && active.drive_offset<=.42f);
        assert(std::abs(active.space_lfo)<=.42f);
    }
    assert(evaluate_performance_field(NaN,NaN,NaN,NaN,NaN,NaN,NaN,NaN).fm_offset==0.f);
    auto a=evaluate_performance_field(0,1,0,0,0,0,.5f,0);
    auto b=evaluate_performance_field(1,1,0,0,0,0,.5f,0);
    assert(b.fm_offset>a.fm_offset && b.drive_offset>a.drive_offset);

    VoiceManager vm;vm.set_sample_rate(48000.f);
    vm.set_amp_envelope(.001f,.005f,1.f,.010f);
    vm.set_filter_envelope(.001f,.005f,1.f,.010f);
    vm.set_voice_mode(true,true);
    auto render=[&](int n){for(int i=0;i<n;++i){float f=0,w=0;vm.process_split(f,w);}};
    vm.note_on(60,.8f,0,100);render(100);
    vm.note_on(64,.8f,0,101);vm.note_on(67,.8f,0,102);
    assert(vm.current_mono_note()==67);
    vm.note_off(64,0,101);assert(vm.current_mono_note()==67);
    vm.note_off(67,0,102);assert(vm.current_mono_note()==60);
    vm.note_off(60,0,100);render(4800);
    assert(vm.active_voice_count()==0);
    vm.note_on(60,.8f,0,-1);vm.note_on(64,.8f,0,-1);
    vm.set_sustain_pedal(true);
    vm.note_off(64,0,-1);assert(vm.current_mono_note()==60);
    vm.note_off(60,0,-1);render(1000);
    assert(vm.active_voice_count()==1);
    vm.set_sustain_pedal(false);render(4800);
    assert(vm.active_voice_count()==0);
    vm.set_voice_mode(false,false);
    assert(vm.current_mono_note()==-1);

    auto p=PresetManager::create_factory_lead();
    assert(p.performance_drift==0.f && p.performance_fracture==0.f);
    auto newPatch=PresetManager::create_clockwork_sub();
    assert(newPatch.performance_drift>.5f&&newPatch.performance_fracture>.5f);
    PresetData roundtrip;
    assert(roundtrip.deserialize(newPatch.serialize()));
    assert(std::abs(roundtrip.performance_drift-newPatch.performance_drift)<1e-5f);
    assert(std::abs(roundtrip.performance_fracture-newPatch.performance_fracture)<1e-5f);

    MonkeysEarEngine reference,neutral,expressive;
    reference.init(48000.f,128);neutral.init(48000.f,128);expressive.init(48000.f,128);
    reference.load_preset(p);neutral.load_preset(p);expressive.load_preset(p);
    neutral.set_parameter_normalized(176,0.f);neutral.set_parameter_normalized(177,0.f);
    expressive.set_parameter_normalized(176,.86f);
    expressive.set_parameter_normalized(177,.73f);
    reference.handle_midi_note_on(55,.8f);
    neutral.handle_midi_note_on(55,.8f);
    expressive.handle_midi_note_on(55,.8f);
    std::array<float,128> zeros{},refL{},refR{},neuL{},neuR{},expL{},expR{};
    double energy=0,difference=0,unchanged=0;
    for(int block=0;block<60;++block){
        reference.process_block(zeros.data(),zeros.data(),refL.data(),refR.data(),128);
        neutral.process_block(zeros.data(),zeros.data(),neuL.data(),neuR.data(),128);
        expressive.process_block(zeros.data(),zeros.data(),expL.data(),expR.data(),128);
        for(int i=0;i<128;++i){
            assert(std::isfinite(refL[i])&&std::isfinite(expL[i]));
            energy+=refL[i]*refL[i];
            difference+=(expL[i]-refL[i])*(expL[i]-refL[i]);
            unchanged+=std::abs(refL[i]-neuL[i]);
        }
    }
    assert(energy>1.e-8 && difference>1.e-7);
    assert(unchanged<1.e-8); // bypassed gestures are the exact old synth
    std::cout<<"PASS\n";
}

void test_arturia_class_midi_keyboard_notes() {
    std::cout<<"[TEST] Physical MIDI Keyboard Semantics: channels, repeated notes, note IDs, pressure... ";
    VoiceManager voices;
    voices.set_sample_rate(48000.0f);
    voices.set_amp_envelope(.001f,.001f,1.f,.004f);
    voices.set_filter_envelope(.001f,.001f,1.f,.004f);
    auto render=[&](int samples){
        for(int i=0;i<samples;++i){
            float filt=0.0f,weight=0.0f;
            const float output=voices.process_split(filt,weight);
            assert(std::isfinite(output));
        }
    };
    voices.note_on(60,.8f,0,10);
    voices.note_on(60,.8f,1,20);
    render(512);
    assert(voices.active_voice_count()==2);
    voices.note_off(60,0,10);render(1000);
    assert(voices.active_voice_count()==1); // same pitch, other MIDI channel survives
    voices.note_off(60,1,20);render(1000);
    assert(voices.active_voice_count()==0);

    voices.note_on(62,.8f,0,101);
    voices.note_on(62,.9f,0,102);
    render(512);
    assert(voices.active_voice_count()==2);
    voices.note_off(62,0,101);render(1000);
    assert(voices.active_voice_count()==1); // VST3 note ID matches physical press
    voices.note_off(62,0,102);render(1000);
    assert(voices.active_voice_count()==0);

    voices.note_on(64,.9f,0,-1);
    voices.note_on(64,.9f,0,-1);
    render(512);
    voices.note_off(64,0,-1);render(1000);
    assert(voices.active_voice_count()==1); // MIDI 1.0 has no note IDs
    voices.note_off(64,0,-1);render(1000);
    assert(voices.active_voice_count()==0);

    voices.note_on(67,.8f,0,301);render(480);
    voices.set_sustain_pedal(true);
    voices.note_off(67,0,301);render(2000);
    assert(voices.active_voice_count()==1);
    voices.set_sustain_pedal(false);render(1000);
    assert(voices.active_voice_count()==0);

    SynthVoice unpressed,pressed;
    unpressed.set_sample_rate(48000.f);pressed.set_sample_rate(48000.f);
    unpressed.note_on(69,.85f);pressed.note_on(69,.85f);
    pressed.set_poly_pressure(.95f);
    double difference=0.0;
    for(int i=0;i<4096;++i){
        float a=0,b=0,aw=0,bw=0;
        float x=unpressed.process_split(a,aw);
        float y=pressed.process_split(b,bw);
        assert(std::isfinite(x)&&std::isfinite(y));
        difference+=std::abs(x-y);
    }
    assert(difference>0.001); // per-note pressure has a real audio consequence
    voices.all_sounds_off();
    assert(voices.active_voice_count()==0);
    std::cout<<"PASS\\n";
}

void test_factory_projection_for_native_editor() {
    std::cout<<"[TEST] Shared Factory Snapshot / PLAY and EXPLORE State Consistency... ";
    constexpr int N=178;
    std::array<float,N> params{};
    std::array<bool,N> covered{};
    for(int patch=0;patch<8;++patch) {
        PresetData p{};
        if(patch==0)p=PresetManager::create_factory_lead();
        else {
            const float choice=(static_cast<float>(patch)+.5f)/8.f;
            assert(ui::factory_patch_for_selector(173,choice,p));
        }
        params.fill(.5f);covered.fill(false);
        ui::emit_preset_params(p,[&](int id,float v){
            assert(id>=0&&id<N);
            assert(!covered[id]); // no repeated/ambiguous projection
            assert(std::isfinite(v)&&v>=0.f&&v<=1.f);
            params[id]=v;covered[id]=true;
        });
        assert(covered[0]&&covered[7]&&covered[8]);
        assert(covered[124]&&covered[129]&&covered[154]&&covered[160]);
        assert(covered[161]&&covered[163]&&covered[167]&&covered[169]);
        assert(covered[170]&&covered[171]&&covered[174]&&covered[175]);
        assert(covered[176]&&covered[177]);
        assert(!covered[79]&&!covered[80]&&!covered[81]&&!covered[172]&&!covered[173]);
        assert(std::abs(params[0]-p.macros[0])<1e-6f);
        assert(std::abs(params[163]-p.amp_sustain)<1e-6f);
        assert(std::abs(params[175]-p.lfo_groove)<1e-6f);
        assert(std::abs(params[176]-p.performance_drift)<1e-6f);
        assert(std::abs(params[177]-p.performance_fracture)<1e-6f);
        for(int i=0;i<4;++i) {
            const int id=129+8*i;
            assert(std::abs(params[id+2]-p.tube_rack.slots[i].drive)<1e-5f);
        }
    }
    // Old selector has not been silently expanded to use the new patches.
    PresetData original,newPalette;
    assert(ui::factory_patch_for_selector(79,.55f,original));
    assert(original.name=="FERAL WOBBLE");
    assert(ui::factory_patch_for_selector(173,.5625f,newPalette));
    assert(newPalette.name=="TUBE FOUNDRY");
    std::cout<<"PASS\n";
}

void test_sustain_pedal_and_patch_selector_compatibility() {
    std::cout << "[TEST] MIDI Sustain, Repeated Notes, Mono, Panic, and Legacy Palette... ";
    constexpr float SR=48000.f;
    VoiceManager voices;voices.set_sample_rate(SR);
    voices.set_amp_envelope(.001f,.002f,1.f,.005f);
    voices.set_filter_envelope(.001f,.002f,1.f,.005f);
    auto render=[&](int count) {
        for(int i=0;i<count;++i){float env=0,weight=0;voices.process_split(env,weight);}
    };
    voices.note_on(60,.8f);render(384);
    voices.set_sustain_pedal(true);
    voices.note_off(60);
    render(2000);
    assert(voices.active_voice_count()==1); // release must wait for pedal-up
    voices.set_sustain_pedal(false);
    render(3000);
    assert(voices.active_voice_count()==0);

    voices.set_sustain_pedal(true);
    voices.note_on(60,.8f);render(384);voices.note_off(60);
    voices.note_on(60,.9f);render(384); // same key retriggers another physical voice
    assert(voices.active_voice_count()==2);
    voices.set_sustain_pedal(false);render(3000);
    assert(voices.active_voice_count()==1); // only original deferred voice releases
    voices.note_off(60);render(3000);assert(voices.active_voice_count()==0);
    voices.set_sustain_pedal(true);
    voices.note_on(48,.8f);voices.note_off(48);
    voices.all_notes_off();
    assert(!voices.sustain_pedal_down());
    render(3000);assert(voices.active_voice_count()==0);
    voices.note_on(60,1.f);
    voices.all_sounds_off();
    assert(voices.active_voice_count()==0);

    voices.set_voice_mode(true,true);
    voices.note_on(60,.8f);render(250);
    voices.set_sustain_pedal(true);voices.note_off(60);
    voices.note_on(64,.8f); // mono retarget must not inherit deferred note
    voices.set_sustain_pedal(false);render(250);
    assert(voices.active_voice_count()==1);
    voices.note_off(64);render(3000);
    assert(voices.active_voice_count()==0);

    MonkeysEarEngine engine;engine.init(SR,64);
    engine.set_parameter_normalized(79,.30f);
    assert(engine.get_current_preset().name=="MONOLITH");
    engine.set_parameter_normalized(79,.55f);
    assert(engine.get_current_preset().name=="FERAL WOBBLE");
    engine.set_parameter_normalized(79,.95f);
    assert(engine.get_current_preset().name=="VELVET LEAD");
    engine.set_parameter_normalized(173,.56f);
    assert(engine.get_current_preset().name=="TUBE FOUNDRY");
    assert(engine.get_current_preset().tube_rack.enabled);
    engine.set_parameter_normalized(172,1.f);
    engine.handle_midi_note_on(60,.8f);
    engine.handle_midi_note_off(60);
    assert(engine.get_active_voices()>0);
    engine.handle_midi_cc(64,0.f);
    engine.handle_all_notes_off();
    engine.reset();
    assert(engine.get_active_voices()==0);
    std::cout << "PASS\n";
}

void test_preset_roundtrip_and_extremes() {
    std::cout << "[TEST] Tonal Preset Round-Trip & Extreme Finite Output... ";
    auto source=PresetManager::create_feral_wobble(); auto text=source.serialize(); PresetData restored; assert(restored.deserialize(text));
    assert(restored.name==source.name && restored.sub_ratio_denominator==source.sub_ratio_denominator);
    assert(restored.eq_type==source.eq_type && std::abs(restored.mod_state_fast_fm-source.mod_state_fast_fm)<1e-5f);
    std::array<PresetData,3> patches={PresetManager::create_monolith(),PresetManager::create_feral_wobble(),PresetManager::create_velvet_lead()};
    for(auto&p:patches){MonkeysEarEngine e;e.init(96000,32);e.load_preset(p);e.handle_midi_note_on(36,1);float in[32]{},l[32]{},r[32]{};for(int block=0;block<500;++block){e.set_aftertouch((block&1)?1:0);e.process_block(in,in,l,r,32);for(float v:l)assert(std::isfinite(v)&&std::abs(v)<=1.01f);}}
    std::cout << "PASS\n";
}

void test_motion_causality() {
    std::cout << "[TEST] Source-Transform-Destination Motion Causality... "; constexpr size_t N=24000,BS=64;
    PresetData moving; moving.state_enabled=false; moving.lfo1_depth=1.0f; moving.lfo1_rate_hz=2.5f; moving.mod_lfo_cutoff=0.65f;
    moving.macros[MACRO_CUTOFF]=0.45f; moving.macros[MACRO_DRIVE]=0.05f; moving.macros[MACRO_BODY]=0.0f; moving.macros[MACRO_DELAY]=0.0f; moving.macros[MACRO_SPACE]=0.0f; moving.master_gain_db=-12.0f;
    auto static_patch=moving; static_patch.mod_lfo_cutoff=0;
    MonkeysEarEngine a,b; a.init(48000,BS);b.init(48000,BS);a.load_preset(moving);b.load_preset(static_patch);a.handle_midi_note_on(40,.9f);b.handle_midi_note_on(40,.9f);
    std::array<float,BS> in{},al{},ar{},bl{},br{};double diff=0;
    for(size_t n=0;n<N;n+=BS){a.process_block(in.data(),in.data(),al.data(),ar.data(),BS);b.process_block(in.data(),in.data(),bl.data(),br.data(),BS);for(size_t i=0;i<BS;++i){float d=al[i]-bl[i];diff+=d*d;}}
    float delta=std::sqrt(diff/static_cast<double>(N)); assert(delta>0.005f); std::cout<<"PASS (motion RMS delta="<<delta<<")\n";
}

void test_bounded_sound_space() {
    std::cout<<"[TEST] Bounded Microfields, Spectral Negotiation & Phase Relationships... ";
    constexpr float SR=48000.0f; BoundedMicrofield field; field.reset(.2f); float maximum=0.0f;
    for(int i=0;i<240000;++i)maximum=std::max(maximum,std::abs(field.process(25.0f,4.0f,.45f,.55f,.8f,.4f,1.0f/SR)));
    assert(maximum<=25.001f&&maximum>1.0f);

    SoundSpaceProcessor processor;processor.set_sample_rate(SR);SoundSpaceControls c;c.enabled=true;
    c.pitch_bound_cents=22;c.harmonic_bound_cents=46;c.modal_bound_cents=38;c.movement_hz=1.3f;
    c.attraction=.58f;c.resistance=.66f;c.repulsion=.72f;c.frequency_freedom=.86f;
    c.pitch_mix=.08f;c.harmonic_mix=.18f;c.modal_mix=.12f;c.spectral_depth_db=12;c.spectral_priority=.75f;c.phase_offset_cycles=.08f;c.phase_coupling=.5f;
    c.routes[0]={0,1,.5f};processor.set_controls(c);SoundSpaceSources s{};float min_cut=0.0f,recovered=-99.0f,peak=0.0f;
    for(int i=0;i<96000;++i){float t=static_cast<float>(i)/SR;s.lfo=std::sin(TWO_PI*.7f*t);s.envelope=i<48000?1.0f:0.0f;s.fast_energy=s.envelope*.7f;s.slow_energy=s.envelope*.5f;
        float character=.18f*std::sin(TWO_PI*180.0f*t);float weight=i<48000?.55f*std::sin(TWO_PI*55.0f*t):0.0f;
        float out=processor.process(character,weight,110.0f,s);assert(std::isfinite(out));peak=std::max(peak,std::abs(out));
        if(i==47999)min_cut=processor.metrics().spectral_gain_db;
        if(i==95999)recovered=processor.metrics().spectral_gain_db;
    }
    const auto& m=processor.metrics();assert(std::abs(m.pitch_cents)<=22.001f);assert(m.max_harmonic_cents<=46.001f);assert(m.max_modal_cents<=38.001f);
    assert(min_cut<-2.0f&&recovered>min_cut&&peak>.1f);
    SoundSpaceControls anchor;c=anchor;c.enabled=true;processor.reset();processor.set_controls(c);
    for(int i=0;i<2048;++i){float w=.31f*std::sin(TWO_PI*61.0f*i/SR);assert(std::abs(processor.process(0.0f,w,110.0f,{})-w)<1e-6f);}
    auto preset=PresetManager::create_sound_space();PresetData restored;assert(restored.deserialize(preset.serialize()));
    assert(restored.sound_space.enabled&&std::abs(restored.sound_space.harmonic_bound_cents-58.0f)<.01f&&restored.sound_space.routes[3].destination==6);
    std::cout<<"PASS (bound="<<maximum<<"c, yield="<<min_cut<<" dB -> "<<recovered<<" dB, peak="<<peak<<")\n";
}

// ── Test 5: A/B Experiment & Multi-Timbre Render ──────────────────────────
void run_ab_experiment_and_renders() {
    std::cout << "\n=======================================================\n";
    std::cout << "  MONKEY'S EAR // STATEFUL DSP V1 A/B EXPERIMENT & PROOF\n";
    std::cout << "=======================================================\n\n";

    constexpr float SR = 48000.0f;
    constexpr size_t BLOCK_SIZE = 64;
    constexpr size_t TOTAL_SECONDS = 3;
    constexpr size_t TOTAL_SAMPLES = static_cast<size_t>(SR * TOTAL_SECONDS);
    constexpr size_t NUM_BLOCKS = TOTAL_SAMPLES / BLOCK_SIZE;

    // ── Experiment: Conventional Baseline (A) vs Chrono-Stateful Body (B) ─
    std::cout << "[EXPERIMENT] Comparing Conventional Baseline vs Stateful Model on Identical Excitation...\n";

    std::vector<float> conv_left(TOTAL_SAMPLES, 0.0f), conv_right(TOTAL_SAMPLES, 0.0f);
    std::vector<float> state_left(TOTAL_SAMPLES, 0.0f), state_right(TOTAL_SAMPLES, 0.0f);
    std::vector<float> block_in(BLOCK_SIZE, 0.0f), blk_out_l(BLOCK_SIZE, 0.0f), blk_out_r(BLOCK_SIZE, 0.0f);

    // Render Conventional (Mode A: State Mechanism Disabled)
    {
        MonkeysEarEngine eng_a;
        eng_a.init(SR, BLOCK_SIZE);
        eng_a.load_preset(PresetManager::create_factory_lead());
        eng_a.set_state_enabled(false); // Clean conventional baseline

        for (size_t b = 0; b < NUM_BLOCKS; ++b) {
            float t = static_cast<float>(b * BLOCK_SIZE) / SR;
            if (b == 0) eng_a.handle_midi_note_on(57, 0.9f); // A3
            else if (t >= 1.5f && t < 1.51f) {
                eng_a.handle_all_notes_off();
                eng_a.handle_midi_note_on(60, 0.95f); // C4
            }

            eng_a.process_block(block_in.data(), block_in.data(), blk_out_l.data(), blk_out_r.data(), BLOCK_SIZE);
            for (size_t s = 0; s < BLOCK_SIZE; ++s) {
                conv_left[b * BLOCK_SIZE + s] = blk_out_l[s];
                conv_right[b * BLOCK_SIZE + s] = blk_out_r[s];
            }
        }
    }

    // Render Stateful (Mode B: Cody's ChronoStateBody Enabled)
    {
        MonkeysEarEngine eng_b;
        eng_b.init(SR, BLOCK_SIZE);
        eng_b.load_preset(PresetManager::create_factory_lead());
        eng_b.set_state_enabled(true); // Chrono-Stateful enabled
        eng_b.set_state_resistance(0.45f);
        eng_b.set_state_repulsion(0.60f);
        eng_b.set_state_coupling(0.50f);

        for (size_t b = 0; b < NUM_BLOCKS; ++b) {
            float t = static_cast<float>(b * BLOCK_SIZE) / SR;
            if (b == 0) eng_b.handle_midi_note_on(57, 0.9f); // A3
            else if (t >= 1.5f && t < 1.51f) {
                eng_b.handle_all_notes_off();
                eng_b.handle_midi_note_on(60, 0.95f); // C4
            }

            eng_b.process_block(block_in.data(), block_in.data(), blk_out_l.data(), blk_out_r.data(), BLOCK_SIZE);
            for (size_t s = 0; s < BLOCK_SIZE; ++s) {
                state_left[b * BLOCK_SIZE + s] = blk_out_l[s];
                state_right[b * BLOCK_SIZE + s] = blk_out_r[s];
            }
        }
    }

    // Measure metrics
    float peak_a = 0.0f, peak_b = 0.0f;
    double energy_a = 0.0, energy_b = 0.0;
    double waveform_diff_sq = 0.0;

    for (size_t i = 0; i < TOTAL_SAMPLES; ++i) {
        float pa = std::max(std::abs(conv_left[i]), std::abs(conv_right[i]));
        float pb = std::max(std::abs(state_left[i]), std::abs(state_right[i]));
        if (pa > peak_a) peak_a = pa;
        if (pb > peak_b) peak_b = pb;
        energy_a += conv_left[i] * conv_left[i];
        energy_b += state_left[i] * state_left[i];
        float d = state_left[i] - conv_left[i];
        waveform_diff_sq += d * d;
    }

    float rms_a = static_cast<float>(std::sqrt(energy_a / TOTAL_SAMPLES));
    float rms_b = static_cast<float>(std::sqrt(energy_b / TOTAL_SAMPLES));
    float rms_diff = static_cast<float>(std::sqrt(waveform_diff_sq / TOTAL_SAMPLES));

    std::cout << "\n[A/B METRICS RESULT]\n";
    std::cout << "  - Mode A (Conventional Baseline): Peak = " << peak_a << " (" << 20.0f * std::log10(peak_a) << " dBFS), RMS = " << rms_a << " (" << 20.0f * std::log10(rms_a) << " dBFS)\n";
    std::cout << "  - Mode B (Chrono-Stateful Body):  Peak = " << peak_b << " (" << 20.0f * std::log10(peak_b) << " dBFS), RMS = " << rms_b << " (" << 20.0f * std::log10(rms_b) << " dBFS)\n";
    std::cout << "  - RMS Timbral Deviation:          " << rms_diff << " (" << 20.0f * std::log10(rms_diff) << " dBFS difference)\n" << std::flush;
    assert(rms_diff > 0.02f); // Significant musical difference proven!

    // Write A/B comparison WAV files
    WavWriter::write_wav_24bit("monkeys_ear_ab_conventional.wav", conv_left, conv_right, static_cast<uint32_t>(SR));
    WavWriter::write_wav_24bit("monkeys_ear_ab_stateful.wav", state_left, state_right, static_cast<uint32_t>(SR));
    std::cout << "[AUDIO] Generated: monkeys_ear_ab_conventional.wav (A baseline)\n";
    std::cout << "[AUDIO] Generated: monkeys_ear_ab_stateful.wav (B stateful model)\n";

    // ── Render Multi-Timbral Proof Files ──────────────────────────────────
    std::cout << "\n[RENDER] Synthesizing deliberate sound-construction patches...\n";

    // 1. Factory Inharmonic Bell (Mathematical Demonstration)
    {
        MonkeysEarEngine eng;
        eng.init(SR, BLOCK_SIZE);
        eng.load_preset(PresetManager::create_factory_stateful_bell());
        std::vector<float> bell_l(TOTAL_SAMPLES, 0.0f), bell_r(TOTAL_SAMPLES, 0.0f);
        for (size_t b = 0; b < NUM_BLOCKS; ++b) {
            float t = static_cast<float>(b * BLOCK_SIZE) / SR;
            if (b == 0) eng.handle_midi_note_on(72, 0.95f); // High strike
            else if (t >= 1.2f && t < 1.21f) eng.handle_midi_note_on(76, 0.85f);
            eng.process_block(block_in.data(), block_in.data(), blk_out_l.data(), blk_out_r.data(), BLOCK_SIZE);
            for (size_t s = 0; s < BLOCK_SIZE; ++s) {
                bell_l[b * BLOCK_SIZE + s] = blk_out_l[s];
                bell_r[b * BLOCK_SIZE + s] = blk_out_r[s];
            }
        }
        WavWriter::write_wav_24bit("monkeys_ear_bell_proof.wav", bell_l, bell_r, static_cast<uint32_t>(SR));
        std::cout << "[AUDIO] Generated: monkeys_ear_bell_proof.wav (Inharmonic Bell)\n";
    }

    // 2. Factory Guitar Processor on Simulated Pick Attack
    {
        MonkeysEarEngine eng;
        eng.init(SR, BLOCK_SIZE);
        eng.load_preset(PresetManager::create_factory_guitar_processor());
        std::vector<float> gtr_in_l(BLOCK_SIZE, 0.0f), gtr_in_r(BLOCK_SIZE, 0.0f);
        std::vector<float> gtr_l(TOTAL_SAMPLES, 0.0f), gtr_r(TOTAL_SAMPLES, 0.0f);

        for (size_t b = 0; b < NUM_BLOCKS; ++b) {
            for (size_t s = 0; s < BLOCK_SIZE; ++s) {
                float t = static_cast<float>(b * BLOCK_SIZE + s) / SR;
                // Guitar string pluck with rich harmonics & decay envelope
                float f0 = 110.0f; // A2
                float decay = std::exp(-t * 2.5f);
                float pick = (0.6f * std::sin(TWO_PI * f0 * t) +
                              0.3f * std::sin(TWO_PI * 2.0f * f0 * t) +
                              0.2f * std::sin(TWO_PI * 3.0f * f0 * t)) * decay;
                gtr_in_l[s] = pick;
                gtr_in_r[s] = pick;
            }
            eng.process_block(gtr_in_l.data(), gtr_in_r.data(), blk_out_l.data(), blk_out_r.data(), BLOCK_SIZE);
            for (size_t s = 0; s < BLOCK_SIZE; ++s) {
                gtr_l[b * BLOCK_SIZE + s] = blk_out_l[s];
                gtr_r[b * BLOCK_SIZE + s] = blk_out_r[s];
            }
        }
        WavWriter::write_wav_24bit("monkeys_ear_guitar_fx_proof.wav", gtr_l, gtr_r, static_cast<uint32_t>(SR));
        std::cout << "[AUDIO] Generated: monkeys_ear_guitar_fx_proof.wav (Live Guitar FX)\n";
    }

    // 3. Factory Vocal Resonator
    {
        MonkeysEarEngine eng;
        eng.init(SR, BLOCK_SIZE);
        eng.load_preset(PresetManager::create_factory_vocal_resonator());
        std::vector<float> voc_in_l(BLOCK_SIZE, 0.0f), voc_in_r(BLOCK_SIZE, 0.0f);
        std::vector<float> voc_l(TOTAL_SAMPLES, 0.0f), voc_r(TOTAL_SAMPLES, 0.0f);

        for (size_t b = 0; b < NUM_BLOCKS; ++b) {
            for (size_t s = 0; s < BLOCK_SIZE; ++s) {
                float t = static_cast<float>(b * BLOCK_SIZE + s) / SR;
                float f0 = 180.0f + 50.0f * std::sin(TWO_PI * 2.0f * t);
                float vox = 0.5f * std::sin(TWO_PI * f0 * t) + 0.25f * std::sin(TWO_PI * f0 * 2.0f * t);
                voc_in_l[s] = vox;
                voc_in_r[s] = vox;
            }
            eng.process_block(voc_in_l.data(), voc_in_r.data(), blk_out_l.data(), blk_out_r.data(), BLOCK_SIZE);
            for (size_t s = 0; s < BLOCK_SIZE; ++s) {
                voc_l[b * BLOCK_SIZE + s] = blk_out_l[s];
                voc_r[b * BLOCK_SIZE + s] = blk_out_r[s];
            }
        }
        WavWriter::write_wav_24bit("monkeys_ear_vocal_proof.wav", voc_l, voc_r, static_cast<uint32_t>(SR));
        std::cout << "[AUDIO] Generated: monkeys_ear_vocal_proof.wav (Vocal Resonator)\n";
    }

    // Required musical acceptance renders and serialized presets.
    auto render_target = [&](const PresetData& preset, const char* stem, int note, bool expressive) {
        MonkeysEarEngine eng; eng.init(SR,BLOCK_SIZE); eng.load_preset(preset);
        std::vector<float> left(TOTAL_SAMPLES,0),right(TOTAL_SAMPLES,0); eng.handle_midi_note_on(note,.92f);
        for(size_t b=0;b<NUM_BLOCKS;++b){float t=static_cast<float>(b*BLOCK_SIZE)/SR;
            if(expressive){eng.set_aftertouch(clamp(t/2.0f,0.0f,1.0f));eng.handle_midi_pitch_bend(std::sin(TWO_PI*.45f*t)*1.8f);if(t>=1.45f&&t<1.452f)eng.handle_midi_note_on(note+5,.88f);}
            eng.process_block(block_in.data(),block_in.data(),blk_out_l.data(),blk_out_r.data(),BLOCK_SIZE);
            for(size_t s=0;s<BLOCK_SIZE;++s){left[b*BLOCK_SIZE+s]=blk_out_l[s];right[b*BLOCK_SIZE+s]=blk_out_r[s];}
        }
        float peak=0;for(float v:left)peak=std::max(peak,std::abs(v));float rms=rms_of(left,4096);
        WavWriter::write_wav_24bit(std::string(stem)+".wav",left,right,static_cast<uint32_t>(SR));
        std::ofstream preset_file(std::string(stem)+".mepreset",std::ios::binary);preset_file<<preset.serialize();preset_file.close();
        std::cout<<"[TARGET] "<<preset.name<<": "<<stem<<".mepreset + .wav | peak="<<20*std::log10(std::max(peak,1e-9f))<<" dBFS, RMS="<<20*std::log10(std::max(rms,1e-9f))<<" dBFS\n";
    };
    render_target(PresetManager::create_monolith(),"MONOLITH",36,false);
    render_target(PresetManager::create_feral_wobble(),"FERAL_WOBBLE",40,false);
    render_target(PresetManager::create_velvet_lead(),"VELVET_LEAD",57,true);
    render_target(PresetManager::create_sound_space(),"SOUND_SPACE",48,true);

    // ── Real-Time Benchmark Contract ─────────────────────────────────────
    std::cout << "\n[REAL-TIME CONTRACT] Verification across Buffer Sizes:\n";
    size_t test_buffers[] = {32, 64, 128, 256};
    for (size_t bs : test_buffers) {
        MonkeysEarEngine bench_eng;
        bench_eng.init(SR, bs);
        bench_eng.load_preset(PresetManager::create_sound_space());
        bench_eng.handle_midi_note_on(60, 0.9f);
        bench_eng.handle_midi_note_on(64, 0.85f);
        bench_eng.handle_midi_note_on(67, 0.85f);

        std::vector<float> inl(bs, 0.0f), inr(bs, 0.0f), outl(bs, 0.0f), outr(bs, 0.0f);
        // Warmup cache and OS page allocations
        for (int i = 0; i < 20; ++i) {
            bench_eng.process_block(inl.data(), inr.data(), outl.data(), outr.data(), bs);
        }
        bench_eng.reset_latency_stats();

        for (int i = 0; i < 300; ++i) {
            bench_eng.process_block(inl.data(), inr.data(), outl.data(), outr.data(), bs);
        }
        LatencyStats b_stats = bench_eng.get_latency_stats();
        float budget = (static_cast<float>(bs) / SR) * 1000000.0f;
        const float p50_margin = ((budget - b_stats.p50_us) / budget) * 100.0f;
        std::cout << "  - Buffer " << bs << " samples (" << std::fixed << std::setprecision(2)
                  << budget << " us deadline): Avg = " << b_stats.avg_us
                  << " us, P50 = " << b_stats.p50_us
                  << " us, P95 = " << b_stats.p95_us
                  << " us, Max = " << b_stats.max_us
                  << " us, Margin = " << b_stats.margin_percent
                  << " %, P50 Margin = " << p50_margin
                  << " %, Wall Misses = " << b_stats.deadline_misses << std::endl;
        // This offline loop runs on a shared desktop thread: a wall-clock miss can
        // be an OS pre-emption, not a DSP overrun. Keep it visible, but gate the
        // deterministic CPU claim on central execution cost. Actual host deadline
        // misses remain a REAPER verification requirement.
        assert(p50_margin > 85.0f);
    }

    std::cout << "\n>>> ALL DETERMINISTIC DSP & OFFLINE CPU TESTS PASSED; HOST DEADLINE PROOF REQUIRES REAPER. <<<\n\n";
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--audio-input-safety") {
            test_dual_layer_vocal_expression();
            test_vocal_reset_clears_all_causal_histories();
            test_audio_input_extreme_finite_safety();
            return 0;
        }
        test_polyblep_oscillator();
        test_osc_cross_fm_and_sync();
        test_chrono_state_mathematics();
        test_external_audio_processor_causality();
        test_standalone_vocal_module();
        test_dual_layer_vocal_expression();
        test_vocal_reset_clears_all_causal_histories();
        test_audio_input_extreme_finite_safety();
        test_vocal_tuning_space_and_articulation();
        test_time_varying_vocal_phrase();
        test_parameter_causality();
        test_weight_and_external_subharmonics();
        test_multipass_filter_and_eq();
        test_preset_roundtrip_and_extremes();
        test_factory_projection_for_native_editor();
        test_arturia_class_midi_keyboard_notes();
        test_gesture_field_and_mono_held_key_priority();
        test_sustain_pedal_and_patch_selector_compatibility();
        test_host_clock_and_groove_in_product();
        test_motion_causality();
        test_bounded_sound_space();
        run_ab_experiment_and_renders();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << "\n";
        return 1;
    }
}
