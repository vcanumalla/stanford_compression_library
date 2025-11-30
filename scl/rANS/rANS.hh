#include <string>
#include <iostream>
#include <tuple>
#include <vector>
#include <map>
#include <array>
#include <bitset>
using namespace std;

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
    uint32_t NUM_BITS_OUT = 1u; // hardcoded to work with NUM_BITS_OUT=1
    uint32_t RANGE_FACTOR = 1u << 16;

    uint32_t M, L, H;
    
    // alphabet size
    uint32_t K;

    map<char, uint32_t> min_shrunk_state;
    map<char, uint32_t> max_shrunk_state;

    uint32_t INITIAL_STATE;
    uint32_t NUM_STATE_BITS;
    uint32_t BITS_OUT_MASK;

    rANSParams(const Frequencies &freqs_, uint32_t DATA_BLOCK_SIZE_BITS_, uint32_t RANGE_FACTOR_) : 
        freqs(freqs_), DATA_BLOCK_SIZE_BITS(DATA_BLOCK_SIZE_BITS_), RANGE_FACTOR(RANGE_FACTOR_) {
        
        M = freqs.total_freq(); // M = sum of frequencies
        L = RANGE_FACTOR * M;
        H = L * (1u << NUM_BITS_OUT) - 1u;

        K = freqs.size();

        uint32_t i = 0;
        for (const auto& kv : freqs.freq_dict) {
            uint32_t f = kv.second;
            min_shrunk_state[kv.first] = RANGE_FACTOR * f;
            max_shrunk_state[kv.first] = RANGE_FACTOR * f * (1u << NUM_BITS_OUT) - 1u;
            i++;
        }

        INITIAL_STATE = L;
        NUM_STATE_BITS = get_bit_width(H);
        BITS_OUT_MASK = (1u << NUM_BITS_OUT) - 1u;
    }

};
class BitArray {
    private:
        vector<bool> bits;

    public:
        // Default constructor - creates empty bitarray
        BitArray() : bits() {}

        // Constructor from vector of bools
        BitArray(const vector<bool>& bits_) : bits(bits_) {}

        // Constructor from uint32_t with optional bit_width
        BitArray(uint32_t x, uint32_t bit_width = 0) {
            if (bit_width == 0) {
                // Calculate minimum bit width needed
                if (x == 0) {
                    bit_width = 1;
                } else {
                    bit_width = 32u - __builtin_clz(x);
                }
            }
            bits.resize(bit_width);
            for (uint32_t i = 0; i < bit_width; i++) {
                bits[i] = (x >> i) & 1u;
            }
        }

        // Get the number of bits
        size_t size() const {
            return bits.size();
        }

        // Access individual bits
        bool operator[](size_t index) const {
            return bits[index];
        }

        // Get bit width needed to represent a uint32_t
        static uint32_t get_bit_width(uint32_t x) {
            if (x == 0) return 1;
            return 32u - __builtin_clz(x);
        }

        // Convert uint32_t to BitArray (static method)
        static BitArray uint_to_bitarray(uint32_t x, uint32_t bit_width = 0) {
            return BitArray(x, bit_width);
        }

        // Convert BitArray to uint32_t
        static uint32_t bitarray_to_uint(const BitArray& bitarray) {
            uint32_t result = 0;
            for (size_t i = 0; i < bitarray.bits.size(); i++) {
                if (bitarray.bits[i]) {
                    result |= (1u << i);
                }
            }
            return result;
        }

        // Instance method version of bitarray_to_uint
        uint32_t to_uint() const {
            return bitarray_to_uint(*this);
        }
};
// note: using custom class to encode bitarrays, operations done on bitarrays will use bitwise operations
class bitstream {
    private:
        vector<uint8_t> data;
        int8_t bit_ptr;
        uint64_t nelem;

    public:
        bitstream() {
            data = {};
            bit_ptr = -1;
            nelem = 0;
        }

        void push(bool b) {
            bit_ptr++;
            if (bit_ptr % 8 == 0) {
                data.push_back(0);
                bit_ptr = 0;
            }

            if (b) {
                data.back() |= 1u << (7 - bit_ptr);
            }

            nelem++;
        }

        void push_many(bool bits[], uint32_t len) {
            for (uint32_t i = 0; i < len; i++) {
                push(bits[i]);
            }
        }

        bool pop() {
            // uint8_t last_elem = bit_ptr - 1;
            bool b = (data.back() >> (7 - bit_ptr)) & 1u;
            data[(nelem + 7) / 8] &= (1u << (7 - bit_ptr));

            if (bit_ptr == 0) {
                if (nelem == 1) {
                    bit_ptr = -1; // marks that data is empty
                }
                bit_ptr = 7;
                data.pop_back(); // remove last byte from data vector
            } else {
                bit_ptr--;
            }

            nelem--;
            return b;
        }

        bool* pop_many(bool arr[], uint32_t n) {
            for (uint32_t i = 0; i < n; i++) {
                arr[i] = pop();
            }
            return arr;
        }

        uint64_t size() {
            return nelem;
        }

        bool equals(bitstream bits) {
            if (nelem != bits.size()) {
                return false;
            }
            for (uint64_t i = 0; i < (nelem + 7) / 8; i++) {
                if (data[i] != bits.data[i]) {
                    return false;
                }
            }
            return true;
        }

        void print() {
            cout << "Printing bitarray of size " << size() << ": ";
            for (uint64_t i = 0; i < (nelem + 7) / 8; i++) {
                cout << bitset<8>(data[i]) << " ";
            }
            cout << endl;
        }

        bool isempty() {
            return data.empty() || bit_ptr == -1; // redundant checks
        }
};

class encoder {
    private:
        rANSParams params;
        uint32_t base_encode_step(char s, uint32_t state);
        void shrink_state(uint32_t& state, char next_symbol, bitstream& bitarray);
        void encode_symbol(uint32_t& state, char next_symbol, bitstream& bitarray);

    public:
        encoder(rANSParams rans_params);
        bitstream encode_block(string data_block);
};

class decoder {
    private:
        rANSParams params;
        uint32_t find_bin(vector<uint32_t> cum_freq_list, uint32_t slot);
        char base_decode_step(uint32_t& state);
        uint32_t expand_state(uint32_t& state, bitstream& encoded_bitarray);
        tuple<char, uint32_t> decode_symbol(uint32_t& state, bitstream& encoded_bitarray);

    public:
        decoder(rANSParams rans_params);
        tuple<string, uint32_t> decode_block(bitstream& encoded_bitarray);
};