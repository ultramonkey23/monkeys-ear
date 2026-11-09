#pragma once

// Production, offline-only 24-bit PCM RIFF writer. Does not run on the audio
// callback. Shared with existing DSP test renders and synchronized stem export.
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace monkeys_ear {
class WavWriter final {
public:
    static bool write_wav_24bit(const std::string& filepath,
                                const std::vector<float>& left,
                                const std::vector<float>& right,
                                std::uint32_t rate) {
        return write_aligned_24bit(filepath, left, right, 0, left.size(), rate);
    }

    // Output is always 24-bit stereo PCM with exactly total_frames and common
    // frame-zero. A mono input (empty right span) is duplicated to the right.
    // Source is placed at offset_frames; all other output frames are silence.
    // out_data_hash is a *non-cryptographic* FNV-1a hash of PCM data bytes.
    static bool write_aligned_24bit(const std::filesystem::path& filename,
                                    std::span<const float> left,
                                    std::span<const float> right,
                                    std::uint64_t offset_frames,
                                    std::uint64_t total_frames,
                                    std::uint32_t sample_rate,
                                    std::uint64_t* out_data_hash = nullptr) {
        if (!sample_rate || !total_frames || offset_frames > total_frames ||
            left.size() > total_frames - offset_frames ||
            (!right.empty() && right.size() != left.size()) ||
            total_frames > (std::numeric_limits<std::uint32_t>::max() - 36ULL) / 6ULL)
            return false;
        // Validate BEFORE opening a file. Under -ffast-math, isfinite() may be
        // optimized away; inspect IEEE-754 exponent bits instead.
        for (float v : left) if (!finite(v)) return false;
        for (float v : right) if (!finite(v)) return false;
        std::ofstream file(filename, std::ios::binary | std::ios::trunc);
        if (!file) return false;
        const auto data_bytes = static_cast<std::uint32_t>(total_frames * 6ULL);
        file.write("RIFF", 4); write_u32(file, 36U + data_bytes);
        file.write("WAVEfmt ", 8); write_u32(file, 16);
        write_u16(file, 1); write_u16(file, 2); // PCM, stereo
        write_u32(file, sample_rate); write_u32(file, sample_rate * 6U);
        write_u16(file, 6); write_u16(file, 24);
        file.write("data", 4); write_u32(file, data_bytes);

        std::uint64_t hash = 14695981039346656037ULL;
        const bool mono = right.empty();
        std::array<char, 4096 * 6> buffer{}; // bounded output batch, offline only
        std::size_t used = 0;
        for (std::uint64_t frame = 0; frame < total_frames; ++frame) {
            const bool active = frame >= offset_frames &&
                frame - offset_frames < left.size();
            const auto index = static_cast<std::size_t>(frame - (active ? offset_frames : 0));
            const float l = active ? left[index] : 0.f;
            const float r = active ? (mono ? l : right[index]) : 0.f;
            write_pcm24(buffer.data() + used, l, hash);
            write_pcm24(buffer.data() + used + 3, r, hash);
            used += 6;
            if (used == buffer.size()) {
                file.write(buffer.data(), static_cast<std::streamsize>(used));
                used = 0;
            }
        }
        if (used) file.write(buffer.data(), static_cast<std::streamsize>(used));
        file.flush();
        if (!file.good()) return false;
        if (out_data_hash) *out_data_hash = hash;
        return true;
    }

private:
    static bool finite(float x) noexcept {
        return (std::bit_cast<std::uint32_t>(x) & 0x7f800000u) != 0x7f800000u;
    }
    static void write_u16(std::ofstream& f, std::uint16_t x) {
        const char b[] = {static_cast<char>(x), static_cast<char>(x >> 8)};
        f.write(b, 2);
    }
    static void write_u32(std::ofstream& f, std::uint32_t x) {
        const char b[] = {static_cast<char>(x), static_cast<char>(x >> 8),
                          static_cast<char>(x >> 16), static_cast<char>(x >> 24)};
        f.write(b, 4);
    }
    static void write_pcm24(char* dest, float sample, std::uint64_t& hash) {
        const float limited = std::clamp(sample, -1.f, 1.f);
        const auto pcm = static_cast<std::int32_t>(limited * 8388607.f);
        dest[0] = static_cast<char>(pcm);
        dest[1] = static_cast<char>(pcm >> 8);
        dest[2] = static_cast<char>(pcm >> 16);
        for (unsigned i = 0; i < 3; ++i) {
            hash ^= static_cast<unsigned char>(dest[i]);
            hash *= 1099511628211ULL;
        }
    }
};
} // namespace monkeys_ear
