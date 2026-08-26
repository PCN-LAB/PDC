# CS-3006 Parallel and Distributed Computing
# Demo 01: pthreads, performance counters, affinity, frequency scaling
#
#   make          build everything into ./bin
#   make run      build and run demos 01-04 with sensible defaults
#   make clean

UNAME_S := $(shell uname -s)
CC      ?= cc
CSTD    := -std=gnu11
WARN    := -Wall -Wextra -Wno-unused-parameter
OPT     := -O2 -g

CFLAGS  := $(CSTD) $(WARN) $(OPT) -pthread -Isrc
LDFLAGS := -pthread

ifeq ($(UNAME_S),Darwin)
  PLAT   := src/plat_macos.c
  CFLAGS += -D_DARWIN_C_SOURCE
else
  PLAT   := src/plat_linux.c
  CFLAGS += -D_GNU_SOURCE
  LDLIBS += -lm
endif

CORE    := src/common.c src/plat_common.c $(PLAT)
DEMOS   := 01_hello 02_scaling 03_false_sharing 04_affinity 05_frequency
BINS    := $(addprefix bin/,$(DEMOS))

.PHONY: all run clean info
all: $(BINS)

bin:
	@mkdir -p bin

bin/%: src/%.c $(CORE) src/common.h src/plat.h | bin
	$(CC) $(CFLAGS) -o $@ $< $(CORE) $(LDFLAGS) $(LDLIBS)

info:
	@echo "platform : $(UNAME_S)"
	@echo "backend  : $(PLAT)"
	@echo "compiler : $(CC)"
	@$(CC) --version | head -1

run: all
	@./bin/01_hello -t 4
	@./bin/02_scaling -s 0.6
	@./bin/03_false_sharing
	@./bin/04_affinity -s 0.6

clean:
	rm -rf bin
