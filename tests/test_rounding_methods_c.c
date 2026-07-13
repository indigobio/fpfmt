#include "test_c.h"

int main(void) {
	int failed = 0;

	typedef struct {
		double x;
		uint64_t floor_, roundHalfDown, round_, roundHalfUp, ceil_;
	} TestCase;

	TestCase cases[] = {
		{6.0,  6, 6, 6, 6, 6},
		{6.25, 6, 6, 6, 6, 7},
		{6.5,  6, 6, 6, 7, 7},
		{6.75, 6, 7, 7, 7, 7},
		{7.0,  7, 7, 7, 7, 7},
		{7.5,  7, 7, 8, 8, 8},
		{8.5,  8, 8, 8, 9, 9},
	};

	printf("%-6s %-6s %-6s %-6s %-6s %-6s %-6s\n", "x", "floor", "round\xc2\xbd\xe2\x86\x93", "round", "round\xc2\xbd\xe2\x86\x91", "ceil", "result");
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		TestCase* tc = &cases[i];
		Unrounded u = unround(tc->x);
		bool ok = unrounded_floor(u) == tc->floor_ && unrounded_round_half_down(u) == tc->roundHalfDown &&
			unrounded_round(u) == tc->round_ && unrounded_round_half_up(u) == tc->roundHalfUp && unrounded_ceil(u) == tc->ceil_;
		if (!ok) failed++;

		char s[64];
		unrounded_string(u, s, sizeof(s));
		printf("%-10s %-6llu %-7llu %-6llu %-7llu %-6llu %s\n", s, (unsigned long long)unrounded_floor(u), (unsigned long long)unrounded_round_half_down(u),
					 (unsigned long long)unrounded_round(u), (unsigned long long)unrounded_round_half_up(u), (unsigned long long)unrounded_ceil(u), ok ? "PASS" : "FAIL");
	}

	printf("\n%s\n", failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
	return failed == 0 ? 0 : 1;
}
