#ifndef LFSR64_H
#define LFSR64_H
#include <iostream>
#include <bitset>
#include <vector>
#include <random>

class LFSR64{
    private:
        uint64_t state;

    public:
        LFSR64(uint64_t seed) : state(seed ? seed : 1){}       // if 64 bits, one bit presenting one register are not initialized, value 1 is used

        uint16_t nextBit(){
            // primitive polynomial x^64 + x^63 + x^61 + x^60 + 1
            uint64_t bit = ((state >> 63) ^ (state >> 61) ^ (state >> 60) ^ (state >> 0)) & 1;      // bit shifting into the MSB
            state = (state >> 1) | (bit << 63);     // state of the lfsr after shifting
            uint16_t lower_16bits = static_cast<uint16_t>(state & 0x000000000000FFFF);
            return static_cast<uint16_t>(lower_16bits);
        }
};

uint16_t generate16Bitstream(LFSR64& lfsr, int numOnes);

#endif