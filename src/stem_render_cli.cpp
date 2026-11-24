#include "monkeys_ear/engine.h"
#include "monkeys_ear/vocal_module.h"
#include "monkeys_ear/onion_eq_module.h"
#include "monkeys_ear/pattern_transport.h"
#include "monkeys_ear/stem_bundle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#endif

using namespace monkeys_ear;

namespace {

bool check_finite_audio(const std::vector<float>& buf) noexcept {
    for (float x : buf) {
        if ((std::bit_cast<std::uint32_t>(x) & 0x7f800000u) == 0x7f800000u) return false;
    }
    return true;
}

double compute_rms(const std::vector<float>& buf) noexcept {
    double sum = 0.0;
    for (float x : buf) sum += static_cast<double>(x) * static_cast<double>(x);
    return std::sqrt(sum / static_cast<double>(std::max<size_t>(1, buf.size())));
}

struct RenderConfig {
    std::filesystem::path out_dir;
    double bpm = 126.0;
    int bars = 2;
    int meter_num = 7;
    int meter_den = 8;
    bool selftest = false;
};

bool render_ecosystem_stems(const RenderConfig& cfg) {
    constexpr std::uint32_t sr = 48000;
    constexpr size_t block_size = 256;

    const double bar_quarters = static_cast<double>(cfg.meter_num) * (4.0 / static_cast<double>(cfg.meter_den));
    const double total_quarters = bar_quarters * static_cast<double>(cfg.bars);
    const std::uint64_t total_frames = static_cast<std::uint64_t>(std::llround(
        (total_quarters * 60.0 / cfg.bpm) * static_cast<double>(sr)));
    const std::uint32_t loop_ticks = static_cast<std::uint32_t>(std::llround(bar_quarters * 960.0));

    // Stem 1: Lead synth playing compiled odd-meter pattern with grown resonator
    PresetData lead_p = PresetManager::create_velvet_lead();
    lead_p.mono_mode = false;
    lead_p.legato = false;
    lead_p.eco_mode = 1; // Publish Lead spectrum/pitch to MEC4 Ch 1
    lead_p.eco_slot = 0;
    lead_p = PresetGrowth::develop(lead_p, 19, 320, 0.55f).preset;

    MonkeysEarEngine lead_eng;
    lead_eng.init(static_cast<float>(sr), block_size);
    lead_eng.load_preset(lead_p);

    std::vector<PatternNote> lead_notes;
    static constexpr int lead_pitches[7] = {62, 65, 69, 70, 69, 65, 64};
    for (int step = 0; step < cfg.meter_num; ++step) {
        const std::int64_t raw_tick = static_cast<std::int64_t>(step) * static_cast<std::int64_t>(loop_ticks) / cfg.meter_num;
        PatternNote n{};
        n.id = step + 1;
        n.raw_tick = raw_tick + (step % 2 ? 12 : -8);
        n.raw_tick = std::clamp<std::int64_t>(n.raw_tick, 0, static_cast<std::int64_t>(loop_ticks) - 1);
        n.length_ticks = std::max<std::uint32_t>(60u, loop_ticks / static_cast<std::uint32_t>(cfg.meter_num * 2));
        n.key = static_cast<std::uint8_t>(lead_pitches[step % 7]);
        n.velocity = static_cast<std::uint8_t>(step == 0 ? 120 : 96);
        lead_notes.push_back(n);
    }
    PatternProgram lead_prog{};
    PatternTiming pt{960, loop_ticks, cfg.bpm, static_cast<double>(sr), 240, 0.90, 0.12};
    if (!PatternProgram::compile(lead_prog, lead_notes.data(), lead_notes.size(), pt)) {
        std::cerr << "[FAIL] could not compile lead PatternProgram\n";
        return false;
    }
    PatternPlayer lead_player;
    lead_player.bind(lead_prog);
    lead_player.start(lead_eng);

    // Stem 2: Sub Foundation with OnionSkinEQ yielding to Lead/Vocal peers on MEC4 Ch 1
    PresetData bass_p = PresetManager::create_monolith();
    bass_p.onion_enabled = true;
    bass_p.onion_intent = 1; // Yield
    bass_p.onion_amount = 0.72f;
    bass_p.onion_harmonic_protect = 0.75f;
    bass_p.eco_mode = 2; // Follow Vocal + Aux on Ch 1
    bass_p.eco_slot = 0;

    MonkeysEarEngine bass_eng;
    bass_eng.init(static_cast<float>(sr), block_size);
    bass_eng.load_preset(bass_p);

    std::vector<PatternNote> bass_notes;
    for (int step = 0; step < 3; ++step) {
        PatternNote n{};
        n.id = step + 1;
        n.raw_tick = static_cast<std::int64_t>(step) * static_cast<std::int64_t>(loop_ticks) / 3;
        n.length_ticks = std::max<std::uint32_t>(120u, loop_ticks / 4);
        n.key = static_cast<std::uint8_t>(step == 2 ? 45 : 38);
        n.velocity = 112;
        bass_notes.push_back(n);
    }
    PatternProgram bass_prog{};
    if (!PatternProgram::compile(bass_prog, bass_notes.data(), bass_notes.size(), pt)) {
        std::cerr << "[FAIL] could not compile bass PatternProgram\n";
        return false;
    }
    PatternPlayer bass_player;
    bass_player.bind(bass_prog);
    bass_player.start(bass_eng);

    // Stem 3: Vocal Harmony quantized cleanly to D Phrygian Dominant + MEC4 Vocal publisher
    VocalModule vocal_mod;
    vocal_mod.prepare(static_cast<float>(sr), block_size);
    vocal_mod.set_eco_slot(0);
    vocal_mod.set_eco_mode(3);
    VocalExpressionControls vc{};
    vc.enabled = true;
    vc.correction_strength = 0.85f;
    vc.drift_retention = 0.25f;
    vc.vibrato_retention = 0.75f;
    vc.mix = 0.90f;
    vc.target.tuning.set_builtin(VocalBuiltinTuning::PhrygianDominant);
    vc.target.tuning.root_cents = 500.0f; // D root (relative to A=0)
    vc.target.portamento = 0.25f;
    vocal_mod.set_controls(vc);

    // Stem 4: Onion EQ Master Sculpt / Companion Bus (MEC4 role 3)
    OnionEqModule onion_bus;
    onion_bus.prepare(static_cast<float>(sr), block_size);
    OnionEqControls oc{};
    oc.dynamic_enabled = true;
    oc.intent = OnionSkinIntent::Yield;
    oc.amount = 0.65f;
    oc.harmonic_protect = 0.60f;
    oc.eco_mode = EcoLinkMode::Bidirectional;
    oc.eco_slot = 0;
    onion_bus.set_controls(oc);

    const size_t N = static_cast<size_t>(total_frames);
    std::vector<float> s1_l(N), s1_r(N), s2_l(N), s2_r(N), s3_l(N), s3_r(N), s4_l(N), s4_r(N);
    std::vector<float> vin_l(block_size), vin_r(block_size), bus_in_l(block_size), bus_in_r(block_size);

    for (size_t pos = 0; pos < N; pos += block_size) {
        const size_t n = std::min(block_size, N - pos);
        lead_player.process(lead_eng, nullptr, nullptr, s1_l.data() + pos, s1_r.data() + pos, n);

        // Synthesize expressive vibrato vocal phrase around D4/Eb4/F#4 into VocalModule
        for (size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(pos + i) / static_cast<double>(sr);
            const float f0 = 300.0f + 12.0f * std::sin(TWO_PI * 5.0f * static_cast<float>(t));
            const float ph = static_cast<float>(TWO_PI * f0 * t);
            const float sig = 0.28f * (std::sin(ph) + 0.45f * std::sin(2.0f * ph) + 0.22f * std::sin(3.0f * ph));
            vin_l[i] = vin_r[i] = sig;
        }
        vocal_mod.set_local_pitch_evidence(302.0f, 0.90f);
        vocal_mod.process_block(vin_l.data(), vin_r.data(), s3_l.data() + pos, s3_r.data() + pos, static_cast<std::uint32_t>(n));

        bass_player.process(bass_eng, nullptr, nullptr, s2_l.data() + pos, s2_r.data() + pos, n);

        for (size_t i = 0; i < n; ++i) {
            bus_in_l[i] = 0.45f * s1_l[pos + i] + 0.45f * s2_l[pos + i] + 0.35f * s3_l[pos + i];
            bus_in_r[i] = 0.45f * s1_r[pos + i] + 0.45f * s2_r[pos + i] + 0.35f * s3_r[pos + i];
        }
        onion_bus.process_block(bus_in_l.data(), bus_in_r.data(),
                                s3_l.data() + pos, s3_r.data() + pos,
                                s4_l.data() + pos, s4_r.data() + pos,
                                static_cast<std::uint32_t>(n));
    }

    lead_eng.reset();
    bass_eng.reset();
    vocal_mod.reset();
    onion_bus.reset();

    if (!check_finite_audio(s1_l) || !check_finite_audio(s2_l) || !check_finite_audio(s3_l) || !check_finite_audio(s4_l)) {
        std::cerr << "[FAIL] non-finite audio in stem render\n";
        return false;
    }
    const double r1 = compute_rms(s1_l), r2 = compute_rms(s2_l), r3 = compute_rms(s3_l), r4 = compute_rms(s4_l);
    if (r1 < 1e-4 || r2 < 1e-4 || r3 < 1e-4 || r4 < 1e-4) {
        std::cerr << "[FAIL] silent stem generated: rms=(" << r1 << ", " << r2 << ", " << r3 << ", " << r4 << ")\n";
        return false;
    }

    const std::array<StemSource, 4> stems{{
        {"01_Lead_Synth", s1_l, s1_r, 0, true},
        {"02_Sub_Foundation", s2_l, s2_r, 0, true},
        {"03_Vocal_Phrygian", s3_l, s3_r, 0, true},
        {"04_Onion_Master_Bus", s4_l, s4_r, 0, true}
    }};
    StemBundleSettings s{};
    s.sample_rate = sr;
    s.total_frames = total_frames;
    s.bpm = cfg.bpm;
    s.meter_num = static_cast<std::uint16_t>(cfg.meter_num);
    s.meter_den = static_cast<std::uint16_t>(cfg.meter_den);
    if (!StemBundle::export_directory(cfg.out_dir, stems, s)) {
        std::cerr << "[FAIL] StemBundle::export_directory failed for " << cfg.out_dir << "\n";
        return false;
    }
    std::cout << "[PASS] Exported 4 aligned stems (" << total_frames << " frames @ " << sr
              << " Hz, " << cfg.bpm << " BPM, " << cfg.meter_num << "/" << cfg.meter_den
              << ") -> " << cfg.out_dir.string() << "\n";
    return true;
}

} // namespace

int main(int argc, char** argv) {
    RenderConfig cfg{};
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--selftest") == 0) {
            cfg.selftest = true;
        } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            cfg.out_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--bpm") == 0 && i + 1 < argc) {
            cfg.bpm = std::clamp(std::atof(argv[++i]), 40.0, 240.0);
        } else if (std::strcmp(argv[i], "--bars") == 0 && i + 1 < argc) {
            cfg.bars = std::clamp(std::atoi(argv[++i]), 1, 16);
        } else if (std::strcmp(argv[i], "--meter") == 0 && i + 1 < argc) {
            int num = 4, den = 4;
            if (std::sscanf(argv[++i], "%d/%d", &num, &den) == 2 && num >= 1 && num <= 32 &&
                (den == 2 || den == 4 || den == 8 || den == 16)) {
                cfg.meter_num = num;
                cfg.meter_den = den;
            }
        }
    }

    if (cfg.selftest) {
        std::error_code ec;
        auto base = std::filesystem::temp_directory_path(ec);
        if (ec) {
            std::cerr << "[FAIL] temp_directory_path failed\n";
            return 1;
        }
        unsigned pid = 0;
#ifdef _WIN32
        pid = static_cast<unsigned>(_getpid());
#endif
        cfg.out_dir = base / ("monkeys_ear_stem_selftest_" + std::to_string(pid));
        std::filesystem::remove_all(cfg.out_dir, ec);
        const bool ok = render_ecosystem_stems(cfg);
        bool verified = ok && std::filesystem::exists(cfg.out_dir / "manifest.json", ec);
        for (int idx = 1; idx <= 4 && verified; ++idx) {
            const std::string fn = "stem_0" + std::to_string(idx) + ".wav";
            verified = std::filesystem::file_size(cfg.out_dir / fn, ec) > 44u;
        }
        std::filesystem::remove_all(cfg.out_dir, ec);
        if (!verified) {
            std::cerr << "[FAIL] stem render selftest verification failed\n";
            return 1;
        }
        std::cout << ">>> MONKEYS EAR STEM RENDERER SELFTEST PASSED! <<<\n";
        return 0;
    }

    if (cfg.out_dir.empty()) {
        std::cerr << "Usage: monkeys_ear_stem_render [--selftest | --out <new-dir> [--bpm 126] [--bars 2] [--meter 7/8]]\n";
        return 1;
    }
    return render_ecosystem_stems(cfg) ? 0 : 1;
}
