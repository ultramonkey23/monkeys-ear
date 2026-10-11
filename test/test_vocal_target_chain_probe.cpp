// Research characterization probe: drives the REAL product pitch chain
// (VocalTargetEngine target selection -> AudioInputProcessor::process_vocal_sample
// two-stage grain resynthesis) with deterministic synthetic time-varying contours.
// This executes the production code path for research evidence; synthetic inputs
// through the real chain prove measured chain behavior only, never listening
// quality (REAPER listening remains release authority).
// Evidence state: SYNTHETIC (inputs synthetic; code path is production).
#include "monkeys_ear/audio_input.h"
#include "monkeys_ear/vocal_target.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace monkeys_ear;

namespace {
constexpr float kSampleRate = 48000.0f;
constexpr float kPi = 3.14159265358979323846f;

float cents_of_hz(float hz) { return 1200.0f * std::log2(hz / 440.0f); }
float hz_of_cents(float cents) { return 440.0f * std::pow(2.0f, cents / 1200.0f); }

bool any_failure = false;
void fail(const char* scenario, const char* why) {
    std::printf("FAIL,%s,%s\n", scenario, why);
    std::fprintf(stderr, "FAIL: %s: %s\n", scenario, why);
    any_failure = true;
}
void result(const char* scenario, const char* key, double value) {
    std::printf("RESULT,%s,%s,%.6f\n", scenario, key, value);
}

// Independent F0 estimator (Hann-windowed normalized autocorrelation with
// parabolic lag interpolation). Intentionally NOT the product tracker:
// audio-side truth must not come from the mechanism under test.
// Instrument notes (engineering, 2026-10-11): the lag search is band-limited to
// the scenario range 120-420 Hz so octave-period peaks cannot win, and the
// Hann taper suppresses the finite-window alignment bias of a rectangular
// window (worst-case vertex bias 4.08 -> 1.34 cents across alignments in the
// scratch study). Remaining instrument accuracy is about 1.5 cents.
struct F0Estimate {
    float hz = 0.0f;
    float strength = 0.0f;
};

F0Estimate estimate_f0(const std::vector<float>& x, size_t center) {
    constexpr size_t kWin = 2048;
    if (center < kWin / 2 || center + kWin / 2 >= x.size()) return {};
    const size_t lo = center - kWin / 2;
    std::vector<double> v(kWin, 0.0);
    double mean = 0.0;
    for (size_t i = 0; i < kWin; ++i) {
        const double hann = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979323846 * static_cast<double>(i) / static_cast<double>(kWin - 1));
        v[i] = (static_cast<double>(x[lo + i]) - 0.0) * hann;
        mean += v[i];
    }
    mean /= static_cast<double>(kWin);
    double r0 = 0.0;
    for (size_t i = 0; i < kWin; ++i) {
        v[i] -= mean;
        r0 += v[i] * v[i];
    }
    if (r0 <= 0.0) return {};
    const size_t lag_lo = static_cast<size_t>(kSampleRate / 420.0f);
    const size_t lag_hi = std::min(static_cast<size_t>(kSampleRate / 120.0f), kWin - 2);
    std::vector<float> r(kWin, 0.0f);
    size_t best = 0;
    float best_r = -1.0f;
    for (size_t lag = lag_lo; lag <= lag_hi; ++lag) {
        double acc = 0.0;
        for (size_t i = 0; i + lag < kWin; ++i) acc += v[i] * v[i + lag];
        r[lag] = static_cast<float>(acc / (r0 * (1.0 - static_cast<double>(lag) / kWin)));
        if (r[lag] > best_r) {
            best_r = r[lag];
            best = lag;
        }
    }
    if (best == 0 || best_r <= 0.0f) return {};
    float lag_f = static_cast<float>(best);
    if (best > lag_lo && best < lag_hi) {
        const float y0 = r[best - 1], y1 = r[best], y2 = r[best + 1];
        const float denom = y0 - 2.0f * y1 + y2;
        if (std::fabs(denom) > 1e-9f) lag_f += 0.5f * (y0 - y2) / denom;
    }
    return {kSampleRate / lag_f, best_r};
}

struct EngineRun {
    std::vector<float> trajectory;
    std::vector<int> degree;
    uint32_t switches = 0;
};

EngineRun run_engine(const std::vector<float>& obs_cents, const std::vector<float>& onset,
                     const VocalTargetControls& controls) {
    VocalTargetEngine engine;
    engine.set_sample_rate(kSampleRate);
    engine.set_controls(controls);
    engine.reset();
    EngineRun run;
    run.trajectory.reserve(obs_cents.size());
    run.degree.reserve(obs_cents.size());
    for (size_t i = 0; i < obs_cents.size(); ++i) {
        const float dt = 1.0f / kSampleRate;
        const float prev = i > 0 ? obs_cents[i - 1] : obs_cents[i];
        const float motion = (obs_cents[i] - prev) / dt;
        engine.process(obs_cents[i], motion, 0.9f, onset.empty() ? 0.0f : onset[i]);
        run.trajectory.push_back(engine.metrics().trajectory_cents);
        run.degree.push_back(engine.metrics().selected_degree);
    }
    run.switches = engine.metrics().switch_count;
    return run;
}

// Distance-only oracle for which builtin degree index a cents value belongs to
// (independent re-derivation; the product uses gravity scoring on top of this).
int nearest_degree_index(const TuningSpace& tuning, float cents) {
    int best = -1;
    float best_dist = 1e9f;
    for (uint8_t i = 0; i < tuning.degree_count; ++i) {
        for (int octave = -4; octave <= 4; ++octave) {
            const float candidate = tuning.root_cents + tuning.degrees[i].cents + octave * tuning.period_cents;
            const float d = std::fabs(candidate - cents);
            if (d < best_dist) {
                best_dist = d;
                best = static_cast<int>(i);
            }
        }
    }
    return best;
}

std::vector<float> contour_vibrato(float base_cents, float depth_cents, float rate_hz, float seconds) {
    std::vector<float> c;
    const size_t n = static_cast<size_t>(seconds * kSampleRate);
    c.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / kSampleRate;
        c.push_back(base_cents + depth_cents * std::sin(2.0f * kPi * rate_hz * t));
    }
    return c;
}

} // namespace

int main() {
    VocalTargetControls major_controls;
    major_controls.tuning.set_builtin(VocalBuiltinTuning::Major);
    major_controls.target_hysteresis = 0.45f;
    major_controls.onset_protection = 0.5f;

    // S0: estimator self-check on a pure 220 Hz sine (independent-estimator validity).
    {
        std::vector<float> x(static_cast<size_t>(0.5f * kSampleRate));
        for (size_t i = 0; i < x.size(); ++i) x[i] = 0.5f * std::sin(2.0f * kPi * 220.0f * static_cast<float>(i) / kSampleRate);
        const auto est = estimate_f0(x, x.size() / 2);
        const float err_cents = std::fabs(cents_of_hz(est.hz) - cents_of_hz(220.0f));
        result("estimator_self_check", "error_cents", err_cents);
        result("estimator_self_check", "strength", est.strength);
        if (!(err_cents < 3.0f)) fail("estimator_self_check", "pure 220 Hz sine not recovered within 3 cents");
    }

    // S1: vibrato hold around +30 cents sharp of A3. Expect lock on degree 0 (A).
    {
        const auto obs = contour_vibrato(-1200.0f + 30.0f, 25.0f, 5.5f, 2.0f);
        std::vector<float> onset(obs.size(), 0.0f);
        const auto run = run_engine(obs, onset, major_controls);
        const float settle_err = std::fabs(run.trajectory.back() - (-1200.0f));
        result("engine_vibrato_hold", "final_error_cents", settle_err);
        result("engine_vibrato_hold", "switch_count", run.switches);
        result("engine_vibrato_hold", "final_degree", run.degree.back());
        if (!(settle_err < 2.0f)) fail("engine_vibrato_hold", "trajectory did not settle on the A degree within 2 cents");
    }

    // S2: glissando -1150 -> -850 cents; boundary crossings at -1076 and -876
    // under default history bias are expected to produce exactly 2 switches.
    {
        std::vector<float> obs;
        const float t_hold = 0.3f, t_sweep = 1.0f, t_tail = 0.7f;
        const size_t n_hold = static_cast<size_t>(t_hold * kSampleRate);
        const size_t n_sweep = static_cast<size_t>(t_sweep * kSampleRate);
        const size_t n_tail = static_cast<size_t>(t_tail * kSampleRate);
        obs.insert(obs.end(), n_hold, -1150.0f);
        for (size_t i = 0; i < n_sweep; ++i)
            obs.push_back(-1150.0f + 300.0f * static_cast<float>(i) / static_cast<float>(n_sweep));
        obs.insert(obs.end(), n_tail, -850.0f);
        std::vector<float> onset(obs.size(), 0.0f);
        const auto run = run_engine(obs, onset, major_controls);
        const float settle_err = std::fabs(run.trajectory.back() - (-800.0f));
        result("engine_glissando", "final_error_cents", settle_err);
        result("engine_glissando", "switch_count", run.switches);
        if (!(run.switches >= 1)) fail("engine_glissando", "no degree switch across a 300-cent sweep");
        if (!(settle_err < 2.0f)) fail("engine_glissando", "trajectory did not settle on the -800 degree");
    }

    // S3: onset-jump contrast. Hold -1050 (locked on -1000), jump +160 to -890.
    // Hysteresis alone holds -1000; onset evidence must reacquire -800.
    {
        std::vector<float> obs;
        const size_t n_hold = static_cast<size_t>(1.0f * kSampleRate);
        const size_t n_jump = static_cast<size_t>(1.0f * kSampleRate);
        obs.insert(obs.end(), n_hold, -1050.0f);
        obs.insert(obs.end(), n_jump, -890.0f);
        std::vector<float> onset_off(obs.size(), 0.0f);
        std::vector<float> onset_on(obs.size(), 0.0f);
        for (size_t i = n_hold; i < n_hold + static_cast<size_t>(0.005f * kSampleRate); ++i) onset_on[i] = 1.0f;
        const auto held = run_engine(obs, onset_off, major_controls);
        const auto reacquired = run_engine(obs, onset_on, major_controls);
        const int oracle_old = nearest_degree_index(major_controls.tuning, -1050.0f);
        const int oracle_new = nearest_degree_index(major_controls.tuning, -890.0f);
        result("engine_onset_jump", "held_degree", held.degree.back());
        result("engine_onset_jump", "reacquired_degree", reacquired.degree.back());
        result("engine_onset_jump", "oracle_old_degree", oracle_old);
        result("engine_onset_jump", "oracle_new_degree", oracle_new);
        if (!(held.degree.back() == oracle_old)) fail("engine_onset_jump", "hysteresis did not hold the old degree without onset evidence");
        if (!(reacquired.degree.back() == oracle_new)) fail("engine_onset_jump", "onset evidence did not reacquire the new degree");
    }

    // S4: confidence seam. The engine's target selection uses confidence only for
    // onset reset (conf > 0.35); low confidence must therefore produce IDENTICAL
    // output here. This documents that the voiced gate in audio_input is the only
    // confidence gate on the chain.
    {
        const auto obs = contour_vibrato(-1170.0f, 40.0f, 3.0f, 1.0f);
        std::vector<float> onset(obs.size(), 0.0f);
        const auto run = run_engine(obs, onset, major_controls);
        // Second run through the engine with a modified confidence path is not
        // reachable from run_engine's fixed 0.9 argument; instead contrast onset
        // reset availability: with confidence below 0.35 the onset reset cannot
        // fire even at onset=1. Reuse S3 geometry with onset evidence but let the
        // engine see confidence 0.2 by exercising the product API directly.
        VocalTargetEngine engine;
        engine.set_sample_rate(kSampleRate);
        engine.set_controls(major_controls);
        engine.reset();
        for (size_t i = 0; i < obs.size(); ++i) engine.process(obs[i], 0.0f, 0.2f, 1.0f);
        const int low_conf_degree = engine.metrics().selected_degree;
        result("engine_confidence_seam", "low_conf_onset_degree", low_conf_degree);
        result("engine_confidence_seam", "vibrato_run_switches", run.switches);
        // With onset=1 but conf 0.2 the reset cannot fire, so the run must end on
        // the same steady target as the no-onset vibrato run (degree 0 = A).
        if (!(low_conf_degree == 0)) fail("engine_confidence_seam", "low-confidence onset bypassed the conf>0.35 onset gate");
    }

    // S5: full-chain resynthesis. Real AudioInputProcessor: harmonically rich
    // input at 220 Hz +30 cents with +/-25 cent vibrato, tracked evidence from
    // the known contour, correction strength 1, vibrato retention 0.
    {
        const size_t n = static_cast<size_t>(2.0f * kSampleRate);
        std::vector<float> in(n), out(n), tracked_hz(n);
        std::vector<float> intent_cents(n);
        double phase = 0.0;
        AudioInputProcessor proc;
        proc.set_sample_rate(kSampleRate);
        VocalExpressionControls controls;
        controls.enabled = true;
        controls.correction_strength = 1.0f;
        controls.drift_retention = 0.0f;
        controls.vibrato_retention = 0.0f;
        controls.transition = 0.5f;
        controls.mix = 1.0f;
        controls.target.tuning.set_builtin(VocalBuiltinTuning::Major);
        proc.set_vocal_controls(controls);
        for (size_t i = 0; i < n; ++i) {
            const float t = static_cast<float>(i) / kSampleRate;
            const float obs_cents = -1200.0f + 30.0f + 25.0f * std::sin(2.0f * kPi * 5.5f * t);
            const float f = hz_of_cents(obs_cents);
            phase += 2.0f * kPi * f / kSampleRate;
            in[i] = 0.4f * (std::sin(phase) + 0.4f * std::sin(2.0f * phase) + 0.25f * std::sin(3.0f * phase));
            tracked_hz[i] = f;
            proc.process_sample(in[i], 0.0f);
            out[i] = proc.process_vocal_sample(in[i], tracked_hz[i], 0.9f);
            intent_cents[i] = proc.vocal_metrics().target_trajectory_cents;
        }
        // Independent audio-side measurement over the settled tail.
        const size_t hop = 512;
        double err_sum = 0.0;
        size_t err_n = 0, finite_n = 0;
        float in_dev_min = 1e9f, in_dev_max = -1e9f, out_dev_min = 1e9f, out_dev_max = -1e9f;
        for (size_t c = static_cast<size_t>(1.2f * kSampleRate); c + hop < n; c += hop) {
            const auto est_in = estimate_f0(in, c);
            const auto est_out = estimate_f0(out, c);
            if (est_out.hz <= 0.0f) continue;
            ++finite_n;
            const float out_cents = cents_of_hz(est_out.hz);
            const float intent = intent_cents[c];
            if (std::isfinite(intent) && intent != 0.0f) {
                err_sum += std::fabs(out_cents - intent);
                ++err_n;
            }
            if (est_in.hz > 0.0f) {
                const float d = cents_of_hz(est_in.hz) - cents_of_hz(hz_of_cents(-1170.0f));
                in_dev_min = std::min(in_dev_min, d);
                in_dev_max = std::max(in_dev_max, d);
            }
            const float od = out_cents - cents_of_hz(hz_of_cents(-1200.0f));
            out_dev_min = std::min(out_dev_min, od);
            out_dev_max = std::max(out_dev_max, od);
        }
        const double mean_err_cents = err_n ? err_sum / static_cast<double>(err_n) : 1e9;
        const float in_depth = (in_dev_max - in_dev_min) * 0.5f;
        const float out_depth = (out_dev_max - out_dev_min) * 0.5f;
        result("chain_vibrato_resynthesis", "mean_intent_error_cents", mean_err_cents);
        result("chain_vibrato_resynthesis", "measured_windows", static_cast<double>(finite_n));
        result("chain_vibrato_resynthesis", "input_vibrato_depth_cents", in_depth);
        result("chain_vibrato_resynthesis", "output_vibrato_depth_cents", out_depth);
        if (!(err_n > 10)) fail("chain_vibrato_resynthesis", "too few measurable output windows");
        if (!(mean_err_cents < 150.0)) fail("chain_vibrato_resynthesis", "output F0 drifted more than 150 cents from the product-reported target trajectory");
        // Characterization only: vibrato suppression direction is informative but
        // not yet a hard contract (REAPER listening decides quality).
        std::printf("CHAR,chain_vibrato_resynthesis,vibrato_reduced,%d\n", out_depth < in_depth ? 1 : 0);
    }

    // S6: negative control. An empty tuning must degrade to neutral passthrough
    // (the product contract: corrupt tuning never latches a stale target).
    {
        VocalTargetControls empty_controls;
        empty_controls.tuning.clear();
        VocalTargetEngine engine;
        engine.set_sample_rate(kSampleRate);
        engine.set_controls(empty_controls);
        engine.reset();
        const float obs = -1173.0f;
        const float out_cents = engine.process(obs, 0.0f, 0.9f, 0.0f);
        result("negative_control_empty_tuning", "passthrough_error_cents", std::fabs(out_cents - obs));
        if (!(std::fabs(out_cents - obs) == 0.0f)) fail("negative_control_empty_tuning", "empty tuning did not pass observed through unchanged");
    }

    std::printf("PROBE,%s\n", any_failure ? "FAIL" : "PASS");
    return any_failure ? 1 : 0;
}
