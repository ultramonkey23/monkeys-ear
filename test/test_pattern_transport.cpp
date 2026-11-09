#include "monkeys_ear/pattern_transport.h"
#include <array>
#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

#ifdef MONKEYS_EAR_PATTERN_INTEGRATION
#include "monkeys_ear/engine.h"
#endif

using namespace monkeys_ear;

struct FakeEngine {
    struct Msg { std::uint64_t frame; bool on; int note, channel, id; };
    std::array<Msg, 128> messages{};
    std::size_t n = 0;
    std::uint64_t clock = 0;
    int held = 0;
    std::array<bool, 256> active{};
    void handle_all_notes_off() { held = 0; active.fill(false); }
    void handle_midi_note_on(int note, float vel, int channel, int id) {
        assert(vel > 0 && vel <= 1); assert(n < messages.size());
        messages[n++] = {clock, true, note, channel, id};
        if (!active.at(static_cast<std::size_t>(id))) { ++held; active[id] = true; }
    }
    void handle_midi_note_off(int note, int channel, int id) {
        assert(n < messages.size());
        messages[n++] = {clock, false, note, channel, id};
        if (active.at(static_cast<std::size_t>(id))) { --held; active[id] = false; }
    }
    void process_block(const float*, const float*, float* l, float* r,
                       std::size_t frames) {
        for (std::size_t i = 0; i < frames; ++i) l[i] = r[i] = held ? 0.25f : 0.0f;
        clock += frames;
    }
};

static void test_compile_and_quantize() {
    PatternProgram p;
    PatternTiming settings;
    settings.sample_rate = 48000;
    settings.bpm = 120;
    settings.loop_ticks = 3840;
    settings.grid_ticks = 480;
    settings.strength = 0;
    PatternNote notes[] = {{1, 490, 240, 10, 36, 120, 9}};
    assert(PatternProgram::compile(p, notes, 1, settings));
    assert(p.event_count == 2 && p.loop_frames == 96000);
    // Original 490 + deliberate +10 = tick 500, 25 audio frames per tick.
    assert(p.events[0].frame == 12500 && p.events[0].note_on);
    assert(p.events[1].frame == 18500 && !p.events[1].note_on);
    settings.strength = 0.5;
    assert(PatternProgram::compile(p, notes, 1, settings));
    assert(p.events[0].frame == 12375); // halfway from tick 490 toward nearest 480, plus 10
    settings.strength = 1;
    assert(PatternProgram::compile(p, notes, 1, settings));
    assert(p.events[0].frame == 12250);
    notes[0].raw_tick = 480;
    notes[0].nudge_ticks = 0;
    settings.swing = 1;
    assert(PatternProgram::compile(p, notes, 1, settings));
    assert(p.events[0].frame == 18000); // swung offbeat tick 720
}

static void test_transactional_rejection() {
    PatternProgram p;
    PatternTiming s;
    PatternNote notes[] = {{1, 0, 480, 0, 60, 100, 0},
                           {2, 960, 480, 0, 62, 100, 0}};
    assert(PatternProgram::compile(p, notes, 2, s));
    const auto saved = p;
    notes[1].id = 1; // duplicate musical identity
    assert(!PatternProgram::compile(p, notes, 2, s));
    assert(p.event_count == saved.event_count &&
           p.events[0].frame == saved.events[0].frame);
    notes[1].id = 2;
    notes[1].velocity = 0;
    assert(!PatternProgram::compile(p, notes, 2, s));
    notes[1].velocity = 100;
    s.bpm = std::numeric_limits<double>::quiet_NaN();
    assert(!PatternProgram::compile(p, notes, 2, s));
    s.bpm = 120;
    s.grid_ticks = s.loop_ticks + 1;
    assert(!PatternProgram::compile(p, notes, 2, s));
    assert(p.event_count == saved.event_count);
    s.grid_ticks = 0;
    s.bpm = std::numeric_limits<double>::infinity();
    assert(!PatternProgram::compile(p, notes, 2, s));
    s.bpm = 120;
    s.strength = std::numeric_limits<double>::quiet_NaN();
    assert(!PatternProgram::compile(p, notes, 2, s));
    s.strength = 1;
    s.swing = std::numeric_limits<double>::infinity();
    assert(!PatternProgram::compile(p, notes, 2, s));
}

static void test_live_block_boundaries() {
    PatternProgram p;
    PatternTiming s;
    s.sample_rate = 1000; // 960 ticks / quarter -> 500 frames / quarter.
    s.loop_ticks = 960;
    PatternNote notes[] = {{11, 0, 480, 0, 36, 127, 9},
                           {12, 480, 480, 0, 38, 110, 9}};
    assert(PatternProgram::compile(p, notes, 2, s));
    assert(p.loop_frames == 500);
    FakeEngine engine;
    PatternPlayer player;
    player.bind(p);
    player.start(engine);
    std::array<float, 1024> l{}, r{};
    for (std::size_t i = 0; i < 1000; i += 73) {
        const auto amount = std::min<std::size_t>(73, 1000 - i);
        player.process(engine, nullptr, nullptr, l.data() + i, r.data() + i, amount);
    }
    // At tick 0: note 11 on and note 12 off from previous loop (safe no-op).
    // At tick 480: note 11 off and note 12 on. Repeats at 500 frames.
    assert(engine.n == 8);
    assert(engine.messages[0].frame == 0 && !engine.messages[0].on);
    assert(engine.messages[1].frame == 0 && engine.messages[1].on);
    assert(engine.messages[2].frame == 250 && !engine.messages[2].on);
    assert(engine.messages[3].frame == 250 && engine.messages[3].on);
    assert(engine.messages[4].frame == 500 && !engine.messages[4].on);
    assert(engine.messages[5].frame == 500 && engine.messages[5].on);
    assert(engine.messages[6].frame == 750 && !engine.messages[6].on);
    assert(engine.messages[7].frame == 750 && engine.messages[7].on);
    assert(l[1] == 0.25f);
    assert(player.frame_position() == 500); // boundary resets next render
    player.stop(engine);
    assert(!player.running() && engine.held == 0);
}

static void test_loop_wrap_and_source_preservation() {
    PatternProgram p;
    PatternTiming s;
    s.grid_ticks = 480;
    PatternNote note{91, 3800, 200, -10, 60, 95, 2};
    const auto original = note;
    assert(PatternProgram::compile(p, &note, 1, s));
    assert(note.raw_tick == original.raw_tick && note.nudge_ticks == original.nudge_ticks);
    // Quantized tick 3840 becomes 0, deliberate -10 puts note near loop end.
    assert(p.events[1].note_on && p.events[1].frame == 95750);
    assert(!p.events[0].note_on && p.events[0].frame == 4750);
}

static void test_empty_program_and_stop_tails() {
    PatternProgram p;
    PatternTiming s;
    assert(PatternProgram::compile(p, nullptr, 0, s));
    FakeEngine e;
    PatternPlayer player;
    player.bind(p);
    player.start(e);
    std::array<float, 64> l{}, r{};
    player.process(e, nullptr, nullptr, l.data(), r.data(), l.size());
    assert(e.clock == 64 && e.n == 0);
    player.stop(e);
    player.process(e, nullptr, nullptr, l.data(), r.data(), l.size());
    assert(e.clock == 128); // DSP tails still processed when sequencer is off.
}


static void test_block_size_invariance_and_odd_meter() {
    PatternTiming s;
    s.sample_rate = 2400;
    s.bpm = 120;
    s.loop_ticks = 2880; // 3/4 bar at 960 ppq; not a four-beat assumption.
    s.grid_ticks = 240;
    s.strength = 0.6;
    s.swing = 0.4;
    PatternNote notes[] = {
        {1, 10, 240, 0, 36, 120, 0},
        {2, 367, 480, -4, 38, 105, 0},
        {3, 2800, 200, 7, 42, 80, 0}
    };
    PatternProgram p;
    assert(PatternProgram::compile(p, notes, 3, s));
    const auto length = static_cast<std::size_t>(p.loop_frames) * 2;
    assert(length == 7200);
    std::array<float, 7200> one_l{}, one_r{}, split_l{}, split_r{};
    FakeEngine whole, partial;
    PatternPlayer a, b;
    a.bind(p); b.bind(p); a.start(whole); b.start(partial);
    a.process(whole, nullptr, nullptr, one_l.data(), one_r.data(), length);
    for (std::size_t offset = 0; offset < length;) {
        const auto step = std::min<std::size_t>((offset % 131) + 1, length - offset);
        b.process(partial, nullptr, nullptr,
                  split_l.data() + offset, split_r.data() + offset, step);
        offset += step;
    }
    assert(whole.n == partial.n);
    for (std::size_t i = 0; i < whole.n; ++i) {
        assert(whole.messages[i].frame == partial.messages[i].frame);
        assert(whole.messages[i].on == partial.messages[i].on);
        assert(whole.messages[i].id == partial.messages[i].id);
    }
    for (std::size_t i = 0; i < length; ++i)
        assert(one_l[i] == split_l[i] && one_r[i] == split_r[i]);
    a.stop(whole); b.stop(partial);
    assert(whole.held == 0 && partial.held == 0);
}

static void test_input_audio_pointer_offsets() {
    struct ThroughEngine : FakeEngine {
        void process_block(const float* in_l, const float* in_r,
                           float* out_l, float* out_r, std::size_t nframes) {
            assert(in_l && in_r);
            for (std::size_t i = 0; i < nframes; ++i) {
                out_l[i] = in_l[i]; out_r[i] = in_r[i];
            }
            clock += nframes;
        }
    } engine;
    PatternTiming t;
    t.bpm = 120; t.sample_rate = 960; t.loop_ticks = 960;
    PatternNote note{4, 480, 240, 0, 60, 127, 0};
    PatternProgram p;
    assert(PatternProgram::compile(p, &note, 1, t));
    PatternPlayer player;
    player.bind(p); player.start(engine);
    std::array<float, 500> in_l{}, in_r{}, out_l{}, out_r{};
    for (std::size_t i = 0; i < 500; ++i) {
        in_l[i] = static_cast<float>(i) / 500.f;
        in_r[i] = 1.f - in_l[i];
    }
    player.process(engine, in_l.data(), in_r.data(), out_l.data(), out_r.data(), 500);
    assert(out_l == in_l && out_r == in_r);
}

#ifdef MONKEYS_EAR_PATTERN_INTEGRATION
static void test_real_monkeys_ear_audio() {
    MonkeysEarEngine engine;
    engine.init(48000.f, 128);
    engine.load_preset(PresetManager::create_monolith());
    PatternTiming t;
    t.loop_ticks = 960;
    PatternNote notes[] = {{101, 0, 480, 0, 48, 127, 0},
                           {102, 480, 480, 0, 55, 120, 0}};
    PatternProgram p;
    assert(PatternProgram::compile(p, notes, 2, t));
    PatternPlayer player;
    player.bind(p);
    player.start(engine);
    std::array<float, 128> l{}, r{};
    double sum = 0;
    for (int i = 0; i < 240; ++i) {
        player.process(engine, nullptr, nullptr, l.data(), r.data(), l.size());
        for (float x : l) { assert((std::bit_cast<std::uint32_t>(x) & 0x7f800000u) != 0x7f800000u); sum += std::abs(x); }
    }
    assert(sum > 0.1); // Actual existing synthesis/FX path consumed MIDI.
    player.stop(engine);
}
#endif

int main() {
    test_compile_and_quantize();
    test_transactional_rejection();
    test_live_block_boundaries();
    test_loop_wrap_and_source_preservation();
    test_empty_program_and_stop_tails();
    test_block_size_invariance_and_odd_meter();
    test_input_audio_pointer_offsets();
#ifdef MONKEYS_EAR_PATTERN_INTEGRATION
    test_real_monkeys_ear_audio();
#endif
    std::puts("[PASS] Monkey's Ear production pattern/quantize transport");
}
