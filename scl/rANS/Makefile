SOURCES = rANS
PROGRAMS = $(SOURCE:%=%)

C++ = g++
# CFLAGS = -g3 -std=c++17 -Wall $$warnflags -fcf-protection=none -fno-pic -no-pie -O2
CFLAGS = -std=c++17 -Wall
export warnflags = -Wfloat-equal -Wtype-limits -Wpointer-arith -Wlogical-op -Wshadow -Winit-self -fno-diagnostics-show-option
LDFLAGS =
LDLIBS =

$(PROGRAMS): %:%.o rANS.hh
	$(C++) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean::
	@rm -f $(PROGRAMS) *.o callgrind.out.*

.PHONY: clean all

.INTERMEDIATE: $(SOURCES:%=%.o)
