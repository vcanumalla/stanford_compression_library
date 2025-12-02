// C++ Implementation of the rANS encoder
#include <cstdint>
#include <vector>
#include <algorithm>
#include <map>
#include <tuple>
#include <random>
#include <chrono>
#include <fstream>
#include <cstdio>

#include "rANS_buf.hh"
using namespace std;

// max block size: 65536
const uint32_t BUFFER_SIZE = 1000000;

encoder::encoder(rANSParams params) : params(params) {};

// == rANS base encode step ==
// take current state, new symbol s (represented as a char), updates state based on rANS algorithm
// returns new state as int
inline void encoder::base_encode_step(char s, uint32_t& state) {
    uint32_t f = params.freqs.frequency(s);
    uint32_t block_id = state / f;
    uint32_t slot = params.freqs.cumulative_freq_dict()[s] + (state % f);
    state = block_id * params.M + slot; // next_state
}

// == rANS shrink state ==
// takes current state, next symbol to be encoded, brings state within [L,H] range if needed
// returns the scaled state and the bits outputted to stream in the process of scaling, through reference
inline void encoder::shrink_state(uint32_t& state, char next_symbol, uint8_t** ptr) {
    while (state > params.max_shrunk_state[next_symbol]) {
        *ptr -= 1;
        **ptr = (uint8_t) (state & 0xFF);
        state >>= params.NUM_BITS_OUT;
    }
}

// == rANS encode symbol ==
// takes current state, next_symbol to be encoded, calculates next state using base_encode_step and shrink_state
// returns next state and bits outputted from shrink state, through reference
void encoder::encode_symbol(uint32_t& state, char s, uint8_t** ptr) {
    shrink_state(state, s, ptr);
    base_encode_step(s, state);
}

// == rANS encode ==
void encoder::encode(string data, size_t* len, uint8_t** ptr_start) {
    size_t in_size = data.length();
    size_t out_size = in_size + (in_size >> 3) + 256; // safe upper bound
    uint8_t* buf = new uint8_t[out_size];
    uint8_t* ptr = (uint8_t *)(buf + out_size); // end of buffer

    uint32_t state = params.INITIAL_STATE;

    // encode symbols
    for (size_t i = in_size; i > 0; i--) {
        char s = data[i-1];
        encode_symbol(state, s, &ptr);
    }
    // ptr now points to "beginning" of bitstream

    // encode state in byte chunks
    ptr -= 4; // 4 bytes max for state
    ptr[0] = (uint8_t) (state >> 0);
    ptr[1] = (uint8_t) (state >> 8);
    ptr[2] = (uint8_t) (state >> 16);
    ptr[3] = (uint8_t) (state >> 24);

    // encode data size in byte chunks
    ptr -= 4; // 4 bytes max for data_size
    ptr[0] = (uint8_t) (in_size >> 0);
    ptr[1] = (uint8_t) (in_size >> 8);
    ptr[2] = (uint8_t) (in_size >> 16);
    ptr[3] = (uint8_t) (in_size >> 24);

    *len = (buf + out_size) - ptr;

    *ptr_start = ptr;
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
// returns new state (modifies by reference)
inline void decoder::expand_state(uint32_t& state, uint8_t** ptr) {
    while (state < params.L) {
        state = (state << 8) | **ptr;
        *ptr += 1;
    }
}

// == rANS decode symbol ==
// takes current state and bitarray, decodes one symbol and ensures range checking of updated state
// returns tuple containing decoded symbol and num bits used to expand (scale) the state; also modifies state by reference
char decoder::decode_symbol(uint32_t& state, uint8_t** ptr) {
    char s = base_decode_step(state);
    expand_state(state, ptr);
    return s;
}

// == rANS decode block ==
// takes bitarray (fully encoded stream from the encoder), extracts data block size and final state, then proceeds through and decodes original string
// return original string and number of bits used
// ** assumes encoded_bitarray is processed by an encoder which does reverse encoding, so that decoding can proceed in forward direction
string decoder::decode(uint8_t** ptr) {
    // get data_block size
    uint32_t data_size = 0;
    uint8_t* pptr = *ptr;
    data_size = pptr[0] << 0;
    data_size |= pptr[1] << 8;
    data_size |= pptr[2] << 16;
    data_size |= pptr[3] << 24;
    pptr += 4;

    // get final state from bitarray
    uint32_t state = 0;
    state = pptr[0] << 0;
    state |= pptr[1] << 8;
    state |= pptr[2] << 16;
    state |= pptr[3] << 24;
    pptr += 4;
    *ptr = pptr;

    string decoded_data = "";
    // decode symbols
    for (uint32_t i = 0; i < data_size; i++) { // data_size = original input size
        char s = decode_symbol(state, ptr);
        decoded_data += s;
    }

    if (state != params.INITIAL_STATE) {
        cout << "FINAL STATE DOES NOT MATCH INITIAL STATE\n" << endl;
    }

    return decoded_data;
}


//////////////////// TESTING ////////////////////

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

        size_t len;
        uint8_t** ptr_begin = NULL;
        auto enc_start = chrono::high_resolution_clock::now();
        enc.encode(data, &len, ptr_begin);
        auto enc_stop = chrono::high_resolution_clock::now();

        auto enc_time = chrono::duration_cast<chrono::microseconds>(enc_stop - enc_start);
        // cout << "Time to encode: " << enc_time.count() << "ms" << endl;
        enc_avg_time += enc_time.count();

        auto dec_start = chrono::high_resolution_clock::now();
        string decoded_data = dec.decode(ptr_begin);
        auto dec_stop = chrono::high_resolution_clock::now();
        
        auto dec_time = chrono::duration_cast<chrono::microseconds>(dec_stop - dec_start);
        // cout << "Time to decode: " << dec_time.count() << "ms" << endl;
        dec_avg_time += dec_time.count();

        // cout << "Input string: " << data << endl;
        // cout << "Decoded string: " << get<0>(decoded_data) << endl;

        if (decoded_data != data) {
            printf("Decoded string does not match input.\n");
            if (len < 100) {
                cout << "Decoded: " << decoded_data << endl;
                cout << "Original: " << data << endl;
            }
            return false;
        }
    }
    
    enc_avg_time /= 3;
    dec_avg_time /= 3;
    return true;
}
Frequencies compute_frequencies_from_file(string file_path) {
    map<char, uint32_t> freq_dict;
    ifstream file(file_path);
    if (!file.is_open()) {
        printf("Error: Could not open file '%s'.\n", file_path.c_str());
        return Frequencies(freq_dict);
    }
    
    string line;
    while (getline(file, line)) {
        // Count all characters in the line
        for (char c : line) {
            if (c != '\r') {
                freq_dict[c]++;
            }
        }
        // getline removes the newline, but we need to count it to match actual file contents
        // Add newline after each line (matches Python behavior where 'for line in f' includes newline)
        freq_dict['\n']++;
    }
    
    return Frequencies(freq_dict);
}

// Helper function to check if file exists
bool file_exists(const string& file_path) {
    ifstream file(file_path);
    return file.good();
}

string read_file_to_string(const string& file_path) {
    ifstream file(file_path);
    if (!file.is_open()) {
        printf("Error: Could not open file '%s' for reading.\n", file_path.c_str());
        return "";
    }
    
    string content;
    string line;
    while (getline(file, line)) {
        content += line;
        content += '\n';
    }
    
    return content;
}

void write_bitarray_to_file(uint8_t** ptr, const size_t len, const string& file_path) {
    ofstream file(file_path, ios::binary);
    if (!file.is_open()) {
        printf("Error: Could not open file '%s' for writing.\n", file_path.c_str());
        return;
    }
    
    uint64_t num_bits = len;
    file.write(reinterpret_cast<const char*>(&num_bits), sizeof(num_bits));
    
    vector<bool> bits;
    for (int i = 0; i < len; i++) {
        bits.push_back(**ptr);
        *ptr += 1;
    }
    
    uint64_t num_bytes = (num_bits + 7) / 8;
    for (uint64_t i = 0; i < num_bytes; i++) {
        uint8_t byte = 0;
        int bits_in_byte = (i == num_bytes - 1 && num_bits % 8 != 0) ? (num_bits % 8) : 8;
        for (int j = 0; j < bits_in_byte; j++) {
            uint64_t bit_idx = i * 8 + j;
            if (bit_idx < bits.size()) {
                bool bit = bits[bit_idx];
                byte |= (bit ? 1u : 0u) << j;
            }
        }
        file.write(reinterpret_cast<const char*>(&byte), sizeof(byte));
    }
}

// Helper function to read BitArray from file
// BitArray read_bitarray_from_file(const string& file_path) {
//     ifstream file(file_path, ios::binary);
//     if (!file.is_open()) {
//         printf("Error: Could not open file '%s' for reading.\n", file_path.c_str());
//         return BitArray();
//     }
    
//     // Read the size (number of bits)
//     uint64_t num_bits;
//     file.read(reinterpret_cast<char*>(&num_bits), sizeof(num_bits));
    
//     // Read bytes and convert to bits
//     BitArray bitarray;
//     uint64_t num_bytes = (num_bits + 7) / 8;
//     vector<bool> bits;
    
//     // Read all bytes and extract bits
//     for (uint64_t i = 0; i < num_bytes; i++) {
//         uint8_t byte;
//         file.read(reinterpret_cast<char*>(&byte), sizeof(byte));
        
//         int bits_in_byte = (i == num_bytes - 1 && num_bits % 8 != 0) ? (num_bits % 8) : 8;
//         for (int j = 0; j < bits_in_byte; j++) {
//             bool bit = (byte >> j) & 1u;
//             bits.push_back(bit);
//         }
//     }
    
//     // Push bits in reverse order (since BitArray is a stack)
//     // The last bit read should be the first popped
//     for (int i = bits.size() - 1; i >= 0; i--) {
//         bitarray.push(bits[i]);
//     }
    
//     return bitarray;
// }

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    string input_file_path = argv[1];
    string output_file_path = argv[2];

    if (!file_exists(input_file_path)) {
        printf("Error: Input file '%s' does not exist.\n", input_file_path.c_str());
        return 1;
    }
    
    printf("Computing frequencies from %s...\n", input_file_path.c_str());
    Frequencies freqs = compute_frequencies_from_file(input_file_path);
    printf("Found %zu unique characters\n", freqs.size());
    
    // output the list of keys to txt (in sorted order, matching Python)
    ofstream freq_file("freqs_c.txt");
    for (const auto& kv : freqs.freq_dict) {
        freq_file << kv.first << endl;
    }
    freq_file.close();
    printf("Total characters: %u\n", freqs.total_freq());
    
    printf("Creating rANS encoder/decoder...\n");
    rANSParams params = rANSParams(freqs, 32, 1);
    
    encoder enc = encoder(params);
    decoder dec = decoder(params);
    
    // Read the entire input file into a string
    printf("Reading input file...\n");
    string data = read_file_to_string(input_file_path);
    if (data.empty()) {
        printf("Error: Failed to read input file.\n");
        return 1;
    }
    
    printf("Compressing %s -> %s...\n", input_file_path.c_str(), output_file_path.c_str());
    size_t len;
    uint8_t** encoded_begin_ptr = NULL;
    auto enc_start = chrono::high_resolution_clock::now();
    enc.encode(data, &len, encoded_begin_ptr);
    auto enc_stop = chrono::high_resolution_clock::now();
    auto enc_time = chrono::duration_cast<chrono::milliseconds>(enc_stop - enc_start);
    printf("Compression time: %.2f seconds\n", enc_time.count() / 1000.0);
    
    // Write encoded bitarray to file
    write_bitarray_to_file(encoded_begin_ptr, len, output_file_path);
    
    // Get file sizes for compression ratio
    ifstream input_file(input_file_path, ios::binary | ios::ate);
    ifstream output_file(output_file_path, ios::binary | ios::ate);
    size_t input_size = input_file.tellg();
    size_t output_size = output_file.tellg();
    double compression_ratio = output_size > 0 ? (double)input_size / output_size : 0.0;
    
    printf("\nCompression complete!\n");
    printf("Input size:  %zu bytes\n", input_size);
    printf("Output size: %zu bytes\n", output_size);
    printf("Compression ratio: %.2fx\n", compression_ratio);

    // printf("Decompressing %s -> %s...\n", output_file_path.c_str(), output_file_path.c_str());
    auto dec_start = chrono::high_resolution_clock::now();
    string decoded_data = dec.decode(encoded_begin_ptr);
    auto dec_stop = chrono::high_resolution_clock::now();
    auto dec_time = chrono::duration_cast<chrono::milliseconds>(dec_stop - dec_start);
    printf("Decompression time: %.2f seconds\n", dec_time.count() / 1000.0);
    
    return 0;
}
