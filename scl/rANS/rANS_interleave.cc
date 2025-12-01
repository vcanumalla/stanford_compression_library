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

inline void RansEncStep(uint32_t& state, uint16_t* &ptr,
                        const uint16_t* cum_freq, uint16_t freq)
{
    // renormalize if needed
    while (state >= (params.M << 16)) {
        *--ptr = state & 0xFFFF;
        state >>= 16;
    }
    // encode symbol
    state = ((state / freq) * params.M) + (state % freq) + cum_freq[0];
}

void encode_block_interleave2_fast(const uint8_t* in_bytes, size_t len,
                                   uint16_t* out_buf, size_t& out_len,
                                   const struct Stats& stats)
{
    uint32_t state[2] = { 0x8000, 0x8000 }; // example initial state
    uint16_t* ptr = out_buf + out_len;      // start at end of buffer

    // handle odd-length
    if (len & 1) {
        int s = in_bytes[len - 1];
        RansEncStep(state[1], ptr, stats.cum_freqs[s], stats.freqs[s]);
        len--; // reduce len to even
    }

    // interleaved loop (reverse)
    for (size_t i = len; i > 0; i -= 2) {
        int s1 = in_bytes[i - 1];
        int s0 = in_bytes[i - 2];
        RansEncStep(state[1], ptr, stats.cum_freqs[s1], stats.freqs[s1]);
        RansEncStep(state[0], ptr, stats.cum_freqs[s0], stats.freqs[s0]);
    }

    // flush remaining state
    *--ptr = state[1] & 0xFFFF;
    *--ptr = state[0] & 0xFFFF;

    // update output length
    out_len = ptr - out_buf;
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

        auto enc_start = chrono::high_resolution_clock::now();
        BitArray encoded_bitarray = enc.encode(data);
        auto enc_stop = chrono::high_resolution_clock::now();

        uint32_t len = encoded_bitarray.size();

        auto dec_start = chrono::high_resolution_clock::now();
        tuple<string,uint32_t> decoded_data = dec.decode_block_interleave2(encoded_bitarray);
        auto dec_stop = chrono::high_resolution_clock::now();
        
        // add time to running sum/avg
        auto enc_time = chrono::duration_cast<chrono::microseconds>(enc_stop - enc_start);
        enc_avg_time += enc_time.count();
        auto dec_time = chrono::duration_cast<chrono::microseconds>(dec_stop - dec_start);
        dec_avg_time += dec_time.count();

        // if (len < 100) {
        //     cout << "Decoded: " << get<0>(decoded_data) << endl;
        //     cout << "Original: " << data << endl;
        // }

        if (get<1>(decoded_data) != len) {
            cout << "Bits consumed (" << get<1>(decoded_data) << ") does not match bitarray length (" << len << ")" << endl;
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
