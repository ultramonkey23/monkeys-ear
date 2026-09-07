#pragma once

#include "monkeys_ear/types.h"
#include <cmath>
#include <algorithm>

namespace monkeys_ear {

// ── Cody's Multiscale Chronofrequency State Variables ─────────────────────
struct ChronoStateVariables {
    float displacement = 0.0f;       // x: instantaneous deformation / modal state
    float velocity = 0.0f;           // v = dx/dt: rate of displacement
    float energy_fast = 0.0f;        // E_fast: micro-temporal energy (tau ~ 3ms, tracks fast transients)
    float energy_slow = 0.0f;        // E_slow: macro-temporal energy (tau ~ 400ms, tracks accumulated fatigue)
    float trend = 0.0f;              // dE/dt normalized: positive during attack surge, negative during decay
    float phase = 0.0f;              // Estimated instantaneous phase of dominant vibration [0, 2*pi)
    int regime = 0;                  // 0: Rest/Linear, 1: Attack/Surge, 2: Sustained Resonant, 3: Boundary Repulsion
    float last_input = 0.0f;         // Previous input sample for derivative tracking
};

// ── ChronoStateBody ───────────────────────────────────────────────────────
// Implements Cody's four state operators:
// 1. RESIST:   History- and strain-opposing resistance F_resist = -rho * R(x, v, E_slow) * v
// 2. ATTRACT:  Equilibrium restoring force F_attract = -omega0^2 * x
// 3. REPEL:    Negative-gravity soft-core repulsion K(d; ell, p) pushing back near boundaries
// 4. COUPLE:   Signed phase-resonance excitation coupling F_couple = kappa * cos(delta_theta) * s_in
class ChronoStateBody {
public:
    ChronoStateBody();
    void set_sample_rate(float sr);
    void reset();

    // Parameter setters (bounded [0.0, 1.0])
    void set_resistance(float r);     // Resistance operator strength rho
    void set_memory_persistence(float tau); // Multiscale balance (macro energy persistence)
    void set_repulsion(float k);      // Negative-gravity boundary repulsion strength
    void set_coupling(float kappa);   // Signed phase resonance coupling strength
    void set_frequency(float freq_hz);// Modal center frequency (attractor pitch)
    void set_mix(float mix);          // Dry/wet blend of stateful body output
    void set_enabled(bool enabled);   // Instant A/B comparison toggle (true: stateful, false: conventional baseline)

    // Getters
    bool is_enabled() const { return enabled_; }
    float get_resistance() const { return resistance_; }
    float get_repulsion() const { return repulsion_; }
    float get_coupling() const { return coupling_; }
    const ChronoStateVariables& get_state() const { return state_; }

    // Hard real-time audio sample processor (zero allocations, lock-free, O(1))
    float process_sample(float exciter_in);

    // Negative gravity soft-core kernel: K(d; ell, p) = 1 / (1 + (d/ell)^p)
    static float negative_gravity_kernel(float distance, float radius_ell, float power_p) {
        if (distance <= 0.0f) return 1.0f;
        float ratio = distance / std::max(0.001f, radius_ell);
        return 1.0f / (1.0f + std::pow(ratio, power_p));
    }

private:
    float sample_rate_;
    float dt_;

    // Operator coefficients
    float resistance_;            // rho: [0, 1]
    float memory_persistence_;    // tau_ratio: [0, 1]
    float repulsion_;             // k_repel: [0, 1]
    float coupling_;              // kappa: [0, 1]
    float freq_hz_;               // Center resonant frequency
    float mix_;                   // Wet mix
    bool enabled_;                // A/B switch

    // Time constants for multiscale energy decomposition
    float alpha_fast_;
    float alpha_slow_;

    // Dynamic physical state
    ChronoStateVariables state_;

    // Internal coefficients
    float omega0_;                // Resonant frequency in rad/s
    float boundary_limit_;        // ell: maximum physical displacement boundary

    void update_coefficients();
};

} // namespace monkeys_ear
