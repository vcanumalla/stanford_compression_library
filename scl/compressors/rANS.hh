#include <string>
#include <iostream>
#include <tuple>
using namespace std;

struct Frequencies {
    int total_freq();
};

struct rANSParams {};

class encoder {
    private:
        rANSParams params;

    public:
        encoder(rANSParams rans_params);
        int base_encode_step(char s, int state);
        bitarray shrink_state(int& state, char next_symbol);
        bitarray encode_symbol(int& state, char next_symbol);
        bitarray encode_block(string data_block);
};

class decoder {
    private:
        rANSParams params;

    public:
        decoder(rANSParams rans_params);
        int find_bin(int cum_freq_list[], int slot);
        char base_decode_step(int& state);
        int expand_state(int& state, bitarray encoded_bitarray);
        tuple<char, int> decode_symbol(int& state, bitarray encoded_bitarray);
        tuple<string, int> decode_block(bitarray encoded_bitarray);
};