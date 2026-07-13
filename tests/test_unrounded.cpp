#include "test.hpp"

int main() {
	int failed = 0;

	std::vector<double> nums = {6.0, 6.001, 6.499, 6.5, 6.501, 6.999, 7.0};
	std::vector<std::string> keys = {"⟨6.0⟩", "⟨6.0+⟩", "⟨6.0+⟩", "⟨6.5⟩", "⟨6.5+⟩", "⟨6.5+⟩", "⟨7.0⟩"};
	
	printf("%-10s %-6s %-8s %s\n", "x", "raw", "str", "result");
	for (int i = 0; i < (int)nums.size(); i++) {
		Unrounded u = unround(nums[i]);
		std::string s = unrounded_string(u);
		bool ok = (s == keys[i]);
		if (!ok) failed++;
		printf("%-10.3f %-6llu %-12s %-6s\n", nums[i], (unsigned long long)u.val, s.c_str(), ok ? "PASS" : "FAIL");
	}

	printf("\n%s\n", failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
	return failed == 0 ? 0 : 1;
}