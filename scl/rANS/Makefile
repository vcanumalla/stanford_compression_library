C++ = g++
CFLAGS = -Wall -g -O3 -march=native
export warnflags = -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option
TFLAGS = -pthread
LDFLAGS =
LDLIBS =


build: rANS.cc rANS.hh
	$(C++) $(CFLAGS) -o rANS rANS.cc

buf: rANS_buf.cc rANS_buf.hh
	$(C++) $(CFLAGS) -o rANS_buf rANS_buf.cc

thread: rANS_thread.cc rANS_thread.hh
	$(C++) $(CFLAGS) $(TFLAGS) -o rANS_thread rANS_thread.cc

clean::
	@rm -f $(PROGRAMS) *.o callgrind.out.* *.decoded *tmp*

.PHONY: clean all

.INTERMEDIATE: $(SOURCES:%=%.o)
