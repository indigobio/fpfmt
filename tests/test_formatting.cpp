#include "test.hpp"

int main() {
	int failed = 0;

	// --- digits tests ---
	printf("--- digits ---\n");
	struct DigitsCase {
		uint64_t d;
		int expected;
	};
	std::vector<DigitsCase> digits_cases = {
		{1,                    1},
		{9,                    1},
		{10,                   2},
		{99,                   2},
		{100,                  3},
		{314159265358979ULL,  15},
		{10000000000000000ULL, 17},
		{99999999999999999ULL, 17},
	};

	for (auto& tc : digits_cases) {
		int got = digits(tc.d);
		bool ok = (got == tc.expected);
		if (!ok) failed++;
		if (!ok) printf("FAIL digits(%llu): got %d want %d\n", (unsigned long long)tc.d, got, tc.expected);
	}
	if (failed == 0) printf("all digits tests passed\n");

	// --- fmt tests ---
	printf("\n--- fmt ---\n");
	int fmt_failed = 0;

	struct FmtCase {
		uint64_t d;
		int p;
		int nd;
		const char* expected;
	};

	std::vector<FmtCase> fmt_cases = {
		{314159265358979ULL,  -14, 15, "3.14159265358979e+00"},
		{1ULL,                 0,  1, "1e+00"},
		{10ULL,               -1,  2, "1.0e+00"},
		{100ULL,              -2,  3, "1.00e+00"},
		{12345ULL,            -4,  5, "1.2345e+00"},
		{10000000000000000ULL, -16, 17, "1.0000000000000000e+00"},
		{1ULL,               100,  1, "1e+100"},
		{1ULL,               -100, 1, "1e-100"},
	};

	for (auto& tc : fmt_cases) {
		char buf[64] = {};
		int n = fmt(buf, tc.d, tc.p, tc.nd);
		buf[n] = '\0';
		bool ok = (std::string(buf) == tc.expected);
		if (!ok) {
			fmt_failed++;
			failed++;
			printf("FAIL fmt(%llue%d, nd=%d): got %s want %s\n", (unsigned long long)tc.d, tc.p, tc.nd, buf, tc.expected);
		}
	}
	if (fmt_failed == 0) printf("all fmt tests passed\n");

	// --- round trip via strtod ---
	printf("\n--- fmt round trip ---\n");
	int rt_failed = 0;

	double vals[] = {
		3.14159265358979323846,
		1.0, 2.0, 0.5, 0.1,
		1e100, 1e-100,
		1.7976931348623157e308,
		5e-324,
	};

	for (double f : vals) {
		auto [d17, p17] = fixedWidth(f, 17);
		int nd = digits(d17);
		char buf[64] = {};
		int n = fmt(buf, d17, p17, nd);
		buf[n] = '\0';

		// parse back with strtod as independent reference
		char* end;
		double f2 = strtod(buf, &end);
		bool ok = (f == f2);
		if (!ok) {
			rt_failed++;
			failed++;
			printf("FAIL fmt round trip(%.17g): formatted as %s parsed back as %.17g\n", f, buf, f2);
		}
	}
	if (rt_failed == 0) printf("all fmt round trip tests passed\n");

	printf("\n%s\n", failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
	return failed == 0 ? 0 : 1;
}