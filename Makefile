PROGRAMS:=\
	generate_cache_data \
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
	sampler.cc

OBJECTS:=$(subst .cc,.o,$(SOURCE))

.SUFFIXES: .d

%.o: %.cc
	@echo "	   [CXX] $<"
	$(V) $(CXX) -c $(INCLUDES) $(CXXFLAGS) -o $@ $<
	@echo "	   [DEP] $<"
	$(V) $(CXX) -MM -MT $(subst .cc,.o,$<) $(INCLUDES) $(TEST_INCLUDES) $(CXXFLAGS) $< > $*.$$$$; \
	sed 's,\([^ :]*\)\.o[ :]*,\1.o : Makefile ,g' < $*.$$$$ > $*.d; \
	$(RM) $*.$$$$


DEPEND_FILES=\
	$(subst .cc,.d,$(SOURCE)) \
	generate_cache_data.d \
	generate_multiqueue_data.d

-include $(DEPEND_FILES)

.PHONY: clean

clean:
	find . -name \*.o -delete
	find . -name \*.d -delete
	$(RM) $(PROGRAMS) lib/*.a

generate_cache_data: $(OBJECTS) generate_cache_data.o
	g++ $(CXXFLAGS) -O8 $+ -o $@ $(LIBS)

generate_multiqueue_data: $(OBJECTS) generate_multiqueue_data.o
	g++ $(CXXFLAGS) -O8 $+ -o $@ $(LIBS)

MULTIQUEUE_DATA_FILES=\
	ha_vs_levels.dat \
	ha_with_changing_pdf_vs_adjustments.dat \
	level_population.dat \
	ha_vs_percent.dat \
	hits_vs_adjustments.dat \
	pdf.dat \
	ha_with_changing_pdf_and_autotune.dat \
	hits_vs_levels.dat \
	summation_table.dat

$(MULTIQUEUE_DATA_FILES): generate_multiqueue_data
	./generate_multiqueue_data

graphs: $(PROGRAMS) $(MULTIQUEUE_DATA_FILES)
	R --no-save < draw_graphs.R
