#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace monkeys_ear {

class WavWriter {
public:
    static bool write_wav_24bit(
        const std::string& filepath,
        const std::vector<float>& left_channel,
        const std::vector<float>& right_channel,
        uint32_t sample_rate
    ) {
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;

        uint32_t num_samples = static_cast<uint32_t>(left_channel.size());
        uint16_t num_channels = 2;
        uint16_t bits_per_sample = 24;
        uint32_t byte_rate = sample_rate * num_channels * (bits_per_sample / 8);
        uint16_t block_align = num_channels * (bits_per_sample / 8);
        uint32_t data_chunk_size = num_samples * block_align;
        uint32_t file_size_minus_8 = 36 + data_chunk_size;

        // RIFF Header
        file.write("RIFF", 4);
        file.write(reinterpret_cast<const char*>(&file_size_minus_8), 4);
        file.write("WAVE", 4);

        // 'fmt ' subchunk
        file.write("fmt ", 4);
        uint32_t fmt_chunk_size = 16;
        file.write(reinterpret_cast<const char*>(&fmt_chunk_size), 4);
        uint16_t audio_format = 1; // PCM
        file.write(reinterpret_cast<const char*>(&audio_format), 2);
        file.write(reinterpret_cast<const char*>(&num_channels), 2);
        file.write(reinterpret_cast<const char*>(&sample_rate), 4);
        file.write(reinterpret_cast<const char*>(&byte_rate), 4);
        file.write(reinterpret_cast<const char*>(&block_align), 2);
        file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);

        // 'data' subchunk
        file.write("data", 4);
        file.write(reinterpret_cast<const char*>(&data_chunk_size), 4);

        // Write 24-bit interleaved PCM samples
        constexpr float SCALE_24BIT = 8388607.0f; // 2^23 - 1
        for (uint32_t i = 0; i < num_samples; ++i) {
            // Left channel
            float sl = std::clamp(left_channel[i], -1.0f, 1.0f);
            int32_t val_l = static_cast<int32_t>(sl * SCALE_24BIT);
            uint8_t b0_l = static_cast<uint8_t>(val_l & 0xFF);
            uint8_t b1_l = static_cast<uint8_t>((val_l >> 8) & 0xFF);
            uint8_t b2_l = static_cast<uint8_t>((val_l >> 16) & 0xFF);
            file.write(reinterpret_cast<const char*>(&b0_l), 1);
            file.write(reinterpret_cast<const char*>(&b1_l), 1);
            file.write(reinterpret_cast<const char*>(&b2_l), 1);

            // Right channel
            float sr = std::clamp(right_channel[i], -1.0f, 1.0f);
            int32_t val_r = static_cast<int32_t>(sr * SCALE_24BIT);
            uint8_t b0_r = static_cast<uint8_t>(val_r & 0xFF);
            uint8_t b1_r = static_cast<uint8_t>((val_r >> 8) & 0xFF);
            uint8_t b2_r = static_cast<uint8_t>((val_r >> 16) & 0xFF);
            file.write(reinterpret_cast<const char*>(&b0_r), 1);
            file.write(reinterpret_cast<const char*>(&b1_r), 1);
            file.write(reinterpret_cast<const char*>(&b2_r), 1);
        }

        return true;
    }
};

} // namespace monkeys_ear
