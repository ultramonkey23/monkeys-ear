#include "vst3/snapshot_replay.h"
#include <array>
#include <cstddef>
#include <iostream>

int main() {
    constexpr std::size_t n=178;
    std::array<float,n> saved{};
    for(std::size_t i=0;i<n;++i) saved[i]=float((i*17)%101)/100.f;
    saved[79]=0.6f; saved[173]=0.69f;
    saved[0]=0.11f; saved[75]=0.91f;
    saved[124]=1.0f; saved[175]=0.08f;
    saved[176]=0.21f; saved[177]=0.94f;
    std::array<float,n> applied{};
    std::array<int,n> calls{};
    std::array<std::size_t,n> order{};
    std::size_t position=0;
    auto setter=[&](std::size_t id,float v){
        if(position>=n)return;
        order[position++]=id;
        ++calls[id];
        if(id==79||id==173)applied.fill(0.5f);
        else applied[id]=v;
    };
    monkeys_ear::vst3::replay_patch_snapshot(saved,setter);
    if(position!=n||order[0]!=79||order[1]!=173)return 1;
    for(std::size_t i=0;i<n;++i){
        if(calls[i]!=1)return 2;
        if(i!=79&&i!=173&&applied[i]!=saved[i])return 3;
    }
    if(saved[0]!=.11f||saved[175]!=.08f)return 4;
    std::cout << "PASS: production snapshot replay places base selectors first, then every user edit\n";
}
