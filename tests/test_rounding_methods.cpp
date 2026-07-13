#include "test.hpp"

int main() {
	int failed = 0;

	struct TestCase {
		double x;
		uint64_t floor, roundHalfDown, round, roundHalfUp, ceil;
	};

	std::vector<TestCase> cases = {
		{6.0,  6, 6, 6, 6, 6},
		{6.25, 6, 6, 6, 6, 7},
		{6.5,  6, 6, 6, 7, 7},
		{6.75, 6, 7, 7, 7, 7},
		{7.0,  7, 7, 7, 7, 7},
		{7.5,  7, 7, 8, 8, 8},
		{8.5,  8, 8, 8, 9, 9},
	};

	printf("%-6s %-6s %-6s %-6s %-6s %-6s %-6s\n", "x", "floor", "round½↓", "round", "round½↑", "ceil", "result");
	for (auto& tc : cases) {
		Unrounded u = unround(tc.x);
		bool ok = u.floor() == tc.floor && u.roundHalfDown() == tc.roundHalfDown && u.round() == tc.round && u.roundHalfUp() == tc.roundHalfUp && u.ceil() == tc.ceil;
		if (!ok) failed++;

		printf("%-10s %-6llu %-7llu %-6llu %-7llu %-6llu %s\n", unrounded_string(u).c_str(), (unsigned long long)u.floor(), (unsigned long long)u.roundHalfDown(),
					 (unsigned long long)u.round(), (unsigned long long)u.roundHalfUp(), (unsigned long long)u.ceil(), ok ? "PASS" : "FAIL");
	}

	printf("\n%s\n", failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
	return failed == 0 ? 0 : 1;
}