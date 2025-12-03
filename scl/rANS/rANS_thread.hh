#include <string>
#include <iostream>
#include <tuple>
#include <vector>
#include <map>
#include <array>
#include <bitset>
#include <thread>

using namespace std;

struct ransDecSym {
    uint32_t freq;
    uint32_t cum_freq;
    char s;
};

struct Frequencies {
    Frequencies(map<char, uint32_t> freq_dict_) : freq_dict(freq_dict_) {};

    map<char, uint32_t> freq_dict;

    uint32_t total_freq() const { 
        uint32_t sum = 0;
        for (const auto& kv : freq_dict) {
            sum += kv.second;
        }
        return sum;
    };

    size_t size() const { return freq_dict.size(); }

    uint32_t frequency(char s) const { return freq_dict.at(s); }

    map<char, uint32_t> cumulative_freq_dict() const {
        map<char, uint32_t> cum_freq_dict;
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
    uint32_t NUM_BITS_OUT = 8u; // one byte at a time
    uint32_t RANGE_FACTOR = 1u << 16;

    uint32_t M, L, H;
    
    // alphabet size
    uint32_t K;

    map<char, uint32_t> min_shrunk_state;
    map<char, uint32_t> max_shrunk_state;

    uint32_t INITIAL_STATE;
    uint32_t NUM_STATE_BITS;
    uint32_t BITS_OUT_MASK;

    vector<ransDecSym> decode_table;

    rANSParams(const Frequencies &freqs_, uint32_t DATA_BLOCK_SIZE_BITS_, uint32_t RANGE_FACTOR_) : 
        freqs(freqs_), DATA_BLOCK_SIZE_BITS(DATA_BLOCK_SIZE_BITS_), RANGE_FACTOR(RANGE_FACTOR_) {
        
        M = freqs.total_freq(); // M = sum of frequencies
        L = RANGE_FACTOR * M;
        H = L * (1u << NUM_BITS_OUT) - 1u;

        K = freqs.size();

        for (const auto& kv : freqs.freq_dict) {
            uint32_t f = kv.second;
            min_shrunk_state[kv.first] = RANGE_FACTOR * f;
            max_shrunk_state[kv.first] = RANGE_FACTOR * f * (1u << NUM_BITS_OUT) - 1u;
        }

        INITIAL_STATE = L;
        NUM_STATE_BITS = get_bit_width(H);
        BITS_OUT_MASK = (1u << NUM_BITS_OUT) - 1u;

        decode_table.resize(M);

        map<char, uint32_t> cum_freq = freqs.cumulative_freq_dict();
        map<char, uint32_t> freq = freqs.freq_dict;
        vector<uint32_t> alphabet = freqs.alphabet();

        for (auto& kv : cum_freq) {
            char s = kv.first;
            uint32_t f = freq.at(s);
            uint32_t cf = kv.second;
            for (uint32_t i = cf; i < cf + f; i++) {
                ransDecSym ds;
                ds.freq = f;
                ds.cum_freq = cf;
                ds.s = s;
                decode_table[i] = ds;
            }
        }
    }

};

class encoder {
    private:
        rANSParams params;
        inline void base_encode_step(char s, uint32_t& state);
        inline void shrink_state(uint32_t& state, char next_symbol, uint8_t** ptr);
        void encode_symbol(uint32_t& state, char next_symbol, uint8_t** ptr);
        void encode_half(const string& data, size_t start_idx, size_t step, uint32_t& state, uint8_t** ptr);

    public:
        encoder(rANSParams rans_params);
        tuple<uint8_t*, uint8_t*, uint8_t*> encode(string data, size_t* len);
};

class decoder {
    private:
        rANSParams params;
        uint32_t find_bin(vector<uint32_t> cum_freq_list, uint32_t slot);
        char base_decode_step(uint32_t& state);
        inline void expand_state(uint32_t& state, uint8_t** ptr);
        char decode_symbol(uint32_t& state, uint8_t** ptr);
        void decode_half(uint32_t data_size, uint8_t** ptr, uint32_t state, string& result, bool is_state1);

    public:
        decoder(rANSParams rans_params);
        string decode(uint8_t** ptr0, uint8_t** ptr1);
};