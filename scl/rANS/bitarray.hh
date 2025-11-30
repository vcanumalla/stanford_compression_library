#ifndef BITARRAY_HH
#define BITARRAY_HH

#include <vector>
#include <string>
#include <stdexcept>
using namespace std;

class BitArray {
private:
    vector<bool> bits;
public:
    BitArray() = default;
    BitArray(const std::string& str) {
        for (char c : str) {
            if (c == '1') {
                bits.push_back(true);
            } else if (c == '0') {
                bits.push_back(false);
            } else {
                throw std::runtime_error("Invalid character in BitArray string");
            }
        }
    }

    size_t size() const {
        return bits.size();
    }

    bool operator[](size_t index) const {
        return bits[index];
    }

    void push_back(bool bit) {
        bits.push_back(bit);
    }

    bool pop_back() {
        if (bits.empty()) {
            throw std::runtime_error("BitArray is empty");
        }
        else {
            bool bit = bits.back();
            bits.pop_back();
            return bit;
        }
    }
    void append(const BitArray& arg2) {
        bits.insert(bits.end(), arg2.bits.begin(), arg2.bits.end());
    }
    void prepend(const BitArray& arg2) {
        bits.insert(bits.begin(), arg2.bits.begin(), arg2.bits.end());
    }
    BitArray slice(size_t start, size_t length) const {
        if (start + length > bits.size()) {
            throw std::runtime_error("Slice out of bounds");
        }
        BitArray result = bits.
    
}
#endif