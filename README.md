## Summary
This repo houses my iterative implementation of Russ Cox's uscale floating point formatting and parsing algorithm. This codebase rewrites his algorithm in C++ (`src/fpfmt.cpp` & `src/fpfmt_unopt.cpp`) following his paper, [Floating-Point Printing and Parsing Can Be Simple And Fast](https://research.swtch.com/fp). 

I also benchmark his C implementation, `src/uscale.c`, found in his [Github Repo](https://github.com/rsc/fpfmt) against my C++ rewrites, Chez Scheme utilizing my C++ rewrite as an FFI (foreign function interface), and Ruby's, Chez Scheme's, and C++'s native floating point formatters/parsers. 

I also attempt to rewrite his `src/uscale.c` into a more portable, standard C implementation (`src/fpfmt_c.c`), to be used on any compiler/system, including building for use as a WASM. In the future, I plan to attempt extending his algorithm to work on any base between 3-36 to potentially be supported/contributed to Chez Scheme. 

All credit where credit is due; this codebase respects [The Go Authors' Copyright](LICENSE.md).

## Benchmarking & Testing Prerequisites
  - C++20 compiler
  - C11 & C99 compiler
  - Ruby
  - Chez Scheme

## Building Prerequisites
  - C++20 compiler (for C++ build)
  - C99 compiler (for C build)
  - EMCC (for WASM build)

## Makefile Usage (Benchmarking & Testing):
| Command                  | Description                                                                                                                                                                                                      |
| ---                      | ---                                                                                                                                                                                                              |
| `make test`              | tests `src/fpfmt.cpp` with targeted unit tests, along with the contents of `data/test.ivy` (obtained from Russ Cox's repo), `data/test.ib` (generated edge cases), and `data/test.more` (more generated cases).  |
| `make test_c`            | tests the portable, standard C `src/fpfmt_c.c` with the same tests as above.                                                                                                                                     |
| `make plots`             | generates static and interactive plots in `plots/graphs/` and `plots/interactive_graphs/` respectively, based on the current `bench/results/` csv files.                                                         |
| `make floats`            | generates shared `data/bench_floats.csv` input set the benches read from. Run by `make bench` and `make bench_denoise`; only needed by itself before running an individual `bench_*` target directly.            |
| `make bench_fpfmt`       | benchmarks the C++ implementation of fpfmt and appends to the csv files in `bench/results/` (~1m).                                                                                                               |
| `make bench_fpfmt_unopt` | benchmarks the unoptimized C++ implementation of fpfmt and appends to the csv files in `bench/results/` (~1m).                                                                                                   |
| `make bench_fpfmt_c`     | benchmarks the portable C implementation of fpfmt and appends to the csv files in `bench/results/` (~1m).                                                                                                        |
| `make bench_fpfmt_ss`    | benchmarks fpfmt FFI (foreign function interface) via Scheme and appends to the csv files in `bench/results/` (~10s).                                                                                            |
| `make bench_cpp`         | benchmarks the native C++ `std::to_chars` and `std::from_chars` methods and appends to the csv files in `bench/results/` (~1m).                                                                                  |
| `make bench_c`           | benchmarks the native C `strtod` and `snprintf` methods and appends to the csv files in `bench/results/`. Doesn't have a shortest width formatting equivalent. (~4m)                                             |
| `make bench_uscale`      | benchmarks Russ Cox's C implementation of fpfmt and appends to the csv files in `bench/results/` (~1m).                                                                                                          |
| `make bench_ruby`        | benchmarks the native Ruby `format`, `to_f`, and `to_s` methods and appends to the csv files in `bench/results/` (~2m).                                                                                          |
| `make bench_scheme`      | benchmarks the native Scheme `format`, `number->string`, and `string->number` methods and appends to the csv files in `bench/results/` (~11m).                                                                   |
| `make bench`             | calls `make clean_results`, then calls the benchmark for each of the above implementations, then calls `make clean_results` and `make clean plots`. (~25m).                                                      |
| `make bench_denoise`     | runs 3 more rounds of benchmarks for `fpfmt`, `fpfmt_unopt`, `fpfmt_c`, `uscale`, `native_cpp`, and `native_c` and appends to the csv files in `bench/results/` to alleviate effects of clock noise (~30m).      |
| `make validate_tests`    | validates `data/test.ivy`, `data/test.ib`, and `data/test.more` with Python's exact Fraction module.                                                                                                             |
| `make validate_benches`  | validates the outputs of the benchmarks using Python's exact Fraction and exact Decimal modules. `make bench` must be run beforehand to generate `bench/results/validate.csv`.                                   |
| `make clean`             | deletes compiled object files, test/benchmark binaries, and .dSYM directories.                                                                                                                                   |
| `make clean_results`     | deletes any current benchmark results from `bench/results/`                                                                                                                                                      |

## CMake Usage (Building):
| Build | Command                                              | Note                              |
| ---   | ---                                                  | ---                               |
| C++   | `cmake --preset cpp && cmake --build --preset cpp`   | builds optimized `src/fpfmt.cpp`. |
| C     | `cmake --preset c && cmake --build --preset c`       | builds portable `src/fpfmt_c.c`.  |
| WASM  | `cmake --preset wasm && cmake --build --preset wasm` | builds C implementation as WASM.  |

## Generated Files
  - `src/pow10.h` / `src/pow10.hpp`: power-of-10 tables generated by `util/gen_pow_table.py`. Do not edit directly unless you understand the implications.
  - `data/test.ib` / `data/test.more`: additional test data generated by `util/gen_print_parse_tests.py`.