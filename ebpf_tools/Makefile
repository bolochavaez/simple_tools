CC = clang
EBPF_CFLAGS =  -O2 -g -target bpf -c
EBPF_SOURCE = arc_evict_tracer.c
CFLAGS = -lbpf 
SOURCE = usl.c
BUILD_DIR = build

all: ebpf_program usl

usl: $(SOURCE)
	$(CC) $(CLFAGS) -o $(BUILD_DIR)/usl $(SOURCE)

ebpf_program: $(EBPF_SOURCE)
	$(CC) $(EBPF_CLFAGS) -o $(BUILD_DIR)/arc_evict_tracer.o $(EBPF_SOURCE)
