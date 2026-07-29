CXX = g++
CXXFLAGS = -std=c++20 -Wall -O3
CC = cc
CFLAGS = -std=c11 -Wall -O3
CFLAGS_C99 = -std=c99 -Wall -O3

SRC = src/fpfmt.cpp
SRC_UNOPT = src/fpfmt_unopt.cpp
SRC_FFI = src/fpfmt_ffi.cpp
SRC_C = src/fpfmt_c.c
SRC_C_OPT = src/fpfmt_c_opt.c
OBJ = src/fpfmt.o
OBJ_UNOPT = src/fpfmt_unopt.o
OBJ_C = src/fpfmt_c.o
OBJ_C_OPT = src/fpfmt_c_opt.o

CHEZ = chez

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	SHARED_EXT = dylib
	SHARED_FLAG = -dynamiclib
else
	SHARED_EXT = so
	SHARED_FLAG = -shared -fPIC
endif
FFI_LIB = bench/libfpfmt_ffi.$(SHARED_EXT)

TEST_SRCS = $(wildcard tests/*.cpp)
TESTS = $(TEST_SRCS:.cpp=)
TESTS_UNOPT = $(addsuffix _unopt,$(TESTS))
TEST_SRCS_C = $(wildcard tests/*_c.c)
TESTS_C = $(TEST_SRCS_C:.c=)
BENCH_SRCS = $(wildcard bench/bench_*.cpp)
BENCHES = $(BENCH_SRCS:.cpp=)
FLOATS_GEN = util/gen_floats

.PHONY: all test test_unopt test_c bench floats validate clean clean_results plots bench_fpfmt bench_fpfmt_unopt bench_uscale bench_fpfmt_c bench_fpfmt_c_opt bench_fpfmt_asm bench_cpp bench_c bench_ruby bench_scheme bench_fpfmt_ss

all: $(OBJ)

$(OBJ): $(SRC) src/fpfmt.hpp
	$(CXX) $(CXXFLAGS) -c $(SRC) -o $(OBJ)

$(OBJ_UNOPT): $(SRC_UNOPT) src/fpfmt_unopt.hpp
	$(CXX) $(CXXFLAGS) -c $(SRC_UNOPT) -o $(OBJ_UNOPT)

tests/%: tests/%.cpp $(OBJ) src/fpfmt.hpp
	$(CXX) $(CXXFLAGS) $< $(OBJ) -o $@

tests/%_unopt: tests/%.cpp $(OBJ_UNOPT) src/fpfmt_unopt.hpp
	$(CXX) $(CXXFLAGS) -DFPFMT_TEST_UNOPT $< $(OBJ_UNOPT) -o $@

$(OBJ_C): $(SRC_C) src/fpfmt_c.h src/pow10.h
	$(CC) $(CFLAGS_C99) -c $(SRC_C) -o $(OBJ_C)

$(OBJ_C_OPT): $(SRC_C_OPT) src/fpfmt_c.h src/pow10.h
	$(CC) $(CFLAGS_C99) -c $(SRC_C_OPT) -o $(OBJ_C_OPT)

tests/%_c: tests/%_c.c $(OBJ_C) src/fpfmt_c.h
	$(CC) $(CFLAGS_C99) $< $(OBJ_C) -lm -o $@

$(FLOATS_GEN): $(FLOATS_GEN).cpp bench/bench.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

bench/bench_fpfmt: bench/bench_fpfmt.cpp $(OBJ) src/fpfmt.hpp bench/bench.hpp
	$(CXX) $(CXXFLAGS) $< $(OBJ) -o $@

bench/bench_fpfmt_unopt: bench/bench_fpfmt_unopt.cpp $(OBJ_UNOPT) src/fpfmt_unopt.hpp bench/bench.hpp
	$(CXX) $(CXXFLAGS) $< $(OBJ_UNOPT) -o $@

bench/bench_cpp: bench/bench_cpp.cpp bench/bench.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

bench/bench_uscale: bench/bench_uscale.c src/uscale.c src/pow10.h bench/bench.h
	$(CC) $(CFLAGS) $< -lm -o $@

bench/bench_fpfmt_c: bench/bench_fpfmt_c.c $(OBJ_C) src/fpfmt_c.h bench/bench.h
	$(CC) $(CFLAGS_C99) $< $(OBJ_C) -lm -o $@

bench/bench_fpfmt_c_opt: bench/bench_fpfmt_c_opt.c $(OBJ_C_OPT) src/fpfmt_c.h bench/bench.h
	$(CC) $(CFLAGS_C99) $< $(OBJ_C_OPT) -lm -o $@

bench/bench_c: bench/bench_c.c bench/bench.h
	$(CC) $(CFLAGS_C99) $< -lm -o $@

$(FFI_LIB): $(SRC_FFI) $(SRC) src/fpfmt.hpp
	$(CXX) $(CXXFLAGS) $(SHARED_FLAG) $(SRC_FFI) $(SRC) -o $(FFI_LIB)

test: $(TESTS)
	@echo "\n=== running tests ===\n"
	@failed=0; \
	for t in $(TESTS); do \
		echo "--- $$t ---"; \
		./$$t || failed=1; \
		echo; \
	done; \
	if [ $$failed -eq 0 ]; then echo "=== ALL PASSED ==="; \
	else echo "=== SOME FAILED ==="; exit 1; fi
	$(MAKE) clean

test_unopt: $(TESTS_UNOPT)
	@echo "\n=== running tests (unopt) ===\n"
	@failed=0; \
	for t in $(TESTS_UNOPT); do \
		echo "--- $$t ---"; \
		./$$t || failed=1; \
		echo; \
	done; \
	if [ $$failed -eq 0 ]; then echo "=== ALL PASSED ==="; \
	else echo "=== SOME FAILED ==="; exit 1; fi
	$(MAKE) clean

test_c: $(TESTS_C)
	@echo "\n=== running tests (C) ===\n"
	@failed=0; \
	for t in $(TESTS_C); do \
		echo "--- $$t ---"; \
		./$$t || failed=1; \
		echo; \
	done; \
	if [ $$failed -eq 0 ]; then echo "=== ALL PASSED ==="; \
	else echo "=== SOME FAILED ==="; exit 1; fi
	$(MAKE) clean

floats: $(FLOATS_GEN)
	./$(FLOATS_GEN)
	rm -f $(FLOATS_GEN)

validate_tests:
	python3 validate/validate_tests.py

bench/results:
	mkdir -p bench/results

bench_fpfmt: bench/bench_fpfmt | bench/results
	@echo "--- bench_fpfmt ---"; \
	start=$$(date +%s); \
	./bench/bench_fpfmt; \
	echo "elapsed: $$(($$(date +%s) - start))s\n"

bench_fpfmt_unopt: bench/bench_fpfmt_unopt | bench/results
	@echo "--- bench_fpfmt_unopt ---"; \
	start=$$(date +%s); \
	./bench/bench_fpfmt_unopt; \
	echo "elapsed: $$(($$(date +%s) - start))s\n"

bench_uscale: bench/bench_uscale | bench/results
	@echo "--- bench_uscale ---"; \
	start=$$(date +%s); \
	./bench/bench_uscale; \
	echo "elapsed: $$(($$(date +%s) - start))s\n"

bench_fpfmt_c: bench/bench_fpfmt_c | bench/results
	@echo "--- bench_fpfmt_c ---"; \
	start=$$(date +%s); \
	./bench/bench_fpfmt_c; \
	echo "elapsed: $$(($$(date +%s) - start))s\n"

bench_fpfmt_c_opt: bench/bench_fpfmt_c_opt | bench/results
	@echo "--- bench_fpfmt_c_opt ---"; \
	start=$$(date +%s); \
	./bench/bench_fpfmt_c_opt; \
	echo "elapsed: $$(($$(date +%s) - start))s\n"

bench_cpp: bench/bench_cpp | bench/results
	@echo "--- bench_cpp ---"; \
	start=$$(date +%s); \
	./bench/bench_cpp; \
	echo "elapsed: $$(($$(date +%s) - start))s\n"

bench_c: bench/bench_c | bench/results
	@echo "--- bench_c ---"; \
	start=$$(date +%s); \
	./bench/bench_c; \
	echo "elapsed: $$(($$(date +%s) - start))s\n"

DENOISE_REPS = 3
bench_denoise: floats
	@echo "\n=== running native benchmarks (x$(DENOISE_REPS)) ===\n"; \
	start=$$(date +%s); \
	for i in $$(seq 1 $(DENOISE_REPS)); do \
		echo "\n--- denoising bench pass $$i/$(DENOISE_REPS) ---"; \
		$(MAKE) bench_fpfmt bench_fpfmt_unopt bench_uscale bench_fpfmt_c bench_cpp bench_c; \
	done; \
	end=$$(date +%s); \
	echo "\ntotal elapsed: $$((end - start))s"
	$(MAKE) clean

bench_ruby: | bench/results
	@echo "--- bench_ruby ---"; \
	start=$$(date +%s); \
	ruby bench/bench_ruby.rb; \
	echo "elapsed: $$(($$(date +%s) - start))s\n"

bench_scheme: | bench/results
	@echo "--- bench_scheme ---"; \
	start=$$(date +%s); \
	$(CHEZ) --script bench/bench_scheme.ss; \
	echo "elapsed: $$(($$(date +%s) - start))s\n"

bench_fpfmt_ss: $(FFI_LIB) | bench/results
	@echo "--- bench_fpfmt_ss ---"; \
	start=$$(date +%s); \
	$(CHEZ) --script bench/bench_fpfmt_ss.ss; \
	echo "elapsed: $$(($$(date +%s) - start))s\n"

bench: floats clean_results
	@echo "\n=== running benchmarks ===\n"; \
	start=$$(date +%s); \
	$(MAKE) bench_fpfmt; \
	$(MAKE) bench_fpfmt_unopt; \
	$(MAKE) bench_uscale; \
	$(MAKE) bench_fpfmt_c; \
	$(MAKE) bench_fpfmt_c_opt; \
	$(MAKE) bench_cpp; \
	$(MAKE) bench_c; \
	$(MAKE) bench_ruby; \
	$(MAKE) bench_scheme; \
	$(MAKE) bench_fpfmt_ss; \
	$(MAKE) clean; \
	$(MAKE) plots; \
	end=$$(date +%s); \
	echo "\ntotal elapsed: $$((end - start))s"

validate_benches:
	python3 validate/validate_benches.py

clean:
	rm -f $(OBJ) $(TESTS) $(TESTS_UNOPT) $(OBJ_UNOPT) $(OBJ_C) $(OBJ_C_OPT) $(OBJ_ASM_C) $(OBJ_ASM_S) $(TESTS_C) $(BENCHES) bench/bench_uscale bench/bench_fpfmt_c bench/bench_fpfmt_c_opt bench/bench_fpfmt_asm bench/bench_c $(FLOATS_GEN) $(FFI_LIB)
	rm -rf $(wildcard tests/*.dSYM) $(wildcard bench/*.dSYM)

clean_results:
	rm -f $(wildcard bench/results/*.csv)

plots:
	python3 plots/graph.py
	python3 plots/int_graph.py