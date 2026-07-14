// standalone, self-contained comparison of strtod vs std::from_chars<double> parsing
// speed. no dependency on the rest of this repo -- built to be copied to other
// machines/platforms as-is to check whether the gap seen on macOS/libc++ holds
// on Linux/libstdc++ and Windows/MSVC.
//
// build:
//   g++/clang++:  g++ -std=c++17 -O3 util/bench_strtod_vs_fromchars.cpp -o bench_strtod_vs_fromchars
//   MSVC:         cl /std:c++17 /O2 /EHsc util\bench_strtod_vs_fromchars.cpp
//
// run:
//   ./bench_strtod_vs_fromchars [n] [reps]     (defaults: n=10000, reps=30)

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

///////////////////////////////////////////////////////////////////////////////////////////////////
// deterministic double generation -- built from raw mt19937_64 output via bit manipulation rather
// than std::uniform_real_distribution, so the exact same doubles are produced on every platform
// (distribution quantization is not required to be identical across standard library implementations,
// but mt19937_64's output stream is).
///////////////////////////////////////////////////////////////////////////////////////////////////
static double make_full_range(uint64_t x) {
	// mirrors fpfmt's own bench float generator: masks into the finite,
	// non-subnormal-excluded double range (avoids inf/nan bit patterns).
	x %= ((1ULL << 63) - (1ULL << 52));
	x += 1;
	double f;
	memcpy(&f, &x, sizeof(f));
	return f;
}

static double make_typical_range(uint64_t x) {
	// fixed exponent field window (~2^-10 .. 2^30 magnitude), random sign/mantissa --
	// deliberately excludes subnormals and extreme exponents to isolate their effect.
	uint64_t sign = (x >> 63) & 1;
	uint64_t mantissa = x & ((1ULL << 52) - 1);
	uint64_t exp_field = 1013 + (x % 40);
	uint64_t bits = (sign << 63) | (exp_field << 52) | mantissa;
	double f;
	memcpy(&f, &bits, sizeof(f));
	return f;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// build a packed, newline-delimited text buffer (same layout fpfmt's bench harness uses), so the
// strtod wrapper has to pay the same "temporarily null-terminate" cost the real benchmark does.
///////////////////////////////////////////////////////////////////////////////////////////////////
static void gen_text(const std::vector<double> &floats, const char *fmt, std::vector<char> &buf) {
	char tmp[64];
	buf.clear();
	for (double f : floats) {
		int n = snprintf(tmp, sizeof(tmp), fmt, f);
		tmp[n] = '\n';
		buf.insert(buf.end(), tmp, tmp + n + 1);
	}
	buf.back() = '\0';
}

static double parse_strtod(char *start, char *end) {
	char saved = *end;
	*end = '\0';
	double f = strtod(start, nullptr);
	*end = saved;
	return f;
}

static double parse_from_chars(char *start, char *end) {
	double f;
	std::from_chars(start, end, f);
	return f;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// timing -- walks the packed buffer `reps` times, records ns/op for each full pass, and reports
// min/median/p99 across passes rather than a single elapsed/count number.
///////////////////////////////////////////////////////////////////////////////////////////////////
struct Stats {
	double min_ns, median_ns, p99_ns, max_ns;
};

static Stats time_parser(std::vector<char> &text, int reps, double (*fn)(char *, char *)) {
	int n = 0;
	for (size_t j = 0; text[j]; j++)
		if (text[j] == '\n') n++;
	n++;

	std::vector<double> pass_ns;
	pass_ns.reserve(reps);
	volatile double sink = 0; // prevent the loop from being optimized away

	for (int r = 0; r < reps; r++) {
		auto t0 = std::chrono::steady_clock::now();
		int start = 0;
		for (size_t j = 0;; j++) {
			int c = text[j];
			if (c == '\0' || c == '\n') {
				sink += fn(text.data() + start, text.data() + j);
				if (c == '\0') break;
				start = (int)j + 1;
			}
		}
		auto t1 = std::chrono::steady_clock::now();
		double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / n;
		pass_ns.push_back(ns);
	}
	(void)sink;

	std::sort(pass_ns.begin(), pass_ns.end());
	Stats s;
	s.min_ns = pass_ns.front();
	s.median_ns = pass_ns[pass_ns.size() / 2];
	s.p99_ns = pass_ns[(size_t)(pass_ns.size() * 0.99)];
	s.max_ns = pass_ns.back();
	return s;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// platform/compiler tag, so results copied from different machines are self-labeled
///////////////////////////////////////////////////////////////////////////////////////////////////
static const char *platform_tag() {
#if defined(_WIN32)
	return "windows";
#elif defined(__APPLE__)
	return "macos";
#elif defined(__linux__)
	return "linux";
#else
	return "unknown";
#endif
}

static const char *compiler_tag() {
#if defined(_MSC_VER)
	return "msvc";
#elif defined(__clang__)
	return "clang";
#elif defined(__GNUC__)
	return "gcc";
#else
	return "unknown";
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// main
///////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv) {
	int n = argc > 1 ? atoi(argv[1]) : 10000;
	int reps = argc > 2 ? atoi(argv[2]) : 30;

	printf("platform=%s compiler=%s n=%d reps=%d\n\n", platform_tag(), compiler_tag(), n, reps);

	std::mt19937_64 rng(12345); // fixed seed -- identical doubles on every platform
	std::vector<double> full_range, typical_range;
	for (int i = 0; i < n; i++) full_range.push_back(make_full_range(rng()));
	for (int i = 0; i < n; i++) typical_range.push_back(make_typical_range(rng()));

	struct Dataset {
		const char *name;
		std::vector<double> *floats;
	};
	Dataset datasets[] = {
		{"full_range (subnormals..near-max, like fpfmt's bench data)", &full_range},
		{"typical_range (~2^-10..2^30 magnitude)", &typical_range},
	};

	struct Format {
		const char *name;
		const char *fmt;
	};
	Format formats[] = {
		{"%.16e (17 sig digits, round-trip)", "%.16e"},
		{"%.6e", "%.6e"},
		{"%.2e", "%.2e"},
		{"%g", "%g"},
	};

	for (auto &ds : datasets) {
		printf("=== dataset: %s ===\n", ds.name);
		printf("  %-38s %10s %10s %10s %10s\n", "format", "impl", "min_ns", "median_ns", "p99_ns");

		for (auto &fm : formats) {
			std::vector<char> text;
			gen_text(*ds.floats, fm.fmt, text);

			Stats s_strtod = time_parser(text, reps, parse_strtod);
			Stats s_fc = time_parser(text, reps, parse_from_chars);
			double ratio = s_fc.median_ns / s_strtod.median_ns;

			printf("  %-38s %10s %10.1f %10.1f %10.1f\n", fm.name, "strtod", s_strtod.min_ns, s_strtod.median_ns, s_strtod.p99_ns);
			printf("  %-38s %10s %10.1f %10.1f %10.1f\n", "", "from_chars", s_fc.min_ns, s_fc.median_ns, s_fc.p99_ns);
			printf("  %-38s %10s %10.2fx (from_chars / strtod)\n", "", "ratio", ratio);
		}
		printf("\n");
	}

	return 0;
}
