#include "monkeys_ear/chrono_state.h"
#include "monkeys_ear/ecosystem_bus.h"
#include "monkeys_ear/engine.h"
#include "monkeys_ear/onion_skin_eq.h"
#include "monkeys_ear/preset.h"
#include "monkeys_ear/sound_space.h"
#include "monkeys_ear/vocal_module.h"
#include "monkeys_ear/voice.h"
#include "monkeys_ear/wavetable.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

using namespace monkeys_ear;

void test_chrono_stability_high_cutoff() {
    std::cout << "[UPGRADE TEST] ChronoStateBody high-freq stability across 44.1/48/96 kHz... ";
    for (float sr : {44100.0f, 48000.0f, 96000.0f}) {
        ChronoStateBody body;
        body.set_sample_rate(sr);
        body.set_enabled(true);
        body.set_frequency(12000.0f);
        body.set_resistance(0.95f);
        body.set_repulsion(1.0f);
        body.set_coupling(1.0f);
        body.set_memory_persistence(0.9f);
        body.set_mix(1.0f);

        float max_abs = 0.0f;
        for (int i = 0; i < 4096; ++i) {
            const float x = (i < 64) ? 1.0f : std::sin(2.0f * PI * 6000.0f * static_cast<float>(i) / sr);
            const float y = body.process_sample(x);
            assert(std::isfinite(y));
            max_abs = std::max(max_abs, std::abs(y));
        }
        assert(max_abs > 0.0f && max_abs < 4.5f);

        // Also verify audible resonant excitation in the sub-stepped high-cutoff regime
        body.reset();
        body.set_frequency(5000.0f);
        body.set_resistance(0.35f);
        float res_peak = 0.0f;
        for (int i = 0; i < 4096; ++i) {
            const float x = std::sin(2.0f * PI * 5000.0f * static_cast<float>(i) / sr);
            const float y = body.process_sample(x);
            assert(std::isfinite(y));
            res_peak = std::max(res_peak, std::abs(y));
        }
        assert(res_peak > 0.005f && res_peak < 4.5f);
    }
    std::cout << "PASS\n";
}

void test_sound_space_sr_and_idle_silence() {
    std::cout << "[UPGRADE TEST] SoundSpaceProcessor normalized decay & idle gating... ";
    auto run_sr = [](float sr) {
        SoundSpaceProcessor sp;
        sp.set_sample_rate(sr);
        SoundSpaceControls c{};
        c.enabled = true;
        c.modal_bound_cents = 140.0f;
        c.harmonic_bound_cents = 90.0f;
        c.pitch_bound_cents = 40.0f;
        c.modal_mix = 0.85f;
        c.harmonic_mix = 0.45f;
        c.pitch_mix = 0.25f;
        sp.set_controls(c);

        SoundSpaceSources s{};
        s.envelope = 1.0f;
        s.audio_energy = 0.7f;
        float max_peak = 0.0f;
        const int active_n = static_cast<int>(sr * 0.2f);
        for (int i = 0; i < active_n; ++i) {
            const float in = 0.6f * std::sin(2.0f * PI * 220.0f * static_cast<float>(i) / sr);
            const float out = sp.process(in, 0.5f, 220.0f, s);
            assert(std::isfinite(out));
            max_peak = std::max(max_peak, std::abs(out));
        }
        assert(max_peak > 0.1f && max_peak < 3.5f);

        // Idle silence check after gate drops
        SoundSpaceSources idle{};
        const int settle_n = static_cast<int>(sr * 0.6f);
        float tail = 0.0f;
        for (int i = 0; i < settle_n; ++i) {
            const float out = sp.process(0.0f, 0.0f, 220.0f, idle);
            assert(std::isfinite(out));
            if (i > settle_n - 512) tail = std::max(tail, std::abs(out));
        }
        assert(tail < 0.002f);
        return max_peak;
    };
    const float p44 = run_sr(44100.0f);
    const float p96 = run_sr(96000.0f);
    assert(std::abs(p44 - p96) < 0.85f);
    std::cout << "PASS\n";
}

void test_wavetable_morph_generator_and_osc() {
    std::cout << "[UPGRADE TEST] WaveformBank morphing, CustomGen, fold, warp, unison & hard-sync... ";
    WaveformBank bank;
    WaveformGeneratorParams gen_even{};
    gen_even.shape = 0.35f;
    gen_even.odd_even = 0.05f;
    gen_even.tilt = 0.8f;
    gen_even.formant = 0.65f;
    bank.regenerate_generator(gen_even);

    PolyBLEPOscillator osc;
    osc.set_sample_rate(48000.0f);
    osc.set_frequency(220.0f);
    osc.set_waveform_bank(&bank);
    osc.set_wave_families(WaveFamily::Saw, WaveFamily::CustomGen);

    auto capture = [&](float morph, float warp, float fold, int unison) {
        osc.reset_phase();
        osc.set_morph(morph);
        osc.set_phase_warp(warp);
        osc.set_wave_fold(fold);
        osc.set_unison(unison, 18.0f);
        std::vector<float> buf(512);
        for (size_t i = 0; i < buf.size(); ++i) {
            buf[i] = osc.process_with_pm(0.0f, 0.0f);
            assert(std::isfinite(buf[i]) && std::abs(buf[i]) <= 2.5f);
        }
        return buf;
    };

    const auto a = capture(0.0f, 0.5f, 0.0f, 1);
    const auto b = capture(1.0f, 0.5f, 0.0f, 1);
    const auto warped = capture(0.5f, 0.85f, 0.65f, 5);

    float diff_ab = 0.0f, diff_aw = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        diff_ab += std::abs(a[i] - b[i]);
        diff_aw += std::abs(a[i] - warped[i]);
    }
    assert(diff_ab > 20.0f);
    assert(diff_aw > 20.0f);

    // Preview curve non-trivial and bounded
    std::array<float, 128> preview{};
    WaveformBank::render_preview(WaveFamily::Saw, WaveFamily::Formant, 0.5f, gen_even, preview);
    float prev_peak = 0.0f;
    for (float s : preview) {
        assert(std::isfinite(s));
        prev_peak = std::max(prev_peak, std::abs(s));
    }
    assert(prev_peak > 0.2f);
    std::cout << "PASS\n";
}

void test_onion_skin_eq_projection_gate() {
    std::cout << "[UPGRADE TEST] OnionSkinEQ 24-band ERB dynamic carve, surgical sculpt & Projection Gate... ";
    static_assert(OnionSkinEQ::kNumBands == 24, "Expected 24 cochlear ERB bands");
    OnionSkinEQ eq;
    eq.set_sample_rate(48000.0f);
    OnionSkinControls c{};
    c.enabled = true;
    c.intent = OnionSkinIntent::Yield;
    c.amount = 0.85f;
    c.sensitivity = 0.85f;
    c.low_protect = 0.90f;
    c.transient_protect = 0.80f;
    c.q_focus = 2.4f;
    c.manual_db[20] = 4.0f; // +4 dB at ~4275 Hz
    eq.set_controls(c);

    // Peer occupies 1.51 kHz (band 14) and 2.55 kHz (band 17) strongly
    std::array<float, OnionSkinEQ::kNumBands> peer_occ{};
    peer_occ[14] = 0.88f;
    peer_occ[17] = 0.82f;
    eq.set_peer_occupancy(peer_occ, 1.0f);

    double energy_in = 0.0, energy_out = 0.0;
    for (int i = 0; i < 6000; ++i) {
        const float t = static_cast<float>(i) / 48000.0f;
        const float x = 0.35f * std::sin(2.0f * PI * 72.0f * t) +
                        0.45f * std::sin(2.0f * PI * 1510.0f * t) +
                        0.25f * std::sin(2.0f * PI * 2550.0f * t);
        const float y = eq.process_sample(x, 0.4f * std::sin(2.0f * PI * 1510.0f * t));
        assert(std::isfinite(y));
        if (i >= 3000) {
            energy_in += x * x;
            energy_out += y * y;
        }
    }
    const auto& m = eq.metrics();
    assert(energy_out < energy_in);                 // Yield mode carved colliding bands
    assert(m.band_carve_db[14] < -0.5f);            // 1.51 kHz presence carved
    assert(std::abs(m.band_carve_db[0]) < 1.0f);    // 40 Hz sub fundamental band protected
    assert(m.total_eq_db[20] > 1.5f);               // Manual 24-band surgical boost reflected in total EQ curve
    assert(m.identity_cosine >= 0.86f - 0.02f);     // Projection Gate preserves identity
    std::cout << "PASS\n";
}

void test_deep_osc_harmonics_and_stereo_unison() {
    std::cout << "[UPGRADE TEST] H1..H8 custom partials, Osc2+RingMod, MorphCurve & stereo unison... ";
    WaveformBank bank;
    WaveformGeneratorParams gp1{};
    WaveformGeneratorParams gp2{};
    gp2.harmonic_bars[0] = 0.08f;
    gp2.harmonic_bars[3] = 0.98f;
    std::array<float, 128> prev1{}, prev2{};
    WaveformBank::render_preview(WaveFamily::CustomGen, WaveFamily::Saw, 0.0f, gp1, prev1);
    WaveformBank::render_preview(WaveFamily::CustomGen, WaveFamily::Saw, 0.0f, gp2, prev2);
    double diff_gen = 0.0;
    for (size_t i = 0; i < 128; ++i) diff_gen += std::abs(prev1[i] - prev2[i]);
    assert(diff_gen > 2.0); // H1..H8 drawbars causally reshape CustomGen waveform

    VoiceManager vm;
    vm.set_sample_rate(48000.0f);
    vm.set_wave_morph_controls(0, 5, 0.5f, 0.35f, 0.4f, 5, 22.0f, 0.3f, 0.0f);
    vm.set_deep_osc_controls(6, 11.0f, 0.45f, 1.0f, 0.25f, 2.0f, 2, 0.55f);
    vm.note_on(48, 0.9f);
    double lr_diff = 0.0, l_energy = 0.0;
    for (int i = 0; i < 2048; ++i) {
        float fe = 0.0f, w = 0.0f, ol = 0.0f, or_ = 0.0f;
        vm.process_stereo_split(fe, w, ol, or_);
        assert(std::isfinite(ol) && std::isfinite(or_));
        lr_diff += std::abs(ol - or_);
        l_energy += ol * ol;
    }
    assert(l_energy > 1.0);
    assert(lr_diff > 1.0); // Stereo unison + independent Osc2/RingMod produces true stereo field
    std::cout << "PASS\n";
}

void test_ecosystem_cross_module_bus() {
    std::cout << "[UPGRADE TEST] EcosystemBusClient Synth <-> Vocal lock-free handshake... ";
    MonkeysEarEngine synth;
    synth.init(48000.0f, 256);
    PresetData p = PresetManager::create_factory_lead();
    p.eco_mode = 3; // Link (Pub+Sub)
    p.eco_slot = 0;
    p.eco_peer_weight = 0.8f;
    p.onion_enabled = true;
    synth.load_preset(p);
    synth.handle_midi_note_on(60, 0.9f); // C4 (6000 cents)

    VocalModule vocal;
    vocal.prepare(48000.0f, 256);
    vocal.set_eco_slot(0);
    vocal.set_eco_link_enabled(true);
    VocalExpressionControls vc{};
    vc.enabled = true;
    vc.correction_strength = 0.8f;
    vc.mix = 1.0f;
    vocal.set_controls(vc);

    std::array<float, 256> zeros{}, out_l{}, out_r{}, vin{}, vout_l{}, vout_r{};
    for (int blk = 0; blk < 12; ++blk) {
        for (int i = 0; i < 256; ++i) {
            vin[i] = 0.35f * std::sin(2.0f * PI * 1600.0f * static_cast<float>(blk * 256 + i) / 48000.0f);
        }
        synth.process_block(zeros.data(), zeros.data(), out_l.data(), out_r.data(), 256);
        vocal.process_block(vin.data(), vin.data(), vout_l.data(), vout_r.data(), 256);
    }
    // Second pass so synth sees vocal's published spectrum
    synth.process_block(zeros.data(), zeros.data(), out_l.data(), out_r.data(), 256);
    assert(vocal.is_eco_peer_linked());
    assert(synth.is_eco_peer_active());
    assert(std::abs(synth.get_eco_peer_pitch_cents()) > 100.0f);
    std::cout << "PASS\n";
}

void test_factory_preset_gain_balance() {
    std::cout << "[UPGRADE TEST] Factory 8-preset RMS/Peak balance & morph richness... ";
    const std::array<PresetData, 8> patches{{
        PresetManager::create_factory_lead(),
        PresetManager::create_monolith(),
        PresetManager::create_feral_wobble(),
        PresetManager::create_velvet_lead(),
        PresetManager::create_sound_space(),
        PresetManager::create_tube_foundry(),
        PresetManager::create_clockwork_sub(),
        PresetManager::create_glass_chamber()
    }};
    std::array<float, 256> zeros{}, out_l{}, out_r{};
    for (const auto& patch : patches) {
        MonkeysEarEngine engine;
        engine.init(48000.0f, 256);
        engine.load_preset(patch);
        engine.handle_midi_note_on(57, 0.85f);
        float peak = 0.0f;
        double sum_sq = 0.0;
        for (int b = 0; b < 24; ++b) {
            engine.process_block(zeros.data(), zeros.data(), out_l.data(), out_r.data(), 256);
            for (int i = 0; i < 256; ++i) {
                assert(std::isfinite(out_l[i]) && std::isfinite(out_r[i]));
                peak = std::max(peak, std::max(std::abs(out_l[i]), std::abs(out_r[i])));
                sum_sq += static_cast<double>(out_l[i]) * out_l[i];
            }
        }
        const double rms = std::sqrt(sum_sq / (24.0 * 256.0));
        assert(rms > 0.015 && rms < 0.75);
        assert(peak <= 1.05f);
    }
    std::cout << "PASS\n";
}

} // namespace

int main() {
    test_chrono_stability_high_cutoff();
    test_sound_space_sr_and_idle_silence();
    test_wavetable_morph_generator_and_osc();
    test_onion_skin_eq_projection_gate();
    test_deep_osc_harmonics_and_stereo_unison();
    test_ecosystem_cross_module_bus();
    test_factory_preset_gain_balance();
    std::cout << "ALL UPGRADE PROBE CHECKS PASSED.\n";
    return 0;
}
