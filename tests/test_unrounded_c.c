#include "test_c.h"

int main(void) {
	int failed = 0;

	double nums[] = {6.0, 6.001, 6.499, 6.5, 6.501, 6.999, 7.0};
	const char* keys[] = {"\xe2\x9f\xa8" "6.0\xe2\x9f\xa9", "\xe2\x9f\xa8" "6.0+\xe2\x9f\xa9", "\xe2\x9f\xa8" "6.0+\xe2\x9f\xa9", "\xe2\x9f\xa8" "6.5\xe2\x9f\xa9", "\xe2\x9f\xa8" "6.5+\xe2\x9f\xa9", "\xe2\x9f\xa8" "6.5+\xe2\x9f\xa9", "\xe2\x9f\xa8" "7.0\xe2\x9f\xa9"};

	printf("%-10s %-6s %-8s %s\n", "x", "raw", "str", "result");
	for (int i = 0; i < (int)(sizeof(nums) / sizeof(nums[0])); i++) {
		Unrounded u = unround(nums[i]);
		char s[64];
		unrounded_string(u, s, sizeof(s));
		bool ok = (strcmp(s, keys[i]) == 0);
		if (!ok) failed++;
		printf("%-10.3f %-6llu %-12s %-6s\n", nums[i], (unsigned long long)u.val, s, ok ? "PASS" : "FAIL");
	}

	printf("\n%s\n", failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
	return failed == 0 ? 0 : 1;
}
