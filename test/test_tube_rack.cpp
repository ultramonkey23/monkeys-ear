#include "monkeys_ear/engine.h"
#include "monkeys_ear/tube_rack.h"
#include "monkeys_ear/preset.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

using namespace monkeys_ear;
#define CHECK(ok,why) do { if(!(ok)){std::cerr<<"[FAIL] "<<why<<"\n"; return 1;} } while(false)

int main() {
    static_assert(kTubeFlavorCount>=10,"ten selectable tubes are a product requirement");
    TubeRack rack;rack.set_sample_rate(48000.0f);
    TubeRackControls c{};
    c.enabled=true;
    for(auto& slot:c.slots){slot.enabled=false;slot.mix=1.f;}
    c.slots[0].enabled=true;
    c.slots[0].drive=.55f;
    c.slots[0].sag=.45f;
    c.shared_supply=.5f;
    std::array<std::array<double,3>,kTubeFlavorCount> signatures{};
    for(int f=0;f<kTubeFlavorCount;++f){
        c.slots[0].flavor=f;rack.set_controls(c);rack.reset();
        double s1=0.0,s2=0.0,s3=0.0;
        for(int i=0;i<8192;++i){
            const float x=.27f*std::sin(TWO_PI*137.0f*i/48000.f);
            const float y=rack.process(x);
            CHECK(std::isfinite(y),"tube output finite across all named profiles");
            if(i>=4096){s1+=y*y;s2+=std::abs(y);s3+=y*y*y;}
        }
        signatures[f]={s1/4096.0,s2/4096.0,s3/4096.0};
    }
    for(int a=0;a<kTubeFlavorCount;++a)
        for(int b=a+1;b<kTubeFlavorCount;++b){
            const double d=std::abs(signatures[a][0]-signatures[b][0])
                          +std::abs(signatures[a][1]-signatures[b][1])
                          +std::abs(signatures[a][2]-signatures[b][2]);
            CHECK(d>1.e-8,"individual tube profiles must not collapse to duplicates");
        }

    // Actual 4-position topology, cathode/supply history and safe bypass.
    c.slots[0].flavor=0;
    for(int i=0;i<4;++i){c.slots[i].enabled=true;c.slots[i].flavor=i*3;c.slots[i].mix=.7f;}
    c.shared_supply=1.0f;rack.set_controls(c);rack.reset();
    for(int i=0;i<48000;++i)CHECK(std::isfinite(rack.process(.8f)),"steady high drive finite");
    const float charged=rack.supply_envelope();
    CHECK(charged>.05f,"supply interaction accumulates shared history");
    for(int i=0;i<48000;++i)rack.process(0.f);
    CHECK(rack.supply_envelope()<charged*.05f,"supply recovers in silence");
    c.parallel=true;c.slots[3].flavor=13;rack.set_controls(c);
    for(int i=0;i<512;++i)CHECK(std::isfinite(rack.process(.45f)),"parallel route finite");
    c.enabled=false;rack.set_controls(c);
    CHECK(rack.process(.42f)==.42f,"rack disabled is exact dry");
    for(float x:{std::numeric_limits<float>::infinity(),-1.e30f,
                 std::numeric_limits<float>::quiet_NaN()})
        CHECK(std::isfinite(rack.process(x)),"hostile input finite");

    // Full native preset: no parallel demo state; older patches still default off.
    const auto legacy=PresetManager::create_factory_lead();
    CHECK(!legacy.tube_rack.enabled,"legacy factory sounds keep their tube path");
    PresetData loaded;CHECK(loaded.deserialize(legacy.serialize()),"old-style preset parsed");
    CHECK(!loaded.tube_rack.enabled,"legacy defaults preserved");
    for(auto creator:{&PresetManager::create_tube_foundry,
                      &PresetManager::create_clockwork_sub,
                      &PresetManager::create_glass_chamber}){
        auto patch=creator();
        CHECK(patch.tube_rack.enabled,"creative factory patch uses the real rack");
        CHECK(patch.lfo_tempo_sync,"factory patch uses musical divisions");
        PresetData roundtrip;
        CHECK(roundtrip.deserialize(patch.serialize()),"real preset text roundtrip");
        CHECK(roundtrip.tube_rack.enabled&&roundtrip.lfo_tempo_sync,"rack and tempo survive recall");
        for(int i=0;i<4;++i)
            CHECK(roundtrip.tube_rack.slots[i].flavor==patch.tube_rack.slots[i].flavor,
                  "four tube types survive patch recall");
    }

    MonkeysEarEngine synth;synth.init(48000.f,128);
    synth.load_preset(PresetManager::create_tube_foundry());
    synth.set_parameter_normalized(163,.82f);
    synth.set_parameter_normalized(164,.51f);
    synth.set_parameter_normalized(167,.37f);
    synth.set_parameter_normalized(171,9.f/11.f);
    synth.set_parameter_normalized(129+8*3+1,1.0f);
    const auto& updated=synth.get_current_preset();
    CHECK(std::abs(updated.amp_sustain-.82f)<1.e-5f,"amp sustain is automatable");
    CHECK(std::abs(updated.filter_sustain-.37f)<1.e-5f,"filter sustain is automatable");
    CHECK(updated.amp_release>1.f,"amp release maps to musical seconds");
    CHECK(updated.lfo_subdivision==9,"triplet subdivision changes actual preset state");
    CHECK(updated.tube_rack.slots[3].flavor==kTubeFlavorCount-1,"last tube option is available");
    CHECK(synth.get_current_preset().serialize().find("LFO_SUBDIVISION=9")!=std::string::npos,
          "subdivision saved with same synth patch");

    std::array<float,128> dry{},left{},right{};
    synth.handle_midi_note_on(48,.8f);
    double energy=0.0;
    for(int b=0;b<80;++b){
        synth.process_block(dry.data(),dry.data(),left.data(),right.data(),128);
        for(int i=0;i<128;++i){
            CHECK(std::isfinite(left[i])&&std::isfinite(right[i]),"full synth output finite");
            energy+=left[i]*left[i];
        }
    }
    CHECK(energy>1.e-8,"tube rack connected to actual synthesized audio");
    synth.handle_midi_note_off(48);
    for(int b=0;b<30;++b)synth.process_block(dry.data(),dry.data(),left.data(),right.data(),128);
    synth.set_parameter_normalized(124,0.0f);
    CHECK(!synth.get_current_preset().tube_rack.enabled,"host rack bypass returns to legacy path");
    std::cout<<"PASS: 14 tubes, four slots, shared power state, old presets, new patches, ADSR, tempo subdivisions, rendered synth\n";
    return 0;
}
