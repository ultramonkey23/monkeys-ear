#include "monkeys_ear/onion_eq_module.h"
#include <algorithm>
#include <atomic>
#include <cmath>

namespace monkeys_ear {
namespace {
std::atomic<uint64_t> g_onion_instance_counter{1};
} // namespace

OnionEqModule::OnionEqModule() noexcept {
    instance_tag_ = g_onion_instance_counter.fetch_add(1, std::memory_order_relaxed);
    prepare(48000.0f, 512);
}

void OnionEqModule::prepare(float sample_rate, uint32_t max_block_size) noexcept {
    sample_rate_ = std::max(8000.0f, sample_rate);
    max_block_size_ = std::max<uint32_t>(1u, max_block_size);
    eq_l_.set_sample_rate(sample_rate_);
    eq_r_.set_sample_rate(sample_rate_);
    self_pitch_tracker_.set_sample_rate(sample_rate_);
    eco_bus_.ensure_connected();
    set_controls(controls_);
}

void OnionEqModule::reset() noexcept {
    eq_l_.reset();
    eq_r_.reset();
    self_pitch_tracker_.reset();
    eco_bus_.publish_inactive(last_eco_slot_, 2);
    merged_metrics_ = OnionSkinMetrics{};
    health_.in_peak = 0.0f;
    health_.out_peak = 0.0f;
    health_.sidechain_active = false;
    health_.eco_peer_active = false;
    health_.self_deharsh_active = false;
    last_self_occ_.fill(0.0f);
}

void OnionEqModule::set_controls(const OnionEqControls& controls) noexcept {
    const int prev_slot = std::clamp(controls_.eco_slot, 0, static_cast<int>(EcosystemBusClient::kSlotCount) - 1);
    controls_ = controls;
    const int next_slot = std::clamp(controls_.eco_slot, 0, static_cast<int>(EcosystemBusClient::kSlotCount) - 1);
    if (prev_slot != next_slot || controls_.bypass ||
        (controls_.eco_mode != EcoLinkMode::Publish && controls_.eco_mode != EcoLinkMode::Bidirectional)) {
        eco_bus_.publish_inactive(prev_slot, 2);
    }
    last_eco_slot_ = next_slot;
    OnionSkinControls inner{};
    inner.enabled = !controls_.bypass && controls_.dynamic_enabled;
    inner.intent = controls_.intent;
    inner.amount = clamp(sanitize(controls_.amount), 0.0f, 1.0f);
    inner.sensitivity = clamp(sanitize(controls_.sensitivity), 0.0f, 1.0f);
    inner.low_protect = clamp(sanitize(controls_.low_protect), 0.0f, 1.0f);
    inner.transient_protect = clamp(sanitize(controls_.transient_protect), 0.0f, 1.0f);
    inner.harmonic_protect = clamp(sanitize(controls_.harmonic_protect), 0.0f, 1.0f);
    inner.tilt_db = clamp(sanitize(controls_.tilt_db), -6.0f, 6.0f);
    inner.q_focus = clamp(sanitize(controls_.q_focus), 0.85f, 4.50f);
    inner.air_db = clamp(sanitize(controls_.air_db), -6.0f, 6.0f);
    for (size_t i = 0; i < OnionSkinEQ::kNumBands; ++i) {
        inner.manual_db[i] = clamp(sanitize(controls_.manual_db[i]), -12.0f, 12.0f);
    }
    eq_l_.set_controls(inner);
    eq_r_.set_controls(inner);
}

void OnionEqModule::process_block(const float* in_l,
                                  const float* in_r,
                                  const float* sc_l,
                                  const float* sc_r,
                                  float* out_l,
                                  float* out_r,
                                  uint32_t num_samples) noexcept {
    if (!out_l || !out_r || num_samples == 0) {
        return;
    }

    const uint32_t n = std::min(num_samples, max_block_size_);
    const int eco_slot = std::clamp(controls_.eco_slot, 0, static_cast<int>(EcosystemBusClient::kSlotCount) - 1);
    const bool allow_sc = (controls_.ref_source == OnionRefSource::Auto || controls_.ref_source == OnionRefSource::Sidechain);
    const bool allow_eco = (controls_.ref_source == OnionRefSource::Auto || controls_.ref_source == OnionRefSource::EcoPeer);
    const bool use_self_deharsh = (controls_.ref_source == OnionRefSource::SelfDeHarsh);

    std::array<float, OnionSkinEQ::kNumBands> peer_occ{};
    float peer_weight = 0.0f;
    bool eco_active = false;
    bool self_deharsh_active = false;

    if (use_self_deharsh && !controls_.bypass && controls_.dynamic_enabled) {
        self_deharsh_active = true;
        for (size_t b = 0; b < OnionSkinEQ::kNumBands; ++b) {
            float nbr_sum = 0.0f;
            int nbr_cnt = 0;
            for (int off = -2; off <= 2; ++off) {
                if (off == 0) continue;
                const int idx = static_cast<int>(b) + off;
                if (idx >= 0 && idx < static_cast<int>(OnionSkinEQ::kNumBands)) {
                    nbr_sum += last_self_occ_[static_cast<size_t>(idx)];
                    ++nbr_cnt;
                }
            }
            const float nbr_avg = nbr_cnt > 0 ? (nbr_sum / static_cast<float>(nbr_cnt)) : 0.0f;
            const float excess = std::max(0.0f, last_self_occ_[b] - nbr_avg * 1.18f - 0.025f);
            peer_occ[b] = clamp(excess * 2.5f, 0.0f, 1.0f);
        }
        peer_weight = 1.0f;
    } else if (allow_eco && (controls_.eco_mode == EcoLinkMode::Follow || controls_.eco_mode == EcoLinkMode::Bidirectional)) {
        EcosystemStreamFrame sf{}, vf{}, af{};
        const uint64_t own_prefix = (instance_tag_ << 32);
        const bool has_s = eco_bus_.read_synth(eco_slot, sf) && sf.active && ((sf.frame_id & 0xFFFFFFFF00000000ULL) != own_prefix);
        const bool has_v = eco_bus_.read_vocal(eco_slot, vf) && vf.active && ((vf.frame_id & 0xFFFFFFFF00000000ULL) != own_prefix);
        const bool has_a = eco_bus_.read_aux_eq(eco_slot, af) && af.active && ((af.frame_id & 0xFFFFFFFF00000000ULL) != own_prefix);
        if (has_s || has_v || has_a) {
            eco_active = true;
            for (size_t b = 0; b < OnionSkinEQ::kNumBands; ++b) {
                const float s_val = has_s ? clamp(sf.spectral_occupancy[b], 0.0f, 1.0f) : 0.0f;
                const float v_val = has_v ? clamp(vf.spectral_occupancy[b], 0.0f, 1.0f) : 0.0f;
                const float a_val = has_a ? clamp(af.spectral_occupancy[b], 0.0f, 1.0f) : 0.0f;
                peer_occ[b] = std::max({s_val, v_val, a_val});
            }
            peer_weight = clamp(sanitize(controls_.peer_mask_weight), 0.0f, 1.0f);
        }
    }

    eq_l_.set_peer_occupancy(peer_occ, peer_weight);
    eq_r_.set_peer_occupancy(peer_occ, peer_weight);

    const float mix = clamp(sanitize(controls_.mix), 0.0f, 1.0f);
    const float trim_lin = std::pow(10.0f, clamp(sanitize(controls_.output_trim_db), -12.0f, 12.0f) * 0.05f);

    float in_pk = 0.0f;
    float out_pk = 0.0f;
    float sc_pk = 0.0f;
    float rms_sq = 0.0f;

    for (uint32_t i = 0; i < n; ++i) {
        const float l_in = in_l ? sanitize(in_l[i]) : 0.0f;
        const float r_in = in_r ? sanitize(in_r[i]) : l_in;
        const float mono_in = 0.5f * (l_in + r_in);
        if (controls_.harmonic_protect > 1e-4f && !controls_.bypass) {
            (void)self_pitch_tracker_.process(mono_in);
            const float f0 = self_pitch_tracker_.tracked_frequency_hz();
            const float conf = self_pitch_tracker_.confidence();
            eq_l_.set_self_pitch(f0, conf);
            eq_r_.set_self_pitch(f0, conf);
        } else {
            eq_l_.set_self_pitch(0.0f, 0.0f);
            eq_r_.set_self_pitch(0.0f, 0.0f);
        }
        const float abs_in = std::max(std::abs(l_in), std::abs(r_in));
        if (abs_in > in_pk) in_pk = abs_in;

        const float raw_scl = (allow_sc && sc_l) ? sanitize(sc_l[i]) : 0.0f;
        const float raw_scr = (allow_sc && sc_r) ? sanitize(sc_r[i]) : raw_scl;
        const float abs_sc = std::max(std::abs(raw_scl), std::abs(raw_scr));
        if (abs_sc > sc_pk) sc_pk = abs_sc;

        if (controls_.bypass) {
            out_l[i] = l_in;
            out_r[i] = r_in;
            if (abs_in > out_pk) out_pk = abs_in;
            continue;
        }

        float proc_l = 0.0f;
        float proc_r = 0.0f;
        if (controls_.stereo_mode == OnionStereoMode::Stereo) {
            const float eq_l = eq_l_.process_sample(l_in, raw_scl);
            const float eq_r = eq_r_.process_sample(r_in, raw_scr);
            proc_l = lerp(l_in, eq_l, mix) * trim_lin;
            proc_r = lerp(r_in, eq_r, mix) * trim_lin;
        } else if (controls_.stereo_mode == OnionStereoMode::MidOnly) {
            const float mid = 0.5f * (l_in + r_in);
            const float side = 0.5f * (l_in - r_in);
            const float sc_mid = 0.5f * (raw_scl + raw_scr);
            const float eq_m = eq_l_.process_sample(mid, sc_mid);
            (void)eq_r_.process_sample(mid, sc_mid);
            const float wet_m = lerp(mid, eq_m, mix);
            proc_l = (wet_m + side) * trim_lin;
            proc_r = (wet_m - side) * trim_lin;
        } else {
            const float mid = 0.5f * (l_in + r_in);
            const float side = 0.5f * (l_in - r_in);
            const float sc_side = 0.5f * (raw_scl - raw_scr);
            const float eq_s = eq_l_.process_sample(side, sc_side);
            (void)eq_r_.process_sample(side, sc_side);
            const float wet_s = lerp(side, eq_s, mix);
            proc_l = (mid + wet_s) * trim_lin;
            proc_r = (mid - wet_s) * trim_lin;
        }

        proc_l = sanitize(proc_l);
        proc_r = sanitize(proc_r);
        if (!std::isfinite(proc_l) || !std::isfinite(proc_r)) {
            proc_l = l_in;
            proc_r = r_in;
            health_.finite_output = false;
        }
        out_l[i] = proc_l;
        out_r[i] = proc_r;

        const float abs_out = std::max(std::abs(proc_l), std::abs(proc_r));
        if (abs_out > out_pk) out_pk = abs_out;
        const float mono = 0.5f * (proc_l + proc_r);
        rms_sq += mono * mono;
    }

    // Overflow samples beyond prepared max_block_size_ pass through safely dry.
    for (uint32_t i = n; i < num_samples; ++i) {
        const float l_in = in_l ? sanitize(in_l[i]) : 0.0f;
        const float r_in = in_r ? sanitize(in_r[i]) : l_in;
        out_l[i] = l_in;
        out_r[i] = r_in;
    }

    const OnionSkinMetrics& ml = eq_l_.metrics();
    const OnionSkinMetrics& mr = eq_r_.metrics();
    for (size_t b = 0; b < OnionSkinEQ::kNumBands; ++b) {
        merged_metrics_.self_occupancy[b] = 0.5f * (ml.self_occupancy[b] + mr.self_occupancy[b]);
        merged_metrics_.ref_occupancy[b] = 0.5f * (ml.ref_occupancy[b] + mr.ref_occupancy[b]);
        merged_metrics_.band_carve_db[b] = 0.5f * (ml.band_carve_db[b] + mr.band_carve_db[b]);
        merged_metrics_.user_db[b] = 0.5f * (ml.user_db[b] + mr.user_db[b]);
        merged_metrics_.total_eq_db[b] = 0.5f * (ml.total_eq_db[b] + mr.total_eq_db[b]);
        last_self_occ_[b] = merged_metrics_.self_occupancy[b];
    }
    merged_metrics_.identity_cosine = std::min(ml.identity_cosine, mr.identity_cosine);
    merged_metrics_.conflict_reduction = 0.5f * (ml.conflict_reduction + mr.conflict_reduction);
    merged_metrics_.transient_hold = 0.5f * (ml.transient_hold + mr.transient_hold);

    health_.in_peak = in_pk;
    health_.out_peak = out_pk;
    health_.sidechain_active = allow_sc && (sc_pk > 1e-4f);
    health_.eco_peer_active = eco_active;
    health_.self_deharsh_active = self_deharsh_active;

    if (!controls_.bypass && n > 0 && (controls_.eco_mode == EcoLinkMode::Publish || controls_.eco_mode == EcoLinkMode::Bidirectional)) {
        EcosystemStreamFrame pub{};
        pub.frame_id = (instance_tag_ << 32) | (++pub_seq_);
        pub.role = 3; // Onion EQ dedicated aux_eq publisher (never collides with synth or vocal)
        pub.active = true;
        const float f0 = self_pitch_tracker_.tracked_frequency_hz();
        const float conf = self_pitch_tracker_.confidence();
        pub.pitch_cents = (f0 > 20.0f) ? (1200.0f * std::log2(f0 / 440.0f)) : 0.0f;
        pub.pitch_confidence = conf;
        pub.loudness = clamp(std::sqrt(rms_sq / static_cast<float>(n)), 0.0f, 1.0f);
        pub.note_gate = pub.loudness > 0.005f ? 1.0f : 0.0f;
        pub.spectral_occupancy = merged_metrics_.self_occupancy;
        eco_bus_.publish_aux_eq(eco_slot, pub);
    } else if (controls_.bypass) {
        eco_bus_.publish_inactive(eco_slot, 2);
    }
}

} // namespace monkeys_ear
