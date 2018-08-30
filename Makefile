DEPDIR = .deps
df = $(DEPDIR)/$(*F)
MAKEDEPEND = $(CXX) -M $(CXXFLAGS) -MT $*.o -MT $*.d -MF $*.d $<

EXECS = runner
TESTS = $(patsubst tests/%.cpp,%,$(wildcard tests/*.cpp))
COMMON_SRC = $(wildcard src/common/*.cpp)
COMMON_O = $(patsubst %.cpp,%.o,$(COMMON_SRC))
PCH = src/stdafx.h
LIB_PATHS += /usr/local/lib
LIBS += -lstdc++fs -lboost_system -lboost_filesystem -lboost_coroutine -pthread
LIBS += -lmlpack -larmadillo

INCLUDES += include

SYS_INCLUDES += /usr/local/include
SYS_INCLUDES += deps/json/single_include
SYS_INCLUDES += deps/spdlog/include
SYS_INCLUDES += deps/Catch2/single_include
SYS_INCLUDES += deps/cxxopts/include

FLAGS += -std=c++14 -ftemplate-backtrace-limit=0
LDFLAGS += $(FLAGS)
LDFLAGS += $(patsubst %,-L%,$(LIB_PATHS))
CXXFLAGS += $(FLAGS) -Wall -Wextra -pedantic -Werror -O2 -g
CXXFLAGS += $(patsubst %,-I%,$(INCLUDES))
CXXFLAGS += $(patsubst %,-isystem %,$(SYS_INCLUDES))

SRCS = $(wildcard $(patsubst %,src/%/*.cpp,$(EXECS)))
SRCS += $(COMMON_SRC) $(PCH)

all: execs tests

.SUFFIXES:
.PRECIOUS: %.cpp %.o %.hpp %.d

.PHONY: execs clean realclean all debug

debug: CXXFLAGS := $(CXXFLAGS) -DSPDLOG_DEBUG_ON -DSPDLOG_TRACE_ON -O0
debug: all

clean:
	-rm $(EXECS:%=bin/%) $(EXECS:%=src/%/*.o) $(COMMON_O)
	-rm $(TESTS:%=bin/tests/%) $(TESTS:%=tests/%.o)
	-rm $(PCH).d $(PCH).gch

realclean: clean
	-rm $(patsubst %,src/%/*.d,$(EXECS))
	-rm $(patsubst %.o,%.d,$(COMMON_O))
	-rm $(patsubst %,tests/%.d,$(TESTS))

execs: $(EXECS:%=bin/%)

tests: $(TESTS:%=bin/tests/%)

PCT = %
.SECONDEXPANSION:
$(EXECS:%=bin/%): bin/% : \
		$$(patsubst $$(PCT).cpp,$$(PCT).o,$$(wildcard src/$$(@:bin/$$(PCT)=$$(PCT))/*.cpp)) \
		$(COMMON_O)
	@mkdir -p bin
	$(CXX) $(LDFLAGS) $^ $(LIBS) -o $@

$(TESTS:%=bin/tests/%): bin/tests/% : $(COMMON_O) tests/%.o
	@mkdir -p bin/tests/
	$(CXX) $(LDFLAGS) $^ $(LIBS) -o $@

$(COMMON_O) : Makefile

%.o: %.d Makefile $(PCH).d $(PCH).gch
	$(CXX) $(CXXFLAGS) -include $(PCH) $(<:%.d=%.cpp) -c -o $@

$(PCH).d: $(PCH) Makefile
	$(CXX) -M $(CXXFLAGS) -MT $(PCH).gch -MF $(PCH).d $(PCH)

%.d: %.cpp Makefile
	$(MAKEDEPEND)

$(PCH).gch : $(PCH)
	$(CXX) $(CXXFLAGS) $< -o $@

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),realclean)
include $(SRCS:%.cpp=%.d) $(PCH).d
endif
endif
