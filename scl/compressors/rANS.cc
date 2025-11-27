// C++ Implementation of the rANS encoder
#include <cstdint>
#include <vector>
#include "rANS.hh"

struct Frequencies {
    int total_freq() const { return 0; };
    size_t size() const { return 0; }

    uint32_t frequency(size_t i) const { return 1; }
};

uint32_t get_bit_width(uint32_t x) {
    if (x == 0) return 1;
    uint32_t width = 32u - __builtin_clz(x);
    return width;
}

struct rANSParams {
    Frequencies freqs;
    uint32_t DATA_BLOCK_SIZE_BITS = 32;
    uint32_t NUM_BITS_OUT = 1u;
    uint32_t RANGE_FACTOR = 1u << 16;

    uint32_t M, L, H;
    
    // alphabet size
    uint32_t K;

    std::vector<uint32_t> min_shrunk_state;
    std::vector<uint32_t> max_shrunk_state;

    uint32_t INITIAL_STATE;
    uint32_t NUM_STATE_BITS;
    uint32_t BITS_OUT_MASK;

    rANSParams(const Frequencies &freqs_) : freqs(freqs_){
        
        M = freqs.total_freq(); // M = sum of frequencies
        L = RANGE_FACTOR * M;
        H = L * (1u << NUM_BITS_OUT) - 1u;

        K = freqs.size();
        min_shrunk_state.resize(K);
        max_shrunk_state.resize(K);

        for (size_t i = 0; i < K; ++i) {
            uint32_t f = freqs.frequency(i);
            min_shrunk_state[i] = RANGE_FACTOR * f;
            max_shrunk_state[i] = RANGE_FACTOR * f * (1u << NUM_BITS_OUT) - 1u;
        }

        INITIAL_STATE = L;
        NUM_STATE_BITS = get_bit_width(H);
        BITS_OUT_MASK = (1u << NUM_BITS_OUT) - 1u;
    }

};

encoder::encoder(rANSParams params) :
    params(params)
    {};


