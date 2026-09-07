#include "monkeys_ear/chrono_state.h"
#include <cmath>
#include <algorithm>

namespace monkeys_ear {

ChronoStateBody::ChronoStateBody()
    : sample_rate_(48000.0f),
      dt_(1.0f / 48000.0f),
      resistance_(0.35f),
      memory_persistence_(0.50f),
      repulsion_(0.40f),
      coupling_(0.30f),
      freq_hz_(220.0f),
      mix_(0.50f),
      enabled_(true),
      alpha_fast_(0.0f),
      alpha_slow_(0.0f),
      omega0_(TWO_PI * 220.0f),
      boundary_limit_(1.0f) {
    update_coefficients();
    reset();
}

void ChronoStateBody::set_sample_rate(float sr) {
    sample_rate_ = std::max(1000.0f, sr);
    dt_ = 1.0f / sample_rate_;
    update_coefficients();
}

void ChronoStateBody::reset() {
    state_.displacement = 0.0f;
    state_.velocity = 0.0f;
    state_.energy_fast = 0.0f;
    state_.energy_slow = 0.0f;
    state_.trend = 0.0f;
    state_.phase = 0.0f;
    state_.regime = 0;
    state_.last_input = 0.0f;
}

void ChronoStateBody::set_resistance(float r) {
    resistance_ = clamp(r, 0.0f, 1.0f);
}

void ChronoStateBody::set_memory_persistence(float tau) {
    memory_persistence_ = clamp(tau, 0.0f, 1.0f);
    update_coefficients();
}

void ChronoStateBody::set_repulsion(float k) {
    repulsion_ = clamp(k, 0.0f, 1.0f);
}

void ChronoStateBody::set_coupling(float kappa) {
    coupling_ = clamp(kappa, 0.0f, 1.0f);
}

void ChronoStateBody::set_frequency(float freq_hz) {
    freq_hz_ = clamp(freq_hz, 20.0f, sample_rate_ * 0.45f);
    omega0_ = TWO_PI * freq_hz_;
}

void ChronoStateBody::set_mix(float mix) {
    mix_ = clamp(mix, 0.0f, 1.0f);
}

void ChronoStateBody::set_enabled(bool enabled) {
    enabled_ = enabled;
}

void ChronoStateBody::update_coefficients() {
    alpha_fast_ = std::exp(-1.0f / (0.003f * sample_rate_));
    float tau_slow = 0.15f + memory_persistence_ * 0.60f;
    alpha_slow_ = std::exp(-1.0f / (tau_slow * sample_rate_));

    omega0_ = TWO_PI * clamp(freq_hz_, 20.0f, sample_rate_ * 0.45f);
    boundary_limit_ = 1.0f;
}

float ChronoStateBody::process_sample(float exciter_in) {
    if (!enabled_) {
        return exciter_in;
    }

    // Soft-saturate extreme excitation spikes before physical coupling
    float in_sig = fast_tanh(exciter_in);

    // ── 1. Input Derivative & Phase Tracking ──────────────────────────────
    float in_deriv = (in_sig - state_.last_input) / dt_;
    state_.last_input = in_sig;

    float est_input_phase = std::atan2(in_deriv, in_sig * omega0_ + 1e-5f);
    if (est_input_phase < 0.0f) est_input_phase += TWO_PI;

    float phase_diff = state_.phase - est_input_phase;
    float signed_cos = std::cos(phase_diff); // Signed resonance [-1, 1]

    // ── 2. COUPLE Operator (Signed Phase-Resonance Coupling) ──────────────
    float f_couple = coupling_ * signed_cos * in_sig * omega0_ * 6.0f;
    float f_drive = (1.0f - coupling_ * 0.5f) * in_sig * omega0_ * 8.0f;

    // ── 3. ATTRACT Operator (Harmonic Restoring Force) ─────────────────────
    float f_attract = -omega0_ * omega0_ * state_.displacement;

    // ── 4. RESIST Operator (Direction- and History-Dependent Resistance) ──
    float disp = state_.displacement;
    float vel = state_.velocity;
    float strain = disp * disp;
    bool moving_away_from_equilibrium = (disp * vel > 0.0f);

    float r_strain = 1.0f + 3.0f * strain;
    float r_history = 1.0f + 5.0f * state_.energy_slow * (moving_away_from_equilibrium ? 1.6f : 0.6f);
    float r_trend = (state_.trend > 0.5f) ? 0.70f : 1.30f;

    float effective_resistance = (0.2f + resistance_ * 8.0f) * r_strain * r_history * r_trend;
    float f_resist = -effective_resistance * vel;

    // ── 5. REPEL Operator (Negative-Gravity Soft-Core Boundary Repulsion) ──
    float clearance = boundary_limit_ - std::abs(disp);
    float repel_kernel = negative_gravity_kernel(clearance, boundary_limit_ * 0.35f, 2.0f);
    float sgn = (disp > 0.0f) ? 1.0f : (disp < 0.0f ? -1.0f : 0.0f);
    float f_repel = -repulsion_ * sgn * repel_kernel * omega0_ * omega0_ * 1.8f;

    // ── 6. Symplectic State Integration (Euler-Cromer / Real-Time Bounded) ─
    float total_accel = f_attract + f_resist + f_repel + f_couple + f_drive;

    state_.velocity += total_accel * dt_;
    state_.velocity = clamp(state_.velocity, -2500.0f, 2500.0f);

    state_.displacement += state_.velocity * dt_;
    state_.displacement = clamp(state_.displacement, -boundary_limit_ * 1.25f, boundary_limit_ * 1.25f);

    // ── 7. Multiscale Energy Decomposition (DISSIPATE) ─────────────────────
    float inst_energy = (state_.velocity * state_.velocity) / (omega0_ * omega0_ + 1e-4f) + (disp * disp);

    state_.energy_fast = alpha_fast_ * state_.energy_fast + (1.0f - alpha_fast_) * inst_energy;
    state_.energy_slow = alpha_slow_ * state_.energy_slow + (1.0f - alpha_slow_) * state_.energy_fast;
    state_.trend = (state_.energy_fast - state_.energy_slow) / std::max(0.001f, state_.energy_slow);

    state_.phase += omega0_ * dt_;
    while (state_.phase >= TWO_PI) state_.phase -= TWO_PI;

    // ── 8. Regime Classification ──────────────────────────────────────────
    if (clearance < boundary_limit_ * 0.15f) {
        state_.regime = 3; // Boundary Repulsion
    } else if (state_.trend > 1.2f) {
        state_.regime = 1; // Attack Surge
    } else if (state_.energy_slow > 0.04f) {
        state_.regime = 2; // Sustained Resonant
    } else {
        state_.regime = 0; // Linear Rest
    }

    // ── 9. Nonlinear Pickup & Wet/Dry Output ──────────────────────────────
    float body_out = state_.displacement * 0.85f + fast_tanh(state_.displacement * 1.2f) * 0.15f;
    float result = lerp(in_sig, body_out, mix_);

    return sanitize(result);
}

} // namespace monkeys_ear
