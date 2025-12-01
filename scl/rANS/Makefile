C++ = g++
CFLAGS = -Wall -g -O3 -march=native
export warnflags = -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option
LDFLAGS =
LDLIBS =


build: rANS.cc rANS.hh
	$(C++) $(CFLAGS) -o rANS rANS.cc

clean::
	rm -f *.o rANS *tmp*

.PHONY: clean all
