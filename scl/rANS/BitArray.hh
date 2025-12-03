// Utility file containing implementation of custom BitArray class
// BitArray class represents an array/stream of bits, implemented as a stack
// - push and pop operations available, interact with the top of the stack
// - push_ and pop_many convenience operations also available, taking an array of bits (bools) and repeatedly calling push/pop
// - underlying structure is a vector of bytes (uint8_t), which supposedly has less internal overhead and better efficiency than vector<bool>
// - slight overhead incurred to do bit-granular operations with bytes in the vector, and to keep track of pointers
// - ** this class uses the assumption that encoding is done in reverse (i.e. top of stack represents "front" of stream, which will be decoded in forward direction)

// note: using custom class to encode bitarrays, operations done on bitarrays will use bitwise operations

#include <vector>

using namespace std;

class BitArray {
    private:
        vector<uint8_t> data;
        int8_t bit_ptr;
        uint64_t nelem;

    public:
        BitArray() {
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

        bool equals(BitArray bits) {
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

        void concatenate(vector<uint8_t>& data2) {
            data.insert(data.end(), data2.begin(), data2.end());
        }
};