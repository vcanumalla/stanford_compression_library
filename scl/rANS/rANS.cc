// C++ Implementation of the rANS encoder
#include <cstdint>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <tuple>

#include "rANS.hh"
using namespace std;

encoder::encoder(rANSParams params) : params(params) {};

// == rANS base encode step ==
// take current state, new symbol s (represented as a char), updates state based on rANS algorithm
// returns new state as int
uint32_t encoder::base_encode_step(char s, uint32_t state) {
    uint32_t f = params.freqs.frequency(s);
    uint32_t block_id = state / f;
    uint32_t slot = params.freqs.cumulative_freq_dict()[s] + (state % f);
    return block_id * params.M + slot; // next_state
}

// == rANS shrink state ==
// takes current state, next symbol to be encoded, brings state within [L,H] range if needed
// returns the scaled state (modifying via reference), and the bits outputted to stream in the process of scaling
vector<bool> encoder::shrink_state(uint32_t& state, char next_symbol) {
    vector<bool> out_bits;

    while (state > params.max_shrunk_state[next_symbol]) {
        bool new_bit = state % (1u << params.NUM_BITS_OUT);
        out_bits.push_back(new_bit); // using vector as a stack, appending bits to end during encode, then popping from end->start during decode (reverse direction)
        state = state >> params.NUM_BITS_OUT;
    }

    return out_bits;
}

// == rANS encode symbol ==
// takes current state, next_symbol to be encoded, calculates next state using base_encode_step and shrink_state
// returns next state (modifying via reference), and bits outputted from shrink state
vector<bool> encoder::encode_symbol(uint32_t& state, char s) {
    vector<bool> out_bits = shrink_state(state, s);
    state = base_encode_step(s, state);
    return out_bits;
}

// == rANS encode block ==
// takes a large portion (block) of data and encodes it together, using encode_symbol as the building block step
// returns the bitarray (vector of bools) corresponding to this data, to be decoded
vector<bool> encoder::encode_block(string data) {
    vector<bool> bitarray;
    uint32_t state = params.INITIAL_STATE;

    for (char s : data) {
        vector<bool> out_bits = encode_symbol(state, s);
        bitarray.insert(bitarray.end(), out_bits.begin(), out_bits.end());
    }

    // put binary encoding of final state into bitarray
    for (int i = 0; i < params.NUM_STATE_BITS; i++) {
        bitarray.push_back(state & 1u);
        state >>= 1u;
    }

    // add data_block size in binary to bitarray
    uint32_t data_size = data.length();
    for (int i = 0; i < params.DATA_BLOCK_SIZE_BITS; i++) {
        bitarray.push_back(data_size & 1u);
        data_size >>= 1u;
    }

    return bitarray;
}



decoder::decoder(rANSParams params) : params(params) {};

// == rANS find bin ==
// takes a cumulative frequency list and a slot (integer), and finds which bin it lies in
// returns the bin, representing as an integer for the index of the bin
uint32_t decoder::find_bin(vector<uint32_t> cum_freq_list, uint32_t slot) {

    uint32_t bin = *upper_bound(cum_freq_list.begin(), cum_freq_list.end(), slot);
    return bin - 1;
}

// == rANS base decode step ==
// takes current state, decodes one symbol
// returns decoded symbol and updated (prev) state (modifies through reference)
char decoder::base_decode_step(uint32_t& state) {
    uint32_t block_id = state / params.M;
    uint32_t slot = state % params.M;

    unordered_map<char, uint32_t> cum_prob_list = params.freqs.cumulative_freq_dict();
    vector<uint32_t> values;
    for (auto& kv : cum_prob_list) {
        values.push_back(kv.second);
    }

    uint32_t symbol_bin = find_bin(values, slot);
    char s = params.freqs.alphabet()[symbol_bin];

    uint32_t prev_state = block_id * params.freqs.frequency(s) + slot - params.freqs.cumulative_freq_dict()[s];
    state = prev_state;
    return s;
}

// == rANS expand state ==
// takes current state and available bits from bitarray, expands state to range [L,H]
// returns number of bits consumed from bitarray to scale, and new state (modifies by reference)
uint32_t decoder::expand_state(uint32_t& state, vector<bool>& encoded_bitarray) {
    uint32_t num_bits = 0;
    while (state < params.L) {
        bool state_remainder = encoded_bitarray.back();
        encoded_bitarray.pop_back();

        num_bits += params.NUM_BITS_OUT;
        state = (state << params.NUM_BITS_OUT) + (state_remainder ? 1u : 0);
    }

    return num_bits;
}

// == rANS decode symbol ==
// takes current state and bitarray, decodes one symbol and ensures range checking of updated state
// returns tuple containing decoded symbol and num bits used to expand (scale) the state; also modifies state by reference
tuple<char, uint32_t> decoder::decode_symbol(uint32_t& state, vector<bool>& encoded_bitarray) {
    char s = base_decode_step(state);

    uint32_t num_bits_consumed = expand_state(state, encoded_bitarray);
    return tuple<char, uint32_t>{s, num_bits_consumed};
}

// rANS decode block
// takes bitarray (fully encoded stream from the encoder), extracts data block size and final state, then proceeds through and decodes original string
// return original string and number of bits used
tuple<string, uint32_t> decoder::decode_block(vector<bool>& encoded_bitarray) {
    // get data_block size from bitarray
    uint32_t data_size = 0;
    for (int i = 0; i < params.DATA_BLOCK_SIZE_BITS; i++) {
        data_size = data_size & encoded_bitarray.back();
        data_size <<= 1u;
        encoded_bitarray.pop_back();
    }

    // get final state from bitarray
    uint32_t state = 0;
    for (int i = 0; i < params.NUM_STATE_BITS; i++) {
        state = state & encoded_bitarray.back();
        state <<= 1u;
        encoded_bitarray.pop_back();
    }
    uint32_t bits_consumed = params.DATA_BLOCK_SIZE_BITS + params.NUM_STATE_BITS;
    string data = "";

    for (int i = 0; i < data_size; i++) {
        tuple<char, uint32_t> state_num_bits = decode_symbol(state, encoded_bitarray);
        string s(1, get<0>(state_num_bits));
        data.insert(0, s);
        bits_consumed += get<1>(state_num_bits);
    }

    if (state != params.INITIAL_STATE) {
        cout << "FINAL STATE DOES NOT MATCH INITIAL STATE\n" << endl;
    }

    return tuple<string, uint32_t>{data, bits_consumed};
}

int main(int argc, char *argv[]) {
    return 0;
}
