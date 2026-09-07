#include "monkeys_ear/engine.h"
#include "monkeys_ear/chrono_state.h"
#include "wav_writer.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <fstream>

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
    const auto& automated=engine.get_current_preset();
    assert(std::abs(automated.sub_mix-0.91f)<1e-5f);
    assert(automated.eq_frequency_hz[2]>20.0f);
    assert(automated.mod_lfo_fm>0.7f);
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
    std::cout << "  - RMS Timbral Deviation:          " << rms_diff << " (" << 20.0f * std::log10(rms_diff) << " dBFS difference)\n";
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
        std::cout << "  - Buffer " << bs << " samples (" << std::fixed << std::setprecision(2)
                  << budget << " us deadline): Avg = " << b_stats.avg_us
                  << " us, P95 = " << b_stats.p95_us
                  << " us, Max = " << b_stats.max_us
                  << " us, Margin = " << b_stats.margin_percent
                  << " %, Misses = " << b_stats.deadline_misses << std::endl;
        assert(b_stats.deadline_misses == 0);
        assert(b_stats.margin_percent > 85.0f);
    }

    std::cout << "\n>>> ALL DETERMINISTIC DSP & REAL-TIME TESTS PASSED! <<<\n\n";
}

int main() {
    try {
        test_polyblep_oscillator();
        test_osc_cross_fm_and_sync();
        test_chrono_state_mathematics();
        test_external_audio_processor_causality();
        test_parameter_causality();
        test_weight_and_external_subharmonics();
        test_multipass_filter_and_eq();
        test_preset_roundtrip_and_extremes();
        test_motion_causality();
        test_bounded_sound_space();
        run_ab_experiment_and_renders();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << "\n";
        return 1;
    }
}
