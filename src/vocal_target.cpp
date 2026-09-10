#include "monkeys_ear/vocal_target.h"
#include <cmath>
#include <cstring>

namespace monkeys_ear {
namespace {
void copy_label(std::array<char, 32>& out, const char* text) { out.fill(0); if (text) std::strncpy(out.data(), text, out.size() - 1); }
struct BuiltinDefinition { const char* name; const float* degrees; size_t count; float period; };
constexpr float chromatic[]={0,100,200,300,400,500,600,700,800,900,1000,1100}; constexpr float major[]={0,200,400,500,700,900,1100}; constexpr float minor[]={0,200,300,500,700,800,1000}; constexpr float dorian[]={0,200,300,500,700,900,1000}; constexpr float phrygian[]={0,100,300,500,700,800,1000}; constexpr float lydian[]={0,200,400,600,700,900,1100}; constexpr float mixolydian[]={0,200,400,500,700,900,1000}; constexpr float locrian[]={0,100,300,500,600,800,1000};
constexpr float harmonic_minor[]={0,200,300,500,700,800,1100}; constexpr float melodic_minor[]={0,200,300,500,700,900,1100}; constexpr float phrygian_dominant[]={0,100,400,500,700,800,1000}; constexpr float lydian_sharp2[]={0,300,400,600,700,900,1100}; constexpr float lydian_dominant[]={0,200,400,600,700,900,1000}; constexpr float altered[]={0,100,300,400,600,800,1000};
constexpr float major_pent[]={0,200,400,700,900}; constexpr float minor_pent[]={0,300,500,700,1000}; constexpr float blues[]={0,300,500,600,700,1000}; constexpr float whole[]={0,200,400,600,800,1000}; constexpr float dim_hw[]={0,100,300,400,600,700,900,1000}; constexpr float dim_wh[]={0,200,300,500,600,800,900,1100}; constexpr float augmented[]={0,300,400,700,800,1100};
constexpr float bebop_major[]={0,200,400,500,700,800,900,1100}; constexpr float bebop_dom[]={0,200,400,500,700,900,1000,1100}; constexpr float bebop_dorian[]={0,200,300,500,700,900,1000,1100}; constexpr float hirajoshi[]={0,200,300,700,800}; constexpr float insen[]={0,100,500,700,1000}; constexpr float iwato[]={0,100,500,600,1000}; constexpr float pelog[]={0,100,300,700,800};
constexpr float quarter[]={0,50,100,150,200,250,300,350,400,450,500,550,600,650,700,750,800,850,900,950,1000,1050,1100,1150}; constexpr float bohlen_pierce[]={0,146.304f,292.608f,438.913f,585.217f,731.521f,877.825f,1024.129f,1170.433f,1316.738f,1463.042f,1609.346f,1755.650f};
constexpr BuiltinDefinition builtins[]={
{"Chromatic",chromatic,12,1200},{"Major / Ionian",major,7,1200},{"Natural Minor / Aeolian",minor,7,1200},{"Dorian",dorian,7,1200},{"Phrygian",phrygian,7,1200},{"Lydian",lydian,7,1200},{"Mixolydian",mixolydian,7,1200},{"Locrian",locrian,7,1200},{"Harmonic Minor",harmonic_minor,7,1200},{"Melodic Minor",melodic_minor,7,1200},{"Phrygian Dominant",phrygian_dominant,7,1200},{"Lydian Sharp 2",lydian_sharp2,7,1200},{"Lydian Dominant",lydian_dominant,7,1200},{"Altered",altered,7,1200},{"Major Pentatonic",major_pent,5,1200},{"Minor Pentatonic",minor_pent,5,1200},{"Blues",blues,6,1200},{"Whole Tone",whole,6,1200},{"Diminished Half-Whole",dim_hw,8,1200},{"Diminished Whole-Half",dim_wh,8,1200},{"Augmented",augmented,6,1200},{"Bebop Major",bebop_major,8,1200},{"Bebop Dominant",bebop_dom,8,1200},{"Bebop Dorian",bebop_dorian,8,1200},{"Hirajoshi (0,2,3,7,8)",hirajoshi,5,1200},{"Insen (0,1,5,7,10)",insen,5,1200},{"Iwato (0,1,5,6,10)",iwato,5,1200},{"Pelog approximation (0,1,3,7,8)",pelog,5,1200},{"24 Equal Divisions",quarter,24,1200},{"Bohlen-Pierce 13",bohlen_pierce,13,1901.955f}};
static_assert(std::size(builtins)==static_cast<size_t>(VocalBuiltinTuning::Count));
}

TuningSpace::TuningSpace(){set_builtin(VocalBuiltinTuning::Chromatic);}
void TuningSpace::clear(float period){degree_count=0;period_cents=std::isfinite(period)?std::max(1.0f,period):1200.0f;builtin=VocalBuiltinTuning::Count;label.fill(0);}
bool TuningSpace::add_degree_cents(float cents,float gravity,float ascending,float descending,const char* degree_label){
    if(degree_count>=MAX_DEGREES||!std::isfinite(cents))return false;
    auto& degree=degrees[degree_count++];degree={};degree.cents=cents;
    degree.gravity=clamp(sanitize(gravity),0.0f,1.0f);degree.ascending_gravity=clamp(sanitize(ascending),0.0f,2.0f);degree.descending_gravity=clamp(sanitize(descending),0.0f,2.0f);
    if(degree_label)std::strncpy(degree.label.data(),degree_label,degree.label.size()-1);return true;
}
bool TuningSpace::add_degree_ratio(float numerator,float denominator,float gravity,float ascending,float descending,const char* degree_label){
    if(!std::isfinite(numerator)||!std::isfinite(denominator)||numerator<=0.0f||denominator<=0.0f)return false;
    return add_degree_cents(ratio_to_cents(numerator,denominator),gravity,ascending,descending,degree_label);
}
float TuningSpace::ratio_to_cents(float numerator,float denominator){if(!std::isfinite(numerator)||!std::isfinite(denominator)||numerator<=0.0f||denominator<=0.0f)return 0.0f;return 1200.0f*std::log2(numerator/denominator);}
float TuningSpace::cents_to_ratio(float cents){if(!std::isfinite(cents))return 1.0f;return std::pow(2.0f,clamp(cents,-24000.0f,24000.0f)/1200.0f);}
void TuningSpace::set_builtin(VocalBuiltinTuning preset){const size_t index=std::min(static_cast<size_t>(preset),std::size(builtins)-1);const auto& d=builtins[index];clear(d.period);builtin=static_cast<VocalBuiltinTuning>(index);copy_label(label,d.name);for(size_t i=0;i<d.count;++i)add_degree_cents(d.degrees[i]);}

void VocalTargetEngine::set_sample_rate(float sample_rate){sample_rate_=std::max(1000.0f,sanitize(sample_rate));}
void VocalTargetEngine::reset(){metrics_={};previous_target_=trajectory_=0.0f;previous_degree_=-1;has_target_=false;}
float VocalTargetEngine::candidate_score(float candidate,const TuningDegree& degree,float observed,float motion)const{
    const bool rising=motion>6.0f,falling=motion<-6.0f;float directional=rising?degree.ascending_gravity:(falling?degree.descending_gravity:1.0f);
    directional=lerp(1.0f,directional,clamp(sanitize(controls_.directionality),0.0f,1.0f));const float gravity=clamp(sanitize(degree.gravity*directional),0.02f,2.0f);const float capture=38.0f+155.0f*gravity;return gravity-std::abs(candidate-observed)/capture;
}
float VocalTargetEngine::process(float observed,float motion,float confidence,float onset){
    observed=sanitize(observed);motion=sanitize(motion);confidence=clamp(sanitize(confidence),0.0f,1.0f);onset=clamp(sanitize(onset),0.0f,1.0f);
    const auto& tuning=controls_.tuning;const uint8_t degree_count=std::min<uint8_t>(tuning.degree_count,static_cast<uint8_t>(TuningSpace::MAX_DEGREES));
    if(degree_count==0)return has_target_?trajectory_:observed;
    const float period=std::isfinite(tuning.period_cents)?std::max(1.0f,tuning.period_cents):1200.0f;const float root=std::isfinite(tuning.root_cents)?tuning.root_cents:0.0f;
    float best_target=observed,best_score=-1e9f;int best_degree=-1;const int center_period=static_cast<int>(std::floor((observed-root)/period));
    for(uint8_t i=0;i<degree_count;++i){const auto& degree=tuning.degrees[i];if(!degree.enabled||!std::isfinite(degree.cents))continue;for(int octave=center_period-2;octave<=center_period+2;++octave){const float candidate=root+degree.cents+static_cast<float>(octave)*period;const float score=candidate_score(candidate,degree,observed,motion);if(score>best_score){best_score=score;best_target=candidate;best_degree=static_cast<int>(i);}}}
    // An all-disabled/corrupt tuning degrades to neutral instead of latching a stale target.
    if(best_degree<0){metrics_.selected_cents=observed;metrics_.trajectory_cents=observed;metrics_.selection_score=0.0f;metrics_.selected_degree=-1;metrics_.switched=false;previous_target_=trajectory_=observed;previous_degree_=-1;has_target_=false;return observed;}
    const float detached=clamp((sanitize(controls_.articulation)+1.0f)*0.5f,0.0f,1.0f);const float onset_threshold=clamp(lerp(0.18f,0.72f,clamp(sanitize(controls_.onset_protection),0.0f,1.0f))-0.55f*detached,0.03f,0.92f);const bool onset_reset=onset>onset_threshold&&confidence>0.35f;
    if(has_target_&&!onset_reset){const bool valid_previous=previous_degree_>=0&&previous_degree_<degree_count&&tuning.degrees[previous_degree_].enabled;const float previous_score=valid_previous?candidate_score(previous_target_,tuning.degrees[previous_degree_],observed,motion):-1e9f;const float history=(1.0f-detached)*(0.22f+0.62f*clamp(sanitize(controls_.target_hysteresis),0.0f,1.0f));if(valid_previous&&best_score<previous_score+history){best_target=previous_target_;best_score=previous_score;best_degree=previous_degree_;}}
    const bool switched=!has_target_||std::abs(best_target-previous_target_)>0.01f;if(switched&&has_target_)++metrics_.switch_count;previous_target_=best_target;previous_degree_=best_degree;if(!has_target_){trajectory_=best_target;has_target_=true;}
    const float connected=1.0f-detached,preserve=clamp(sanitize(controls_.transition_preservation),0.0f,1.0f);const float glide_ms=3.0f+preserve*(12.0f+280.0f*connected)+clamp(sanitize(controls_.portamento),0.0f,1.0f)*420.0f*connected;const float alpha=1.0f-std::exp(-1.0f/(std::max(1.0f,glide_ms)*0.001f*sample_rate_));trajectory_+= (best_target-trajectory_)*alpha;trajectory_=sanitize(trajectory_);
    metrics_.selected_cents=best_target;metrics_.trajectory_cents=trajectory_;metrics_.motion_cents_per_second=motion;metrics_.selection_score=best_score;metrics_.selected_degree=best_degree;metrics_.switched=switched;return trajectory_;
}

} // namespace monkeys_ear
