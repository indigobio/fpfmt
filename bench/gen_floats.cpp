#include "bench.hpp"

// generates the shared benchmark float set once and writes it as raw IEEE-754
// bit patterns to data/floats.csv, so every bench (C++, Ruby, Scheme) can load
// the exact same doubles instead of regenerating them independently.
int main() {
	auto floats = gen_floats(10000);

	const char* path = "data/bench_floats.csv";
	FILE* f = fopen(path, "w");
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
