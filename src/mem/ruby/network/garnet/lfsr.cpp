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

uint16_t generate16Bitstream(LFSR64& lfsr, int numOnes){
    std::vector<int> bits(16, 0);

    for (int i = 0; i < numOnes; ++i){
        bits[i] = 1;
    }

    // shuffle the bits
    for (int i = 15; i > 0; --i){
        int j = lfsr.nextBit() % (i + 1);
        std::swap(bits[i], bits[j]);
    }

    // convert the bits vector into an uint16_t number
    uint16_t result = 0;
    for (int i = 0; i < 16; ++i){
        result |= (bits[i] << i);
    }
    return result;
}

/*
// an example use
int main(){
    LFSR64 lfsr(0x7a3d9f4c1e0b8a2du);       // create a LFSR instance
    int numOnes = 12;                       // predefine the number of 1s in the bitstream

    for(int i = 0; i < 1000; ++i){
        uint16_t val = generate16Bitstream(lfsr, numOnes);
        std::bitset<16> bits(val);
        std::cout << bits << " " << bits.count() << std::endl;
    }
    return 0;
}
 */