C++ = g++
CFLAGS = -Wall -g -O0 -march=native
export warnflags = -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option
TFLAGS = -pthread
LDFLAGS =
LDLIBS =

PROGRAMS = rANS rANS_buf rANS_thread rANS_explicit rANS_explicit_opt rANS_implicit

all: default buf thread explicit explicit_opt implicit

default: rANS.cc rANS.hh
	$(C++) $(CFLAGS) -o rANS rANS.cc

buf: rANS_buf.cc rANS_buf.hh
	$(C++) $(CFLAGS) -o rANS_buf rANS_buf.cc

thread: rANS_thread.cc rANS_thread.hh
	$(C++) $(CFLAGS) $(TFLAGS) -o rANS_thread rANS_thread.cc

explicit: rANS_explicit.cc rANS_explicit.hh
	$(C++) $(CFLAGS) $(TFLAGS) -o rANS_explicit rANS_explicit.cc

explicit_opt: rANS_explicit_opt.cc rANS_explicit_opt.hh
	$(C++) $(CFLAGS) $(TFLAGS) -o rANS_explicit_opt rANS_explicit_opt.cc

implicit: rANS_implicit.cc rANS_implicit.hh
	$(C++) $(CFLAGS) $(TFLAGS) -o rANS_implicit rANS_implicit.cc 

clean::
	rm -rf $(PROGRAMS) *.o *.decoded *tmp* *.dSYM

.PHONY: clean all

.INTERMEDIATE: $(SOURCES:%=%.o)
