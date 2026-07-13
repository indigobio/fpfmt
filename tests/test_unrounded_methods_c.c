#include "test_c.h"

int main(void) {
	int failed = 0;

	// --- div tests ---
	printf("--- div ---\n");
	typedef struct {
		double x;
		uint64_t d;
		uint64_t expected_round;
		const char* expected_str;
	} DivCase;

	DivCase div_keys[] = {
		{15.4, 6, 3, "\xe2\x9f\xa8" "2.5+\xe2\x9f\xa9"},
		{12.0, 4, 3, "\xe2\x9f\xa8" "3.0\xe2\x9f\xa9"},
		{15.0, 2, 8, "\xe2\x9f\xa8" "7.5\xe2\x9f\xa9"},
	};

	for (size_t i = 0; i < sizeof(div_keys) / sizeof(div_keys[0]); i++) {
		DivCase* tc = &div_keys[i];
		Unrounded u = unrounded_div(unround(tc->x), tc->d);
		char s[64];
		unrounded_string(u, s, sizeof(s));
		bool str_ok = (strcmp(s, tc->expected_str) == 0);
		bool round_ok = (unrounded_round(u) == tc->expected_round);
		bool ok = str_ok && round_ok;
		if (!ok) failed++;
		printf("unround(%.1f).div(%llu) = %-10s round=%llu %s\n", tc->x, (unsigned long long)tc->d, s, (unsigned long long)unrounded_round(u), ok ? "PASS" : "FAIL");
	}

	// --- rsh tests ---
	printf("\n--- rsh ---\n");
	typedef struct {
		double x;
		int s;
		const char* expected_str;
	} RshCase;

	RshCase rsh_keys[] = {
		{6.0,  1, "\xe2\x9f\xa8" "3.0\xe2\x9f\xa9"},
		{7.0,  1, "\xe2\x9f\xa8" "3.5\xe2\x9f\xa9"},
		{6.5,  2, "\xe2\x9f\xa8" "1.5+\xe2\x9f\xa9"},
		{6.25, 1, "\xe2\x9f\xa8" "3.0+\xe2\x9f\xa9"},
	};

	for (size_t i = 0; i < sizeof(rsh_keys) / sizeof(rsh_keys[0]); i++) {
		RshCase* tc = &rsh_keys[i];
		Unrounded u = unrounded_rsh(unround(tc->x), tc->s);
		char s[64];
		unrounded_string(u, s, sizeof(s));
		bool ok = (strcmp(s, tc->expected_str) == 0);
		if (!ok) failed++;
		printf("unround(%.2f).rsh(%d) = %-10s %s\n", tc->x, tc->s, s, ok ? "PASS" : "FAIL");
	}

	// --- nudge tests ---
	printf("\n--- nudge ---\n");
	typedef struct {
		double x;
		int delta;
		uint64_t expected_floor;
		uint64_t expected_ceil;
	} NudgeCase;

	NudgeCase nudge_keys[] = {
		{15.0,  -1, 14, 15},
		{15.1,  -1, 15, 15},
		{15.75, -1, 15, 16},
		{16.0,  -1, 15, 16},
		{15.0,  +1, 15, 16},
		{15.1,  +1, 15, 16},
		{15.75, +1, 16, 16},
		{16.0,  +1, 16, 17},
	};

	printf("%-6s %-6s %-8s %-6s %-5s %-6s\n", "x", "delta", "nudged", "floor", "ceil", "result");
	for (size_t i = 0; i < sizeof(nudge_keys) / sizeof(nudge_keys[0]); i++) {
		NudgeCase* tc = &nudge_keys[i];
		Unrounded u = unrounded_nudge(unround(tc->x), tc->delta);
		bool ok = (unrounded_floor(u) == tc->expected_floor) && (unrounded_ceil(u) == tc->expected_ceil);
		if (!ok) failed++;
		char s[64];
		unrounded_string(u, s, sizeof(s));
		printf("%-6.1f %+-6d %-12s %-6llu %-5llu %s\n", tc->x, tc->delta, s, (unsigned long long)unrounded_floor(u), (unsigned long long)unrounded_ceil(u), ok ? "PASS" : "FAIL");
	}

	printf("\n%s\n", failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
	return failed == 0 ? 0 : 1;
}
