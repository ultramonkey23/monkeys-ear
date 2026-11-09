#pragma once

// Production musical-time -> audio-frame bridge for Monkey's Ear.
// Edit/compile off the audio thread. Immutable PatternProgram + PatternPlayer
// drive the existing MonkeysEarEngine MIDI/audio interface without allocation.
#include <algorithm>
#include <bit>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace monkeys_ear {

constexpr std::size_t kPatternNoteCapacity = 512;
constexpr std::size_t kPatternEventCapacity = 2 * kPatternNoteCapacity;

struct PatternNote {
    int id = 0;                 // Stable positive identity, distinct within pattern.
    std::int64_t raw_tick = 0;  // Original, unquantized onset; never overwritten.
    std::uint32_t length_ticks = 1;
    std::int32_t nudge_ticks = 0; // Deliberate offset AFTER quantization.
    std::uint8_t key = 60;
    std::uint8_t velocity = 100;
    std::uint8_t channel = 0;
};

struct PatternTiming {
    std::uint32_t ticks_per_quarter = 960;
    std::uint32_t loop_ticks = 3840;
    double bpm = 120.0;
    double sample_rate = 48000.0;
    std::uint32_t grid_ticks = 0; // 0 means do not quantize; e.g. 480 = eighth.
    double strength = 1.0;       // 0 = original timing; 1 = full grid.
    double swing = 0.0;          // 0..1 moves odd grid slots by up to 1/2 grid.
};

struct PatternMidiEvent {
    std::uint64_t frame = 0; // Position in one compiled loop, [0, loop_frames).
    int id = 0;
    std::uint8_t key = 0;
    std::uint8_t velocity = 0;
    std::uint8_t channel = 0;
    bool note_on = false;
};

// Bit-level finite check remains effective under the project-wide -ffast-math.
inline bool pattern_finite(double value) noexcept {
    return (std::bit_cast<std::uint64_t>(value) & 0x7ff0000000000000ULL)
           != 0x7ff0000000000000ULL;
}

class PatternProgram final {
public:
    std::array<PatternMidiEvent, kPatternEventCapacity> events{};
    std::size_t event_count = 0;
    std::uint64_t loop_frames = 0;

    // Transactional compile: failed/invalid input does not mutate current program.
    // Caller must not change a published program while its player is rendering.
    static bool compile(PatternProgram& destination, const PatternNote* notes,
                        std::size_t count, const PatternTiming& settings) {
        if (count > kPatternNoteCapacity || (count && notes == nullptr) ||
            settings.ticks_per_quarter == 0 || settings.loop_ticks == 0 ||
            !pattern_finite(settings.bpm) || settings.bpm <= 0.0 ||
            !pattern_finite(settings.sample_rate) || settings.sample_rate <= 0.0 ||
            !pattern_finite(settings.strength) || settings.strength < 0.0 ||
            settings.strength > 1.0 || !pattern_finite(settings.swing) ||
            settings.swing < 0.0 || settings.swing > 1.0 ||
            settings.grid_ticks > settings.loop_ticks) return false;
        const long double frames_per_tick =
            static_cast<long double>(settings.sample_rate) * 60.0L /
            (static_cast<long double>(settings.bpm) * settings.ticks_per_quarter);
        const long double frames = frames_per_tick * settings.loop_ticks;
        if (!std::isfinite(frames) || frames < 1.0L ||
            frames >= static_cast<long double>(std::uint64_t{1} << 52)) return false;

        PatternProgram built;
        built.loop_frames = static_cast<std::uint64_t>(std::llround(frames));
        for (std::size_t i = 0; i < count; ++i) {
            const auto& note = notes[i];
            if (note.id <= 0 || note.raw_tick < 0 ||
                static_cast<std::uint64_t>(note.raw_tick) >= settings.loop_ticks ||
                note.length_ticks == 0 || note.length_ticks > settings.loop_ticks ||
                note.key > 127 || note.velocity == 0 || note.velocity > 127 ||
                note.channel > 15) return false;
            for (std::size_t j = 0; j < i; ++j)
                if (notes[j].id == note.id) return false;

            const long double raw = static_cast<long double>(note.raw_tick);
            long double target = raw;
            if (settings.grid_ticks != 0) {
                const long double grid = settings.grid_ticks;
                const long double slot = std::floor(raw / grid + 0.5L);
                target = slot * grid;
                if (std::fmod(slot, 2.0L) == 1.0L)
                    target += static_cast<long double>(settings.swing) * grid * 0.5L;
            }
            const long double shifted = raw + (target - raw) * settings.strength +
                                        note.nudge_ticks;
            long double wrapped = std::fmod(shifted,
                                            static_cast<long double>(settings.loop_ticks));
            if (wrapped < 0.0L) wrapped += settings.loop_ticks;
            auto on = static_cast<std::uint64_t>(std::llround(wrapped * frames_per_tick));
            on %= built.loop_frames;
            const auto dur = std::max<std::uint64_t>(
                1, static_cast<std::uint64_t>(std::llround(
                    static_cast<long double>(note.length_ticks) * frames_per_tick)));
            const std::uint64_t off = (on + dur) % built.loop_frames;
            built.events[built.event_count++] =
                {on, note.id, note.key, note.velocity, note.channel, true};
            built.events[built.event_count++] =
                {off, note.id, note.key, note.velocity, note.channel, false};
        }
        std::sort(built.events.begin(), built.events.begin() + built.event_count,
                  [](const PatternMidiEvent& a, const PatternMidiEvent& b) {
            if (a.frame != b.frame) return a.frame < b.frame;
            if (a.note_on != b.note_on) return !a.note_on; // release before retrigger
            return a.id < b.id;
        });
        destination = built;
        return true;
    }
};

class PatternPlayer final {
public:
    // Bind and start/stop only while outside the active audio callback.
    void bind(const PatternProgram& program) noexcept {
        program_ = &program;
        frame_ = 0;
        next_ = 0;
        running_ = false;
    }
    template <class Engine> void start(Engine& engine) noexcept {
        engine.handle_all_notes_off();
        frame_ = 0;
        next_ = 0;
        running_ = (program_ && program_->loop_frames > 0);
    }
    template <class Engine> void stop(Engine& engine) noexcept {
        engine.handle_all_notes_off();
        running_ = false;
    }
    bool running() const noexcept { return running_; }
    std::uint64_t frame_position() const noexcept { return frame_; }

    // Calls the same audio/MIDI methods as MonkeysEarEngine, splitting blocks at
    // exact compiled event frames. All buffers and events preexist; no heap/locks.
    template <class Engine>
    void process(Engine& engine, const float* in_l, const float* in_r,
                 float* out_l, float* out_r, std::size_t count) noexcept {
        if (!running_ || !program_ || program_->event_count == 0) {
            engine.process_block(in_l, in_r, out_l, out_r, count);
            if (running_ && program_) frame_ = (frame_ + count) % program_->loop_frames;
            return;
        }
        std::size_t done = 0;
        while (done < count) {
            if (frame_ == program_->loop_frames) {
                frame_ = 0;
                next_ = 0;
            }
            const std::uint64_t next_frame =
                next_ < program_->event_count ? program_->events[next_].frame
                                              : program_->loop_frames;
            if (next_ < program_->event_count && next_frame == frame_) {
                const auto& ev = program_->events[next_++];
                if (ev.note_on)
                    engine.handle_midi_note_on(ev.key, ev.velocity / 127.0f,
                                               ev.channel, ev.id);
                else
                    engine.handle_midi_note_off(ev.key, ev.channel, ev.id);
                continue;
            }
            const auto chunk = static_cast<std::size_t>(std::min<std::uint64_t>(
                count - done, next_frame - frame_));
            engine.process_block(in_l ? in_l + done : nullptr,
                                 in_r ? in_r + done : nullptr,
                                 out_l + done, out_r + done, chunk);
            done += chunk;
            frame_ += chunk;
        }
    }
private:
    const PatternProgram* program_ = nullptr; // immutable lifetime owned by caller
    std::uint64_t frame_ = 0;
    std::size_t next_ = 0;
    bool running_ = false;
};

} // namespace monkeys_ear
