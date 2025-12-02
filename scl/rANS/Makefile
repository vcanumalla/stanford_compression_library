C++ = g++
CFLAGS = -Wall -g -O3 -march=native
export warnflags = -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option
TFLAGS = -pthread
LDFLAGS =
LDLIBS =

PROGRAMS = rANS rANS_buf rANS_thread rANS_implicit rANS_implicit_opt


build: rANS.cc rANS.hh
	$(C++) $(CFLAGS) -o rANS rANS.cc

buf: rANS_buf.cc rANS_buf.hh
	$(C++) $(CFLAGS) -o rANS_buf rANS_buf.cc

thread: rANS_thread.cc rANS_thread.hh
	$(C++) $(CFLAGS) $(TFLAGS) -o rANS_thread rANS_thread.cc

implicit: rANS_implicit.cc rANS_implicit.hh
	$(C++) $(CFLAGS) $(TFLAGS) -o rANS_implicit rANS_implicit.cc

implicit_opt: rANS_implicit_opt.cc rANS_implicit_opt.hh
	$(C++) $(CFLAGS) $(TFLAGS) -o rANS_implicit_opt rANS_implicit_opt.cc

clean::
	@rm -f $(PROGRAMS) *.o *.decoded *tmp*

.PHONY: clean all

.INTERMEDIATE: $(SOURCES:%=%.o)
