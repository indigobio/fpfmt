#include <cstdint>
#include <vector>
#include <cstring>
#include <cstdio>

// generates the shared benchmark float set once and writes it as raw IEEE-754
// bit patterns to data/floats.csv, so every bench (C++, Ruby, Scheme) can load
// the exact same doubles instead of regenerating them independently.
uint64_t lcg_next(uint64_t& state) {
	state = state * 6364136223846793005ULL + 1442695040888963407ULL;
	return state;
}

std::vector<double> gen_floats(int n, uint64_t seed = 12345) {
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

int main() {
	auto floats = gen_floats(10000);

	const char* path = "data/bench_floats.csv";
	FILE* f = fopen(path, "wb");
	if (!f) { printf("ERROR: could not open %s\n", path); return 1; }
	fprintf(f, "bits\n");
	for (double x : floats) {
		uint64_t bits;
		memcpy(&bits, &x, 8);
		fprintf(f, "%llu\n", (unsigned long long)bits);
	}
	fclose(f);
	printf("wrote %zu floats to %s\n", floats.size(), path);
	return 0;
}
