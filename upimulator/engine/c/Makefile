CC ?= cc
CPPFLAGS := -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -pthread
LDLIBS := -lm -pthread
ifeq ($(shell uname -s),Linux)
LDLIBS += -ldl
endif

SOURCES := \
	src/main.c \
	src/engine.c \
	src/input_loader.c \
	src/partitioner.c \
	src/cpu_backend.c \
	src/dpu_backend.c \
	src/workload_loader.c
OBJECTS := $(SOURCES:.c=.o)
PLUGIN_SOURCES := $(wildcard workloads/*/plugin.c)
PLUGINS := $(PLUGIN_SOURCES:.c=.so)
TEST_PLUGIN := tests/incompatible_plugin.so
OVERLAP_TEST_PLUGIN := tests/overlap_plugin.so
JOIN_FAILURE_INJECTOR := tests/join_failure_injector.so

.PHONY: all clean test

all: engine $(PLUGINS)

engine: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDLIBS) -o $@

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

workloads/%.so: workloads/%.c include/workload_plugin.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $< -o $@

tests/incompatible_plugin.so: tests/incompatible_plugin.c include/workload_plugin.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $< -o $@

tests/overlap_plugin.so: tests/overlap_plugin.c include/workload_plugin.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $< -o $@

tests/join_failure_injector.so: tests/join_failure_injector.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $< -o $@

test: all $(TEST_PLUGIN) $(OVERLAP_TEST_PLUGIN) $(JOIN_FAILURE_INJECTOR)
	./tests/test_engine.sh

clean:
	rm -f $(OBJECTS) engine $(PLUGINS) $(TEST_PLUGIN) \
		$(OVERLAP_TEST_PLUGIN) $(JOIN_FAILURE_INJECTOR)
