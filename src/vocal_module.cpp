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
    clear_ecosystem_evidence(); health_={true,true,true};
}

void VocalModule::reset() noexcept { processor_.reset(); local_pitch_tracker_.reset(); clear_ecosystem_evidence(); health_.finite_output=true; }
void VocalModule::set_controls(const VocalExpressionControls& controls) noexcept {
    controls_=controls; processor_.set_vocal_controls(controls_);
}

void VocalModule::set_local_pitch_evidence(float tracked_hz, float confidence) noexcept {
    tracked_hz=sanitize(tracked_hz); confidence=clamp(sanitize(confidence),0.0f,1.0f);
    local_pitch_hz_=(tracked_hz>=20.0f&&tracked_hz<=5000.0f)?tracked_hz:0.0f;
    local_pitch_confidence_=local_pitch_hz_>0.0f?confidence:0.0f;
}

void VocalModule::clear_ecosystem_evidence() noexcept {
    shared_pitch_hz_=0.0f; shared_pitch_confidence_=0.0f; shared_pitch_valid_=false;
}

void VocalModule::consume_ecosystem(const EcosystemSnapshot<32>& snapshot) noexcept {
    clear_ecosystem_evidence();
    const auto* pitch=snapshot.freshest(EvidenceKind::PitchCents,0.10f);
    if(!pitch) return;
    const float hz=440.0f*std::pow(2.0f,clamp(pitch->value,-7200.0f,7200.0f)/1200.0f);
    if(!std::isfinite(hz)||hz<20.0f||hz>5000.0f) return;
    shared_pitch_hz_=hz; shared_pitch_confidence_=clamp(pitch->confidence,0.0f,1.0f); shared_pitch_valid_=true;
}

void VocalModule::process_block(const float* input_l,const float* input_r,float* output_l,float* output_r,uint32_t num_samples) noexcept {
    if(!output_l||!output_r){health_.state_valid=false;return;}
    // Shared evidence enriches local analysis but does not automatically seize
    // authority: it wins only when valid and at least as confident as local evidence.
    const uint32_t n=std::min(num_samples,max_block_size_);
    for(uint32_t i=0;i<n;++i){
        const float l=input_l?sanitize(input_l[i]):0.0f,r=input_r?sanitize(input_r[i]):l;
        if(bypass_){output_l[i]=l;output_r[i]=r;continue;}
        const float mid=.5f*(l+r),side=.5f*(l-r);
        // The existing causal external tracker is the module's local authority.
        // A connected ecosystem may enrich it only when its evidence is stronger.
        local_pitch_tracker_.process(mid);
        const float detected_hz=local_pitch_tracker_.tracked_frequency_hz();
        const float detected_confidence=local_pitch_tracker_.confidence();
        const bool use_manual=local_pitch_confidence_>=detected_confidence;
        const float local_hz=use_manual?local_pitch_hz_:detected_hz;
        const float local_confidence=use_manual?local_pitch_confidence_:detected_confidence;
        const bool use_shared=shared_pitch_valid_&&shared_pitch_confidence_>=local_confidence;
        const float tracked_hz=use_shared?shared_pitch_hz_:local_hz;
        const float confidence=use_shared?shared_pitch_confidence_:local_confidence;
        const float conditioned=processor_.process_sample(mid,0.0f);
        const float wet_mid=processor_.process_vocal_sample(conditioned,tracked_hz,confidence);
        float out_l=sanitize(wet_mid+side),out_r=sanitize(wet_mid-side);
        if(!std::isfinite(out_l)||!std::isfinite(out_r)){out_l=l;out_r=r;health_.finite_output=false;}
        output_l[i]=out_l;output_r[i]=out_r;
    }
    for(uint32_t i=n;i<num_samples;++i){output_l[i]=input_l?sanitize(input_l[i]):0.0f;output_r[i]=input_r?sanitize(input_r[i]):output_l[i];}
}

} // namespace monkeys_ear
