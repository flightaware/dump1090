CC ?= gcc
CFLAGS = -O3 -Wall -g

STARCH_COMPILE := $(CC) $(CFLAGS) -c

ARCH := $(shell uname -m)

all: generate starch-benchmark

ifneq (,$(findstring arm,$(ARCH)))
  -include generated/makefile.arm
else ifneq (,$(findstring x86_64,$(ARCH)))
  -include generated/makefile.x86_64
else
  -include generated/makefile.generic
endif

support.o: support.c
	$(CC) $(CFLAGS) -c -o $@ $^

starch-benchmark: $(STARCH_OBJS) $(STARCH_BENCHMARK_OBJ) support.o
	$(CC) $(CFLAGS) -o $@ $^

generate:
	./starchgen.py

clean:
	rm -f $(STARCH_OBJS) $(STARCH_BENCHMARK_OBJ) support.o starch-benchmark
