CC := $(HOME)/opencilk/build/bin/clang
CXX := $(HOME)/opencilk/build/bin/clang++

HEADERS = $(wildcard *.h)
PRODUCT_SOURCES = $(wildcard *.c)
PRODUCT_OBJECTS = $(PRODUCT_SOURCES:.c=.o)

CFLAGS := -Wall -O1 -g -fopencilk -fno-vectorize -fno-slp-vectorize
EXTRA_CFLAGS :=
LDFLAGS := -fopencilk -fcilktool=cilkscale

ifneq ($(VECTOR_LEN),)
	EXTRA_CFLAGS := -DVECTOR_LEN=$(VECTOR_LEN) $(EXTRA_CFLAGS)
endif

ifneq ($(METHOD),)
	EXTRA_CFLAGS := -DMETHOD=$(METHOD) $(EXTRA_CFLAGS)
endif

all:	$(PRODUCT_OBJECTS)
	$(CC) $(LDFLAGS) -o main $(PRODUCT_OBJECTS)

%.o:		%.c $(HEADERS)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ -c $<

clean:
	rm -f main *.o *.s *.ll
