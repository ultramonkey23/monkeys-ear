#pragma once

// REAPER-compatible aligned WAV stems + versioned sidecar, offline only.
// Never call from the real-time audio callback.
#include "monkeys_ear/wav_writer.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <span>
#include <string>
#include <vector>

namespace monkeys_ear {

struct StemSource {
    std::string name;
    std::span<const float> left;
    std::span<const float> right; // Empty means duplicate left into stereo.
    std::uint64_t offset_frames = 0; // Timeline position of first input frame.
    bool effects_printed = true;
};

struct StemBundleSettings {
    std::uint32_t sample_rate = 48000;
    std::uint64_t total_frames = 0; // Identical frame count for EVERY WAV.
    double bpm = 120.0;
    std::uint16_t meter_num = 4;
    std::uint16_t meter_den = 4;
};

class StemBundle final {
public:
    // Creates ONLY a previously absent directory. No existing work is
    // overwritten. If any write fails, remove only the directory we created.
    static bool export_directory(const std::filesystem::path& target,
                                 std::span<const StemSource> stems,
                                 const StemBundleSettings& s) {
        if (stems.empty() || stems.size() > 64 || s.total_frames == 0 ||
            s.sample_rate < 8000 || s.sample_rate > 384000 ||
            !finite(s.bpm) || s.bpm <= 0 || s.bpm > 2000 ||
            s.meter_num == 0 || s.meter_den == 0 ||
            (s.meter_den & (s.meter_den - 1)) != 0 ||
            s.total_frames > (UINT32_MAX - 36ULL) / 6ULL) return false;
        for (const auto& stem : stems) {
            if (stem.name.empty() || stem.name.size() > 256 ||
                !valid_utf8(stem.name) || stem.offset_frames > s.total_frames ||
                stem.left.size() > s.total_frames - stem.offset_frames ||
                (!stem.right.empty() && stem.left.size() != stem.right.size()))
                return false;
        }
        std::error_code error;
        if (!std::filesystem::create_directory(target, error) || error)
            return false; // Existing path, even an empty directory, is sacred.
        bool ok = false;
        do {
            std::vector<std::uint64_t> checksums;
            checksums.reserve(stems.size());
            for (std::size_t i = 0; i < stems.size(); ++i) {
                std::uint64_t hash = 0;
                if (!WavWriter::write_aligned_24bit(target / filename(i),
                    stems[i].left, stems[i].right, stems[i].offset_frames,
                    s.total_frames, s.sample_rate, &hash)) break;
                checksums.push_back(hash);
            }
            if (checksums.size() != stems.size()) break;
            std::ofstream meta(target / "manifest.json", std::ios::binary);
            if (!meta) break;
            meta << "{\n  \"format\": \"monkeys-ear-stems\",\n  \"version\": 1,\n"
                 << "  \"timeline_start_frame\": 0,\n  \"total_frames\": " << s.total_frames
                 << ",\n  \"sample_rate\": " << s.sample_rate
                 << ",\n  \"tempo_bpm\": " << std::setprecision(16) << s.bpm
                 << ",\n  \"meter\": [" << s.meter_num << ", " << s.meter_den
                 << "],\n  \"codec\": \"PCM_S24LE_STEREO\",\n"
                 << "  \"checksum\": \"data-fnv1a64-noncryptographic\",\n"
                 << "  \"stems\": [\n";
            for (std::size_t i = 0; i < stems.size(); ++i) {
                const auto& stem = stems[i];
                meta << "    {\"id\": " << (i + 1) << ", \"name\": \""
                     << escape_json(stem.name) << "\", \"file\": \""
                     << filename(i) << "\", \"offset_frames\": "
                     << stem.offset_frames << ", \"source_frames\": "
                     << stem.left.size() << ", \"effects_printed\": "
                     << (stem.effects_printed ? "true" : "false")
                     << ", \"data_fnv1a64\": \"" << hex_hash(checksums[i])
                     << "\"}" << (i + 1 < stems.size() ? "," : "") << "\n";
            }
            meta << "  ]\n}\n";
            meta.flush();
            if (!meta.good()) break;
            ok = true;
        } while (false);
        if (!ok) std::filesystem::remove_all(target, error);
        return ok;
    }
private:
    static bool finite(double x) noexcept {
        return (std::bit_cast<std::uint64_t>(x) & 0x7ff0000000000000ULL)
            != 0x7ff0000000000000ULL;
    }
    static std::string filename(std::size_t index) {
        const auto n = index + 1;
        return "stem_" + (n < 10 ? std::string("0") : std::string()) +
               std::to_string(n) + ".wav";
    }
    static std::string hex_hash(std::uint64_t hash) {
        constexpr char d[] = "0123456789abcdef";
        std::string s(16, '0');
        for (int i = 15; i >= 0; --i) { s[i] = d[hash & 15]; hash >>= 4; }
        return s;
    }
    static bool valid_utf8(const std::string& s) {
        for (std::size_t i = 0; i < s.size();) {
            const auto c = static_cast<unsigned char>(s[i]);
            if (c < 0x80) { ++i; continue; }
            std::size_t length = 0;
            std::uint32_t codepoint = 0;
            std::uint32_t minval = 0;
            if ((c & 0xe0) == 0xc0) { length = 2; codepoint = c & 0x1f; minval = 0x80; }
            else if ((c & 0xf0) == 0xe0) { length = 3; codepoint = c & 0x0f; minval = 0x800; }
            else if ((c & 0xf8) == 0xf0) { length = 4; codepoint = c & 0x07; minval = 0x10000; }
            else return false;
            if (length > s.size() - i) return false;
            for (std::size_t j = 1; j < length; ++j) {
                const auto d = static_cast<unsigned char>(s[i+j]);
                if ((d & 0xc0) != 0x80) return false;
                codepoint = (codepoint << 6) | (d & 0x3f);
            }
            if (codepoint < minval || codepoint > 0x10ffff ||
                (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
            i += length;
        }
        return true;
    }
    static std::string escape_json(const std::string& s) {
        std::string out;
        constexpr char d[] = "0123456789abcdef";
        for (unsigned char c : s) {
            if (c == '\\' || c == '"') { out += '\\'; out += char(c); }
            else if (c < 32) {
                out += "\\u00"; out += d[c >> 4]; out += d[c & 15];
            } else out += char(c);
        }
        return out;
    }
};
} // namespace monkeys_ear
