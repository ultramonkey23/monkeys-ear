#pragma once

#include <array>
#include <cstddef>

namespace monkeys_ear {

// Fixed-capacity held-key bookkeeping for one legato voice. This is not
// a sequencer or a second voice allocator; it only retains physical key
// identity while the existing SynthVoice produces the sound.
class MonoKeyStack final {
public:
    struct Key {
        int note=0,channel=0,note_id=-1;
        float velocity=0.0f;
    };
    struct Release {
        bool found=false;
        bool was_top=false;
        bool has_next=false;
        Key next{};
    };
    void clear() noexcept {size_=0;}
    std::size_t size() const noexcept {return size_;}
    void push(Key key) noexcept {
        if(size_==keys_.size()){
            for(std::size_t i=1;i<size_;++i)keys_[i-1]=keys_[i];
            --size_;
        }
        keys_[size_++]=key;
    }
    Release release(int note,int channel,int note_id) noexcept {
        int found=-1;
        for(int i=static_cast<int>(size_)-1;i>=0;--i){
            const Key& k=keys_[static_cast<std::size_t>(i)];
            if(k.note!=note||(channel>=0&&k.channel!=channel)||
               (note_id>=0&&k.note_id!=note_id))continue;
            found=i;break;
        }
        if(found<0)return {};
        const bool top=static_cast<std::size_t>(found)==size_-1;
        for(std::size_t i=static_cast<std::size_t>(found)+1;i<size_;++i)
            keys_[i-1]=keys_[i];
        --size_;
        Release result{};
        result.found=true;
        result.was_top=top;
        if(top&&size_>0){
            result.has_next=true;
            result.next=keys_[size_-1];
        }
        return result;
    }
private:
    std::array<Key,32> keys_{};
    std::size_t size_=0;
};

} // namespace monkeys_ear
