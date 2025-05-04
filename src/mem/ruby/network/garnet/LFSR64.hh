#ifndef LFSR64_H
#define LFSR64_H
#include <bitset>
#include <iostream>
#include <random>
#include <vector>

#include "params/LFSR64.hh"
#include "sim/clocked_object.hh"

namespace gem5{

namespace ruby{

namespace garnet{

class LFSR64 : public ClockedObject
{
    private:
        EventFunctionWrapper event;

        uint64_t state;
        Cycles m_latency;
        uint32_t m_seed;

    public:
        typedef LFSR64Params Params;
        LFSR64(const Params &p);
        ~LFSR64() = default;

        void nextBit();

        uint16_t generate16Bitstream(int numOnes);
};

} // namespace garnet
} // namespace ruby
} // namespace gem5
#endif
