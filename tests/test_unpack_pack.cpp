#include "test.hpp"

int main() {
	int failed = 0;
 
	// test 1: pi round trip
	double pi = 3.14159265358979323846;
	auto [m, e] = unpack64(pi);
	printf("--- pi ---\n");
	printf("m = 0x%llx\n", (unsigned long long)m);
	printf("e = %d\n", e);
	double pi2 = safeUnpackPack(pi);
	bool pi_ok = (pi == pi2);
	if (!pi_ok) failed++;
	printf("round trip: %s\n", pi_ok ? "PASS" : "FAIL");
	printf("pi  bits: 0x%llx\n", (unsigned long long)float64bits(pi));
	printf("pi2 bits: 0x%llx\n", (unsigned long long)float64bits(pi2));
 
	// test 2: paper says m should be 0x1921fb54442d18
	// our m is left-shifted to 64 bits, so check the high 53 bits
	bool mantissa_ok = ((m >> 11) == 0x1921fb54442d18ULL);
	if (!mantissa_ok) failed++;
	printf("m >> 11 = 0x%llx (expect 0x1921fb54442d18): %s\n", (unsigned long long)(m >> 11), mantissa_ok ? "PASS" : "FAIL");
 
	// test 3: round trips
	printf("\n--- round trips ---\n");
	double vals[] = { 1.0, 2.0, 0.1, 1.5, 1e100, 1e-100 };
	for (double v : vals) {
		double v2 = safeUnpackPack(v);
		bool ok = (v == v2);
		if (!ok) failed++;
		printf("%e: %s\n", v, ok ? "PASS" : "FAIL");
	}

	printf("\n%s\n", failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
	return failed == 0 ? 0 : 1;
}