
from scl.compressors.rANS import rANSEncoder, rANSDecoder, rANSParams

import sys
import os
import time

from scl.core.prob_dist import Frequencies
from scl.core.data_block import DataBlock
from scl.core.data_stream import TextFileDataStream

def compute_frequencies_from_file(file_path):
    """Read a text file and compute character frequencies."""
    counts = {}
    with open(file_path, 'r', encoding='utf-8') as f:
        for line in f:
            for char in line:
                counts[char] = counts.get(char, 0) + 1
    return Frequencies(counts)


# Usage: python run_python.py <input_file> <output_file>
def main():
    if len(sys.argv) != 3:
        print("Usage: python run_python.py <input_file> <output_file>")
        sys.exit(1)
    
    input_file_path = sys.argv[1]
    output_file_path = sys.argv[2]

    if not os.path.exists(input_file_path):
        print(f"Error: Input file '{input_file_path}' does not exist.")
        sys.exit(1)
    
    print(f"Computing frequencies from {input_file_path}...")
    freqs = compute_frequencies_from_file(input_file_path)
    print(f"Found {freqs.size} unique characters")
    print(f"Total characters: {freqs.total_freq}")
    
    # Create rANS parameters
    print("Creating rANS encoder/decoder...")
   
    # IMPORTANT: Sort freq keys lexicographically in the dict. This mirrors the
    # std::map ordering on the C++ side and keeps benchmarks deterministic.
    freqs.freq_dict = dict(sorted(freqs.freq_dict.items(), key=lambda item: item[0]))
    # output list of keys to txt
    with open("freqs_python.txt", "w") as f:
        for key in freqs.freq_dict.keys():
            f.write(key + "\n")
    rans_params = rANSParams(freqs)

    encoder = rANSEncoder(rans_params)
    decoder = rANSDecoder(rans_params)

    print(f"Compressing {input_file_path} -> {output_file_path}...")
    start_time = time.time()
    encoder.encode_file(input_file_path, output_file_path)
    end_time = time.time()
    print(f"Compression time: {end_time - start_time:.2f} seconds")
    
    input_size = os.path.getsize(input_file_path)
    output_size = os.path.getsize(output_file_path)
    compression_ratio = input_size / output_size if output_size > 0 else 0

    print(f"\nCompression complete!")
    print(f"Input size:  {input_size:,} bytes")
    print(f"Output size: {output_size:,} bytes")
    print(f"Compression ratio: {compression_ratio:.2f}x")


    decompressed_file = output_file_path + ".decompressed"
    print(f"Decompressing to {decompressed_file}...")
    start_time = time.time()
    decoder.decode_file(output_file_path, decompressed_file)
    end_time = time.time()
    print(f"Decompression time: {end_time - start_time:.2f} seconds")
    
    # Check if files match
    with open(input_file_path, 'r', encoding='utf-8') as f1:
        with open(decompressed_file, 'r', encoding='utf-8') as f2:
            if f1.read() == f2.read():
                print("PASS: Decompression verified: files match!")
                os.remove(decompressed_file)
                print(f"Removed temporary file {decompressed_file}")
                sys.exit(0)
            else:
                print("FAIL: Decompressed file does not match original!")
                print(f"Decompressed file kept at {decompressed_file} for inspection")
                sys.exit(1)
    
if __name__ == "__main__":
    main()
    
