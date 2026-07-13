#include "test.hpp"

int main() {
	int failed = 0;

	// --- div tests ---
	printf("--- div ---\n");
	struct DivCase {
		double x;
		uint64_t d;
		uint64_t expected_round;
		std::string expected_str;
	};

	std::vector<DivCase> div_keys = {
		{15.4, 6, 3, "⟨2.5+⟩"},
		{12.0, 4, 3, "⟨3.0⟩"},
		{15.0, 2, 8, "⟨7.5⟩"},
	};

	for (auto& tc : div_keys) {
		Unrounded u = unround(tc.x).div(tc.d);
		bool str_ok = (unrounded_string(u) == tc.expected_str);
		bool round_ok = (u.round() == tc.expected_round);
		bool ok = str_ok && round_ok;
		if (!ok) failed++;
		printf("unround(%.1f).div(%llu) = %-10s round=%llu %s\n", tc.x, (unsigned long long)tc.d, unrounded_string(u).c_str(), (unsigned long long)u.round(), ok ? "PASS" : "FAIL");
	}

	// --- rsh tests ---
	printf("\n--- rsh ---\n");
	struct RshCase {
		double x;
		int s;
		std::string expected_str;
	};

	std::vector<RshCase> rsh_keys = {
		{6.0,  1, "⟨3.0⟩"},
		{7.0,  1, "⟨3.5⟩"},
		{6.5,  2, "⟨1.5+⟩"},
		{6.25, 1, "⟨3.0+⟩"},
	};

	for (auto& tc : rsh_keys) {
		Unrounded u = unround(tc.x).rsh(tc.s);
		bool ok = (unrounded_string(u) == tc.expected_str);
		if (!ok) failed++;
		printf("unround(%.2f).rsh(%d) = %-10s %s\n", tc.x, tc.s, unrounded_string(u).c_str(), ok ? "PASS" : "FAIL");
	}

	// --- nudge tests ---
	printf("\n--- nudge ---\n");
	struct NudgeCase {
		double x;
		int delta;
		uint64_t expected_floor;
		uint64_t expected_ceil;
	};

	std::vector<NudgeCase> nudge_keys = {
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
	for (auto& tc : nudge_keys) {
		Unrounded u = unround(tc.x).nudge(tc.delta);
		bool ok = (u.floor() == tc.expected_floor) && (u.ceil() == tc.expected_ceil);
		if (!ok) failed++;
		printf("%-6.1f %+-6d %-12s %-6llu %-5llu %s\n", tc.x, tc.delta, unrounded_string(u).c_str(), (unsigned long long)u.floor(), (unsigned long long)u.ceil(), ok ? "PASS" : "FAIL");
	}

	printf("\n%s\n", failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
	return failed == 0 ? 0 : 1;
}