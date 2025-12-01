// C++ Implementation of the rANS encoder
#include <cstdint>
#include <vector>
#include <algorithm>
#include <map>
#include <tuple>
#include <random>
#include <chrono>

#include "rANS.hh"
using namespace std;

// max block size: 65536
const uint32_t BUFFER_SIZE = 65536;

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
// returns the scaled state and the bits outputted to stream in the process of scaling, through reference
void encoder::shrink_state(uint32_t& state, char next_symbol, BitArray& bitarray) {
    while (state > params.max_shrunk_state[next_symbol]) {
        bool new_bit = state % (1u << params.NUM_BITS_OUT);
        bitarray.push(new_bit); // using vector as a stack, appending bits to end during encode, then popping from end->start during decode (reverse direction)
        state = state >> params.NUM_BITS_OUT;
    }
}

// == rANS encode symbol ==
// takes current state, next_symbol to be encoded, calculates next state using base_encode_step and shrink_state
// returns next state and bits outputted from shrink state, through reference
void encoder::encode_symbol(uint32_t& state, char s, BitArray& bitarray) {
    shrink_state(state, s, bitarray);
    state = base_encode_step(s, state);
}

// == rANS encode block (standard) ==
// takes a block of data of size len and encodes it together, using encode_symbol as the building block step
// returns the bitarray corresponding to this data, to be decoded
// IMPORTANT: data length must be <= BUFFER_SIZE, otherwise behavior is undefined
void encoder::encode_block(char** buf, size_t len, BitArray& out_stream) {
    uint32_t state = params.INITIAL_STATE;
    char* ptr = *buf;

    // read symbols from buf in forward order, encode into bitstream
    for (size_t i = 0; i < len; i++) {
        char s = *ptr;
        encode_symbol(state, s, out_stream);
        ptr++; // move ptr to next char (symbol)
    }

    // put binary encoding of final state into bitarray
    for (uint32_t i = 0; i < params.NUM_STATE_BITS; i++) {
        out_stream.push(state & 1u);
        state >>= 1u;
    }

    // add data_block size in binary to bitarray
    uint32_t data_size = len;
    for (uint32_t i = 0; i < params.DATA_BLOCK_SIZE_BITS; i++) {
        out_stream.push(data_size & 1u);
        data_size >>= 1u;
    }
}

// NOTE: can be replaced by a function which streams in data from a file
BitArray encoder::encode(string data) {
    BitArray bitstream;
    char* buf = new char[BUFFER_SIZE];
    char* ptr = (char *)(buf + BUFFER_SIZE); // end of buffer
    size_t len = data.length();

    for (size_t i = 0; i < len; i++) {
        // read symbols into buf, reverse order for ptr so that buf fills in reverse with data
        ptr--;
        *ptr = data[i];  
    }

    encode_block(&ptr, len, bitstream);

    return bitstream;
}



decoder::decoder(rANSParams params) : params(params) {};

// == rANS find bin ==
// takes a cumulative frequency list and a slot (integer), and finds which bin it lies in
// returns the bin, representing as an integer for the index of the bin
uint32_t decoder::find_bin(vector<uint32_t> cum_freq_list, uint32_t slot) {

    auto bin = upper_bound(cum_freq_list.begin(), cum_freq_list.end(), slot);
    return (bin - cum_freq_list.begin()) - 1;
}

// == rANS base decode step ==
// takes current state, decodes one symbol
// returns decoded symbol and updated (prev) state (modifies through reference)
char decoder::base_decode_step(uint32_t& state) {
    uint32_t block_id = state / params.M;
    uint32_t slot = state % params.M;

    map<char, uint32_t> cum_prob_list = params.freqs.cumulative_freq_dict();
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
uint32_t decoder::expand_state(uint32_t& state, BitArray& encoded_bitarray) {
    uint32_t num_bits = 0;
    while (state < params.L) {
        bool state_remainder = encoded_bitarray.pop();

        num_bits += params.NUM_BITS_OUT;
        state = (state << params.NUM_BITS_OUT) + (state_remainder ? 1u : 0);
    }

    return num_bits;
}

// == rANS decode symbol ==
// takes current state and bitarray, decodes one symbol and ensures range checking of updated state
// returns tuple containing decoded symbol and num bits used to expand (scale) the state; also modifies state by reference
tuple<char, uint32_t> decoder::decode_symbol(uint32_t& state, BitArray& encoded_bitarray) {
    char s = base_decode_step(state);

    uint32_t num_bits_consumed = expand_state(state, encoded_bitarray);
    return tuple<char, uint32_t>{s, num_bits_consumed};
}

// == rANS decode block ==
// takes bitarray (fully encoded stream from the encoder), extracts data block size and final state, then proceeds through and decodes original string
// return original string and number of bits used
// ** assumes encoded_bitarray is processed by an encoder which does reverse encoding, so that decoding can proceed in forward direction
tuple<string, uint32_t> decoder::decode_block(BitArray& encoded_bitarray) {
    // get data_block size from bitarray
    uint32_t data_size = 0;
    for (uint32_t i = 0; i < params.DATA_BLOCK_SIZE_BITS; i++) {
        data_size <<= 1u;
        data_size |= encoded_bitarray.pop();
    }

    // get final state from bitarray
    uint32_t state = 0;
    for (uint32_t i = 0; i < params.NUM_STATE_BITS; i++) {
        state <<= 1u;
        uint8_t next = encoded_bitarray.pop() ? 1u : 0;
        state |= next;
    }
    uint32_t bits_consumed = params.DATA_BLOCK_SIZE_BITS + params.NUM_STATE_BITS;
    string data = "";

    for (uint32_t i = 0; i < data_size; i++) {
        tuple<char, uint32_t> state_num_bits = decode_symbol(state, encoded_bitarray);
        string s(1, get<0>(state_num_bits));
        data.append(s);
        bits_consumed += get<1>(state_num_bits);
    }

    if (state != params.INITIAL_STATE) {
        cout << "FINAL STATE DOES NOT MATCH INITIAL STATE\n" << endl;
    }

    return tuple<string, uint32_t>{data, bits_consumed};
}


//////////////////// TESTING ////////////////////

bool test_bitarray() { // DEPRECATED: encoded_bitarray does not match python implementation, because we encode in reverse direction. final decoded check still applicable
    map<char, uint32_t> freq_dict = {
        {'A', 3},
        {'B', 3},
        {'C', 2}
    };
    
    Frequencies freq = Frequencies(freq_dict);
    string data = "ACBBBCAAB";
    rANSParams params = rANSParams(freq, 5, 1);

    BitArray expected_bitarray = {};

    // initial state
    uint32_t st = 8; // state variable
    if (params.INITIAL_STATE != 8) {
        cout << "Initial state is not 8.\n" << endl;
        return false;
    }

    // first symbol: A
    // rescale state to be within [3,5]
    st = 4;
    expected_bitarray.push(0);
    // encode
    st = 9;

    // second symbol: C
    // rescale
    st = 4;
    expected_bitarray.push(1);
    st = 2;
    expected_bitarray.push(0);
    // encode; state = (st//3)*8 + 0 + (st%3)
    st = 14;

    // third symbol: B
    // rescale
    st = 7;
    expected_bitarray.push(0);
    st = 3;
    expected_bitarray.push(1);
    // encode
    st = 11;

    printf("Final expected state: %d\n", st);

    // add final state to bitarray
    uint32_t num_state_bits = 4;
    if (params.NUM_STATE_BITS != num_state_bits) {
        cout << "Num state bits is not 4.\n" << endl;
        return false;
    }
    expected_bitarray.push(1);
    expected_bitarray.push(1);
    expected_bitarray.push(0);
    expected_bitarray.push(1);

    // add number of symbols (3) to bitarray
    expected_bitarray.push(1);
    expected_bitarray.push(1);
    expected_bitarray.push(0);
    expected_bitarray.push(0);
    expected_bitarray.push(0);
    // state = 01001 1101 11000

    // use encoder-decoder and check
    encoder enc = encoder(params);
    BitArray actual_bitarray = enc.encode(data);
    if (actual_bitarray.size() != expected_bitarray.size()) {
        printf("Size mismatch. Actual: %zu, Expected: %zu\n", actual_bitarray.size(), expected_bitarray.size());
    }

    cout << "actual: ";
    actual_bitarray.print();
    cout << "expected : ";
    expected_bitarray.print();
    
    // if (!actual_bitarray.equals(expected_bitarray)) {
    //     return false;
    // }

    decoder dec = decoder(params);
    tuple<string,uint32_t> decoded_data = dec.decode_block(actual_bitarray);
    cout << "Input string: " << data << endl;
    cout << "Decoded string: " << get<0>(decoded_data) << endl;
    return data == get<0>(decoded_data);
}

string random_string(uint32_t n, const vector<char>& alphabet) {
    static mt19937 rng(random_device{}());
    uniform_int_distribution<> dist(0, alphabet.size() - 1);

    string s;
    s.reserve(n);

    for (uint32_t i = 0; i < n; ++i) {
        s.push_back(alphabet[dist(rng)]);
    }
    return s;
}

bool test_rANS(uint32_t& enc_avg_time, uint32_t& dec_avg_time) {
    map<char, uint32_t> freq_dict1 = {
        {'A', 1},
        {'B', 1},
        {'C', 2}
    };
    vector<char> alpha1 = {'A', 'B', 'C'};

    map<char, uint32_t> freq_dict2 = {
        {'A', 3},
        {'B', 3},
        {'C', 2},
        {'D', 5}
    };
    vector<char> alpha2 = {'A', 'B', 'C', 'D'};

    map<char, uint32_t> freq_dict3 = {
        {'A', 301},
        {'B', 41},
        {'C', 5},
        {'D', 129},
        {'E', 60}
    };
    vector<char> alpha3 = {'A', 'B', 'C', 'D', 'E'};

    map<char, uint32_t> freq_dicts[] = {freq_dict1, freq_dict2, freq_dict3};
    vector<char> alphas[] = {alpha1, alpha2, alpha3};
    for (int i = 0; i < 3; i++) {
        Frequencies freq = Frequencies(freq_dicts[i]);
        string data = random_string(10000, alphas[i]);
        rANSParams params = rANSParams(freq, 32, 1);

        encoder enc = encoder(params);
        decoder dec = decoder(params);

        auto enc_start = chrono::high_resolution_clock::now();
        BitArray encoded_bitarray = enc.encode(data);
        auto enc_stop = chrono::high_resolution_clock::now();
        uint32_t len = encoded_bitarray.size();

        auto enc_time = chrono::duration_cast<chrono::microseconds>(enc_stop - enc_start);
        // cout << "Time to encode: " << enc_time.count() << "ms" << endl;
        enc_avg_time += enc_time.count();

        auto dec_start = chrono::high_resolution_clock::now();
        tuple<string,uint32_t> decoded_data = dec.decode_block(encoded_bitarray);
        auto dec_stop = chrono::high_resolution_clock::now();
        
        auto dec_time = chrono::duration_cast<chrono::microseconds>(dec_stop - dec_start);
        // cout << "Time to decode: " << dec_time.count() << "ms" << endl;
        dec_avg_time += dec_time.count();

        // cout << "Input string: " << data << endl;
        // cout << "Decoded string: " << get<0>(decoded_data) << endl;

        if (get<1>(decoded_data) != len) {
            printf("Did not consume correct number of bits.\n");
            return false;
        }
        if (get<0>(decoded_data) != data) {
            printf("Decoded string does not match input.\n");
            if (len < 100) {
                cout << "Decoded: " << get<0>(decoded_data) << endl;
                cout << "Original: " << data << endl;
            }
            return false;
        }
    }
    
    enc_avg_time /= 3;
    dec_avg_time /= 3;
    return true;
}

int main(int argc, char *argv[]) {
    uint32_t enc_avg_time = 0;
    uint32_t dec_avg_time = 0;

    uint32_t num_iter = 100;
    for (uint32_t i = 0; i < num_iter; i++) {
        uint32_t enc_iter_time = 0;
        uint32_t dec_iter_time = 0;
        bool test_result = test_rANS(enc_iter_time, dec_iter_time);
        if (!test_result) {
            printf("TEST FAILED. EXITING EARLY...\n");
            break;
        } else {
            enc_avg_time += enc_iter_time;
            dec_avg_time += dec_iter_time;
        }
    }

    enc_avg_time /= num_iter;
    dec_avg_time /= num_iter;

    cout << "Avg encode time: " << enc_avg_time << "us" << endl;
    cout << "Avg decode time: " << dec_avg_time << "us" << endl;

    return 0;
}
