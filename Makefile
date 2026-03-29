CC       = gcc
NVCC     = nvcc

CFLAGS    = -Wall -O3 -fopenmp
# NVCCFLAGS = --generate-code arch=compute_60,code=sm_60 -O3
NVCCFLAGS = --generate-code arch=compute_86,code=sm_86 -O3
LDFLAGS   = -lm

C_SRCS  := $(wildcard src/*.c)
CU_SRCS := $(wildcard src/*.cu)

C_BINS  := $(C_SRCS:.c=)
CU_BINS := $(CU_SRCS:.cu=)

all: $(C_BINS) $(CU_BINS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

%: %.cu
	$(NVCC) $(NVCCFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(C_BINS) $(CU_BINS)