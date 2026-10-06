all: libwasmmutator.so

BINARYEN_SRC ?= ../../../binaryen/src
LINKER_PATH  ?= /usr/local/lib

INCLUDE_FLAGS = -I../../include -I$(BINARYEN_SRC) -I.
CFLAGS       += -fPIC -g $(INCLUDE_FLAGS)
CXXFLAGS     += -fPIC -g -std=c++17 $(INCLUDE_FLAGS)
LDFLAGS      += -L$(LINKER_PATH) -lm -lbinaryen -Wl,-rpath,$(LINKER_PATH)

C_SOURCES = wasm_mutators.c mutator_func.c helper_func.c init_func.c hash_func.c generator_func.c
OBJECTS = $(C_SOURCES:.c=.o) binaryen_bridge.o

libwasmmutator.so: $(OBJECTS)
	$(CXX) -shared $^ -o $@ $(LDFLAGS)

%.o: %.c mutator_func.h binaryen_bridge.h
	$(CC) $(CFLAGS) -c $< -o $@

binaryen_bridge.o: binaryen_bridge.cpp binaryen_bridge.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f libwasmmutator.so $(OBJECTS)

.PHONY: all clean test
