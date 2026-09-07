#include "monkeys_ear/sound_space.h"
#include <cmath>

namespace monkeys_ear {

void BoundedMicrofield::reset(float seed) {
    position_=0.0f; velocity_=0.0f; phase_=seed-std::floor(seed);
}

float BoundedMicrofield::process(float bound, float rate, float attraction,
                                 float resistance, float repulsion, float drive, float dt) {
    rate=clamp(rate,0.01f,24.0f); attraction=clamp(attraction,0.0f,1.0f);
    resistance=clamp(resistance,0.0f,1.0f); repulsion=clamp(repulsion,0.0f,1.0f);
    phase_+=rate*dt; phase_-=std::floor(phase_);
    float directed=std::sin(TWO_PI*phase_)*0.72f+clamp(drive,-1.0f,1.0f);
    float clearance=std::max(0.0f,1.0f-std::abs(position_));
    float edge=1.0f/(1.0f+std::pow(clearance/0.22f,2.0f));
    float sign=position_>0.0f?1.0f:(position_<0.0f?-1.0f:0.0f);
    float history=1.0f+2.5f*std::abs(position_)*(position_*velocity_>0.0f?1.0f:0.35f);
    float accel=directed*rate*rate*3.0f-attraction*position_*rate*rate*5.0f
               -velocity_*(0.4f+resistance*9.0f)*history-sign*repulsion*edge*rate*rate*8.0f;
    velocity_=clamp(velocity_+accel*dt,-4.0f*rate,4.0f*rate);
    position_=clamp(position_+velocity_*dt,-1.0f,1.0f);
    if(std::abs(position_)>=0.999f && position_*velocity_>0.0f) velocity_*=-0.35f;
    return position_*std::max(0.0f,bound);
}

void SoundSpaceProcessor::set_sample_rate(float sr) { sample_rate_=std::max(1000.0f,sr); reset(); }

void SoundSpaceProcessor::reset() {
    pitch_phase_=low_state_=allpass_z_=weight_fast_=weight_slow_=spectral_gain_db_=0.0f;
    pitch_field_.reset(0.13f);
    for(size_t i=0;i<HARMONICS;++i){harmonic_fields_[i].reset(0.17f+0.137f*static_cast<float>(i));harmonic_phase_[i]=0.0f;}
    for(size_t i=0;i<MODES;++i){modal_fields_[i].reset(0.31f+0.191f*static_cast<float>(i));resonators_[i]={};}
    metrics_={};
}

float SoundSpaceProcessor::source_value(int source,const SoundSpaceSources& s) const {
    switch(static_cast<SpaceSource>(std::clamp(source,0,7))){
      case SpaceSource::LFO:return s.lfo; case SpaceSource::Envelope:return s.envelope*2.0f-1.0f;
      case SpaceSource::FastEnergy:return s.fast_energy*2.0f-1.0f; case SpaceSource::SlowEnergy:return s.slow_energy*2.0f-1.0f;
      case SpaceSource::Direction:return s.direction; case SpaceSource::Aftertouch:return s.aftertouch*2.0f-1.0f;
      case SpaceSource::Velocity:return s.velocity*2.0f-1.0f; case SpaceSource::AudioEnergy:return s.audio_energy*2.0f-1.0f;
    } return 0.0f;
}

float SoundSpaceProcessor::process(float character,float weight,float center_hz,const SoundSpaceSources& s) {
    if(!controls_.enabled) return sanitize(character+weight);
    float pitch_b=controls_.pitch_bound_cents,harm_b=controls_.harmonic_bound_cents;
    float modal_b=controls_.modal_bound_cents,rate=controls_.movement_hz,attract=controls_.attraction;
    float spec_depth=controls_.spectral_depth_db,phase=controls_.phase_offset_cycles,harm_mix=controls_.harmonic_mix;
    for(const auto& route:controls_.routes){
        float m=source_value(route.source,s)*route.depth;
        switch(static_cast<SpaceDestination>(std::clamp(route.destination,0,7))){
          case SpaceDestination::PitchBound:pitch_b*=clamp(1.0f+m,0.0f,2.0f);break;
          case SpaceDestination::HarmonicBound:harm_b*=clamp(1.0f+m,0.0f,2.0f);break;
          case SpaceDestination::ModalBound:modal_b*=clamp(1.0f+m,0.0f,2.0f);break;
          case SpaceDestination::MovementRate:rate*=std::pow(4.0f,m);break;
          case SpaceDestination::Attraction:attract=clamp(attract+m*0.5f,0.0f,1.0f);break;
          case SpaceDestination::SpectralDepth:spec_depth*=clamp(1.0f+m,0.0f,2.0f);break;
          case SpaceDestination::Phase:phase+=m*0.25f;break;
          case SpaceDestination::HarmonicMix:harm_mix=clamp(harm_mix+m*0.5f,0.0f,1.0f);break;
        }
    }
    float energy_scale=clamp(1.0f+controls_.energy_widen*(s.fast_energy-s.slow_energy),0.1f,2.0f);
    float gesture=controls_.attack_freedom*s.envelope+controls_.release_relaxation*(1.0f-s.envelope);
    float dt=1.0f/sample_rate_, drive=0.35f*s.direction+0.25f*s.lfo+gesture*0.2f;
    float cents=pitch_field_.process(pitch_b*energy_scale,rate,attract,controls_.resistance,controls_.repulsion,drive,dt);
    float pitch_hz=clamp(center_hz*std::pow(2.0f,cents/1200.0f),20.0f,sample_rate_*0.45f);
    pitch_phase_+=pitch_hz/sample_rate_; pitch_phase_-=std::floor(pitch_phase_);
    float pitch_layer=std::sin(TWO_PI*(pitch_phase_+phase));

    float harmonic=0.0f,max_h=0.0f;
    for(size_t i=0;i<HARMONICS;++i){
        float partial=static_cast<float>(i+2); float freedom=lerp(0.08f,1.0f,std::pow(static_cast<float>(i+1)/HARMONICS,0.35f+2.65f*(1.0f-controls_.frequency_freedom)));
        float hc=harmonic_fields_[i].process(harm_b*freedom*energy_scale,rate*(1.0f+0.09f*i),attract,controls_.resistance,controls_.repulsion,drive,dt);
        max_h=std::max(max_h,std::abs(hc)); float hz=center_hz*partial*std::pow(2.0f,hc/1200.0f);
        if(hz<sample_rate_*0.47f){harmonic_phase_[i]+=hz/sample_rate_;harmonic_phase_[i]-=std::floor(harmonic_phase_[i]);harmonic+=std::sin(TWO_PI*(harmonic_phase_[i]+phase*freedom))/(partial*1.35f);}
    }

    float modal=0.0f,max_m=0.0f; constexpr float ratios[MODES]={1.49f,2.03f,2.91f,4.17f};
    for(size_t i=0;i<MODES;++i){
        float freedom=lerp(0.12f,1.0f,static_cast<float>(i+1)/MODES*controls_.frequency_freedom);
        float mc=modal_fields_[i].process(modal_b*freedom*energy_scale,rate*(0.63f+0.11f*i),attract,controls_.resistance,controls_.repulsion,drive,dt);
        max_m=std::max(max_m,std::abs(mc)); float hz=clamp(center_hz*ratios[i]*std::pow(2.0f,mc/1200.0f),30.0f,sample_rate_*0.44f);
        float r=0.986f+0.011f*controls_.resistance,theta=TWO_PI*hz/sample_rate_;
        float y=character+2.0f*r*std::cos(theta)*resonators_[i].y1-r*r*resonators_[i].y2;
        resonators_[i].y2=resonators_[i].y1;resonators_[i].y1=clamp(y,-8.0f,8.0f);modal+=y*0.045f;
    }

    float low_coeff=1.0f-std::exp(-TWO_PI*clamp(center_hz*1.8f,55.0f,420.0f)/sample_rate_);
    low_state_+=(character-low_state_)*low_coeff; float high=character-low_state_;
    float absw=std::abs(weight); float af=1.0f-std::exp(-1.0f/(0.004f*sample_rate_)),as=1.0f-std::exp(-1.0f/(0.180f*sample_rate_));
    weight_fast_+=(absw-weight_fast_)*af;weight_slow_+=(weight_fast_-weight_slow_)*as;
    float detector=lerp(weight_slow_,weight_fast_,controls_.spectral_priority);
    float target_db=-clamp(detector*spec_depth*3.0f,0.0f,spec_depth);
    float slew=1.0f-std::exp(-1.0f/((0.006f+controls_.resistance*0.090f)*sample_rate_));
    spectral_gain_db_+=(target_db-spectral_gain_db_)*slew;
    float negotiated=high+low_state_*std::pow(10.0f,spectral_gain_db_/20.0f);

    float a=clamp(std::sin(TWO_PI*phase)*0.82f,-0.82f,0.82f); float ap=-a*negotiated+allpass_z_; allpass_z_=negotiated+a*ap;
    float phase_character=lerp(negotiated,ap,controls_.phase_coupling);
    float out=weight+phase_character+controls_.pitch_mix*pitch_layer+harm_mix*harmonic+controls_.modal_mix*modal;
    metrics_={cents,max_h,max_m,spectral_gain_db_,weight_fast_,weight_slow_};
    return sanitize(out);
}

} // namespace monkeys_ear
