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

#include "rANS_explicit_opt.hh"
using namespace std;

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

// == rANS encode half ==
void encoder::encode_half(const string& data, size_t start_idx, size_t step,uint32_t& state, uint8_t** ptr) {
    for (size_t i = start_idx; i >= 0 && i < data.size(); i -= step) {
        char s = data[i];
        encode_symbol(state, s, ptr);
    }
}


tuple<uint8_t*, uint8_t*, uint8_t*> encoder::encode(string data, size_t* len)
{
    size_t n = data.length();

    size_t out_size = n + (n >> 3) + 256;
    uint8_t* buf  = new uint8_t[out_size];

    uint8_t* ptr0 = buf + out_size;
    uint8_t* ptr1 = buf + (out_size / 2);

    uint32_t state0 = params.INITIAL_STATE;
    uint32_t state1 = params.INITIAL_STATE;

    for (size_t i = n; i > 0; i--) {
        char s = data[i-1];

        if ((i-1) & 1u) {
            encode_symbol(state0, s, &ptr0); // odd index
        } else {
            encode_symbol(state1, s, &ptr1); // even index
        }
    }

    // Store final states
    ptr0 -= 4;
    ptr0[0] = (state0 >> 0);
    ptr0[1] = (state0 >> 8);
    ptr0[2] = (state0 >> 16);
    ptr0[3] = (state0 >> 24);

    ptr1 -= 4;
    ptr1[0] = (state1 >> 0);
    ptr1[1] = (state1 >> 8);
    ptr1[2] = (state1 >> 16);
    ptr1[3] = (state1 >> 24);

    // Store size (decoder expects it at ptr1)
    ptr1 -= 4;
    ptr1[0] = (n >> 0);
    ptr1[1] = (n >> 8);
    ptr1[2] = (n >> 16);
    ptr1[3] = (n >> 24);

    // Compute final length
    *len  = (buf + out_size) - ptr0;
    *len += (buf + (out_size / 2)) - ptr1;

    return {buf, ptr0, ptr1};
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
    uint32_t slot = state % params.M;
    const ransDecSym& ds = params.decode_table[slot];

    char s = ds.s;

    uint32_t block_id = state / params.M;
    state = block_id * ds.freq + (slot - ds.cum_freq);
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

// == rANS decode half  ==
// decodes one half of the data using the provided state and pointer
// stores result in the provided string reference
void decoder::decode_half(uint32_t data_size, uint8_t** ptr, uint32_t state, string& result, bool is_state1) {
    for (uint32_t i = 0; i < data_size; i++) {
        char s = decode_symbol(state, ptr);
        result += s;
    }
    
    if (state != params.INITIAL_STATE) {
        cout << "FINAL STATE DOES NOT MATCH INITIAL STATE (half: " << (is_state1 ? "1" : "0") << ")\n" << endl;
    }
}

// == rANS decode block (parallel version) ==
// takes two pointers to encoded data halves, decodes both in parallel, and merges results
// returns the combined decoded string
string decoder::decode(uint8_t** ptr0, uint8_t** ptr1) {
    uint8_t* pptr0 = *ptr0;
    uint8_t* pptr1 = *ptr1;

    // get data_block size
    uint32_t data_size = 0;
    data_size = pptr1[0] << 0;
    data_size |= pptr1[1] << 8;
    data_size |= pptr1[2] << 16;
    data_size |= pptr1[3] << 24;
    pptr1 += 4;

    // get final states from bitarray
    uint32_t state0 = 0;
    uint32_t state1 = 0;
    state0 = pptr0[0] << 0;
    state0 |= pptr0[1] << 8;
    state0 |= pptr0[2] << 16;
    state0 |= pptr0[3] << 24;
    pptr0 += 4;
    *ptr0 = pptr0;
    state1 = pptr1[0] << 0;
    state1 |= pptr1[1] << 8;
    state1 |= pptr1[2] << 16;
    state1 |= pptr1[3] << 24;
    pptr1 += 4;
    *ptr1 = pptr1;

    // Decode both halves in parallel
    string result;
    result.resize(data_size);

    for (uint32_t i = 0; i < data_size; i++) {
        if ((i & 1) == 0) {
            // even index → decode from stream1/state1
            result[i] = decode_symbol(state1, ptr1);
        } else {
            // odd index → decode from stream0/state0
            result[i] = decode_symbol(state0, ptr0);
        }
    }

    return result;
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
        string data = random_string(50, alphas[i]);
        rANSParams params = rANSParams(freq, 32, 1);

        encoder enc = encoder(params);
        decoder dec = decoder(params);

        size_t len;
        auto enc_start = chrono::high_resolution_clock::now();
        tuple<uint8_t*, uint8_t*, uint8_t*> ptrs = enc.encode(data, &len);
        auto enc_stop = chrono::high_resolution_clock::now();

        uint8_t* buf = get<0>(ptrs);
        uint8_t* ptr0_begin = get<1>(ptrs);
        uint8_t* ptr1_begin = get<2>(ptrs);

        auto enc_time = chrono::duration_cast<chrono::microseconds>(enc_stop - enc_start);
        enc_avg_time += enc_time.count();

        auto dec_start = chrono::high_resolution_clock::now();
        string decoded_data = dec.decode(&ptr0_begin, &ptr1_begin);
        auto dec_stop = chrono::high_resolution_clock::now();
        
        auto dec_time = chrono::duration_cast<chrono::microseconds>(dec_stop - dec_start);
        dec_avg_time += dec_time.count();

        delete[] buf;

        if (len < 100) {
            cout << "\nOriginal: " << data << endl;
            cout << "Decoded: " << decoded_data << endl;
        }
        if (decoded_data != data) {
            printf("Decoded string does not match input.\n");
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

void write_bitarray_to_file(uint8_t* ptr0, uint8_t* ptr1, const size_t len, const string& file_path) {
    ofstream file(file_path, ios::binary);
    if (!file.is_open()) {
        printf("Error: Could not open file '%s' for writing.\n", file_path.c_str());
        return;
    }
    
    uint64_t num_bits = len;
    file.write(reinterpret_cast<const char*>(&num_bits), sizeof(num_bits));
    
    vector<bool> bits;
    bool use_ptr0 = false;
    for (size_t i = 0; i < len; i++) {
        bits.push_back(use_ptr0 ? *ptr0 : *ptr1);
        if (use_ptr0) {
            ptr0++;
        } else {
            ptr1++;
        }
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

tuple<int, int, string> run_test(string data, string input_file_path, string output_file_path, rANSParams params, bool verbose) {
    encoder enc = encoder(params);
    decoder dec = decoder(params);
    size_t len;

    if (verbose) {
        printf("Compressing %s -> %s...\n", input_file_path.c_str(), output_file_path.c_str());
    }
    
    auto enc_start = chrono::high_resolution_clock::now();
    tuple<uint8_t*, uint8_t*, uint8_t*> ptrs = enc.encode(data, &len);
    auto enc_stop = chrono::high_resolution_clock::now();
    auto enc_time = chrono::duration_cast<chrono::milliseconds>(enc_stop - enc_start);
    if (verbose) {
        printf("Compression time: %.2f ms\n", enc_time.count() * 1.0);
    }

    uint8_t* buf = get<0>(ptrs);
    uint8_t* encoded_begin_ptr0 = get<1>(ptrs);
    uint8_t* encoded_begin_ptr1 = get<2>(ptrs);
    
    // Write encoded bitarray to file
    write_bitarray_to_file(encoded_begin_ptr0, encoded_begin_ptr1, len, output_file_path);
    
    // Get file sizes for compression ratio
    if (verbose) {
        ifstream input_file(input_file_path, ios::binary | ios::ate);
        ifstream output_file(output_file_path, ios::binary | ios::ate);
        size_t input_size = input_file.tellg();
        size_t output_size = output_file.tellg();
        double compression_ratio = output_size > 0 ? (double)input_size / output_size : 0.0;

        printf("\nCompression complete!\n");
        printf("Input size:  %zu bytes\n", input_size);
        printf("Output size: %zu bytes\n", output_size);
        printf("Compression ratio: %.2fx\n", compression_ratio);
    }
    
    auto dec_start = chrono::high_resolution_clock::now();
    string decoded_data = dec.decode(&encoded_begin_ptr0, &encoded_begin_ptr1);
    auto dec_stop = chrono::high_resolution_clock::now();
    auto dec_time = chrono::duration_cast<chrono::milliseconds>(dec_stop - dec_start);

    if (verbose) {
        printf("\nDecompression complete!\n");
        printf("Decompression time: %.2f ms\n", dec_time.count() * 1.0);
    }

    // write decoded data to a file for verification
    string decoded_output_path = output_file_path + ".decoded";
    ofstream decoded_out(decoded_output_path, ios::binary);
    if (!decoded_out.is_open()) {
        printf("Error: Could not open file '%s' for writing decoded output.\n", decoded_output_path.c_str());
    } else {
        decoded_out.write(decoded_data.data(), decoded_data.size());
        decoded_out.close();
        if (verbose) {
            printf("Decoded output written to: %s\n", decoded_output_path.c_str());
        }
    }

    delete[] buf;

    tuple<int, int, string> ret_tup = tuple<int, int, string>{enc_time.count(), dec_time.count(), decoded_data};
    return ret_tup;
}

int main(int argc, char *argv[]) {
    if (argc <= 2) {
        printf("Usage: %s <input_file> <output_file> [--eval]\n", argv[0]);
        return 1;
    }

    bool eval_mode = false;
    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "--eval") eval_mode = true;
    }

    string input_file_path = argv[1];
    string output_file_path = argv[2];

    if (!file_exists(input_file_path)) {
        printf("Error: Input file '%s' does not exist.\n", input_file_path.c_str());
        return 1;
    }

    if (!eval_mode) {
        printf("Computing frequencies from %s...\n", input_file_path.c_str());
    }
    Frequencies freqs = compute_frequencies_from_file(input_file_path);

    if (!eval_mode) {
        printf("Found %zu unique characters\n", freqs.size());
    }

    ofstream freq_file("freqs_c.txt");
    for (const auto& kv : freqs.freq_dict) freq_file << kv.first << endl;
    freq_file.close();

    if (!eval_mode) {
        printf("Total characters: %u\n", freqs.total_freq());
        printf("Creating rANS encoder/decoder...\n");
    }

    rANSParams params = rANSParams(freqs, 32, 1);

    encoder enc(params);
    decoder dec(params);

    if (!eval_mode) printf("Reading input file...\n");
    string data = read_file_to_string(input_file_path);
    if (data.empty()) {
        printf("Error: Failed to read input file.\n");
        return 1;
    }

    if (!eval_mode) printf("Running test...\n");

    auto enc_avg_time = 0;
    auto dec_avg_time = 0;
    size_t num_iter = 1;

    for (size_t i = 0; i < num_iter; i++) {
        tuple<int, int, string> ret_val =
            run_test(data, input_file_path, output_file_path, params, !eval_mode);
        enc_avg_time += get<0>(ret_val);
        dec_avg_time += get<1>(ret_val);
    }

    enc_avg_time /= num_iter;
    dec_avg_time /= num_iter;

    // verify decoded file matches original
    string decoded_output_path = output_file_path + ".decoded";
    ifstream original_file(input_file_path, ios::binary);
    ifstream decoded_file(decoded_output_path, ios::binary);

    if (!original_file.is_open() || !decoded_file.is_open()) {
        std::cerr << "Error opening files.\n";
        return 1;
    }

    original_file.seekg(0, ios::end);
    decoded_file.seekg(0, ios::end);
    streamsize size1 = original_file.tellg();
    streamsize size2 = decoded_file.tellg();

    bool verified = (size1 == size2);

    // compare byte-by-byte
    original_file.seekg(0);
    decoded_file.seekg(0);
    vector<char> buf1(4096), buf2(4096);

    while (verified && original_file && decoded_file) {
        original_file.read(buf1.data(), buf1.size());
        decoded_file.read(buf2.data(), buf2.size());

        if (original_file.gcount() != decoded_file.gcount()) {
            verified = false;
            break;
        }
        if (!equal(buf1.begin(), buf1.begin() + original_file.gcount(), buf2.begin())) {
            verified = false;
            break;
        }
    }
    // eval mode, should only be used when running through python script.
    if (eval_mode) {
        string eval_output_path = output_file_path + ".out.json";
        ofstream eval_out(eval_output_path);
        if (!eval_out.is_open()) {
            cerr << "Error: Failed to open eval output file: "
                << eval_output_path << endl;
            return 1;
        }
        eval_out << "{\n";
        eval_out << "  \"input_file\": \"" << input_file_path << "\",\n";
        eval_out << "  \"output_file\": \"" << output_file_path << "\",\n";
        eval_out << "  \"unique_chars\": " << freqs.size() << ",\n";
        eval_out << "  \"total_chars\": " << freqs.total_freq() << ",\n";
        eval_out << "  \"avg_compression_time_ms\": " << enc_avg_time << ",\n";
        eval_out << "  \"avg_decompression_time_ms\": " << dec_avg_time << ",\n";
        eval_out << "  \"file_size_bytes\": " << size1 << ",\n";
        eval_out << "  \"compressed_size_bytes\": " << size2 << ",\n";
        eval_out << "  \"compression_ratio\": " 
             << (double)size1 / (double)(size2 > 0 ? size2 : 1) << ",\n";
        eval_out << "  \"verified\": " << (verified ? "true" : "false") << "\n";
        eval_out << "}\n";
        return 0;
    }

    printf("\n=====================================");
    printf("\nDone with %zu test iterations. Timed results:", num_iter);
    printf("\nAvg compression time: %.2f ms\n", enc_avg_time * 1.0);
    printf("Avg decompression time: %.2f ms\n", dec_avg_time * 1.0);

    if (!verified) {
        printf("Error: mismatch in decoded output and original file.\n");
    }

    return 0;
}