#include "mem/ruby/network/garnet/LFSR64.hh"

#include <bitset>
#include <iostream>
#include <random>
#include <vector>

#include "sim/eventq.hh"

namespace gem5{

namespace ruby{

namespace garnet{

LFSR64::LFSR64(const Params &p)
    : ClockedObject(p),
    event([this] {nextBit();}, name() + ".event",
            false, EventFunctionWrapper::Maximum_Pri),
            // handle before any routing decisions are made
    state(p.seed),
    m_latency(p.latency)
{
}

void LFSR64::nextBit(){
    // primitive polynomial x^64 + x^63 + x^61 + x^60 + 1
    uint64_t bit = ((state >> 63) ^ (state >> 61) ^
                    (state >> 60) ^ (state >> 0)) & 1;
                    // bit shifting into the MSB
    // state of the lfsr after shifting
    state = (state >> 1) | (bit << 63);
    Tick tick_delay = cyclesToTicks(m_latency);
    schedule(event, curTick() + tick_delay);
}
uint16_t LFSR64::generate16Bitstream(int numOnes){
    std::vector<int> bits(16, 0);

    for (int i = 0; i < numOnes; ++i){
        bits[i] = 1;
    }

    // shuffle the bits
    for (int i = 15; i > 0; --i){
        int j = static_cast<uint16_t>(state & 0xFFFF) % (i + 1);
        std::swap(bits[i], bits[j]);
    }

    // convert the bits vector into an uint16_t number
    uint16_t result = 0;
    for (int i = 0; i < 16; ++i){
        result |= (bits[i] << i);
    }
    return result;
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
