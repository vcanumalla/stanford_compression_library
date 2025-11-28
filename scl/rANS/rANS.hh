#include <string>
#include <iostream>
#include <tuple>
#include <vector>
using namespace std;

struct Frequencies {
    unordered_map<char, uint32_t> freq_dict;

    uint32_t total_freq() const { 
        uint32_t sum = 0;
        for (const auto& kv : freq_dict) {
            sum += kv.second;
        }
        return sum;
    };

    size_t size() const { return freq_dict.size(); }

    uint32_t frequency(char s) const { return freq_dict.at(s); }

    unordered_map<char, uint32_t> cumulative_freq_dict() const {
        unordered_map<char, uint32_t> cum_freq_dict;
        uint32_t sum = 0;
        for (const auto& kv : freq_dict) {
            cum_freq_dict[kv.first] = sum;
            sum += kv.second;
        }
        return cum_freq_dict;
    }

    vector<uint32_t> alphabet() {
        vector<uint32_t> keys;
        for (const auto& kv : freq_dict) {
            keys.push_back(kv.first);
        }
        return keys;
    }
};

uint32_t get_bit_width(uint32_t x) {
    if (x == 0) return 1;
    uint32_t width = 32u - __builtin_clz(x);
    return width;
}

struct rANSParams {
    Frequencies freqs;
    uint32_t DATA_BLOCK_SIZE_BITS = 32;
    uint32_t NUM_BITS_OUT = 1u; // hardcoded to work with NUM_BITS_OUT=1
    uint32_t RANGE_FACTOR = 1u << 16;

    uint32_t M, L, H;
    
    // alphabet size
    uint32_t K;

    vector<uint32_t> min_shrunk_state;
    vector<uint32_t> max_shrunk_state;

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

// note: using vector<bool> to encode bitarrays, operations done on bitarrays will use bitwise operations

class encoder {
    private:
        rANSParams params;

    public:
        encoder(rANSParams rans_params);
        uint32_t base_encode_step(char s, uint32_t state);
        vector<bool> shrink_state(uint32_t& state, char next_symbol);
        vector<bool> encode_symbol(uint32_t& state, char next_symbol);
        vector<bool> encode_block(string data_block);
};

class decoder {
    private:
        rANSParams params;

    public:
        decoder(rANSParams rans_params);
        uint32_t find_bin(vector<uint32_t> cum_freq_list, uint32_t slot);
        char base_decode_step(uint32_t& state);
        uint32_t expand_state(uint32_t& state, vector<bool>& encoded_bitarray);
        tuple<char, uint32_t> decode_symbol(uint32_t& state, vector<bool>& encoded_bitarray);
        tuple<string, uint32_t> decode_block(vector<bool>& encoded_bitarray);
};