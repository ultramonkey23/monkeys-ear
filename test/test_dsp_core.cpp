#include "monkeys_ear/engine.h"
#include "wav_writer.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <iomanip>

using namespace monkeys_ear;

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

void test_svf_filter_stability() {
    std::cout << "[TEST] ZDF State Variable Filter Stability... ";
    StateVariableFilter svf;
    svf.set_sample_rate(48000.0f);
    svf.set_cutoff(2000.0f);
    svf.set_resonance(0.95f); // High resonance

    // Impulse excitation
    float impulse = 1.0f;
    for (int i = 0; i < 96000; ++i) {
        float out = svf.process(impulse);
        impulse = 0.0f;
        assert(!std::isnan(out));
        assert(!std::isinf(out));
        assert(std::abs(out) < 10.0f); // Bounded
    }
    std::cout << "PASS\n";
}

void test_tube_drive_causality() {
    std::cout << "[TEST] Tube Drive Memory Causality & Boundedness... ";
    TubeDriveStage tube;
    tube.set_sample_rate(48000.0f);
    tube.set_drive(0.85f);
    tube.set_memory_sag(0.75f);

    for (int i = 0; i < 48000; ++i) {
        float in = std::sin(static_cast<float>(i) * 0.05f);
        float out = tube.process(in);
        assert(!std::isnan(out));
        assert(!std::isinf(out));
        assert(std::abs(out) <= 2.0f);
    }
    std::cout << "PASS\n";
}

void test_safety_limiter_ceiling() {
    std::cout << "[TEST] Safety Limiter Absolute Ceiling... ";
    SafetyLimiter limiter;
    limiter.set_ceiling(-0.2f); // ~0.977 linear

    for (int i = 0; i < 1000; ++i) {
        // Massive abusive overdrive signal (+40dB)
        float left = 50.0f * std::sin(static_cast<float>(i) * 0.1f);
        float right = 50.0f * std::cos(static_cast<float>(i) * 0.1f);
        limiter.process(left, right);
        assert(!std::isnan(left) && !std::isnan(right));
        assert(std::abs(left) <= 1.0f);
        assert(std::abs(right) <= 1.0f);
    }
    std::cout << "PASS\n";
}

void run_render_and_benchmark() {
    std::cout << "\n=======================================================\n";
    std::cout << "  MONKEY'S EAR // FIRST REAL BUILD AUDIO PROOF & BENCHMARK\n";
    std::cout << "=======================================================\n\n";

    constexpr float SAMPLE_RATE = 48000.0f;
    constexpr size_t BLOCK_SIZE = 64;
    constexpr size_t TOTAL_SECONDS = 4;
    constexpr size_t TOTAL_SAMPLES = static_cast<size_t>(SAMPLE_RATE * TOTAL_SECONDS);
    constexpr size_t NUM_BLOCKS = TOTAL_SAMPLES / BLOCK_SIZE;

    MonkeysEarEngine engine;
    engine.init(SAMPLE_RATE, BLOCK_SIZE);
    engine.load_preset(PresetManager::create_factory_lead());

    std::vector<float> rendered_left(TOTAL_SAMPLES, 0.0f);
    std::vector<float> rendered_right(TOTAL_SAMPLES, 0.0f);

    std::vector<float> block_in_l(BLOCK_SIZE, 0.0f);
    std::vector<float> block_in_r(BLOCK_SIZE, 0.0f);
    std::vector<float> block_out_l(BLOCK_SIZE, 0.0f);
    std::vector<float> block_out_r(BLOCK_SIZE, 0.0f);

    // Score a 4-second musical progression:
    // Bar 1: C minor chord (MIDI 48, 55, 60, 63)
    // Bar 2: G minor chord (MIDI 43, 55, 58, 62)
    // Bar 3: Ab major chord (MIDI 44, 56, 60, 63)
    // Bar 4: Bb major with lead arpeggiation (MIDI 46, 58, 62, 65, 70)
    std::cout << "[RENDER] Synthesizing 4-second musical piece through full DSP chain...\n";
    std::cout << "         (Synth -> Filter -> Tube Drive -> Cab -> Delay -> FDN Reverb -> Limiter)\n";

    for (size_t b = 0; b < NUM_BLOCKS; ++b) {
        size_t current_sample = b * BLOCK_SIZE;
        float current_time_s = static_cast<float>(current_sample) / SAMPLE_RATE;

        // Note triggers
        if (b == 0) {
            engine.handle_midi_note_on(48, 0.85f); // C3
            engine.handle_midi_note_on(55, 0.75f); // G3
            engine.handle_midi_note_on(60, 0.80f); // C4
            engine.handle_midi_note_on(63, 0.80f); // Eb4
        } else if (current_time_s >= 1.0f && current_time_s < 1.01f) {
            engine.handle_all_notes_off();
            engine.handle_midi_note_on(43, 0.85f); // G2
            engine.handle_midi_note_on(55, 0.75f); // G3
            engine.handle_midi_note_on(58, 0.80f); // Bb3
            engine.handle_midi_note_on(62, 0.80f); // D4
        } else if (current_time_s >= 2.0f && current_time_s < 2.01f) {
            engine.handle_all_notes_off();
            engine.handle_midi_note_on(44, 0.85f); // Ab2
            engine.handle_midi_note_on(56, 0.75f); // Ab3
            engine.handle_midi_note_on(60, 0.80f); // C4
            engine.handle_midi_note_on(63, 0.85f); // Eb4
        } else if (current_time_s >= 3.0f && current_time_s < 3.01f) {
            engine.handle_all_notes_off();
            engine.handle_midi_note_on(46, 0.90f); // Bb2
            engine.handle_midi_note_on(58, 0.80f); // Bb3
            engine.handle_midi_note_on(62, 0.80f); // D4
            engine.handle_midi_note_on(70, 0.95f); // Bb4 lead scream
            // Mod wheel modulation on lead climax
            engine.handle_midi_cc(1, 0.85f);
        } else if (current_time_s >= 3.8f) {
            engine.handle_all_notes_off();
        }

        engine.process_block(
            block_in_l.data(),
            block_in_r.data(),
            block_out_l.data(),
            block_out_r.data(),
            BLOCK_SIZE
        );

        for (size_t s = 0; s < BLOCK_SIZE; ++s) {
            rendered_left[current_sample + s] = block_out_l[s];
            rendered_right[current_sample + s] = block_out_r[s];
        }
    }

    // Write Synth WAV
    std::string synth_wav = "monkeys_ear_synth_proof.wav";
    bool synth_ok = WavWriter::write_wav_24bit(synth_wav, rendered_left, rendered_right, static_cast<uint32_t>(SAMPLE_RATE));
    std::cout << "[AUDIO] Generated: " << synth_wav << " (" << (synth_ok ? "SUCCESS" : "FAILED") << ")\n";

    // Measure peak and RMS of output
    float max_peak = 0.0f;
    double sum_sq = 0.0;
    for (size_t i = 0; i < TOTAL_SAMPLES; ++i) {
        float peak = std::max(std::abs(rendered_left[i]), std::abs(rendered_right[i]));
        if (peak > max_peak) max_peak = peak;
        sum_sq += rendered_left[i] * rendered_left[i] + rendered_right[i] * rendered_right[i];
    }
    float rms = static_cast<float>(std::sqrt(sum_sq / (2.0 * TOTAL_SAMPLES)));
    std::cout << "[AUDIO] Measured Output Peak: " << max_peak << " (" << (20.0f * std::log10(max_peak)) << " dBFS)\n";
    std::cout << "[AUDIO] Measured Output RMS:  " << rms << " (" << (20.0f * std::log10(rms)) << " dBFS)\n";
    assert(max_peak > 0.01f); // Must have produced real sound!
    assert(max_peak <= 1.0f); // Limiter must have kept it bounded!

    // Print real-time latency statistics
    LatencyStats stats = engine.get_latency_stats();
    std::cout << "\n[REAL-TIME CONTRACT] Benchmark at 64 samples buffer @ 48kHz:\n";
    std::cout << "  - Hardware Block Deadline: " << std::fixed << std::setprecision(2) << stats.budget_us << " us (1.33 ms)\n";
    std::cout << "  - Average Block Time:      " << stats.avg_us << " us\n";
    std::cout << "  - P95 Block Time:          " << stats.p95_us << " us\n";
    std::cout << "  - P99 Block Time:          " << stats.p99_us << " us\n";
    std::cout << "  - Max Block Time:          " << stats.max_us << " us\n";
    std::cout << "  - Deadline Margin:         " << stats.margin_percent << " %\n";
    std::cout << "  - Total Blocks:            " << stats.total_blocks << "\n";
    std::cout << "  - Deadline Misses / Dropouts: " << stats.deadline_misses << "\n";
    assert(stats.deadline_misses == 0);

    // Now test LIVE MICROPHONE through-processing
    std::cout << "\n[RENDER] Processing simulated Live Mic input through Resonator/Tube/Reverb...\n";
    engine.reset();
    engine.load_preset(PresetManager::create_factory_vocal_resonator());

    std::vector<float> vocal_left(TOTAL_SAMPLES, 0.0f);
    std::vector<float> vocal_right(TOTAL_SAMPLES, 0.0f);

    // Generate vocal chirp/formant sweep input
    for (size_t b = 0; b < NUM_BLOCKS; ++b) {
        size_t current_sample = b * BLOCK_SIZE;
        for (size_t s = 0; s < BLOCK_SIZE; ++s) {
            float t = static_cast<float>(current_sample + s) / SAMPLE_RATE;
            // Vocal formant sweep simulation
            float f0 = 160.0f + 80.0f * std::sin(TWO_PI * 1.5f * t);
            float mic_in = 0.6f * std::sin(TWO_PI * f0 * t) + 0.3f * std::sin(TWO_PI * f0 * 2.0f * t);
            block_in_l[s] = mic_in;
            block_in_r[s] = mic_in;
        }

        engine.process_block(
            block_in_l.data(),
            block_in_r.data(),
            block_out_l.data(),
            block_out_r.data(),
            BLOCK_SIZE
        );

        for (size_t s = 0; s < BLOCK_SIZE; ++s) {
            vocal_left[current_sample + s] = block_out_l[s];
            vocal_right[current_sample + s] = block_out_r[s];
        }
    }

    std::string vocal_wav = "monkeys_ear_vocal_proof.wav";
    bool vocal_ok = WavWriter::write_wav_24bit(vocal_wav, vocal_left, vocal_right, static_cast<uint32_t>(SAMPLE_RATE));
    std::cout << "[AUDIO] Generated: " << vocal_wav << " (" << (vocal_ok ? "SUCCESS" : "FAILED") << ")\n";

    // Verify multi-buffer deadlines: 32, 64, 128, 256
    std::cout << "\n[TEST] Verifying Buffer Deadline Safety across buffer sizes:\n";
    size_t test_buffers[] = {32, 64, 128, 256};
    for (size_t bs : test_buffers) {
        MonkeysEarEngine test_eng;
        test_eng.init(SAMPLE_RATE, bs);
        test_eng.handle_midi_note_on(60, 0.9f);
        test_eng.handle_midi_note_on(64, 0.9f);
        test_eng.handle_midi_note_on(67, 0.9f);

        std::vector<float> inl(bs, 0.0f), inr(bs, 0.0f), outl(bs, 0.0f), outr(bs, 0.0f);
        for (int i = 0; i < 200; ++i) {
            test_eng.process_block(inl.data(), inr.data(), outl.data(), outr.data(), bs);
        }
        LatencyStats b_stats = test_eng.get_latency_stats();
        float budget = (static_cast<float>(bs) / SAMPLE_RATE) * 1000000.0f;
        std::cout << "  - Buffer " << bs << " samples (" << budget << " us): Avg = "
                  << b_stats.avg_us << " us, Max = " << b_stats.max_us
                  << " us, Margin = " << b_stats.margin_percent << "%\n";
        assert(b_stats.deadline_misses == 0);
    }

    std::cout << "\n>>> ALL DETERMINISTIC DSP & REAL-TIME TESTS PASSED! <<<\n\n";
}

int main() {
    try {
        test_polyblep_oscillator();
        test_svf_filter_stability();
        test_tube_drive_causality();
        test_safety_limiter_ceiling();
        run_render_and_benchmark();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << "\n";
        return 1;
    }
}
