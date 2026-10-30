#include "monkeys_ear/types.h"
#include "monkeys_ear/filter.h"
#include "monkeys_ear/drive_tube.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace monkeys_ear;

[[noreturn]] static void fail(const char* name) {
    std::fprintf(stderr,"[FAIL] %s\n",name);
    std::exit(1);
}
static void require(bool good,const char* name) {
    if(!good)fail(name);
}
static double sine_rms(float rate,float hz,float drive) {
    TubeDriveStage tube;
    tube.set_sample_rate(rate);tube.set_drive(drive);tube.set_mix(1.f);
    tube.set_bias(.23f);tube.set_memory_sag(.5f);
    double power=0;
    const int settle=static_cast<int>(rate*.08f);
    const int count=static_cast<int>(rate*.32f);
    for(int n=0;n<settle+count;++n) {
        const float x=.2f*std::sin(TWO_PI*hz*n/rate);
        const float y=tube.process(x);
        require(std::isfinite(y),"tube remains finite");
        if(n>=settle)power+=double(y)*y;
    }
    return std::sqrt(power/count);
}
static double harmonic(float frequency,float drive,int harmonic_index) {
    constexpr int rate=48000;
    TubeDriveStage tube;tube.set_sample_rate(float(rate));
    tube.set_drive(drive);tube.set_mix(1.f);
    tube.set_bias(.27f);tube.set_memory_sag(.45f);
    double c=0,s=0;
    const int nstart=rate/4,stop=nstart+rate;
    for(int n=0;n<stop;++n) {
        const float x=.16f*std::sin(TWO_PI*frequency*n/rate);
        const float y=tube.process(x);
        if(n>=nstart) {
            const double phase=TWO_PI*harmonic_index*frequency*n/rate;
            c+=y*std::cos(phase);s+=y*std::sin(phase);
        }
    }
    return 2*std::hypot(c,s)/rate;
}
int main() {
    // Old fast_tanh exceeded unity at x=9 (1.2857). Bound every voice,
    // filter and nonlinear module using the common rational transfer.
    float previous=-1.f;
    for(int n=0;n<=60000;++n) {
        const float x=-30.f+n*.001f;
        const float y=fast_tanh(x);
        require(std::isfinite(y)&&y>=-1.000001f&&y<=1.000001f,
                "fast_tanh amplitude is strictly bounded");
        require(y+1.e-6f>=previous,"fast_tanh never folds back at high drive");
        previous=y;
    }
    require(fast_tanh(std::numeric_limits<float>::infinity())==0.f,
            "Inf cannot poison rational saturator");
    require(fast_tanh(std::numeric_limits<float>::quiet_NaN())==0.f,
            "NaN cannot poison rational saturator");

    // Biased triodes should create even harmonics, NOT a constant tone/DC
    // when the synth is idle or after the filter memory has settled.
    for(float rate:{44100.f,48000.f,96000.f,192000.f}){
        TubeDriveStage tube;
        tube.set_sample_rate(rate);
        tube.set_mix(1.f);tube.set_bias(.9f);tube.set_memory_sag(.8f);
        for(float heat:{0.f,.3f,.7f,1.f}){
            tube.set_drive(heat);tube.reset();
            for(int n=0;n<8192;++n)
                require(tube.process(0.f)==0.f,
                        "silence emits no bias-dependent DC from tube");
        }
        tube.set_drive(.85f);tube.set_mix(.8f);tube.reset();
        for(int n=0;n<12000;++n) {
            const float x=.6f*std::sin(TWO_PI*390.f*n/rate);
            const float y=tube.process(x);
            require(std::isfinite(y)&&std::abs(y)<4.f,
                    "high-drive tube remains finite/bounded");
        }
    }
    // Distinct harmonic color: second harmonic from operating-point
    // asymmetry; stronger odd harmonics at high drive than near-clean.
    const double clean_second=harmonic(250.f,.08f,2);
    const double dirty_second=harmonic(250.f,.83f,2);
    const double clean_third=harmonic(250.f,.08f,3);
    const double dirty_third=harmonic(250.f,.83f,3);
    require(dirty_second>1.e-4,"dirty character retains even harmonics");
    require(dirty_third>clean_third*1.1,"Heat materially grows odd harmonics");

    // Drive-dependent wet bandwidth shapes warmth (less extreme treble),
    // while a deliberately dark filter is lower-energy at 4 kHz.
    const double warm_low=sine_rms(48000.f,1000.f,.90f);
    const double warm_high=sine_rms(48000.f,9000.f,.90f);
    require(warm_high<warm_low*.92,"hot tube damps topmost grit");

    const auto filter_rms=[](float cutoff,float sine_hz,float drive){
        StateVariableFilter f;
        f.set_sample_rate(48000.f);f.set_mode(FilterMode::Lowpass);
        f.set_cutoff(cutoff);f.set_resonance(.1f);f.set_drive(drive);
        f.reset();
        double p=0;
        for(int i=0;i<24000;++i) {
            const float in=.3f*std::sin(TWO_PI*sine_hz*i/48000.f);
            const float y=f.process(in);
            require(std::isfinite(y),"filter remains finite");
            if(i>=12000)p+=double(y)*y;
        }
        return std::sqrt(p/12000);
    };
    const double dark=filter_rms(700.f,4000.f,0.f);
    const double bright=filter_rms(12000.f,4000.f,0.f);
    require(bright>dark*3.0,"Brightness preserves useful high-frequency range");

    StateVariableFilter moving;
    moving.set_sample_rate(48000.f);moving.set_mode(FilterMode::Lowpass);
    moving.set_cutoff(450.f);moving.set_resonance(.85f);moving.set_drive(0.f);
    moving.reset();
    for(int n=0;n<25000;++n) {
        if(n==4000)moving.set_cutoff(18000.f);
        if(n==5000)moving.set_cutoff(140.f);
        if(n==8000)moving.set_drive(.8f);
        if(n==13000)moving.set_drive(0.f);
        const float input=.65f*std::sin(TWO_PI*280.f*n/48000.f);
        const float y=moving.process(input,.5f*std::sin(TWO_PI*3*n/48000.f),.13f);
        require(std::isfinite(y)&&std::abs(y)<8.f,
                "modulated resonance and cutoff remain stable across drive edits");
    }
    std::printf("PASS sound-quality primitives: saturation bounded, silence DC-free, "
                "harmonics 2nd clean %.5f dirty %.5f, 3rd clean %.5f dirty %.5f; "
                "warm wet RMS 1k %.5f 9k %.5f; dark/bright 4k %.5f/%.5f\n",
                clean_second,dirty_second,clean_third,dirty_third,
                warm_low,warm_high,dark,bright);
}
