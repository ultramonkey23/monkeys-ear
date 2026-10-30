#include "monkeys_ear/fdn_reverb.h"
#include "monkeys_ear/delay.h"
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

using monkeys_ear::FDNReverb;
using monkeys_ear::StereoDelay;

namespace {
std::atomic<unsigned> allocation_count{0};
std::atomic<bool> measure_allocations{false};
void* counted_allocate(std::size_t bytes) {
    if(measure_allocations.load(std::memory_order_relaxed))
        allocation_count.fetch_add(1,std::memory_order_relaxed);
    if(void* p=std::malloc(bytes?bytes:1))return p;
    throw std::bad_alloc();
}
bool finite_bits(float value) {
    return (std::bit_cast<std::uint32_t>(value)&0x7f800000u)!=0x7f800000u;
}
void demand(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
}

void* operator new(std::size_t n) {return counted_allocate(n);}
void* operator new[](std::size_t n) {return counted_allocate(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

int main() {
    try {
        for(const float sr:{44100.0f,48000.0f,96000.0f,192000.0f}) {
            FDNReverb reverb;
            reverb.set_sample_rate(sr); // setupProcessing: allocations permitted here
            reverb.set_mix(.85f);
            reverb.set_decay_time(4.0f);
            reverb.reset();
            allocation_count.store(0);
            measure_allocations.store(true);
            double total_energy=0;
            for(int i=0;i<static_cast<int>(sr*2.0f);++i) {
                if((i&63)==0) {
                    const float t=static_cast<float>(i)/sr;
                    reverb.set_room_size(1.45f+1.05f*std::sin(t*9.5f));
                    reverb.set_decay_time(2.0f+4.5f*(.5f+.5f*std::sin(t*5.7f)));
                }
                float l=0.0f,r=0.0f;
                const float input=i<256?.28f*std::sin(.015f*i):0.0f;
                reverb.process(input,input,l,r);
                demand(finite_bits(l)&&finite_bits(r),"nonfinite reverb under room morph");
                demand(std::abs(l)<8.0f&&std::abs(r)<8.0f,"unbounded reverb morph");
                total_energy+=static_cast<double>(l)*l+static_cast<double>(r)*r;
            }
            reverb.reset(); // panic/reset must not allocate either
            measure_allocations.store(false);
            demand(allocation_count.load()==0,"heap allocation in realtime reverb path");
            demand(total_energy>1.e-7,"reverb never emitted audio");
        }

        // Unrelated continuous host parameter changes must preserve the exact
        // time-varying diffusion/reverb state when room/decay are unchanged.
        FDNReverb control,settings_spam;
        control.set_sample_rate(48000.0f);
        settings_spam.set_sample_rate(48000.0f);
        control.set_mix(1.0f);settings_spam.set_mix(1.0f);
        control.set_room_size(1.18f);settings_spam.set_room_size(1.18f);
        control.set_decay_time(3.5f);settings_spam.set_decay_time(3.5f);
        double error=0;
        for(int i=0;i<96000;++i) {
            settings_spam.set_room_size(1.18f);
            settings_spam.set_decay_time(3.5f);
            float a=0,b=0,c=0,d=0;
            const float in=i<800?.3f*std::sin(.071f*i):0.f;
            control.process(in,in,a,b);
            settings_spam.process(in,in,c,d);
            error+=std::abs(a-c)+std::abs(b-d);
        }
        demand(error<1.e-7,"unchanged room/decay setter altered a reverb tail");

        // Full dry bypass must not freeze the internal echo history.
        FDNReverb always_wet,hidden_wet;
        always_wet.set_sample_rate(48000.f);hidden_wet.set_sample_rate(48000.f);
        always_wet.set_mix(.60f);hidden_wet.set_mix(0.f);
        always_wet.set_decay_time(3.0f);hidden_wet.set_decay_time(3.0f);
        for(int i=0;i<20000;++i) {
            float a=0,b=0,c=0,d=0;
            const float input=i<128?.35f*std::sin(.04f*i):0.0f;
            always_wet.process(input,input,a,b);
            hidden_wet.process(input,input,c,d);
            demand(c==input&&d==input,"zero-wet return not transparent");
        }
        hidden_wet.set_mix(.60f);
        double mismatch=0,late_energy=0;
        for(int i=0;i<96000;++i) {
            float a=0,b=0,c=0,d=0;
            always_wet.process(0,0,a,b);
            hidden_wet.process(0,0,c,d);
            mismatch+=std::abs(a-c)+std::abs(b-d);
            if(i>48000)late_energy+=static_cast<double>(a)*a+b*b;
        }
        demand(mismatch<1.e-7,"re-enabling wet mix resumed a frozen tail");
        demand(late_energy>1.e-12,"long reverb was silent before its documented tail");
        // A real three-second echo must arrive after three seconds even at
        // 96/192 kHz. The previous 192000-sample fixed ring wrapped it to
        // a shorter, incorrect delay at elevated host sample rates.
        for(const float sr:{96000.0f,192000.0f}) {
            StereoDelay delay;
            delay.set_sample_rate(sr); // off audio thread: capacity may grow
            delay.set_time_left(3.0f);
            delay.set_time_right(3.0f);
            delay.set_feedback(0.0f);
            delay.set_mix(1.0f);
            delay.reset(); // snap targets for deterministic transport start
            const int target=static_cast<int>(sr*3.0f);
            float late=0.f,early=0.f;
            allocation_count.store(0);
            measure_allocations.store(true);
            for(int i=0;i<target+64;++i) {
                float l=0,r=0;
                delay.process(i==0?1.0f:0.0f,0.0f,l,r);
                demand(finite_bits(l)&&finite_bits(r),"nonfinite 3s stereo delay");
                if(i<target-3)early=std::max(early,std::abs(l));
                if(i>=target-1&&i<=target+1)late=std::max(late,std::abs(l));
            }
            measure_allocations.store(false);
            demand(allocation_count.load()==0,"delay processing allocated heap");
            demand(early<1.e-7f,"3s echo wrapped to earlier ring offset");
            demand(late>.99f,"3s echo missing at requested sample offset");
        }

        // Automated time/feedback changes are realtime-safe and remain finite.
        StereoDelay tape;
        tape.set_sample_rate(96000.f);
        tape.set_feedback(.84f);
        tape.set_mix(.75f);
        allocation_count.store(0);
        measure_allocations.store(true);
        double tape_energy=0;
        for(int i=0;i<192000;++i) {
            if((i&63)==0){
                const float phase=.001f*i;
                tape.set_time_left(1.5f+1.45f*std::sin(phase));
                tape.set_time_right(1.6f+1.40f*std::cos(phase));
            }
            float l=0,r=0;
            tape.process(i<1000?.5f*std::sin(.12f*i):0.0f,0,l,r);
            demand(finite_bits(l)&&finite_bits(r),"nonfinite tape echo during time morph");
            demand(std::abs(l)<8.f&&std::abs(r)<8.f,"unstable tape echo");
            tape_energy+=static_cast<double>(l)*l+static_cast<double>(r)*r;
        }
        measure_allocations.store(false);
        demand(allocation_count.load()==0,"time automation allocated heap");
        demand(tape_energy>1.e-7,"tape signal unexpectedly silent");

        // Dry mix never stops history advancement, so returning a wet control
        // restores the correct *present* echo rather than an old frozen one.
        StereoDelay wet,dry;
        wet.set_sample_rate(48000.f);dry.set_sample_rate(48000.f);
        wet.set_feedback(.65f);dry.set_feedback(.65f);
        wet.set_time_left(.11f);dry.set_time_left(.11f);
        wet.set_time_right(.13f);dry.set_time_right(.13f);
        wet.set_mix(.7f);dry.set_mix(0.f);
        wet.reset();dry.reset();
        for(int i=0;i<12000;++i) {
            float a=0,b=0,c=0,d=0;
            const float impulse=i==0?.8f:0.f;
            wet.process(impulse,impulse,a,b);
            dry.process(impulse,impulse,c,d);
            demand(c==impulse&&d==impulse,"zero-wet delay was not transparent");
        }
        dry.set_mix(.7f);
        double delay_mismatch=0;
        for(int i=0;i<60000;++i){
            float a=0,b=0,c=0,d=0;
            wet.process(0,0,a,b);
            dry.process(0,0,c,d);
            delay_mismatch+=std::abs(a-c)+std::abs(b-d);
        }
        demand(delay_mismatch<1.e-7,"dry delay history froze while wet mix zero");

        std::cout<<"PASS: exact product FDN and stereo delay; 44.1/48/96/192 kHz,"
                    " zero realtime allocation, smooth time/room morph,"
                    " no resets, 3s echoes, dry spillover and finite tails\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
