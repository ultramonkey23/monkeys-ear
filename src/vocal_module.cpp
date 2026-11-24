#include "monkeys_ear/vocal_module.h"
#include <algorithm>
#include <cmath>

namespace monkeys_ear {

const ModuleIdentity VocalModule::kIdentity{
    "monkeys-ear.vocal", "Monkey's Ear Vocal", "Fx|Pitch Shift",
    ModuleKind::AudioEffect, 1u
};

VocalModule::VocalModule() noexcept {
    controls_.enabled = true; controls_.correction_strength = 0.45f;
    controls_.drift_retention = 0.35f; controls_.vibrato_retention = 0.80f;
    controls_.transition = 0.55f; controls_.formant_repair = 0.50f;
    controls_.spectral_residual_mix = 0.10f; controls_.character = 0.0f;
    controls_.mix = 1.0f; controls_.sequential_stage_mix = 0.35f;
    controls_.aperiodic_protection = 0.90f; processor_.set_vocal_controls(controls_);
}

const ModuleIdentity& VocalModule::identity() const noexcept { return kIdentity; }
ModuleIOContract VocalModule::io_contract() const noexcept {
    ModuleIOContract io{}; io.audio_inputs=2; io.audio_outputs=2;
    io.accepts_midi=false; io.produces_midi=false; io.reports_zero_latency=true; return io;
}

void VocalModule::prepare(float sample_rate, uint32_t max_block_size) noexcept {
    sample_rate=sanitize(sample_rate);
    sample_rate_=sample_rate>=1000.0f?clamp(sample_rate,1000.0f,768000.0f):48000.0f;
    max_block_size_=std::max<uint32_t>(1u,std::min<uint32_t>(max_block_size,65536u));
    processor_.set_sample_rate(sample_rate_); local_pitch_tracker_.set_sample_rate(sample_rate_); processor_.set_gain(0.0f); processor_.set_mix(1.0f);
    processor_.set_highpass_enabled(false); processor_.set_vocal_controls(controls_); processor_.reset();
    eco_analyzer_.set_sample_rate(sample_rate_);
    eco_analyzer_.reset();
    eco_bus_.ensure_connected();
    last_eco_synth_frame_id_ = 0;
    last_tracked_hz_ = 0.0f;
    last_tracked_conf_ = 0.0f;
    eco_peer_linked_ = false;
    local_pitch_hz_=local_pitch_confidence_=0.f; clear_ecosystem_evidence(); health_={true,true,true};
}

void VocalModule::reset() noexcept {
    processor_.reset();
    local_pitch_tracker_.reset();
    eco_analyzer_.reset();
    eco_bus_.publish_inactive(eco_slot_, 1);
    local_pitch_hz_=local_pitch_confidence_=0.f;
    last_tracked_hz_ = 0.0f;
    last_tracked_conf_ = 0.0f;
    eco_peer_linked_ = false;
    clear_ecosystem_evidence();
    health_.finite_output=true;
}

void VocalModule::set_eco_slot(int slot) noexcept {
    const int next = clamp(slot, 0, 7);
    if (next != eco_slot_) {
        eco_bus_.publish_inactive(eco_slot_, 1);
        eco_slot_ = next;
    }
}

void VocalModule::set_eco_mode(int mode) noexcept {
    const int next = clamp(mode, 0, 3);
    if ((next == 0 || next == 2) && (eco_mode_ == 1 || eco_mode_ == 3)) {
        eco_bus_.publish_inactive(eco_slot_, 1);
    }
    eco_mode_ = next;
    eco_link_enabled_ = (eco_mode_ != 0);
}
void VocalModule::set_controls(const VocalExpressionControls& controls) noexcept {
    controls_=controls;
    auto processor_controls=controls_;
    // The module owns the plugin-level dry/wet contract. Keep the inner vocal
    // processor fully wet so Mix=0 can return the original input exactly.
    processor_controls.mix=1.0f;
    processor_.set_vocal_controls(processor_controls);
}

void VocalModule::set_local_pitch_evidence(float tracked_hz, float confidence) noexcept {
    tracked_hz=sanitize(tracked_hz); confidence=clamp(sanitize(confidence),0.0f,1.0f);
    local_pitch_hz_=(tracked_hz>=20.0f&&tracked_hz<=5000.0f)?tracked_hz:0.0f;
    local_pitch_confidence_=local_pitch_hz_>0.0f?confidence:0.0f;
}

void VocalModule::clear_ecosystem_evidence() noexcept {
    shared_pitch_hz_=0.0f; shared_pitch_confidence_=0.0f; shared_pitch_samples_remaining_=0; shared_pitch_valid_=false;
}

void VocalModule::consume_ecosystem(const EcosystemSnapshot<32>& snapshot) noexcept {
    clear_ecosystem_evidence();
    const auto* pitch=snapshot.freshest(EvidenceKind::PitchCents,0.10f);
    if(!pitch) return;
    const float hz=440.0f*std::pow(2.0f,clamp(pitch->value,-7200.0f,7200.0f)/1200.0f);
    if(!ecosystem_finite(hz)||hz<20.0f||hz>5000.0f) return;
    shared_pitch_hz_=hz; shared_pitch_confidence_=clamp(pitch->confidence,0.0f,1.0f);
    // Age continues while processing, even when bypassed; a snapshot does not
    // grant indefinite authority if the producer stops publishing.
    shared_pitch_samples_remaining_=static_cast<uint32_t>(std::max(0.f,(.10f-pitch->age_seconds)*sample_rate_));
    shared_pitch_valid_=shared_pitch_samples_remaining_>0;
}

void VocalModule::process_block(const float* input_l,const float* input_r,float* output_l,float* output_r,uint32_t num_samples) noexcept {
    if(!output_l||!output_r){health_.state_valid=false;return;}
    // Shared evidence enriches local analysis but does not automatically seize
    // authority: it wins only when valid and at least as confident as local evidence.
    const bool allow_follow = (eco_mode_ == 2 || eco_mode_ == 3);
    const bool allow_publish = (eco_mode_ == 1 || eco_mode_ == 3);
    if (allow_follow) {
        EcosystemStreamFrame sf{};
        if (eco_bus_.read_synth_or_any(eco_slot_, sf) && sf.frame_id != last_eco_synth_frame_id_) {
            last_eco_synth_frame_id_ = sf.frame_id;
            if (sf.note_gate > 0.5f && sf.pitch_confidence > 0.25f) {
                const float hz = 440.0f * std::pow(2.0f, clamp(sf.pitch_cents, -4800.0f, 4800.0f) / 1200.0f);
                if (ecosystem_finite(hz) && hz >= 20.0f && hz <= 5000.0f) {
                    shared_pitch_hz_ = hz;
                    shared_pitch_confidence_ = clamp(sf.pitch_confidence, 0.0f, 1.0f);
                    shared_pitch_samples_remaining_ = static_cast<uint32_t>(sample_rate_ * 0.10f);
                    shared_pitch_valid_ = true;
                    eco_peer_linked_ = true;
                }
            }
        } else if (!shared_pitch_valid_) {
            eco_peer_linked_ = false;
        }
    } else {
        eco_peer_linked_ = false;
    }

    float block_tracked_hz = 0.0f;
    float block_conf = 0.0f;
    float block_rms_sq = 0.0f;
    const uint32_t n=std::min(num_samples,max_block_size_);
    for(uint32_t i=0;i<n;++i){
        const float l=input_l?sanitize(input_l[i]):0.0f,r=input_r?sanitize(input_r[i]):l;
        if(shared_pitch_valid_ && shared_pitch_samples_remaining_==0){ clear_ecosystem_evidence(); eco_peer_linked_ = false; }
        if(shared_pitch_samples_remaining_>0)--shared_pitch_samples_remaining_;
        if(bypass_){output_l[i]=l;output_r[i]=r;continue;}
        const float mid=.5f*(l+r),side=.5f*(l-r);
        // The existing causal external tracker is the module's local authority.
        // A connected ecosystem may enrich it only when its evidence is stronger.
        local_pitch_tracker_.process(mid);
        (void)eco_analyzer_.process_sample(mid, mid);
        const float detected_hz=local_pitch_tracker_.tracked_frequency_hz();
        const float detected_confidence=local_pitch_tracker_.confidence();
        const bool use_manual=local_pitch_confidence_>=detected_confidence;
        const float local_hz=use_manual?local_pitch_hz_:detected_hz;
        const float local_confidence=use_manual?local_pitch_confidence_:detected_confidence;
        const bool use_shared=shared_pitch_valid_&&shared_pitch_confidence_>=local_confidence;
        const float tracked_hz=use_shared?shared_pitch_hz_:local_hz;
        const float confidence=use_shared?shared_pitch_confidence_:local_confidence;
        block_tracked_hz = tracked_hz;
        block_conf = confidence;
        block_rms_sq += mid * mid;
        const float conditioned=processor_.process_sample(mid,0.0f);
        const float wet_mid=processor_.process_vocal_sample(conditioned,tracked_hz,confidence);
        const float module_mix=clamp(sanitize(controls_.mix),0.0f,1.0f);
        const float mixed_mid=lerp(mid,wet_mid,module_mix);
        float out_l=sanitize(mixed_mid+side),out_r=sanitize(mixed_mid-side);
        if(!std::isfinite(out_l)||!std::isfinite(out_r)){out_l=l;out_r=r;health_.finite_output=false;}
        output_l[i]=out_l;output_r[i]=out_r;
    }
    last_tracked_hz_ = block_tracked_hz;
    last_tracked_conf_ = block_conf;
    if (allow_publish && !bypass_ && n > 0) {
        EcosystemStreamFrame vpub{};
        vpub.frame_id = ++vocal_pub_frame_id_;
        vpub.role = 2;
        vpub.active = true;
        vpub.pitch_cents = (block_tracked_hz > 20.0f) ? (1200.0f * std::log2(block_tracked_hz / 440.0f)) : 0.0f;
        vpub.pitch_confidence = block_conf;
        vpub.loudness = clamp(std::sqrt(block_rms_sq / static_cast<float>(n)), 0.0f, 1.0f);
        vpub.note_gate = (vpub.loudness > 0.01f && block_conf > 0.25f) ? 1.0f : 0.0f;
        vpub.spectral_occupancy = eco_analyzer_.self_occupancy();
        eco_bus_.publish_vocal(eco_slot_, vpub);
    } else if (bypass_) {
        eco_bus_.publish_inactive(eco_slot_, 1);
    }
    // Oversized blocks fall back to dry after max_block_size_, but time still
    // advances and must not keep a shared pitch alive into the next block.
    const uint32_t skipped=num_samples-n;
    if(skipped>=shared_pitch_samples_remaining_)shared_pitch_samples_remaining_=0;
    else shared_pitch_samples_remaining_-=skipped;
    if(shared_pitch_valid_&&shared_pitch_samples_remaining_==0)clear_ecosystem_evidence();
    for(uint32_t i=n;i<num_samples;++i){output_l[i]=input_l?sanitize(input_l[i]):0.0f;output_r[i]=input_r?sanitize(input_r[i]):output_l[i];}
}

} // namespace monkeys_ear
