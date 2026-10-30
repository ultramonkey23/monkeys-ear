#include "monkeys_ear/tube_rack.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace monkeys_ear {
namespace {
// Phenomenological envelopes/operating points inspired by tube families.
// Actual plate curves, grid current, screen and transformers are not modeled.
struct Profile {
    float grid_offset,input_gain,knee,load,output_gain,bandwidth_hz;
};
constexpr Profile profiles[kTubeFlavorCount] = {
    {0.85f,5.50f,0.26f,0.72f,1.70f,10500.f}, // 12AX7
    {1.05f,4.00f,0.34f,0.48f,1.45f,14500.f}, // 12AT7
    {1.20f,3.10f,0.49f,0.57f,1.50f,12500.f}, // 12AY7
    {1.42f,2.30f,0.60f,0.28f,1.05f,17500.f}, // 12AU7
    {0.98f,4.85f,0.31f,0.64f,1.55f,12000.f}, // 5751
    {1.16f,4.55f,0.42f,0.63f,1.42f,13300.f}, // 12DW7
    {1.35f,2.70f,0.55f,0.32f,1.14f,16300.f}, // 6SN7
    {1.26f,3.55f,0.37f,0.34f,1.38f,19200.f}, // 6DJ8
    {0.72f,6.10f,0.25f,0.88f,1.70f, 8900.f}, // EF86
    {1.07f,3.80f,0.43f,0.65f,1.48f,10300.f}, // 6V6
    {1.38f,2.85f,0.52f,0.36f,1.24f,17000.f}, // 6L6GC
    {0.91f,4.75f,0.34f,0.77f,1.63f,14300.f}, // EL84
    {0.79f,5.20f,0.29f,0.83f,1.67f,11400.f}, // EL34
    {1.51f,2.25f,0.64f,0.31f,1.16f,20000.f}  // KT88
};
inline bool finite(float x) noexcept {
    return (std::bit_cast<std::uint32_t>(x)&0x7f800000u)!=0x7f800000u;
}
inline float clean(float x) noexcept {
    return !finite(x)||std::abs(x)<1.0e-15f?0.0f:x;
}
inline float unit(float x,float fallback) noexcept {
    return finite(x)?clamp(x,0.0f,1.0f):fallback;
}
inline float current(float grid,const Profile& p,float load) noexcept {
    float x=clamp(grid,-16.f,16.f);
    float soft=0.5f*(x+std::sqrt(x*x+p.knee*p.knee));
    return soft/(1.0f+load*soft);
}
// Exact antiderivative for the selected smooth-positive, loaded proxy.
// A(x)= (L*p-log(1+L*p))/L^2 + (k^2/4)*(log(p)-log(1+L*p)),
// p=(x+sqrt(x^2+k^2))/2.
inline double primitive(double x,const Profile& p,double load) noexcept {
    const double k=p.knee;
    const double pp=0.5*(x+std::sqrt(x*x+k*k));
    const double lp=load*pp;
    return (lp-std::log1p(lp))/(load*load)
        +0.25*k*k*(std::log(pp)-std::log1p(lp));
}
inline float shape(float x,float previous,float offset,const Profile& p,float load,bool aa) noexcept {
    const float baseline=current(offset,p,load);
    if(!aa)return (current(offset+x,p,load)-baseline)*p.output_gain;
    const double a=clamp(previous+offset,-16.f,16.f);
    const double b=clamp(x+offset,-16.f,16.f);
    const double d=b-a;
    const float averaged=std::abs(d)<1.e-3
        ? current(static_cast<float>(0.5*(a+b)),p,load)
        : static_cast<float>((primitive(b,p,load)-primitive(a,p,load))/d);
    return (averaged-baseline)*p.output_gain;
}
} // namespace

void TubeRack::set_sample_rate(float sr) noexcept {
    sample_rate_=finite(sr)&&sr>=1000.f?clamp(sr,1000.f,768000.f):48000.f;
    smooth_coeff_=1.f-std::exp(-1.f/(.006f*sample_rate_));
    supply_coeff_=1.f-std::exp(-1.f/(.075f*sample_rate_));
    update_coefficients();
}
void TubeRack::set_controls(const TubeRackControls& in) noexcept {
    controls_.enabled=in.enabled;
    controls_.parallel=in.parallel;
    controls_.antialias=in.antialias;
    controls_.shared_supply=unit(in.shared_supply,.35f);
    controls_.output=unit(in.output,.5f);
    for(size_t i=0;i<states_.size();++i){
        const auto& src=in.slots[i];
        auto& dst=controls_.slots[i];
        dst.enabled=src.enabled;
        dst.flavor=std::clamp(src.flavor,0,kTubeFlavorCount-1);
        dst.drive=unit(src.drive,.3f);
        dst.bias=finite(src.bias)?clamp(src.bias,-1.f,1.f):0.f;
        dst.plate_load=unit(src.plate_load,.5f);
        dst.sag=unit(src.sag,.3f);
        dst.coupling=unit(src.coupling,.2f);
        dst.mix=unit(src.mix,.65f);
        if(states_[i].target_flavor!=dst.flavor){
            states_[i].previous_flavor=states_[i].target_flavor;
            states_[i].target_flavor=dst.flavor;
            states_[i].flavor_blend=0.f;
        }
    }
    update_coefficients();
}
void TubeRack::update_coefficients() noexcept {
    for(size_t i=0;i<states_.size();++i){
        const auto& c=controls_.slots[i];
        auto& s=states_[i];
        const float hp=10.f*std::pow(60.f,c.coupling);
        const float lp=std::min(profiles[c.flavor].bandwidth_hz,sample_rate_*.45f);
        s.target_hp=std::exp(-TWO_PI*hp/sample_rate_);
        s.target_lp=1.f-std::exp(-TWO_PI*lp/sample_rate_);
        s.cathode_coeff=1.f-std::exp(-1.f/(sample_rate_*(.025f+.130f*c.sag)));
    }
}
void TubeRack::set_drive_modulation(float v) noexcept {
    drive_modulation_=finite(v)?clamp(v,-1.f,1.f):0.f;
}
void TubeRack::set_character(float v) noexcept {character_=unit(v,.5f);}
void TubeRack::reset() noexcept {
    supply_state_=0.f;
    for(size_t i=0;i<states_.size();++i){
        auto& s=states_[i];
        s.cathode=s.hp_x=s.hp_y=s.lp_y=s.activity=0.f;
        s.runtime_drive=controls_.slots[i].drive;
        s.runtime_bias=controls_.slots[i].bias;
        s.runtime_mix=controls_.slots[i].enabled?controls_.slots[i].mix:0.f;
        s.hp_coeff=s.target_hp;s.lp_coeff=s.target_lp;
        s.previous_flavor=s.target_flavor=controls_.slots[i].flavor;
        s.flavor_blend=1.f;s.previous_excited=0.f;s.antialias_ready=false;
    }
}
float TubeRack::process_stage(size_t i,float input) noexcept {
    auto& s=states_[i];
    const auto& c=controls_.slots[i];
    const float want_mix=c.enabled?c.mix:0.f;
    s.runtime_mix+=(want_mix-s.runtime_mix)*smooth_coeff_;
    if(s.runtime_mix<.00001f&&!c.enabled){s.activity=0.f;return input;}
    s.runtime_drive+=(clamp(c.drive+drive_modulation_*.5f,0.f,1.f)-s.runtime_drive)*smooth_coeff_;
    s.runtime_bias+=(c.bias-s.runtime_bias)*smooth_coeff_;
    s.hp_coeff+=(s.target_hp-s.hp_coeff)*smooth_coeff_;
    s.lp_coeff+=(s.target_lp-s.lp_coeff)*smooth_coeff_;
    const Profile& p=profiles[c.flavor];
    const float excited=clamp(input*(1.f+s.runtime_drive*p.input_gain),-12.f,12.f);
    const float e=std::abs(excited);
    s.cathode+=(std::min(e,4.f)-s.cathode)*s.cathode_coeff;
    const float bias=s.runtime_bias*.60f-std::min(s.cathode,4.f)
                   *c.sag*(.25f+.75f*character_)*.18f-supply_state_*.45f;
    const float offset=p.grid_offset+bias,load=p.load*(.50f+c.plate_load);
    const bool aa=controls_.antialias&&s.antialias_ready;
    float shaped=shape(excited,s.previous_excited,offset,p,load,aa);
    if(s.previous_flavor!=c.flavor||s.flavor_blend<.999f){
        const Profile& previous=profiles[s.previous_flavor];
        float old=shape(excited,s.previous_excited,previous.grid_offset+bias,
                        previous,previous.load*(.50f+c.plate_load),aa);
        shaped=lerp(old,shaped,s.flavor_blend);
        s.flavor_blend=std::min(1.f,s.flavor_blend+smooth_coeff_);
        if(s.flavor_blend>=.999f)s.previous_flavor=c.flavor;
    }
    s.previous_excited=excited;s.antialias_ready=true;
    const float hp=s.hp_coeff*(s.hp_y+shaped-s.hp_x);
    s.hp_x=clean(shaped);s.hp_y=clean(hp);
    s.lp_y=clean(s.lp_y+(hp-s.lp_y)*s.lp_coeff);
    s.activity=std::min(1.f,e*.25f);
    return clean(clamp(lerp(input,clamp(s.lp_y,-3.f,3.f),s.runtime_mix),-6.f,6.f));
}
float TubeRack::process(float input) noexcept {
    input=clamp(clean(input),-6.f,6.f);
    if(!controls_.enabled)return input;
    float y=input;
    if(!controls_.parallel)for(size_t i=0;i<states_.size();++i)y=process_stage(i,y);
    else {
        const float left=process_stage(1,process_stage(0,input));
        const float right=process_stage(3,process_stage(2,input));
        y=.5f*(left+right);
    }
    float activity=0.f;
    for(const auto& s:states_)activity+=s.activity;
    supply_state_+=(activity*.25f*controls_.shared_supply-supply_state_)*supply_coeff_;
    supply_state_=clamp(clean(supply_state_),0.f,1.f);
    return clean(clamp(y*controls_.output*2.f,-6.f,6.f));
}
} // namespace monkeys_ear
