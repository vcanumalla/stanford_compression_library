## rANS readme
### Running the compressor
There are two ways to run the compressor. First, to run all tests and receive a structured output with plots and CSV logs, use the Python script in /eval/run_tests.py. This script automatically compiles all implementations and runs them on three source texts: Carol, Shakespeare, and Sherlock. The three texts can be found in /texts/ and were obtained from Project Gutenberg. Results will be foldered in /eval/.
Second, to run a specific or custom test, the Makefile can be used. There is a target for each implementation, or one can run `make` and have all implementations be compiled and built. Then, they can be run individually with `./rANS{impl} input.txt output.txt`. Statistics such as encode and decode time and compression ratio will be logged and printed to the terminal. It is also possible to modify a parameter in the source C++ code to run the internal test multiple times to get averages across a batch of runs.
Finally, the run_python.py script retrieves SCL's Python implementation and runs it on an input file specified in the command line, outputting detailed statistics to be compared with other implementations.

### Results
Results and analysis can be found in our report.

### Notes
Besides run_python.py, there are no dependencies on any SCL utility. The rANS directory is essentially standalone.
The implementations are currently limited to ASCII texts, as the corresponding symbols are one byte and can be read by our parsing functions. Source texts with multi-byte characters may be compressed, but might result in suboptimal performance. Adding this support is a future extension and requires a small amount of work to the frontend.

### Files
This folder contains the source files needed to run our rANS compressor on a given input text. There are five implementations:
- rANS(.cc/.hh): This is the base C++ implementation with no optimizations added
- rANS_buf(.cc/.hh): This is a modification of the base implementation with a different data structure backing up the bitstream and a faster decoding method.
- rANS_explicit(.cc/.hh): This is an implementation using explicit interleaving for both encoding and decoding, without the optimizations of rANS_buf
- rANS_explicit_opt(.cc/.hh): This is the same explicit interleaving algorithm, but with the added optimizations of rANS_buf
- rANS_implicit(.cc/.hh): This is an implicit version of the interleaving implementation, with previous optimizations applied
- rANS_thread(.cc/.hh): This is a multithreaded approach to the rANS algorithm, with previous optimizations applied

- Both rANS and rANS_explicit use BitArrays (BitArray.hh) to represent the encoded bitstream. The optimized implementations use pointers and dynamically allocated memory.

