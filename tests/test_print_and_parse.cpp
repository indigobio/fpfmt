#include "test.hpp"

int main() {
	int failed = 0;

	auto tests = load_ivy_tests("data/test.ivy");
	auto edge_tests = load_ivy_tests("data/test.ib");
	auto more_tests = load_ivy_tests("data/test.more");
	if (tests.empty() || edge_tests.empty() || more_tests.empty()) {
		printf("ERROR: no tests loaded\n");
		return 1;
	}
	int ivy_test_size = tests.size();
	tests.insert(tests.end(), edge_tests.begin(), edge_tests.end());
	tests.insert(tests.end(), more_tests.begin(), more_tests.end());
	printf("loaded %zu tests (%d ivy tests, %zu edge cases, and %zu more test cases)\n\n", tests.size(), ivy_test_size, edge_tests.size(), more_tests.size());

	// --- fixedWidth tests ---
	printf("--- fixedWidth ---\n");
	int fw_failed = 0;
	int fw_passed = 0;
	for (auto& tt : tests) {
		if (tt.n > 18) continue;
		auto [d, p] = fixedWidth(tt.f, tt.n);
		bool ok = (d == tt.d) && (p == tt.p);
		if (!ok) {
			fw_failed++;
			failed++;
			printf("FAIL fixedWidth(%.17g, %d): got %llue%d want %llue%d\n", tt.f, tt.n, (unsigned long long)d, p, (unsigned long long)tt.d, tt.p);
		} else fw_passed++;
	}
	if (fw_failed == 0) printf("all %d fixedWidth tests passed\n", fw_passed);

	// --- parse tests ---
	// use 17-digit cases for exact round trip verification
	printf("\n--- parse ---\n");
	int parse_failed = 0;
	int parse_passed = 0;
	for (auto& tt : tests) {
		if (tt.n != 17) continue;
		double f2 = parse(tt.d, tt.p);
		bool ok = (tt.f == f2);
		if (!ok) {
			parse_failed++;
			failed++;
			printf("FAIL parse(%llue%d): got %.17g want %.17g\n", (unsigned long long)tt.d, tt.p, f2, tt.f);
		} else parse_passed++;
	}
	if (parse_failed == 0) printf("all %d parse tests passed\n", parse_passed);

	// --- shortWidth tests ---
	printf("\n--- shortWidth ---\n");
	int short_failed = 0;
	int short_passed = 0;
	std::set<double> seen;
	for (auto& tt : tests) {
		if (seen.count(tt.f)) continue;
		seen.insert(tt.f);

		auto [d, p] = shortWidth(tt.f);

		bool roundtrip = (parse(d, p) == tt.f);
		bool shortest = (d < 10) || (parse(d / 10, p + 1) != tt.f);

		bool ok = roundtrip && shortest;
		if (!ok) {
			short_failed++;
			failed++;
			printf("FAIL shortWidth(%.17g): got %llue%d roundtrip=%s shortest=%s\n", tt.f, (unsigned long long)d, p, roundtrip ? "PASS" : "FAIL", shortest  ? "PASS" : "FAIL");
		} else short_passed++;
	}
	if (short_failed == 0) printf("all %d shortWidth tests passed\n", short_passed);

	printf("\n%s\n", failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
	return failed == 0 ? 0 : 1;
}