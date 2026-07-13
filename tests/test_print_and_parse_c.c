#include "test_c.h"

int main(void) {
	int failed = 0;

	size_t ivy_test_size, edge_count, more_count;
	IvyTest* ivy_tests = load_ivy_tests("data/test.ivy", &ivy_test_size);
	IvyTest* edge_tests = load_ivy_tests("data/test.ib", &edge_count);
	IvyTest* more_tests = load_ivy_tests("data/test.more", &more_count);
	if (ivy_test_size == 0 || edge_count == 0 || more_count == 0) {
		printf("ERROR: no tests loaded\n");
		return 1;
	}

	size_t total = ivy_test_size + edge_count + more_count;
	IvyTest* tests = malloc(total * sizeof(IvyTest));
	memcpy(tests, ivy_tests, ivy_test_size * sizeof(IvyTest));
	memcpy(tests + ivy_test_size, edge_tests, edge_count * sizeof(IvyTest));
	memcpy(tests + ivy_test_size + edge_count, more_tests, more_count * sizeof(IvyTest));
	free(ivy_tests);
	free(edge_tests);
	free(more_tests);

	printf("loaded %zu tests (%zu ivy tests, %zu edge cases, and %zu more test cases)\n\n", total, ivy_test_size, edge_count, more_count);

	// --- fixedWidth tests ---
	printf("--- fixedWidth ---\n");
	int fw_failed = 0;
	int fw_passed = 0;
	for (size_t i = 0; i < total; i++) {
		IvyTest* tt = &tests[i];
		if (tt->n > 18) continue;
		FloatRep fr = fixedWidth(tt->f, tt->n);
		bool ok = (fr.m == tt->d) && (fr.e == tt->p);
		if (!ok) {
			fw_failed++;
			failed++;
			printf("FAIL fixedWidth(%.17g, %d): got %llue%d want %llue%d\n", tt->f, tt->n, (unsigned long long)fr.m, fr.e, (unsigned long long)tt->d, tt->p);
		} else fw_passed++;
	}
	if (fw_failed == 0) printf("all %d fixedWidth tests passed\n", fw_passed);

	// --- parse tests ---
	// use 17-digit cases for exact round trip verification
	printf("\n--- parse ---\n");
	int parse_failed = 0;
	int parse_passed = 0;
	for (size_t i = 0; i < total; i++) {
		IvyTest* tt = &tests[i];
		if (tt->n != 17) continue;
		double f2 = parse(tt->d, tt->p);
		bool ok = (tt->f == f2);
		if (!ok) {
			parse_failed++;
			failed++;
			printf("FAIL parse(%llue%d): got %.17g want %.17g\n", (unsigned long long)tt->d, tt->p, f2, tt->f);
		} else parse_passed++;
	}
	if (parse_failed == 0) printf("all %d parse tests passed\n", parse_passed);

	// --- shortWidth tests ---
	printf("\n--- shortWidth ---\n");
	int short_failed = 0;
	int short_passed = 0;
	DoubleSet seen;
	doubleset_init(&seen, total * 4 + 16);
	for (size_t i = 0; i < total; i++) {
		IvyTest* tt = &tests[i];
		if (doubleset_seen(&seen, tt->f)) continue;

		FloatRep fr = shortWidth(tt->f);

		bool roundtrip = (parse(fr.m, fr.e) == tt->f);
		bool shortest = (fr.m < 10) || (parse(fr.m / 10, fr.e + 1) != tt->f);

		bool ok = roundtrip && shortest;
		if (!ok) {
			short_failed++;
			failed++;
			printf("FAIL shortWidth(%.17g): got %llue%d roundtrip=%s shortest=%s\n", tt->f, (unsigned long long)fr.m, fr.e, roundtrip ? "PASS" : "FAIL", shortest  ? "PASS" : "FAIL");
		} else short_passed++;
	}
	doubleset_free(&seen);
	if (short_failed == 0) printf("all %d shortWidth tests passed\n", short_passed);

	free(tests);

	printf("\n%s\n", failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
	return failed == 0 ? 0 : 1;
}
