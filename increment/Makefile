DPU_CC := dpu-upmem-dpurte-clang
HOST_CC := gcc
BUILD_DIR := build
DPU_BINARY := $(BUILD_DIR)/dpu
HOST_BINARY := $(BUILD_DIR)/host

.PHONY: all run clean versions

all: $(DPU_BINARY) $(HOST_BINARY)

$(BUILD_DIR):
	mkdir -p $@

$(DPU_BINARY): dpu.c | $(BUILD_DIR)
	$(DPU_CC) -O2 -o $@ $<

$(HOST_BINARY): host.c | $(BUILD_DIR)
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -DDPU_BINARY='"$(DPU_BINARY)"' $< -o $@ $$(dpu-pkg-config --cflags --libs dpu)

run: all
	UPMEM_PROFILE_BASE=backend=simulator ./$(HOST_BINARY)

versions:
	@echo "DPU compiler:" && $(DPU_CC) --version | head -n 1
	@echo "Host compiler:" && $(HOST_CC) --version | head -n 1
	@echo "DPU pkg-config:" && dpu-pkg-config --modversion dpu

clean:
	rm -rf $(BUILD_DIR)
