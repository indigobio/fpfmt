#ifndef BENCH_HPP
#define BENCH_HPP

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <vector>
#include <algorithm>
#include <string>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <charconv>
#include "../src/fpfmt.hpp"
#include "../src/fpfmt_unopt.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////
// timing
///////////////////////////////////////////////////////////////////////////////////////////////////
inline uint64_t now_ns() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// bench harness
///////////////////////////////////////////////////////////////////////////////////////////////////
inline double bench_fixed(char* dst, int count, double* f, int nf, int digits, void (*fn)(char*, double, int)) {
	uint64_t start = now_ns();
	for (int i = 0; i < count; i++)
		for (int j = 0; j < nf; j++)
			fn(dst, f[j], digits);
	uint64_t end = now_ns();
	return (double)(end - start) / (count * nf);
}

inline double bench_short(char* dst, int count, double* f, int nf, void (*fn)(char*, double)) {
	uint64_t start = now_ns();
	for (int i = 0; i < count; i++)
		for (int j = 0; j < nf; j++)
			fn(dst, f[j]);
	uint64_t end = now_ns();
	return (double)(end - start) / (count * nf);
}

inline double bench_short_raw(uint64_t* dp, int64_t* pp, int count, double* f, int nf, void (*fn)(uint64_t*, int64_t*, double)) {
	uint64_t start = now_ns();
	for (int i = 0; i < count; i++)
		for (int j = 0; j < nf; j++)
			fn(dp, pp, f[j]);
	uint64_t end = now_ns();
	return (double)(end - start) / (count * nf);
}

inline double bench_parse(int count, char* text, double (*fn)(char* start, char* end)) {
	double total = 0;
	uint64_t start_t = now_ns();
	for (int i = 0; i < count; i++) {
		int start = 0;
		total = 0;
		for (int j = 0;; j++) {
			int c = text[j];
			if (c == '\0' || c == '\n') {
				total += fn(text + start, text + j);
				if (c == '\0') break;
				start = j + 1;
			}
		}
	}
	uint64_t end_t = now_ns();
	int n = 0;
	for (int j = 0; text[j]; j++) if (text[j] == '\n') n++;
	n++;
	(void)total;
	return (double)(end_t - start_t) / (count * n);
}

inline double bench_parse_raw(int count, int64_t* raw, int nraw, double (*fn)(uint64_t, int)) {
	double total = 0;
	uint64_t start_t = now_ns();
	for (int i = 0; i < count; i++) {
		total = 0;
		for (int j = 0; j < nraw; j += 2)
			total += fn((uint64_t)raw[j], (int)raw[j+1]);
	}
	uint64_t end_t = now_ns();
	(void)total;
	return (double)(end_t - start_t) / (count * (nraw / 2));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// scatter harness — per-input timing, reps per input, median taken
///////////////////////////////////////////////////////////////////////////////////////////////////
struct ScatterPoint {
	std::string impl;
	double log2f;
	double ns;
};

inline void scatter_fixed(const char* impl, double* f, int nf, int digits, void (*fn)(char*, double, int), std::vector<ScatterPoint>& out, int reps = 25, int batch = 1000) {
	char dst[64];
	std::vector<double> times(reps);
	for (int i = 0; i < nf; i++) {
		for (int r = 0; r < reps; r++) {
			uint64_t s = now_ns();
			for (int b = 0; b < batch; b++)
				fn(dst, f[i], digits);
			times[r] = (double)(now_ns() - s) / batch;
		}
		std::sort(times.begin(), times.end());
		out.push_back({impl, std::log2(std::abs(f[i])), times[reps / 2]});
	}
}

inline void scatter_short(const char* impl, double* f, int nf, void (*fn)(char*, double), std::vector<ScatterPoint>& out, int reps = 25, int batch = 1000) {
    char dst[64];
    std::vector<double> times(reps);
    for (int i = 0; i < nf; i++) {
			for (int r = 0; r < reps; r++) {
				uint64_t s = now_ns();
				for (int b = 0; b < batch; b++)
					fn(dst, f[i]);
				times[r] = (double)(now_ns() - s) / batch;
			}
			std::sort(times.begin(), times.end());
			out.push_back({impl, std::log2(std::abs(f[i])), times[reps / 2]});
    }
}

inline void scatter_short_raw(const char* impl, double* f, int nf, void (*fn)(uint64_t*, int64_t*, double), std::vector<ScatterPoint>& out, int reps = 25, int batch = 1000) {
    uint64_t dp; int64_t pp;
    std::vector<double> times(reps);
    for (int i = 0; i < nf; i++) {
			for (int r = 0; r < reps; r++) {
				uint64_t s = now_ns();
				for (int b = 0; b < batch; b++)
					fn(&dp, &pp, f[i]);
				times[r] = (double)(now_ns() - s) / batch;
			}
			std::sort(times.begin(), times.end());
			out.push_back({impl, std::log2(std::abs(f[i])), times[reps / 2]});
    }
}

inline void scatter_parse_raw(const char* impl, int64_t* raw, int nraw, double (*fn)(uint64_t, int), std::vector<ScatterPoint>& out, int reps = 25, int batch = 1000) {
	std::vector<double> times(reps);
	for (int i = 0; i < nraw; i += 2) {
		uint64_t d = (uint64_t)raw[i];
		int p = (int)raw[i + 1];
		double ref = fn(d, p);
		for (int r = 0; r < reps; r++) {
			uint64_t s = now_ns();
			for (int b = 0; b < batch; b++) {
				volatile double sink = fn(d, p);
				(void)sink;
			}
			times[r] = (double)(now_ns() - s) / batch;
		}
		std::sort(times.begin(), times.end());
		out.push_back({impl, std::log2(std::abs(ref)), times[reps / 2]});
	}
}

inline void scatter_parse_text(const char* impl, std::vector<std::string>& texts, double (*fn)(char* start, char* end), std::vector<ScatterPoint>& out, int reps = 25, int batch = 1000) {
    std::vector<double> times(reps);
    for (auto& s : texts) {
			char* start = const_cast<char*>(s.c_str());
			char* end   = start + s.size();
			double ref  = fn(start, end);
			for (int r = 0; r < reps; r++) {
				uint64_t ts = now_ns();
				for (int b = 0; b < batch; b++) {
					volatile double sink = fn(start, end);
					(void)sink;
				}
				times[r] = (double)(now_ns() - ts) / batch;
			}
			std::sort(times.begin(), times.end());
			out.push_back({impl, std::log2(std::abs(ref)), times[reps / 2]});
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// input generation
///////////////////////////////////////////////////////////////////////////////////////////////////
inline uint64_t lcg_next(uint64_t& state) {
	state = state * 6364136223846793005ULL + 1442695040888963407ULL;
	return state;
}

inline std::vector<double> gen_floats(int n, uint64_t seed = 12345) {
	std::vector<double> out;
	out.reserve(n);
	uint64_t state = seed;
	for (int i = 0; i < n; i++) {
		uint64_t x = lcg_next(state);
		x = x % ((1ULL << 63) - (1ULL << 52));
		x += 1;
		double f;
		memcpy(&f, &x, 8);
		out.push_back(f);
	}
	return out;
}

inline std::vector<double> load_floats(const char* path = "data/bench_floats.csv") {
	std::vector<double> out;
	FILE* f = fopen(path, "r");
	if (!f) {
		printf("ERROR: could not open %s — run `make floats` first\n", path);
		exit(1);
	}
	char line[64];
	fgets(line, sizeof(line), f);  // skip header
	while (fgets(line, sizeof(line), f)) {
		uint64_t bits = strtoull(line, nullptr, 10);
		double x;
		memcpy(&x, &bits, 8);
		out.push_back(x);
	}
	fclose(f);
	return out;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// result reporting
///////////////////////////////////////////////////////////////////////////////////////////////////
struct BenchResult {
	std::string name;
	std::string op;
	double median_ns;
};

// append=true so repeated runs accumulate rows
inline void write_csv(const std::vector<BenchResult>& results, const char* path, bool append = true) {
	bool exists = false;
	if (append) { FILE* chk = fopen(path, "r"); if (chk) { exists = true; fclose(chk); } }
	FILE* f = fopen(path, append ? "a" : "w");
	if (!f) { printf("ERROR: could not open %s\n", path); return; }
	if (!exists) fprintf(f, "impl,op,median_ns\n");  // header only on first run
	for (auto& r : results) {
		fprintf(f, "%s,%s,%.2f\n", r.name.c_str(), r.op.c_str(), r.median_ns);
	}
	fclose(f);
	printf("appended to %s\n", path);
}

inline void write_scatter_csv(const std::vector<ScatterPoint>& points, const char* path, bool append = true) {
	bool exists = false;
	if (append) { FILE* chk = fopen(path, "r"); if (chk) { exists = true; fclose(chk); } }
	FILE* f = fopen(path, append ? "a" : "w");
	if (!f) { printf("ERROR: could not open %s\n", path); return; }
	if (!exists) fprintf(f, "impl,log2f,ns\n");
	for (auto& p : points) {
		fprintf(f, "%s,%.6f,%.2f\n", p.impl.c_str(), p.log2f, p.ns);
	}
	fclose(f);
	printf("appended to %s\n", path);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// validation -- records actual output values (not timings) per input index so
// bench_validation.py can cross-check implementations against each other and against an
// independent Fraction-based oracle. value is the formatted string for format ops, or the
// resulting double's bit pattern (as a decimal uint64) for parse ops.
///////////////////////////////////////////////////////////////////////////////////////////////////
struct ValidatePoint {
	std::string impl;
	std::string op;
	int index;
	std::string value;
};

inline void write_validate_csv(const std::vector<ValidatePoint>& points, const char* path, bool append = true) {
	bool exists = false;
	if (append) { FILE* chk = fopen(path, "r"); if (chk) { exists = true; fclose(chk); } }
	FILE* f = fopen(path, append ? "a" : "w");
	if (!f) { printf("ERROR: could not open %s\n", path); return; }
	if (!exists) fprintf(f, "impl,op,index,value\n");
	for (auto& p : points) {
		fprintf(f, "%s,%s,%d,%s\n", p.impl.c_str(), p.op.c_str(), p.index, p.value.c_str());
	}
	fclose(f);
	printf("appended to %s\n", path);
}

#endif