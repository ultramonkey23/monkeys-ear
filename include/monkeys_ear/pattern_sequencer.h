#pragma once

// Zero-allocation, lock-free, real-time playable Pattern Sequencer built directly
// on top of the canonical PatternProgram & PatternPlayer transport infrastructure.
// Drives MonkeysEarEngine MIDI note_on / note_off with dedicated note_id namespace
// (>= 10000) so live hardware keyboard notes remain isolated and uninterrupted.
#include "monkeys_ear/pattern_transport.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace monkeys_ear {

struct PatternSequencerSettings {
    bool enabled = false;
    int pattern_index = 0;         // 0..7 built-in musical patterns
    int loop_steps = 16;           // 1..32 (odd meters 5/7/11/12/16/32 supported)
    int division = 3;              // 0=1/4, 1=1/8, 2=1/8T, 3=1/16, 4=1/16T, 5=1/32
    float quantize_strength = 1.0f;// 0..1 non-destructive pull toward step grid
    float swing = 0.0f;            // 0..1 odd-step groove swing
    float gate = 0.65f;            // 0.1..1.0 note duration fraction of step
    int transpose_semi = 0;        // -12..+12 semitones
    double bpm = 120.0;
    double sample_rate = 48000.0;

    bool operator==(const PatternSequencerSettings& o) const noexcept {
        return enabled == o.enabled
            && pattern_index == o.pattern_index
            && loop_steps == o.loop_steps
            && division == o.division
            && std::abs(quantize_strength - o.quantize_strength) < 1e-5f
            && std::abs(swing - o.swing) < 1e-5f
            && std::abs(gate - o.gate) < 1e-5f
            && transpose_semi == o.transpose_semi
            && std::abs(bpm - o.bpm) < 1e-4
            && std::abs(sample_rate - o.sample_rate) < 0.5;
    }
};

constexpr int kSequencerNoteIdBase = 10000;

constexpr std::array<const char*, 8> kPatternPresetNames{{
    "Feral 7/8 Riff",
    "Swung Boom-Bap Sub",
    "Abyssal 5/4 Pulse",
    "Glass Cathedral Arp",
    "Acid 16th Drive",
    "Triplet Poly Weave",
    "Sub Pedal & Fifth",
    "Chrono Chord Stabs"
}};

constexpr std::array<const char*, 6> kPatternDivisionNames{{
    "1/4", "1/8", "1/8 Triplet", "1/16", "1/16 Triplet", "1/32"
}};

class EnginePatternSequencer final {
public:
    void prepare(double sample_rate) noexcept {
        settings_.sample_rate = (pattern_finite(sample_rate) && sample_rate >= 8000.0) ? sample_rate : 48000.0;
        rebuild_if_needed(true);
    }

    template <class Engine>
    void reset(Engine& engine) noexcept {
        release_sequencer_notes(engine);
        frame_ = 0;
        next_ = 0;
        current_step_ = 0;
    }

    template <class Engine>
    void configure(Engine& engine, const PatternSequencerSettings& next) noexcept {
        PatternSequencerSettings clean = next;
        clean.pattern_index = std::clamp(clean.pattern_index, 0, 7);
        clean.loop_steps = std::clamp(clean.loop_steps, 1, 32);
        clean.division = std::clamp(clean.division, 0, 5);
        clean.quantize_strength = std::clamp(clean.quantize_strength, 0.0f, 1.0f);
        clean.swing = std::clamp(clean.swing, 0.0f, 1.0f);
        clean.gate = std::clamp(clean.gate, 0.10f, 1.0f);
        clean.transpose_semi = std::clamp(clean.transpose_semi, -12, 12);
        clean.bpm = (pattern_finite(clean.bpm) && clean.bpm >= 20.0 && clean.bpm <= 400.0) ? clean.bpm : 120.0;
        clean.sample_rate = (pattern_finite(clean.sample_rate) && clean.sample_rate >= 8000.0) ? clean.sample_rate : 48000.0;

        if (clean == settings_ && compiled_valid_) {
            return;
        }
        const bool was_enabled = settings_.enabled;
        const bool pattern_changed = (clean.pattern_index != settings_.pattern_index ||
                                      clean.loop_steps != settings_.loop_steps ||
                                      clean.transpose_semi != settings_.transpose_semi);
        if ((was_enabled && !clean.enabled) || pattern_changed) {
            release_sequencer_notes(engine);
            frame_ = 0;
            next_ = 0;
        }
        settings_ = clean;
        rebuild_if_needed(true);
    }

    // Aligns loop phase deterministically when DAW transport is playing and sync is valid.
    void sync_to_host_quarters(double project_quarters, bool beat_valid, bool playing) noexcept {
        if (!settings_.enabled || !compiled_valid_ || !beat_valid || !playing ||
            !pattern_finite(project_quarters) || program_.loop_frames == 0) {
            return;
        }
        const double ticks_per_step = step_ticks();
        const double loop_quarters = (ticks_per_step * static_cast<double>(settings_.loop_steps)) / 960.0;
        if (loop_quarters <= 1e-6) return;
        double mod_q = std::fmod(project_quarters, loop_quarters);
        if (mod_q < 0.0) mod_q += loop_quarters;
        const std::uint64_t target_frame =
            static_cast<std::uint64_t>(std::llround((mod_q / loop_quarters) * static_cast<double>(program_.loop_frames)))
            % program_.loop_frames;
        // Only jump if host phase drifted by > 64 samples (avoids continuous resets on continuous audio blocks)
        const std::int64_t diff = static_cast<std::int64_t>(target_frame) - static_cast<std::int64_t>(frame_);
        const std::int64_t wrap_diff = std::min(std::abs(diff),
            static_cast<std::int64_t>(program_.loop_frames) - std::abs(diff));
        if (wrap_diff > 64) {
            frame_ = target_frame;
            next_ = 0;
            while (next_ < program_.event_count && program_.events[next_].frame < frame_) {
                ++next_;
            }
        }
    }

    template <class Engine, class RenderChunk>
    void process(Engine& engine, size_t total_samples, RenderChunk&& render_chunk) noexcept {
        if (!settings_.enabled || !compiled_valid_ || program_.loop_frames == 0 || program_.event_count == 0) {
            render_chunk(0, total_samples);
            return;
        }
        size_t done = 0;
        while (done < total_samples) {
            if (frame_ >= program_.loop_frames) {
                frame_ = 0;
                next_ = 0;
            }
            const std::uint64_t next_frame =
                (next_ < program_.event_count) ? program_.events[next_].frame : program_.loop_frames;
            if (next_ < program_.event_count && next_frame <= frame_) {
                const auto& ev = program_.events[next_++];
                const int nid = kSequencerNoteIdBase + ev.id;
                if (ev.note_on) {
                    note_active_[static_cast<size_t>(std::clamp(ev.id - 1, 0, kMaxNotes - 1))] = {
                        true, ev.key, ev.channel, nid
                    };
                    engine.handle_midi_note_on(ev.key, static_cast<float>(ev.velocity) / 127.0f,
                                               ev.channel, nid);
                } else {
                    if (ev.id >= 1 && ev.id <= kMaxNotes) {
                        note_active_[static_cast<size_t>(ev.id - 1)].active = false;
                    }
                    engine.handle_midi_note_off(ev.key, ev.channel, nid);
                }
                continue;
            }
            const size_t chunk = static_cast<size_t>(std::min<std::uint64_t>(
                static_cast<std::uint64_t>(total_samples - done), next_frame - frame_));
            render_chunk(done, chunk);
            done += chunk;
            frame_ += chunk;
        }
        const double frac = static_cast<double>(frame_) / static_cast<double>(std::max<std::uint64_t>(1, program_.loop_frames));
        current_step_ = std::clamp(static_cast<int>(frac * settings_.loop_steps), 0, settings_.loop_steps - 1);
    }

    int current_step() const noexcept { return current_step_; }
    const PatternSequencerSettings& settings() const noexcept { return settings_; }
    const PatternProgram& program() const noexcept { return program_; }

private:
    static constexpr int kMaxNotes = 128;
    struct ActiveSlot {
        bool active = false;
        std::uint8_t key = 60;
        std::uint8_t channel = 0;
        int note_id = 0;
    };

    double step_ticks() const noexcept {
        static constexpr std::array<double, 6> kDivTicks{{960.0, 480.0, 320.0, 240.0, 160.0, 120.0}};
        return kDivTicks[static_cast<size_t>(std::clamp(settings_.division, 0, 5))];
    }

    template <class Engine>
    void release_sequencer_notes(Engine& engine) noexcept {
        for (auto& slot : note_active_) {
            if (slot.active) {
                engine.handle_midi_note_off(slot.key, slot.channel, slot.note_id);
                slot.active = false;
            }
        }
    }

    void rebuild_if_needed(bool force) noexcept {
        if (!force && compiled_valid_) return;
        const std::uint32_t s_ticks = static_cast<std::uint32_t>(std::lround(step_ticks()));
        const std::uint32_t steps = static_cast<std::uint32_t>(std::clamp(settings_.loop_steps, 1, 32));
        const std::uint32_t loop_ticks = s_ticks * steps;

        std::array<PatternNote, kMaxNotes> notes{};
        std::size_t count = 0;
        auto push_note = [&](int step, int semi_offset, std::uint8_t vel, std::int32_t nudge_frac_pct, float len_mult = 1.0f) {
            if (step < 0 || static_cast<std::uint32_t>(step) >= steps || count >= kMaxNotes) return;
            const int midi = std::clamp(48 + settings_.transpose_semi + semi_offset, 24, 108);
            const std::int64_t ideal_tick = static_cast<std::int64_t>(step) * static_cast<std::int64_t>(s_ticks);
            // Expressive raw humanization offset pulled smoothly toward grid by PatternTiming::strength.
            const std::int64_t raw_off = (static_cast<std::int64_t>(nudge_frac_pct) * static_cast<std::int64_t>(s_ticks)) / 100;
            const std::int64_t raw_tick = std::clamp<std::int64_t>(
                ideal_tick + raw_off, 0, static_cast<std::int64_t>(loop_ticks) - 1);
            const std::uint32_t len = std::clamp<std::uint32_t>(
                static_cast<std::uint32_t>(std::lround(static_cast<double>(s_ticks) * settings_.gate * len_mult)),
                1u, loop_ticks);
            PatternNote n{};
            n.id = static_cast<int>(count + 1);
            n.raw_tick = raw_tick;
            n.length_ticks = len;
            n.nudge_ticks = 0;
            n.key = static_cast<std::uint8_t>(midi);
            n.velocity = std::clamp<std::uint8_t>(vel, 1, 127);
            n.channel = 0;
            notes[count++] = n;
        };

        switch (settings_.pattern_index) {
            case 0: { // Feral 7/8 Riff (odd-meter chromatic/phrygian drive)
                static constexpr int seq[16] = {0, 0, 12, 1, 0, 7, 10, 0, 1, 13, 7, 0, 3, 1, 0, -2};
                static constexpr int vels[16] = {120, 88, 110, 96, 118, 92, 104, 122, 95, 112, 98, 90, 115, 100, 88, 108};
                for (int s = 0; s < 32; ++s) {
                    if (s % 2 == 0 || (s % 7 == 3) || (s % 7 == 5)) {
                        push_note(s, seq[s % 16] - 12, static_cast<std::uint8_t>(vels[s % 16]), (s % 2 ? 12 : -6));
                    }
                }
                break;
            }
            case 1: { // Swung Boom-Bap Sub (pocket hits with laid-back drag)
                static constexpr int steps_on[8] = {0, 3, 6, 8, 10, 11, 14, 15};
                static constexpr int offs[8] = {-12, -12, -9, -12, -5, -7, -2, -9};
                static constexpr int vels[8] = {124, 96, 110, 118, 86, 106, 94, 78};
                for (int k = 0; k < 16; ++k) {
                    const int s = (k < 8) ? steps_on[k] : (steps_on[k - 8] + 16);
                    push_note(s, offs[k % 8], static_cast<std::uint8_t>(vels[k % 8]), 18, (k % 4 == 0) ? 1.5f : 0.9f);
                }
                break;
            }
            case 2: { // Abyssal 5/4 Pulse (horror tension ostinato)
                static constexpr int mel[10] = {0, 7, 8, 7, 1, 0, 12, 8, 7, 6};
                for (int s = 0; s < 32; ++s) {
                    push_note(s, mel[s % 10], static_cast<std::uint8_t>(s % 5 == 0 ? 118 : 88), (s % 3 == 1 ? -10 : 8));
                }
                break;
            }
            case 3: { // Glass Cathedral Arp (9th / sus2 shimmer)
                static constexpr int arp[8] = {0, 7, 14, 19, 15, 19, 24, 14};
                for (int s = 0; s < 32; ++s) {
                    push_note(s, arp[s % 8], static_cast<std::uint8_t>(92 + (s % 4) * 8), 0, 1.15f);
                }
                break;
            }
            case 4: { // Acid 16th Drive
                static constexpr int notes16[16] = {-12, 0, -12, 0, 3, 0, 12, 0, -12, -12, 7, 10, 12, 10, 7, 3};
                for (int s = 0; s < 32; ++s) {
                    if (s % 5 != 4) {
                        push_note(s, notes16[s % 16], static_cast<std::uint8_t>((s % 4 == 0) ? 124 : 84), (s % 2 ? 10 : -4));
                    }
                }
                break;
            }
            case 5: { // Triplet Poly Weave
                static constexpr int weave[12] = {0, 5, 10, 12, 7, 5, 0, 3, 7, 14, 12, 7};
                for (int s = 0; s < 32; ++s) {
                    push_note(s, weave[s % 12], static_cast<std::uint8_t>(s % 3 == 0 ? 115 : 85), 8);
                }
                break;
            }
            case 6: { // Sub Pedal & Fifth
                for (int s = 0; s < 32; s += 2) {
                    push_note(s, -12, 118, 0, 1.4f);
                    if (s + 1 < static_cast<int>(steps) && (s % 4 == 2)) {
                        push_note(s + 1, 7, 94, 14, 0.8f);
                    }
                }
                break;
            }
            default: { // 7: Chrono Chord Stabs (3-voice polyphonic)
                static constexpr int chords[4][3] = {{0, 3, 7}, {-2, 2, 7}, {-4, 0, 5}, {-5, 2, 7}};
                for (int s = 0; s < 32; s += 4) {
                    const int c = (s / 4) % 4;
                    for (int v = 0; v < 3; ++v) {
                        push_note(s, chords[c][v], 102, (v * 6) - 6, 2.2f);
                    }
                }
                break;
            }
        }

        PatternTiming timing{};
        timing.ticks_per_quarter = 960;
        timing.loop_ticks = loop_ticks;
        timing.bpm = settings_.bpm;
        timing.sample_rate = settings_.sample_rate;
        timing.grid_ticks = s_ticks;
        timing.strength = settings_.quantize_strength;
        timing.swing = settings_.swing;

        compiled_valid_ = PatternProgram::compile(program_, notes.data(), count, timing);
        if (program_.loop_frames > 0) {
            frame_ %= program_.loop_frames;
        } else {
            frame_ = 0;
        }
        next_ = 0;
        while (next_ < program_.event_count && program_.events[next_].frame < frame_) {
            ++next_;
        }
    }

    PatternSequencerSettings settings_{};
    PatternProgram program_{};
    bool compiled_valid_ = false;
    std::uint64_t frame_ = 0;
    std::size_t next_ = 0;
    int current_step_ = 0;
    std::array<ActiveSlot, kMaxNotes> note_active_{};
};

} // namespace monkeys_ear
