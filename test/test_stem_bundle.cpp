#include "monkeys_ear/stem_bundle.h"
#include "monkeys_ear/pattern_transport.h"
#include <array>
#include <cassert>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#ifdef MONKEYS_EAR_STEM_INTEGRATION
#include "monkeys_ear/engine.h"
#endif

using namespace monkeys_ear;
namespace fs = std::filesystem;

static std::vector<unsigned char> contents(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    assert(f);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
static std::uint32_t u32(const std::vector<unsigned char>& b, size_t o) {
    return std::uint32_t(b.at(o)) | (std::uint32_t(b.at(o+1)) << 8) |
           (std::uint32_t(b.at(o+2)) << 16) | (std::uint32_t(b.at(o+3)) << 24);
}
static int pcm24(const std::vector<unsigned char>& b, size_t frame, size_t channel) {
    const auto index = 44 + frame*6 + channel*3;
    const auto u = std::uint32_t(b.at(index)) |
                   (std::uint32_t(b.at(index+1))<<8) |
                   (std::uint32_t(b.at(index+2))<<16);
    return (u & 0x800000u) ? static_cast<int>(u | 0xff000000u) : static_cast<int>(u);
}

static void test_stems_and_rollback() {
    const fs::path root = "me_stem_bundle_probe_output";
    std::error_code error;
    assert(!fs::exists(root)); // Never erase a pre-existing user directory.
    const std::array<float,4> drum = {0.f, 0.25f, -0.5f, 1.5f};
    const std::array<float,3> bass = {-0.2f, 0.f, 0.4f};
    const std::array<StemSource,2> sources = {
        StemSource{"Drums \"FERAL\"", drum, {}, 0, false},
        StemSource{"Bass\\Sub", bass, {}, 4, true}
    };
    const StemBundleSettings settings{48000, 12, 139.5, 7, 8};
    assert(StemBundle::export_directory(root, sources, settings));
    assert(fs::is_regular_file(root / "stem_01.wav"));
    assert(fs::is_regular_file(root / "stem_02.wav"));
    auto wav1 = contents(root / "stem_01.wav");
    auto wav2 = contents(root / "stem_02.wav");
    assert(wav1.size() == 44 + 12*6 && wav2.size() == 44 + 12*6);
    assert(std::string(wav1.begin(), wav1.begin()+4) == "RIFF");
    assert(std::string(wav1.begin()+8, wav1.begin()+12) == "WAVE");
    assert(u32(wav1,4) == 36+12*6 && u32(wav1,24) == 48000);
    assert(u32(wav1,40) == 12*6 && u32(wav2,40) == 12*6);
    assert(pcm24(wav1,0,0) == 0 && pcm24(wav1,1,0) > 0);
    assert(pcm24(wav1,2,1) < 0 && pcm24(wav1,3,0) == 8388607); // hard limit
    assert(pcm24(wav2,0,0) == 0 && pcm24(wav2,3,0) == 0);
    assert(pcm24(wav2,4,0) < 0 && pcm24(wav2,6,0) > 0);
    assert(pcm24(wav2,7,0) == 0 && pcm24(wav2,11,1) == 0);
    const auto meta = contents(root / "manifest.json");
    const std::string m(meta.begin(),meta.end());
    assert(m.find("\"total_frames\": 12") != std::string::npos);
    assert(m.find("\"meter\": [7, 8]") != std::string::npos);
    assert(m.find("Drums \\\"FERAL\\\"") != std::string::npos);
    assert(m.find("Bass\\\\Sub") != std::string::npos);
    assert(m.find("\"offset_frames\": 4") != std::string::npos);
    // A second export never overwrites the user's existing folder.
    assert(!StemBundle::export_directory(root, sources, settings));
    assert(contents(root / "stem_01.wav") == wav1);
    // Reject an invalid source without creating any output path.
    auto invalid = sources;
    invalid[1].offset_frames = 13;
    const fs::path bad = "me_stem_bundle_invalid_output";
    assert(!fs::exists(bad));
    assert(!StemBundle::export_directory(bad, invalid, settings));
    assert(!fs::exists(bad));
    std::array<float,1> nan_data{std::numeric_limits<float>::quiet_NaN()};
    std::array<StemSource,1> nonfinite{StemSource{"Bad",nan_data,{},0,false}};
    assert(!StemBundle::export_directory(bad, nonfinite, settings));
    assert(!fs::exists(bad));
    // Correctly roll back an export if a later stem contains invalid samples.
    std::array<StemSource,2> partial = sources;
    partial[1] = StemSource{"Invalid", nan_data, {}, 4, false};
    assert(!StemBundle::export_directory(bad, partial, settings));
    assert(!fs::exists(bad));
    // Reject broken UTF-8 and impossible meters before creating a directory.
    std::array<StemSource,1> broken_name{StemSource{std::string(1, char(0xff)),drum,{},0,false}};
    assert(!StemBundle::export_directory(bad, broken_name, settings));
    StemBundleSettings impossible = settings;
    impossible.meter_den = 3;
    assert(!StemBundle::export_directory(bad, sources, impossible));
    assert(!fs::exists(bad));
    // Silent tracks are valid; their absence of samples must preserve alignment.
    const fs::path silent = "me_stem_bundle_silent_output";
    assert(!fs::exists(silent));
    std::array<StemSource,1> muted{StemSource{"Muted",{}, {}, 0, false}};
    assert(StemBundle::export_directory(silent, muted, settings));
    const auto silent_data = contents(silent/"stem_01.wav");
    assert(silent_data.size()==44+12*6);
    for (std::size_t i=44; i<silent_data.size(); ++i) assert(silent_data[i]==0);
    fs::remove_all(silent,error);
    // Legacy WavWriter call still works through the new production implementation.
    std::vector<float> l = {0.f,.5f},r = {0.f,-.5f};
    assert(WavWriter::write_wav_24bit((root/"legacy.wav").string(),l,r,44100));
    assert(contents(root/"legacy.wav").size() == 56);
    fs::remove_all(root,error);
}

#ifdef MONKEYS_EAR_STEM_INTEGRATION
static void test_real_instrument_to_stems() {
    const fs::path target = "me_actual_instrument_stems";
    std::error_code error;
    assert(!fs::exists(target));
    constexpr std::uint32_t sr = 48000;
    constexpr std::size_t frames = 48000; // one second, two quarter-note loops at 120 bpm
    std::array<std::vector<float>,2> audio_l, audio_r;
    PatternTiming timing;
    timing.bpm = 120; timing.sample_rate = sr; timing.loop_ticks = 960;
    PatternNote notes[] = {{1,0,480,0,48,127,0},{2,480,480,0,55,110,0}};
    PatternProgram program;
    assert(PatternProgram::compile(program,notes,2,timing));
    for (int track = 0; track < 2; ++track) {
        MonkeysEarEngine engine;
        engine.init(static_cast<float>(sr),128);
        engine.load_preset(track == 0 ? PresetManager::create_monolith()
                                      : PresetManager::create_feral_wobble());
        PatternPlayer player;
        player.bind(program); player.start(engine);
        audio_l[track].resize(frames);
        audio_r[track].resize(frames);
        for (std::size_t i = 0; i < frames; i += 128) {
            const auto n = std::min<std::size_t>(128,frames-i);
            player.process(engine,nullptr,nullptr,audio_l[track].data()+i,
                           audio_r[track].data()+i,n);
        }
        player.stop(engine);
    }
    const std::array<StemSource,2> stems = {
        StemSource{"Monolith synth",audio_l[0],audio_r[0],0,true},
        StemSource{"Feral synth",audio_l[1],audio_r[1],0,true}
    };
    assert(StemBundle::export_directory(target,stems,{sr,frames,120,4,4}));
    const auto a = contents(target/"stem_01.wav");
    const auto b = contents(target/"stem_02.wav");
    assert(a.size()==44+frames*6 && b.size()==a.size());
    assert(a!=b); // Existing engine gave different instrument stems.
    fs::remove_all(target,error);
}
#endif

int main() {
    test_stems_and_rollback();
#ifdef MONKEYS_EAR_STEM_INTEGRATION
    test_real_instrument_to_stems();
#endif
    std::puts("[PASS] Monkey's Ear aligned WAV stems and manifest");
}
