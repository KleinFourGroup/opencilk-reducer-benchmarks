CC := $(HOME)/opencilk/build/bin/clang
CXX := $(HOME)/opencilk/build/bin/clang++

OPT := -O3
CILKFLAGS := -fopencilk -fcilktool=cilkscale

CFLAGS :=
LIBFLAGS := 

ifdef perf_num
	CFLAGS += -D PERF_NUM
endif

TARGET := BlackScholes
SOURCES := $(wildcard *.cpp)
OBJECTS := $(SOURCES:.cpp=.o)

%.o: %.cpp
	$(CXX) $(OPT) -g -c $(CILKFLAGS) $(CFLAGS) -o $@ $< 

$(TARGET): $(OBJECTS)
	$(CXX) $(OPT) $(CILKFLAGS) $^ $(LIBFLAGS) -o $(TARGET)

all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -fr $(TARGET) $(OBJECTS)
	rm -f *.bmp *.valsig

.PHONY: all clean
