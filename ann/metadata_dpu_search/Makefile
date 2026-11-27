UPMEM_HOME ?= /home/vboxuser/pim/upmem-sdk
DPU_CC := $(UPMEM_HOME)/bin/dpu-upmem-dpurte-clang
DPU_PKG_CONFIG := UPMEM_HOME=$(UPMEM_HOME) $(UPMEM_HOME)/bin/dpu-pkg-config
HOST_CXX := g++

BUILD_DIR := build
DPU_BINARY := $(BUILD_DIR)/dpu_search
HOST_BINARY := $(BUILD_DIR)/metadata_dpu_search

DPU_SOURCE := dpu/search.c
HOST_SOURCES := src/host.cpp src/metadata.cpp ../faiss_cpp/src/vecs_io.cpp
HOST_INCLUDES := -Iinclude -I../faiss_cpp/include

.PHONY: all run test clean versions

all: $(DPU_BINARY) $(HOST_BINARY)

$(BUILD_DIR):
	mkdir -p $@

$(DPU_BINARY): $(DPU_SOURCE) include/search_common.h | $(BUILD_DIR)
	$(DPU_CC) -std=c11 -O2 -DNR_TASKLETS=1 -Iinclude -o $@ $(DPU_SOURCE)

$(HOST_BINARY): $(HOST_SOURCES) include/search_common.h include/metadata.hpp | $(BUILD_DIR)
	$(HOST_CXX) -std=c++20 -O2 -ffp-contract=off -Wall -Wextra -Wpedantic \
		-Wconversion -Wshadow $(HOST_INCLUDES) \
		-DDPU_BINARY='"$(abspath $(DPU_BINARY))"' $(HOST_SOURCES) -o $@ \
		$$($(DPU_PKG_CONFIG) --cflags --libs dpu) \
		-Wl,-rpath,$(UPMEM_HOME)/lib

run: all
	UPMEM_HOME=$(UPMEM_HOME) LD_LIBRARY_PATH=$(UPMEM_HOME)/lib \
		UPMEM_PROFILE_BASE=backend=simulator ./$(HOST_BINARY) $(ARGS)

test: all
	./run_tests.sh

versions:
	@$(DPU_CC) --version | head -n 1
	@$(DPU_PKG_CONFIG) --modversion dpu

clean:
	rm -rf $(BUILD_DIR)

