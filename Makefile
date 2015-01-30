PROGRAMS:=\
        generate_multiqueue_data

.PHONY: all
all: $(PROGRAMS)

CXX:=g++
CXXFLAGS+=-g -Wall -fno-strict-aliasing -O8 -std=c++11
INCLUDES+=-I.
LIBS:=-lstdc++ -laio
INSTALL:=/usr/bin/install -c
PREFIX:=/usr
BINDIR:=$(DESTDIR)$(PREFIX)/bin
DATADIR:=$(DESTDIR)$(PREFIX)/share
MANPATH:=$(DATADIR)/man

SOURCE:=\
	generate_multiqueue_data.cc \
	multiqueue.cc \
	sampler.cc

OBJECTS:=$(subst .cc,.o,$(SOURCE))

.SUFFIXES: .d

%.o: %.cc
	@echo "	   [CXX] $<"
	$(V) $(CXX) -c $(INCLUDES) $(CXXFLAGS) -o $@ $<
	@echo "	   [DEP] $<"
	$(V) $(CXX) -MM -MT $(subst .cc,.o,$<) $(INCLUDES) $(TEST_INCLUDES) $(CXXFLAGS) $< > $*.$$$$; \
	sed 's,\([^ :]*\)\.o[ :]*,\1.o \1.gmo : Makefile ,g' < $*.$$$$ > $*.d; \
	$(RM) $*.$$$$


DEPEND_FILES=\
	$(subst .cc,.d,$(SOURCE))

-include $(DEPEND_FILES)

.PHONY: clean

clean:
	find . -name \*.o -delete
	find . -name \*.d -delete
	$(RM) $(PROGRAMS) lib/*.a

generate_multiqueue_data: $(OBJECTS)
	g++ $(CXXFLAGS) -O8 $+ -o $@ $(LIBS)
